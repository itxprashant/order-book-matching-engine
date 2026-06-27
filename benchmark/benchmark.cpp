#include "order_book.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Stats {
    double p50;
    double p99;
    double p999;
    double mean;
};

Stats summarize(std::vector<double>& samples) {
    std::sort(samples.begin(), samples.end());
    auto percentile = [&](double p) {
        const std::size_t i =
            static_cast<std::size_t>(p * static_cast<double>(samples.size() - 1));
        return samples[i];
    };
    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    return {percentile(0.50), percentile(0.99), percentile(0.999),
            sum / static_cast<double>(samples.size())};
}

void print_stats(const char* name, const Stats& stats) {
    std::printf("%-14s  p50=%7.1f ns  p99=%7.1f ns  p99.9=%7.1f ns  mean=%7.1f ns\n",
                name, stats.p50, stats.p99, stats.p999, stats.mean);
}

template <typename Setup, typename Op, typename Teardown>
Stats measure(int warmup, int iters, Setup&& setup, Op&& op,
              Teardown&& teardown) {
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(iters));
    for (int i = 0; i < warmup + iters; ++i) {
        setup();
        const auto t0 = Clock::now();
        op();
        const auto t1 = Clock::now();
        teardown();
        if (i >= warmup) {
            samples.push_back(
                std::chrono::duration<double, std::nano>(t1 - t0).count());
        }
    }
    return summarize(samples);
}

}  // namespace

int main() {
    constexpr int kWarmup = 20000;
    constexpr int kIters = 200000;

    std::printf("samples=%d warmup=%d\n", kIters, kWarmup);

    {
        ob::OrderBook book;
        for (int i = 0; i < 1024; ++i) {
            book.add(ob::Order{static_cast<uint64_t>(i + 1), ob::Side::Buy, 100, 1});
        }
        uint64_t id = 10'000;
        uint64_t cur = 0;
        const Stats stats = measure(
            kWarmup, kIters,
            [&] { cur = id++; },
            [&] { book.add(ob::Order{cur, ob::Side::Buy, 90, 1}); },
            [&] { book.cancel(cur); });
        print_stats("add_no_match", stats);
    }

    {
        ob::OrderBook book;
        uint64_t id = 1;
        uint64_t cur = 0;
        const Stats stats = measure(
            kWarmup, kIters,
            [&] {
                cur = id++;
                book.add(ob::Order{cur, ob::Side::Sell, 110, 1});
            },
            [&] { book.cancel(cur); },
            [&] {});
        print_stats("cancel", stats);
    }

    {
        ob::OrderBook book;
        uint64_t id = 1;
        uint64_t cur = 0;
        const Stats stats = measure(
            kWarmup, kIters,
            [&] {
                cur = id++;
                book.add(ob::Order{cur, ob::Side::Buy, 100, 10});
            },
            [&] { book.modify(cur, 100, 4); },
            [&] { book.cancel(cur); });
        print_stats("modify_qty", stats);
    }

    {
        ob::OrderBook book;
        uint64_t maker = 1;
        uint64_t taker = 1'000'000;
        uint64_t live_maker = maker;
        book.add(ob::Order{live_maker, ob::Side::Sell, 100, 1});
        const Stats stats = measure(
            kWarmup, kIters,
            [&] {},
            [&] { book.add(ob::Order{taker++, ob::Side::Buy, 100, 1}); },
            [&] {
                live_maker = ++maker;
                book.add(ob::Order{live_maker, ob::Side::Sell, 100, 1});
            });
        print_stats("match", stats);
    }

    {
        ob::OrderBook book;
        uint64_t id = 1;
        for (int i = 0; i < 64; ++i) {
            book.add(ob::Order{id++, ob::Side::Sell, 100 + (i % 8), 2});
        }
        const Stats stats = measure(
            kWarmup, kIters,
            [&] {},
            [&] { book.add(ob::Order{id++, ob::Side::Buy, 107, 5}); },
            [&] { book.add(ob::Order{id++, ob::Side::Sell, 100, 5}); });
        print_stats("match_multi", stats);
    }

    return 0;
}
