#include <atomic>
#include <future>
#include <stdexcept>
#include <gtest/gtest.h>
#include <workloom/executor.hpp>

TEST(ExecutorTest, NegativeThreadSizeConstructor) {
    ASSERT_THROW(workloom::Executor(-1), std::length_error);
}

TEST(ExecutorTest, ExecutesTasks) {
    std::atomic<int> counter{0};

    {
        workloom::Executor executor(3);

        for (int i = 0; i < 100; ++i) {
            executor.enqueue([&counter] {
                ++counter;
            });
        }
    }

    EXPECT_EQ(counter.load(), 100);
}

TEST(ExecutorTest, ZeroThreadsRejected) {
    EXPECT_THROW(workloom::Executor(0), std::invalid_argument);
}

TEST(ExecutorTest, WaitIsReusable) {
    workloom::Executor executor(3);
    std::atomic<int> counter{0};
    executor.wait_for_all();
    for (int round = 1; round <= 2; ++round) {
        for (int i = 0; i < 100; ++i) executor.enqueue([&] { ++counter; });
        executor.wait_for_all();
        EXPECT_EQ(counter.load(), round * 100);
    }
}

TEST(ExecutorTest, ExceptionsAreCapturedAndWorkersContinue) {
    workloom::Executor executor(1);
    executor.enqueue([] { throw std::runtime_error("task failure"); });
    std::atomic<bool> completed{false};
    executor.enqueue([&] { completed = true; });
    executor.wait_for_all();
    EXPECT_TRUE(completed);
    auto error = executor.take_exception();
    ASSERT_TRUE(error);
    EXPECT_THROW(std::rethrow_exception(error), std::runtime_error);
    EXPECT_FALSE(executor.take_exception());
}

TEST(ExecutorTest, BoundsQueueAndTimesOutWhileTaskIsRunning) {
    workloom::Executor executor(1, 1);
    std::promise<void> started, release;
    auto gate = release.get_future().share();
    executor.enqueue([&] { started.set_value(); gate.wait(); });
    started.get_future().wait();
    EXPECT_FALSE(executor.wait_for_all_for(std::chrono::milliseconds(1)));
    EXPECT_TRUE(executor.try_enqueue([] {}));
    EXPECT_FALSE(executor.try_enqueue([] {}));
    EXPECT_THROW(executor.enqueue([] {}), std::overflow_error);
    release.set_value();
    executor.wait_for_all();
    EXPECT_TRUE(executor.wait_for_all_for(std::chrono::milliseconds(0)));
    EXPECT_TRUE(executor.try_enqueue([] {}));
}

TEST(ExecutorTest, RejectsEmptyTasksAndWorkerWaits) {
    workloom::Executor executor(1);
    EXPECT_THROW(executor.enqueue({}), std::invalid_argument);
    executor.enqueue([&] {
        EXPECT_THROW(executor.wait_for_all(), std::logic_error);
        EXPECT_THROW(executor.wait_for_all_for(std::chrono::seconds(1)), std::logic_error);
    });
    executor.wait_for_all();
}

TEST(ExecutorTest, CanBeDestroyedByOwnTaskAndDrainQueue) {
    auto executor = std::make_unique<workloom::Executor>(1);
    std::promise<void> release, completed;
    auto gate = release.get_future().share();
    executor->enqueue([&] { gate.wait(); executor.reset(); });
    executor->enqueue([&] { completed.set_value(); });
    release.set_value();
    completed.get_future().wait();
    EXPECT_FALSE(executor);
}

TEST(ExecutorTest, ConcurrentSubmissionsAndMoveOnlyCaptures) {
    workloom::Executor executor(4);
    std::atomic<int> counter{0};
    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i) {
        producers.emplace_back([&] {
            for (int j = 0; j < 100; ++j) {
                executor.enqueue([&, value = std::make_unique<int>(1)] { counter += *value; });
            }
        });
    }
    for (auto& producer : producers) producer.join();
    executor.wait_for_all();
    EXPECT_EQ(counter.load(), 400);
}
