#include "flint/log/config.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "flint/json/parser.hpp"
#include "flint/json/value.hpp"
#include "flint/log/category.hpp"
#include "flint/log/console_sink.hpp"
#include "flint/log/file_sink.hpp"
#include "flint/log/filter.hpp"
#include "flint/log/json_file_sink.hpp"
#include "flint/log/level.hpp"
#include "flint/log/logger.hpp"
#include "flint/log/network_sink.hpp"
#include "flint/log/pattern_formatter.hpp"
#include "flint/log/rotating_file.hpp"
#include "flint/log/sink_interface.hpp"
#include "flint/log/tracy_sink.hpp"
#include "flint/meta/enum.hpp"
#include "flint/platform/terminal.hpp"
#include "flint/text/ascii.hpp"

namespace flint::log {
namespace {

using Code = ConfigError::Code;

auto Fail(Code code, std::string where, std::string detail = {}) -> std::unexpected<ConfigError> {
  return std::unexpected(ConfigError{.code = code, .where = std::move(where), .detail = std::move(detail)});
}

auto WrongType(const std::string& where, std::string_view expected, const json::Value& actual) -> std::unexpected<ConfigError> {
  return Fail(Code::WrongType, where, std::format("expected {}, found {}", expected, json::KindName(actual.GetKind())));
}

// Typed accessors for optional members. Each returns nothing when the member is absent, and an error when it is
// present with the wrong type.
class Reader {
public:
  Reader(const json::Object& object, std::string where) : m_object{&object}, m_where{std::move(where)} {}

  [[nodiscard]] auto Where(std::string_view key) const -> std::string { return std::format("{}/{}", m_where, key); }

  [[nodiscard]] auto String(std::string_view key) const -> std::expected<std::optional<std::string>, ConfigError> {
    const json::Value* value = m_object->Find(key);
    if (value == nullptr) {
      return std::optional<std::string>{};
    }
    const auto text = value->AsString();
    if (!text.has_value()) {
      return WrongType(Where(key), "a string", *value);
    }
    return std::optional<std::string>{std::string{*text}};
  }

  [[nodiscard]] auto Unsigned(std::string_view key, std::uint64_t maximum) const
      -> std::expected<std::optional<std::uint64_t>, ConfigError> {
    const json::Value* value = m_object->Find(key);
    if (value == nullptr) {
      return std::optional<std::uint64_t>{};
    }
    const auto integer = value->AsInteger();
    if (!integer.has_value()) {
      return WrongType(Where(key), "an integer", *value);
    }
    if (std::cmp_less(*integer, 0) || std::cmp_greater(*integer, maximum)) {
      return Fail(Code::InvalidValue, Where(key), std::format("must be between 0 and {}", maximum));
    }
    return std::optional<std::uint64_t>{static_cast<std::uint64_t>(*integer)};
  }

  template <meta::Enumeration Enum>
  [[nodiscard]] auto Enumeration(std::string_view key) const -> std::expected<std::optional<Enum>, ConfigError> {
    auto text = String(key);
    if (!text.has_value()) {
      return std::unexpected(text.error());
    }
    if (!text->has_value()) {
      return std::optional<Enum>{};
    }
    const auto parsed = meta::EnumFromNameIgnoreCase<Enum>(**text);
    if (!parsed.has_value()) {
      return Fail(Code::InvalidValue, Where(key), std::format("unknown value '{}'", **text));
    }
    return parsed;
  }

  [[nodiscard]] auto Find(std::string_view key) const -> const json::Value* { return m_object->Find(key); }

private:
  const json::Object* m_object;
  std::string m_where;
};

auto ParseLevel(const json::Value& value, const std::string& where) -> std::expected<Level, ConfigError> {
  if (const auto name = value.AsString(); name.has_value()) {
    const auto level = FindLevel(*name);
    if (!level.has_value()) {
      return Fail(Code::UnknownLevel, where, std::format("no level named '{}'", *name));
    }
    return *level;
  }
  if (const auto severity = value.AsInteger(); severity.has_value()) {
    if (!std::in_range<std::uint16_t>(*severity)) {
      return Fail(Code::InvalidValue, where, "severity must be between 0 and 65535");
    }
    return Level{.severity = static_cast<std::uint16_t>(*severity)};
  }
  return WrongType(where, "a level name or severity", value);
}

auto RegisterLevels(const json::Value& levels) -> std::expected<void, ConfigError> {
  const json::Array* array = levels.AsArray();
  if (array == nullptr) {
    return WrongType("/levels", "an array", levels);
  }
  for (std::size_t index = 0; index < array->size(); ++index) {
    const std::string where = std::format("/levels/{}", index);
    const json::Object* object = (*array)[index].AsObject();
    if (object == nullptr) {
      return WrongType(where, "an object", (*array)[index]);
    }
    const Reader reader{*object, where};
    auto name = reader.String("name");
    if (!name.has_value()) {
      return std::unexpected(name.error());
    }
    if (!name->has_value()) {
      return Fail(Code::MissingField, reader.Where("name"));
    }
    auto severity = reader.Unsigned("severity", std::numeric_limits<std::uint16_t>::max());
    if (!severity.has_value()) {
      return std::unexpected(severity.error());
    }
    if (!severity->has_value()) {
      return Fail(Code::MissingField, reader.Where("severity"));
    }
    auto colour = reader.Enumeration<Colour>("colour");
    if (!colour.has_value()) {
      return std::unexpected(colour.error());
    }
    const auto registered = RegisterLevel(**name, static_cast<std::uint16_t>(**severity), colour->value_or(Colour::Default));
    if (!registered.has_value()) {
      return Fail(Code::InvalidLevel, where, std::string{Describe(registered.error())});
    }
  }
  return {};
}

auto RegisterCategories(const json::Value& categories) -> std::expected<void, ConfigError> {
  const json::Array* array = categories.AsArray();
  if (array == nullptr) {
    return WrongType("/categories", "an array", categories);
  }
  for (std::size_t index = 0; index < array->size(); ++index) {
    const std::string where = std::format("/categories/{}", index);
    const auto name = (*array)[index].AsString();
    if (!name.has_value()) {
      return WrongType(where, "a string", (*array)[index]);
    }
    if (auto registered = RegisterCategory(*name); !registered.has_value()) {
      return Fail(Code::InvalidCategory, where, std::string{Describe(registered.error())});
    }
  }
  return {};
}

auto ParseFilter(const Reader& reader) -> std::expected<Filter, ConfigError> {
  Filter filter{kTrace};
  if (const json::Value* minimum = reader.Find("minLevel"); minimum != nullptr) {
    auto level = ParseLevel(*minimum, reader.Where("minLevel"));
    if (!level.has_value()) {
      return std::unexpected(level.error());
    }
    filter.SetMinimum(*level);
  }
  if (const json::Value* categories = reader.Find("categories"); categories != nullptr) {
    const json::Object* object = categories->AsObject();
    if (object == nullptr) {
      return WrongType(reader.Where("categories"), "an object of category: level", *categories);
    }
    for (const json::Member& member : object->Members()) {
      const std::string where = std::format("{}/{}", reader.Where("categories"), member.key);
      auto category = RegisterCategory(member.key);
      if (!category.has_value()) {
        return Fail(Code::InvalidCategory, where, std::string{Describe(category.error())});
      }
      auto level = ParseLevel(member.value, where);
      if (!level.has_value()) {
        return std::unexpected(level.error());
      }
      filter.SetCategoryMinimum(*category, *level);
    }
  }
  return filter;
}

auto ParseRotation(const Reader& reader) -> std::expected<RotationOptions, ConfigError> {
  RotationOptions rotation;
  auto maxBytes = reader.Unsigned("maxBytes", std::numeric_limits<std::int64_t>::max());
  if (!maxBytes.has_value()) {
    return std::unexpected(maxBytes.error());
  }
  auto maxFiles = reader.Unsigned("maxFiles", std::numeric_limits<std::uint16_t>::max());
  if (!maxFiles.has_value()) {
    return std::unexpected(maxFiles.error());
  }
  rotation.maxBytes = maxBytes->value_or(rotation.maxBytes);
  rotation.maxFiles = static_cast<std::size_t>(maxFiles->value_or(rotation.maxFiles));
  return rotation;
}

auto MakeConsoleSink(const Reader& reader) -> std::expected<std::unique_ptr<SinkInterface>, ConfigError> {
  ConsoleSinkOptions options;
  auto stream = reader.String("stream");
  auto colour = reader.Enumeration<ColourMode>("colour");
  auto pattern = reader.String("pattern");
  auto timeZone = reader.Enumeration<TimeZone>("timeZone");
  if (!stream.has_value()) {
    return std::unexpected(stream.error());
  }
  if (!colour.has_value()) {
    return std::unexpected(colour.error());
  }
  if (!pattern.has_value()) {
    return std::unexpected(pattern.error());
  }
  if (!timeZone.has_value()) {
    return std::unexpected(timeZone.error());
  }
  if (stream->has_value()) {
    if (text::EqualsIgnoreCase(**stream, "stdout")) {
      options.stream = platform::StandardStream::Output;
    } else if (text::EqualsIgnoreCase(**stream, "stderr")) {
      options.stream = platform::StandardStream::Error;
    } else {
      return Fail(Code::InvalidValue, reader.Where("stream"), R"(expected "stdout" or "stderr")");
    }
  }
  options.colour = colour->value_or(options.colour);
  options.pattern = pattern->value_or(options.pattern);
  options.timeZone = timeZone->value_or(options.timeZone);
  auto sink = ConsoleSink::Create(options);
  if (!sink.has_value()) {
    return Fail(Code::InvalidPattern, reader.Where("pattern"), sink.error().Describe());
  }
  return std::unique_ptr<SinkInterface>{std::move(*sink)};
}

auto MakeFileSink(const Reader& reader) -> std::expected<std::unique_ptr<SinkInterface>, ConfigError> {
  FileSinkOptions options;
  auto path = reader.String("path");
  auto pattern = reader.String("pattern");
  auto timeZone = reader.Enumeration<TimeZone>("timeZone");
  auto rotation = ParseRotation(reader);
  if (!path.has_value()) {
    return std::unexpected(path.error());
  }
  if (!pattern.has_value()) {
    return std::unexpected(pattern.error());
  }
  if (!timeZone.has_value()) {
    return std::unexpected(timeZone.error());
  }
  if (!rotation.has_value()) {
    return std::unexpected(rotation.error());
  }
  if (!path->has_value()) {
    return Fail(Code::MissingField, reader.Where("path"));
  }
  options.path = **path;
  options.pattern = pattern->value_or(options.pattern);
  options.timeZone = timeZone->value_or(options.timeZone);
  options.rotation = *rotation;
  auto sink = FileSink::Create(options);
  if (!sink.has_value()) {
    if (const auto* patternError = std::get_if<PatternError>(&sink.error()); patternError != nullptr) {
      return Fail(Code::InvalidPattern, reader.Where("pattern"), patternError->Describe());
    }
    return Fail(Code::CannotOpenSink, reader.Where("path"), std::get<std::error_code>(sink.error()).message());
  }
  return std::unique_ptr<SinkInterface>{std::move(*sink)};
}

auto MakeJsonFileSink(const Reader& reader) -> std::expected<std::unique_ptr<SinkInterface>, ConfigError> {
  JsonFileSinkOptions options;
  auto path = reader.String("path");
  auto rotation = ParseRotation(reader);
  if (!path.has_value()) {
    return std::unexpected(path.error());
  }
  if (!rotation.has_value()) {
    return std::unexpected(rotation.error());
  }
  if (!path->has_value()) {
    return Fail(Code::MissingField, reader.Where("path"));
  }
  options.path = **path;
  options.rotation = *rotation;
  auto sink = JsonFileSink::Create(options);
  if (!sink.has_value()) {
    return Fail(Code::CannotOpenSink, reader.Where("path"), sink.error().message());
  }
  return std::unique_ptr<SinkInterface>{std::move(*sink)};
}

auto MakeNetworkSink(const Reader& reader) -> std::expected<std::unique_ptr<SinkInterface>, ConfigError> {
  NetworkSinkOptions options;
  auto host = reader.String("host");
  auto port = reader.Unsigned("port", std::numeric_limits<std::uint16_t>::max());
  auto capacity = reader.Unsigned("bufferCapacity", std::numeric_limits<std::uint32_t>::max());
  auto reconnect = reader.Unsigned("reconnectDelayMs", std::numeric_limits<std::uint32_t>::max());
  auto maxReconnect = reader.Unsigned("maxReconnectDelayMs", std::numeric_limits<std::uint32_t>::max());
  auto timeout = reader.Unsigned("connectTimeoutMs", std::numeric_limits<std::uint32_t>::max());
  for (const auto* result : {&port, &capacity, &reconnect, &maxReconnect, &timeout}) {
    if (!result->has_value()) {
      return std::unexpected(result->error());
    }
  }
  if (!host.has_value()) {
    return std::unexpected(host.error());
  }
  options.host = host->value_or(options.host);
  options.port = static_cast<std::uint16_t>(port->value_or(options.port));
  options.bufferCapacity = static_cast<std::size_t>(capacity->value_or(options.bufferCapacity));
  options.reconnectDelay = std::chrono::milliseconds{reconnect->value_or(static_cast<std::uint64_t>(options.reconnectDelay.count()))};
  options.maxReconnectDelay =
      std::chrono::milliseconds{maxReconnect->value_or(static_cast<std::uint64_t>(options.maxReconnectDelay.count()))};
  options.connectTimeout = std::chrono::milliseconds{timeout->value_or(static_cast<std::uint64_t>(options.connectTimeout.count()))};
  return std::unique_ptr<SinkInterface>{std::make_unique<NetworkSink>(std::move(options))};
}

struct ConfiguredSink {
  std::unique_ptr<SinkInterface> sink;
  Filter filter;
};

auto MakeSink(const json::Value& value, const std::string& where) -> std::expected<ConfiguredSink, ConfigError> {
  const json::Object* object = value.AsObject();
  if (object == nullptr) {
    return WrongType(where, "an object", value);
  }
  const Reader reader{*object, where};
  auto type = reader.String("type");
  if (!type.has_value()) {
    return std::unexpected(type.error());
  }
  if (!type->has_value()) {
    return Fail(Code::MissingField, reader.Where("type"));
  }
  auto filter = ParseFilter(reader);
  if (!filter.has_value()) {
    return std::unexpected(filter.error());
  }

  const std::string kind = text::ToLowerAscii(**type);
  std::expected<std::unique_ptr<SinkInterface>, ConfigError> sink;
  if (kind == "console") {
    sink = MakeConsoleSink(reader);
  } else if (kind == "file") {
    sink = MakeFileSink(reader);
  } else if (kind == "json") {
    sink = MakeJsonFileSink(reader);
  } else if (kind == "network") {
    sink = MakeNetworkSink(reader);
  } else if (kind == "tracy") {
    sink = std::unique_ptr<SinkInterface>{std::make_unique<TracySink>()};
  } else {
    return Fail(Code::UnknownSinkType, reader.Where("type"), std::format("'{}' (expected console, file, json, network or tracy)", **type));
  }
  if (!sink.has_value()) {
    return std::unexpected(sink.error());
  }
  return ConfiguredSink{.sink = std::move(*sink), .filter = std::move(*filter)};
}

auto CodeDescription(Code code) -> std::string_view {
  switch (code) {
    case Code::CannotReadFile:
      return "cannot read configuration file";
    case Code::MalformedJson:
      return "malformed JSON";
    case Code::WrongType:
      return "wrong type";
    case Code::MissingField:
      return "missing required field";
    case Code::UnknownSinkType:
      return "unknown sink type";
    case Code::UnknownLevel:
      return "unknown level";
    case Code::UnknownColour:
      return "unknown colour";
    case Code::InvalidValue:
      return "invalid value";
    case Code::InvalidLevel:
      return "invalid custom level";
    case Code::InvalidCategory:
      return "invalid category";
    case Code::InvalidPattern:
      return "invalid pattern";
    case Code::CannotOpenSink:
      return "cannot open sink";
  }
  return "configuration error";
}

}  // namespace

auto ConfigError::Describe() const -> std::string {
  std::string text{CodeDescription(code)};
  if (!where.empty()) {
    text += std::format(" at {}", where);
  }
  if (!detail.empty()) {
    text += std::format(": {}", detail);
  }
  return text;
}

auto Configure(Logger& logger, const json::Value& config) -> std::expected<void, ConfigError> {
  if (!config.IsObject()) {
    return WrongType("", "an object", config);
  }
  if (const json::Value* levels = config.Find("levels"); levels != nullptr) {
    if (auto registered = RegisterLevels(*levels); !registered.has_value()) {
      return registered;
    }
  }
  if (const json::Value* categories = config.Find("categories"); categories != nullptr) {
    if (auto registered = RegisterCategories(*categories); !registered.has_value()) {
      return registered;
    }
  }

  // Build every sink before touching the logger, so a bad entry leaves the current configuration in place.
  std::vector<ConfiguredSink> sinks;
  if (const json::Value* sinkList = config.Find("sinks"); sinkList != nullptr) {
    const json::Array* array = sinkList->AsArray();
    if (array == nullptr) {
      return WrongType("/sinks", "an array", *sinkList);
    }
    for (std::size_t index = 0; index < array->size(); ++index) {
      auto sink = MakeSink((*array)[index], std::format("/sinks/{}", index));
      if (!sink.has_value()) {
        return std::unexpected(sink.error());
      }
      sinks.push_back(std::move(*sink));
    }
  }

  logger.RemoveAllSinks();
  for (ConfiguredSink& configured : sinks) {
    logger.AddSink(std::move(configured.sink), std::move(configured.filter));
  }
  return {};
}

auto ConfigureFromText(Logger& logger, std::string_view text) -> std::expected<void, ConfigError> {
  auto parsed = json::Parse(text, json::kConfigParseOptions);
  if (!parsed.has_value()) {
    return Fail(Code::MalformedJson, "", parsed.error().Describe());
  }
  return Configure(logger, *parsed);
}

auto ConfigureFromFile(Logger& logger, const std::filesystem::path& path) -> std::expected<void, ConfigError> {
  std::ifstream stream{path, std::ios::binary};
  if (!stream.is_open()) {
    return Fail(Code::CannotReadFile, "", path.string());
  }
  const std::string text{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
  return ConfigureFromText(logger, text);
}

}  // namespace flint::log
