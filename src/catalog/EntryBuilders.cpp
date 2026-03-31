#include "catalog/EntryBuilders.h"
#include "catalog/KitLayoutMetadata.h"

#include "ArmorUtils.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_set>

namespace {
const auto kModexKitPath =
    std::filesystem::path("data") / "interface" / "modex" / "user" / "kits";

template <class Strings> std::string JoinStrings(const Strings &a_strings) {
  std::string output;
  for (const auto &value : a_strings) {
    if (!output.empty()) {
      output.append(", ");
    }
    output.append(value);
  }
  return output;
}

void SortUniqueStrings(std::vector<std::string> &a_values) {
  std::ranges::sort(a_values);
  a_values.erase(std::unique(a_values.begin(), a_values.end()), a_values.end());
}

std::string CopyCString(const char *a_text) {
  if (!a_text || a_text[0] == '\0') {
    return {};
  }
  return a_text;
}

std::string GetName(const RE::TESForm *a_form) {
  return a_form ? CopyCString(a_form->GetName()) : std::string{};
}

std::string BuildEntryID(const RE::TESForm *a_form) {
  if (!a_form) {
    return "unknown|00000000";
  }

  const auto plugin = sosr::armor::GetPluginName(a_form);
  return (plugin.empty() ? std::string{"Unknown"} : plugin) + "|" +
         sosr::armor::FormatFormID(a_form->GetLocalFormID());
}

std::string GetArmorCategory(const RE::TESObjectARMO *a_armor) {
  if (!a_armor) {
    return "Armor";
  }

  switch (a_armor->GetArmorType()) {
  case RE::TESObjectARMO::ArmorType::kLightArmor:
    return "Light Armor";
  case RE::TESObjectARMO::ArmorType::kHeavyArmor:
    return "Heavy Armor";
  case RE::TESObjectARMO::ArmorType::kClothing:
    return "Clothing";
  default:
    return "Armor";
  }
}

std::vector<std::string> GetArmorSlots(const RE::TESObjectARMO *a_armor) {
  return sosr::armor::GetArmorSlotLabels(
      a_armor ? a_armor->GetSlotMask().underlying() : 0);
}

const sosr::ArmorMetadata &GetOrBuildArmorMetadata(
    const RE::TESObjectARMO *a_armor,
    std::unordered_map<RE::FormID, sosr::ArmorMetadata> &a_cache) {
  static const sosr::ArmorMetadata kEmptyMetadata{};
  if (!a_armor) {
    return kEmptyMetadata;
  }

  const auto formID = a_armor->GetFormID();
  const auto it = a_cache.find(formID);
  if (it != a_cache.end()) {
    return it->second;
  }

  sosr::ArmorMetadata metadata{};
  metadata.displayName = sosr::armor::GetDisplayName(a_armor);
  metadata.category = GetArmorCategory(a_armor);
  metadata.slotMask = a_armor->GetSlotMask().underlying();
  metadata.slots = sosr::armor::GetArmorSlotLabels(metadata.slotMask);
  return a_cache.emplace(formID, std::move(metadata)).first->second;
}

std::string GetPrimaryArmorSlot(const RE::TESObjectARMO *a_armor) {
  auto slots = GetArmorSlots(a_armor);
  return slots.empty() ? std::string{"None"} : std::move(slots.front());
}

sosr::CatalogCollectionItemNode BuildCachedCollectionNode(
    const RE::TESForm *a_form, const std::int32_t a_level,
    std::shared_ptr<const sosr::CatalogCollectionChildren> a_children = {}) {
  sosr::CatalogCollectionItemNode node{};
  node.formID = a_form ? a_form->GetFormID() : 0;
  node.level = a_level;
  node.children = std::move(a_children);

  if (!a_form || a_form->As<RE::TESObjectARMO>()) {
    return node;
  }

  node.cachedName = sosr::armor::GetDisplayName(a_form);
  return node;
}

std::shared_ptr<const sosr::CatalogCollectionChildren>
MakeSharedChildren(std::vector<sosr::CatalogCollectionItemNode> a_children) {
  if (a_children.empty()) {
    return {};
  }
  return std::make_shared<const sosr::CatalogCollectionChildren>(
      std::move(a_children));
}

template <class T> std::vector<std::string> GetKeywords(const T *a_form) {
  std::vector<std::string> keywords;
  if (!a_form) {
    return keywords;
  }

  const auto *keywordForm = static_cast<const RE::BGSKeywordForm *>(a_form);
  keywords.reserve(keywordForm->GetNumKeywords());
  for (std::uint32_t index = 0; index < keywordForm->GetNumKeywords();
       ++index) {
    const auto keyword = keywordForm->GetKeywordAt(index);
    if (!keyword || !keyword.value()) {
      continue;
    }

    auto text = sosr::armor::GetEditorID(keyword.value());
    if (text.empty()) {
      text = GetName(keyword.value());
    }

    if (!text.empty()) {
      keywords.push_back(std::move(text));
    }
  }

  SortUniqueStrings(keywords);
  return keywords;
}

void AppendSearchToken(std::string &a_searchText, std::string_view a_token) {
  if (a_token.empty()) {
    return;
  }

  if (!a_searchText.empty()) {
    a_searchText.push_back(' ');
  }
  a_searchText.append(a_token);
}

std::string BuildGearSearchText(const sosr::GearEntry &a_entry) {
  std::string text;
  text.reserve(256);

  AppendSearchToken(text, a_entry.name);
  AppendSearchToken(text, a_entry.editorID);
  AppendSearchToken(text, a_entry.plugin);
  AppendSearchToken(text, a_entry.category);
  AppendSearchToken(text, a_entry.slot);
  AppendSearchToken(text, a_entry.keywordsText);

  return text;
}

std::string BuildOutfitSearchText(const sosr::OutfitEntry &a_entry) {
  std::string text;
  text.reserve(256);

  AppendSearchToken(text, a_entry.name);
  AppendSearchToken(text, a_entry.editorID);
  AppendSearchToken(text, a_entry.plugin);
  AppendSearchToken(text, a_entry.summary);
  AppendSearchToken(text, a_entry.GetPiecesText());

  return text;
}

std::string BuildKitSearchText(const sosr::KitEntry &a_entry) {
  std::string text;
  text.reserve(256);

  AppendSearchToken(text, a_entry.name);
  AppendSearchToken(text, a_entry.collection);
  AppendSearchToken(text, a_entry.summary);
  AppendSearchToken(text, a_entry.GetPiecesText());

  return text;
}

struct OutfitDescription {
  std::vector<RE::FormID> armorFormIDs;
  std::vector<sosr::CatalogCollectionItemNode> itemTree;
  std::vector<std::string> pieces;
  std::size_t armorCount{0};
};

struct KitDescription {
  std::vector<RE::FormID> armorFormIDs;
  std::vector<sosr::CatalogCollectionItemNode> itemTree;
  std::vector<std::string> pieces;
  bool hasMissingItems{false};
};

void AppendCachedLeveledListDescription(
    const sosr::ResolvedReferenceCollection &a_cache,
    OutfitDescription &a_description,
    std::unordered_set<RE::FormID> &a_seenArmor) {
  a_description.pieces.insert(a_description.pieces.end(),
                              a_cache.pieces.begin(), a_cache.pieces.end());
  for (const auto formID : a_cache.armorFormIDs) {
    if (a_seenArmor.insert(formID).second) {
      a_description.armorFormIDs.push_back(formID);
      ++a_description.armorCount;
    }
  }
}

auto GetOrBuildLeveledListCache(
    const RE::FormID a_formID, const RE::TESLeveledList *a_list,
    std::unordered_map<RE::FormID, sosr::ResolvedReferenceCollection> &a_cache,
    std::unordered_set<RE::FormID> &a_activeLeveledLists,
    std::unordered_map<RE::FormID, sosr::ArmorMetadata> &a_armorMetadataCache)
    -> const sosr::ResolvedReferenceCollection & {
  const auto cacheIt = a_cache.find(a_formID);
  if (cacheIt != a_cache.end()) {
    return cacheIt->second;
  }

  sosr::ResolvedReferenceCollection built;
  std::vector<sosr::CatalogCollectionItemNode> builtItemTree;
  if (!a_activeLeveledLists.insert(a_formID).second) {
    return a_cache.emplace(a_formID, std::move(built)).first->second;
  }

  std::unordered_set<RE::FormID> seenArmorForms;
  std::unordered_set<RE::FormID> seenDirectArmorChildren;
  for (const auto &entry : a_list->entries) {
    const auto *form = entry.form;
    if (!form || form->IsDeleted() || form->IsIgnored()) {
      continue;
    }

    if (const auto *armor = form->As<RE::TESObjectARMO>()) {
      const auto &metadata =
          GetOrBuildArmorMetadata(armor, a_armorMetadataCache);
      if (seenDirectArmorChildren.insert(armor->GetFormID()).second) {
        auto &child = builtItemTree.emplace_back();
        child.formID = armor->GetFormID();
        child.level = entry.level;
      }
      built.pieces.push_back(metadata.displayName);

      if (seenArmorForms.insert(armor->GetFormID()).second) {
        built.armorFormIDs.push_back(armor->GetFormID());
      }
      continue;
    }

    if (form->GetFormType() == RE::FormType::LeveledItem) {
      const auto *nestedList = form->As<RE::TESLeveledList>();
      if (!nestedList) {
        continue;
      }

      const auto &nestedCache = GetOrBuildLeveledListCache(
          form->GetFormID(), nestedList, a_cache, a_activeLeveledLists,
          a_armorMetadataCache);
      builtItemTree.push_back(
          BuildCachedCollectionNode(form, entry.level, nestedCache.itemTree));
      built.pieces.insert(built.pieces.end(), nestedCache.pieces.begin(),
                          nestedCache.pieces.end());
      for (const auto formID : nestedCache.armorFormIDs) {
        if (seenArmorForms.insert(formID).second) {
          built.armorFormIDs.push_back(formID);
        }
      }
      continue;
    }

    builtItemTree.push_back(BuildCachedCollectionNode(form, entry.level));
    built.pieces.push_back(sosr::armor::GetDisplayName(form));
  }

  a_activeLeveledLists.erase(a_formID);
  SortUniqueStrings(built.pieces);
  built.itemTree = MakeSharedChildren(std::move(builtItemTree));
  return a_cache.emplace(a_formID, std::move(built)).first->second;
}

void AccumulateArmorDescription(
    const RE::TESObjectARMO *a_armor, OutfitDescription &a_description,
    std::unordered_set<RE::FormID> &a_seenArmor,
    std::unordered_map<RE::FormID, sosr::ArmorMetadata> &a_armorMetadataCache) {
  if (!a_armor) {
    return;
  }

  a_description.pieces.push_back(
      GetOrBuildArmorMetadata(a_armor, a_armorMetadataCache).displayName);

  if (!a_seenArmor.insert(a_armor->GetFormID()).second) {
    return;
  }

  a_description.armorFormIDs.push_back(a_armor->GetFormID());
  ++a_description.armorCount;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
auto BuildOutfitItemNode(
    const RE::TESForm *a_item, OutfitDescription &a_description,
    std::unordered_set<RE::FormID> &a_seenArmor,
    std::unordered_set<RE::FormID> &a_activeLeveledLists,
    std::unordered_map<RE::FormID, sosr::ResolvedReferenceCollection>
        &a_leveledListCache,
    std::unordered_map<RE::FormID, sosr::ArmorMetadata> &a_armorMetadataCache)
    -> std::optional<sosr::CatalogCollectionItemNode> {
  if (!a_item || a_item->IsDeleted() || a_item->IsIgnored()) {
    return std::nullopt;
  }

  if (const auto *armor = a_item->As<RE::TESObjectARMO>()) {
    AccumulateArmorDescription(armor, a_description, a_seenArmor,
                               a_armorMetadataCache);
    return sosr::CatalogCollectionItemNode{.formID = armor->GetFormID()};
  }

  if (a_item->GetFormType() == RE::FormType::LeveledItem) {
    const auto *list = a_item->As<RE::TESLeveledList>();
    if (!list) {
      return std::nullopt;
    }

    const auto &cache = GetOrBuildLeveledListCache(
        a_item->GetFormID(), list, a_leveledListCache, a_activeLeveledLists,
        a_armorMetadataCache);
    AppendCachedLeveledListDescription(cache, a_description, a_seenArmor);
    return BuildCachedCollectionNode(a_item, -1, cache.itemTree);
  }

  a_description.pieces.push_back(sosr::armor::GetDisplayName(a_item));
  return BuildCachedCollectionNode(a_item, -1);
}
// NOLINTEND(bugprone-easily-swappable-parameters)

OutfitDescription DescribeOutfit(
    const RE::BGSOutfit *a_outfit,
    std::unordered_map<RE::FormID, sosr::ResolvedReferenceCollection>
        &a_leveledListCache,
    std::unordered_map<RE::FormID, sosr::ArmorMetadata> &a_armorMetadataCache) {
  OutfitDescription description;
  if (!a_outfit) {
    return description;
  }

  std::unordered_set<RE::FormID> seenArmorForms;
  std::unordered_set<RE::FormID> activeLeveledLists;

  a_outfit->ForEachItem([&](RE::TESForm *a_item) {
    if (const auto node = BuildOutfitItemNode(
            a_item, description, seenArmorForms, activeLeveledLists,
            a_leveledListCache, a_armorMetadataCache)) {
      description.itemTree.push_back(*node);
    }

    return RE::BSContainer::ForEachResult::kContinue;
  });

  SortUniqueStrings(description.pieces);
  return description;
}

std::shared_ptr<const sosr::CatalogResolvedData> FinalizeResolvedData(
    std::vector<RE::FormID> a_armorFormIDs,
    std::vector<sosr::CatalogCollectionItemNode> a_itemTree,
    std::vector<std::string> a_pieces,
    std::unordered_map<RE::FormID, sosr::ArmorMetadata> &a_armorMetadataCache) {
  auto resolved = std::make_shared<sosr::CatalogResolvedData>();
  resolved->armorFormIDs = std::move(a_armorFormIDs);
  resolved->itemTree = MakeSharedChildren(std::move(a_itemTree));
  resolved->pieces = std::move(a_pieces);

  for (const auto formID : resolved->armorFormIDs) {
    const auto *armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(formID);
    if (!armor) {
      continue;
    }

    const auto metadataIt = a_armorMetadataCache.find(formID);
    const auto &metadata =
        metadataIt != a_armorMetadataCache.end()
            ? metadataIt->second
            : GetOrBuildArmorMetadata(armor, a_armorMetadataCache);

    resolved->slotMask |= metadata.slotMask;
  }

  SortUniqueStrings(resolved->pieces);
  resolved->piecesText = JoinStrings(resolved->pieces);
  return resolved;
}

std::string BuildOutfitSummary(const OutfitDescription &a_description) {
  const auto totalPieces = a_description.pieces.size();
  if (totalPieces == 0) {
    return "Empty outfit.";
  }

  std::string summary = "Contains " + std::to_string(totalPieces) + " item";
  if (totalPieces != 1) {
    summary.push_back('s');
  }

  if (a_description.armorCount > 0) {
    summary.append(": ");
    summary.append(std::to_string(a_description.armorCount));
    summary.append(" armor");
  } else {
    summary.push_back('.');
  }

  return summary;
}

nlohmann::json OpenJsonFile(const std::filesystem::path &a_path) {
  if (!std::filesystem::exists(a_path)) {
    return nlohmann::json::object();
  }

  try {
    std::ifstream file(a_path);
    if (!file.is_open()) {
      return nlohmann::json::object();
    }

    nlohmann::json data;
    file >> data;
    return data;
  } catch (...) {
    return nlohmann::json::object();
  }
}

KitDescription DescribeKitItems(
    const nlohmann::json &a_items,
    std::unordered_map<RE::FormID, sosr::ArmorMetadata> &a_armorMetadataCache) {
  KitDescription description;
  if (!a_items.is_object()) {
    return description;
  }

  std::unordered_set<RE::FormID> seenArmorForms;
  for (const auto &[editorID, _] : a_items.items()) {
    auto *form = RE::TESForm::LookupByEditorID(editorID);
    if (!form) {
      description.hasMissingItems = true;
      break;
    }

    const auto *armor = form->As<RE::TESObjectARMO>();
    if (!armor) {
      continue;
    }

    if (!seenArmorForms.insert(armor->GetFormID()).second) {
      continue;
    }

    description.armorFormIDs.push_back(armor->GetFormID());
    const auto &metadata = GetOrBuildArmorMetadata(armor, a_armorMetadataCache);
    description.pieces.push_back(metadata.displayName);
    auto &itemNode = description.itemTree.emplace_back();
    itemNode.formID = armor->GetFormID();
  }

  SortUniqueStrings(description.pieces);
  return description;
}

std::string BuildKitSummary(const KitDescription &a_description) {
  const auto armorCount = a_description.armorFormIDs.size();
  if (armorCount == 0) {
    return "No armor items.";
  }

  std::string summary =
      "Contains " + std::to_string(armorCount) + " armor item";
  if (armorCount != 1) {
    summary.push_back('s');
  }
  return summary;
}
} // namespace

namespace sosr::catalog {
const std::filesystem::path &GetModexKitPath() { return kModexKitPath; }

std::optional<GearEntry> BuildGearEntry(
    RE::TESObjectARMO *a_armor,
    std::unordered_map<RE::FormID, sosr::ArmorMetadata> &a_armorMetadataCache) {
  if (!a_armor || a_armor->IsDeleted() || a_armor->IsIgnored() ||
      !a_armor->GetFile(0)) {
    return std::nullopt;
  }

  const auto editorID = sosr::armor::GetEditorID(a_armor);
  const auto &metadata = GetOrBuildArmorMetadata(a_armor, a_armorMetadataCache);
  auto displayName = metadata.displayName;
  if (displayName.empty()) {
    displayName = editorID;
  }
  if (displayName.empty()) {
    return std::nullopt;
  }

  GearEntry entry{};
  entry.formID = a_armor->GetFormID();
  entry.id = BuildEntryID(a_armor);
  entry.name = std::move(displayName);
  entry.editorID = editorID;
  entry.plugin = sosr::armor::GetPluginName(a_armor);
  entry.category = metadata.category;
  entry.slots = metadata.slots;
  entry.slot = entry.slots.empty() ? std::string{} : entry.slots.front();
  entry.statValue = static_cast<int>(a_armor->GetArmorRating());
  entry.weight = a_armor->GetWeight();
  entry.value = a_armor->GetGoldValue();
  entry.keywords.insert(entry.keywords.end(), entry.slots.begin(),
                        entry.slots.end());
  entry.keywords.push_back(entry.category);
  SortUniqueStrings(entry.keywords);
  entry.keywordsText = JoinStrings(entry.keywords);
  entry.searchText = BuildGearSearchText(entry);
  return entry;
}

std::optional<OutfitEntry> BuildOutfitEntry(
    const RE::BGSOutfit *a_outfit,
    std::unordered_map<RE::FormID, ResolvedReferenceCollection>
        &a_leveledListCache,
    std::unordered_map<RE::FormID, sosr::ArmorMetadata> &a_armorMetadataCache) {
  if (!a_outfit || a_outfit->IsDeleted() || a_outfit->IsIgnored() ||
      !a_outfit->GetFile(0)) {
    return std::nullopt;
  }

  auto description =
      DescribeOutfit(a_outfit, a_leveledListCache, a_armorMetadataCache);
  if (description.pieces.empty()) {
    return std::nullopt;
  }

  auto editorID = sosr::armor::GetEditorID(a_outfit);
  auto displayName = GetName(a_outfit);
  if (displayName.empty()) {
    displayName = editorID;
  }
  if (displayName.empty()) {
    displayName = "Form " + sosr::armor::FormatFormID(a_outfit->GetFormID());
  }

  OutfitEntry entry{};
  entry.formID = a_outfit->GetFormID();
  entry.id = BuildEntryID(a_outfit);
  entry.name = std::move(displayName);
  entry.editorID = std::move(editorID);
  entry.plugin = sosr::armor::GetPluginName(a_outfit);
  entry.summary = BuildOutfitSummary(description);
  entry.resolved = FinalizeResolvedData(
      std::move(description.armorFormIDs), std::move(description.itemTree),
      std::move(description.pieces), a_armorMetadataCache);
  entry.searchText = BuildOutfitSearchText(entry);
  return entry;
}

std::optional<KitEntry> BuildKitEntry(
    const std::filesystem::path &a_path,
    std::unordered_map<RE::FormID, sosr::ArmorMetadata> &a_armorMetadataCache) {
  if (!std::filesystem::is_regular_file(a_path) ||
      a_path.extension() != ".json") {
    return std::nullopt;
  }

  const auto relativePath = a_path.lexically_relative(kModexKitPath);
  if (relativePath.string().starts_with("..")) {
    return std::nullopt;
  }

  const auto json = OpenJsonFile(a_path);
  if (!json.is_object() || json.size() != 1) {
    return std::nullopt;
  }

  const auto kitItem = json.items().begin();
  const auto &kitData = kitItem.value();
  if (!kitData.is_object()) {
    return std::nullopt;
  }

  auto description = DescribeKitItems(
      kitData.value("Items", nlohmann::json::object()), a_armorMetadataCache);
  std::shared_ptr<const KitEntry::Layout> layout;
  if (const auto svsMetadataIt =
          kitData.find(std::string(sosr::catalog::kSvsKitMetadataKey));
      svsMetadataIt != kitData.end()) {
    if (auto parsedLayout = ParseKitLayout(*svsMetadataIt);
        parsedLayout.has_value()) {
      layout =
          std::make_shared<const KitEntry::Layout>(std::move(*parsedLayout));
    }
  }

  if (description.hasMissingItems ||
      (description.armorFormIDs.empty() && !layout)) {
    return std::nullopt;
  }

  auto name = kitItem.key();
  if (name.empty()) {
    name = a_path.stem().string();
  }

  KitEntry entry{};
  entry.id = "kit:" + relativePath.generic_string();
  entry.key = relativePath.generic_string();
  entry.name = std::move(name);
  entry.collection = relativePath.parent_path().generic_string();
  entry.filepath = a_path.string();
  entry.summary = BuildKitSummary(description);
  entry.resolved = FinalizeResolvedData(
      std::move(description.armorFormIDs), std::move(description.itemTree),
      std::move(description.pieces), a_armorMetadataCache);
  entry.layout = std::move(layout);
  entry.searchText = BuildKitSearchText(entry);
  return entry;
}
} // namespace sosr::catalog
