#include "Menu.h"

#include "workbench/ItemFactory.h"

namespace {
enum class GearColumn : ImGuiID { Name = 1, Plugin };
}

namespace sosr {
bool Menu::DrawGearTab() {
  const auto &rows = GetFilteredGearRows();
  ImGui::Text("Results: %zu", rows.size());
  ImGui::SameLine();
  ImGui::Text("| Equipped rows: %zu", workbench_.GetRowCount());
  return DrawGearCatalogTable();
}

bool Menu::DrawGearCatalogTable() {
  const auto &browser = CatalogBrowserState();
  bool rowClicked = false;
  if (ImGui::BeginTable("##gear-table", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, 0.0f))) {
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            static_cast<ImGuiID>(GearColumn::Name));
    ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_None, 0.0f,
                            static_cast<ImGuiID>(GearColumn::Plugin));
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    const auto &rows = GetSortedGearRows(ImGui::TableGetSortSpecs());

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) {
      for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd;
           ++rowIndex) {
        const auto &entry = *rows[static_cast<std::size_t>(rowIndex)];
        const auto favorite =
            IsFavorite(ui::catalog::BrowserTab::Gear, entry.id);
        workbench::EquipmentWidgetItem item{};
        if (!workbench::BuildCatalogItem(entry.formID, item)) {
          item.formID = entry.formID;
          item.key = "catalog:" + entry.id;
          item.name = entry.name;
        }
        item.name = BuildFavoriteLabel(item.name, favorite);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const auto rowContentPos = ImGui::GetCursorScreenPos();
        const auto rowHeight = 18.0f + (ImGui::GetTextLineHeight() * 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
        const bool selected = browser.selectedKey == entry.id &&
                              (!browser.previewSelected ||
                               workbench_.IsPreviewingSelection(entry.id));
        ImGui::Selectable(
            ("##catalog-row-hit-" + std::to_string(rowIndex)).c_str(), selected,
            ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap |
                ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(0.0f, rowHeight));
        const bool rowHovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor(3);
        if (ImGui::BeginPopupContextItem()) {
          const auto favoriteLabel =
              favorite ? "Remove from Favorites" : "Add to Favorites";
          if (ImGui::MenuItem(favoriteLabel)) {
            SetFavorite(ui::catalog::BrowserTab::Gear, entry.id, !favorite);
          }
          ImGui::Separator();
          if (ImGui::MenuItem("Add to Workbench")) {
            workbench_.AddCatalogSelectionAsRows(
                std::vector<RE::FormID>{entry.formID},
                ResolveNewWorkbenchRowConditionId(),
                ResolveWorkbenchPreviewActor());
          }
          ImGui::Separator();
          if (ImGui::MenuItem("Add Override")) {
            AddGearEntryToWorkbench(entry);
          }
          ImGui::EndPopup();
        }
        ImGui::SetCursorScreenPos(rowContentPos);
        const auto widgetResult =
            DrawCatalogDragWidget(item, DragSourceKind::Catalog);

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(entry.plugin.data());

        if (selected) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("PRIMARY", 0.40f));
        } else if (rowHovered) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("TABLE_HOVER", 0.12f));
        }

        const bool doubleClicked =
            (rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) ||
            widgetResult.doubleClicked;
        const bool releasedOnRow =
            ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
            (rowHovered || widgetResult.hovered);
        if (doubleClicked) {
          rowClicked = true;
          AddGearEntryToWorkbench(entry);
        } else if (releasedOnRow) {
          rowClicked = true;
          if (selected) {
            ClearCatalogSelection();
          } else {
            CatalogBrowserState().selectedKey = entry.id;
            if (browser.previewSelected) {
              PreviewGearEntry(entry);
            } else {
              workbench_.ClearPreview();
            }
          }
        }
      }
    }

    ImGui::EndTable();
  }

  return rowClicked;
}
} // namespace sosr
