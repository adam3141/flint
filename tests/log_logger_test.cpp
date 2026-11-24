#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "flint/log/callback_sink.hpp"
#include "flint/log/category.hpp"
#include "flint/log/filter.hpp"
#include "flint/log/level.hpp"
#include "flint/log/log.hpp"
#include "flint/log/logger.hpp"
#include "flint/log/record.hpp"
#include "flint/meta/enum.hpp"

namespace flint::log {

// Names parameterised tests after the mode. Must be in Mode's namespace, and not an anonymous one, for ADL.
// NOLINTNEXTLINE(misc-use-anonymous-namespace): ADL ignores anonymous namespaces
static auto PrintTo(Mode mode, std::ostream* out) -> void {
  *out << flint::meta::EnumName(mode);
}

}  // namespace flint::log

namespace {

using namespace flint::log;  // NOLINT(google-build-using-namespace): test brevity

// Collects records thread-safely.
class Collector {
public:
  auto MakeSink() -> std::unique_ptr<CallbackSink> {
    return std::make_unique<CallbackSink>([this](const Record& record) {
      const std::scoped_lock lock{m_mutex};
      m_records.push_back(record);
    });
  }

  auto Records() -> std::vector<Record> {
    const std::scoped_lock lock{m_mutex};
    return m_records;
  }

private:
  std::mutex m_mutex;
  std::vector<Record> m_records;
};

class LoggerTest : public ::testing::TestWithParam<Mode> {};

TEST_P(LoggerTest, CapturesSourceLocationOfTheCall) {
  Collector collector;
  Logger logger{{.mode = GetParam()}};
  logger.AddSink(collector.MakeSink());
  const auto line = __LINE__ + 1;
  logger.Info("value is {}", 42);
  logger.Flush();

  const auto records = collector.Records();
  ASSERT_EQ(records.size(), 1U);
  EXPECT_EQ(records[0].message, "value is 42");
  EXPECT_EQ(records[0].level, kInfo);
  EXPECT_EQ(records[0].source.line, static_cast<std::uint32_t>(line));
  EXPECT_TRUE(records[0].source.file.ends_with("log_logger_test.cpp"));
  EXPECT_NE(records[0].source.function.find("CapturesSourceLocationOfTheCall"), std::string_view::npos);
  EXPECT_EQ(records[0].sequence, 1U);
  EXPECT_GT(records[0].systemUptime.count(), 0);
}

TEST_P(LoggerTest, AppliesPerSinkFilters) {
  Collector everything;
  Collector errorsOnly;
  Logger logger{{.mode = GetParam()}};
  logger.AddSink(everything.MakeSink(), Filter{kTrace});
  logger.AddSink(errorsOnly.MakeSink(), Filter{kError});
  logger.Debug("debug");
  logger.Error("error");
  logger.Flush();
  EXPECT_EQ(everything.Records().size(), 2U);
  ASSERT_EQ(errorsOnly.Records().size(), 1U);
  EXPECT_EQ(errorsOnly.Records()[0].message, "error");
}

TEST_P(LoggerTest, FiltersByCategory) {
  const Category render = *RegisterCategory("LoggerTestRender");
  Collector collector;
  Logger logger{{.mode = GetParam()}};
  Filter filter{kInfo};
  filter.SetCategoryMinimum(render, kOff);
  logger.AddSink(collector.MakeSink(), filter);
  const Channel renderLog{logger, render};
  renderLog.Error("hidden");
  logger.Info("shown");
  logger.Flush();
  ASSERT_EQ(collector.Records().size(), 1U);
  EXPECT_EQ(collector.Records()[0].message, "shown");
}

TEST_P(LoggerTest, LogsAtCustomLevels) {
  const Level notice = *RegisterLevel("LoggerTestNotice", 350, Colour::Blue);
  Collector collector;
  Logger logger{{.mode = GetParam()}};
  logger.AddSink(collector.MakeSink(), Filter{notice});
  logger.Info("below");
  logger.Log(notice, kDefaultCategory, "at {}", "notice");
  logger.Flush();
  ASSERT_EQ(collector.Records().size(), 1U);
  EXPECT_EQ(collector.Records()[0].level, notice);
}

TEST_P(LoggerTest, DoesNotFormatDisabledRecords) {
  Logger logger{{.mode = GetParam()}};
  Collector collector;
  logger.AddSink(collector.MakeSink(), Filter{kWarning});
  EXPECT_FALSE(logger.IsEnabled(kDebug, kDefaultCategory));
  EXPECT_TRUE(logger.IsEnabled(kWarning, kDefaultCategory));
}

TEST_P(LoggerTest, PreservesOrderAcrossThreads) {
  Collector collector;
  Logger logger{{.mode = GetParam(), .queueCapacity = 64}};
  logger.AddSink(collector.MakeSink());
  constexpr int kThreads = 4;
  constexpr int kPerThread = 500;
  {
    std::vector<std::jthread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
      threads.emplace_back([&logger, t] {
        for (int i = 0; i < kPerThread; ++i) {
          logger.Info("{} {}", t, i);
        }
      });
    }
  }
  logger.Flush();
  const auto records = collector.Records();
  ASSERT_EQ(records.size(), static_cast<std::size_t>(kThreads * kPerThread));
  for (std::size_t i = 0; i < records.size(); ++i) {
    EXPECT_EQ(records[i].sequence, i + 1);
  }
}

TEST_P(LoggerTest, FatalIsWrittenBeforeTheCallReturns) {
  Collector collector;
  Logger logger{{.mode = GetParam()}};
  logger.AddSink(collector.MakeSink());
  logger.Fatal("boom");
  EXPECT_EQ(collector.Records().size(), 1U);
}

TEST_P(LoggerTest, RemovedSinksReceiveEverythingQueuedBeforeRemoval) {
  Collector collector;
  Logger logger{{.mode = GetParam()}};
  const SinkId id = logger.AddSink(collector.MakeSink());
  logger.Info("one");
  EXPECT_TRUE(logger.RemoveSink(id));
  logger.Info("two");
  logger.Flush();
  ASSERT_EQ(collector.Records().size(), 1U);
  EXPECT_FALSE(logger.RemoveSink(id));
}

INSTANTIATE_TEST_SUITE_P(Modes, LoggerTest, ::testing::Values(Mode::Asynchronous, Mode::Synchronous), [](const auto& paramInfo) {
  return paramInfo.param == Mode::Asynchronous ? std::string{"Asynchronous"} : std::string{"Synchronous"};
});

TEST(LoggerDropTest, CountsDroppedRecordsWhenQueueIsFullAndPolicyIsDrop) {
  std::atomic<bool> release{false};
  Logger logger{{.mode = Mode::Asynchronous, .queueCapacity = 1, .overflow = OverflowPolicy::Drop}};
  logger.AddSink(std::make_unique<CallbackSink>([&release](const Record&) {
    while (!release.load()) {
      std::this_thread::yield();
    }
  }));
  for (int i = 0; i < 100; ++i) {
    logger.Info("{}", i);
  }
  release.store(true);
  logger.Flush();
  EXPECT_GT(logger.DroppedCount(), 0U);
}

TEST(ApplicationUptimeTest, IsMeasuredFromTheFirstLogger) {
  const auto epoch = Logger::ApplicationEpoch();
  const Logger second;
  EXPECT_EQ(Logger::ApplicationEpoch(), epoch);
}

}  // namespace
