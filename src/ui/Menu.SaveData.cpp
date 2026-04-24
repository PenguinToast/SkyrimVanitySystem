#include "Menu.h"

#include "Utf8Path.h"
#include "ui/ConditionData.h"

#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {
constexpr auto kSaveDataFormat = "SkyrimVanitySystem.SaveData";
constexpr int kSaveDataVersion = 1;

std::string FormatPathForStatus(const std::filesystem::path &a_path) {
  return sosr::utf8::PathToUtf8String(a_path);
}
} // namespace

namespace sosr {
bool Menu::ExportSaveDataJson(std::string_view a_path,
                              std::string &a_error) const {
  try {
    if (a_path.empty()) {
      a_error = "Choose a JSON file path.";
      return false;
    }

    const auto path = utf8::PathFromUtf8(a_path);
    if (const auto parentPath = path.parent_path(); !parentPath.empty()) {
      std::filesystem::create_directories(parentPath);
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
      a_error = "Failed to open JSON file for writing.";
      return false;
    }

    const nlohmann::json root{{"format", kSaveDataFormat},
                              {"version", kSaveDataVersion},
                              {"workbench", workbench_.SerializeState()},
                              {"conditions", SerializeConditionState()}};
    output << root.dump(2) << '\n';
    if (!output.good()) {
      a_error = "Failed to write JSON export.";
      return false;
    }

    logger::info("Exported Skyrim Vanity System save data to {}",
                 FormatPathForStatus(path));
    return true;
  } catch (const std::exception &exception) {
    a_error = std::format("Failed to export JSON: {}", exception.what());
    return false;
  }
}

bool Menu::ImportSaveDataJson(std::string_view a_path, std::string &a_error) {
  try {
    if (a_path.empty()) {
      a_error = "Choose a JSON file path.";
      return false;
    }

    const auto path = utf8::PathFromUtf8(a_path);
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
      a_error = "Failed to open JSON file for reading.";
      return false;
    }

    const auto root = nlohmann::json::parse(input, nullptr, false, true);
    if (root.is_discarded() || !root.is_object()) {
      a_error = "Failed to parse JSON import.";
      return false;
    }

    if (root.value("format", std::string{}) != kSaveDataFormat) {
      a_error = "JSON file is not a Skyrim Vanity System save data export.";
      return false;
    }

    if (root.value("version", 0) != kSaveDataVersion) {
      a_error = "JSON export version is not supported.";
      return false;
    }

    const auto workbenchIt = root.find("workbench");
    const auto conditionsIt = root.find("conditions");
    if (workbenchIt == root.end() || !workbenchIt->is_object() ||
        conditionsIt == root.end() || !conditionsIt->is_object()) {
      a_error = "JSON export is missing workbench or condition data.";
      return false;
    }

    workbench::VariantWorkbench importedWorkbench;
    if (!importedWorkbench.DeserializeState(
            *workbenchIt, std::string(ui::conditions::kDefaultConditionId),
            &a_error)) {
      return false;
    }

    if (!DeserializeConditionState(*conditionsIt, &a_error)) {
      return false;
    }

    workbench_.ReplaceState(std::move(importedWorkbench));
    workbench_.SyncDynamicArmorVariantsExtended(GetConditions());
    logger::info("Imported Skyrim Vanity System save data from {}",
                 FormatPathForStatus(path));
    return true;
  } catch (const std::exception &exception) {
    a_error = std::format("Failed to import JSON: {}", exception.what());
    return false;
  }
}

void Menu::ResetSaveDataPath() {
  auto path = std::filesystem::path(settingsDirectory_) / "save-data.json";
  path.make_preferred();
  const auto pathText = utf8::PathToUtf8String(path);
  std::snprintf(saveDataPath_.data(), saveDataPath_.size(), "%s",
                pathText.c_str());
}
} // namespace sosr
