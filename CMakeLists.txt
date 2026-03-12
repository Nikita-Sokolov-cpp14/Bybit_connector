cmake_minimum_required(VERSION 3.5)
project(connector)

add_subdirectory(data_loader)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    data_loader
)
