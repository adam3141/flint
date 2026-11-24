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

#include "flint/json/value.hpp"
#include "flint/json/writer.hpp"
#include "flint/log/category.hpp"
#include "flint/log/level.hpp"
#include "flint/log/record.hpp"
#include "flint/platform/process.hpp"

namespace flint::log {
namespace {

struct ProcessIdentity {
  std::uint64_t pid{platform::CurrentProcessId()};
  std::string host{platform::HostName()};
};

auto Identity() -> const ProcessIdentity& {
  static const ProcessIdentity kIdentity;
  return kIdentity;
}

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
  json::Writer writer{out};
  writer.BeginObject();
  writer.Key("timestamp");
  std::string timestamp;
  std::format_to(std::back_inserter(timestamp), "{:%FT%TZ}", std::chrono::floor<std::chrono::microseconds>(record.timestamp));
  writer.String(timestamp);
  writer.Key("level");
  const auto name = LevelName(record.level);
  writer.String(name.has_value() ? *name : std::format("L{}", record.level.severity));
  writer.Key("severity");
  writer.Unsigned(record.level.severity);
  writer.Key("category");
  writer.String(CategoryName(record.category));
  writer.Key("message");
  writer.String(record.message);
  writer.Key("file");
  writer.String(record.source.file);
  writer.Key("line");
  writer.Unsigned(record.source.line);
  writer.Key("column");
  writer.Unsigned(record.source.column);
  writer.Key("function");
  writer.String(record.source.function);
  writer.Key("thread");
  writer.Unsigned(record.threadId);
  writer.Key("threadName");
  writer.String(record.threadName);
  writer.Key("appUptimeNs");
  writer.Integer(record.applicationUptime.count());
  writer.Key("systemUptimeNs");
  writer.Integer(record.systemUptime.count());
  writer.Key("sequence");
  writer.Unsigned(record.sequence);
  writer.Key("pid");
  writer.Unsigned(Identity().pid);
  writer.Key("host");
  writer.String(Identity().host);
  writer.EndObject();
}

auto RecordFromJson(const json::Value& value) -> std::expected<OwnedRecord, JsonRecordError> {
  if (!value.IsObject()) {
    return std::unexpected(JsonRecordError::NotAnObject);
  }
  const auto text = [&value](std::string_view key) -> std::string_view {
    const json::Value* member = value.Find(key);
    return member == nullptr ? std::string_view{} : member->AsString().value_or(std::string_view{});
  };
  const auto integer = [&value](std::string_view key) -> std::int64_t {
    const json::Value* member = value.Find(key);
    return member == nullptr ? 0 : member->AsInteger().value_or(0);
  };

  const json::Value* messageValue = value.Find("message");
  const auto message = messageValue == nullptr ? std::nullopt : messageValue->AsString();
  if (!message.has_value()) {
    return std::unexpected(JsonRecordError::MissingMessage);
  }

  OwnedRecord owned;
  owned.file = std::string{text("file")};
  owned.function = std::string{text("function")};
  owned.line = static_cast<std::uint32_t>(integer("line"));
  owned.column = static_cast<std::uint32_t>(integer("column"));
  Record& record = owned.record;
  record.message = std::string{*message};

  const std::int64_t severity = integer("severity");
  if (severity <= 0 || std::cmp_greater_equal(severity, kOff.severity)) {
    return std::unexpected(JsonRecordError::BadField);
  }
  record.level = Level{.severity = static_cast<std::uint16_t>(severity)};
  if (const std::string_view levelName = text("level"); !levelName.empty() && !LevelName(record.level).has_value()) {
    // A custom level from another process; register it so it prints by name. Conflicts just print by number.
    [[maybe_unused]] const auto registered = RegisterLevel(levelName, record.level.severity);
  }
  if (const std::string_view categoryName = text("category"); !categoryName.empty()) {
    record.category = RegisterCategory(categoryName).value_or(kDefaultCategory);
  }
  record.timestamp = ParseIso8601(text("timestamp")).value_or(std::chrono::system_clock::now());
  record.applicationUptime = std::chrono::nanoseconds{integer("appUptimeNs")};
  record.systemUptime = std::chrono::nanoseconds{integer("systemUptimeNs")};
  record.threadId = static_cast<std::uint64_t>(integer("thread"));
  record.threadName = std::string{text("threadName")};
  record.sequence = static_cast<std::uint64_t>(integer("sequence"));
  return owned;
}

}  // namespace flint::log
