#include "flint/platform/terminal.hpp"

#include <cstdlib>
#include <string_view>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace flint::platform {
namespace {

auto EnvironmentIsSet(const char* name) -> bool {
  // NOLINTNEXTLINE(concurrency-mt-unsafe): environment is read-only after start-up in silica programs
  const char* value = std::getenv(name);
  return value != nullptr && !std::string_view{value}.empty() && std::string_view{value} != "0";
}

}  // namespace

auto IsTerminal(StandardStream stream) noexcept -> bool {
#ifdef _WIN32
  return _isatty(stream == StandardStream::Output ? 1 : 2) != 0;
#else
  return isatty(stream == StandardStream::Output ? STDOUT_FILENO : STDERR_FILENO) != 0;
#endif
}

auto SupportsAnsiColour(StandardStream stream) noexcept -> bool {
  // https://no-color.org takes precedence over FORCE_COLOR.
  if (EnvironmentIsSet("NO_COLOR")) {
    return false;
  }
  if (EnvironmentIsSet("FORCE_COLOR")) {
    return true;
  }
  if (!IsTerminal(stream)) {
    return false;
  }
#ifdef _WIN32
  HANDLE handle = GetStdHandle(stream == StandardStream::Output ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
  DWORD mode = 0;
  if (GetConsoleMode(handle, &mode) == 0) {
    return false;
  }
  return SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
  return true;
#endif
}

}  // namespace flint::platform
