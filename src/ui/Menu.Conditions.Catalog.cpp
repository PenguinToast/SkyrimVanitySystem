#include "Menu.h"

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "ThemeConfig.h"
#include "imgui_internal.h"
#include "ui/components/PinnableTooltip.h"
#include "ui/conditions/Widgets.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <optional>

namespace sosr {
namespace {
using ConditionClause = ui::conditions::Clause;
using ConditionDefinition = ui::conditions::Definition;

void MoveConditionDefinitionToSlot(
    std::vector<ConditionDefinition> &a_conditions,
    const std::size_t a_sourceIndex, std::size_t a_slotIndex) {
  if (a_sourceIndex >= a_conditions.size() ||
      a_slotIndex > a_conditions.size()) {
    return;
  }

  auto condition = std::move(a_conditions[a_sourceIndex]);
  a_conditions.erase(a_conditions.begin() +
                     static_cast<std::ptrdiff_t>(a_sourceIndex));
  if (a_sourceIndex < a_slotIndex) {
    --a_slotIndex;
  }
  a_conditions.insert(a_conditions.begin() +
                          static_cast<std::ptrdiff_t>(a_slotIndex),
                      std::move(condition));
}

struct ConditionDeleteUsage {
  std::size_t referencingConditionCount{0};
  std::size_t appliedRowCount{0};

  [[nodiscard]] bool CanDelete() const {
    return referencingConditionCount == 0 && appliedRowCount == 0;
  }

  [[nodiscard]] std::string BuildTooltip() const {
    if (CanDelete()) {
      return {};
    }

    if (referencingConditionCount != 0 && appliedRowCount != 0) {
      return "Condition is in use by other conditions and applied to one or "
             "more workbench rows.";
    }
    if (referencingConditionCount != 0) {
      return "Condition is in use by other conditions.";
    }
    return "Condition is applied to one or more workbench rows.";
  }
};

constexpr char kIconTrash[] = "\xee\x86\x8c"; // ICON_LC_TRASH

std::string BuildActorTargetLabel(const RE::FormID a_formID) {
  auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_formID);
  if (!actor) {
    return "Actor " + armor::FormatFormID(a_formID);
  }

  std::string label;
  if (const auto *displayName = actor->GetDisplayFullName();
      displayName != nullptr && displayName[0] != '\0') {
    label = displayName;
  } else if (const auto *name = actor->GetName();
             name != nullptr && name[0] != '\0') {
    label = name;
  } else if (const auto *actorBase = actor->GetActorBase()) {
    label = armor::GetDisplayName(actorBase);
  }

  auto editorId = armor::GetEditorID(actor);
  if (editorId.empty()) {
    if (const auto *actorBase = actor->GetActorBase()) {
      editorId = armor::GetEditorID(actorBase);
    }
  }

  if (label.empty()) {
    if (!editorId.empty()) {
      return editorId;
    }
    return "Actor " + armor::FormatFormID(a_formID);
  }

  if (!editorId.empty() && editorId != label) {
    label.append(" (");
    label.append(editorId);
    label.push_back(')');
  }
  return label;
}

void DrawConditionTooltipHeader(std::string_view a_title,
                                const ui::conditions::Color &a_color) {
  const auto *theme = ThemeConfig::GetSingleton();
  const auto headerMin = ImGui::GetCursorScreenPos();
  const auto headerWidth = ImGui::GetContentRegionAvail().x;
  const auto headerHeight = ImGui::GetFontSize() * 2.4f;
  const auto headerMax =
      ImVec2(headerMin.x + headerWidth, headerMin.y + headerHeight);
  auto *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(headerMin, headerMax, theme->GetColorU32("BG"), 8.0f);
  drawList->AddRect(headerMin, headerMax,
                    ImGui::GetColorU32(ui::conditions::ToImGuiColor(a_color)),
                    8.0f);
  drawList->AddRectFilledMultiColor(
      headerMin, headerMax,
      ImGui::GetColorU32(ImVec4(a_color.x, a_color.y, a_color.z, 0.18f)),
      ImGui::GetColorU32(ImVec4(a_color.x, a_color.y, a_color.z, 0.18f)),
      theme->GetColorU32("NONE"), theme->GetColorU32("NONE"));

  const auto titleFontSize = ImGui::GetFontSize() * 1.15f;
  const auto titleSize =
      ImGui::CalcTextSize(a_title.data(), nullptr, false, headerWidth);
  drawList->AddText(
      ImGui::GetFont(), titleFontSize,
      ImVec2(headerMin.x + (headerWidth - titleSize.x) * 0.5f,
             headerMin.y + (headerHeight - titleFontSize) * 0.5f - 1.0f),
      theme->GetColorU32("TEXT"),
      a_title.data(), a_title.data() + a_title.size());
  ImGui::Dummy(ImVec2(headerWidth, headerHeight));

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Separator,
                        ImGui::GetColorU32(ui::conditions::ToImGuiColor(a_color)));
  ImGui::Separator();
  ImGui::PopStyleColor();
  ImGui::Spacing();
}

void DrawConditionTooltipSectionHeader(const char *a_title) {
  ImGui::TextDisabled("%s", a_title);
  ImGui::Spacing();
}

void DrawConditionWidgetTooltip(const ConditionDefinition &a_condition,
                                const bool a_hoveredSource,
                                std::vector<ConditionDefinition> &a_conditions) {
  const auto tooltipId = "condition:" + a_condition.id;
  if (!ui::components::ShouldDrawPinnableTooltip(tooltipId, a_hoveredSource)) {
    return;
  }

  const auto materialized =
      conditions::MaterializeConditionById(a_condition.id, a_conditions);
  const auto tooltipWidth = 460.0f;
  ImGui::SetNextWindowSize(
      ImVec2(tooltipWidth + ImGui::GetStyle().WindowPadding.x * 2.0f, 0.0f),
      ImGuiCond_Always);
  ui::components::DrawPinnableTooltip(tooltipId, a_hoveredSource, [&]() {
    DrawConditionTooltipHeader(a_condition.name, a_condition.color);

    if (!a_condition.description.empty()) {
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextDisabled("%s", a_condition.description.c_str());
      ImGui::PopTextWrapPos();
      ImGui::Spacing();
    }

    DrawConditionTooltipSectionHeader("Targeted Actor Refs");
    if (!materialized.has_value()) {
      ImGui::BulletText("Condition could not be materialized.");
    } else if (!materialized->refreshTargets.actorFormIDs.empty()) {
      for (const auto actorFormID : materialized->refreshTargets.actorFormIDs) {
        ImGui::Bullet();
        ImGui::SameLine();
        ImGui::PushTextWrapPos(0.0f);
        const auto label = BuildActorTargetLabel(actorFormID);
        ImGui::TextUnformatted(label.c_str());
        ImGui::PopTextWrapPos();
      }
    } else {
      ImGui::BulletText("No explicit actor refs targeted.");
    }
    if (materialized.has_value() &&
        materialized->refreshTargets.useNearbyFallback) {
      ImGui::BulletText("Nearby actors within 2048 units.");
    }

    ImGui::Spacing();
    DrawConditionTooltipSectionHeader("Expanded Form");
    if (!materialized.has_value() || materialized->displayCnf.empty()) {
      ImGui::TextDisabled("Unavailable");
      return;
    }

    if (ImGui::BeginTable("##condition-expanded-form", 2,
                          ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Expression", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("##operator", ImGuiTableColumnFlags_WidthFixed,
                              44.0f);

      for (std::size_t groupIndex = 0; groupIndex < materialized->displayCnf.size();
           ++groupIndex) {
        const auto &group = materialized->displayCnf[groupIndex];
        const bool isOrGroup = group.size() > 1;
        for (std::size_t literalIndex = 0; literalIndex < group.size();
             ++literalIndex) {
          ImGui::TableNextRow();
          if (isOrGroup) {
            ImGui::TableSetBgColor(
                ImGuiTableBgTarget_RowBg0,
                ImGui::GetColorU32(
                    ImVec4(a_condition.color.x, a_condition.color.y,
                           a_condition.color.z, 0.10f)));
          }

          ImGui::TableSetColumnIndex(0);
          ImGui::PushTextWrapPos(0.0f);
          ImGui::TextUnformatted(group[literalIndex].c_str());
          ImGui::PopTextWrapPos();

          ImGui::TableSetColumnIndex(1);
          const char *op = "";
          if (literalIndex + 1 < group.size()) {
            op = "OR";
          } else if (groupIndex + 1 < materialized->displayCnf.size()) {
            op = "AND";
          }
          if (op[0] != '\0') {
            ImGui::TextDisabled("%s", op);
          }
        }
      }
      ImGui::EndTable();
    }
  });
}
} // namespace

bool Menu::DrawConditionTab() {
  EnsureDefaultConditions();

  if (ImGui::Button("Add New")) {
    OpenNewConditionDialog();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("%zu condition%s", ConditionDefinitions().size(),
                      ConditionDefinitions().size() == 1 ? "" : "s");
  ImGui::Spacing();

  if (!ImGui::BeginTable("##conditions-table", 1,
                         ImGuiTableFlags_SizingStretchProp |
                             ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_BordersInnerV,
                         ImVec2(0.0f, 0.0f))) {
    return false;
  }

  ImGui::TableSetupColumn("Condition", ImGuiTableColumnFlags_WidthStretch);
  bool rowClicked = false;
  std::optional<std::size_t> pendingDeleteIndex;
  std::vector<ImRect> conditionRowRects;
  conditionRowRects.reserve(ConditionDefinitions().size());

  for (std::size_t index = 0; index < ConditionDefinitions().size(); ++index) {
    const auto &condition = ConditionDefinitions()[index];
    ConditionDeleteUsage deleteUsage;
    for (const auto &otherCondition : ConditionDefinitions()) {
      if (otherCondition.id == condition.id) {
        continue;
      }
      if (std::ranges::any_of(
              otherCondition.clauses, [&](const ConditionClause &a_clause) {
                return a_clause.customConditionId == condition.id;
              })) {
        ++deleteUsage.referencingConditionCount;
      }
    }
    for (const auto &row : workbench_.GetRows()) {
      if (row.conditionId && *row.conditionId == condition.id &&
          row.HasOverridesOrHideState()) {
        ++deleteUsage.appliedRowCount;
      }
    }
    const auto deleteTooltip = deleteUsage.BuildTooltip();
    const bool deleteEnabled = deleteUsage.CanDelete();

    const auto rowWrapWidth =
        (std::max)(ImGui::GetContentRegionAvail().x - 52.0f, 120.0f);
    const auto rowHeight = ui::condition_widgets::MeasureConditionRowHeight(
        condition, rowWrapWidth);

    ImGui::TableNextRow(0, rowHeight);
    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(static_cast<int>(index));

    const auto width = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("##condition-row", ImVec2(width, rowHeight));
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    const auto stripeWidth = 6.0f;
    const auto deletePaneWidth = 34.0f;
    const auto rounding = ImGui::GetStyle().FrameRounding;
    const ImVec2 deleteMin(max.x - deletePaneWidth, min.y);
    const ImVec2 deleteMax = max;
    const bool deleteHovered =
        ImGui::IsMouseHoveringRect(deleteMin, deleteMax, false);
    const auto hovered = ImGui::IsItemHovered() || deleteHovered;
    const bool rowBodyHovered =
        hovered && !deleteHovered &&
        ImGui::IsMouseHoveringRect(min, ImVec2(deleteMin.x, max.y), false);
    DrawConditionWidgetTooltip(condition, rowBodyHovered, ConditionDefinitions());
    rowClicked |= rowBodyHovered && ImGui::IsItemClicked(ImGuiMouseButton_Left);
    if (rowBodyHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      OpenConditionEditorDialog(index);
    }

    auto *drawList = ImGui::GetWindowDrawList();
    const auto bodyColor =
        hovered ? IM_COL32(42, 42, 44, 240) : IM_COL32(34, 34, 36, 225);
    drawList->AddRectFilled(min, max, bodyColor, rounding);
    drawList->AddRect(
        min, max,
        ImGui::GetColorU32(ImVec4(condition.color.x, condition.color.y,
                                  condition.color.z, 0.75f)),
        rounding);
    drawList->AddRectFilled(
        min, ImVec2(min.x + stripeWidth, max.y),
        ImGui::GetColorU32(ui::conditions::ToImGuiColor(condition.color)),
        rounding,
        ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomLeft);

    auto *theme = ThemeConfig::GetSingleton();
    const ImU32 deleteFillColor =
        deleteEnabled
            ? (deleteHovered ? theme->GetColorU32("DECLINE", 0.95f)
                             : theme->GetColorU32("DECLINE", 0.78f))
            : theme->GetColorU32("DECLINE", deleteHovered ? 0.35f : 0.24f);
    drawList->AddRectFilled(deleteMin, deleteMax, deleteFillColor, rounding,
                            ImDrawFlags_RoundCornersTopRight |
                                ImDrawFlags_RoundCornersBottomRight);
    drawList->AddLine(ImVec2(deleteMin.x, deleteMin.y),
                      ImVec2(deleteMin.x, deleteMax.y),
                      IM_COL32(255, 255, 255, 18), 1.0f);
    const auto deleteIconSize = ImGui::CalcTextSize(kIconTrash);
    drawList->AddText(
        ImVec2(deleteMin.x + ((deletePaneWidth - deleteIconSize.x) * 0.5f),
               deleteMin.y +
                   (((deleteMax.y - deleteMin.y) - deleteIconSize.y) * 0.5f)),
        ImGui::GetColorU32(deleteEnabled ? ImGuiCol_Text
                                         : ImGuiCol_TextDisabled),
        kIconTrash);

    const auto contentMin = ImVec2(min.x + stripeWidth + 10.0f,
                                   min.y + ImGui::GetStyle().FramePadding.y);
    const auto textColor = ImGui::GetColorU32(
        ImVec4(condition.color.x, condition.color.y, condition.color.z, 1.0f));
    const auto clipRect =
        ImVec4(contentMin.x, min.y, deleteMin.x - 8.0f, max.y);
    drawList->PushClipRect(ImVec2(clipRect.x, clipRect.y),
                           ImVec2(clipRect.z, clipRect.w), true);
    drawList->AddText(contentMin, textColor, condition.name.c_str());
    if (!condition.description.empty()) {
      drawList->AddText(
          ImGui::GetFont(), ImGui::GetFontSize(),
          ImVec2(contentMin.x, contentMin.y + ImGui::GetTextLineHeight() +
                                   ImGui::GetStyle().ItemSpacing.y),
          ImGui::GetColorU32(ImVec4(0.70f, 0.72f, 0.75f, 1.0f)),
          condition.description.c_str(), nullptr, rowWrapWidth);
    }
    drawList->PopClipRect();

    if (deleteHovered) {
      if (deleteEnabled) {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
          pendingDeleteIndex = index;
        }
      } else if (!deleteTooltip.empty()) {
        ui::condition_widgets::DrawHoverDescription(
            "conditions:delete-disabled:" + condition.id, deleteTooltip, 0.2f);
      }
    }

    if (!deleteHovered && ImGui::BeginDragDropSource()) {
      DraggedConditionPayload payload{};
      std::snprintf(payload.conditionId.data(), payload.conditionId.size(),
                    "%s", condition.id.c_str());
      ImGui::SetDragDropPayload("SVS_CONDITION", &payload, sizeof(payload));
      ImGui::TextUnformatted(condition.name.c_str());
      if (!condition.description.empty()) {
        ImGui::TextUnformatted(condition.description.c_str());
      }
      ImGui::EndDragDropSource();
    }

    conditionRowRects.emplace_back(min, max);

    ImGui::PopID();
  }

  ImGui::EndTable();

  std::optional<std::size_t> hoveredInsertionSlot;
  ImRect hoveredInsertionRect;
  float insertionLineY = -1.0f;
  float insertionLineXMin = 0.0f;
  float insertionLineXMax = 0.0f;

  if (ImGui::IsDragDropActive() && !conditionRowRects.empty()) {
    const float fallbackBandHalfHeight =
        (std::max)(6.0f, ImGui::GetStyle().ItemSpacing.y * 0.5f);
    const auto xMin = conditionRowRects.front().Min.x;
    const auto xMax = conditionRowRects.front().Max.x;
    std::vector<float> insertionLineYs;
    insertionLineYs.reserve(conditionRowRects.size() + 1);
    insertionLineYs.push_back(conditionRowRects.front().Min.y -
                              fallbackBandHalfHeight);
    for (std::size_t index = 0; index + 1 < conditionRowRects.size(); ++index) {
      const auto &currentRect = conditionRowRects[index];
      const auto &nextRect = conditionRowRects[index + 1];
      insertionLineYs.push_back((currentRect.Max.y + nextRect.Min.y) * 0.5f);
    }
    insertionLineYs.push_back(conditionRowRects.back().Max.y +
                              fallbackBandHalfHeight);

    for (std::size_t slotIndex = 0; slotIndex < insertionLineYs.size();
         ++slotIndex) {
      const float lineY = insertionLineYs[slotIndex];
      const float bandMinY =
          slotIndex == 0 ? (lineY - fallbackBandHalfHeight)
                         : ((insertionLineYs[slotIndex - 1] + lineY) * 0.5f);
      const float bandMaxY =
          slotIndex + 1 == insertionLineYs.size()
              ? (lineY + fallbackBandHalfHeight)
              : ((lineY + insertionLineYs[slotIndex + 1]) * 0.5f);
      const ImRect slotRect(ImVec2(xMin, bandMinY), ImVec2(xMax, bandMaxY));
      if (!ImGui::IsMouseHoveringRect(slotRect.Min, slotRect.Max, false)) {
        continue;
      }

      hoveredInsertionSlot = slotIndex;
      hoveredInsertionRect = slotRect;
      insertionLineY = lineY;
      insertionLineXMin = xMin;
      insertionLineXMax = xMax;
      break;
    }
  }

  if (insertionLineY >= 0.0f && insertionLineXMax > insertionLineXMin) {
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(insertionLineXMin, insertionLineY),
        ImVec2(insertionLineXMax, insertionLineY),
        ThemeConfig::GetSingleton()->GetColorU32("PRIMARY"), 2.0f);
  }

  if (hoveredInsertionSlot &&
      ImGui::BeginDragDropTargetCustom(
          hoveredInsertionRect,
          ImGui::GetID(("##condition-reorder-slot-" +
                        std::to_string(*hoveredInsertionSlot))
                           .c_str()))) {
    if (const auto *payload = ImGui::AcceptDragDropPayload(
            "SVS_CONDITION", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
        payload && payload->DataSize == sizeof(DraggedConditionPayload)) {
      DraggedConditionPayload dragPayload{};
      std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));
      if (const auto it = std::ranges::find(
              ConditionDefinitions(), std::string_view(dragPayload.conditionId.data()),
              &ConditionDefinition::id);
          it != ConditionDefinitions().end()) {
        const auto sourceIndex =
            static_cast<std::size_t>(std::distance(ConditionDefinitions().begin(), it));
        MoveConditionDefinitionToSlot(ConditionDefinitions(), sourceIndex,
                                      *hoveredInsertionSlot);
      }
    }
    ImGui::EndDragDropTarget();
  }

  if (pendingDeleteIndex && *pendingDeleteIndex < ConditionDefinitions().size()) {
    const auto deletedConditionId = ConditionDefinitions()[*pendingDeleteIndex].id;
    workbench_.DeleteRowsByConditionId(deletedConditionId, true);
    ConditionDefinitions().erase(ConditionDefinitions().begin() +
                      static_cast<std::ptrdiff_t>(*pendingDeleteIndex));
    sosr::conditions::RebuildConditionDependencyMetadata(ConditionDefinitions());
    sosr::conditions::InvalidateConditionMaterializationCaches(ConditionDefinitions());
    for (auto &editor : ConditionEditors()) {
      if (editor.sourceConditionId == deletedConditionId) {
        editor.error.clear();
        editor.open = false;
      }
    }
  }

  return rowClicked;
}
} // namespace sosr
