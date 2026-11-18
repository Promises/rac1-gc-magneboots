// SPDX-License-Identifier: MIT
//
// Where Ratchet & Clank keeps what this mod reads and writes, on both discs. The hero's field
// offsets are the same on PAL and NTSC; the bases differ (NTSC is not a flat 0x100 below PAL).
//   games/rac1/decomp/include/structs/hero.h (PAL), games/rac1/ntsc-decomp/include/rnc/hero/hero.h
#pragma once

#include <cstdint>

namespace magneboots {

// Hero.state.current: the hero's state.
constexpr std::uint32_t kHeroState = 0x2084u;
// Hero.target_speed: the ground speed (per frame) he eases toward; the stick handler writes it.
constexpr std::uint32_t kTargetSpeed = 0x190u;
// Hero.stick_magnitude: how far the stick is pushed, 0 to 1.
constexpr std::uint32_t kStickMagnitude = 0x229Cu;

// The state for walking on a magnetic surface with the Magneboots (hero_water_and_ground_motion.c,
// case 0x3F; HERO_STATE_DIRECTION_LIMITED_GROUND_MOVE in the NTSC decomp).
constexpr std::int32_t kMagnetWalk = 0x3F;

// hero_speed_steps: 16-byte rows {lower, upper, target_speed, step_up}. On foot,
// update_hero_target_speed_from_stick picks row 0's target (walk) for a stick-derived target below
// row 0's step_up, else row 1's (run), times the frame time.
constexpr std::uint32_t kWalkTarget = 0x08u;
constexpr std::uint32_t kWalkStepUp = 0x0Cu;
constexpr std::uint32_t kRunTarget = 0x18u;

struct Disc {
    // g_hero.
    std::uint32_t hero;
    // g_frame_time (the executable's): seconds a frame.
    std::uint32_t frame_time;
    // hero_speed_steps, level 0's address (each level program has its copy: relocate_level).
    std::uint32_t speed_steps;
};

inline Disc disc_for(bool ntsc) {
    if (ntsc) {
        return {0x0013F350u, 0x0015ED6Cu, 0x0017BDB8u};
    }
    return {0x0013F450u, 0x0015EE6Cu, 0x0017BEB8u};
}

}  // namespace magneboots
