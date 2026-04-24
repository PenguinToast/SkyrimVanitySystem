#include "Menu.h"

#include "ConditionMaterializer.h"
#include "ui/Localization.h"
#include "ui/components/EquipmentWidget.h"
#include "ui/workbench/Common.h"

#include <algorithm>
#include <format>

namespace sosr {
namespace {
void DrawApplyWithConditionOverridesPreviewTable(
    const std::vector<workbench::VariantWorkbenchRow> &a_rows) {
  auto *localization = ui::Localization::GetSingleton();
  const auto tableHeight = (std::min)(320.0f, ImGui::GetFrameHeight() * 12.0f);

  if (ImGui::BeginChild("##apply-condition-preview-scroll",
                        ImVec2(0.0f, tableHeight),
                        ImGuiChildFlags_Borders)) {
    if (ImGui::BeginTable("##apply-condition-preview", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY)) {
      ImGui::TableSetupColumn(localization->Get("workbench.equipped").data(),
                              ImGuiTableColumnFlags_WidthStretch, 0.80f);
      ImGui::TableSetupColumn(localization->Get("workbench.overrides").data(),
                              ImGuiTableColumnFlags_WidthStretch, 1.05f);
      ImGui::TableSetupColumn(localization->Get("workbench.hide").data(),
                              ImGuiTableColumnFlags_WidthFixed |
                                  ImGuiTableColumnFlags_NoResize,
                              72.0f);
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableHeadersRow();

      for (int rowIndex = 0; rowIndex < static_cast<int>(a_rows.size());
           ++rowIndex) {
        const auto &row = a_rows[static_cast<std::size_t>(rowIndex)];
        const auto overrideCount = row.overrides.size();
        const auto widgetHeight = 18.0f + (ImGui::GetTextLineHeight() * 2.0f);
        const auto overrideHeight =
            overrideCount > 0
                ? (static_cast<float>(overrideCount) * widgetHeight) +
                      ((overrideCount > 1)
                           ? static_cast<float>(overrideCount - 1) *
                                 ui::workbench::kWorkbenchOverrideGapY
                           : 0.0f)
                : widgetHeight;
        const auto contentHeight = (std::max)(widgetHeight, overrideHeight);
        const auto rowHeight = contentHeight + ui::workbench::kWorkbenchRowGapY;

        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
        ImGui::TableSetBgColor(
            ImGuiTableBgTarget_RowBg0,
            ThemeConfig::GetSingleton()->GetColorU32(
                (rowIndex % 2) == 0 ? "TABLE_BG" : "TABLE_BG_ALT"));
        if (row.isEquipped) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg1,
              ThemeConfig::GetSingleton()->GetColorU32("SECONDARY", 0.16f));
        }

        ImGui::TableSetColumnIndex(0);
        (void)ui::components::DrawEquipmentWidget(
            ("preview-equipped-" + std::to_string(rowIndex)).c_str(),
            row.equipped,
            {.disabledAppearance = !row.isEquipped, .interactive = false});

        ImGui::TableSetColumnIndex(1);
        if (row.overrides.empty()) {
          ImGui::TextDisabled("%s", localization->GetCStr("common.empty"));
        } else {
          const auto oldItemSpacing = ImGui::GetStyle().ItemSpacing;
          ImGui::PushStyleVar(
              ImGuiStyleVar_ItemSpacing,
              ImVec2(oldItemSpacing.x, ui::workbench::kWorkbenchOverrideGapY));
          for (int overrideIndex = 0;
               overrideIndex < static_cast<int>(row.overrides.size());
               ++overrideIndex) {
            (void)ui::components::DrawEquipmentWidget(
                ("preview-override-" + std::to_string(rowIndex) + "-" +
                 std::to_string(overrideIndex))
                    .c_str(),
                row.overrides[static_cast<std::size_t>(overrideIndex)],
                {.interactive = false});
          }
          ImGui::PopStyleVar();
        }

        ImGui::TableSetColumnIndex(2);
        bool hideEquipped = row.hideEquipped;
        ImGui::BeginDisabled();
        ImGui::Checkbox(("##preview-hide-equipped-" +
                         std::to_string(rowIndex))
                            .c_str(),
                        &hideEquipped);
        ImGui::EndDisabled();
      }

      ImGui::EndTable();
    }
  }
  ImGui::EndChild();
}
} // namespace

void Menu::DrawApplyWithConditionMenu(
    const ConditionOverrideApplicationSource &a_source) {
  const auto *localization = ui::Localization::GetSingleton();
  if (!ImGui::BeginMenu(localization->GetCStr("catalog.apply_with_condition"),
                        a_source.HasPayload() &&
                            CountCatalogConditions() != 0)) {
    return;
  }

  for (const auto &condition : ConditionDefinitions()) {
    if (!IsWorkbenchSelectableCondition(condition)) {
      continue;
    }

    if (ImGui::MenuItem(condition.name.c_str())) {
      OpenApplyWithConditionOverridesDialog(a_source, condition);
    }
  }

  ImGui::EndMenu();
}

RE::Actor *Menu::ResolveApplyWithConditionActor(
    const std::string_view a_conditionId,
    const ConditionOverrideApplicationSource &a_source,
    workbench::VariantWorkbench::ConditionOverrideApplicationPlan &a_plan) {
  a_plan = {};
  const auto buildPlan = [&](RE::Actor *a_actor) {
    return a_source.layout.has_value()
               ? workbench_.PlanConditionOverrideApplication(
                     *a_source.layout, a_conditionId, a_actor)
               : workbench_.PlanConditionOverrideApplication(
                     a_source.formIDs, a_conditionId, a_actor);
  };

  const auto materialized =
      conditions::MaterializeConditionById(a_conditionId, ConditionDefinitions());
  if (!materialized.has_value() ||
      materialized->refreshTargets.actorFormIDs.size() != 1) {
    a_plan = buildPlan(nullptr);
    return nullptr;
  }

  auto *actor = RE::TESForm::LookupByID<RE::Actor>(
      materialized->refreshTargets.actorFormIDs.front());
  if (actor != nullptr) {
    auto actorPlan = buildPlan(actor);
    if (actorPlan.sourceKind ==
            workbench::VariantWorkbench::ConditionOverrideSourceKind::
                ActorSource &&
        actorPlan.sourceRowCount > 0 && actorPlan.CanApply()) {
      a_plan = actorPlan;
      return actor;
    }

    a_plan = buildPlan(nullptr);
    return actor;
  }

  a_plan = buildPlan(nullptr);
  return nullptr;
}

void Menu::OpenApplyWithConditionOverridesDialog(
    const ConditionOverrideApplicationSource &a_source,
    const ui::conditions::Definition &a_condition) {
  auto &dialog = applyWithConditionOverridesDialog_;
  dialog.source = a_source;
  dialog.conditionId = a_condition.id;
  dialog.conditionName = a_condition.name;

  auto *actor =
      ResolveApplyWithConditionActor(dialog.conditionId, dialog.source,
                                     dialog.plan);
  dialog.actorFormID = actor != nullptr ? actor->GetFormID() : 0;
  dialog.openRequested = true;
}

void Menu::DrawApplyWithConditionOverridesDialog() {
  constexpr float kDialogWidth = 720.0f;
  const auto *localization = ui::Localization::GetSingleton();
  auto &dialog = applyWithConditionOverridesDialog_;
  const auto title = localization->Get("catalog.apply_with_condition.title");

  if (dialog.openRequested) {
    ImGui::OpenPopup(title.data());
    dialog.openRequested = false;
  }

  const auto closeDialog = [&]() {
    dialog.source = {};
    dialog.conditionId.clear();
    dialog.conditionName.clear();
    dialog.actorFormID = 0;
    dialog.plan = {};
    dialog.cancelRequested = false;
    dialog.open = false;
    ImGui::CloseCurrentPopup();
  };

  dialog.open = false;
  if (const auto *viewport = ImGui::GetMainViewport()) {
    ImGui::SetNextWindowSize(ImVec2(kDialogWidth, 0.0f),
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
  }

  if (ImGui::BeginPopupModal(title.data(), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings)) {
    dialog.open = true;

    const auto body = std::vformat(
        std::string(localization->Get("catalog.apply_with_condition.body")),
        std::make_format_args(dialog.source.name, dialog.conditionName));
    ImGui::TextWrapped("%s", body.c_str());
    ImGui::Spacing();
    ImGui::Separator();

    if (dialog.plan.skippedCount > 0) {
      const auto skippedCount = std::vformat(
          std::string(
              localization->Get("catalog.apply_with_condition.skipped_count")),
          std::make_format_args(dialog.plan.skippedCount));
      ImGui::TextUnformatted(skippedCount.c_str());
    }

    if (!dialog.plan.previewRows.empty()) {
      ImGui::Spacing();
      DrawApplyWithConditionOverridesPreviewTable(dialog.plan.previewRows);
    }

    if (!dialog.plan.CanApply()) {
      ImGui::Spacing();
      ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                             ThemeConfig::GetSingleton()->GetColorU32("WARN")),
                         "%s",
                         localization->GetCStr(
                             "catalog.apply_with_condition.no_overrides"));
    }

    ImGui::Spacing();
    const bool requestClose =
        dialog.cancelRequested ||
        ImGui::Shortcut(ImGuiKey_Escape, ImGuiInputFlags_RouteFocused);
    ImGui::BeginDisabled(!dialog.plan.CanApply());
    if (ImGui::Button(localization->GetCStr("common.apply"),
                      ImVec2(120.0f, 0.0f))) {
      RE::Actor *actor = nullptr;
      if (dialog.actorFormID != 0) {
        actor = RE::TESForm::LookupByID<RE::Actor>(dialog.actorFormID);
      }
      const auto initialEquippedState =
          actor != nullptr
              ? workbench::VariantWorkbench::BuildInitialEquippedState(actor)
              : BuildWorkbenchInitialEquippedState();
      if (actor != nullptr) {
        workbench_.SyncRowsFromActor(actor, dialog.conditionId);
      }
      workbench_.ApplyConditionOverridePlan(dialog.plan, dialog.conditionId,
                                            &initialEquippedState);
      workbenchFilter_.kind = WorkbenchFilterKind::Condition;
      workbenchFilter_.conditionId = dialog.conditionId;
      workbenchFilter_.actorFormID = 0;
      workbench_.ClearPreview();
      closeDialog();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button(localization->GetCStr("common.cancel"),
                      ImVec2(120.0f, 0.0f)) ||
        requestClose) {
      closeDialog();
    }

    ImGui::EndPopup();
  } else {
    dialog.open = false;
    dialog.cancelRequested = false;
  }
}
} // namespace sosr
