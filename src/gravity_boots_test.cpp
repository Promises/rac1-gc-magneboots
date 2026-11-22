// SPDX-License-Identifier: MIT
//
// The Gravity Boots' rules (gravity_boots.hpp) on a plain buffer standing in for game memory.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "gravity_boots.hpp"

namespace {

using namespace magneboots;

struct Buffer {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x4000);

    template <class T>
    T read(std::uint32_t address) const {
        T value;
        std::memcpy(&value, bytes.data() + address, sizeof value);
        return value;
    }

    template <class T>
    void write(std::uint32_t address, const T& value) const {
        std::memcpy(const_cast<std::uint8_t*>(bytes.data()) + address, &value, sizeof value);
    }
};

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

constexpr std::uint32_t kHero = 0x100u;

std::int16_t flag(const Buffer& b) { return b.read<std::int16_t>(kHero + kMagnetFlag); }

void the_flag_is_hidden_and_put_back() {
    Buffer b;
    b.write(kHero + kMagnetFlag, std::int16_t{1});
    {
        const MagnetFlagHidden<Buffer> hidden(b, kHero);
        check(flag(b) == 0, "hidden while alive");
        {
            const MagnetFlagHidden<Buffer> inner(b, kHero);
            check(flag(b) == 0, "nested: still hidden");
        }
        check(flag(b) == 0, "nested: the inner one puts nothing back");
    }
    check(flag(b) == 1, "put back");
}

void a_flag_the_game_sets_meanwhile_stays() {
    Buffer b;
    b.write(kHero + kMagnetFlag, std::int16_t{1});
    {
        const MagnetFlagHidden<Buffer> hidden(b, kHero);
        b.write(kHero + kMagnetFlag, std::int16_t{2});
    }
    check(flag(b) == 2, "the game's value kept");
}

void a_clear_flag_is_left_alone() {
    Buffer b;
    {
        const MagnetFlagHidden<Buffer> hidden(b, kHero);
    }
    check(flag(b) == 0, "clear stays clear");
}

void magnet_states_become_plain_ground_ones() {
    check(plain_ground_state(kMagnetWalk) == kRun, "magnet walk: run");
    check(plain_ground_state(kMagnetHop) == kJump, "hop: jump");
    check(plain_ground_state(kJump) == kJump, "others as asked");
    check(already_there(kMagnetWalk, kRun), "the run's tick asking for the walk: left out");
    check(!already_there(kMagnetWalk, 0), "from idle: entered (as the run)");
    check(!already_there(kRun, kRun), "a run asked for: the game's to decide");
}

void the_surface_holds_within_2_5() {
    const Probe on{kMagneticGround, 0.0f, true, false};
    check(holds(on), "standing on it");
    check(holds({kMagneticGround, 2.4f, true, false}), "2.4 above it (a jump)");
    check(!holds({kMagneticGround, 2.5f, true, false}), "2.5 above it: let go");
    check(!holds({1, 0.0f, true, false}), "plain ground");
    check(!holds({-1, 32.0f, true, false}), "nothing under him");
    check(!holds({kMagneticGround, 0.0f, false, false}), "no boots");
    check(!holds({kMagneticGround, 0.0f, true, true}), "in a vehicle");
    check(!holds({kMagneticGround, -1.0f, true, false}), "a bad distance");
}

void the_hold_lasts_through_a_jump_until_landing() {
    SurfaceHold h;
    const Vec3 wall{1.0f, 0.0f, 0.0f};
    h.update({kMagneticGround, 0.0f, true, false}, 0, wall);
    check(h.active() && h.found() && h.normal().x == 1.0f, "on the wall: held, its normal kept");
    h.update({-1, 32.0f, true, false}, 30, Vec3{});
    check(h.active() && !h.found(), "high in a double jump, nothing found: still held, normal stands in");
    check(h.normal().x == 1.0f, "the wall's normal still");
    h.update({kMagneticGround, 1.0f, true, false}, 40, wall);
    check(h.active(), "coming back down onto it");
    h.update({kMagneticGround, 0.0f, true, false}, 0, wall);
    check(h.active(), "landed on it");
}

void the_hold_ends_on_other_ground_or_after_a_while() {
    SurfaceHold h;
    h.update({kMagneticGround, 0.0f, true, false}, 0, Vec3{0, 0, 1});
    h.update({1, 0.0f, true, false}, 0, Vec3{0, 0, 1});
    check(!h.active(), "landed on plain ground: let go");
    h.update({kMagneticGround, 0.0f, true, false}, 0, Vec3{0, 0, 1});
    h.update({-1, 32.0f, true, false}, kAirHoldFrames + 1, Vec3{});
    check(!h.active(), "too long in the air: let go");
    h.update({kMagneticGround, 0.0f, true, false}, 0, Vec3{0, 0, 1});
    h.update({-1, 32.0f, true, true}, 5, Vec3{});
    check(!h.active(), "into a vehicle: let go");
}

void a_bad_normal_is_not_remembered() {
    SurfaceHold h;
    h.update({kMagneticGround, 0.0f, true, false}, 0, Vec3{0, 0, 1});
    h.update({kMagneticGround, 0.0f, true, false}, 0, Vec3{0, 0, 0});
    check(h.normal().z == 1.0f, "a zero normal: the last good one kept");
    check(!usable(Vec3{0, 0, 0}) && usable(Vec3{0, 1, 0}), "usable");
}

void the_hold_goes_on_over_tilted_plain_ground() {
    SurfaceHold h;
    const Vec3 wall{1.0f, 0.0f, 0.0f};
    h.update({kMagneticGround, 0.0f, true, false}, 0, wall);
    // Onto the underside of a plain pad (facing down), on the ground.
    const Vec3 down{0.0f, 0.1f, -0.995f};
    h.update({0, 0.0f, true, false}, 0, down);
    check(h.active() && h.found() && !h.magnetic(), "held on, upside down on plain ground: held");
    check(h.normal().z < -0.9f, "its normal taken");
    // Steep plain ground (60 degrees from upright).
    h.update({0, 0.0f, true, false}, 0, Vec3{0.866f, 0.0f, 0.5f});
    check(h.active() && h.found(), "60 degrees from upright: held");
    // Back near upright: the old rule, plain ground lets go.
    h.update({0, 0.0f, true, false}, 0, Vec3{0.5f, 0.0f, 0.866f});
    check(!h.active() && !h.found(), "30 degrees from upright, plain ground: let go");
}

void tilted_plain_ground_never_starts_a_hold() {
    SurfaceHold h;
    h.update({0, 0.0f, true, false}, 0, Vec3{0.0f, 0.0f, -1.0f});
    check(!h.active() && !h.found(), "never held: plain ground upside down starts nothing");
    h.update({kMagneticGround, 0.0f, true, false}, 0, Vec3{1.0f, 0.0f, 0.0f});
    h.update({0, 0.0f, true, true}, 0, Vec3{0.0f, 0.0f, -1.0f});
    check(!h.active(), "in a vehicle: let go");
    h.update({kMagneticGround, 0.0f, true, false}, 0, Vec3{1.0f, 0.0f, 0.0f});
    h.update({0, 0.0f, false, false}, 0, Vec3{0.0f, 0.0f, -1.0f});
    check(!h.active(), "boots off: let go");
}

}  // namespace

// What hero_set_state does with the feet slot (hero_idle_and_items.c): the Magneboots on, the flag
// reading 0 and another foot item waiting: the take-off is asked for.
void game_state_change(const Buffer& b) {
    if (b.read<std::int16_t>(kHero + kMagnetFlag) == 0 && b.read<std::int32_t>(kHero + kFeetItem) == kMagnebootsItem) {
        b.write(kHero + kRestoreFeetItem, std::int32_t{1});
    }
}

std::int32_t restore(const Buffer& b) { return b.read<std::int32_t>(kHero + kRestoreFeetItem); }

void the_boots_stay_on_through_a_state_change_on_the_surface() {
    Buffer b;
    b.write(kHero + kMagnetFlag, std::int16_t{1});
    b.write(kHero + kFeetItem, kMagnebootsItem);
    {
        const BootsKeptOn<Buffer> boots(b, kHero);
        const MagnetFlagHidden<Buffer> hidden(b, kHero);
        game_state_change(b);
        check(restore(b) == 1, "the game asks for the take-off with the flag hidden");
    }
    check(restore(b) == 0, "on the surface: the take-off is withdrawn");
    check(flag(b) == 1, "and the flag is back");
}

void off_the_surface_the_boots_come_off_as_ever() {
    Buffer b;
    b.write(kHero + kFeetItem, kMagnebootsItem);
    {
        const BootsKeptOn<Buffer> boots(b, kHero);
        const MagnetFlagHidden<Buffer> hidden(b, kHero);
        game_state_change(b);
    }
    check(restore(b) == 1, "off the surface: the game's take-off stands");
}

void a_take_off_asked_for_before_stands() {
    Buffer b;
    b.write(kHero + kMagnetFlag, std::int16_t{1});
    b.write(kHero + kFeetItem, kMagnebootsItem);
    b.write(kHero + kRestoreFeetItem, std::int32_t{1});
    {
        const BootsKeptOn<Buffer> boots(b, kHero);
        const MagnetFlagHidden<Buffer> hidden(b, kHero);
        game_state_change(b);
    }
    check(restore(b) == 1, "a request from before the state change is left alone");
}

void other_foot_items_are_left_alone() {
    Buffer b;
    b.write(kHero + kMagnetFlag, std::int16_t{1});
    b.write(kHero + kFeetItem, std::int32_t{0x1D});
    {
        const BootsKeptOn<Buffer> boots(b, kHero);
        b.write(kHero + kRestoreFeetItem, std::int32_t{1});  // the Grindboots' own take-off
    }
    check(restore(b) == 1, "the Grindboots' take-off stands");
}

int main() {
    the_flag_is_hidden_and_put_back();
    a_flag_the_game_sets_meanwhile_stays();
    a_clear_flag_is_left_alone();
    magnet_states_become_plain_ground_ones();
    the_surface_holds_within_2_5();
    the_hold_lasts_through_a_jump_until_landing();
    the_hold_ends_on_other_ground_or_after_a_while();
    a_bad_normal_is_not_remembered();
    the_hold_goes_on_over_tilted_plain_ground();
    tilted_plain_ground_never_starts_a_hold();
    the_boots_stay_on_through_a_state_change_on_the_surface();
    off_the_surface_the_boots_come_off_as_ever();
    a_take_off_asked_for_before_stands();
    other_foot_items_are_left_alone();
    if (failures == 0) {
        std::printf("gravity_boots: all passed\n");
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
