cmake_minimum_required(VERSION 3.5)

add_library(indicators
    imbalance/imbalance_indicator.cpp
    trade_flow/trade_flow.cpp
)

target_link_libraries(indicators PRIVATE
    data_structures
    loger
)

target_include_directories(indicators PUBLIC
    ${PROJECT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
