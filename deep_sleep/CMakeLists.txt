cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/components" "../components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(deep_sleep)

