#pragma once

#include "ui/ConditionData.h"
#include "ui/components/EditableCombo.h"

#include <RE/Skyrim.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sosr::ui::condition_editor {
struct FunctionInfo {
  std::string name;
  std::array<std::string, 2> parameterLabels{};
  std::array<bool, 2> parameterOptional{false, false};
  std::array<RE::SCRIPT_PARAM_TYPE, 2> parameterTypes{
      RE::SCRIPT_PARAM_TYPE::kForm, RE::SCRIPT_PARAM_TYPE::kForm};
  std::uint16_t parameterCount{0};
  bool returnsBooleanResult{false};
};

[[nodiscard]] std::string TrimText(std::string_view a_text);
[[nodiscard]] int CompareTextInsensitive(std::string_view a_left,
                                         std::string_view a_right);

[[nodiscard]] const std::vector<FunctionInfo> &GetConditionFunctionInfos();
[[nodiscard]] const FunctionInfo *
FindConditionFunctionInfo(std::string_view a_name);
[[nodiscard]] const FunctionInfo *ResolveConditionFunctionInfo(
    const sosr::conditions::Clause &a_clause,
    const std::vector<sosr::conditions::Definition> &a_conditions,
    std::optional<FunctionInfo> &a_customInfo);
[[nodiscard]] std::string ResolveClauseDisplayName(
    const sosr::conditions::Clause &a_clause,
    const std::vector<sosr::conditions::Definition> &a_conditions);
[[nodiscard]] std::vector<sosr::ui::components::EditableDropdownItem<std::string>>
BuildConditionFunctionItems(
    const std::vector<sosr::conditions::Definition> &a_conditions,
    std::string_view a_excludedConditionId);
} // namespace sosr::ui::condition_editor
