cmake_minimum_required(VERSION 3.20)
project(workloom_package_consumer LANGUAGES CXX)

find_package(workloom ${WORKLOOM_VERSION} EXACT CONFIG REQUIRED)
# Repeated discovery should also work for consumers with shared dependencies.
find_package(workloom CONFIG REQUIRED)

add_executable(package_consumer main.cpp)
target_link_libraries(package_consumer PRIVATE workloom::workloom)

enable_testing()
add_test(NAME package_consumer COMMAND package_consumer)
