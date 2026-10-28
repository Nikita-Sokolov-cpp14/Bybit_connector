cmake_minimum_required(VERSION 3.5)

add_executable(check_place_orders
    main.cpp
    check_place_orders.cpp
)

target_link_libraries(check_place_orders PRIVATE
    connection_manager
    trading_strategy
    data_structures
    websocket_client
    json_parser
    utils
    loger
)

target_include_directories(check_place_orders PRIVATE
    ${PROJECT_SOURCE_DIR}/
)
