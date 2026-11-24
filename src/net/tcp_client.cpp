#include "flint/net/tcp_client.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "net/socket_posix.hpp"

namespace flint::net {

auto NetError::Describe() const -> std::string {
  std::string_view what;
  switch (code) {
    case Code::ResolveFailed:
      what = "could not resolve host";
      break;
    case Code::ConnectFailed:
      what = "could not connect";
      break;
    case Code::Timeout:
      what = "timed out";
      break;
    case Code::SendFailed:
      what = "send failed";
      break;
    case Code::ReceiveFailed:
      what = "receive failed";
      break;
    case Code::Closed:
      what = "connection closed";
      break;
    case Code::ListenFailed:
      what = "could not listen";
      break;
    case Code::AcceptFailed:
      what = "accept failed";
      break;
    case Code::NotSupported:
      what = "networking is not supported on this platform yet";
      break;
  }
  if (systemError == 0) {
    return std::string{what};
  }
  return std::format("{} ({})", what, std::strerror(systemError));  // NOLINT(concurrency-mt-unsafe): message text only
}

TcpClient::~TcpClient() {
  Close();
}

TcpClient::TcpClient(TcpClient&& other) noexcept : m_handle{std::exchange(other.m_handle, kInvalidHandle)} {}

auto TcpClient::operator=(TcpClient&& other) noexcept -> TcpClient& {
  if (this != &other) {
    Close();
    m_handle = std::exchange(other.m_handle, kInvalidHandle);
  }
  return *this;
}

auto TcpClient::Adopt(NativeHandle handle) noexcept -> TcpClient {
  return TcpClient{handle};
}

auto TcpClient::Close() noexcept -> void {
  if (m_handle != kInvalidHandle) {
    detail::CloseSocket(m_handle);
    m_handle = kInvalidHandle;
  }
}

auto TcpClient::Connect(std::string_view host, std::uint16_t port, std::chrono::milliseconds timeout)
    -> std::expected<TcpClient, NetError> {
  return detail::ConnectSocket(host, port, timeout).transform([](NativeHandle handle) { return TcpClient{handle}; });
}

// NOLINTNEXTLINE(readability-make-member-function-const): sending changes the stream's state
auto TcpClient::SendAll(std::span<const std::byte> data) -> std::expected<void, NetError> {
  if (!IsOpen()) {
    return std::unexpected(NetError{.code = NetError::Code::Closed});
  }
  return detail::SendAll(m_handle, data);
}

auto TcpClient::SendAll(std::string_view text) -> std::expected<void, NetError> {
  return SendAll(std::as_bytes(std::span{text.data(), text.size()}));
}

// NOLINTNEXTLINE(readability-make-member-function-const): receiving consumes the stream
auto TcpClient::Receive(std::span<std::byte> buffer, std::chrono::milliseconds timeout) -> std::expected<std::size_t, NetError> {
  if (!IsOpen()) {
    return std::unexpected(NetError{.code = NetError::Code::Closed});
  }
  return detail::Receive(m_handle, buffer, timeout);
}

}  // namespace flint::net
