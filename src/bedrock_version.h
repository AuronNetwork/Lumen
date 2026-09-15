// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>

namespace lumen {
// Windows file version 1.26.50.4 corresponds to package 1.26.5004.0.
// Other builds must be audited before enabling their hooks.
constexpr bool supportedBedrock(std::uint32_t major, std::uint32_t minor,
                               std::uint32_t patch, std::uint32_t revision) {
    return major == 1 && minor == 26 && patch == 50 && revision == 4;
}
}
