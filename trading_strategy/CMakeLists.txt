cmake_minimum_required(VERSION 3.5)

add_subdirectory(trade_manager)

add_library(trading_strategy
    base_trading_strategy.cpp
    imbalance_and_large/imbalance_and_large.cpp
)

target_link_libraries(trading_strategy PRIVATE
    data_structures
    trade_manager
)

target_include_directories(trading_strategy PUBLIC
    ${PROJECT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/
)
