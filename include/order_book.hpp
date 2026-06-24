#pragma once

#include <cstdint>
#include <optional>

namespace ob {

enum class Side : uint8_t { Buy, Sell };

struct Order {
    uint64_t id;
    Side side;
    int64_t price;
    int64_t quantity;
};

class OrderBook {
public:
    void add(const Order& order);
    void cancel(uint64_t order_id);
    void modify(uint64_t order_id, int64_t new_price, int64_t new_quantity);

    std::optional<Order> best_bid() const;
    std::optional<Order> best_ask() const;

private:
};

}  // namespace ob
