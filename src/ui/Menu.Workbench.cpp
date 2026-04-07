#include "Menu.h"

#include "ui/Localization.h"
#include "ui/workbench/Common.h"

#include <cstring>
namespace sosr {
namespace {
int AdjustSourceRowIndexAfterInsert(const int a_sourceRowIndex,
                                    const int a_targetRowIndex,
                                    const bool a_insertAfter) {
  auto insertIndex = a_targetRowIndex + (a_insertAfter ? 1 : 0);
  if (a_sourceRowIndex >= insertIndex) {
    return a_sourceRowIndex + 1;
  }
  return a_sourceRowIndex;
}
} // namespace

void Menu::AcceptOverridePayload(int a_targetRowIndex) {
  const auto *payload =
      ImGui::AcceptDragDropPayload(ui::workbench::kVariantItemPayloadType);
  if (!payload || payload->DataSize != sizeof(DraggedEquipmentPayload)) {
    return;
  }

  DraggedEquipmentPayload dragPayload{};
  std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));

  if (dragPayload.sourceKind ==
      static_cast<std::uint32_t>(DragSourceKind::Override)) {
    workbench_.MoveOverride(dragPayload.rowIndex, dragPayload.itemIndex,
                            a_targetRowIndex);
  } else if (dragPayload.sourceKind ==
             static_cast<std::uint32_t>(DragSourceKind::Row)) {
    if (!workbench_.AddCatalogOverride(a_targetRowIndex, dragPayload.formID)) {
      return;
    }

    const auto &rows = workbench_.GetRows();
    if (dragPayload.rowIndex < 0 ||
        dragPayload.rowIndex >= static_cast<int>(rows.size())) {
      return;
    }

    const auto &sourceRow = rows[static_cast<std::size_t>(dragPayload.rowIndex)];
    if (!sourceRow.overrides.empty()) {
      return;
    }

    workbench_.DeleteRow(dragPayload.rowIndex);
  } else if (dragPayload.sourceKind ==
             static_cast<std::uint32_t>(DragSourceKind::Catalog)) {
    workbench_.AddCatalogOverride(a_targetRowIndex, dragPayload.formID);
  }
}

bool Menu::ApplyWorkbenchRowDrop(const DraggedEquipmentPayload &a_dragPayload,
                                 const int a_targetRowIndex,
                                 const bool a_insertAfter) {
  const auto initialEquippedState = BuildWorkbenchInitialEquippedState();
  if (a_targetRowIndex < 0) {
    if (a_dragPayload.sourceKind ==
            static_cast<std::uint32_t>(DragSourceKind::Catalog) ||
        a_dragPayload.sourceKind ==
            static_cast<std::uint32_t>(DragSourceKind::Override)) {
      const bool added = workbench_.AddCatalogSelectionAsRows(
          std::vector<RE::FormID>{a_dragPayload.formID},
          ResolveNewWorkbenchRowConditionId(), &initialEquippedState);
      if (added && a_dragPayload.sourceKind ==
                       static_cast<std::uint32_t>(DragSourceKind::Override)) {
        workbench_.DeleteOverride(a_dragPayload.rowIndex,
                                  a_dragPayload.itemIndex);
      }
      return added;
    }
    if (a_dragPayload.sourceKind ==
        static_cast<std::uint32_t>(DragSourceKind::SlotCatalog)) {
      return workbench_.AddSlotRow(a_dragPayload.slotMask,
                                   ResolveNewWorkbenchRowConditionId(),
                                   &initialEquippedState);
    }
    return false;
  }

  ApplyRowReorder(a_dragPayload, a_targetRowIndex, a_insertAfter);
  return true;
}

void Menu::ApplyRowReorder(const DraggedEquipmentPayload &a_dragPayload,
                           int a_targetRowIndex, bool a_insertAfter) {
  const auto initialEquippedState = BuildWorkbenchInitialEquippedState();
  if (a_dragPayload.sourceKind ==
      static_cast<std::uint32_t>(DragSourceKind::Row)) {
    workbench_.ApplyRowReorder(a_dragPayload.rowIndex, a_targetRowIndex,
                               a_insertAfter);
  } else if (a_dragPayload.sourceKind ==
                 static_cast<std::uint32_t>(DragSourceKind::Catalog) ||
             a_dragPayload.sourceKind ==
                 static_cast<std::uint32_t>(DragSourceKind::Override)) {
    const bool inserted =
        workbench_.InsertCatalogRow(a_dragPayload.formID, a_targetRowIndex,
                                    a_insertAfter,
                                    ResolveNewWorkbenchRowConditionId(),
                                    &initialEquippedState);
    if (inserted && a_dragPayload.sourceKind ==
                        static_cast<std::uint32_t>(DragSourceKind::Override)) {
      workbench_.DeleteOverride(
          AdjustSourceRowIndexAfterInsert(a_dragPayload.rowIndex,
                                          a_targetRowIndex, a_insertAfter),
          a_dragPayload.itemIndex);
    }
  } else if (a_dragPayload.sourceKind ==
             static_cast<std::uint32_t>(DragSourceKind::SlotCatalog)) {
    workbench_.InsertSlotRow(a_dragPayload.slotMask, a_targetRowIndex,
                             a_insertAfter, ResolveNewWorkbenchRowConditionId(),
                             &initialEquippedState);
  }
}

void Menu::DrawVariantWorkbenchPane() {
  auto *localization = ui::Localization::GetSingleton();
  EnsureWorkbenchDerivedState();
  if (!IsWorkbenchFilterSelectionValid()) {
    workbenchFilter_ = {};
    EnsureWorkbenchDerivedState();
  }
  DrawWorkbenchFilterBar();
  ImGui::Separator();
  DrawWorkbenchToolbar();

  const auto &rows = workbench_.GetRows();
  const auto &visibleRowIndices = BuildVisibleWorkbenchRowIndices();
  if (rows.empty()) {
    DrawWorkbenchEmptyState(
        "##variant-workbench-empty", "##empty-workbench-row-target",
        localization->GetCStr("workbench.empty.no_rows"));
    return;
  }

  if (visibleRowIndices.empty()) {
    DrawWorkbenchEmptyState(
        "##variant-workbench-filter-empty", "##filtered-workbench-row-target",
        localization->GetCStr("workbench.empty.no_filtered_rows"));
    return;
  }
  DrawWorkbenchTable(visibleRowIndices);
}
} // namespace sosr
