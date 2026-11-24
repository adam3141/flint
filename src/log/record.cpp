#include "flint/log/record.hpp"

#include <cstddef>
#include <source_location>
#include <string_view>

namespace flint::log {

auto SourcePosition::From(const std::source_location& location) noexcept -> SourcePosition {
  return SourcePosition{
      .file = location.file_name(),
      .function = location.function_name(),
      .line = location.line(),
      .column = location.column(),
  };
}

auto ShortFunctionName(std::string_view signature) -> std::string_view {
  // Signatures look like "void ns::Type::Name(Args) const" or "ns::Type::Type(Args)". Walk back from the parameter
  // list's opening parenthesis, skipping template argument lists, to the space before the name.
  const std::size_t open = [&] {
    std::size_t depth = 0;
    for (std::size_t i = 0; i < signature.size(); ++i) {
      const char character = signature[i];
      if (character == '<') {
        ++depth;
      } else if (character == '>' && depth > 0) {
        --depth;
      } else if (character == '(' && depth == 0) {
        // "operator()" contains a parenthesis that is part of the name.
        if (signature.substr(0, i).ends_with("operator")) {
          continue;
        }
        return i;
      }
    }
    return signature.size();
  }();

  std::size_t start = open;
  std::size_t depth = 0;
  while (start > 0) {
    const char character = signature[start - 1];
    if (character == '>') {
      ++depth;
    } else if (character == '<' && depth > 0) {
      --depth;
    } else if ((character == ' ' || character == '*' || character == '&') && depth == 0) {
      break;
    }
    --start;
  }
  return signature.substr(start, open - start);
}

auto FileNameOf(std::string_view path) -> std::string_view {
  const std::size_t separator = path.find_last_of("/\\");
  return separator == std::string_view::npos ? path : path.substr(separator + 1);
}

}  // namespace flint::log
