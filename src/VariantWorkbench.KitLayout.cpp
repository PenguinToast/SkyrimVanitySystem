#include "VariantWorkbench.h"

#include "ArmorUtils.h"
#include "workbench/ItemFactory.h"

#include <algorithm>
#include <unordered_map>

namespace sosr::workbench {
namespace {
bool IsValidRowIndex(const int a_rowIndex, const std::size_t a_rowCount) {
  return a_rowIndex >= 0 && a_rowIndex < static_cast<int>(a_rowCount);
}

std::optional<VariantWorkbenchRow> FindExistingSlotRow(
    const std::vector<VariantWorkbenchRow> &a_rows, std::uint64_t a_slotMask,
    const std::optional<std::string> &a_conditionId) {
  const auto rowIt = std::ranges::find_if(
      a_rows, [&](const VariantWorkbenchRow &a_row) {
        return a_row.IsSlotRow() && a_row.equipped.slotMask == a_slotMask &&
               a_row.conditionId == a_conditionId;
      });
  if (rowIt == a_rows.end()) {
    return std::nullopt;
  }

  return *rowIt;
}
} // namespace

void VariantWorkbench::AppendOverrideItem(
    std::vector<EquipmentWidgetItem> &a_overrides,
    const RE::TESObjectARMO *a_overrideArmor) {
  if (a_overrideArmor == nullptr) {
    return;
  }
  if (std::ranges::find(a_overrides, a_overrideArmor->GetFormID(),
                        &EquipmentWidgetItem::formID) != a_overrides.end()) {
    return;
  }

  EquipmentWidgetItem item{};
  if (!workbench::BuildCatalogItem(a_overrideArmor->GetFormID(), item) ||
      !item.SupportsDavArmorReplacement() ||
      armor::GetFormIdentifier(a_overrideArmor).empty()) {
    return;
  }

  a_overrides.push_back(std::move(item));
}

std::vector<const RE::TESObjectARMO *>
VariantWorkbench::ResolveKitLayoutOverrideArmors(
    const KitEntry::LayoutRow &a_layoutRow) {
  std::vector<const RE::TESObjectARMO *> overrideArmors;
  overrideArmors.reserve(a_layoutRow.overrideIdentifiers.size());
  for (const auto &identifier : a_layoutRow.overrideIdentifiers) {
    if (const auto *overrideArmor =
            armor::LookupByIdentifier<RE::TESObjectARMO>(identifier);
        overrideArmor != nullptr) {
      overrideArmors.push_back(overrideArmor);
    }
  }
  return overrideArmors;
}

int VariantWorkbench::FindKitLayoutTargetRowIndex(
    const KitEntry::LayoutRow &a_layoutRow,
    const std::vector<int> &a_candidateRowIndices) const {
  if (a_layoutRow.targetKind == KitEntry::LayoutTargetKind::Slot) {
    for (const auto rowIndex : a_candidateRowIndices) {
      if (!IsValidRowIndex(rowIndex, rows_.size())) {
        continue;
      }

      const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
      if (row.IsSlotRow() &&
          row.equipped.slotMask == a_layoutRow.targetSlotMask) {
        return rowIndex;
      }
    }
    return -1;
  }

  if (!a_layoutRow.targetIdentifier.empty()) {
    const auto *targetArmor = armor::LookupByIdentifier<RE::TESObjectARMO>(
        a_layoutRow.targetIdentifier);
    if (targetArmor != nullptr) {
      const auto targetFormID = targetArmor->GetFormID();
      for (const auto rowIndex : a_candidateRowIndices) {
        if (!IsValidRowIndex(rowIndex, rows_.size())) {
          continue;
        }

        const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
        if (!row.IsSlotRow() && row.isEquipped &&
            row.equipped.formID == targetFormID) {
          return rowIndex;
        }
      }
    }
  }

  for (const auto rowIndex : a_candidateRowIndices) {
    if (!IsValidRowIndex(rowIndex, rows_.size())) {
      continue;
    }

    const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    if (!row.IsSlotRow() && row.isEquipped &&
        row.GetSelectionConflictSlotMask() == a_layoutRow.targetSlotMask) {
      return rowIndex;
    }
  }

  return FindBestItemTargetRowIndexBySlotMask(
      a_layoutRow.targetSlotMask, false, nullptr, nullptr,
      &a_candidateRowIndices);
}

VariantWorkbench::KitLayoutProjection VariantWorkbench::ProjectKitLayoutRows(
    const KitEntry::Layout &a_layout,
    const std::vector<int> *a_candidateRowIndices,
    const KitLayoutFallbackMode a_fallbackMode,
    std::optional<std::string> a_fallbackConditionId,
    const bool a_replaceExisting) const {
  KitLayoutProjection projection;
  const auto candidateRowIndices =
      BuildCandidateRowIndices(a_candidateRowIndices, rows_.size());

  std::unordered_map<int, std::size_t> projectedIndexByTargetRow;
  std::unordered_map<std::uint64_t, std::size_t> fallbackIndexBySlotMask;
  std::size_t projectedFallbackCount = 0;

  const auto canFallbackToSlot =
      [&](const KitEntry::LayoutRow &a_layoutRow) {
        if (a_fallbackMode == KitLayoutFallbackMode::None ||
            a_layoutRow.targetSlotMask == 0) {
          return false;
        }
        return a_fallbackMode == KitLayoutFallbackMode::AnyTargetSlot ||
               a_layoutRow.targetKind == KitEntry::LayoutTargetKind::Slot;
      };

  for (const auto &layoutRow : a_layout.rows) {
    auto targetRowIndex =
        FindKitLayoutTargetRowIndex(layoutRow, candidateRowIndices);
    auto overrideArmors = ResolveKitLayoutOverrideArmors(layoutRow);

    if (targetRowIndex < 0) {
      if (!canFallbackToSlot(layoutRow)) {
        continue;
      }

      auto slotIt = fallbackIndexBySlotMask.find(layoutRow.targetSlotMask);
      if (slotIt == fallbackIndexBySlotMask.end()) {
        auto row = BuildSlotRow(layoutRow.targetSlotMask,
                                a_fallbackConditionId, nullptr);
        if (!row.has_value()) {
          row = FindExistingSlotRow(rows_, layoutRow.targetSlotMask,
                                    a_fallbackConditionId);
        }
        if (!row.has_value()) {
          continue;
        }

        if (a_replaceExisting) {
          row->overrides.clear();
          row->hideEquipped = false;
        }
        const auto projectedIndex = projection.rows.size();
        fallbackIndexBySlotMask.emplace(layoutRow.targetSlotMask,
                                        projectedIndex);
        projection.rows.push_back(
            {.row = std::move(*row),
             .priorityRowIndex = rows_.size() + projectedFallbackCount});
        ++projectedFallbackCount;
        slotIt = fallbackIndexBySlotMask.find(layoutRow.targetSlotMask);
      }

      auto &projectedRow = projection.rows[slotIt->second].row;
      for (const auto *overrideArmor : overrideArmors) {
        AppendOverrideItem(projectedRow.overrides, overrideArmor);
      }
      projectedRow.hideEquipped = layoutRow.hideEquipped;
      continue;
    }

    const auto [rowIt, inserted] = projectedIndexByTargetRow.try_emplace(
        targetRowIndex, projection.rows.size());
    if (inserted) {
      auto row = rows_[static_cast<std::size_t>(targetRowIndex)];
      if (a_replaceExisting) {
        row.overrides.clear();
        row.hideEquipped = false;
      }
      projection.rows.push_back(
          {.row = std::move(row),
           .priorityRowIndex = static_cast<std::size_t>(targetRowIndex)});
    }

    auto &projectedRow = projection.rows[rowIt->second].row;
    for (const auto *overrideArmor : overrideArmors) {
      AppendOverrideItem(projectedRow.overrides, overrideArmor);
    }
    projectedRow.hideEquipped = layoutRow.hideEquipped;
  }

  std::erase_if(projection.rows, [](const ProjectedKitLayoutRow &a_row) {
    return !a_row.row.HasOverridesOrHideState();
  });
  return projection;
}
} // namespace sosr::workbench
