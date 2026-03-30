#pragma once

#include "imgui.h"
#include "ui/ConditionData.h"
#include "ui/ThemeConfig.h"
#include "ui/WorkbenchConflicts.h"
#include "ui/workbench/FilterState.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sosr::ui::workbench {
struct RowConditionVisualState {
  std::optional<ImVec4> color;
  std::string name;
  std::string description;
  bool disabled{false};
  bool missing{false};
  bool disabledCondition{false};
};

void DrawWrappedColoredTextRuns(
    std::initializer_list<std::pair<std::string_view, ImU32>> a_runs);

void DrawConflictEntry(
    const ui::workbench_conflicts::ConflictEntry &a_desc,
    const ThemeConfig *a_theme);

void DrawConflictTooltipSection(
    const char *a_title,
    const std::function<void(const ThemeConfig *)> &a_drawEntry);

void DrawSimplePinnableTooltip(const std::string_view a_id,
                               bool a_hoveredSource,
                               const std::function<void()> &a_drawBody);
void DrawWorkbenchFilterOptionTooltip(
    const ui::workbench::FilterOption &a_option,
    const std::vector<ui::conditions::Definition> &a_conditions);

RowConditionVisualState ResolveRowConditionVisualState(
    const ::sosr::workbench::VariantWorkbenchRow &a_row,
    const std::vector<ui::conditions::Definition> &a_conditions);
} // namespace sosr::ui::workbench
