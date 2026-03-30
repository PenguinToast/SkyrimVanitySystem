#pragma once

#include "conditions/Definition.h"
#include "ui/workbench/FilterState.h"

#include <RE/Skyrim.h>

#include <optional>
#include <vector>

namespace sosr::workbench {
struct InitialFilterSelection {
  ui::workbench::FilterState filter{};
  std::optional<conditions::Definition> createdCondition;
};

[[nodiscard]] std::optional<std::string> FindFirstConditionForActorFilter(
    RE::FormID a_actorFormID,
    std::vector<conditions::Definition> &a_conditions);

[[nodiscard]] InitialFilterSelection BuildInitialFilterSelection(
    std::vector<conditions::Definition> &a_conditions,
    int a_nextConditionId);
} // namespace sosr::workbench
