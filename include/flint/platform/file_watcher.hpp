#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <system_error>
#include <vector>

namespace flint::platform {

/// Reports watched files that changed on disk, for hot reload (flint ADR-0014).
///
/// Editors often save by writing a new file and renaming it over the old one, so a watched file stays watched when it
/// is replaced, and also when it is deleted and created again. Poll() never blocks: call it from a timer or a frame
/// loop. Not thread-safe.
///
/// @code
/// auto watcher = flint::platform::FileWatcher::Create();
/// (void)watcher->Watch("views/main.xml");
/// ...
/// for (const std::filesystem::path& changed : watcher->Poll()) { Reload(changed); }
/// @endcode
class FileWatcher {
public:
  /// Creates the best watcher for this platform: inotify on Linux, otherwise CreatePolling().
  [[nodiscard]] static auto Create() -> std::unique_ptr<FileWatcher>;

  /// Creates a watcher that compares each file's modification time and size when polled. Works everywhere.
  [[nodiscard]] static auto CreatePolling() -> std::unique_ptr<FileWatcher>;

  FileWatcher() = default;
  virtual ~FileWatcher() = default;

  FileWatcher(const FileWatcher&) = delete;
  auto operator=(const FileWatcher&) -> FileWatcher& = delete;
  FileWatcher(FileWatcher&&) = delete;
  auto operator=(FileWatcher&&) -> FileWatcher& = delete;

  /// Starts watching a file. Watching a file twice is harmless.
  /// @param file The file, which must be in a directory that exists.
  virtual auto Watch(const std::filesystem::path& file) -> std::expected<void, std::error_code> = 0;

  /// Stops watching a file.
  virtual auto Unwatch(const std::filesystem::path& file) -> void = 0;

  /// Returns the watched files that changed since the last call, each once, as absolute paths. Never blocks.
  [[nodiscard]] virtual auto Poll() -> std::vector<std::filesystem::path> = 0;
};

}  // namespace flint::platform
