#include "Menu.h"

#include "ui/workbench/Common.h"

#include <cstring>
namespace sosr {
void Menu::AcceptOverridePayload(int a_targetRowIndex) {
  const auto *payload = ImGui::AcceptDragDropPayload(
      ui::workbench::kVariantItemPayloadType);
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
                 static_cast<std::uint32_t>(DragSourceKind::Catalog) ||
             dragPayload.sourceKind ==
                 static_cast<std::uint32_t>(DragSourceKind::Row)) {
    workbench_.AddCatalogOverride(a_targetRowIndex, dragPayload.formID);
  }
}

bool Menu::ApplyWorkbenchRowDrop(const DraggedEquipmentPayload &a_dragPayload,
                                 const int a_targetRowIndex,
                                 const bool a_insertAfter) {
  if (a_targetRowIndex < 0) {
    if (a_dragPayload.sourceKind ==
        static_cast<std::uint32_t>(DragSourceKind::Catalog)) {
      return workbench_.AddCatalogSelectionAsRows(
          std::vector<RE::FormID>{a_dragPayload.formID},
          ResolveNewWorkbenchRowConditionId());
    }
    if (a_dragPayload.sourceKind ==
        static_cast<std::uint32_t>(DragSourceKind::SlotCatalog)) {
      return workbench_.AddSlotRow(a_dragPayload.slotMask,
                                   ResolveNewWorkbenchRowConditionId());
    }
    return false;
  }

  ApplyRowReorder(a_dragPayload, a_targetRowIndex, a_insertAfter);
  return true;
}

void Menu::ApplyRowReorder(const DraggedEquipmentPayload &a_dragPayload,
                           int a_targetRowIndex, bool a_insertAfter) {
  if (a_dragPayload.sourceKind ==
      static_cast<std::uint32_t>(DragSourceKind::Row)) {
    workbench_.ApplyRowReorder(a_dragPayload.rowIndex, a_targetRowIndex,
                               a_insertAfter);
  } else if (a_dragPayload.sourceKind ==
             static_cast<std::uint32_t>(DragSourceKind::Catalog)) {
    workbench_.InsertCatalogRow(a_dragPayload.formID, a_targetRowIndex,
                                a_insertAfter,
                                ResolveNewWorkbenchRowConditionId());
  } else if (a_dragPayload.sourceKind ==
             static_cast<std::uint32_t>(DragSourceKind::SlotCatalog)) {
    workbench_.InsertSlotRow(a_dragPayload.slotMask, a_targetRowIndex,
                             a_insertAfter,
                             ResolveNewWorkbenchRowConditionId());
  }
}

void Menu::DrawVariantWorkbenchPane() {
  ValidateWorkbenchFilterSelection();
  DrawWorkbenchFilterBar();
  ImGui::Separator();
  DrawWorkbenchToolbar();

  const auto &rows = workbench_.GetRows();
  const auto visibleRowIndices = BuildVisibleWorkbenchRowIndices();
  if (rows.empty()) {
    DrawWorkbenchEmptyState(
        "##variant-workbench-empty", "##empty-workbench-row-target",
        "No workbench rows yet. Drag equipment or slot entries to the "
        "Equipped column to create one.");
    return;
  }

  if (visibleRowIndices.empty()) {
    DrawWorkbenchEmptyState(
        "##variant-workbench-filter-empty", "##filtered-workbench-row-target",
        "No workbench rows match the current filter. Drag equipment or "
        "equipment slots here to create one.");
    return;
  }
  DrawWorkbenchTable(visibleRowIndices);
}
} // namespace sosr
