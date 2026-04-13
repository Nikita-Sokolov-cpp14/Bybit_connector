cmake_minimum_required(VERSION 3.5)

add_library(utils
    ini_reader.cpp
)

target_link_libraries(utils PRIVATE
    data_structures
    json_parser
)

target_include_directories(utils PUBLIC
    ${PROJECT_SOURCE_DIR}/
)
