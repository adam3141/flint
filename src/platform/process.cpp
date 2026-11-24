#include "flint/platform/process.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace flint::platform {
namespace {

constexpr std::size_t kMaxHostName = 256;

}  // namespace

auto CurrentProcessId() noexcept -> std::uint64_t {
#ifdef _WIN32
  return GetCurrentProcessId();
#else
  return static_cast<std::uint64_t>(getpid());
#endif
}

auto HostName() -> std::string {
  std::array<char, kMaxHostName> buffer{};
#ifdef _WIN32
  auto size = static_cast<DWORD>(buffer.size());
  if (GetComputerNameA(buffer.data(), &size) == 0) {
    return {};
  }
  return std::string{buffer.data(), size};
#else
  if (gethostname(buffer.data(), buffer.size() - 1) != 0) {
    return {};
  }
  return std::string{buffer.data()};
#endif
}

}  // namespace flint::platform
