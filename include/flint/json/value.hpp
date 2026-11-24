#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace flint::json {

/// The kind of value held by a json::Value.
enum class Kind : std::uint8_t {
  Null,
  Bool,
  Integer,
  Double,
  String,
  Array,
  Object,
};

class Value;
struct Member;

/// A JSON array.
using Array = std::vector<Value>;

/// A JSON object. Members keep the order in which they were parsed or added, which keeps configuration files and theme
/// files diff-friendly when they are written back out. Lookup is linear, which is faster than hashing for the small
/// objects found in configuration.
class Object {
public:
  Object() = default;
  Object(std::initializer_list<Member> members);

  /// Returns the value of a member, or nullptr if the object has no member with that key.
  /// @param key The member name.
  [[nodiscard]] auto Find(std::string_view key) const -> const Value*;

  /// Returns the value of a member for modification, or nullptr if the object has no member with that key.
  /// @param key The member name.
  [[nodiscard]] auto Find(std::string_view key) -> Value*;

  /// Sets a member, replacing the value of an existing member with the same key.
  /// @param key The member name.
  /// @param value The member value.
  auto Set(std::string key, Value value) -> void;

  /// Removes a member if present.
  /// @param key The member name.
  /// @return true if a member was removed.
  auto Erase(std::string_view key) -> bool;

  /// Returns the members in order.
  [[nodiscard]] auto Members() const noexcept -> std::span<const Member>;

  [[nodiscard]] auto Size() const noexcept -> std::size_t;
  [[nodiscard]] auto IsEmpty() const noexcept -> bool;

  friend auto operator==(const Object& left, const Object& right) -> bool;

private:
  std::vector<Member> m_members;
};

/// A JSON value. Integers that fit in 64 bits are kept exactly; other numbers are stored as double.
class Value {
public:
  Value() noexcept;

  // Implicit conversions let JSON be written naturally: Object{{"name", "Topaz"}, {"dark", true}}.
  // Defined after Member, because constructing the variant needs Member to be complete.
  // NOLINTBEGIN(google-explicit-constructor)
  Value(std::nullptr_t) noexcept;
  Value(bool value) noexcept;
  Value(std::int64_t value) noexcept;
  Value(int value) noexcept;
  Value(std::uint32_t value) noexcept;
  Value(double value) noexcept;
  Value(std::string value) noexcept;
  Value(std::string_view value);
  Value(const char* value);
  Value(Array value) noexcept;
  Value(Object value) noexcept;
  // NOLINTEND(google-explicit-constructor)

  [[nodiscard]] auto GetKind() const noexcept -> Kind;

  [[nodiscard]] auto IsNull() const noexcept -> bool { return GetKind() == Kind::Null; }

  [[nodiscard]] auto IsBool() const noexcept -> bool { return GetKind() == Kind::Bool; }

  /// True for integer and floating-point values.
  [[nodiscard]] auto IsNumber() const noexcept -> bool { return GetKind() == Kind::Integer || GetKind() == Kind::Double; }

  [[nodiscard]] auto IsString() const noexcept -> bool { return GetKind() == Kind::String; }

  [[nodiscard]] auto IsArray() const noexcept -> bool { return GetKind() == Kind::Array; }

  [[nodiscard]] auto IsObject() const noexcept -> bool { return GetKind() == Kind::Object; }

  /// Returns the boolean, or nothing if this is not a boolean.
  [[nodiscard]] auto AsBool() const noexcept -> std::optional<bool>;

  /// Returns the number as an integer. Doubles convert only when they hold an exact integer value in range.
  [[nodiscard]] auto AsInteger() const noexcept -> std::optional<std::int64_t>;

  /// Returns the number as a double. Integers convert, possibly losing precision above 2^53.
  [[nodiscard]] auto AsDouble() const noexcept -> std::optional<double>;

  /// Returns a view of the string, valid while this value is unchanged, or nothing if this is not a string.
  [[nodiscard]] auto AsString() const noexcept -> std::optional<std::string_view>;

  /// Returns the array, or nullptr if this is not an array.
  [[nodiscard]] auto AsArray() const noexcept -> const Array*;

  /// Returns the array for modification, or nullptr if this is not an array.
  [[nodiscard]] auto AsArray() noexcept -> Array*;

  /// Returns the object, or nullptr if this is not an object.
  [[nodiscard]] auto AsObject() const noexcept -> const Object*;

  /// Returns the object for modification, or nullptr if this is not an object.
  [[nodiscard]] auto AsObject() noexcept -> Object*;

  /// Looks up an object member. Returns nullptr if this is not an object or the member is missing.
  /// @param key The member name.
  [[nodiscard]] auto Find(std::string_view key) const -> const Value*;

  friend auto operator==(const Value& left, const Value& right) -> bool;

private:
  // No default member initializer: it would instantiate the variant before Member is complete, which clang rejects.
  // A default-constructed variant already holds its first alternative, null.
  std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, Array, Object> m_data;
};

/// A named member of a JSON object.
struct Member {
  std::string key;
  Value value;

  friend auto operator==(const Member& left, const Member& right) -> bool = default;
};

inline Value::Value() noexcept = default;

inline Value::Value(std::nullptr_t) noexcept {}

inline Value::Value(bool value) noexcept : m_data{value} {}

inline Value::Value(std::int64_t value) noexcept : m_data{value} {}

inline Value::Value(int value) noexcept : m_data{std::int64_t{value}} {}

inline Value::Value(std::uint32_t value) noexcept : m_data{std::int64_t{value}} {}

inline Value::Value(double value) noexcept : m_data{value} {}

inline Value::Value(std::string value) noexcept : m_data{std::move(value)} {}

inline Value::Value(std::string_view value) : m_data{std::string{value}} {}

inline Value::Value(const char* value) : m_data{std::string{value}} {}

inline Value::Value(Array value) noexcept : m_data{std::move(value)} {}

inline Value::Value(Object value) noexcept : m_data{std::move(value)} {}

/// Returns the name of a value kind, for error messages: "null", "boolean", "integer", "number", "string", "array",
/// "object".
[[nodiscard]] auto KindName(Kind kind) noexcept -> std::string_view;

}  // namespace flint::json
