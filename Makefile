CXX ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -Iinclude
RELEASE_FLAGS ?= -O3 -DNDEBUG
DEBUG_FLAGS ?= -O0 -g

.PHONY: all test bench clean

all: build/order_book_test build/order_book_benchmark

build/order_book_test: include/order_book.hpp src/order_book.cpp tests/order_book_test.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -o $@ src/order_book.cpp tests/order_book_test.cpp

build/order_book_benchmark: include/order_book.hpp src/order_book.cpp benchmark/benchmark.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) -o $@ src/order_book.cpp benchmark/benchmark.cpp

test: build/order_book_test
	./build/order_book_test

bench: build/order_book_benchmark
	./build/order_book_benchmark

clean:
	rm -rf build
