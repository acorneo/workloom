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
cmake -S . -B build -DBUILD_TESTING=OFF
cmake --build build
```

Build and run tests (`BUILD_TESTING` is enabled by default):

```sh
cmake -S . -B build-tests -DBUILD_TESTING=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

If needed, select a compiler during the initial configuration, for example with `-DCMAKE_CXX_COMPILER=g++-16`. The existing build was verified with GCC 16 on macOS; both tests pass.

## Usage

```cpp
#include <atomic>
#include <condition_variable>
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
    } // The destructor waits for all tasks and worker threads to finish.

    std::cout << counter.load() << '\n'; // 100
}
```

Objects referenced by tasks must remain alive until those tasks finish. Tasks are responsible for synchronizing their own access to shared data.

To use the library from another CMake project, place this repository under a directory such as `external/workloom`:

```cmake
set(BUILD_TESTING OFF CACHE BOOL "Build tests")
add_subdirectory(external/workloom)

add_executable(example main.cpp)
target_compile_features(example PRIVATE cxx_std_23)
target_link_libraries(example PRIVATE workloom)
```

The `workloom` target exposes its include directory. The consumer explicitly requests C++23 because the library's current CMake configuration does not propagate that requirement through `target_compile_features`. Installation through `cmake --install` and a package for `find_package` are not provided yet.

## Current limitations

- **`wait_for_all()` cannot be used to wait for tasks.** It calls `join()` without setting the stop flag. With a nonzero worker count, it blocks indefinitely: after draining the queue, workers keep waiting for new tasks. With the current API, let the executor go out of scope as shown above.
- The thread count is not validated. `Executor(0)` accepts tasks but never executes them. The parameter is a `std::size_t`, so negative arguments are converted to large unsigned values. The test using `-1` expects a `std::length_error` from reserving storage for threads, rather than explicit argument validation. Use a reasonable positive thread count.
- Task exceptions are not caught. An exception escaping a task causes `std::terminate`; handle errors inside tasks.
- The task queue is unbounded. There is no mechanism to limit task submission based on queue size.
- The destructor waits without a timeout. If a task never finishes, destruction never completes either. Do not destroy the executor from one of its own tasks: the destructor would attempt to join the current thread.
- `executor.hpp` uses `std::condition_variable` without directly including `<condition_variable>`. The example includes that header first; portability of the library build itself depends on transitive standard library includes.

## Repository layout

```text
include/workloom/executor.hpp  — public Executor interface
src/executor.cpp              — task queue, worker loop, and shutdown
tests/executor_test.cpp       — constructor and 100-task execution tests
CMakeLists.txt                — static library target and test setup
tests/CMakeLists.txt          — GoogleTest download and test registration
```

The task execution test verifies completion through the destructor. There are currently no dedicated tests for `wait_for_all()`, task exceptions, or concurrent task submission.
