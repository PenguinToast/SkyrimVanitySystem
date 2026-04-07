#include "conditions/Validation.h"

#include "StringUtils.h"
#include "ui/Localization.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace {
using Definition = sosr::conditions::Definition;

const Definition *
ResolveDefinitionForValidation(const std::vector<Definition> &a_conditions,
                               const Definition &a_draft,
                               std::string_view a_conditionId) {
  if (!a_draft.id.empty() && a_conditionId == a_draft.id) {
    return std::addressof(a_draft);
  }
  return sosr::conditions::FindDefinitionById(a_conditions, a_conditionId);
}

std::string BuildDependencyLabel(const Definition &a_definition) {
  return !a_definition.name.empty() ? a_definition.name : a_definition.id;
}

void CollectMissingDependencyChainsRecursive(
    const Definition &a_definition, const std::vector<Definition> &a_conditions,
    std::vector<sosr::conditions::MissingDependencyChain> &a_missingChains,
    std::vector<std::string_view> &a_visitStack,
    sosr::conditions::MissingDependencyChain &a_path) {
  if (!a_definition.id.empty()) {
    if (std::ranges::find(a_visitStack, std::string_view{a_definition.id}) !=
        a_visitStack.end()) {
      return;
    }
    a_visitStack.push_back(a_definition.id);
  }
  a_path.push_back(BuildDependencyLabel(a_definition));

  for (const auto &clause : a_definition.clauses) {
    if (clause.customConditionId.empty()) {
      continue;
    }

    const auto *referenced = sosr::conditions::FindDefinitionById(
        a_conditions, clause.customConditionId);
    if (!referenced) {
      auto chain = a_path;
      chain.push_back(clause.customConditionId);
      if (std::ranges::find(a_missingChains, chain) == a_missingChains.end()) {
        a_missingChains.push_back(std::move(chain));
      }
      continue;
    }

    CollectMissingDependencyChainsRecursive(
        *referenced, a_conditions, a_missingChains, a_visitStack, a_path);
  }

  a_path.pop_back();
  if (!a_definition.id.empty()) {
    a_visitStack.pop_back();
  }
}
} // namespace

namespace sosr::conditions {
const Definition *
FindDefinitionById(const std::vector<Definition> &a_conditions,
                   const std::string_view a_id) {
  const auto it = std::ranges::find(a_conditions, a_id, &Definition::id);
  return it != a_conditions.end() ? std::addressof(*it) : nullptr;
}

Definition *FindDefinitionById(std::vector<Definition> &a_conditions,
                               const std::string_view a_id) {
  const auto it = std::ranges::find(a_conditions, a_id, &Definition::id);
  return it != a_conditions.end() ? std::addressof(*it) : nullptr;
}

const Definition *
FindDefinitionByName(const std::vector<Definition> &a_conditions,
                     const std::string_view a_name,
                     const std::string_view a_excludedId) {
  const auto it =
      std::ranges::find_if(a_conditions, [&](const Definition &condition) {
        return condition.id != a_excludedId &&
               sosr::strings::CompareTextInsensitive(condition.name, a_name) ==
                   0;
      });
  return it != a_conditions.end() ? std::addressof(*it) : nullptr;
}

bool HasDependencyCycle(const Definition &a_draft,
                        const std::vector<Definition> &a_conditions) {
  std::vector<std::string_view> ids;
  ids.reserve(a_conditions.size() + (a_draft.id.empty() ? 0u : 1u));
  for (const auto &condition : a_conditions) {
    if (condition.id.empty()) {
      continue;
    }
    ids.push_back(condition.id);
  }
  if (!a_draft.id.empty() &&
      std::ranges::find(ids, std::string_view{a_draft.id}) == ids.end()) {
    ids.push_back(a_draft.id);
  }

  std::vector<std::uint8_t> states(ids.size(), 0);
  std::function<bool(std::string_view)> visit = [&](const std::string_view id) {
    const auto idIt = std::ranges::find(ids, id);
    if (idIt == ids.end()) {
      return false;
    }

    const auto index =
        static_cast<std::size_t>(std::distance(ids.begin(), idIt));
    if (states[index] == 1) {
      return true;
    }
    if (states[index] == 2) {
      return false;
    }

    states[index] = 1;
    if (const auto *condition =
            ResolveDefinitionForValidation(a_conditions, a_draft, id);
        condition != nullptr) {
      for (const auto &clause : condition->clauses) {
        if (clause.customConditionId.empty()) {
          continue;
        }
        if (visit(clause.customConditionId)) {
          return true;
        }
      }
    }
    states[index] = 2;
    return false;
  };

  for (const auto id : ids) {
    if (visit(id)) {
      return true;
    }
  }
  return false;
}

std::vector<MissingDependencyChain>
CollectMissingDependencyChains(const Definition &a_definition,
                               const std::vector<Definition> &a_conditions) {
  std::vector<MissingDependencyChain> missingChains;
  std::vector<std::string_view> visitStack;
  MissingDependencyChain path;
  CollectMissingDependencyChainsRecursive(a_definition, a_conditions,
                                          missingChains, visitStack, path);
  return missingChains;
}

std::string FormatMissingDependencyChain(const MissingDependencyChain &a_chain,
                                         const std::size_t a_skipFrontCount) {
  std::string formatted;
  const auto start = (std::min)(a_skipFrontCount, a_chain.size());
  bool first = true;
  for (std::size_t index = start; index < a_chain.size(); ++index) {
    if (!first) {
      formatted.append(" -> ");
    }
    formatted.append(a_chain[index]);
    first = false;
  }
  return formatted;
}

void RenameConditionReferences(std::vector<Definition> &a_definitions,
                               std::string_view a_oldId,
                               std::string_view a_newId) {
  if (a_oldId.empty() || a_oldId == a_newId) {
    return;
  }

  for (auto &definition : a_definitions) {
    for (auto &clause : definition.clauses) {
      if (clause.customConditionId == a_oldId) {
        clause.customConditionId = std::string(a_newId);
      }
    }
  }
}

std::string ValidateDefinitionNameAndGraph(
    const Definition &a_definition, const std::vector<Definition> &a_conditions,
    const std::function<bool(std::string_view)> &a_reservedNameConflict) {
  auto *localization = sosr::ui::Localization::GetSingleton();
  const auto name = sosr::strings::TrimText(a_definition.name);
  if (name.empty()) {
    return std::string(localization->Get("conditions.validation.name_required"));
  }

  if (a_reservedNameConflict && a_reservedNameConflict(name)) {
    return std::string(
        localization->Get("conditions.validation.name_conflicts_function"));
  }

  if (FindDefinitionByName(a_conditions, name, a_definition.id) != nullptr) {
    return std::string(
        localization->Get("conditions.validation.name_conflicts_custom"));
  }

  if (a_definition.clauses.empty()) {
    return std::string(localization->Get("conditions.validation.clause_required"));
  }

  if (HasDependencyCycle(a_definition, a_conditions)) {
    return std::string(localization->Get("conditions.validation.circular_dependency"));
  }

  return {};
}
} // namespace sosr::conditions
