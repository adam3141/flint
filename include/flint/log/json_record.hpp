#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "flint/log/record.hpp"

namespace flint::log {

/// Appends a record to `out` as one compact JSON object, without a trailing newline. This is the format written by
/// JsonFileSink and NetworkSink (JSON Lines). Fields:
///
/// ```json
/// {"timestamp":"2026-11-24T10:15:03.123456Z","level":"Info","severity":300,"category":"app","message":"...",
///  "file":"/src/main.cpp","line":12,"column":5,"function":"int main()","thread":4242,"threadName":"main",
///  "appUptimeNs":1234567,"systemUptimeNs":86400123456789,"sequence":1,"pid":4242,"host":"devbox"}
/// ```
/// @param record The record.
/// @param out The buffer to append to.
auto AppendJsonRecord(const Record& record, std::string& out) -> void;

/// A record rebuilt from JSON, with the storage its source-position views refer to.
struct OwnedRecord {
  Record record;
  std::string file;
  std::string function;
  std::uint32_t line{0};
  std::uint32_t column{0};

  /// Points the record's source position at this object's strings and returns the record. Call again after moving.
  [[nodiscard]] auto Get() -> const Record& {
    record.source = SourcePosition{.file = file, .function = function, .line = line, .column = column};
    return record;
  }
};

/// Why a line could not be read as a record.
enum class JsonRecordError : std::uint8_t {
  /// Not a JSON object, or a field has the wrong type.
  NotARecord,
  MissingMessage,
  BadField,
};

/// Rebuilds a record from a line written by AppendJsonRecord. Unknown fields are ignored, so newer writers can add
/// fields. Unknown level names are registered as custom levels using the record's severity.
/// @param line One JSON object.
[[nodiscard]] auto RecordFromJson(std::string_view line) -> std::expected<OwnedRecord, JsonRecordError>;

}  // namespace flint::log
