# Workloom

Workloom is a small C++23 static library for running tasks in a thread pool. Its current implementation provides a single class, `workloom::Executor`, which creates a fixed number of worker threads and distributes tasks from a shared queue.

## Current functionality

- Worker threads start when an `Executor` is constructed.
- `enqueue()` adds a task to the queue and wakes one waiting worker. The queue is protected by a mutex; multiple threads can submit tasks while the executor is alive and is not being destroyed.
- Tasks use `std::move_only_function<void()>`, allowing lambdas with move-only captures such as `std::unique_ptr`.
- Workers take tasks from a FIFO queue and execute them outside the queue lock. Parallel execution does not guarantee task start or completion order.
- During destruction, the executor wakes its workers, lets them drain the remaining queue, and waits for them to finish using `join()`.

Timed scheduling, priorities, cancellation, futures, and an API for retrieving results are not implemented. The repository does not include benchmarks.

## Requirements and building

- CMake 3.20 or later.
- A compiler and standard library supporting C++23, including `std::move_only_function`.
- For tests: GoogleTest 1.18.0. CMake downloads it using `FetchContent`, so the initial configuration with tests enabled requires access to GitHub.

Build the library without test dependencies:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build
```

Build and run tests (`BUILD_TESTING` is enabled by default):

```sh
cmake -S . -B build-tests -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

If needed, select a compiler during the initial configuration, for example with `-DCMAKE_CXX_COMPILER=g++-16`. The existing build was verified with GCC 16 on macOS; the test suite covers waiting, exceptions, queue limits, and worker-initiated destruction.

GCC and Clang builds enable `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`. Debug uses `-Og -g` and frame pointers, with AddressSanitizer and UndefinedBehaviorSanitizer enabled by default. Sanitizer failures stop execution. Release uses `-O3` and interprocedural optimization (IPO/LTO) when supported. MSVC builds use `/W4 /permissive-`, `/Od` in Debug, and `/O2` in Release; automatic sanitizer setup is currently available for GCC and Clang with the GNU command-line interface.

Use `-DWORKLOOM_ENABLE_SANITIZERS=OFF` to disable Debug sanitizers or `-DWORKLOOM_ENABLE_IPO=OFF` to disable Release IPO. To check for data races, use a separate Debug build with `-DWORKLOOM_SANITIZERS=thread`; ThreadSanitizer cannot be combined with AddressSanitizer ([GCC sanitizer documentation](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html)). Configuration checks that the selected sanitizers can compile and link. Sanitizer compile and runtime link flags propagate to targets that link `workloom`; warnings and optimization flags apply only to Workloom and its tests.

For multi-configuration generators, select the configuration at build and test time with `cmake --build build --config Debug` and `ctest --test-dir build -C Debug --output-on-failure`.

## Installing and using find_package

Build and install the library, public headers, and CMake package:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-release
cmake --install build-release --prefix "$HOME/.local"
```

The install prefix can be any writable directory. Standard CMake install variables such as `CMAKE_INSTALL_LIBDIR` and `CMAKE_INSTALL_INCLUDEDIR` are supported. For multi-configuration generators, add `--config Release` to both the build and install commands. GoogleTest is not installed with Workloom.

In a separate project's `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(example LANGUAGES CXX)

find_package(workloom 0.1 CONFIG REQUIRED)

add_executable(example main.cpp)
target_link_libraries(example PRIVATE workloom::workloom)
```

Configure and build that project with the installation prefix and a compatible C++23 compiler:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$HOME/.local"
cmake --build build
```

`workloom::workloom` supplies its headers, C++23 requirement, and thread dependency. The package is versioned as `0.1.0`, accepts compatible version requests within the same minor version, and can be moved to another prefix. Its config and target files are installed under `<libdir>/cmake/workloom`, following [CMake's package export conventions](https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html).

Use matching build configurations when installing and consuming the library. Debug installs use `libworkloomd.a` (or `workloomd.lib` on MSVC), allowing Debug and Release archives to coexist in one prefix. Debug sanitizer requirements are exported along with the target; use `-DWORKLOOM_ENABLE_SANITIZERS=OFF` when building an uninstrumented Debug package.

## Usage

```cpp
#include <atomic>
#include <iostream>
#include <workloom/executor.hpp>

int main() {
    std::atomic<int> counter{0};

    {
        workloom::Executor executor(3);

        for (int i = 0; i < 100; ++i) {
            executor.enqueue([&counter] {
                ++counter;
            });
        }
        executor.wait_for_all(); // Workers remain available for more tasks.
    } // The destructor joins the worker threads.

    std::cout << counter.load() << '\n'; // 100
}
```

Objects referenced by tasks must remain alive until those tasks finish. Tasks are responsible for synchronizing their own access to shared data.

To use the library from another CMake project, place this repository under a directory such as `external/workloom`:

```cmake
set(BUILD_TESTING OFF CACHE BOOL "Build tests")
add_subdirectory(external/workloom)

add_executable(example main.cpp)
target_link_libraries(example PRIVATE workloom::workloom)
```

The same `workloom::workloom` target is available through `add_subdirectory()` and `find_package()`.

## Waiting, errors, and queue limits

`wait_for_all()` waits until the queue is empty and all running tasks (including their captures) have finished. It leaves the pool usable. `wait_for_all_for(std::chrono::seconds(1))` returns `false` on timeout and `true` when idle. Concurrent submissions can extend a wait; new tasks can arrive after it returns. Calling either wait from this executor's own tasks throws `std::logic_error` to avoid deadlock.

Task exceptions are caught and workers continue. `take_exception()` returns and clears the first recorded `std::exception_ptr`; rethrow it with `std::rethrow_exception()` to inspect the error. Additional failures while an exception is stored are discarded. Neither waiting nor destruction rethrows task exceptions.

Use `Executor executor(3, 100)` to limit the waiting queue to 100 tasks, excluding tasks already running. The default capacity of zero is unlimited. `try_enqueue()` returns `false` when full; `enqueue()` throws `std::overflow_error`. Both consume the supplied callable even on rejection, and neither blocks waiting for space. Empty tasks throw `std::invalid_argument`.

## Shutdown constraints

Normal destruction drains the queue and joins workers. C++ cannot safely forcibly stop an arbitrary task: if one never finishes, destruction still cannot finish. Use the timed wait to detect slow tasks before destruction, and arrange cooperative cancellation inside tasks when needed. A timed wait does not cancel tasks or impose a timeout on destruction.

When a task destroys its own executor, workers are detached and continue draining independently using shared internal state. Destruction returns without waiting in this case. Any objects referenced by those tasks must remain alive until they finish; use an external completion signal to coordinate their lifetime. As with other C++ objects, calls on the executor must not race with its destruction.

## Repository layout

```text
include/workloom/executor.hpp  — public Executor interface
src/executor.cpp              — task queue, worker loop, and shutdown
tests/executor_test.cpp       — executor lifecycle and task submission tests
CMakeLists.txt                — static library target and test setup
tests/CMakeLists.txt          — GoogleTest download and test registration
cmake/workloomConfig.cmake.in — installed find_package configuration
tests/package/               — install, relocation, and consumer verification
```

Tests cover reusable and timed waits, exception capture, bounded queues, concurrent submission, move-only tasks, and destruction from a worker. The package test installs to a temporary prefix, relocates it, and builds and runs a separate consumer using only the installed package.
