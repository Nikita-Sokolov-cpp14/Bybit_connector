cmake_minimum_required(VERSION 3.5)
project(connector)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2") # для профилирования
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2") # флаг -02 повышает производительность сильно

add_subdirectory(data_loader)
add_subdirectory(data_structures)
add_subdirectory(json_parser)
add_subdirectory(p999_latency)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    data_loader
    data_structures
    json_parser
    check_latency
)
