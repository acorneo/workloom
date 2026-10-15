cmake_minimum_required(VERSION 3.20)

project(workloom LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(workloom STATIC
        src/executor.cpp
)

target_include_directories(workloom PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()