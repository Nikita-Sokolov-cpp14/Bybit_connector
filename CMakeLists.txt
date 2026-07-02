cmake_minimum_required(VERSION 3.5)
project(connector)

# Указываем C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2") # для профилирования
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2") # флаг -02 повышает производительность сильно

if(CMAKE_CROSSCOMPILING)
    # Пути к вашим библиотекам
    set(BOOST_ROOT "/home/sokolovna/HFT/win_libraries/boost/install_dir")
    set(OPENSSL_ROOT_DIR "/home/sokolovna/HFT/win_libraries/openssl/install_dir")

    # Добавляем пути к заголовкам
    include_directories(${BOOST_ROOT}/include)
    include_directories(${OPENSSL_ROOT_DIR}/include)

    # Добавляем пути к библиотекам
    link_directories(${BOOST_ROOT}/lib)
    link_directories(${OPENSSL_ROOT_DIR}/lib64)

    # Системные библиотеки Windows, необходимые для Boost.Asio и сетевых операций
    set(WIN_SYSTEM_LIBRARIES
        ws2_32      # Winsock 2.2 (WSAStartup, WSACleanup, WSARecv, WSASend и др.)
        wsock32     # Winsock 1.1 (для обратной совместимости)
        crypt32     # Криптографические функции (может понадобиться OpenSSL)
        user32      # Пользовательский интерфейс (может понадобиться)
        gdi32       # Графика (иногда требуется)
    )

    # Поиск библиотек
    find_package(Boost REQUIRED COMPONENTS locale)
    find_package(OpenSSL REQUIRED)

    # Если find_package не сработал, используем ручные пути
    if(NOT Boost_FOUND)
        message(STATUS "Boost not found by find_package, using manual path")
        set(Boost_INCLUDE_DIRS "${BOOST_ROOT}/include")
        set(Boost_LIBRARY_DIRS "${BOOST_ROOT}/lib")
        set(Boost_LIBRARIES "${BOOST_ROOT}/lib/libboost_locale.a")
        set(Boost_FOUND TRUE)
    endif()

    if(NOT OPENSSL_FOUND)
        message(STATUS "OpenSSL not found by find_package, using manual path")
        set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
        set(OPENSSL_LIBRARIES
            "${OPENSSL_ROOT_DIR}/lib64/libssl.a"
            "${OPENSSL_ROOT_DIR}/lib64/libcrypto.a"
        )
        set(OPENSSL_FOUND TRUE)
    endif()
endif()

set(CMAKE_PREFIX_PATH
    ${BOOST_ROOT}
    ${OPENSSL_ROOT_DIR}
)

add_subdirectory(websocket_client)
add_subdirectory(data_structures)
add_subdirectory(json_parser)
add_subdirectory(p999_latency)
add_subdirectory(check_parsing)
add_subdirectory(connection_manager)
add_subdirectory(check_place_orders)
add_subdirectory(utils)
add_subdirectory(trading_strategy)
add_subdirectory(loger)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    websocket_client
    data_structures
    json_parser
    check_latency
    check_parsing
    check_place_orders
    utils
    trading_strategy
    connection_manager
    loger
)

if(CMAKE_CROSSCOMPILING)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
        ${WIN_SYSTEM_LIBRARIES}
    )

    # Флаги для статической линковки MinGW
    target_link_options(${PROJECT_NAME} PRIVATE
        -static-libgcc
        -static-libstdc++
        -static
    )
endif()
