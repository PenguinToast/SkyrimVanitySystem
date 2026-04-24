#include "VariantWorkbench.h"

#include "ArmorUtils.h"
#include "workbench/ItemFactory.h"

#include <nlohmann/json.hpp>
#include <unordered_set>

namespace {
constexpr std::uint32_t kSerializationType = 'ROWS';
constexpr std::uint32_t kSerializationVersion = 4;

std::string BuildRowKey(const std::string_view a_sourceKey,
                        const std::optional<std::string> &a_conditionId) {
  std::string key(a_sourceKey);
  key.append("|condition:");
  if (a_conditionId.has_value()) {
    key.append(*a_conditionId);
  } else {
    key.append("null");
  }
  return key;
}

void UpdateRowIdentity(sosr::workbench::VariantWorkbenchRow &a_row) {
  a_row.key = BuildRowKey(a_row.sourceKey, a_row.conditionId);
  a_row.equipped.key = a_row.key;
}

bool SerializeRowSource(const sosr::workbench::VariantWorkbenchRow &a_row,
                        nlohmann::json &a_serializedRow) {
  a_serializedRow["type"] = a_row.IsSlotRow() ? "slot" : "armor";
  if (a_row.IsSlotRow()) {
    const auto slotNumber =
        sosr::armor::GetArmorSlotNumber(a_row.equipped.slotMask);
    if (slotNumber == 0) {
      return false;
    }
    a_serializedRow["slot"] = slotNumber;
    return true;
  }

  a_serializedRow["equipped"] = sosr::armor::GetFormIdentifier(
      RE::TESForm::LookupByID(a_row.equipped.formID));
  return !a_serializedRow["equipped"].get_ref<const std::string &>().empty();
}

bool DeserializeRowSource(const nlohmann::json &a_serializedRow,
                          sosr::workbench::VariantWorkbenchRow &a_row) {
  const auto rowType = a_serializedRow.value("type", std::string{"armor"});
  if (rowType == "slot") {
    const auto slotMask =
        sosr::armor::GetArmorSlotMask(a_serializedRow.value("slot", 0));
    sosr::workbench::EquipmentWidgetItem slotItem{};
    if (slotMask == 0 || !sosr::workbench::BuildSlotItem(slotMask, slotItem)) {
      return false;
    }

    a_row.sourceKey = slotItem.key;
    a_row.equipped = std::move(slotItem);
    return true;
  }

  const auto *equippedForm = sosr::armor::LookupByIdentifier<RE::TESObjectARMO>(
      a_serializedRow.value("equipped", std::string{}));
  sosr::workbench::EquipmentWidgetItem equipped{};
  if (!equippedForm ||
      !sosr::workbench::BuildCatalogItem(equippedForm->GetFormID(), equipped)) {
    return false;
  }

  a_row.sourceKey =
      "armor:" + sosr::armor::FormatFormID(equippedForm->GetFormID());
  a_row.equipped = std::move(equipped);
  return true;
}
} // namespace

namespace sosr::workbench {
nlohmann::json VariantWorkbench::SerializeState() const {
  nlohmann::json root;
  root["rows"] = nlohmann::json::array();

  for (const auto &row : rows_) {
    if (row.overrides.empty() && !row.hideEquipped) {
      continue;
    }

    nlohmann::json serializedRow;
    if (!SerializeRowSource(row, serializedRow)) {
      continue;
    }
    if (row.conditionId.has_value()) {
      serializedRow["conditionId"] = *row.conditionId;
    } else {
      serializedRow["conditionId"] = nullptr;
    }
    serializedRow["hideEquipped"] = row.hideEquipped;
    serializedRow["overrides"] = nlohmann::json::array();
    for (const auto &overrideItem : row.overrides) {
      if (const auto *overrideForm =
              RE::TESForm::LookupByID(overrideItem.formID)) {
        serializedRow["overrides"].push_back(
            armor::GetFormIdentifier(overrideForm));
      }
    }

    root["rows"].push_back(std::move(serializedRow));
  }

  return root;
}

bool VariantWorkbench::DeserializeState(
    const nlohmann::json &a_root,
    const std::optional<std::string> &a_missingConditionId,
    std::string *a_error) {
  if (!a_root.is_object() || !a_root["rows"].is_array()) {
    if (a_error) {
      *a_error = "Workbench JSON is missing a rows array.";
    }
    return false;
  }

  Revert();

  for (const auto &serializedRow : a_root["rows"]) {
    if (!serializedRow.is_object()) {
      continue;
    }

    VariantWorkbenchRow row{};
    if (!DeserializeRowSource(serializedRow, row)) {
      continue;
    }
    if (const auto conditionIt = serializedRow.find("conditionId");
        conditionIt != serializedRow.end()) {
      if (conditionIt->is_null()) {
        row.conditionId = std::nullopt;
      } else if (conditionIt->is_string()) {
        const auto conditionId = conditionIt->get<std::string>();
        row.conditionId = conditionId.empty()
                              ? std::nullopt
                              : std::optional<std::string>(conditionId);
      } else {
        row.conditionId = a_missingConditionId;
      }
    } else {
      row.conditionId = a_missingConditionId;
    }
    UpdateRowIdentity(row);
    row.hideEquipped = serializedRow.value("hideEquipped", false);

    std::unordered_set<RE::FormID> seenOverrideForms;
    if (const auto &serializedOverrides = serializedRow["overrides"];
        serializedOverrides.is_array()) {
      for (const auto &overrideValue : serializedOverrides) {
        if (!overrideValue.is_string()) {
          continue;
        }

        const auto *overrideForm = armor::LookupByIdentifier<RE::TESObjectARMO>(
            overrideValue.get<std::string>());
        EquipmentWidgetItem overrideItem{};
        if (!overrideForm ||
            !sosr::workbench::BuildCatalogItem(overrideForm->GetFormID(),
                                               overrideItem) ||
            !seenOverrideForms.insert(overrideForm->GetFormID()).second) {
          continue;
        }

        row.overrides.push_back(std::move(overrideItem));
      }
    }

    rows_.push_back(std::move(row));
    rowOrder_.push_back(rows_.back().key);
  }

  MarkChanged();
  return true;
}

void VariantWorkbench::ReplaceState(VariantWorkbench &&a_source) {
  ClearPreview();
  rows_ = std::move(a_source.rows_);
  rowOrder_ = std::move(a_source.rowOrder_);
  MarkChanged();
}

void VariantWorkbench::Serialize(SKSE::SerializationInterface *a_skse) const {
  const auto root = SerializeState();
  const auto payload = root.dump();
  a_skse->WriteRecord(kSerializationType, kSerializationVersion, payload.data(),
                      static_cast<std::uint32_t>(payload.size()));
}

void VariantWorkbench::Deserialize(
    SKSE::SerializationInterface *a_skse,
    std::optional<std::string> a_missingConditionId) {
  Revert();

  std::uint32_t type = 0;
  std::uint32_t version = 0;
  std::uint32_t length = 0;
  if (!a_skse->GetNextRecordInfo(type, version, length)) {
    return;
  }

  if (type != kSerializationType) {
    return;
  }

  if (version != 2 && version != 3 && version != kSerializationVersion) {
    logger::warn("Skipping SOSR serialized rows from unsupported version {}",
                 version);
    return;
  }

  std::string payload(length, '\0');
  if (!a_skse->ReadRecordData(payload.data(), length)) {
    logger::error("Failed to read SOSR serialized workbench payload");
    return;
  }

  const auto root = nlohmann::json::parse(payload, nullptr, false, true);
  if (root.is_discarded() || !root.is_object() || !root["rows"].is_array()) {
    logger::error("Failed to parse SOSR serialized workbench payload");
    return;
  }

  if (version < 4) {
    auto migratedRoot = root;
    try {
      for (auto &serializedRow : migratedRoot["rows"]) {
        if (serializedRow.is_object()) {
          if (a_missingConditionId.has_value()) {
            serializedRow["conditionId"] = *a_missingConditionId;
          } else {
            serializedRow["conditionId"] = nullptr;
          }
        }
      }
      static_cast<void>(DeserializeState(migratedRoot, a_missingConditionId));
    } catch (const std::exception &exception) {
      logger::error("Failed to load SOSR serialized workbench payload: {}",
                    exception.what());
      Revert();
    }
    return;
  }

  try {
    static_cast<void>(DeserializeState(root, a_missingConditionId));
  } catch (const std::exception &exception) {
    logger::error("Failed to load SOSR serialized workbench payload: {}",
                  exception.what());
    Revert();
  }
}
} // namespace sosr::workbench
