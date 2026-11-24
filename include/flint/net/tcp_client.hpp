#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace flint::net {

/// Why a network operation failed.
struct NetError {
  enum class Code : std::uint8_t {
    ResolveFailed,
    ConnectFailed,
    Timeout,
    SendFailed,
    ReceiveFailed,
    Closed,
    ListenFailed,
    AcceptFailed,
    NotSupported,
  };

  Code code{Code::ConnectFailed};
  /// The OS error number (errno or WSAGetLastError), or 0.
  int systemError{0};

  [[nodiscard]] auto Describe() const -> std::string;
};

/// A connected TCP stream. Move-only; closes on destruction.
class TcpClient {
public:
  /// The platform socket handle type, wide enough for POSIX descriptors and Windows SOCKETs.
  using NativeHandle = std::intptr_t;

  static constexpr NativeHandle kInvalidHandle = -1;

  TcpClient() noexcept = default;
  ~TcpClient();

  TcpClient(const TcpClient&) = delete;
  auto operator=(const TcpClient&) -> TcpClient& = delete;
  TcpClient(TcpClient&& other) noexcept;
  auto operator=(TcpClient&& other) noexcept -> TcpClient&;

  /// Connects to a host, trying each resolved address in turn.
  /// @param host A host name or numeric address.
  /// @param port The TCP port.
  /// @param timeout The longest to wait for each address.
  [[nodiscard]] static auto Connect(std::string_view host, std::uint16_t port, std::chrono::milliseconds timeout)
      -> std::expected<TcpClient, NetError>;

  /// Adopts an already-connected socket, such as one returned by accept().
  /// @param handle The socket. The client owns it from now on.
  [[nodiscard]] static auto Adopt(NativeHandle handle) noexcept -> TcpClient;

  /// Sends every byte, waiting as needed.
  /// @param data The bytes to send.
  [[nodiscard]] auto SendAll(std::span<const std::byte> data) -> std::expected<void, NetError>;

  /// Sends all of a string's bytes.
  [[nodiscard]] auto SendAll(std::string_view text) -> std::expected<void, NetError>;

  /// Waits up to `timeout` for data and reads what is available.
  /// @param buffer Where to put the data.
  /// @param timeout The longest to wait.
  /// @return The number of bytes read; 0 if the wait timed out. A closed connection is NetError::Code::Closed.
  [[nodiscard]] auto Receive(std::span<std::byte> buffer, std::chrono::milliseconds timeout) -> std::expected<std::size_t, NetError>;

  [[nodiscard]] auto IsOpen() const noexcept -> bool { return m_handle != kInvalidHandle; }

  auto Close() noexcept -> void;

private:
  explicit TcpClient(NativeHandle handle) noexcept : m_handle{handle} {}

  NativeHandle m_handle{kInvalidHandle};
};

}  // namespace flint::net
