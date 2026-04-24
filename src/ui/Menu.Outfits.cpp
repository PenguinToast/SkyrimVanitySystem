#include "Menu.h"

#include "imgui_internal.h"
#include "ui/Localization.h"
#include "ui/catalog/Widgets.h"
#include "ui/components/PinnableTooltip.h"

#include <format>

namespace {
enum class OutfitColumn : ImGuiID { Name = 1, Plugin, Pieces };
}

namespace sosr {
bool Menu::DrawOutfitTab() {
  auto *localization = ui::Localization::GetSingleton();
  const auto &browser = CatalogBrowserState();
  const auto &rows = GetFilteredOutfitRows();
  const auto resultCount = rows.size();
  const auto resultsLabel = std::vformat(
      std::string(localization->Get("catalog.results")),
      std::make_format_args(resultCount));
  ImGui::TextUnformatted(resultsLabel.c_str());
  bool rowClicked = false;

  const auto tableHeight = ImGui::GetContentRegionAvail().y;
  if (ImGui::BeginTable("##outfit-table", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, tableHeight))) {
    const auto outfitLabel = localization->Get("common.outfit");
    const auto pluginLabel = localization->Get("common.plugin");
    const auto piecesLabel = localization->Get("common.pieces");
    const auto removeFavoriteLabel = localization->Get("favorites.remove");
    const auto addFavoriteLabel = localization->Get("favorites.add");
    const auto addWorkbenchLabel = localization->Get("workbench.add");
    const auto addOverrideLabel = localization->Get("workbench.add_override");
    const auto appendOverridesLabel =
        localization->Get("workbench.append_overrides");
    ImGui::TableSetupColumn(outfitLabel.data(), ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            static_cast<ImGuiID>(OutfitColumn::Name));
    ImGui::TableSetupColumn(pluginLabel.data(), ImGuiTableColumnFlags_None, 0.0f,
                            static_cast<ImGuiID>(OutfitColumn::Plugin));
    ImGui::TableSetupColumn(piecesLabel.data(),
                            ImGuiTableColumnFlags_PreferSortDescending, 0.0f,
                            static_cast<ImGuiID>(OutfitColumn::Pieces));
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    const auto &sortedRows = GetSortedOutfitRows(ImGui::TableGetSortSpecs());

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(sortedRows.size()));
    while (clipper.Step()) {
      for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd;
           ++rowIndex) {
        const auto &outfit = *sortedRows[static_cast<std::size_t>(rowIndex)];
        const auto favorite =
            IsFavorite(ui::catalog::BrowserTab::Outfits, outfit.id);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const auto rowContentPos = ImGui::GetCursorScreenPos();
        const auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
        const bool selected = browser.selectedKey == outfit.id &&
                              (!browser.previewSelected ||
                               workbench_.IsPreviewingSelection(outfit.id));
        ImGui::Selectable(
            ("##outfit-row-hit-" + std::to_string(rowIndex)).c_str(), selected,
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
            SetFavorite(ui::catalog::BrowserTab::Outfits, outfit.id, !favorite);
          }
          ImGui::Separator();
          if (ImGui::MenuItem(addWorkbenchLabel.data())) {
            const auto initialEquippedState = BuildWorkbenchInitialEquippedState();
            workbench_.AddCatalogSelectionAsRows(
                outfit.GetArmorFormIDs(), ResolveNewWorkbenchRowConditionId(),
                &initialEquippedState);
          }
          ImGui::Separator();
          if (ImGui::MenuItem(addOverrideLabel.data())) {
            AddOutfitEntryToWorkbench(outfit, true);
          }
          if (ImGui::MenuItem(appendOverridesLabel.data())) {
            AddOutfitEntryToWorkbench(outfit, false);
          }
          DrawApplyWithConditionMenu({.name = outfit.name,
                                      .formIDs = outfit.GetArmorFormIDs()});
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

        const bool doubleClicked =
            rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        const bool releasedOnRow =
            ImGui::IsMouseReleased(ImGuiMouseButton_Left) && rowHovered;
        if (doubleClicked) {
          rowClicked = true;
          AddOutfitEntryToWorkbench(outfit, true);
        } else if (releasedOnRow) {
          rowClicked = true;
          if (selected) {
            ClearCatalogSelection();
          } else {
            CatalogBrowserState().selectedKey = outfit.id;
            if (browser.previewSelected) {
              PreviewOutfitEntry(outfit);
            } else {
              workbench_.ClearPreview();
            }
          }
        }
        if (!ImGui::IsDragDropActive() &&
            ui::components::ShouldDrawPinnableTooltip("outfit:" + outfit.id,
                                                      rowHovered)) {
          ui::catalog::DrawOutfitTooltip(outfit, rowHovered);
        }

        const auto displayName = BuildFavoriteLabel(outfit.name, favorite);
        ImGui::TextUnformatted(displayName.c_str());

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(outfit.plugin.data());

        ImGui::TableSetColumnIndex(2);
        {
          const auto availableWidth = ImGui::GetContentRegionAvail().x;
          const auto displayText = ui::catalog::TruncateTextToWidth(
              outfit.GetPiecesText(), availableWidth);
          ImGui::TextUnformatted(displayText.c_str());
        }
      }
    }

    ImGui::EndTable();
  }

  return rowClicked;
}
} // namespace sosr
