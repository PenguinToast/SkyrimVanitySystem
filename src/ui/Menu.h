#pragma once

#include "EquipmentCatalog.h"
#include "Keycode.h"
#include "ThemeConfig.h"
#include "VariantWorkbench.h"
#include "components/EquipmentWidget.h"
#include "conditions/Library.h"
#include "conditions/Store.h"
#include "imgui.h"
#include "ui/ConditionData.h"
#include "ui/WorkbenchConflicts.h"
#include "ui/catalog/DerivedState.h"
#include "ui/catalog/PaneState.h"
#include "ui/components/EditableCombo.h"
#include "ui/conditions/EditorState.h"
#include "ui/conditions/PaneState.h"
#include "ui/workbench/FilterState.h"
#include "ui/workbench/Tooltips.h"

#include <array>
#include <limits>
#include <optional>
#include <unordered_set>
#include <vector>

struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;
namespace SKSE {
class SerializationInterface;
}

namespace sosr {
class MenuHost;

class Menu {
public:
  struct FontOption {
    std::string label;
    std::string path;
    bool isBundled{false};
  };

  static Menu *GetSingleton();

  void Init(IDXGISwapChain *a_swapChain, ID3D11Device *a_device,
            ID3D11DeviceContext *a_context);
  void Draw();
  void Open();
  void Close();
  void NotifyWindowShutdown();
  void Toggle();
  void SetGameDataLoaded(bool a_loaded) { gameDataLoaded_ = a_loaded; }
  [[nodiscard]] bool IsEnabled() const { return enabled_; }
  [[nodiscard]] bool IsInitialized() const { return initialized_; }
  [[nodiscard]] bool IsGameDataLoaded() const { return gameDataLoaded_; }
  [[nodiscard]] bool PauseGameWhenOpen() const { return pauseGameWhenOpen_; }
  [[nodiscard]] bool WantsTextInput() const { return wantTextInput_; }
  [[nodiscard]] bool QueueSmoothScroll(float a_deltaY);
  [[nodiscard]] std::string GetToggleKeyLabel() const;
  [[nodiscard]] std::uint32_t GetToggleKey() const { return toggleKey_; }
  [[nodiscard]] std::uint32_t GetToggleModifier() const {
    return toggleModifier_;
  }
  [[nodiscard]] bool IsCapturingToggleKey() const {
    return awaitingToggleKeyCapture_;
  }
  void OpenToggleKeyCapture();
  void CloseToggleKeyCapture();
  void HandleToggleKeyCapture(std::uint32_t a_scanCode,
                              std::uint32_t a_modifierScanCode);
  [[nodiscard]] workbench::VariantWorkbench &GetWorkbench() {
    return workbench_;
  }
  [[nodiscard]] std::vector<ui::conditions::Definition> &GetConditions() {
    return ConditionDefinitions();
  }
  [[nodiscard]] const std::vector<ui::conditions::Definition> &
  GetConditions() const {
    return ConditionDefinitions();
  }
  void SerializeConditions(SKSE::SerializationInterface *a_skse) const;
  void DeserializeConditions(SKSE::SerializationInterface *a_skse);
  void RevertConditions();

private:
  static constexpr const char *kDefaultFontPath =
      "Data/Interface/SkyrimVanitySystem/fonts/Ubuntu-R.ttf";
  static constexpr const char *kDefaultIconFontPath =
      "Data/Interface/SkyrimVanitySystem/fonts/lucide.ttf";
  static constexpr const char *kBundledFontDirectory =
      "Data/Interface/SkyrimVanitySystem/fonts";
  static constexpr int kDefaultFontSizePixels = 18;
  static constexpr int kMinFontSizePixels = 8;
  static constexpr int kMaxFontSizePixels = 48;

  enum class DragSourceKind : std::uint32_t {
    Catalog = 1,
    Override = 2,
    Row = 3,
    SlotCatalog = 4
  };

  struct DraggedEquipmentPayload {
    std::uint32_t sourceKind{0};
    std::int32_t rowIndex{-1};
    std::int32_t itemIndex{-1};
    RE::FormID formID{0};
    std::uint64_t slotMask{0};
  };

  struct DraggedConditionPayload {
    std::array<char, 64> conditionId{};
  };

  enum class VisibilityState : std::uint8_t { Closed, Opening, Open, Closing };
  using KitCreationSource = ui::catalog::KitCreationSource;
  using ConditionEditorState = ui::conditions::editor::State;
  using WorkbenchFilterState = ui::workbench::FilterState;
  using WorkbenchFilterOption = ui::workbench::FilterOption;
  using WorkbenchFilterKind = ui::workbench::FilterKind;

  Menu() = default;
  friend class MenuHost;

  void ApplyStyle();
  void LoadUserSettings();
  void SaveUserSettings() const;
  void LoadFavorites();
  void SaveFavorites() const;
  void RefreshAvailableFonts();
  void NormalizeSelectedFontPath();
  void RebuildFontAtlas();
  void SyncAllowTextInput();
  void UpdateVisibilityAnimation(float a_deltaTime);
  void QueueHideMessage();
  void ApplySmoothScroll();
  void OnMenuShow();
  void OnMenuHide();
  void HandleCancel();
  void DrawWindow();
  void DrawCatalogWindow();
  void DrawCatalogHostControls(bool a_inPopout);
  void DrawCatalogHostBody(bool a_drawBodyChild);
  void DrawCatalogPaneBody();
  void QueueCatalogRefresh(
      ui::catalog::RefreshMode a_mode = ui::catalog::RefreshMode::Full);
  void UpdateCatalogRefresh();
  void DrawCatalogLoadingPane() const;
  void DrawCatalogFilters();
  [[nodiscard]] sosr::ui::components::EquipmentWidgetResult
  DrawCatalogDragWidget(const workbench::EquipmentWidgetItem &a_item,
                        DragSourceKind a_sourceKind);
  [[nodiscard]] bool DrawGearTab();
  [[nodiscard]] bool DrawGearCatalogTable();
  void DrawVariantWorkbenchPane();
  void DrawWorkbenchFilterBar();
  void DrawWorkbenchToolbar();
  void DrawWorkbenchEmptyState(const char *a_tableId, const char *a_targetId,
                               const char *a_message);
  void DrawWorkbenchTable(const std::vector<int> &a_visibleRowIndices);
  [[nodiscard]] bool DrawOutfitTab();
  [[nodiscard]] bool DrawKitTab();
  [[nodiscard]] bool DrawSlotTab();
  [[nodiscard]] bool DrawConditionTab();
  [[nodiscard]] bool DrawConditionCatalogTable();
  void DrawConditionLibraryTable();
  void DrawOptionsTab();
  void DrawConditionEditorDialog();
  [[nodiscard]] bool DrawConditionEditorClauseTable(
      ConditionEditorState &a_editor,
      const std::vector<ui::components::EditableDropdownItem<std::string>>
          &a_conditionFunctionItems,
      float a_editButtonWidth, float a_deleteButtonWidth,
      float a_actionsColumnWidth);
  void DrawCreateKitDialog();
  void DrawDeleteKitDialog();
  void AcceptOverridePayload(int a_targetRowIndex);
  bool ApplyWorkbenchRowDrop(const DraggedEquipmentPayload &a_dragPayload,
                             int a_targetRowIndex = -1,
                             bool a_insertAfter = false);
  void ApplyRowReorder(const DraggedEquipmentPayload &a_dragPayload,
                       int a_targetRowIndex, bool a_insertAfter);
  void ClearCatalogSelection();
  [[nodiscard]] std::string BuildFavoriteKey(ui::catalog::BrowserTab a_tab,
                                             std::string_view a_id) const;
  [[nodiscard]] bool IsFavorite(ui::catalog::BrowserTab a_tab,
                                std::string_view a_id) const;
  void SetFavorite(ui::catalog::BrowserTab a_tab, std::string_view a_id,
                   bool a_favorite);
  [[nodiscard]] std::string BuildFavoriteLabel(std::string_view a_name,
                                               bool a_favorite) const;
  void SyncSelectedSlotFilters();
  [[nodiscard]] bool HasAnySelectedSlotFilter() const;
  [[nodiscard]] bool
  MatchesSelectedSlotsOr(const std::vector<std::string> &a_slots) const;
  [[nodiscard]] bool MatchesSelectedSlotsAnd(std::uint64_t a_slotMask) const;
  [[nodiscard]] std::string BuildSelectedSlotPreview() const;

  [[nodiscard]] bool MatchesGearFilters(const GearEntry &a_entry) const;
  [[nodiscard]] bool MatchesOutfitFilters(const OutfitEntry &a_entry) const;
  [[nodiscard]] bool MatchesKitFilters(const KitEntry &a_entry) const;
  void AddGearEntryToWorkbench(const GearEntry &a_entry);
  void AddOutfitEntryToWorkbench(const OutfitEntry &a_entry,
                                 bool a_replaceExisting = true);
  void AddKitEntryToWorkbench(const KitEntry &a_entry,
                              bool a_replaceExisting = true);
  void
  OpenCreateKitDialog(KitCreationSource a_source,
                      const std::vector<int> *a_candidateRowIndices = nullptr);
  void OpenDeleteKitDialog(const KitEntry &a_entry);
  [[nodiscard]] bool SavePendingKit();
  [[nodiscard]] bool DeletePendingKit();
  void PreviewGearEntry(const GearEntry &a_entry);
  void PreviewOutfitEntry(const OutfitEntry &a_entry);
  void PreviewKitEntry(const KitEntry &a_entry);
  void SyncWorkbenchRowsForCurrentFilter();
  void ValidateWorkbenchFilterSelection();
  void EnsureWorkbenchDerivedState();
  void RebuildWorkbenchDerivedState();
  void BumpConditionStoreRevision();
  [[nodiscard]] bool IsWorkbenchFilterSelectionValid() const;
  [[nodiscard]] const std::vector<int> &BuildVisibleWorkbenchRowIndices();
  [[nodiscard]] bool
  MatchesWorkbenchFilter(const workbench::VariantWorkbenchRow &a_row);
  void ApplyInitialWorkbenchFilterSelection();
  void
  BuildWorkbenchFilterOptions(std::vector<WorkbenchFilterOption> &a_options);
  [[nodiscard]] std::optional<std::string>
  ResolveFirstConditionForActorFilter(RE::FormID a_actorFormID);
  [[nodiscard]] std::optional<std::string> ResolveNewWorkbenchRowConditionId();
  [[nodiscard]] RE::Actor *ResolveWorkbenchPreviewActor();
  [[nodiscard]] workbench::VariantWorkbench::InitialEquippedState
  BuildWorkbenchInitialEquippedState();
  void EnsureDefaultConditions();
  [[nodiscard]] int AllocateConditionEditorWindowSlot() const;
  void OpenNewConditionDialog();
  void OpenNewLibraryConditionDialog();
  void OpenConditionEditorDialog(std::size_t a_index);
  void OpenConditionEditorDialogById(std::string_view a_conditionId);
  [[nodiscard]] bool SaveConditionEditor(ConditionEditorState &a_editor);
  void
  ApplyLibraryChangeResult(const conditions::LibraryChangeResult &a_result);
  void LoadConditionLibrary();
  [[nodiscard]] std::size_t CountCatalogConditions() const;
  [[nodiscard]] std::size_t CountLibraryConditions() const;
  [[nodiscard]] bool IsWorkbenchSelectableCondition(
      const ui::conditions::Definition &a_condition) const;
  [[nodiscard]] std::vector<const GearEntry *> BuildFilteredGear() const;
  [[nodiscard]] std::vector<const OutfitEntry *> BuildFilteredOutfits() const;
  [[nodiscard]] std::vector<const KitEntry *> BuildFilteredKits() const;
  void SortGearRows(std::vector<const GearEntry *> &a_rows,
                    ImGuiTableSortSpecs *a_sortSpecs) const;
  void SortOutfitRows(std::vector<const OutfitEntry *> &a_rows,
                      ImGuiTableSortSpecs *a_sortSpecs) const;
  void SortKitRows(std::vector<const KitEntry *> &a_rows,
                   ImGuiTableSortSpecs *a_sortSpecs) const;
  [[nodiscard]] const std::vector<const GearEntry *> &GetFilteredGearRows();
  [[nodiscard]] const std::vector<const GearEntry *> &
  GetSortedGearRows(ImGuiTableSortSpecs *a_sortSpecs);
  [[nodiscard]] const std::vector<const OutfitEntry *> &GetFilteredOutfitRows();
  [[nodiscard]] const std::vector<const OutfitEntry *> &
  GetSortedOutfitRows(ImGuiTableSortSpecs *a_sortSpecs);
  [[nodiscard]] const std::vector<const KitEntry *> &GetFilteredKitRows();
  [[nodiscard]] const std::vector<const KitEntry *> &
  GetSortedKitRows(ImGuiTableSortSpecs *a_sortSpecs);
  void InvalidateCatalogDerivedState();
  [[nodiscard]] ui::catalog::BrowserState &CatalogBrowserState() {
    return catalogPane_.browser;
  }
  [[nodiscard]] const ui::catalog::BrowserState &CatalogBrowserState() const {
    return catalogPane_.browser;
  }
  [[nodiscard]] ui::catalog::CreateKitDialogState &CreateKitDialogState() {
    return catalogPane_.createKitDialog;
  }
  [[nodiscard]] ui::catalog::DeleteKitDialogState &DeleteKitDialogState() {
    return catalogPane_.deleteKitDialog;
  }
  [[nodiscard]] ui::conditions::PaneState &ConditionsPaneState() {
    return conditionsPane_;
  }
  [[nodiscard]] const ui::conditions::PaneState &ConditionsPaneState() const {
    return conditionsPane_;
  }
  [[nodiscard]] std::vector<ui::conditions::Definition> &
  ConditionDefinitions() {
    return conditionStore_.definitions;
  }
  [[nodiscard]] const std::vector<ui::conditions::Definition> &
  ConditionDefinitions() const {
    return conditionStore_.definitions;
  }
  [[nodiscard]] int &NextConditionId() {
    return conditionStore_.nextConditionId;
  }
  [[nodiscard]] const int &NextConditionId() const {
    return conditionStore_.nextConditionId;
  }
  [[nodiscard]] std::vector<ConditionEditorState> &ConditionEditors() {
    return conditionsPane_.editors;
  }
  [[nodiscard]] const std::vector<ConditionEditorState> &
  ConditionEditors() const {
    return conditionsPane_.editors;
  }
  [[nodiscard]] int &FocusedConditionEditorWindowSlot() {
    return conditionsPane_.focusedEditorWindowSlot;
  }
  [[nodiscard]] const int &FocusedConditionEditorWindowSlot() const {
    return conditionsPane_.focusedEditorWindowSlot;
  }

  bool initialized_{false};
  bool enabled_{false};
  bool gameDataLoaded_{false};
  ID3D11Device *device_{nullptr};
  ID3D11DeviceContext *context_{nullptr};
  ui::catalog::PaneState catalogPane_;
  ui::conditions::PaneState conditionsPane_;
  conditions::Store conditionStore_;
  WorkbenchFilterState workbenchFilter_;
  int fontSizePixels_{18};
  int pendingFontSizePixels_{18};
  std::string fontPath_{kDefaultFontPath};
  bool pendingFontAtlasRebuild_{false};
  bool pauseGameWhenOpen_{false};
  bool smoothScroll_{true};
  std::uint32_t toggleKey_{0x40};
  std::uint32_t toggleModifier_{0};
  std::string themeName_{"default"};
  std::string toggleKeyCaptureError_;
  bool awaitingToggleKeyCapture_{false};
  bool openToggleKeyPopup_{false};
  bool wantTextInput_{false};
  bool hideMessageQueued_{false};
  VisibilityState visibilityState_{VisibilityState::Closed};
  float windowAlpha_{0.0f};
  float pendingSmoothWheelDelta_{0.0f};
  ImGuiID smoothScrollWindowId_{0};
  float smoothScrollTargetY_{0.0f};
  std::string settingsDirectory_;
  std::string imguiIniPath_;
  std::string userSettingsPath_;
  std::string favoritesPath_;
  std::vector<FontOption> bundledFontOptions_;
  std::vector<FontOption> systemFontOptions_;
  workbench::VariantWorkbench workbench_;
  struct WorkbenchDerivedState {
    std::uint64_t workbenchRevision{0};
    std::uint64_t conditionRevision{0};
    bool revisionsInitialized{false};
    WorkbenchFilterState filterState{};
    bool filterStateInitialized{false};
    std::vector<WorkbenchFilterOption> filterOptions;
    std::vector<ui::workbench::RowConditionVisualState> rowConditionStates;
    std::vector<int> visibleRowIndices;
    ui::workbench_conflicts::ConflictState conflictState;
  };
  WorkbenchDerivedState workbenchDerived_;
  ui::catalog::DerivedState catalogDerived_;
};
} // namespace sosr
