#include "flint/log/file_sink.hpp"

#include <expected>
#include <memory>
#include <string>
#include <utility>

#include "flint/log/pattern_formatter.hpp"
#include "flint/log/record.hpp"
#include "flint/log/rotating_file.hpp"

namespace flint::log {

auto FileSink::Create(const FileSinkOptions& options) -> std::expected<std::unique_ptr<FileSink>, FileSinkError> {
  auto formatter = PatternFormatter::Create(options.pattern, options.timeZone, Colouring::Plain);
  if (!formatter.has_value()) {
    return std::unexpected(FileSinkError{formatter.error()});
  }
  auto file = RotatingFile::Open(options.path, options.rotation);
  if (!file.has_value()) {
    return std::unexpected(FileSinkError{file.error()});
  }
  return std::make_unique<FileSink>(std::move(*file), std::move(*formatter));
}

FileSink::FileSink(RotatingFile file, PatternFormatter formatter) : m_file{std::move(file)}, m_formatter{std::move(formatter)} {}

auto FileSink::Write(const Record& record) -> void {
  m_buffer.clear();
  m_formatter.Format(record, m_buffer);
  m_buffer.push_back('\n');
  m_file.Append(m_buffer);
}

auto FileSink::Flush() -> void {
  m_file.Flush();
}

}  // namespace flint::log
