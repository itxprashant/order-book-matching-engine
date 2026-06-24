cmake_minimum_required(VERSION 3.20)
project(order_book_matching_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(order_book
    src/order_book.cpp
)
target_include_directories(order_book PUBLIC include)

add_executable(order_book_benchmark benchmark/benchmark.cpp)
target_link_libraries(order_book_benchmark PRIVATE order_book)
