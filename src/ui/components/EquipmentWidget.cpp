#include "ui/components/EquipmentWidget.h"

#include "ArmorUtils.h"
#include "imgui_internal.h"
#include "ui/InputWidgets.h"
#include "ui/ThemeConfig.h"
#include "ui/components/PinnableTooltip.h"
#include "workbench/ItemFactory.h"

#include <algorithm>
#include <string>

namespace {
constexpr std::string_view kIconEditorId = "\xee\x84\x8b";   // ICON_LC_LIST
constexpr std::string_view kIconPlugin = "\xee\x84\xac";     // ICON_LC_PACKAGE
constexpr std::string_view kIconFormId = "\xee\x83\xb2";     // ICON_LC_HASH
constexpr std::string_view kIconIdentifier = "\xee\x84\x87"; // ICON_LC_LINK
constexpr std::string_view kIconSlot = "\xee\x87\x89";       // ICON_LC_SHIRT
constexpr std::string_view kIconTrash = "\xee\x86\x8c";      // ICON_LC_TRASH

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void DrawTooltipInfoRow(const char *a_icon, const char *a_label,
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
  if (a_label && a_label[0] != '\0') {
    ImGui::TextDisabled("%s", a_label);
  }

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

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
float ComputeEquipmentTooltipWidth(
    const std::string &a_displayName, const std::string &a_editorID,
    const std::string &a_plugin, const std::string &a_formID,
    const std::string &a_identifier,
    const std::vector<std::string> &a_slotLabels,
    const std::vector<std::string> &a_addonSlotLabels) {
  float widestValueWidth = ImGui::CalcTextSize(a_displayName.c_str()).x;
  for (const auto *value : {a_editorID.c_str(), a_plugin.c_str(),
                            a_formID.c_str(), a_identifier.c_str()}) {
    widestValueWidth =
        (std::max)(widestValueWidth, ImGui::CalcTextSize(value).x);
  }
  for (const auto &slotLabel : a_slotLabels) {
    widestValueWidth =
        (std::max)(widestValueWidth, ImGui::CalcTextSize(slotLabel.c_str()).x);
  }
  for (const auto &slotLabel : a_addonSlotLabels) {
    widestValueWidth =
        (std::max)(widestValueWidth, ImGui::CalcTextSize(slotLabel.c_str()).x);
  }

  return (std::max)(330.0f, widestValueWidth + 190.0f);
}

void DrawEquipmentTooltipHeader(const std::string &a_displayName) {
  const auto *theme = sosr::ThemeConfig::GetSingleton();
  const auto headerMin = ImGui::GetCursorScreenPos();
  const auto headerWidth = ImGui::GetContentRegionAvail().x;
  const auto headerHeight = ImGui::GetFontSize() * 2.4f;
  const auto headerMax =
      ImVec2(headerMin.x + headerWidth, headerMin.y + headerHeight);
  auto *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(headerMin, headerMax, theme->GetColorU32("BG"), 8.0f);
  drawList->AddRect(headerMin, headerMax, theme->GetColorU32("BORDER"), 8.0f);
  drawList->AddRectFilledMultiColor(
      headerMin, headerMax, theme->GetColorU32("PRIMARY", 0.18f),
      theme->GetColorU32("PRIMARY", 0.18f), theme->GetColorU32("NONE"),
      theme->GetColorU32("NONE"));

  const auto titleFontSize = ImGui::GetFontSize() * 1.15f;
  const auto titleSize =
      ImGui::CalcTextSize(a_displayName.c_str(), nullptr, false, headerWidth);
  drawList->AddText(
      ImGui::GetFont(), titleFontSize,
      ImVec2(headerMin.x + (headerWidth - titleSize.x) * 0.5f,
             headerMin.y + (headerHeight - titleFontSize) * 0.5f - 1.0f),
      theme->GetColorU32("TEXT"), a_displayName.c_str());
  ImGui::Dummy(ImVec2(headerWidth, headerHeight));

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Separator, theme->GetColorU32("PRIMARY"));
  ImGui::Separator();
  ImGui::PopStyleColor();
  ImGui::Spacing();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void DrawEquipmentInfoTooltipBody(
    const sosr::workbench::EquipmentWidgetItem &a_item,
    const std::string &a_displayName, const std::string &a_editorID,
    const std::string &a_plugin, const std::string &a_formID,
    const std::string &a_identifier,
    const std::vector<std::string> &a_slotLabels,
    const std::vector<std::string> &a_addonSlotLabels) {
  const auto *form = RE::TESForm::LookupByID(a_item.formID);
  if (!form) {
    return;
  }

  DrawEquipmentTooltipHeader(a_displayName);

  if (ImGui::BeginTable("##equipment-info-tooltip", 2,
                        ImGuiTableFlags_NoSavedSettings |
                            ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed,
                            138.0f);
    ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

    DrawTooltipInfoRow(kIconEditorId.data(), "Editor ID", a_editorID);
    DrawTooltipInfoRow(kIconPlugin.data(), "Plugin", a_plugin);
    DrawTooltipInfoRow(kIconFormId.data(), "Form ID", a_formID);
    DrawTooltipInfoRow(kIconIdentifier.data(), "Identifier", a_identifier);
    for (std::size_t index = 0; index < a_slotLabels.size(); ++index) {
      DrawTooltipInfoRow(index == 0 ? kIconSlot.data() : "",
                         index == 0 ? "Slots" : "", a_slotLabels[index]);
    }
    for (std::size_t index = 0; index < a_addonSlotLabels.size(); ++index) {
      DrawTooltipInfoRow(index == 0 ? kIconSlot.data() : "",
                         index == 0 ? "Addon Slots" : "",
                         a_addonSlotLabels[index]);
    }

    ImGui::EndTable();
  }
}
} // namespace

namespace sosr::ui::components {
bool BuildEquipmentTooltipItem(const RE::FormID a_formID, const char *a_key,
                               workbench::EquipmentWidgetItem &a_item) {
  if (!workbench::BuildCatalogItem(a_formID, a_item)) {
    return false;
  }

  a_item.key = a_key ? a_key : "";
  return true;
}

void DrawEquipmentInfoTooltip(const std::string_view a_tooltipId,
                              const bool a_hoveredSource,
                              const workbench::EquipmentWidgetItem &a_item,
                              const std::function<void()> &a_drawExtras) {
  if (!ShouldDrawPinnableTooltip(a_tooltipId, a_hoveredSource)) {
    return;
  }

  const bool hasInfoBody = a_item.SupportsInfoTooltip();
  const auto *form =
      hasInfoBody ? RE::TESForm::LookupByID(a_item.formID) : nullptr;
  const auto displayName =
      form ? sosr::armor::GetDisplayName(form) : a_item.name;
  const auto editorID =
      hasInfoBody && form ? sosr::armor::GetEditorID(form) : std::string{};
  const auto plugin =
      hasInfoBody && form ? sosr::armor::GetPluginName(form) : std::string{};
  const auto formID = hasInfoBody
                          ? (form ? sosr::armor::FormatFormID(form->GetFormID())
                                  : sosr::armor::FormatFormID(a_item.formID))
                          : std::string{};
  const auto identifier = hasInfoBody && form
                              ? sosr::armor::GetFormIdentifier(form)
                              : std::string{};
  const auto slotLabels = hasInfoBody
                              ? sosr::armor::GetArmorSlotLabels(a_item.slotMask)
                              : std::vector<std::string>{};
  const auto addonSlotLabels =
      hasInfoBody && form && form->As<RE::TESObjectARMO>()
          ? sosr::armor::GetArmorAddonSlotLabels(form->As<RE::TESObjectARMO>())
          : std::vector<std::string>{};
  const auto tooltipContentWidth =
      hasInfoBody ? ComputeEquipmentTooltipWidth(displayName, editorID, plugin,
                                                 formID, identifier, slotLabels,
                                                 addonSlotLabels)
                  : 360.0f;
  ImGui::SetNextWindowSize(
      ImVec2(tooltipContentWidth + ImGui::GetStyle().WindowPadding.x * 2.0f,
             0.0f),
      ImGuiCond_Always);
  DrawPinnableTooltip(a_tooltipId, a_hoveredSource, [&]() {
    if (hasInfoBody) {
      DrawEquipmentInfoTooltipBody(a_item, displayName, editorID, plugin,
                                   formID, identifier, slotLabels,
                                   addonSlotLabels);
    } else {
      DrawEquipmentTooltipHeader(displayName);
    }
    if (a_drawExtras) {
      if (hasInfoBody || !displayName.empty()) {
        ImGui::Spacing();
      }
      a_drawExtras();
    }
  });
}

EquipmentWidgetResult
DrawEquipmentWidget(const char *a_id,
                    const workbench::EquipmentWidgetItem &a_item,
                    const EquipmentWidgetOptions &a_options) {
  ImGui::PushID(a_id);

  constexpr float paddingX = 10.0f;
  constexpr float paddingY = 7.0f;
  const auto lineHeight = ImGui::GetTextLineHeight();
  const auto frameHeight = paddingY * 2.0f + lineHeight * 2.0f + 4.0f;
  const auto width = ImGui::GetContentRegionAvail().x;
  const auto size = ImVec2(width > 0.0f ? width : 1.0f, frameHeight);

  ImGui::InvisibleButton("##equipment-widget", size);

  EquipmentWidgetResult result{};
  result.hovered = ImGui::IsItemHovered();
  result.active = ImGui::IsItemActive();
  result.clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  result.doubleClicked =
      result.hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

  const auto rectMin = ImGui::GetItemRectMin();
  const auto rectMax = ImGui::GetItemRectMax();
  auto *drawList = ImGui::GetWindowDrawList();
  const auto *theme = ThemeConfig::GetSingleton();
  const auto hasWarningConflict =
      a_options.conflictStyle == EquipmentWidgetConflictStyle::Warning;
  const auto hasErrorConflict =
      a_options.conflictStyle == EquipmentWidgetConflictStyle::Error;
  constexpr float accentWidth = 5.0f;

  ImU32 fillColor = theme->GetColorU32("BG_LIGHT");
  ImU32 borderColor = theme->GetColorU32("BORDER");
  if (a_options.disabledAppearance) {
    fillColor = theme->GetColorU32("BG", 0.92f);
    borderColor = theme->GetColorU32("TEXT_DISABLED", 0.55f);
  }
  if (hasWarningConflict) {
    fillColor = theme->GetColorU32("WARN", 0.28f);
    borderColor = theme->GetColorU32("WARN", 0.88f);
  } else if (hasErrorConflict) {
    fillColor = theme->GetColorU32("ERROR", 0.45f);
    borderColor = theme->GetColorU32("ERROR");
  }
  if (result.active) {
    if (hasWarningConflict) {
      fillColor = theme->GetColorU32("WARN", 0.48f);
      borderColor = theme->GetColorU32("WARN");
    } else if (hasErrorConflict) {
      fillColor = theme->GetColorU32("ERROR", 0.75f);
      borderColor = theme->GetColorU32("ERROR");
    } else {
      fillColor = theme->GetColorU32("PRIMARY", 0.80f);
      borderColor = theme->GetColorU32("PRIMARY");
    }
  } else if (result.hovered) {
    if (hasWarningConflict) {
      fillColor = theme->GetColorU32("WARN", 0.38f);
      borderColor = theme->GetColorU32("WARN");
    } else if (hasErrorConflict) {
      fillColor = theme->GetColorU32("ERROR", 0.62f);
      borderColor = theme->GetColorU32("ERROR");
    } else {
      fillColor = theme->GetColorU32("BG_LIGHT", 1.0f);
      borderColor = theme->GetColorU32("PRIMARY", 0.70f);
    }
  }

  drawList->AddRectFilled(rectMin, rectMax, fillColor, 8.0f);
  drawList->AddRect(rectMin, rectMax, borderColor, 8.0f);
  if (a_options.accentColor.has_value()) {
    drawList->AddRectFilled(rectMin, ImVec2(rectMin.x + accentWidth, rectMax.y),
                            ImGui::GetColorU32(*a_options.accentColor), 8.0f,
                            ImDrawFlags_RoundCornersTopLeft |
                                ImDrawFlags_RoundCornersBottomLeft);
  }

  const auto contentStartX =
      rectMin.x + paddingX +
      (a_options.accentColor.has_value() ? accentWidth : 0.0f);
  const auto namePos = ImVec2(contentStartX, rectMin.y + paddingY);
  const auto slotPos =
      ImVec2(contentStartX, rectMin.y + paddingY + lineHeight + 4.0f);
  const auto deletePaneWidth = a_options.showDeleteButton ? 34.0f : 0.0f;
  const auto clipMin = ImVec2(contentStartX, rectMin.y + paddingY);
  const auto clipMax =
      ImVec2(rectMax.x - paddingX - deletePaneWidth, rectMax.y - paddingY);
  const auto buttonMin = ImVec2(rectMax.x - deletePaneWidth, rectMin.y);
  const auto buttonMax = rectMax;
  bool deleteHeld = false;
  if (a_options.showDeleteButton) {
    const auto deleteState = ui::input_widgets::EvaluateRectClickTarget(
        ImGui::GetID("##equipment-widget-delete"), buttonMin, buttonMax);
    result.deleteHovered = deleteState.hovered;
    deleteHeld = deleteState.held;
    result.deleteClicked = deleteState.pressed;
  } else {
    result.deleteHovered = false;
  }
  if (result.deleteHovered) {
    result.clicked = false;
    result.doubleClicked = false;
    result.active = false;
  }
  const auto nameColor = a_options.disabledAppearance
                             ? theme->GetColorU32("TEXT_DISABLED")
                             : theme->GetColorU32("TEXT");
  const auto slotColor = a_options.disabledAppearance
                             ? theme->GetColorU32("TEXT_DISABLED", 0.82f)
                             : theme->GetColorU32("TEXT_HEADER", 0.92f);

  drawList->PushClipRect(clipMin, clipMax, true);
  drawList->AddText(namePos, nameColor, a_item.name.c_str());
  drawList->AddText(slotPos, slotColor, a_item.slotText.c_str());
  drawList->PopClipRect();

  if (a_options.showDeleteButton) {
    const auto deleteFill = deleteHeld ? theme->GetColorU32("DECLINE")
                            : result.deleteHovered
                                ? theme->GetColorU32("DECLINE", 0.95f)
                                : theme->GetColorU32("DECLINE", 0.78f);
    drawList->AddRectFilled(buttonMin, buttonMax, deleteFill, 8.0f,
                            ImDrawFlags_RoundCornersRight);
    drawList->AddLine(ImVec2(buttonMin.x, rectMin.y + 1.0f),
                      ImVec2(buttonMin.x, rectMax.y - 1.0f),
                      theme->GetColorU32("BORDER"), 1.0f);

    const char *deleteLabel = kIconTrash.data();
    const auto labelSize = ImGui::CalcTextSize(deleteLabel);
    drawList->AddText(
        ImVec2(buttonMin.x + ((deletePaneWidth - labelSize.x) * 0.5f),
               rectMin.y + ((frameHeight - labelSize.y) * 0.5f) - 1.0f),
        theme->GetColorU32("TEXT"), deleteLabel);
  }

  const auto tooltipId = "equipment:" + a_item.key;
  if ((a_item.SupportsInfoTooltip() || a_item.IsSlot() ||
       a_options.drawTooltipExtras) &&
      !result.deleteHovered && !ImGui::IsDragDropActive()) {
    DrawEquipmentInfoTooltip(tooltipId, result.hovered, a_item,
                             a_options.drawTooltipExtras);
  }

  const bool hasContextMenuEntries =
      static_cast<bool>(a_options.drawContextMenuEntries) ||
      a_options.showDeleteButton;
  if (hasContextMenuEntries &&
      ImGui::BeginPopupContextItem("##equipment-widget-context")) {
    bool drewCustomEntries = false;
    if (a_options.drawContextMenuEntries) {
      a_options.drawContextMenuEntries();
      drewCustomEntries = true;
    }

    if (a_options.showDeleteButton) {
      if (drewCustomEntries) {
        ImGui::Separator();
      }
      ImGui::PushStyleColor(ImGuiCol_Text, theme->GetColor("DECLINE"));
      ImGui::BeginDisabled(a_options.disabledAppearance);
      const std::string deleteLabel = std::string(kIconTrash) + " Delete";
      if (ImGui::MenuItem(deleteLabel.c_str())) {
        result.deleteClicked = true;
      }
      ImGui::EndDisabled();
      ImGui::PopStyleColor();
    }

    ImGui::EndPopup();
  }

  ImGui::PopID();
  return result;
}
} // namespace sosr::ui::components
