#include <gtest/gtest.h>
#include <workloom/executor.hpp>

TEST(ExecutorTest, NegativeThreadSizeConstructor) {
    ASSERT_THROW(workloom::Executor(-1), std::length_error);
}

TEST(ExecutorTest, NegativeThreadIdxManualTaskStart) {
    auto ex = workloom::Executor(3);
    ASSERT_THROW(ex.start_manual_task([](){}, -1), std::runtime_error);
}

TEST(ExecutorTest, WrongThreadIdxManualTaskStart) {
    auto ex = workloom::Executor(3);
    ASSERT_THROW(ex.start_manual_task([](){}, 4), std::runtime_error);
}

TEST(ExecutorTest, SimpleThreadEndedCheck) {
    auto ex = workloom::Executor(1);

    std::atomic<bool> executed = false;
    ex.start_manual_task([&executed]() {
        executed = true;
    }, 0);

    ex.wait_for_all();

    ASSERT_TRUE(executed);
}
