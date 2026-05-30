cmake_minimum_required(VERSION 3.5)

add_library(loger
    loger.cpp
)

target_include_directories(loger PUBLIC
    ${PROJECT_SOURCE_DIR}/
)
