#include "ui/components/EditableCombo.h"

#include "InputManager.h"
#include "StringUtils.h"
#include "imgui_internal.h"
#include "ui/ThemeConfig.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <string_view>

namespace {
constexpr std::string_view kSectionPrefix = "\x1fsection:";

bool IsSectionEntry(std::string_view a_option) {
  return a_option.size() >= kSectionPrefix.size() &&
         sosr::strings::EqualsInsensitive(
             a_option.substr(0, kSectionPrefix.size()), kSectionPrefix);
}

std::string_view GetSectionLabel(std::string_view a_option) {
  return IsSectionEntry(a_option) ? a_option.substr(kSectionPrefix.size())
                                  : a_option;
}

const std::string *
FindTopAutocompleteOption(const std::vector<std::string> &a_options,
                          std::string_view a_buffer) {
  const auto needle = sosr::strings::TrimText(a_buffer);
  for (const auto &option : a_options) {
    if (IsSectionEntry(option)) {
      continue;
    }
    if (!needle.empty() &&
        !sosr::strings::ContainsInsensitive(option, needle)) {
      continue;
    }
    return std::addressof(option);
  }
  return nullptr;
}

struct EditableDropdownAutocompleteData {
  const std::vector<std::string> *options{nullptr};
};

int EditableDropdownInputCallback(ImGuiInputTextCallbackData *a_data) {
  auto *userData =
      static_cast<EditableDropdownAutocompleteData *>(a_data->UserData);
  if (!userData || !userData->options ||
      a_data->EventFlag != ImGuiInputTextFlags_CallbackCompletion) {
    return 0;
  }

  const std::string_view buffer{a_data->Buf,
                                static_cast<std::size_t>(a_data->BufTextLen)};
  const auto *topOption = FindTopAutocompleteOption(*userData->options, buffer);
  if (!topOption) {
    return 0;
  }

  a_data->DeleteChars(0, a_data->BufTextLen);
  a_data->InsertChars(0, topOption->c_str());
  a_data->SelectAll();
  return 0;
}
} // namespace

namespace sosr::ui::components {
void DrawTextInputOutline(const ImVec2 &a_min, const ImVec2 &a_max,
                          const bool a_hovered, const bool a_active,
                          const float a_rounding) {
  if (!a_hovered && !a_active) {
    return;
  }

  auto *theme = ThemeConfig::GetSingleton();
  auto *drawList = ImGui::GetWindowDrawList();
  const auto rounding =
      a_rounding >= 0.0f ? a_rounding : ImGui::GetStyle().FrameRounding;
  const auto color = a_active ? theme->GetColorU32("PRIMARY")
                              : theme->GetColorU32("TABLE_HOVER", 0.75f);
  const auto thickness = a_active ? 2.0f : 1.5f;
  drawList->AddRect(a_min, a_max, color, rounding, 0, thickness);
}

bool DrawSearchableStringCombo(const char *a_label, const char *a_allLabel,
                               const std::vector<std::string> &a_options,
                               int &a_index, ImGuiTextFilter &a_filter) {
  const char *preview =
      a_index == 0 ? a_allLabel
                   : a_options[static_cast<std::size_t>(a_index - 1)].c_str();
  const float width = ImGui::CalcItemWidth();

  std::vector<std::string> options;
  options.reserve(a_options.size() + 1);
  options.emplace_back(a_allLabel);
  options.insert(options.end(), a_options.begin(), a_options.end());

  const std::string fallbackSelection = preview;
  std::string selectedOption;
  const bool changed = DrawEditableDropdown(
      a_label, preview, a_filter.InputBuf, IM_ARRAYSIZE(a_filter.InputBuf),
      options, width, &selectedOption, false, &fallbackSelection);

  if (!selectedOption.empty()) {
    std::snprintf(a_filter.InputBuf, IM_ARRAYSIZE(a_filter.InputBuf), "%s",
                  selectedOption.c_str());
  }

  if (a_filter.InputBuf[0] == '\0') {
    a_index = 0;
    a_filter.Build();
    return changed;
  }

  for (std::size_t index = 0; index < options.size(); ++index) {
    if (sosr::strings::CompareTextInsensitive(options[index],
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

bool DrawEditableDropdown(const char *a_label, const char *a_hint,
                          char *a_buffer, const std::size_t a_bufferSize,
                          const std::vector<std::string> &a_options,
                          const float a_width, std::string *a_selectedOption,
                          const bool a_allowCustomInput,
                          const std::string *a_fallbackSelection) {
  bool changed = false;
  const bool acceptAutocompleteOnEnter = !a_allowCustomInput;
  const auto popupId = std::string(a_label) + "##popup";
  const auto openId = std::string(a_label) + "##open";
  const auto highlightId = std::string(a_label) + "##highlight";
  const auto filterHashId = std::string(a_label) + "##filterhash";
  const auto arrowAreaWidth = ImGui::GetFrameHeight();
  const auto inputWidth = (std::max)(1.0f, a_width - arrowAreaWidth);
  EditableDropdownAutocompleteData autocompleteData{std::addressof(a_options)};
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
      const auto exactMatchIt =
          std::ranges::find_if(a_options, [&](const std::string &option) {
            if (IsSectionEntry(option)) {
              return false;
            }
            return sosr::strings::EqualsInsensitive(option, rawNeedle);
          });
      const std::string *exactMatch = exactMatchIt != a_options.end()
                                          ? std::addressof(*exactMatchIt)
                                          : nullptr;
      const auto needle = exactMatch ? std::string{} : rawNeedle;
      bool anyVisible = false;
      std::vector<const std::string *> visibleOptions;
      visibleOptions.reserve(a_options.size());
      for (const auto &option : a_options) {
        if (IsSectionEntry(option)) {
          continue;
        }
        if (!needle.empty() &&
            !sosr::strings::ContainsInsensitive(option, needle)) {
          continue;
        }
        visibleOptions.push_back(std::addressof(option));
      }

      const std::string *topOption =
          visibleOptions.empty()
              ? nullptr
              : (exactMatch ? exactMatch : visibleOptions.front());
      const auto findVisibleOptionIndex =
          [&](const std::string *a_option) -> int {
        if (!a_option) {
          return -1;
        }
        for (std::size_t visibleIndex = 0; visibleIndex < visibleOptions.size();
             ++visibleIndex) {
          if (visibleOptions[visibleIndex] == a_option) {
            return static_cast<int>(visibleIndex);
          }
        }
        return -1;
      };

      int highlightedIndex = storage->GetInt(highlightStorageId, -1);
      const int preferredHighlightIndex =
          exactMatch ? findVisibleOptionIndex(exactMatch)
                     : findVisibleOptionIndex(topOption);
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

      const auto commitOption = [&](const std::string &a_option) {
        std::snprintf(a_buffer, a_bufferSize, "%s", a_option.c_str());
        if (a_selectedOption) {
          *a_selectedOption = a_option;
        }
        changed = true;
        dropdownOpen = false;
        ImGui::CloseCurrentPopup();
      };
      int visibleIndex = 0;
      for (const auto &option : a_options) {
        if (IsSectionEntry(option)) {
          if (anyVisible) {
            ImGui::Spacing();
          }
          ImGui::TextDisabled("%.*s",
                              static_cast<int>(GetSectionLabel(option).size()),
                              GetSectionLabel(option).data());
          continue;
        }
        if (!needle.empty() &&
            !sosr::strings::ContainsInsensitive(option, needle)) {
          continue;
        }

        anyVisible = true;
        const bool selected = visibleIndex == highlightedIndex;
        if (ImGui::Selectable(option.c_str(), selected,
                              ImGuiSelectableFlags_SelectOnClick)) {
          commitOption(option);
          ImGui::SetKeyboardFocusHere(-1);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary) ||
            ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          ImGui::TextUnformatted(option.c_str());
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
            *visibleOptions[static_cast<std::size_t>(highlightedIndex)]);
        ImGui::SetKeyboardFocusHere(-1);
      }

      if (!anyVisible) {
        ImGui::TextDisabled("No matches");
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
  DrawTextInputOutline(inputMin, fullControlMax, wholeControlHovered,
                       inputTextActive, ImGui::GetStyle().FrameRounding);
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
    const auto exactMatch =
        std::ranges::find_if(a_options, [&](const std::string &option) {
          return sosr::strings::EqualsInsensitive(option, a_buffer);
        });
    if (exactMatch == a_options.end() && !inputTextActive && !dropdownOpen) {
      if (a_fallbackSelection && !a_fallbackSelection->empty()) {
        std::snprintf(a_buffer, a_bufferSize, "%s",
                      a_fallbackSelection->c_str());
      } else if (a_selectedOption && !a_selectedOption->empty()) {
        std::snprintf(a_buffer, a_bufferSize, "%s", a_selectedOption->c_str());
      } else if (const auto *topOption =
                     FindTopAutocompleteOption(a_options, a_buffer);
                 topOption) {
        std::snprintf(a_buffer, a_bufferSize, "%s", topOption->c_str());
      } else if (!a_options.empty()) {
        std::snprintf(a_buffer, a_bufferSize, "%s", a_options.front().c_str());
      } else if (a_bufferSize > 0) {
        a_buffer[0] = '\0';
      }
    }
  }

  return changed;
}

bool DrawSearchableDropdown(const char *a_label, const char *a_hint,
                            std::string &a_value,
                            const std::vector<std::string> &a_options,
                            const float a_width) {
  char buffer[128];
  std::snprintf(buffer, sizeof(buffer), "%s", a_value.c_str());
  const std::string fallbackSelection = a_value;
  std::string selectedOption;
  const bool changed =
      DrawEditableDropdown(a_label, a_hint, buffer, sizeof(buffer), a_options,
                           a_width, &selectedOption, false, &fallbackSelection);
  a_value = selectedOption.empty() ? buffer : selectedOption;
  return changed;
}
} // namespace sosr::ui::components
