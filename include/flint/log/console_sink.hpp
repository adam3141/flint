#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <string>

#include "flint/log/pattern_formatter.hpp"
#include "flint/log/record.hpp"
#include "flint/log/sink_interface.hpp"
#include "flint/platform/terminal.hpp"

namespace flint::log {

/// When a console sink uses ANSI colour.
enum class ColourMode : std::uint8_t {
  /// Colour when the stream is a terminal, honouring NO_COLOR and FORCE_COLOR.
  Auto,
  Always,
  Never,
};

/// Options for a ConsoleSink.
struct ConsoleSinkOptions {
  platform::StandardStream stream{platform::StandardStream::Error};
  ColourMode colour{ColourMode::Auto};
  std::string pattern{PatternFormatter::kConsolePattern};
  TimeZone timeZone{TimeZone::Local};
};

/// Writes records to standard output or standard error, one line each, optionally in ANSI colour.
class ConsoleSink final : public SinkInterface {
public:
  /// Creates a console sink.
  /// @param options Stream, colour and pattern.
  /// @return The sink, or why the pattern is malformed.
  [[nodiscard]] static auto Create(const ConsoleSinkOptions& options = {}) -> std::expected<std::unique_ptr<ConsoleSink>, PatternError>;

  /// @param stream The stream to write to.
  /// @param formatter A compiled pattern; its Colouring decides whether ANSI colour is written.
  ConsoleSink(platform::StandardStream stream, PatternFormatter formatter);

  auto Write(const Record& record) -> void override;
  auto Flush() -> void override;

private:
  platform::StandardStream m_stream;
  PatternFormatter m_formatter;
  std::string m_buffer;
};

}  // namespace flint::log
