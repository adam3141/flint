#include "net/socket_posix.hpp"

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "flint/net/tcp_client.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace flint::net::detail {

#ifdef _WIN32

auto CloseSocket(Handle /*handle*/) noexcept -> void {}

auto ConnectSocket(std::string_view /*host*/, std::uint16_t /*port*/, std::chrono::milliseconds /*timeout*/)
    -> std::expected<Handle, NetError> {
  return std::unexpected(NetError{.code = NetError::Code::NotSupported});
}

auto SendAll(Handle /*handle*/, std::span<const std::byte> /*data*/) -> std::expected<void, NetError> {
  return std::unexpected(NetError{.code = NetError::Code::NotSupported});
}

auto Receive(Handle /*handle*/, std::span<std::byte> /*buffer*/, std::chrono::milliseconds /*timeout*/)
    -> std::expected<std::size_t, NetError> {
  return std::unexpected(NetError{.code = NetError::Code::NotSupported});
}

auto ListenSocket(std::string_view /*address*/, std::uint16_t /*port*/) -> std::expected<std::pair<Handle, std::uint16_t>, NetError> {
  return std::unexpected(NetError{.code = NetError::Code::NotSupported});
}

auto AcceptSocket(Handle /*listener*/, std::chrono::milliseconds /*timeout*/) -> std::expected<std::optional<Handle>, NetError> {
  return std::unexpected(NetError{.code = NetError::Code::NotSupported});
}

#else

namespace {

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

auto ToFd(Handle handle) -> int {
  return static_cast<int>(handle);
}

auto LastError(NetError::Code code) -> std::unexpected<NetError> {
  return std::unexpected(NetError{.code = code, .systemError = errno});
}

auto PollFor(int fd, short events, std::chrono::milliseconds timeout) -> std::expected<bool, int> {
  pollfd request{.fd = fd, .events = events, .revents = 0};
  while (true) {
    const int ready = poll(&request, 1, static_cast<int>(timeout.count()));
    if (ready >= 0) {
      return ready > 0;
    }
    if (errno != EINTR) {
      return std::unexpected(errno);
    }
  }
}

auto SetNonBlocking(int fd, bool nonBlocking) -> bool {
  const int flags = fcntl(fd, F_GETFL, 0);  // NOLINT(cppcoreguidelines-pro-type-vararg): POSIX API
  if (flags < 0) {
    return false;
  }
  // NOLINTNEXTLINE(bugprone-signed-bitwise): POSIX defines the flags as int
  const int updated = nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
  return fcntl(fd, F_SETFL, updated) == 0;  // NOLINT(cppcoreguidelines-pro-type-vararg): POSIX API
}

auto ConfigureStream(int fd) -> void {
  int enable = 1;
  // Log lines are small and latency matters more than packet count.
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));
#ifdef SO_NOSIGPIPE
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enable, sizeof(enable));
#endif
}

struct AddressInfoDeleter {
  auto operator()(addrinfo* info) const noexcept -> void { freeaddrinfo(info); }
};

auto TryConnect(const addrinfo& address, std::chrono::milliseconds timeout) -> std::expected<int, NetError> {
  const int fd = socket(address.ai_family, address.ai_socktype, address.ai_protocol);
  if (fd < 0) {
    return LastError(NetError::Code::ConnectFailed);
  }
  if (!SetNonBlocking(fd, /*nonBlocking=*/true)) {
    auto error = LastError(NetError::Code::ConnectFailed);
    close(fd);
    return error;
  }
  if (connect(fd, address.ai_addr, address.ai_addrlen) != 0 && errno != EINPROGRESS) {
    auto error = LastError(NetError::Code::ConnectFailed);
    close(fd);
    return error;
  }
  const auto ready = PollFor(fd, POLLOUT, timeout);
  if (!ready.has_value() || !*ready) {
    close(fd);
    return std::unexpected(NetError{
        .code = ready.has_value() ? NetError::Code::Timeout : NetError::Code::ConnectFailed,
        .systemError = ready.has_value() ? 0 : ready.error(),
    });
  }
  int socketError = 0;
  socklen_t length = sizeof(socketError);
  getsockopt(fd, SOL_SOCKET, SO_ERROR, &socketError, &length);
  if (socketError != 0) {
    close(fd);
    return std::unexpected(NetError{.code = NetError::Code::ConnectFailed, .systemError = socketError});
  }
  SetNonBlocking(fd, /*nonBlocking=*/false);
  ConfigureStream(fd);
  return fd;
}

}  // namespace

auto CloseSocket(Handle handle) noexcept -> void {
  close(ToFd(handle));
}

auto ConnectSocket(std::string_view host, std::uint16_t port, std::chrono::milliseconds timeout) -> std::expected<Handle, NetError> {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* found = nullptr;
  const std::string hostName{host};
  const std::string service = std::to_string(port);
  const int status = getaddrinfo(hostName.c_str(), service.c_str(), &hints, &found);
  if (status != 0) {
    return std::unexpected(NetError{.code = NetError::Code::ResolveFailed});
  }
  const std::unique_ptr<addrinfo, AddressInfoDeleter> addresses{found};

  NetError lastError{.code = NetError::Code::ConnectFailed};
  for (const addrinfo* address = addresses.get(); address != nullptr; address = address->ai_next) {
    auto connected = TryConnect(*address, timeout);
    if (connected.has_value()) {
      return Handle{*connected};
    }
    lastError = connected.error();
  }
  return std::unexpected(lastError);
}

auto SendAll(Handle handle, std::span<const std::byte> data) -> std::expected<void, NetError> {
  while (!data.empty()) {
    const ssize_t sent = send(ToFd(handle), data.data(), data.size(), kSendFlags);
    if (sent < 0) {
      if (errno == EINTR) {
        continue;
      }
      return LastError(errno == EPIPE || errno == ECONNRESET ? NetError::Code::Closed : NetError::Code::SendFailed);
    }
    data = data.subspan(static_cast<std::size_t>(sent));
  }
  return {};
}

auto Receive(Handle handle, std::span<std::byte> buffer, std::chrono::milliseconds timeout) -> std::expected<std::size_t, NetError> {
  const auto ready = PollFor(ToFd(handle), POLLIN, timeout);
  if (!ready.has_value()) {
    return std::unexpected(NetError{.code = NetError::Code::ReceiveFailed, .systemError = ready.error()});
  }
  if (!*ready) {
    return std::size_t{0};
  }
  while (true) {
    const ssize_t received = recv(ToFd(handle), buffer.data(), buffer.size(), 0);
    if (received > 0) {
      return static_cast<std::size_t>(received);
    }
    if (received == 0) {
      return std::unexpected(NetError{.code = NetError::Code::Closed});
    }
    if (errno != EINTR) {
      return LastError(errno == ECONNRESET ? NetError::Code::Closed : NetError::Code::ReceiveFailed);
    }
  }
}

auto ListenSocket(std::string_view address, std::uint16_t port) -> std::expected<std::pair<Handle, std::uint16_t>, NetError> {
  sockaddr_in bindAddress{};
  bindAddress.sin_family = AF_INET;
  bindAddress.sin_port = htons(port);
  const std::string text{address};
  if (inet_pton(AF_INET, text.c_str(), &bindAddress.sin_addr) != 1) {
    return std::unexpected(NetError{.code = NetError::Code::ResolveFailed});
  }
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return LastError(NetError::Code::ListenFailed);
  }
  int enable = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the sockets API takes sockaddr*
  if (bind(fd, reinterpret_cast<const sockaddr*>(&bindAddress), sizeof(bindAddress)) != 0 || listen(fd, SOMAXCONN) != 0) {
    auto error = LastError(NetError::Code::ListenFailed);
    close(fd);
    return error;
  }
  sockaddr_in boundAddress{};
  socklen_t length = sizeof(boundAddress);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the sockets API takes sockaddr*
  getsockname(fd, reinterpret_cast<sockaddr*>(&boundAddress), &length);
  return std::pair{Handle{fd}, ntohs(boundAddress.sin_port)};
}

auto AcceptSocket(Handle listener, std::chrono::milliseconds timeout) -> std::expected<std::optional<Handle>, NetError> {
  const auto ready = PollFor(ToFd(listener), POLLIN, timeout);
  if (!ready.has_value()) {
    return std::unexpected(NetError{.code = NetError::Code::AcceptFailed, .systemError = ready.error()});
  }
  if (!*ready) {
    return std::optional<Handle>{};
  }
  const int fd = accept(ToFd(listener), nullptr, nullptr);
  if (fd < 0) {
    return LastError(NetError::Code::AcceptFailed);
  }
  ConfigureStream(fd);
  return std::optional<Handle>{Handle{fd}};
}

#endif

}  // namespace flint::net::detail
