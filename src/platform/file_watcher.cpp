#include "flint/platform/file_watcher.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef __linux__
#include <array>
#include <cstring>

#include <string.h>  // NOLINT(modernize-deprecated-headers): strnlen is POSIX, not in <cstring>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace flint::platform {
namespace {

auto Absolute(const std::filesystem::path& file) -> std::filesystem::path {
  std::error_code error;
  const std::filesystem::path canonical = std::filesystem::weakly_canonical(file, error);
  return error ? std::filesystem::absolute(file).lexically_normal() : canonical;
}

// Compares modification times and sizes when polled.
class PollingFileWatcher final : public FileWatcher {
public:
  auto Watch(const std::filesystem::path& file) -> std::expected<void, std::error_code> override {
    if (!std::filesystem::is_directory(file.parent_path().empty() ? std::filesystem::path{"."} : file.parent_path())) {
      return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
    }
    const std::filesystem::path absolute = Absolute(file);
    m_files.try_emplace(absolute, Stamp(absolute));
    return {};
  }

  auto Unwatch(const std::filesystem::path& file) -> void override { m_files.erase(Absolute(file)); }

  [[nodiscard]] auto Poll() -> std::vector<std::filesystem::path> override {
    std::vector<std::filesystem::path> changed;
    for (auto& [file, stamp] : m_files) {
      const FileStamp now = Stamp(file);
      if (now != stamp) {
        stamp = now;
        changed.push_back(file);
      }
    }
    return changed;
  }

private:
  struct FileStamp {
    std::filesystem::file_time_type time;
    std::uintmax_t size{0};
    bool exists{false};

    friend auto operator==(const FileStamp& left, const FileStamp& right) -> bool = default;
  };

  static auto Stamp(const std::filesystem::path& file) -> FileStamp {
    std::error_code error;
    const auto time = std::filesystem::last_write_time(file, error);
    if (error) {
      return FileStamp{};
    }
    const auto size = std::filesystem::file_size(file, error);
    return FileStamp{.time = time, .size = error ? 0 : size, .exists = true};
  }

  std::map<std::filesystem::path, FileStamp> m_files;
};

#ifdef __linux__

// Watches the directories that hold the files, so saving by rename is seen.
class InotifyFileWatcher final : public FileWatcher {
public:
  explicit InotifyFileWatcher(int descriptor) : m_descriptor{descriptor} {}

  ~InotifyFileWatcher() override { ::close(m_descriptor); }

  InotifyFileWatcher(const InotifyFileWatcher&) = delete;
  auto operator=(const InotifyFileWatcher&) -> InotifyFileWatcher& = delete;
  InotifyFileWatcher(InotifyFileWatcher&&) = delete;
  auto operator=(InotifyFileWatcher&&) -> InotifyFileWatcher& = delete;

  auto Watch(const std::filesystem::path& file) -> std::expected<void, std::error_code> override {
    const std::filesystem::path absolute = Absolute(file);
    const std::filesystem::path directory = absolute.parent_path();
    auto found = std::ranges::find(m_directories, directory, &Directory::path);
    if (found == m_directories.end()) {
      constexpr std::uint32_t kEvents = IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_DELETE;
      const int watch = ::inotify_add_watch(m_descriptor, directory.c_str(), kEvents);
      if (watch < 0) {
        return std::unexpected(std::error_code{errno, std::generic_category()});
      }
      m_directories.push_back(Directory{.watch = watch, .path = directory, .names = {}});
      found = std::prev(m_directories.end());
    }
    found->names.insert(absolute.filename().string());
    return {};
  }

  auto Unwatch(const std::filesystem::path& file) -> void override {
    const std::filesystem::path absolute = Absolute(file);
    const auto found = std::ranges::find(m_directories, absolute.parent_path(), &Directory::path);
    if (found == m_directories.end()) {
      return;
    }
    found->names.erase(absolute.filename().string());
    if (found->names.empty()) {
      ::inotify_rm_watch(m_descriptor, found->watch);
      m_directories.erase(found);
    }
  }

  [[nodiscard]] auto Poll() -> std::vector<std::filesystem::path> override {
    std::set<std::filesystem::path> changed;
    alignas(inotify_event) std::array<char, kBufferSize> buffer{};
    while (true) {
      const ssize_t count = ::read(m_descriptor, buffer.data(), buffer.size());
      if (count <= 0) {
        break;  // EAGAIN: nothing more pending.
      }
      std::size_t offset = 0;
      while (offset + sizeof(inotify_event) <= static_cast<std::size_t>(count)) {
        inotify_event event{};
        std::memcpy(&event, &buffer.at(offset), sizeof(event));
        const std::string name{&buffer.at(offset + sizeof(inotify_event)),
                               ::strnlen(&buffer.at(offset + sizeof(inotify_event)), event.len)};
        Record(event.wd, name, changed);
        offset += sizeof(inotify_event) + event.len;
      }
    }
    return {changed.begin(), changed.end()};
  }

private:
  static constexpr std::size_t kBufferSize = std::size_t{16} * 1024;

  struct Directory {
    int watch{-1};
    std::filesystem::path path;
    std::set<std::string> names;
  };

  auto Record(int watch, const std::string& name, std::set<std::filesystem::path>& changed) const -> void {
    const auto found = std::ranges::find(m_directories, watch, &Directory::watch);
    if (found != m_directories.end() && found->names.contains(name)) {
      changed.insert(found->path / name);
    }
  }

  int m_descriptor;
  std::vector<Directory> m_directories;
};

#endif

}  // namespace

auto FileWatcher::Create() -> std::unique_ptr<FileWatcher> {
#ifdef __linux__
  if (const int descriptor = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC); descriptor >= 0) {
    return std::make_unique<InotifyFileWatcher>(descriptor);
  }
#endif
  return CreatePolling();
}

auto FileWatcher::CreatePolling() -> std::unique_ptr<FileWatcher> {
  return std::make_unique<PollingFileWatcher>();
}

}  // namespace flint::platform
