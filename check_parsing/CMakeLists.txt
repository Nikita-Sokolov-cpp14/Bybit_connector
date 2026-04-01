cmake_minimum_required(VERSION 3.5)

add_library(check_parsing
    check_parsing.cpp
)

target_link_libraries(check_parsing PRIVATE
    data_structures
    json_parser
)

target_include_directories(check_parsing PUBLIC
    ${PROJECT_SOURCE_DIR}/
)
