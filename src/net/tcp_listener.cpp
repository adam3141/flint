#include "flint/net/tcp_listener.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <utility>

#include "flint/net/tcp_client.hpp"
#include "net/socket_posix.hpp"

namespace flint::net {

TcpListener::~TcpListener() {
  if (m_handle != TcpClient::kInvalidHandle) {
    detail::CloseSocket(m_handle);
  }
}

TcpListener::TcpListener(TcpListener&& other) noexcept
    : m_handle{std::exchange(other.m_handle, TcpClient::kInvalidHandle)}, m_port{std::exchange(other.m_port, std::uint16_t{0})} {}

auto TcpListener::operator=(TcpListener&& other) noexcept -> TcpListener& {
  if (this != &other) {
    if (m_handle != TcpClient::kInvalidHandle) {
      detail::CloseSocket(m_handle);
    }
    m_handle = std::exchange(other.m_handle, TcpClient::kInvalidHandle);
    m_port = std::exchange(other.m_port, std::uint16_t{0});
  }
  return *this;
}

auto TcpListener::Listen(std::string_view address, std::uint16_t port) -> std::expected<TcpListener, NetError> {
  auto bound = detail::ListenSocket(address, port);
  if (!bound.has_value()) {
    return std::unexpected(bound.error());
  }
  return TcpListener{bound->first, bound->second};
}

// NOLINTNEXTLINE(readability-make-member-function-const): accepting dequeues a pending connection
auto TcpListener::Accept(std::chrono::milliseconds timeout) -> std::expected<std::optional<TcpClient>, NetError> {
  auto accepted = detail::AcceptSocket(m_handle, timeout);
  if (!accepted.has_value()) {
    return std::unexpected(accepted.error());
  }
  if (!accepted->has_value()) {
    return std::optional<TcpClient>{};
  }
  return std::optional<TcpClient>{TcpClient::Adopt(**accepted)};
}

}  // namespace flint::net
