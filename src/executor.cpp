#include <workloom/executor.hpp>

#include <stdexcept>
#include <utility>

thread_local workloom::Executor::State* workloom::Executor::current_worker = nullptr;

workloom::Executor::Executor(std::size_t thread_size, std::size_t queue_capacity)
    : state(std::make_shared<State>()) {
    if (thread_size == 0) throw std::invalid_argument("thread_size cannot be 0");
    state->capacity = queue_capacity;
    threads.reserve(thread_size);
    try {
        for (std::size_t i = 0; i < thread_size; ++i) {
            threads.emplace_back([shared = state] { worker_loop(shared); });
        }
    } catch (...) {
        {
            std::lock_guard lock(state->mutex);
            state->stopping = true;
        }
        state->ready.notify_all();
        for (auto& thread : threads) thread.join();
        throw;
    }
}

workloom::Executor::~Executor() {
    {
        std::lock_guard lock(state->mutex);
        state->stopping = true;
    }
    state->ready.notify_all();
    // A worker cannot join itself. All workers own the shared state, so they
    // can safely drain it after the Executor has been destroyed.
    const bool from_worker = current_worker == state.get();
    for (auto& thread : threads) {
        if (from_worker) thread.detach();
        else thread.join();
    }
}

void workloom::Executor::wait_for_all() {
    if (current_worker == state.get()) throw std::logic_error("Cannot wait from this executor's worker");
    std::unique_lock lock(state->mutex);
    state->idle.wait(lock, [this] { return state->tasks.empty() && state->active == 0; });
}

bool workloom::Executor::wait_for_all_for(std::chrono::steady_clock::duration timeout) {
    if (current_worker == state.get()) throw std::logic_error("Cannot wait from this executor's worker");
    std::unique_lock lock(state->mutex);
    return state->idle.wait_for(lock, timeout, [this] { return state->tasks.empty() && state->active == 0; });
}

void workloom::Executor::enqueue(std::move_only_function<void()> task) {
    if (!try_enqueue(std::move(task))) throw std::overflow_error("Executor queue is full");
}

bool workloom::Executor::try_enqueue(std::move_only_function<void()> task) {
    if (!task) throw std::invalid_argument("Task cannot be empty");
    {
        std::lock_guard lock(state->mutex);
        if (state->stopping) throw std::runtime_error("Executor is stopping");
        if (state->capacity && state->tasks.size() >= state->capacity) return false;
        state->tasks.push(std::move(task));
    }
    state->ready.notify_one();
    return true;
}

std::exception_ptr workloom::Executor::take_exception() {
    std::lock_guard lock(state->mutex);
    return std::exchange(state->exception, {});
}

void workloom::Executor::worker_loop(std::shared_ptr<State> state) {
    current_worker = state.get();
    while (true) {
        std::move_only_function<void()> task;
        {
            std::unique_lock lock(state->mutex);
            state->ready.wait(lock, [&] { return !state->tasks.empty() || state->stopping; });
            if (state->stopping && state->tasks.empty()) break;
            task = std::move(state->tasks.front());
            state->tasks.pop();
            ++state->active;
        }
        try {
            task();
        } catch (...) {
            std::lock_guard lock(state->mutex);
            if (!state->exception) state->exception = std::current_exception();
        }
        // Destroy captures before publishing completion.
        task = nullptr;
        {
            std::lock_guard lock(state->mutex);
            --state->active;
            if (state->tasks.empty() && state->active == 0) state->idle.notify_all();
        }
    }
    current_worker = nullptr;
}
