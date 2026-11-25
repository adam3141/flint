#include "flint/log/config.hpp"

#include <algorithm>
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

#include <glaze/glaze.hpp>

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

// Configuration files may have comments; a misspelt key is an error rather than silently ignored.
constexpr glz::opts kReadOptions{.null_terminated = false, .comments = true, .error_on_unknown_keys = true};

auto Fail(Code code, std::string where, std::string detail = {}) -> std::unexpected<ConfigError> {
  return std::unexpected(ConfigError{.code = code, .where = std::move(where), .detail = std::move(detail)});
}

// Turns a glaze read error into a line and column, with glaze's reason and the offending line.
auto DescribeReadError(const glz::error_ctx& error, std::string_view text) -> ConfigError {
  const std::size_t offset = std::min(error.count, text.size());
  const std::size_t previousNewline = offset == 0 ? std::string_view::npos : text.rfind('\n', offset - 1);
  const std::size_t lineStart = previousNewline == std::string_view::npos ? 0 : previousNewline + 1;
  const std::size_t lineEnd = std::min(text.find('\n', offset), text.size());
  const auto line = std::ranges::count(text.substr(0, offset), '\n') + 1;
  std::string reason = glz::format_error(error.ec);
  std::ranges::replace(reason, '_', ' ');
  std::string_view context = text.substr(lineStart, lineEnd - lineStart);
  while (!context.empty() && (context.front() == ' ' || context.front() == '\t')) {
    context.remove_prefix(1);
  }
  return ConfigError{
      .code = Code::MalformedJson,
      .where = std::format("line {}, column {}", line, offset - lineStart + 1),
      .detail = context.empty() ? reason : std::format("{} in `{}`", reason, context),
  };
}

template <meta::Enumeration Enum>
auto ParseEnumeration(const std::optional<std::string>& text, const std::string& where) -> std::expected<std::optional<Enum>, ConfigError> {
  if (!text.has_value()) {
    return std::optional<Enum>{};
  }
  const auto parsed = meta::EnumFromNameIgnoreCase<Enum>(*text);
  if (!parsed.has_value()) {
    std::string names;
    for (const Enum value : meta::EnumValues<Enum>()) {
      names += names.empty() ? "" : ", ";
      names += meta::EnumName(value);
    }
    return Fail(Code::InvalidValue, where, std::format("'{}' is not one of {}", *text, names));
  }
  return parsed;
}

auto ParseLevel(const LevelSetting& setting, const std::string& where) -> std::expected<Level, ConfigError> {
  if (const auto* severity = std::get_if<std::uint16_t>(&setting); severity != nullptr) {
    return Level{.severity = *severity};
  }
  const auto& name = std::get<std::string>(setting);
  const auto level = FindLevel(name);
  if (!level.has_value()) {
    return Fail(Code::UnknownLevel, where, std::format("no level named '{}'", name));
  }
  return *level;
}

auto RegisterLevels(const std::vector<LevelDefinition>& levels) -> std::expected<void, ConfigError> {
  for (std::size_t index = 0; index < levels.size(); ++index) {
    const LevelDefinition& definition = levels[index];
    const std::string where = std::format("/levels/{}", index);
    if (definition.name.empty()) {
      return Fail(Code::MissingField, where + "/name");
    }
    if (!definition.severity.has_value()) {
      return Fail(Code::MissingField, where + "/severity");
    }
    auto colour = ParseEnumeration<Colour>(definition.colour, where + "/colour");
    if (!colour.has_value()) {
      return std::unexpected(colour.error());
    }
    const auto registered = RegisterLevel(definition.name, *definition.severity, colour->value_or(Colour::Default));
    if (!registered.has_value()) {
      return Fail(Code::InvalidLevel, where, std::string{Describe(registered.error())});
    }
  }
  return {};
}

auto RegisterCategories(const std::vector<std::string>& categories) -> std::expected<void, ConfigError> {
  for (std::size_t index = 0; index < categories.size(); ++index) {
    if (auto registered = RegisterCategory(categories[index]); !registered.has_value()) {
      return Fail(Code::InvalidCategory, std::format("/categories/{}", index), std::string{Describe(registered.error())});
    }
  }
  return {};
}

auto ParseFilter(const SinkConfig& config, const std::string& where) -> std::expected<Filter, ConfigError> {
  Filter filter{kTrace};
  if (config.minLevel.has_value()) {
    auto level = ParseLevel(*config.minLevel, where + "/minLevel");
    if (!level.has_value()) {
      return std::unexpected(level.error());
    }
    filter.SetMinimum(*level);
  }
  for (const auto& [name, setting] : config.categories) {
    const std::string categoryWhere = std::format("{}/categories/{}", where, name);
    auto category = RegisterCategory(name);
    if (!category.has_value()) {
      return Fail(Code::InvalidCategory, categoryWhere, std::string{Describe(category.error())});
    }
    auto level = ParseLevel(setting, categoryWhere);
    if (!level.has_value()) {
      return std::unexpected(level.error());
    }
    filter.SetCategoryMinimum(*category, *level);
  }
  return filter;
}

auto ParseRotation(const SinkConfig& config, const std::string& where) -> std::expected<RotationOptions, ConfigError> {
  RotationOptions rotation;
  if (config.maxBytes.has_value() && *config.maxBytes > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return Fail(Code::InvalidValue, where + "/maxBytes", "too large");
  }
  rotation.maxBytes = config.maxBytes.value_or(rotation.maxBytes);
  rotation.maxFiles = config.maxFiles.has_value() ? std::size_t{*config.maxFiles} : rotation.maxFiles;
  return rotation;
}

auto MakeConsoleSink(const SinkConfig& config, const std::string& where) -> std::expected<std::unique_ptr<SinkInterface>, ConfigError> {
  ConsoleSinkOptions options;
  auto colour = ParseEnumeration<ColourMode>(config.colour, where + "/colour");
  if (!colour.has_value()) {
    return std::unexpected(colour.error());
  }
  auto timeZone = ParseEnumeration<TimeZone>(config.timeZone, where + "/timeZone");
  if (!timeZone.has_value()) {
    return std::unexpected(timeZone.error());
  }
  if (config.stream.has_value()) {
    if (text::EqualsIgnoreCase(*config.stream, "stdout")) {
      options.stream = platform::StandardStream::Output;
    } else if (text::EqualsIgnoreCase(*config.stream, "stderr")) {
      options.stream = platform::StandardStream::Error;
    } else {
      return Fail(Code::InvalidValue, where + "/stream", R"(expected "stdout" or "stderr")");
    }
  }
  options.colour = colour->value_or(options.colour);
  options.pattern = config.pattern.value_or(options.pattern);
  options.timeZone = timeZone->value_or(options.timeZone);
  auto sink = ConsoleSink::Create(options);
  if (!sink.has_value()) {
    return Fail(Code::InvalidPattern, where + "/pattern", sink.error().Describe());
  }
  return std::unique_ptr<SinkInterface>{std::move(*sink)};
}

auto MakeFileSink(const SinkConfig& config, const std::string& where) -> std::expected<std::unique_ptr<SinkInterface>, ConfigError> {
  if (!config.path.has_value()) {
    return Fail(Code::MissingField, where + "/path");
  }
  auto timeZone = ParseEnumeration<TimeZone>(config.timeZone, where + "/timeZone");
  if (!timeZone.has_value()) {
    return std::unexpected(timeZone.error());
  }
  auto rotation = ParseRotation(config, where);
  if (!rotation.has_value()) {
    return std::unexpected(rotation.error());
  }
  FileSinkOptions options;
  options.path = *config.path;
  options.pattern = config.pattern.value_or(options.pattern);
  options.timeZone = timeZone->value_or(options.timeZone);
  options.rotation = *rotation;
  auto sink = FileSink::Create(options);
  if (!sink.has_value()) {
    if (const auto* patternError = std::get_if<PatternError>(&sink.error()); patternError != nullptr) {
      return Fail(Code::InvalidPattern, where + "/pattern", patternError->Describe());
    }
    return Fail(Code::CannotOpenSink, where + "/path", std::get<std::error_code>(sink.error()).message());
  }
  return std::unique_ptr<SinkInterface>{std::move(*sink)};
}

auto MakeJsonFileSink(const SinkConfig& config, const std::string& where) -> std::expected<std::unique_ptr<SinkInterface>, ConfigError> {
  if (!config.path.has_value()) {
    return Fail(Code::MissingField, where + "/path");
  }
  auto rotation = ParseRotation(config, where);
  if (!rotation.has_value()) {
    return std::unexpected(rotation.error());
  }
  JsonFileSinkOptions options;
  options.path = *config.path;
  options.rotation = *rotation;
  auto sink = JsonFileSink::Create(options);
  if (!sink.has_value()) {
    return Fail(Code::CannotOpenSink, where + "/path", sink.error().message());
  }
  return std::unique_ptr<SinkInterface>{std::move(*sink)};
}

auto MakeNetworkSink(const SinkConfig& config) -> std::unique_ptr<SinkInterface> {
  NetworkSinkOptions options;
  const auto milliseconds = [](std::optional<std::uint32_t> value, std::chrono::milliseconds fallback) {
    return value.has_value() ? std::chrono::milliseconds{*value} : fallback;
  };
  options.host = config.host.value_or(options.host);
  options.port = config.port.value_or(options.port);
  options.bufferCapacity = config.bufferCapacity.has_value() ? std::size_t{*config.bufferCapacity} : options.bufferCapacity;
  options.reconnectDelay = milliseconds(config.reconnectDelayMs, options.reconnectDelay);
  options.maxReconnectDelay = milliseconds(config.maxReconnectDelayMs, options.maxReconnectDelay);
  options.connectTimeout = milliseconds(config.connectTimeoutMs, options.connectTimeout);
  return std::make_unique<NetworkSink>(std::move(options));
}

struct ConfiguredSink {
  std::unique_ptr<SinkInterface> sink;
  Filter filter;
};

auto MakeSink(const SinkConfig& config, const std::string& where) -> std::expected<ConfiguredSink, ConfigError> {
  if (config.type.empty()) {
    return Fail(Code::MissingField, where + "/type");
  }
  auto filter = ParseFilter(config, where);
  if (!filter.has_value()) {
    return std::unexpected(filter.error());
  }
  const std::string kind = text::ToLowerAscii(config.type);
  std::expected<std::unique_ptr<SinkInterface>, ConfigError> sink;
  if (kind == "console") {
    sink = MakeConsoleSink(config, where);
  } else if (kind == "file") {
    sink = MakeFileSink(config, where);
  } else if (kind == "json") {
    sink = MakeJsonFileSink(config, where);
  } else if (kind == "network") {
    sink = MakeNetworkSink(config);
  } else if (kind == "tracy") {
    sink = std::unique_ptr<SinkInterface>{std::make_unique<TracySink>()};
  } else {
    return Fail(Code::UnknownSinkType, where + "/type", std::format("'{}' (expected console, file, json, network or tracy)", config.type));
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
    case Code::MissingField:
      return "missing required field";
    case Code::UnknownSinkType:
      return "unknown sink type";
    case Code::UnknownLevel:
      return "unknown level";
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

auto Configure(Logger& logger, const LoggingConfig& config) -> std::expected<void, ConfigError> {
  if (auto registered = RegisterLevels(config.levels); !registered.has_value()) {
    return registered;
  }
  if (auto registered = RegisterCategories(config.categories); !registered.has_value()) {
    return registered;
  }
  // Build every sink before touching the logger, so a bad entry leaves the current configuration in place.
  std::vector<ConfiguredSink> sinks;
  for (std::size_t index = 0; index < config.sinks.size(); ++index) {
    auto sink = MakeSink(config.sinks[index], std::format("/sinks/{}", index));
    if (!sink.has_value()) {
      return std::unexpected(sink.error());
    }
    sinks.push_back(std::move(*sink));
  }
  logger.RemoveAllSinks();
  for (ConfiguredSink& configured : sinks) {
    logger.AddSink(std::move(configured.sink), std::move(configured.filter));
  }
  return {};
}

auto ConfigureFromText(Logger& logger, std::string_view text) -> std::expected<void, ConfigError> {
  LoggingConfig config;
  if (const glz::error_ctx error = glz::read<kReadOptions>(config, text); error) {
    return std::unexpected(DescribeReadError(error, text));
  }
  return Configure(logger, config);
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
