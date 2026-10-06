cmake_minimum_required(VERSION 3.25.0)
project(MorgoJSON VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)

option(MORGOJSON_BUILD_TESTS "Build tests" ON)
if(MORGOJSON_BUILD_TESTS)
    set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
    include(CTest)
    add_subdirectory(tests)
endif()
