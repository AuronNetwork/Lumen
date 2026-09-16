// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>

namespace lumen {
// Windows file version 1.26.51.1 corresponds to package 1.26.5101.0.
// Other builds must be audited before enabling their hooks.
constexpr bool supportedBedrock(std::uint32_t major, std::uint32_t minor,
                               std::uint32_t patch, std::uint32_t revision) {
    return major == 1 && minor == 26 && patch == 51 && revision == 1;
}
}
