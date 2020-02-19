cmake_minimum_required(VERSION 3.1)
project(arrow1
    VERSION 2.0
    DESCRIPTION "Play and record multi-channel audio using jack."
    HOMEPAGE_URL "https://github.com/cbrown1/arrow1"
    LANGUAGES CXX
)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
add_subdirectory(src)
