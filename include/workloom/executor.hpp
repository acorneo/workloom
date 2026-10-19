#pragma once
#include <thread>
#include <vector>
#include <functional>

namespace workloom {

class Executor {
public:
    explicit Executor(std::size_t thread_size);
    ~Executor();

    void start_manual_task(std::move_only_function<void()> task, int thread_idx);
    void wait_for_all();

private:
    std::vector<std::thread> threads;
    std::size_t thread_size;
};

}