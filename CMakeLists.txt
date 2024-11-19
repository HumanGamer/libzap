cmake_minimum_required(VERSION 3.20)

add_subdirectory(vendor/M4Image)
project(libzap)

set(CMAKE_CXX_STANDARD 17)

add_library(libzap STATIC "")
target_include_directories(libzap PUBLIC include)
target_include_directories(libzap PRIVATE vendor)

target_compile_definitions(libzap PRIVATE CRT_SECURE_NO_WARNINGS)

target_sources(libzap
    PRIVATE
        src/libzap.cpp
    PUBLIC
        include/libzap.h
)
target_link_libraries(libzap M4Image)
