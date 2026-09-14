// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>

namespace lumen {
// Camera projection scales, not degrees. Limit invalid game data before writes.
inline bool validProjection(float x, float y) {
    return std::isfinite(x) && std::isfinite(y) && x > 0.001f && y > 0.001f && x < 10000.f && y < 10000.f;
}
inline float zoomScale(int factor) { return float(std::clamp(factor, 2, 20)); }
inline float gammaValue(float original, bool enabled) { return enabled ? 25.f : original; }
// Restore only values we still own; leave a fresh game-written value alone.
inline void restoreProjection(float& x, float& y, float originalX, float originalY, float writtenX, float writtenY) {
    if(x == writtenX) x = originalX;
    if(y == writtenY) y = originalY;
}
}
