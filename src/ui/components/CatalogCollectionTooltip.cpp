#include "ui/components/CatalogCollectionTooltip.h"

#include "ArmorUtils.h"
#include "imgui_internal.h"
#include "ui/ThemeConfig.h"
#include "ui/components/EquipmentWidget.h"
#include "ui/components/PinnableTooltip.h"

#include <algorithm>

namespace {
constexpr float kTreeIndentWidth = 18.0f;

struct GroupedTooltipItemNode {
  RE::FormID formID{0};
  std::int32_t level{-1};
  std::string cachedName;
  std::vector<sosr::CatalogCollectionItemNode> duplicates;
  std::vector<GroupedTooltipItemNode> children;
  std::size_t count{1};
  bool isCollection{false};
};

std::string GetNodeName(const sosr::CatalogCollectionItemNode &a_node) {
  if (!a_node.cachedName.empty()) {
    return a_node.cachedName;
  }

  if (const auto *gear =
          sosr::EquipmentCatalog::Get().FindGear(a_node.formID)) {
    return gear->name;
  }

  const auto *form = RE::TESForm::LookupByID(a_node.formID);
  return form ? sosr::armor::GetDisplayName(form)
              : ("Form " + sosr::armor::FormatFormID(a_node.formID));
}

std::string GetNodeName(const GroupedTooltipItemNode &a_node) {
  if (!a_node.cachedName.empty()) {
    return a_node.cachedName;
  }

  return GetNodeName(
      sosr::CatalogCollectionItemNode{.formID = a_node.formID,
                                      .cachedName = a_node.cachedName,
                                      .level = a_node.level});
}

std::string GetNodeSlots(const sosr::CatalogCollectionItemNode &a_node) {
  if (const auto *gear =
          sosr::EquipmentCatalog::Get().FindGear(a_node.formID)) {
    return sosr::armor::JoinStrings(gear->slots);
  }

  return {};
}

std::string GetNodeSlots(const GroupedTooltipItemNode &a_node) {
  return GetNodeSlots(
      sosr::CatalogCollectionItemNode{.formID = a_node.formID,
                                      .cachedName = a_node.cachedName,
                                      .level = a_node.level});
}

std::string FormatLevelLabel(const std::int32_t a_level) {
  if (a_level < 0) {
    return "Any";
  }

  return "Level " + std::to_string(a_level);
}

void DrawTooltipInfoRow(const char *a_icon, const std::string &a_label,
                        const std::string &a_value) {
  if (a_value.empty()) {
    return;
  }

  const auto *theme = sosr::ThemeConfig::GetSingleton();
  ImGui::TableNextRow();

  ImGui::TableSetColumnIndex(0);
  if (a_icon && a_icon[0] != '\0') {
    ImGui::TextColored(theme->GetColor("PRIMARY"), "%s", a_icon);
    ImGui::SameLine(0.0f, 6.0f);
  }
  ImGui::TextDisabled("%s", a_label.c_str());

  ImGui::TableSetColumnIndex(1);
  const auto availableWidth = ImGui::GetContentRegionAvail().x;
  const auto valueWidth = ImGui::CalcTextSize(a_value.c_str()).x;
  if (valueWidth < availableWidth) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - valueWidth);
    ImGui::TextUnformatted(a_value.c_str());
  } else {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + availableWidth);
    ImGui::TextWrapped("%s", a_value.c_str());
    ImGui::PopTextWrapPos();
  }
}

struct ItemTreeMetrics {
  float widestLabelWidth{0.0f};
  float widestValueWidth{0.0f};
};

void MergeGroupedChildren(std::vector<GroupedTooltipItemNode> &a_target,
                          const std::vector<GroupedTooltipItemNode> &a_source) {
  for (const auto &sourceNode : a_source) {
    auto existingIt = std::find_if(
        a_target.begin(), a_target.end(),
        [&](const GroupedTooltipItemNode &a_node) {
          return GetNodeName(a_node) == GetNodeName(sourceNode) &&
                 a_node.isCollection == sourceNode.isCollection &&
                 (!a_node.isCollection
                      ? GetNodeSlots(a_node) == GetNodeSlots(sourceNode)
                      : true);
        });

    if (existingIt != a_target.end()) {
      existingIt->count += sourceNode.count;
      MergeGroupedChildren(existingIt->children, sourceNode.children);
      continue;
    }

    a_target.push_back(sourceNode);
  }
}

auto BuildGroupedItemTree(
    const std::vector<sosr::CatalogCollectionItemNode> &a_items)
    -> std::vector<GroupedTooltipItemNode> {
  std::vector<GroupedTooltipItemNode> grouped;
  grouped.reserve(a_items.size());

  for (const auto &item : a_items) {
    const auto groupedChildren = BuildGroupedItemTree(item.GetChildren());
    const bool isCollection = !groupedChildren.empty();

    auto existingIt = std::find_if(
        grouped.begin(), grouped.end(),
        [&](const GroupedTooltipItemNode &a_node) {
          return GetNodeName(a_node) == GetNodeName(item) &&
                 a_node.isCollection == isCollection &&
                 (!isCollection ? GetNodeSlots(a_node) == GetNodeSlots(item)
                                : true);
        });

    GroupedTooltipItemNode node{};
    node.formID = item.formID;
    node.level = item.level;
    node.cachedName = item.cachedName;
    node.duplicates.push_back(item);
    node.children = groupedChildren;
    node.count = 1;
    node.isCollection = isCollection;
    if (existingIt != grouped.end()) {
      existingIt->count += 1;
      existingIt->duplicates.push_back(item);
      MergeGroupedChildren(existingIt->children, node.children);
    } else {
      grouped.push_back(std::move(node));
    }
  }

  return grouped;
}

std::string BuildDisplayLabel(const GroupedTooltipItemNode &a_item) {
  if (a_item.count <= 1) {
    return GetNodeName(a_item);
  }
  return GetNodeName(a_item) + " x" + std::to_string(a_item.count);
}

void MeasureItemTree(const std::vector<GroupedTooltipItemNode> &a_items,
                     int a_depth, ItemTreeMetrics &a_metrics) {
  for (const auto &item : a_items) {
    const auto label = BuildDisplayLabel(item);
    a_metrics.widestLabelWidth =
        (std::max)(a_metrics.widestLabelWidth,
                   ImGui::CalcTextSize(label.c_str()).x +
                       static_cast<float>(a_depth) * kTreeIndentWidth);
    const auto slots = GetNodeSlots(item);
    a_metrics.widestValueWidth = (std::max)(
        a_metrics.widestValueWidth, ImGui::CalcTextSize(slots.c_str()).x);

    if (!item.children.empty()) {
      MeasureItemTree(item.children, a_depth + 1, a_metrics);
    }
  }
}

void DrawDuplicateItemsTooltip(const std::string_view a_tooltipId,
                               const bool a_hoveredSource,
                               const GroupedTooltipItemNode &a_item) {
  if (a_item.duplicates.size() <= 1 ||
      !sosr::ui::components::ShouldDrawPinnableTooltip(a_tooltipId,
                                                       a_hoveredSource)) {
    return;
  }

  float widestNameWidth = 0.0f;
  float widestLevelWidth = 0.0f;
  for (const auto &duplicate : a_item.duplicates) {
    const auto duplicateName = GetNodeName(duplicate);
    widestNameWidth = (std::max)(widestNameWidth,
                                 ImGui::CalcTextSize(duplicateName.c_str()).x);
    widestLevelWidth = (std::max)(
        widestLevelWidth,
        ImGui::CalcTextSize(FormatLevelLabel(duplicate.level).c_str()).x);
  }

  ImGui::SetNextWindowSize(
      ImVec2((std::max)(360.0f, widestNameWidth + widestLevelWidth + 64.0f +
                                    ImGui::GetStyle().WindowPadding.x * 2.0f),
             0.0f),
      ImGuiCond_Always);
  sosr::ui::components::DrawPinnableTooltip(
      a_tooltipId, a_hoveredSource, [&]() {
        ImGui::TextUnformatted("Duplicate Leveled Entries");
        ImGui::Spacing();
        if (ImGui::BeginTable("##duplicate-items", 2,
                              ImGuiTableFlags_NoSavedSettings |
                                  ImGuiTableFlags_SizingFixedFit)) {
          ImGui::TableSetupColumn("##duplicate-name",
                                  ImGuiTableColumnFlags_WidthFixed,
                                  widestNameWidth + 8.0f);
          ImGui::TableSetupColumn("##duplicate-level",
                                  ImGuiTableColumnFlags_WidthStretch);

          for (std::size_t index = 0; index < a_item.duplicates.size();
               ++index) {
            const auto &duplicate = a_item.duplicates[index];
            const auto duplicateName = GetNodeName(duplicate);
            const auto duplicateTooltipId =
                std::string(a_tooltipId) + "/item/" + std::to_string(index);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            const auto *table = ImGui::GetCurrentTable();
            const auto rowContentPos = ImGui::GetCursorScreenPos();
            const auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
            const auto rowWidth =
                table != nullptr ? table->WorkRect.Max.x - table->WorkRect.Min.x
                                 : ImGui::GetContentRegionAvail().x;
            if (table != nullptr) {
              ImGui::SetCursorScreenPos(
                  ImVec2(table->WorkRect.Min.x, rowContentPos.y));
            }
            ImGui::InvisibleButton(("##duplicate-row-hit-" +
                                    std::string(a_tooltipId) + "-" +
                                    std::to_string(index))
                                       .c_str(),
                                   ImVec2(rowWidth, rowHeight));
            const bool rowHovered = ImGui::IsItemHovered();
            ImGui::SetCursorScreenPos(rowContentPos);
            ImGui::TextUnformatted(duplicateName.c_str());

            ImGui::TableSetColumnIndex(1);
            const auto levelLabel = FormatLevelLabel(duplicate.level);
            const auto availableWidth = ImGui::GetContentRegionAvail().x;
            const auto levelWidth = ImGui::CalcTextSize(levelLabel.c_str()).x;
            if (levelWidth < availableWidth) {
              ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth -
                                   levelWidth);
            }
            ImGui::TextUnformatted(levelLabel.c_str());

            if (duplicate.formID != 0) {
              sosr::workbench::EquipmentWidgetItem tooltipItem{};
              const auto duplicateItemKey =
                  "duplicate:" + std::to_string(index);
              if (sosr::ui::components::BuildEquipmentTooltipItem(
                      duplicate.formID, duplicateItemKey.c_str(),
                      tooltipItem)) {
                sosr::ui::components::DrawEquipmentInfoTooltip(
                    duplicateTooltipId, rowHovered, tooltipItem);
              }
            }
          }

          ImGui::EndTable();
        }
      });
}

void DrawItemTreeRows(const std::vector<GroupedTooltipItemNode> &a_items,
                      int a_depth, const std::string_view a_tooltipIdPrefix,
                      const std::string_view a_pathPrefix = {}) {
  const auto *theme = sosr::ThemeConfig::GetSingleton();
  for (std::size_t index = 0; index < a_items.size(); ++index) {
    const auto &item = a_items[index];
    const auto rowPath = a_pathPrefix.empty() ? std::to_string(index)
                                              : std::string(a_pathPrefix) +
                                                    "/" + std::to_string(index);
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    const auto *table = ImGui::GetCurrentTable();
    const auto rowContentPos = ImGui::GetCursorScreenPos();
    const auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
    const auto rowWidth = table != nullptr
                              ? table->WorkRect.Max.x - table->WorkRect.Min.x
                              : ImGui::GetContentRegionAvail().x;
    if (table != nullptr) {
      ImGui::SetCursorScreenPos(ImVec2(table->WorkRect.Min.x, rowContentPos.y));
    }
    ImGui::InvisibleButton(("##collection-row-hit-" +
                            std::string(a_tooltipIdPrefix) + "-" + rowPath)
                               .c_str(),
                           ImVec2(rowWidth, rowHeight));
    const bool rowHovered = ImGui::IsItemHovered();
    ImGui::SetCursorScreenPos(rowContentPos);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                         static_cast<float>(a_depth) * kTreeIndentWidth);
    const auto label = BuildDisplayLabel(item);
    if (item.isCollection) {
      ImGui::TextColored(theme->GetColor("TEXT_HEADER"), "%s", label.c_str());
    } else {
      ImGui::TextUnformatted(label.c_str());
    }

    ImGui::TableSetColumnIndex(1);
    const auto itemSlots = GetNodeSlots(item);
    if (!item.isCollection && !itemSlots.empty()) {
      const auto availableWidth = ImGui::GetContentRegionAvail().x;
      const auto slotsWidth = ImGui::CalcTextSize(itemSlots.c_str()).x;
      if (slotsWidth < availableWidth) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth -
                             slotsWidth);
      }
      ImGui::TextUnformatted(itemSlots.c_str());
    }

    const auto rowTooltipId =
        std::string(a_tooltipIdPrefix) + "/row/" + rowPath;
    if (item.count > 1) {
      DrawDuplicateItemsTooltip(rowTooltipId + "/duplicates", rowHovered, item);
    } else if (!item.isCollection && item.formID != 0) {
      sosr::workbench::EquipmentWidgetItem tooltipItem{};
      if (sosr::ui::components::BuildEquipmentTooltipItem(
              item.formID, rowTooltipId.c_str(), tooltipItem)) {
        sosr::ui::components::DrawEquipmentInfoTooltip(rowTooltipId + "/item",
                                                       rowHovered, tooltipItem);
      }
    }

    if (!item.children.empty()) {
      DrawItemTreeRows(item.children, a_depth + 1, a_tooltipIdPrefix, rowPath);
    }
  }
}
} // namespace

namespace sosr::ui::components {
void DrawCatalogCollectionTooltip(
    const std::string_view a_id, const bool a_hoveredSource,
    std::string_view a_title,
    const std::vector<CatalogTooltipMetaRow> &a_metaRows,
    const std::vector<sosr::CatalogCollectionItemNode> &a_items) {
  const auto *theme = sosr::ThemeConfig::GetSingleton();
  const auto &style = ImGui::GetStyle();
  float widestMetaValueWidth = ImGui::CalcTextSize(a_title.data()).x;
  for (const auto &row : a_metaRows) {
    widestMetaValueWidth = (std::max)(widestMetaValueWidth,
                                      ImGui::CalcTextSize(row.value.c_str()).x);
  }

  const auto groupedItems = BuildGroupedItemTree(a_items);
  ItemTreeMetrics itemMetrics{};
  MeasureItemTree(groupedItems, 0, itemMetrics);

  const auto metaSectionWidth = widestMetaValueWidth + 190.0f;
  const auto itemSectionWidth =
      itemMetrics.widestLabelWidth + itemMetrics.widestValueWidth +
      style.CellPadding.x * 4.0f + style.ItemSpacing.x + 28.0f;
  const auto tooltipContentWidth =
      (std::max)(360.0f, (std::max)(metaSectionWidth, itemSectionWidth));
  if (!ShouldDrawPinnableTooltip(a_id, a_hoveredSource)) {
    return;
  }

  ImGui::SetNextWindowSize(
      ImVec2(tooltipContentWidth + style.WindowPadding.x * 2.0f, 0.0f),
      ImGuiCond_Always);
  DrawPinnableTooltip(a_id, a_hoveredSource, [&]() {
    const auto headerMin = ImGui::GetCursorScreenPos();
    const auto headerWidth = ImGui::GetContentRegionAvail().x;
    const auto headerHeight = ImGui::GetFontSize() * 2.4f;
    const auto headerMax =
        ImVec2(headerMin.x + headerWidth, headerMin.y + headerHeight);
    auto *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(headerMin, headerMax, theme->GetColorU32("BG"),
                            8.0f);
    drawList->AddRect(headerMin, headerMax, theme->GetColorU32("BORDER"), 8.0f);
    drawList->AddRectFilledMultiColor(
        headerMin, headerMax, theme->GetColorU32("PRIMARY", 0.18f),
        theme->GetColorU32("PRIMARY", 0.18f), theme->GetColorU32("NONE"),
        theme->GetColorU32("NONE"));

    const auto titleFontSize = ImGui::GetFontSize() * 1.15f;
    const auto titleSize =
        ImGui::CalcTextSize(a_title.data(), nullptr, false, headerWidth);
    drawList->AddText(
        ImGui::GetFont(), titleFontSize,
        ImVec2(headerMin.x + (headerWidth - titleSize.x) * 0.5f,
               headerMin.y + (headerHeight - titleFontSize) * 0.5f - 1.0f),
        theme->GetColorU32("TEXT"), a_title.data());
    ImGui::Dummy(ImVec2(headerWidth, headerHeight));

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, theme->GetColorU32("PRIMARY"));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    if (ImGui::BeginTable("##catalog-collection-meta", 2,
                          ImGuiTableFlags_NoSavedSettings |
                              ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed,
                              122.0f);
      ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
      for (const auto &row : a_metaRows) {
        DrawTooltipInfoRow(row.icon, row.label, row.value);
      }
      ImGui::EndTable();
    }

    if (!a_items.empty()) {
      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Separator, theme->GetColorU32("PRIMARY"));
      ImGui::Separator();
      ImGui::PopStyleColor();
      ImGui::Spacing();

      constexpr auto sectionTitle = "Included Items";
      const auto sectionWidth = ImGui::CalcTextSize(sectionTitle).x;
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                           (ImGui::GetContentRegionAvail().x - sectionWidth) *
                               0.5f);
      ImGui::TextUnformatted(sectionTitle);
      ImGui::Spacing();
      if (ImGui::BeginTable("##catalog-collection-items", 2,
                            ImGuiTableFlags_NoSavedSettings |
                                ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("##item-name", ImGuiTableColumnFlags_WidthFixed,
                                itemMetrics.widestLabelWidth + 8.0f);
        ImGui::TableSetupColumn("##item-slots",
                                ImGuiTableColumnFlags_WidthStretch);
        DrawItemTreeRows(groupedItems, 0, a_id);

        ImGui::EndTable();
      }
    }
  });
}
} // namespace sosr::ui::components
