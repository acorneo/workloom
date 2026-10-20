include(FetchContent)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
        URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
)

if(WIN32)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

# Test dependencies are not part of the Workloom installation.
set(INSTALL_GTEST OFF)
FetchContent_MakeAvailable(googletest)

add_executable(workloom_tests
        executor_test.cpp
)
workloom_configure_target(workloom_tests)

target_link_libraries(workloom_tests PRIVATE
        workloom
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(workloom_tests PROPERTIES TIMEOUT 15)

add_test(NAME workloom.install_package
        COMMAND ${CMAKE_COMMAND}
        "-DWORKLOOM_BUILD_DIR=${PROJECT_BINARY_DIR}"
        "-DWORKLOOM_CONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/package"
        "-DWORKLOOM_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/install-package"
        "-DWORKLOOM_CONFIG=$<CONFIG>"
        "-DWORKLOOM_VERSION=${PROJECT_VERSION}"
        "-DWORKLOOM_GENERATOR=${CMAKE_GENERATOR}"
        "-DWORKLOOM_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DWORKLOOM_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
        "-DWORKLOOM_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DWORKLOOM_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/package/install_package.cmake"
)
set_tests_properties(workloom.install_package PROPERTIES TIMEOUT 120)
