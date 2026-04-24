#include "Menu.h"

#include "ArmorUtils.h"
#include "StringUtils.h"
#include "Utf8Path.h"
#include "catalog/KitLayoutMetadata.h"
#include "imgui_internal.h"
#include "ui/Localization.h"
#include "ui/catalog/Widgets.h"
#include "ui/components/EditableCombo.h"
#include "ui/components/PinnableTooltip.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <format>
#include <nlohmann/json.hpp>

namespace {
constexpr auto kModexKitDirectory = "data/interface/modex/user/kits";
enum class KitColumn : ImGuiID { Name = 1, Collection, Pieces };

std::string NormalizeKitCollection(std::string_view a_collection) {
  auto normalized = sosr::strings::TrimText(a_collection);
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  while (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  while (!normalized.empty() && normalized.back() == '/') {
    normalized.pop_back();
  }
  return normalized;
}

} // namespace

namespace sosr {
bool Menu::DrawKitTab() {
  auto *localization = ui::Localization::GetSingleton();
  const auto &browser = CatalogBrowserState();
  const auto &rows = GetFilteredKitRows();
  const auto resultCount = rows.size();
  const auto resultsLabel = std::vformat(
      std::string(localization->Get("catalog.results")),
      std::make_format_args(resultCount));
  ImGui::TextUnformatted(resultsLabel.c_str());
  bool rowClicked = false;

  const auto tableHeight = ImGui::GetContentRegionAvail().y;
  if (ImGui::BeginTable("##kit-table", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, tableHeight))) {
    const auto kitLabel = localization->Get("common.kit");
    const auto collectionLabel = localization->Get("common.collection");
    const auto piecesLabel = localization->Get("common.pieces");
    const auto removeFavoriteLabel = localization->Get("favorites.remove");
    const auto addFavoriteLabel = localization->Get("favorites.add");
    const auto addWorkbenchLabel = localization->Get("workbench.add");
    const auto addOverrideLabel = localization->Get("workbench.add_override");
    const auto appendOverridesLabel =
        localization->Get("workbench.append_overrides");
    const auto deleteKitLabel = localization->Get("kits.delete");
    const auto rootLabel = localization->Get("common.root");
    ImGui::TableSetupColumn(kitLabel.data(), ImGuiTableColumnFlags_DefaultSort, 0.0f,
                            static_cast<ImGuiID>(KitColumn::Name));
    ImGui::TableSetupColumn(collectionLabel.data(), ImGuiTableColumnFlags_None, 0.0f,
                            static_cast<ImGuiID>(KitColumn::Collection));
    ImGui::TableSetupColumn(piecesLabel.data(),
                            ImGuiTableColumnFlags_PreferSortDescending, 0.0f,
                            static_cast<ImGuiID>(KitColumn::Pieces));
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    const auto &sortedRows = GetSortedKitRows(ImGui::TableGetSortSpecs());

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(sortedRows.size()));
    while (clipper.Step()) {
      for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd;
           ++rowIndex) {
        const auto &kit = *sortedRows[static_cast<std::size_t>(rowIndex)];
        const auto favorite = IsFavorite(ui::catalog::BrowserTab::Kits, kit.id);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const auto rowContentPos = ImGui::GetCursorScreenPos();
        const auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
        const bool selected = browser.selectedKey == kit.id &&
                              (!browser.previewSelected ||
                               workbench_.IsPreviewingSelection(kit.id));
        ImGui::Selectable(
            ("##kit-row-hit-" + std::to_string(rowIndex)).c_str(), selected,
            ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap |
                ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(0.0f, rowHeight));
        const bool rowHovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor(3);
        if (ImGui::BeginPopupContextItem()) {
          const auto favoriteLabel =
              favorite ? removeFavoriteLabel.data() : addFavoriteLabel.data();
          if (ImGui::MenuItem(favoriteLabel)) {
            SetFavorite(ui::catalog::BrowserTab::Kits, kit.id, !favorite);
          }
          ImGui::Separator();
          if (ImGui::MenuItem(addWorkbenchLabel.data())) {
            const auto initialEquippedState = BuildWorkbenchInitialEquippedState();
            workbench_.AddCatalogSelectionAsRows(
                kit.GetArmorFormIDs(), ResolveNewWorkbenchRowConditionId(),
                &initialEquippedState);
          }
          ImGui::Separator();
          if (ImGui::MenuItem(addOverrideLabel.data())) {
            AddKitEntryToWorkbench(kit, true);
          }
          if (ImGui::MenuItem(appendOverridesLabel.data())) {
            AddKitEntryToWorkbench(kit, false);
          }
          ConditionOverrideApplicationSource source{
              .name = kit.name,
              .formIDs = kit.GetArmorFormIDs(),
              .layout = kit.GetLayout() != nullptr
                            ? std::optional<KitEntry::Layout>(*kit.GetLayout())
                            : std::nullopt};
          DrawApplyWithConditionMenu(source);
          ImGui::Separator();
          ImGui::PushStyleColor(
              ImGuiCol_Text,
              ImGui::ColorConvertU32ToFloat4(
                  ThemeConfig::GetSingleton()->GetColorU32("DECLINE")));
          if (ImGui::MenuItem(deleteKitLabel.data())) {
            OpenDeleteKitDialog(kit);
          }
          ImGui::PopStyleColor();
          ImGui::EndPopup();
        }
        ImGui::SetCursorScreenPos(rowContentPos);

        if (selected) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("PRIMARY", 0.40f));
        } else if (rowHovered) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("TABLE_HOVER", 0.12f));
        }

        const bool doubleClicked =
            rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        const bool releasedOnRow =
            ImGui::IsMouseReleased(ImGuiMouseButton_Left) && rowHovered;
        if (doubleClicked) {
          rowClicked = true;
          AddKitEntryToWorkbench(kit, true);
        } else if (releasedOnRow) {
          rowClicked = true;
          if (selected) {
            ClearCatalogSelection();
          } else {
            CatalogBrowserState().selectedKey = kit.id;
            if (browser.previewSelected) {
              PreviewKitEntry(kit);
            } else {
              workbench_.ClearPreview();
            }
          }
        }
        if (!ImGui::IsDragDropActive() &&
            ui::components::ShouldDrawPinnableTooltip("kit:" + kit.id,
                                                      rowHovered)) {
          ui::catalog::DrawKitTooltip(kit, rowHovered);
        }

        const auto displayName = BuildFavoriteLabel(kit.name, favorite);
        ImGui::TextUnformatted(displayName.c_str());

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(kit.collection.empty() ? rootLabel.data()
                                                      : kit.collection.c_str());

        ImGui::TableSetColumnIndex(2);
        const auto availableWidth = ImGui::GetContentRegionAvail().x;
        const auto displayText = ui::catalog::TruncateTextToWidth(
            kit.GetPiecesText(), availableWidth);
        ImGui::TextUnformatted(displayText.c_str());
      }
    }

    ImGui::EndTable();
  }

  return rowClicked;
}

void Menu::AddKitEntryToWorkbench(const KitEntry &a_entry,
                                  const bool a_replaceExisting) {
  const auto &visibleRowIndices = BuildVisibleWorkbenchRowIndices();
  const auto initialEquippedState = BuildWorkbenchInitialEquippedState();
  if (const auto *layout = a_entry.GetLayout(); layout != nullptr) {
    workbench_.ApplyKitLayout(*layout, a_replaceExisting,
                              ResolveNewWorkbenchRowConditionId(),
                              &initialEquippedState,
                              &visibleRowIndices);
    return;
  }

  if (a_replaceExisting) {
    workbench_.ReplaceCatalogSelectionInWorkbench(a_entry.GetArmorFormIDs(),
                                                  &visibleRowIndices);
  } else {
    workbench_.AddCatalogSelectionToWorkbench(a_entry.GetArmorFormIDs(),
                                              &visibleRowIndices);
  }
}

void Menu::OpenCreateKitDialog(const KitCreationSource a_source,
                               const std::vector<int> *a_candidateRowIndices) {
  auto &createDialog = CreateKitDialogState();
  createDialog.pendingFormIDs.clear();
  createDialog.pendingLayout.reset();
  if (a_source == KitCreationSource::Equipped) {
    createDialog.pendingFormIDs =
        workbench_.CollectEquippedArmorFormIDs(a_candidateRowIndices);
    createDialog.pendingLayout =
        workbench_.CaptureEquippedKitLayout(a_candidateRowIndices);
  } else {
    createDialog.pendingFormIDs =
        workbench_.CollectOverrideArmorFormIDsFromEquippedRows(
            a_candidateRowIndices);
    createDialog.pendingLayout =
        workbench_.CaptureKitLayout(a_candidateRowIndices);
  }

  if (createDialog.pendingFormIDs.empty() && !createDialog.pendingLayout) {
    return;
  }

  createDialog.source = a_source;
  createDialog.pendingName.fill('\0');
  createDialog.pendingCollection.fill('\0');
  createDialog.error.clear();
  createDialog.openRequested = true;
}

void Menu::OpenDeleteKitDialog(const KitEntry &a_entry) {
  auto &deleteDialog = DeleteKitDialogState();
  deleteDialog.pendingKitId = a_entry.id;
  deleteDialog.pendingKitName = a_entry.name;
  deleteDialog.pendingKitPath = a_entry.filepath;
  deleteDialog.error.clear();
  deleteDialog.openRequested = true;
}

bool Menu::SavePendingKit() {
  const auto *localization = ui::Localization::GetSingleton();
  auto &createDialog = CreateKitDialogState();
  const auto name = sosr::strings::TrimText(createDialog.pendingName.data());
  if (name.empty()) {
    createDialog.error =
        std::string(localization->Get("kits.create_error.name_required"));
    return false;
  }
  if (name.find_first_of("\"'") != std::string::npos) {
    createDialog.error =
        std::string(localization->Get("kits.create_error.name_quotes"));
    return false;
  }
  if (name.find_first_of("/\\") != std::string::npos) {
    createDialog.error =
        std::string(localization->Get("kits.create_error.name_path"));
    return false;
  }

  auto collection =
      NormalizeKitCollection(createDialog.pendingCollection.data());
  if (collection.find_first_of("\"'") != std::string::npos) {
    createDialog.error =
        std::string(localization->Get("kits.create_error.collection_quotes"));
    return false;
  }

  nlohmann::json items = nlohmann::json::object();
  for (const auto formID : createDialog.pendingFormIDs) {
    const auto *armorForm = RE::TESForm::LookupByID<RE::TESObjectARMO>(formID);
    if (!armorForm) {
      continue;
    }

    const auto editorID = armor::GetEditorID(armorForm);
    if (editorID.empty()) {
      continue;
    }

    items[editorID] = {{"Plugin", armor::GetPluginName(armorForm)},
                       {"Name", armor::GetDisplayName(armorForm)},
                       {"Amount", 1},
                       {"Equipped", true}};
  }

  if (items.empty() && !createDialog.pendingLayout) {
    createDialog.error =
        std::string(localization->Get("kits.create_error.no_valid_items"));
    return false;
  }

  const auto &kits = EquipmentCatalog::Get().GetKits();
  const auto existingIt =
      std::ranges::find_if(kits, [&](const KitEntry &a_kit) {
        return sosr::strings::CompareTextInsensitive(a_kit.name, name) == 0;
      });

  std::filesystem::path relativePath;
  std::filesystem::path fullPath;
  if (existingIt != kits.end()) {
    relativePath = utf8::PathFromUtf8(existingIt->key);
    fullPath = utf8::PathFromUtf8(existingIt->filepath);
    collection = existingIt->collection;
  } else {
    relativePath =
        utf8::PathFromUtf8(collection) / utf8::PathFromUtf8(name + ".json");
    fullPath = std::filesystem::path(kModexKitDirectory) / relativePath;
  }

  try {
    std::filesystem::create_directories(fullPath.parent_path());
  } catch (const std::exception &exception) {
    const auto message = std::string(exception.what());
    createDialog.error = std::vformat(
        std::string(localization->Get("kits.create_error.create_directory")),
        std::make_format_args(message));
    return false;
  }

  nlohmann::json data = nlohmann::json::object();
  data[name] = nlohmann::json::object();
  data[name]["Collection"] = collection;
  data[name]["Description"] = "Created by Skyrim Vanity System.";
  data[name]["Items"] = std::move(items);
  if (createDialog.pendingLayout.has_value()) {
    data[name][std::string(catalog::kSvsKitMetadataKey)] =
        catalog::SerializeKitLayout(*createDialog.pendingLayout);
  }

  std::ofstream file(fullPath, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    createDialog.error =
        std::string(localization->Get("kits.create_error.open_write"));
    return false;
  }

  file << data.dump(4) << '\n';
  file.close();

  CatalogBrowserState().pendingSelectionAfterRefresh =
      "kit:" + utf8::PathToUtf8GenericString(relativePath);
  QueueCatalogRefresh(ui::catalog::RefreshMode::KitsOnly);
  CatalogBrowserState().selectedKey.clear();
  CatalogBrowserState().activeTab = ui::catalog::BrowserTab::Kits;
  createDialog.openRequested = false;
  createDialog.pendingFormIDs.clear();
  createDialog.pendingLayout.reset();
  createDialog.error.clear();
  return true;
}

bool Menu::DeletePendingKit() {
  const auto *localization = ui::Localization::GetSingleton();
  auto &deleteDialog = DeleteKitDialogState();
  if (deleteDialog.pendingKitPath.empty()) {
    deleteDialog.error =
        std::string(localization->Get("kits.delete_error.path_unavailable"));
    return false;
  }

  std::error_code error;
  const auto removed = std::filesystem::remove(
      utf8::PathFromUtf8(deleteDialog.pendingKitPath), error);
  if (error) {
    auto message = error.message();
    deleteDialog.error = std::vformat(
        std::string(localization->Get("kits.delete_error.failed")),
        std::make_format_args(message));
    return false;
  }
  if (!removed) {
    deleteDialog.error =
        std::string(localization->Get("kits.delete_error.missing"));
    return false;
  }

  CatalogBrowserState().favoriteKeys.erase(BuildFavoriteKey(
      ui::catalog::BrowserTab::Kits, deleteDialog.pendingKitId));
  SaveFavorites();

  if (CatalogBrowserState().selectedKey == deleteDialog.pendingKitId) {
    ClearCatalogSelection();
  }

  QueueCatalogRefresh(ui::catalog::RefreshMode::KitsOnly);
  CatalogBrowserState().activeTab = ui::catalog::BrowserTab::Kits;
  deleteDialog.pendingKitId.clear();
  deleteDialog.pendingKitName.clear();
  deleteDialog.pendingKitPath.clear();
  deleteDialog.error.clear();
  return true;
}

void Menu::PreviewKitEntry(const KitEntry &a_entry) {
  const auto &visibleRowIndices = BuildVisibleWorkbenchRowIndices();
  if (const auto *layout = a_entry.GetLayout(); layout != nullptr) {
    workbench_.PreviewKitLayout(a_entry.id, *layout,
                                ResolveWorkbenchPreviewActor(),
                                &visibleRowIndices);
    return;
  }

  workbench_.ApplyCatalogPreview(a_entry.id, a_entry.GetArmorFormIDs(),
                                 ResolveWorkbenchPreviewActor(),
                                 &visibleRowIndices);
}

void Menu::DrawCreateKitDialog() {
  constexpr float kCreateDialogWidth = 400.0f;
  const auto *localization = ui::Localization::GetSingleton();
  auto &createDialog = CreateKitDialogState();
  const auto createDialogTitle = localization->Get("kits.create_title");
  if (createDialog.openRequested) {
    ImGui::OpenPopup(createDialogTitle.data());
    createDialog.openRequested = false;
  }

  const auto &catalog = EquipmentCatalog::Get();
  std::vector<std::string> existingNames;
  existingNames.reserve(catalog.GetKits().size());
  for (const auto &kit : catalog.GetKits()) {
    if (std::ranges::find(existingNames, kit.name) == existingNames.end()) {
      existingNames.push_back(kit.name);
    }
  }
  std::ranges::sort(existingNames);

  std::vector<ui::components::EditableDropdownItem<std::string>>
      existingNameItems;
  existingNameItems.reserve(existingNames.size());
  for (const auto &name : existingNames) {
    existingNameItems.push_back({.label = name, .value = name});
  }

  std::vector<ui::components::EditableDropdownItem<std::string>>
      collectionItems;
  collectionItems.reserve(catalog.GetKitCollections().size());
  for (const auto &collection : catalog.GetKitCollections()) {
    collectionItems.push_back({.label = collection, .value = collection});
  }

  createDialog.open = false;
  if (const auto *viewport = ImGui::GetMainViewport()) {
    ImGui::SetNextWindowSize(ImVec2(kCreateDialogWidth, 0.0f),
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
  }
  if (ImGui::BeginPopupModal(createDialogTitle.data(), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings)) {
    createDialog.open = true;
    ImGui::TextWrapped(
        "%s",
        createDialog.source == KitCreationSource::Equipped
            ? localization->GetCStr("kits.create_help.equipped")
            : localization->GetCStr("kits.create_help.overrides"));
    ImGui::Separator();
    const auto itemCount = createDialog.pendingFormIDs.size();
    const auto itemsText = std::vformat(
        std::string(localization->Get("kits.create_items")),
        std::make_format_args(itemCount));
    ImGui::TextUnformatted(itemsText.c_str());

    constexpr float fieldWidth = 360.0f;
    std::optional<std::string> selectedName;
    ImGui::TextUnformatted(localization->GetCStr("common.name"));
    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere();
    }
    if (ui::components::DrawEditableStringDropdown(
            "kit-name", localization->GetCStr("kits.name_hint"),
            createDialog.pendingName.data(), createDialog.pendingName.size(),
            std::span<const ui::components::EditableDropdownItem<std::string>>(
                existingNameItems),
            fieldWidth, nullptr, &selectedName) &&
        selectedName.has_value() && !selectedName->empty()) {
      if (const auto it = std::ranges::find_if(catalog.GetKits(),
                                               [&](const KitEntry &a_entry) {
                                                 return a_entry.name ==
                                                        *selectedName;
                                               });
          it != catalog.GetKits().end()) {
        std::snprintf(createDialog.pendingCollection.data(),
                      createDialog.pendingCollection.size(), "%s",
                      it->collection.c_str());
      }
    }

    ImGui::Spacing();
    ImGui::TextUnformatted(localization->GetCStr("common.collection"));
    std::optional<std::string> selectedCollection;
    ui::components::DrawEditableStringDropdown(
        "kit-collection", localization->GetCStr("kits.collection_optional"),
        createDialog.pendingCollection.data(),
        createDialog.pendingCollection.size(),
        std::span<const ui::components::EditableDropdownItem<std::string>>(
            collectionItems),
        fieldWidth, nullptr, &selectedCollection);

    if (!createDialog.error.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                             ThemeConfig::GetSingleton()->GetColorU32("WARN")),
                         "%s", createDialog.error.c_str());
    }

    ImGui::Spacing();
    const bool requestClose =
        createDialog.cancelRequested ||
        ImGui::Shortcut(ImGuiKey_Escape, ImGuiInputFlags_RouteFocused);
    if (ImGui::Button(localization->GetCStr("common.save"),
                      ImVec2(120.0f, 0.0f))) {
      if (SavePendingKit()) {
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(localization->GetCStr("common.cancel"),
                      ImVec2(120.0f, 0.0f)) ||
        requestClose) {
      createDialog.error.clear();
      createDialog.pendingFormIDs.clear();
      createDialog.pendingLayout.reset();
      createDialog.pendingName.fill('\0');
      createDialog.pendingCollection.fill('\0');
      createDialog.open = false;
      createDialog.cancelRequested = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  } else {
    createDialog.open = false;
    createDialog.cancelRequested = false;
  }
}

void Menu::DrawDeleteKitDialog() {
  constexpr float kDeleteDialogWidth = 460.0f;
  const auto *localization = ui::Localization::GetSingleton();
  auto &deleteDialog = DeleteKitDialogState();
  const auto deleteDialogTitle = localization->Get("kits.delete_title");

  if (deleteDialog.openRequested) {
    ImGui::OpenPopup(deleteDialogTitle.data());
    deleteDialog.openRequested = false;
  }

  const auto closeDialog = [&]() {
    deleteDialog.pendingKitId.clear();
    deleteDialog.pendingKitName.clear();
    deleteDialog.pendingKitPath.clear();
    deleteDialog.error.clear();
    deleteDialog.open = false;
    deleteDialog.cancelRequested = false;
    ImGui::CloseCurrentPopup();
  };

  deleteDialog.open = false;
  if (const auto *viewport = ImGui::GetMainViewport()) {
    ImGui::SetNextWindowSize(ImVec2(kDeleteDialogWidth, 0.0f),
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
  }
  if (ImGui::BeginPopupModal(deleteDialogTitle.data(), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    deleteDialog.open = true;

    ImGui::TextWrapped("%s", localization->GetCStr("kits.delete_confirm"));
    ImGui::Spacing();
    ImGui::Separator();

    ImGui::TextUnformatted(localization->GetCStr("common.name"));
    ImGui::TextWrapped("%s", deleteDialog.pendingKitName.c_str());
    if (!deleteDialog.pendingKitPath.empty()) {
      ImGui::Spacing();
      ImGui::TextUnformatted(localization->GetCStr("common.file"));
      ImGui::TextWrapped("%s", deleteDialog.pendingKitPath.c_str());
    }

    if (!deleteDialog.error.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                             ThemeConfig::GetSingleton()->GetColorU32("WARN")),
                         "%s", deleteDialog.error.c_str());
    }

    ImGui::Spacing();
    const bool requestClose =
        deleteDialog.cancelRequested ||
        ImGui::Shortcut(ImGuiKey_Escape, ImGuiInputFlags_RouteFocused);

    ImGui::PushStyleColor(
        ImGuiCol_Button,
        ThemeConfig::GetSingleton()->GetColorU32("DECLINE", 0.90f));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonHovered,
        ThemeConfig::GetSingleton()->GetColorU32("DECLINE", 1.00f));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonActive,
        ThemeConfig::GetSingleton()->GetColorU32("DECLINE", 0.80f));
    if (ImGui::Button(localization->GetCStr("common.delete"),
                      ImVec2(120.0f, 0.0f))) {
      if (DeletePendingKit()) {
        closeDialog();
      }
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (ImGui::Button(localization->GetCStr("common.cancel"),
                      ImVec2(120.0f, 0.0f)) ||
        requestClose) {
      closeDialog();
    }

    ImGui::EndPopup();
  } else {
    deleteDialog.open = false;
    deleteDialog.cancelRequested = false;
  }
}
} // namespace sosr
