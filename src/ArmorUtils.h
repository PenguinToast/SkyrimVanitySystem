#pragma once

#include <RE/Skyrim.h>

#include <sstream>

namespace sosr::armor {
std::string FormatFormID(RE::FormID a_formID);
std::string GetDisplayName(const RE::TESForm *a_form);
std::string GetEditorID(const RE::TESForm *a_form);
std::string JoinStrings(const std::vector<std::string> &a_values);
std::vector<std::uint64_t> GetAllArmorSlotMasks();
std::uint64_t GetArmorSlotMask(std::uint32_t a_slotNumber);
std::uint32_t GetArmorSlotNumber(std::uint64_t a_slotMask);
std::vector<std::string> GetArmorSlotLabels(std::uint64_t a_slotMask);
std::uint64_t GetArmorAddonSlotMask(const RE::TESObjectARMO *a_armor);
bool HasArmorAddons(const RE::TESObjectARMO *a_armor);
std::vector<std::string>
GetArmorAddonSlotLabels(const RE::TESObjectARMO *a_armor);
std::string GetPluginName(const RE::TESForm *a_form);
std::string GetFormIdentifier(const RE::TESForm *a_form);
std::string GetReplacementIdentifier(const RE::TESObjectARMO *a_armor,
                                     const RE::TESObjectARMA *a_armorAddon);

template <class T = RE::TESForm>
[[nodiscard]] auto LookupByIdentifier(const std::string &a_identifier) -> T * {
  std::istringstream stream(a_identifier);
  std::string plugin;
  std::string localFormIDText;
  if (!std::getline(stream, plugin, '|') ||
      !std::getline(stream, localFormIDText) || plugin.empty() ||
      localFormIDText.empty()) {
    return nullptr;
  }

  RE::FormID localFormID = 0;
  std::istringstream(localFormIDText) >> std::hex >> localFormID;

  auto *dataHandler = RE::TESDataHandler::GetSingleton();
  if (!dataHandler) {
    return nullptr;
  }

  auto *form = dataHandler->LookupForm(localFormID, plugin);
  return form ? form->As<T>() : nullptr;
}
} // namespace sosr::armor
