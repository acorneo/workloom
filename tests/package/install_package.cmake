cmake_minimum_required(VERSION 3.20)

function(run)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Package verification failed (${result}): ${ARGV}")
    endif()
endfunction()

set(prefix "${WORKLOOM_TEST_DIR}/prefix")
set(relocated "${WORKLOOM_TEST_DIR}/relocated")
file(REMOVE_RECURSE "${WORKLOOM_TEST_DIR}")
run("${CMAKE_COMMAND}" --install "${WORKLOOM_BUILD_DIR}"
        --prefix "${prefix}" --config "${WORKLOOM_CONFIG}")
# Verify that package metadata does not rely on its original install prefix.
file(RENAME "${prefix}" "${relocated}")

set(generator_options)
if(WORKLOOM_GENERATOR_PLATFORM)
    list(APPEND generator_options -A "${WORKLOOM_GENERATOR_PLATFORM}")
endif()
if(WORKLOOM_GENERATOR_TOOLSET)
    list(APPEND generator_options -T "${WORKLOOM_GENERATOR_TOOLSET}")
endif()
if(WORKLOOM_TOOLCHAIN_FILE)
    list(APPEND generator_options "-DCMAKE_TOOLCHAIN_FILE=${WORKLOOM_TOOLCHAIN_FILE}")
endif()
run("${CMAKE_COMMAND}" -S "${WORKLOOM_CONSUMER_SOURCE_DIR}"
        -B "${WORKLOOM_TEST_DIR}/consumer" -G "${WORKLOOM_GENERATOR}"
        ${generator_options}
        "-DCMAKE_CXX_COMPILER=${WORKLOOM_CXX_COMPILER}"
        "-DCMAKE_BUILD_TYPE=${WORKLOOM_CONFIG}"
        "-DCMAKE_PREFIX_PATH=${relocated}"
        "-DWORKLOOM_VERSION=${WORKLOOM_VERSION}")
run("${CMAKE_COMMAND}" --build "${WORKLOOM_TEST_DIR}/consumer" --config "${WORKLOOM_CONFIG}")
run("${CMAKE_CTEST_COMMAND}" --test-dir "${WORKLOOM_TEST_DIR}/consumer"
        -C "${WORKLOOM_CONFIG}" --output-on-failure)
