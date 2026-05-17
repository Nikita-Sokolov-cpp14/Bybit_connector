cmake_minimum_required(VERSION 3.5)

add_library(trade_manager
    trade_manager.cpp
)

target_link_libraries(trade_manager PRIVATE
    data_structures
)

target_include_directories(trade_manager PUBLIC
    ${PROJECT_SOURCE_DIR}/
)
