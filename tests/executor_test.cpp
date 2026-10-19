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
