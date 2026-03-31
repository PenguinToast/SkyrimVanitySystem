#include "ui/InputWidgets.h"

#include "ui/ThemeConfig.h"

namespace sosr::ui::input_widgets {
RectClickTargetState
EvaluateRectClickTarget(const ImGuiID a_id, const ImVec2 &a_min,
                        const ImVec2 &a_max,
                        const ImGuiMouseButton a_mouseButton) {
  RectClickTargetState state{};
  state.hovered = ImGui::IsMouseHoveringRect(a_min, a_max, false);

  auto *storage = ImGui::GetStateStorage();
  if (state.hovered && ImGui::IsMouseClicked(a_mouseButton)) {
    storage->SetBool(a_id, true);
  }

  const bool primed = storage->GetBool(a_id, false);
  state.held = primed && ImGui::IsMouseDown(a_mouseButton);
  if (primed && ImGui::IsMouseReleased(a_mouseButton)) {
    state.pressed = state.hovered;
    storage->SetBool(a_id, false);
  }

  return state;
}

void DrawInputOutline(const ImVec2 &a_min, const ImVec2 &a_max,
                      const bool a_hovered, const bool a_active,
                      const float a_rounding) {
  if (!a_hovered && !a_active) {
    return;
  }

  auto *theme = ThemeConfig::GetSingleton();
  auto *drawList = ImGui::GetWindowDrawList();
  const auto rounding =
      a_rounding >= 0.0f ? a_rounding : ImGui::GetStyle().FrameRounding;
  const auto color = a_active ? theme->GetColorU32("PRIMARY")
                              : theme->GetColorU32("TABLE_HOVER", 0.75f);
  const auto thickness = a_active ? 2.0f : 1.5f;
  drawList->AddRect(a_min, a_max, color, rounding, 0, thickness);
}
} // namespace sosr::ui::input_widgets
