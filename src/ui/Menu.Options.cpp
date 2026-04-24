#include "Menu.h"

#include "ThemeConfig.h"
#include "backends/imgui_impl_dx11.h"
#include "ui/Localization.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <unordered_set>

namespace sosr {
namespace {
constexpr std::array<ImWchar, 3> kLucideIconRanges = {0xE038, 0xE63F, 0};

bool IsFontFile(const std::filesystem::path &a_path) {
  const auto extension = a_path.extension().string();
  if (extension.empty()) {
    return false;
  }

  std::string lowerExtension;
  lowerExtension.reserve(extension.size());
  for (const auto ch : extension) {
    lowerExtension.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }

  return lowerExtension == ".ttf" || lowerExtension == ".otf" ||
         lowerExtension == ".ttc" || lowerExtension == ".otc";
}

int CompareFontText(std::string_view a_left, std::string_view a_right) {
  const auto leftSize = a_left.size();
  const auto rightSize = a_right.size();
  const auto count = (std::min)(leftSize, rightSize);

  for (std::size_t index = 0; index < count; ++index) {
    const auto left = static_cast<unsigned char>(
        std::tolower(static_cast<unsigned char>(a_left[index])));
    const auto right = static_cast<unsigned char>(
        std::tolower(static_cast<unsigned char>(a_right[index])));
    if (left < right) {
      return -1;
    }
    if (left > right) {
      return 1;
    }
  }

  if (leftSize < rightSize) {
    return -1;
  }
  if (leftSize > rightSize) {
    return 1;
  }
  return 0;
}

std::string BuildFontLabel(const std::filesystem::path &a_path) {
  if (const auto stem = a_path.stem().string(); !stem.empty()) {
    return stem;
  }

  return a_path.filename().string();
}

void SortFontOptions(std::vector<Menu::FontOption> &a_options) {
  std::ranges::sort(a_options, [](const auto &left, const auto &right) {
    return CompareFontText(left.label, right.label) < 0;
  });
}

std::vector<std::filesystem::path> BuildSystemFontDirectories() {
  std::vector<std::filesystem::path> directories;
  auto getEnvironmentPath =
      [](const char *name) -> std::optional<std::filesystem::path> {
    char *value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr ||
        length <= 1) {
      free(value);
      return std::nullopt;
    }

    std::filesystem::path result(value);
    free(value);
    return result;
  };

  if (const auto windowsDir = getEnvironmentPath("WINDIR")) {
    directories.emplace_back(*windowsDir / "Fonts");
  } else {
    directories.emplace_back("C:/Windows/Fonts");
  }

  if (const auto localAppData = getEnvironmentPath("LOCALAPPDATA")) {
    directories.emplace_back(*localAppData / "Microsoft/Windows/Fonts");
  }

  std::vector<std::filesystem::path> uniqueDirectories;
  for (const auto &directory : directories) {
    if (std::ranges::find(uniqueDirectories, directory) ==
        uniqueDirectories.end()) {
      uniqueDirectories.push_back(directory);
    }
  }
  return uniqueDirectories;
}

const char *
FindSelectedFontLabel(const std::vector<Menu::FontOption> &a_options,
                      const std::string &a_fontPath) {
  if (const auto it = std::ranges::find_if(a_options,
                                           [&](const Menu::FontOption &option) {
                                             return option.path == a_fontPath;
                                           });
      it != a_options.end()) {
    return it->label.c_str();
  }

  return nullptr;
}
} // namespace

void Menu::RefreshAvailableFonts() {
  bundledFontOptions_.clear();
  systemFontOptions_.clear();

  std::unordered_set<std::string> seenPaths;
  const auto iconFontPath =
      std::filesystem::path(kDefaultIconFontPath).lexically_normal().string();

  auto collectFonts = [&](const std::filesystem::path &directory,
                          std::vector<FontOption> &target, bool isBundled) {
    std::error_code error;
    if (!std::filesystem::exists(directory, error) ||
        !std::filesystem::is_directory(directory, error)) {
      return;
    }

    for (const auto &entry :
         std::filesystem::directory_iterator(directory, error)) {
      if (error || !entry.is_regular_file()) {
        continue;
      }

      const auto &path = entry.path();
      if (!IsFontFile(path)) {
        continue;
      }

      const auto normalizedPath = path.lexically_normal().string();
      if (normalizedPath == iconFontPath ||
          !seenPaths.insert(normalizedPath).second) {
        continue;
      }

      target.push_back({.label = BuildFontLabel(path),
                        .path = normalizedPath,
                        .isBundled = isBundled});
    }
  };

  collectFonts(kBundledFontDirectory, bundledFontOptions_, true);
  for (const auto &directory : BuildSystemFontDirectories()) {
    collectFonts(directory, systemFontOptions_, false);
  }

  SortFontOptions(bundledFontOptions_);
  SortFontOptions(systemFontOptions_);
}

void Menu::NormalizeSelectedFontPath() {
  if (fontPath_.empty()) {
    fontPath_ = kDefaultFontPath;
  }

  auto matchesPath = [&](const FontOption &option) {
    return option.path == fontPath_;
  };

  if (std::ranges::find_if(bundledFontOptions_, matchesPath) !=
          bundledFontOptions_.end() ||
      std::ranges::find_if(systemFontOptions_, matchesPath) !=
          systemFontOptions_.end()) {
    return;
  }

  fontPath_ = kDefaultFontPath;
}

void Menu::RebuildFontAtlas() {
  auto &io = ImGui::GetIO();
  auto &style = ImGui::GetStyle();
  ImFontConfig fontConfig{};
  fontConfig.SizePixels = static_cast<float>(fontSizePixels_);
  fontConfig.OversampleH = 1;
  fontConfig.OversampleV = 1;
  fontConfig.PixelSnapH = true;

  io.Fonts->Clear();
  io.FontDefault = nullptr;
  if (std::filesystem::exists(fontPath_)) {
    io.FontDefault = io.Fonts->AddFontFromFileTTF(
        fontPath_.c_str(), static_cast<float>(fontSizePixels_), &fontConfig);
  } else if (fontPath_ != kDefaultFontPath &&
             std::filesystem::exists(kDefaultFontPath)) {
    io.FontDefault = io.Fonts->AddFontFromFileTTF(
        kDefaultFontPath, static_cast<float>(fontSizePixels_), &fontConfig);
  }
  if (!io.FontDefault) {
    io.FontDefault = io.Fonts->AddFontDefaultVector(&fontConfig);
  }
  if (io.FontDefault && std::filesystem::exists(kDefaultIconFontPath)) {
    ImFontConfig iconConfig{};
    iconConfig.MergeMode = true;
    iconConfig.PixelSnapH = true;
    iconConfig.GlyphOffset.y = 3.0f;
    iconConfig.DstFont = io.FontDefault;
    io.Fonts->AddFontFromFileTTF(kDefaultIconFontPath,
                                 static_cast<float>(fontSizePixels_),
                                 &iconConfig, kLucideIconRanges.data());
  }
  io.Fonts->Build();
  style.FontScaleMain = 1.0f;
  style._NextFrameFontSizeBase = static_cast<float>(fontSizePixels_);
  style.FontSizeBase = static_cast<float>(fontSizePixels_);

  if (initialized_) {
    ImGui_ImplDX11_InvalidateDeviceObjects();
  }
}

void Menu::DrawOptionsTab() {
  auto *localization = ui::Localization::GetSingleton();
  const auto interfaceLabel = localization->Get("options.interface");
  const auto descriptionLabel = localization->Get("options.description");
  const auto controlsLabel = localization->Get("options.controls");
  const auto languageLabel = localization->Get("options.language");
  const auto fontLabel = localization->Get("options.font");
  const auto defaultLabel = localization->Get("common.default");
  const auto bundledFontsLabel = localization->Get("options.font.bundled");
  const auto systemFontsLabel = localization->Get("options.font.system");
  const auto fontSizeLabel = localization->Get("options.font_size");
  const auto minFontSize = kMinFontSizePixels;
  const auto maxFontSize = kMaxFontSizePixels;
  const auto fontSizeRangeLabel =
      std::vformat(std::string(localization->Get("options.font_size.range")),
                   std::make_format_args(minFontSize, maxFontSize));
  const auto resetToDefaultLabel = localization->Get("options.reset_default");
  const auto toggleUIButtonLabel =
      localization->Get("options.toggle_ui_button");
  const auto pauseGameLabel =
      localization->Get("options.pause_game_while_open");
  const auto smoothScrollingLabel =
      localization->Get("options.smooth_scrolling");
  const auto saveDataLabel = localization->Get("options.save_data");
  const auto saveDataPathHint =
      localization->Get("options.save_data.path_hint");
  const auto exportJsonLabel = localization->Get("options.export_json");
  const auto importJsonLabel = localization->Get("options.import_json");
  const auto importJsonErrorTitle =
      localization->Get("options.import_json.error_title");
  const auto toggleCaptureTitle =
      localization->Get("options.toggle_capture.title");
  const auto toggleCaptureBody =
      localization->Get("options.toggle_capture.body");
  const auto toggleCaptureHint =
      localization->Get("options.toggle_capture.hint");
  const auto cancelLabel = localization->Get("common.cancel");

  ClearCatalogSelection();
  ImGui::TextUnformatted(interfaceLabel.data());
  ImGui::Separator();

  if (ImGui::BeginChild("##options-font-panel", ImVec2(0.0f, 0.0f),
                        ImGuiChildFlags_Borders |
                            ImGuiChildFlags_AutoResizeY)) {
    if (ImGui::BeginTable("##options-layout", 2,
                          ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0.0f, 0.0f))) {
      ImGui::TableSetupColumn(descriptionLabel.data(),
                              ImGuiTableColumnFlags_WidthStretch, 1.15f);
      ImGui::TableSetupColumn(controlsLabel.data(),
                              ImGuiTableColumnFlags_WidthStretch, 0.85f);
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(languageLabel.data());

      ImGui::TableSetColumnIndex(1);
      {
        const auto &localeOptions = localization->GetAvailableLocales();
        const auto &selectedLocaleName = localization->GetCurrentLocaleName();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##locale", selectedLocaleName.c_str())) {
          for (const auto &option : localeOptions) {
            const bool selected = option.id == localeId_;
            if (ImGui::Selectable(option.name.c_str(), selected)) {
              localeId_ = option.id;
              NormalizeSelectedLocaleId();
              SaveUserSettings();
            }
            if (selected) {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }
      }

      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(fontLabel.data());

      ImGui::TableSetColumnIndex(1);
      const char *selectedFontLabel =
          FindSelectedFontLabel(bundledFontOptions_, fontPath_);
      if (!selectedFontLabel) {
        selectedFontLabel =
            FindSelectedFontLabel(systemFontOptions_, fontPath_);
      }
      if (!selectedFontLabel) {
        selectedFontLabel = defaultLabel.data();
      }

      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::BeginCombo("##font-path", selectedFontLabel)) {
        if (!bundledFontOptions_.empty()) {
          ImGui::SeparatorText(bundledFontsLabel.data());
          for (const auto &option : bundledFontOptions_) {
            const bool selected = option.path == fontPath_;
            if (ImGui::Selectable(option.label.c_str(), selected)) {
              fontPath_ = option.path;
              SaveUserSettings();
              pendingFontAtlasRebuild_ = true;
            }
            if (selected) {
              ImGui::SetItemDefaultFocus();
            }
          }
        }

        if (!systemFontOptions_.empty()) {
          ImGui::SeparatorText(systemFontsLabel.data());
          for (const auto &option : systemFontOptions_) {
            const bool selected = option.path == fontPath_;
            if (ImGui::Selectable(option.label.c_str(), selected)) {
              fontPath_ = option.path;
              SaveUserSettings();
              pendingFontAtlasRebuild_ = true;
            }
            if (selected) {
              ImGui::SetItemDefaultFocus();
            }
          }
        }

        ImGui::EndCombo();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(fontSizeLabel.data());
      ImGui::TextDisabled("%s", fontSizeRangeLabel.c_str());

      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      ImGui::SliderInt("##font-size", &pendingFontSizePixels_,
                       kMinFontSizePixels, kMaxFontSizePixels, "%d px");
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        fontSizePixels_ = pendingFontSizePixels_;
        SaveUserSettings();
        pendingFontAtlasRebuild_ = true;
      }

      if (ImGui::Button(resetToDefaultLabel.data())) {
        fontPath_ = kDefaultFontPath;
        fontSizePixels_ = kDefaultFontSizePixels;
        pendingFontSizePixels_ = fontSizePixels_;
        SaveUserSettings();
        pendingFontAtlasRebuild_ = true;
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(toggleUIButtonLabel.data());

      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::Button(GetToggleKeyLabel().c_str(), ImVec2(-FLT_MIN, 0.0f))) {
        OpenToggleKeyCapture();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(pauseGameLabel.data());

      ImGui::TableSetColumnIndex(1);
      bool pauseGameWhenOpen = pauseGameWhenOpen_;
      if (ImGui::Checkbox("##pause-game-while-open", &pauseGameWhenOpen)) {
        pauseGameWhenOpen_ = pauseGameWhenOpen;
        SaveUserSettings();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(smoothScrollingLabel.data());

      ImGui::TableSetColumnIndex(1);
      bool smoothScroll = smoothScroll_;
      if (ImGui::Checkbox("##smooth-scrolling", &smoothScroll)) {
        smoothScroll_ = smoothScroll;
        pendingSmoothWheelDelta_ = 0.0f;
        smoothScrollWindowId_ = 0;
        smoothScrollTargetY_ = 0.0f;
        SaveUserSettings();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(saveDataLabel.data());

      ImGui::TableSetColumnIndex(1);
      if (saveDataPath_[0] == '\0') {
        ResetSaveDataPath();
      }
      ImGui::SetNextItemWidth(-FLT_MIN);
      ImGui::InputTextWithHint("##save-data-path", saveDataPathHint.data(),
                               saveDataPath_.data(), saveDataPath_.size());
      if (ImGui::Button(exportJsonLabel.data())) {
        std::string error;
        if (ExportSaveDataJson(saveDataPath_.data(), error)) {
          const std::string pathText(saveDataPath_.data());
          saveDataStatus_ = std::vformat(
              std::string(localization->Get("options.export_json.success")),
              std::make_format_args(pathText));
          saveDataStatusIsError_ = false;
        } else {
          saveDataStatus_ = std::move(error);
          saveDataStatusIsError_ = true;
        }
      }
      ImGui::SameLine();
      if (ImGui::Button(importJsonLabel.data())) {
        std::string error;
        if (ImportSaveDataJson(saveDataPath_.data(), error)) {
          const std::string pathText(saveDataPath_.data());
          saveDataStatus_ = std::vformat(
              std::string(localization->Get("options.import_json.success")),
              std::make_format_args(pathText));
          saveDataStatusIsError_ = false;
        } else {
          saveDataStatus_ = std::move(error);
          saveDataStatusIsError_ = true;
          openSaveDataErrorPopup_ = true;
        }
      }
      if (!saveDataStatus_.empty()) {
        const auto color = ThemeConfig::GetSingleton()->GetColor(
            saveDataStatusIsError_ ? "ERROR" : "SUCCESS");
        ImGui::TextColored(color, "%s", saveDataStatus_.c_str());
      }

      ImGui::EndTable();
    }
    ImGui::EndChild();
  }

  if (openSaveDataErrorPopup_) {
    ImGui::OpenPopup("##save-data-error");
    openSaveDataErrorPopup_ = false;
  }

  ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("##save-data-error", nullptr,
                             ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove)) {
    ImGui::TextUnformatted(importJsonErrorTitle.data());
    ImGui::Separator();
    ImGui::TextWrapped("%s", saveDataStatus_.c_str());
    ImGui::Spacing();
    if (ImGui::Button(localization->GetCStr("common.ok"),
                      ImVec2(-FLT_MIN, 0.0f))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (openToggleKeyPopup_) {
    ImGui::OpenPopup("##toggle-key-capture");
    openToggleKeyPopup_ = false;
  }

  ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("##toggle-key-capture", nullptr,
                             ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove)) {
    ImGui::TextUnformatted(toggleCaptureTitle.data());
    ImGui::Separator();
    ImGui::TextWrapped("%s", toggleCaptureBody.data());
    ImGui::TextWrapped("%s", toggleCaptureHint.data());
    if (!toggleKeyCaptureError_.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(ThemeConfig::GetSingleton()->GetColor("ERROR"), "%s",
                         toggleKeyCaptureError_.c_str());
    }
    ImGui::Spacing();
    if (ImGui::Button(cancelLabel.data(), ImVec2(-FLT_MIN, 0.0f))) {
      CloseToggleKeyCapture();
      ImGui::CloseCurrentPopup();
    }
    if (!awaitingToggleKeyCapture_) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
} // namespace sosr
