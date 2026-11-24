#include "flint/json/parser.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "flint/json/value.hpp"
#include "flint/text/number.hpp"

namespace flint::json {
namespace {

// Deep enough for any real document, shallow enough that recursion cannot overflow the stack.
constexpr std::size_t kMaxDepth = 256;

constexpr char32_t kHighSurrogateFirst = 0xD800;
constexpr char32_t kLowSurrogateFirst = 0xDC00;
constexpr char32_t kLowSurrogateLast = 0xDFFF;
constexpr char32_t kSurrogateOffset = 0x10000;
constexpr unsigned kSurrogateShift = 10;
constexpr unsigned kHexDigitsInEscape = 4;
constexpr char kFirstNonControl = 0x20;

// Appends a code point to `out` as UTF-8.
auto AppendUtf8(std::string& out, char32_t codePoint) -> void {
  constexpr char32_t kOneByteMax = 0x7F;
  constexpr char32_t kTwoByteMax = 0x7FF;
  constexpr char32_t kThreeByteMax = 0xFFFF;
  constexpr unsigned kContinuationBits = 6;
  constexpr unsigned kTwoContinuations = 2 * kContinuationBits;
  constexpr unsigned kThreeContinuations = 3 * kContinuationBits;
  constexpr char32_t kContinuationMask = 0x3F;
  constexpr unsigned char kContinuation = 0x80;
  constexpr unsigned char kTwoByteLead = 0xC0;
  constexpr unsigned char kThreeByteLead = 0xE0;
  constexpr unsigned char kFourByteLead = 0xF0;

  const auto byte = [&out](char32_t bits) { out.push_back(static_cast<char>(static_cast<unsigned char>(bits))); };
  if (codePoint <= kOneByteMax) {
    byte(codePoint);
  } else if (codePoint <= kTwoByteMax) {
    byte(kTwoByteLead | (codePoint >> kContinuationBits));
    byte(kContinuation | (codePoint & kContinuationMask));
  } else if (codePoint <= kThreeByteMax) {
    byte(kThreeByteLead | (codePoint >> kTwoContinuations));
    byte(kContinuation | ((codePoint >> kContinuationBits) & kContinuationMask));
    byte(kContinuation | (codePoint & kContinuationMask));
  } else {
    byte(kFourByteLead | (codePoint >> kThreeContinuations));
    byte(kContinuation | ((codePoint >> kTwoContinuations) & kContinuationMask));
    byte(kContinuation | ((codePoint >> kContinuationBits) & kContinuationMask));
    byte(kContinuation | (codePoint & kContinuationMask));
  }
}

class Parser {
public:
  Parser(std::string_view text, ParseOptions options) : m_text{text}, m_options{options} {}

  auto ParseDocument() -> std::expected<Value, ParseError> {
    if (auto skipped = SkipWhitespace(); !skipped.has_value()) {
      return std::unexpected(skipped.error());
    }
    auto value = ParseValue(0);
    if (!value.has_value()) {
      return value;
    }
    if (auto skipped = SkipWhitespace(); !skipped.has_value()) {
      return std::unexpected(skipped.error());
    }
    if (m_position != m_text.size()) {
      return Fail(ParseErrorCode::TrailingCharacters);
    }
    return value;
  }

private:
  [[nodiscard]] auto AtEnd() const -> bool { return m_position >= m_text.size(); }

  [[nodiscard]] auto Peek() const -> char { return AtEnd() ? '\0' : m_text[m_position]; }

  [[nodiscard]] auto Fail(ParseErrorCode code) const -> std::unexpected<ParseError> { return FailAt(code, m_position); }

  [[nodiscard]] auto FailAt(ParseErrorCode code, std::size_t offset) const -> std::unexpected<ParseError> {
    ParseError error{.code = code, .offset = offset};
    for (std::size_t i = 0; i < offset && i < m_text.size(); ++i) {
      if (m_text[i] == '\n') {
        ++error.line;
        error.column = 1;
      } else {
        ++error.column;
      }
    }
    return std::unexpected(error);
  }

  auto SkipWhitespace() -> std::expected<void, ParseError> {
    while (!AtEnd()) {
      const char current = Peek();
      if (current == ' ' || current == '\t' || current == '\n' || current == '\r') {
        ++m_position;
      } else if (current == '/' && m_options.allowComments) {
        if (auto skipped = SkipComment(); !skipped.has_value()) {
          return skipped;
        }
      } else {
        break;
      }
    }
    return {};
  }

  auto SkipComment() -> std::expected<void, ParseError> {
    const std::size_t start = m_position;
    const std::string_view rest = m_text.substr(m_position);
    if (rest.starts_with("//")) {
      const std::size_t end = m_text.find('\n', m_position);
      m_position = end == std::string_view::npos ? m_text.size() : end + 1;
      return {};
    }
    if (rest.starts_with("/*")) {
      const std::size_t end = m_text.find("*/", m_position + 2);
      if (end == std::string_view::npos) {
        return FailAt(ParseErrorCode::UnexpectedEnd, start);
      }
      m_position = end + 2;
      return {};
    }
    return Fail(ParseErrorCode::UnexpectedCharacter);
  }

  auto ParseValue(std::size_t depth) -> std::expected<Value, ParseError> {
    if (depth > kMaxDepth) {
      return Fail(ParseErrorCode::NestingTooDeep);
    }
    if (AtEnd()) {
      return Fail(ParseErrorCode::UnexpectedEnd);
    }
    switch (Peek()) {
      case '{':
        return ParseObject(depth);
      case '[':
        return ParseArray(depth);
      case '"':
        return ParseString().transform([](std::string text) { return Value{std::move(text)}; });
      case 't':
        return ParseLiteral("true", Value{/*value=*/true});
      case 'f':
        return ParseLiteral("false", Value{/*value=*/false});
      case 'n':
        return ParseLiteral("null", Value{nullptr});
      default:
        return ParseNumber();
    }
  }

  auto ParseLiteral(std::string_view literal, Value value) -> std::expected<Value, ParseError> {
    if (!m_text.substr(m_position).starts_with(literal)) {
      return Fail(ParseErrorCode::UnexpectedCharacter);
    }
    m_position += literal.size();
    return value;
  }

  auto ParseNumber() -> std::expected<Value, ParseError> {
    const std::size_t start = m_position;
    // Validate the RFC 8259 grammar first; from_chars is more lenient (it accepts leading '+', "inf", "nan").
    if (Peek() == '-') {
      ++m_position;
    }
    if (Peek() == '0') {
      ++m_position;
    } else if (Peek() >= '1' && Peek() <= '9') {
      SkipDigits();
    } else {
      return FailAt(AtEnd() ? ParseErrorCode::UnexpectedEnd : ParseErrorCode::UnexpectedCharacter, start);
    }
    bool isInteger = true;
    if (Peek() == '.') {
      isInteger = false;
      ++m_position;
      if (!IsDigit(Peek())) {
        return Fail(ParseErrorCode::InvalidNumber);
      }
      SkipDigits();
    }
    if (Peek() == 'e' || Peek() == 'E') {
      isInteger = false;
      ++m_position;
      if (Peek() == '+' || Peek() == '-') {
        ++m_position;
      }
      if (!IsDigit(Peek())) {
        return Fail(ParseErrorCode::InvalidNumber);
      }
      SkipDigits();
    }

    const std::string_view token = m_text.substr(start, m_position - start);
    if (isInteger) {
      if (const auto integer = text::ParseInteger<std::int64_t>(token); integer.has_value()) {
        return Value{*integer};
      }
      // Integers too large for 64 bits fall through and are stored as double.
    }
    const auto number = text::ParseFloating<double>(token);
    if (!number.has_value()) {
      return FailAt(ParseErrorCode::InvalidNumber, start);
    }
    return Value{*number};
  }

  [[nodiscard]] static auto IsDigit(char character) -> bool { return character >= '0' && character <= '9'; }

  auto SkipDigits() -> void {
    while (IsDigit(Peek())) {
      ++m_position;
    }
  }

  auto ParseHex4() -> std::expected<char32_t, ParseError> {
    if (m_position + kHexDigitsInEscape > m_text.size()) {
      return Fail(ParseErrorCode::UnexpectedEnd);
    }
    const auto parsed = text::ParseInteger<std::uint32_t>(m_text.substr(m_position, kHexDigitsInEscape), /*base=*/16);
    if (!parsed.has_value()) {
      return Fail(ParseErrorCode::InvalidEscape);
    }
    const char32_t value = *parsed;
    m_position += kHexDigitsInEscape;
    return value;
  }

  auto ParseUnicodeEscape(std::string& out) -> std::expected<void, ParseError> {
    const std::size_t start = m_position;
    auto first = ParseHex4();
    if (!first.has_value()) {
      return std::unexpected(first.error());
    }
    char32_t codePoint = *first;
    if (codePoint >= kLowSurrogateFirst && codePoint <= kLowSurrogateLast) {
      return FailAt(ParseErrorCode::InvalidUnicode, start);
    }
    if (codePoint >= kHighSurrogateFirst && codePoint < kLowSurrogateFirst) {
      if (!m_text.substr(m_position).starts_with("\\u")) {
        return FailAt(ParseErrorCode::InvalidUnicode, start);
      }
      m_position += 2;
      auto second = ParseHex4();
      if (!second.has_value()) {
        return std::unexpected(second.error());
      }
      if (*second < kLowSurrogateFirst || *second > kLowSurrogateLast) {
        return FailAt(ParseErrorCode::InvalidUnicode, start);
      }
      codePoint = kSurrogateOffset + ((codePoint - kHighSurrogateFirst) << kSurrogateShift) + (*second - kLowSurrogateFirst);
    }
    AppendUtf8(out, codePoint);
    return {};
  }

  auto ParseString() -> std::expected<std::string, ParseError> {
    ++m_position;  // opening quote
    std::string out;
    while (true) {
      if (AtEnd()) {
        return Fail(ParseErrorCode::UnexpectedEnd);
      }
      const char current = m_text[m_position];
      if (current == '"') {
        ++m_position;
        return out;
      }
      if (static_cast<unsigned char>(current) < static_cast<unsigned char>(kFirstNonControl)) {
        return Fail(ParseErrorCode::ControlCharacterInString);
      }
      if (current != '\\') {
        // Copy the run of ordinary characters in one go.
        const std::size_t end = m_text.find_first_of("\"\\", m_position);
        const std::size_t runEnd = end == std::string_view::npos ? m_text.size() : end;
        const std::string_view run = m_text.substr(m_position, runEnd - m_position);
        const auto* const control = std::ranges::find_if(
            run, [](char character) { return static_cast<unsigned char>(character) < static_cast<unsigned char>(kFirstNonControl); });
        if (control != run.end()) {
          m_position += static_cast<std::size_t>(control - run.begin());
          return Fail(ParseErrorCode::ControlCharacterInString);
        }
        out.append(run);
        m_position = runEnd;
        continue;
      }
      ++m_position;
      if (AtEnd()) {
        return Fail(ParseErrorCode::UnexpectedEnd);
      }
      const char escape = m_text[m_position++];
      switch (escape) {
        case '"':
          out.push_back('"');
          break;
        case '\\':
          out.push_back('\\');
          break;
        case '/':
          out.push_back('/');
          break;
        case 'b':
          out.push_back('\b');
          break;
        case 'f':
          out.push_back('\f');
          break;
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        case 'u':
          if (auto parsed = ParseUnicodeEscape(out); !parsed.has_value()) {
            return std::unexpected(parsed.error());
          }
          break;
        default:
          return FailAt(ParseErrorCode::InvalidEscape, m_position - 1);
      }
    }
  }

  // Shared loop for arrays and objects: elements separated by commas, optional trailing comma, then `close`.
  template <typename ParseElement>
  auto ParseSequence(char close, ParseElement parseElement) -> std::expected<void, ParseError> {
    ++m_position;  // opening bracket
    if (auto skipped = SkipWhitespace(); !skipped.has_value()) {
      return skipped;
    }
    if (Peek() == close) {
      ++m_position;
      return {};
    }
    while (true) {
      if (auto parsed = parseElement(); !parsed.has_value()) {
        return parsed;
      }
      if (auto skipped = SkipWhitespace(); !skipped.has_value()) {
        return skipped;
      }
      if (Peek() == close) {
        ++m_position;
        return {};
      }
      if (Peek() != ',') {
        return Fail(AtEnd() ? ParseErrorCode::UnexpectedEnd : ParseErrorCode::UnexpectedCharacter);
      }
      ++m_position;
      if (auto skipped = SkipWhitespace(); !skipped.has_value()) {
        return skipped;
      }
      if (m_options.allowTrailingCommas && Peek() == close) {
        ++m_position;
        return {};
      }
    }
  }

  auto ParseArray(std::size_t depth) -> std::expected<Value, ParseError> {
    Array array;
    auto parsed = ParseSequence(']', [&] -> std::expected<void, ParseError> {
      auto element = ParseValue(depth + 1);
      if (!element.has_value()) {
        return std::unexpected(element.error());
      }
      array.push_back(std::move(*element));
      return {};
    });
    if (!parsed.has_value()) {
      return std::unexpected(parsed.error());
    }
    return Value{std::move(array)};
  }

  auto ParseObject(std::size_t depth) -> std::expected<Value, ParseError> {
    Object object;
    auto parsed = ParseSequence('}', [&] -> std::expected<void, ParseError> {
      if (Peek() != '"') {
        return Fail(AtEnd() ? ParseErrorCode::UnexpectedEnd : ParseErrorCode::UnexpectedCharacter);
      }
      auto key = ParseString();
      if (!key.has_value()) {
        return std::unexpected(key.error());
      }
      if (auto skipped = SkipWhitespace(); !skipped.has_value()) {
        return skipped;
      }
      if (Peek() != ':') {
        return Fail(AtEnd() ? ParseErrorCode::UnexpectedEnd : ParseErrorCode::UnexpectedCharacter);
      }
      ++m_position;
      if (auto skipped = SkipWhitespace(); !skipped.has_value()) {
        return skipped;
      }
      auto value = ParseValue(depth + 1);
      if (!value.has_value()) {
        return std::unexpected(value.error());
      }
      object.Set(std::move(*key), std::move(*value));
      return {};
    });
    if (!parsed.has_value()) {
      return std::unexpected(parsed.error());
    }
    return Value{std::move(object)};
  }

  std::string_view m_text;
  ParseOptions m_options;
  std::size_t m_position{0};
};

auto CodeDescription(ParseErrorCode code) -> std::string_view {
  switch (code) {
    case ParseErrorCode::UnexpectedEnd:
      return "unexpected end of input";
    case ParseErrorCode::UnexpectedCharacter:
      return "unexpected character";
    case ParseErrorCode::InvalidNumber:
      return "invalid number";
    case ParseErrorCode::InvalidEscape:
      return "invalid escape sequence";
    case ParseErrorCode::InvalidUnicode:
      return "invalid unicode escape";
    case ParseErrorCode::ControlCharacterInString:
      return "control character in string";
    case ParseErrorCode::TrailingCharacters:
      return "unexpected characters after the document";
    case ParseErrorCode::NestingTooDeep:
      return "nesting too deep";
  }
  return "unknown error";
}

}  // namespace

auto ParseError::Describe() const -> std::string {
  return std::format("{} at line {}, column {}", CodeDescription(code), line, column);
}

auto Parse(std::string_view text, ParseOptions options) -> std::expected<Value, ParseError> {
  return Parser{text, options}.ParseDocument();
}

}  // namespace flint::json
