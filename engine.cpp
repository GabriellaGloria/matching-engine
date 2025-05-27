#include <iostream>
#include <thread>

#include "io.hpp"
#include "engine.hpp"
#include <vector>

void Engine::accept(ClientConnection connection)
{
	auto thread = std::thread(&Engine::connection_thread, this, std::move(connection));
	thread.detach();
}

void Engine::connection_thread(ClientConnection connection)
{
	std::unordered_map<int, std::shared_ptr<Order> > allOrders;

	while(true)
	{
		ClientCommand input {};
		switch(connection.readInput(input))
		{
			case ReadResult::Error: SyncCerr {} << "Error reading input" << std::endl;
			case ReadResult::EndOfFile: return;
			case ReadResult::Success: break;
		}

		switch(input.type)
		{
			case input_buy: {
				SyncCerr {} << "Got Buy: ID: " << input.order_id << std::endl;
				std::shared_ptr<BuyOrder> order = create_buy(input, allOrders);
				match_buy(order);

				break;
			}

			case input_sell: {
				SyncCerr {} << "Got Sell: ID: " << input.order_id << std::endl;
				std::shared_ptr<SellOrder> order = create_sell(input, allOrders);
				match_sell(order);

				break;
			}

			case input_cancel: {
				SyncCerr {} << "Got cancel: ID: " << input.order_id << std::endl;
				cancel_order(input, allOrders);

				break;
			}

		}

	}
}

std::shared_ptr<BuyOrder> Engine::create_buy(const ClientCommand& input, std::unordered_map<int, std::shared_ptr<Order> >& allOrders) {
	std::shared_ptr<BuyOrder> order = std::make_shared<BuyOrder>(
		input.order_id,
		input.instrument,
		input.price,
		input.count,
		getCurrentTimestamp()
	);
	allOrders[input.order_id] = order;

	return order;
}

std::shared_ptr<SellOrder> Engine::create_sell(const ClientCommand& input, std::unordered_map<int, std::shared_ptr<Order> >& allOrders) {
	std::shared_ptr<SellOrder> order = std::make_shared<SellOrder>(
		input.order_id,
		input.instrument,
		input.price,
		input.count,
		getCurrentTimestamp()
	);
	allOrders[input.order_id] = order;

	return order;
}

void Engine::match_buy(std::shared_ptr<BuyOrder> curOrder) {
	BuySellMutex& mtx = mutexMap.read(curOrder->instrument);

	bool add_order = false;
	int remaining = 0;
	std::vector<Execution> executed_orders;
	mtx.lock_buy();

	auto& sellOrders = sellOrderMap.read(curOrder->instrument);

	for(auto it=sellOrders.begin(); it != sellOrders.end(); it++) {
		auto& cur = *it;

		std::scoped_lock<std::mutex> lock(cur->mutex);
		if (curOrder->count <= 0 || cur->price > curOrder->price) {
			break;
		}

		if (cur->count <= 0) {
			continue;
		}
		
		int num_count = std::min(cur->count, curOrder->count);
		curOrder->count -= num_count;
		cur->count -= num_count;

		executed_orders.push_back(Execution::create(cur->order_id, curOrder->order_id, cur->execution_id, cur->price, num_count, getCurrentTimestamp(), false));
		cur->execution_id++;
	}

	if(curOrder->count > 0) {
		auto& mp = buyOrderMap.read(curOrder->instrument);
		curOrder->timestamp = getCurrentTimestamp();
		mp.insert(curOrder);
		remaining = curOrder->count;
		add_order = true;
	}

	mtx.unlock_buy();

	if (add_order) {
		Output::OrderAdded(curOrder->order_id, curOrder->instrument.c_str(), curOrder->price, remaining, false, curOrder->timestamp);
	}

	for(auto i : executed_orders) {
		Output::OrderExecuted(i.current_order_id, i.matching_order_id, i.execution_id, i.price, i.count, i.timestamp);			
	}
	
}

void Engine::match_sell(std::shared_ptr<SellOrder> curOrder) {
	BuySellMutex& mtx = mutexMap.read(curOrder->instrument);

	std::vector<Execution> executed_orders;

	bool add_order = false;
	int remaining = 0;

	mtx.lock_sell();
	auto& buyOrders = buyOrderMap.read(curOrder->instrument);

	for(auto it=buyOrders.begin(); it != buyOrders.end(); it++) {
		auto& cur = *it;
		std::scoped_lock<std::mutex> lock(cur->mutex);

		if (curOrder->count <= 0 || cur->price < curOrder->price) {
			break;
		}

		if (cur->count <= 0) {
			continue;
		}
		
		int num_count = std::min(cur->count, curOrder->count);
		curOrder->count -= num_count;
		cur->count -= num_count;

		executed_orders.push_back(Execution::create(cur->order_id, curOrder->order_id, cur->execution_id, cur->price, num_count, getCurrentTimestamp(), false));
		cur->execution_id++;
	}

	if(curOrder->count > 0) {
		auto& mp = sellOrderMap.read(curOrder->instrument);
		curOrder->timestamp = getCurrentTimestamp();
		mp.insert(curOrder);
		remaining = curOrder->count;
		add_order = true;
	}

	mtx.unlock_sell();

	if (add_order) {
		Output::OrderAdded(curOrder->order_id, curOrder->instrument.c_str(), curOrder->price, remaining, true, curOrder->timestamp);
	}

	for(auto i : executed_orders) {
		Output::OrderExecuted(i.current_order_id, i.matching_order_id, i.execution_id, i.price, i.count, i.timestamp);			
	}
}

void Engine::cancel_order(const ClientCommand& input, std::unordered_map<int, std::shared_ptr<Order> >& allOrders) {
	std::shared_ptr<Order> cur = allOrders[input.order_id];

	(cur->mutex).lock();

	bool avail = true;
	if (cur->count <= 0) {
		avail = false;
	}
	cur->count = 0;

	(cur->mutex).unlock();

	Output::OrderDeleted(cur->order_id, avail, getCurrentTimestamp());

}	