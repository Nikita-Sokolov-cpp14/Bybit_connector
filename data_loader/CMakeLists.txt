cmake_minimum_required(VERSION 3.0.0)

find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS locale)

add_library(data_loader
    data_loader.cpp
)

target_include_directories(data_loader PUBLIC
    ${PROJECT_SOURCE_DIR}/
)

target_link_libraries(data_loader PRIVATE
    OpenSSL::SSL
    Boost::locale
    json_parser
)
