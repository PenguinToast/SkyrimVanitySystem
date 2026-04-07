#include "ui/components/EditableCombo.h"

#include "InputManager.h"
#include "StringUtils.h"
#include "imgui_internal.h"
#include "ui/InputWidgets.h"
#include "ui/Localization.h"
#include "ui/ThemeConfig.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <string_view>

namespace {
using OptionView = sosr::ui::components::EditableDropdownOptionView;

bool IsSectionEntry(const OptionView &a_option) { return a_option.isSection; }

std::string_view GetSectionLabel(const OptionView &a_option) {
  return a_option.label;
}

const OptionView *
FindTopAutocompleteOption(std::span<const OptionView> a_options,
                          std::string_view a_buffer) {
  const auto needle = sosr::strings::TrimText(a_buffer);
  for (const auto &option : a_options) {
    if (IsSectionEntry(option)) {
      continue;
    }
    if (!needle.empty() &&
        !sosr::strings::ContainsInsensitive(option.label, needle)) {
      continue;
    }
    return std::addressof(option);
  }
  return nullptr;
}

struct EditableDropdownAutocompleteData {
  std::span<const OptionView> options;
};

int EditableDropdownInputCallback(ImGuiInputTextCallbackData *a_data) {
  auto *userData =
      static_cast<EditableDropdownAutocompleteData *>(a_data->UserData);
  if (!userData || userData->options.empty() ||
      a_data->EventFlag != ImGuiInputTextFlags_CallbackCompletion) {
    return 0;
  }

  const std::string_view buffer{a_data->Buf,
                                static_cast<std::size_t>(a_data->BufTextLen)};
  const auto *topOption = FindTopAutocompleteOption(userData->options, buffer);
  if (!topOption) {
    return 0;
  }

  a_data->DeleteChars(0, a_data->BufTextLen);
  a_data->InsertChars(0, std::string(topOption->label).c_str());
  a_data->SelectAll();
  return 0;
}
} // namespace

namespace sosr::ui::components {
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool DrawSearchableStringCombo(const char *a_label, const char *a_allLabel,
                               const std::vector<std::string> &a_options,
                               int &a_index, ImGuiTextFilter &a_filter) {
  const char *preview =
      a_index == 0 ? a_allLabel
                   : a_options[static_cast<std::size_t>(a_index - 1)].c_str();
  const float width = ImGui::CalcItemWidth();

  std::vector<EditableDropdownOptionView> optionViews;
  optionViews.reserve(a_options.size() + 1);
  optionViews.push_back({.label = a_allLabel, .isSection = false});
  for (const auto &option : a_options) {
    optionViews.push_back({.label = option, .isSection = false});
  }

  const bool changed = detail::DrawEditableDropdownIndexed(
      a_label, preview, a_filter.InputBuf, IM_ARRAYSIZE(a_filter.InputBuf),
      std::span<const EditableDropdownOptionView>(optionViews), width, &a_index,
      false, a_index);

  if (a_filter.InputBuf[0] == '\0') {
    a_index = 0;
    a_filter.Build();
    return changed;
  }

  for (std::size_t index = 0; index < optionViews.size(); ++index) {
    if (sosr::strings::CompareTextInsensitive(optionViews[index].label,
                                              a_filter.InputBuf) != 0) {
      continue;
    }

    a_index = static_cast<int>(index);
    a_filter.Build();
    return true;
  }

  a_filter.Build();
  return changed;
}

namespace detail {
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool DrawEditableDropdownIndexed(
    const char *a_label, const char *a_hint, char *a_buffer,
    const std::size_t a_bufferSize,
    std::span<const EditableDropdownOptionView> a_options, const float a_width,
    int *a_selectedIndex, const bool a_allowCustomInput,
    const int a_fallbackIndex,
    const std::function<void(int)> *a_drawItemTooltip) {
  bool changed = false;
  const bool acceptAutocompleteOnEnter = !a_allowCustomInput;
  const auto popupId = std::string(a_label) + "##popup";
  const auto openId = std::string(a_label) + "##open";
  const auto highlightId = std::string(a_label) + "##highlight";
  const auto filterHashId = std::string(a_label) + "##filterhash";
  const auto arrowAreaWidth = ImGui::GetFrameHeight();
  const auto inputWidth = (std::max)(1.0f, a_width - arrowAreaWidth);
  EditableDropdownAutocompleteData autocompleteData{a_options};
  ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_AutoSelectAll |
                                   ImGuiInputTextFlags_CallbackCompletion;
  if (acceptAutocompleteOnEnter) {
    inputFlags |= ImGuiInputTextFlags_EnterReturnsTrue;
  }

  ImGui::PushID(a_label);
  auto *storage = ImGui::GetStateStorage();
  const auto openStorageId = ImGui::GetID(openId.c_str());
  const auto highlightStorageId = ImGui::GetID(highlightId.c_str());
  const auto filterHashStorageId = ImGui::GetID(filterHashId.c_str());
  const auto popupImGuiId = ImGui::GetID(popupId.c_str());

  ImGui::SetNextItemWidth(inputWidth);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  const bool submitted = ImGui::InputTextWithHint(
      "##input", a_hint, a_buffer, a_bufferSize, inputFlags,
      EditableDropdownInputCallback, std::addressof(autocompleteData));
  ImGui::PopStyleVar();
  const auto inputItemId = ImGui::GetItemID();
  if (submitted && a_allowCustomInput) {
    changed = true;
  }
  const bool inputTextActive = ImGui::IsItemActive();
  if (ImGui::IsItemActivated()) {
    InputManager::GetSingleton()->Flush();
    storage->SetBool(openStorageId, true);
    ImGui::OpenPopup(popupId.c_str());
  }
  if (inputTextActive) {
    storage->SetBool(openStorageId, true);
  }

  const auto inputMin = ImGui::GetItemRectMin();
  const auto inputMax = ImGui::GetItemRectMax();
  const auto inputFrameHeight = inputMax.y - inputMin.y;
  const auto fullControlMax = ImVec2(inputMin.x + a_width, inputMax.y);
  const auto arrowMin = ImVec2(fullControlMax.x - arrowAreaWidth, inputMin.y);
  const auto arrowMax = fullControlMax;
  const bool wholeControlHovered =
      ImGui::IsMouseHoveringRect(inputMin, fullControlMax, false);
  const bool arrowHovered =
      ImGui::IsMouseHoveringRect(arrowMin, arrowMax, false);
  if (arrowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    InputManager::GetSingleton()->Flush();
    ImGui::SetKeyboardFocusHere(-1);
    storage->SetBool(openStorageId, true);
    ImGui::OpenPopup(popupId.c_str());
  }
  bool dropdownOpen = storage->GetBool(openStorageId, false);

  if (dropdownOpen) {
    ImGui::SetNextWindowPos(
        ImVec2(inputMin.x, inputMax.y + ImGui::GetStyle().FramePadding.y));
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(a_width, 0.0f),
        ImVec2(a_width, ImGui::GetTextLineHeightWithSpacing() * 12.0f));
    ImGuiWindowFlags popupFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;
    bool popupVisible = false;
    bool popupHovered = false;
    if (ImGui::BeginPopupEx(popupImGuiId, popupFlags)) {
      popupVisible = true;
      ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
      popupHovered = ImGui::IsWindowHovered(
          ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

      const auto rawNeedle = sosr::strings::TrimText(a_buffer);
      const auto exactMatchIt = std::ranges::find_if(
          a_options, [&](const EditableDropdownOptionView &option) {
            if (IsSectionEntry(option)) {
              return false;
            }
            return sosr::strings::EqualsInsensitive(option.label, rawNeedle);
          });
      const int exactMatchIndex =
          exactMatchIt != a_options.end()
              ? static_cast<int>(std::distance(a_options.begin(), exactMatchIt))
              : -1;
      const auto needle = exactMatchIndex >= 0 ? std::string{} : rawNeedle;
      bool anyVisible = false;
      std::vector<int> visibleOptions;
      visibleOptions.reserve(a_options.size());
      for (std::size_t optionIndex = 0; optionIndex < a_options.size();
           ++optionIndex) {
        const auto &option = a_options[optionIndex];
        if (IsSectionEntry(option)) {
          continue;
        }
        if (!needle.empty() &&
            !sosr::strings::ContainsInsensitive(option.label, needle)) {
          continue;
        }
        visibleOptions.push_back(static_cast<int>(optionIndex));
      }

      const auto findVisibleOptionIndex = [&](const int a_optionIndex) -> int {
        if (a_optionIndex < 0) {
          return -1;
        }
        for (std::size_t visibleIndex = 0; visibleIndex < visibleOptions.size();
             ++visibleIndex) {
          if (visibleOptions[visibleIndex] == a_optionIndex) {
            return static_cast<int>(visibleIndex);
          }
        }
        return -1;
      };

      int highlightedIndex = storage->GetInt(highlightStorageId, -1);
      const int preferredHighlightIndex =
          exactMatchIndex >= 0
              ? findVisibleOptionIndex(exactMatchIndex)
              : (!visibleOptions.empty()
                     ? findVisibleOptionIndex(visibleOptions.front())
                     : -1);
      const int filterHash =
          static_cast<int>(ImHashStr(rawNeedle.c_str(), rawNeedle.size()));
      const int previousFilterHash = storage->GetInt(
          filterHashStorageId, (std::numeric_limits<int>::lowest)());
      const bool filterChanged = filterHash != previousFilterHash;
      storage->SetInt(filterHashStorageId, filterHash);
      if (visibleOptions.empty()) {
        highlightedIndex = -1;
      } else {
        if (filterChanged || highlightedIndex < 0 ||
            highlightedIndex >= static_cast<int>(visibleOptions.size())) {
          highlightedIndex =
              preferredHighlightIndex >= 0 ? preferredHighlightIndex : 0;
        }

        ImGui::SetKeyOwner(ImGuiKey_DownArrow, inputItemId,
                           ImGuiInputFlags_LockThisFrame);
        ImGui::SetKeyOwner(ImGuiKey_UpArrow, inputItemId,
                           ImGuiInputFlags_LockThisFrame);
        ImGui::SetKeyOwner(ImGuiKey_Enter, inputItemId,
                           ImGuiInputFlags_LockThisFrame);
        ImGui::SetKeyOwner(ImGuiKey_KeypadEnter, inputItemId,
                           ImGuiInputFlags_LockThisFrame);

        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_Repeat,
                                inputItemId)) {
          highlightedIndex =
              (highlightedIndex + 1) % static_cast<int>(visibleOptions.size());
        } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, ImGuiInputFlags_Repeat,
                                       inputItemId)) {
          highlightedIndex =
              (highlightedIndex + static_cast<int>(visibleOptions.size()) - 1) %
              static_cast<int>(visibleOptions.size());
        }
      }
      storage->SetInt(highlightStorageId, highlightedIndex);

      const auto commitOption = [&](const int a_optionIndex) {
        const auto &option = a_options[static_cast<std::size_t>(a_optionIndex)];
        std::snprintf(a_buffer, a_bufferSize, "%.*s",
                      static_cast<int>(option.label.size()),
                      option.label.data());
        if (a_selectedIndex) {
          *a_selectedIndex = a_optionIndex;
        }
        changed = true;
        dropdownOpen = false;
        ImGui::CloseCurrentPopup();
      };
      int visibleIndex = 0;
      for (std::size_t optionIndex = 0; optionIndex < a_options.size();
           ++optionIndex) {
        const auto &option = a_options[optionIndex];
        if (IsSectionEntry(option)) {
          if (anyVisible) {
            ImGui::Spacing();
          }
          ImGui::TextDisabled("%.*s",
                              static_cast<int>(GetSectionLabel(option).size()),
                              GetSectionLabel(option).data());
          if ((ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary) ||
               ImGui::IsItemHovered()) &&
              a_drawItemTooltip) {
            ImGui::BeginTooltip();
            (*a_drawItemTooltip)(static_cast<int>(optionIndex));
            ImGui::EndTooltip();
          }
          continue;
        }
        if (!needle.empty() &&
            !sosr::strings::ContainsInsensitive(option.label, needle)) {
          continue;
        }

        anyVisible = true;
        const bool selected = visibleIndex == highlightedIndex;
        if (ImGui::Selectable(std::string(option.label).c_str(), selected,
                              ImGuiSelectableFlags_SelectOnClick)) {
          commitOption(static_cast<int>(optionIndex));
          ImGui::SetKeyboardFocusHere(-1);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary) ||
            ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          ImGui::TextUnformatted(option.label.data(),
                                 option.label.data() + option.label.size());
          if (a_drawItemTooltip) {
            ImGui::Separator();
            (*a_drawItemTooltip)(static_cast<int>(optionIndex));
          }
          ImGui::EndTooltip();
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
          ImGui::SetScrollHereY();
        }
        ++visibleIndex;
      }

      const bool enterPressed =
          ImGui::IsKeyPressed(ImGuiKey_Enter, 0, inputItemId) ||
          ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, 0, inputItemId);
      if (a_allowCustomInput && enterPressed) {
        dropdownOpen = false;
        ImGui::CloseCurrentPopup();
      }
      if ((submitted || (acceptAutocompleteOnEnter && enterPressed)) &&
          acceptAutocompleteOnEnter && highlightedIndex >= 0 &&
          highlightedIndex < static_cast<int>(visibleOptions.size())) {
        commitOption(
            visibleOptions[static_cast<std::size_t>(highlightedIndex)]);
        ImGui::SetKeyboardFocusHere(-1);
      }

      if (!anyVisible) {
        ImGui::TextDisabled("%s",
                            sosr::ui::Localization::GetSingleton()
                                ->GetCStr("common.no_matches"));
      }
      ImGui::EndPopup();
    }

    if (!popupVisible || (!inputTextActive && !popupHovered)) {
      dropdownOpen = false;
    }
  }

  auto *drawList = ImGui::GetWindowDrawList();
  const auto *theme = ThemeConfig::GetSingleton();
  const auto arrowFillColor =
      inputTextActive
          ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
          : (wholeControlHovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                                 : ImGui::GetColorU32(ImGuiCol_Button));
  drawList->AddRectFilled(
      arrowMin, arrowMax, arrowFillColor, ImGui::GetStyle().FrameRounding,
      ImDrawFlags_RoundCornersTopRight | ImDrawFlags_RoundCornersBottomRight);
  drawList->AddRect(inputMin, fullControlMax, theme->GetColorU32("BORDER"),
                    ImGui::GetStyle().FrameRounding);
  ui::input_widgets::DrawInputOutline(inputMin, fullControlMax,
                                      wholeControlHovered, inputTextActive,
                                      ImGui::GetStyle().FrameRounding);
  drawList->AddLine(ImVec2(arrowMin.x, inputMin.y + 1.0f),
                    ImVec2(arrowMin.x, inputMin.y + inputFrameHeight - 1.0f),
                    theme->GetColorU32("BORDER"));
  ImGui::RenderArrow(
      drawList,
      ImVec2(arrowMin.x + ((arrowAreaWidth - ImGui::GetFontSize()) * 0.5f),
             inputMin.y + ((inputFrameHeight - ImGui::GetFontSize()) * 0.5f)),
      theme->GetColorU32(
          inputTextActive || wholeControlHovered ? "TEXT" : "TEXT_DISABLED"),
      ImGuiDir_Down);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  ImGui::SameLine(0.0f, 0.0f);
  ImGui::Dummy(ImVec2(arrowAreaWidth, inputFrameHeight));
  ImGui::PopStyleVar();

  storage->SetBool(openStorageId, dropdownOpen);
  ImGui::PopID();
  if (!a_allowCustomInput) {
    const auto exactMatch = std::ranges::find_if(
        a_options, [&](const EditableDropdownOptionView &option) {
          return !option.isSection &&
                 sosr::strings::EqualsInsensitive(option.label, a_buffer);
        });
    if (exactMatch == a_options.end() && !inputTextActive && !dropdownOpen) {
      if (a_fallbackIndex >= 0 &&
          a_fallbackIndex < static_cast<int>(a_options.size()) &&
          !a_options[static_cast<std::size_t>(a_fallbackIndex)].isSection) {
        const auto &fallbackOption =
            a_options[static_cast<std::size_t>(a_fallbackIndex)];
        std::snprintf(a_buffer, a_bufferSize, "%.*s",
                      static_cast<int>(fallbackOption.label.size()),
                      fallbackOption.label.data());
      } else if (a_selectedIndex && *a_selectedIndex >= 0 &&
                 *a_selectedIndex < static_cast<int>(a_options.size()) &&
                 !a_options[static_cast<std::size_t>(*a_selectedIndex)]
                      .isSection) {
        const auto &selectedOption =
            a_options[static_cast<std::size_t>(*a_selectedIndex)];
        std::snprintf(a_buffer, a_bufferSize, "%.*s",
                      static_cast<int>(selectedOption.label.size()),
                      selectedOption.label.data());
      } else if (const auto *topOption =
                     FindTopAutocompleteOption(a_options, a_buffer);
                 topOption) {
        std::snprintf(a_buffer, a_bufferSize, "%.*s",
                      static_cast<int>(topOption->label.size()),
                      topOption->label.data());
      } else if (const auto firstOptionIt = std::ranges::find_if(
                     a_options,
                     [](const EditableDropdownOptionView &option) {
                       return !option.isSection;
                     });
                 firstOptionIt != a_options.end()) {
        std::snprintf(a_buffer, a_bufferSize, "%.*s",
                      static_cast<int>(firstOptionIt->label.size()),
                      firstOptionIt->label.data());
      } else if (a_bufferSize > 0) {
        a_buffer[0] = '\0';
      }
    }
  }

  return changed;
}
} // namespace detail

bool DrawEditableStringDropdown(
    const char *a_label, const char *a_hint, char *a_buffer,
    const std::size_t a_bufferSize,
    std::span<const EditableDropdownItem<std::string>> a_items,
    const float a_width, int *a_selectedIndex,
    std::optional<std::string> *a_selectedValue, const int a_fallbackIndex) {
  std::vector<EditableDropdownOptionView> optionViews;
  optionViews.reserve(a_items.size());
  for (const auto &item : a_items) {
    optionViews.push_back(item.AsView());
  }

  int selectedIndex = a_selectedIndex ? *a_selectedIndex : -1;
  const bool changed = detail::DrawEditableDropdownIndexed(
      a_label, a_hint, a_buffer, a_bufferSize, optionViews, a_width,
      &selectedIndex, true, a_fallbackIndex);

  if (a_selectedIndex) {
    *a_selectedIndex = selectedIndex;
  }
  if (a_selectedValue) {
    if (selectedIndex >= 0 &&
        selectedIndex < static_cast<int>(a_items.size()) &&
        a_items[static_cast<std::size_t>(selectedIndex)].value.has_value()) {
      *a_selectedValue = a_items[static_cast<std::size_t>(selectedIndex)].value;
    } else {
      a_selectedValue->reset();
    }
  }

  return changed;
}
} // namespace sosr::ui::components
