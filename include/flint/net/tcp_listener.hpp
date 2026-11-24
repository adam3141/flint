#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>

#include "flint/net/tcp_client.hpp"

namespace flint::net {

/// A listening TCP socket. Move-only; closes on destruction.
class TcpListener {
public:
  TcpListener() noexcept = default;
  ~TcpListener();

  TcpListener(const TcpListener&) = delete;
  auto operator=(const TcpListener&) -> TcpListener& = delete;
  TcpListener(TcpListener&& other) noexcept;
  auto operator=(TcpListener&& other) noexcept -> TcpListener&;

  /// Starts listening.
  /// @param address A numeric address to bind, such as "127.0.0.1", or "0.0.0.0" for every interface.
  /// @param port The port, or 0 to let the OS choose (see GetPort).
  [[nodiscard]] static auto Listen(std::string_view address, std::uint16_t port) -> std::expected<TcpListener, NetError>;

  /// Waits up to `timeout` for a connection.
  /// @return The connection, nothing if the wait timed out, or an error.
  [[nodiscard]] auto Accept(std::chrono::milliseconds timeout) -> std::expected<std::optional<TcpClient>, NetError>;

  /// Returns the port actually bound.
  [[nodiscard]] auto GetPort() const noexcept -> std::uint16_t { return m_port; }

private:
  TcpListener(TcpClient::NativeHandle handle, std::uint16_t port) noexcept : m_handle{handle}, m_port{port} {}

  TcpClient::NativeHandle m_handle{TcpClient::kInvalidHandle};
  std::uint16_t m_port{0};
};

}  // namespace flint::net
