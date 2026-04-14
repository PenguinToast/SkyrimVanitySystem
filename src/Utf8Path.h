#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace sosr::utf8 {
inline std::u8string ToU8String(std::string_view a_text) {
  std::u8string result;
  result.reserve(a_text.size());
  for (const auto character : a_text) {
    result.push_back(static_cast<char8_t>(character));
  }
  return result;
}

inline std::string ToString(std::u8string_view a_text) {
  std::string result;
  result.reserve(a_text.size());
  for (const auto character : a_text) {
    result.push_back(static_cast<char>(character));
  }
  return result;
}

inline std::filesystem::path PathFromUtf8(std::string_view a_text) {
  return std::filesystem::path(ToU8String(a_text));
}

inline std::string PathToUtf8String(const std::filesystem::path &a_path) {
  return ToString(a_path.u8string());
}

inline std::string PathToUtf8GenericString(const std::filesystem::path &a_path) {
  return ToString(a_path.generic_u8string());
}
} // namespace sosr::utf8
