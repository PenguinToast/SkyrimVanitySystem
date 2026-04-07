#include "ui/Localization.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <ranges>
#include <unordered_map>

namespace {
bool IsLocaleFile(const std::filesystem::path &a_path) {
  return a_path.has_extension() && a_path.extension() == ".json";
}
} // namespace

namespace sosr::ui {
Localization *Localization::GetSingleton() {
  static Localization singleton;
  return std::addressof(singleton);
}

void Localization::RefreshAvailableLocales(const std::string_view a_directory) {
  localeOptions_.clear();

  std::error_code error;
  const std::filesystem::path directory(a_directory);
  if (!std::filesystem::exists(directory, error) ||
      !std::filesystem::is_directory(directory, error)) {
    logger::warn("SVS locale directory not found: {}", directory.string());
    return;
  }

  for (const auto &entry : std::filesystem::directory_iterator(directory, error)) {
    if (error || !entry.is_regular_file()) {
      continue;
    }

    const auto &path = entry.path();
    if (!IsLocaleFile(path)) {
      continue;
    }

    try {
      std::ifstream input(path);
      if (!input.is_open()) {
        continue;
      }

      const auto json = nlohmann::json::parse(input, nullptr, true, true);
      LocaleOption option{};
      option.id = json.value("id", path.stem().string());
      option.name = json.value("name", option.id);
      option.path = path.lexically_normal().string();
      if (!option.id.empty()) {
        localeOptions_.push_back(std::move(option));
      }
    } catch (const std::exception &exception) {
      logger::warn("Failed to inspect SVS locale {}: {}", path.string(),
                   exception.what());
    }
  }

  std::ranges::sort(localeOptions_, [](const LocaleOption &a_left,
                                       const LocaleOption &a_right) {
    return a_left.name < a_right.name;
  });
}

void Localization::SelectLocale(const std::string_view a_localeId) {
  if (localeOptions_.empty()) {
    strings_.clear();
    missingStrings_.clear();
    currentLocaleId_ = a_localeId.empty() ? "en" : std::string(a_localeId);
    currentLocaleName_ = currentLocaleId_;
    return;
  }

  const auto *selected = FindLocale(a_localeId);
  if (!selected) {
    selected = FindLocale("en");
  }
  if (!selected) {
    selected = std::addressof(localeOptions_.front());
  }

  if (!LoadLocaleFile(*selected)) {
    if (selected->id != "en") {
      if (const auto *english = FindLocale("en");
          english && english != selected && LoadLocaleFile(*english)) {
        return;
      }
    }

    strings_.clear();
    missingStrings_.clear();
    currentLocaleId_ = selected->id;
    currentLocaleName_ = selected->name;
  }
}

std::string_view Localization::Get(const std::string_view a_key) const {
  if (const auto *value = FindString(a_key); value != nullptr) {
    return *value;
  }

  return *FindMissingString(a_key);
}

const char *Localization::GetCStr(const std::string_view a_key) const {
  return Get(a_key).data();
}

const std::string *Localization::FindString(const std::string_view a_key) const {
  const auto it = strings_.find(a_key);
  return it != strings_.end() ? std::addressof(it->second) : nullptr;
}

const std::string *Localization::FindMissingString(
    const std::string_view a_key) const {
  if (const auto it = missingStrings_.find(a_key); it != missingStrings_.end()) {
    return std::addressof(it->second);
  }

  auto missing = std::string("!!missing:");
  missing.append(a_key);
  const auto [it, _] =
      missingStrings_.emplace(std::string(a_key), std::move(missing));
  return std::addressof(it->second);
}

bool Localization::LoadLocaleFile(const LocaleOption &a_option) {
  std::ifstream input(a_option.path);
  if (!input.is_open()) {
    logger::warn("Failed to open SVS locale {}", a_option.path);
    return false;
  }

  try {
    const auto json = nlohmann::json::parse(input, nullptr, true, true);
    const auto stringsIt = json.find("strings");
    if (stringsIt == json.end() || !stringsIt->is_object()) {
      logger::warn("SVS locale {} has no strings object", a_option.path);
      return false;
    }

    decltype(strings_) newStrings;
    for (auto it = stringsIt->begin(); it != stringsIt->end(); ++it) {
      if (it.value().is_string()) {
        newStrings.emplace(it.key(), it.value().get<std::string>());
      }
    }

    strings_ = std::move(newStrings);
    missingStrings_.clear();
    currentLocaleId_ = a_option.id;
    currentLocaleName_ = a_option.name;
    logger::info("Loaded SVS locale {} ({})", currentLocaleId_,
                 currentLocaleName_);
    return true;
  } catch (const std::exception &exception) {
    logger::warn("Failed to parse SVS locale {}: {}", a_option.path,
                 exception.what());
    return false;
  }
}

const Localization::LocaleOption *
Localization::FindLocale(const std::string_view a_localeId) const {
  const auto it =
      std::ranges::find(localeOptions_, a_localeId, &LocaleOption::id);
  return it != localeOptions_.end() ? std::addressof(*it) : nullptr;
}
} // namespace sosr::ui
