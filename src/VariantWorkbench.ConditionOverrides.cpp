#include "VariantWorkbench.h"

#include "ArmorUtils.h"
#include "EquipmentCatalog.h"
#include "workbench/ItemFactory.h"

#include <algorithm>
#include <iterator>
#include <unordered_set>

namespace sosr::workbench {
namespace {
bool IsValidRowIndex(const int a_rowIndex, const std::size_t a_rowCount) {
  return a_rowIndex >= 0 && a_rowIndex < static_cast<int>(a_rowCount);
}

int CountOverrides(const std::vector<VariantWorkbenchRow> &a_rows) {
  int count = 0;
  for (const auto &row : a_rows) {
    count += static_cast<int>(row.overrides.size());
  }
  return count;
}

std::uint64_t SelectPrimarySlotMask(const RE::TESObjectARMO *a_armor,
                                    const EquipmentWidgetItem &a_item) {
  if (a_armor == nullptr) {
    return 0;
  }

  auto slotMask = armor::GetArmorAddonSlotMask(a_armor);
  if (slotMask == 0) {
    slotMask = a_item.slotMask;
  }

  for (const auto singleSlotMask : armor::GetAllArmorSlotMasks()) {
    if ((slotMask & singleSlotMask) != 0) {
      return singleSlotMask;
    }
  }

  return 0;
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

bool VariantWorkbench::PlanSlotFallbackAssignments(
    const std::vector<RE::FormID> &a_formIDs,
    std::vector<PlannedSlotFallbackAssignment> &a_assignments,
    int &a_skippedCount) const {
  a_assignments.clear();
  a_skippedCount = 0;

  std::vector<const RE::TESObjectARMO *> armors;
  if (!ResolveCatalogArmors(a_formIDs, armors)) {
    return false;
  }

  std::unordered_set<RE::FormID> seenArmorForms;
  for (const auto *armor : armors) {
    if (armor == nullptr ||
        !seenArmorForms.insert(armor->GetFormID()).second) {
      continue;
    }

    EquipmentWidgetItem item{};
    if (!workbench::BuildCatalogItem(armor->GetFormID(), item) ||
        !item.SupportsDavArmorReplacement()) {
      ++a_skippedCount;
      continue;
    }

    const auto slotMask = SelectPrimarySlotMask(armor, item);
    if (slotMask == 0) {
      ++a_skippedCount;
      continue;
    }

    a_assignments.push_back({slotMask, armor->GetFormID()});
  }

  return !a_assignments.empty();
}

VariantWorkbench VariantWorkbench::BuildPlanningWorkbench() const {
  VariantWorkbench workbench;
  workbench.rows_ = rows_;
  workbench.rowOrder_ = rowOrder_;
  return workbench;
}

std::vector<int> VariantWorkbench::CollectConditionSourceRowIndices(
    const std::string_view a_conditionId) const {
  std::vector<int> rowIndices;
  for (int rowIndex = 0; rowIndex < static_cast<int>(rows_.size());
       ++rowIndex) {
    const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    if (row.conditionId.has_value() && *row.conditionId == a_conditionId &&
        row.isEquipped && !row.IsSlotRow()) {
      rowIndices.push_back(rowIndex);
    }
  }
  return rowIndices;
}

VariantWorkbench::ConditionOverrideApplicationPlan
VariantWorkbench::PlanKitLayoutConditionOverrideApplication(
    const KitEntry::Layout &a_layout, const std::string_view a_conditionId,
    const std::vector<int> *a_candidateRowIndices,
    const bool a_allowSlotFallback) const {
  ConditionOverrideApplicationPlan plan;
  const auto projection = ProjectKitLayoutRows(
      a_layout, a_candidateRowIndices,
      a_allowSlotFallback ? KitLayoutFallbackMode::AnyTargetSlot
                          : KitLayoutFallbackMode::None,
      std::string(a_conditionId), true);
  plan.previewRows.reserve(projection.rows.size());
  for (const auto &projectedRow : projection.rows) {
    plan.previewRows.push_back(projectedRow.row);
  }

  plan.overrideCount = CountOverrides(plan.previewRows);
  return plan;
}

VariantWorkbench::ConditionOverrideApplicationPlan
VariantWorkbench::PlanConditionOverrideApplication(
    const std::vector<RE::FormID> &a_formIDs,
    const std::string_view a_conditionId, RE::Actor *a_sourceActor) const {
  ConditionOverrideApplicationPlan plan;

  if (a_sourceActor != nullptr) {
    auto workbench = BuildPlanningWorkbench();
    workbench.SyncRowsFromActor(a_sourceActor, std::string(a_conditionId));
    const auto sourceRows =
        workbench.CollectConditionSourceRowIndices(a_conditionId);
    if (!sourceRows.empty()) {
      const auto resetApplied = workbench.ResetAllRows(&sourceRows);
      (void)resetApplied;
      std::vector<PlannedCatalogAssignment> assignments;
      if (!workbench.PlanCatalogAssignments(a_formIDs, assignments,
                                            &sourceRows)) {
        assignments.clear();
      }
      std::vector<int> targetRows;
      std::unordered_set<int> seenTargetRows;
      targetRows.reserve(assignments.size());
      for (const auto &assignment : assignments) {
        if (seenTargetRows.insert(assignment.rowIndex).second) {
          targetRows.push_back(assignment.rowIndex);
        }
      }

      plan.sourceKind = ConditionOverrideSourceKind::ActorSource;
      plan.sourceRowCount = static_cast<int>(sourceRows.size());
      const auto addedAny =
          workbench.AddCatalogSelectionToWorkbench(a_formIDs, &targetRows);
      (void)addedAny;
      plan.previewRows.reserve(targetRows.size());
      for (const auto rowIndex : targetRows) {
        if (IsValidRowIndex(rowIndex, workbench.rows_.size())) {
          const auto &row = workbench.rows_[static_cast<std::size_t>(rowIndex)];
          if (row.HasOverridesOrHideState()) {
            plan.previewRows.push_back(row);
          }
        }
      }
      plan.overrideCount = CountOverrides(plan.previewRows);
      plan.skippedCount =
          static_cast<int>(EquipmentCatalog::Get()
                               .ResolveArmorFormIDs(a_formIDs)
                               .size()) -
          plan.overrideCount;
      plan.skippedCount = (std::max)(0, plan.skippedCount);
      return plan;
    }
  }

  std::vector<PlannedSlotFallbackAssignment> assignments;
  int skippedCount = 0;
  if (!PlanSlotFallbackAssignments(a_formIDs, assignments, skippedCount)) {
    assignments.clear();
  }

  std::vector<std::uint64_t> slotMasks;
  std::unordered_set<std::uint64_t> seenSlotMasks;
  for (const auto &assignment : assignments) {
    if (seenSlotMasks.insert(assignment.slotMask).second) {
      slotMasks.push_back(assignment.slotMask);
    }
  }

  plan.sourceKind = ConditionOverrideSourceKind::SlotFallback;
  plan.slotRowCount = static_cast<int>(slotMasks.size());
  plan.overrideCount = static_cast<int>(assignments.size());
  plan.skippedCount = skippedCount;

  for (const auto slotMask : slotMasks) {
    auto row = BuildSlotRow(slotMask, std::string(a_conditionId), nullptr);
    if (!row.has_value()) {
      row = FindExistingSlotRow(rows_, slotMask, std::string(a_conditionId));
      if (row.has_value()) {
        row->overrides.clear();
        row->hideEquipped = false;
      }
    }

    if (row.has_value()) {
      plan.previewRows.push_back(std::move(*row));
    }
  }

  for (const auto &assignment : assignments) {
    const auto rowIt = std::ranges::find(
        plan.previewRows, assignment.slotMask,
        [](const VariantWorkbenchRow &a_row) { return a_row.equipped.slotMask; });
    if (rowIt == plan.previewRows.end()) {
      continue;
    }

    EquipmentWidgetItem item{};
    if (workbench::BuildCatalogItem(assignment.armorFormID, item)) {
      rowIt->overrides.push_back(std::move(item));
    }
  }

  std::erase_if(plan.previewRows, [](const VariantWorkbenchRow &a_row) {
    return !a_row.HasOverridesOrHideState();
  });
  plan.overrideCount = CountOverrides(plan.previewRows);
  return plan;
}

VariantWorkbench::ConditionOverrideApplicationPlan
VariantWorkbench::PlanConditionOverrideApplication(
    const KitEntry::Layout &a_layout, const std::string_view a_conditionId,
    RE::Actor *a_sourceActor) const {
  ConditionOverrideApplicationPlan plan;

  if (a_sourceActor != nullptr) {
    auto workbench = BuildPlanningWorkbench();
    workbench.SyncRowsFromActor(a_sourceActor, std::string(a_conditionId));
    const auto sourceRows =
        workbench.CollectConditionSourceRowIndices(a_conditionId);
    if (!sourceRows.empty()) {
      plan = workbench.PlanKitLayoutConditionOverrideApplication(
          a_layout, a_conditionId, &sourceRows, false);
      plan.sourceKind = ConditionOverrideSourceKind::ActorSource;
      plan.sourceRowCount = static_cast<int>(sourceRows.size());
      return plan;
    }
  }

  const std::vector<int> noCandidateRows;
  plan = PlanKitLayoutConditionOverrideApplication(a_layout, a_conditionId,
                                                  &noCandidateRows, true);
  plan.sourceKind = ConditionOverrideSourceKind::SlotFallback;
  plan.slotRowCount = static_cast<int>(plan.previewRows.size());
  return plan;
}

bool VariantWorkbench::ApplyConditionOverridePlan(
    const ConditionOverrideApplicationPlan &a_plan,
    const std::string_view a_conditionId,
    const InitialEquippedState *a_initialEquippedState) {
  if (!a_plan.CanApply()) {
    return false;
  }

  std::vector<VariantWorkbenchRow> missingEquippedRows;
  std::vector<std::string> missingEquippedRowKeys;
  for (const auto &previewRow : a_plan.previewRows) {
    if (!previewRow.HasOverridesOrHideState() || previewRow.IsSlotRow()) {
      continue;
    }
    if (std::ranges::find(rows_, previewRow.key, &VariantWorkbenchRow::key) !=
        rows_.end()) {
      continue;
    }

    missingEquippedRowKeys.push_back(previewRow.key);
    missingEquippedRows.push_back(previewRow);
  }

  bool changed = false;
  bool directlyChanged = false;
  if (!missingEquippedRows.empty()) {
    rows_.insert(rows_.begin(),
                 std::make_move_iterator(missingEquippedRows.begin()),
                 std::make_move_iterator(missingEquippedRows.end()));
    rowOrder_.insert(rowOrder_.begin(),
                     std::make_move_iterator(missingEquippedRowKeys.begin()),
                     std::make_move_iterator(missingEquippedRowKeys.end()));
    directlyChanged = true;
  }

  for (const auto &previewRow : a_plan.previewRows) {
    if (!previewRow.HasOverridesOrHideState()) {
      continue;
    }

    auto rowIt = std::ranges::find(rows_, previewRow.key,
                                   &VariantWorkbenchRow::key);
    if (rowIt == rows_.end() && previewRow.IsSlotRow()) {
      const auto added =
          AddSlotRow(previewRow.equipped.slotMask, std::string(a_conditionId),
                     a_initialEquippedState);
      changed |= added;
      rowIt = std::ranges::find(rows_, previewRow.key,
                                &VariantWorkbenchRow::key);
    }
    if (rowIt == rows_.end()) {
      continue;
    }

    if (rowIt->overrides == previewRow.overrides &&
        rowIt->hideEquipped == previewRow.hideEquipped) {
      continue;
    }

    rowIt->overrides = previewRow.overrides;
    rowIt->hideEquipped = previewRow.hideEquipped;
    directlyChanged = true;
  }

  if (directlyChanged) {
    MarkChanged();
  }
  return changed || directlyChanged;
}
} // namespace sosr::workbench
