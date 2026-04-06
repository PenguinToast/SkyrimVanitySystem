#include "Menu.h"

#include "Keycode.h"
#include "ThemeConfig.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {
constexpr auto kSettingsDirectory = "Data/SKSE/Plugins/SkyrimVanitySystem";
constexpr auto kLegacySettingsDirectory =
    "Data/SKSE/Plugins/SkyrimOutfitSystemRedone";
constexpr auto kImGuiIniFilename = "imgui.ini";
constexpr auto kUserSettingsFilename = "settings.json";
constexpr auto kFavoritesFilename = "favorites.json";

void MigrateLegacyFileIfNeeded(const std::filesystem::path &a_newPath,
                               const std::filesystem::path &a_legacyPath) {
  if (std::filesystem::exists(a_newPath) ||
      !std::filesystem::exists(a_legacyPath)) {
    return;
  }

  std::error_code error;
  std::filesystem::create_directories(a_newPath.parent_path(), error);
  if (error) {
    logger::warn("Failed to prepare migrated settings path {}: {}",
                 a_newPath.string(), error.message());
    return;
  }

  std::filesystem::copy_file(a_legacyPath, a_newPath,
                             std::filesystem::copy_options::overwrite_existing,
                             error);
  if (error) {
    logger::warn("Failed to migrate legacy settings file {} to {}: {}",
                 a_legacyPath.string(), a_newPath.string(), error.message());
    return;
  }

  std::filesystem::remove(a_legacyPath, error);
  if (error) {
    logger::warn("Migrated legacy settings file {} to {} but failed to delete "
                 "the old file: {}",
                 a_legacyPath.string(), a_newPath.string(), error.message());
    return;
  }

  logger::info("Migrated legacy settings file {} to {}", a_legacyPath.string(),
               a_newPath.string());
}
} // namespace

namespace sosr {
void Menu::LoadUserSettings() {
  try {
    std::filesystem::create_directories(settingsDirectory_);
  } catch (const std::exception &exception) {
    logger::error("Failed to create settings directory {}: {}",
                  settingsDirectory_, exception.what());
    return;
  }

  std::ifstream input(userSettingsPath_);
  if (!input.is_open()) {
    SaveUserSettings();
    return;
  }

  try {
    const auto json = nlohmann::json::parse(input, nullptr, true, true);
    auto &browser = CatalogBrowserState();
    fontSizePixels_ = std::clamp(json.value("fontSizePx", fontSizePixels_),
                                 kMinFontSizePixels, kMaxFontSizePixels);
    pendingFontSizePixels_ = fontSizePixels_;
    fontPath_ = json.value("fontPath", fontPath_);
    pauseGameWhenOpen_ = json.value("pauseGameWhileOpen", pauseGameWhenOpen_);
    smoothScroll_ = json.value("smoothScroll", smoothScroll_);
    toggleKey_ = json.value("toggleKey", toggleKey_);
    toggleModifier_ = json.value("toggleModifier", toggleModifier_);
    themeName_ = json.value("theme", themeName_);
    const auto hostMode = json.value("catalogHostMode", std::string("docked"));
    if (hostMode == "popout") {
      catalogPane_.hostMode = ui::catalog::HostMode::Popout;
      catalogPane_.popoutOpen = true;
    } else {
      catalogPane_.hostMode = ui::catalog::HostMode::Docked;
      catalogPane_.popoutOpen = false;
    }
    browser.previewSelected =
        json.value("catalogPreviewSelected", browser.previewSelected);
    browser.showAllSlots = json.value("catalogShowAllSlots", browser.showAllSlots);
    browser.favoritesOnly =
        json.value("catalogFavoritesOnly", browser.favoritesOnly);
    browser.inventoryOnly =
        json.value("catalogInventoryOnly", browser.inventoryOnly);
    browser.hideUnnamedGear =
        json.value("catalogHideUnnamedGear", browser.hideUnnamedGear);
  } catch (const std::exception &exception) {
    logger::warn("Failed to parse user settings from {}: {}", userSettingsPath_,
                 exception.what());
  }

  EnsureDefaultConditions();
}

void Menu::SaveUserSettings() const {
  try {
    std::filesystem::create_directories(settingsDirectory_);
  } catch (const std::exception &exception) {
    logger::error("Failed to create settings directory {}: {}",
                  settingsDirectory_, exception.what());
    return;
  }

  std::ofstream output(userSettingsPath_, std::ios::trunc);
  if (!output.is_open()) {
    logger::error("Failed to write settings to {}", userSettingsPath_);
    return;
  }

  const nlohmann::json json = {
      {"fontSizePx", fontSizePixels_},
      {"fontPath", fontPath_},
      {"pauseGameWhileOpen", pauseGameWhenOpen_},
      {"smoothScroll", smoothScroll_},
      {"toggleKey", toggleKey_},
      {"toggleModifier", toggleModifier_},
      {"theme", themeName_},
      {"catalogPreviewSelected", CatalogBrowserState().previewSelected},
      {"catalogShowAllSlots", CatalogBrowserState().showAllSlots},
      {"catalogFavoritesOnly", CatalogBrowserState().favoritesOnly},
      {"catalogInventoryOnly", CatalogBrowserState().inventoryOnly},
      {"catalogHideUnnamedGear", CatalogBrowserState().hideUnnamedGear},
      {"catalogHostMode", catalogPane_.hostMode == ui::catalog::HostMode::Popout
                              ? "popout"
                              : "docked"}};
  output << json.dump(2) << '\n';
}

void Menu::LoadFavorites() {
  try {
    std::filesystem::create_directories(settingsDirectory_);
  } catch (const std::exception &exception) {
    logger::error("Failed to create favorites directory {}: {}",
                  settingsDirectory_, exception.what());
    return;
  }

  CatalogBrowserState().favoriteKeys.clear();

  std::ifstream input(favoritesPath_);
  if (!input.is_open()) {
    SaveFavorites();
    return;
  }

  try {
    const auto json = nlohmann::json::parse(input, nullptr, true, true);
    if (const auto it = json.find("favorites");
        it != json.end() && it->is_array()) {
      for (const auto &entry : *it) {
        if (entry.is_string()) {
          CatalogBrowserState().favoriteKeys.insert(entry.get<std::string>());
        }
      }
    }
  } catch (const std::exception &exception) {
    logger::warn("Failed to parse favorites from {}: {}", favoritesPath_,
                 exception.what());
  }
}

void Menu::SaveFavorites() const {
  try {
    std::filesystem::create_directories(settingsDirectory_);
  } catch (const std::exception &exception) {
    logger::error("Failed to create favorites directory {}: {}",
                  settingsDirectory_, exception.what());
    return;
  }

  std::ofstream output(favoritesPath_, std::ios::trunc);
  if (!output.is_open()) {
    logger::error("Failed to write favorites to {}", favoritesPath_);
    return;
  }

  std::vector<std::string> favorites(CatalogBrowserState().favoriteKeys.begin(),
                                     CatalogBrowserState().favoriteKeys.end());
  std::ranges::sort(favorites);
  const nlohmann::json json = {{"favorites", favorites}};
  output << json.dump(2) << '\n';
}

void Menu::ApplyStyle() {
  ImGui::StyleColorsDark();
  auto &style = ImGui::GetStyle();
  style.WindowRounding = 4.0f;
  style.ChildRounding = 3.0f;
  style.FrameRounding = 4.0f;
  style.PopupRounding = 4.0f;
  style.ScrollbarRounding = 4.0f;
  style.GrabRounding = 4.0f;
  style.TabRounding = 4.0f;
  style.FrameBorderSize = 1.0f;
  style.WindowBorderSize = 1.0f;
  style.WindowPadding = {12.0f, 12.0f};
  style.FramePadding = {8.0f, 6.0f};
  style.ItemSpacing = {8.0f, 6.0f};
  style.CellPadding = {6.0f, 5.0f};
  ThemeConfig::GetSingleton()->ApplyToImGui();
}

void Menu::OpenToggleKeyCapture() {
  awaitingToggleKeyCapture_ = true;
  openToggleKeyPopup_ = true;
  toggleKeyCaptureError_.clear();
}

void Menu::CloseToggleKeyCapture() {
  awaitingToggleKeyCapture_ = false;
  openToggleKeyPopup_ = false;
  toggleKeyCaptureError_.clear();
}

void Menu::HandleToggleKeyCapture(const std::uint32_t a_scanCode,
                                  const std::uint32_t a_modifierScanCode) {
  if (a_scanCode == 0x01) {
    CloseToggleKeyCapture();
    return;
  }

  if (a_scanCode == 0x14) {
    toggleKey_ = 0x40;
    toggleModifier_ = 0;
    SaveUserSettings();
    CloseToggleKeyCapture();
    return;
  }

  if (!keycode::IsValidHotkey(a_scanCode) ||
      keycode::IsKeyModifier(a_scanCode)) {
    toggleKeyCaptureError_ =
        "Choose a non-modifier key. Hold Shift, Ctrl, or Alt for a modifier.";
    return;
  }

  toggleKey_ = a_scanCode;
  toggleModifier_ = a_modifierScanCode;
  SaveUserSettings();
  CloseToggleKeyCapture();
}

std::string Menu::GetToggleKeyLabel() const {
  if (toggleModifier_ != 0) {
    return keycode::GetKeyName(toggleModifier_) + " + " +
           keycode::GetKeyName(toggleKey_);
  }

  return keycode::GetKeyName(toggleKey_);
}

void Menu::Init(IDXGISwapChain *a_swapChain, ID3D11Device *a_device,
                ID3D11DeviceContext *a_context) {
  if (initialized_) {
    return;
  }

  settingsDirectory_ = kSettingsDirectory;
  MigrateLegacyFileIfNeeded(
      std::filesystem::path(settingsDirectory_) / kImGuiIniFilename,
      std::filesystem::path(kLegacySettingsDirectory) / kImGuiIniFilename);
  MigrateLegacyFileIfNeeded(
      std::filesystem::path(settingsDirectory_) / kUserSettingsFilename,
      std::filesystem::path(kLegacySettingsDirectory) / kUserSettingsFilename);
  MigrateLegacyFileIfNeeded(
      std::filesystem::path(settingsDirectory_) / kFavoritesFilename,
      std::filesystem::path(kLegacySettingsDirectory) / kFavoritesFilename);
  imguiIniPath_ =
      (std::filesystem::path(settingsDirectory_) / kImGuiIniFilename).string();
  userSettingsPath_ =
      (std::filesystem::path(settingsDirectory_) / kUserSettingsFilename)
          .string();
  favoritesPath_ =
      (std::filesystem::path(settingsDirectory_) / kFavoritesFilename).string();
  RefreshAvailableFonts();
  LoadUserSettings();
  NormalizeSelectedFontPath();
  LoadFavorites();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  auto &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = imguiIniPath_.c_str();
  io.LogFilename = nullptr;
  ImGui::GetStyle().FontScaleMain = 1.0f;
  pendingFontSizePixels_ = fontSizePixels_;
  RebuildFontAtlas();

  DXGI_SWAP_CHAIN_DESC description{};
  a_swapChain->GetDesc(&description);

  ImGui_ImplWin32_Init(description.OutputWindow);
  ImGui_ImplDX11_Init(a_device, a_context);

  device_ = a_device;
  context_ = a_context;

  ThemeConfig::GetSingleton()->Load(themeName_);
  ApplyStyle();
  initialized_ = true;

  logger::info("Initialized Dear ImGui menu");
}
} // namespace sosr
