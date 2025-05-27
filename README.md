# matching-engine
Multithreaded C++ matching engine for processing orders with price-time priority and safe concurrent access to order books.
This project is adapted from coursework for CS3211: Parallel and Concurrent Programming.


### Structure and Object diagram
<p align="center">
<img width="450" alt="image" src="https://github.com/user-attachments/assets/bd7ad063-215a-4de1-996b-93bd686e9ee9" />
</p>

### 1.	Data Structures Used :<br>
The Engine class uses various data structures to manage orders and facilitate the matching process for buy and sell orders.<br>
#### •	Safe_Map: <br>
A map structure that uses std::shared_mutex and std::unordered_map to provide thread-safe access to its elements. The map is used for storing key-value and ensuring concurrency when reading and writing to the map. Safe_Map allows multiple threads to read from the map simultaneously (with std::shared_lock) while ensuring exclusive access when writing (with std::unique_lock).<br>
#### •	Safe_Set: <br>
A set structure that uses std::shared_mutex and std::set to provide thread-safe access to its elements. Safe_Set ensures that elements can be inserted and accessed safely across multiple threads. Safe_Set enables use of custom comparators.<br>
#### •	Order, BuyOrder , and SellOrder: <br>
These represent buy and sell orders. They store information about the order ID, price, quantity, timestamp, and the type of order (buy or
sell). These orders are stored in Safe_Set, with custom sort comparators. We also utilize
Safe_Map to map different symbol’s Safe_Set.<br>
<br>

### 2.	Synchronization Primitives : <br>
**•	Mutexes:**
The program uses multiple types of mutexes to ensure proper synchronisation and thread safety.<br><br>
  &nbsp;&nbsp;&nbsp;&nbsp;o	**std::mutex:** This is used in every Order to ensure mutual exclusion when modifying order information such as order count, since any thread can access any order.<br>
  &nbsp;&nbsp;&nbsp;&nbsp;o	**std::shared_mutex:** Used in Safe_Map and Safe_Set to provide shared access to the orders while allowing exclusive access during insertions. This is used with std::shared_lock and std::unique_lock for readers and writer respectively.<br>
  &nbsp;&nbsp;&nbsp;&nbsp;o	**CounterMutex:** This is a custom mutex that maintains a counter to lock and unlock the associated mutex in a more granular manner. It ensures that the lock is only acquired when the counter reaches one, and released when it reaches zero.<br>
  &nbsp;&nbsp;&nbsp;&nbsp;o	**BuySellMutex:** This is custom lock that uses CounterMutex for both buy and sell locks to allow concurrent access to buy and sell operations. BuySellMutex ensures single-lane bridge synchronization mechanism, since only multiple buys or multiple sells at a time.<br>
  <br>
### 3.	Level Of Concurrency :
#### •	Phase-level concurrency :<br>
With the use of BuySellMutex for each symbol, orders of same side and symbol can execute concurrently (single-lane-bridge problem). BuySellMutex ensures multiple buys / multiple sells can execute at the same time. Every order will only traverse the Safe_Set of other side as a reader and not changing the set (e.g buy orders traverses sell orders set, and not writing to it), hence multiple orders of same side can happen concurrently.<br>
<br>
Cancel orders can also execute concurrently for each symbol with other orders (buy/sell/cancel), since order cancel requests can only come from the client that originally sent the order, so no other thread can cancel same id at the same time. Cancel order will use the order’s own mutex, hence multiple cancel orders from same symbol can be done concurrently.
<br><br>
#### •	Instrument-level concurrency :<br>
Orders of different symbol can execute independently of other symbols, since each has its own order set and mutexes. Hence, the execution can be done concurrently without waiting for other symbols.<br>
 <br>
### 4.	Testing Methodology :<br>
Our testing methodology involves generating test cases with random buy, sell, and cancel operations. We generate test cases from scripts/test-generator.py python script. The tests are designed to simulate multiple scenarios, ranging from small testcases, to huge testcases with up to 40 clients. We use grader to test our correctness with these testcases.<br>

#### Testing with sanitizers :<br>
Using  the Address  sanitizer  (-fsanitize=address)  flag  did  not  show  any  errors <br>
<p align="center">
<img width="900" alt="image" src="https://github.com/user-attachments/assets/7a06d278-88f2-4488-978d-1c70d8dfeb0d" />
</p>

Using the Thread sanitizer (-fsanitize=thread) flag did not show any errors <br>
<p align="center">
<img width="900" alt="image" src="https://github.com/user-attachments/assets/c02ef3b0-5de6-46d9-8431-a31faaa3e7c3" />
</p>

