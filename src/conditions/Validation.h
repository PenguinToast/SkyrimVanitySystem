#pragma once

#include "conditions/Definition.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace sosr::conditions {
[[nodiscard]] const Definition *
FindDefinitionById(const std::vector<Definition> &a_conditions,
                   std::string_view a_id);

[[nodiscard]] Definition *
FindDefinitionById(std::vector<Definition> &a_conditions,
                   std::string_view a_id);

[[nodiscard]] const Definition *
FindDefinitionByName(const std::vector<Definition> &a_conditions,
                     std::string_view a_name,
                     std::string_view a_excludedId = {});

[[nodiscard]] bool
HasDependencyCycle(const Definition &a_draft,
                   const std::vector<Definition> &a_conditions);

[[nodiscard]] std::vector<std::string>
CollectMissingDependencyIds(const Definition &a_definition,
                            const std::vector<Definition> &a_conditions);

void RenameConditionReferences(std::vector<Definition> &a_definitions,
                               std::string_view a_oldId,
                               std::string_view a_newId);

[[nodiscard]] std::string ValidateDefinitionNameAndGraph(
    const Definition &a_definition, const std::vector<Definition> &a_conditions,
    const std::function<bool(std::string_view)> &a_reservedNameConflict = {});
} // namespace sosr::conditions
