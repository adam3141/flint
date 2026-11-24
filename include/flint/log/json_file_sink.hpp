#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

#include "flint/log/record.hpp"
#include "flint/log/rotating_file.hpp"
#include "flint/log/sink_interface.hpp"

namespace flint::log {

/// Options for a JsonFileSink.
struct JsonFileSinkOptions {
  std::filesystem::path path{"app.jsonl"};
  RotationOptions rotation;
};

/// Writes records to a JSON Lines file: one JSON object per line (see AppendJsonRecord).
///
/// JSON Lines is used rather than one JSON array so the file is valid after every write, survives a crash, can be
/// appended to across runs, and can be streamed by tools such as jq without reading it whole.
class JsonFileSink final : public SinkInterface {
public:
  /// Opens the file.
  /// @param options Path and rotation.
  [[nodiscard]] static auto Create(const JsonFileSinkOptions& options) -> std::expected<std::unique_ptr<JsonFileSink>, std::error_code>;

  explicit JsonFileSink(RotatingFile file);

  auto Write(const Record& record) -> void override;
  auto Flush() -> void override;

private:
  RotatingFile m_file;
  std::string m_buffer;
};

}  // namespace flint::log
