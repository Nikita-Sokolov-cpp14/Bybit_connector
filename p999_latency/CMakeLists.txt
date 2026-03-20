cmake_minimum_required(VERSION 3.5)

add_library(check_latency
    check_latency.cpp
)

target_link_libraries(check_latency PRIVATE
    order_book
    json_parser
)

target_include_directories(check_latency PUBLIC
    ${PROJECT_SOURCE_DIR}/
)
