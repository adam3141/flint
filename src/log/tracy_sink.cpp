#include "flint/log/tracy_sink.hpp"

#include "flint/log/record.hpp"

#ifdef FLINT_ENABLE_TRACY
#include <cstdint>

#include <tracy/Tracy.hpp>

#include "flint/log/category.hpp"
#include "flint/log/level.hpp"
#endif

namespace flint::log {
namespace {

#ifdef FLINT_ENABLE_TRACY
// Tracy takes 0xRRGGBB.
auto TracyColour(Colour colour) -> std::uint32_t {
  switch (colour) {
    case Colour::Default:
    case Colour::White:
    case Colour::BrightWhite:
      return 0xE0E0E0;
    case Colour::Black:
      return 0x404040;
    case Colour::Red:
    case Colour::BrightRed:
      return 0xFF5050;
    case Colour::Green:
    case Colour::BrightGreen:
      return 0x60D060;
    case Colour::Yellow:
    case Colour::BrightYellow:
      return 0xF0D040;
    case Colour::Blue:
    case Colour::BrightBlue:
      return 0x6090FF;
    case Colour::Magenta:
    case Colour::BrightMagenta:
      return 0xD070D0;
    case Colour::Cyan:
    case Colour::BrightCyan:
      return 0x50D0D0;
    case Colour::Grey:
      return 0x909090;
    case Colour::Alarm:
      return 0xFF0000;
  }
  return 0xE0E0E0;
}
#endif

}  // namespace

auto TracySink::Write([[maybe_unused]] const Record& record) -> void {
#ifdef FLINT_ENABLE_TRACY
  m_buffer.clear();
  m_buffer.append("[");
  m_buffer.append(CategoryName(record.category));
  m_buffer.append("] ");
  m_buffer.append(record.message);
  TracyMessageC(m_buffer.data(), m_buffer.size(), TracyColour(LevelColour(record.level)));
#endif
}

}  // namespace flint::log
