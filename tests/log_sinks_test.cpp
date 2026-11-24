#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "flint/json/parser.hpp"
#include "flint/log/file_sink.hpp"
#include "flint/log/json_file_sink.hpp"
#include "flint/log/json_record.hpp"
#include "flint/log/level.hpp"
#include "flint/log/logger.hpp"
#include "flint/log/network_sink.hpp"
#include "flint/log/record.hpp"
#include "flint/net/tcp_listener.hpp"

namespace {

using namespace flint::log;  // NOLINT(google-build-using-namespace): test brevity

class TempDirectory {
public:
  TempDirectory() : m_path{std::filesystem::temp_directory_path() / ("flint-test-" + std::to_string(std::random_device{}()))} {
    std::filesystem::create_directories(m_path);
  }

  ~TempDirectory() {
    std::error_code error;
    std::filesystem::remove_all(m_path, error);
  }

  TempDirectory(const TempDirectory&) = delete;
  auto operator=(const TempDirectory&) -> TempDirectory& = delete;
  TempDirectory(TempDirectory&&) = delete;
  auto operator=(TempDirectory&&) -> TempDirectory& = delete;

  [[nodiscard]] auto Path() const -> const std::filesystem::path& { return m_path; }

private:
  std::filesystem::path m_path;
};

auto ReadLines(const std::filesystem::path& path) -> std::vector<std::string> {
  std::ifstream stream{path};
  std::vector<std::string> lines;
  for (std::string line; std::getline(stream, line);) {
    lines.push_back(line);
  }
  return lines;
}

TEST(FileSinkTest, WritesFormattedLines) {
  const TempDirectory directory;
  const auto path = directory.Path() / "logs" / "app.log";
  {
    Logger logger{{.mode = Mode::Synchronous}};
    auto sink = FileSink::Create({.path = path, .pattern = "{level}: {message}", .rotation = {}});
    ASSERT_TRUE(sink.has_value());
    logger.AddSink(std::move(*sink));
    logger.Info("hello");
    logger.Warning("careful");
  }
  EXPECT_EQ(ReadLines(path), (std::vector<std::string>{"Info: hello", "Warning: careful"}));
}

TEST(FileSinkTest, RotatesBySize) {
  const TempDirectory directory;
  const auto path = directory.Path() / "app.log";
  {
    Logger logger{{.mode = Mode::Synchronous}};
    auto sink = FileSink::Create({.path = path, .pattern = "{message}", .rotation = {.maxBytes = 20, .maxFiles = 2}});
    ASSERT_TRUE(sink.has_value());
    logger.AddSink(std::move(*sink));
    for (int i = 0; i < 10; ++i) {
      logger.Info("line {:04}", i);  // 10 bytes with newline
    }
  }
  EXPECT_EQ(ReadLines(path), (std::vector<std::string>{"line 0008", "line 0009"}));
  EXPECT_EQ(ReadLines(directory.Path() / "app.1.log"), (std::vector<std::string>{"line 0006", "line 0007"}));
  EXPECT_EQ(ReadLines(directory.Path() / "app.2.log"), (std::vector<std::string>{"line 0004", "line 0005"}));
  EXPECT_FALSE(std::filesystem::exists(directory.Path() / "app.3.log"));
}

TEST(JsonFileSinkTest, WritesOneParseableObjectPerLine) {
  const TempDirectory directory;
  const auto path = directory.Path() / "app.jsonl";
  {
    Logger logger{{.mode = Mode::Synchronous}};
    auto sink = JsonFileSink::Create({.path = path, .rotation = {}});
    ASSERT_TRUE(sink.has_value());
    logger.AddSink(std::move(*sink));
    logger.Info("quote \" and newline \n inside");
    logger.Error("second");
  }
  const auto lines = ReadLines(path);
  ASSERT_EQ(lines.size(), 2U);
  const auto first = flint::json::Parse(lines[0]);
  ASSERT_TRUE(first.has_value()) << first.error().Describe();
  EXPECT_EQ(first->Find("message")->AsString(), "quote \" and newline \n inside");
  EXPECT_EQ(first->Find("level")->AsString(), "Info");
  EXPECT_EQ(first->Find("severity")->AsInteger(), 300);
  EXPECT_TRUE(first->Find("function")->AsString()->contains("WritesOneParseableObjectPerLine"));
  EXPECT_TRUE(first->Find("timestamp")->AsString()->ends_with("Z"));
  EXPECT_TRUE(first->Find("appUptimeNs")->AsInteger().has_value());
  EXPECT_TRUE(first->Find("systemUptimeNs")->AsInteger().has_value());
}

TEST(JsonRecordTest, RoundTripsThroughRecordFromJson) {
  Record record;
  record.level = kError;
  record.message = "round trip";
  record.source = SourcePosition{.file = "a.cpp", .function = "void f()", .line = 7, .column = 1};
  record.timestamp = std::chrono::sys_days{std::chrono::year{2026} / 1 / 2} + std::chrono::microseconds{5};
  record.threadName = "worker";
  std::string text;
  AppendJsonRecord(record, text);

  auto parsed = flint::json::Parse(text);
  ASSERT_TRUE(parsed.has_value());
  auto owned = RecordFromJson(*parsed);
  ASSERT_TRUE(owned.has_value());
  const Record& rebuilt = owned->Get();
  EXPECT_EQ(rebuilt.level, kError);
  EXPECT_EQ(rebuilt.message, "round trip");
  EXPECT_EQ(rebuilt.source.file, "a.cpp");
  EXPECT_EQ(rebuilt.source.line, 7U);
  EXPECT_EQ(rebuilt.threadName, "worker");
  EXPECT_EQ(rebuilt.timestamp, record.timestamp);
}

TEST(NetworkSinkTest, StreamsJsonLinesToACollector) {
  auto listener = flint::net::TcpListener::Listen("127.0.0.1", 0);
  ASSERT_TRUE(listener.has_value()) << listener.error().Describe();

  {
    Logger logger{{.mode = Mode::Synchronous}};
    logger.AddSink(std::make_unique<NetworkSink>(NetworkSinkOptions{.host = "127.0.0.1", .port = listener->GetPort()}));
    logger.Info("over the wire");
    logger.Warning("second line");
    // The sink connects in the background; the destructor sends what is buffered once connected.
    auto accepted = listener->Accept(std::chrono::seconds{5});
    ASSERT_TRUE(accepted.has_value() && accepted->has_value());
    std::string received;
    std::array<std::byte, 4096> buffer{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (std::ranges::count(received, '\n') < 2 && std::chrono::steady_clock::now() < deadline) {
      auto count = (*accepted)->Receive(buffer, std::chrono::milliseconds{100});
      ASSERT_TRUE(count.has_value());
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): received bytes are text
      received.append(reinterpret_cast<const char*>(buffer.data()), *count);
    }
    std::istringstream lines{received};
    std::string line;
    ASSERT_TRUE(std::getline(lines, line));
    EXPECT_EQ(flint::json::Parse(line)->Find("message")->AsString(), "over the wire");
    ASSERT_TRUE(std::getline(lines, line));
    EXPECT_EQ(flint::json::Parse(line)->Find("level")->AsString(), "Warning");
  }
}

}  // namespace
