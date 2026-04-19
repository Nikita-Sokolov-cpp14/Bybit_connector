cmake_minimum_required(VERSION 3.5)

add_library(check_place_orders
    check_place_orders.cpp
)

target_link_libraries(check_place_orders PRIVATE
    data_structures
    websocket_client
)

target_include_directories(check_place_orders PUBLIC
    ${PROJECT_SOURCE_DIR}/
)
