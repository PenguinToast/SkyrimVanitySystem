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

  shiftDown_ = false;
  ctrlDown_ = false;
  altDown_ = false;
  imguiKeyDown_.fill(false);
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

  shiftDown_ = false;
  ctrlDown_ = false;
  altDown_ = false;
  imguiKeyDown_.fill(false);
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
      const auto scanCode = buttonEvent->GetIDCode();
      const auto imguiKey = MapScanCodeToImGuiKey(scanCode);
      const bool keyWentDown = buttonEvent->IsDown();
      const bool keyWentUp = !buttonEvent->IsPressed();

      if (imguiKey == ImGuiKey_LeftShift || imguiKey == ImGuiKey_RightShift) {
        if (keyWentDown) {
          shiftDown_ = true;
          io.AddKeyEvent(ImGuiMod_Shift, true);
        } else if (keyWentUp) {
          shiftDown_ = false;
          io.AddKeyEvent(ImGuiMod_Shift, false);
        }
      } else if (imguiKey == ImGuiKey_LeftCtrl ||
                 imguiKey == ImGuiKey_RightCtrl) {
        if (keyWentDown) {
          ctrlDown_ = true;
          io.AddKeyEvent(ImGuiMod_Ctrl, true);
        } else if (keyWentUp) {
          ctrlDown_ = false;
          io.AddKeyEvent(ImGuiMod_Ctrl, false);
        }
      } else if (imguiKey == ImGuiKey_LeftAlt ||
                 imguiKey == ImGuiKey_RightAlt) {
        if (keyWentDown) {
          altDown_ = true;
          io.AddKeyEvent(ImGuiMod_Alt, true);
        } else if (keyWentUp) {
          altDown_ = false;
          io.AddKeyEvent(ImGuiMod_Alt, false);
        }
      }

      switch (buttonEvent->device.get()) {
      case RE::INPUT_DEVICE::kMouse:
        break;
      case RE::INPUT_DEVICE::kKeyboard:
        if (wantsTextInput) {
          if (imguiKey != ImGuiKey_None) {
            const bool keyIsDown = buttonEvent->IsPressed();
            if (imguiKeyDown_[imguiKey] != keyIsDown) {
              imguiKeyDown_[imguiKey] = keyIsDown;
              io.AddKeyEvent(imguiKey, keyIsDown);
            }
          }
          break;
        }

        if (inputSinkState.capturingToggleKey && buttonEvent->IsDown()) {
          if (keycode::IsKeyModifier(scanCode)) {
            break;
          }

          ui::HandleToggleKeyCapture(scanCode, GetActiveModifierScanCode());
          io.ClearInputKeys();
          break;
        }

        if (!inputSinkState.wantsTextInput &&
            scanCode == inputSinkState.toggleKey && IsBoundModifierDown() &&
            buttonEvent->IsDown()) {
          ui::ToggleInputSinkVisibility();
          io.ClearInputKeys();
          break;
        }

        if (!inputSinkState.enabled) {
          break;
        }

        break;
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
    return shiftDown_;
  case 0x1D:
  case 0x9D:
    return ctrlDown_;
  case 0x38:
  case 0xB8:
    return altDown_;
  default:
    return false;
  }
}

std::uint32_t InputManager::GetActiveModifierScanCode() const {
  if (shiftDown_) {
    return 0x2A;
  }
  if (ctrlDown_) {
    return 0x1D;
  }
  if (altDown_) {
    return 0x38;
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
