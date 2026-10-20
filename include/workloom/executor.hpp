#pragma once

#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace workloom {

class Executor {
public:
    // A zero queue capacity means unlimited. Running tasks do not count.
    explicit Executor(std::size_t thread_size, std::size_t queue_capacity = 0);
    ~Executor();
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;

    // Wait for an idle pool; workers remain available for further submissions.
    // Calling either wait from this executor's worker throws std::logic_error.
    void wait_for_all();
    bool wait_for_all_for(std::chrono::steady_clock::duration timeout);

    // enqueue throws std::overflow_error when the queue is full.
    void enqueue(std::move_only_function<void()> task);
    bool try_enqueue(std::move_only_function<void()> task);

    // Returns and clears the first task exception recorded since the last call.
    std::exception_ptr take_exception();

private:
    struct State {
        std::queue<std::move_only_function<void()>> tasks;
        std::mutex mutex;
        std::condition_variable ready;
        std::condition_variable idle;
        std::size_t active = 0;
        std::size_t capacity = 0;
        bool stopping = false;
        std::exception_ptr exception;
    };

    static void worker_loop(std::shared_ptr<State> state);
    static thread_local State* current_worker;
    std::shared_ptr<State> state;
    std::vector<std::thread> threads;
};

}
