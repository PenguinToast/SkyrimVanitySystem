#pragma once

#include "EquipmentCatalog.h"
#include "imgui.h"

#include <cstdint>
#include <string>
#include <vector>

namespace sosr::ui::catalog {
struct SortState {
  ImGuiID columnUserId{0};
  ImGuiSortDirection direction{ImGuiSortDirection_None};

  [[nodiscard]] bool operator==(const SortState &a_other) const {
    return columnUserId == a_other.columnUserId &&
           direction == a_other.direction;
  }
};

struct GearFilterState {
  std::string catalogRevision;
  std::uint64_t favoritesRevision{0};
  bool favoritesOnly{false};
  bool inventoryOnly{false};
  bool hideUnnamedGear{true};
  int pluginIndex{0};
  std::vector<bool> selectedSlotFilters;
  std::string searchText;

  [[nodiscard]] bool operator==(const GearFilterState &a_other) const {
    return catalogRevision == a_other.catalogRevision &&
           favoritesRevision == a_other.favoritesRevision &&
           favoritesOnly == a_other.favoritesOnly &&
           inventoryOnly == a_other.inventoryOnly &&
           hideUnnamedGear == a_other.hideUnnamedGear &&
           pluginIndex == a_other.pluginIndex &&
           selectedSlotFilters == a_other.selectedSlotFilters &&
           searchText == a_other.searchText;
  }
};

struct OutfitFilterState {
  std::string catalogRevision;
  std::uint64_t favoritesRevision{0};
  bool favoritesOnly{false};
  int pluginIndex{0};
  std::vector<bool> selectedSlotFilters;
  std::string searchText;

  [[nodiscard]] bool operator==(const OutfitFilterState &a_other) const {
    return catalogRevision == a_other.catalogRevision &&
           favoritesRevision == a_other.favoritesRevision &&
           favoritesOnly == a_other.favoritesOnly &&
           pluginIndex == a_other.pluginIndex &&
           selectedSlotFilters == a_other.selectedSlotFilters &&
           searchText == a_other.searchText;
  }
};

struct KitFilterState {
  std::string catalogRevision;
  std::uint64_t favoritesRevision{0};
  bool favoritesOnly{false};
  int collectionIndex{0};
  std::vector<bool> selectedSlotFilters;
  std::string searchText;

  [[nodiscard]] bool operator==(const KitFilterState &a_other) const {
    return catalogRevision == a_other.catalogRevision &&
           favoritesRevision == a_other.favoritesRevision &&
           favoritesOnly == a_other.favoritesOnly &&
           collectionIndex == a_other.collectionIndex &&
           selectedSlotFilters == a_other.selectedSlotFilters &&
           searchText == a_other.searchText;
  }
};

template <class RowPtr, class FilterState> struct RowsCache {
  bool filterInitialized{false};
  FilterState filterState{};
  std::uint64_t filteredRevision{0};
  std::vector<RowPtr> filteredRows;
  bool sortInitialized{false};
  SortState sortState{};
  std::uint64_t sortedFilteredRevision{0};
  std::vector<RowPtr> sortedRows;
};

struct DerivedState {
  std::uint64_t favoritesRevision{0};
  std::uint64_t nextFilteredRevision{1};
  RowsCache<const GearEntry *, GearFilterState> gear;
  RowsCache<const OutfitEntry *, OutfitFilterState> outfits;
  RowsCache<const KitEntry *, KitFilterState> kits;
};
} // namespace sosr::ui::catalog
