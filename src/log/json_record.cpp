#include "flint/log/json_record.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <format>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <glaze/glaze.hpp>

#include "flint/log/category.hpp"
#include "flint/log/level.hpp"
#include "flint/log/record.hpp"
#include "flint/platform/process.hpp"

namespace flint::log {
namespace detail {

// One record as written, field for field. glaze reads and writes it by reflection (outside an anonymous namespace,
// which clang tooling's fallback reflection cannot see into), so the member names are the JSON
// keys and their order is the order written.
template <typename Text>
struct WireRecord {  // NOLINT(misc-use-internal-linkage): glaze needs linkage under clang tooling (see above)
  Text timestamp;
  Text level;
  std::uint16_t severity{0};
  Text category;
  std::optional<Text> message;
  Text file;
  std::uint32_t line{0};
  std::uint32_t column{0};
  Text function;
  std::uint64_t thread{0};
  Text threadName;
  std::int64_t appUptimeNs{0};
  std::int64_t systemUptimeNs{0};
  std::uint64_t sequence{0};
  std::uint64_t pid{0};
  Text host;
};

}  // namespace detail

namespace {

using detail::WireRecord;

struct ProcessIdentity {
  std::uint64_t pid{platform::CurrentProcessId()};
  std::string host{platform::HostName()};
};

auto Identity() -> const ProcessIdentity& {
  static const ProcessIdentity kIdentity;
  return kIdentity;
}

// Lines are views into a larger buffer, and newer writers may add fields.
constexpr glz::opts kReadOptions{.null_terminated = false, .error_on_unknown_keys = false};

auto ParseIso8601(std::string_view text) -> std::optional<std::chrono::system_clock::time_point> {
  // Accepts the exact form AppendJsonRecord writes: 2026-11-24T10:15:03.123456Z.
  std::chrono::sys_time<std::chrono::microseconds> parsed;
  std::istringstream stream{std::string{text}};
  stream >> std::chrono::parse("%FT%TZ", parsed);
  if (stream.fail()) {
    return std::nullopt;
  }
  return std::chrono::time_point_cast<std::chrono::system_clock::duration>(parsed);
}

}  // namespace

auto AppendJsonRecord(const Record& record, std::string& out) -> void {
  std::string timestamp;
  std::format_to(std::back_inserter(timestamp), "{:%FT%TZ}", std::chrono::floor<std::chrono::microseconds>(record.timestamp));
  const auto name = LevelName(record.level);
  const std::string unnamedLevel = name.has_value() ? std::string{} : std::format("L{}", record.level.severity);
  const WireRecord<std::string_view> wire{
      .timestamp = timestamp,
      .level = name.has_value() ? *name : std::string_view{unnamedLevel},
      .severity = record.level.severity,
      .category = CategoryName(record.category),
      .message = record.message,
      .file = record.source.file,
      .line = record.source.line,
      .column = record.source.column,
      .function = record.source.function,
      .thread = record.threadId,
      .threadName = record.threadName,
      .appUptimeNs = record.applicationUptime.count(),
      .systemUptimeNs = record.systemUptime.count(),
      .sequence = record.sequence,
      .pid = Identity().pid,
      .host = Identity().host,
  };
  // glaze writes from the start of its buffer, so it gets a reused one per thread and the result is appended.
  thread_local std::string buffer;
  if (const glz::error_ctx error = glz::write_json(wire, buffer); error) {
    return;
  }
  out += buffer;
}

auto RecordFromJson(std::string_view line) -> std::expected<OwnedRecord, JsonRecordError> {
  WireRecord<std::string> wire;
  if (const glz::error_ctx error = glz::read<kReadOptions>(wire, line); error) {
    return std::unexpected(JsonRecordError::NotARecord);
  }
  if (!wire.message.has_value()) {
    return std::unexpected(JsonRecordError::MissingMessage);
  }
  if (wire.severity == 0 || wire.severity >= kOff.severity) {
    return std::unexpected(JsonRecordError::BadField);
  }
  OwnedRecord owned;
  owned.file = std::move(wire.file);
  owned.function = std::move(wire.function);
  owned.line = wire.line;
  owned.column = wire.column;
  Record& record = owned.record;
  record.message = std::move(*wire.message);
  record.level = Level{.severity = wire.severity};
  if (!wire.level.empty() && !LevelName(record.level).has_value()) {
    // A custom level from another process; register it so it prints by name. Conflicts just print by number.
    [[maybe_unused]] const auto registered = RegisterLevel(wire.level, record.level.severity);
  }
  if (!wire.category.empty()) {
    record.category = RegisterCategory(wire.category).value_or(kDefaultCategory);
  }
  record.timestamp = ParseIso8601(wire.timestamp).value_or(std::chrono::system_clock::now());
  record.applicationUptime = std::chrono::nanoseconds{wire.appUptimeNs};
  record.systemUptime = std::chrono::nanoseconds{wire.systemUptimeNs};
  record.threadId = wire.thread;
  record.threadName = std::move(wire.threadName);
  record.sequence = wire.sequence;
  return owned;
}

}  // namespace flint::log
