#include "Menu.h"

#include "ConditionMaterializer.h"
#include "conditions/Library.h"
#include "ui/conditions/DraftValidation.h"

#include <algorithm>
#include <nlohmann/json.hpp>

namespace {
constexpr std::uint32_t kConditionSerializationType = 'COND';
constexpr std::uint32_t kConditionSerializationVersion = 1;

using ConditionComparator = sosr::ui::conditions::Comparator;
using ConditionColor = sosr::ui::conditions::Color;
using ConditionConnective = sosr::ui::conditions::Connective;

std::string SerializeConditionComparator(const ConditionComparator a_value) {
  switch (a_value) {
  case ConditionComparator::Equal:
    return "==";
  case ConditionComparator::NotEqual:
    return "!=";
  case ConditionComparator::Greater:
    return ">";
  case ConditionComparator::GreaterOrEqual:
    return ">=";
  case ConditionComparator::Less:
    return "<";
  case ConditionComparator::LessOrEqual:
    return "<=";
  }

  return "==";
}

ConditionComparator ParseConditionComparator(const std::string_view a_value) {
  if (a_value == "!=") {
    return ConditionComparator::NotEqual;
  }
  if (a_value == ">") {
    return ConditionComparator::Greater;
  }
  if (a_value == ">=") {
    return ConditionComparator::GreaterOrEqual;
  }
  if (a_value == "<") {
    return ConditionComparator::Less;
  }
  if (a_value == "<=") {
    return ConditionComparator::LessOrEqual;
  }
  return ConditionComparator::Equal;
}

std::string SerializeConditionConnective(const ConditionConnective a_value) {
  return a_value == ConditionConnective::Or ? "OR" : "AND";
}

ConditionConnective ParseConditionConnective(const std::string_view a_value) {
  return a_value == "OR" ? ConditionConnective::Or : ConditionConnective::And;
}

ConditionColor ParseConditionColor(const nlohmann::json &a_value,
                                   const ConditionColor &a_fallback) {
  if (!a_value.is_array() || a_value.size() < 3) {
    return a_fallback;
  }

  ConditionColor color = a_fallback;
  color.x = a_value[0].is_number() ? a_value[0].get<float>() : color.x;
  color.y = a_value[1].is_number() ? a_value[1].get<float>() : color.y;
  color.z = a_value[2].is_number() ? a_value[2].get<float>() : color.z;
  color.w = a_value.size() > 3 && a_value[3].is_number()
                ? a_value[3].get<float>()
                : color.w;
  return color;
}

nlohmann::json SerializeConditionColor(const ConditionColor &a_color) {
  return nlohmann::json::array({a_color.x, a_color.y, a_color.z, a_color.w});
}
} // namespace

namespace sosr {
void Menu::SerializeConditions(SKSE::SerializationInterface *a_skse) const {
  nlohmann::json root{{"nextConditionId", NextConditionId()},
                      {"conditions", nlohmann::json::array()}};

  for (const auto &condition : ConditionDefinitions()) {
    if (!condition.IsCatalog()) {
      continue;
    }
    const auto *catalog = condition.GetCatalog();
    if (catalog == nullptr) {
      continue;
    }
    nlohmann::json conditionJson{
        {"id", condition.id},
        {"name", condition.name},
        {"description", condition.description},
        {"color", SerializeConditionColor(catalog->color)},
        {"enabled", catalog->enabled},
        {"clauses", nlohmann::json::array()}};
    for (const auto &clause : condition.clauses) {
      conditionJson["clauses"].push_back(
          {{"function", clause.customConditionId.empty() ? clause.functionName
                                                         : std::string{}},
           {"customConditionId", clause.customConditionId},
           {"arg1", clause.arguments[0]},
           {"arg2", clause.arguments[1]},
           {"comparator", SerializeConditionComparator(clause.comparator)},
           {"value", clause.comparand},
           {"join", SerializeConditionConnective(clause.connectiveToNext)}});
    }
    root["conditions"].push_back(std::move(conditionJson));
  }

  const auto payload = root.dump();
  a_skse->WriteRecord(kConditionSerializationType,
                      kConditionSerializationVersion, payload.data(),
                      static_cast<std::uint32_t>(payload.size()));
}

void Menu::DeserializeConditions(SKSE::SerializationInterface *a_skse) {
  RevertConditions();

  std::uint32_t type = 0;
  std::uint32_t version = 0;
  std::uint32_t length = 0;
  if (!a_skse->GetNextRecordInfo(type, version, length)) {
    EnsureDefaultConditions();
    return;
  }

  if (type != kConditionSerializationType) {
    EnsureDefaultConditions();
    return;
  }

  if (version != kConditionSerializationVersion) {
    logger::warn(
        "Skipping SOSR serialized conditions from unsupported version {}",
        version);
    EnsureDefaultConditions();
    return;
  }

  std::string payload(length, '\0');
  if (!a_skse->ReadRecordData(payload.data(), length)) {
    logger::error("Failed to read SOSR serialized condition payload");
    EnsureDefaultConditions();
    return;
  }

  const auto root = nlohmann::json::parse(payload, nullptr, false, true);
  if (root.is_discarded() || !root.is_object()) {
    logger::error("Failed to parse SOSR serialized condition payload");
    EnsureDefaultConditions();
    return;
  }

  NextConditionId() = (std::max)(1, root.value("nextConditionId", 1));

  int maxConditionId = 0;
  if (const auto conditionsIt = root.find("conditions");
      conditionsIt != root.end() && conditionsIt->is_array()) {
    ConditionDefinitions().reserve(ConditionDefinitions().size() +
                                   conditionsIt->size());
    for (const auto &conditionJson : *conditionsIt) {
      if (!conditionJson.is_object()) {
        continue;
      }

      ui::conditions::Definition condition;
      condition.id = conditionJson.value("id", std::string{});
      condition.name = conditionJson.value("name", std::string{});
      condition.description = conditionJson.value("description", std::string{});
      auto &catalog = condition.EnsureCatalog();
      catalog.enabled = conditionJson.value("enabled", true);
      catalog.color = ParseConditionColor(
          conditionJson.value("color", nlohmann::json{}), catalog.color);

      if (const auto clausesIt = conditionJson.find("clauses");
          clausesIt != conditionJson.end() && clausesIt->is_array()) {
        condition.clauses.reserve(clausesIt->size());
        for (const auto &clauseJson : *clausesIt) {
          if (!clauseJson.is_object()) {
            continue;
          }

          ui::conditions::Clause clause;
          clause.customConditionId =
              clauseJson.value("customConditionId", std::string{});
          clause.functionName = clauseJson.value("function", std::string{});
          clause.arguments[0] = clauseJson.value("arg1", std::string{});
          clause.arguments[1] = clauseJson.value("arg2", std::string{});
          clause.comparator = ParseConditionComparator(
              clauseJson.value("comparator", std::string{"=="}));
          clause.comparand = clauseJson.value("value", std::string{"1"});
          clause.connectiveToNext = ParseConditionConnective(
              clauseJson.value("join", std::string{"AND"}));
          condition.clauses.push_back(std::move(clause));
        }
      }

      if (!condition.id.empty() && !condition.name.empty() &&
          !condition.clauses.empty()) {
        if (conditions::FindDefinitionByName(ConditionDefinitions(),
                                             condition.name) != nullptr ||
            RE::SCRIPT_FUNCTION::LocateScriptCommand(condition.name.c_str()) !=
                nullptr) {
          condition.name = ui::condition_editor::BuildUniqueConditionName(
              condition.name, ConditionDefinitions(), condition.id);
        }
        if (condition.id.rfind("condition-", 0) == 0) {
          try {
            maxConditionId = (std::max)(
                maxConditionId,
                std::stoi(condition.id.substr(std::size("condition-") - 1)));
          } catch (const std::exception &) {
          }
        }
        ConditionDefinitions().push_back(std::move(condition));
      }
    }
  }

  NextConditionId() = (std::max)(NextConditionId(), maxConditionId + 1);
  EnsureDefaultConditions();
  BumpConditionStoreRevision();
  sosr::conditions::RebuildConditionDependencyMetadata(ConditionDefinitions());
  sosr::conditions::InvalidateConditionMaterializationCaches(
      ConditionDefinitions());
}

void Menu::RevertConditions() {
  ConditionDefinitions().clear();
  LoadConditionLibrary();
  ConditionEditors().clear();
  NextConditionId() = 1;
  BumpConditionStoreRevision();
}
} // namespace sosr
