#pragma once

#include "IncrementalLoader.h"

#include <memory>
#include <unordered_map>

namespace sosr {
struct CatalogCollectionItemNode;
using CatalogCollectionChildren = std::vector<CatalogCollectionItemNode>;

struct CatalogCollectionItemNode {
  RE::FormID formID{0};
  std::string cachedName;
  std::int32_t level{-1};
  std::shared_ptr<const CatalogCollectionChildren> children;

  [[nodiscard]] const CatalogCollectionChildren &GetChildren() const {
    static const CatalogCollectionChildren kEmptyChildren;
    return children ? *children : kEmptyChildren;
  }

  [[nodiscard]] bool HasChildren() const {
    return children && !children->empty();
  }
};

struct CatalogResolvedData {
  std::vector<RE::FormID> armorFormIDs;
  std::shared_ptr<const CatalogCollectionChildren> itemTree;
  std::vector<std::string> pieces;
  std::uint64_t slotMask{0};
  std::string piecesText;
};

struct ResolvedReferenceCollection {
  std::vector<RE::FormID> armorFormIDs;
  std::shared_ptr<const CatalogCollectionChildren> itemTree;
  std::vector<std::string> pieces;
};

struct ArmorMetadata {
  std::string displayName;
  std::string category;
  std::uint64_t slotMask{0};
  std::vector<std::string> slots;
};

struct GearEntry {
  RE::FormID formID;
  std::string id;
  std::string name;
  std::string editorID;
  std::string plugin;
  std::string category;
  std::string slot;
  std::vector<std::string> slots;
  int statValue;
  float weight;
  int value;
  std::vector<std::string> keywords;
  std::string keywordsText;
  std::string searchText;
};

struct OutfitEntry {
  RE::FormID formID;
  std::string id;
  std::string name;
  std::string editorID;
  std::string plugin;
  std::string summary;
  std::shared_ptr<const CatalogResolvedData> resolved;
  std::string searchText;

  [[nodiscard]] const std::vector<RE::FormID> &GetArmorFormIDs() const;
  [[nodiscard]] const std::vector<CatalogCollectionItemNode> &
  GetItemTree() const;
  [[nodiscard]] const std::vector<std::string> &GetPieces() const;
  [[nodiscard]] std::uint64_t GetSlotMask() const;
  [[nodiscard]] std::string_view GetPiecesText() const;
};

struct KitEntry {
  enum class LayoutTargetKind : std::uint8_t { Item, Slot };
  struct LayoutRow {
    LayoutTargetKind targetKind{LayoutTargetKind::Item};
    std::uint64_t targetSlotMask{0};
    std::vector<std::string> overrideIdentifiers;
    bool hideEquipped{false};
  };
  struct Layout {
    std::vector<LayoutRow> rows;
  };

  std::string id;
  std::string key;
  std::string name;
  std::string collection;
  std::string filepath;
  std::string summary;
  std::shared_ptr<const CatalogResolvedData> resolved;
  std::shared_ptr<const Layout> layout;
  std::string searchText;

  [[nodiscard]] const std::vector<RE::FormID> &GetArmorFormIDs() const;
  [[nodiscard]] const std::vector<CatalogCollectionItemNode> &
  GetItemTree() const;
  [[nodiscard]] const std::vector<std::string> &GetPieces() const;
  [[nodiscard]] std::uint64_t GetSlotMask() const;
  [[nodiscard]] std::string_view GetPiecesText() const;
  [[nodiscard]] const Layout *GetLayout() const { return layout.get(); }
};

class EquipmentCatalog {
public:
  enum class RefreshMode : std::uint8_t { Full, KitsOnly };

  static EquipmentCatalog &Get();

  void StartRefreshFromGame(RefreshMode a_mode = RefreshMode::Full);
  bool ContinueRefreshFromGame(double a_maxMillisecondsPerTick = 12.0);
  void RefreshFromGame();
  [[nodiscard]] bool IsRefreshing() const;
  [[nodiscard]] float GetRefreshProgress() const;
  [[nodiscard]] std::string_view GetRefreshStatus() const;

  [[nodiscard]] const std::vector<GearEntry> &GetGear() const { return gear_; }
  [[nodiscard]] const GearEntry *FindGear(RE::FormID a_formID) const;
  [[nodiscard]] const std::vector<OutfitEntry> &GetOutfits() const {
    return outfits_;
  }
  [[nodiscard]] const std::vector<KitEntry> &GetKits() const { return kits_; }
  [[nodiscard]] const OutfitEntry *FindOutfit(RE::FormID a_formID) const;
  [[nodiscard]] std::vector<RE::FormID>
  ResolveArmorFormIDs(RE::FormID a_formID) const;
  [[nodiscard]] std::vector<RE::FormID>
  ResolveArmorFormIDs(const std::vector<RE::FormID> &a_formIDs) const;

  [[nodiscard]] const std::vector<std::string> &GetGearPlugins() const {
    return gearPlugins_;
  }
  [[nodiscard]] const std::vector<std::string> &GetGearSlots() const {
    return gearSlots_;
  }
  [[nodiscard]] const std::vector<std::string> &GetOutfitPlugins() const {
    return outfitPlugins_;
  }
  [[nodiscard]] const std::vector<std::string> &GetKitCollections() const {
    return kitCollections_;
  }

  [[nodiscard]] std::string_view GetSource() const { return source_; }
  [[nodiscard]] std::string_view GetRevision() const { return revision_; }

private:
  struct RefreshState;
  EquipmentCatalog();
  void RebuildDerivedData();

  std::vector<GearEntry> gear_;
  std::vector<OutfitEntry> outfits_;
  std::vector<KitEntry> kits_;
  std::vector<std::string> gearPlugins_;
  std::vector<std::string> gearSlots_;
  std::vector<std::string> outfitPlugins_;
  std::vector<std::string> kitCollections_;
  std::unordered_map<RE::FormID, ArmorMetadata> armorMetadataCache_;
  std::unordered_map<RE::FormID, ResolvedReferenceCollection> leveledListCache_;
  std::unordered_map<RE::FormID, std::size_t> gearIndexByFormID_;
  std::unordered_map<RE::FormID, std::size_t> outfitIndexByFormID_;
  std::string source_;
  std::string revision_;
  std::unique_ptr<RefreshState> refreshState_;
};
} // namespace sosr
