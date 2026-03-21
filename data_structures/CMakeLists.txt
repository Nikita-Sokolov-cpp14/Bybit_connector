cmake_minimum_required(VERSION 3.0.0)

add_library(data_structures
    orderbook.cpp
    public_trade.cpp
    status_message.cpp
)

target_include_directories(data_structures PUBLIC
    ${PROJECT_SOURCE_DIR}/
)
