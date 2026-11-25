#include "flint/xml/parser.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <pugixml.hpp>

#include "flint/text/number.hpp"
#include "flint/xml/element.hpp"

namespace flint::xml {
namespace {

constexpr std::size_t kMaximumDepth = 256;
constexpr std::string_view kByteOrderMark = "\xEF\xBB\xBF";

// pugixml checks the structure (ADR-0011). Entities are decoded here rather than with parse_escapes, so an unknown
// entity is an error with a location instead of text left as written. DOCTYPEs are parsed only to be rejected, and
// fragment mode keeps text outside the root element, so that it can be reported.
constexpr unsigned kParseOptions =
    pugi::parse_cdata | pugi::parse_wconv_attribute | pugi::parse_eol | pugi::parse_doctype | pugi::parse_fragment;

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

// Maps byte offsets to lines and columns.
class Locator {
public:
  explicit Locator(std::string_view text) {
    m_lineStarts.push_back(0);
    for (std::size_t i = 0; i < text.size(); ++i) {
      if (text[i] == '\n') {
        m_lineStarts.push_back(i + 1);
      }
    }
  }

  [[nodiscard]] auto At(std::size_t offset) const -> Location {
    const auto next = std::ranges::upper_bound(m_lineStarts, offset);
    const auto line = static_cast<std::size_t>(std::distance(m_lineStarts.begin(), next));
    return Location{.line = static_cast<std::uint32_t>(line), .column = static_cast<std::uint32_t>(offset - *std::prev(next) + 1)};
  }

private:
  std::vector<std::size_t> m_lineStarts;
};

// Returns the position of the '>' that ends the start tag beginning before `from`, skipping quoted attribute values,
// which may contain '>'.
auto FindStartTagEnd(std::string_view text, std::size_t from) -> std::size_t {
  char quote = 0;
  for (std::size_t position = from; position < text.size(); ++position) {
    const char character = text[position];
    if (quote != 0) {
      quote = character == quote ? char{0} : quote;
    } else if (character == '"' || character == '\'') {
      quote = character;
    } else if (character == '>') {
      return position;
    }
  }
  return text.size();
}

// The names of the elements still open at `end`, outermost first. pugixml has already checked the text before `end`,
// so a simple scan is enough; this only serves error messages.
auto FindOpenElements(std::string_view text, std::size_t end) -> std::vector<std::string_view> {
  std::vector<std::string_view> open;
  std::size_t position = 0;
  const auto skipPast = [&text, &position](std::string_view terminator) {
    const std::size_t found = text.find(terminator, position);
    position = found == std::string_view::npos ? text.size() : found + terminator.size();
  };
  while ((position = text.find('<', position)) < end) {
    const std::string_view rest = text.substr(position);
    if (rest.starts_with("<!--")) {
      skipPast("-->");
    } else if (rest.starts_with("<![CDATA[")) {
      skipPast("]]>");
    } else if (rest.starts_with("<?")) {
      skipPast("?>");
    } else if (rest.starts_with("<!")) {
      skipPast(">");
    } else if (rest.starts_with("</")) {
      if (!open.empty()) {
        open.pop_back();
      }
      skipPast(">");
    } else {
      const std::size_t nameEnd = std::min(text.find_first_of(" \t\r\n/>", position + 1), text.size());
      const std::size_t tagEnd = FindStartTagEnd(text, nameEnd);
      if (tagEnd < text.size() && text[tagEnd - 1] != '/') {
        open.push_back(text.substr(position + 1, nameEnd - position - 1));
      }
      position = tagEnd + 1;
    }
  }
  return open;
}

// Turns pugixml's tree, parsed in place in a copy of the text, into flint::xml elements.
class Converter {
public:
  Converter(std::string_view text, const char* buffer) : m_text{text}, m_buffer{buffer}, m_locator{text} {}

  [[nodiscard]] auto ConvertDocument(const pugi::xml_document& document) const -> std::expected<Element, ParseError> {
    std::optional<Element> root;
    for (const pugi::xml_node& node : document.children()) {
      const auto offset = static_cast<std::size_t>(node.offset_debug());
      switch (node.type()) {
        case pugi::node_element:
          if (root.has_value()) {
            return Fail(ParseErrorCode::TrailingContent, offset - 1, "a document has one root element");
          }
          if (auto element = ConvertElement(node, 0); element.has_value()) {
            root = std::move(*element);
          } else {
            return element;
          }
          break;
        case pugi::node_doctype:
          return Fail(ParseErrorCode::DocumentTypeNotAllowed, m_text.rfind("<!", offset), {});
        case pugi::node_pcdata:
        case pugi::node_cdata:
          return root.has_value()
                     ? Fail(ParseErrorCode::TrailingContent, offset, "only comments and whitespace may follow the root element")
                     : Fail(ParseErrorCode::UnexpectedCharacter, offset, "text before the root element");
        default:
          break;
      }
    }
    if (!root.has_value()) {
      return Fail(ParseErrorCode::NoRootElement, m_text.size(), "expected the root element");
    }
    return std::move(*root);
  }

  [[nodiscard]] auto DescribeFailure(const pugi::xml_parse_result& result) const -> ParseError {
    const auto offset = static_cast<std::size_t>(std::max<std::ptrdiff_t>(result.offset, 0));
    if (result.status == pugi::status_end_element_mismatch) {
      const bool atEndTag = offset >= 2 && m_text.substr(offset - 2, 2) == "</";
      const std::size_t tagStart = atEndTag ? offset - 2 : m_text.size();
      const std::vector<std::string_view> open = FindOpenElements(m_text, tagStart);
      const std::string expected = open.empty() ? std::string{} : std::format("expected </{}>", open.back());
      if (!atEndTag) {
        return Error(ParseErrorCode::UnexpectedEnd, m_text.size(), expected);
      }
      const std::size_t nameEnd = std::min(m_text.find_first_of(" \t\r\n>", offset), m_text.size());
      return Error(
          ParseErrorCode::MismatchedEndTag, tagStart, std::format("{}, found </{}>", expected, m_text.substr(offset, nameEnd - offset)));
    }
    std::string detail{result.description()};
    if (!detail.empty()) {
      detail.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(detail.front())));
    }
    if (offset >= m_text.size()) {
      return Error(ParseErrorCode::UnexpectedEnd, m_text.size(), std::move(detail));
    }
    return Error(ParseErrorCode::UnexpectedCharacter, offset, std::move(detail));
  }

private:
  [[nodiscard]] auto Error(ParseErrorCode code, std::size_t offset, std::string detail) const -> ParseError {
    return ParseError{.code = code, .location = m_locator.At(offset), .detail = std::move(detail)};
  }

  [[nodiscard]] auto Fail(ParseErrorCode code, std::size_t offset, std::string detail) const -> std::unexpected<ParseError> {
    return std::unexpected{Error(code, offset, std::move(detail))};
  }

  // Strings pugixml parsed in place point into the buffer, at the same offsets as in the text.
  [[nodiscard]] auto OffsetOf(const char* pointer) const -> std::size_t { return static_cast<std::size_t>(pointer - m_buffer); }

  [[nodiscard]] auto ConvertElement(const pugi::xml_node& node, std::size_t depth) const -> std::expected<Element, ParseError> {
    // offset_debug is where the name starts, just after '<'.
    const auto offset = static_cast<std::size_t>(node.offset_debug()) - 1;
    if (depth >= kMaximumDepth) {
      return Fail(ParseErrorCode::NestingTooDeep, offset, std::format("elements may nest {} deep", kMaximumDepth));
    }
    Element element;
    element.name = node.name();
    element.location = m_locator.At(offset);
    for (const pugi::xml_attribute& attribute : node.attributes()) {
      auto converted = ConvertAttribute(attribute);
      if (!converted.has_value()) {
        return std::unexpected{std::move(converted.error())};
      }
      if (element.FindAttribute(converted->name) != nullptr) {
        return Fail(ParseErrorCode::DuplicateAttribute, OffsetOf(attribute.name()), std::format("{} is set twice", converted->name));
      }
      element.attributes.push_back(std::move(*converted));
    }
    for (const pugi::xml_node& child : node.children()) {
      if (child.type() == pugi::node_element) {
        auto converted = ConvertElement(child, depth + 1);
        if (!converted.has_value()) {
          return converted;
        }
        element.children.push_back(std::move(*converted));
      } else if (child.type() == pugi::node_pcdata) {
        auto text = Decode(child.value(), OffsetOf(child.value()), /*inAttribute=*/false);
        if (!text.has_value()) {
          return std::unexpected{std::move(text.error())};
        }
        element.text += *text;
      } else if (child.type() == pugi::node_cdata) {
        element.text += child.value();
      }
    }
    return element;
  }

  [[nodiscard]] auto ConvertAttribute(const pugi::xml_attribute& attribute) const -> std::expected<Attribute, ParseError> {
    const std::string_view name = attribute.name();
    const std::size_t nameOffset = OffsetOf(attribute.name());
    // An empty value is not in the buffer, so find the opening quote in the text instead.
    const std::size_t quote = m_text.find_first_of("\"'", nameOffset + name.size());
    const std::size_t valueOffset = quote == std::string_view::npos ? nameOffset : quote + 1;
    auto value = Decode(attribute.value(), valueOffset, /*inAttribute=*/true);
    if (!value.has_value()) {
      return std::unexpected{std::move(value.error())};
    }
    return Attribute{
        .name = std::string{name},
        .value = std::move(*value),
        .location = m_locator.At(nameOffset),
        .valueLocation = m_locator.At(valueOffset),
    };
  }

  // Decodes the predefined and numeric entities in text that starts at `offset`.
  [[nodiscard]] auto Decode(std::string_view raw, std::size_t offset, bool inAttribute) const -> std::expected<std::string, ParseError> {
    std::string out;
    out.reserve(raw.size());
    for (std::size_t index = 0; index < raw.size(); ++index) {
      const char character = raw[index];
      if (character == '&') {
        if (auto decoded = DecodeEntity(raw, index, offset, out); !decoded.has_value()) {
          return std::unexpected{std::move(decoded.error())};
        }
      } else if (character == '<' && inAttribute) {
        return Fail(ParseErrorCode::UnexpectedCharacter, offset + index, "write '<' as &lt; in an attribute value");
      } else {
        out.push_back(character);
      }
    }
    return out;
  }

  // Decodes the entity starting at raw[index] ('&'), appends it, and leaves index on its ';'.
  [[nodiscard]] auto DecodeEntity(std::string_view raw, std::size_t& index, std::size_t offset, std::string& out) const
      -> std::expected<void, ParseError> {
    const std::size_t start = offset + index;
    const std::size_t semicolon = raw.find(';', index);
    constexpr std::size_t kLongestEntity = 12;
    if (semicolon == std::string_view::npos || semicolon - index > kLongestEntity) {
      return Fail(ParseErrorCode::InvalidEntity, start, "unterminated entity; write '&' as &amp;");
    }
    const std::string_view name = raw.substr(index + 1, semicolon - index - 1);
    index = semicolon;
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
      constexpr int kHex = 16;
      constexpr int kDecimal = 10;
      const auto codePoint = text::ParseInteger<std::uint32_t>(name.substr(isHex ? 2 : 1), isHex ? kHex : kDecimal);
      if (!codePoint.has_value() || !AppendUtf8(out, *codePoint)) {
        return Fail(ParseErrorCode::InvalidEntity, start, std::format("invalid character reference &{};", name));
      }
    } else {
      return Fail(ParseErrorCode::InvalidEntity, start, std::format("unknown entity &{};", name));
    }
    return {};
  }

  std::string_view m_text;
  const char* m_buffer;
  Locator m_locator;
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
    case ParseErrorCode::DocumentTypeNotAllowed:
      what = "document type declarations (<!DOCTYPE>) are not supported";
      break;
  }
  return detail.empty() ? std::format("line {}, column {}: {}", location.line, location.column, what)
                        : std::format("line {}, column {}: {}: {}", location.line, location.column, what, detail);
}

auto Parse(std::string_view text) -> std::expected<Element, ParseError> {
  if (text.starts_with(kByteOrderMark)) {
    text.remove_prefix(kByteOrderMark.size());
  }
  // pugixml parses in place, so it gets a copy. Offsets into the copy are offsets into the text.
  std::string buffer{text};
  pugi::xml_document document;
  const pugi::xml_parse_result result = document.load_buffer_inplace(buffer.data(), buffer.size(), kParseOptions, pugi::encoding_utf8);
  const Converter converter{text, buffer.data()};
  if (!result) {
    return std::unexpected{converter.DescribeFailure(result)};
  }
  return converter.ConvertDocument(document);
}

}  // namespace flint::xml
