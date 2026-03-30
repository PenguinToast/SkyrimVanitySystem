#include "ui/conditions/Widgets.h"

#include "imgui_internal.h"
#include "ui/components/PinnableTooltip.h"

namespace sosr::ui::condition_widgets {
void DrawHoverDescription(const std::string_view a_id,
                          const std::string_view a_text,
                          const float a_delaySeconds,
                          const ImGuiHoveredFlags a_hoveredFlags) {
  if (!ImGui::IsItemHovered(a_hoveredFlags) ||
      ImGui::GetCurrentContext()->HoveredIdTimer < a_delaySeconds) {
    return;
  }

  components::DrawPinnableTooltip(a_id, true, [&]() {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 360.0f);
    ImGui::TextUnformatted(a_text.data(), a_text.data() + a_text.size());
    ImGui::PopTextWrapPos();
  });
}

void DrawConditionColorSwatch(const char *a_id,
                              const conditions::Color &a_color,
                              const std::string_view a_tooltip) {
  ImGui::ColorButton(
      a_id, conditions::ToImGuiColor(a_color),
      ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop |
          ImGuiColorEditFlags_NoBorder,
      ImVec2(ImGui::GetFrameHeight() - 2.0f, ImGui::GetFrameHeight() - 2.0f));
  DrawHoverDescription(std::string(a_id), a_tooltip);
}

void DrawConditionPlaceholderSwatch(const char *a_id,
                                    const std::string_view a_tooltip) {
  const auto size = ImVec2(ImGui::GetFrameHeight() - 2.0f,
                           ImGui::GetFrameHeight() - 2.0f);
  const auto min = ImGui::GetCursorScreenPos();
  const auto max = ImVec2(min.x + size.x, min.y + size.y);
  ImGui::InvisibleButton(a_id, size);
  ImGui::GetWindowDrawList()->AddRect(
      min, max, ImGui::GetColorU32(ImGuiCol_Border), 3.0f);
  DrawHoverDescription(std::string(a_id), a_tooltip);
}

float MeasureConditionRowHeight(const conditions::Definition &a_definition,
                                const float a_wrapWidth) {
  const auto &style = ImGui::GetStyle();
  const auto nameHeight = ImGui::GetTextLineHeight();
  if (a_definition.description.empty()) {
    return style.FramePadding.y * 2.0f + nameHeight;
  }

  const auto descriptionSize = ImGui::CalcTextSize(
      a_definition.description.c_str(), nullptr, false, a_wrapWidth);
  return style.FramePadding.y * 2.0f + nameHeight + style.ItemSpacing.y +
         descriptionSize.y;
}
} // namespace sosr::ui::condition_widgets
