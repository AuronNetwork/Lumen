// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace lumen {
// Bedrock 26.51 MouseDevice event layout, adapted from Latite.
struct MouseAction {int16_t x,y,dx,dy;int8_t action,data;int pointerId;bool motionless;};
static_assert(sizeof(MouseAction)==20 && offsetof(MouseAction,pointerId)==12);
inline void consumeMouseButtons(std::vector<MouseAction>& inputs){
    // Action 0 carries motion and must reach the game's cursor handling.
    // Positive actions are buttons/wheel; suppress both press and release.
    std::erase_if(inputs,[](const MouseAction& event){return event.action>0;});
}
}
