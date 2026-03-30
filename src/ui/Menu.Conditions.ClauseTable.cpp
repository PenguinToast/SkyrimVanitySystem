#include "Menu.h"

#include "ThemeConfig.h"
#include "conditions/Validation.h"
#include "imgui_internal.h"
#include "ui/components/EditableCombo.h"
#include "ui/components/PinnableTooltip.h"
#include "ui/conditions/EditorSupport.h"
#include "ui/conditions/Widgets.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <span>

namespace sosr {
namespace {
using ConditionClause = ui::conditions::Clause;
using ConditionComparator = ui::conditions::Comparator;
using ConditionConnective = ui::conditions::Connective;
using ConditionFunctionInfo = ui::condition_editor::FunctionInfo;
using ConditionValueEditorKind = ui::condition_editor::ValueEditorKind;
using ui::condition_editor::DrawConditionParamEditor;
using ui::condition_editor::DrawNumericClauseValueEditor;
using ui::condition_editor::IsBooleanComparator;
using ui::condition_editor::ParseBooleanComparand;
using ui::condition_editor::ResolveConditionFunctionInfo;
using ui::condition_editor::ResolveEditorParamType;
using ui::condition_widgets::DrawConditionColorSwatch;
using ui::condition_widgets::DrawHoverDescription;

constexpr char kIconTrash[] = "\xee\x86\x8c";        // ICON_LC_TRASH
constexpr char kIconGripVertical[] = "\xee\x83\xae"; // ICON_LC_GRIP_VERTICAL
constexpr char kConditionClausePayloadType[] = "SVS_CONDITION_CLAUSE";
constexpr float kOrGroupIndicatorWidth = 6.0f;
constexpr float kOrGroupIndicatorInsetX = 4.0f;
constexpr float kOrGroupBoundaryGap = 4.0f;
constexpr float kOrGroupIndicatorRounding = 4.0f;
constexpr float kOrGroupTooltipWrapWidth = 360.0f;
constexpr std::string_view kOrGroupTooltip =
    "In Skyrim, consecutive OR clauses are evaluated as a grouped block "
    "before surrounding AND clauses. Example: A AND B OR C AND D becomes "
    "A AND (B OR C) AND D.";

struct OrGroupSpan {
  std::size_t startIndex{0};
  std::size_t endIndex{0};
};

void DrawClauseDragHandle(const char *a_id, const ImVec2 a_size) {
  ImGui::InvisibleButton(a_id, a_size);

  const auto min = ImGui::GetItemRectMin();
  const auto max = ImGui::GetItemRectMax();
  const auto color = ImGui::GetColorU32(
      ImGui::IsItemHovered() ? ImGuiCol_Text : ImGuiCol_TextDisabled);
  auto *drawList = ImGui::GetWindowDrawList();
  const auto iconSize = ImGui::CalcTextSize(kIconGripVertical);
  drawList->AddText(ImVec2(min.x + ((max.x - min.x) - iconSize.x) * 0.5f,
                           min.y + ((max.y - min.y) - iconSize.y) * 0.5f),
                    color, kIconGripVertical);
}

void MoveConditionClause(std::vector<ConditionClause> &a_clauses,
                         const std::size_t a_sourceIndex,
                         const std::size_t a_targetIndex,
                         const bool a_insertAfter) {
  if (a_sourceIndex >= a_clauses.size() || a_targetIndex >= a_clauses.size()) {
    return;
  }

  auto destinationIndex = a_targetIndex;
  if (a_insertAfter) {
    ++destinationIndex;
  }
  if (a_sourceIndex < destinationIndex) {
    --destinationIndex;
  }
  if (a_sourceIndex == destinationIndex) {
    return;
  }

  auto clause = std::move(a_clauses[a_sourceIndex]);
  a_clauses.erase(a_clauses.begin() +
                  static_cast<std::ptrdiff_t>(a_sourceIndex));
  a_clauses.insert(a_clauses.begin() +
                       static_cast<std::ptrdiff_t>(destinationIndex),
                   std::move(clause));
}

const char *ComparatorLabel(const ConditionComparator a_comparator) {
  switch (a_comparator) {
  case ConditionComparator::Equal:
    return "==";
  case ConditionComparator::NotEqual:
    return "!=";
  case ConditionComparator::Greater:
    return ">";
  case ConditionComparator::GreaterOrEqual:
    return ">=";
  case ConditionComparator::Less:
    return "<";
  case ConditionComparator::LessOrEqual:
    return "<=";
  }

  return "==";
}

void DrawClauseHeaderCell(const char *a_id, const char *a_label,
                          const std::string_view a_tooltip) {
  ImGui::TextDisabled("%s", a_label);
  DrawHoverDescription(a_id, a_tooltip);
}

void DrawOrGroupIndicator(const std::string &a_id, const ImRect &a_groupRect) {
  constexpr ImDrawFlags drawFlags =
      ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersTopRight |
      ImDrawFlags_RoundCornersBottomLeft | ImDrawFlags_RoundCornersBottomRight;

  const auto indicatorMin = ImVec2(a_groupRect.Min.x + kOrGroupIndicatorInsetX,
                                   a_groupRect.Min.y + kOrGroupBoundaryGap);
  const auto indicatorMax =
      ImVec2(indicatorMin.x + kOrGroupIndicatorWidth,
             a_groupRect.Max.y - kOrGroupBoundaryGap);

  const auto *theme = ThemeConfig::GetSingleton();
  const bool hovered =
      ImGui::IsMouseHoveringRect(indicatorMin, indicatorMax, false);
  const auto color =
      theme->GetColorU32("PRIMARY", hovered ? 0.95f : 0.78f);
  ImGui::GetWindowDrawList()->AddRectFilled(indicatorMin, indicatorMax, color,
                                            kOrGroupIndicatorRounding, drawFlags);

  ui::components::DrawPinnableTooltip(a_id, hovered, [&]() {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kOrGroupTooltipWrapWidth);
    ImGui::TextUnformatted(kOrGroupTooltip.data(),
                           kOrGroupTooltip.data() + kOrGroupTooltip.size());
    ImGui::PopTextWrapPos();
  });
}
} // namespace

void Menu::DrawConditionEditorClauseTable(
    ConditionEditorState &a_editor,
    const std::vector<std::string> &a_conditionFunctionNames,
    const float a_editButtonWidth, const float a_deleteButtonWidth,
    const float a_actionsColumnWidth) {
  constexpr ImGuiTableFlags clauseTableFlags =
      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV |
      ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollX |
      ImGuiTableFlags_Resizable;
  if (!ImGui::BeginTable("##condition-clause-table", 8, clauseTableFlags,
                         ImVec2(0.0f, 0.0f))) {
    return;
  }

  const auto tableOuterRect = ImGui::GetCurrentTable()->OuterRect;
  ImGui::TableSetupColumn(
      "", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize,
      18.0f);
  ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_WidthFixed, 240.0f);
  ImGui::TableSetupColumn("Arg 1", ImGuiTableColumnFlags_WidthFixed, 170.0f);
  ImGui::TableSetupColumn("Arg 2", ImGuiTableColumnFlags_WidthFixed, 170.0f);
  ImGui::TableSetupColumn("Comparator", ImGuiTableColumnFlags_WidthFixed,
                          120.0f);
  ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 130.0f);
  ImGui::TableSetupColumn("Join", ImGuiTableColumnFlags_WidthFixed, 90.0f);
  ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,
                          a_actionsColumnWidth);

  ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
  ImGui::TableSetColumnIndex(0);
  ImGui::TextDisabled(" ");
  DrawHoverDescription("conditions:help:move", "Drag to reorder clauses.");
  ImGui::TableSetColumnIndex(1);
  DrawClauseHeaderCell("conditions:help:function", "Function",
                       "Condition function to evaluate for this clause.");
  ImGui::TableSetColumnIndex(2);
  DrawClauseHeaderCell("conditions:help:arg1", "Arg 1",
                       "First function argument. Input type follows the "
                       "selected function.");
  ImGui::TableSetColumnIndex(3);
  DrawClauseHeaderCell("conditions:help:arg2", "Arg 2",
                       "Second function argument when the selected function "
                       "needs one.");
  ImGui::TableSetColumnIndex(4);
  DrawClauseHeaderCell("conditions:help:comparator", "Comparator",
                       "How the function result is compared.");
  ImGui::TableSetColumnIndex(5);
  DrawClauseHeaderCell(
      "conditions:help:value", "Value",
      "Value compared against the function result. Boolean-returning "
      "functions use a true/false checkbox.");
  ImGui::TableSetColumnIndex(6);
  DrawClauseHeaderCell("conditions:help:join", "Join",
                       "How this clause combines with the next one.");
  ImGui::TableSetColumnIndex(7);
  DrawClauseHeaderCell("conditions:help:actions", "Actions",
                       "Remove this clause.");

  const auto *activePayload = ImGui::GetDragDropPayload();
  const bool reorderPreviewActive =
      activePayload && activePayload->Data != nullptr &&
      activePayload->IsDataType(kConditionClausePayloadType) &&
      activePayload->DataSize == sizeof(int);
  float insertionLineY = -1.0f;
  float insertionLineX1 = -1.0f;
  float insertionLineX2 = -1.0f;
  int hoveredClauseReorderIndex = -1;
  bool hoveredClauseInsertAfter = false;
  std::optional<std::size_t> acceptedSourceClauseIndex;
  bool acceptedInsertAfter = false;
  std::vector<std::optional<ImRect>> joinCellRects(a_editor.draft.clauses.size());
  std::vector<OrGroupSpan> orGroupSpans;
  for (std::size_t index = 0; index < a_editor.draft.clauses.size();) {
    if (a_editor.draft.clauses[index].connectiveToNext !=
        ConditionConnective::Or) {
      ++index;
      continue;
    }

    std::size_t endIndex = index + 1;
    while (endIndex < a_editor.draft.clauses.size() &&
           a_editor.draft.clauses[endIndex - 1].connectiveToNext ==
               ConditionConnective::Or) {
      ++endIndex;
    }

    orGroupSpans.push_back(
        OrGroupSpan{.startIndex = index, .endIndex = endIndex - 1});
    index = endIndex;
  }

  for (std::size_t index = 0; index < a_editor.draft.clauses.size();) {
    ImGui::PushID(static_cast<int>(index));
    auto &clause = a_editor.draft.clauses[index];
    const bool previousClauseOr =
        index > 0 &&
        a_editor.draft.clauses[index - 1].connectiveToNext == ConditionConnective::Or;
    const bool nextClauseOr = clause.connectiveToNext == ConditionConnective::Or;
    const bool inOrGroup = previousClauseOr || nextClauseOr;
    std::optional<ConditionFunctionInfo> customFunctionInfo;
    const auto *functionInfo =
        ResolveConditionFunctionInfo(clause, ConditionDefinitions(), customFunctionInfo);
    auto selectedFunctionName =
        functionInfo ? functionInfo->name : clause.functionName;

    ImGui::TableNextRow();
    if (inOrGroup) {
      ImGui::TableSetBgColor(
          ImGuiTableBgTarget_RowBg0,
          ThemeConfig::GetSingleton()->GetColorU32("PRIMARY", 0.05f));
    }

    ImGui::TableSetColumnIndex(0);
    DrawClauseDragHandle(
        "##drag-handle",
        ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight()));
    DrawHoverDescription("conditions:editor:drag:" + std::to_string(index),
                         "Drag this handle to reorder the clause.");
    if (ImGui::BeginDragDropSource()) {
      const int payloadIndex = static_cast<int>(index);
      ImGui::SetDragDropPayload(kConditionClausePayloadType, &payloadIndex,
                                sizeof(payloadIndex));
      ImGui::Text("Move clause %zu", index + 1);
      ImGui::EndDragDropSource();
    }

    ImGui::TableSetColumnIndex(1);
    const auto *customCondition =
        clause.customConditionId.empty()
            ? nullptr
            : conditions::FindDefinitionById(ConditionDefinitions(),
                                             clause.customConditionId);
    float functionWidth = ImGui::GetContentRegionAvail().x;
    if (customCondition != nullptr) {
      const float swatchSize = ImGui::GetFrameHeight() - 2.0f;
      const float spacing = ImGui::GetStyle().ItemSpacing.x;
      DrawConditionColorSwatch(
          "##custom-condition-color", customCondition->color,
          "Referenced custom condition color: " + customCondition->name);
      ImGui::SameLine(0.0f, spacing);
      functionWidth = (std::max)(0.0f, functionWidth - swatchSize - spacing);
    }
    if (ui::components::DrawSearchableDropdown(
            "##function", "Condition function", selectedFunctionName,
            a_conditionFunctionNames, functionWidth)) {
      clause.arguments[0].clear();
      clause.arguments[1].clear();
      if (const auto *selectedCustomCondition =
              conditions::FindDefinitionByName(ConditionDefinitions(),
                                               selectedFunctionName,
                                               a_editor.sourceConditionId);
          selectedCustomCondition != nullptr) {
        clause.customConditionId = selectedCustomCondition->id;
        clause.functionName.clear();
      } else {
        clause.customConditionId.clear();
        clause.functionName = selectedFunctionName;
      }
      customFunctionInfo.reset();
      functionInfo =
          ResolveConditionFunctionInfo(clause, ConditionDefinitions(), customFunctionInfo);
      if (functionInfo && functionInfo->returnsBooleanResult &&
          !IsBooleanComparator(clause.comparator)) {
        clause.comparator = ConditionComparator::Equal;
      }
    }
    DrawHoverDescription(
        "conditions:editor:function:" + std::to_string(index),
        "Select a condition function. Only functions whose parameters SVS can "
        "model with typed inputs are shown.");

    const auto argumentCount =
        functionInfo ? functionInfo->parameterCount : std::uint16_t{2};
    for (std::uint16_t paramIndex = 0; paramIndex < 2; ++paramIndex) {
      ImGui::TableSetColumnIndex(2 + paramIndex);
      ImGui::BeginDisabled(paramIndex >= argumentCount);
      if (functionInfo && paramIndex < argumentCount) {
        DrawConditionParamEditor(
            paramIndex == 0 ? "##arg1" : "##arg2", clause.arguments[paramIndex],
            ResolveEditorParamType(clause.functionName, paramIndex,
                                   functionInfo->parameterTypes[paramIndex]),
            ImGui::GetContentRegionAvail().x);
      } else {
        ImGui::BeginDisabled();
        char buffer[16] = "";
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputText(paramIndex == 0 ? "##arg1" : "##arg2", buffer,
                         sizeof(buffer), ImGuiInputTextFlags_ReadOnly);
        ImGui::EndDisabled();
      }
      ImGui::EndDisabled();

      std::string tooltip = functionInfo && paramIndex < argumentCount
                                ? functionInfo->parameterLabels[paramIndex]
                                : "Unused argument for the current function.";
      if (functionInfo && paramIndex < argumentCount &&
          functionInfo->parameterOptional[paramIndex]) {
        tooltip.append(" Optional.");
      } else if (functionInfo && paramIndex < argumentCount) {
        tooltip.append(" Required.");
      }
      DrawHoverDescription("conditions:editor:arg:" + std::to_string(index) +
                               ":" + std::to_string(paramIndex),
                           tooltip);
    }
    for (std::uint16_t paramIndex = argumentCount; paramIndex < 2;
         ++paramIndex) {
      clause.arguments[paramIndex].clear();
    }

    ImGui::TableSetColumnIndex(4);
    constexpr std::array kBooleanComparators = {ConditionComparator::Equal,
                                                ConditionComparator::NotEqual};
    constexpr std::array kAllComparators = {
        ConditionComparator::Equal,   ConditionComparator::NotEqual,
        ConditionComparator::Greater, ConditionComparator::GreaterOrEqual,
        ConditionComparator::Less,    ConditionComparator::LessOrEqual};
    const std::span<const ConditionComparator> availableComparators =
        functionInfo && functionInfo->returnsBooleanResult
            ? std::span<const ConditionComparator>(kBooleanComparators.begin(),
                                                   kBooleanComparators.end())
            : std::span<const ConditionComparator>(kAllComparators.begin(),
                                                   kAllComparators.end());
    if (functionInfo && functionInfo->returnsBooleanResult &&
        !IsBooleanComparator(clause.comparator)) {
      clause.comparator = ConditionComparator::Equal;
    }
    const auto comparatorIt =
        std::ranges::find(availableComparators, clause.comparator);
    const int comparatorIndex =
        comparatorIt != availableComparators.end()
            ? static_cast<int>(
                  std::distance(availableComparators.begin(), comparatorIt))
            : 0;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo(
            "##comparator",
            ComparatorLabel(availableComparators[comparatorIndex]))) {
      for (int optionIndex = 0;
           optionIndex < static_cast<int>(availableComparators.size());
           ++optionIndex) {
        const bool selected = comparatorIndex == optionIndex;
        if (ImGui::Selectable(
                ComparatorLabel(availableComparators[optionIndex]), selected)) {
          clause.comparator = availableComparators[optionIndex];
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    DrawHoverDescription("conditions:editor:comparator:" +
                             std::to_string(index),
                         "Comparison operator applied to the function result.");

    ImGui::TableSetColumnIndex(5);
    if (functionInfo && functionInfo->returnsBooleanResult) {
      bool boolComparand = ParseBooleanComparand(clause.comparand, true);
      if (ImGui::Checkbox("##comparand", &boolComparand)) {
        clause.comparand = boolComparand ? "1" : "0";
      } else if (clause.comparand.empty() ||
                 clause.comparand != "0" && clause.comparand != "1") {
        clause.comparand = boolComparand ? "1" : "0";
      }
    } else {
      DrawNumericClauseValueEditor("##comparand", clause.comparand,
                                   ConditionValueEditorKind::Number,
                                   ImGui::GetContentRegionAvail().x);
    }
    DrawHoverDescription(
        "conditions:editor:value:" + std::to_string(index),
        functionInfo && functionInfo->returnsBooleanResult
            ? "Checked stores 1 (true). Unchecked stores 0 (false)."
            : "Numeric value compared against the function result.");

    ImGui::TableSetColumnIndex(6);
    joinCellRects[index] =
        ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), 6);
    const bool hasNextClause = index + 1 < a_editor.draft.clauses.size();
    ImGui::BeginDisabled(!hasNextClause);
    int connectiveIndex =
        clause.connectiveToNext == ConditionConnective::Or ? 1 : 0;
    constexpr std::array connectiveLabels = {"AND", "OR"};
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo("##join", connectiveLabels[connectiveIndex])) {
      for (int optionIndex = 0; optionIndex < 2; ++optionIndex) {
        const bool selected = connectiveIndex == optionIndex;
        if (ImGui::Selectable(connectiveLabels[optionIndex], selected)) {
          connectiveIndex = optionIndex;
          clause.connectiveToNext = connectiveIndex == 1
                                        ? ConditionConnective::Or
                                        : ConditionConnective::And;
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    DrawHoverDescription("conditions:editor:join:" + std::to_string(index),
                         "How this clause combines with the next clause.");

    ImGui::TableSetColumnIndex(7);
    const bool hasCustomEditTarget = !clause.customConditionId.empty();
    if (hasCustomEditTarget) {
      if (ImGui::Button("Edit", ImVec2(a_editButtonWidth, 0.0f))) {
        OpenConditionEditorDialogById(clause.customConditionId);
      }
      DrawHoverDescription("conditions:editor:edit-custom:" +
                               std::to_string(index),
                           "Open the referenced custom condition.");
      if (a_editor.draft.clauses.size() > 1) {
        ImGui::SameLine();
      }
    }
    if (a_editor.draft.clauses.size() > 1) {
      auto *theme = ThemeConfig::GetSingleton();
      ImGui::PushStyleColor(ImGuiCol_Button, theme->GetColor("DECLINE", 0.78f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            theme->GetColor("DECLINE", 0.95f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            theme->GetColor("DECLINE", 0.90f));
      if (ImGui::Button(kIconTrash, ImVec2(a_deleteButtonWidth, 0.0f))) {
        a_editor.draft.clauses.erase(a_editor.draft.clauses.begin() +
                                     static_cast<std::ptrdiff_t>(index));
        ImGui::PopStyleColor(3);
        ImGui::PopID();
        continue;
      }
      ImGui::PopStyleColor(3);
      DrawHoverDescription("conditions:editor:remove:" + std::to_string(index),
                           "Remove this clause from the condition.");
    }

    if (const auto *table = ImGui::GetCurrentTable(); table != nullptr) {
      const auto firstCellRect = ImGui::TableGetCellBgRect(table, 0);
      const auto lastCellRect = ImGui::TableGetCellBgRect(table, 7);
      const auto rowDropRect =
          ImRect(ImVec2(tableOuterRect.Min.x, firstCellRect.Min.y),
                 ImVec2(tableOuterRect.Max.x, lastCellRect.Max.y));
      const bool insertAfter = ImGui::GetIO().MousePos.y >
                               ((rowDropRect.Min.y + rowDropRect.Max.y) * 0.5f);
      if (reorderPreviewActive &&
          ImGui::IsMouseHoveringRect(rowDropRect.Min, rowDropRect.Max, false)) {
        hoveredClauseReorderIndex = static_cast<int>(index);
        hoveredClauseInsertAfter = insertAfter;
        insertionLineY = insertAfter ? rowDropRect.Max.y : rowDropRect.Min.y;
        insertionLineX1 = table->OuterRect.Min.x + 2.0f;
        insertionLineX2 = table->OuterRect.Max.x - 2.0f;
      }
    }

    ImGui::PopID();
    ++index;
  }

  if (reorderPreviewActive && insertionLineY >= 0.0f &&
      insertionLineX2 > insertionLineX1) {
    auto *drawList = ImGui::GetWindowDrawList();
    if (const auto *table = ImGui::GetCurrentTable(); table != nullptr) {
      drawList->PushClipRect(table->OuterRect.Min, table->OuterRect.Max, false);
      drawList->AddLine(ImVec2(insertionLineX1, insertionLineY),
                        ImVec2(insertionLineX2, insertionLineY),
                        ThemeConfig::GetSingleton()->GetColorU32("PRIMARY"),
                        2.0f);
      drawList->PopClipRect();
    }
  }

  for (const auto &group : orGroupSpans) {
    if (group.startIndex >= joinCellRects.size() ||
        group.endIndex >= joinCellRects.size() ||
        !joinCellRects[group.startIndex].has_value() ||
        !joinCellRects[group.endIndex].has_value()) {
      continue;
    }

    const auto &startRect = *joinCellRects[group.startIndex];
    const auto &endRect = *joinCellRects[group.endIndex];
    DrawOrGroupIndicator(
        "conditions:editor:or-group:" + std::to_string(group.startIndex),
        ImRect(ImVec2(startRect.Min.x, startRect.Min.y),
               ImVec2(startRect.Max.x, endRect.Max.y)));
  }

  ImGui::EndTable();
  if (hoveredClauseReorderIndex >= 0 &&
      ImGui::BeginDragDropTargetCustom(
          tableOuterRect, ImGui::GetID("##condition-clause-reorder-target"))) {
    if (const auto *payload = ImGui::AcceptDragDropPayload(
            kConditionClausePayloadType,
            ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
        payload && payload->Data != nullptr &&
        payload->DataSize == sizeof(int)) {
      acceptedSourceClauseIndex =
          static_cast<std::size_t>(*static_cast<const int *>(payload->Data));
      acceptedInsertAfter = hoveredClauseInsertAfter;
    }
    ImGui::EndDragDropTarget();
  }
  if (acceptedSourceClauseIndex && hoveredClauseReorderIndex >= 0) {
    MoveConditionClause(a_editor.draft.clauses, *acceptedSourceClauseIndex,
                        static_cast<std::size_t>(hoveredClauseReorderIndex),
                        acceptedInsertAfter);
  }
}
} // namespace sosr
