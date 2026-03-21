cmake_minimum_required(VERSION 3.5)

add_library(check_latency
    check_latency.cpp
)

target_link_libraries(check_latency PRIVATE
    data_structures
    json_parser
)

target_include_directories(check_latency PUBLIC
    ${PROJECT_SOURCE_DIR}/
)
