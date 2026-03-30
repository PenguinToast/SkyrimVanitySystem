#pragma once

#include "imgui.h"

namespace sosr::ui::input_widgets {
struct RectClickTargetState {
  bool hovered{false};
  bool held{false};
  bool pressed{false};
};

[[nodiscard]] RectClickTargetState EvaluateRectClickTarget(
    ImGuiID a_id, const ImVec2 &a_min, const ImVec2 &a_max,
    ImGuiMouseButton a_mouseButton = ImGuiMouseButton_Left);

void DrawInputOutline(const ImVec2 &a_min, const ImVec2 &a_max, bool a_hovered,
                      bool a_active, float a_rounding = -1.0f);
} // namespace sosr::ui::input_widgets
