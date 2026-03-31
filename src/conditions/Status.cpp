#include "conditions/Status.h"

#include "conditions/Validation.h"

#include <unordered_map>

namespace {
using ConditionStatusMap =
    std::unordered_map<std::string, sosr::conditions::DefinitionStatus>;

ConditionStatusMap &GetConditionStatusMap() {
  static ConditionStatusMap cache;
  return cache;
}
} // namespace

namespace sosr::conditions {
bool IsWorkbenchSelectable(const Definition &a_definition) {
  return a_definition.IsCatalog();
}

DefinitionStatus
EvaluateDefinitionStatus(const Definition &a_definition,
                         const std::vector<Definition> &a_conditions) {
  auto &cache = GetConditionStatusMap();
  if (!a_definition.id.empty()) {
    if (const auto it = cache.find(a_definition.id); it != cache.end()) {
      return it->second;
    }
  }

  DefinitionStatus status;
  status.missingDependencyChains =
      CollectMissingDependencyChains(a_definition, a_conditions);
  if (!status.missingDependencyChains.empty()) {
    status.availability = DefinitionAvailability::Broken;
  } else if (const auto *catalog = a_definition.GetCatalog();
             catalog != nullptr && !catalog->enabled) {
    status.availability = DefinitionAvailability::Disabled;
  }

  if (!a_definition.id.empty()) {
    cache.insert_or_assign(a_definition.id, status);
  }
  return status;
}

void PruneConditionStatusCache(const std::vector<Definition> &a_conditions) {
  auto &cache = GetConditionStatusMap();
  for (auto it = cache.begin(); it != cache.end();) {
    if (FindDefinitionById(a_conditions, it->first) == nullptr) {
      it = cache.erase(it);
    } else {
      ++it;
    }
  }
}

void ClearConditionStatusCache() { GetConditionStatusMap().clear(); }

void EraseConditionStatusCache(const std::string_view a_conditionId) {
  GetConditionStatusMap().erase(std::string(a_conditionId));
}
} // namespace sosr::conditions
