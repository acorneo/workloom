#pragma once
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace workloom {

class Executor {
public:
    explicit Executor(std::size_t thread_size);
    ~Executor();

    void start_manual_task(std::move_only_function<void()> task, int thread_idx);
    void wait_for_all();

    void enqueue(std::move_only_function<void()> task);

private:
    std::vector<std::thread> threads;
    std::size_t thread_size;

    std::queue<std::move_only_function<void()>> tasks;
    std::mutex task_mutex;
};

}