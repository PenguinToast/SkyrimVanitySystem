#pragma once

#include "VariantWorkbench.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace sosr::ui::workbench_conflicts {
struct ConflictEntry {
  std::string primaryName;
  std::string secondaryName;
  bool isOverride{false};
  bool isHideConflict{false};
  std::string targetLabel;
};

struct RowConflictInfo {
  std::vector<std::string> targetWidgetIds;
  std::vector<ConflictEntry> targetDescriptions;
};

struct OverrideConflictInfo {
  std::vector<std::string> targetWidgetIds;
  std::vector<ConflictEntry> targetDescriptions;
};

struct ConflictState {
  std::unordered_map<std::string, RowConflictInfo> rowConflicts;
  std::unordered_map<std::string, OverrideConflictInfo> overrideConflicts;
};

[[nodiscard]] ConflictState BuildConflictState(
    const std::vector<::sosr::workbench::VariantWorkbenchRow> &a_rows);
} // namespace sosr::ui::workbench_conflicts
