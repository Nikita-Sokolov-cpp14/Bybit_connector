cmake_minimum_required(VERSION 3.0.0)

find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS locale)

add_library(websocket_client
    base_websocket_client.cpp
    private_connector.cpp
    private_data_handler.cpp
    public_data_handler.cpp
    order_sender.cpp
)

target_include_directories(websocket_client PUBLIC
    ${PROJECT_SOURCE_DIR}/
)

target_link_libraries(websocket_client PRIVATE
    json_parser
    check_latency
)

# Для кросскомпиляции Windows добавляем системные библиотеки
if(CMAKE_CROSSCOMPILING)
    target_link_libraries(websocket_client PRIVATE
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        ws2_32
        wsock32
        crypt32
    )
else()
    target_link_libraries(websocket_client PRIVATE
        OpenSSL::SSL
        Boost::locale
    )
endif()
