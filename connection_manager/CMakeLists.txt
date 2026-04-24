cmake_minimum_required(VERSION 3.0.0)

find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS locale)

add_library(connection_manager
    connection_manager.cpp
)

target_include_directories(connection_manager PUBLIC
    ${PROJECT_SOURCE_DIR}/
)

target_link_libraries(connection_manager PRIVATE
    websocket_client
    data_structures
    json_parser
    check_latency
    check_parsing
    check_place_orders
    utils
)
