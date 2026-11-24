#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "flint/log/record.hpp"

namespace flint::log {

/// Which clock face timestamps are shown in.
enum class TimeZone : std::uint8_t {
  Local,
  Utc,
};

/// Whether a formatter adds ANSI colour escape sequences.
enum class Colouring : std::uint8_t {
  Plain,
  Ansi,
};

/// Why a pattern could not be compiled.
struct PatternError {
  enum class Code : std::uint8_t {
    UnknownField,
    UnterminatedField,
    InvalidSpec,
  };

  Code code{Code::UnknownField};
  /// Byte offset of the problem in the pattern.
  std::size_t offset{0};
  /// The field name involved, if any.
  std::string field;

  [[nodiscard]] auto Describe() const -> std::string;
};

/// Turns records into text using a pattern such as "{time} {level:<7} [{category}] {message}".
///
/// Fields are written `{name}` or `{name:spec}`, where spec is a std::format specification applied to the field's text
/// (for example `<7` to left-align in seven columns). `{{` and `}}` write literal braces. Fields:
///
/// | Field          | Example                                         |
/// |----------------|-------------------------------------------------|
/// | `timestamp`    | 2026-11-24 20:15:03.123456                      |
/// | `iso8601`      | 2026-11-24T10:15:03.123456Z (always UTC)        |
/// | `date`         | 2026-11-24                                      |
/// | `time`         | 20:15:03.123                                    |
/// | `level`        | Warning                                         |
/// | `severity`     | 400                                             |
/// | `category`     | layout                                          |
/// | `message`      | the message text                                |
/// | `file`         | /workspaces/silica/agate/src/panel.cpp          |
/// | `filename`     | panel.cpp                                       |
/// | `line`         | 42                                              |
/// | `function`     | void agate::Panel::Measure(agate::Size)         |
/// | `func`         | agate::Panel::Measure                           |
/// | `thread`       | 12345 (OS thread id)                            |
/// | `threadname`   | main                                            |
/// | `uptime`       | 12.345678 (application uptime, seconds)         |
/// | `sysuptime`    | 86400.123456 (machine uptime, seconds)          |
/// | `sequence`     | 17                                              |
///
/// With Colouring::Ansi, `level` is drawn in the level's colour, `message` in the level's colour for Warning and
/// above, and the source fields (`file`, `filename`, `line`, `function`, `func`) are dimmed.
class PatternFormatter {
public:
  /// The default pattern for terminals.
  static constexpr std::string_view kConsolePattern = "{time} {level:<7} {category:<8} {message}  {filename}:{line}";

  /// The default pattern for text files.
  static constexpr std::string_view kFilePattern =
      "{timestamp} +{uptime} {level:<7} [{category}] [{threadname}:{thread}] {message}  ({file}:{line} {func})";

  /// Compiles a pattern.
  /// @param pattern The pattern text.
  /// @param timeZone Clock face for the `timestamp`, `date` and `time` fields.
  /// @param colouring Whether to emit ANSI colour.
  /// @return The formatter, or where the pattern is malformed.
  [[nodiscard]] static auto Create(std::string_view pattern, TimeZone timeZone = TimeZone::Local, Colouring colouring = Colouring::Plain)
      -> std::expected<PatternFormatter, PatternError>;

  /// Appends a formatted record, without a trailing newline, to `out`.
  /// @param record The record.
  /// @param out The buffer to append to.
  auto Format(const Record& record, std::string& out) const -> void;

  [[nodiscard]] auto GetColouring() const noexcept -> Colouring { return m_colouring; }

private:
  enum class Field : std::uint8_t {
    Literal,
    Timestamp,
    Iso8601,
    Date,
    Time,
    Level,
    Severity,
    Category,
    Message,
    File,
    FileName,
    Line,
    Function,
    Func,
    Thread,
    ThreadName,
    Uptime,
    SysUptime,
    Sequence,
  };

  struct Segment {
    Field field{Field::Literal};
    // Literal text, or the std::format string ("{:<7}") applied to the field's text; empty means no spec.
    std::string text;
  };

  PatternFormatter(std::vector<Segment> segments, TimeZone timeZone, Colouring colouring);

  auto AppendField(const Segment& segment, const Record& record, std::string& out) const -> void;

  std::vector<Segment> m_segments;
  TimeZone m_timeZone{TimeZone::Local};
  Colouring m_colouring{Colouring::Plain};
};

/// Returns the ANSI SGR escape sequence that selects a colour, or an empty view for Colour::Default.
[[nodiscard]] auto AnsiSequence(Colour colour) noexcept -> std::string_view;

/// The ANSI sequence that resets colour and style.
inline constexpr std::string_view kAnsiReset = "\x1b[0m";

/// The ANSI sequence for dim text.
inline constexpr std::string_view kAnsiDim = "\x1b[2m";

}  // namespace flint::log
