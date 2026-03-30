#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sosr::conditions {
inline constexpr std::string_view kDefaultConditionId = "condition-1";

enum class Comparator : std::uint8_t {
  Equal,
  NotEqual,
  Greater,
  GreaterOrEqual,
  Less,
  LessOrEqual
};

enum class Connective : std::uint8_t { And, Or };

struct Clause {
  std::string functionName;
  std::string customConditionId;
  std::array<std::string, 2> arguments{};
  Comparator comparator{Comparator::Equal};
  std::string comparand{"1"};
  Connective connectiveToNext{Connective::And};
};

struct Color {
  float x{0.55f};
  float y{0.55f};
  float z{0.55f};
  float w{1.0f};
};

struct Definition {
  std::string id;
  std::string name;
  std::string description;
  Color color{};
  bool enabled{true};
  std::vector<Clause> clauses;
};
} // namespace sosr::conditions
