cmake_minimum_required(VERSION 3.5)

# Set the EXTRA_COMPONENT_DIRS to include the components directory
set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/components" "../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ble_server)
