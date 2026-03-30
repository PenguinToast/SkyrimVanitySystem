#include "ui/TableReorder.h"

#include <vector>

namespace sosr::ui::table_reorder {
LinearReorderPreview
ComputeLinearReorderPreview(std::span<const ImRect> a_rowRects,
                            const float a_lineX1, const float a_lineX2,
                            const float a_edgeBandHalfHeight) {
  LinearReorderPreview preview{
      .lineX1 = a_lineX1,
      .lineX2 = a_lineX2,
  };
  if (a_rowRects.empty() || a_lineX2 <= a_lineX1) {
    return preview;
  }

  std::vector<float> insertionLineYs;
  insertionLineYs.reserve(a_rowRects.size() + 1);
  insertionLineYs.push_back(a_rowRects.front().Min.y);
  for (std::size_t index = 0; index + 1 < a_rowRects.size(); ++index) {
    insertionLineYs.push_back((a_rowRects[index].Max.y +
                               a_rowRects[index + 1].Min.y) *
                              0.5f);
  }
  insertionLineYs.push_back(a_rowRects.back().Max.y);

  for (std::size_t slotIndex = 0; slotIndex < insertionLineYs.size();
       ++slotIndex) {
    const float lineY = insertionLineYs[slotIndex];
    const float bandMinY =
        slotIndex == 0 ? (lineY - a_edgeBandHalfHeight)
                       : ((insertionLineYs[slotIndex - 1] + lineY) * 0.5f);
    const float bandMaxY =
        slotIndex + 1 == insertionLineYs.size()
            ? (lineY + a_edgeBandHalfHeight)
            : ((lineY + insertionLineYs[slotIndex + 1]) * 0.5f);
    const ImRect slotRect(ImVec2(a_lineX1, bandMinY), ImVec2(a_lineX2, bandMaxY));
    if (!ImGui::IsMouseHoveringRect(slotRect.Min, slotRect.Max, false)) {
      continue;
    }

    preview.hoveredSlotIndex = slotIndex;
    preview.hoveredSlotRect = slotRect;
    preview.lineY = lineY;
    break;
  }

  return preview;
}

void DrawLinearReorderInsertionLine(const LinearReorderPreview &a_preview,
                                    const ImU32 a_color,
                                    const float a_thickness,
                                    const ImRect *a_clipRect) {
  if (!a_preview.HasInsertionLine()) {
    return;
  }

  auto *drawList = ImGui::GetWindowDrawList();
  if (a_clipRect != nullptr) {
    drawList->PushClipRect(a_clipRect->Min, a_clipRect->Max, false);
  }
  drawList->AddLine(ImVec2(a_preview.lineX1, a_preview.lineY),
                    ImVec2(a_preview.lineX2, a_preview.lineY), a_color,
                    a_thickness);
  if (a_clipRect != nullptr) {
    drawList->PopClipRect();
  }
}
} // namespace sosr::ui::table_reorder
