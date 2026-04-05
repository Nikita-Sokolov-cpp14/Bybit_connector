cmake_minimum_required(VERSION 3.0.0)

find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS locale)

add_library(data_loader
    base_data_loader.cpp
    private_connector.cpp
    private_data_handler.cpp
    data_loader.cpp
    order_sender.cpp
)

target_include_directories(data_loader PUBLIC
    ${PROJECT_SOURCE_DIR}/
)

target_link_libraries(data_loader PRIVATE
    OpenSSL::SSL
    Boost::locale
    json_parser
    check_latency
)
