#pragma once

#include "conditions/Definition.h"

#include <vector>

namespace sosr::conditions {
enum class DefinitionAvailability : std::uint8_t { Active, Disabled, Broken };

struct DefinitionStatus {
  DefinitionAvailability availability{DefinitionAvailability::Active};
  std::vector<std::string> missingDependencyIds;

  [[nodiscard]] bool IsActive() const {
    return availability == DefinitionAvailability::Active;
  }

  [[nodiscard]] bool IsDisabled() const {
    return availability == DefinitionAvailability::Disabled;
  }

  [[nodiscard]] bool IsBroken() const {
    return availability == DefinitionAvailability::Broken;
  }
};

[[nodiscard]] bool IsWorkbenchSelectable(const Definition &a_definition);

[[nodiscard]] DefinitionStatus
EvaluateDefinitionStatus(const Definition &a_definition,
                         const std::vector<Definition> &a_conditions);
} // namespace sosr::conditions
