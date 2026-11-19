// SPDX-License-Identifier: MIT
//
// Going Commando Magneboots: Going Commando's Gravity Boots in place of Ratchet & Clank's
// Magneboots (gravity_boots.hpp says what that is and how).
//
// Four hook points:
//   rac1.player.update_motion_input     after his state machine and movement, the ground probe
//                                       says whether the surface holds him next tick
//   rac1.player.enter_move_state        each state change, flag hidden, the magnet walk and hop
//                                       asked for as the run and the jump (his state machine asks
//                                       for them on a magnetic surface: walk, X)
//   rac1.player.resolve_gravity_mode    the surface's gravity while a magnetic surface holds him
//                                       (Going Commando's 2.5, not 0.3), and on through a jump or a
//                                       step off its edge until he lands (SurfaceHold)
//   rac1.player.update_equipped_items   his items, flag hidden: no lock on weapons or quick select
// The last three are new in OpenRAC (branch rac1-magneboot-hooks); a game without them refuses
// the mod with the reason.
#include <cstdint>

#include "openrac/games/rac1.hpp"
#include "openrac/mod.hpp"

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

    mod.info("Gravity Boots in place of the Magneboots");
}

}  // namespace

OPENRAC_MOD_CPP("gc-magneboots", "1.0.1", load)
