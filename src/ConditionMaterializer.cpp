#include "ConditionMaterializer.h"

#include "conditions/GraphMetadata.h"
#include "conditions/Lowering.h"
#include "conditions/MaterializationState.h"
#include "conditions/Status.h"

#include <unordered_set>
#include <utility>

namespace {
using ConditionGraphMap =
    std::unordered_map<std::string, sosr::conditions::GraphMetadata>;
using ConditionRuntimeMap =
    std::unordered_map<std::string, sosr::conditions::MaterializationState>;

ConditionGraphMap &GetConditionGraphMap() {
  static ConditionGraphMap graph;
  return graph;
}

ConditionRuntimeMap &GetConditionRuntimeMap() {
  static ConditionRuntimeMap runtime;
  return runtime;
}
} // namespace

namespace sosr::conditions {
void RebuildConditionDependencyMetadata(std::vector<Definition> &a_conditions) {
  ConditionGraphMap rebuiltGraph;
  rebuiltGraph.reserve(a_conditions.size());

  for (auto &condition : a_conditions) {
    auto &metadata = rebuiltGraph[condition.id];
    std::unordered_set<std::string> seenReferencedConditionIds;
    for (const auto &clause : condition.clauses) {
      if (clause.customConditionId.empty()) {
        continue;
      }
      if (seenReferencedConditionIds.insert(clause.customConditionId).second) {
        metadata.referencedConditionIds.push_back(clause.customConditionId);
      }
    }
  }

  for (const auto &condition : a_conditions) {
    const auto graphIt = rebuiltGraph.find(condition.id);
    if (graphIt == rebuiltGraph.end()) {
      continue;
    }
    for (const auto &referencedConditionId :
         graphIt->second.referencedConditionIds) {
      if (auto referencedIt = rebuiltGraph.find(referencedConditionId);
          referencedIt != rebuiltGraph.end()) {
        referencedIt->second.reverseDependencyIds.push_back(condition.id);
      }
    }
  }

  auto &runtime = GetConditionRuntimeMap();
  for (auto it = runtime.begin(); it != runtime.end();) {
    if (!rebuiltGraph.contains(it->first)) {
      it = runtime.erase(it);
    } else {
      ++it;
    }
  }
  PruneConditionStatusCache(a_conditions);

  GetConditionGraphMap() = std::move(rebuiltGraph);
}

void InvalidateConditionMaterializationCaches(
    std::vector<Definition> &a_conditions) {
  (void)a_conditions;
  GetConditionRuntimeMap().clear();
  ClearConditionStatusCache();
}

void InvalidateConditionMaterializationCachesFrom(
    std::vector<Definition> &a_conditions,
    const std::string_view a_conditionId) {
  (void)a_conditions;
  std::vector<std::string> stack;
  std::unordered_set<std::string> visited;
  stack.emplace_back(a_conditionId);
  auto &graph = GetConditionGraphMap();
  auto &runtime = GetConditionRuntimeMap();

  while (!stack.empty()) {
    auto conditionId = std::move(stack.back());
    stack.pop_back();

    if (!visited.insert(conditionId).second) {
      continue;
    }

    runtime.erase(conditionId);
    EraseConditionStatusCache(conditionId);

    const auto graphIt = graph.find(conditionId);
    if (graphIt == graph.end()) {
      continue;
    }

    for (const auto &reverseDependencyId :
         graphIt->second.reverseDependencyIds) {
      stack.push_back(reverseDependencyId);
    }
  }
}

std::optional<MaterializedCondition>
MaterializeConditionById(std::string_view a_conditionId,
                         std::vector<Definition> &a_conditions) {
  auto *definition = FindDefinitionById(a_conditions, a_conditionId);
  if (!definition) {
    return std::nullopt;
  }

  auto &runtime = GetConditionRuntimeMap();
  if (auto runtimeIt = runtime.find(definition->id);
      runtimeIt != runtime.end() && runtimeIt->second.attempted) {
    if (!runtimeIt->second.valid || !runtimeIt->second.condition) {
      return std::nullopt;
    }
    return MaterializedCondition{
        .condition = runtimeIt->second.condition,
        .signature = runtimeIt->second.signature,
        .displayCnf = runtimeIt->second.displayCnf,
        .refreshTargets =
            RefreshTargets{runtimeIt->second.refreshActorFormIDs,
                           runtimeIt->second.refreshUseNearbyFallback}};
  }

  const auto lowered = LowerAndEmitCondition(*definition, a_conditions);
  if (!lowered) {
    logger::warn("Failed to materialize SVS condition {}", definition->id);
    runtime[definition->id].MarkFailure();
    return std::nullopt;
  }

  const auto refreshTargets = BuildRefreshTargets(lowered->condition);
  auto &runtimeState = runtime[definition->id];
  runtimeState.attempted = true;
  runtimeState.valid = true;
  runtimeState.condition = lowered->condition;
  runtimeState.signature = lowered->signature;
  runtimeState.displayCnf = lowered->displayCnf;
  runtimeState.refreshActorFormIDs.assign(refreshTargets.actorFormIDs.begin(),
                                          refreshTargets.actorFormIDs.end());
  runtimeState.refreshUseNearbyFallback = refreshTargets.useNearbyFallback;

  return MaterializedCondition{.condition = runtimeState.condition,
                               .signature = runtimeState.signature,
                               .displayCnf = runtimeState.displayCnf,
                               .refreshTargets = refreshTargets};
}
} // namespace sosr::conditions
