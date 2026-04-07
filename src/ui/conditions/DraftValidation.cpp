#include "ui/conditions/DraftValidation.h"

#include "conditions/Validation.h"
#include "ui/Localization.h"
#include "ui/conditions/FunctionRegistry.h"
#include "ui/conditions/ValueEditors.h"

#include <algorithm>
#include <format>

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
  const auto baseName =
      std::string(sosr::ui::Localization::GetSingleton()->Get(
          "conditions.default_name"));
  const auto conflicts = [&](std::string_view a_candidate) {
    if (FindConditionFunctionInfo(a_candidate) != nullptr ||
        sosr::conditions::FindDefinitionByName(a_conditions, a_candidate) !=
            nullptr) {
      return true;
    }

    return a_extraConflict && a_extraConflict(a_candidate);
  };

  for (int index = (std::max)(a_seed, 1);; ++index) {
    const auto candidate = baseName + " " + std::to_string(index);
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
    baseName = std::string(
        sosr::ui::Localization::GetSingleton()->Get("conditions.default_name"));
  }

  const auto conflicts = [&](std::string_view a_candidate) {
    if (FindConditionFunctionInfo(a_candidate) != nullptr ||
        sosr::conditions::FindDefinitionByName(a_conditions, a_candidate,
                                               a_excludedId) != nullptr) {
      return true;
    }
    return a_reservedOrExtraConflict && a_reservedOrExtraConflict(a_candidate);
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

std::string
ValidateConditionDraft(const Definition &a_definition,
                       const std::vector<Definition> &a_conditions) {
  auto *localization = sosr::ui::Localization::GetSingleton();
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
    const auto clauseNumber = index + 1;
    std::optional<FunctionInfo> customFunctionInfo;
    const auto *functionInfo =
        ResolveConditionFunctionInfo(clause, a_conditions, customFunctionInfo);
    if (!functionInfo) {
      return std::vformat(
          std::string(localization->Get("conditions.validation.unknown_function")),
          std::make_format_args(clauseNumber));
    }
    if (!clause.customConditionId.empty() &&
        clause.customConditionId == a_definition.id &&
        !a_definition.id.empty()) {
      return std::vformat(
          std::string(localization->Get("conditions.validation.self_reference")),
          std::make_format_args(clauseNumber));
    }

    for (std::uint16_t paramIndex = 0;
         paramIndex < functionInfo->parameterCount && paramIndex < 2;
         ++paramIndex) {
      const auto argument = TrimText(clause.arguments[paramIndex]);
      const auto &parameterLabel = functionInfo->parameterLabels[paramIndex];
      if (!functionInfo->parameterOptional[paramIndex] && argument.empty()) {
        return std::vformat(
            std::string(localization->Get("conditions.validation.parameter_required")),
            std::make_format_args(parameterLabel, clauseNumber));
      }

      const auto editorKind = GetEditorKindForParamType(
          ResolveEditorParamType(functionInfo->name, paramIndex,
                                 functionInfo->parameterTypes[paramIndex]));
      if (editorKind == ValueEditorKind::Unsupported) {
        return std::vformat(
            std::string(localization->Get("conditions.validation.parameter_unsupported")),
            std::make_format_args(parameterLabel, clauseNumber));
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
          return std::vformat(
              std::string(localization->Get(
                  editorKind == ValueEditorKind::Integer
                      ? "conditions.validation.parameter_integer"
                      : "conditions.validation.parameter_numeric")),
              std::make_format_args(parameterLabel, clauseNumber));
        }
      }
    }

    const auto comparand = TrimText(clause.comparand);
    if (functionInfo->returnsBooleanResult) {
      if (!IsBooleanComparator(clause.comparator)) {
        return std::vformat(
            std::string(localization->Get("conditions.validation.boolean_comparator")),
            std::make_format_args(clauseNumber));
      }
      if (!comparand.empty() && comparand != "0" && comparand != "1") {
        return std::vformat(
            std::string(localization->Get("conditions.validation.boolean_value")),
            std::make_format_args(clauseNumber));
      }
    } else {
      if (comparand.empty()) {
        return std::vformat(
            std::string(localization->Get("conditions.validation.comparison_required")),
            std::make_format_args(clauseNumber));
      }

      try {
        (void)std::stod(comparand);
      } catch (const std::exception &) {
        return std::vformat(
            std::string(localization->Get("conditions.validation.comparison_numeric")),
            std::make_format_args(clauseNumber));
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
