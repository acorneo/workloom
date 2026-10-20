#include <cstddef>

#include <workloom/executor.hpp>

workloom::Executor::Executor(std::size_t thread_size) {
    if (thread_size == 0) throw std::runtime_error("thread_size cannot be 0");
    this->thread_size = thread_size;
    threads.reserve(thread_size);
    for (int i = 0; i < thread_size; i++) {
        threads.emplace_back([this] {
            worker_loop();
        });
    }
}

workloom::Executor::~Executor() {
    {
        std::lock_guard<std::mutex> lock(task_mutex);
        stopping = true;
    }

    task_cv.notify_all();

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void workloom::Executor::wait_for_all() {
    for (int i = 0; i < thread_size; i++) {
        if (threads[i].joinable()) {
            threads[i].join();
        }
    }
}

void workloom::Executor::enqueue(std::move_only_function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(task_mutex);

        if (stopping) {
            throw std::runtime_error("Executor is stopping");
        }

        tasks.push(std::move(task));
    }

    task_cv.notify_one();
}

void workloom::Executor::worker_loop() {
    while (true) {
        std::move_only_function<void()> task;

        {
            std::unique_lock<std::mutex> lock(task_mutex);
            task_cv.wait(lock, [this] {
                return !tasks.empty() || stopping;
            });

            if (stopping && tasks.empty()) {
                return;
            }

            task = std::move(tasks.front());
            tasks.pop();
        }

        task();
    }
}
