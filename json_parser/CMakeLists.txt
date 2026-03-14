cmake_minimum_required(VERSION 3.5)

add_library(json_parser
    json_parser.cpp
)

target_include_directories(json_parser PUBLIC
    ${PROJECT_SOURCE_DIR}/
)
