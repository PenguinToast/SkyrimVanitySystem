#pragma once

#include "imgui.h"
#include "workbench/Items.h"

#include <functional>
#include <optional>

namespace sosr::ui::components {
enum class EquipmentWidgetConflictStyle { None, Warning, Error };

struct EquipmentWidgetOptions {
  bool showDeleteButton{false};
  bool disabledAppearance{false};
  bool interactive{true};
  std::optional<ImVec4> accentColor;
  EquipmentWidgetConflictStyle conflictStyle{
      EquipmentWidgetConflictStyle::None};
  std::function<void()> drawTooltipExtras{};
  std::function<void()> drawContextMenuEntries{};
};

struct EquipmentWidgetResult {
  bool hovered{false};
  bool active{false};
  bool clicked{false};
  bool doubleClicked{false};
  bool deleteHovered{false};
  bool deleteClicked{false};
};

[[nodiscard]] bool
BuildEquipmentTooltipItem(RE::FormID a_formID, const char *a_key,
                          workbench::EquipmentWidgetItem &a_item);
void DrawEquipmentInfoTooltip(std::string_view a_tooltipId,
                              bool a_hoveredSource,
                              const workbench::EquipmentWidgetItem &a_item,
                              const std::function<void()> &a_drawExtras = {});
[[nodiscard]] EquipmentWidgetResult
DrawEquipmentWidget(const char *a_id,
                    const workbench::EquipmentWidgetItem &a_item,
                    const EquipmentWidgetOptions &a_options = {});
} // namespace sosr::ui::components
