#include "ui/WorkbenchConflicts.h"

#include "ui/Localization.h"

#include <algorithm>

namespace {
struct ActiveWorkbenchVisual {
  std::string widgetId;
  std::string primaryName;
  std::string secondaryName;
  std::string conditionId;
  bool isOverride{false};
  std::string slotText;
  std::uint64_t slotMask{0};
  int rowIndex{-1};
};

bool HasVariantSelectionConflictSource(
    const sosr::workbench::VariantWorkbenchRow &a_row) {
  return a_row.HasCondition() && a_row.IsVisualConflictSource() &&
         (a_row.hideEquipped || !a_row.overrides.empty());
}

bool IsVariantSelectionConflictPair(
    const sosr::workbench::VariantWorkbenchRow &a_left,
    const sosr::workbench::VariantWorkbenchRow &a_right) {
  return a_left.conditionId == a_right.conditionId &&
         (a_left.IsSlotRow() || a_right.IsSlotRow());
}

std::string
DescribeRowSelectionReason(const sosr::workbench::VariantWorkbenchRow &a_row) {
  if (a_row.hideEquipped) {
    return std::string(sosr::ui::Localization::GetSingleton()->Get(
        "workbench.conflict.hide_equipped"));
  }
  return std::string(sosr::ui::Localization::GetSingleton()->Get(
      a_row.IsSlotRow() ? "workbench.conflict.slot_override"
                        : "workbench.conflict.item_override"));
}

template <class TConflictInfo>
void AppendConflictTarget(TConflictInfo &a_info,
                          const std::string_view a_widgetId,
                          sosr::ui::workbench_conflicts::ConflictEntry a_desc) {
  if (std::find(a_info.targetWidgetIds.begin(), a_info.targetWidgetIds.end(),
                a_widgetId) != a_info.targetWidgetIds.end()) {
    return;
  }

  a_info.targetWidgetIds.emplace_back(a_widgetId);
  a_info.targetDescriptions.push_back(std::move(a_desc));
}
} // namespace

namespace sosr::ui::workbench_conflicts {
ConflictState BuildConflictState(
    const std::vector<::sosr::workbench::VariantWorkbenchRow> &a_rows) {
  ConflictState state{};

  state.rowConflicts.reserve(a_rows.size());
  for (int rowIndex = 0; rowIndex < static_cast<int>(a_rows.size());
       ++rowIndex) {
    const auto &row = a_rows[static_cast<std::size_t>(rowIndex)];
    if (!HasVariantSelectionConflictSource(row)) {
      continue;
    }

    RowConflictInfo info{};
    for (int otherRowIndex = 0; otherRowIndex < rowIndex; ++otherRowIndex) {
      const auto &otherRow = a_rows[static_cast<std::size_t>(otherRowIndex)];
      const auto rowSlotMask = row.GetSelectionConflictSlotMask();
      const auto otherRowSlotMask = otherRow.GetSelectionConflictSlotMask();
      if (!HasVariantSelectionConflictSource(otherRow) ||
          !IsVariantSelectionConflictPair(row, otherRow) ||
          (rowSlotMask & otherRowSlotMask) == 0) {
        continue;
      }

      AppendConflictTarget(
          info, otherRow.key,
          {.primaryName = otherRow.equipped.name,
           .secondaryName = DescribeRowSelectionReason(otherRow),
           .targetLabel = otherRow.equipped.slotText});
    }

    if (!info.targetWidgetIds.empty()) {
      state.rowConflicts.emplace(row.key, std::move(info));
    }
  }

  std::vector<ActiveWorkbenchVisual> activeVisuals;
  activeVisuals.reserve(a_rows.size() * 2);
  for (int rowIndex = 0; rowIndex < static_cast<int>(a_rows.size());
       ++rowIndex) {
    const auto &row = a_rows[static_cast<std::size_t>(rowIndex)];
    if (!row.HasCondition() || !row.IsVisualConflictSource()) {
      continue;
    }

    if (!row.overrides.empty()) {
      for (int overrideIndex = 0;
           overrideIndex < static_cast<int>(row.overrides.size());
           ++overrideIndex) {
        const auto &item =
            row.overrides[static_cast<std::size_t>(overrideIndex)];
        activeVisuals.push_back(
            {.widgetId = "override:" + std::to_string(rowIndex) + ":" +
                         std::to_string(overrideIndex),
             .primaryName = item.name,
             .secondaryName = row.equipped.name,
             .conditionId = *row.conditionId,
             .isOverride = true,
             .slotText = item.slotText,
             .slotMask = row.GetOverrideVisualSlotMask(item),
             .rowIndex = rowIndex});
      }
    } else if (!row.IsSlotRow() && !row.hideEquipped && row.isEquipped) {
      activeVisuals.push_back({.widgetId = row.key,
                               .primaryName = row.equipped.name,
                               .conditionId = *row.conditionId,
                               .slotText = row.equipped.slotText,
                               .slotMask = row.equipped.slotMask,
                               .rowIndex = rowIndex});
    }
  }

  state.overrideConflicts.reserve(a_rows.size() * 2);
  for (int rowIndex = 0; rowIndex < static_cast<int>(a_rows.size());
       ++rowIndex) {
    const auto &row = a_rows[static_cast<std::size_t>(rowIndex)];
    if (!row.HasCondition() || !row.IsVisualConflictSource()) {
      continue;
    }

    for (int overrideIndex = 0;
         overrideIndex < static_cast<int>(row.overrides.size());
         ++overrideIndex) {
      const auto &item = row.overrides[static_cast<std::size_t>(overrideIndex)];
      const auto affectedSlotMask = row.GetOverrideVisualSlotMask(item);
      OverrideConflictInfo info{};
      for (const auto &activeVisual : activeVisuals) {
        if (activeVisual.rowIndex == rowIndex ||
            activeVisual.conditionId != *row.conditionId ||
            (activeVisual.slotMask & affectedSlotMask) == 0) {
          continue;
        }
        if (row.IsSlotRow() && !activeVisual.isOverride) {
          continue;
        }

        AppendConflictTarget(info, activeVisual.widgetId,
                             {.primaryName = activeVisual.primaryName,
                              .secondaryName = activeVisual.secondaryName,
                              .isOverride = activeVisual.isOverride,
                              .targetLabel = activeVisual.slotText});
      }

      if (!info.targetWidgetIds.empty()) {
        state.overrideConflicts.emplace("override:" + std::to_string(rowIndex) +
                                            ":" + std::to_string(overrideIndex),
                                        std::move(info));
      }
    }
  }

  return state;
}
} // namespace sosr::ui::workbench_conflicts
