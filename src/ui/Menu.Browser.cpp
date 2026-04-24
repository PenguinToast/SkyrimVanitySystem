#include "Menu.h"

#include "PlayerInventory.h"
#include "ui/Localization.h"
#include "ui/catalog/Widgets.h"

#include <format>

namespace {
constexpr std::string_view kFavoritePrefix = "\xEE\x83\xB5 ";
constexpr char kIconPanelRightOpen[] =
    "\xee\x90\xb8"; // ICON_LC_PANEL_RIGHT_OPEN
constexpr char kIconPanelRightClose[] =
    "\xee\x90\xb6"; // ICON_LC_PANEL_RIGHT_CLOSE
} // namespace

namespace sosr {
void Menu::ClearCatalogSelection() {
  CatalogBrowserState().selectedKey.clear();
  workbench_.ClearPreview();
}

std::string Menu::BuildFavoriteKey(const ui::catalog::BrowserTab a_tab,
                                   const std::string_view a_id) const {
  std::string prefix;
  switch (a_tab) {
  case ui::catalog::BrowserTab::Gear:
    prefix = "gear:";
    break;
  case ui::catalog::BrowserTab::Outfits:
    prefix = "outfit:";
    break;
  case ui::catalog::BrowserTab::Kits:
    prefix = "kit:";
    break;
  case ui::catalog::BrowserTab::Slots:
    prefix = "slot:";
    break;
  case ui::catalog::BrowserTab::Conditions:
    prefix = "condition:";
    break;
  case ui::catalog::BrowserTab::Options:
    prefix = "options:";
    break;
  }

  return prefix + std::string(a_id);
}

bool Menu::IsFavorite(const ui::catalog::BrowserTab a_tab,
                      const std::string_view a_id) const {
  return CatalogBrowserState().favoriteKeys.contains(
      BuildFavoriteKey(a_tab, a_id));
}

void Menu::SetFavorite(const ui::catalog::BrowserTab a_tab,
                       const std::string_view a_id, const bool a_favorite) {
  const auto key = BuildFavoriteKey(a_tab, a_id);
  if (a_favorite) {
    CatalogBrowserState().favoriteKeys.insert(key);
  } else {
    CatalogBrowserState().favoriteKeys.erase(key);
    if (CatalogBrowserState().favoritesOnly &&
        CatalogBrowserState().activeTab == a_tab &&
        CatalogBrowserState().selectedKey == a_id) {
      ClearCatalogSelection();
    }
  }
  ++catalogDerived_.favoritesRevision;
  SaveFavorites();
}

std::string Menu::BuildFavoriteLabel(const std::string_view a_name,
                                     const bool a_favorite) const {
  if (!a_favorite) {
    return std::string(a_name);
  }

  return std::string(kFavoritePrefix) + std::string(a_name);
}

void Menu::QueueCatalogRefresh(const ui::catalog::RefreshMode a_mode) {
  auto &browser = CatalogBrowserState();
  browser.refreshQueued = true;
  browser.queuedRefreshMode = a_mode;
  if (a_mode == ui::catalog::RefreshMode::Full) {
    browser.initialized = false;
    browser.pendingSelectionAfterRefresh.clear();
    ClearCatalogSelection();
  }
  InvalidateCatalogDerivedState();
}

void Menu::UpdateCatalogRefresh() {
  auto &browser = CatalogBrowserState();
  auto &catalog = EquipmentCatalog::Get();
  if (browser.refreshQueued && !catalog.IsRefreshing()) {
    catalog.StartRefreshFromGame(browser.queuedRefreshMode ==
                                         ui::catalog::RefreshMode::KitsOnly
                                     ? EquipmentCatalog::RefreshMode::KitsOnly
                                     : EquipmentCatalog::RefreshMode::Full);
    browser.refreshQueued = false;
  }

  if (!catalog.IsRefreshing()) {
    return;
  }

  if (!catalog.ContinueRefreshFromGame(16.0)) {
    browser.initialized = true;
    if (!browser.pendingSelectionAfterRefresh.empty()) {
      browser.selectedKey = browser.pendingSelectionAfterRefresh;
      browser.pendingSelectionAfterRefresh.clear();
    }
  }
}

void Menu::DrawCatalogLoadingPane() const {
  const auto &catalog = EquipmentCatalog::Get();
  const auto avail = ImGui::GetContentRegionAvail();
  const auto barWidth = (std::min)(avail.x, 420.0f);
  const auto progress = std::clamp(catalog.GetRefreshProgress(), 0.0f, 1.0f);
  const auto status = catalog.GetRefreshStatus();

  ImGui::Dummy(ImVec2(0.0f, (std::max)(avail.y * 0.30f, 0.0f)));
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + barWidth);
  ImGui::TextUnformatted(status.data(), status.data() + status.size());
  ImGui::PopTextWrapPos();
  ImGui::Spacing();
  ImGui::ProgressBar(progress, ImVec2(barWidth, 0.0f));
  ImGui::Spacing();
  ImGui::TextDisabled("%.0f%%", progress * 100.0f);
}

void Menu::AddGearEntryToWorkbench(const GearEntry &a_entry) {
  const auto &visibleRowIndices = BuildVisibleWorkbenchRowIndices();
  workbench_.AddCatalogSelectionToWorkbench(
      std::vector<RE::FormID>{a_entry.formID}, &visibleRowIndices);
}

void Menu::AddOutfitEntryToWorkbench(const OutfitEntry &a_entry,
                                     const bool a_replaceExisting) {
  const auto &visibleRowIndices = BuildVisibleWorkbenchRowIndices();
  if (a_replaceExisting) {
    workbench_.ReplaceCatalogSelectionInWorkbench(a_entry.GetArmorFormIDs(),
                                                  &visibleRowIndices);
  } else {
    workbench_.AddCatalogSelectionToWorkbench(a_entry.GetArmorFormIDs(),
                                              &visibleRowIndices);
  }
}

void Menu::PreviewGearEntry(const GearEntry &a_entry) {
  const auto &visibleRowIndices = BuildVisibleWorkbenchRowIndices();
  workbench_.ApplyCatalogPreview(
      a_entry.id, std::vector<RE::FormID>{a_entry.formID},
      ResolveWorkbenchPreviewActor(), &visibleRowIndices);
}

void Menu::PreviewOutfitEntry(const OutfitEntry &a_entry) {
  const auto &visibleRowIndices = BuildVisibleWorkbenchRowIndices();
  workbench_.ApplyCatalogPreview(a_entry.id, a_entry.GetArmorFormIDs(),
                                 ResolveWorkbenchPreviewActor(),
                                 &visibleRowIndices);
}

void Menu::DrawCatalogHostControls(const bool) {
  auto &browser = CatalogBrowserState();
  auto *localization = ui::Localization::GetSingleton();
  const auto browserLabel = localization->Get("nav.browser");
  const auto optionsLabel = localization->Get("nav.options");
  const auto &style = ImGui::GetStyle();
  const auto optionsButtonWidth =
      ImGui::CalcTextSize(optionsLabel.data()).x +
      (style.FramePadding.x * 2.0f);
  const auto browserButtonWidth =
      ImGui::CalcTextSize(browserLabel.data()).x +
      (style.FramePadding.x * 2.0f);
  const auto browserOptionsWidth =
      browserButtonWidth + optionsButtonWidth + style.ItemSpacing.x;
  const auto browserOptionsStartX =
      ImGui::GetWindowContentRegionMax().x - browserOptionsWidth;
  if (browserOptionsStartX > ImGui::GetCursorPosX()) {
    ImGui::SameLine(browserOptionsStartX);
  } else {
    ImGui::SameLine();
  }

  if (ImGui::Selectable(browserLabel.data(),
                        browser.activeTab != ui::catalog::BrowserTab::Options,
                        0, ImVec2(browserButtonWidth, 0.0f)) &&
      browser.activeTab == ui::catalog::BrowserTab::Options) {
    browser.activeTab = ui::catalog::BrowserTab::Gear;
  }
  ImGui::SameLine();
  if (ImGui::Selectable(optionsLabel.data(),
                        browser.activeTab == ui::catalog::BrowserTab::Options,
                        0, ImVec2(optionsButtonWidth, 0.0f))) {
    browser.activeTab = ui::catalog::BrowserTab::Options;
  }
}

void Menu::DrawCatalogPaneBody() {
  auto &browser = CatalogBrowserState();
  bool catalogRowClicked = false;
  if (browser.activeTab != ui::catalog::BrowserTab::Conditions &&
      EquipmentCatalog::Get().IsRefreshing()) {
    DrawCatalogLoadingPane();
  } else {
    if (browser.activeTab == ui::catalog::BrowserTab::Gear) {
      catalogRowClicked = DrawGearTab();
    } else if (browser.activeTab == ui::catalog::BrowserTab::Outfits) {
      catalogRowClicked = DrawOutfitTab();
    } else if (browser.activeTab == ui::catalog::BrowserTab::Kits) {
      catalogRowClicked = DrawKitTab();
    } else if (browser.activeTab == ui::catalog::BrowserTab::Conditions) {
      catalogRowClicked = DrawConditionTab();
    } else {
      catalogRowClicked = DrawSlotTab();
    }

    if (!browser.selectedKey.empty() &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !catalogRowClicked) {
      ClearCatalogSelection();
    }
  }
}

void Menu::DrawCatalogHostBody(const bool a_drawBodyChild) {
  auto &browser = CatalogBrowserState();
  const auto davAvailabilityMessage = ui::catalog::GetDavAvailabilityMessage();
  if (davAvailabilityMessage) {
    ui::catalog::DrawDavAvailabilityBanner(*davAvailabilityMessage);
    ImGui::Spacing();
  }

  if (browser.activeTab == ui::catalog::BrowserTab::Options) {
    if (a_drawBodyChild) {
      DrawOptionsTab();
    }
    return;
  } else {
    UpdateCatalogRefresh();
    const auto applySelectedPreview = [&]() {
      if (EquipmentCatalog::Get().IsRefreshing()) {
        return;
      }
      if (!browser.previewSelected || browser.selectedKey.empty()) {
        return;
      }

      if (browser.activeTab == ui::catalog::BrowserTab::Gear) {
        const auto entry = std::ranges::find(
            EquipmentCatalog::Get().GetGear(), browser.selectedKey,
            [](const GearEntry &a_entry) { return a_entry.id; });
        if (entry != EquipmentCatalog::Get().GetGear().end()) {
          PreviewGearEntry(*entry);
        }
      } else if (browser.activeTab == ui::catalog::BrowserTab::Outfits) {
        const auto entry = std::ranges::find(
            EquipmentCatalog::Get().GetOutfits(), browser.selectedKey,
            [](const OutfitEntry &a_entry) { return a_entry.id; });
        if (entry != EquipmentCatalog::Get().GetOutfits().end()) {
          PreviewOutfitEntry(*entry);
        }
      } else if (browser.activeTab == ui::catalog::BrowserTab::Kits) {
        const auto entry = std::ranges::find(
            EquipmentCatalog::Get().GetKits(), browser.selectedKey,
            [](const KitEntry &a_entry) { return a_entry.id; });
        if (entry != EquipmentCatalog::Get().GetKits().end()) {
          PreviewKitEntry(*entry);
        }
      } else if (browser.activeTab == ui::catalog::BrowserTab::Slots ||
                 browser.activeTab == ui::catalog::BrowserTab::Conditions) {
        workbench_.ClearPreview();
      }
    };

    const bool inPopout =
        catalogPane_.hostMode == ui::catalog::HostMode::Popout;
    const char *hostToggleIcon =
        inPopout ? kIconPanelRightClose : kIconPanelRightOpen;
    auto *localization = ui::Localization::GetSingleton();
    const auto gearTabLabel = localization->Get("tabs.gear");
    const auto outfitsTabLabel = localization->Get("tabs.outfits");
    const auto kitsTabLabel = localization->Get("tabs.kits");
    const auto slotsTabLabel = localization->Get("tabs.equipment_slots");
    const auto conditionsTabLabel = localization->Get("tabs.conditions");
    const auto favoritesOnlyLabel = localization->Get("catalog.favorites_only");
    const auto inventoryOnlyLabel = localization->Get("catalog.inventory_only");
    const auto hideUnnamedLabel = localization->Get("catalog.hide_unnamed");
    const auto showAllLabel = localization->Get("catalog.show_all");
    const auto previewSelectedLabel =
        localization->Get("catalog.preview_selected");
    if (ImGui::BeginTabBar("##catalog-tabs")) {
      const bool gearTabOpen = ImGui::BeginTabItem(gearTabLabel.data());
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:gear-tab", ui::catalog::IsDelayedHover(),
          {localization->Get("help.gear.1").data(),
           localization->Get("help.gear.2").data(),
           localization->Get("help.gear.3").data()});
      if (gearTabOpen) {
        if (browser.activeTab != ui::catalog::BrowserTab::Gear) {
          ClearCatalogSelection();
        }
        browser.activeTab = ui::catalog::BrowserTab::Gear;
        ImGui::EndTabItem();
      }

      const bool outfitsTabOpen = ImGui::BeginTabItem(outfitsTabLabel.data());
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:outfits-tab", ui::catalog::IsDelayedHover(),
          {localization->Get("help.outfits.1").data(),
           localization->Get("help.outfits.2").data()});
      if (outfitsTabOpen) {
        if (browser.activeTab != ui::catalog::BrowserTab::Outfits) {
          ClearCatalogSelection();
        }
        browser.activeTab = ui::catalog::BrowserTab::Outfits;
        ImGui::EndTabItem();
      }

      const bool kitsTabOpen = ImGui::BeginTabItem(kitsTabLabel.data());
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:kits-tab", ui::catalog::IsDelayedHover(),
          {localization->Get("help.kits.1").data(),
           localization->Get("help.kits.2").data(),
           localization->Get("help.kits.3").data()});
      if (kitsTabOpen) {
        if (browser.activeTab != ui::catalog::BrowserTab::Kits) {
          ClearCatalogSelection();
        }
        browser.activeTab = ui::catalog::BrowserTab::Kits;
        ImGui::EndTabItem();
      }

      const bool slotsTabOpen = ImGui::BeginTabItem(slotsTabLabel.data());
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:slots-tab", ui::catalog::IsDelayedHover(),
          {localization->Get("help.slots.1").data(),
           localization->Get("help.slots.2").data(),
           localization->Get("help.slots.3").data(),
           localization->Get("help.slots.4").data()});
      if (slotsTabOpen) {
        if (browser.activeTab != ui::catalog::BrowserTab::Slots) {
          ClearCatalogSelection();
        }
        browser.activeTab = ui::catalog::BrowserTab::Slots;
        ImGui::EndTabItem();
      }

      const bool conditionsTabOpen = ImGui::BeginTabItem(conditionsTabLabel.data());
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:conditions-tab", ui::catalog::IsDelayedHover(),
          {localization->Get("help.conditions.1").data(),
           localization->Get("help.conditions.2").data(),
           localization->Get("help.conditions.3").data()});
      if (conditionsTabOpen) {
        if (browser.activeTab != ui::catalog::BrowserTab::Conditions) {
          ClearCatalogSelection();
        }
        browser.activeTab = ui::catalog::BrowserTab::Conditions;
        ImGui::EndTabItem();
      }

      const bool hostToggleHovered =
          ImGui::TabItemButton(hostToggleIcon, ImGuiTabItemFlags_Trailing);
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:host-toggle", ImGui::IsItemHovered(),
          {inPopout ? localization->Get("help.host_toggle.docked").data()
                    : localization->Get("help.host_toggle.popout").data()});
      if (hostToggleHovered) {
        if (inPopout) {
          catalogPane_.hostMode = ui::catalog::HostMode::Docked;
          catalogPane_.popoutOpen = false;
        } else {
          catalogPane_.hostMode = ui::catalog::HostMode::Popout;
          catalogPane_.popoutOpen = true;
        }
        SaveUserSettings();
      }

      ImGui::EndTabBar();
    }

    ImGui::Separator();
    DrawCatalogFilters();
    if (browser.activeTab != ui::catalog::BrowserTab::Slots &&
        browser.activeTab != ui::catalog::BrowserTab::Conditions) {
      if (ImGui::Checkbox(favoritesOnlyLabel.data(), &browser.favoritesOnly)) {
        if (browser.favoritesOnly && !browser.selectedKey.empty() &&
            !IsFavorite(browser.activeTab, browser.selectedKey)) {
          ClearCatalogSelection();
        }
        SaveUserSettings();
      }
    }
    if (browser.activeTab == ui::catalog::BrowserTab::Gear) {
      ImGui::SameLine();
      if (ImGui::Checkbox(inventoryOnlyLabel.data(), &browser.inventoryOnly) &&
          browser.inventoryOnly && !browser.selectedKey.empty()) {
        const auto &catalog = EquipmentCatalog::Get().GetGear();
        const auto selectedIt =
            std::ranges::find(catalog, browser.selectedKey, &GearEntry::id);
        const auto inventoryFormIDs =
            player_inventory::GetInventoryArmorFormIDs();
        if (selectedIt != catalog.end() &&
            !inventoryFormIDs.contains(selectedIt->formID)) {
          ClearCatalogSelection();
        }
        SaveUserSettings();
      }

      ImGui::SameLine();
      if (ImGui::Checkbox(hideUnnamedLabel.data(), &browser.hideUnnamedGear) &&
          browser.hideUnnamedGear && !browser.selectedKey.empty()) {
        const auto &catalog = EquipmentCatalog::Get().GetGear();
        const auto selectedIt =
            std::ranges::find(catalog, browser.selectedKey, &GearEntry::id);
        if (selectedIt != catalog.end() && !selectedIt->hasDisplayName) {
          ClearCatalogSelection();
        }
        SaveUserSettings();
      }
    }
    if (browser.activeTab == ui::catalog::BrowserTab::Slots) {
      if (ImGui::Checkbox(showAllLabel.data(), &browser.showAllSlots)) {
        SaveUserSettings();
      }
      ui::catalog::DrawCatalogTabHelpTooltip(
          "catalog:slots-show-all", ui::catalog::IsDelayedHover(0.55f),
          {localization->Get("help.show_all_slots.1").data(),
           localization->Get("help.show_all_slots.2").data()});
    }
    if (browser.activeTab != ui::catalog::BrowserTab::Slots &&
        browser.activeTab != ui::catalog::BrowserTab::Conditions) {
      ImGui::SameLine();
      if (ImGui::Checkbox(previewSelectedLabel.data(), &browser.previewSelected)) {
        if (!browser.previewSelected) {
          workbench_.ClearPreview();
        } else {
          applySelectedPreview();
        }
        SaveUserSettings();
      }
    }
    if (a_drawBodyChild &&
        ImGui::BeginChild("##catalog-pane", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_Borders)) {
      DrawCatalogPaneBody();
      ImGui::EndChild();
    } else if (a_drawBodyChild) {
      ImGui::EndChild();
    }
  }
}

void Menu::DrawCatalogWindow() {
  if (catalogPane_.hostMode != ui::catalog::HostMode::Popout) {
    return;
  }
  auto *localization = ui::Localization::GetSingleton();
  const auto catalogWindowTitle =
      localization->Get("window.catalog");

  auto &io = ImGui::GetIO();
  ImGui::SetNextWindowSize(
      ImVec2(io.DisplaySize.x * 0.42f, io.DisplaySize.y * 0.70f),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(
      ImVec2(io.DisplaySize.x * 0.32f, io.DisplaySize.y * 0.52f),
      ImGuiCond_FirstUseEver, ImVec2(0.50f, 0.50f));

  bool popoutOpen = catalogPane_.popoutOpen;
  if (!ImGui::Begin(catalogWindowTitle.data(), &popoutOpen,
                    ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    catalogPane_.popoutOpen = popoutOpen;
    if (!catalogPane_.popoutOpen) {
      catalogPane_.hostMode = ui::catalog::HostMode::Docked;
      SaveUserSettings();
    }
    return;
  }
  catalogPane_.popoutOpen = popoutOpen;

  DrawCatalogHostBody(true);
  ImGui::End();

  if (!catalogPane_.popoutOpen) {
    catalogPane_.hostMode = ui::catalog::HostMode::Docked;
    SaveUserSettings();
  }
}

void Menu::DrawWindow() {
  auto *localization = ui::Localization::GetSingleton();
  const auto windowTitle = localization->Get("window.main");
  const auto browserTitle = localization->Get("window.browser_title");
  const auto toggleKeyLabel = GetToggleKeyLabel();
  const auto toggleHint = std::vformat(
      std::string(localization->Get("window.toggle_hint")),
      std::make_format_args(toggleKeyLabel));
  const auto catalogColumn = localization->Get("window.catalog_column");
  const auto variantsColumn = localization->Get("window.variants_column");
  auto &io = ImGui::GetIO();
  ImGui::SetNextWindowSize(
      ImVec2(io.DisplaySize.x * 0.50f, io.DisplaySize.y * 0.50f),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(
      ImVec2(io.DisplaySize.x * 0.50f, io.DisplaySize.y * 0.50f),
      ImGuiCond_FirstUseEver, ImVec2(0.50f, 0.50f));

  bool open = enabled_;
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, windowAlpha_);
  if (!ImGui::Begin(windowTitle.data(), &open,
                    ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    DrawCatalogWindow();
    DrawCreateKitDialog();
    DrawDeleteKitDialog();
    DrawApplyWithConditionOverridesDialog();
    DrawConditionEditorDialog();
    ImGui::PopStyleVar();
    if (!open) {
      Close();
    }
    return;
  }

  ImGui::TextUnformatted(browserTitle.data());
  ImGui::SameLine();
  ImGui::TextDisabled("%s", toggleHint.c_str());
  DrawCatalogHostControls(false);
  ImGui::Separator();
  DrawCatalogWindow();

  if (catalogPane_.hostMode == ui::catalog::HostMode::Docked) {
    if (CatalogBrowserState().activeTab == ui::catalog::BrowserTab::Options) {
      DrawCatalogHostBody(false);
      if (ImGui::BeginChild("##options-pane", ImVec2(0.0f, 0.0f),
                            ImGuiChildFlags_Borders)) {
        DrawOptionsTab();
      }
      ImGui::EndChild();
    } else {
      DrawCatalogHostBody(false);
      if (ImGui::BeginTable("##browser-layout", 2,
                            ImGuiTableFlags_Resizable |
                                ImGuiTableFlags_SizingStretchProp,
                            ImVec2(0.0f, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupColumn(catalogColumn.data(), ImGuiTableColumnFlags_WidthStretch,
                                1.20f);
        ImGui::TableSetupColumn(variantsColumn.data(), ImGuiTableColumnFlags_WidthStretch,
                                0.95f);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginChild("##catalog-pane", ImVec2(0.0f, 0.0f),
                              ImGuiChildFlags_Borders)) {
          DrawCatalogPaneBody();
        }
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginChild("##variant-pane", ImVec2(0.0f, 0.0f),
                              ImGuiChildFlags_Borders)) {
          DrawVariantWorkbenchPane();
        }
        ImGui::EndChild();

        ImGui::EndTable();
      }
    }
  } else {
    if (CatalogBrowserState().activeTab == ui::catalog::BrowserTab::Options) {
      if (ImGui::BeginChild("##options-pane", ImVec2(0.0f, 0.0f),
                            ImGuiChildFlags_Borders)) {
        DrawOptionsTab();
      }
      ImGui::EndChild();
    } else {
      if (ImGui::BeginChild("##variant-pane", ImVec2(0.0f, 0.0f),
                            ImGuiChildFlags_Borders)) {
        DrawVariantWorkbenchPane();
      }
      ImGui::EndChild();
    }
  }

  workbench_.SyncDynamicArmorVariantsExtended(ConditionDefinitions());

  DrawCreateKitDialog();
  DrawDeleteKitDialog();
  DrawApplyWithConditionOverridesDialog();
  DrawConditionEditorDialog();

  ImGui::End();
  ImGui::PopStyleVar();

  if (!open) {
    Close();
  }
}
} // namespace sosr
