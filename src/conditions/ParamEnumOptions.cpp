#include "conditions/ParamEnumOptions.h"

#include "StringUtils.h"

#include <RE/C/CRC.h>
#include <RE/F/FormTypes.h>
#include <RE/M/MagicSystem.h>
#include <RE/S/Sexes.h>

#include <array>
#include <charconv>
#include <span>
#include <system_error>
#include <utility>

namespace {
struct ParamEnumOption {
  std::string_view label;
  std::int32_t value;
};

using ParamType = RE::SCRIPT_PARAM_TYPE;

constexpr std::array kSexOptions{
    ParamEnumOption{"Male", static_cast<std::int32_t>(RE::SEX::kMale)},
    ParamEnumOption{"Female", static_cast<std::int32_t>(RE::SEX::kFemale)},
};

constexpr std::array kCastingSourceOptions{
    ParamEnumOption{"LeftHand", static_cast<std::int32_t>(
                                    RE::MagicSystem::CastingSource::kLeftHand)},
    ParamEnumOption{
        "RightHand",
        static_cast<std::int32_t>(RE::MagicSystem::CastingSource::kRightHand)},
    ParamEnumOption{"Other", static_cast<std::int32_t>(
                                 RE::MagicSystem::CastingSource::kOther)},
    ParamEnumOption{"Instant", static_cast<std::int32_t>(
                                   RE::MagicSystem::CastingSource::kInstant)},
};

constexpr std::array kWardStateOptions{
    ParamEnumOption{
        "None", static_cast<std::int32_t>(RE::MagicSystem::WardState::kNone)},
    ParamEnumOption{"Absorb", static_cast<std::int32_t>(
                                  RE::MagicSystem::WardState::kAbsorb)},
    ParamEnumOption{
        "Break", static_cast<std::int32_t>(RE::MagicSystem::WardState::kBreak)},
};

constexpr std::array kCriticalStageOptions{
    ParamEnumOption{"None", 0},
    ParamEnumOption{"GooStart", 1},
    ParamEnumOption{"GooEnd", 2},
    ParamEnumOption{"DisintegrateStart", 3},
    ParamEnumOption{"DisintegrateEnd", 4},
};

constexpr std::array kFurnitureMarkerOptions{
    ParamEnumOption{"None", 0},   ParamEnumOption{"Front", 1},
    ParamEnumOption{"Behind", 2}, ParamEnumOption{"Right", 4},
    ParamEnumOption{"Left", 8},   ParamEnumOption{"Up", 16},
};

constexpr std::array kFurnitureEntryOptions{
    ParamEnumOption{"None", 0},           ParamEnumOption{"Front", 1 << 16},
    ParamEnumOption{"Behind", 1 << 17},   ParamEnumOption{"Right", 1 << 18},
    ParamEnumOption{"Left", 1 << 19},     ParamEnumOption{"Up", 1 << 20},
};

constexpr std::array kMiscStatOptions{
    "Locations Discovered",
    "Dungeons Cleared",
    "Days passed",
    "Hours Slept",
    "Hours Waiting",
    "Standing Stones Found",
    "Gold Found",
    "Most Gold Carried",
    "Chests Looted",
    "Skill Increases",
    "Skill Books Read",
    "Food Eaten",
    "Training Sessions",
    "Books Read",
    "Horses Owned",
    "Houses Owned",
    "Stores Invested In",
    "Barters",
    "Persuasions",
    "Bribes",
    "Intimidations",
    "Diseases Contracted",
    "Quests Completed",
    "Misc Objectives Completed",
    "Main Quests Completed",
    "Side Quests Completed",
    "The Companions Quests Completed",
    "College of Winterhold Quests Completed",
    "Thieves' Guild Quests Completed",
    "The Dark Brotherhood Quests Completed",
    "Civil War Quests Completed",
    "Daedric Quests Completed",
    "Questlines Completed",
    "People Killed",
    "Animals Killed",
    "Creatures Killed",
    "Undead Killed",
    "Daedra Killed",
    "Automatons Killed",
    "Favorite Weapon",
    "Critical Strikes",
    "Sneak Attacks",
    "Backstabs",
    "Weapons Disarmed",
    "Brawls Won",
    "Bunnies Slaughtered",
    "Spells Learned",
    "Favorite Spell",
    "Favorite School",
    "Dragon Souls Collected",
    "Words Of Power Learned",
    "Words Of Power Unlocked",
    "Shouts Learned",
    "Shouts Unlocked",
    "Shouts Mastered",
    "Times Shouted",
    "Favorite Shout",
    "Soul Gems Used",
    "Souls Trapped",
    "Magic Items Made",
    "Weapons Improved",
    "Weapons Made",
    "Armor Improved",
    "Armor Made",
    "Potions Mixed",
    "Potions Used",
    "Poisons Mixed",
    "Poisons Used",
    "Ingredients Harvested",
    "Ingredients Eaten",
    "Nirnroots Found",
    "Wings Plucked",
    "Total Lifetime Bounty",
    "Largest Bounty",
    "Locks Picked",
    "Pockets Picked",
    "Items Pickpocketed",
    "Times Jailed",
    "Days Jailed",
    "Fines Paid",
    "Jail Escapes",
    "Items Stolen",
    "Assaults",
    "Murders",
    "Horses Stolen",
    "Trespasses",
};

std::span<const ParamEnumOption> GetFixedOptions(const ParamType a_type) {
  switch (a_type) {
  case ParamType::kSex:
    return kSexOptions;
  case ParamType::kCastingSource:
    return kCastingSourceOptions;
  case ParamType::kWardState:
    return kWardStateOptions;
  case ParamType::kCritStage:
    return kCriticalStageOptions;
  case ParamType::kFurnitureAnimType:
    return kFurnitureMarkerOptions;
  case ParamType::kFurnitureEntryType:
    return kFurnitureEntryOptions;
  default:
    return {};
  }
}

std::optional<std::int32_t> ParseInteger(std::string_view a_token) {
  const auto trimmed = sosr::strings::TrimText(a_token);
  if (trimmed.empty()) {
    return std::nullopt;
  }

  std::int32_t value = 0;
  const auto *begin = trimmed.data();
  const auto *end = trimmed.data() + trimmed.size();
  const auto [ptr, error] = std::from_chars(begin, end, value);
  if (error == std::errc{} && ptr == end) {
    return value;
  }
  return std::nullopt;
}

std::optional<std::int32_t> ParseFixedOption(const ParamType a_type,
                                             std::string_view a_token) {
  const auto trimmed = sosr::strings::TrimText(a_token);
  const auto options = GetFixedOptions(a_type);
  for (const auto &option : options) {
    if (sosr::strings::EqualsInsensitive(option.label, trimmed)) {
      return option.value;
    }
  }

  if (a_type == ParamType::kCastingSource) {
    if (sosr::strings::EqualsInsensitive(trimmed, "LEFT")) {
      return static_cast<std::int32_t>(
          RE::MagicSystem::CastingSource::kLeftHand);
    }
    if (sosr::strings::EqualsInsensitive(trimmed, "RIGHT")) {
      return static_cast<std::int32_t>(
          RE::MagicSystem::CastingSource::kRightHand);
    }
    if (sosr::strings::EqualsInsensitive(trimmed, "VOICE")) {
      return static_cast<std::int32_t>(RE::MagicSystem::CastingSource::kOther);
    }
  }

  return std::nullopt;
}

std::optional<std::int32_t> ParseFormType(std::string_view a_token) {
  const auto trimmed = sosr::strings::TrimText(a_token);
  if (trimmed.empty()) {
    return std::nullopt;
  }

  for (auto value = 0; value < std::to_underlying(RE::FormType::Max); ++value) {
    const auto formType = static_cast<RE::FormType>(value);
    if (sosr::strings::EqualsInsensitive(RE::FormTypeToString(formType),
                                         trimmed)) {
      return value;
    }
  }
  return std::nullopt;
}
} // namespace

namespace sosr::conditions {
bool HasParamEnumOptions(const RE::SCRIPT_PARAM_TYPE a_type) {
  return a_type == ParamType::kFormType || !GetFixedOptions(a_type).empty();
}

bool HasParamTextOptions(const RE::SCRIPT_PARAM_TYPE a_type) {
  return a_type == ParamType::kMiscStat;
}

std::vector<std::string>
BuildParamEnumOptionLabels(const RE::SCRIPT_PARAM_TYPE a_type) {
  std::vector<std::string> labels;
  if (a_type == ParamType::kFormType) {
    labels.reserve(std::to_underlying(RE::FormType::Max));
    for (auto value = 0; value < std::to_underlying(RE::FormType::Max);
         ++value) {
      labels.emplace_back(
          RE::FormTypeToString(static_cast<RE::FormType>(value)));
    }
    return labels;
  }

  const auto options = GetFixedOptions(a_type);
  labels.reserve(options.size());
  for (const auto &option : options) {
    labels.emplace_back(option.label);
  }
  return labels;
}

std::optional<std::int32_t>
ParseParamEnumOption(const RE::SCRIPT_PARAM_TYPE a_type,
                     const std::string_view a_token) {
  if (const auto integer = ParseInteger(a_token)) {
    return integer;
  }

  if (a_type == ParamType::kFormType) {
    return ParseFormType(a_token);
  }
  return ParseFixedOption(a_type, a_token);
}

std::vector<std::string>
BuildParamTextOptionLabels(const RE::SCRIPT_PARAM_TYPE a_type) {
  std::vector<std::string> labels;
  if (a_type != ParamType::kMiscStat) {
    return labels;
  }

  labels.reserve(kMiscStatOptions.size());
  for (const auto *label : kMiscStatOptions) {
    labels.emplace_back(label);
  }
  return labels;
}

std::optional<std::int32_t>
ParseParamTextOption(const RE::SCRIPT_PARAM_TYPE a_type,
                     const std::string_view a_token) {
  if (a_type != ParamType::kMiscStat) {
    return std::nullopt;
  }

  const auto trimmed = sosr::strings::TrimText(a_token);
  for (const auto *label : kMiscStatOptions) {
    if (sosr::strings::EqualsInsensitive(label, trimmed)) {
      // MiscStat CTDA params store the game's CRC of the stat label.
      return static_cast<std::int32_t>(
          RE::BSCRC32<std::string_view>{}(label));
    }
  }
  return std::nullopt;
}
} // namespace sosr::conditions
