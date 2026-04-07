#include "Menu.h"

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "StringUtils.h"
#include "ThemeConfig.h"
#include "conditions/Creation.h"
#include "conditions/Defaults.h"
#include "conditions/Library.h"
#include "conditions/Status.h"
#include "imgui_internal.h"
#include "ui/InputWidgets.h"
#include "ui/Localization.h"
#include "ui/TableReorder.h"
#include "ui/catalog/Widgets.h"
#include "ui/components/PinnableTooltip.h"
#include "ui/conditions/DraftValidation.h"
#include "ui/conditions/Widgets.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <optional>

namespace sosr {
namespace {
using ConditionClause = ui::conditions::Clause;
using ConditionDefinition = ui::conditions::Definition;

void MoveConditionDefinitionToSlot(
    std::vector<ConditionDefinition> &a_conditions,
    const std::size_t a_sourceIndex, std::size_t a_slotIndex) {
  if (a_sourceIndex >= a_conditions.size() ||
      a_slotIndex > a_conditions.size()) {
    return;
  }

  auto condition = std::move(a_conditions[a_sourceIndex]);
  a_conditions.erase(a_conditions.begin() +
                     static_cast<std::ptrdiff_t>(a_sourceIndex));
  if (a_sourceIndex < a_slotIndex) {
    --a_slotIndex;
  }
  a_conditions.insert(a_conditions.begin() +
                          static_cast<std::ptrdiff_t>(a_slotIndex),
                      std::move(condition));
}

std::vector<std::size_t> BuildConditionIndicesByKind(
    const std::vector<ConditionDefinition> &a_conditions,
    const conditions::DefinitionKind a_kind) {
  std::vector<std::size_t> indices;
  indices.reserve(a_conditions.size());
  for (std::size_t index = 0; index < a_conditions.size(); ++index) {
    if (a_conditions[index].kind == a_kind) {
      indices.push_back(index);
    }
  }
  return indices;
}

void SortConditionIndicesByName(
    const std::vector<ConditionDefinition> &a_conditions,
    std::vector<std::size_t> &a_indices) {
  std::ranges::sort(
      a_indices, [&](const std::size_t a_left, const std::size_t a_right) {
        return strings::CompareTextInsensitive(a_conditions[a_left].name,
                                               a_conditions[a_right].name) < 0;
      });
}

void MoveFilteredConditionDefinitionToSlot(
    std::vector<ConditionDefinition> &a_conditions,
    const std::vector<std::size_t> &a_filteredIndices,
    const std::size_t a_sourceFilteredIndex, const std::size_t a_slotIndex) {
  if (a_sourceFilteredIndex >= a_filteredIndices.size() ||
      a_slotIndex > a_filteredIndices.size()) {
    return;
  }

  const auto sourceIndex = a_filteredIndices[a_sourceFilteredIndex];
  std::size_t rawSlotIndex = a_conditions.size();
  if (a_slotIndex < a_filteredIndices.size()) {
    rawSlotIndex = a_filteredIndices[a_slotIndex];
  }
  MoveConditionDefinitionToSlot(a_conditions, sourceIndex, rawSlotIndex);
}

struct ConditionDeleteUsage {
  std::size_t referencingConditionCount{0};
  std::size_t appliedRowCount{0};

  [[nodiscard]] bool CanDelete() const {
    return referencingConditionCount == 0 && appliedRowCount == 0;
  }

  [[nodiscard]] std::string BuildTooltip() const {
    if (CanDelete()) {
      return {};
    }

    if (referencingConditionCount != 0 && appliedRowCount != 0) {
      return std::string(ui::Localization::GetSingleton()->Get(
          "conditions.catalog.delete_reason_both"));
    }
    if (referencingConditionCount != 0) {
      return std::string(ui::Localization::GetSingleton()->Get(
          "conditions.catalog.delete_reason_referenced"));
    }
    return std::string(ui::Localization::GetSingleton()->Get(
        "conditions.catalog.delete_reason_applied"));
  }
};

struct ConditionMoveToLibraryUsage {
  std::size_t appliedRowCount{0};

  [[nodiscard]] bool CanMove() const { return appliedRowCount == 0; }

  [[nodiscard]] std::string BuildTooltip() const {
    if (CanMove()) {
      return {};
    }
    return std::string(ui::Localization::GetSingleton()->Get(
        "conditions.catalog.move_reason_applied"));
  }
};

constexpr char kIconTrash[] = "\xee\x86\x8c";      // ICON_LC_TRASH
constexpr char kIconCircleHelp[] = "\xee\x82\x82"; // ICON_LC_CIRCLE_HELP
constexpr float kTooltipOrGroupIndicatorWidth = 6.0f;
constexpr float kTooltipOrGroupIndicatorInsetX = 4.0f;
constexpr float kTooltipOrGroupBoundaryGap = 4.0f;
constexpr float kTooltipOrGroupIndicatorRounding = 4.0f;

struct TooltipOrGroupVisual {
  ImRect operatorColumnRect;
  bool initialized{false};
};

[[nodiscard]] float
ComputeCatalogConditionRowHeight(const ConditionDefinition &a_condition) {
  const auto &style = ImGui::GetStyle();
  const auto lineHeight = ImGui::GetTextLineHeight();
  auto height = style.FramePadding.y * 2.0f + lineHeight;
  if (!a_condition.description.empty()) {
    height += style.ItemSpacing.y + lineHeight;
  }
  return height;
}

std::vector<ui::conditions::Color> CollectCatalogColorsForNewCondition(
    const std::vector<ConditionDefinition> &a_conditions,
    const std::vector<ui::conditions::editor::State> &a_editors) {
  std::vector<ui::conditions::Color> colors;
  colors.reserve(a_conditions.size() + a_editors.size());
  for (const auto &condition : a_conditions) {
    if (const auto *catalog = condition.GetCatalog(); catalog != nullptr) {
      colors.push_back(catalog->color);
    }
  }
  for (const auto &editor : a_editors) {
    if (!editor.isNew) {
      continue;
    }
    if (const auto *catalog = editor.draft.GetCatalog(); catalog != nullptr) {
      colors.push_back(catalog->color);
    }
  }
  return colors;
}

std::string BuildActorTargetLabel(const RE::FormID a_formID) {
  auto *localization = ui::Localization::GetSingleton();
  auto *actor = RE::TESForm::LookupByID<RE::Actor>(a_formID);
  if (!actor) {
    return std::string(localization->Get("conditions.catalog.actor_label")) +
           armor::FormatFormID(a_formID);
  }

  std::string label;
  if (const auto *displayName = actor->GetDisplayFullName();
      displayName != nullptr && displayName[0] != '\0') {
    label = displayName;
  } else if (const auto *name = actor->GetName();
             name != nullptr && name[0] != '\0') {
    label = name;
  } else if (const auto *actorBase = actor->GetActorBase()) {
    label = armor::GetDisplayName(actorBase);
  }

  auto editorId = armor::GetEditorID(actor);
  if (editorId.empty()) {
    if (const auto *actorBase = actor->GetActorBase()) {
      editorId = armor::GetEditorID(actorBase);
    }
  }

  if (label.empty()) {
    if (!editorId.empty()) {
      return editorId;
    }
    return std::string(localization->Get("conditions.catalog.actor_label")) +
           armor::FormatFormID(a_formID);
  }

  if (!editorId.empty() && editorId != label) {
    label.append(" (");
    label.append(editorId);
    label.push_back(')');
  }
  return label;
}

void DrawConditionTooltipHeader(
    std::string_view a_title,
    const std::optional<ui::conditions::Color> &a_color = std::nullopt) {
  const auto *theme = ThemeConfig::GetSingleton();
  const auto headerMin = ImGui::GetCursorScreenPos();
  const auto headerWidth = ImGui::GetContentRegionAvail().x;
  const auto headerHeight = ImGui::GetFontSize() * 2.4f;
  const auto headerMax =
      ImVec2(headerMin.x + headerWidth, headerMin.y + headerHeight);
  auto *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(headerMin, headerMax, theme->GetColorU32("BG"), 8.0f);
  const auto accentColor =
      a_color.has_value()
          ? ImGui::GetColorU32(ui::conditions::ToImGuiColor(*a_color))
          : theme->GetColorU32("PRIMARY", 0.65f);
  drawList->AddRect(headerMin, headerMax, accentColor, 8.0f);
  if (a_color.has_value()) {
    drawList->AddRectFilledMultiColor(
        headerMin, headerMax,
        ImGui::GetColorU32(ImVec4(a_color->x, a_color->y, a_color->z, 0.18f)),
        ImGui::GetColorU32(ImVec4(a_color->x, a_color->y, a_color->z, 0.18f)),
        theme->GetColorU32("NONE"), theme->GetColorU32("NONE"));
  }

  const auto titleFontSize = ImGui::GetFontSize() * 1.15f;
  const auto titleSize =
      ImGui::CalcTextSize(a_title.data(), nullptr, false, headerWidth);
  drawList->AddText(
      ImGui::GetFont(), titleFontSize,
      ImVec2(headerMin.x + (headerWidth - titleSize.x) * 0.5f,
             headerMin.y + (headerHeight - titleFontSize) * 0.5f - 1.0f),
      theme->GetColorU32("TEXT"), a_title.data(),
      a_title.data() + a_title.size());
  ImGui::Dummy(ImVec2(headerWidth, headerHeight));

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Separator, accentColor);
  ImGui::Separator();
  ImGui::PopStyleColor();
  ImGui::Spacing();
}

void DrawConditionTooltipSectionHeader(const char *a_title) {
  ImGui::TextDisabled("%s", a_title);
  ImGui::Spacing();
}

void DrawConditionTooltipBulletLine(std::string_view a_text) {
  ImGui::Bullet();
  ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x + 4.0f);
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(a_text.data(), a_text.data() + a_text.size());
  ImGui::PopTextWrapPos();
}

void DrawConditionTooltip(const ConditionDefinition &a_condition,
                          const bool a_hoveredSource,
                          std::vector<ConditionDefinition> &a_conditions,
                          const bool a_showActorRefs) {
  const auto tooltipId = "condition:" + a_condition.id;
  if (!ui::components::ShouldDrawPinnableTooltip(tooltipId, a_hoveredSource)) {
    return;
  }

  const auto materialized =
      conditions::MaterializeConditionById(a_condition.id, a_conditions);
  const auto conditionStatus =
      conditions::EvaluateDefinitionStatus(a_condition, a_conditions);
  const auto tooltipWidth = 460.0f;
  ImGui::SetNextWindowSize(
      ImVec2(tooltipWidth + ImGui::GetStyle().WindowPadding.x * 2.0f, 0.0f),
      ImGuiCond_Always);
  ui::components::DrawPinnableTooltip(tooltipId, a_hoveredSource, [&]() {
    auto *localization = ui::Localization::GetSingleton();
    DrawConditionTooltipHeader(
        a_condition.name, [&]() -> std::optional<ui::conditions::Color> {
          if (const auto *catalog = a_condition.GetCatalog();
              catalog != nullptr) {
            return catalog->color;
          }
          return std::nullopt;
        }());

    if (!a_condition.description.empty()) {
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextDisabled("%s", a_condition.description.c_str());
      ImGui::PopTextWrapPos();
      ImGui::Spacing();
    }

    if (!conditionStatus.missingDependencyChains.empty()) {
      DrawConditionTooltipSectionHeader(
          localization->GetCStr("conditions.catalog.missing_references"));
      for (const auto &missingChain : conditionStatus.missingDependencyChains) {
        const auto label =
            conditions::FormatMissingDependencyChain(missingChain, 1);
        DrawConditionTooltipBulletLine(label);
      }
      ImGui::Spacing();
    }

    if (a_showActorRefs) {
      DrawConditionTooltipSectionHeader(
          localization->GetCStr("conditions.catalog.targeted_actor_refs"));
      if (!materialized.has_value()) {
        DrawConditionTooltipBulletLine(localization->Get(
            "conditions.catalog.materialize_unavailable"));
      } else if (!materialized->refreshTargets.actorFormIDs.empty()) {
        for (const auto actorFormID :
             materialized->refreshTargets.actorFormIDs) {
          const auto label = BuildActorTargetLabel(actorFormID);
          DrawConditionTooltipBulletLine(label);
        }
      } else {
        DrawConditionTooltipBulletLine(localization->Get(
            "conditions.catalog.no_explicit_actor_refs"));
      }
      if (materialized.has_value() &&
          materialized->refreshTargets.useNearbyFallback) {
        DrawConditionTooltipBulletLine(
            localization->Get("conditions.catalog.nearby_actor_refs"));
      }
      ImGui::Spacing();
    }
    DrawConditionTooltipSectionHeader(
        localization->GetCStr("conditions.catalog.expanded_form"));
    if (!materialized.has_value() || materialized->displayCnf.empty()) {
      ImGui::TextDisabled(
          "%s", localization->GetCStr("conditions.catalog.unavailable"));
      return;
    }

    if (ImGui::BeginTable("##condition-expanded-form", 2,
                          ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn(
          localization->Get("conditions.catalog.expression").data(),
          ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("##operator", ImGuiTableColumnFlags_WidthFixed,
                              44.0f);
      std::vector<TooltipOrGroupVisual> orGroupVisuals;
      orGroupVisuals.reserve(materialized->displayCnf.size());

      for (std::size_t groupIndex = 0;
           groupIndex < materialized->displayCnf.size(); ++groupIndex) {
        const auto &group = materialized->displayCnf[groupIndex];
        const bool isOrGroup = group.size() > 1;
        TooltipOrGroupVisual groupVisual;
        for (std::size_t literalIndex = 0; literalIndex < group.size();
             ++literalIndex) {
          ImGui::TableNextRow();

          ImGui::TableSetColumnIndex(0);
          ImGui::PushTextWrapPos(0.0f);
          ImGui::TextUnformatted(group[literalIndex].c_str());
          ImGui::PopTextWrapPos();

          ImGui::TableSetColumnIndex(1);
          const char *op = "";
          if (literalIndex + 1 < group.size()) {
            op = "OR";
          } else if (groupIndex + 1 < materialized->displayCnf.size()) {
            op = "AND";
          }
          if (op[0] != '\0') {
            ImGui::TextDisabled("%s", op);
          }

          if (isOrGroup) {
            const auto rowRect =
                ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), 1);
            if (!groupVisual.initialized) {
              groupVisual.operatorColumnRect = rowRect;
              groupVisual.initialized = true;
            } else {
              groupVisual.operatorColumnRect.Add(rowRect.Min);
              groupVisual.operatorColumnRect.Add(rowRect.Max);
            }
          }
        }

        if (groupVisual.initialized) {
          orGroupVisuals.push_back(groupVisual);
        }
      }

      const auto *theme = ThemeConfig::GetSingleton();
      auto *drawList = ImGui::GetWindowDrawList();
      for (const auto &groupVisual : orGroupVisuals) {
        const auto indicatorMin = ImVec2(groupVisual.operatorColumnRect.Min.x +
                                             kTooltipOrGroupIndicatorInsetX,
                                         groupVisual.operatorColumnRect.Min.y +
                                             kTooltipOrGroupBoundaryGap);
        const auto indicatorMax = ImVec2(
            indicatorMin.x + kTooltipOrGroupIndicatorWidth,
            groupVisual.operatorColumnRect.Max.y - kTooltipOrGroupBoundaryGap);
        drawList->AddRectFilled(indicatorMin, indicatorMax,
                                theme->GetColorU32("PRIMARY", 0.85f),
                                kTooltipOrGroupIndicatorRounding);
      }

      ImGui::EndTable();
    }
  });
}
} // namespace

bool Menu::DrawConditionTab() {
  auto *localization = ui::Localization::GetSingleton();
  EnsureDefaultConditions();

  auto &paneState = ConditionsPaneState();
  constexpr float kSplitterThickness = 8.0f;
  constexpr float kMinCatalogPaneHeight = 160.0f;
  constexpr float kMinLibraryPaneHeight = 160.0f;

  const float totalAvailableHeight = ImGui::GetContentRegionAvail().y;
  paneState.libraryPaneHeight =
      std::clamp(paneState.libraryPaneHeight, kMinLibraryPaneHeight,
                 (std::max)(kMinLibraryPaneHeight, totalAvailableHeight -
                                                       kMinCatalogPaneHeight -
                                                       kSplitterThickness));
  float catalogPaneHeight =
      totalAvailableHeight - paneState.libraryPaneHeight - kSplitterThickness;
  catalogPaneHeight = (std::max)(catalogPaneHeight, kMinCatalogPaneHeight);

  bool rowClicked = false;
  if (ImGui::BeginChild("##conditions-catalog-pane",
                        ImVec2(0.0f, catalogPaneHeight),
                        ImGuiChildFlags_None)) {
    rowClicked = DrawConditionCatalogTable();
  }
  ImGui::EndChild();

  ImGui::PushID("##conditions-library-splitter");
  ImGui::InvisibleButton("splitter", ImVec2(-FLT_MIN, kSplitterThickness));
  const bool splitterHovered = ImGui::IsItemHovered();
  const bool splitterActive = ImGui::IsItemActive();
  if (splitterActive) {
    paneState.libraryPaneHeight -= ImGui::GetIO().MouseDelta.y;
    paneState.libraryPaneHeight =
        std::clamp(paneState.libraryPaneHeight, kMinLibraryPaneHeight,
                   (std::max)(kMinLibraryPaneHeight, totalAvailableHeight -
                                                         kMinCatalogPaneHeight -
                                                         kSplitterThickness));
  }
  if (splitterHovered || splitterActive) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
  }
  const auto splitterMin = ImGui::GetItemRectMin();
  const auto splitterMax = ImGui::GetItemRectMax();
  const auto splitterColor =
      splitterActive ? ImGui::GetColorU32(
                           ThemeConfig::GetSingleton()->GetActive("PRIMARY"))
      : splitterHovered
          ? ImGui::GetColorU32(ThemeConfig::GetSingleton()->GetColor("PRIMARY"))
          : ImGui::GetColorU32(ImGuiCol_Border);
  if (splitterHovered || splitterActive) {
    ImGui::GetWindowDrawList()->AddRectFilled(
        splitterMin, splitterMax,
        ImGui::GetColorU32(
            ImVec4(ImGui::ColorConvertU32ToFloat4(splitterColor).x,
                   ImGui::ColorConvertU32ToFloat4(splitterColor).y,
                   ImGui::ColorConvertU32ToFloat4(splitterColor).z, 0.12f)));
  }
  ImGui::GetWindowDrawList()->AddLine(
      ImVec2(splitterMin.x, (splitterMin.y + splitterMax.y) * 0.5f),
      ImVec2(splitterMax.x, (splitterMin.y + splitterMax.y) * 0.5f),
      splitterColor, splitterActive ? 2.0f : 1.0f);
  ImGui::PopID();

  if (ImGui::BeginChild("##conditions-library-pane", ImVec2(0.0f, 0.0f),
                        ImGuiChildFlags_None)) {
    ImGui::TextUnformatted(localization->GetCStr("conditions.library.title"));
    ImGui::SameLine();
    ImGui::TextColored(ThemeConfig::GetSingleton()->GetColor("TEXT_DISABLED"),
                       "%s", kIconCircleHelp);
    const auto helpMin = ImGui::GetItemRectMin();
    const auto helpSize = ImGui::GetItemRectSize();
    ImGui::SetCursorScreenPos(helpMin);
    ImGui::InvisibleButton("##conditions-library-help", helpSize);
    ui::catalog::DrawCatalogTabHelpTooltip(
        "conditions:library:help", ImGui::IsItemHovered(),
        {localization->Get("conditions.library.help.1").data(),
         localization->Get("conditions.library.help.2").data()});
    ImGui::Spacing();
    DrawConditionLibraryTable();
  }
  ImGui::EndChild();

  return rowClicked;
}

bool Menu::DrawConditionCatalogTable() {
  EnsureDefaultConditions();
  const auto catalogIndices = BuildConditionIndicesByKind(
      ConditionDefinitions(), conditions::DefinitionKind::Catalog);

  if (!ImGui::BeginTable("##conditions-table", 2,
                         ImGuiTableFlags_SizingStretchProp |
                             ImGuiTableFlags_Resizable |
                             ImGuiTableFlags_PadOuterX | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_BordersInnerV,
                         ImVec2(0.0f, 0.0f))) {
    return false;
  }

  auto *localization = ui::Localization::GetSingleton();
  ImGui::TableSetupColumn(
      localization->GetCStr("conditions.catalog.condition"),
      ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(
      localization->GetCStr("conditions.catalog.disable"),
      ImGuiTableColumnFlags_WidthFixed, 72.0f);
  ImGui::TableHeadersRow();
  bool rowClicked = false;
  std::optional<std::size_t> pendingDeleteIndex;
  std::optional<std::size_t> pendingCopyIndex;
  std::optional<std::size_t> pendingCopyToLibraryIndex;
  std::optional<std::size_t> pendingMoveToLibraryIndex;
  std::vector<ImRect> reorderRowRects;
  reorderRowRects.reserve(catalogIndices.size());

  for (std::size_t filteredIndex = 0; filteredIndex < catalogIndices.size();
       ++filteredIndex) {
    const auto index = catalogIndices[filteredIndex];
    auto &condition = ConditionDefinitions()[index];
    const auto conditionStatus =
        conditions::EvaluateDefinitionStatus(condition, ConditionDefinitions());
    const bool broken = conditionStatus.IsBroken();
    ConditionDeleteUsage deleteUsage;
    ConditionMoveToLibraryUsage moveToLibraryUsage;
    for (const auto &otherCondition : ConditionDefinitions()) {
      if (otherCondition.id == condition.id) {
        continue;
      }
      if (std::ranges::any_of(
              otherCondition.clauses, [&](const ConditionClause &a_clause) {
                return a_clause.customConditionId == condition.id;
              })) {
        ++deleteUsage.referencingConditionCount;
      }
    }
    for (const auto &row : workbench_.GetRows()) {
      if (row.conditionId && *row.conditionId == condition.id &&
          row.HasOverridesOrHideState()) {
        ++deleteUsage.appliedRowCount;
      }
      if (row.conditionId && *row.conditionId == condition.id) {
        ++moveToLibraryUsage.appliedRowCount;
      }
    }
    const auto deleteTooltip = deleteUsage.BuildTooltip();
    const bool deleteEnabled = deleteUsage.CanDelete();
    const auto moveToLibraryTooltip = moveToLibraryUsage.BuildTooltip();
    const bool moveToLibraryEnabled = moveToLibraryUsage.CanMove();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(static_cast<int>(index));

    if (const auto *rowTable = ImGui::GetCurrentTable(); rowTable != nullptr) {
      reorderRowRects.emplace_back();
      const auto rowCellRect = ImGui::TableGetCellBgRect(rowTable, 0);
      const auto cellPadding = ImGui::GetStyle().CellPadding;
      const auto rowHeight = ComputeCatalogConditionRowHeight(condition);
      const auto cellContentHeight =
          (rowCellRect.Max.y - rowCellRect.Min.y) - (cellPadding.y * 2.0f);
      const auto cellContentOffsetY =
          (std::max)(0.0f, (cellContentHeight - rowHeight) * 0.5f);
      ImGui::SetCursorScreenPos(
          ImVec2(rowCellRect.Min.x + cellPadding.x,
                 rowCellRect.Min.y + cellPadding.y + cellContentOffsetY));
      const auto width =
          (std::max)(0.0f, (rowCellRect.Max.x - rowCellRect.Min.x) -
                               (cellPadding.x * 2.0f));
      ImGui::InvisibleButton("##condition-row", ImVec2(width, rowHeight));
    } else {
      const auto rowHeight = ComputeCatalogConditionRowHeight(condition);
      const auto width = ImGui::GetContentRegionAvail().x;
      ImGui::InvisibleButton("##condition-row", ImVec2(width, rowHeight));
      reorderRowRects.emplace_back(ImGui::GetItemRectMin(),
                                   ImGui::GetItemRectMax());
    }
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    const auto stripeWidth = 6.0f;
    const auto deletePaneWidth = 34.0f;
    const auto rounding = ImGui::GetStyle().FrameRounding;
    const ImVec2 deleteMin(max.x - deletePaneWidth, min.y);
    const ImVec2 deleteMax = max;
    const auto deleteState = ui::input_widgets::EvaluateRectClickTarget(
        ImGui::GetID("##condition-row-delete"), deleteMin, deleteMax);
    const bool deleteHovered = deleteState.hovered;
    const bool deleteHeld = deleteState.held;
    const bool deletePressed = deleteState.pressed;
    const auto hovered = ImGui::IsItemHovered() || deleteHovered;
    const bool rowBodyHovered =
        hovered && !deleteHovered &&
        ImGui::IsMouseHoveringRect(min, ImVec2(deleteMin.x, max.y), false);
    DrawConditionTooltip(condition, rowBodyHovered, ConditionDefinitions(),
                         true);
    rowClicked |= rowBodyHovered && ImGui::IsItemClicked(ImGuiMouseButton_Left);
    if (rowBodyHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      OpenConditionEditorDialog(index);
    }

    auto *drawList = ImGui::GetWindowDrawList();
    auto *theme = ThemeConfig::GetSingleton();
    const auto *catalog = condition.GetCatalog();
    if (catalog == nullptr) {
      ImGui::PopID();
      continue;
    }
    const auto &conditionColor = catalog->color;
    const bool enabled = conditionStatus.IsActive();
    const auto bodyColor =
        broken
            ? (hovered ? IM_COL32(56, 42, 28, 235) : IM_COL32(44, 34, 24, 220))
            : (enabled ? (hovered ? IM_COL32(42, 42, 44, 240)
                                  : IM_COL32(34, 34, 36, 225))
                       : (hovered ? IM_COL32(35, 35, 38, 225)
                                  : IM_COL32(28, 28, 30, 210)));
    drawList->AddRectFilled(min, max, bodyColor, rounding);
    drawList->AddRect(
        min, max,
        broken ? theme->GetColorU32("WARN", hovered ? 0.95f : 0.78f)
               : ImGui::GetColorU32(ImVec4(conditionColor.x, conditionColor.y,
                                           conditionColor.z,
                                           enabled ? 0.75f : 0.42f)),
        rounding);
    drawList->AddRectFilled(
        min, ImVec2(min.x + stripeWidth, max.y),
        broken ? theme->GetColorU32("WARN", 0.95f)
               : ImGui::GetColorU32(ImVec4(conditionColor.x, conditionColor.y,
                                           conditionColor.z,
                                           enabled ? 1.0f : 0.55f)),
        rounding,
        ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomLeft);
    const ImU32 deleteFillColor =
        deleteEnabled
            ? (deleteHeld      ? theme->GetColorU32("DECLINE")
               : deleteHovered ? theme->GetColorU32("DECLINE", 0.95f)
                               : theme->GetColorU32("DECLINE", 0.78f))
            : theme->GetColorU32("DECLINE", deleteHovered ? 0.35f : 0.24f);
    drawList->AddRectFilled(deleteMin, deleteMax, deleteFillColor, rounding,
                            ImDrawFlags_RoundCornersTopRight |
                                ImDrawFlags_RoundCornersBottomRight);
    drawList->AddLine(ImVec2(deleteMin.x, deleteMin.y),
                      ImVec2(deleteMin.x, deleteMax.y),
                      IM_COL32(255, 255, 255, 18), 1.0f);
    const auto deleteIconSize = ImGui::CalcTextSize(kIconTrash);
    drawList->AddText(
        ImVec2(deleteMin.x + ((deletePaneWidth - deleteIconSize.x) * 0.5f),
               deleteMin.y +
                   (((deleteMax.y - deleteMin.y) - deleteIconSize.y) * 0.5f)),
        ImGui::GetColorU32(deleteEnabled ? ImGuiCol_Text
                                         : ImGuiCol_TextDisabled),
        kIconTrash);

    const auto contentMin = ImVec2(min.x + stripeWidth + 10.0f,
                                   min.y + ImGui::GetStyle().CellPadding.y);
    const auto textColor = ImGui::GetColorU32(
        ImVec4(conditionColor.x, conditionColor.y, conditionColor.z, 1.0f));
    const auto clipRect =
        ImVec4(contentMin.x, min.y, deleteMin.x - 2.0f, max.y);
    const auto titleColor =
        broken ? theme->GetColorU32("WARN")
        : enabled
            ? textColor
            : ImGui::GetColorU32(ImVec4(conditionColor.x, conditionColor.y,
                                        conditionColor.z, 0.68f));
    drawList->PushClipRect(ImVec2(clipRect.x, clipRect.y),
                           ImVec2(clipRect.z, clipRect.w), true);
    drawList->AddText(contentMin, titleColor, condition.name.c_str());
    if (!condition.description.empty()) {
      const auto descriptionMin =
          ImVec2(contentMin.x, contentMin.y + ImGui::GetTextLineHeight() +
                                   ImGui::GetStyle().ItemSpacing.y);
      const auto descriptionMax = ImVec2(
          deleteMin.x - 3.0f, descriptionMin.y + ImGui::GetTextLineHeight());
      const auto descriptionSize =
          ImGui::CalcTextSize(condition.description.c_str());
      ImGui::RenderTextEllipsis(drawList, descriptionMin, descriptionMax,
                                descriptionMax.x, condition.description.c_str(),
                                nullptr, &descriptionSize);
    }
    drawList->PopClipRect();

    if (deleteHovered) {
      if (deleteEnabled) {
        if (deletePressed) {
          pendingDeleteIndex = index;
        }
      } else if (!deleteTooltip.empty()) {
        ui::condition_widgets::DrawHoverDescription(
            "conditions:delete-disabled:" + condition.id, true, deleteTooltip,
            0.2f);
      }
    }

    if (!deleteHovered &&
        ImGui::BeginPopupContextItem("##condition-row-context")) {
      if (ImGui::MenuItem(localization->GetCStr("common.edit"))) {
        OpenConditionEditorDialog(index);
      }
      if (ImGui::MenuItem(localization->GetCStr("common.copy"))) {
        pendingCopyIndex = index;
      }
      if (ImGui::MenuItem(
              localization->GetCStr("conditions.copy_to_library"))) {
        pendingCopyToLibraryIndex = index;
      }
      if (ImGui::MenuItem(localization->GetCStr("conditions.move_to_library"),
                          nullptr, false,
                          moveToLibraryEnabled)) {
        pendingMoveToLibraryIndex = index;
      }
      if (!moveToLibraryEnabled) {
        ui::condition_widgets::DrawHoverDescription(
            "conditions:move-to-library-disabled:" + condition.id,
            moveToLibraryTooltip, 0.2f, ImGuiHoveredFlags_AllowWhenDisabled);
      }
      ImGui::Separator();
      ImGui::PushStyleColor(
          ImGuiCol_Text,
          ImGui::ColorConvertU32ToFloat4(
              ThemeConfig::GetSingleton()->GetColorU32("DECLINE")));
      if (ImGui::MenuItem(localization->GetCStr("common.delete"),
                          nullptr, false, deleteEnabled)) {
        pendingDeleteIndex = index;
      }
      ImGui::PopStyleColor();
      if (!deleteEnabled) {
        ui::condition_widgets::DrawHoverDescription(
            "conditions:delete-disabled-menu:" + condition.id, deleteTooltip,
            0.2f, ImGuiHoveredFlags_AllowWhenDisabled);
      }
      ImGui::EndPopup();
    }

    if (!deleteHovered && ImGui::BeginDragDropSource()) {
      DraggedConditionPayload payload{};
      std::snprintf(payload.conditionId.data(), payload.conditionId.size(),
                    "%s", condition.id.c_str());
      ImGui::SetDragDropPayload("SVS_CONDITION", &payload, sizeof(payload));
      ImGui::TextUnformatted(condition.name.c_str());
      if (!condition.description.empty()) {
        ImGui::TextUnformatted(condition.description.c_str());
      }
      ImGui::EndDragDropSource();
    }
    ImGui::TableSetColumnIndex(1);
    const auto *checkboxTable = ImGui::GetCurrentTable();
    if (checkboxTable != nullptr) {
      const auto checkboxCellRect = ImGui::TableGetCellBgRect(checkboxTable, 1);
      const auto checkboxSize =
          ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());
      ImGui::SetCursorScreenPos(
          ImVec2(checkboxCellRect.Min.x +
                     ((checkboxCellRect.Max.x - checkboxCellRect.Min.x) -
                      checkboxSize.x) *
                         0.5f,
                 checkboxCellRect.Min.y +
                     ((checkboxCellRect.Max.y - checkboxCellRect.Min.y) -
                      checkboxSize.y) *
                         0.5f));
    }
    bool disabled = !enabled;
    ImGui::BeginDisabled(broken);
    if (ImGui::Checkbox("##condition-disabled", &disabled)) {
      condition.EnsureCatalog().enabled = !disabled;
      conditions::EraseConditionStatusCache(condition.id);
      BumpConditionStoreRevision();
    }
    ImGui::EndDisabled();
    if (broken) {
      ui::condition_widgets::DrawHoverDescription(
          "conditions:broken:" + condition.id,
          localization->Get("conditions.broken_tooltip"),
          0.2f, ImGuiHoveredFlags_AllowWhenDisabled);
    }
    if (const auto *rowTable = ImGui::GetCurrentTable(); rowTable != nullptr) {
      reorderRowRects.back() = ImGui::TableGetCellBgRect(rowTable, 0);
    }
    ImGui::PopID();
  }

  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  if (ImGui::Button(localization->GetCStr("common.add_new"),
                    ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
    OpenNewConditionDialog();
  }
  ImGui::TableSetColumnIndex(1);
  ImGui::Dummy(ImVec2(0.0f, 0.0f));

  const auto *conditionTable = ImGui::GetCurrentTable();
  const bool hasConditionTable = conditionTable != nullptr;
  const auto conditionTableRect =
      hasConditionTable ? conditionTable->OuterRect : ImRect{};
  const auto reorderPreview =
      ImGui::IsDragDropActive() && hasConditionTable && !reorderRowRects.empty()
          ? ui::table_reorder::ComputeLinearReorderPreview(
                reorderRowRects, conditionTableRect.Min.x + 2.0f,
                conditionTableRect.Max.x - 2.0f)
          : ui::table_reorder::LinearReorderPreview{};
  ImGui::EndTable();
  if (hasConditionTable) {
    ui::table_reorder::DrawLinearReorderInsertionLine(
        reorderPreview,
        ImGui::GetColorU32(ThemeConfig::GetSingleton()->GetActive("PRIMARY")),
        3.0f);
  }

  if (reorderPreview.HasHoveredSlot() &&
      ImGui::BeginDragDropTargetCustom(
          reorderPreview.hoveredSlotRect,
          ImGui::GetID(("##condition-reorder-slot-" +
                        std::to_string(*reorderPreview.hoveredSlotIndex))
                           .c_str()))) {
    if (const auto *payload = ImGui::AcceptDragDropPayload(
            "SVS_CONDITION", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
        payload && payload->DataSize == sizeof(DraggedConditionPayload)) {
      DraggedConditionPayload dragPayload{};
      std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));
      if (const auto it = std::ranges::find(
              ConditionDefinitions(),
              std::string_view(dragPayload.conditionId.data()),
              &ConditionDefinition::id);
          it != ConditionDefinitions().end()) {
        const auto sourceIndex = static_cast<std::size_t>(
            std::distance(ConditionDefinitions().begin(), it));
        if (const auto filteredSourceIt =
                std::ranges::find(catalogIndices, sourceIndex);
            filteredSourceIt != catalogIndices.end()) {
          MoveFilteredConditionDefinitionToSlot(
              ConditionDefinitions(), catalogIndices,
              static_cast<std::size_t>(
                  std::distance(catalogIndices.begin(), filteredSourceIt)),
              *reorderPreview.hoveredSlotIndex);
          BumpConditionStoreRevision();
        }
      }
    }
    ImGui::EndDragDropTarget();
  }

  const auto buildExtraNameConflict = [&]() {
    return [&](std::string_view a_candidate) {
      return std::ranges::any_of(
          ConditionEditors(),
          [&](const ui::conditions::editor::State &a_editor) {
            return a_editor.isNew && strings::CompareTextInsensitive(
                                         strings::TrimText(a_editor.draft.name),
                                         a_candidate) == 0;
          });
    };
  };

  if (pendingCopyIndex && *pendingCopyIndex < ConditionDefinitions().size()) {
    const auto &source = ConditionDefinitions()[*pendingCopyIndex];
    auto copy = source;
    copy.id = conditions::BuildConditionId(NextConditionId()++);
    copy.name = ui::condition_editor::BuildUniqueConditionName(
        source.name, ConditionDefinitions(), source.id,
        buildExtraNameConflict());
    if (auto *catalog = copy.GetCatalog(); catalog != nullptr) {
      const auto existingColors = CollectCatalogColorsForNewCondition(
          ConditionDefinitions(), ConditionEditors());
      catalog->color = conditions::PickDistinctConditionColor(existingColors);
    }
    ConditionDefinitions().push_back(std::move(copy));
    BumpConditionStoreRevision();
    conditions::RebuildConditionDependencyMetadata(ConditionDefinitions());
    conditions::InvalidateConditionMaterializationCaches(
        ConditionDefinitions());
  }

  if (pendingCopyToLibraryIndex &&
      *pendingCopyToLibraryIndex < ConditionDefinitions().size()) {
    const auto &source = ConditionDefinitions()[*pendingCopyToLibraryIndex];
    auto libraryCopy = source;
    libraryCopy.EnsureLibrary();
    libraryCopy.name = ui::condition_editor::BuildUniqueConditionName(
        source.name, ConditionDefinitions(), source.id,
        buildExtraNameConflict());
    libraryCopy.id = libraryCopy.name;

    conditions::LibraryChangeResult saveResult;
    std::string error;
    if (!conditions::CommitLibraryConditionEdit(
            ConditionDefinitions(), {}, libraryCopy, saveResult, error)) {
      logger::error("Failed to copy SVS condition {} to library: {}", source.id,
                    error);
    } else {
      ApplyLibraryChangeResult(saveResult);
    }
  }

  if (pendingMoveToLibraryIndex &&
      *pendingMoveToLibraryIndex < ConditionDefinitions().size()) {
    const auto &source = ConditionDefinitions()[*pendingMoveToLibraryIndex];
    auto libraryVersion = source;
    libraryVersion.EnsureLibrary();
    libraryVersion.id = libraryVersion.name;

    conditions::LibraryChangeResult saveResult;
    std::string error;
    if (!conditions::CommitLibraryConditionEdit(ConditionDefinitions(),
                                                source.id, libraryVersion,
                                                saveResult, error)) {
      logger::error("Failed to move SVS condition {} to library: {}", source.id,
                    error);
    } else {
      ApplyLibraryChangeResult(saveResult);
    }
  }

  if (pendingDeleteIndex &&
      *pendingDeleteIndex < ConditionDefinitions().size()) {
    const auto deletedConditionId =
        ConditionDefinitions()[*pendingDeleteIndex].id;
    workbench_.DeleteRowsByConditionId(deletedConditionId, true);
    ConditionDefinitions().erase(
        ConditionDefinitions().begin() +
        static_cast<std::ptrdiff_t>(*pendingDeleteIndex));
    BumpConditionStoreRevision();
    sosr::conditions::RebuildConditionDependencyMetadata(
        ConditionDefinitions());
    sosr::conditions::InvalidateConditionMaterializationCaches(
        ConditionDefinitions());
    for (auto &editor : ConditionEditors()) {
      if (editor.sourceConditionId == deletedConditionId) {
        editor.error.clear();
        editor.open = false;
      }
    }
  }

  return rowClicked;
}

void Menu::DrawConditionLibraryTable() {
  auto *localization = ui::Localization::GetSingleton();
  auto libraryIndices = BuildConditionIndicesByKind(
      ConditionDefinitions(), conditions::DefinitionKind::Library);
  SortConditionIndicesByName(ConditionDefinitions(), libraryIndices);
  if (!ImGui::BeginTable("##condition-library-table", 2,
                         ImGuiTableFlags_SizingStretchProp |
                             ImGuiTableFlags_Resizable |
                             ImGuiTableFlags_PadOuterX | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_BordersInnerV,
                         ImVec2(0.0f, 0.0f))) {
    return;
  }

  ImGui::TableSetupColumn(localization->GetCStr("common.name"),
                          ImGuiTableColumnFlags_WidthFixed, 130.0f);
  ImGui::TableSetupColumn(
      localization->GetCStr("options.description"),
      ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableHeadersRow();

  std::optional<std::size_t> pendingDeleteIndex;
  for (const auto index : libraryIndices) {
    auto &condition = ConditionDefinitions()[index];
    const auto conditionStatus =
        conditions::EvaluateDefinitionStatus(condition, ConditionDefinitions());
    const bool broken = conditionStatus.IsBroken();
    ConditionDeleteUsage deleteUsage;
    for (const auto &otherCondition : ConditionDefinitions()) {
      if (otherCondition.id == condition.id) {
        continue;
      }
      if (std::ranges::any_of(
              otherCondition.clauses, [&](const ConditionClause &a_clause) {
                return a_clause.customConditionId == condition.id;
              })) {
        ++deleteUsage.referencingConditionCount;
      }
    }
    const auto deleteTooltip = deleteUsage.BuildTooltip();
    const bool deleteEnabled = deleteUsage.CanDelete();
    ImGui::PushID(static_cast<int>(index));
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    const auto rowMin = ImGui::GetCursorScreenPos();
    ImGui::Selectable("##library-row-hit", false,
                      ImGuiSelectableFlags_SpanAllColumns |
                          ImGuiSelectableFlags_AllowDoubleClick,
                      ImVec2(0.0f, 0.0f));
    const bool rowHovered = ImGui::IsItemHovered();
    ImGui::SetCursorScreenPos(rowMin);
    if (broken) {
      ImGui::TextColored(ThemeConfig::GetSingleton()->GetColor("WARN"), "%s",
                         condition.name.c_str());
    } else {
      ImGui::TextUnformatted(condition.name.c_str());
    }
    if (rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      OpenConditionEditorDialog(index);
    }
    DrawConditionTooltip(condition, rowHovered, ConditionDefinitions(), false);
    if (ImGui::BeginPopupContextItem("##library-row-context")) {
      if (ImGui::MenuItem(localization->GetCStr("common.edit"))) {
        OpenConditionEditorDialog(index);
      }
      ImGui::Separator();
      ImGui::PushStyleColor(
          ImGuiCol_Text,
          ImGui::ColorConvertU32ToFloat4(
              ThemeConfig::GetSingleton()->GetColorU32("DECLINE")));
      if (ImGui::MenuItem(localization->GetCStr("common.delete"),
                          nullptr, false, deleteEnabled)) {
        pendingDeleteIndex = index;
      }
      ImGui::PopStyleColor();
      if (!deleteEnabled) {
        ui::condition_widgets::DrawHoverDescription(
            "conditions:library:delete-disabled:" + condition.id, deleteTooltip,
            0.2f, ImGuiHoveredFlags_AllowWhenDisabled);
      }
      ImGui::EndPopup();
    }

    ImGui::TableSetColumnIndex(1);
    if (condition.description.empty()) {
      ImGui::TextDisabled(
          "%s", localization->GetCStr("conditions.no_description"));
    } else {
      const auto *table = ImGui::GetCurrentTable();
      if (table != nullptr) {
        const auto cellRect = ImGui::TableGetCellBgRect(table, 1);
        const auto &style = ImGui::GetStyle();
        const auto textMin = ImVec2(cellRect.Min.x + style.CellPadding.x,
                                    cellRect.Min.y + style.CellPadding.y);
        const auto textMax = ImVec2(cellRect.Max.x - style.CellPadding.x,
                                    textMin.y + ImGui::GetTextLineHeight());
        const auto textSize =
            ImGui::CalcTextSize(condition.description.c_str());
        if (broken) {
          ImGui::PushStyleColor(ImGuiCol_Text,
                                ThemeConfig::GetSingleton()->GetColor("WARN"));
        }
        ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), textMin, textMax,
                                  textMax.x, condition.description.c_str(),
                                  nullptr, &textSize);
        if (broken) {
          ImGui::PopStyleColor();
        }
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight()));
      } else {
        if (broken) {
          ImGui::PushStyleColor(ImGuiCol_Text,
                                ThemeConfig::GetSingleton()->GetColor("WARN"));
        }
        ImGui::TextUnformatted(condition.description.c_str());
        if (broken) {
          ImGui::PopStyleColor();
        }
      }
    }
    ImGui::PopID();
  }

  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  if (ImGui::Button(localization->GetCStr("common.add_new"),
                    ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
    OpenNewLibraryConditionDialog();
  }
  ImGui::TableSetColumnIndex(1);
  ImGui::Dummy(ImVec2(0.0f, 0.0f));
  ImGui::EndTable();

  if (pendingDeleteIndex &&
      *pendingDeleteIndex < ConditionDefinitions().size()) {
    const auto deletedConditionId =
        ConditionDefinitions()[*pendingDeleteIndex].id;
    conditions::LibraryChangeResult deleteResult;
    std::string error;
    if (!conditions::CommitLibraryConditionDelete(
            ConditionDefinitions(), deletedConditionId, deleteResult, error)) {
      logger::error("Failed to delete SVS library condition {}: {}",
                    deletedConditionId, error);
      return;
    }
    ConditionDefinitions() = std::move(deleteResult.definitions);
    BumpConditionStoreRevision();
    conditions::RebuildConditionDependencyMetadata(ConditionDefinitions());
    conditions::InvalidateConditionMaterializationCaches(
        ConditionDefinitions());
    for (auto &editor : ConditionEditors()) {
      if (editor.sourceConditionId == deletedConditionId) {
        editor.error.clear();
        editor.open = false;
      }
    }
  }
}
} // namespace sosr
