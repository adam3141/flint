#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "flint/json/value.hpp"

namespace flint::json {

/// Output layout for serialised JSON.
struct WriteOptions {
  /// Spaces per nesting level. Zero writes compact JSON on a single line.
  int indent{0};
};

/// Serialises a value.
/// @param value The value to write.
/// @param options Layout options.
/// @return The JSON text.
[[nodiscard]] auto Serialise(const Value& value, WriteOptions options = {}) -> std::string;

/// Appends a string to `out` as a quoted JSON string literal, escaping as RFC 8259 requires.
/// @param out The buffer to append to.
/// @param text UTF-8 text.
auto AppendQuoted(std::string& out, std::string_view text) -> void;

/// Writes JSON directly into a string buffer without building a Value tree. Used on hot paths such as log sinks.
/// The writer inserts commas and, when indenting, newlines. Calls must be balanced; misuse is a programmer error and is
/// caught by assertions.
///
/// @code
/// std::string out;
/// json::Writer writer{out};
/// writer.BeginObject();
/// writer.Key("level");
/// writer.String("Info");
/// writer.EndObject();
/// @endcode
class Writer {
public:
  /// @param out The buffer to append to. It must outlive the writer.
  /// @param options Layout options.
  explicit Writer(std::string& out, WriteOptions options = {});

  auto BeginObject() -> void;
  auto EndObject() -> void;
  auto BeginArray() -> void;
  auto EndArray() -> void;

  /// Writes an object member name. The next call writes its value.
  auto Key(std::string_view key) -> void;

  auto Null() -> void;
  auto Bool(bool value) -> void;
  auto Integer(std::int64_t value) -> void;
  auto Unsigned(std::uint64_t value) -> void;
  auto Double(double value) -> void;
  auto String(std::string_view value) -> void;

  /// Writes a complete value tree.
  auto Write(const Value& value) -> void;

private:
  enum class Scope : std::uint8_t {
    InObject,
    InArray,
  };

  auto BeforeValue() -> void;
  auto NewLine() -> void;

  std::string* m_out{nullptr};
  WriteOptions m_options;
  // One entry per open container: its kind and whether it has any elements yet.
  std::vector<std::pair<Scope, bool>> m_scopes;
  bool m_afterKey{false};
};

}  // namespace flint::json
