# Order Book Matching Engine

High-performance limit order book with add, cancel, modify, and match operations.

## Features

- Cache-friendly order book layout
- Single-writer / lock-free hot path design
- Price-time priority matching
- Latency benchmark harness (p50 / p99 / p99.9)

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

```bash
./build/order_book_benchmark
```

## Stack

- C++20
- Google Benchmark
- perf / Intel VTune (optional profiling)
