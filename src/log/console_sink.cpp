#include "flint/log/console_sink.hpp"

#include <expected>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "flint/log/pattern_formatter.hpp"
#include "flint/log/record.hpp"
#include "flint/platform/terminal.hpp"

namespace flint::log {
namespace {

auto StreamFor(platform::StandardStream stream) -> std::ostream& {
  return stream == platform::StandardStream::Output ? std::cout : std::cerr;
}

auto ResolveColouring(ColourMode mode, platform::StandardStream stream) -> Colouring {
  switch (mode) {
    case ColourMode::Always:
      return Colouring::Ansi;
    case ColourMode::Never:
      return Colouring::Plain;
    case ColourMode::Auto:
      return platform::SupportsAnsiColour(stream) ? Colouring::Ansi : Colouring::Plain;
  }
  return Colouring::Plain;
}

}  // namespace

auto ConsoleSink::Create(const ConsoleSinkOptions& options) -> std::expected<std::unique_ptr<ConsoleSink>, PatternError> {
  return PatternFormatter::Create(options.pattern, options.timeZone, ResolveColouring(options.colour, options.stream))
      .transform([&options](PatternFormatter formatter) { return std::make_unique<ConsoleSink>(options.stream, std::move(formatter)); });
}

ConsoleSink::ConsoleSink(platform::StandardStream stream, PatternFormatter formatter)
    : m_stream{stream}, m_formatter{std::move(formatter)} {}

auto ConsoleSink::Write(const Record& record) -> void {
  m_buffer.clear();
  m_formatter.Format(record, m_buffer);
  m_buffer.push_back('\n');
  StreamFor(m_stream).write(m_buffer.data(), static_cast<std::streamsize>(m_buffer.size()));
}

auto ConsoleSink::Flush() -> void {
  StreamFor(m_stream).flush();
}

}  // namespace flint::log
