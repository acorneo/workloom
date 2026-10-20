cmake_minimum_required(VERSION 3.20)

project(workloom LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WORKLOOM_ENABLE_SANITIZERS "Enable sanitizers in Debug builds" ON)
set(WORKLOOM_SANITIZERS "address,undefined" CACHE STRING
        "Debug sanitizers (for example address,undefined or thread)")
option(WORKLOOM_ENABLE_IPO "Enable interprocedural optimization in Release builds" ON)

# Keep warnings and optimization flags local to our targets, including tests.
function(workloom_configure_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-
                $<$<CONFIG:Debug>:/Od>
                $<$<CONFIG:Release>:/O2>)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        target_compile_options(${target} PRIVATE
                -Wall -Wextra -Wpedantic -Wconversion -Wshadow
                "$<$<CONFIG:Debug>:-Og;-g;-fno-omit-frame-pointer>"
                $<$<CONFIG:Release>:-O3>)
    endif()

    if(WORKLOOM_ENABLE_IPO AND WORKLOOM_IPO_SUPPORTED)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    endif()
endfunction()

if(WORKLOOM_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT WORKLOOM_IPO_SUPPORTED OUTPUT WORKLOOM_IPO_ERROR LANGUAGES CXX)
    if(NOT WORKLOOM_IPO_SUPPORTED)
        message(WARNING "Release IPO is unavailable: ${WORKLOOM_IPO_ERROR}")
    endif()
endif()

add_library(workloom STATIC
        src/executor.cpp
)
target_compile_features(workloom PUBLIC cxx_std_23)
workloom_configure_target(workloom)

if(WORKLOOM_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$" AND NOT MSVC)
        include(CMakePushCheckState)
        include(CheckCXXSourceCompiles)
        cmake_push_check_state(RESET)
        set(CMAKE_REQUIRED_FLAGS "-fsanitize=${WORKLOOM_SANITIZERS}")
        set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=${WORKLOOM_SANITIZERS}")
        # Recheck when the selected sanitizer set changes.
        unset(WORKLOOM_SANITIZERS_SUPPORTED CACHE)
        check_cxx_source_compiles("int main() { return 0; }" WORKLOOM_SANITIZERS_SUPPORTED)
        cmake_pop_check_state()
        if(NOT WORKLOOM_SANITIZERS_SUPPORTED)
            message(FATAL_ERROR "Sanitizers '${WORKLOOM_SANITIZERS}' are unavailable. Select a supported set or use -DWORKLOOM_ENABLE_SANITIZERS=OFF.")
        endif()

        # A static library has no link step. Consumers need the runtime flags
        # as well as instrumentation, so propagate these requirements.
        target_compile_options(workloom PUBLIC
                "$<$<CONFIG:Debug>:-fsanitize=${WORKLOOM_SANITIZERS};-fno-sanitize-recover=all;-fno-omit-frame-pointer>")
        target_link_options(workloom INTERFACE
                $<$<CONFIG:Debug>:-fsanitize=${WORKLOOM_SANITIZERS}>)
    else()
        message(WARNING "Automatic Debug sanitizers require GCC or Clang with the GNU command-line interface; use -DWORKLOOM_ENABLE_SANITIZERS=OFF for this compiler.")
    endif()
endif()

target_include_directories(workloom PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
