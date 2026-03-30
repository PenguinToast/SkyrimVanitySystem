#pragma once

#include "conditions/Definition.h"

#include <cstdint>
#include <vector>

namespace sosr::conditions {
struct Store {
  int nextConditionId{1};
  std::vector<Definition> definitions;
  std::uint64_t revision{0};
};
} // namespace sosr::conditions
