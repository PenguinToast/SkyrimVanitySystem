#include "Menu.h"

#include "ConditionMaterializer.h"
#include "conditions/Defaults.h"
#include "imgui_internal.h"
#include "ui/ConditionParamOptionCache.h"
#include "ui/conditions/EditorSupport.h"
#include "ui/conditions/Widgets.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>

namespace sosr {
namespace {
using ui::condition_editor::BuildConditionFunctionItems;
using ui::condition_editor::ValidateConditionDraft;
using ui::condition_widgets::DrawHoverDescription;

void CopyTextToBuffer(const std::string &a_text, char *a_buffer,
                      const std::size_t a_bufferSize) {
  if (a_bufferSize == 0) {
    return;
  }

  std::snprintf(a_buffer, a_bufferSize, "%s", a_text.c_str());
}
} // namespace

void Menu::DrawConditionEditorDialog() {
  ui::conditions::ConditionParamOptionCache::Get().Continue(16.0);

  for (auto &editor : ConditionEditors()) {
    if (!editor.open) {
      continue;
    }

    std::string title = editor.isNew ? "New Condition" : editor.draft.name;
    if (title.empty()) {
      title = "Untitled";
    }
    title.insert(0, "Condition Editor [");
    title.push_back(']');
    title.append("###");
    title.append("condition-editor-");
    title.append(std::to_string((std::max)(editor.windowSlot, 1)));

    ImGui::SetNextWindowSize(ImVec2(1180.0f, 560.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(900.0f, 480.0f),
                                        ImVec2(FLT_MAX, FLT_MAX));
    if (editor.focusOnNextDraw) {
      ImGui::SetNextWindowFocus();
      editor.focusOnNextDraw = false;
    }

    if (!ImGui::Begin(title.c_str(), &editor.open,
                      ImGuiWindowFlags_NoCollapse)) {
      ImGui::End();
      continue;
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
      FocusedConditionEditorWindowSlot() = editor.windowSlot;
    }

    const auto footerHeight =
        ImGui::GetFrameHeightWithSpacing() * 2.0f +
        ImGui::GetStyle().ItemSpacing.y * 3.0f +
        ((!editor.error.empty())
             ? (ImGui::GetTextLineHeightWithSpacing() * 2.0f)
             : 0.0f);

    if (ImGui::BeginChild("##condition-editor-body",
                          ImVec2(0.0f, -footerHeight), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse)) {
      char nameBuffer[256];
      CopyTextToBuffer(editor.draft.name, nameBuffer, sizeof(nameBuffer));
      ImGui::SetNextItemWidth(
          (std::min)(420.0f, ImGui::GetContentRegionAvail().x));
      if (ImGui::InputTextWithHint("##name", "Name", nameBuffer,
                                   sizeof(nameBuffer))) {
        editor.draft.name = nameBuffer;
      }
      DrawHoverDescription("conditions:editor:name",
                           "Friendly name shown in the Conditions catalog.");

      char descriptionBuffer[512];
      CopyTextToBuffer(editor.draft.description, descriptionBuffer,
                       sizeof(descriptionBuffer));
      ImGui::SetNextItemWidth(
          (std::min)(520.0f, ImGui::GetContentRegionAvail().x));
      if (ImGui::InputTextWithHint("##description", "Description",
                                   descriptionBuffer,
                                   sizeof(descriptionBuffer))) {
        editor.draft.description = descriptionBuffer;
      }
      DrawHoverDescription(
          "conditions:editor:description",
          "Optional description shown on the condition widget.");

      auto color = editor.draft.color;
      ImGui::SetNextItemWidth(
          (std::min)(260.0f, ImGui::GetContentRegionAvail().x));
      if (ImGui::ColorEdit3("##color", &color.x,
                            ImGuiColorEditFlags_DisplayRGB)) {
        color.w = 1.0f;
        editor.draft.color = color;
      }
      DrawHoverDescription(
          "conditions:editor:color",
          "Associated display color used by the Conditions catalog row.");

      ImGui::Spacing();
      ImGui::TextUnformatted("Clauses");
      ImGui::Separator();

      const auto conditionFunctionItems =
          BuildConditionFunctionItems(ConditionDefinitions(),
                                      editor.sourceConditionId);
      const auto clausePaneHeight =
          (std::max)(220.0f, ImGui::GetContentRegionAvail().y);
      const auto &style = ImGui::GetStyle();
      const auto editButtonWidth =
          ImGui::CalcTextSize("Edit").x + style.FramePadding.x * 2.0f;
      const auto deleteButtonWidth =
          ImGui::CalcTextSize("\xee\x86\x8c").x + style.FramePadding.x * 2.0f;
      const auto actionsColumnWidth = editButtonWidth + style.ItemSpacing.x +
                                      deleteButtonWidth +
                                      style.CellPadding.x * 2.0f;
      if (ImGui::BeginChild("##condition-clauses",
                            ImVec2(0.0f, clausePaneHeight),
                            ImGuiChildFlags_Borders)) {
        DrawConditionEditorClauseTable(editor, conditionFunctionItems,
                                       editButtonWidth, deleteButtonWidth,
                                       actionsColumnWidth);
      }
      ImGui::EndChild();
    }
    ImGui::EndChild();

    if (ImGui::Button("Add Clause")) {
      editor.draft.clauses.push_back(conditions::BuildDefaultPlayerClause());
    }
    DrawHoverDescription("conditions:editor:add-clause",
                         "Append another clause to this condition.");

    const auto draftValidationError =
        ValidateConditionDraft(editor.draft, ConditionDefinitions());
    if (draftValidationError.empty()) {
      editor.error.clear();
    }
    const bool showInlineError =
        !editor.error.empty() && draftValidationError.empty();

    if (showInlineError) {
      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 205, 205, 255));
      ImGui::TextWrapped("%s", editor.error.c_str());
      ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!draftValidationError.empty());
    if (ImGui::Button("Save")) {
      if (SaveConditionEditor(editor)) {
        editor.open = false;
      }
    }
    ImGui::EndDisabled();
    if (!draftValidationError.empty()) {
      DrawHoverDescription("conditions:editor:save-disabled:" +
                               std::to_string((std::max)(editor.windowSlot, 1)),
                           draftValidationError, 0.2f,
                           ImGuiHoveredFlags_AllowWhenDisabled);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      editor.error.clear();
      editor.open = false;
    }

    ImGui::End();
  }

  ConditionEditors().erase(
      std::remove_if(
          ConditionEditors().begin(), ConditionEditors().end(),
          [](const ConditionEditorState &a_editor) { return !a_editor.open; }),
      ConditionEditors().end());
  if (ConditionEditors().empty()) {
    FocusedConditionEditorWindowSlot() = 0;
  } else if (FocusedConditionEditorWindowSlot() > 0 &&
             std::ranges::find(ConditionEditors(),
                               FocusedConditionEditorWindowSlot(),
                               &ConditionEditorState::windowSlot) ==
                 ConditionEditors().end()) {
    FocusedConditionEditorWindowSlot() = 0;
  }
}
} // namespace sosr
