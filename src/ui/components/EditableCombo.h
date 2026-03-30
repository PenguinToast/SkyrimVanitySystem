#pragma once

#include "imgui.h"

#include <cstdio>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sosr::ui::components {
struct EditableDropdownOptionView {
  std::string_view label;
  bool isSection{false};
};

template <class TValue> struct EditableDropdownItem {
  std::string label;
  std::optional<TValue> value;

  EditableDropdownOptionView AsView() const {
    return {.label = label, .isSection = !value.has_value()};
  }
};

namespace detail {
bool DrawEditableDropdownIndexed(const char *a_label, const char *a_hint,
                                 char *a_buffer, std::size_t a_bufferSize,
                                 std::span<const EditableDropdownOptionView> a_options,
                                 float a_width,
                                 int *a_selectedIndex = nullptr,
                                 bool a_allowCustomInput = true,
                                 int a_fallbackIndex = -1);
} // namespace detail

bool DrawEditableStringDropdown(
    const char *a_label, const char *a_hint, char *a_buffer,
    std::size_t a_bufferSize,
    std::span<const EditableDropdownItem<std::string>> a_items, float a_width,
    int *a_selectedIndex = nullptr,
    std::optional<std::string> *a_selectedValue = nullptr,
    int a_fallbackIndex = -1);

template <class TValue>
bool DrawSearchableDropdown(const char *a_label, const char *a_hint,
                            std::string &a_value,
                            std::span<const EditableDropdownItem<TValue>> a_items,
                            float a_width, int *a_selectedIndex = nullptr,
                            std::optional<TValue> *a_selectedValue = nullptr) {
  std::vector<EditableDropdownOptionView> optionViews;
  optionViews.reserve(a_items.size());
  for (const auto &item : a_items) {
    optionViews.push_back(item.AsView());
  }

  int selectedIndex = a_selectedIndex ? *a_selectedIndex : -1;
  char buffer[128];
  std::snprintf(buffer, sizeof(buffer), "%s", a_value.c_str());
  const bool changed = detail::DrawEditableDropdownIndexed(
      a_label, a_hint, buffer, sizeof(buffer), optionViews, a_width,
      &selectedIndex, false, selectedIndex);

  if (a_selectedIndex) {
    *a_selectedIndex = selectedIndex;
  }
  if (a_selectedValue) {
    if (selectedIndex >= 0 &&
        selectedIndex < static_cast<int>(a_items.size()) &&
        a_items[static_cast<std::size_t>(selectedIndex)].value.has_value()) {
      *a_selectedValue =
          a_items[static_cast<std::size_t>(selectedIndex)].value;
    } else {
      a_selectedValue->reset();
    }
  }
  if (selectedIndex >= 0 && selectedIndex < static_cast<int>(a_items.size())) {
    a_value = a_items[static_cast<std::size_t>(selectedIndex)].label;
  } else {
    a_value = buffer;
  }
  return changed;
}

bool DrawSearchableStringCombo(const char *a_label, const char *a_allLabel,
                               const std::vector<std::string> &a_options,
                               int &a_index, ImGuiTextFilter &a_filter);
} // namespace sosr::ui::components
