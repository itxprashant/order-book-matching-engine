#include "order_book.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,     \
                         #cond);                                             \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

ob::Order buy(uint64_t id, int64_t price, int64_t qty) {
    return {id, ob::Side::Buy, price, qty};
}

ob::Order sell(uint64_t id, int64_t price, int64_t qty) {
    return {id, ob::Side::Sell, price, qty};
}

void test_empty_book_has_no_top() {
    ob::OrderBook book;
    CHECK(!book.best_bid());
    CHECK(!book.best_ask());
}

void test_resting_buy_is_best_bid() {
    ob::OrderBook book;
    auto result = book.add(buy(1, 100, 10));
    CHECK(result.fills.empty());
    CHECK(result.remaining_qty == 10);
    auto bid = book.best_bid();
    CHECK(bid.has_value());
    CHECK(bid->id == 1);
    CHECK(bid->price == 100);
    CHECK(bid->quantity == 10);
    CHECK(bid->side == ob::Side::Buy);
    CHECK(!book.best_ask());
}

void test_resting_sell_is_best_ask() {
    ob::OrderBook book;
    auto result = book.add(sell(1, 101, 5));
    CHECK(result.fills.empty());
    CHECK(result.remaining_qty == 5);
    auto ask = book.best_ask();
    CHECK(ask.has_value());
    CHECK(ask->id == 1);
    CHECK(ask->price == 101);
    CHECK(ask->quantity == 5);
    CHECK(!book.best_bid());
}

void test_higher_bid_becomes_best() {
    ob::OrderBook book;
    book.add(buy(1, 100, 10));
    book.add(buy(2, 101, 4));
    auto bid = book.best_bid();
    CHECK(bid.has_value());
    CHECK(bid->id == 2);
    CHECK(bid->price == 101);
}

void test_lower_ask_becomes_best() {
    ob::OrderBook book;
    book.add(sell(1, 102, 10));
    book.add(sell(2, 101, 4));
    auto ask = book.best_ask();
    CHECK(ask.has_value());
    CHECK(ask->id == 2);
    CHECK(ask->price == 101);
}

void test_same_price_best_is_oldest() {
    ob::OrderBook book;
    book.add(buy(1, 100, 10));
    book.add(buy(2, 100, 7));
    auto bid = book.best_bid();
    CHECK(bid.has_value());
    CHECK(bid->id == 1);
    CHECK(bid->quantity == 10);
}

void test_non_crossing_orders_both_rest() {
    ob::OrderBook book;
    book.add(buy(1, 100, 10));
    book.add(sell(2, 101, 5));
    CHECK(book.best_bid()->id == 1);
    CHECK(book.best_ask()->id == 2);
}

void test_full_match_clears_book() {
    ob::OrderBook book;
    book.add(sell(1, 100, 10));
    auto result = book.add(buy(2, 100, 10));
    CHECK(result.fills.size() == 1);
    CHECK(result.fills[0].maker_id == 1);
    CHECK(result.fills[0].taker_id == 2);
    CHECK(result.fills[0].price == 100);
    CHECK(result.fills[0].quantity == 10);
    CHECK(result.remaining_qty == 0);
    CHECK(!book.best_bid());
    CHECK(!book.best_ask());
}

void test_trade_price_is_resting_price() {
    ob::OrderBook book;
    book.add(sell(1, 100, 10));
    auto result = book.add(buy(2, 105, 10));
    CHECK(result.fills.size() == 1);
    CHECK(result.fills[0].price == 100);
}

void test_partial_maker_stays_at_head() {
    ob::OrderBook book;
    book.add(sell(1, 100, 10));
    auto result = book.add(buy(2, 100, 4));
    CHECK(result.fills.size() == 1);
    CHECK(result.fills[0].quantity == 4);
    CHECK(result.remaining_qty == 0);
    auto ask = book.best_ask();
    CHECK(ask.has_value());
    CHECK(ask->id == 1);
    CHECK(ask->quantity == 6);
}

void test_partial_taker_remainder_rests() {
    ob::OrderBook book;
    book.add(sell(1, 100, 4));
    auto result = book.add(buy(2, 100, 10));
    CHECK(result.fills.size() == 1);
    CHECK(result.fills[0].quantity == 4);
    CHECK(result.remaining_qty == 6);
    CHECK(!book.best_ask());
    auto bid = book.best_bid();
    CHECK(bid.has_value());
    CHECK(bid->id == 2);
    CHECK(bid->quantity == 6);
    CHECK(bid->price == 100);
}

void test_fifo_at_same_price() {
    ob::OrderBook book;
    book.add(sell(1, 100, 5));
    book.add(sell(2, 100, 5));
    auto result = book.add(buy(3, 100, 5));
    CHECK(result.fills.size() == 1);
    CHECK(result.fills[0].maker_id == 1);
    CHECK(book.best_ask()->id == 2);
}

void test_multi_level_walk() {
    ob::OrderBook book;
    book.add(sell(1, 100, 5));
    book.add(sell(2, 101, 5));
    auto result = book.add(buy(3, 101, 8));
    CHECK(result.fills.size() == 2);
    CHECK(result.fills[0].maker_id == 1);
    CHECK(result.fills[0].price == 100);
    CHECK(result.fills[0].quantity == 5);
    CHECK(result.fills[1].maker_id == 2);
    CHECK(result.fills[1].price == 101);
    CHECK(result.fills[1].quantity == 3);
    CHECK(result.remaining_qty == 0);
    auto ask = book.best_ask();
    CHECK(ask.has_value());
    CHECK(ask->id == 2);
    CHECK(ask->quantity == 2);
}

void test_does_not_walk_through_non_crossing_level() {
    ob::OrderBook book;
    book.add(sell(1, 100, 5));
    book.add(sell(2, 105, 5));
    auto result = book.add(buy(3, 100, 10));
    CHECK(result.fills.size() == 1);
    CHECK(result.remaining_qty == 5);
    CHECK(book.best_ask()->id == 2);
    CHECK(book.best_bid()->id == 3);
}

void test_cancel_removes_order() {
    ob::OrderBook book;
    book.add(buy(1, 100, 10));
    book.add(buy(2, 99, 10));
    book.cancel(1);
    auto bid = book.best_bid();
    CHECK(bid.has_value());
    CHECK(bid->id == 2);
}

void test_cancel_unknown_is_noop() {
    ob::OrderBook book;
    book.add(buy(1, 100, 10));
    book.cancel(99);
    CHECK(book.best_bid()->id == 1);
}

void test_cancel_last_order_clears_level() {
    ob::OrderBook book;
    book.add(buy(1, 100, 10));
    book.cancel(1);
    CHECK(!book.best_bid());
}

void test_modify_qty_down_keeps_priority() {
    ob::OrderBook book;
    book.add(buy(1, 100, 10));
    book.add(buy(2, 100, 10));
    book.modify(1, 100, 5);
    CHECK(book.best_bid()->id == 1);
    CHECK(book.best_bid()->quantity == 5);
    auto result = book.add(sell(3, 100, 5));
    CHECK(result.fills.size() == 1);
    CHECK(result.fills[0].maker_id == 1);
    CHECK(book.best_bid()->id == 2);
}

void test_modify_qty_up_loses_priority() {
    ob::OrderBook book;
    book.add(buy(1, 100, 10));
    book.add(buy(2, 100, 10));
    book.modify(1, 100, 20);
    auto result = book.add(sell(3, 100, 10));
    CHECK(result.fills.size() == 1);
    CHECK(result.fills[0].maker_id == 2);
    CHECK(book.best_bid()->id == 1);
    CHECK(book.best_bid()->quantity == 20);
}

void test_modify_price_loses_priority_and_can_become_best() {
    ob::OrderBook book;
    book.add(buy(1, 100, 10));
    book.add(buy(2, 101, 10));
    book.modify(1, 102, 10);
    CHECK(book.best_bid()->id == 1);
    CHECK(book.best_bid()->price == 102);
}

void test_modify_that_crosses_matches() {
    ob::OrderBook book;
    book.add(sell(1, 100, 10));
    book.add(buy(2, 90, 10));
    book.modify(2, 100, 10);
    CHECK(!book.best_bid());
    CHECK(!book.best_ask());
}

void test_modify_qty_zero_cancels() {
    ob::OrderBook book;
    book.add(buy(1, 100, 10));
    book.modify(1, 100, 0);
    CHECK(!book.best_bid());
}

void test_modify_unknown_is_noop() {
    ob::OrderBook book;
    book.add(buy(1, 100, 10));
    book.modify(99, 100, 5);
    CHECK(book.best_bid()->id == 1);
    CHECK(book.best_bid()->quantity == 10);
}

void test_duplicate_id_rejected() {
    ob::OrderBook book;
    book.add(buy(1, 100, 10));
    auto result = book.add(buy(1, 101, 5));
    CHECK(result.fills.empty());
    CHECK(result.remaining_qty == 0);
    CHECK(book.best_bid()->id == 1);
    CHECK(book.best_bid()->price == 100);
    CHECK(book.best_bid()->quantity == 10);
}

void test_rejects_non_positive_price_and_qty() {
    ob::OrderBook book;
    CHECK(book.add(buy(1, 0, 10)).remaining_qty == 0);
    CHECK(book.add(buy(2, -1, 10)).remaining_qty == 0);
    CHECK(book.add(buy(3, 100, 0)).remaining_qty == 0);
    CHECK(book.add(buy(4, 100, -5)).remaining_qty == 0);
    CHECK(!book.best_bid());
}

void test_sell_into_bids_matches() {
    ob::OrderBook book;
    book.add(buy(1, 100, 10));
    auto result = book.add(sell(2, 99, 4));
    CHECK(result.fills.size() == 1);
    CHECK(result.fills[0].maker_id == 1);
    CHECK(result.fills[0].taker_id == 2);
    CHECK(result.fills[0].price == 100);
    CHECK(result.fills[0].quantity == 4);
    CHECK(book.best_bid()->quantity == 6);
}

void test_fill_span_invalidated_on_next_add() {
    ob::OrderBook book;
    book.add(sell(1, 100, 10));
    auto first = book.add(buy(2, 100, 3));
    CHECK(first.fills.size() == 1);
    book.add(buy(3, 90, 1));
    // next mutate clears the buffer; this call only checks we can still add
    CHECK(book.best_bid()->id == 3);
    CHECK(book.best_ask()->quantity == 7);
}

}  // namespace

int main() {
    test_empty_book_has_no_top();
    test_resting_buy_is_best_bid();
    test_resting_sell_is_best_ask();
    test_higher_bid_becomes_best();
    test_lower_ask_becomes_best();
    test_same_price_best_is_oldest();
    test_non_crossing_orders_both_rest();
    test_full_match_clears_book();
    test_trade_price_is_resting_price();
    test_partial_maker_stays_at_head();
    test_partial_taker_remainder_rests();
    test_fifo_at_same_price();
    test_multi_level_walk();
    test_does_not_walk_through_non_crossing_level();
    test_cancel_removes_order();
    test_cancel_unknown_is_noop();
    test_cancel_last_order_clears_level();
    test_modify_qty_down_keeps_priority();
    test_modify_qty_up_loses_priority();
    test_modify_price_loses_priority_and_can_become_best();
    test_modify_that_crosses_matches();
    test_modify_qty_zero_cancels();
    test_modify_unknown_is_noop();
    test_duplicate_id_rejected();
    test_rejects_non_positive_price_and_qty();
    test_sell_into_bids_matches();
    test_fill_span_invalidated_on_next_add();

    if (failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all tests passed\n");
    return 0;
}
