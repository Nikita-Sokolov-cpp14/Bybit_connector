cmake_minimum_required(VERSION 3.0.0)

add_library(order_book
    order_book.cpp
)

target_include_directories(order_book PUBLIC
    ${PROJECT_SOURCE_DIR}/
)
