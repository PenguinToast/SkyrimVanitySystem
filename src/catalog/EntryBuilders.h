#pragma once

#include "EquipmentCatalog.h"

#include <filesystem>
#include <optional>
#include <unordered_map>

namespace sosr::catalog {
[[nodiscard]] const std::filesystem::path &GetModexKitPath();

[[nodiscard]] std::optional<GearEntry> BuildGearEntry(
    RE::TESObjectARMO *a_armor,
    std::unordered_map<RE::FormID, ArmorMetadata> &a_armorMetadataCache);
[[nodiscard]] std::optional<OutfitEntry> BuildOutfitEntry(
    const RE::BGSOutfit *a_outfit,
    std::unordered_map<RE::FormID, ResolvedReferenceCollection>
        &a_leveledListCache,
    std::unordered_map<RE::FormID, ArmorMetadata> &a_armorMetadataCache);
[[nodiscard]] std::optional<KitEntry> BuildKitEntry(
    const std::filesystem::path &a_path,
    std::unordered_map<RE::FormID, ArmorMetadata> &a_armorMetadataCache);
} // namespace sosr::catalog
