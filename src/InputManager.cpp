#include "InputManager.h"

#include "Keycode.h"
#include "imgui.h"
#include "ui/InputSinkBridge.h"

#include <memory>

namespace sosr {
namespace {
enum ModifierSideBit : std::uint8_t {
  kLeftShiftBit = 1u << 0u,
  kRightShiftBit = 1u << 1u,
  kLeftCtrlBit = 1u << 2u,
  kRightCtrlBit = 1u << 3u,
  kLeftAltBit = 1u << 4u,
  kRightAltBit = 1u << 5u,
};

auto GetModifierBit(const std::uint32_t a_scanCode) -> std::uint8_t {
  switch (a_scanCode) {
  case 0x2A:
    return kLeftShiftBit;
  case 0x36:
    return kRightShiftBit;
  case 0x1D:
    return kLeftCtrlBit;
  case 0x9D:
    return kRightCtrlBit;
  case 0x38:
    return kLeftAltBit;
  case 0xB8:
    return kRightAltBit;
  default:
    return 0;
  }
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

  for (const auto *event : queuedEvents) {
    switch (event->GetEventType()) {
    case RE::INPUT_EVENT_TYPE::kChar:
      break;
    case RE::INPUT_EVENT_TYPE::kButton: {
      const auto *buttonEvent = static_cast<const RE::ButtonEvent *>(event);
      const auto device = buttonEvent->device.get();
      const auto scanCode = buttonEvent->GetIDCode();
      const bool keyIsDown = buttonEvent->IsPressed();
      const bool keyWentDown = buttonEvent->IsDown();

      if (const auto modifierBit = GetModifierBit(scanCode); modifierBit != 0) {
        const bool modifierWasDown = (modifierSidesDown_ & modifierBit) != 0;
        if (modifierWasDown != keyIsDown) {
          if (keyIsDown) {
            modifierSidesDown_ |= modifierBit;
          } else {
            modifierSidesDown_ &= static_cast<std::uint8_t>(~modifierBit);
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

        if (!inputSinkState.enabled) {
          break;
        }

        if (scanCode == keycode::kTabScanCode &&
            (keyWentDown || buttonEvent->IsUp())) {
          io.AddKeyEvent(ImGuiKey_Tab, keyIsDown);
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
