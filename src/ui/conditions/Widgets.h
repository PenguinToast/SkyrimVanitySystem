#pragma once

#include "ui/ConditionData.h"

#include <imgui.h>

#include <string_view>

namespace sosr::ui::condition_widgets {
void DrawHoverDescription(std::string_view a_id, std::string_view a_text,
                          float a_delaySeconds = 0.45f,
                          ImGuiHoveredFlags a_hoveredFlags = 0);
void DrawHoverDescription(std::string_view a_id, bool a_hoveredSource,
                          std::string_view a_text,
                          float a_delaySeconds = 0.45f);
void DrawConditionColorSwatch(const char *a_id,
                              const conditions::Color &a_color,
                              std::string_view a_tooltip);
void DrawConditionPlaceholderSwatch(const char *a_id,
                                    std::string_view a_tooltip);
[[nodiscard]] float
MeasureConditionRowHeight(const conditions::Definition &a_definition,
                          float a_wrapWidth);
} // namespace sosr::ui::condition_widgets
