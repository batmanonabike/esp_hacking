cmake_minimum_required(VERSION 3.16)
project(QtBleGattServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Bluetooth)

add_executable(QtBleGattServer
    main.cpp
    gatt_server.h
    gatt_server.cpp
)

target_link_libraries(QtBleGattServer PRIVATE
    Qt6::Core
    Qt6::Bluetooth
)

# Define a C++ macro for the custom service UUID
target_compile_definitions(QtBleGattServer PRIVATE 
    CUSTOM_SERVICE_UUID="DA782B1A-F2F0-4C88-B47A-E93F3A55A9C5"
)

install(TARGETS QtBleGattServer
    RUNTIME DESTINATION bin
)
