#pragma once

#include "conditions/Definition.h"

#include <span>
#include <string>

namespace sosr::conditions {
[[nodiscard]] Color
PickDistinctConditionColor(std::span<const Color> a_existingColors);

[[nodiscard]] Definition BuildNewConditionTemplate(const std::string &a_name,
                                                  const Color &a_color);
} // namespace sosr::conditions
