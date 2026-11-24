#include "flint/json/writer.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "flint/json/value.hpp"
#include "flint/text/number.hpp"

namespace flint::json {
namespace {

constexpr char kFirstNonControl = 0x20;
constexpr std::string_view kHexDigits = "0123456789abcdef";
constexpr unsigned kNibbleBits = 4;
constexpr unsigned kNibbleMask = 0xF;

}  // namespace

auto AppendQuoted(std::string& out, std::string_view text) -> void {
  out.push_back('"');
  std::size_t runStart = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char character = text[i];
    const bool isControl = static_cast<unsigned char>(character) < static_cast<unsigned char>(kFirstNonControl);
    if (character != '"' && character != '\\' && !isControl) {
      continue;
    }
    out.append(text.substr(runStart, i - runStart));
    runStart = i + 1;
    switch (character) {
      case '"':
        out.append("\\\"");
        break;
      case '\\':
        out.append("\\\\");
        break;
      case '\n':
        out.append("\\n");
        break;
      case '\r':
        out.append("\\r");
        break;
      case '\t':
        out.append("\\t");
        break;
      case '\b':
        out.append("\\b");
        break;
      case '\f':
        out.append("\\f");
        break;
      default: {
        const auto code = static_cast<unsigned>(static_cast<unsigned char>(character));
        out.append("\\u00");
        out.push_back(kHexDigits[(code >> kNibbleBits) & kNibbleMask]);
        out.push_back(kHexDigits[code & kNibbleMask]);
        break;
      }
    }
  }
  out.append(text.substr(runStart));
  out.push_back('"');
}

Writer::Writer(std::string& out, WriteOptions options) : m_out{&out}, m_options{options} {}

auto Writer::NewLine() -> void {
  if (m_options.indent <= 0) {
    return;
  }
  m_out->push_back('\n');
  m_out->append(m_scopes.size() * static_cast<std::size_t>(m_options.indent), ' ');
}

auto Writer::BeforeValue() -> void {
  if (m_afterKey) {
    m_afterKey = false;
    return;
  }
  if (m_scopes.empty()) {
    return;
  }
  auto& [scope, hasElements] = m_scopes.back();
  assert(scope == Scope::InArray && "object members need a Key() before their value");
  if (hasElements) {
    m_out->push_back(',');
  }
  hasElements = true;
  NewLine();
}

auto Writer::BeginObject() -> void {
  BeforeValue();
  m_out->push_back('{');
  m_scopes.emplace_back(Scope::InObject, false);
}

auto Writer::EndObject() -> void {
  assert(!m_scopes.empty() && m_scopes.back().first == Scope::InObject);
  const bool hadElements = m_scopes.back().second;
  m_scopes.pop_back();
  if (hadElements) {
    NewLine();
  }
  m_out->push_back('}');
}

auto Writer::BeginArray() -> void {
  BeforeValue();
  m_out->push_back('[');
  m_scopes.emplace_back(Scope::InArray, false);
}

auto Writer::EndArray() -> void {
  assert(!m_scopes.empty() && m_scopes.back().first == Scope::InArray);
  const bool hadElements = m_scopes.back().second;
  m_scopes.pop_back();
  if (hadElements) {
    NewLine();
  }
  m_out->push_back(']');
}

auto Writer::Key(std::string_view key) -> void {
  assert(!m_scopes.empty() && m_scopes.back().first == Scope::InObject && !m_afterKey);
  auto& hasElements = m_scopes.back().second;
  if (hasElements) {
    m_out->push_back(',');
  }
  hasElements = true;
  NewLine();
  AppendQuoted(*m_out, key);
  m_out->push_back(':');
  if (m_options.indent > 0) {
    m_out->push_back(' ');
  }
  m_afterKey = true;
}

auto Writer::Null() -> void {
  BeforeValue();
  m_out->append("null");
}

auto Writer::Bool(bool value) -> void {
  BeforeValue();
  m_out->append(value ? "true" : "false");
}

auto Writer::Integer(std::int64_t value) -> void {
  BeforeValue();
  text::AppendNumber(*m_out, value);
}

auto Writer::Unsigned(std::uint64_t value) -> void {
  BeforeValue();
  text::AppendNumber(*m_out, value);
}

auto Writer::Double(double value) -> void {
  BeforeValue();
  // JSON has no representation for NaN or infinity.
  if (!std::isfinite(value)) {
    m_out->append("null");
    return;
  }
  text::AppendNumber(*m_out, value);
}

auto Writer::String(std::string_view value) -> void {
  BeforeValue();
  AppendQuoted(*m_out, value);
}

auto Writer::Write(const Value& value) -> void {
  switch (value.GetKind()) {
    case Kind::Null:
      Null();
      break;
    case Kind::Bool:
      Bool(value.AsBool().value_or(false));
      break;
    case Kind::Integer:
      Integer(value.AsInteger().value_or(0));
      break;
    case Kind::Double:
      Double(value.AsDouble().value_or(0.0));
      break;
    case Kind::String:
      String(value.AsString().value_or(std::string_view{}));
      break;
    case Kind::Array:
      BeginArray();
      for (const Value& element : *value.AsArray()) {
        Write(element);
      }
      EndArray();
      break;
    case Kind::Object:
      BeginObject();
      for (const Member& member : value.AsObject()->Members()) {
        Key(member.key);
        Write(member.value);
      }
      EndObject();
      break;
  }
}

auto Serialize(const Value& value, WriteOptions options) -> std::string {
  std::string out;
  Writer writer{out, options};
  writer.Write(value);
  return out;
}

}  // namespace flint::json
