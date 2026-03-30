#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <string>

namespace sosr::ui::workbench {
enum class FilterKind : std::uint8_t { All, ActorRef, Condition };

struct FilterState {
  FilterKind kind{FilterKind::All};
  RE::FormID actorFormID{0};
  std::string conditionId;
};

struct FilterOption {
  std::string label;
  bool isSection{false};
  FilterKind kind{FilterKind::All};
  RE::FormID actorFormID{0};
  std::string conditionId;
};
} // namespace sosr::ui::workbench
