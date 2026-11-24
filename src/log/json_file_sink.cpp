#include "flint/log/json_file_sink.hpp"

#include <expected>
#include <memory>
#include <system_error>
#include <utility>

#include "flint/log/json_record.hpp"
#include "flint/log/record.hpp"
#include "flint/log/rotating_file.hpp"

namespace flint::log {

auto JsonFileSink::Create(const JsonFileSinkOptions& options) -> std::expected<std::unique_ptr<JsonFileSink>, std::error_code> {
  return RotatingFile::Open(options.path, options.rotation).transform([](RotatingFile file) {
    return std::make_unique<JsonFileSink>(std::move(file));
  });
}

JsonFileSink::JsonFileSink(RotatingFile file) : m_file{std::move(file)} {}

auto JsonFileSink::Write(const Record& record) -> void {
  m_buffer.clear();
  AppendJsonRecord(record, m_buffer);
  m_buffer.push_back('\n');
  m_file.Append(m_buffer);
}

auto JsonFileSink::Flush() -> void {
  m_file.Flush();
}

}  // namespace flint::log
