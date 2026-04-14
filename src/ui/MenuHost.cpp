#include "MenuHost.h"

#include "Hooks.h"
#include "Menu.h"

namespace RE {
class GFxCharEvent : public RE::GFxEvent {
public:
  GFxCharEvent() = default;

  explicit GFxCharEvent(std::uint32_t a_wcharCode,
                        std::uint8_t a_keyboardIndex = 0)
      : GFxEvent(EventType::kCharEvent), wcharCode(a_wcharCode),
        keyboardIndex(a_keyboardIndex) {}

  std::uint32_t wcharCode{};
  std::uint32_t keyboardIndex{};
};

static_assert(sizeof(GFxCharEvent) == 0x0C);
} // namespace RE

namespace {
[[nodiscard]] bool IsTextInputCharacter(const std::uint32_t a_codePoint) {
  return a_codePoint >= 0x20U && a_codePoint != 0x7FU;
}

struct GFxToImGuiKey {
  RE::GFxKey::Code gfxCode;
  ImGuiKey imguiKey;
};

constexpr std::array<GFxToImGuiKey, 32> kGFxKeyTable{{
    {RE::GFxKey::kAlt, ImGuiMod_Alt},
    {RE::GFxKey::kControl, ImGuiMod_Ctrl},
    {RE::GFxKey::kShift, ImGuiMod_Shift},
    {RE::GFxKey::kCapsLock, ImGuiKey_CapsLock},
    {RE::GFxKey::kHome, ImGuiKey_Home},
    {RE::GFxKey::kEnd, ImGuiKey_End},
    {RE::GFxKey::kPageUp, ImGuiKey_PageUp},
    {RE::GFxKey::kPageDown, ImGuiKey_PageDown},
    {RE::GFxKey::kComma, ImGuiKey_Comma},
    {RE::GFxKey::kPeriod, ImGuiKey_Period},
    {RE::GFxKey::kSlash, ImGuiKey_Slash},
    {RE::GFxKey::kBackslash, ImGuiKey_Backslash},
    {RE::GFxKey::kQuote, ImGuiKey_Apostrophe},
    {RE::GFxKey::kBracketLeft, ImGuiKey_LeftBracket},
    {RE::GFxKey::kBracketRight, ImGuiKey_RightBracket},
    {RE::GFxKey::kTab, ImGuiKey_Tab},
    {RE::GFxKey::kReturn, ImGuiKey_Enter},
    {RE::GFxKey::kEqual, ImGuiKey_Equal},
    {RE::GFxKey::kMinus, ImGuiKey_Minus},
    {RE::GFxKey::kEscape, ImGuiKey_Escape},
    {RE::GFxKey::kLeft, ImGuiKey_LeftArrow},
    {RE::GFxKey::kUp, ImGuiKey_UpArrow},
    {RE::GFxKey::kRight, ImGuiKey_RightArrow},
    {RE::GFxKey::kDown, ImGuiKey_DownArrow},
    {RE::GFxKey::kSpace, ImGuiKey_Space},
    {RE::GFxKey::kBackspace, ImGuiKey_Backspace},
    {RE::GFxKey::kDelete, ImGuiKey_Delete},
    {RE::GFxKey::kInsert, ImGuiKey_Insert},
    {RE::GFxKey::kKP_Multiply, ImGuiKey_KeypadMultiply},
    {RE::GFxKey::kKP_Add, ImGuiKey_KeypadAdd},
    {RE::GFxKey::kKP_Enter, ImGuiKey_KeypadEnter},
    {RE::GFxKey::kKP_Subtract, ImGuiKey_KeypadSubtract},
}};

auto GFxKeyToImGuiKey(RE::GFxKey::Code a_keyCode) -> ImGuiKey {
  if (a_keyCode >= RE::GFxKey::kA && a_keyCode <= RE::GFxKey::kZ) {
    return static_cast<ImGuiKey>(a_keyCode - RE::GFxKey::kA + ImGuiKey_A);
  }
  if (a_keyCode >= RE::GFxKey::kF1 && a_keyCode <= RE::GFxKey::kF15) {
    return static_cast<ImGuiKey>(a_keyCode - RE::GFxKey::kF1 + ImGuiKey_F1);
  }
  if (a_keyCode >= RE::GFxKey::kNum0 && a_keyCode <= RE::GFxKey::kNum9) {
    return static_cast<ImGuiKey>(a_keyCode - RE::GFxKey::kNum0 + ImGuiKey_0);
  }
  if (a_keyCode >= RE::GFxKey::kKP_0 && a_keyCode <= RE::GFxKey::kKP_9) {
    return static_cast<ImGuiKey>(a_keyCode - RE::GFxKey::kKP_0 +
                                 ImGuiKey_Keypad0);
  }

  for (const auto &[gfxCode, imguiKey] : kGFxKeyTable) {
    if (a_keyCode == gfxCode) {
      return imguiKey;
    }
  }

  return ImGuiKey_None;
}

void OnMouseEvent(const RE::GFxEvent *a_event, bool a_down) {
  const auto *mouseEvent = reinterpret_cast<const RE::GFxMouseEvent *>(a_event);
  auto &io = ImGui::GetIO();
  io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
  io.AddMouseButtonEvent(static_cast<int>(mouseEvent->button), a_down);
}

void OnMouseWheelEvent(const RE::GFxEvent *a_event) {
  const auto *mouseEvent = reinterpret_cast<const RE::GFxMouseEvent *>(a_event);
  if (!sosr::Menu::GetSingleton()->QueueSmoothScroll(mouseEvent->scrollDelta)) {
    ImGui::GetIO().AddMouseWheelEvent(0.0f, mouseEvent->scrollDelta);
  }
}

void OnKeyEvent(const RE::GFxEvent *a_event, bool a_down) {
  const auto *keyEvent = reinterpret_cast<const RE::GFxKeyEvent *>(a_event);
  const auto imguiKey = GFxKeyToImGuiKey(keyEvent->keyCode);
  if (imguiKey != ImGuiKey_None) {
    ImGui::GetIO().AddKeyEvent(imguiKey, a_down);
  }
}

void OnCharEvent(const RE::GFxEvent *a_event) {
  const auto *charEvent = reinterpret_cast<const RE::GFxCharEvent *>(a_event);
  auto &io = ImGui::GetIO();
  if (io.WantTextInput && IsTextInputCharacter(charEvent->wcharCode)) {
    io.AddInputCharacter(charEvent->wcharCode);
  }
}

void ProcessScaleformEvent(const RE::BSUIScaleformData *a_data) {
  if (!a_data || !a_data->scaleformEvent) {
    return;
  }

  switch (const auto *event = a_data->scaleformEvent; event->type.get()) {
  case RE::GFxEvent::EventType::kMouseDown:
    OnMouseEvent(event, true);
    break;
  case RE::GFxEvent::EventType::kMouseUp:
    OnMouseEvent(event, false);
    break;
  case RE::GFxEvent::EventType::kMouseWheel:
    OnMouseWheelEvent(event);
    break;
  case RE::GFxEvent::EventType::kKeyDown:
    OnKeyEvent(event, true);
    break;
  case RE::GFxEvent::EventType::kKeyUp:
    OnKeyEvent(event, false);
    break;
  case RE::GFxEvent::EventType::kCharEvent:
    OnCharEvent(event);
    break;
  default:
    break;
  }
}
} // namespace

namespace sosr {
void MenuHost::RegisterMenu() {
  static bool registered = false;
  if (registered) {
    return;
  }

  if (auto *ui = RE::UI::GetSingleton(); ui != nullptr) {
    ui->Register(MENU_NAME, Creator);
    registered = true;
  }
}

void MenuHost::PostDisplay() {
  if (hooks::IsWindowShutdownObserved()) {
    return;
  }
  Menu::GetSingleton()->Draw();
  ForceCursor();
}

RE::UI_MESSAGE_RESULTS MenuHost::ProcessMessage(RE::UIMessage &a_message) {
  switch (a_message.type.get()) {
  case RE::UI_MESSAGE_TYPE::kShow:
    Menu::GetSingleton()->OnMenuShow();
    break;
  case RE::UI_MESSAGE_TYPE::kHide:
    Menu::GetSingleton()->OnMenuHide();
    break;
  case RE::UI_MESSAGE_TYPE::kUserEvent: {
    if (const auto *data =
            reinterpret_cast<RE::BSUIMessageData *>(a_message.data);
        data != nullptr) {
      if (data->fixedStr == "Cancel") {
        Menu::GetSingleton()->HandleCancel();
        return RE::UI_MESSAGE_RESULTS::kHandled;
      }
    }
    break;
  }
  case RE::UI_MESSAGE_TYPE::kScaleformEvent: {
    const auto *scaleformData =
        reinterpret_cast<RE::BSUIScaleformData *>(a_message.data);
    if (scaleformData != nullptr && scaleformData->scaleformEvent != nullptr) {
      ProcessScaleformEvent(scaleformData);
      return RE::UI_MESSAGE_RESULTS::kHandled;
    }
    break;
  }
  default:
    break;
  }

  return IMenu::ProcessMessage(a_message);
}

RE::IMenu *MenuHost::Creator() {
  using Flags = RE::UI_MENU_FLAGS;
  auto *menu = new MenuHost();
  menu->menuFlags.set(Flags::kUpdateUsesCursor, Flags::kUsesCursor);
  menu->menuFlags.set(Flags::kCustomRendering);
  menu->menuFlags.set(Flags::kUsesMenuContext);
  menu->depthPriority = 11;

  if (Menu::GetSingleton()->PauseGameWhenOpen()) {
    menu->menuFlags.set(Flags::kPausesGame);
  }

  menu->inputContext.set(Context::kMenuMode);
  return menu;
}

void MenuHost::ForceCursor() {
  auto *ui = RE::UI::GetSingleton();
  if (!ui || ui->IsMenuOpen(RE::CursorMenu::MENU_NAME)) {
    return;
  }

  SKSE::GetTaskInterface()->AddUITask([]() {
    if (auto *messagingQueue = RE::UIMessageQueue::GetSingleton();
        messagingQueue != nullptr) {
      messagingQueue->AddMessage(RE::CursorMenu::MENU_NAME,
                                 RE::UI_MESSAGE_TYPE::kShow, nullptr);
    }
  });
}
} // namespace sosr
