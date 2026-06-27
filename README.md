# Order Book Matching Engine

Single-writer price-time limit order book. GTC limits only.

## Semantics

- Crossing `add` matches immediately; leftover quantity rests.
- Trade price is the resting order's price.
- FIFO at a price. Oldest order is `best_bid` / `best_ask`.
- `modify`: quantity down keeps priority; quantity `0` cancels; price change or quantity up is cancel + reinsert (same id) and may match.
- Duplicate id, unknown id, and non-positive price/quantity are no-ops.
- `AddResult::fills` is valid until the next `add` / `cancel` / `modify`.
- Not thread-safe. The caller serializes the hot path. No locks.

## Build

CMake (if installed):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Make:

```bash
make
```

## Test

```bash
make test
# or
./build/order_book_test
```

## Benchmark

```bash
make bench
# or
./build/order_book_benchmark
```

Reports p50 / p99 / p99.9 for add, cancel, modify, and match.

## Stack

- C++20
- Optional CMake 3.20+
- perf / Intel VTune (optional profiling)
