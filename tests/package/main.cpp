#include <workloom/executor.hpp>

#include <atomic>

int main() {
    std::atomic<int> count{0};
    workloom::Executor executor(2);
    executor.enqueue([&] { ++count; });
    executor.wait_for_all();
    return count.load() == 1 ? 0 : 1;
}
