#pragma once

#include "ui/conditions/EditorState.h"

#include <vector>

namespace sosr::ui::conditions {
struct PaneState {
  int focusedEditorWindowSlot{0};
  float libraryPaneHeight{220.0f};
  std::vector<editor::State> editors;
};
} // namespace sosr::ui::conditions
