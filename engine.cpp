#include <iostream>
#include <thread>

#include "io.hpp"
#include "engine.hpp"

void OrderBook::addOrder(Order &order)
{
    std::multiset<Order> &oppositeBook = (order.type == input_buy) ? sellOrders : buyOrders;

    // SyncCerr{} << "Active Order ID: " << order.order_id << std::endl;
    // before adding the order to the order book, check if the active order matches with any resting order
    matchOrder(order, oppositeBook);

    // if the active order is not fully executed, add it to the respective order book (converts active to resting order)
    if (order.quantity > 0)
    {
        auto &targetBook = (order.type == input_buy) ? buyOrders : sellOrders;
        auto iter = targetBook.insert(order);
        orderMap[order.order_id] = iter;
        // SyncCerr{}
        //     << "Adding order: " << static_cast<char>(order.type) << " " << order.instrument << " x " << order.quantity << " @ "
        //     << order.price << " ID: " << order.order_id << std::endl;
        Output::OrderAdded(order.order_id, order.instrument.c_str(), order.price, order.quantity, order.type == input_sell, getCurrentTimestamp());

        // create execution ID for this new order
        executionIds[order.order_id] = 0;
    }
}

void OrderBook::matchOrder(Order &activeOrder, std::multiset<Order> &oppositeBook)
{

    for (auto it = oppositeBook.begin(); it != oppositeBook.end() && activeOrder.quantity > 0;)
    {
        if (it->instrument == activeOrder.instrument &&
            ((activeOrder.type == input_buy && activeOrder.price >= it->price) ||
             (activeOrder.type == input_sell && activeOrder.price <= it->price)))
        {
            int executedQuantity = std::min(activeOrder.quantity, it->quantity);
            int executedPrice = it->price; // Execute at the price of the resting order

            // SyncCerr{}
            //     << "Order " << activeOrder.order_id << " matched with order " << it->order_id << " for " << executedQuantity << " at " << executedPrice << std::endl;

            // SyncCerr{}
            //     << "Order " << it->order_id << " " << it->instrument << " x " << it->quantity << " @ " << it->price << std::endl;

            // increment execution ID for the resting order
            int executionId = ++executionIds[it->order_id];

            Output::OrderExecuted(it->order_id, activeOrder.order_id, executionId, executedPrice, executedQuantity, getCurrentTimestamp());

            activeOrder.quantity -= executedQuantity;

            // if the order is partially filled
            if (it->quantity - executedQuantity > 0)
            {
                // update the order in the opposite book
                Order updatedOrder = *it;
                updatedOrder.quantity -= executedQuantity;
                oppositeBook.erase(it);
                auto updatedIt = oppositeBook.insert(updatedOrder);
                // update the iterator in the orderMap to the new location
                orderMap[updatedOrder.order_id] = updatedIt;
                ++it;
            }
            else
            {
                // if the order is completely filled, remove the order
                int orderIdToDelete = it->order_id;
                // note: increment iterator before deletion to avoid invalidation
                it++;
                deleteOrder(orderIdToDelete);
            }
        }
        else
        {
            ++it;
        }
    }
}

bool OrderBook::deleteOrder(int orderId)
{
    auto it = orderMap.find(orderId);
    if (it != orderMap.end())
    {
        auto &book = (it->second->type == input_buy) ? buyOrders : sellOrders;
        book.erase(it->second);
        orderMap.erase(it);
        executionIds.erase(orderId);
        return true;
    }
    return false;
}

void Engine::accept(ClientConnection connection)
{
    auto thread = std::thread(&Engine::connection_thread, this, std::move(connection));
    thread.detach();
}

void Engine::processCommand(const ClientCommand &input)
{
    switch (input.type)
    {
    case input_cancel:
    {
        bool deleted = orderBook.deleteOrder(input.order_id);
        Output::OrderDeleted(input.order_id, deleted, getCurrentTimestamp());
        break;
    }

    default:
    {
        // SyncCerr{}
        //     << "Got order: " << static_cast<char>(input.type) << " " << input.instrument << " x " << input.count << " @ "
        //     << input.price << " ID: " << input.order_id << std::endl;

        OrderBook::Order order(input.order_id, input.instrument, input.price, input.count, input.type, getCurrentTimestamp());

        // SyncCerr{} << "Order created! " << std::endl;

        orderBook.addOrder(order);

        // SyncCerr{} << "Order added! " << std::endl;
        break;
    }
    }
}


void Engine::connection_thread(ClientConnection connection)
{
    while (true)
    {
        ClientCommand input{};
        switch (connection.readInput(input))
        {
        case ReadResult::Error:
            SyncCerr{} << "Error reading input" << std::endl;
        case ReadResult::EndOfFile:
            return;
        case ReadResult::Success:
        {
            // lock the orderMutexes map as it needs to be modified when adding a new mutex which happens every time a new order_id is seen
            std::lock_guard<std::mutex> mapLock(orderMutexesMutex);
            std::lock_guard<std::mutex> lock(orderMutexes[input.order_id]);
            // SyncCerr{} << "Instrument: " << input.instrument << std::endl;
            // SyncCerr{} << "Order ID: " << input.order_id << std::endl;
            // SyncCerr{} << "Size of orderMutexes: " << orderMutexes.size() << std::endl;
            processCommand(input);
        }
        break;
        }
    }
}
