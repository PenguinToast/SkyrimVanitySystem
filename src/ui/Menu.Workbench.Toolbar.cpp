#include "Menu.h"

#include "imgui_internal.h"
#include "ui/components/EditableCombo.h"
#include "ui/workbench/Common.h"
#include "ui/workbench/Tooltips.h"

#include <algorithm>
#include <array>
#include <functional>

namespace {
struct WorkbenchToolbarAction {
  std::string_view label;
  bool enabled{true};
  std::function<void()> callback;
  std::function<void()> tooltip;
};
} // namespace

namespace sosr {
void Menu::DrawWorkbenchFilterBar() {
  EnsureWorkbenchDerivedState();
  const auto &filterOptions = workbenchDerived_.filterOptions;

  std::vector<ui::components::EditableDropdownItem<WorkbenchFilterOption>>
      filterItems;
  filterItems.reserve(filterOptions.size());

  const auto matchesCurrentFilter = [&](const WorkbenchFilterOption &a_option) {
    if (a_option.kind != workbenchFilter_.kind) {
      return false;
    }

    switch (a_option.kind) {
    case WorkbenchFilterKind::All:
      return true;
    case WorkbenchFilterKind::ActorRef:
      return a_option.actorFormID == workbenchFilter_.actorFormID;
    case WorkbenchFilterKind::Condition:
      return a_option.conditionId == workbenchFilter_.conditionId;
    }

    return false;
  };

  std::string selectedFilterLabel = "Show All";
  int selectedFilterIndex = -1;
  if (const auto it = std::ranges::find_if(filterOptions, matchesCurrentFilter);
      it != filterOptions.end()) {
    selectedFilterLabel = it->label;
    selectedFilterIndex =
        static_cast<int>(std::distance(filterOptions.begin(), it));
  }

  for (const auto &option : filterOptions) {
    filterItems.push_back(
        {.label = option.label,
         .value = option.isSection
                      ? std::nullopt
                      : std::optional<WorkbenchFilterOption>(option)});
  }

  std::optional<WorkbenchFilterOption> selectedFilterOption;
  const std::function<void(
      const ui::components::EditableDropdownItem<WorkbenchFilterOption> &)>
      drawFilterTooltip =
          [&](const ui::components::EditableDropdownItem<WorkbenchFilterOption>
                  &a_item) {
            if (a_item.value.has_value()) {
              ui::workbench::DrawWorkbenchFilterOptionTooltip(
                  *a_item.value, ConditionDefinitions());
            } else {
              ui::workbench::DrawWorkbenchFilterSectionTooltip(a_item.label);
            }
          };
  if (ui::components::DrawSearchableDropdown(
          "##workbench-filter", "Filter workbench...", selectedFilterLabel,
          std::span<const ui::components::EditableDropdownItem<
              WorkbenchFilterOption>>(filterItems),
          ImGui::GetContentRegionAvail().x, &selectedFilterIndex,
          &selectedFilterOption, drawFilterTooltip)) {
    if (selectedFilterOption.has_value()) {
      workbenchFilter_.kind = selectedFilterOption->kind;
      workbenchFilter_.actorFormID = selectedFilterOption->actorFormID;
      workbenchFilter_.conditionId = selectedFilterOption->conditionId;
      workbench_.ClearPreview();
      SyncWorkbenchRowsForCurrentFilter();
    }
  }
}

void Menu::DrawWorkbenchToolbar() {
  const auto &visibleRowIndices = BuildVisibleWorkbenchRowIndices();
  const auto equippedKitFormIDs =
      workbench_.CollectEquippedArmorFormIDs(&visibleRowIndices);
  const auto overrideKitFormIDs =
      workbench_.CollectOverrideArmorFormIDsFromEquippedRows(
          &visibleRowIndices);
  const bool canCreateEquippedKit = !equippedKitFormIDs.empty();
  const bool canCreateOverrideKit = !overrideKitFormIDs.empty();

  const std::array actions = {
      WorkbenchToolbarAction{
          .label = "Reset Equipped",
          .callback =
              [&]() {
                workbench_.ClearPreview();
                if (workbench_.ResetEquippedRows(&visibleRowIndices)) {
                  workbench_.SyncDynamicArmorVariantsExtended(
                      ConditionDefinitions());
                }
              },
          .tooltip =
              []() {
                ImGui::TextUnformatted(
                    "Reset equipped rows visible in the current filter.");
              },
      },
      WorkbenchToolbarAction{
          .label = "Reset All",
          .callback =
              [&]() {
                workbench_.ClearPreview();
                if (workbench_.ResetAllRows(&visibleRowIndices)) {
                  SyncWorkbenchRowsForCurrentFilter();
                  workbench_.SyncDynamicArmorVariantsExtended(
                      ConditionDefinitions());
                }
              },
          .tooltip =
              []() {
                ImGui::TextUnformatted(
                    "Reset all visible rows in the current filter.");
              },
      },
      WorkbenchToolbarAction{
          .label = "Kit from Equipped",
          .enabled = canCreateEquippedKit,
          .callback =
              [&]() {
                OpenCreateKitDialog(KitCreationSource::Equipped,
                                    &visibleRowIndices);
              },
          .tooltip =
              []() {
                ImGui::TextUnformatted(
                    "Create a Modex kit from equipped rows visible in the "
                    "current filter.");
              },
      },
      WorkbenchToolbarAction{
          .label = "Kit from Overrides",
          .enabled = canCreateOverrideKit,
          .callback =
              [&]() {
                OpenCreateKitDialog(KitCreationSource::Overrides,
                                    &visibleRowIndices);
              },
          .tooltip =
              []() {
                ImGui::TextUnformatted(
                    "Create a Modex kit from overrides on visible equipped "
                    "rows only.");
              },
      },
  };

  const auto &style = ImGui::GetStyle();
  const auto spacingX = style.ItemSpacing.x;
  const auto buttonWidth = [](std::string_view label) {
    return ImGui::CalcTextSize(label.data(), label.data() + label.size()).x +
           ImGui::GetStyle().FramePadding.x * 2.0f;
  };

  std::vector<float> widths;
  widths.reserve(actions.size());
  float totalWidth = 0.0f;
  for (const auto &action : actions) {
    const auto width = buttonWidth(action.label);
    widths.push_back(width);
    if (widths.size() > 1) {
      totalWidth += spacingX;
    }
    totalWidth += width;
  }

  const auto availableWidth = ImGui::GetContentRegionAvail().x;
  const auto moreButtonWidth = buttonWidth(ui::workbench::kIconEllipsis);
  std::size_t visibleCount = actions.size();
  if (totalWidth > availableWidth) {
    visibleCount = 0;
    float usedWidth = 0.0f;
    for (std::size_t index = 0; index < actions.size(); ++index) {
      const auto remainingActions = actions.size() - (index + 1);
      const auto width = widths[index];
      const auto leadingSpacing = visibleCount > 0 ? spacingX : 0.0f;
      const auto overflowReserve =
          remainingActions > 0
              ? (visibleCount > 0 || index > 0 ? spacingX : 0.0f) +
                    moreButtonWidth
              : 0.0f;

      if (usedWidth + leadingSpacing + width + overflowReserve <=
          availableWidth) {
        usedWidth += leadingSpacing + width;
        ++visibleCount;
        continue;
      }

      break;
    }
  }

  const auto drawToolbarAction = [&](const WorkbenchToolbarAction &a_action,
                                     const std::string_view a_tooltipId) {
    if (!a_action.enabled) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button(a_action.label.data())) {
      a_action.callback();
    }
    if (!a_action.enabled) {
      ImGui::EndDisabled();
    }
    if (a_action.tooltip) {
      ui::workbench::DrawSimplePinnableTooltip(
          a_tooltipId,
          ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort |
                               ImGuiHoveredFlags_AllowWhenDisabled),
          a_action.tooltip);
    }
  };

  for (std::size_t index = 0; index < visibleCount; ++index) {
    if (index > 0) {
      ImGui::SameLine();
    }
    drawToolbarAction(actions[index],
                      "workbench:toolbar:" + std::to_string(index));
  }

  if (visibleCount < actions.size()) {
    if (visibleCount > 0) {
      ImGui::SameLine();
    }
    if (ImGui::Button(ui::workbench::kIconEllipsis)) {
      ImGui::OpenPopup(ui::workbench::kWorkbenchOverflowPopupId);
    }
    if (ImGui::BeginPopup(ui::workbench::kWorkbenchOverflowPopupId)) {
      for (std::size_t index = visibleCount; index < actions.size(); ++index) {
        const auto &action = actions[index];
        if (ImGui::MenuItem(action.label.data(), nullptr, false,
                            action.enabled)) {
          action.callback();
          ImGui::CloseCurrentPopup();
        }
        if (action.tooltip) {
          ui::workbench::DrawSimplePinnableTooltip(
              "workbench:toolbar:overflow:" + std::to_string(index),
              ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort |
                                   ImGuiHoveredFlags_AllowWhenDisabled),
              action.tooltip);
        }
      }
      ImGui::EndPopup();
    }
  }
  ImGui::Spacing();
}

void Menu::DrawWorkbenchEmptyState(const char *a_tableId,
                                   const char *a_targetId,
                                   const char *a_message) {
  ImGui::TextWrapped("%s", a_message);

  if (!ImGui::BeginTable(a_tableId, 3,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_Resizable,
                         ImVec2(0.0f, 180.0f))) {
    return;
  }

  ImGui::TableSetupColumn("Equipped", ImGuiTableColumnFlags_WidthStretch,
                          0.80f);
  ImGui::TableSetupColumn("Overrides", ImGuiTableColumnFlags_WidthStretch,
                          1.05f);
  ImGui::TableSetupColumn(
      "Hide", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize,
      72.0f);
  ImGui::TableHeadersRow();
  ImGui::TableNextRow(ImGuiTableRowFlags_None, 116.0f);

  ImGui::TableSetColumnIndex(0);
  if (const auto *table = ImGui::GetCurrentTable(); table != nullptr) {
    const ImRect leftCellRect = ImGui::TableGetCellBgRect(table, 0);
    ImGui::SetCursorScreenPos(
        ImVec2(leftCellRect.Min.x + ImGui::GetStyle().CellPadding.x,
               leftCellRect.Min.y + ImGui::GetStyle().CellPadding.y));
    ImGui::PushTextWrapPos(leftCellRect.Max.x -
                           ImGui::GetStyle().CellPadding.x);
    ImGui::TextDisabled("Drop equipment or equipment slots here.");
    ImGui::PopTextWrapPos();

    if (ImGui::BeginDragDropTargetCustom(leftCellRect,
                                         ImGui::GetID(a_targetId))) {
      if (const auto *payload = ImGui::AcceptDragDropPayload(
              ui::workbench::kVariantItemPayloadType);
          payload && payload->Data != nullptr &&
          payload->DataSize == sizeof(DraggedEquipmentPayload)) {
        DraggedEquipmentPayload dragPayload{};
        std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));
        ApplyWorkbenchRowDrop(dragPayload);
      }
      ImGui::EndDragDropTarget();
    }
  }

  ImGui::TableSetColumnIndex(1);
  ImGui::TextDisabled("Add a row first, then drop equipment overrides here.");

  ImGui::TableSetColumnIndex(2);
  ImGui::TextDisabled("-");

  ImGui::EndTable();
}
} // namespace sosr
