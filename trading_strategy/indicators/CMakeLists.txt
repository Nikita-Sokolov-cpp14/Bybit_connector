cmake_minimum_required(VERSION 3.5)

add_library(indicators
    order_book_imbalance/order_book_imbalance.cpp
    trade_flow_imbalance/trade_flow_imbalance.cpp
)

target_link_libraries(indicators PRIVATE
    data_structures
    loger
)

target_include_directories(indicators PUBLIC
    ${PROJECT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
