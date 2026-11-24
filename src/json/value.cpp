#include "flint/json/value.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace flint::json {

Object::Object(std::initializer_list<Member> members) : m_members{members} {}

auto Object::Find(std::string_view key) const -> const Value* {
  const auto found = std::ranges::find(m_members, key, &Member::key);
  return found == m_members.end() ? nullptr : &found->value;
}

auto Object::Find(std::string_view key) -> Value* {
  const auto found = std::ranges::find(m_members, key, &Member::key);
  return found == m_members.end() ? nullptr : &found->value;
}

auto Object::Set(std::string key, Value value) -> void {
  if (Value* existing = Find(key); existing != nullptr) {
    *existing = std::move(value);
    return;
  }
  m_members.push_back(Member{.key = std::move(key), .value = std::move(value)});
}

auto Object::Erase(std::string_view key) -> bool {
  return std::erase_if(m_members, [key](const Member& member) { return member.key == key; }) > 0;
}

auto Object::Members() const noexcept -> std::span<const Member> {
  return m_members;
}

auto Object::Size() const noexcept -> std::size_t {
  return m_members.size();
}

auto Object::IsEmpty() const noexcept -> bool {
  return m_members.empty();
}

auto operator==(const Object& left, const Object& right) -> bool {
  return left.m_members == right.m_members;
}

auto Value::GetKind() const noexcept -> Kind {
  return static_cast<Kind>(m_data.index());
}

auto Value::AsBool() const noexcept -> std::optional<bool> {
  if (const auto* value = std::get_if<bool>(&m_data); value != nullptr) {
    return *value;
  }
  return std::nullopt;
}

auto Value::AsInteger() const noexcept -> std::optional<std::int64_t> {
  if (const auto* value = std::get_if<std::int64_t>(&m_data); value != nullptr) {
    return *value;
  }
  if (const auto* value = std::get_if<double>(&m_data); value != nullptr) {
    // 2^63 is exactly representable as a double; anything at or above it does not fit.
    constexpr double kLimit = 9223372036854775808.0;
    if (std::trunc(*value) == *value && *value >= -kLimit && *value < kLimit) {
      return static_cast<std::int64_t>(*value);
    }
  }
  return std::nullopt;
}

auto Value::AsDouble() const noexcept -> std::optional<double> {
  if (const auto* value = std::get_if<double>(&m_data); value != nullptr) {
    return *value;
  }
  if (const auto* value = std::get_if<std::int64_t>(&m_data); value != nullptr) {
    return static_cast<double>(*value);
  }
  return std::nullopt;
}

auto Value::AsString() const noexcept -> std::optional<std::string_view> {
  if (const auto* value = std::get_if<std::string>(&m_data); value != nullptr) {
    return std::string_view{*value};
  }
  return std::nullopt;
}

auto Value::AsArray() const noexcept -> const Array* {
  return std::get_if<Array>(&m_data);
}

auto Value::AsArray() noexcept -> Array* {
  return std::get_if<Array>(&m_data);
}

auto Value::AsObject() const noexcept -> const Object* {
  return std::get_if<Object>(&m_data);
}

auto Value::AsObject() noexcept -> Object* {
  return std::get_if<Object>(&m_data);
}

auto Value::Find(std::string_view key) const -> const Value* {
  const Object* object = AsObject();
  return object == nullptr ? nullptr : object->Find(key);
}

auto operator==(const Value& left, const Value& right) -> bool {
  return left.m_data == right.m_data;
}

auto KindName(Kind kind) noexcept -> std::string_view {
  switch (kind) {
    case Kind::Null:
      return "null";
    case Kind::Bool:
      return "boolean";
    case Kind::Integer:
      return "integer";
    case Kind::Double:
      return "number";
    case Kind::String:
      return "string";
    case Kind::Array:
      return "array";
    case Kind::Object:
      return "object";
  }
  return "unknown";
}

}  // namespace flint::json
