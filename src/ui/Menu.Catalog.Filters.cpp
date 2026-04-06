#include "Menu.h"

#include "ArmorUtils.h"
#include "InputManager.h"
#include "PlayerInventory.h"
#include "ThemeConfig.h"
#include "ui/InputWidgets.h"
#include "ui/components/EditableCombo.h"
#include "ui/conditions/EditorSupport.h"

#include <algorithm>
#include <cfloat>
#include <charconv>
#include <unordered_set>

namespace sosr {
namespace {
enum class GearColumn : ImGuiID { Name = 1, Plugin };

enum class OutfitColumn : ImGuiID { Name = 1, Plugin, Pieces };

enum class KitColumn : ImGuiID { Name = 1, Collection, Pieces };

int CompareCatalogText(const std::string_view a_left,
                       const std::string_view a_right) {
  return ui::condition_editor::CompareTextInsensitive(a_left, a_right);
}

std::uint64_t GetCatalogSlotFilterMask(const std::string_view a_label) {
  if (a_label == "None") {
    return 0;
  }

  const auto delimiter = a_label.find(' ');
  const auto numberText = a_label.substr(0, delimiter);
  std::uint32_t slotNumber = 0;
  const auto [_, error] = std::from_chars(
      numberText.data(), numberText.data() + numberText.size(), slotNumber);
  return error == std::errc{} ? armor::GetArmorSlotMask(slotNumber) : 0;
}

std::string ReadFilterInputText(const ImGuiTextFilter &a_filter) {
  return a_filter.InputBuf;
}
} // namespace

bool Menu::MatchesGearFilters(const GearEntry &a_entry) const {
  const auto &browser = CatalogBrowserState();
  if (browser.favoritesOnly &&
      !IsFavorite(ui::catalog::BrowserTab::Gear, a_entry.id)) {
    return false;
  }

  if (browser.hideUnnamedGear && !a_entry.hasDisplayName) {
    return false;
  }

  const auto &catalog = EquipmentCatalog::Get();
  if (browser.gearPluginIndex > 0 &&
      a_entry.plugin != catalog.GetGearPlugins()[browser.gearPluginIndex - 1]) {
    return false;
  }

  return MatchesSelectedSlotsOr(a_entry.slots) &&
         browser.gearSearch.PassFilter(a_entry.searchText.c_str());
}

bool Menu::MatchesOutfitFilters(const OutfitEntry &a_entry) const {
  const auto &browser = CatalogBrowserState();
  if (browser.favoritesOnly &&
      !IsFavorite(ui::catalog::BrowserTab::Outfits, a_entry.id)) {
    return false;
  }

  const auto &catalog = EquipmentCatalog::Get();
  if (browser.outfitPluginIndex > 0 &&
      a_entry.plugin !=
          catalog.GetOutfitPlugins()[browser.outfitPluginIndex - 1]) {
    return false;
  }

  return MatchesSelectedSlotsAnd(a_entry.GetSlotMask()) &&
         browser.outfitSearch.PassFilter(a_entry.searchText.c_str());
}

bool Menu::MatchesKitFilters(const KitEntry &a_entry) const {
  const auto &browser = CatalogBrowserState();
  if (browser.favoritesOnly &&
      !IsFavorite(ui::catalog::BrowserTab::Kits, a_entry.id)) {
    return false;
  }

  const auto &catalog = EquipmentCatalog::Get();
  if (browser.kitCollectionIndex > 0 &&
      a_entry.collection !=
          catalog.GetKitCollections()[browser.kitCollectionIndex - 1]) {
    return false;
  }

  return MatchesSelectedSlotsAnd(a_entry.GetSlotMask()) &&
         browser.kitSearch.PassFilter(a_entry.searchText.c_str());
}

void Menu::SyncSelectedSlotFilters() {
  const auto size = EquipmentCatalog::Get().GetGearSlots().size();
  auto &selectedSlotFilters = CatalogBrowserState().selectedSlotFilters;
  if (selectedSlotFilters.size() != size) {
    selectedSlotFilters.resize(size, false);
  }
}

bool Menu::HasAnySelectedSlotFilter() const {
  return std::ranges::any_of(CatalogBrowserState().selectedSlotFilters,
                             [](bool a_selected) { return a_selected; });
}

bool Menu::MatchesSelectedSlotsOr(
    const std::vector<std::string> &a_slots) const {
  if (!HasAnySelectedSlotFilter()) {
    return true;
  }

  const auto &slotOptions = EquipmentCatalog::Get().GetGearSlots();
  const auto &selectedSlotFilters = CatalogBrowserState().selectedSlotFilters;
  for (std::size_t index = 0; index < selectedSlotFilters.size(); ++index) {
    if (!selectedSlotFilters[index]) {
      continue;
    }
    if (std::ranges::find(a_slots, slotOptions[index]) != a_slots.end()) {
      return true;
    }
  }

  return false;
}

bool Menu::MatchesSelectedSlotsAnd(const std::uint64_t a_slotMask) const {
  if (!HasAnySelectedSlotFilter()) {
    return true;
  }

  const auto &slotOptions = EquipmentCatalog::Get().GetGearSlots();
  const auto &selectedSlotFilters = CatalogBrowserState().selectedSlotFilters;
  for (std::size_t index = 0; index < selectedSlotFilters.size(); ++index) {
    if (!selectedSlotFilters[index]) {
      continue;
    }

    const auto requiredMask = GetCatalogSlotFilterMask(slotOptions[index]);
    if (requiredMask == 0) {
      if (a_slotMask != 0) {
        return false;
      }
      continue;
    }

    if ((a_slotMask & requiredMask) == 0) {
      return false;
    }
  }

  return true;
}

std::string Menu::BuildSelectedSlotPreview() const {
  const auto &slotOptions = EquipmentCatalog::Get().GetGearSlots();
  const auto &selectedSlotFilters = CatalogBrowserState().selectedSlotFilters;
  std::size_t selectedCount = 0;
  std::string_view selectedLabel = "Any slot";

  for (std::size_t index = 0; index < selectedSlotFilters.size(); ++index) {
    if (!selectedSlotFilters[index]) {
      continue;
    }
    ++selectedCount;
    selectedLabel = slotOptions[index];
  }

  if (selectedCount == 0) {
    return "Any slot";
  }
  if (selectedCount == 1) {
    return std::string(selectedLabel);
  }
  return std::to_string(selectedCount) + " slots";
}

std::vector<const GearEntry *> Menu::BuildFilteredGear() const {
  std::vector<const GearEntry *> rows;
  rows.reserve(EquipmentCatalog::Get().GetGear().size());
  const auto &browser = CatalogBrowserState();
  const auto inventoryFormIDs =
      browser.inventoryOnly ? player_inventory::GetInventoryArmorFormIDs()
                            : std::unordered_set<RE::FormID>{};

  for (const auto &entry : EquipmentCatalog::Get().GetGear()) {
    if (MatchesGearFilters(entry) &&
        (!browser.inventoryOnly || inventoryFormIDs.contains(entry.formID))) {
      rows.push_back(std::addressof(entry));
    }
  }
  return rows;
}

std::vector<const OutfitEntry *> Menu::BuildFilteredOutfits() const {
  std::vector<const OutfitEntry *> rows;
  rows.reserve(EquipmentCatalog::Get().GetOutfits().size());
  for (const auto &entry : EquipmentCatalog::Get().GetOutfits()) {
    if (MatchesOutfitFilters(entry)) {
      rows.push_back(std::addressof(entry));
    }
  }
  return rows;
}

std::vector<const KitEntry *> Menu::BuildFilteredKits() const {
  std::vector<const KitEntry *> rows;
  rows.reserve(EquipmentCatalog::Get().GetKits().size());
  for (const auto &entry : EquipmentCatalog::Get().GetKits()) {
    if (MatchesKitFilters(entry)) {
      rows.push_back(std::addressof(entry));
    }
  }
  return rows;
}

void Menu::InvalidateCatalogDerivedState() {
  catalogDerived_.nextFilteredRevision = 1;
  catalogDerived_.gear = {};
  catalogDerived_.outfits = {};
  catalogDerived_.kits = {};
}

const std::vector<const GearEntry *> &Menu::GetFilteredGearRows() {
  const auto &browser = CatalogBrowserState();
  ui::catalog::GearFilterState currentState{
      .catalogRevision = std::string(EquipmentCatalog::Get().GetRevision()),
      .favoritesRevision = catalogDerived_.favoritesRevision,
      .favoritesOnly = browser.favoritesOnly,
      .inventoryOnly = browser.inventoryOnly,
      .hideUnnamedGear = browser.hideUnnamedGear,
      .pluginIndex = browser.gearPluginIndex,
      .selectedSlotFilters = browser.selectedSlotFilters,
      .searchText = ReadFilterInputText(browser.gearSearch),
  };

  auto &cache = catalogDerived_.gear;
  if (!cache.filterInitialized || !(cache.filterState == currentState)) {
    cache.filteredRows = BuildFilteredGear();
    cache.filterState = std::move(currentState);
    cache.filterInitialized = true;
    cache.filteredRevision = catalogDerived_.nextFilteredRevision++;
    cache.sortInitialized = false;
  }

  return cache.filteredRows;
}

const std::vector<const OutfitEntry *> &Menu::GetFilteredOutfitRows() {
  const auto &browser = CatalogBrowserState();
  ui::catalog::OutfitFilterState currentState{
      .catalogRevision = std::string(EquipmentCatalog::Get().GetRevision()),
      .favoritesRevision = catalogDerived_.favoritesRevision,
      .favoritesOnly = browser.favoritesOnly,
      .pluginIndex = browser.outfitPluginIndex,
      .selectedSlotFilters = browser.selectedSlotFilters,
      .searchText = ReadFilterInputText(browser.outfitSearch),
  };

  auto &cache = catalogDerived_.outfits;
  if (!cache.filterInitialized || !(cache.filterState == currentState)) {
    cache.filteredRows = BuildFilteredOutfits();
    cache.filterState = std::move(currentState);
    cache.filterInitialized = true;
    cache.filteredRevision = catalogDerived_.nextFilteredRevision++;
    cache.sortInitialized = false;
  }

  return cache.filteredRows;
}

const std::vector<const KitEntry *> &Menu::GetFilteredKitRows() {
  const auto &browser = CatalogBrowserState();
  ui::catalog::KitFilterState currentState{
      .catalogRevision = std::string(EquipmentCatalog::Get().GetRevision()),
      .favoritesRevision = catalogDerived_.favoritesRevision,
      .favoritesOnly = browser.favoritesOnly,
      .collectionIndex = browser.kitCollectionIndex,
      .selectedSlotFilters = browser.selectedSlotFilters,
      .searchText = ReadFilterInputText(browser.kitSearch),
  };

  auto &cache = catalogDerived_.kits;
  if (!cache.filterInitialized || !(cache.filterState == currentState)) {
    cache.filteredRows = BuildFilteredKits();
    cache.filterState = std::move(currentState);
    cache.filterInitialized = true;
    cache.filteredRevision = catalogDerived_.nextFilteredRevision++;
    cache.sortInitialized = false;
  }

  return cache.filteredRows;
}

const std::vector<const GearEntry *> &
Menu::GetSortedGearRows(ImGuiTableSortSpecs *a_sortSpecs) {
  const auto &filteredRows = GetFilteredGearRows();
  auto &cache = catalogDerived_.gear;
  const ui::catalog::SortState currentSort{
      .columnUserId = (a_sortSpecs && a_sortSpecs->SpecsCount > 0)
                          ? a_sortSpecs->Specs[0].ColumnUserID
                          : 0,
      .direction = (a_sortSpecs && a_sortSpecs->SpecsCount > 0)
                       ? a_sortSpecs->Specs[0].SortDirection
                       : ImGuiSortDirection_None,
  };

  if (!cache.sortInitialized || !(cache.sortState == currentSort) ||
      cache.sortedFilteredRevision != cache.filteredRevision ||
      (a_sortSpecs && a_sortSpecs->SpecsDirty)) {
    cache.sortedRows = filteredRows;
    SortGearRows(cache.sortedRows, a_sortSpecs);
    cache.sortState = currentSort;
    cache.sortInitialized = true;
    cache.sortedFilteredRevision = cache.filteredRevision;
  }

  return cache.sortedRows;
}

const std::vector<const OutfitEntry *> &
Menu::GetSortedOutfitRows(ImGuiTableSortSpecs *a_sortSpecs) {
  const auto &filteredRows = GetFilteredOutfitRows();
  auto &cache = catalogDerived_.outfits;
  const ui::catalog::SortState currentSort{
      .columnUserId = (a_sortSpecs && a_sortSpecs->SpecsCount > 0)
                          ? a_sortSpecs->Specs[0].ColumnUserID
                          : 0,
      .direction = (a_sortSpecs && a_sortSpecs->SpecsCount > 0)
                       ? a_sortSpecs->Specs[0].SortDirection
                       : ImGuiSortDirection_None,
  };

  if (!cache.sortInitialized || !(cache.sortState == currentSort) ||
      cache.sortedFilteredRevision != cache.filteredRevision ||
      (a_sortSpecs && a_sortSpecs->SpecsDirty)) {
    cache.sortedRows = filteredRows;
    SortOutfitRows(cache.sortedRows, a_sortSpecs);
    cache.sortState = currentSort;
    cache.sortInitialized = true;
    cache.sortedFilteredRevision = cache.filteredRevision;
  }

  return cache.sortedRows;
}

const std::vector<const KitEntry *> &
Menu::GetSortedKitRows(ImGuiTableSortSpecs *a_sortSpecs) {
  const auto &filteredRows = GetFilteredKitRows();
  auto &cache = catalogDerived_.kits;
  const ui::catalog::SortState currentSort{
      .columnUserId = (a_sortSpecs && a_sortSpecs->SpecsCount > 0)
                          ? a_sortSpecs->Specs[0].ColumnUserID
                          : 0,
      .direction = (a_sortSpecs && a_sortSpecs->SpecsCount > 0)
                       ? a_sortSpecs->Specs[0].SortDirection
                       : ImGuiSortDirection_None,
  };

  if (!cache.sortInitialized || !(cache.sortState == currentSort) ||
      cache.sortedFilteredRevision != cache.filteredRevision ||
      (a_sortSpecs && a_sortSpecs->SpecsDirty)) {
    cache.sortedRows = filteredRows;
    SortKitRows(cache.sortedRows, a_sortSpecs);
    cache.sortState = currentSort;
    cache.sortInitialized = true;
    cache.sortedFilteredRevision = cache.filteredRevision;
  }

  return cache.sortedRows;
}

void Menu::SortGearRows(std::vector<const GearEntry *> &a_rows,
                        ImGuiTableSortSpecs *a_sortSpecs) const {
  if (!a_sortSpecs || a_sortSpecs->SpecsCount == 0) {
    std::ranges::sort(a_rows, [](const auto *a_left, const auto *a_right) {
      return CompareCatalogText(a_left->name, a_right->name) < 0;
    });
    return;
  }

  const auto &spec = a_sortSpecs->Specs[0];
  const auto ascending = spec.SortDirection == ImGuiSortDirection_Ascending;

  std::ranges::sort(a_rows, [&](const auto *a_left, const auto *a_right) {
    int compare = 0;
    switch (static_cast<GearColumn>(spec.ColumnUserID)) {
    case GearColumn::Plugin:
      compare = CompareCatalogText(a_left->plugin, a_right->plugin);
      break;
    case GearColumn::Name:
    default:
      compare = CompareCatalogText(a_left->name, a_right->name);
      break;
    }

    if (compare == 0) {
      compare = CompareCatalogText(a_left->name, a_right->name);
    }

    return ascending ? compare < 0 : compare > 0;
  });

  a_sortSpecs->SpecsDirty = false;
}

void Menu::SortOutfitRows(std::vector<const OutfitEntry *> &a_rows,
                          ImGuiTableSortSpecs *a_sortSpecs) const {
  if (!a_sortSpecs || a_sortSpecs->SpecsCount == 0) {
    std::ranges::sort(a_rows, [](const auto *a_left, const auto *a_right) {
      return CompareCatalogText(a_left->name, a_right->name) < 0;
    });
    return;
  }

  const auto &spec = a_sortSpecs->Specs[0];
  const auto ascending = spec.SortDirection == ImGuiSortDirection_Ascending;

  std::ranges::sort(a_rows, [&](const auto *a_left, const auto *a_right) {
    int compare = 0;
    switch (static_cast<OutfitColumn>(spec.ColumnUserID)) {
    case OutfitColumn::Plugin:
      compare = CompareCatalogText(a_left->plugin, a_right->plugin);
      break;
    case OutfitColumn::Pieces:
      compare =
          CompareCatalogText(a_left->GetPiecesText(), a_right->GetPiecesText());
      break;
    case OutfitColumn::Name:
    default:
      compare = CompareCatalogText(a_left->name, a_right->name);
      break;
    }

    if (compare == 0) {
      compare = CompareCatalogText(a_left->name, a_right->name);
    }

    return ascending ? compare < 0 : compare > 0;
  });

  a_sortSpecs->SpecsDirty = false;
}

void Menu::SortKitRows(std::vector<const KitEntry *> &a_rows,
                       ImGuiTableSortSpecs *a_sortSpecs) const {
  if (!a_sortSpecs || a_sortSpecs->SpecsCount == 0) {
    std::ranges::sort(a_rows, [](const auto *a_left, const auto *a_right) {
      return CompareCatalogText(a_left->name, a_right->name) < 0;
    });
    return;
  }

  const auto &spec = a_sortSpecs->Specs[0];
  const auto ascending = spec.SortDirection == ImGuiSortDirection_Ascending;

  std::ranges::sort(a_rows, [&](const auto *a_left, const auto *a_right) {
    int compare = 0;
    switch (static_cast<KitColumn>(spec.ColumnUserID)) {
    case KitColumn::Collection:
      compare = CompareCatalogText(a_left->collection, a_right->collection);
      break;
    case KitColumn::Pieces:
      compare =
          CompareCatalogText(a_left->GetPiecesText(), a_right->GetPiecesText());
      break;
    case KitColumn::Name:
    default:
      compare = CompareCatalogText(a_left->name, a_right->name);
      break;
    }

    if (compare == 0) {
      compare = CompareCatalogText(a_left->name, a_right->name);
    }

    return ascending ? compare < 0 : compare > 0;
  });

  a_sortSpecs->SpecsDirty = false;
}

void Menu::DrawCatalogFilters() {
  auto &browser = CatalogBrowserState();
  if (browser.activeTab == ui::catalog::BrowserTab::Slots ||
      browser.activeTab == ui::catalog::BrowserTab::Conditions) {
    return;
  }

  const auto &catalog = EquipmentCatalog::Get();
  SyncSelectedSlotFilters();
  const auto itemSpacingX = ImGui::GetStyle().ItemSpacing.x;
  const auto filterBarWidth = ImGui::GetContentRegionAvail().x;
  const auto drawSearchField = [&](ImGuiTextFilter &a_filter,
                                   const float a_width, const char *a_id,
                                   const char *a_placeholder) {
    ImGui::PushItemWidth(a_width);
    if (ImGui::InputText(a_id, a_filter.InputBuf,
                         IM_ARRAYSIZE(a_filter.InputBuf),
                         ImGuiInputTextFlags_AutoSelectAll)) {
      a_filter.Build();
    }
    ImGui::PopItemWidth();
    const auto rectMin = ImGui::GetItemRectMin();
    const auto rectMax = ImGui::GetItemRectMax();
    ui::input_widgets::DrawInputOutline(
        rectMin, rectMax, ImGui::IsItemHovered(), ImGui::IsItemActive());
    if (ImGui::IsItemActivated()) {
      InputManager::GetSingleton()->Flush();
    }
    if (a_filter.InputBuf[0] == '\0') {
      const auto padding = ImGui::GetStyle().FramePadding;
      ImGui::GetWindowDrawList()->AddText(
          ImVec2(rectMin.x + padding.x, rectMin.y + padding.y),
          ThemeConfig::GetSingleton()->GetColorU32("TEXT_DISABLED", 0.90f),
          a_placeholder);
    }
  };
  const auto drawSlotMultiCombo = [&]() {
    const auto preview = BuildSelectedSlotPreview();
    if (ImGui::BeginCombo("##slot-filter", preview.c_str())) {
      const auto anySelected = !HasAnySelectedSlotFilter();
      if (ImGui::Selectable("Any slot", anySelected,
                            ImGuiSelectableFlags_DontClosePopups)) {
        std::fill(browser.selectedSlotFilters.begin(),
                  browser.selectedSlotFilters.end(), false);
      }
      if (anySelected) {
        ImGui::SetItemDefaultFocus();
      }

      ImGui::Separator();

      for (std::size_t index = 0; index < catalog.GetGearSlots().size();
           ++index) {
        bool selected = browser.selectedSlotFilters[index];
        if (ImGui::Selectable(catalog.GetGearSlots()[index].c_str(), selected,
                              ImGuiSelectableFlags_DontClosePopups)) {
          browser.selectedSlotFilters[index] = !selected;
        }
      }
      ImGui::EndCombo();
    }
  };

  if (filterBarWidth < 560.0f) {
    if (browser.activeTab == ui::catalog::BrowserTab::Gear) {
      drawSearchField(browser.gearSearch, -FLT_MIN, "##gear-search",
                      "Search installed armor");
    } else if (browser.activeTab == ui::catalog::BrowserTab::Outfits) {
      drawSearchField(browser.outfitSearch, -FLT_MIN, "##outfit-search",
                      "Search installed outfits");
    } else {
      drawSearchField(browser.kitSearch, -FLT_MIN, "##kit-search",
                      "Search Modex kits");
    }

    const auto halfWidth = (filterBarWidth - itemSpacingX) * 0.5f;
    ImGui::SetNextItemWidth(halfWidth);
    if (browser.activeTab == ui::catalog::BrowserTab::Gear) {
      ui::components::DrawSearchableStringCombo(
          "##gear-plugin", "All plugins", catalog.GetGearPlugins(),
          browser.gearPluginIndex, browser.gearPluginFilter);
    } else if (browser.activeTab == ui::catalog::BrowserTab::Outfits) {
      ui::components::DrawSearchableStringCombo(
          "##outfit-plugin", "All plugins", catalog.GetOutfitPlugins(),
          browser.outfitPluginIndex, browser.outfitPluginFilter);
    } else {
      ui::components::DrawSearchableStringCombo(
          "##kit-collection", "All collections", catalog.GetKitCollections(),
          browser.kitCollectionIndex, browser.kitCollectionFilter);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    drawSlotMultiCombo();
  } else {
    const auto searchWidth = filterBarWidth * 0.40f;
    const auto pluginWidth = filterBarWidth * 0.28f;
    const auto slotWidth =
        filterBarWidth - searchWidth - pluginWidth - (itemSpacingX * 2.0f);

    if (browser.activeTab == ui::catalog::BrowserTab::Gear) {
      drawSearchField(browser.gearSearch, searchWidth, "##gear-search",
                      "Search installed armor");
    } else if (browser.activeTab == ui::catalog::BrowserTab::Outfits) {
      drawSearchField(browser.outfitSearch, searchWidth, "##outfit-search",
                      "Search installed outfits");
    } else {
      drawSearchField(browser.kitSearch, searchWidth, "##kit-search",
                      "Search Modex kits");
    }
    ImGui::SameLine();

    ImGui::SetNextItemWidth(pluginWidth);
    if (browser.activeTab == ui::catalog::BrowserTab::Gear) {
      ui::components::DrawSearchableStringCombo(
          "##gear-plugin", "All plugins", catalog.GetGearPlugins(),
          browser.gearPluginIndex, browser.gearPluginFilter);
    } else if (browser.activeTab == ui::catalog::BrowserTab::Outfits) {
      ui::components::DrawSearchableStringCombo(
          "##outfit-plugin", "All plugins", catalog.GetOutfitPlugins(),
          browser.outfitPluginIndex, browser.outfitPluginFilter);
    } else {
      ui::components::DrawSearchableStringCombo(
          "##kit-collection", "All collections", catalog.GetKitCollections(),
          browser.kitCollectionIndex, browser.kitCollectionFilter);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(slotWidth);
    drawSlotMultiCombo();
  }
}
} // namespace sosr
