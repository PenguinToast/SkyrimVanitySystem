#include "conditions/Creation.h"

#include "conditions/Defaults.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

namespace {
using ConditionColor = sosr::conditions::Color;

float ComputeColorDistanceSq(const ConditionColor &a_left,
                             const ConditionColor &a_right) {
  const auto dr = a_left.x - a_right.x;
  const auto dg = a_left.y - a_right.y;
  const auto db = a_left.z - a_right.z;
  return (dr * dr) + (dg * dg) + (db * db);
}

ConditionColor HsvToRgb(const float a_hue, const float a_saturation,
                        const float a_value) {
  if (a_saturation <= 0.0f) {
    return {a_value, a_value, a_value, 1.0f};
  }

  const auto hue = a_hue - std::floor(a_hue);
  const auto scaledHue = hue * 6.0f;
  const auto sector = static_cast<int>(scaledHue);
  const auto fraction = scaledHue - static_cast<float>(sector);
  const auto p = a_value * (1.0f - a_saturation);
  const auto q = a_value * (1.0f - (a_saturation * fraction));
  const auto t = a_value * (1.0f - (a_saturation * (1.0f - fraction)));

  switch (sector % 6) {
  case 0:
    return {a_value, t, p, 1.0f};
  case 1:
    return {q, a_value, p, 1.0f};
  case 2:
    return {p, a_value, t, 1.0f};
  case 3:
    return {p, q, a_value, 1.0f};
  case 4:
    return {t, p, a_value, 1.0f};
  default:
    return {a_value, p, q, 1.0f};
  }
}
} // namespace

namespace sosr::conditions {
Color PickDistinctConditionColor(std::span<const Color> a_existingColors) {
  static const std::array<Color, 20> kPalette{
      Color{0.86f, 0.25f, 0.28f, 1.0f}, Color{0.17f, 0.62f, 0.32f, 1.0f},
      Color{0.19f, 0.48f, 0.85f, 1.0f}, Color{0.86f, 0.58f, 0.16f, 1.0f},
      Color{0.55f, 0.30f, 0.86f, 1.0f}, Color{0.10f, 0.67f, 0.67f, 1.0f},
      Color{0.84f, 0.35f, 0.63f, 1.0f}, Color{0.61f, 0.50f, 0.18f, 1.0f},
      Color{0.24f, 0.71f, 0.86f, 1.0f}, Color{0.93f, 0.40f, 0.13f, 1.0f},
      Color{0.78f, 0.22f, 0.19f, 1.0f}, Color{0.27f, 0.72f, 0.46f, 1.0f},
      Color{0.29f, 0.39f, 0.90f, 1.0f}, Color{0.78f, 0.67f, 0.18f, 1.0f},
      Color{0.67f, 0.28f, 0.76f, 1.0f}, Color{0.14f, 0.56f, 0.76f, 1.0f},
      Color{0.91f, 0.48f, 0.55f, 1.0f}, Color{0.37f, 0.64f, 0.21f, 1.0f},
      Color{0.48f, 0.57f, 0.86f, 1.0f}, Color{0.20f, 0.74f, 0.58f, 1.0f}};

  const auto scoreColor = [&](const Color &a_candidate) {
    float minDistanceSq = FLT_MAX;
    bool hasExisting = false;
    for (const auto &existing : a_existingColors) {
      minDistanceSq = (std::min)(
          minDistanceSq, ComputeColorDistanceSq(a_candidate, existing));
      hasExisting = true;
    }
    return hasExisting ? minDistanceSq : FLT_MAX;
  };

  Color bestColor = kPalette.front();
  float bestScore = -1.0f;
  for (const auto &candidate : kPalette) {
    const float score = scoreColor(candidate);
    if (score > bestScore) {
      bestScore = score;
      bestColor = candidate;
    }
  }

  for (int index = 0; index < 24; ++index) {
    const auto candidate = HsvToRgb(index / 24.0f, 0.70f, 0.88f);
    const float score = scoreColor(candidate);
    if (score > bestScore) {
      bestScore = score;
      bestColor = candidate;
    }
  }

  return bestColor;
}

Definition BuildNewConditionTemplate(const std::string &a_name,
                                     const Color &a_color) {
  Definition definition;
  definition.name = a_name;
  definition.color = a_color;
  definition.clauses.push_back(BuildDefaultPlayerClause());
  return definition;
}
} // namespace sosr::conditions
