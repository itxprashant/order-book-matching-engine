#include "order_book.hpp"

namespace ob {

void OrderBook::add(const Order& /*order*/) {}

void OrderBook::cancel(uint64_t /*order_id*/) {}

void OrderBook::modify(uint64_t /*order_id*/, int64_t /*new_price*/, int64_t /*new_quantity*/) {}

std::optional<Order> OrderBook::best_bid() const { return std::nullopt; }

std::optional<Order> OrderBook::best_ask() const { return std::nullopt; }

}  // namespace ob
