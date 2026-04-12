cmake_minimum_required(VERSION 3.5)
project(connector)

# Указываем C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2") # для профилирования
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2") # флаг -02 повышает производительность сильно

add_subdirectory(data_loader)
add_subdirectory(data_structures)
add_subdirectory(json_parser)
add_subdirectory(p999_latency)
add_subdirectory(check_parsing)
add_subdirectory(check_place_orders)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    data_loader
    data_structures
    json_parser
    check_latency
    check_parsing
    check_place_orders
)
