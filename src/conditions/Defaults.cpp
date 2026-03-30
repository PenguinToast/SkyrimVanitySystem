#include "conditions/Defaults.h"

#include <algorithm>

namespace {
constexpr sosr::conditions::Color kDefaultConditionColor{0.55f, 0.55f, 0.55f,
                                                         1.0f};
}

namespace sosr::conditions {
Clause BuildDefaultPlayerClause() {
  Clause clause;
  clause.functionName = "GetIsReference";
  clause.arguments[0] = "Player";
  clause.comparator = Comparator::Equal;
  clause.comparand = "1";
  clause.connectiveToNext = Connective::And;
  return clause;
}

Definition BuildDefaultPlayerCondition() {
  Definition definition;
  definition.id = std::string(kDefaultConditionId);
  definition.name = "Player";
  definition.description = "Applies to Player";
  definition.EnsureCatalog().color = kDefaultConditionColor;
  definition.clauses.push_back(BuildDefaultPlayerClause());
  return definition;
}

std::string BuildConditionId(const int a_nextConditionId) {
  return "condition-" + std::to_string((std::max)(a_nextConditionId, 1));
}
} // namespace sosr::conditions
