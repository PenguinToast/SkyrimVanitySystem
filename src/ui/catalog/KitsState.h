#pragma once

#include <RE/Skyrim.h>

#include "EquipmentCatalog.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace sosr::ui::catalog {
enum class KitCreationSource : std::uint8_t { Equipped, Overrides };

struct CreateKitDialogState {
  bool openRequested{false};
  bool open{false};
  bool cancelRequested{false};
  KitCreationSource source{KitCreationSource::Equipped};
  std::vector<RE::FormID> pendingFormIDs;
  std::optional<KitEntry::Layout> pendingLayout;
  std::array<char, 256> pendingName{};
  std::array<char, 256> pendingCollection{};
  std::string error;
};

struct DeleteKitDialogState {
  bool openRequested{false};
  bool open{false};
  bool cancelRequested{false};
  std::string pendingKitId;
  std::string pendingKitName;
  std::string pendingKitPath;
  std::string error;
};
} // namespace sosr::ui::catalog
