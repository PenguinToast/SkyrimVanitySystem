#include "catalog/KitLayoutMetadata.h"

#include <nlohmann/json.hpp>

namespace sosr::catalog {
namespace {
std::string ToTargetKindText(const KitEntry::LayoutTargetKind a_kind) {
  return a_kind == KitEntry::LayoutTargetKind::Slot ? "slot" : "item";
}

std::optional<KitEntry::LayoutTargetKind>
ParseTargetKind(const nlohmann::json &a_json) {
  if (!a_json.is_string()) {
    return std::nullopt;
  }

  const auto value = a_json.get<std::string>();
  if (value == "item") {
    return KitEntry::LayoutTargetKind::Item;
  }
  if (value == "slot") {
    return KitEntry::LayoutTargetKind::Slot;
  }
  return std::nullopt;
}
} // namespace

nlohmann::json SerializeKitLayout(const KitEntry::Layout &a_layout) {
  nlohmann::json rows = nlohmann::json::array();
  for (const auto &row : a_layout.rows) {
    rows.push_back({{"targetKind", ToTargetKindText(row.targetKind)},
                    {"targetIdentifier", row.targetIdentifier},
                    {"targetSlotMask", row.targetSlotMask},
                    {"overrideIdentifiers", row.overrideIdentifiers},
                    {"hideEquipped", row.hideEquipped}});
  }

  return nlohmann::json{{"layoutRows", std::move(rows)}};
}

std::optional<KitEntry::Layout> ParseKitLayout(const nlohmann::json &a_json) {
  if (!a_json.is_object()) {
    return std::nullopt;
  }

  const auto rowsIt = a_json.find("layoutRows");
  if (rowsIt == a_json.end() || !rowsIt->is_array()) {
    return std::nullopt;
  }

  KitEntry::Layout layout;
  for (const auto &rowJson : *rowsIt) {
    if (!rowJson.is_object()) {
      continue;
    }

    const auto kind = ParseTargetKind(rowJson.value("targetKind", ""));
    const auto targetSlotMask = rowJson.value("targetSlotMask", 0ULL);
    if (!kind.has_value() || targetSlotMask == 0) {
      continue;
    }

    KitEntry::LayoutRow row;
    row.targetKind = *kind;
    row.targetIdentifier =
        rowJson.value("targetIdentifier", std::string{});
    row.targetSlotMask = targetSlotMask;
    row.hideEquipped = rowJson.value("hideEquipped", false);

    const auto overrideIt = rowJson.find("overrideIdentifiers");
    if (overrideIt != rowJson.end() && overrideIt->is_array()) {
      for (const auto &identifier : *overrideIt) {
        if (identifier.is_string()) {
          const auto value = identifier.get<std::string>();
          if (!value.empty()) {
            row.overrideIdentifiers.push_back(value);
          }
        }
      }
    }

    if (!row.hideEquipped && row.overrideIdentifiers.empty()) {
      continue;
    }

    layout.rows.push_back(std::move(row));
  }

  if (layout.rows.empty()) {
    return std::nullopt;
  }

  return layout;
}
} // namespace sosr::catalog
