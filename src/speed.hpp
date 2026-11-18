// SPDX-License-Identifier: MIT
//
// The ground speed Ratchet aims for on a magnetic surface, as on any other ground: Going
// Commando's Gravity Boots walk and run like plain ground, where Ratchet & Clank's Magneboots have
// a slow walk of their own.
//
// The game's magnet walk (state 0x3F) sets its target speed to stick * 3.5 u/s (at least 2), and to
// nothing when the stick points more than 70 degrees from where Ratchet faces, so he stops to turn.
// On plain ground (update_hero_target_speed_from_stick) the target is the speed table's walk or
// run speed by how far the stick is pushed, whatever its direction.
#pragma once

#include <cmath>
#include <optional>

namespace magneboots {

struct SpeedSteps {
    // Row 0's target_speed (u/s): walking.
    float walk;
    // Row 0's step_up: a stick-derived target at or above it runs.
    float step_up;
    // Row 1's target_speed (u/s): running.
    float run;
};

// Whether the table read from the game looks like the speed table, and not a level's other data.
inline bool plausible(const SpeedSteps& s) {
    return std::isfinite(s.walk) && std::isfinite(s.step_up) && std::isfinite(s.run) && s.walk > 0.0f &&
           s.run >= s.walk && s.run < 50.0f && s.step_up > 0.0f && s.step_up < 50.0f;
}

// The plain-ground target speed (per frame) for a stick pushed `stick` (0 to 1), or none with the
// stick centred: then the game's own (slowing to a stop) stays.
inline std::optional<float> ground_target(const SpeedSteps& s, float stick, float frame_time) {
    if (!(stick > 0.0f) || !(frame_time > 0.0f)) {
        return std::nullopt;
    }
    return (stick < s.step_up ? s.walk : s.run) * frame_time;
}

}  // namespace magneboots
