#include "Menu.h"

#include "ui/workbench/Common.h"

namespace sosr {
ui::components::EquipmentWidgetResult
Menu::DrawCatalogDragWidget(const workbench::EquipmentWidgetItem &a_item,
                            const DragSourceKind a_sourceKind) {
  const auto widgetResult =
      ui::components::DrawEquipmentWidget(a_item.key.c_str(), a_item);
  if (!a_item.SupportsDavArmorReplacement() || !ImGui::BeginDragDropSource()) {
    return widgetResult;
  }

  DraggedEquipmentPayload payload{};
  payload.sourceKind = static_cast<std::uint32_t>(a_sourceKind);
  payload.rowIndex = -1;
  payload.itemIndex = -1;
  payload.formID = a_item.formID;
  payload.slotMask = a_item.slotMask;
  ImGui::SetDragDropPayload(ui::workbench::kVariantItemPayloadType, &payload,
                            sizeof(payload));
  ImGui::TextUnformatted(a_item.name.c_str());
  ImGui::Text("%s", a_item.slotText.c_str());
  ImGui::EndDragDropSource();
  return widgetResult;
}

Menu *Menu::GetSingleton() {
  static Menu singleton;
  return std::addressof(singleton);
}
} // namespace sosr
