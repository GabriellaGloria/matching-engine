import random

# Execute with: python3 test_generator.py

def create_test(file_path, num_queries, num_instruments, num_clients):
    order_counter = 0
    query_options = ['B', 'S', 'C']
    
    with open(file_path, 'w') as file:
        client_orders = [[] for _ in range(num_clients)]

        print(num_clients, file=file)
        print('o', file=file)

        for _ in range(num_queries):
            client_id = random.randint(0, num_clients - 1)
            query_type = random.choice(query_options)

            if query_type == 'C':
                if not client_orders[client_id]:  
                    query_type = random.choice(['B', 'S'])
                    continue
                selected_order = random.choice(client_orders[client_id])
                print(client_id, query_type, selected_order, file=file)

            if query_type in {'B', 'S'}:
                instrument_id = random.randint(0, num_instruments - 1)
                order_id = order_counter
                client_orders[client_id].append(order_id)
                order_counter += 1
                bid_price = random.randint(1, 9999)
                quantity = random.randint(1, 99)
                print(client_id, query_type, order_id, instrument_id, bid_price, quantity, file=file)

        print('.', file=file)
        print('x', file=file)

create_test('tests/1.in', 100, 10, 5)
create_test('tests/2.in', 500, 50, 10)
create_test('tests/3.in', 30000, 250, 20)
create_test('tests/4.in', 1000000, 500, 40)
