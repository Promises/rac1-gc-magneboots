// SPDX-License-Identifier: MIT
//
// Going Commando Magneboots: on a magnetic surface Ratchet walks and runs at his plain-ground
// speeds, and no longer stops to turn when the stick points away from him (speed.hpp).
//
// The magnet walk (state 0x3F) works out its target speed, then eases his ground speed toward it
// through speed_step, which is the SDK's Hero_ApproachSpeed in every level (each level family's
// copy of the hero code calls the shared one). The hook replaces the target just before that, in
// that state only; the easing itself is the same as the plain run's (7.5 up, 8.5 down).
//
// Not here yet, because no hook point reaches them: a jump on the magnetic surface (the game
// re-enters the magnet walk every tick and turns X into a hop), the plain run's animation, and
// weapons (update_hero_equipped_items locks them and uses the items within the same function).
#include <cstdint>
#include <string>

#include "openrac/games/rac1.hpp"
#include "openrac/mod.hpp"

#include "layout.hpp"
#include "speed.hpp"

namespace {

struct State {
    openrac::Mod* mod = nullptr;
    magneboots::Disc disc{};
    // The speed table was reported (or found wrong) once; it is read each time, as the level moves it.
    bool reported = false;
};

State& state() {
    static State s;
    return s;
}

// In the magnet walk, its target speed becomes the plain-ground one.
void ground_speed_on_magnets() {
    State& s = state();
    const openrac::Memory memory = s.mod->memory();
    const std::uint32_t hero = s.disc.hero;
    if (memory.read<std::int32_t>(hero + magneboots::kHeroState) != magneboots::kMagnetWalk) {
        return;
    }
    const std::uint32_t steps = s.mod->relocate_level(0, s.disc.speed_steps);
    const magneboots::SpeedSteps table{
        memory.read<float>(steps + magneboots::kWalkTarget),
        memory.read<float>(steps + magneboots::kWalkStepUp),
        memory.read<float>(steps + magneboots::kRunTarget),
    };
    if (!magneboots::plausible(table)) {
        if (!s.reported) {
            s.reported = true;
            s.mod->warn("the hero's speed table does not look right here; the Magneboots keep their own speed");
        }
        return;
    }
    if (!s.reported) {
        s.reported = true;
        s.mod->info("speed table: walk " + std::to_string(table.walk) + ", run " + std::to_string(table.run) +
                    " (from a stick of " + std::to_string(table.step_up) + ")");
    }
    const float stick = memory.read<float>(hero + magneboots::kStickMagnitude);
    const float frame_time = memory.read<float>(s.mod->relocate(s.disc.frame_time));
    if (const auto target = magneboots::ground_target(table, stick, frame_time)) {
        memory.write(hero + magneboots::kTargetSpeed, *target);
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
    state().disc = magneboots::disc_for(ntsc);

    mod.hook<openrac::rac1::Hero_ApproachSpeed>([](auto original, float up, float down) {
        ground_speed_on_magnets();
        original(up, down);
    });
    mod.info("Magneboots walk and run at ground speed");
}

}  // namespace

OPENRAC_MOD_CPP("gc-magneboots", "0.1.0", load)
