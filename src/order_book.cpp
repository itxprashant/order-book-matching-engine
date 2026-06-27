#include "order_book.hpp"

#include <functional>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ob {
namespace {

constexpr std::size_t kSlabSize = 1024;

struct PriceLevel;

struct OrderNode {
    uint64_t id = 0;
    Side side = Side::Buy;
    int64_t price = 0;
    int64_t quantity = 0;
    OrderNode* prev = nullptr;
    OrderNode* next = nullptr;
    PriceLevel* level = nullptr;
};

struct PriceLevel {
    int64_t price = 0;
    int64_t total_qty = 0;
    std::size_t count = 0;
    OrderNode* head = nullptr;
    OrderNode* tail = nullptr;
    PriceLevel* free_next = nullptr;
};

}  // namespace

struct OrderBook::Impl {
    std::vector<std::unique_ptr<OrderNode[]>> order_slabs;
    std::vector<std::unique_ptr<PriceLevel[]>> level_slabs;
    OrderNode* free_orders = nullptr;
    PriceLevel* free_levels = nullptr;

    std::map<int64_t, PriceLevel*, std::greater<int64_t>> bids;
    std::map<int64_t, PriceLevel*> asks;
    std::unordered_map<uint64_t, OrderNode*> by_id;
    std::vector<Fill> fills;

    void grow_orders() {
        auto slab = std::make_unique<OrderNode[]>(kSlabSize);
        for (std::size_t i = 0; i < kSlabSize; ++i) {
            slab[i].next = free_orders;
            free_orders = &slab[i];
        }
        order_slabs.push_back(std::move(slab));
    }

    void grow_levels() {
        auto slab = std::make_unique<PriceLevel[]>(kSlabSize);
        for (std::size_t i = 0; i < kSlabSize; ++i) {
            slab[i].free_next = free_levels;
            free_levels = &slab[i];
        }
        level_slabs.push_back(std::move(slab));
    }

    OrderNode* alloc_order() {
        if (free_orders == nullptr) {
            grow_orders();
        }
        OrderNode* node = free_orders;
        free_orders = node->next;
        node->prev = nullptr;
        node->next = nullptr;
        node->level = nullptr;
        return node;
    }

    void free_order(OrderNode* node) {
        node->prev = nullptr;
        node->next = free_orders;
        node->level = nullptr;
        free_orders = node;
    }

    PriceLevel* alloc_level(int64_t price) {
        if (free_levels == nullptr) {
            grow_levels();
        }
        PriceLevel* level = free_levels;
        free_levels = level->free_next;
        level->price = price;
        level->total_qty = 0;
        level->count = 0;
        level->head = nullptr;
        level->tail = nullptr;
        level->free_next = nullptr;
        return level;
    }

    void free_level(PriceLevel* level) {
        level->head = nullptr;
        level->tail = nullptr;
        level->free_next = free_levels;
        free_levels = level;
    }

    PriceLevel* get_or_create_level(Side side, int64_t price) {
        if (side == Side::Buy) {
            auto it = bids.find(price);
            if (it != bids.end()) {
                return it->second;
            }
            PriceLevel* level = alloc_level(price);
            bids.emplace(price, level);
            return level;
        }
        auto it = asks.find(price);
        if (it != asks.end()) {
            return it->second;
        }
        PriceLevel* level = alloc_level(price);
        asks.emplace(price, level);
        return level;
    }

    void unlink(OrderNode* node) {
        PriceLevel* level = node->level;
        if (node->prev != nullptr) {
            node->prev->next = node->next;
        } else {
            level->head = node->next;
        }
        if (node->next != nullptr) {
            node->next->prev = node->prev;
        } else {
            level->tail = node->prev;
        }
        node->prev = nullptr;
        node->next = nullptr;
        level->count -= 1;
        level->total_qty -= node->quantity;
    }

    void drop_level_if_empty(PriceLevel* level, Side side) {
        if (level->count != 0) {
            return;
        }
        if (side == Side::Buy) {
            bids.erase(level->price);
        } else {
            asks.erase(level->price);
        }
        free_level(level);
    }

    void remove_node(OrderNode* node) {
        PriceLevel* level = node->level;
        const Side side = node->side;
        by_id.erase(node->id);
        unlink(node);
        free_order(node);
        drop_level_if_empty(level, side);
    }

    void append(PriceLevel* level, OrderNode* node) {
        node->level = level;
        node->prev = level->tail;
        node->next = nullptr;
        if (level->tail != nullptr) {
            level->tail->next = node;
        } else {
            level->head = node;
        }
        level->tail = node;
        level->count += 1;
        level->total_qty += node->quantity;
    }

    void rest(const Order& order, int64_t quantity) {
        OrderNode* node = alloc_order();
        node->id = order.id;
        node->side = order.side;
        node->price = order.price;
        node->quantity = quantity;
        PriceLevel* level = get_or_create_level(order.side, order.price);
        append(level, node);
        by_id.emplace(order.id, node);
    }

    void fill_level(PriceLevel* level, uint64_t taker_id, int64_t& remaining) {
        while (remaining > 0 && level->head != nullptr) {
            OrderNode* maker = level->head;
            const int64_t qty =
                remaining < maker->quantity ? remaining : maker->quantity;
            fills.push_back(Fill{maker->id, taker_id, maker->price, qty});
            maker->quantity -= qty;
            level->total_qty -= qty;
            remaining -= qty;
            if (maker->quantity == 0) {
                const bool last = level->count == 1;
                remove_node(maker);
                if (last) {
                    return;
                }
            }
        }
    }

    int64_t match_buy(uint64_t taker_id, int64_t limit_price, int64_t remaining) {
        while (remaining > 0 && !asks.empty()) {
            auto it = asks.begin();
            if (it->first > limit_price) {
                break;
            }
            PriceLevel* level = it->second;
            fill_level(level, taker_id, remaining);
        }
        return remaining;
    }

    int64_t match_sell(uint64_t taker_id, int64_t limit_price, int64_t remaining) {
        while (remaining > 0 && !bids.empty()) {
            auto it = bids.begin();
            if (it->first < limit_price) {
                break;
            }
            PriceLevel* level = it->second;
            fill_level(level, taker_id, remaining);
        }
        return remaining;
    }

    AddResult add(const Order& order) {
        fills.clear();
        if (order.quantity <= 0 || order.price <= 0 ||
            by_id.find(order.id) != by_id.end()) {
            return {std::span<const Fill>(fills), 0};
        }

        int64_t remaining = order.quantity;
        if (order.side == Side::Buy) {
            remaining = match_buy(order.id, order.price, remaining);
        } else {
            remaining = match_sell(order.id, order.price, remaining);
        }
        if (remaining > 0) {
            rest(order, remaining);
        }
        return {std::span<const Fill>(fills), remaining};
    }

    void cancel(uint64_t order_id) {
        auto it = by_id.find(order_id);
        if (it == by_id.end()) {
            return;
        }
        remove_node(it->second);
    }

    void modify(uint64_t order_id, int64_t new_price, int64_t new_quantity) {
        auto it = by_id.find(order_id);
        if (it == by_id.end()) {
            return;
        }
        if (new_quantity <= 0) {
            remove_node(it->second);
            return;
        }
        if (new_price <= 0) {
            return;
        }

        OrderNode* node = it->second;
        if (new_price == node->price && new_quantity == node->quantity) {
            return;
        }
        if (new_price == node->price && new_quantity < node->quantity) {
            const int64_t delta = node->quantity - new_quantity;
            node->quantity = new_quantity;
            node->level->total_qty -= delta;
            return;
        }

        const Order replacement{node->id, node->side, new_price, new_quantity};
        remove_node(node);
        add(replacement);
    }

    std::optional<Order> best(const PriceLevel* level) const {
        if (level == nullptr || level->head == nullptr) {
            return std::nullopt;
        }
        const OrderNode* node = level->head;
        return Order{node->id, node->side, node->price, node->quantity};
    }
};

OrderBook::OrderBook() : impl_(std::make_unique<Impl>()) {}

OrderBook::~OrderBook() = default;

AddResult OrderBook::add(const Order& order) { return impl_->add(order); }

void OrderBook::cancel(uint64_t order_id) { impl_->cancel(order_id); }

void OrderBook::modify(uint64_t order_id, int64_t new_price,
                       int64_t new_quantity) {
    impl_->modify(order_id, new_price, new_quantity);
}

std::optional<Order> OrderBook::best_bid() const {
    if (impl_->bids.empty()) {
        return std::nullopt;
    }
    return impl_->best(impl_->bids.begin()->second);
}

std::optional<Order> OrderBook::best_ask() const {
    if (impl_->asks.empty()) {
        return std::nullopt;
    }
    return impl_->best(impl_->asks.begin()->second);
}

}  // namespace ob
