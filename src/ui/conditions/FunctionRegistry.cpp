#include "ui/conditions/FunctionRegistry.h"

#include "StringUtils.h"
#include "conditions/Validation.h"
#include "ui/ConditionFunctionMetadata.h"
#include "ui/conditions/ValueEditors.h"

#include <RE/C/CommandTable.h>

#include <algorithm>

namespace {
using Clause = sosr::conditions::Clause;
using Definition = sosr::conditions::Definition;
using DropdownItem = sosr::ui::components::EditableDropdownItem<std::string>;
using FunctionInfo = sosr::ui::condition_editor::FunctionInfo;

bool SupportsTypedParamInput(const RE::SCRIPT_PARAM_TYPE a_type) {
  return sosr::ui::condition_editor::GetEditorKindForParamType(a_type) !=
         sosr::ui::condition_editor::ValueEditorKind::Unsupported;
}

bool SupportsTypedFunction(const RE::SCRIPT_FUNCTION &a_command) {
  if (!a_command.conditionFunction || a_command.numParams > 2 ||
      sosr::ui::conditions::IsObsoleteConditionFunction(a_command)) {
    return false;
  }

  for (std::uint16_t paramIndex = 0; paramIndex < a_command.numParams;
       ++paramIndex) {
    const auto paramType = sosr::ui::condition_editor::ResolveEditorParamType(
        a_command.functionName ? a_command.functionName : "", paramIndex,
        a_command.params ? a_command.params[paramIndex].paramType.get()
                         : RE::SCRIPT_PARAM_TYPE::kForm);
    if (!SupportsTypedParamInput(paramType)) {
      return false;
    }
  }

  return true;
}
} // namespace

namespace sosr::ui::condition_editor {
std::string TrimText(std::string_view a_text) {
  return sosr::strings::TrimText(a_text);
}

int CompareTextInsensitive(std::string_view a_left, std::string_view a_right) {
  return sosr::strings::CompareTextInsensitive(a_left, a_right);
}

const std::vector<FunctionInfo> &GetConditionFunctionInfos() {
  static const auto infos = [] {
    std::vector<FunctionInfo> result;

    const auto *commands = RE::SCRIPT_FUNCTION::GetFirstScriptCommand();
    if (!commands) {
      return result;
    }

    for (std::uint32_t index = 0;
         index < RE::SCRIPT_FUNCTION::Commands::kScriptCommandsEnd; ++index) {
      const auto &command = commands[index];
      if (!command.functionName || !SupportsTypedFunction(command)) {
        continue;
      }

      const auto name = TrimText(command.functionName);
      if (name.empty()) {
        continue;
      }

      if (std::ranges::find_if(result, [&](const FunctionInfo &a_info) {
            return CompareTextInsensitive(a_info.name, name) == 0;
          }) != result.end()) {
        continue;
      }

      FunctionInfo info;
      info.name = name;
      info.parameterCount = command.numParams;
      info.returnsBooleanResult =
          sosr::ui::conditions::ReturnsBooleanConditionResult(info.name);
      for (std::uint16_t paramIndex = 0;
           paramIndex < command.numParams && paramIndex < 2; ++paramIndex) {
        if (command.params && command.params[paramIndex].paramName) {
          info.parameterLabels[paramIndex] =
              TrimText(command.params[paramIndex].paramName);
        }
        if (info.parameterLabels[paramIndex].empty()) {
          info.parameterLabels[paramIndex] =
              "Argument " + std::to_string(paramIndex + 1);
        }
        info.parameterOptional[paramIndex] =
            command.params != nullptr && command.params[paramIndex].optional;
        info.parameterTypes[paramIndex] =
            command.params ? command.params[paramIndex].paramType.get()
                           : RE::SCRIPT_PARAM_TYPE::kForm;
      }

      result.push_back(std::move(info));
    }

    std::ranges::sort(result, [](const auto &a_left, const auto &a_right) {
      return CompareTextInsensitive(a_left.name, a_right.name) < 0;
    });
    return result;
  }();

  return infos;
}

const FunctionInfo *FindConditionFunctionInfo(std::string_view a_name) {
  const auto &infos = GetConditionFunctionInfos();
  const auto it = std::ranges::find_if(infos, [&](const auto &a_info) {
    return CompareTextInsensitive(a_info.name, a_name) == 0;
  });
  return it != infos.end() ? std::addressof(*it) : nullptr;
}

const FunctionInfo *
ResolveConditionFunctionInfo(const Clause &a_clause,
                             const std::vector<Definition> &a_conditions,
                             std::optional<FunctionInfo> &a_customInfo) {
  if (!a_clause.customConditionId.empty()) {
    if (const auto *condition = sosr::conditions::FindDefinitionById(
            a_conditions, a_clause.customConditionId);
        condition != nullptr) {
      a_customInfo = FunctionInfo{};
      a_customInfo->name = condition->name;
      a_customInfo->parameterCount = 0;
      a_customInfo->returnsBooleanResult = true;
      return std::addressof(*a_customInfo);
    }
  }

  return FindConditionFunctionInfo(a_clause.functionName);
}

std::string
ResolveClauseDisplayName(const Clause &a_clause,
                         const std::vector<Definition> &a_conditions) {
  if (!a_clause.customConditionId.empty()) {
    if (const auto *condition = sosr::conditions::FindDefinitionById(
            a_conditions, a_clause.customConditionId);
        condition != nullptr) {
      return condition->name;
    }
  }
  return a_clause.functionName;
}

std::vector<DropdownItem>
BuildConditionFunctionItems(const std::vector<Definition> &a_conditions,
                            std::string_view a_excludedConditionId) {
  std::vector<DropdownItem> items;
  const auto &infos = GetConditionFunctionInfos();

  std::vector<std::string> customNames;
  std::vector<std::string> libraryNames;
  customNames.reserve(a_conditions.size());
  libraryNames.reserve(a_conditions.size());
  for (const auto &condition : a_conditions) {
    if (condition.id == a_excludedConditionId) {
      continue;
    }
    if (condition.IsLibrary()) {
      libraryNames.push_back(condition.name);
    } else {
      customNames.push_back(condition.name);
    }
  }
  std::ranges::sort(customNames, [](const auto &a_left, const auto &a_right) {
    return CompareTextInsensitive(a_left, a_right) < 0;
  });
  std::ranges::sort(libraryNames, [](const auto &a_left, const auto &a_right) {
    return CompareTextInsensitive(a_left, a_right) < 0;
  });

  items.reserve(customNames.size() + libraryNames.size() + infos.size() + 3);
  if (!customNames.empty()) {
    items.push_back({.label = "Conditions", .value = std::nullopt});
    for (const auto &name : customNames) {
      items.push_back({.label = name, .value = name});
    }
  }
  if (!libraryNames.empty()) {
    items.push_back({.label = "Condition Library", .value = std::nullopt});
    for (const auto &name : libraryNames) {
      items.push_back({.label = name, .value = name});
    }
  }
  items.push_back({.label = "Condition Functions", .value = std::nullopt});
  for (const auto &info : infos) {
    items.push_back({.label = info.name, .value = info.name});
  }
  return items;
}
} // namespace sosr::ui::condition_editor
