#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <chrono>
#include <thread>
#include <unordered_map>
#include <set>
#include <memory>
#include <shared_mutex>

#include "io.hpp"

struct Order {
public :
	uint32_t order_id;
	std::string instrument;
	uint32_t price;
	mutable uint32_t count;
	mutable std::chrono::microseconds::rep timestamp;
	mutable bool is_buy;
	mutable int execution_id = 1;

	mutable std::mutex mutex;

	Order(uint32_t id, const std::string& instr, uint32_t pr, uint32_t cnt, std::chrono::microseconds::rep ts, bool buy)
		: order_id(id), instrument(instr), price(pr), count(cnt), timestamp(ts), is_buy(buy) {}
};

struct Execution {
	uint32_t current_order_id;
	uint32_t matching_order_id;
	mutable int execution_id = 1;
	uint32_t price;
	mutable uint32_t count;
	mutable std::chrono::microseconds::rep timestamp;
	mutable int is_buy;

	Execution() = default;

    static inline Execution create(uint32_t cur_id, uint32_t match_id, uint32_t exec_id, uint32_t price, 
                                   uint32_t count, std::chrono::microseconds::rep ts, bool buy) noexcept {
        Execution order;
        order.current_order_id = cur_id;
        order.matching_order_id = match_id;
        order.execution_id = exec_id;
        order.price = price;
        order.count = count;
        order.timestamp = ts;
        order.is_buy = buy;
        return order;
    }

};

struct BuyOrder : public Order {
	BuyOrder(uint32_t id, const std::string& instr, uint32_t pr, uint32_t cnt, std::chrono::microseconds::rep ts)
		: Order(id, instr, pr, cnt, ts, true) {} 

	bool operator<(const BuyOrder& otherBuy) const {
		if (price == otherBuy.price) {
			return timestamp < otherBuy.timestamp;
		} 
		return price > otherBuy.price;
	}
};

struct BuyComparator {
	bool operator()(const std::shared_ptr<BuyOrder>& l, const std::shared_ptr<BuyOrder>& r) const {
		return *l < *r;
	}
};


struct SellOrder : public Order {
	SellOrder(uint32_t id, const std::string& instr, uint32_t pr, uint32_t cnt, std::chrono::microseconds::rep ts)
		: Order(id, instr, pr, cnt, ts, false) {} 

	bool operator<(const SellOrder& otherSell) const {
		if (price == otherSell.price) {
			return timestamp < otherSell.timestamp;
		}
		return price < otherSell.price;
	}
};

struct SellComparator {
	bool operator()(const std::shared_ptr<SellOrder>& l, const std::shared_ptr<SellOrder>& r) const {
		return *l < *r;
	}
};


template <typename Key, typename Val>
class Safe_Map {
	std::shared_mutex mutex;
	std::unordered_map<Key, Val> mp;

public:
	int size() {
		std::shared_lock<std::shared_mutex> lock(mutex);
		return (int) mp.size();
	}

	void insert(const Key& key, const Val& val) {	
		std::unique_lock<std::shared_mutex> lock(mutex);
		mp[key] = val;
	}

	Val& read(const Key& key) {
		mutex.lock();
		auto it = mp.find(key);
		bool exist = it != mp.end();
		mutex.unlock();
			
		if (exist) {
			return it->second;
		}

		std::unique_lock<std::shared_mutex> lock(mutex);
		return mp[key];
	}
};

template <typename T, typename Compare>
class Safe_Set {
	std::shared_mutex mutex;
	std::set<T, Compare> st;

public:
	int size() {
	std::shared_lock<std::shared_mutex> lock(mutex);
		return (int) st.size();
	}

	std::set<T, Compare>::iterator begin() {
		std::shared_lock<std::shared_mutex> lock(mutex);
		return st.begin();
	}

	std::set<T, Compare>::iterator end() {
		std::shared_lock<std::shared_mutex> lock(mutex);
		return st.end();
	}

	void insert(const T& val) {
		std::unique_lock<std::shared_mutex> lock(mutex);
		st.insert(val);
	}

	void erase(const T& val) {
		std::unique_lock<std::shared_mutex> lock(mutex);
		if (st.find(val) != st.end()) {
			st.erase(st.find(val));
		}
	}

	bool exist(const T& val) {
		std::shared_lock<std::shared_mutex> lock(mutex);
		return st.find(val) != st.end();
	}

};

class CounterMutex {
	int counter = 0;
	std::mutex counter_mutex;

public:
	void lock(std::mutex& mutex) {
		std::lock_guard<std::mutex> cur_lock(counter_mutex);
		counter += 1;

		if(counter == 1) {
			mutex.lock();
		}
	}

	void unlock(std::mutex& mutex) {
		std::lock_guard<std::mutex> cur_lock(counter_mutex);
		counter -= 1;
			
		if(counter == 0) {
			mutex.unlock();
		}
	}
};

class BuySellMutex {
	CounterMutex buyMutex;
	CounterMutex sellMutex;
	std::mutex mut;

public:
	void lock_buy() {
		buyMutex.lock(std::ref(mut));
	}	

	void unlock_buy() {
		buyMutex.unlock(std::ref(mut));
	}

	void lock_sell() {
		sellMutex.lock(std::ref(mut));
	}

	void unlock_sell() {
		sellMutex.unlock(std::ref(mut));
	}
};

struct Engine
{
public:
	void accept(ClientConnection conn);

private:
	void connection_thread(ClientConnection conn);

	Safe_Map<std::string, Safe_Set<std::shared_ptr<BuyOrder>, BuyComparator> > buyOrderMap;
	Safe_Map<std::string, Safe_Set<std::shared_ptr<SellOrder>, SellComparator> > sellOrderMap;

	Safe_Map<std::string, BuySellMutex> mutexMap;

	std::shared_ptr<BuyOrder> create_buy(const ClientCommand& input, std::unordered_map<int, std::shared_ptr<Order> >& allOrders);
	std::shared_ptr<SellOrder> create_sell(const ClientCommand& input, std::unordered_map<int, std::shared_ptr<Order> >& allOrders);

	void match_buy(std::shared_ptr<BuyOrder> curOrder);
	void match_sell(std::shared_ptr<SellOrder> curOrder);

	void cancel_order(const ClientCommand& input, std::unordered_map<int, std::shared_ptr<Order> >& allOrders);

};

inline std::chrono::microseconds::rep getCurrentTimestamp() noexcept
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

#endif
