#include "Menu.h"

#include "ConditionMaterializer.h"
#include "conditions/Creation.h"
#include "conditions/Defaults.h"
#include "ui/conditions/EditorSupport.h"

#include <algorithm>

namespace sosr {
using ConditionDefinition = ui::conditions::Definition;
using ConditionFunctionInfo = ui::condition_editor::FunctionInfo;
using ConditionValueEditorKind = ui::condition_editor::ValueEditorKind;
using ui::condition_editor::BuildSuggestedConditionName;
using ui::condition_editor::CompareTextInsensitive;
using ui::condition_editor::FindConditionFunctionInfo;
using ui::condition_editor::GetEditorKindForParamType;
using ui::condition_editor::IsBooleanComparator;
using ui::condition_editor::ResolveConditionFunctionInfo;
using ui::condition_editor::ResolveEditorParamType;
using ui::condition_editor::TrimText;
using ui::condition_editor::ValidateConditionDraft;

void Menu::EnsureDefaultConditions() {
  if (!ConditionDefinitions().empty()) {
    return;
  }

  ConditionDefinitions().push_back(conditions::BuildDefaultPlayerCondition());
  NextConditionId() = 2;
  sosr::conditions::RebuildConditionDependencyMetadata(ConditionDefinitions());
  sosr::conditions::InvalidateConditionMaterializationCaches(ConditionDefinitions());
}

int Menu::AllocateConditionEditorWindowSlot() const {
  int slot = 1;
  while (true) {
    const auto it = std::ranges::find(ConditionEditors(), slot,
                                      &ConditionEditorState::windowSlot);
    if (it == ConditionEditors().end()) {
      return slot;
    }
    ++slot;
  }
}

void Menu::OpenNewConditionDialog() {
  std::vector<ui::conditions::Color> existingColors;
  existingColors.reserve(ConditionDefinitions().size() + ConditionEditors().size());
  for (const auto &condition : ConditionDefinitions()) {
    existingColors.push_back(condition.color);
  }
  for (const auto &existingEditor : ConditionEditors()) {
    if (existingEditor.isNew) {
      existingColors.push_back(existingEditor.draft.color);
    }
  }

  const auto suggestedName = BuildSuggestedConditionName(
      ConditionDefinitions(), NextConditionId(), [&](std::string_view a_candidate) {
        return std::ranges::any_of(
            ConditionEditors(), [&](const ConditionEditorState &a_editor) {
              return a_editor.isNew &&
                     CompareTextInsensitive(TrimText(a_editor.draft.name),
                                            a_candidate) == 0;
            });
      });

  ConditionEditorState editor;
  editor.windowSlot = AllocateConditionEditorWindowSlot();
  editor.draft = conditions::BuildNewConditionTemplate(
      suggestedName, conditions::PickDistinctConditionColor(existingColors));
  editor.isNew = true;
  editor.focusOnNextDraw = true;
  ConditionEditors().push_back(std::move(editor));
}

void Menu::OpenConditionEditorDialog(const std::size_t a_index) {
  if (a_index >= ConditionDefinitions().size()) {
    return;
  }

  const auto &condition = ConditionDefinitions()[a_index];
  const auto existingIt =
      std::ranges::find(ConditionEditors(), condition.id,
                        &ConditionEditorState::sourceConditionId);
  if (existingIt != ConditionEditors().end()) {
    existingIt->focusOnNextDraw = true;
    existingIt->open = true;
    return;
  }

  ConditionEditorState editor;
  editor.windowSlot = AllocateConditionEditorWindowSlot();
  editor.sourceConditionId = condition.id;
  editor.draft = condition;
  editor.focusOnNextDraw = true;
  ConditionEditors().push_back(std::move(editor));
}

void Menu::OpenConditionEditorDialogById(const std::string_view a_conditionId) {
  const auto it =
      std::ranges::find(ConditionDefinitions(), a_conditionId, &ConditionDefinition::id);
  if (it == ConditionDefinitions().end()) {
    return;
  }
  OpenConditionEditorDialog(
      static_cast<std::size_t>(std::distance(ConditionDefinitions().begin(), it)));
}

bool Menu::SaveConditionEditor(ConditionEditorState &a_editor) {
  if (const auto validationError =
          ValidateConditionDraft(a_editor.draft, ConditionDefinitions());
      !validationError.empty()) {
    a_editor.error = validationError;
    return false;
  }

  for (std::size_t index = 0; index < a_editor.draft.clauses.size(); ++index) {
    auto &clause = a_editor.draft.clauses[index];
    clause.functionName = TrimText(clause.functionName);
    clause.arguments[0] = TrimText(clause.arguments[0]);
    clause.arguments[1] = TrimText(clause.arguments[1]);
    clause.comparand = TrimText(clause.comparand);

    std::optional<ConditionFunctionInfo> customFunctionInfo;
    const auto *functionInfo =
        ResolveConditionFunctionInfo(clause, ConditionDefinitions(), customFunctionInfo);
    if (!functionInfo) {
      a_editor.error = "Unknown or unsupported condition function in clause " +
                       std::to_string(index + 1) + ".";
      return false;
    }
    if (!clause.customConditionId.empty()) {
      if (clause.customConditionId == a_editor.draft.id &&
          !a_editor.draft.id.empty()) {
        a_editor.error = "A condition cannot reference itself in clause " +
                         std::to_string(index + 1) + ".";
        return false;
      }
      clause.arguments[0].clear();
      clause.arguments[1].clear();
      clause.functionName.clear();
    }

    for (std::uint16_t paramIndex = 0;
         paramIndex < functionInfo->parameterCount && paramIndex < 2;
         ++paramIndex) {
      if (!functionInfo->parameterOptional[paramIndex] &&
          clause.arguments[paramIndex].empty()) {
        a_editor.error = functionInfo->parameterLabels[paramIndex] +
                         " is required in clause " + std::to_string(index + 1) +
                         ".";
        return false;
      }

      const auto editorKind = GetEditorKindForParamType(
          ResolveEditorParamType(clause.functionName, paramIndex,
                                 functionInfo->parameterTypes[paramIndex]));
      if (editorKind == ConditionValueEditorKind::Unsupported) {
        a_editor.error = functionInfo->parameterLabels[paramIndex] +
                         " uses an unsupported parameter type in clause " +
                         std::to_string(index + 1) + ".";
        return false;
      }
      if (editorKind == ConditionValueEditorKind::Integer &&
          !clause.arguments[paramIndex].empty()) {
        try {
          clause.arguments[paramIndex] =
              std::to_string(std::stoi(clause.arguments[paramIndex]));
        } catch (const std::exception &) {
          a_editor.error = functionInfo->parameterLabels[paramIndex] +
                           " must be an integer in clause " +
                           std::to_string(index + 1) + ".";
          return false;
        }
      } else if (editorKind == ConditionValueEditorKind::Number &&
                 !clause.arguments[paramIndex].empty()) {
        try {
          clause.arguments[paramIndex] =
              ui::condition_editor::FormatNumberString(
                  std::stod(clause.arguments[paramIndex]));
        } catch (const std::exception &) {
          a_editor.error = functionInfo->parameterLabels[paramIndex] +
                           " must be numeric in clause " +
                           std::to_string(index + 1) + ".";
          return false;
        }
      }
    }

    if (functionInfo->returnsBooleanResult) {
      if (!IsBooleanComparator(clause.comparator)) {
        a_editor.error =
            "Boolean-return conditions only support == and != in clause " +
            std::to_string(index + 1) + ".";
        return false;
      }
      clause.comparand =
          ui::condition_editor::ParseBooleanComparand(clause.comparand, false)
              ? "1"
              : "0";
    } else {
      if (clause.comparand.empty()) {
        a_editor.error = "A comparison value is required in clause " +
                         std::to_string(index + 1) + ".";
        return false;
      }

      try {
        clause.comparand = ui::condition_editor::FormatNumberString(
            std::stod(clause.comparand));
      } catch (const std::exception &) {
        a_editor.error = "Comparison value must be numeric in clause " +
                         std::to_string(index + 1) + ".";
        return false;
      }
    }
  }

  a_editor.draft.name = TrimText(a_editor.draft.name);
  a_editor.draft.color.w = 1.0f;

  if (a_editor.isNew) {
    a_editor.draft.id = conditions::BuildConditionId(NextConditionId()++);
    ConditionDefinitions().push_back(a_editor.draft);
    a_editor.isNew = false;
    a_editor.sourceConditionId = a_editor.draft.id;
  } else {
    const auto it = std::ranges::find(ConditionDefinitions(), a_editor.sourceConditionId,
                                      &ConditionDefinition::id);
    if (it == ConditionDefinitions().end()) {
      a_editor.error = "Condition no longer exists.";
      return false;
    }
    *it = a_editor.draft;
  }

  sosr::conditions::RebuildConditionDependencyMetadata(ConditionDefinitions());
  sosr::conditions::InvalidateConditionMaterializationCachesFrom(
      ConditionDefinitions(), a_editor.draft.id);

  a_editor.error.clear();
  return true;
}
} // namespace sosr
