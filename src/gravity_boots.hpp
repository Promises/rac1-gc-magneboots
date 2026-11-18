// SPDX-License-Identifier: MIT
//
// Going Commando's Gravity Boots in place of Ratchet & Clank's Magneboots.
//
// Going Commando has no state of its own for magnetic surfaces: Ratchet walks, runs, jumps,
// strafes and fights on them as on any ground, and only his gravity changes, along the surface's
// normal, held while a magnetic surface is within 2.5 units under him (in the air too, so a jump
// comes back down onto the wall). Ratchet & Clank instead puts him in a slow magnet walk (state
// 0x3F), turns X into a hop (0x71), locks his items, and lets go of the surface 0.3 units off it.
//
// What decides all of that is one flag, the hero's "held by the Magneboots" (Hero.unk308). His
// state machine asks for the magnet walk and the hop by it; the mod turns those into the run and
// the jump as they are entered, with the flag hidden from the state change (MagnetFlagHidden), and
// hides it from his item update. His collision keeps it: it lets him walk from the floor up a
// magnetic wall. His gravity mode gets Going Commando's rule (holds).
//
// Memory is anything with read<T>(address) and write(address, value), openrac::Memory in the game
// and a plain buffer in the tests.
#pragma once

#include <cmath>
#include <cstdint>

#include "layout.hpp"

namespace magneboots {

// How far under him a magnetic surface still holds him (Going Commando's).
constexpr float kHoldDistance = 2.5f;
// Off a magnetic surface (a jump, a double jump, a step off its edge) its gravity stays, as in
// Going Commando's jumps on one (its states 59, 109, 110 hold gravity whatever the distance), until
// he lands or this many frames have gone by; then the world's.
constexpr int kAirHoldFrames = 90;

// While alive, the hero's Magneboots flag reads 0; then it is put back, unless the game set it
// meanwhile. Nests: an inner one finds it hidden and changes nothing.
template <class Memory>
class MagnetFlagHidden {
public:
    MagnetFlagHidden(const Memory& memory, std::uint32_t hero)
        : m_memory(memory), m_flag(hero + kMagnetFlag), m_saved(memory.template read<std::int16_t>(m_flag)) {
        if (m_saved != 0) {
            memory.write(m_flag, std::int16_t{0});
        }
    }

    ~MagnetFlagHidden() {
        if (m_saved != 0 && m_memory.template read<std::int16_t>(m_flag) == 0) {
            m_memory.write(m_flag, m_saved);
        }
    }

    MagnetFlagHidden(const MagnetFlagHidden&) = delete;
    MagnetFlagHidden& operator=(const MagnetFlagHidden&) = delete;

private:
    const Memory& m_memory;
    std::uint32_t m_flag;
    std::int16_t m_saved;
};

// The state the game asked for, as Going Commando would have it: its own on plain ground.
inline std::int32_t plain_ground_state(std::int32_t state) {
    switch (state) {
    case kMagnetWalk:
        return kRun;
    case kMagnetHop:
        return kJump;
    default:
        return state;
    }
}

// Whether a request for `asked` is one to leave out: the magnet walk asked for while he already
// runs (the run's tick asks for it every tick on a magnetic surface), which entered again would
// restart his run.
inline bool already_there(std::int32_t asked, std::int32_t current) {
    return asked == kMagnetWalk && current == kRun;
}

struct Probe {
    // The collision type found under him (kMagneticGround for a magnetic surface).
    std::int16_t ground_type;
    // How far it is, along his gravity.
    float ground_distance;
    // The Magneboots on his feet.
    bool wearing_boots;
    // In a vehicle, as Clank, on a board: the boots do nothing.
    bool in_vehicle;
};

// Going Commando's rule: the surface's gravity while a magnetic surface is within kHoldDistance
// under him, in the air too.
inline bool holds(const Probe& p) {
    return p.wearing_boots && !p.in_vehicle && p.ground_type == kMagneticGround && p.ground_distance >= 0.0f &&
           p.ground_distance < kHoldDistance;
}

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    float length() const { return std::sqrt(x * x + y * y + z * z); }
};

// A normal the game can pull against: about unit length.
inline bool usable(const Vec3& n) {
    const float l = n.length();
    return std::isfinite(l) && l > 0.5f && l < 1.5f;
}

// The surface's gravity, from one update to the next: on a magnetic surface within kHoldDistance,
// and on, with that surface's normal, through the air until he lands elsewhere or kAirHoldFrames.
class SurfaceHold {
public:
    // After his update: what the probe found, his frames in the air and the probe's normal.
    void update(const Probe& probe, int air_frames, const Vec3& normal) {
        m_found = holds(probe);
        if (m_found) {
            m_active = true;
            if (usable(normal)) {
                m_normal = normal;
            }
            return;
        }
        if (m_active && air_frames > 0 && air_frames <= kAirHoldFrames && !probe.in_vehicle) {
            return;  // off the surface, in the air: its gravity stays
        }
        m_active = false;
    }

    bool active() const { return m_active; }

    // Whether this update's probe found the surface itself; if not, the game's ground normal is
    // what is left of the probe's clearing (no direction, or a wrong one on a sloped wall), and
    // normal() stands in for it.
    bool found() const { return m_found; }

    // The normal to pull against when the probe did not find the surface.
    const Vec3& normal() const { return m_normal; }

private:
    bool m_active = false;
    bool m_found = false;
    Vec3 m_normal{0.0f, 0.0f, 1.0f};
};

}  // namespace magneboots
