cmake_minimum_required(VERSION 3.5)

# Set the EXTRA_COMPONENT_DIRS to include the components directory
# This is how we tell the build system where to find our shared components
set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/components" "../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ble_client)
