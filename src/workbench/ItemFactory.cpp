#include "workbench/ItemFactory.h"

#include "ArmorUtils.h"

namespace {
std::string BuildSlotKey(const std::uint64_t a_slotMask) {
  const auto slotNumber = sosr::armor::GetArmorSlotNumber(a_slotMask);
  if (slotNumber == 0) {
    return {};
  }

  return "slot:" + std::to_string(slotNumber);
}
} // namespace

namespace sosr::workbench {
bool BuildCatalogItem(const RE::FormID a_formID, EquipmentWidgetItem &a_item) {
  const auto *form = RE::TESForm::LookupByID(a_formID);
  if (!form) {
    return false;
  }

  a_item = {};
  a_item.kind = EquipmentWidgetItemKind::Armor;
  a_item.formID = form->GetFormID();
  a_item.key = "form:" + armor::FormatFormID(form->GetFormID());
  a_item.name = armor::GetDisplayName(form);

  if (const auto *armorForm = form->As<RE::TESObjectARMO>()) {
    a_item.slotMask = armorForm->GetSlotMask().underlying();
    a_item.slotText =
        armor::JoinStrings(armor::GetArmorSlotLabels(a_item.slotMask));
    return true;
  }

  return false;
}

bool BuildSlotItem(const std::uint64_t a_slotMask,
                   EquipmentWidgetItem &a_item) {
  const auto slotNumber = armor::GetArmorSlotNumber(a_slotMask);
  if (slotNumber == 0) {
    return false;
  }

  a_item = {};
  a_item.kind = EquipmentWidgetItemKind::Slot;
  a_item.formID = 0;
  a_item.key = BuildSlotKey(a_slotMask);
  a_item.name = armor::JoinStrings(armor::GetArmorSlotLabels(a_slotMask));
  a_item.slotText = "Equipment slot";
  a_item.slotMask = a_slotMask;
  return true;
}
} // namespace sosr::workbench
