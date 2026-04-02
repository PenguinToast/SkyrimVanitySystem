#include "InputManager.h"

#include <Windows.h>

#include "Keycode.h"
#include "imgui.h"
#include "ui/InputSinkBridge.h"

namespace sosr {
namespace {
[[nodiscard]] bool IsTextInputCharacter(const std::uint32_t a_codePoint) {
  return a_codePoint >= 0x20U && a_codePoint != 0x7FU;
}

enum ModifierSideBit : std::uint8_t {
  kLeftShiftBit = 1u << 0u,
  kRightShiftBit = 1u << 1u,
  kLeftCtrlBit = 1u << 2u,
  kRightCtrlBit = 1u << 3u,
  kLeftAltBit = 1u << 4u,
  kRightAltBit = 1u << 5u,
};

auto GetModifierBit(const ImGuiKey a_key) -> std::uint8_t {
  switch (a_key) {
  case ImGuiKey_LeftShift:
    return kLeftShiftBit;
  case ImGuiKey_RightShift:
    return kRightShiftBit;
  case ImGuiKey_LeftCtrl:
    return kLeftCtrlBit;
  case ImGuiKey_RightCtrl:
    return kRightCtrlBit;
  case ImGuiKey_LeftAlt:
    return kLeftAltBit;
  case ImGuiKey_RightAlt:
    return kRightAltBit;
  default:
    return 0;
  }
}

auto GetModifierAggregateMask(const ImGuiKey a_key) -> std::uint8_t {
  switch (a_key) {
  case ImGuiKey_LeftShift:
  case ImGuiKey_RightShift:
    return kLeftShiftBit | kRightShiftBit;
  case ImGuiKey_LeftCtrl:
  case ImGuiKey_RightCtrl:
    return kLeftCtrlBit | kRightCtrlBit;
  case ImGuiKey_LeftAlt:
  case ImGuiKey_RightAlt:
    return kLeftAltBit | kRightAltBit;
  default:
    return 0;
  }
}

auto NormalizeScanCodeForWindows(const std::uint32_t a_scanCode)
    -> std::uint32_t {
  return (a_scanCode & 0x100U) != 0U ? (((a_scanCode & 0xFFU) << 8U) | 0xE000U)
                                     : (a_scanCode & 0xFFU);
}

auto MapScanCodeToVirtualKey(const std::uint32_t a_scanCode) -> std::uint32_t {
  const auto layout = GetKeyboardLayout(0);
  return MapVirtualKeyExW(NormalizeScanCodeForWindows(a_scanCode),
                          MAPVK_VSC_TO_VK_EX, layout);
}

auto MapVirtualKeyToImGuiKey(const std::uint32_t a_virtualKey) -> ImGuiKey {
  if (a_virtualKey >= 'A' && a_virtualKey <= 'Z') {
    return static_cast<ImGuiKey>(ImGuiKey_A + (a_virtualKey - 'A'));
  }
  if (a_virtualKey >= '0' && a_virtualKey <= '9') {
    return static_cast<ImGuiKey>(ImGuiKey_0 + (a_virtualKey - '0'));
  }
  if (a_virtualKey >= VK_F1 && a_virtualKey <= VK_F24) {
    return static_cast<ImGuiKey>(ImGuiKey_F1 + (a_virtualKey - VK_F1));
  }
  if (a_virtualKey >= VK_NUMPAD0 && a_virtualKey <= VK_NUMPAD9) {
    return static_cast<ImGuiKey>(ImGuiKey_Keypad0 +
                                 (a_virtualKey - VK_NUMPAD0));
  }

  switch (a_virtualKey) {
  case VK_OEM_1:
    return ImGuiKey_Semicolon;
  case VK_OEM_PLUS:
    return ImGuiKey_Equal;
  case VK_OEM_COMMA:
    return ImGuiKey_Comma;
  case VK_OEM_MINUS:
    return ImGuiKey_Minus;
  case VK_OEM_PERIOD:
    return ImGuiKey_Period;
  case VK_OEM_2:
    return ImGuiKey_Slash;
  case VK_OEM_3:
    return ImGuiKey_GraveAccent;
  case VK_OEM_4:
    return ImGuiKey_LeftBracket;
  case VK_OEM_5:
    return ImGuiKey_Backslash;
  case VK_OEM_6:
    return ImGuiKey_RightBracket;
  case VK_OEM_7:
    return ImGuiKey_Apostrophe;
  case VK_DECIMAL:
    return ImGuiKey_KeypadDecimal;
  case VK_DIVIDE:
    return ImGuiKey_KeypadDivide;
  case VK_MULTIPLY:
    return ImGuiKey_KeypadMultiply;
  case VK_SUBTRACT:
    return ImGuiKey_KeypadSubtract;
  case VK_ADD:
    return ImGuiKey_KeypadAdd;
  default:
    return ImGuiKey_None;
  }
}

auto MapScanCodeToImGuiKey(std::uint32_t a_scanCode) -> ImGuiKey {
  switch (a_scanCode) {
  case 0x01:
    return ImGuiKey_Escape;
  case 0x0E:
    return ImGuiKey_Backspace;
  case 0x0F:
    return ImGuiKey_Tab;
  case 0x1C:
    return ImGuiKey_Enter;
  case 0x39:
    return ImGuiKey_Space;
  case 0xC8:
    return ImGuiKey_UpArrow;
  case 0xD0:
    return ImGuiKey_DownArrow;
  case 0xCB:
    return ImGuiKey_LeftArrow;
  case 0xCD:
    return ImGuiKey_RightArrow;
  case 0xC9:
    return ImGuiKey_PageUp;
  case 0xD1:
    return ImGuiKey_PageDown;
  case 0xC7:
    return ImGuiKey_Home;
  case 0xCF:
    return ImGuiKey_End;
  case 0xD2:
    return ImGuiKey_Insert;
  case 0xD3:
    return ImGuiKey_Delete;
  case 0x2A:
    return ImGuiKey_LeftShift;
  case 0x36:
    return ImGuiKey_RightShift;
  case 0x1D:
    return ImGuiKey_LeftCtrl;
  case 0x9D:
    return ImGuiKey_RightCtrl;
  case 0x38:
    return ImGuiKey_LeftAlt;
  case 0xB8:
    return ImGuiKey_RightAlt;
  case 0x3B:
    return ImGuiKey_F1;
  case 0x3C:
    return ImGuiKey_F2;
  case 0x3D:
    return ImGuiKey_F3;
  case 0x3E:
    return ImGuiKey_F4;
  case 0x3F:
    return ImGuiKey_F5;
  case 0x40:
    return ImGuiKey_F6;
  case 0x41:
    return ImGuiKey_F7;
  case 0x42:
    return ImGuiKey_F8;
  case 0x43:
    return ImGuiKey_F9;
  case 0x44:
    return ImGuiKey_F10;
  case 0x57:
    return ImGuiKey_F11;
  case 0x58:
    return ImGuiKey_F12;
  default:
    break;
  }

  const auto virtualKey = MapScanCodeToVirtualKey(a_scanCode);
  return MapVirtualKeyToImGuiKey(virtualKey);
}

} // namespace

auto InputManager::GetSingleton() -> InputManager * {
  static InputManager singleton;
  return std::addressof(singleton);
}

void InputManager::OnFocusChange(bool a_focus) {
  if (ImGui::GetCurrentContext() == nullptr) {
    return;
  }

  auto &io = ImGui::GetIO();
  io.ClearInputKeys();
  io.ClearEventsQueue();
  io.AddFocusEvent(a_focus);

  imguiKeyDown_.fill(false);
  toggleKeyDown_ = false;
  modifierSidesDown_ = 0;
}

void InputManager::Flush() {
  {
    std::scoped_lock lock(inputLock_);
    inputQueue_.clear();
  }

  if (auto *inputMgr = RE::BSInputDeviceManager::GetSingleton();
      inputMgr != nullptr) {
    if (auto *device = inputMgr->GetKeyboard(); device != nullptr) {
      device->ClearInputState();
    }
  }

  if (auto *eventQueue = RE::BSInputEventQueue::GetSingleton();
      eventQueue != nullptr) {
    eventQueue->ClearInputQueue();
  }

  imguiKeyDown_.fill(false);
  toggleKeyDown_ = false;
  modifierSidesDown_ = 0;
  if (ImGui::GetCurrentContext() == nullptr) {
    return;
  }

  auto &io = ImGui::GetIO();
  io.ClearInputKeys();
  io.ClearEventsQueue();
}

void InputManager::AddEventToQueue(RE::InputEvent **a_events) {
  if (!a_events || !*a_events) {
    return;
  }

  std::scoped_lock lock(inputLock_);
  for (auto event = *a_events; event; event = event->next) {
    inputQueue_.push_back(event);
  }
}

void InputManager::ProcessInputEvents() {
  std::vector<RE::InputEvent *> queuedEvents;
  {
    std::scoped_lock lock(inputLock_);
    if (inputQueue_.empty()) {
      return;
    }

    queuedEvents.swap(inputQueue_);
  }

  if (ImGui::GetCurrentContext() == nullptr) {
    return;
  }

  auto &io = ImGui::GetIO();
  const auto inputSinkState = ui::GetInputSinkState();
  const bool wantsTextInput =
      inputSinkState.enabled && inputSinkState.wantsTextInput;

  for (const auto *event : queuedEvents) {
    switch (event->GetEventType()) {
    case RE::INPUT_EVENT_TYPE::kChar:
      if (wantsTextInput) {
        const auto *charEvent = static_cast<const RE::CharEvent *>(event);
        if (IsTextInputCharacter(charEvent->keyCode)) {
          io.AddInputCharacter(charEvent->keyCode);
        }
      }
      break;
    case RE::INPUT_EVENT_TYPE::kButton: {
      const auto *buttonEvent = static_cast<const RE::ButtonEvent *>(event);
      const auto device = buttonEvent->device.get();
      const auto scanCode = buttonEvent->GetIDCode();
      const auto imguiKey = MapScanCodeToImGuiKey(scanCode);
      const bool keyIsDown = buttonEvent->IsPressed();
      const bool keyWentDown = buttonEvent->IsDown();

      if (const auto modifierBit = GetModifierBit(imguiKey); modifierBit != 0) {
        const bool modifierWasDown = (modifierSidesDown_ & modifierBit) != 0;
        if (modifierWasDown != keyIsDown) {
          if (keyIsDown) {
            modifierSidesDown_ |= modifierBit;
          } else {
            modifierSidesDown_ &= static_cast<std::uint8_t>(~modifierBit);
          }

          const auto aggregateMask = GetModifierAggregateMask(imguiKey);
          const bool modifierGroupDown =
              (modifierSidesDown_ & aggregateMask) != 0;
          if (aggregateMask == (kLeftShiftBit | kRightShiftBit)) {
            io.AddKeyEvent(ImGuiMod_Shift, modifierGroupDown);
          } else if (aggregateMask == (kLeftCtrlBit | kRightCtrlBit)) {
            io.AddKeyEvent(ImGuiMod_Ctrl, modifierGroupDown);
          } else if (aggregateMask == (kLeftAltBit | kRightAltBit)) {
            io.AddKeyEvent(ImGuiMod_Alt, modifierGroupDown);
          }
        }
      }

      switch (device) {
      case RE::INPUT_DEVICE::kMouse:
        break;
      case RE::INPUT_DEVICE::kKeyboard: {
        if (inputSinkState.capturingToggleKey && keyWentDown) {
          if (keycode::IsKeyModifier(scanCode)) {
            break;
          }

          ui::HandleToggleKeyCapture(scanCode, GetActiveModifierScanCode());
          io.ClearInputKeys();
          break;
        }

        const bool isToggleKey = scanCode == inputSinkState.toggleKey;
        const bool toggleKeyWentDown = isToggleKey && keyIsDown && !toggleKeyDown_;
        if (isToggleKey) {
          toggleKeyDown_ = keyIsDown;
        }

        if (!inputSinkState.wantsTextInput &&
            scanCode == inputSinkState.toggleKey && IsBoundModifierDown() &&
            toggleKeyWentDown) {
          ui::ToggleInputSinkVisibility();
          io.ClearInputKeys();
          break;
        }

        if (wantsTextInput) {
          if (imguiKey != ImGuiKey_None) {
            if (imguiKeyDown_[imguiKey] != keyIsDown) {
              imguiKeyDown_[imguiKey] = keyIsDown;
              io.AddKeyEvent(imguiKey, keyIsDown);
            }
          }
          break;
        }

        if (!inputSinkState.enabled) {
          break;
        }

        break;
      }
      default:
        break;
      }
      break;
    }
    default:
      break;
    }
  }
}

bool InputManager::IsBoundModifierDown() const {
  switch (ui::GetInputSinkState().toggleModifier) {
  case 0x00:
    return true;
  case 0x2A:
  case 0x36:
    return (modifierSidesDown_ & (kLeftShiftBit | kRightShiftBit)) != 0;
  case 0x1D:
  case 0x9D:
    return (modifierSidesDown_ & (kLeftCtrlBit | kRightCtrlBit)) != 0;
  case 0x38:
  case 0xB8:
    return (modifierSidesDown_ & (kLeftAltBit | kRightAltBit)) != 0;
  default:
    return false;
  }
}

std::uint32_t InputManager::GetActiveModifierScanCode() const {
  if ((modifierSidesDown_ & kLeftShiftBit) != 0) {
    return 0x2A;
  }
  if ((modifierSidesDown_ & kRightShiftBit) != 0) {
    return 0x36;
  }
  if ((modifierSidesDown_ & kLeftCtrlBit) != 0) {
    return 0x1D;
  }
  if ((modifierSidesDown_ & kRightCtrlBit) != 0) {
    return 0x9D;
  }
  if ((modifierSidesDown_ & kLeftAltBit) != 0) {
    return 0x38;
  }
  if ((modifierSidesDown_ & kRightAltBit) != 0) {
    return 0xB8;
  }
  return 0;
}

void InputManager::UpdateMousePosition() const {
  if (ImGui::GetCurrentContext() == nullptr) {
    return;
  }

  auto *ui = RE::UI::GetSingleton();
  if (ui == nullptr) {
    return;
  }

  auto &io = ImGui::GetIO();
  if (ui->IsMenuOpen(RE::CursorMenu::MENU_NAME)) {
    if (const auto *menuCursor = RE::MenuCursor::GetSingleton();
        menuCursor != nullptr) {
      io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
      io.AddMousePosEvent(menuCursor->cursorPosX, menuCursor->cursorPosY);
    }
    return;
  }

  POINT cursorPos{};
  if (GetCursorPos(&cursorPos) != FALSE) {
    io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
    io.AddMousePosEvent(static_cast<float>(cursorPos.x),
                        static_cast<float>(cursorPos.y));
  }
}
} // namespace sosr
