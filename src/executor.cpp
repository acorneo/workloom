#include <cstddef>

#include <workloom/executor.hpp>

workloom::Executor::Executor(std::size_t thread_size) {
    this->thread_size = thread_size;
    threads.resize(thread_size);
}

workloom::Executor::~Executor() {
    threads.clear();
}

void workloom::Executor::start_manual_task(std::move_only_function<void()> task, int thread_idx) {
    if (thread_idx < 0 || thread_idx >= thread_size) {
        throw std::runtime_error("thread_idx incorrect");
    }

    threads[thread_idx] = std::thread(std::move(task));
}

void workloom::Executor::wait_for_all() {
    for (int i = 0; i < thread_size; i++) {
        if (threads[i].joinable()) {
            threads[i].join();
        }
    }
}
