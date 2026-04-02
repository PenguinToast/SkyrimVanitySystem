#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include "ConditionRefreshTargets.h"
#include "EquipmentCatalog.h"
#include "conditions/Definition.h"
#include "workbench/Items.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sosr::workbench {
struct VariantWorkbenchRow {
  std::string key;
  std::string sourceKey;
  std::optional<std::string> conditionId;
  EquipmentWidgetItem equipped;
  std::vector<EquipmentWidgetItem> overrides;
  bool hideEquipped{false};
  bool isEquipped{false};

  [[nodiscard]] bool HasCondition() const { return conditionId.has_value(); }
  [[nodiscard]] bool HasOverridesOrHideState() const {
    return hideEquipped || !overrides.empty();
  }

  [[nodiscard]] bool IsSlotRow() const { return equipped.IsSlot(); }

  [[nodiscard]] bool IsVisualConflictSource() const {
    return IsSlotRow() || isEquipped;
  }

  [[nodiscard]] std::uint64_t GetSelectionConflictSlotMask() const;

  [[nodiscard]] std::uint64_t
  GetOverrideVisualSlotMask(const EquipmentWidgetItem &a_item) const {
    return a_item.slotMask;
  }
};

class VariantWorkbench {
public:
  struct InitialEquippedState {
    std::unordered_set<RE::FormID> wornArmorForms;
    std::uint64_t occupiedSlotMask{0};
  };

  [[nodiscard]] static InitialEquippedState
  BuildInitialEquippedState(RE::Actor *a_actor);
  void SyncRowsFromActor(RE::Actor *a_actor,
                         std::optional<std::string> a_newRowConditionId);
  void SyncRowsFromPlayer(std::optional<std::string> a_newRowConditionId);
  [[nodiscard]] bool
  IsPreviewingSelection(std::string_view a_selectionKey) const;
  [[nodiscard]] bool CanAcceptOverride(int a_targetRowIndex,
                                       const EquipmentWidgetItem &a_item,
                                       int a_sourceRowIndex = -1,
                                       int a_sourceItemIndex = -1) const;
  bool AddCatalogOverride(int a_targetRowIndex, RE::FormID a_formID);
  bool AddCatalogSelectionToWorkbench(
      const std::vector<RE::FormID> &a_formIDs,
      const std::vector<int> *a_candidateRowIndices = nullptr);
  bool ReplaceCatalogSelectionInWorkbench(
      const std::vector<RE::FormID> &a_formIDs,
      const std::vector<int> *a_candidateRowIndices = nullptr);
  bool AddCatalogSelectionAsRows(const std::vector<RE::FormID> &a_formIDs,
                                 std::optional<std::string> a_conditionId,
                                 const InitialEquippedState
                                     *a_initialEquippedState = nullptr);
  bool AddSlotRow(std::uint64_t a_slotMask,
                  std::optional<std::string> a_conditionId,
                  const InitialEquippedState *a_initialEquippedState = nullptr);
  bool
  ApplyCatalogPreview(std::string_view a_selectionKey,
                      const std::vector<RE::FormID> &a_formIDs,
                      RE::Actor *a_actor = nullptr,
                      const std::vector<int> *a_candidateRowIndices = nullptr);
  void ClearPreview();
  bool MoveOverride(int a_sourceRowIndex, int a_sourceItemIndex,
                    int a_targetRowIndex);
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  bool DeleteOverride(int a_rowIndex, int a_itemIndex);
  bool DeleteRow(int a_rowIndex);
  std::size_t DeleteRowsByConditionId(std::string_view a_conditionId,
                                      bool a_onlyIfEmpty);
  bool SetHideEquipped(int a_rowIndex, bool a_hideEquipped);
  bool
  ResetEquippedRows(const std::vector<int> *a_candidateRowIndices = nullptr);
  bool ResetAllRows(const std::vector<int> *a_candidateRowIndices = nullptr);
  [[nodiscard]] std::vector<RE::FormID> CollectEquippedArmorFormIDs(
      const std::vector<int> *a_candidateRowIndices = nullptr) const;
  [[nodiscard]] std::vector<RE::FormID>
  CollectOverrideArmorFormIDsFromEquippedRows(
      const std::vector<int> *a_candidateRowIndices = nullptr) const;
  [[nodiscard]] std::optional<KitEntry::Layout> CaptureKitLayout(
      const std::vector<int> *a_candidateRowIndices = nullptr) const;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  bool InsertCatalogRow(RE::FormID a_formID, int a_targetRowIndex,
                        bool a_insertAfter,
                        std::optional<std::string> a_conditionId,
                        const InitialEquippedState
                            *a_initialEquippedState = nullptr);
  bool InsertSlotRow(std::uint64_t a_slotMask, int a_targetRowIndex,
                     bool a_insertAfter,
                     std::optional<std::string> a_conditionId,
                     const InitialEquippedState
                         *a_initialEquippedState = nullptr);
  bool ApplyRowReorder(int a_sourceRowIndex, int a_targetRowIndex,
                       bool a_insertAfter);
  bool SetConditionId(int a_rowIndex, std::optional<std::string> a_conditionId);
  bool ApplyKitLayout(const KitEntry::Layout &a_layout, bool a_replaceExisting,
                      std::optional<std::string> a_newSlotRowConditionId,
                      const InitialEquippedState
                          *a_initialEquippedState = nullptr,
                      const std::vector<int> *a_candidateRowIndices = nullptr);
  bool
  PreviewKitLayout(std::string_view a_selectionKey,
                   const KitEntry::Layout &a_layout,
                   RE::Actor *a_actor = nullptr,
                   const std::vector<int> *a_candidateRowIndices = nullptr);
  void SyncDynamicArmorVariantsExtended(
      std::vector<conditions::Definition> &a_conditions);
  void Serialize(SKSE::SerializationInterface *a_skse) const;
  void Deserialize(SKSE::SerializationInterface *a_skse,
                   std::optional<std::string> a_missingConditionId);
  void Revert();

  [[nodiscard]] const std::vector<VariantWorkbenchRow> &GetRows() const {
    return rows_;
  }
  [[nodiscard]] std::size_t GetRowCount() const { return rows_.size(); }
  [[nodiscard]] std::uint64_t GetRevision() const { return revision_; }

private:
  struct PlannedCatalogAssignment {
    int rowIndex{-1};
    RE::FormID armorFormID{0};
  };

  [[nodiscard]] static std::vector<int>
  BuildCandidateRowIndices(const std::vector<int> *a_candidateRowIndices,
                           std::size_t a_rowCount);
  [[nodiscard]] bool
  ResolveCatalogArmors(const std::vector<RE::FormID> &a_formIDs,
                       std::vector<const RE::TESObjectARMO *> &a_armors) const;
  [[nodiscard]] int FindBestCatalogTargetRowIndex(
      const EquipmentWidgetItem &a_item, bool a_requireAcceptable,
      const std::vector<PlannedCatalogAssignment> *a_pendingAssignments,
      const std::vector<int> *a_candidateRowIndices) const;
  [[nodiscard]] bool CanAcceptOverrideWithPendingAssignments(
      int a_targetRowIndex, const EquipmentWidgetItem &a_item,
      const std::vector<PlannedCatalogAssignment> &a_pendingAssignments) const;
  [[nodiscard]] int FindBestItemTargetRowIndexBySlotMask(
      std::uint64_t a_targetSlotMask, bool a_requireAcceptable,
      const EquipmentWidgetItem *a_item,
      const std::vector<PlannedCatalogAssignment> *a_pendingAssignments,
      const std::vector<int> *a_candidateRowIndices) const;
  [[nodiscard]] bool
  PlanCatalogAssignments(const std::vector<RE::FormID> &a_formIDs,
                         std::vector<PlannedCatalogAssignment> &a_assignments,
                         const std::vector<int> *a_candidateRowIndices) const;
  [[nodiscard]] std::vector<VariantWorkbenchRow>
  BuildCatalogRows(const std::vector<RE::FormID> &a_formIDs,
                   std::optional<std::string> a_conditionId,
                   const InitialEquippedState *a_initialEquippedState) const;
  [[nodiscard]] std::optional<VariantWorkbenchRow>
  BuildSlotRow(std::uint64_t a_slotMask,
               std::optional<std::string> a_conditionId,
               const InitialEquippedState *a_initialEquippedState) const;
  [[nodiscard]] int
  FindBestCatalogTargetRowIndex(const EquipmentWidgetItem &a_item,
                                bool a_requireAcceptable) const;
  void RebuildRowOrder();
  void MarkChanged();

  std::vector<VariantWorkbenchRow> rows_;
  std::vector<std::string> rowOrder_;
  std::uint64_t revision_{0};
  struct ActiveDavVariantState {
    std::string variantJson;
    std::string conditionSignature;
    conditions::RefreshTargets refreshTargets;
  };
  std::unordered_map<std::string, ActiveDavVariantState> activeDavVariants_;
  std::string previewSelectionKey_;
  RE::FormID previewActorFormID_{0};
  std::unordered_map<std::string, std::string> previewDavVariants_;
};
} // namespace sosr::workbench
