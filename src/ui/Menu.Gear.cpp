#include "Menu.h"

#include "ui/Localization.h"
#include "workbench/ItemFactory.h"

#include <format>

namespace {
enum class GearColumn : ImGuiID { Name = 1, Plugin };
}

namespace sosr {
bool Menu::DrawGearTab() {
  auto *localization = ui::Localization::GetSingleton();
  const auto &rows = GetFilteredGearRows();
  const auto resultCount = rows.size();
  const auto equippedRowCount = workbench_.GetRowCount();
  const auto resultsLabel = std::vformat(
      std::string(localization->Get("catalog.results")),
      std::make_format_args(resultCount));
  const auto equippedRowsLabel = std::vformat(
      std::string(localization->Get("catalog.equipped_rows")),
      std::make_format_args(equippedRowCount));
  ImGui::TextUnformatted(resultsLabel.c_str());
  ImGui::SameLine();
  ImGui::TextUnformatted(equippedRowsLabel.c_str());
  return DrawGearCatalogTable();
}

bool Menu::DrawGearCatalogTable() {
  auto *localization = ui::Localization::GetSingleton();
  const auto &browser = CatalogBrowserState();
  bool rowClicked = false;
  if (ImGui::BeginTable("##gear-table", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, 0.0f))) {
    const auto nameLabel = localization->Get("common.name");
    const auto pluginLabel = localization->Get("common.plugin");
    const auto removeFavoriteLabel = localization->Get("favorites.remove");
    const auto addFavoriteLabel = localization->Get("favorites.add");
    const auto addWorkbenchLabel = localization->Get("workbench.add");
    const auto addOverrideLabel = localization->Get("workbench.add_override");
    ImGui::TableSetupColumn(nameLabel.data(), ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            static_cast<ImGuiID>(GearColumn::Name));
    ImGui::TableSetupColumn(pluginLabel.data(), ImGuiTableColumnFlags_None, 0.0f,
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
        const auto supportsDavArmorReplacement =
            item.SupportsDavArmorReplacement();
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
              favorite ? removeFavoriteLabel.data() : addFavoriteLabel.data();
          if (ImGui::MenuItem(favoriteLabel)) {
            SetFavorite(ui::catalog::BrowserTab::Gear, entry.id, !favorite);
          }
          ImGui::Separator();
          ImGui::BeginDisabled(!supportsDavArmorReplacement);
          if (ImGui::MenuItem(addWorkbenchLabel.data())) {
            const auto initialEquippedState = BuildWorkbenchInitialEquippedState();
            workbench_.AddCatalogSelectionAsRows(
                std::vector<RE::FormID>{entry.formID},
                ResolveNewWorkbenchRowConditionId(), &initialEquippedState);
          }
          ImGui::Separator();
          if (ImGui::MenuItem(addOverrideLabel.data())) {
            AddGearEntryToWorkbench(entry);
          }
          ImGui::EndDisabled();
          ImGui::EndPopup();
        }
        ImGui::SetCursorScreenPos(rowContentPos);
        const auto widgetResult = supportsDavArmorReplacement
                                      ? DrawCatalogDragWidget(
                                            item, DragSourceKind::Catalog)
                                      : ui::components::DrawEquipmentWidget(
                                            item.key.c_str(), item,
                                            {.disabledAppearance = true});

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
          if (supportsDavArmorReplacement) {
            AddGearEntryToWorkbench(entry);
          }
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
