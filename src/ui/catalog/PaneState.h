#pragma once

#include "ui/catalog/BrowserState.h"
#include "ui/catalog/KitsState.h"

namespace sosr::ui::catalog {
enum class HostMode : std::uint8_t { Docked, Popout };
enum class TransientPopup : std::uint8_t { None, SlotFilter };

struct PaneState {
  BrowserState browser;
  CreateKitDialogState createKitDialog;
  DeleteKitDialogState deleteKitDialog;
  HostMode hostMode{HostMode::Docked};
  bool popoutOpen{false};
  TransientPopup activeTransientPopup{TransientPopup::None};
  bool closeActiveTransientPopupRequested{false};
};
} // namespace sosr::ui::catalog
