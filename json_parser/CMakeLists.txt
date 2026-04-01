cmake_minimum_required(VERSION 3.5)

add_library(json_parser
    base_json_parser.cpp
    public_trade_json_parser.cpp
    orderbook_json_parser.cpp
    status_json_parser.cpp
    position_json_parser.cpp
)

target_link_libraries(json_parser PRIVATE
    data_structures
)

target_include_directories(json_parser PUBLIC
    ${PROJECT_SOURCE_DIR}/
)
