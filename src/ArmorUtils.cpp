#include "ArmorUtils.h"
#include "ui/Localization.h"

#include <algorithm>
#include <cstdio>
#include <format>
#include <windows.h>

namespace {
using BipedSlot = RE::BGSBipedObjectForm::BipedObjectSlot;

constexpr std::array<std::pair<BipedSlot, std::string_view>, 32>
    kArmorSlotNames{
        {{BipedSlot::kHead, "30 - Head"},
         {BipedSlot::kHair, "31 - Hair"},
         {BipedSlot::kBody, "32 - Body"},
         {BipedSlot::kHands, "33 - Hands"},
         {BipedSlot::kForearms, "34 - Forearms"},
         {BipedSlot::kAmulet, "35 - Amulet"},
         {BipedSlot::kRing, "36 - Ring"},
         {BipedSlot::kFeet, "37 - Feet"},
         {BipedSlot::kCalves, "38 - Calves"},
         {BipedSlot::kShield, "39 - Shield"},
         {BipedSlot::kTail, "40 - Tail"},
         {BipedSlot::kLongHair, "41 - Long Hair"},
         {BipedSlot::kCirclet, "42 - Circlet"},
         {BipedSlot::kEars, "43 - Ears"},
         {BipedSlot::kModMouth, "44 - Mod Mouth"},
         {BipedSlot::kModNeck, "45 - Mod Neck"},
         {BipedSlot::kModChestPrimary, "46 - Mod Chest Primary"},
         {BipedSlot::kModBack, "47 - Mod Back"},
         {BipedSlot::kModMisc1, "48 - Mod Misc1"},
         {BipedSlot::kModPelvisPrimary, "49 - Mod Pelvis Primary"},
         {BipedSlot::kDecapitateHead, "50 - Decapitate Head"},
         {BipedSlot::kDecapitate, "51 - Decapitate"},
         {BipedSlot::kModPelvisSecondary, "52 - Mod Pelvis Secondary"},
         {BipedSlot::kModLegRight, "53 - Mod Leg Right"},
         {BipedSlot::kModLegLeft, "54 - Mod Leg Left"},
         {BipedSlot::kModFaceJewelry, "55 - Mod Face Jewelry"},
         {BipedSlot::kModChestSecondary, "56 - Mod Chest Secondary"},
         {BipedSlot::kModShoulder, "57 - Mod Shoulder"},
         {BipedSlot::kModArmLeft, "58 - Mod Arm Left"},
         {BipedSlot::kModArmRight, "59 - Mod Arm Right"},
         {BipedSlot::kModMisc2, "60 - Mod Misc2"},
         {BipedSlot::kFX01, "61 - FX01"}}};

std::string CopyCString(const char *a_text) {
  if (!a_text || a_text[0] == '\0') {
    return {};
  }
  return a_text;
}

using GetPo3EditorIDFn = const char *(*)(std::uint32_t);

std::string GetPo3EditorID(RE::FormID a_formID) {
  static auto *tweaks = GetModuleHandleA("po3_Tweaks");
  static auto *function = reinterpret_cast<GetPo3EditorIDFn>(
      tweaks ? GetProcAddress(tweaks, "GetFormEditorID") : nullptr);
  return function ? CopyCString(function(a_formID)) : std::string{};
}
} // namespace

namespace sosr::armor {
std::vector<std::uint64_t> GetAllArmorSlotMasks() {
  std::vector<std::uint64_t> masks;
  masks.reserve(kArmorSlotNames.size());
  for (const auto &[slot, _] : kArmorSlotNames) {
    masks.push_back(static_cast<std::uint64_t>(std::to_underlying(slot)));
  }
  return masks;
}

std::uint64_t GetArmorSlotMask(const std::uint32_t a_slotNumber) {
  for (std::size_t index = 0; index < kArmorSlotNames.size(); ++index) {
    if ((index + 30) == a_slotNumber) {
      return static_cast<std::uint64_t>(
          std::to_underlying(kArmorSlotNames[index].first));
    }
  }

  return 0;
}

std::uint32_t GetArmorSlotNumber(const std::uint64_t a_slotMask) {
  for (std::size_t index = 0; index < kArmorSlotNames.size(); ++index) {
    if (static_cast<std::uint64_t>(
            std::to_underlying(kArmorSlotNames[index].first)) == a_slotMask) {
      return static_cast<std::uint32_t>(index + 30);
    }
  }

  return 0;
}

std::string FormatFormID(RE::FormID a_formID) {
  std::array<char, 16> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%08X", a_formID);
  return buffer.data();
}

std::string GetDisplayName(const RE::TESForm *a_form) {
  const auto *localization = ui::Localization::GetSingleton();
  if (!a_form) {
    return std::string(localization->Get("common.unknown"));
  }

  auto name = CopyCString(a_form->GetName());
  if (!name.empty()) {
    return name;
  }

  auto editorID = GetEditorID(a_form);
  if (!editorID.empty()) {
    return editorID;
  }

  const auto formID = FormatFormID(a_form->GetFormID());
  return std::vformat(std::string(localization->Get("common.form_fallback")),
                      std::make_format_args(formID));
}

std::string GetEditorID(const RE::TESForm *a_form) {
  if (!a_form) {
    return {};
  }

  auto editorID = CopyCString(a_form->GetFormEditorID());
  if (editorID.empty()) {
    editorID = GetPo3EditorID(a_form->GetFormID());
  }
  return editorID;
}

std::string JoinStrings(const std::vector<std::string> &a_values) {
  std::string output;
  for (const auto &value : a_values) {
    if (!output.empty()) {
      output.append(", ");
    }
    output.append(value);
  }
  return output;
}

std::vector<std::string> GetArmorSlotLabels(std::uint64_t a_slotMask) {
  std::vector<std::string> labels;
  for (const auto &[slot, label] : kArmorSlotNames) {
    if ((a_slotMask & static_cast<std::uint64_t>(std::to_underlying(slot))) !=
        0) {
      labels.emplace_back(label);
    }
  }

  if (labels.empty()) {
    labels.emplace_back(ui::Localization::GetSingleton()->Get("common.none"));
  }

  return labels;
}

std::uint64_t GetArmorAddonSlotMask(const RE::TESObjectARMO *a_armor) {
  if (!a_armor) {
    return 0;
  }

  std::uint64_t slotMask = 0;
  for (const auto *armorAddon : a_armor->armorAddons) {
    if (!armorAddon) {
      continue;
    }

    slotMask |= armorAddon->GetSlotMask().underlying();
  }

  return slotMask;
}

bool HasArmorAddons(const RE::TESObjectARMO *a_armor) {
  if (!a_armor) {
    return false;
  }

  return std::ranges::any_of(a_armor->armorAddons,
                             [](const auto *a_armorAddon) {
                               return a_armorAddon != nullptr;
                             });
}

std::vector<std::string>
GetArmorAddonSlotLabels(const RE::TESObjectARMO *a_armor) {
  return GetArmorSlotLabels(GetArmorAddonSlotMask(a_armor));
}

std::string GetPluginName(const RE::TESForm *a_form) {
  if (!a_form) {
    return {};
  }

  const auto *file = a_form->GetFile(0);
  if (!file) {
    return {};
  }

  return std::string(file->GetFilename());
}

std::string GetFormIdentifier(const RE::TESForm *a_form) {
  if (!a_form) {
    return {};
  }

  const auto plugin = GetPluginName(a_form);
  if (plugin.empty()) {
    return {};
  }

  return plugin + "|" + FormatFormID(a_form->GetLocalFormID());
}

std::string GetReplacementIdentifier(const RE::TESObjectARMO *a_armor,
                                     const RE::TESObjectARMA *a_armorAddon) {
  const auto armorIdentifier = GetFormIdentifier(a_armor);
  const auto armorAddonIdentifier = GetFormIdentifier(a_armorAddon);
  if (armorIdentifier.empty() || armorAddonIdentifier.empty()) {
    return {};
  }

  return armorIdentifier + "|" + armorAddonIdentifier;
}
} // namespace sosr::armor
