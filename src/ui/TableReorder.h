#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include <cstddef>
#include <optional>
#include <span>

namespace sosr::ui::table_reorder {
struct LinearReorderPreview {
  std::optional<std::size_t> hoveredSlotIndex;
  ImRect hoveredSlotRect{};
  float lineY{-1.0f};
  float lineX1{0.0f};
  float lineX2{0.0f};

  [[nodiscard]] bool HasHoveredSlot() const {
    return hoveredSlotIndex.has_value();
  }

  [[nodiscard]] bool HasInsertionLine() const {
    return lineY >= 0.0f && lineX2 > lineX1;
  }
};

[[nodiscard]] LinearReorderPreview
ComputeLinearReorderPreview(std::span<const ImRect> a_rowRects, float a_lineX1,
                            float a_lineX2, float a_edgeBandHalfHeight = 6.0f);

void DrawLinearReorderInsertionLine(const LinearReorderPreview &a_preview,
                                    ImU32 a_color, float a_thickness = 2.0f,
                                    const ImRect *a_clipRect = nullptr);
} // namespace sosr::ui::table_reorder
