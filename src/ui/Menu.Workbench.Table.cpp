#include "Menu.h"

#include "conditions/Validation.h"
#include "imgui_internal.h"
#include "ui/TableReorder.h"
#include "ui/WorkbenchConflicts.h"
#include "ui/components/EquipmentWidget.h"
#include "ui/workbench/Common.h"
#include "ui/workbench/Tooltips.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <optional>
#include <unordered_map>

namespace sosr {
void Menu::DrawWorkbenchTable(const std::vector<int> &a_visibleRowIndices) {
  EnsureWorkbenchDerivedState();
  const auto &rows = workbench_.GetRows();
  const auto &rowConditionStates = workbenchDerived_.rowConditionStates;
  const auto &rowConflicts = workbenchDerived_.conflictState.rowConflicts;
  const auto &overrideConflicts =
      workbenchDerived_.conflictState.overrideConflicts;

  const auto tableHeight = (std::max)(0.0f, ImGui::GetContentRegionAvail().y);
  if (ImGui::BeginChild("##variant-workbench-scroll", ImVec2(0.0f, tableHeight),
                        ImGuiChildFlags_None)) {
    if (ImGui::BeginTable("##variant-workbench", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(0.0f, 0.0f))) {
      ImGui::TableSetupColumn("Equipped", ImGuiTableColumnFlags_WidthStretch,
                              0.80f);
      ImGui::TableSetupColumn("Overrides", ImGuiTableColumnFlags_WidthStretch,
                              1.05f);
      ImGui::TableSetupColumn("Hide",
                              ImGuiTableColumnFlags_WidthFixed |
                                  ImGuiTableColumnFlags_NoResize,
                              72.0f);
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableHeadersRow();

      const auto *activePayload = ImGui::GetDragDropPayload();
      const bool reorderPreviewActive =
          activePayload && activePayload->Data != nullptr &&
          activePayload->IsDataType(ui::workbench::kVariantItemPayloadType) &&
          activePayload->DataSize == sizeof(DraggedEquipmentPayload) &&
          ([](const DraggedEquipmentPayload *a_payload) {
            return a_payload->sourceKind ==
                       static_cast<std::uint32_t>(DragSourceKind::Row) ||
                   a_payload->sourceKind ==
                       static_cast<std::uint32_t>(DragSourceKind::Catalog) ||
                   a_payload->sourceKind ==
                       static_cast<std::uint32_t>(DragSourceKind::Override) ||
                   a_payload->sourceKind ==
                       static_cast<std::uint32_t>(DragSourceKind::SlotCatalog);
          })(static_cast<const DraggedEquipmentPayload *>(activePayload->Data));
      std::optional<DraggedEquipmentPayload> acceptedRowReorderPayload;
      int acceptedRowReorderIndex = -1;
      bool acceptedRowInsertAfter = false;
      std::optional<std::pair<int, std::optional<std::string>>>
          pendingConditionAssignment;
      bool suppressRowReorderPreview = false;
      std::unordered_map<std::string, ImRect> widgetRects;
      widgetRects.reserve(a_visibleRowIndices.size() * 3);
      std::vector<std::string> hoveredConflictWidgetIds;
      std::vector<ImRect> reorderRowRects;
      reorderRowRects.reserve(a_visibleRowIndices.size());

      for (int visibleRowIndex = 0;
           visibleRowIndex < static_cast<int>(a_visibleRowIndices.size());
           ++visibleRowIndex) {
        const int rowIndex =
            a_visibleRowIndices[static_cast<std::size_t>(visibleRowIndex)];
        const auto &row = rows[static_cast<std::size_t>(rowIndex)];
        const auto &conditionState =
            rowConditionStates[static_cast<std::size_t>(rowIndex)];
        const auto overrideCount = row.overrides.size();
        const auto widgetHeight = 18.0f + (ImGui::GetTextLineHeight() * 2.0f);
        const auto dropZoneHeight =
            overrideCount > 0
                ? (static_cast<float>(overrideCount) * widgetHeight) +
                      ((overrideCount > 1)
                           ? static_cast<float>(overrideCount - 1) *
                                 ui::workbench::kWorkbenchOverrideGapY
                           : 0.0f)
                : widgetHeight;
        const auto contentHeight = (std::max)(widgetHeight, dropZoneHeight);
        const auto rowHeight = contentHeight + ui::workbench::kWorkbenchRowGapY;
        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
        ImGui::TableSetBgColor(
            ImGuiTableBgTarget_RowBg0,
            ThemeConfig::GetSingleton()->GetColorU32(
                (rowIndex % 2) == 0 ? "TABLE_BG" : "TABLE_BG_ALT"));
        if (row.isEquipped) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg1,
              ThemeConfig::GetSingleton()->GetColorU32("SECONDARY", 0.16f));
        }

        ImGui::TableSetColumnIndex(0);
        const auto *table = ImGui::GetCurrentTable();
        if (table) {
          const ImRect leftCellRect = ImGui::TableGetCellBgRect(table, 0);
          const auto leftCellContentHeight =
              (leftCellRect.Max.y - leftCellRect.Min.y) -
              (ImGui::GetStyle().CellPadding.y * 2.0f);
          const auto leftCellContentOffsetY =
              (std::max)(0.0f, (leftCellContentHeight - contentHeight) * 0.5f);
          ImGui::SetCursorScreenPos(
              ImVec2(leftCellRect.Min.x + ImGui::GetStyle().CellPadding.x,
                     leftCellRect.Min.y + ImGui::GetStyle().CellPadding.y +
                         leftCellContentOffsetY));
        }

        const auto equippedWidget = ui::components::DrawEquipmentWidget(
            row.key.c_str(), row.equipped,
            {.showDeleteButton = true,
             .disabledAppearance = conditionState.disabled,
             .accentColor = conditionState.color,
             .conflictStyle =
                 rowConflicts.contains(row.key)
                     ? ui::components::EquipmentWidgetConflictStyle::Warning
                     : ui::components::EquipmentWidgetConflictStyle::None,
             .drawTooltipExtras =
                 rowConflicts.contains(row.key)
                     ? std::function<void()>{[&, rowIndex]() {
                         const auto &tooltipRow =
                             rows[static_cast<std::size_t>(rowIndex)];
                         const auto &tooltipState =
                             rowConditionStates[static_cast<std::size_t>(
                                 rowIndex)];
                         ImGui::Separator();
                         ImGui::TextDisabled("Condition");
                         if (tooltipState.color.has_value()) {
                           ImGui::SameLine(0.0f, 8.0f);
                           ImGui::ColorButton(
                               "##row-condition-tooltip-color",
                               *tooltipState.color,
                               ImGuiColorEditFlags_NoTooltip |
                                   ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoBorder,
                               ImVec2(ImGui::GetTextLineHeight(),
                                      ImGui::GetTextLineHeight()));
                           ImGui::SameLine(0.0f, 8.0f);
                         }
                         ImGui::TextUnformatted(tooltipState.name.c_str());
                         if (!tooltipState.description.empty()) {
                           ImGui::PushTextWrapPos(0.0f);
                           ImGui::TextDisabled(
                               "%s", tooltipState.description.c_str());
                           ImGui::PopTextWrapPos();
                         }

                         const auto conflictIt =
                             rowConflicts.find(tooltipRow.key);
                         if (conflictIt == rowConflicts.end()) {
                           return;
                         }

                         ui::workbench::DrawConflictTooltipSection(
                             "Warning: Variant selection conflict.",
                             [&](const ThemeConfig *a_theme) {
                               for (const auto &description :
                                    conflictIt->second.targetDescriptions) {
                                 ImGui::Bullet();
                                 ImGui::SameLine(
                                     0.0f,
                                     ImGui::GetStyle().ItemInnerSpacing.x);
                                 ImGui::BeginGroup();
                                 ui::workbench::DrawWrappedColoredTextRuns(
                                     {{description.primaryName,
                                       a_theme->GetColorU32("TEXT")},
                                      {description.secondaryName.empty() ? ""
                                                                         : " (",
                                       a_theme->GetColorU32("TEXT_DISABLED")},
                                      {description.secondaryName,
                                       a_theme->GetColorU32("TEXT_DISABLED")},
                                      {description.secondaryName.empty() ? ""
                                                                         : ")",
                                       a_theme->GetColorU32("TEXT_DISABLED")},
                                      {description.targetLabel.empty() ? ""
                                                                       : "  [",
                                       a_theme->GetColorU32("TEXT")},
                                      {description.targetLabel,
                                       a_theme->GetColorU32("TEXT_HEADER",
                                                            0.92f)},
                                      {description.targetLabel.empty() ? ""
                                                                       : "]",
                                       a_theme->GetColorU32("TEXT")}});
                                 ImGui::EndGroup();
                               }
                             });
                       }}
                     : std::function<void()>{[&, rowIndex]() {
                         const auto &tooltipState =
                             rowConditionStates[static_cast<std::size_t>(
                                 rowIndex)];
                         ImGui::Separator();
                         ImGui::TextDisabled("Condition");
                         if (tooltipState.color.has_value()) {
                           ImGui::SameLine(0.0f, 8.0f);
                           ImGui::ColorButton(
                               "##row-condition-tooltip-color",
                               *tooltipState.color,
                               ImGuiColorEditFlags_NoTooltip |
                                   ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoBorder,
                               ImVec2(ImGui::GetTextLineHeight(),
                                      ImGui::GetTextLineHeight()));
                           ImGui::SameLine(0.0f, 8.0f);
                         }
                         ImGui::TextUnformatted(tooltipState.name.c_str());
                         if (!tooltipState.description.empty()) {
                           ImGui::PushTextWrapPos(0.0f);
                           ImGui::TextDisabled(
                               "%s", tooltipState.description.c_str());
                           ImGui::PopTextWrapPos();
                         }
                       }},
             .drawContextMenuEntries = [&, rowIndex]() {
               const auto &contextRow =
                   rows[static_cast<std::size_t>(rowIndex)];
               const auto &contextConditionState =
                   rowConditionStates[static_cast<std::size_t>(rowIndex)];
               if (ImGui::BeginMenu(contextConditionState.disabled
                                        ? "Enable Condition"
                                        : "Set Condition")) {
                 for (const auto &condition : ConditionDefinitions()) {
                   if (!IsWorkbenchSelectableCondition(condition)) {
                     continue;
                   }
                   const bool isCurrent =
                       contextRow.conditionId.has_value() &&
                       *contextRow.conditionId == condition.id;
                   if (ImGui::MenuItem(condition.name.c_str(), nullptr,
                                       isCurrent)) {
                     pendingConditionAssignment = std::pair{
                         rowIndex, std::optional<std::string>(condition.id)};
                   }
                 }
                 ImGui::EndMenu();
               }
             }});
        if (!equippedWidget.deleteHovered && ImGui::BeginDragDropSource()) {
          DraggedEquipmentPayload payload{};
          payload.sourceKind = static_cast<std::uint32_t>(DragSourceKind::Row);
          payload.rowIndex = rowIndex;
          payload.itemIndex = -1;
          payload.formID = row.equipped.formID;
          payload.slotMask = row.equipped.slotMask;
          ImGui::SetDragDropPayload(ui::workbench::kVariantItemPayloadType,
                                    &payload, sizeof(payload));
          ImGui::TextUnformatted(row.equipped.name.c_str());
          ImGui::Text("%s", row.equipped.slotText.c_str());
          ImGui::EndDragDropSource();
        }
        if (equippedWidget.deleteClicked) {
          workbench_.DeleteRow(rowIndex);
          break;
        }
        widgetRects.insert_or_assign(
            row.key, ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
        if (ImGui::BeginDragDropTargetCustom(
                ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()),
                ImGui::GetID(
                    ("##row-condition-" + std::to_string(rowIndex)).c_str()))) {
          if (const auto *payload = ImGui::AcceptDragDropPayload(
                  ui::workbench::kConditionPayloadType);
              payload && payload->Data != nullptr &&
              payload->DataSize == sizeof(DraggedConditionPayload)) {
            DraggedConditionPayload dragPayload{};
            std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));
            const std::string conditionId(dragPayload.conditionId.data());
            if (!conditionId.empty()) {
              const auto *condition = conditions::FindDefinitionById(
                  ConditionDefinitions(), conditionId);
              if (!condition || !IsWorkbenchSelectableCondition(*condition)) {
                ImGui::EndDragDropTarget();
                continue;
              }
              pendingConditionAssignment =
                  std::pair{rowIndex, std::optional<std::string>(conditionId)};
            }
          }
          ImGui::EndDragDropTarget();
        }
        if (rowConflicts.contains(row.key) && equippedWidget.hovered) {
          hoveredConflictWidgetIds = rowConflicts.at(row.key).targetWidgetIds;
        }
        if (table) {
          const ImRect leftCellRect = ImGui::TableGetCellBgRect(table, 0);
          reorderRowRects.push_back(leftCellRect);
        }

        ImGui::TableSetColumnIndex(1);
        const auto *currentTable = ImGui::GetCurrentTable();
        const auto overrideCellRect =
            currentTable ? ImGui::TableGetCellBgRect(currentTable, 1)
                         : ImRect(ImGui::GetCursorScreenPos(),
                                  ImGui::GetCursorScreenPos());
        const auto cellPadding = ImGui::GetStyle().CellPadding;
        const auto overrideDropMin =
            ImVec2(overrideCellRect.Min.x + cellPadding.x,
                   overrideCellRect.Min.y + cellPadding.y);
        const auto overrideDropMaxX = overrideCellRect.Max.x - cellPadding.x;
        const auto overrideCellContentHeight =
            (overrideCellRect.Max.y - overrideCellRect.Min.y) -
            (cellPadding.y * 2.0f);
        const auto overrideCellContentOffsetY = (std::max)(
            0.0f, (overrideCellContentHeight - contentHeight) * 0.5f);
        ImGui::SetCursorScreenPos(ImVec2(overrideCellRect.Min.x + cellPadding.x,
                                         overrideCellRect.Min.y +
                                             cellPadding.y +
                                             overrideCellContentOffsetY));

        if (overrideCount == 0) {
          const auto wrappedWidth = (overrideCellRect.Max.x - cellPadding.x) -
                                    (overrideCellRect.Min.x + cellPadding.x);
          const auto textSize = ImGui::CalcTextSize(
              "Drop equipment overrides here.", nullptr, false, wrappedWidth);
          ImGui::SetCursorScreenPos(
              ImVec2(overrideCellRect.Min.x + cellPadding.x,
                     overrideCellRect.Min.y + cellPadding.y +
                         overrideCellContentOffsetY +
                         ((dropZoneHeight - textSize.y) * 0.5f)));
          ImGui::PushTextWrapPos(overrideCellRect.Max.x - cellPadding.x);
          ImGui::TextWrapped("Drop equipment overrides here.");
          ImGui::PopTextWrapPos();
        } else {
          const auto oldItemSpacing = ImGui::GetStyle().ItemSpacing;
          ImGui::PushStyleVar(
              ImGuiStyleVar_ItemSpacing,
              ImVec2(oldItemSpacing.x, ui::workbench::kWorkbenchOverrideGapY));
          for (int overrideIndex = 0;
               overrideIndex < static_cast<int>(overrideCount);
               ++overrideIndex) {
            const auto widgetId = "override:" + std::to_string(rowIndex) + ":" +
                                  std::to_string(overrideIndex);
            const auto overrideWidget = ui::components::DrawEquipmentWidget(
                widgetId.c_str(),
                row.overrides[static_cast<std::size_t>(overrideIndex)],
                {.showDeleteButton = true,
                 .conflictStyle =
                     overrideConflicts.contains(widgetId)
                         ? ui::components::EquipmentWidgetConflictStyle::Error
                         : ui::components::EquipmentWidgetConflictStyle::None,
                 .drawTooltipExtras =
                     overrideConflicts.contains(widgetId)
                         ? std::function<void()>{[&, widgetId]() {
                             ui::workbench::DrawConflictTooltipSection(
                                 "Warning: Conflicting visuals.",
                                 [&](const ThemeConfig *a_theme) {
                                   for (const auto &description :
                                        overrideConflicts.at(widgetId)
                                            .targetDescriptions) {
                                     ui::workbench::DrawConflictEntry(
                                         description, a_theme);
                                   }
                                 });
                           }}
                         : std::function<void()>{}});
            if (!overrideWidget.deleteHovered && ImGui::BeginDragDropSource()) {
              DraggedEquipmentPayload payload{};
              payload.sourceKind =
                  static_cast<std::uint32_t>(DragSourceKind::Override);
              payload.rowIndex = rowIndex;
              payload.itemIndex = overrideIndex;
              payload.formID =
                  row.overrides[static_cast<std::size_t>(overrideIndex)].formID;
              ImGui::SetDragDropPayload(ui::workbench::kVariantItemPayloadType,
                                        &payload, sizeof(payload));
              ImGui::TextUnformatted(
                  row.overrides[static_cast<std::size_t>(overrideIndex)]
                      .name.c_str());
              ImGui::Text("%s",
                          row.overrides[static_cast<std::size_t>(overrideIndex)]
                              .slotText.c_str());
              ImGui::EndDragDropSource();
            }
            if (overrideWidget.deleteClicked) {
              workbench_.DeleteOverride(rowIndex, overrideIndex);
              break;
            }
            widgetRects.insert_or_assign(
                widgetId,
                ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
            if (overrideConflicts.contains(widgetId) &&
                overrideWidget.hovered) {
              hoveredConflictWidgetIds =
                  overrideConflicts.at(widgetId).targetWidgetIds;
            }
          }
          ImGui::PopStyleVar();
        }

        ImGui::TableSetColumnIndex(2);
        bool hideEquipped = row.hideEquipped;
        if (const auto *hideTable = ImGui::GetCurrentTable();
            hideTable != nullptr) {
          const auto hideCellRect = ImGui::TableGetCellBgRect(hideTable, 2);
          const auto checkboxSize =
              ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());
          const auto checkboxPos = ImVec2(
              hideCellRect.Min.x +
                  ((hideCellRect.Max.x - hideCellRect.Min.x) - checkboxSize.x) *
                      0.5f,
              hideCellRect.Min.y +
                  ((hideCellRect.Max.y - hideCellRect.Min.y) - checkboxSize.y) *
                      0.5f);
          ImGui::SetCursorScreenPos(checkboxPos);
        }
        if (ImGui::Checkbox(
                ("##hide-equipped-" + std::to_string(rowIndex)).c_str(),
                &hideEquipped)) {
          workbench_.SetHideEquipped(rowIndex, hideEquipped);
        }

        const ImRect overrideDropRect(
            overrideDropMin,
            ImVec2(overrideDropMaxX, overrideCellRect.Max.y - cellPadding.y));
        ImGui::TableSetColumnIndex(1);
        if (const bool overrideTargetActive = ImGui::BeginDragDropTargetCustom(
                overrideDropRect, ImGui::GetID(("##override-cell-target-" +
                                                std::to_string(rowIndex))
                                                   .c_str()));
            overrideTargetActive) {
          suppressRowReorderPreview = true;
          AcceptOverridePayload(rowIndex);
          ImGui::EndDragDropTarget();
        }
      }

      const auto *reorderTable = ImGui::GetCurrentTable();
      const auto reorderTableRect =
          reorderTable != nullptr ? reorderTable->OuterRect : ImRect{};
      const auto reorderPreview =
          reorderPreviewActive && !suppressRowReorderPreview &&
                  reorderTable != nullptr
              ? ui::table_reorder::ComputeLinearReorderPreview(
                    reorderRowRects, reorderTable->OuterRect.Min.x + 2.0f,
                    reorderTable->OuterRect.Max.x - 2.0f)
              : ui::table_reorder::LinearReorderPreview{};
      if (reorderPreviewActive && !suppressRowReorderPreview &&
          reorderTable != nullptr) {
        ui::table_reorder::DrawLinearReorderInsertionLine(
            reorderPreview,
            ImGui::GetColorU32(
                ThemeConfig::GetSingleton()->GetActive("PRIMARY")),
            3.0f, &reorderTable->OuterRect);
      }

      if (!hoveredConflictWidgetIds.empty()) {
        auto *drawList = ImGui::GetWindowDrawList();
        if (const auto *table = ImGui::GetCurrentTable(); table != nullptr) {
          drawList->PushClipRect(table->OuterRect.Min, table->OuterRect.Max,
                                 false);
        }
        for (const auto &targetWidgetId : hoveredConflictWidgetIds) {
          if (const auto rectIt = widgetRects.find(targetWidgetId);
              rectIt != widgetRects.end()) {
            drawList->AddRectFilled(
                rectIt->second.Min, rectIt->second.Max,
                ThemeConfig::GetSingleton()->GetColorU32("WARN", 0.18f), 8.0f);
            drawList->AddRect(rectIt->second.Min, rectIt->second.Max,
                              ThemeConfig::GetSingleton()->GetColorU32("WARN"),
                              8.0f, 0, 3.0f);
          }
        }
        if (ImGui::GetCurrentTable() != nullptr) {
          drawList->PopClipRect();
        }
      }

      ImGui::EndTable();

      if (reorderPreview.HasHoveredSlot() &&
          ImGui::BeginDragDropTargetCustom(
              reorderTableRect,
              ImGui::GetID("##variant-workbench-reorder-target"))) {
        if (const auto *payload = ImGui::AcceptDragDropPayload(
                ui::workbench::kVariantItemPayloadType,
                ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
            payload && payload->Data != nullptr &&
            payload->DataSize == sizeof(DraggedEquipmentPayload)) {
          DraggedEquipmentPayload dragPayload{};
          std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));
          if (dragPayload.sourceKind ==
                  static_cast<std::uint32_t>(DragSourceKind::Row) ||
              dragPayload.sourceKind ==
                  static_cast<std::uint32_t>(DragSourceKind::Catalog) ||
              dragPayload.sourceKind ==
                  static_cast<std::uint32_t>(DragSourceKind::Override) ||
              dragPayload.sourceKind ==
                  static_cast<std::uint32_t>(DragSourceKind::SlotCatalog)) {
            acceptedRowReorderPayload = dragPayload;
            if (*reorderPreview.hoveredSlotIndex >=
                a_visibleRowIndices.size()) {
              acceptedRowReorderIndex =
                  a_visibleRowIndices.empty() ? -1 : a_visibleRowIndices.back();
              acceptedRowInsertAfter = true;
            } else {
              acceptedRowReorderIndex =
                  a_visibleRowIndices[*reorderPreview.hoveredSlotIndex];
              acceptedRowInsertAfter = false;
            }
          }
        }
        ImGui::EndDragDropTarget();
      }

      if (acceptedRowReorderPayload && acceptedRowReorderIndex >= 0) {
        ApplyWorkbenchRowDrop(*acceptedRowReorderPayload,
                              acceptedRowReorderIndex, acceptedRowInsertAfter);
      }
      if (pendingConditionAssignment.has_value()) {
        workbench_.SetConditionId(pendingConditionAssignment->first,
                                  pendingConditionAssignment->second);
      }
    }
    ImGui::EndChild();
  }
}
} // namespace sosr
