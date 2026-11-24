#include <chrono>
#include <cstdint>
#include <thread>

#include <gtest/gtest.h>

#include "flint/platform/process.hpp"
#include "flint/platform/thread.hpp"
#include "flint/platform/uptime.hpp"

namespace {

TEST(UptimeTest, SystemUptimeIsPositiveAndMonotonic) {
  const auto first = flint::platform::SystemUptime();
  ASSERT_TRUE(first.has_value());
  EXPECT_GT(first->count(), 0);
  const auto second = flint::platform::SystemUptime();
  ASSERT_TRUE(second.has_value());
  EXPECT_GE(*second, *first);
}

TEST(ThreadTest, NamesAndIdentifiesThreads) {
  std::uint64_t otherId = 0;
  std::string otherName;
  std::jthread{[&] {
    flint::platform::SetCurrentThreadName("a-long-thread-name-beyond-linux-limit");
    otherId = flint::platform::CurrentThreadId();
    otherName = flint::platform::CurrentThreadName();
  }}.join();
  EXPECT_NE(otherId, flint::platform::CurrentThreadId());
  EXPECT_EQ(otherName, "a-long-thread-name-beyond-linux-limit");
}

TEST(ProcessTest, ReportsProcessIdAndHostName) {
  EXPECT_GT(flint::platform::CurrentProcessId(), 0U);
  EXPECT_FALSE(flint::platform::HostName().empty());
}

}  // namespace
