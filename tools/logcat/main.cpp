// flint-logcat: receives JSON Lines from NetworkSink and prints them in colour.
//
//   flint-logcat [--port 9020] [--bind 127.0.0.1] [--pattern "<pattern>"] [--raw]
//
// --raw prints the JSON lines unchanged, for piping into jq.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <iostream>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "flint/json/parser.hpp"
#include "flint/log/console_sink.hpp"
#include "flint/log/json_record.hpp"
#include "flint/log/network_sink.hpp"
#include "flint/log/pattern_formatter.hpp"
#include "flint/net/tcp_client.hpp"
#include "flint/net/tcp_listener.hpp"
#include "flint/platform/terminal.hpp"
#include "flint/text/number.hpp"

namespace {

constexpr std::uint16_t kDefaultPort = flint::log::kDefaultLogPort;
constexpr std::string_view kDefaultPattern = "{time} {level:<7} {category:<8} [{threadname}] {message}  {filename}:{line} {func}";
constexpr auto kPollInterval = std::chrono::milliseconds{50};
constexpr std::size_t kReceiveBufferSize = std::size_t{64} * 1024;
constexpr auto kIdleAcceptWait = std::chrono::milliseconds{1000};

struct Options {
  std::uint16_t port{kDefaultPort};
  std::string bind{"127.0.0.1"};
  std::string pattern{kDefaultPattern};
  bool raw{false};
};

auto ParseOptions(std::span<char*> arguments) -> std::optional<Options> {
  Options options;
  for (std::size_t i = 1; i < arguments.size(); ++i) {
    const std::string_view argument{arguments[i]};
    const bool hasValue = i + 1 < arguments.size();
    if (argument == "--raw") {
      options.raw = true;
    } else if (argument == "--port" && hasValue) {
      const auto port = flint::text::ParseInteger<std::uint16_t>(arguments[++i]);
      if (!port.has_value()) {
        return std::nullopt;
      }
      options.port = *port;
    } else if (argument == "--bind" && hasValue) {
      options.bind = arguments[++i];
    } else if (argument == "--pattern" && hasValue) {
      options.pattern = arguments[++i];
    } else {
      return std::nullopt;
    }
  }
  return options;
}

struct Connection {
  flint::net::TcpClient client;
  std::string pending;
};

// Prints one received line: decoded and coloured, or as-is when raw or not a flint record.
auto PrintLine(const std::string& line, bool raw, flint::log::ConsoleSink& console) -> void {
  if (raw) {
    std::println("{}", line);
    return;
  }
  const auto parsed = flint::json::Parse(line);
  auto record = parsed.has_value() ? flint::log::RecordFromJson(*parsed) : std::unexpected(flint::log::JsonRecordError::NotAnObject);
  if (!record.has_value()) {
    std::println("{}", line);
    return;
  }
  console.Write(record->Get());
}

// Reads what is available on a connection and prints every complete line. Closes the connection on error or EOF.
auto Drain(Connection& connection, std::span<std::byte> buffer, std::chrono::milliseconds wait, bool raw, flint::log::ConsoleSink& console)
    -> void {
  const auto received = connection.client.Receive(buffer, wait);
  if (!received.has_value()) {
    connection.client.Close();
    return;
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): received bytes are UTF-8 text
  connection.pending.append(reinterpret_cast<const char*>(buffer.data()), *received);
  for (std::size_t newline = connection.pending.find('\n'); newline != std::string::npos; newline = connection.pending.find('\n')) {
    const std::string line = connection.pending.substr(0, newline);
    connection.pending.erase(0, newline + 1);
    PrintLine(line, raw, console);
  }
  console.Flush();
}

auto Run(const Options& options) -> int {
  auto console = flint::log::ConsoleSink::Create({.stream = flint::platform::StandardStream::Output, .pattern = options.pattern});
  if (!console.has_value()) {
    std::println(std::cerr, "flint-logcat: {}", console.error().Describe());
    return 2;
  }
  auto listener = flint::net::TcpListener::Listen(options.bind, options.port);
  if (!listener.has_value()) {
    std::println(std::cerr, "flint-logcat: {}", listener.error().Describe());
    return 1;
  }
  std::println(std::cerr, "flint-logcat: listening on {}:{}", options.bind, listener->GetPort());

  std::vector<Connection> connections;
  std::array<std::byte, kReceiveBufferSize> buffer{};
  while (true) {
    // Block on accept only when there is nothing else to service.
    const auto acceptWait = connections.empty() ? kIdleAcceptWait : std::chrono::milliseconds{0};
    if (auto accepted = listener->Accept(acceptWait); accepted.has_value() && accepted->has_value()) {
      connections.push_back(Connection{.client = std::move(**accepted), .pending = {}});
      std::println(std::cerr, "flint-logcat: client connected ({} open)", connections.size());
    }
    const auto wait = kPollInterval / std::max<std::size_t>(connections.size(), 1);
    for (Connection& connection : connections) {
      Drain(connection, buffer, wait, options.raw, **console);
    }
    if (std::erase_if(connections, [](const Connection& connection) { return !connection.client.IsOpen(); }) > 0) {
      std::println(std::cerr, "flint-logcat: client disconnected ({} open)", connections.size());
    }
  }
}

}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): this is the top-level handler; only reporting the failure itself could throw
auto main(int argc, char** argv) -> int {
  try {
    const auto options = ParseOptions(std::span{argv, static_cast<std::size_t>(argc)});
    if (!options.has_value()) {
      std::println(std::cerr, "usage: flint-logcat [--port 9020] [--bind 127.0.0.1] [--pattern \"<pattern>\"] [--raw]");
      return 2;
    }
    return Run(*options);
  } catch (const std::exception& exception) {
    // Top-level handler for truly exceptional failures (coding-style.md section 13).
    std::println(std::cerr, "flint-logcat: fatal: {}", exception.what());
    return 1;
  } catch (...) {
    std::println(std::cerr, "flint-logcat: fatal: unknown exception");
    return 1;
  }
}
