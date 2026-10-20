cmake_minimum_required(VERSION 3.20)

project(workloom VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Threads REQUIRED)

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
add_library(workloom::workloom ALIAS workloom)
set_target_properties(workloom PROPERTIES DEBUG_POSTFIX d)
target_compile_features(workloom PUBLIC cxx_std_23)
target_link_libraries(workloom PUBLIC Threads::Threads)
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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set(workloom_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/workloom")
configure_package_config_file(
        cmake/workloomConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/workloomConfig.cmake"
        INSTALL_DESTINATION "${workloom_INSTALL_CMAKEDIR}"
)
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/workloomConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMinorVersion
)

install(TARGETS workloom EXPORT workloomTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
install(DIRECTORY include/workloom DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(EXPORT workloomTargets
        FILE workloomTargets.cmake
        NAMESPACE workloom::
        DESTINATION "${workloom_INSTALL_CMAKEDIR}"
)
install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/workloomConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/workloomConfigVersion.cmake"
        DESTINATION "${workloom_INSTALL_CMAKEDIR}"
)

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
