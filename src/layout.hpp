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
// Hero.unk308: 1 while the Magneboots hold him to a magnetic surface under his feet (set by
// update_hero_ground_material_effects after his update each tick: magnetic ground within 0.3,
// and the boots on). The state machine reads it to put him in the magnet walk, to turn X into a
// hop and to lock his items; resolve_hero_gravity_mode reads it for surface gravity.
constexpr std::uint32_t kMagnetFlag = 0x308u;
// Hero.ground_type: the collision type the ground probe found under him this tick (2 magnetic,
// -1 none); update_hero_ground_material_effects reads and clears it after his update.
constexpr std::uint32_t kGroundType = 0x12E0u;
// Hero.ground_distance: from him to the probe's ground point, along his gravity.
constexpr std::uint32_t kGroundDistance = 0x2DCu;
// Hero.ground_normal: the probed ground's normal, which surface gravity pulls against. The probe
// clears it (x and y) before it looks, so with nothing found it holds no direction.
constexpr std::uint32_t kGroundNormal = 0x270u;
// Hero.air_frames: frames off the ground, 0 on landing.
constexpr std::uint32_t kAirFrames = 0x30Eu;
// Hero.motion.pos: where he stands (his feet; ground_distance is from it).
constexpr std::uint32_t kPosition = 0x80u;
// Hero.motion.velocity: his velocity, in units a tick. Hero_SetPlanarVelocity sets it from his
// ground speed (along his facing in the surface's frame on a magnetic surface); his state's movement
// then clips it and adds gravity, and his collision moves him by it.
constexpr std::uint32_t kVelocity = 0xE0u;
// Hero.stick_x, stick_y: the stick (the d-pad when the stick is idle).
constexpr std::uint32_t kStick = 0x1D20u;
// Hero.moby: his moby (its slot passed over by the assist's collision rays).
constexpr std::uint32_t kHeroMoby = 0x2080u;
// Hero.vehicle_mode: nonzero in a vehicle, as Clank, on a board.
constexpr std::uint32_t kVehicleMode = 0x20A4u;
// Hero.items[1].moby: what he wears on his feet (the Magneboots: moby class 0xAD).
constexpr std::uint32_t kFeetModel = 0x1090u + 1u * 0x50u;
// Moby.oclass.
constexpr std::uint32_t kMobyClass = 0xA6u;
constexpr std::int16_t kMagnebootsClass = 0xAD;
// Hero.items[1].item_id: the item on his feet (the Magneboots: 0x1C, forced in on a magnetic
// surface, with what was there before kept in saved_item[1]).
constexpr std::uint32_t kFeetItem = 0x1090u + 1u * 0x50u + 0x28u;
constexpr std::int32_t kMagnebootsItem = 0x1C;
// Hero.restore_item[1]: 1 asks the item code to put saved_item[1] back on his feet. hero_set_state
// (enter_move_state) asks for it when the Magneboots are on, the flag reads 0 and another foot item
// waits in saved_item[1].
constexpr std::uint32_t kRestoreFeetItem = 0x210Cu + 1u * 4u;

// Collision type of a magnetic surface.
constexpr std::int16_t kMagneticGround = 2;

// Hero states (games/rac1/decomp/include/enums/hero_state.h).
constexpr std::int32_t kRun = 0x2;
// Unnamed; hero_update_movement moves him in it as in the run (the same case).
constexpr std::int32_t kRunToo = 0x73;
constexpr std::int32_t kJump = 0x7;
// The Magneboots' own: the magnet walk, its wrench swing and its hop.
constexpr std::int32_t kMagnetWalk = 0x3F;
constexpr std::int32_t kMagnetWrench = 0x70;
constexpr std::int32_t kMagnetHop = 0x71;

struct Disc {
    // g_hero.
    std::uint32_t hero;
};

inline Disc disc_for(bool ntsc) {
    if (ntsc) {
        return {0x0013F350u};
    }
    return {0x0013F450u};
}

}  // namespace magneboots
