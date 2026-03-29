#pragma once

#include "conditions/Definition.h"

#include <RE/Skyrim.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sosr::conditions {
using DisplayOrClause = std::vector<std::string>;
using DisplayCnf = std::vector<DisplayOrClause>;

struct LoweredMaterialization {
  std::shared_ptr<RE::TESCondition> condition;
  std::string signature;
  DisplayCnf displayCnf;
};

[[nodiscard]] std::optional<LoweredMaterialization>
LowerAndEmitCondition(const Definition &a_definition,
                      const std::vector<Definition> &a_conditions);
} // namespace sosr::conditions
