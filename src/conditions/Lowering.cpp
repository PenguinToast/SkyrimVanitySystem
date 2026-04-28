#include "conditions/Lowering.h"

#include "ArmorUtils.h"
#include "RE/A/ActorValueList.h"
#include "StringUtils.h"
#include "conditions/ParamEnumOptions.h"
#include "conditions/Validation.h"

#include <RE/M/MagicItem.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESFurniture.h>
#include <RE/T/TESWorldSpace.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <exception>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
using Clause = sosr::conditions::Clause;
using Comparator = sosr::conditions::Comparator;
using Connective = sosr::conditions::Connective;
using Definition = sosr::conditions::Definition;
using ParamType = RE::SCRIPT_PARAM_TYPE;

struct NativeLiteral {
  const RE::SCRIPT_FUNCTION *command{nullptr};
  std::string functionName;
  std::array<std::string, 2> arguments{};
  std::array<ParamType, 2> parameterTypes{ParamType::kForm, ParamType::kForm};
  std::uint16_t parameterCount{0};
  Comparator comparator{Comparator::Equal};
  std::string comparand{"1"};
};

using OrClause = std::vector<NativeLiteral>;
using ConditionCnf = std::vector<OrClause>;
using ConditionVisitSet = std::unordered_set<std::string>;

union ConditionParam {
  char c;
  std::int32_t i;
  float f;
  RE::TESForm *form;
};

ParamType ResolveEditorParamType(const std::string_view a_functionName,
                                 const std::uint16_t a_paramIndex,
                                 const ParamType a_type) {
  // SVS conditions are evaluated against actors, so constrain GetIsID to actor
  // bases even if the runtime command table exposes the parameter more broadly.
  if (a_paramIndex == 0 &&
      sosr::strings::EqualsInsensitive(a_functionName, "GetIsID")) {
    return ParamType::kActorBase;
  }

  return a_type;
}

std::string Uppercase(std::string_view a_text) {
  std::string result(a_text);
  std::ranges::transform(result, result.begin(),
                         [](const unsigned char a_char) {
                           return static_cast<char>(std::toupper(a_char));
                         });
  return result;
}

Comparator InvertComparator(const Comparator a_comparator) {
  switch (a_comparator) {
  case Comparator::Equal:
    return Comparator::NotEqual;
  case Comparator::NotEqual:
    return Comparator::Equal;
  case Comparator::Greater:
    return Comparator::LessOrEqual;
  case Comparator::GreaterOrEqual:
    return Comparator::Less;
  case Comparator::Less:
    return Comparator::GreaterOrEqual;
  case Comparator::LessOrEqual:
    return Comparator::Greater;
  }

  return Comparator::NotEqual;
}

std::string ComparatorToken(const Comparator a_comparator) {
  switch (a_comparator) {
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

RE::CONDITION_ITEM_DATA::OpCode ToOpCode(const Comparator a_comparator) {
  switch (a_comparator) {
  case Comparator::Equal:
    return RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
  case Comparator::NotEqual:
    return RE::CONDITION_ITEM_DATA::OpCode::kNotEqualTo;
  case Comparator::Greater:
    return RE::CONDITION_ITEM_DATA::OpCode::kGreaterThan;
  case Comparator::GreaterOrEqual:
    return RE::CONDITION_ITEM_DATA::OpCode::kGreaterThanOrEqualTo;
  case Comparator::Less:
    return RE::CONDITION_ITEM_DATA::OpCode::kLessThan;
  case Comparator::LessOrEqual:
    return RE::CONDITION_ITEM_DATA::OpCode::kLessThanOrEqualTo;
  }

  return RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
}

bool IsIntegerParamType(const ParamType a_type) {
  switch (a_type) {
  case ParamType::kChar:
  case ParamType::kInt:
  case ParamType::kStage:
  case ParamType::kRelationshipRank:
  case ParamType::kCrimeType:
  case ParamType::kFormType:
  case ParamType::kAlignment:
  case ParamType::kEquipType:
  case ParamType::kCritStage:
  case ParamType::kWardState:
  case ParamType::kFurnitureAnimType:
  case ParamType::kFurnitureEntryType:
  case ParamType::kSkillAction:
    return true;
  default:
    return false;
  }
}

bool IsValueParamType(const ParamType a_type) {
  return IsIntegerParamType(a_type) || a_type == ParamType::kFloat ||
         a_type == ParamType::kActorValue || a_type == ParamType::kAxis ||
         a_type == ParamType::kSex || a_type == ParamType::kCastingSource ||
         a_type == ParamType::kMiscStat;
}

const RE::SCRIPT_FUNCTION *
FindConditionFunction(const std::string_view a_name) {
  const auto trimmed = sosr::strings::TrimText(a_name);
  if (trimmed.empty()) {
    return nullptr;
  }

  const auto *command =
      RE::SCRIPT_FUNCTION::LocateScriptCommand(trimmed.c_str());
  if (!command || !command->conditionFunction) {
    return nullptr;
  }

  return command;
}

template <class T> T *LookupTypedFormByToken(const std::string &a_token) {
  if (auto *form = RE::TESForm::LookupByEditorID(a_token)) {
    if (auto *typed = form->As<T>()) {
      return typed;
    }
  }

  auto *dataHandler = RE::TESDataHandler::GetSingleton();
  if (!dataHandler) {
    return nullptr;
  }

  for (auto *form : dataHandler->GetFormArray<T>()) {
    if (sosr::armor::GetEditorID(form) == a_token) {
      return form;
    }
  }

  return nullptr;
}

template <class T>
RE::TESForm *LookupAssignableFormByToken(const std::string &a_token) {
  if (auto *form = RE::TESForm::LookupByEditorID(a_token)) {
    if (form->As<T>()) {
      return form;
    }
  }

  auto *dataHandler = RE::TESDataHandler::GetSingleton();
  if (!dataHandler) {
    return nullptr;
  }

  for (const auto &forms : dataHandler->formArrays) {
    for (auto *form : forms) {
      if (form && form->As<T>() && sosr::armor::GetEditorID(form) == a_token) {
        return form;
      }
    }
  }

  return nullptr;
}

template <class... T>
RE::TESForm *LookupAnyAssignableFormByToken(const std::string &a_token) {
  RE::TESForm *result = nullptr;
  ((result = result ? result : LookupAssignableFormByToken<T>(a_token)), ...);
  return result;
}

RE::TESObjectREFR *LookupReferenceByToken(const std::string &a_token) {
  if (sosr::strings::EqualsInsensitive(a_token, "Player")) {
    return RE::PlayerCharacter::GetSingleton();
  }

  if (auto *ref = sosr::armor::LookupByIdentifier<RE::TESObjectREFR>(a_token)) {
    return ref;
  }

  if (auto *form = RE::TESForm::LookupByEditorID(a_token)) {
    if (auto *ref = form->As<RE::TESObjectREFR>()) {
      return ref;
    }
  }

  const auto trimmed = sosr::strings::TrimText(a_token);
  if (!trimmed.empty() &&
      std::ranges::all_of(trimmed, [](const unsigned char a_char) {
        return std::isxdigit(a_char) != 0;
      })) {
    RE::FormID formID = 0;
    const auto *begin = trimmed.data();
    const auto *end = begin + trimmed.size();
    if (const auto [ptr, ec] = std::from_chars(begin, end, formID, 16);
        ec == std::errc{} && ptr == end) {
      return RE::TESForm::LookupByID<RE::TESObjectREFR>(formID);
    }
  }

  return nullptr;
}

RE::TESForm *LookupGenericFormByToken(const std::string &a_token) {
  if (auto *form = RE::TESForm::LookupByEditorID(a_token)) {
    return form;
  }

  auto *dataHandler = RE::TESDataHandler::GetSingleton();
  if (!dataHandler) {
    return nullptr;
  }

  for (const auto &forms : dataHandler->formArrays) {
    for (auto *form : forms) {
      if (form && sosr::armor::GetEditorID(form) == a_token) {
        return form;
      }
    }
  }

  return nullptr;
}

std::optional<std::int32_t> TryParseInt(std::string_view a_text) {
  const auto trimmed = sosr::strings::TrimText(a_text);
  if (trimmed.empty()) {
    return std::nullopt;
  }

  std::int32_t value = 0;
  const auto *begin = trimmed.data();
  const auto *end = begin + trimmed.size();
  const auto [ptr, error] = std::from_chars(begin, end, value);
  if (error == std::errc{} && ptr == end) {
    return value;
  }
  return std::nullopt;
}

std::optional<float> TryParseFloat(std::string_view a_text) {
  const auto trimmed = sosr::strings::TrimText(a_text);
  if (trimmed.empty()) {
    return std::nullopt;
  }

  try {
    std::size_t parsed = 0;
    const auto value = std::stof(trimmed, std::addressof(parsed));
    if (parsed == trimmed.size()) {
      return value;
    }
  } catch (const std::exception &) {
  }
  return std::nullopt;
}

std::optional<ConditionParam> ParseParam(const std::string &a_text,
                                         const ParamType a_type) {
  ConditionParam param{};
  const auto trimmed = sosr::strings::TrimText(a_text);

  switch (a_type) {
  case ParamType::kChar:
  case ParamType::kInt:
  case ParamType::kStage:
  case ParamType::kRelationshipRank:
  case ParamType::kCrimeType:
  case ParamType::kAlignment:
  case ParamType::kEquipType:
  case ParamType::kSkillAction:
    if (const auto value = TryParseInt(trimmed)) {
      param.i = *value;
    } else {
      return std::nullopt;
    }
    break;
  case ParamType::kFormType:
  case ParamType::kCritStage:
  case ParamType::kWardState:
  case ParamType::kFurnitureAnimType:
  case ParamType::kFurnitureEntryType:
    if (const auto value =
            sosr::conditions::ParseParamEnumOption(a_type, trimmed)) {
      param.i = *value;
    } else if (const auto integer = TryParseInt(trimmed)) {
      param.i = *integer;
    } else {
      return std::nullopt;
    }
    break;
  case ParamType::kFloat:
    if (const auto value = TryParseFloat(trimmed)) {
      param.f = *value;
    } else {
      return std::nullopt;
    }
    break;
  case ParamType::kActorValue: {
    auto actorValue =
        RE::ActorValueList::LookupActorValueByName(trimmed.c_str());
    if (actorValue == RE::ActorValue::kNone) {
      actorValue = RE::ActorValueList::LookupActorValueByName(
          Uppercase(trimmed).c_str());
    }
    param.i = static_cast<std::int32_t>(std::to_underlying(actorValue));
    break;
  }
  case ParamType::kAxis:
    param.i = sosr::strings::EqualsInsensitive(trimmed, "X")
                  ? 0
                  : (sosr::strings::EqualsInsensitive(trimmed, "Y") ? 1 : 2);
    break;
  case ParamType::kSex:
    if (const auto value =
            sosr::conditions::ParseParamEnumOption(a_type, trimmed)) {
      param.i = *value;
    } else {
      return std::nullopt;
    }
    break;
  case ParamType::kCastingSource:
    if (const auto value =
            sosr::conditions::ParseParamEnumOption(a_type, trimmed)) {
      param.i = *value;
    } else {
      return std::nullopt;
    }
    break;
  case ParamType::kMiscStat:
    if (const auto value =
            sosr::conditions::ParseParamTextOption(a_type, trimmed)) {
      param.i = *value;
    } else {
      return std::nullopt;
    }
    break;
  case ParamType::kObjectRef:
    param.form = LookupReferenceByToken(trimmed);
    break;
  case ParamType::kActor:
  case ParamType::kActorBase:
  case ParamType::kNPC:
    param.form = LookupTypedFormByToken<RE::TESNPC>(trimmed);
    break;
  case ParamType::kRace:
    param.form = LookupTypedFormByToken<RE::TESRace>(trimmed);
    break;
  case ParamType::kClass:
    param.form = LookupTypedFormByToken<RE::TESClass>(trimmed);
    break;
  case ParamType::kFaction:
    param.form = LookupTypedFormByToken<RE::TESFaction>(trimmed);
    break;
  case ParamType::kGlobal:
    param.form = LookupTypedFormByToken<RE::TESGlobal>(trimmed);
    break;
  case ParamType::kQuest:
    param.form = LookupTypedFormByToken<RE::TESQuest>(trimmed);
    break;
  case ParamType::kKeyword:
    param.form = LookupTypedFormByToken<RE::BGSKeyword>(trimmed);
    break;
  case ParamType::kPerk:
    param.form = LookupTypedFormByToken<RE::BGSPerk>(trimmed);
    break;
  case ParamType::kVoiceType:
    param.form = LookupTypedFormByToken<RE::BGSVoiceType>(trimmed);
    break;
  case ParamType::kCell:
    param.form = LookupTypedFormByToken<RE::TESObjectCELL>(trimmed);
    break;
  case ParamType::kLocation:
    param.form = LookupTypedFormByToken<RE::BGSLocation>(trimmed);
    break;
  case ParamType::kWeather:
    param.form = LookupTypedFormByToken<RE::TESWeather>(trimmed);
    break;
  case ParamType::kShout:
    param.form = LookupTypedFormByToken<RE::TESShout>(trimmed);
    break;
  case ParamType::kWordOfPower:
    param.form = LookupTypedFormByToken<RE::TESWordOfPower>(trimmed);
    break;
  case ParamType::kFormList:
    param.form = LookupTypedFormByToken<RE::BGSListForm>(trimmed);
    break;
  case ParamType::kSpellItem:
    param.form = LookupTypedFormByToken<RE::SpellItem>(trimmed);
    break;
  case ParamType::kRegion:
    param.form = LookupTypedFormByToken<RE::TESRegion>(trimmed);
    break;
  case ParamType::kPackage:
    param.form = LookupTypedFormByToken<RE::TESPackage>(trimmed);
    break;
  case ParamType::kMagicEffect:
    param.form = LookupTypedFormByToken<RE::EffectSetting>(trimmed);
    break;
  case ParamType::kBGSScene:
    param.form = LookupTypedFormByToken<RE::BGSScene>(trimmed);
    break;
  case ParamType::kAssociationType:
    param.form = LookupTypedFormByToken<RE::BGSAssociationType>(trimmed);
    break;
  case ParamType::kNote:
    param.form = LookupTypedFormByToken<RE::BGSNote>(trimmed);
    break;
  case ParamType::kEncounterZone:
    param.form = LookupTypedFormByToken<RE::BGSEncounterZone>(trimmed);
    break;
  case ParamType::kIdleForm:
    param.form = LookupTypedFormByToken<RE::TESIdleForm>(trimmed);
    break;
  case ParamType::kRefType:
    param.form = LookupTypedFormByToken<RE::BGSLocationRefType>(trimmed);
    break;
  case ParamType::kMagicItem:
    param.form = LookupAssignableFormByToken<RE::MagicItem>(trimmed);
    break;
  case ParamType::kObject:
  case ParamType::kInventoryObject:
    param.form = LookupAssignableFormByToken<RE::TESBoundObject>(trimmed);
    break;
  case ParamType::kFurnitureOrFormList:
    param.form = LookupAnyAssignableFormByToken<RE::TESFurniture,
                                                RE::BGSListForm>(trimmed);
    break;
  case ParamType::kOwner:
    param.form =
        LookupAnyAssignableFormByToken<RE::TESNPC, RE::TESFaction>(trimmed);
    break;
  case ParamType::kInvObjectOrFormList:
  case ParamType::kObjectOrFormList:
    param.form = LookupAnyAssignableFormByToken<RE::TESBoundObject,
                                                RE::BGSListForm>(trimmed);
    break;
  case ParamType::kWorldOrList:
    param.form = LookupAnyAssignableFormByToken<RE::TESWorldSpace,
                                                RE::BGSListForm>(trimmed);
    break;
  case ParamType::kKnowableForm:
  case ParamType::kForm:
  default:
    param.form = LookupGenericFormByToken(trimmed);
    break;
  }

  return param;
}

std::string BuildLiteralSignature(const NativeLiteral &a_literal) {
  std::string signature = a_literal.functionName;
  for (std::uint16_t paramIndex = 0; paramIndex < a_literal.parameterCount &&
                                     paramIndex < a_literal.arguments.size();
       ++paramIndex) {
    signature.push_back('(');
    signature.append(a_literal.arguments[paramIndex]);
    signature.push_back(')');
  }
  signature.push_back(' ');
  signature.append(ComparatorToken(a_literal.comparator));
  signature.push_back(' ');
  signature.append(a_literal.comparand);
  return signature;
}

std::string BuildLiteralDisplay(const NativeLiteral &a_literal) {
  std::string display = a_literal.functionName;
  display.push_back('(');
  bool firstArgument = true;
  for (std::uint16_t paramIndex = 0; paramIndex < a_literal.parameterCount &&
                                     paramIndex < a_literal.arguments.size();
       ++paramIndex) {
    if (!firstArgument) {
      display.append(", ");
    }
    firstArgument = false;
    display.append(a_literal.arguments[paramIndex]);
  }
  display.push_back(')');
  display.push_back(' ');
  display.append(ComparatorToken(a_literal.comparator));
  display.push_back(' ');
  display.append(a_literal.comparand);
  return display;
}

std::string BuildCnfSignature(const ConditionCnf &a_cnf) {
  std::string signature;
  bool firstGroup = true;
  for (const auto &group : a_cnf) {
    if (!firstGroup) {
      signature.append(" AND ");
    }
    firstGroup = false;

    signature.push_back('(');
    bool firstLiteral = true;
    for (const auto &literal : group) {
      if (!firstLiteral) {
        signature.append(" OR ");
      }
      firstLiteral = false;
      signature.append(BuildLiteralSignature(literal));
    }
    signature.push_back(')');
  }
  return signature;
}

sosr::conditions::DisplayCnf BuildDisplayCnf(const ConditionCnf &a_cnf) {
  sosr::conditions::DisplayCnf displayCnf;
  displayCnf.reserve(a_cnf.size());
  for (const auto &group : a_cnf) {
    auto &displayGroup = displayCnf.emplace_back();
    displayGroup.reserve(group.size());
    for (const auto &literal : group) {
      displayGroup.push_back(BuildLiteralDisplay(literal));
    }
  }
  return displayCnf;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
ConditionCnf AndCnf(const ConditionCnf &a_left, const ConditionCnf &a_right) {
  ConditionCnf result = a_left;
  result.insert(result.end(), a_right.begin(), a_right.end());
  return result;
}

ConditionCnf OrCnf(const ConditionCnf &a_left, const ConditionCnf &a_right) {
  if (a_left.empty()) {
    return a_right;
  }
  if (a_right.empty()) {
    return a_left;
  }

  ConditionCnf result;
  result.reserve(a_left.size() * a_right.size());
  for (const auto &leftGroup : a_left) {
    for (const auto &rightGroup : a_right) {
      OrClause merged = leftGroup;
      merged.insert(merged.end(), rightGroup.begin(), rightGroup.end());
      result.push_back(std::move(merged));
    }
  }
  return result;
}

ConditionCnf NegateCnf(const ConditionCnf &a_cnf) {
  ConditionCnf result;
  bool firstGroup = true;

  for (const auto &group : a_cnf) {
    ConditionCnf negatedGroup;
    negatedGroup.reserve(group.size());
    for (const auto &literal : group) {
      auto negatedLiteral = literal;
      negatedLiteral.comparator = InvertComparator(negatedLiteral.comparator);
      negatedGroup.push_back(OrClause{std::move(negatedLiteral)});
    }

    if (firstGroup) {
      result = std::move(negatedGroup);
      firstGroup = false;
    } else {
      result = OrCnf(result, negatedGroup);
    }
  }

  return result;
}

std::optional<bool> EvaluateNestedConditionPolarity(const Clause &a_clause) {
  if (a_clause.comparator != Comparator::Equal &&
      a_clause.comparator != Comparator::NotEqual) {
    return std::nullopt;
  }

  const auto comparand = sosr::strings::TrimText(a_clause.comparand);
  if (comparand != "0" && comparand != "1") {
    return std::nullopt;
  }

  const bool truthy = comparand == "1";
  return (a_clause.comparator == Comparator::Equal && truthy) ||
         (a_clause.comparator == Comparator::NotEqual && !truthy);
}

std::optional<NativeLiteral> BuildNativeLiteral(const Clause &a_clause) {
  const auto *command = FindConditionFunction(a_clause.functionName);
  if (!command) {
    return std::nullopt;
  }

  NativeLiteral literal;
  literal.command = command;
  literal.functionName = sosr::strings::TrimText(a_clause.functionName);
  literal.parameterCount = command->numParams;
  literal.comparator = a_clause.comparator;
  literal.comparand = sosr::strings::TrimText(a_clause.comparand);

  for (std::uint16_t paramIndex = 0;
       paramIndex < command->numParams && paramIndex < 2; ++paramIndex) {
    literal.arguments[paramIndex] =
        sosr::strings::TrimText(a_clause.arguments[paramIndex]);
    literal.parameterTypes[paramIndex] = ResolveEditorParamType(
        literal.functionName, paramIndex,
        command->params ? command->params[paramIndex].paramType.get()
                        : ParamType::kForm);
  }

  return literal;
}

std::optional<ConditionCnf>
BuildConditionCnf(const Definition &a_definition,
                  const std::vector<Definition> &a_conditions,
                  ConditionVisitSet &a_visiting);

std::optional<ConditionCnf>
BuildClauseCnf(const Clause &a_clause,
               const std::vector<Definition> &a_conditions,
               ConditionVisitSet &a_visiting) {
  if (!a_clause.customConditionId.empty()) {
    const auto *definition = sosr::conditions::FindDefinitionById(
        a_conditions, a_clause.customConditionId);
    if (!definition) {
      return std::nullopt;
    }

    const auto polarity = EvaluateNestedConditionPolarity(a_clause);
    if (!polarity.has_value()) {
      return std::nullopt;
    }

    auto nestedCnf = BuildConditionCnf(*definition, a_conditions, a_visiting);
    if (!nestedCnf) {
      return std::nullopt;
    }

    if (*polarity) {
      return nestedCnf;
    }
    return NegateCnf(*nestedCnf);
  }

  auto literal = BuildNativeLiteral(a_clause);
  if (!literal) {
    return std::nullopt;
  }

  return ConditionCnf{OrClause{std::move(*literal)}};
}

std::optional<ConditionCnf>
BuildConditionCnf(const Definition &a_definition,
                  const std::vector<Definition> &a_conditions,
                  ConditionVisitSet &a_visiting) {
  if (a_definition.clauses.empty()) {
    return std::nullopt;
  }
  if (!a_definition.id.empty() && !a_visiting.insert(a_definition.id).second) {
    return std::nullopt;
  }

  auto currentBlock =
      BuildClauseCnf(a_definition.clauses.front(), a_conditions, a_visiting);
  if (!currentBlock) {
    if (!a_definition.id.empty()) {
      a_visiting.erase(a_definition.id);
    }
    return std::nullopt;
  }

  ConditionCnf result;
  bool hasResult = false;

  for (std::size_t index = 1; index < a_definition.clauses.size(); ++index) {
    auto clauseCnf =
        BuildClauseCnf(a_definition.clauses[index], a_conditions, a_visiting);
    if (!clauseCnf) {
      if (!a_definition.id.empty()) {
        a_visiting.erase(a_definition.id);
      }
      return std::nullopt;
    }

    const auto connective = a_definition.clauses[index - 1].connectiveToNext;
    if (connective == Connective::Or) {
      currentBlock = OrCnf(*currentBlock, *clauseCnf);
    } else {
      if (!hasResult) {
        result = *currentBlock;
        hasResult = true;
      } else {
        result = AndCnf(result, *currentBlock);
      }
      currentBlock = std::move(clauseCnf);
    }
  }

  if (!hasResult) {
    result = *currentBlock;
  } else {
    result = AndCnf(result, *currentBlock);
  }

  if (!a_definition.id.empty()) {
    a_visiting.erase(a_definition.id);
  }
  return result;
}

std::optional<RE::CONDITION_ITEM_DATA>
BuildConditionItemData(const NativeLiteral &a_literal,
                       const bool a_isORToNext) {
  RE::CONDITION_ITEM_DATA data{};

  const auto functionIndex = std::to_underlying(a_literal.command->output) -
                             RE::SCRIPT_FUNCTION::Commands::kScriptOpBase;
  data.functionData.function =
      static_cast<RE::FUNCTION_DATA::FunctionID>(functionIndex);

  for (std::uint16_t paramIndex = 0;
       paramIndex < a_literal.parameterCount && paramIndex < 2; ++paramIndex) {
    const auto &argument = a_literal.arguments[paramIndex];
    if (argument.empty()) {
      continue;
    }

    const auto param =
        ParseParam(argument, a_literal.parameterTypes[paramIndex]);
    if (!param) {
      return std::nullopt;
    }
    if (!IsValueParamType(a_literal.parameterTypes[paramIndex]) &&
        !param->form) {
      return std::nullopt;
    }

    data.functionData.params[paramIndex] = std::bit_cast<void *>(*param);
  }

  data.flags.opCode = ToOpCode(a_literal.comparator);
  data.flags.isOR = a_isORToNext;
  data.object = RE::CONDITIONITEMOBJECT::kSelf;
  const auto comparand = TryParseFloat(a_literal.comparand);
  if (!comparand) {
    return std::nullopt;
  }
  data.comparisonValue.f = *comparand;
  data.flags.global = false;
  return data;
}

std::optional<std::shared_ptr<RE::TESCondition>>
EmitCondition(const ConditionCnf &a_cnf) {
  auto condition = std::make_shared<RE::TESCondition>();
  RE::TESConditionItem *previous = nullptr;

  for (const auto &group : a_cnf) {
    for (std::size_t literalIndex = 0; literalIndex < group.size();
         ++literalIndex) {
      const auto data = BuildConditionItemData(group[literalIndex],
                                               literalIndex + 1 < group.size());
      if (!data) {
        return std::nullopt;
      }

      auto *item = new RE::TESConditionItem();
      item->data = *data;
      item->next = nullptr;
      if (previous) {
        previous->next = item;
      } else {
        condition->head = item;
      }
      previous = item;
    }
  }

  return condition;
}
} // namespace

namespace sosr::conditions {
std::optional<LoweredMaterialization>
LowerAndEmitCondition(const Definition &a_definition,
                      const std::vector<Definition> &a_conditions) {
  ConditionVisitSet visiting;
  auto cnf = BuildConditionCnf(a_definition, a_conditions, visiting);
  if (!cnf || cnf->empty()) {
    return std::nullopt;
  }

  auto condition = EmitCondition(*cnf);
  if (!condition) {
    return std::nullopt;
  }

  return LoweredMaterialization{.condition = *condition,
                                .signature = BuildCnfSignature(*cnf),
                                .displayCnf = BuildDisplayCnf(*cnf)};
}
} // namespace sosr::conditions
