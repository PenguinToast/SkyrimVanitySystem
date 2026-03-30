#pragma once

#include "imgui.h"

namespace sosr::ui::input_widgets {
void DrawInputOutline(const ImVec2 &a_min, const ImVec2 &a_max, bool a_hovered,
                      bool a_active, float a_rounding = -1.0f);
} // namespace sosr::ui::input_widgets
