#pragma once

#include <RE/C/CommandTable.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sosr::conditions {
[[nodiscard]] bool HasParamEnumOptions(RE::SCRIPT_PARAM_TYPE a_type);
[[nodiscard]] std::vector<std::string>
BuildParamEnumOptionLabels(RE::SCRIPT_PARAM_TYPE a_type);
[[nodiscard]] std::optional<std::int32_t>
ParseParamEnumOption(RE::SCRIPT_PARAM_TYPE a_type, std::string_view a_token);
[[nodiscard]] bool HasParamTextOptions(RE::SCRIPT_PARAM_TYPE a_type);
[[nodiscard]] std::vector<std::string>
BuildParamTextOptionLabels(RE::SCRIPT_PARAM_TYPE a_type);
[[nodiscard]] std::optional<std::int32_t>
ParseParamTextOption(RE::SCRIPT_PARAM_TYPE a_type, std::string_view a_token);
} // namespace sosr::conditions
