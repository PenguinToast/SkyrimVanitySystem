#pragma once

#include "EquipmentCatalog.h"

#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string_view>

namespace sosr::catalog {
inline constexpr std::string_view kSvsKitMetadataKey = "SkyrimVanitySystem";

[[nodiscard]] nlohmann::json
SerializeKitLayout(const KitEntry::Layout &a_layout);
[[nodiscard]] std::optional<KitEntry::Layout>
ParseKitLayout(const nlohmann::json &a_json);
} // namespace sosr::catalog
