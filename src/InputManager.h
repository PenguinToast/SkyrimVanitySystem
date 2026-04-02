#pragma once

#include <array>
#include <mutex>
#include <vector>

#include "imgui.h"

namespace sosr {
class InputManager {
public:
  static InputManager *GetSingleton();

  void OnFocusChange(bool a_focus);
  void AddEventToQueue(RE::InputEvent **a_events);
  void Flush();
  void ProcessInputEvents();
  void UpdateMousePosition() const;
  [[nodiscard]] bool IsBoundModifierDown() const;
  [[nodiscard]] std::uint32_t GetActiveModifierScanCode() const;

private:
  std::mutex inputLock_;
  std::vector<RE::InputEvent *> inputQueue_;
  std::array<bool, ImGuiKey_COUNT> imguiKeyDown_{};
  bool toggleKeyDown_{false};
  std::uint8_t modifierSidesDown_{0};
};
} // namespace sosr
