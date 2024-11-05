#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <list>
#include <string>
#include <unordered_map>
#include <set>
#include <iostream>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <map>
#include "io.hpp"

inline std::chrono::microseconds::rep getCurrentTimestamp() noexcept
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

class OrderBook
{
public:
    struct Order
    {
        int order_id;
        std::string instrument;
        int price;
        int quantity;
        CommandType type;
        std::chrono::nanoseconds::rep timestamp;

        Order(int id, const std::string &inst, int p, int qty, CommandType t, std::chrono::nanoseconds::rep ts)
            : order_id(id), instrument(inst), price(p), quantity(qty), type(t), timestamp(ts) {}

        bool operator<(const Order &other) const
        {
            if (price == other.price)
            {
                return timestamp < other.timestamp;
            }
            else if (type == input_sell)
            {
                return price < other.price;
            }
            else
            {
                return price > other.price;
            }
        }
    };

    void addOrder(Order &order);
    void matchOrder(Order &activeOrder, std::multiset<Order> &oppositeBook);
    bool deleteOrder(int orderId);

private:
    std::multiset<Order> buyOrders, sellOrders;
    std::map<int, std::multiset<Order>::iterator> orderMap;
    std::map<int, int> executionIds;
};

struct Engine
{
public:
    void accept(ClientConnection conn);

private:
    void processCommand(const ClientCommand &input);
    void connection_thread(ClientConnection conn);
    OrderBook orderBook;
    std::mutex orderMutexesMutex;
    std::map<uint32_t, std::mutex> orderMutexes;
};

#endif // ENGINE_HPP