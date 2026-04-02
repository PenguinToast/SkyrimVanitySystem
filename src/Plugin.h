#pragma once

#ifndef SVS_VERSION_MAJOR
#error "SVS_VERSION_MAJOR must be provided by the build system"
#endif

#ifndef SVS_VERSION_MINOR
#error "SVS_VERSION_MINOR must be provided by the build system"
#endif

#ifndef SVS_VERSION_PATCH
#error "SVS_VERSION_PATCH must be provided by the build system"
#endif

#ifndef SVS_VERSION_STRING
#error "SVS_VERSION_STRING must be provided by the build system"
#endif

namespace Plugin {
inline constexpr auto NAME = "Skyrim Vanity System"sv;
inline constexpr REL::Version VERSION{SVS_VERSION_MAJOR, SVS_VERSION_MINOR,
                                      SVS_VERSION_PATCH, 0};
inline constexpr std::string_view VERSION_STRING{SVS_VERSION_STRING};
inline constexpr auto AUTHOR = "PenguinToast"sv;
} // namespace Plugin
