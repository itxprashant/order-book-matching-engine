#include "order_book.hpp"

#include <cstdio>

int main() {
    ob::OrderBook book;
    book.add(ob::Order{1, ob::Side::Buy, 100, 10});
    book.add(ob::Order{2, ob::Side::Sell, 101, 5});

    if (auto bid = book.best_bid()) {
        std::printf("best bid: id=%llu price=%lld qty=%lld\n",
                    static_cast<unsigned long long>(bid->id),
                    static_cast<long long>(bid->price),
                    static_cast<long long>(bid->quantity));
    }

    return 0;
}
