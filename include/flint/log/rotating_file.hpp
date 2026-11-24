#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

namespace flint::log {

/// Size-based rotation for log files.
struct RotationOptions {
  static constexpr std::size_t kDefaultMaxFiles = 5;

  /// Rotate when the file would grow past this many bytes. Zero disables rotation.
  std::uintmax_t maxBytes{0};
  /// Rotated files to keep: app.1.log (newest) to app.N.log (oldest).
  std::size_t maxFiles{kDefaultMaxFiles};
};

/// An append-only file that rotates by size. Used by the text and JSON file sinks.
class RotatingFile {
public:
  /// Opens (creating directories as needed) a file for appending.
  /// @param path The active log file.
  /// @param rotation When to rotate and how many old files to keep.
  /// @return The open file, or the filesystem error.
  [[nodiscard]] static auto Open(std::filesystem::path path, RotationOptions rotation) -> std::expected<RotatingFile, std::error_code>;

  /// Appends text, rotating first if it would overflow the size limit.
  /// @param text The bytes to append.
  auto Append(std::string_view text) -> void;

  auto Flush() -> void;

  [[nodiscard]] auto GetPath() const noexcept -> const std::filesystem::path& { return m_path; }

private:
  RotatingFile(std::filesystem::path path, RotationOptions rotation, std::ofstream stream, std::uintmax_t size);

  auto Rotate() -> void;
  [[nodiscard]] auto RotatedPath(std::size_t index) const -> std::filesystem::path;

  std::filesystem::path m_path;
  RotationOptions m_rotation;
  std::ofstream m_stream;
  std::uintmax_t m_size{0};
};

}  // namespace flint::log
