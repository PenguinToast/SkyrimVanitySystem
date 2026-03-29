#pragma once

#include "ConditionRefreshTargets.h"
#include "conditions/Definition.h"
#include "conditions/Lowering.h"
#include "conditions/Validation.h"

#include <RE/Skyrim.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sosr::conditions {
struct MaterializedCondition {
  std::shared_ptr<RE::TESCondition> condition;
  std::string signature;
  DisplayCnf displayCnf;
  RefreshTargets refreshTargets;
};

void RebuildConditionDependencyMetadata(std::vector<Definition> &a_conditions);

void InvalidateConditionMaterializationCaches(
    std::vector<Definition> &a_conditions);

void InvalidateConditionMaterializationCachesFrom(
    std::vector<Definition> &a_conditions, std::string_view a_conditionId);

[[nodiscard]] std::optional<MaterializedCondition>
MaterializeConditionById(std::string_view a_conditionId,
                         std::vector<Definition> &a_conditions);
} // namespace sosr::conditions
