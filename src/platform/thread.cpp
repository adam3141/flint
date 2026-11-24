#include "flint/platform/thread.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#elifdef __APPLE__
#include <pthread.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

namespace flint::platform {
namespace {

// Linux limits thread names to 16 bytes including the terminator.
constexpr std::size_t kMaxOsThreadName = 15;

auto ThreadNameStorage() -> std::string& {
  thread_local std::string name;
  return name;
}

}  // namespace

auto CurrentThreadId() noexcept -> std::uint64_t {
#ifdef _WIN32
  return GetCurrentThreadId();
#elifdef __APPLE__
  std::uint64_t id{0};
  pthread_threadid_np(nullptr, &id);
  return id;
#else
  return static_cast<std::uint64_t>(gettid());
#endif
}

auto SetCurrentThreadName(std::string_view name) -> void {
  ThreadNameStorage() = std::string{name};
  const std::string osName{name.substr(0, kMaxOsThreadName)};
#ifdef _WIN32
  const std::wstring wide(osName.begin(), osName.end());
  SetThreadDescription(GetCurrentThread(), wide.c_str());
#elifdef __APPLE__
  pthread_setname_np(osName.c_str());
#else
  pthread_setname_np(pthread_self(), osName.c_str());
#endif
}

auto CurrentThreadName() -> const std::string& {
  return ThreadNameStorage();
}

}  // namespace flint::platform
