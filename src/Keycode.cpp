#include "Keycode.h"
#include "ui/Localization.h"

#include <Windows.h>

#include <array>
#include <format>

namespace sosr::keycode {
bool IsKeyModifier(const std::uint32_t a_key) {
  return a_key == 0x2A || a_key == 0x36 || a_key == 0x1D || a_key == 0x9D ||
         a_key == 0x38 || a_key == 0xB8;
}

bool IsValidHotkey(const std::uint32_t a_key) {
  return a_key != 0x01 && a_key != 0x0F && a_key != 0x00 && a_key != 0x1C &&
         a_key != 0x39 && a_key != 0x14;
}

std::string GetKeyName(const std::uint32_t a_scanCode) {
  if (a_scanCode == 0) {
    return std::string(ui::Localization::GetSingleton()->Get("common.none"));
  }

  LONG lParam = (static_cast<LONG>(a_scanCode & 0xFFU) << 16U);
  if ((a_scanCode & 0x100U) != 0U) {
    lParam |= (1L << 24);
  }

  std::array<char, 128> keyName{};
  const auto length =
      GetKeyNameTextA(lParam, keyName.data(), static_cast<int>(keyName.size()));
  if (length > 0) {
    return {keyName.data(), static_cast<std::size_t>(length)};
  }

  return std::vformat(
      std::string(ui::Localization::GetSingleton()->Get("common.scan_code")),
      std::make_format_args(a_scanCode));
}
} // namespace sosr::keycode
