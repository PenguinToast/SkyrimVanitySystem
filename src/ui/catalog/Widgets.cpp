#include "ui/catalog/Widgets.h"

#include "ArmorUtils.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "integrations/DynamicArmorVariantsExtendedClient.h"
#include "ui/ThemeConfig.h"
#include "ui/components/CatalogCollectionTooltip.h"
#include "ui/components/PinnableTooltip.h"

namespace {
constexpr std::string_view kIconEditorId = "\xee\x84\x8b";   // ICON_LC_LIST
constexpr std::string_view kIconPlugin = "\xee\x84\xac";     // ICON_LC_PACKAGE
constexpr std::string_view kIconFormId = "\xee\x83\xb2";     // ICON_LC_HASH
constexpr std::string_view kIconIdentifier = "\xee\x84\x87"; // ICON_LC_LINK
constexpr std::string_view kIconCollection =
    "\xee\x97\xbf";                                    // ICON_LC_FOLDER_CODE
constexpr std::string_view kIconFile = "\xee\x83\x87"; // ICON_LC_FILE_CODE
} // namespace

namespace sosr::ui::catalog {
std::string TruncateTextToWidth(std::string_view a_text, const float a_width) {
  if (a_text.empty() || a_width <= 0.0f) {
    return {};
  }

  if (ImGui::CalcTextSize(a_text.data(), a_text.data() + a_text.size()).x <=
      a_width) {
    return std::string(a_text);
  }

  constexpr std::string_view ellipsis = "...";
  const auto ellipsisWidth =
      ImGui::CalcTextSize(ellipsis.data(), ellipsis.data() + ellipsis.size()).x;
  if (ellipsisWidth >= a_width) {
    return std::string(ellipsis);
  }

  std::string truncated;
  truncated.reserve(a_text.size());
  for (char character : a_text) {
    truncated.push_back(character);
    const auto currentWidth =
        ImGui::CalcTextSize(truncated.data(),
                            truncated.data() + truncated.size())
            .x;
    if ((currentWidth + ellipsisWidth) > a_width) {
      truncated.pop_back();
      break;
    }
  }

  truncated.append(ellipsis);
  return truncated;
}

void DrawOutfitTooltip(const OutfitEntry &a_outfit,
                       const bool a_hoveredSource) {
  std::vector<components::CatalogTooltipMetaRow> metaRows;
  if (!a_outfit.editorID.empty()) {
    metaRows.push_back({kIconEditorId.data(), "Editor ID", a_outfit.editorID});
  }
  if (!a_outfit.plugin.empty()) {
    metaRows.push_back({kIconPlugin.data(), "Plugin", a_outfit.plugin});
  }
  metaRows.push_back(
      {kIconFormId.data(), "Form ID", armor::FormatFormID(a_outfit.formID)});
  if (const auto *form = RE::TESForm::LookupByID(a_outfit.formID)) {
    if (const auto identifier = armor::GetFormIdentifier(form);
        !identifier.empty()) {
      metaRows.push_back({kIconIdentifier.data(), "Identifier", identifier});
    }
  }

  components::DrawCatalogCollectionTooltip("outfit:" + a_outfit.id,
                                           a_hoveredSource, a_outfit.name,
                                           metaRows, a_outfit.GetItemTree());
}

void DrawKitTooltip(const KitEntry &a_kit, const bool a_hoveredSource) {
  std::vector<components::CatalogTooltipMetaRow> metaRows;
  metaRows.push_back({kIconCollection.data(), "Collection",
                      a_kit.collection.empty() ? "Root" : a_kit.collection});
  if (!a_kit.filepath.empty()) {
    metaRows.push_back({kIconFile.data(), "File", a_kit.filepath});
  }
  metaRows.push_back({kIconIdentifier.data(), "Identifier", a_kit.id});

  components::DrawCatalogCollectionTooltip("kit:" + a_kit.id, a_hoveredSource,
                                           a_kit.name, metaRows,
                                           a_kit.GetItemTree());
}

void DrawSimplePinnableTooltip(const std::string_view a_id,
                               const bool a_hoveredSource,
                               const std::function<void()> &a_drawBody) {
  components::DrawPinnableTooltip(a_id, a_hoveredSource, a_drawBody);
}

void DrawCatalogTabHelpTooltip(
    const std::string_view a_id, const bool a_hoveredSource,
    const std::initializer_list<const char *> a_lines) {
  const auto mousePos = ImGui::GetIO().MousePos;
  components::HoveredTooltipOptions tooltipOptions;
  tooltipOptions.useCustomPlacement = true;
  tooltipOptions.pos = ImVec2(mousePos.x - 2.0f, mousePos.y + 12.0f);
  tooltipOptions.pivot = ImVec2(1.0f, 0.0f);
  components::DrawPinnableTooltip(
      a_id, a_hoveredSource,
      [&]() {
        bool first = true;
        for (const auto *line : a_lines) {
          if (!first) {
            ImGui::Spacing();
          }
          ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 420.0f);
          ImGui::TextUnformatted(line);
          ImGui::PopTextWrapPos();
          first = false;
        }
      },
      tooltipOptions);
}

bool IsDelayedHover(const float a_delaySeconds) {
  return ImGui::IsItemHovered() &&
         ImGui::GetCurrentContext()->HoveredIdTimer >= a_delaySeconds;
}

std::optional<std::string> GetDavAvailabilityMessage() {
  return integrations::DynamicArmorVariantsExtendedClient::
      GetAvailabilityMessage();
}

void DrawDavAvailabilityBanner(const std::string_view a_message) {
  if (a_message.empty()) {
    return;
  }

  ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(88, 30, 30, 180));
  ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(196, 78, 78, 255));
  if (ImGui::BeginChild("##dav-unavailable-banner", ImVec2(0.0f, 0.0f),
                        ImGuiChildFlags_Borders |
                            ImGuiChildFlags_AutoResizeY)) {
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 205, 205, 255));
    ImGui::TextWrapped("%.*s", static_cast<int>(a_message.size()),
                       a_message.data());
    ImGui::PopStyleColor();
  }
  ImGui::EndChild();
  ImGui::PopStyleColor(2);
}
} // namespace sosr::ui::catalog
