#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <variant>

#include "flint/log/pattern_formatter.hpp"
#include "flint/log/record.hpp"
#include "flint/log/rotating_file.hpp"
#include "flint/log/sink_interface.hpp"

namespace flint::log {

/// Options for a FileSink.
struct FileSinkOptions {
  std::filesystem::path path{"app.log"};
  std::string pattern{PatternFormatter::kFilePattern};
  TimeZone timeZone{TimeZone::Local};
  RotationOptions rotation;
};

/// Why a file sink could not be created.
using FileSinkError = std::variant<std::error_code, PatternError>;

/// Writes records to a text file, one line each, with optional size-based rotation.
class FileSink final : public SinkInterface {
public:
  /// Opens the file and compiles the pattern.
  /// @param options Path, pattern and rotation.
  [[nodiscard]] static auto Create(const FileSinkOptions& options) -> std::expected<std::unique_ptr<FileSink>, FileSinkError>;

  FileSink(RotatingFile file, PatternFormatter formatter);

  auto Write(const Record& record) -> void override;
  auto Flush() -> void override;

private:
  RotatingFile m_file;
  PatternFormatter m_formatter;
  std::string m_buffer;
};

}  // namespace flint::log
