#include "VariantWorkbench.h"

#include "ArmorUtils.h"
#include "EquipmentCatalog.h"
#include "workbench/ItemFactory.h"

#include <algorithm>
#include <array>
#include <limits>
#include <unordered_set>

namespace sosr::workbench {
namespace {
using BipedSlot = RE::BGSBipedObjectForm::BipedObjectSlot;

constexpr std::uint64_t SlotBit(const BipedSlot a_slot) {
  return static_cast<std::uint64_t>(std::to_underlying(a_slot));
}

std::string BuildSlotKey(const std::uint64_t a_slotMask) {
  const auto slotNumber = sosr::armor::GetArmorSlotNumber(a_slotMask);
  if (slotNumber == 0) {
    return {};
  }

  return "slot:" + std::to_string(slotNumber);
}

std::string BuildArmorSourceKey(const RE::FormID a_formID) {
  return "armor:" + sosr::armor::FormatFormID(a_formID);
}

std::string BuildRowKey(const std::string_view a_sourceKey,
                        const std::optional<std::string> &a_conditionId) {
  std::string key(a_sourceKey);
  key.append("|condition:");
  if (a_conditionId.has_value()) {
    key.append(*a_conditionId);
  } else {
    key.append("null");
  }
  return key;
}

void UpdateRowIdentity(VariantWorkbenchRow &a_row) {
  a_row.key = BuildRowKey(a_row.sourceKey, a_row.conditionId);
  a_row.equipped.key = a_row.key;
}

int ScorePreferredTargetSlots(
    const std::uint64_t a_itemMask, const std::uint64_t a_targetMask,
    const std::initializer_list<BipedSlot> &a_sourceSlots,
    const std::initializer_list<BipedSlot> &a_preferredTargetSlots) {
  for (const auto sourceSlot : a_sourceSlots) {
    if ((a_itemMask & SlotBit(sourceSlot)) == 0) {
      continue;
    }

    int score = static_cast<int>(a_preferredTargetSlots.size());
    for (const auto targetSlot : a_preferredTargetSlots) {
      if ((a_targetMask & SlotBit(targetSlot)) != 0) {
        return score;
      }
      --score;
    }
  }

  return -1;
}

int ScoreFallbackTargetRow(const std::uint64_t a_itemMask,
                           const std::uint64_t a_targetMask) {
  int bestScore = -1;

  const std::array scores{
      ScorePreferredTargetSlots(a_itemMask, a_targetMask, {BipedSlot::kCirclet},
                                {BipedSlot::kHead, BipedSlot::kHair,
                                 BipedSlot::kLongHair, BipedSlot::kEars}),
      ScorePreferredTargetSlots(a_itemMask, a_targetMask, {BipedSlot::kHead},
                                {BipedSlot::kCirclet, BipedSlot::kHair,
                                 BipedSlot::kLongHair, BipedSlot::kEars}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask,
          {BipedSlot::kHair, BipedSlot::kLongHair, BipedSlot::kEars},
          {BipedSlot::kHead, BipedSlot::kCirclet}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask, {BipedSlot::kAmulet, BipedSlot::kModNeck},
          {BipedSlot::kModNeck, BipedSlot::kAmulet, BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask, {BipedSlot::kRing},
          {BipedSlot::kModFaceJewelry, BipedSlot::kCirclet, BipedSlot::kEars}),
      ScorePreferredTargetSlots(a_itemMask, a_targetMask, {BipedSlot::kHands},
                                {BipedSlot::kForearms, BipedSlot::kBody}),
      ScorePreferredTargetSlots(a_itemMask, a_targetMask,
                                {BipedSlot::kForearms},
                                {BipedSlot::kHands, BipedSlot::kBody}),
      ScorePreferredTargetSlots(a_itemMask, a_targetMask, {BipedSlot::kFeet},
                                {BipedSlot::kCalves, BipedSlot::kBody}),
      ScorePreferredTargetSlots(a_itemMask, a_targetMask, {BipedSlot::kCalves},
                                {BipedSlot::kFeet, BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask,
          {BipedSlot::kModFaceJewelry, BipedSlot::kModMouth},
          {BipedSlot::kHead, BipedSlot::kCirclet, BipedSlot::kEars}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask,
          {BipedSlot::kModChestPrimary, BipedSlot::kModChestSecondary,
           BipedSlot::kModBack, BipedSlot::kModShoulder,
           BipedSlot::kModPelvisPrimary, BipedSlot::kModPelvisSecondary},
          {BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask,
          {BipedSlot::kModArmLeft, BipedSlot::kModArmRight},
          {BipedSlot::kHands, BipedSlot::kForearms, BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask,
          {BipedSlot::kModLegLeft, BipedSlot::kModLegRight},
          {BipedSlot::kFeet, BipedSlot::kCalves, BipedSlot::kBody}),
  };

  for (const auto score : scores) {
    bestScore = (std::max)(bestScore, score);
  }

  return bestScore;
}
} // namespace

std::uint64_t VariantWorkbenchRow::GetSelectionConflictSlotMask() const {
  if (equipped.IsSlot()) {
    return equipped.slotMask;
  }

  const auto *armor =
      RE::TESForm::LookupByID<RE::TESObjectARMO>(equipped.formID);
  if (!armor) {
    return equipped.slotMask;
  }

  const auto addonSlotMask = armor::GetArmorAddonSlotMask(armor);
  return addonSlotMask != 0 ? addonSlotMask : equipped.slotMask;
}

bool VariantWorkbench::ResolveCatalogArmors(
    const std::vector<RE::FormID> &a_formIDs,
    std::vector<const RE::TESObjectARMO *> &a_armors) const {
  a_armors.clear();

  for (const auto formID :
       EquipmentCatalog::Get().ResolveArmorFormIDs(a_formIDs)) {
    const auto *armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(formID);
    if (!armor) {
      continue;
    }
    a_armors.push_back(armor);
  }

  return !a_armors.empty();
}

bool VariantWorkbench::CanAcceptOverrideWithPendingAssignments(
    int a_targetRowIndex, const EquipmentWidgetItem &a_item,
    const std::vector<PlannedCatalogAssignment> &a_pendingAssignments) const {
  if (!CanAcceptOverride(a_targetRowIndex, a_item)) {
    return false;
  }

  for (const auto &assignment : a_pendingAssignments) {
    if (assignment.rowIndex == a_targetRowIndex &&
        assignment.armorFormID == a_item.formID) {
      return false;
    }
  }

  return true;
}

void VariantWorkbench::RebuildRowOrder() {
  rowOrder_.clear();
  rowOrder_.reserve(rows_.size());
  for (const auto &row : rows_) {
    rowOrder_.push_back(row.key);
  }
}

void VariantWorkbench::MarkChanged() { ++revision_; }

int VariantWorkbench::FindBestCatalogTargetRowIndex(
    const EquipmentWidgetItem &a_item, bool a_requireAcceptable,
    const std::vector<PlannedCatalogAssignment> *a_pendingAssignments,
    const std::vector<int> *a_candidateRowIndices) const {
  int fallbackRowIndex = -1;
  int bestPrecedenceRowIndex = -1;
  int bestPrecedenceScore = -1;
  const auto visitRow = [&](const int rowIndex) -> bool {
    const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    if (!row.isEquipped) {
      return false;
    }

    if (a_requireAcceptable &&
        ((a_pendingAssignments &&
          !CanAcceptOverrideWithPendingAssignments(rowIndex, a_item,
                                                   *a_pendingAssignments)) ||
         (!a_pendingAssignments && !CanAcceptOverride(rowIndex, a_item)))) {
      return false;
    }

    if ((row.equipped.slotMask & a_item.slotMask) != 0) {
      fallbackRowIndex = rowIndex;
      bestPrecedenceRowIndex = rowIndex;
      bestPrecedenceScore = (std::numeric_limits<int>::max)();
      return true;
    }

    if (fallbackRowIndex < 0) {
      fallbackRowIndex = rowIndex;
    }

    const auto precedenceScore =
        ScoreFallbackTargetRow(a_item.slotMask, row.equipped.slotMask);
    if (precedenceScore > bestPrecedenceScore) {
      bestPrecedenceScore = precedenceScore;
      bestPrecedenceRowIndex = rowIndex;
    }
    return false;
  };

  if (a_candidateRowIndices != nullptr) {
    for (const auto rowIndex : *a_candidateRowIndices) {
      if (rowIndex < 0 || rowIndex >= static_cast<int>(rows_.size())) {
        continue;
      }

      if (visitRow(rowIndex)) {
        return rowIndex;
      }
    }
  } else {
    for (int rowIndex = 0; rowIndex < static_cast<int>(rows_.size());
         ++rowIndex) {
      if (visitRow(rowIndex)) {
        return rowIndex;
      }
    }
  }

  if (bestPrecedenceRowIndex >= 0) {
    return bestPrecedenceRowIndex;
  }

  return fallbackRowIndex;
}

int VariantWorkbench::FindBestCatalogTargetRowIndex(
    const EquipmentWidgetItem &a_item, bool a_requireAcceptable) const {
  return FindBestCatalogTargetRowIndex(a_item, a_requireAcceptable, nullptr,
                                       nullptr);
}

bool VariantWorkbench::PlanCatalogAssignments(
    const std::vector<RE::FormID> &a_formIDs,
    std::vector<PlannedCatalogAssignment> &a_assignments,
    const std::vector<int> *a_candidateRowIndices) const {
  a_assignments.clear();

  std::vector<const RE::TESObjectARMO *> armors;
  if (!ResolveCatalogArmors(a_formIDs, armors)) {
    return false;
  }

  for (const auto *armor : armors) {
    EquipmentWidgetItem item{};
    if (!armor || !workbench::BuildCatalogItem(armor->GetFormID(), item)) {
      continue;
    }

    const auto rowIndex = FindBestCatalogTargetRowIndex(
        item, true, &a_assignments, a_candidateRowIndices);
    if (rowIndex < 0) {
      continue;
    }

    a_assignments.push_back({rowIndex, armor->GetFormID()});
  }

  return !a_assignments.empty();
}

void VariantWorkbench::SyncRowsFromActor(
    RE::Actor *a_actor, std::optional<std::string> a_newRowConditionId) {
  if (!a_actor) {
    bool changed = false;
    for (auto &row : rows_) {
      changed = changed || row.isEquipped;
      row.isEquipped = false;
    }
    if (changed) {
      MarkChanged();
    }
    return;
  }

  const auto syncConditionId = std::move(a_newRowConditionId);

  for (auto &row : rows_) {
    row.isEquipped = false;
  }

  std::uint64_t occupiedSlotMask = 0;
  std::unordered_set<RE::FormID> seenArmorForms;
  std::vector<VariantWorkbenchRow> newlyEquippedRows;
  std::vector<std::string> newlyEquippedRowKeys;

  for (const auto slot :
       {RE::BGSBipedObjectForm::BipedObjectSlot::kHead,
        RE::BGSBipedObjectForm::BipedObjectSlot::kHair,
        RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
        RE::BGSBipedObjectForm::BipedObjectSlot::kHands,
        RE::BGSBipedObjectForm::BipedObjectSlot::kForearms,
        RE::BGSBipedObjectForm::BipedObjectSlot::kAmulet,
        RE::BGSBipedObjectForm::BipedObjectSlot::kRing,
        RE::BGSBipedObjectForm::BipedObjectSlot::kFeet,
        RE::BGSBipedObjectForm::BipedObjectSlot::kCalves,
        RE::BGSBipedObjectForm::BipedObjectSlot::kShield,
        RE::BGSBipedObjectForm::BipedObjectSlot::kTail,
        RE::BGSBipedObjectForm::BipedObjectSlot::kLongHair,
        RE::BGSBipedObjectForm::BipedObjectSlot::kCirclet,
        RE::BGSBipedObjectForm::BipedObjectSlot::kEars,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModMouth,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModNeck,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModChestPrimary,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModBack,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModMisc1,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisPrimary,
        RE::BGSBipedObjectForm::BipedObjectSlot::kDecapitateHead,
        RE::BGSBipedObjectForm::BipedObjectSlot::kDecapitate,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisSecondary,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModLegRight,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModLegLeft,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModFaceJewelry,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModChestSecondary,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModShoulder,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModArmLeft,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModArmRight,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModMisc2,
        RE::BGSBipedObjectForm::BipedObjectSlot::kFX01}) {
    auto *armor = a_actor->GetWornArmor(slot);
    if (!armor) {
      continue;
    }

    const auto formID = armor->GetFormID();
    if (!seenArmorForms.insert(formID).second) {
      continue;
    }

    EquipmentWidgetItem equipped{};
    if (!workbench::BuildCatalogItem(formID, equipped)) {
      continue;
    }
    const auto addonSlotMask = armor::GetArmorAddonSlotMask(armor);
    occupiedSlotMask |= addonSlotMask != 0 ? addonSlotMask : equipped.slotMask;

    const auto sourceKey = BuildArmorSourceKey(formID);
    bool hasSyncConditionRow = false;
    for (auto &row : rows_) {
      if (row.sourceKey != sourceKey) {
        continue;
      }

      row.equipped = equipped;
      row.isEquipped = true;
      UpdateRowIdentity(row);
      hasSyncConditionRow |= row.conditionId == syncConditionId;
    }
    if (hasSyncConditionRow) {
      continue;
    }

    VariantWorkbenchRow row{};
    row.sourceKey = sourceKey;
    row.conditionId = syncConditionId;
    row.equipped = std::move(equipped);
    row.isEquipped = true;
    UpdateRowIdentity(row);
    newlyEquippedRowKeys.push_back(row.key);
    newlyEquippedRows.push_back(std::move(row));
  }

  if (!newlyEquippedRows.empty()) {
    rows_.insert(rows_.begin(),
                 std::make_move_iterator(newlyEquippedRows.begin()),
                 std::make_move_iterator(newlyEquippedRows.end()));
    rowOrder_.insert(rowOrder_.begin(),
                     std::make_move_iterator(newlyEquippedRowKeys.begin()),
                     std::make_move_iterator(newlyEquippedRowKeys.end()));
  }

  for (auto &row : rows_) {
    if (row.equipped.IsSlot()) {
      row.isEquipped = (row.equipped.slotMask & occupiedSlotMask) != 0;
    }
  }
  MarkChanged();
}

void VariantWorkbench::SyncRowsFromPlayer(
    std::optional<std::string> a_newRowConditionId) {
  SyncRowsFromActor(RE::PlayerCharacter::GetSingleton(),
                    std::move(a_newRowConditionId));
}

bool VariantWorkbench::IsPreviewingSelection(
    std::string_view a_selectionKey) const {
  return previewSelectionKey_ == a_selectionKey && !previewDavVariants_.empty();
}

bool VariantWorkbench::CanAcceptOverride(int a_targetRowIndex,
                                         const EquipmentWidgetItem &a_item,
                                         int a_sourceRowIndex,
                                         int a_sourceItemIndex) const {
  if (a_targetRowIndex < 0 ||
      a_targetRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  if (a_sourceRowIndex == a_targetRowIndex && a_sourceItemIndex >= 0) {
    return false;
  }

  const auto &row = rows_[static_cast<std::size_t>(a_targetRowIndex)];
  for (int itemIndex = 0; itemIndex < static_cast<int>(row.overrides.size());
       ++itemIndex) {
    if (a_targetRowIndex == a_sourceRowIndex &&
        itemIndex == a_sourceItemIndex) {
      continue;
    }

    if (row.overrides[static_cast<std::size_t>(itemIndex)].formID ==
        a_item.formID) {
      return false;
    }
  }

  if (a_item.slotMask == 0) {
    return false;
  }
  return true;
}

bool VariantWorkbench::AddCatalogOverride(int a_targetRowIndex,
                                          RE::FormID a_formID) {
  EquipmentWidgetItem item{};
  if (!workbench::BuildCatalogItem(a_formID, item) ||
      !CanAcceptOverride(a_targetRowIndex, item)) {
    return false;
  }

  rows_[static_cast<std::size_t>(a_targetRowIndex)].overrides.push_back(
      std::move(item));
  MarkChanged();
  return true;
}

bool VariantWorkbench::AddCatalogSelectionToWorkbench(
    const std::vector<RE::FormID> &a_formIDs,
    const std::vector<int> *a_candidateRowIndices) {
  std::vector<PlannedCatalogAssignment> assignments;
  if (!PlanCatalogAssignments(a_formIDs, assignments, a_candidateRowIndices)) {
    return false;
  }

  bool addedAny = false;
  for (const auto &assignment : assignments) {
    addedAny |= AddCatalogOverride(assignment.rowIndex, assignment.armorFormID);
  }

  return addedAny;
}

bool VariantWorkbench::ReplaceCatalogSelectionInWorkbench(
    const std::vector<RE::FormID> &a_formIDs,
    const std::vector<int> *a_candidateRowIndices) {
  std::vector<PlannedCatalogAssignment> assignments;
  if (!PlanCatalogAssignments(a_formIDs, assignments, a_candidateRowIndices)) {
    return false;
  }

  std::unordered_set<int> targetRows;
  targetRows.reserve(assignments.size());
  for (const auto &assignment : assignments) {
    targetRows.insert(assignment.rowIndex);
  }

  for (const auto rowIndex : targetRows) {
    auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    row.overrides.clear();
  }

  bool addedAny = false;
  for (const auto &assignment : assignments) {
    addedAny |= AddCatalogOverride(assignment.rowIndex, assignment.armorFormID);
  }

  return addedAny;
}

bool VariantWorkbench::AddCatalogSelectionAsRows(
    const std::vector<RE::FormID> &a_formIDs,
    std::optional<std::string> a_conditionId) {
  auto newRows = BuildCatalogRows(a_formIDs, std::move(a_conditionId));
  bool addedAny = false;
  for (auto &row : newRows) {
    rowOrder_.push_back(row.key);
    rows_.push_back(std::move(row));
    addedAny = true;
  }

  if (addedAny) {
    MarkChanged();
  }

  return addedAny;
}

bool VariantWorkbench::AddSlotRow(const std::uint64_t a_slotMask,
                                  std::optional<std::string> a_conditionId) {
  auto row = BuildSlotRow(a_slotMask, std::move(a_conditionId));
  if (!row) {
    return false;
  }

  rowOrder_.push_back(row->key);
  rows_.push_back(std::move(*row));
  MarkChanged();
  return true;
}

std::vector<VariantWorkbenchRow> VariantWorkbench::BuildCatalogRows(
    const std::vector<RE::FormID> &a_formIDs,
    std::optional<std::string> a_conditionId) const {
  std::vector<VariantWorkbenchRow> newRows;
  const auto resolvedConditionId = std::move(a_conditionId);

  std::vector<const RE::TESObjectARMO *> armors;
  if (!ResolveCatalogArmors(a_formIDs, armors)) {
    return newRows;
  }

  std::unordered_set<std::string> seenRowKeys;
  for (const auto &row : rows_) {
    seenRowKeys.insert(row.key);
  }

  for (const auto *armor : armors) {
    if (!armor) {
      continue;
    }

    const auto sourceKey = BuildArmorSourceKey(armor->GetFormID());
    const auto rowKey = BuildRowKey(sourceKey, resolvedConditionId);
    if (!seenRowKeys.insert(rowKey).second) {
      continue;
    }

    EquipmentWidgetItem equipped{};
    if (!workbench::BuildCatalogItem(armor->GetFormID(), equipped)) {
      continue;
    }

    VariantWorkbenchRow row{};
    row.sourceKey = sourceKey;
    row.conditionId = resolvedConditionId;
    row.equipped = std::move(equipped);
    UpdateRowIdentity(row);
    newRows.push_back(std::move(row));
  }

  return newRows;
}

std::optional<VariantWorkbenchRow>
VariantWorkbench::BuildSlotRow(const std::uint64_t a_slotMask,
                               std::optional<std::string> a_conditionId) const {
  const auto sourceKey = BuildSlotKey(a_slotMask);
  if (sourceKey.empty()) {
    return std::nullopt;
  }

  const auto resolvedConditionId = std::move(a_conditionId);
  const auto rowKey = BuildRowKey(sourceKey, resolvedConditionId);
  if (std::ranges::find(rows_, rowKey, &VariantWorkbenchRow::key) !=
      rows_.end()) {
    return std::nullopt;
  }

  EquipmentWidgetItem slotItem{};
  if (!workbench::BuildSlotItem(a_slotMask, slotItem)) {
    return std::nullopt;
  }

  VariantWorkbenchRow row{};
  row.sourceKey = sourceKey;
  row.conditionId = resolvedConditionId;
  row.equipped = std::move(slotItem);
  UpdateRowIdentity(row);
  return row;
}

bool VariantWorkbench::MoveOverride(int a_sourceRowIndex, int a_sourceItemIndex,
                                    int a_targetRowIndex) {
  if (a_sourceRowIndex < 0 ||
      a_sourceRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto &sourceOverrides =
      rows_[static_cast<std::size_t>(a_sourceRowIndex)].overrides;
  if (a_sourceItemIndex < 0 ||
      a_sourceItemIndex >= static_cast<int>(sourceOverrides.size())) {
    return false;
  }

  auto item = sourceOverrides[static_cast<std::size_t>(a_sourceItemIndex)];
  if (!CanAcceptOverride(a_targetRowIndex, item, a_sourceRowIndex,
                         a_sourceItemIndex)) {
    return false;
  }

  sourceOverrides.erase(sourceOverrides.begin() + a_sourceItemIndex);
  rows_[static_cast<std::size_t>(a_targetRowIndex)].overrides.push_back(
      std::move(item));
  MarkChanged();
  return true;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool VariantWorkbench::DeleteOverride(int a_rowIndex, int a_itemIndex) {
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto &overrides = rows_[static_cast<std::size_t>(a_rowIndex)].overrides;
  if (a_itemIndex < 0 || a_itemIndex >= static_cast<int>(overrides.size())) {
    return false;
  }

  overrides.erase(overrides.begin() + a_itemIndex);
  MarkChanged();
  return true;
}

bool VariantWorkbench::DeleteRow(int a_rowIndex) {
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  rows_.erase(rows_.begin() + a_rowIndex);
  RebuildRowOrder();
  MarkChanged();
  return true;
}

std::size_t VariantWorkbench::DeleteRowsByConditionId(
    const std::string_view a_conditionId, const bool a_onlyIfEmpty) {
  std::size_t removedCount = 0;

  for (auto index = static_cast<int>(rows_.size()) - 1; index >= 0; --index) {
    const auto &row = rows_[static_cast<std::size_t>(index)];
    if (!row.conditionId.has_value() || *row.conditionId != a_conditionId) {
      continue;
    }
    if (a_onlyIfEmpty && row.HasOverridesOrHideState()) {
      continue;
    }

    rows_.erase(rows_.begin() + index);
    ++removedCount;
  }

  if (removedCount != 0) {
    RebuildRowOrder();
    MarkChanged();
  }

  return removedCount;
}

bool VariantWorkbench::SetConditionId(
    const int a_rowIndex, std::optional<std::string> a_conditionId) {
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto rowIndex = static_cast<std::size_t>(a_rowIndex);
  const auto nextKey = BuildRowKey(rows_[rowIndex].sourceKey, a_conditionId);
  const auto duplicateIt =
      std::ranges::find_if(rows_, [&](const VariantWorkbenchRow &a_other) {
        return &a_other != &rows_[rowIndex] && a_other.key == nextKey;
      });
  if (duplicateIt != rows_.end()) {
    if (!duplicateIt->HasOverridesOrHideState()) {
      const auto duplicateIndex = static_cast<std::size_t>(
          std::distance(rows_.begin(), duplicateIt));
      DeleteRow(static_cast<int>(duplicateIndex));
      if (duplicateIndex < rowIndex) {
        --rowIndex;
      }
    } else {
      return false;
    }
  }

  auto &row = rows_[rowIndex];
  row.conditionId = std::move(a_conditionId);
  UpdateRowIdentity(row);
  RebuildRowOrder();
  MarkChanged();
  return true;
}

bool VariantWorkbench::SetHideEquipped(int a_rowIndex, bool a_hideEquipped) {
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  rows_[static_cast<std::size_t>(a_rowIndex)].hideEquipped = a_hideEquipped;
  MarkChanged();
  return true;
}

bool VariantWorkbench::ResetEquippedRows() {
  bool changed = false;
  for (auto &row : rows_) {
    if (!row.isEquipped) {
      continue;
    }

    if (!row.overrides.empty()) {
      row.overrides.clear();
      changed = true;
    }
    if (row.hideEquipped) {
      row.hideEquipped = false;
      changed = true;
    }
  }

  if (changed) {
    MarkChanged();
  }

  return changed;
}

void VariantWorkbench::ResetAllRows() { Revert(); }

std::vector<RE::FormID> VariantWorkbench::CollectEquippedArmorFormIDs() const {
  std::vector<RE::FormID> formIDs;
  std::unordered_set<RE::FormID> seen;
  formIDs.reserve(rows_.size());

  for (const auto &row : rows_) {
    if (!row.isEquipped || row.equipped.formID == 0) {
      continue;
    }
    if (seen.insert(row.equipped.formID).second) {
      formIDs.push_back(row.equipped.formID);
    }
  }

  return formIDs;
}

std::vector<RE::FormID>
VariantWorkbench::CollectOverrideArmorFormIDsFromEquippedRows() const {
  std::vector<RE::FormID> formIDs;
  std::unordered_set<RE::FormID> seen;

  for (const auto &row : rows_) {
    if (!row.isEquipped) {
      continue;
    }

    for (const auto &item : row.overrides) {
      if (item.formID == 0) {
        continue;
      }
      if (seen.insert(item.formID).second) {
        formIDs.push_back(item.formID);
      }
    }
  }

  return formIDs;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool VariantWorkbench::InsertCatalogRow(
    RE::FormID a_formID, int a_targetRowIndex, bool a_insertAfter,
    std::optional<std::string> a_conditionId) {
  if (a_targetRowIndex < 0 ||
      a_targetRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto newRows = BuildCatalogRows(std::vector<RE::FormID>{a_formID},
                                  std::move(a_conditionId));
  if (newRows.empty()) {
    return false;
  }

  auto insertIndex = a_targetRowIndex + (a_insertAfter ? 1 : 0);
  insertIndex = std::clamp(insertIndex, 0, static_cast<int>(rows_.size()));
  rows_.insert(rows_.begin() + insertIndex, std::move(newRows.front()));

  RebuildRowOrder();
  MarkChanged();
  return true;
}

bool VariantWorkbench::InsertSlotRow(const std::uint64_t a_slotMask,
                                     int a_targetRowIndex, bool a_insertAfter,
                                     std::optional<std::string> a_conditionId) {
  if (a_targetRowIndex < 0 ||
      a_targetRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto row = BuildSlotRow(a_slotMask, std::move(a_conditionId));
  if (!row) {
    return false;
  }

  auto insertIndex = a_targetRowIndex + (a_insertAfter ? 1 : 0);
  insertIndex = std::clamp(insertIndex, 0, static_cast<int>(rows_.size()));
  rows_.insert(rows_.begin() + insertIndex, std::move(*row));

  RebuildRowOrder();
  MarkChanged();
  return true;
}

bool VariantWorkbench::ApplyRowReorder(int a_sourceRowIndex,
                                       int a_targetRowIndex,
                                       bool a_insertAfter) {
  if (a_sourceRowIndex < 0 ||
      a_sourceRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }
  if (a_targetRowIndex < 0 ||
      a_targetRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }
  if (a_sourceRowIndex == a_targetRowIndex) {
    return false;
  }

  auto movedRow = std::move(rows_[static_cast<std::size_t>(a_sourceRowIndex)]);
  rows_.erase(rows_.begin() + a_sourceRowIndex);

  auto insertIndex = a_targetRowIndex + (a_insertAfter ? 1 : 0);
  if (a_sourceRowIndex < insertIndex) {
    --insertIndex;
  }

  rows_.insert(rows_.begin() + insertIndex, std::move(movedRow));

  RebuildRowOrder();
  MarkChanged();
  return true;
}
} // namespace sosr::workbench
