// SPDX-License-Identifier: MIT
//
// Going Commando Magneboots: Going Commando's Gravity Boots in place of Ratchet & Clank's
// Magneboots (gravity_boots.hpp says what that is and how).
//
// Five hook points:
//   rac1.player.update_motion_input     after his state machine and movement, the ground probe
//                                       says whether the surface holds him next tick
//   rac1.player.enter_move_state        each state change, flag hidden, the magnet walk and hop
//                                       asked for as the run and the jump (his state machine asks
//                                       for them on a magnetic surface: walk, X), and the boots
//                                       kept on (BootsKeptOn)
//   rac1.player.resolve_gravity_mode    the surface's gravity while a magnetic surface holds him
//                                       (Going Commando's 2.5, not 0.3), on any ground tilted past
//                                       45 degrees he walks on while held (as the magnet walk), and
//                                       on through a jump or a step off its edge until he lands
//                                       (SurfaceHold)
//   rac1.player.update_equipped_items   his items, flag hidden: no lock on weapons or quick select
//   rac1.player.set_planar_velocity     the edge assist (edge_assist.hpp, setting "edge_assist"):
//                                       his run's velocity on a magnetic surface, just set, turned
//                                       along an edge ahead before his movement and collision use it
// enter_move_state, resolve_gravity_mode and update_equipped_items need mod interface 1.22 or
// later; a game without them refuses the mod with the reason. The edge assist's collision rays
// (collision_line) need 1.17.
#include <cstdint>

#include "openrac/games/rac1.hpp"
#include "openrac/mod.hpp"

#include "edge_assist.hpp"
#include "gravity_boots.hpp"
#include "layout.hpp"

namespace {

namespace mb = magneboots;
using Hidden = mb::MagnetFlagHidden<openrac::Memory>;

struct State {
    openrac::Mod* mod = nullptr;
    mb::Disc disc{};
    // The magnetic surface's gravity from one update to the next (gravity_boots.hpp).
    mb::SurfaceHold hold;
    // The edge assist, and its setting (read again when the launcher changes it).
    mb::EdgeAssist assist;
    bool edge_assist = true;
};

State& state() {
    static State s;
    return s;
}

openrac::Memory memory() { return state().mod->memory(); }

// The ground probe of the update that just ran: what is under him and how far.
mb::Probe probe(const openrac::Memory& m, std::uint32_t hero) {
    const auto feet = m.read<std::uint32_t>(hero + mb::kFeetModel);
    const bool boots = feet != 0 && m.read<std::int16_t>(feet + mb::kMobyClass) == mb::kMagnebootsClass;
    return {
        m.read<std::int16_t>(hero + mb::kGroundType),
        m.read<float>(hero + mb::kGroundDistance),
        boots,
        m.read<std::uint8_t>(hero + mb::kVehicleMode) != 0,
    };
}

mb::Vec3 read_normal(const openrac::Memory& m, std::uint32_t hero) {
    return {m.read<float>(hero + mb::kGroundNormal), m.read<float>(hero + mb::kGroundNormal + 4),
            m.read<float>(hero + mb::kGroundNormal + 8)};
}

mb::Vec3 read_vec3(const openrac::Memory& m, std::uint32_t address) {
    return {m.read<float>(address), m.read<float>(address + 4), m.read<float>(address + 8)};
}

// The game's collision on a line, the level's faces and the mobys' (a moving magnetic platform is
// one), his own moby passed over.
struct GameRay {
    const openrac::Mod& mod;
    int ignore;

    mb::LineHit operator()(const mb::Vec3& from, const mb::Vec3& to) const {
        const float a[3] = {from.x, from.y, from.z};
        const float b[3] = {to.x, to.y, to.z};
        openrac_line_hit out{};
        if (!mod.collision_line(a, b, out, OPENRAC_LINE_LEVEL | OPENRAC_LINE_MOBYS, ignore) || out.hit == 0) {
            return {};
        }
        return {true, out.surface, {out.normal[0], out.normal[1], out.normal[2]}};
    }
};

// After his state's movement has set his velocity along the ground (Hero_SetPlanarVelocity): on a
// magnetic surface, running, the edge assist may turn it along an edge ahead.
void assist_at_edges() {
    State& s = state();
    const openrac::Memory m = memory();
    const std::uint32_t hero = s.disc.hero;
    const std::int32_t hero_state = m.read<std::int32_t>(hero + mb::kHeroState);
    mb::AssistInput in;
    in.enabled = s.edge_assist && s.mod->has_collision_line();
    // Magnetic ground under him, not just tilted ground the hold goes on over (keeps_tilted).
    in.on_magnetic = s.hold.magnetic();
    in.grounded = m.read<std::int16_t>(hero + mb::kAirFrames) == 0 && m.read<std::uint8_t>(hero + mb::kVehicleMode) == 0;
    in.running = hero_state == mb::kRun || hero_state == mb::kRunToo;
    if (!in.enabled || !in.on_magnetic || !in.grounded || !in.running) {
        s.assist.reset();
        return;
    }
    in.position = read_vec3(m, hero + mb::kPosition);
    in.normal = s.hold.normal();
    in.velocity = read_vec3(m, hero + mb::kVelocity);
    in.stick_x = m.read<float>(hero + mb::kStick);
    in.stick_y = m.read<float>(hero + mb::kStick + 4);
    GameRay ray{*s.mod, s.mod->moby_slot_of(m.read<std::uint32_t>(hero + mb::kHeroMoby))};
    mb::Vec3 velocity;
    if (s.assist.step(in, ray, velocity)) {
        m.write(hero + mb::kVelocity, velocity.x);
        m.write(hero + mb::kVelocity + 4, velocity.y);
        m.write(hero + mb::kVelocity + 8, velocity.z);
    }
}

void load(openrac::Mod& mod) {
    if (mod.game() != "rac1") {
        throw openrac::Error("this mod is for Ratchet & Clank");
    }
    const bool ntsc = mod.version() == "ntsc";
    if (!ntsc && mod.version() != "pal") {
        throw openrac::Error("this mod knows the PAL and NTSC-U discs only");
    }
    state().mod = &mod;
    state().disc = mb::disc_for(ntsc);
    namespace rac1 = openrac::rac1;

    // Not hidden here: his collision reads the flag too, to let him walk from the floor up a
    // magnetic wall (clip_hero_horizontal_velocity, clip_hero_velocity_against_probed_surfaces),
    // and hidden it stops him at the corner. His state machine's magnet walk and hop come through
    // enter_move_state, where they are turned into the run and the jump.
    mod.hook<rac1::Hero_UpdateMotionInput>([](auto original) {
        original();
        const openrac::Memory m = memory();
        const std::uint32_t hero = state().disc.hero;
        state().hold.update(probe(m, hero), m.read<std::int16_t>(hero + mb::kAirFrames), read_normal(m, hero));
    });

    mod.hook<rac1::Hero_EnterMoveState>([](auto original, std::int32_t asked, std::int32_t force) -> std::int32_t {
        const openrac::Memory m = memory();
        const std::uint32_t hero = state().disc.hero;
        if (mb::already_there(asked, m.read<std::int32_t>(hero + mb::kHeroState))) {
            return 0;
        }
        // Made first, so it sees the flag as it really is, and gone last.
        const mb::BootsKeptOn<openrac::Memory> boots(m, hero);
        const Hidden hidden(m, hero);
        return original(mb::plain_ground_state(asked), force);
    });

    // Its gravity pulls against the ground normal, which the probe clears when it finds nothing
    // (off an edge, high in a jump): the surface's, remembered, stands in.
    mod.hook<rac1::Hero_ResolveGravityMode>([](auto original) -> std::int32_t {
        const std::int32_t mode = original();
        if (mode != 0 || !state().hold.active()) {
            return mode;
        }
        if (!state().hold.found()) {
            const openrac::Memory m = memory();
            const std::uint32_t hero = state().disc.hero;
            const mb::Vec3& n = state().hold.normal();
            m.write(hero + mb::kGroundNormal, n.x);
            m.write(hero + mb::kGroundNormal + 4, n.y);
            m.write(hero + mb::kGroundNormal + 8, n.z);
        }
        return 1;
    });

    mod.hook<rac1::Hero_UpdateEquippedItems>([](auto original) {
        const Hidden hidden(memory(), state().disc.hero);
        original();
    });

    // Set by the run's movement (hero_update_movement, every level's copy of it) before the run
    // clips it, adds gravity and his collision moves him by it, all in the same tick.
    mod.hook<rac1::Hero_SetPlanarVelocity>([](auto original, float angle) {
        original(angle);
        assist_at_edges();
    });

    state().edge_assist = mod.setting<bool>("edge_assist");
    if (mod.has_settings_set()) {
        mod.on_settings([] { state().edge_assist = state().mod->setting<bool>("edge_assist"); });
    }

    mod.info(state().edge_assist ? "Gravity Boots in place of the Magneboots, with the edge assist"
                                 : "Gravity Boots in place of the Magneboots");
}

}  // namespace

OPENRAC_MOD_CPP("gc-magneboots", "1.0.3", load)
