#pragma once

namespace sosr::ui::workbench {
inline constexpr char kVariantItemPayloadType[] = "SVS_VARIANT_ITEM";
inline constexpr char kConditionPayloadType[] = "SVS_CONDITION";
inline constexpr char kIconEllipsis[] = "\xee\x82\xba"; // ICON_LC_ELLIPSIS
inline constexpr float kWorkbenchRowGapY = 20.0f;
inline constexpr float kWorkbenchOverrideGapY = 5.0f;
inline constexpr char kWorkbenchOverflowPopupId[] =
    "##workbench-toolbar-overflow";
} // namespace sosr::ui::workbench
