#include <memory>
#include <string_view>
#include <utility>

#include "flint/log/category.hpp"
#include "flint/log/console_sink.hpp"
#include "flint/log/filter.hpp"
#include "flint/log/level.hpp"
#include "flint/log/log.hpp"
#include "flint/log/logger.hpp"

namespace flint::log {
namespace {

auto MakeDefaultLogger() -> std::unique_ptr<Logger> {
  auto logger = std::make_unique<Logger>();
  if (auto console = ConsoleSink::Create(); console.has_value()) {
    logger->AddSink(std::move(*console), Filter{kInfo});
  }
  return logger;
}

}  // namespace

auto DefaultLogger() -> Logger& {
  // Constructed on first use. Destroyed after main returns, which flushes it; logging from static destructors that run
  // later is not supported.
  static const std::unique_ptr<Logger> kLogger = MakeDefaultLogger();
  return *kLogger;
}

Channel::Channel(std::string_view name) noexcept
    : m_logger{&DefaultLogger()}, m_category{RegisterCategory(name).value_or(kDefaultCategory)} {}

Channel::Channel(Logger& logger, Category category) noexcept : m_logger{&logger}, m_category{category} {}

}  // namespace flint::log
