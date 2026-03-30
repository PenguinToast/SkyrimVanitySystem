#pragma once

#include "conditions/Definition.h"

#include <utility>
#include <string>
#include <string_view>
#include <vector>

namespace sosr::conditions {
struct LibraryChangeResult {
  std::vector<Definition> definitions;
  std::vector<std::pair<std::string, std::string>> renamedIds;
};

[[nodiscard]] std::vector<Definition> LoadConditionLibrary();

[[nodiscard]] bool CommitLibraryConditionEdit(
    const std::vector<Definition> &a_definitions,
    std::string_view a_sourceConditionId, const Definition &a_draft,
    LibraryChangeResult &a_result, std::string &a_error);

[[nodiscard]] bool CommitLibraryConditionDelete(
    const std::vector<Definition> &a_definitions, std::string_view a_conditionId,
    LibraryChangeResult &a_result, std::string &a_error);

[[nodiscard]] bool IsLibraryFileNameValid(std::string_view a_name);
} // namespace sosr::conditions
