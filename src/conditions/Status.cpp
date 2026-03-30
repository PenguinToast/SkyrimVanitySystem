#include "conditions/Status.h"

#include "conditions/Validation.h"

namespace sosr::conditions {
bool IsWorkbenchSelectable(const Definition &a_definition) {
  return a_definition.IsCatalog();
}

DefinitionStatus EvaluateDefinitionStatus(const Definition &a_definition,
                                          const std::vector<Definition> &a_conditions) {
  DefinitionStatus status;
  status.missingDependencyIds =
      CollectMissingDependencyIds(a_definition, a_conditions);
  if (!status.missingDependencyIds.empty()) {
    status.availability = DefinitionAvailability::Broken;
    return status;
  }

  if (const auto *catalog = a_definition.GetCatalog();
      catalog != nullptr && !catalog->enabled) {
    status.availability = DefinitionAvailability::Disabled;
  }

  return status;
}
} // namespace sosr::conditions
