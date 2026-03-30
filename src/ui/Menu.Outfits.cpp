#include "Menu.h"

#include "imgui_internal.h"
#include "ui/catalog/Widgets.h"
#include "ui/components/PinnableTooltip.h"

namespace {
enum class OutfitColumn : ImGuiID { Name = 1, Plugin, Pieces };
}

namespace sosr {
bool Menu::DrawOutfitTab() {
  const auto &browser = CatalogBrowserState();
  auto rows = BuildFilteredOutfits();
  ImGui::Text("Results: %zu", rows.size());
  bool rowClicked = false;

  const auto tableHeight = ImGui::GetContentRegionAvail().y;
  if (ImGui::BeginTable("##outfit-table", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, tableHeight))) {
    ImGui::TableSetupColumn("Outfit", ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            static_cast<ImGuiID>(OutfitColumn::Name));
    ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_None, 0.0f,
                            static_cast<ImGuiID>(OutfitColumn::Plugin));
    ImGui::TableSetupColumn("Pieces",
                            ImGuiTableColumnFlags_PreferSortDescending, 0.0f,
                            static_cast<ImGuiID>(OutfitColumn::Pieces));
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    SortOutfitRows(rows, ImGui::TableGetSortSpecs());

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) {
      for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd;
           ++rowIndex) {
        const auto &outfit = *rows[static_cast<std::size_t>(rowIndex)];
        const auto favorite =
            IsFavorite(ui::catalog::BrowserTab::Outfits, outfit.id);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const auto rowContentPos = ImGui::GetCursorScreenPos();
        const auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
        const bool selected =
            browser.selectedKey == outfit.id &&
            (!browser.previewSelected ||
             workbench_.IsPreviewingSelection(outfit.id));
        const bool clicked = ImGui::Selectable(
            ("##outfit-row-hit-" + std::to_string(rowIndex)).c_str(), selected,
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
            SetFavorite(ui::catalog::BrowserTab::Outfits, outfit.id, !favorite);
          }
          ImGui::Separator();
          if (ImGui::MenuItem("Add to Workbench")) {
            workbench_.AddCatalogSelectionAsRows(
                outfit.GetArmorFormIDs(), ResolveNewWorkbenchRowConditionId());
          }
          ImGui::Separator();
          if (ImGui::MenuItem("Add Override")) {
            AddOutfitEntryToWorkbench(outfit, true);
          }
          if (ImGui::MenuItem("Append Overrides")) {
            AddOutfitEntryToWorkbench(outfit, false);
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

        if (clicked) {
          rowClicked = true;
          if (browser.selectedKey == outfit.id) {
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

        if (rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          rowClicked = true;
          AddOutfitEntryToWorkbench(outfit, true);
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
