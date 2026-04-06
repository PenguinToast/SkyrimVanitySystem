#include "Menu.h"

#include "InputManager.h"
#include "MenuHost.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "imgui_internal.h"
#include "ui/components/PinnableTooltip.h"

#include <cmath>

namespace {
constexpr float kFadeDurationSeconds = 0.30f;
constexpr float kSmoothScrollStepMultiplier = 5.0f;
constexpr float kSmoothScrollLerpFactor = 0.20f;

using UserEventFlag = RE::ControlMap::UEFlag;

constexpr auto kBlockedGameplayControls = static_cast<UserEventFlag>(
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kMovement) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kLooking) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kActivate) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kFighting) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kSneaking) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kJumping) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kPOVSwitch) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kMainFour) |
    static_cast<std::underlying_type_t<UserEventFlag>>(
        UserEventFlag::kWheelZoom) |
    static_cast<std::underlying_type_t<UserEventFlag>>(UserEventFlag::kVATS));

void AllowTextInput([[maybe_unused]] RE::ControlMap *a_controlMap,
                    [[maybe_unused]] bool a_allow) {
#ifdef EXCLUSIVE_SKYRIM_VR
  return;
#else
  using Func = decltype(&AllowTextInput);
  static REL::Relocation<Func> func{RELOCATION_ID(67252, 68552)};
  func(a_controlMap, a_allow);
#endif
}
} // namespace

namespace sosr {

void Menu::Open() {
  if (!initialized_ || enabled_ || !gameDataLoaded_) {
    return;
  }

  if (!CatalogBrowserState().initialized) {
    QueueCatalogRefresh();
  }

  if (auto *messageQueue = RE::UIMessageQueue::GetSingleton();
      messageQueue != nullptr) {
    messageQueue->AddMessage(MenuHost::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow,
                             nullptr);
  }
}

void Menu::Close() {
  if (!initialized_ || !enabled_ ||
      visibilityState_ == VisibilityState::Closing) {
    return;
  }
  visibilityState_ = VisibilityState::Closing;
}

void Menu::NotifyWindowShutdown() {
  if (auto *controlMap = RE::ControlMap::GetSingleton();
      controlMap != nullptr && wantTextInput_) {
    AllowTextInput(controlMap, false);
    wantTextInput_ = false;
  }

  if (ImGui::GetCurrentContext() != nullptr) {
    auto &io = ImGui::GetIO();
    io.MouseDrawCursor = false;
    io.ClearInputKeys();
    io.ClearEventsQueue();
  }

  ui::components::ClearPinnedTooltips();
  CloseToggleKeyCapture();
  hideMessageQueued_ = false;
  visibilityState_ = VisibilityState::Closed;
  windowAlpha_ = 0.0f;
  pendingSmoothWheelDelta_ = 0.0f;
  smoothScrollWindowId_ = 0;
  smoothScrollTargetY_ = 0.0f;
  lastAppliedSmoothScrollY_ = 0.0f;
  FocusedConditionEditorWindowSlot() = 0;
  enabled_ = false;
}

void Menu::OnMenuShow() {
  if (!initialized_ || enabled_) {
    return;
  }

  if (CatalogBrowserState().inventoryOnly) {
    catalogDerived_.gear = {};
  }

  if (auto *controlMap = RE::ControlMap::GetSingleton();
      controlMap != nullptr) {
    controlMap->ToggleControls(kBlockedGameplayControls, false, false);
  }

  auto &io = ImGui::GetIO();
  io.MouseDrawCursor = false;
  io.ClearInputKeys();
  io.ClearEventsQueue();
  visibilityState_ = VisibilityState::Opening;
  windowAlpha_ = 0.0f;
  hideMessageQueued_ = false;
  pendingSmoothWheelDelta_ = 0.0f;
  smoothScrollWindowId_ = 0;
  smoothScrollTargetY_ = 0.0f;
  lastAppliedSmoothScrollY_ = 0.0f;
  ApplyInitialWorkbenchFilterSelection();
  SyncWorkbenchRowsForCurrentFilter();
  enabled_ = true;
}

void Menu::OnMenuHide() {
  if (!initialized_ || !enabled_) {
    return;
  }

  ClearCatalogSelection();
  CloseToggleKeyCapture();

  if (auto *controlMap = RE::ControlMap::GetSingleton();
      controlMap != nullptr) {
    if (wantTextInput_) {
      AllowTextInput(controlMap, false);
      wantTextInput_ = false;
    }
    controlMap->ToggleControls(kBlockedGameplayControls, true, false);
  }

  auto &io = ImGui::GetIO();
  io.MouseDrawCursor = false;
  io.ClearInputKeys();
  io.ClearEventsQueue();
  pendingSmoothWheelDelta_ = 0.0f;
  smoothScrollWindowId_ = 0;
  smoothScrollTargetY_ = 0.0f;
  lastAppliedSmoothScrollY_ = 0.0f;
  FocusedConditionEditorWindowSlot() = 0;
  hideMessageQueued_ = false;
  visibilityState_ = VisibilityState::Closed;
  windowAlpha_ = 0.0f;
  if (io.IniFilename) {
    ImGui::SaveIniSettingsToDisk(io.IniFilename);
  }
  SaveUserSettings();
  enabled_ = false;
}

void Menu::HandleCancel() {
  auto &createDialog = CreateKitDialogState();
  if (createDialog.open) {
    createDialog.cancelRequested = true;
    return;
  }

  if (awaitingToggleKeyCapture_ || openToggleKeyPopup_) {
    CloseToggleKeyCapture();
    return;
  }

  if (!ConditionEditors().empty()) {
    auto closeEditor = [&](ConditionEditorState &a_editor) {
      a_editor.error.clear();
      a_editor.open = false;
    };

    if (FocusedConditionEditorWindowSlot() > 0) {
      const auto focusedIt = std::ranges::find(
          ConditionEditors(), FocusedConditionEditorWindowSlot(),
          &ConditionEditorState::windowSlot);
      if (focusedIt != ConditionEditors().end()) {
        closeEditor(*focusedIt);
        FocusedConditionEditorWindowSlot() = 0;
        return;
      }
    }

    closeEditor(ConditionEditors().back());
    FocusedConditionEditorWindowSlot() = 0;
    return;
  }

  if (ui::components::HasPinnedTooltips()) {
    ui::components::ClearPinnedTooltips();
    return;
  }

  Close();
}

void Menu::Toggle() {
  if (enabled_) {
    Close();
  } else {
    Open();
  }
}

bool Menu::QueueSmoothScroll(const float a_deltaY) {
  if (!enabled_ || !smoothScroll_ || a_deltaY == 0.0f) {
    return false;
  }

  pendingSmoothWheelDelta_ += a_deltaY;
  return true;
}

void Menu::QueueHideMessage() {
  if (hideMessageQueued_) {
    return;
  }

  if (auto *messageQueue = RE::UIMessageQueue::GetSingleton();
      messageQueue != nullptr) {
    messageQueue->AddMessage(MenuHost::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide,
                             nullptr);
    hideMessageQueued_ = true;
  }
}

void Menu::UpdateVisibilityAnimation(const float a_deltaTime) {
  switch (visibilityState_) {
  case VisibilityState::Opening:
    windowAlpha_ += a_deltaTime / kFadeDurationSeconds;
    if (windowAlpha_ >= 1.0f) {
      windowAlpha_ = 1.0f;
      visibilityState_ = VisibilityState::Open;
    }
    break;
  case VisibilityState::Closing:
    windowAlpha_ -= a_deltaTime / kFadeDurationSeconds;
    if (windowAlpha_ <= 0.0f) {
      windowAlpha_ = 0.0f;
      QueueHideMessage();
    }
    break;
  case VisibilityState::Open:
    windowAlpha_ = 1.0f;
    break;
  case VisibilityState::Closed:
    windowAlpha_ = 0.0f;
    break;
  }
}

void Menu::ApplySmoothScroll() {
  if (!smoothScroll_) {
    pendingSmoothWheelDelta_ = 0.0f;
    smoothScrollWindowId_ = 0;
    lastAppliedSmoothScrollY_ = 0.0f;
    return;
  }

  auto *context = ImGui::GetCurrentContext();
  if (context == nullptr) {
    pendingSmoothWheelDelta_ = 0.0f;
    smoothScrollWindowId_ = 0;
    lastAppliedSmoothScrollY_ = 0.0f;
    return;
  }

  auto findScrollWindow = [](ImGuiWindow *a_window) -> ImGuiWindow * {
    while (a_window != nullptr) {
      if (!(a_window->Flags & ImGuiWindowFlags_NoScrollWithMouse) &&
          a_window->ScrollMax.y > 0.0f) {
        return a_window;
      }
      a_window = a_window->ParentWindow;
    }
    return nullptr;
  };

  if (pendingSmoothWheelDelta_ != 0.0f) {
    if (auto *scrollWindow = findScrollWindow(context->HoveredWindow);
        scrollWindow != nullptr) {
      if (smoothScrollWindowId_ != scrollWindow->ID) {
        smoothScrollWindowId_ = scrollWindow->ID;
        smoothScrollTargetY_ = scrollWindow->Scroll.y;
        lastAppliedSmoothScrollY_ = scrollWindow->Scroll.y;
      }

      const auto scrollStep =
          ImGui::GetTextLineHeightWithSpacing() * kSmoothScrollStepMultiplier;
      smoothScrollTargetY_ = std::clamp(
          smoothScrollTargetY_ - pendingSmoothWheelDelta_ * scrollStep, 0.0f,
          scrollWindow->ScrollMax.y);
    }
    pendingSmoothWheelDelta_ = 0.0f;
  }

  if (smoothScrollWindowId_ == 0) {
    return;
  }

  ImGuiWindow *scrollWindow = nullptr;
  for (auto *window : context->Windows) {
    if (window->ID == smoothScrollWindowId_) {
      scrollWindow = window;
      break;
    }
  }

  if (scrollWindow == nullptr) {
    smoothScrollWindowId_ = 0;
    smoothScrollTargetY_ = 0.0f;
    lastAppliedSmoothScrollY_ = 0.0f;
    return;
  }

  smoothScrollTargetY_ =
      std::clamp(smoothScrollTargetY_, 0.0f, scrollWindow->ScrollMax.y);
  const auto currentScrollY = scrollWindow->Scroll.y;
  if (pendingSmoothWheelDelta_ == 0.0f &&
      std::abs(currentScrollY - lastAppliedSmoothScrollY_) > 0.5f &&
      std::abs(currentScrollY - smoothScrollTargetY_) > 0.5f) {
    smoothScrollTargetY_ = currentScrollY;
  }
  auto nextScrollY = currentScrollY + ((smoothScrollTargetY_ - currentScrollY) *
                                       kSmoothScrollLerpFactor);
  if (std::abs(smoothScrollTargetY_ - nextScrollY) <= 0.5f) {
    nextScrollY = smoothScrollTargetY_;
  }

  scrollWindow->Scroll.y = nextScrollY;
  scrollWindow->ScrollTarget.y = nextScrollY;
  scrollWindow->ScrollTargetCenterRatio.y = 0.0f;
  lastAppliedSmoothScrollY_ = nextScrollY;

  if (std::abs(smoothScrollTargetY_ - nextScrollY) <= 0.5f) {
    smoothScrollWindowId_ = 0;
    lastAppliedSmoothScrollY_ = 0.0f;
  }
}

void Menu::SyncAllowTextInput() {
  const bool currentWantTextInput = ImGui::GetIO().WantTextInput;

  if (!wantTextInput_ && currentWantTextInput) {
    if (auto *controlMap = RE::ControlMap::GetSingleton();
        controlMap != nullptr) {
      AllowTextInput(controlMap, true);
    }
  } else if (wantTextInput_ && !currentWantTextInput) {
    if (auto *controlMap = RE::ControlMap::GetSingleton();
        controlMap != nullptr) {
      AllowTextInput(controlMap, false);
    }
  }

  wantTextInput_ = currentWantTextInput;
}

void Menu::Draw() {
  if (!initialized_ || !enabled_) {
    return;
  }

  if (pendingFontAtlasRebuild_) {
    RebuildFontAtlas();
    pendingFontAtlasRebuild_ = false;
    return;
  }

  ImGui_ImplWin32_NewFrame();
  ImGui_ImplDX11_NewFrame();
  InputManager::GetSingleton()->UpdateMousePosition();
  ImGui::NewFrame();
  ui::components::BeginPinnableTooltipFrame();
  SyncAllowTextInput();

  DrawWindow();
  ui::components::EndPinnableTooltipFrame();
  ApplySmoothScroll();
  UpdateVisibilityAnimation(ImGui::GetIO().DeltaTime);

  ImGui::Render();
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
} // namespace sosr
