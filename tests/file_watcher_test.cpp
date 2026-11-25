#include "flint/platform/file_watcher.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace flint::platform {
namespace {

auto WriteFile(const std::filesystem::path& path, std::string_view text) -> void {
  std::ofstream stream{path, std::ios::binary | std::ios::trunc};
  stream << text;
}

// Polls until something changes or the time is up; inotify delivers events asynchronously.
auto PollFor(FileWatcher& watcher, std::chrono::milliseconds patience = std::chrono::seconds{1}) -> std::vector<std::filesystem::path> {
  const auto deadline = std::chrono::steady_clock::now() + patience;
  while (std::chrono::steady_clock::now() < deadline) {
    if (auto changed = watcher.Poll(); !changed.empty()) {
      return changed;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{5});
  }
  return {};
}

class FileWatcherTest : public ::testing::TestWithParam<bool> {
protected:
  void SetUp() override {
    m_directory = std::filesystem::temp_directory_path() / ("flint-watch-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(m_directory);
    m_watcher = GetParam() ? FileWatcher::CreatePolling() : FileWatcher::Create();
  }

  void TearDown() override { std::filesystem::remove_all(m_directory); }

  [[nodiscard]] auto Directory() const -> const std::filesystem::path& { return m_directory; }

  [[nodiscard]] auto Watcher() -> FileWatcher& { return *m_watcher; }

private:
  std::filesystem::path m_directory;
  std::unique_ptr<FileWatcher> m_watcher;
};

TEST_P(FileWatcherTest, ReportsWritesAndSavesByRename) {
  const auto view = Directory() / "view.xml";
  WriteFile(view, "<A/>");
  ASSERT_TRUE(Watcher().Watch(view).has_value());
  EXPECT_TRUE(Watcher().Poll().empty());

  WriteFile(view, "<Changed/>");
  EXPECT_EQ(PollFor(Watcher()), std::vector{std::filesystem::weakly_canonical(view)});

  // Editors often write a new file and rename it over the old one.
  const auto temporary = Directory() / "view.xml.tmp";
  WriteFile(temporary, "<Renamed over/>");
  std::filesystem::rename(temporary, view);
  EXPECT_EQ(PollFor(Watcher()), std::vector{std::filesystem::weakly_canonical(view)});

  WriteFile(Directory() / "other.xml", "<Unwatched/>");
  EXPECT_TRUE(PollFor(Watcher(), std::chrono::milliseconds{150}).empty());

  Watcher().Unwatch(view);
  WriteFile(view, "<Ignored now/>");
  EXPECT_TRUE(PollFor(Watcher(), std::chrono::milliseconds{150}).empty());
}

INSTANTIATE_TEST_SUITE_P(Watchers, FileWatcherTest, ::testing::Values(false, true), [](const auto& parameter) {
  return std::string{parameter.param ? "Polling" : "Native"};
});

}  // namespace
}  // namespace flint::platform
