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

constexpr std::array kTrackedWornSlots{
    BipedSlot::kHead,
    BipedSlot::kHair,
    BipedSlot::kBody,
    BipedSlot::kHands,
    BipedSlot::kForearms,
    BipedSlot::kAmulet,
    BipedSlot::kRing,
    BipedSlot::kFeet,
    BipedSlot::kCalves,
    BipedSlot::kShield,
    BipedSlot::kTail,
    BipedSlot::kLongHair,
    BipedSlot::kCirclet,
    BipedSlot::kEars,
    BipedSlot::kModMouth,
    BipedSlot::kModNeck,
    BipedSlot::kModChestPrimary,
    BipedSlot::kModBack,
    BipedSlot::kModMisc1,
    BipedSlot::kModPelvisPrimary,
    BipedSlot::kDecapitateHead,
    BipedSlot::kDecapitate,
    BipedSlot::kModPelvisSecondary,
    BipedSlot::kModLegRight,
    BipedSlot::kModLegLeft,
    BipedSlot::kModFaceJewelry,
    BipedSlot::kModChestSecondary,
    BipedSlot::kModShoulder,
    BipedSlot::kModArmLeft,
    BipedSlot::kModArmRight,
    BipedSlot::kModMisc2,
    BipedSlot::kFX01};

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

bool IsValidRowIndex(const int a_rowIndex, const std::size_t a_rowCount) {
  return a_rowIndex >= 0 && a_rowIndex < static_cast<int>(a_rowCount);
}

template <class F> void VisitDistinctWornArmorItems(RE::Actor *a_actor, F &&a_visit) {
  if (a_actor == nullptr) {
    return;
  }

  std::unordered_set<RE::FormID> seenArmorForms;
  for (const auto slot : kTrackedWornSlots) {
    const auto *armor = a_actor->GetWornArmor(slot);
    if (armor == nullptr ||
        !seenArmorForms.insert(armor->GetFormID()).second) {
      continue;
    }

    EquipmentWidgetItem equipped{};
    if (!workbench::BuildCatalogItem(armor->GetFormID(), equipped)) {
      continue;
    }

    a_visit(armor, equipped);
  }
}

} // namespace

VariantWorkbench::InitialEquippedState
VariantWorkbench::BuildInitialEquippedState(RE::Actor *a_actor) {
  InitialEquippedState initialState;
  VisitDistinctWornArmorItems(
      a_actor, [&](const RE::TESObjectARMO *a_armor,
                   const EquipmentWidgetItem &a_equipped) {
        initialState.wornArmorForms.insert(a_armor->GetFormID());
        const auto addonSlotMask = armor::GetArmorAddonSlotMask(a_armor);
        initialState.occupiedSlotMask |=
            addonSlotMask != 0 ? addonSlotMask : a_equipped.slotMask;
      });
  return initialState;
}

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
    if (!armor || !armor::HasArmorAddons(armor)) {
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

std::vector<int> VariantWorkbench::BuildCandidateRowIndices(
    const std::vector<int> *a_candidateRowIndices,
    const std::size_t a_rowCount) {
  if (a_candidateRowIndices != nullptr) {
    return *a_candidateRowIndices;
  }

  std::vector<int> indices;
  indices.reserve(a_rowCount);
  for (int rowIndex = 0; rowIndex < static_cast<int>(a_rowCount); ++rowIndex) {
    indices.push_back(rowIndex);
  }
  return indices;
}

int VariantWorkbench::FindBestCatalogTargetRowIndex(
    const EquipmentWidgetItem &a_item, bool a_requireAcceptable,
    const std::vector<PlannedCatalogAssignment> *a_pendingAssignments,
    const std::vector<int> *a_candidateRowIndices) const {
  return FindBestItemTargetRowIndexBySlotMask(
      a_item.slotMask, a_requireAcceptable, &a_item, a_pendingAssignments,
      a_candidateRowIndices);
}

int VariantWorkbench::FindBestItemTargetRowIndexBySlotMask(
    const std::uint64_t a_targetSlotMask, const bool a_requireAcceptable,
    const EquipmentWidgetItem *a_item,
    const std::vector<PlannedCatalogAssignment> *a_pendingAssignments,
    const std::vector<int> *a_candidateRowIndices) const {
  int fallbackRowIndex = -1;
  int bestPrecedenceRowIndex = -1;
  int bestPrecedenceScore = -1;

  const auto visitRow = [&](const int rowIndex) -> bool {
    const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    if (!row.isEquipped || row.IsSlotRow()) {
      return false;
    }

    if (a_requireAcceptable && a_item != nullptr &&
        ((a_pendingAssignments != nullptr &&
          !CanAcceptOverrideWithPendingAssignments(rowIndex, *a_item,
                                                   *a_pendingAssignments)) ||
         (a_pendingAssignments == nullptr &&
          !CanAcceptOverride(rowIndex, *a_item)))) {
      return false;
    }

    if (a_targetSlotMask == 0) {
      if (fallbackRowIndex < 0) {
        fallbackRowIndex = rowIndex;
      }
      return false;
    }

    if ((row.equipped.slotMask & a_targetSlotMask) != 0) {
      fallbackRowIndex = rowIndex;
      bestPrecedenceRowIndex = rowIndex;
      bestPrecedenceScore = (std::numeric_limits<int>::max)();
      return true;
    }

    if (fallbackRowIndex < 0) {
      fallbackRowIndex = rowIndex;
    }

    const auto precedenceScore =
        ScoreFallbackTargetRow(a_targetSlotMask, row.equipped.slotMask);
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
  std::vector<VariantWorkbenchRow> newlyEquippedRows;
  std::vector<std::string> newlyEquippedRowKeys;

  VisitDistinctWornArmorItems(
      a_actor, [&](const RE::TESObjectARMO *a_armor,
                   const EquipmentWidgetItem &a_equipped) {
        const auto formID = a_armor->GetFormID();
        const auto addonSlotMask = armor::GetArmorAddonSlotMask(a_armor);
        occupiedSlotMask |=
            addonSlotMask != 0 ? addonSlotMask : a_equipped.slotMask;

        const auto sourceKey = BuildArmorSourceKey(formID);
        bool hasSyncConditionRow = false;
        for (auto &row : rows_) {
          if (row.sourceKey != sourceKey) {
            continue;
          }

          row.equipped = a_equipped;
          row.isEquipped = true;
          UpdateRowIdentity(row);
          hasSyncConditionRow |= row.conditionId == syncConditionId;
        }
        if (hasSyncConditionRow) {
          return;
        }

        VariantWorkbenchRow row{};
        row.sourceKey = sourceKey;
        row.conditionId = syncConditionId;
        row.equipped = a_equipped;
        row.isEquipped = true;
        UpdateRowIdentity(row);
        newlyEquippedRowKeys.push_back(row.key);
        newlyEquippedRows.push_back(std::move(row));
      });

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

  if (!a_item.SupportsDavArmorReplacement()) {
    return false;
  }

  if (!a_item.IsSlot()) {
    const auto *armorForm =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(a_item.formID);
    if (!armorForm || armor::GetFormIdentifier(armorForm).empty()) {
      return false;
    }
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
    std::optional<std::string> a_conditionId,
    const InitialEquippedState *a_initialEquippedState) {
  auto newRows = BuildCatalogRows(a_formIDs, std::move(a_conditionId),
                                  a_initialEquippedState);
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
                                  std::optional<std::string> a_conditionId,
                                  const InitialEquippedState
                                      *a_initialEquippedState) {
  auto row = BuildSlotRow(a_slotMask, std::move(a_conditionId),
                          a_initialEquippedState);
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
    std::optional<std::string> a_conditionId,
    const InitialEquippedState *a_initialEquippedState) const {
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
    row.isEquipped = a_initialEquippedState != nullptr &&
                     a_initialEquippedState->wornArmorForms.contains(
                         armor->GetFormID());
    UpdateRowIdentity(row);
    newRows.push_back(std::move(row));
  }

  return newRows;
}

std::optional<VariantWorkbenchRow>
VariantWorkbench::BuildSlotRow(const std::uint64_t a_slotMask,
                               std::optional<std::string> a_conditionId,
                               const InitialEquippedState
                                   *a_initialEquippedState) const {
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
  row.isEquipped = a_initialEquippedState != nullptr &&
                   (a_slotMask & a_initialEquippedState->occupiedSlotMask) != 0;
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

std::size_t
VariantWorkbench::DeleteRowsByConditionId(const std::string_view a_conditionId,
                                          const bool a_onlyIfEmpty) {
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
      const auto duplicateIndex =
          static_cast<std::size_t>(std::distance(rows_.begin(), duplicateIt));
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

bool VariantWorkbench::ResetEquippedRows(
    const std::vector<int> *a_candidateRowIndices) {
  bool changed = false;

  const auto resetRow = [&](VariantWorkbenchRow &a_row) {
    if (!a_row.isEquipped) {
      return;
    }

    if (!a_row.overrides.empty()) {
      a_row.overrides.clear();
      changed = true;
    }
    if (a_row.hideEquipped) {
      a_row.hideEquipped = false;
      changed = true;
    }
  };

  if (a_candidateRowIndices != nullptr) {
    for (const auto rowIndex : *a_candidateRowIndices) {
      if (rowIndex < 0 || rowIndex >= static_cast<int>(rows_.size())) {
        continue;
      }
      resetRow(rows_[static_cast<std::size_t>(rowIndex)]);
    }
  } else {
    for (auto &row : rows_) {
      resetRow(row);
    }
  }

  if (changed) {
    MarkChanged();
  }

  return changed;
}

bool VariantWorkbench::ResetAllRows(
    const std::vector<int> *a_candidateRowIndices) {
  if (a_candidateRowIndices == nullptr) {
    Revert();
    return true;
  }

  bool changed = false;
  for (const auto rowIndex : *a_candidateRowIndices) {
    if (rowIndex < 0 || rowIndex >= static_cast<int>(rows_.size())) {
      continue;
    }

    auto &row = rows_[static_cast<std::size_t>(rowIndex)];
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

std::vector<RE::FormID> VariantWorkbench::CollectEquippedArmorFormIDs(
    const std::vector<int> *a_candidateRowIndices) const {
  std::vector<RE::FormID> formIDs;
  std::unordered_set<RE::FormID> seen;

  const auto collectRow = [&](const VariantWorkbenchRow &a_row) {
    if (!a_row.isEquipped || a_row.equipped.formID == 0) {
      return;
    }
    if (seen.insert(a_row.equipped.formID).second) {
      formIDs.push_back(a_row.equipped.formID);
    }
  };

  formIDs.reserve(a_candidateRowIndices != nullptr
                      ? a_candidateRowIndices->size()
                      : rows_.size());
  if (a_candidateRowIndices != nullptr) {
    for (const auto rowIndex : *a_candidateRowIndices) {
      if (rowIndex < 0 || rowIndex >= static_cast<int>(rows_.size())) {
        continue;
      }
      collectRow(rows_[static_cast<std::size_t>(rowIndex)]);
    }
  } else {
    for (const auto &row : rows_) {
      collectRow(row);
    }
  }

  return formIDs;
}

std::vector<RE::FormID>
VariantWorkbench::CollectOverrideArmorFormIDsFromEquippedRows(
    const std::vector<int> *a_candidateRowIndices) const {
  std::vector<RE::FormID> formIDs;
  std::unordered_set<RE::FormID> seen;

  const auto collectRow = [&](const VariantWorkbenchRow &a_row) {
    if (!a_row.isEquipped) {
      return;
    }

    for (const auto &item : a_row.overrides) {
      if (item.formID == 0) {
        continue;
      }
      if (seen.insert(item.formID).second) {
        formIDs.push_back(item.formID);
      }
    }
  };

  if (a_candidateRowIndices != nullptr) {
    for (const auto rowIndex : *a_candidateRowIndices) {
      if (rowIndex < 0 || rowIndex >= static_cast<int>(rows_.size())) {
        continue;
      }
      collectRow(rows_[static_cast<std::size_t>(rowIndex)]);
    }
  } else {
    for (const auto &row : rows_) {
      collectRow(row);
    }
  }

  return formIDs;
}

std::optional<KitEntry::Layout> VariantWorkbench::CaptureKitLayout(
    const std::vector<int> *a_candidateRowIndices) const {
  KitEntry::Layout layout;
  const auto candidateRowIndices =
      BuildCandidateRowIndices(a_candidateRowIndices, rows_.size());
  layout.rows.reserve(candidateRowIndices.size());

  for (const auto rowIndex : candidateRowIndices) {
    if (!IsValidRowIndex(rowIndex, rows_.size())) {
      continue;
    }

    const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    if (!row.isEquipped) {
      continue;
    }
    if (!row.hideEquipped && row.overrides.empty()) {
      continue;
    }

    KitEntry::LayoutRow layoutRow;
    layoutRow.targetKind = row.IsSlotRow() ? KitEntry::LayoutTargetKind::Slot
                                           : KitEntry::LayoutTargetKind::Item;
    layoutRow.targetSlotMask = row.IsSlotRow()
                                   ? row.equipped.slotMask
                                   : row.GetSelectionConflictSlotMask();
    layoutRow.hideEquipped = row.hideEquipped;

    for (const auto &overrideItem : row.overrides) {
      if (!overrideItem.HasForm()) {
        continue;
      }
      if (const auto *overrideArmor =
              RE::TESForm::LookupByID<RE::TESObjectARMO>(overrideItem.formID);
          overrideArmor != nullptr) {
        const auto identifier = armor::GetFormIdentifier(overrideArmor);
        if (!identifier.empty()) {
          layoutRow.overrideIdentifiers.push_back(identifier);
        }
      }
    }

    if (layoutRow.targetSlotMask == 0 ||
        (!layoutRow.hideEquipped && layoutRow.overrideIdentifiers.empty())) {
      continue;
    }

    layout.rows.push_back(std::move(layoutRow));
  }

  if (layout.rows.empty()) {
    return std::nullopt;
  }

  return layout;
}

std::optional<KitEntry::Layout> VariantWorkbench::CaptureEquippedKitLayout(
    const std::vector<int> *a_candidateRowIndices) const {
  KitEntry::Layout layout;
  const auto candidateRowIndices =
      BuildCandidateRowIndices(a_candidateRowIndices, rows_.size());
  layout.rows.reserve(candidateRowIndices.size());

  for (const auto rowIndex : candidateRowIndices) {
    if (!IsValidRowIndex(rowIndex, rows_.size())) {
      continue;
    }

    const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    if (!row.isEquipped || row.IsSlotRow()) {
      continue;
    }

    const auto *equippedArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(row.equipped.formID);
    if (!equippedArmor) {
      continue;
    }

    const auto targetSlotMask = row.GetSelectionConflictSlotMask();
    const auto identifier = armor::GetFormIdentifier(equippedArmor);
    if (targetSlotMask == 0 || identifier.empty()) {
      continue;
    }

    KitEntry::LayoutRow layoutRow;
    layoutRow.targetKind = KitEntry::LayoutTargetKind::Item;
    layoutRow.targetSlotMask = targetSlotMask;
    layoutRow.overrideIdentifiers.push_back(identifier);
    layout.rows.push_back(std::move(layoutRow));
  }

  if (layout.rows.empty()) {
    return std::nullopt;
  }

  return layout;
}

bool VariantWorkbench::ApplyKitLayout(
    const KitEntry::Layout &a_layout, const bool a_replaceExisting,
    std::optional<std::string> a_newSlotRowConditionId,
    const InitialEquippedState *a_initialEquippedState,
    const std::vector<int> *a_candidateRowIndices) {
  auto candidateRowIndices =
      BuildCandidateRowIndices(a_candidateRowIndices, rows_.size());

  bool changed = false;
  bool directlyChanged = false;
  if (a_replaceExisting) {
    changed |= ResetAllRows(&candidateRowIndices);
  }

  const auto projection = ProjectKitLayoutRows(
      a_layout, &candidateRowIndices,
      a_newSlotRowConditionId.has_value()
          ? KitLayoutFallbackMode::SlotTargetsOnly
          : KitLayoutFallbackMode::None,
      a_newSlotRowConditionId, a_replaceExisting);

  for (const auto &projectedRow : projection.rows) {
    const auto &row = projectedRow.row;
    auto rowIt = std::ranges::find(rows_, row.key, &VariantWorkbenchRow::key);
    if (rowIt == rows_.end() && row.IsSlotRow() &&
        a_newSlotRowConditionId.has_value()) {
      const auto added =
          AddSlotRow(row.equipped.slotMask, a_newSlotRowConditionId,
                     a_initialEquippedState);
      changed |= added;
      rowIt = std::ranges::find(rows_, row.key, &VariantWorkbenchRow::key);
    }
    if (rowIt == rows_.end()) {
      continue;
    }

    if (rowIt->overrides == row.overrides &&
        rowIt->hideEquipped == row.hideEquipped) {
      continue;
    }

    rowIt->overrides = row.overrides;
    rowIt->hideEquipped = row.hideEquipped;
    directlyChanged = true;
  }

  if (directlyChanged) {
    MarkChanged();
  }
  return changed || directlyChanged;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool VariantWorkbench::InsertCatalogRow(
    RE::FormID a_formID, int a_targetRowIndex, bool a_insertAfter,
    std::optional<std::string> a_conditionId,
    const InitialEquippedState *a_initialEquippedState) {
  if (a_targetRowIndex < 0 ||
      a_targetRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto newRows = BuildCatalogRows(std::vector<RE::FormID>{a_formID},
                                  std::move(a_conditionId),
                                  a_initialEquippedState);
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
                                     std::optional<std::string> a_conditionId,
                                     const InitialEquippedState
                                         *a_initialEquippedState) {
  if (a_targetRowIndex < 0 ||
      a_targetRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto row = BuildSlotRow(a_slotMask, std::move(a_conditionId),
                          a_initialEquippedState);
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
