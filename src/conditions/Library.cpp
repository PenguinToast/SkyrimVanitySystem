#include "conditions/Library.h"

#include "conditions/Validation.h"
#include "ui/Localization.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <format>
#include <nlohmann/json.hpp>
#include <unordered_set>

namespace {
constexpr auto kConditionLibraryDirectory =
    "Data/SKSE/Plugins/SkyrimVanitySystem/ConditionLibrary";

std::filesystem::path BuildLibraryDirectory() {
  return {kConditionLibraryDirectory};
}

std::filesystem::path BuildLibraryPath(std::string_view a_name) {
  return BuildLibraryDirectory() / (std::string(a_name) + ".json");
}

nlohmann::json SerializeComparator(const sosr::conditions::Comparator a_value) {
  using Comparator = sosr::conditions::Comparator;
  switch (a_value) {
  case Comparator::Equal:
    return "==";
  case Comparator::NotEqual:
    return "!=";
  case Comparator::Greater:
    return ">";
  case Comparator::GreaterOrEqual:
    return ">=";
  case Comparator::Less:
    return "<";
  case Comparator::LessOrEqual:
    return "<=";
  }
  return "==";
}

sosr::conditions::Comparator ParseComparator(std::string_view a_value) {
  using Comparator = sosr::conditions::Comparator;
  if (a_value == "!=") {
    return Comparator::NotEqual;
  }
  if (a_value == ">") {
    return Comparator::Greater;
  }
  if (a_value == ">=") {
    return Comparator::GreaterOrEqual;
  }
  if (a_value == "<") {
    return Comparator::Less;
  }
  if (a_value == "<=") {
    return Comparator::LessOrEqual;
  }
  return Comparator::Equal;
}

nlohmann::json SerializeConnective(const sosr::conditions::Connective a_value) {
  return a_value == sosr::conditions::Connective::Or ? "OR" : "AND";
}

sosr::conditions::Connective ParseConnective(std::string_view a_value) {
  return a_value == "OR" ? sosr::conditions::Connective::Or
                         : sosr::conditions::Connective::And;
}

nlohmann::json
SerializeDefinition(const sosr::conditions::Definition &a_definition) {
  nlohmann::json root{{"description", a_definition.description},
                      {"clauses", nlohmann::json::array()}};
  for (const auto &clause : a_definition.clauses) {
    root["clauses"].push_back(
        {{"function", clause.customConditionId.empty() ? clause.functionName
                                                       : std::string{}},
         {"customConditionId", clause.customConditionId},
         {"arg1", clause.arguments[0]},
         {"arg2", clause.arguments[1]},
         {"comparator", SerializeComparator(clause.comparator)},
         {"value", clause.comparand},
         {"join", SerializeConnective(clause.connectiveToNext)}});
  }
  return root;
}

bool WriteLibraryJson(const std::filesystem::path &a_path,
                      const nlohmann::json &a_root, std::string &a_error) {
  const auto *localization = sosr::ui::Localization::GetSingleton();
  std::ofstream output(a_path);
  if (!output.is_open()) {
    a_error = std::string(
        localization->Get("conditions.library.error.open_write"));
    return false;
  }
  output << a_root.dump(2);
  output.close();
  return true;
}

bool PersistLibraryDefinitions(
    std::vector<sosr::conditions::Definition> &a_definitions,
    std::string &a_error) {
  const auto *localization = sosr::ui::Localization::GetSingleton();
  a_error.clear();
  const auto directory = BuildLibraryDirectory();
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) {
    auto message = error.message();
    a_error = std::vformat(std::string(localization->Get(
                               "conditions.library.error.create_directory")),
                           std::make_format_args(message));
    return false;
  }

  const auto tempDirectory = directory / ".tmp-write";
  std::filesystem::remove_all(tempDirectory, error);
  error.clear();
  std::filesystem::create_directories(tempDirectory, error);
  if (error) {
    auto message = error.message();
    a_error = std::vformat(
        std::string(
            localization->Get("conditions.library.error.create_temp_directory")),
        std::make_format_args(message));
    return false;
  }

  std::unordered_set<std::string> desiredFileNames;
  for (auto &definition : a_definitions) {
    if (!definition.IsLibrary()) {
      continue;
    }
    auto &library = definition.EnsureLibrary();
    const auto finalPath = BuildLibraryPath(definition.name);
    library.storagePath = finalPath.string();
    desiredFileNames.insert(finalPath.filename().string());
    if (!WriteLibraryJson(tempDirectory / finalPath.filename(),
                          SerializeDefinition(definition), a_error)) {
      std::filesystem::remove_all(tempDirectory, error);
      return false;
    }
  }

  std::vector<std::filesystem::path> stalePaths;
  for (const auto &entry :
       std::filesystem::directory_iterator(directory, error)) {
    if (error) {
      auto message = error.message();
      a_error = std::vformat(
          std::string(localization->Get(
              "conditions.library.error.enumerate_directory")),
          std::make_format_args(message));
      std::filesystem::remove_all(tempDirectory, error);
      return false;
    }
    if (!entry.is_regular_file() || entry.path().extension() != ".json") {
      continue;
    }
    if (desiredFileNames.contains(entry.path().filename().string())) {
      continue;
    }
    stalePaths.push_back(entry.path());
  }

  for (const auto &fileName : desiredFileNames) {
    const auto stagedPath = tempDirectory / fileName;
    const auto finalPath = directory / fileName;
    std::filesystem::copy_file(
        stagedPath, finalPath,
        std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
      auto message = error.message();
      a_error = std::vformat(
          std::string(
              localization->Get("conditions.library.error.commit_file")),
          std::make_format_args(message));
      std::filesystem::remove_all(tempDirectory, error);
      return false;
    }
  }

  for (const auto &stalePath : stalePaths) {
    std::filesystem::remove(stalePath, error);
    if (error) {
      auto message = error.message();
      a_error = std::vformat(
          std::string(
              localization->Get("conditions.library.error.remove_stale")),
          std::make_format_args(message));
      std::filesystem::remove_all(tempDirectory, error);
      return false;
    }
  }

  std::filesystem::remove_all(tempDirectory, error);
  error.clear();
  return true;
}
} // namespace

namespace sosr::conditions {
std::vector<Definition> LoadConditionLibrary() {
  std::vector<Definition> definitions;
  std::error_code error;
  const auto directory = BuildLibraryDirectory();
  if (!std::filesystem::exists(directory, error) ||
      !std::filesystem::is_directory(directory, error)) {
    return definitions;
  }

  for (const auto &entry :
       std::filesystem::directory_iterator(directory, error)) {
    if (error || !entry.is_regular_file() ||
        entry.path().extension() != ".json") {
      continue;
    }

    std::ifstream input(entry.path());
    if (!input.is_open()) {
      continue;
    }

    const auto root = nlohmann::json::parse(input, nullptr, false, true);
    if (root.is_discarded() || !root.is_object()) {
      continue;
    }

    Definition definition;
    definition.name = entry.path().stem().string();
    definition.description = root.value("description", std::string{});
    definition.id = definition.name;
    definition.EnsureLibrary().storagePath = entry.path().string();

    if (const auto clausesIt = root.find("clauses");
        clausesIt != root.end() && clausesIt->is_array()) {
      definition.clauses.reserve(clausesIt->size());
      for (const auto &clauseJson : *clausesIt) {
        if (!clauseJson.is_object()) {
          continue;
        }
        Clause clause;
        clause.customConditionId =
            clauseJson.value("customConditionId", std::string{});
        clause.functionName = clauseJson.value("function", std::string{});
        clause.arguments[0] = clauseJson.value("arg1", std::string{});
        clause.arguments[1] = clauseJson.value("arg2", std::string{});
        clause.comparator =
            ParseComparator(clauseJson.value("comparator", std::string{"=="}));
        clause.comparand = clauseJson.value("value", std::string{"1"});
        clause.connectiveToNext =
            ParseConnective(clauseJson.value("join", std::string{"AND"}));
        definition.clauses.push_back(std::move(clause));
      }
    }

    if (!definition.name.empty() && !definition.clauses.empty()) {
      definitions.push_back(std::move(definition));
    }
  }

  return definitions;
}

bool CommitLibraryConditionEdit(const std::vector<Definition> &a_definitions,
                                std::string_view a_sourceConditionId,
                                const Definition &a_draft,
                                LibraryChangeResult &a_result,
                                std::string &a_error) {
  const auto *localization = sosr::ui::Localization::GetSingleton();
  a_result = {};
  a_error.clear();

  auto updatedDefinitions = a_definitions;
  auto updatedDraft = a_draft;
  updatedDraft.EnsureLibrary();
  updatedDraft.id = updatedDraft.name;

  const auto existingIt = std::ranges::find(
      updatedDefinitions, a_sourceConditionId, &Definition::id);
  if (a_sourceConditionId.empty()) {
    updatedDefinitions.push_back(updatedDraft);
  } else {
    if (existingIt == updatedDefinitions.end()) {
      a_error = std::string(
          localization->Get("conditions.validation.no_longer_exists"));
      return false;
    }
    const auto previousId = existingIt->id;
    *existingIt = updatedDraft;
    if (previousId != updatedDraft.id) {
      RenameConditionReferences(updatedDefinitions, previousId,
                                updatedDraft.id);
      a_result.renamedIds.emplace_back(previousId, updatedDraft.id);
    }
  }

  if (!PersistLibraryDefinitions(updatedDefinitions, a_error)) {
    return false;
  }

  a_result.definitions = std::move(updatedDefinitions);
  return true;
}

bool CommitLibraryConditionDelete(const std::vector<Definition> &a_definitions,
                                  std::string_view a_conditionId,
                                  LibraryChangeResult &a_result,
                                  std::string &a_error) {
  const auto *localization = sosr::ui::Localization::GetSingleton();
  a_result = {};
  a_error.clear();

  auto updatedDefinitions = a_definitions;
  const auto it =
      std::ranges::find(updatedDefinitions, a_conditionId, &Definition::id);
  if (it == updatedDefinitions.end() || !it->IsLibrary()) {
    a_error = std::string(
        localization->Get("conditions.library.error.no_longer_exists"));
    return false;
  }
  updatedDefinitions.erase(it);

  if (!PersistLibraryDefinitions(updatedDefinitions, a_error)) {
    return false;
  }

  a_result.definitions = std::move(updatedDefinitions);
  return true;
}

bool IsLibraryFileNameValid(const std::string_view a_name) {
  if (a_name.empty()) {
    return false;
  }
  return a_name.find_first_of("\\/:*?\"<>|") == std::string_view::npos;
}
} // namespace sosr::conditions
