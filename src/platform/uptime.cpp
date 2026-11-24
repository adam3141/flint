#include "flint/platform/uptime.hpp"

#include <cerrno>
#include <chrono>
#include <expected>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#elifdef __APPLE__
#include <array>
#include <ctime>

#include <sys/sysctl.h>
#include <sys/time.h>
#else
#include <ctime>
#endif

namespace flint::platform {

auto SystemUptime() -> std::expected<std::chrono::nanoseconds, std::error_code> {
#ifdef _WIN32
  // Milliseconds since boot, including time asleep.
  return std::chrono::milliseconds{GetTickCount64()};
#elifdef __APPLE__
  // kern.boottime is wall-clock time at boot; uptime is now minus that.
  timeval bootTime{};
  std::size_t size = sizeof(bootTime);
  std::array<int, 2> mib{CTL_KERN, KERN_BOOTTIME};
  if (sysctl(mib.data(), static_cast<unsigned>(mib.size()), &bootTime, &size, nullptr, 0) != 0) {
    return std::unexpected(std::error_code{errno, std::generic_category()});
  }
  const auto boot =
      std::chrono::system_clock::time_point{std::chrono::seconds{bootTime.tv_sec} + std::chrono::microseconds{bootTime.tv_usec}};
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now() - boot);
#else
  // CLOCK_BOOTTIME counts time spent suspended, unlike CLOCK_MONOTONIC.
  timespec now{};
  if (clock_gettime(CLOCK_BOOTTIME, &now) != 0) {
    return std::unexpected(std::error_code{errno, std::generic_category()});
  }
  return std::chrono::seconds{now.tv_sec} + std::chrono::nanoseconds{now.tv_nsec};
#endif
}

}  // namespace flint::platform
