cmake_minimum_required(VERSION 3.0.0)

add_library(data_structures
    orderbook.cpp
    public_trade.cpp
    status_message.cpp
    position.cpp
    execution_fast.cpp
    order.cpp
    wallet.cpp
    order_request.cpp
    order_operation.cpp
)

target_include_directories(data_structures PUBLIC
    ${PROJECT_SOURCE_DIR}/
)
