#include "Menu.h"

#include "ConditionMaterializer.h"
#include "conditions/Defaults.h"
#include "conditions/Library.h"
#include "conditions/Store.h"
#include "ui/conditions/DraftValidation.h"

#include <algorithm>
#include <charconv>
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

bool HasCatalogConditions(
    const std::vector<sosr::conditions::Definition> &a_definitions) {
  return std::ranges::any_of(a_definitions, [](const auto &a_definition) {
    return a_definition.IsCatalog();
  });
}

void FinalizeConditionStore(sosr::conditions::Store &a_store) {
  if (!HasCatalogConditions(a_store.definitions)) {
    a_store.definitions.push_back(
        sosr::conditions::BuildDefaultPlayerCondition());
    a_store.nextConditionId = (std::max)(a_store.nextConditionId, 2);
  }

  sosr::conditions::RebuildConditionDependencyMetadata(a_store.definitions);
  sosr::conditions::InvalidateConditionMaterializationCaches(
      a_store.definitions);
}

bool DeserializeConditionStore(const nlohmann::json &a_root,
                               sosr::conditions::Store &a_store,
                               std::string *a_error) {
  if (!a_root.is_object()) {
    if (a_error) {
      *a_error = "Condition JSON is not an object.";
    }
    return false;
  }

  sosr::conditions::Store parsedStore;
  parsedStore.definitions = sosr::conditions::LoadConditionLibrary();
  parsedStore.nextConditionId =
      (std::max)(1, a_root.value("nextConditionId", 1));

  int maxConditionId = 0;
  if (const auto conditionsIt = a_root.find("conditions");
      conditionsIt != a_root.end() && conditionsIt->is_array()) {
    parsedStore.definitions.reserve(parsedStore.definitions.size() +
                                    conditionsIt->size());
    for (const auto &conditionJson : *conditionsIt) {
      if (!conditionJson.is_object()) {
        continue;
      }

      sosr::ui::conditions::Definition condition;
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

          sosr::ui::conditions::Clause clause;
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
        if (sosr::conditions::FindDefinitionByName(parsedStore.definitions,
                                                   condition.name) != nullptr ||
            RE::SCRIPT_FUNCTION::LocateScriptCommand(condition.name.c_str()) !=
                nullptr) {
          condition.name =
              sosr::ui::condition_editor::BuildUniqueConditionName(
                  condition.name, parsedStore.definitions, condition.id);
        }
        if (condition.id.rfind("condition-", 0) == 0) {
          const auto idSuffix = std::string_view(condition.id)
                                    .substr(std::size("condition-") - 1);
          int parsedConditionId = 0;
          const auto *begin = idSuffix.data();
          const auto *end = begin + idSuffix.size();
          const auto [ptr, error] =
              std::from_chars(begin, end, parsedConditionId);
          if (error == std::errc{} && ptr == end) {
            maxConditionId = (std::max)(maxConditionId, parsedConditionId);
          }
        }
        parsedStore.definitions.push_back(std::move(condition));
      }
    }
  }

  parsedStore.nextConditionId =
      (std::max)(parsedStore.nextConditionId, maxConditionId + 1);
  FinalizeConditionStore(parsedStore);
  a_store = std::move(parsedStore);
  return true;
}
} // namespace

namespace sosr {
nlohmann::json Menu::SerializeConditionState() const {
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

  return root;
}

bool Menu::DeserializeConditionState(const nlohmann::json &a_root,
                                     std::string *a_error) {
  conditions::Store parsedStore;
  if (!DeserializeConditionStore(a_root, parsedStore, a_error)) {
    return false;
  }

  parsedStore.revision = conditionStore_.revision;
  conditionStore_ = std::move(parsedStore);
  ConditionEditors().clear();
  BumpConditionStoreRevision();
  return true;
}

void Menu::SerializeConditions(SKSE::SerializationInterface *a_skse) const {
  const auto root = SerializeConditionState();
  const auto payload = root.dump();
  a_skse->WriteRecord(kConditionSerializationType,
                      kConditionSerializationVersion, payload.data(),
                      static_cast<std::uint32_t>(payload.size()));
}

void Menu::DeserializeConditions(SKSE::SerializationInterface *a_skse) {
  const auto resetToDefaults = [&]() {
    RevertConditions();
    EnsureDefaultConditions();
  };

  std::uint32_t type = 0;
  std::uint32_t version = 0;
  std::uint32_t length = 0;
  if (!a_skse->GetNextRecordInfo(type, version, length)) {
    resetToDefaults();
    return;
  }

  if (type != kConditionSerializationType) {
    resetToDefaults();
    return;
  }

  if (version != kConditionSerializationVersion) {
    logger::warn(
        "Skipping SOSR serialized conditions from unsupported version {}",
        version);
    resetToDefaults();
    return;
  }

  std::string payload(length, '\0');
  if (!a_skse->ReadRecordData(payload.data(), length)) {
    logger::error("Failed to read SOSR serialized condition payload");
    resetToDefaults();
    return;
  }

  const auto root = nlohmann::json::parse(payload, nullptr, false, true);
  if (root.is_discarded() || !root.is_object()) {
    logger::error("Failed to parse SOSR serialized condition payload");
    resetToDefaults();
    return;
  }

  try {
    if (!DeserializeConditionState(root)) {
      resetToDefaults();
    }
  } catch (const std::exception &exception) {
    logger::error("Failed to load SOSR serialized condition payload: {}",
                  exception.what());
    resetToDefaults();
  }
}

void Menu::RevertConditions() {
  ConditionDefinitions().clear();
  LoadConditionLibrary();
  ConditionEditors().clear();
  NextConditionId() = 1;
  BumpConditionStoreRevision();
}
} // namespace sosr
