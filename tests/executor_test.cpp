#include <gtest/gtest.h>
#include <workloom/executor.hpp>

TEST(ExecutorTest, AddsPositiveNumbers) {
	EXPECT_EQ(workloom::add(2, 3), 5);
}

TEST(ExecutorTest, AddsOppositeNumbers) {
	EXPECT_EQ(workloom::add(-1, 1), 0);
}
