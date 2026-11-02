cmake_minimum_required(VERSION 3.16.0)

set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS}" "${CMAKE_CURRENT_LIST_DIR}/extras/max30102")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Fall_Detection_ESP32)
