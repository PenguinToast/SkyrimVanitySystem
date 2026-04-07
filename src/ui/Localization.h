#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sosr::ui {
class Localization {
public:
  struct TransparentStringHash {
    using is_transparent = void;

    [[nodiscard]] std::size_t operator()(std::string_view a_value) const noexcept {
      return std::hash<std::string_view>{}(a_value);
    }
    [[nodiscard]] std::size_t operator()(const std::string &a_value) const noexcept {
      return (*this)(std::string_view(a_value));
    }
    [[nodiscard]] std::size_t operator()(const char *a_value) const noexcept {
      return (*this)(std::string_view(a_value != nullptr ? a_value : ""));
    }
  };

  struct TransparentStringEqual {
    using is_transparent = void;

    [[nodiscard]] bool operator()(std::string_view a_left,
                                  std::string_view a_right) const noexcept {
      return a_left == a_right;
    }
  };

  struct LocaleOption {
    std::string id;
    std::string name;
    std::string path;
  };

  static Localization *GetSingleton();

  void RefreshAvailableLocales(std::string_view a_directory);
  void SelectLocale(std::string_view a_localeId);

  [[nodiscard]] const std::vector<LocaleOption> &GetAvailableLocales() const {
    return localeOptions_;
  }
  [[nodiscard]] const std::string &GetCurrentLocaleId() const {
    return currentLocaleId_;
  }
  [[nodiscard]] const std::string &GetCurrentLocaleName() const {
    return currentLocaleName_;
  }
  [[nodiscard]] std::string_view Get(std::string_view a_key) const;
  [[nodiscard]] const char *GetCStr(std::string_view a_key) const;

private:
  [[nodiscard]] const std::string *FindString(std::string_view a_key) const;
  [[nodiscard]] const std::string *FindMissingString(std::string_view a_key) const;
  [[nodiscard]] bool LoadLocaleFile(const LocaleOption &a_option);
  [[nodiscard]] const LocaleOption *FindLocale(std::string_view a_localeId) const;

  std::vector<LocaleOption> localeOptions_;
  std::unordered_map<std::string, std::string, TransparentStringHash,
                     TransparentStringEqual>
      strings_;
  mutable std::unordered_map<std::string, std::string, TransparentStringHash,
                             TransparentStringEqual>
      missingStrings_;
  std::string currentLocaleId_{"en"};
  std::string currentLocaleName_{"English"};
};
} // namespace sosr::ui
