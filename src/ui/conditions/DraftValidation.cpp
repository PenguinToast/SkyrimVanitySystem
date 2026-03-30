#include "ui/conditions/DraftValidation.h"

#include "conditions/Validation.h"
#include "ui/conditions/FunctionRegistry.h"
#include "ui/conditions/ValueEditors.h"

#include <algorithm>

namespace {
using Clause = sosr::conditions::Clause;
using Comparator = sosr::conditions::Comparator;
using Definition = sosr::conditions::Definition;
using FunctionInfo = sosr::ui::condition_editor::FunctionInfo;
using ValueEditorKind = sosr::ui::condition_editor::ValueEditorKind;
} // namespace

namespace sosr::ui::condition_editor {
bool IsBooleanComparator(const Comparator a_comparator) {
  return a_comparator == Comparator::Equal ||
         a_comparator == Comparator::NotEqual;
}

std::string BuildSuggestedConditionName(
    const std::vector<Definition> &a_conditions, const int a_seed,
    const std::function<bool(std::string_view)> &a_extraConflict) {
  const auto conflicts = [&](std::string_view a_candidate) {
    if (FindConditionFunctionInfo(a_candidate) != nullptr ||
        sosr::conditions::FindDefinitionByName(a_conditions, a_candidate) !=
            nullptr) {
      return true;
    }

    return a_extraConflict && a_extraConflict(a_candidate);
  };

  for (int index = (std::max)(a_seed, 1);; ++index) {
    const auto candidate = "Condition " + std::to_string(index);
    if (!conflicts(candidate)) {
      return candidate;
    }
  }
}

std::string BuildUniqueConditionName(
    const std::string_view a_baseName,
    const std::vector<Definition> &a_conditions,
    const std::string_view a_excludedId,
    const std::function<bool(std::string_view)> &a_reservedOrExtraConflict) {
  auto baseName = TrimText(a_baseName);
  if (baseName.empty()) {
    baseName = "Condition";
  }

  const auto conflicts = [&](std::string_view a_candidate) {
    if (FindConditionFunctionInfo(a_candidate) != nullptr ||
        sosr::conditions::FindDefinitionByName(a_conditions, a_candidate,
                                               a_excludedId) != nullptr) {
      return true;
    }
    return a_reservedOrExtraConflict &&
           a_reservedOrExtraConflict(a_candidate);
  };

  if (!conflicts(baseName)) {
    return baseName;
  }

  for (int index = 2;; ++index) {
    const auto candidate = baseName + " " + std::to_string(index);
    if (!conflicts(candidate)) {
      return candidate;
    }
  }
}

std::string ValidateConditionDraft(const Definition &a_definition,
                                   const std::vector<Definition> &a_conditions) {
  if (const auto baseValidation =
          sosr::conditions::ValidateDefinitionNameAndGraph(
              a_definition, a_conditions,
              [](std::string_view a_name) {
                return FindConditionFunctionInfo(a_name) != nullptr;
              });
      !baseValidation.empty()) {
    return baseValidation;
  }

  for (std::size_t index = 0; index < a_definition.clauses.size(); ++index) {
    const auto &clause = a_definition.clauses[index];
    std::optional<FunctionInfo> customFunctionInfo;
    const auto *functionInfo =
        ResolveConditionFunctionInfo(clause, a_conditions, customFunctionInfo);
    if (!functionInfo) {
      return "Unknown or unsupported condition function in clause " +
             std::to_string(index + 1) + ".";
    }
    if (!clause.customConditionId.empty() &&
        clause.customConditionId == a_definition.id &&
        !a_definition.id.empty()) {
      return "A condition cannot reference itself in clause " +
             std::to_string(index + 1) + ".";
    }

    for (std::uint16_t paramIndex = 0;
         paramIndex < functionInfo->parameterCount && paramIndex < 2;
         ++paramIndex) {
      const auto argument = TrimText(clause.arguments[paramIndex]);
      if (!functionInfo->parameterOptional[paramIndex] && argument.empty()) {
        return functionInfo->parameterLabels[paramIndex] +
               " is required in clause " + std::to_string(index + 1) + ".";
      }

      const auto editorKind = GetEditorKindForParamType(ResolveEditorParamType(
          functionInfo->name, paramIndex, functionInfo->parameterTypes[paramIndex]));
      if (editorKind == ValueEditorKind::Unsupported) {
        return functionInfo->parameterLabels[paramIndex] +
               " uses an unsupported parameter type in clause " +
               std::to_string(index + 1) + ".";
      }

      if ((editorKind == ValueEditorKind::Integer ||
           editorKind == ValueEditorKind::Number) &&
          !argument.empty()) {
        try {
          if (editorKind == ValueEditorKind::Integer) {
            (void)std::stoi(argument);
          } else {
            (void)std::stod(argument);
          }
        } catch (const std::exception &) {
          return functionInfo->parameterLabels[paramIndex] +
                 (editorKind == ValueEditorKind::Integer
                      ? " must be an integer in clause "
                      : " must be numeric in clause ") +
                 std::to_string(index + 1) + ".";
        }
      }
    }

    const auto comparand = TrimText(clause.comparand);
    if (functionInfo->returnsBooleanResult) {
      if (!IsBooleanComparator(clause.comparator)) {
        return "Boolean-return conditions only support == and != in clause " +
               std::to_string(index + 1) + ".";
      }
      if (!comparand.empty() && comparand != "0" && comparand != "1") {
        return "Boolean comparison value must be 0 or 1 in clause " +
               std::to_string(index + 1) + ".";
      }
    } else {
      if (comparand.empty()) {
        return "A comparison value is required in clause " +
               std::to_string(index + 1) + ".";
      }

      try {
        (void)std::stod(comparand);
      } catch (const std::exception &) {
        return "Comparison value must be numeric in clause " +
               std::to_string(index + 1) + ".";
      }
    }
  }

  return {};
}

bool ParseBooleanComparand(std::string_view a_text, bool a_defaultValue) {
  const auto trimmed = TrimText(a_text);
  if (trimmed.empty()) {
    return a_defaultValue;
  }

  try {
    return std::stod(trimmed) != 0.0;
  } catch (const std::exception &) {
    return a_defaultValue;
  }
}
} // namespace sosr::ui::condition_editor
