cmake_minimum_required(VERSION 3.20)
project(order_book_matching_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_library(order_book
    src/order_book.cpp
)
target_include_directories(order_book PUBLIC include)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(order_book PRIVATE -Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(order_book PRIVATE -O3)
    endif()
endif()

add_executable(order_book_test tests/order_book_test.cpp)
target_link_libraries(order_book_test PRIVATE order_book)

add_executable(order_book_benchmark benchmark/benchmark.cpp)
target_link_libraries(order_book_benchmark PRIVATE order_book)

enable_testing()
add_test(NAME order_book COMMAND order_book_test)
