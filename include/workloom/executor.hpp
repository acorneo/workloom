#pragma once
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <condition_variable>

namespace workloom {

class Executor {
public:
    explicit Executor(std::size_t thread_size);
    ~Executor();

    void wait_for_all();

    void enqueue(std::move_only_function<void()> task);

private:
    void worker_loop();

    std::vector<std::thread> threads;
    std::size_t thread_size;

    std::queue<std::move_only_function<void()>> tasks;
    std::mutex task_mutex;
    std::condition_variable task_cv;

    bool stopping = false;
};

}