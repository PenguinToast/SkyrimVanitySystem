#include "Menu.h"

#include "ArmorUtils.h"
#include "StringUtils.h"
#include "ui/components/EquipmentWidget.h"
#include "workbench/ItemFactory.h"

#include <algorithm>

namespace {
enum class SlotColumn : ImGuiID { Slot = 1, Occupied };
} // namespace

namespace sosr {
bool Menu::DrawSlotTab() {
  const auto &browser = CatalogBrowserState();
  struct SlotCatalogRow {
    workbench::EquipmentWidgetItem slotItem;
    std::vector<workbench::EquipmentWidgetItem> occupantItems;
    std::string occupantSortText;
  };

  std::vector<SlotCatalogRow> rows;
  rows.reserve(32);

  for (const auto slotMask : armor::GetAllArmorSlotMasks()) {
    SlotCatalogRow row{};
    if (!workbench::BuildSlotItem(slotMask, row.slotItem)) {
      continue;
    }

    for (const auto &workbenchRow : workbench_.GetRows()) {
      if (!workbenchRow.isEquipped || workbenchRow.equipped.formID == 0) {
        continue;
      }

      const auto *armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(
          workbenchRow.equipped.formID);
      if (!armor || (armor::GetArmorAddonSlotMask(armor) & slotMask) == 0) {
        continue;
      }

      row.occupantItems.push_back(workbenchRow.equipped);
    }

    std::ranges::sort(row.occupantItems, [](const auto &a_left,
                                            const auto &a_right) {
      return sosr::strings::CompareTextInsensitive(a_left.name, a_right.name) <
             0;
    });
    for (const auto &item : row.occupantItems) {
      if (!row.occupantSortText.empty()) {
        row.occupantSortText.append(", ");
      }
      row.occupantSortText.append(item.name);
    }
    if (row.occupantSortText.empty()) {
      row.occupantSortText = "Empty";
    }

    if (!browser.showAllSlots && row.occupantItems.empty()) {
      continue;
    }

    rows.push_back(std::move(row));
  }

  ImGui::Text("Results: %zu", rows.size());
  bool rowClicked = false;

  if (ImGui::BeginTable("##slot-table", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, 0.0f))) {
    ImGui::TableSetupColumn("Slot",
                            ImGuiTableColumnFlags_WidthStretch |
                                ImGuiTableColumnFlags_DefaultSort,
                            0.68f, static_cast<ImGuiID>(SlotColumn::Slot));
    ImGui::TableSetupColumn("Occupied", ImGuiTableColumnFlags_WidthStretch,
                            1.00f, static_cast<ImGuiID>(SlotColumn::Occupied));
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    if (auto *sortSpecs = ImGui::TableGetSortSpecs();
        sortSpecs && sortSpecs->SpecsCount > 0) {
      const auto &spec = sortSpecs->Specs[0];
      const auto ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
      std::ranges::sort(rows, [&](const auto &a_left, const auto &a_right) {
        int compare = 0;
        switch (static_cast<SlotColumn>(spec.ColumnUserID)) {
        case SlotColumn::Occupied:
          compare = sosr::strings::CompareTextInsensitive(
              a_left.occupantSortText, a_right.occupantSortText);
          break;
        case SlotColumn::Slot:
        default:
          compare = sosr::strings::CompareTextInsensitive(
              a_left.slotItem.name, a_right.slotItem.name);
          break;
        }

        if (compare == 0) {
          compare = sosr::strings::CompareTextInsensitive(
              a_left.slotItem.name, a_right.slotItem.name);
        }

        return ascending ? compare < 0 : compare > 0;
      });
      sortSpecs->SpecsDirty = false;
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) {
      for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd;
           ++rowIndex) {
        auto &row = rows[static_cast<std::size_t>(rowIndex)];
        const auto widgetHeight = 18.0f + (ImGui::GetTextLineHeight() * 2.0f);
        const auto occupantHeight =
            row.occupantItems.empty()
                ? widgetHeight
                : (static_cast<float>(row.occupantItems.size()) *
                   widgetHeight) +
                      ((row.occupantItems.size() > 1)
                           ? static_cast<float>(row.occupantItems.size() - 1) *
                                 5.0f
                           : 0.0f);
        const auto rowHeight = (std::max)(widgetHeight, occupantHeight);

        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
        ImGui::TableSetColumnIndex(0);
        const auto rowContentPos = ImGui::GetCursorScreenPos();
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
        const bool selected = browser.selectedKey == row.slotItem.key;
        ImGui::Selectable(
            ("##slot-row-hit-" + std::to_string(rowIndex)).c_str(), selected,
            ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap |
                ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(0.0f, rowHeight));
        const bool rowHovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor(3);
        if (ImGui::BeginPopupContextItem()) {
          if (ImGui::MenuItem("Add to Workbench")) {
            workbench_.AddSlotRow(row.slotItem.slotMask,
                                  ResolveNewWorkbenchRowConditionId(),
                                  ResolveWorkbenchPreviewActor());
          }
          ImGui::EndPopup();
        }
        ImGui::SetCursorScreenPos(rowContentPos);

        if (selected) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("PRIMARY", 0.40f));
        } else if (rowHovered) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("TABLE_HOVER", 0.12f));
        }

        const auto widgetResult =
            DrawCatalogDragWidget(row.slotItem, DragSourceKind::SlotCatalog);

        const bool doubleClicked =
            (rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) ||
            widgetResult.doubleClicked;
        const bool releasedOnRow =
            ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
            (rowHovered || widgetResult.hovered);
        if (doubleClicked) {
          rowClicked = true;
          workbench_.AddSlotRow(row.slotItem.slotMask,
                                ResolveNewWorkbenchRowConditionId(),
                                ResolveWorkbenchPreviewActor());
        } else if (releasedOnRow) {
          rowClicked = true;
          if (selected) {
            ClearCatalogSelection();
          } else {
            CatalogBrowserState().selectedKey = row.slotItem.key;
            workbench_.ClearPreview();
          }
        }

        ImGui::TableSetColumnIndex(1);
        if (!row.occupantItems.empty()) {
          const auto oldItemSpacing = ImGui::GetStyle().ItemSpacing;
          ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                              ImVec2(oldItemSpacing.x, 5.0f));
          for (std::size_t occupantIndex = 0;
               occupantIndex < row.occupantItems.size(); ++occupantIndex) {
            const auto widgetId =
                row.slotItem.key + ":occupant:" + std::to_string(occupantIndex);
            [[maybe_unused]] const auto occupantWidget =
                ui::components::DrawEquipmentWidget(
                    widgetId.c_str(), row.occupantItems[occupantIndex]);
          }
          ImGui::PopStyleVar();
        } else {
          ImGui::TextDisabled("Empty");
        }
      }
    }

    ImGui::EndTable();
  }

  return rowClicked;
}
} // namespace sosr
