#include "flint/xml/parser.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "flint/text/number.hpp"
#include "flint/xml/element.hpp"

namespace flint::xml {
namespace {

constexpr std::size_t kMaximumDepth = 256;
constexpr std::string_view kByteOrderMark = "\xEF\xBB\xBF";
constexpr std::string_view kCdataEnd = "]]>";
// Bytes from 0x80 up are UTF-8 sequences, which XML allows in names.
constexpr unsigned kFirstNonAscii = 0x80U;

auto IsSpace(char character) -> bool {
  return character == ' ' || character == '\t' || character == '\n' || character == '\r';
}

auto IsNameStart(char character) -> bool {
  const auto byte = static_cast<unsigned char>(character);
  return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') || character == '_' || character == ':' ||
         byte >= kFirstNonAscii;
}

auto IsNameCharacter(char character) -> bool {
  return IsNameStart(character) || (character >= '0' && character <= '9') || character == '-' || character == '.';
}

auto AppendUtf8(std::string& out, std::uint32_t codePoint) -> bool {
  constexpr std::uint32_t kOneByte = 0x80;
  constexpr std::uint32_t kTwoBytes = 0x800;
  constexpr std::uint32_t kThreeBytes = 0x10000;
  constexpr std::uint32_t kMaximum = 0x10FFFF;
  constexpr std::uint32_t kSurrogateFirst = 0xD800;
  constexpr std::uint32_t kSurrogateLast = 0xDFFF;
  constexpr std::uint32_t kSixBits = 0x3F;
  constexpr std::uint32_t kContinuation = 0x80;
  constexpr std::uint32_t kTwoByteLead = 0xC0;
  constexpr std::uint32_t kThreeByteLead = 0xE0;
  constexpr std::uint32_t kFourByteLead = 0xF0;
  constexpr unsigned kShift1 = 6;
  constexpr unsigned kShift2 = 12;
  constexpr unsigned kShift3 = 18;
  if (codePoint == 0 || codePoint > kMaximum || (codePoint >= kSurrogateFirst && codePoint <= kSurrogateLast)) {
    return false;
  }
  const auto put = [&out](std::uint32_t byte) { out.push_back(static_cast<char>(byte)); };
  if (codePoint < kOneByte) {
    put(codePoint);
  } else if (codePoint < kTwoBytes) {
    put(kTwoByteLead | (codePoint >> kShift1));
    put(kContinuation | (codePoint & kSixBits));
  } else if (codePoint < kThreeBytes) {
    put(kThreeByteLead | (codePoint >> kShift2));
    put(kContinuation | ((codePoint >> kShift1) & kSixBits));
    put(kContinuation | (codePoint & kSixBits));
  } else {
    put(kFourByteLead | (codePoint >> kShift3));
    put(kContinuation | ((codePoint >> kShift2) & kSixBits));
    put(kContinuation | ((codePoint >> kShift1) & kSixBits));
    put(kContinuation | (codePoint & kSixBits));
  }
  return true;
}

class Parser {
public:
  explicit Parser(std::string_view text) : m_text{text} {}

  auto ParseDocument() -> std::expected<Element, ParseError> {
    SkipByteOrderMark();
    if (auto prolog = SkipMisc(); !prolog.has_value()) {
      return std::unexpected{prolog.error()};
    }
    if (AtEnd() || Peek() != '<') {
      return Fail(ParseErrorCode::NoRootElement, "expected the root element");
    }
    auto root = ParseElement(0);
    if (!root.has_value()) {
      return root;
    }
    if (auto epilog = SkipMisc(); !epilog.has_value()) {
      return std::unexpected{epilog.error()};
    }
    if (!AtEnd()) {
      return Fail(ParseErrorCode::TrailingContent, "only comments and whitespace may follow the root element");
    }
    return root;
  }

private:
  [[nodiscard]] auto AtEnd() const -> bool { return m_position >= m_text.size(); }

  [[nodiscard]] auto Peek() const -> char { return m_text[m_position]; }

  [[nodiscard]] auto StartsWith(std::string_view prefix) const -> bool { return m_text.substr(m_position).starts_with(prefix); }

  [[nodiscard]] auto Here() const -> Location { return m_location; }

  auto Advance(std::size_t count = 1) -> void {
    for (std::size_t i = 0; i < count && !AtEnd(); ++i) {
      if (m_text[m_position] == '\n') {
        ++m_location.line;
        m_location.column = 1;
      } else {
        ++m_location.column;
      }
      ++m_position;
    }
  }

  [[nodiscard]] auto Fail(ParseErrorCode code, std::string detail) const -> std::unexpected<ParseError> {
    return std::unexpected{ParseError{.code = code, .location = m_location, .detail = std::move(detail)}};
  }

  auto SkipByteOrderMark() -> void {
    if (StartsWith(kByteOrderMark)) {
      m_position += kByteOrderMark.size();
    }
  }

  auto SkipSpace() -> void {
    while (!AtEnd() && IsSpace(Peek())) {
      Advance();
    }
  }

  // Skips whitespace, comments and processing instructions (including the XML declaration).
  auto SkipMisc() -> std::expected<void, ParseError> {
    while (true) {
      SkipSpace();
      if (StartsWith("<!--")) {
        if (auto skipped = SkipPast("-->", "unterminated comment"); !skipped.has_value()) {
          return skipped;
        }
      } else if (StartsWith("<?")) {
        if (auto skipped = SkipPast("?>", "unterminated processing instruction"); !skipped.has_value()) {
          return skipped;
        }
      } else if (StartsWith("<!DOCTYPE")) {
        return Fail(ParseErrorCode::UnexpectedCharacter, "document type declarations are not supported");
      } else {
        return {};
      }
    }
  }

  auto SkipPast(std::string_view terminator, std::string_view what) -> std::expected<void, ParseError> {
    const std::size_t end = m_text.find(terminator, m_position);
    if (end == std::string_view::npos) {
      return Fail(ParseErrorCode::UnexpectedEnd, std::string{what});
    }
    Advance(end + terminator.size() - m_position);
    return {};
  }

  auto ParseName() -> std::expected<std::string, ParseError> {
    if (AtEnd() || !IsNameStart(Peek())) {
      return Fail(ParseErrorCode::UnexpectedCharacter, "expected a name");
    }
    const std::size_t start = m_position;
    while (!AtEnd() && IsNameCharacter(Peek())) {
      Advance();
    }
    return std::string{m_text.substr(start, m_position - start)};
  }

  // Decodes an entity starting at '&' and appends it.
  auto ParseEntity(std::string& out) -> std::expected<void, ParseError> {
    const Location start = Here();
    const std::size_t semicolon = m_text.find(';', m_position);
    constexpr std::size_t kLongestEntity = 12;
    if (semicolon == std::string_view::npos || semicolon - m_position > kLongestEntity) {
      return std::unexpected{ParseError{.code = ParseErrorCode::InvalidEntity, .location = start, .detail = "unterminated entity"}};
    }
    const std::string_view name = m_text.substr(m_position + 1, semicolon - m_position - 1);
    if (name == "lt") {
      out.push_back('<');
    } else if (name == "gt") {
      out.push_back('>');
    } else if (name == "amp") {
      out.push_back('&');
    } else if (name == "quot") {
      out.push_back('"');
    } else if (name == "apos") {
      out.push_back('\'');
    } else if (name.starts_with('#')) {
      const bool isHex = name.size() > 1 && (name[1] == 'x' || name[1] == 'X');
      const auto digits = name.substr(isHex ? 2 : 1);
      constexpr int kHex = 16;
      constexpr int kDecimal = 10;
      const auto codePoint = text::ParseInteger<std::uint32_t>(digits, isHex ? kHex : kDecimal);
      if (!codePoint.has_value() || !AppendUtf8(out, *codePoint)) {
        return std::unexpected{ParseError{
            .code = ParseErrorCode::InvalidEntity,
            .location = start,
            .detail = std::format("invalid character reference &{};", name),
        }};
      }
    } else {
      return std::unexpected{
          ParseError{.code = ParseErrorCode::InvalidEntity, .location = start, .detail = std::format("unknown entity &{};", name)}};
    }
    Advance(semicolon + 1 - m_position);
    return {};
  }

  auto ParseAttributeValue() -> std::expected<std::string, ParseError> {
    if (AtEnd() || (Peek() != '"' && Peek() != '\'')) {
      return Fail(ParseErrorCode::UnexpectedCharacter, "expected a quoted attribute value");
    }
    const char quote = Peek();
    Advance();
    std::string value;
    while (true) {
      if (AtEnd()) {
        return Fail(ParseErrorCode::UnexpectedEnd, "unterminated attribute value");
      }
      const char character = Peek();
      if (character == quote) {
        Advance();
        return value;
      }
      if (character == '<') {
        return Fail(ParseErrorCode::UnexpectedCharacter, "'<' is not allowed in an attribute value");
      }
      if (character == '&') {
        if (auto entity = ParseEntity(value); !entity.has_value()) {
          return std::unexpected{entity.error()};
        }
        continue;
      }
      value.push_back(character);
      Advance();
    }
  }

  auto ParseAttributes(Element& element) -> std::expected<void, ParseError> {
    while (true) {
      const bool hadSpace = !AtEnd() && IsSpace(Peek());
      SkipSpace();
      if (AtEnd()) {
        return Fail(ParseErrorCode::UnexpectedEnd, std::format("unterminated start tag <{}>", element.name));
      }
      if (Peek() == '>' || StartsWith("/>")) {
        return {};
      }
      if (!hadSpace) {
        return Fail(ParseErrorCode::UnexpectedCharacter, "expected whitespace before an attribute");
      }
      const Location location = Here();
      auto name = ParseName();
      if (!name.has_value()) {
        return std::unexpected{name.error()};
      }
      SkipSpace();
      if (AtEnd() || Peek() != '=') {
        return Fail(ParseErrorCode::UnexpectedCharacter, std::format("expected '=' after attribute {}", *name));
      }
      Advance();
      SkipSpace();
      const Location valueLocation{.line = Here().line, .column = Here().column + 1};
      auto value = ParseAttributeValue();
      if (!value.has_value()) {
        return std::unexpected{value.error()};
      }
      if (element.FindAttribute(*name) != nullptr) {
        return std::unexpected{ParseError{.code = ParseErrorCode::DuplicateAttribute, .location = location, .detail = *name}};
      }
      element.attributes.push_back(
          Attribute{.name = std::move(*name), .value = std::move(*value), .location = location, .valueLocation = valueLocation});
    }
  }

  // Parses text up to the next '<', decoding entities, and appends it unless it is only whitespace.
  auto ParseText(Element& element) -> std::expected<void, ParseError> {
    std::string text;
    while (!AtEnd() && Peek() != '<') {
      if (Peek() == '&') {
        if (auto entity = ParseEntity(text); !entity.has_value()) {
          return entity;
        }
        continue;
      }
      text.push_back(Peek());
      Advance();
    }
    if (std::ranges::any_of(text, [](char character) { return !IsSpace(character); })) {
      element.text += text;
    }
    return {};
  }

  auto ParseContent(Element& element, std::size_t depth) -> std::expected<void, ParseError> {
    while (true) {
      if (AtEnd()) {
        return Fail(ParseErrorCode::UnexpectedEnd, std::format("missing </{}>", element.name));
      }
      if (StartsWith("</")) {
        return ParseEndTag(element);
      }
      if (StartsWith("<!--")) {
        if (auto skipped = SkipPast("-->", "unterminated comment"); !skipped.has_value()) {
          return skipped;
        }
      } else if (StartsWith("<![CDATA[")) {
        constexpr std::size_t kOpen = 9;
        const std::size_t end = m_text.find(kCdataEnd, m_position);
        if (end == std::string_view::npos) {
          return Fail(ParseErrorCode::UnexpectedEnd, "unterminated CDATA section");
        }
        element.text += m_text.substr(m_position + kOpen, end - m_position - kOpen);
        Advance(end + kCdataEnd.size() - m_position);
      } else if (StartsWith("<?")) {
        if (auto skipped = SkipPast("?>", "unterminated processing instruction"); !skipped.has_value()) {
          return skipped;
        }
      } else if (Peek() == '<') {
        auto child = ParseElement(depth + 1);
        if (!child.has_value()) {
          return std::unexpected{child.error()};
        }
        element.children.push_back(std::move(*child));
      } else if (auto text = ParseText(element); !text.has_value()) {
        return text;
      }
    }
  }

  auto ParseEndTag(const Element& element) -> std::expected<void, ParseError> {
    const Location location = Here();
    Advance(2);
    auto name = ParseName();
    if (!name.has_value()) {
      return std::unexpected{name.error()};
    }
    if (*name != element.name) {
      return std::unexpected{ParseError{
          .code = ParseErrorCode::MismatchedEndTag,
          .location = location,
          .detail = std::format("expected </{}>, found </{}>", element.name, *name),
      }};
    }
    SkipSpace();
    if (AtEnd() || Peek() != '>') {
      return Fail(ParseErrorCode::UnexpectedCharacter, "expected '>'");
    }
    Advance();
    return {};
  }

  auto ParseElement(std::size_t depth) -> std::expected<Element, ParseError> {
    if (depth > kMaximumDepth) {
      return Fail(ParseErrorCode::NestingTooDeep, std::format("more than {} nested elements", kMaximumDepth));
    }
    Element element;
    element.location = Here();
    Advance();  // '<'
    auto name = ParseName();
    if (!name.has_value()) {
      return std::unexpected{name.error()};
    }
    element.name = std::move(*name);
    if (auto attributes = ParseAttributes(element); !attributes.has_value()) {
      return std::unexpected{attributes.error()};
    }
    if (StartsWith("/>")) {
      Advance(2);
      return element;
    }
    Advance();  // '>'
    if (auto content = ParseContent(element, depth); !content.has_value()) {
      return std::unexpected{content.error()};
    }
    return element;
  }

  std::string_view m_text;
  std::size_t m_position{0};
  Location m_location;
};

}  // namespace

auto Element::FindAttribute(std::string_view attributeName) const noexcept -> const Attribute* {
  const auto found = std::ranges::find(attributes, attributeName, &Attribute::name);
  return found == attributes.end() ? nullptr : &*found;
}

auto Element::GetAttribute(std::string_view attributeName, std::string_view fallback) const -> std::string_view {
  const Attribute* attribute = FindAttribute(attributeName);
  return attribute != nullptr ? std::string_view{attribute->value} : fallback;
}

auto Element::GetLocalName() const noexcept -> std::string_view {
  const std::string_view full{name};
  const std::size_t colon = full.find(':');
  return colon == std::string_view::npos ? full : full.substr(colon + 1);
}

auto Element::GetPrefix() const noexcept -> std::string_view {
  const std::string_view full{name};
  const std::size_t colon = full.find(':');
  return colon == std::string_view::npos ? std::string_view{} : full.substr(0, colon);
}

auto ParseError::Describe() const -> std::string {
  std::string_view what;
  switch (code) {
    case ParseErrorCode::UnexpectedEnd:
      what = "unexpected end of document";
      break;
    case ParseErrorCode::UnexpectedCharacter:
      what = "unexpected character";
      break;
    case ParseErrorCode::MismatchedEndTag:
      what = "mismatched end tag";
      break;
    case ParseErrorCode::DuplicateAttribute:
      what = "duplicate attribute";
      break;
    case ParseErrorCode::InvalidEntity:
      what = "invalid entity";
      break;
    case ParseErrorCode::NoRootElement:
      what = "no root element";
      break;
    case ParseErrorCode::TrailingContent:
      what = "content after the root element";
      break;
    case ParseErrorCode::NestingTooDeep:
      what = "nesting too deep";
      break;
  }
  return detail.empty() ? std::format("line {}, column {}: {}", location.line, location.column, what)
                        : std::format("line {}, column {}: {}: {}", location.line, location.column, what, detail);
}

auto Parse(std::string_view text) -> std::expected<Element, ParseError> {
  return Parser{text}.ParseDocument();
}

}  // namespace flint::xml
