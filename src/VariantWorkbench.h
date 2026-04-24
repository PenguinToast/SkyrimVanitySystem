#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include "ConditionRefreshTargets.h"
#include "EquipmentCatalog.h"
#include "conditions/Definition.h"
#include "workbench/Items.h"

#include <nlohmann/json_fwd.hpp>
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
  enum class ConditionOverrideSourceKind : std::uint8_t {
    ActorSource,
    SlotFallback
  };
  struct ConditionOverrideApplicationPlan {
    ConditionOverrideSourceKind sourceKind{
        ConditionOverrideSourceKind::SlotFallback};
    int sourceRowCount{0};
    int slotRowCount{0};
    int overrideCount{0};
    int skippedCount{0};
    std::vector<VariantWorkbenchRow> previewRows;

    [[nodiscard]] bool CanApply() const {
      for (const auto &row : previewRows) {
        if (row.HasOverridesOrHideState()) {
          return true;
        }
      }
      return false;
    }
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
  [[nodiscard]] ConditionOverrideApplicationPlan
  PlanConditionOverrideApplication(const std::vector<RE::FormID> &a_formIDs,
                                   std::string_view a_conditionId,
                                   RE::Actor *a_sourceActor) const;
  [[nodiscard]] ConditionOverrideApplicationPlan
  PlanConditionOverrideApplication(const KitEntry::Layout &a_layout,
                                   std::string_view a_conditionId,
                                   RE::Actor *a_sourceActor) const;
  bool ApplyConditionOverridePlan(
      const ConditionOverrideApplicationPlan &a_plan,
      std::string_view a_conditionId,
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
  [[nodiscard]] std::optional<KitEntry::Layout> CaptureEquippedKitLayout(
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
  [[nodiscard]] nlohmann::json SerializeState() const;
  [[nodiscard]] bool
  DeserializeState(const nlohmann::json &a_root,
                   const std::optional<std::string> &a_missingConditionId,
                   std::string *a_error = nullptr);
  void ReplaceState(VariantWorkbench &&a_source);
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
  struct PlannedSlotFallbackAssignment {
    std::uint64_t slotMask{0};
    RE::FormID armorFormID{0};
  };
  enum class KitLayoutFallbackMode : std::uint8_t {
    None,
    SlotTargetsOnly,
    AnyTargetSlot
  };
  struct ProjectedKitLayoutRow {
    VariantWorkbenchRow row;
    std::size_t priorityRowIndex{0};
  };
  struct KitLayoutProjection {
    std::vector<ProjectedKitLayoutRow> rows;
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
  [[nodiscard]] std::vector<int>
  CollectConditionSourceRowIndices(std::string_view a_conditionId) const;
  [[nodiscard]] bool PlanSlotFallbackAssignments(
      const std::vector<RE::FormID> &a_formIDs,
      std::vector<PlannedSlotFallbackAssignment> &a_assignments,
      int &a_skippedCount) const;
  [[nodiscard]] VariantWorkbench BuildPlanningWorkbench() const;
  [[nodiscard]] static std::vector<const RE::TESObjectARMO *>
  ResolveKitLayoutOverrideArmors(const KitEntry::LayoutRow &a_layoutRow);
  [[nodiscard]] int
  FindKitLayoutTargetRowIndex(const KitEntry::LayoutRow &a_layoutRow,
                              const std::vector<int> &a_candidateRowIndices)
      const;
  [[nodiscard]] KitLayoutProjection ProjectKitLayoutRows(
      const KitEntry::Layout &a_layout,
      const std::vector<int> *a_candidateRowIndices,
      KitLayoutFallbackMode a_fallbackMode,
      std::optional<std::string> a_fallbackConditionId,
      bool a_replaceExisting) const;
  static void AppendOverrideItem(std::vector<EquipmentWidgetItem> &a_overrides,
                                 const RE::TESObjectARMO *a_overrideArmor);
  [[nodiscard]] ConditionOverrideApplicationPlan
  PlanKitLayoutConditionOverrideApplication(
      const KitEntry::Layout &a_layout, std::string_view a_conditionId,
      const std::vector<int> *a_candidateRowIndices,
      bool a_allowSlotFallback) const;
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
