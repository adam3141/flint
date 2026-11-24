#include "flint/log/pattern_formatter.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <format>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "flint/log/category.hpp"
#include "flint/log/level.hpp"
#include "flint/log/record.hpp"

namespace flint::log {
namespace {

// Must match PatternFormatter::Field, starting at Timestamp (1).
constexpr std::array<std::string_view, 18> kFieldNames{
    "timestamp",
    "iso8601",
    "date",
    "time",
    "level",
    "severity",
    "category",
    "message",
    "file",
    "filename",
    "line",
    "function",
    "func",
    "thread",
    "threadname",
    "uptime",
    "sysuptime",
    "sequence",
};

auto Seconds(std::chrono::nanoseconds duration) -> double {
  return std::chrono::duration<double>{duration}.count();
}

auto AppendLocalTime(std::string& out, std::chrono::system_clock::time_point time, std::string_view format) -> void {
  const auto rounded = std::chrono::floor<std::chrono::microseconds>(time);
  // current_zone() throws only if the time-zone database is unusable; fall back to UTC rather than fail to log.
  try {
    const std::chrono::zoned_time local{std::chrono::current_zone(), rounded};
    std::vformat_to(std::back_inserter(out), format, std::make_format_args(local));
  } catch (const std::runtime_error&) {
    std::vformat_to(std::back_inserter(out), format, std::make_format_args(rounded));
  }
}

auto AppendUtcTime(std::string& out, std::chrono::system_clock::time_point time, std::string_view format) -> void {
  const auto rounded = std::chrono::floor<std::chrono::microseconds>(time);
  std::vformat_to(std::back_inserter(out), format, std::make_format_args(rounded));
}

// Formats a timestamp on the requested clock face.
auto FormatClock(std::chrono::system_clock::time_point time, TimeZone timeZone, std::string_view format) -> std::string {
  std::string text;
  if (timeZone == TimeZone::Local) {
    AppendLocalTime(text, time, format);
  } else {
    AppendUtcTime(text, time, format);
  }
  return text;
}

}  // namespace

auto PatternError::Describe() const -> std::string {
  switch (code) {
    case Code::UnknownField:
      return std::format("unknown pattern field '{}' at offset {}", field, offset);
    case Code::UnterminatedField:
      return std::format("unterminated '{{' at offset {}", offset);
    case Code::InvalidSpec:
      return std::format("invalid format spec for field '{}' at offset {}", field, offset);
  }
  return "invalid pattern";
}

auto AnsiSequence(Colour colour) noexcept -> std::string_view {
  switch (colour) {
    case Colour::Default:
      return {};
    case Colour::Black:
      return "\x1b[30m";
    case Colour::Red:
      return "\x1b[31m";
    case Colour::Green:
      return "\x1b[32m";
    case Colour::Yellow:
      return "\x1b[33m";
    case Colour::Blue:
      return "\x1b[34m";
    case Colour::Magenta:
      return "\x1b[35m";
    case Colour::Cyan:
      return "\x1b[36m";
    case Colour::White:
      return "\x1b[37m";
    case Colour::Grey:
      return "\x1b[90m";
    case Colour::BrightRed:
      return "\x1b[91m";
    case Colour::BrightGreen:
      return "\x1b[92m";
    case Colour::BrightYellow:
      return "\x1b[93m";
    case Colour::BrightBlue:
      return "\x1b[94m";
    case Colour::BrightMagenta:
      return "\x1b[95m";
    case Colour::BrightCyan:
      return "\x1b[96m";
    case Colour::BrightWhite:
      return "\x1b[97m";
    case Colour::Alarm:
      return "\x1b[1;97;41m";
  }
  return {};
}

PatternFormatter::PatternFormatter(std::vector<Segment> segments, TimeZone timeZone, Colouring colouring)
    : m_segments{std::move(segments)}, m_timeZone{timeZone}, m_colouring{colouring} {}

auto PatternFormatter::Create(std::string_view pattern, TimeZone timeZone, Colouring colouring)
    -> std::expected<PatternFormatter, PatternError> {
  std::vector<Segment> segments;
  std::string literal;
  const auto flushLiteral = [&] {
    if (!literal.empty()) {
      segments.push_back(Segment{.field = Field::Literal, .text = std::exchange(literal, {})});
    }
  };

  for (std::size_t i = 0; i < pattern.size(); ++i) {
    const char character = pattern[i];
    if (character == '}' && i + 1 < pattern.size() && pattern[i + 1] == '}') {
      literal.push_back('}');
      ++i;
      continue;
    }
    if (character != '{') {
      literal.push_back(character);
      continue;
    }
    if (i + 1 < pattern.size() && pattern[i + 1] == '{') {
      literal.push_back('{');
      ++i;
      continue;
    }
    const std::size_t close = pattern.find('}', i);
    if (close == std::string_view::npos) {
      return std::unexpected(PatternError{.code = PatternError::Code::UnterminatedField, .offset = i, .field = {}});
    }
    const std::string_view body = pattern.substr(i + 1, close - i - 1);
    const std::size_t colon = body.find(':');
    const std::string_view name = body.substr(0, colon);
    const std::string_view spec = colon == std::string_view::npos ? std::string_view{} : body.substr(colon + 1);

    const auto* found = std::ranges::find(kFieldNames, name);
    if (found == kFieldNames.end()) {
      return std::unexpected(PatternError{.code = PatternError::Code::UnknownField, .offset = i, .field = std::string{name}});
    }
    std::string format;
    if (!spec.empty()) {
      format = std::format("{{:{}}}", spec);
      // Validate the spec now, so formatting a record can never fail.
      try {
        static_cast<void>(std::vformat(format, std::make_format_args(name)));
      } catch (const std::format_error&) {
        return std::unexpected(PatternError{.code = PatternError::Code::InvalidSpec, .offset = i, .field = std::string{name}});
      }
    }
    flushLiteral();
    const auto field = static_cast<Field>(static_cast<std::size_t>(found - kFieldNames.begin()) + 1);
    segments.push_back(Segment{.field = field, .text = std::move(format)});
    i = close;
  }
  flushLiteral();
  return PatternFormatter{std::move(segments), timeZone, colouring};
}

auto PatternFormatter::Format(const Record& record, std::string& out) const -> void {
  for (const Segment& segment : m_segments) {
    AppendField(segment, record, out);
  }
}

auto PatternFormatter::AppendField(const Segment& segment, const Record& record, std::string& out) const -> void {
  const bool ansi = m_colouring == Colouring::Ansi;
  const auto appendText = [&](std::string_view text, std::string_view style = {}) {
    if (ansi && !style.empty()) {
      out.append(style);
    }
    if (segment.text.empty()) {
      out.append(text);
    } else {
      std::vformat_to(std::back_inserter(out), segment.text, std::make_format_args(text));
    }
    if (ansi && !style.empty()) {
      out.append(kAnsiReset);
    }
  };
  const auto appendNumber = [&](auto number, std::string_view numberFormat, std::string_view style = {}) {
    appendText(std::vformat(numberFormat, std::make_format_args(number)), style);
  };

  switch (segment.field) {
    case Field::Literal:
      out.append(segment.text);
      break;
    case Field::Timestamp:
      appendText(FormatClock(record.timestamp, m_timeZone, "{:%F %T}"));
      break;
    case Field::Iso8601:
      appendText(FormatClock(record.timestamp, TimeZone::Utc, "{:%FT%TZ}"));
      break;
    case Field::Date:
      appendText(FormatClock(record.timestamp, m_timeZone, "{:%F}"));
      break;
    case Field::Time: {
      std::string text = FormatClock(record.timestamp, m_timeZone, "{:%T}");
      // Microseconds are noise on a terminal; keep milliseconds.
      constexpr std::size_t kMicrosecondDigits = 3;
      text.resize(text.size() > kMicrosecondDigits ? text.size() - kMicrosecondDigits : text.size());
      appendText(text, kAnsiDim);
      break;
    }
    case Field::Level: {
      const auto name = LevelName(record.level);
      const std::string fallback = name.has_value() ? std::string{} : std::format("L{}", record.level.severity);
      appendText(name.value_or(fallback), AnsiSequence(LevelColour(record.level)));
      break;
    }
    case Field::Severity:
      appendNumber(record.level.severity, "{}");
      break;
    case Field::Category:
      appendText(CategoryName(record.category), AnsiSequence(Colour::BrightBlue));
      break;
    case Field::Message:
      appendText(record.message, record.level >= kWarning ? AnsiSequence(LevelColour(record.level)) : std::string_view{});
      break;
    case Field::File:
      appendText(record.source.file, kAnsiDim);
      break;
    case Field::FileName:
      appendText(FileNameOf(record.source.file), kAnsiDim);
      break;
    case Field::Line:
      appendNumber(record.source.line, "{}", kAnsiDim);
      break;
    case Field::Function:
      appendText(record.source.function, kAnsiDim);
      break;
    case Field::Func:
      appendText(ShortFunctionName(record.source.function), kAnsiDim);
      break;
    case Field::Thread:
      appendNumber(record.threadId, "{}");
      break;
    case Field::ThreadName:
      appendText(record.threadName);
      break;
    case Field::Uptime:
      appendNumber(Seconds(record.applicationUptime), "{:.6f}");
      break;
    case Field::SysUptime:
      appendNumber(Seconds(record.systemUptime), "{:.6f}");
      break;
    case Field::Sequence:
      appendNumber(record.sequence, "{}");
      break;
  }
}

}  // namespace flint::log
