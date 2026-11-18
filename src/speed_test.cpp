// SPDX-License-Identifier: MIT
//
// The plain-ground target speed the magnet walk gets (speed.hpp).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include "speed.hpp"

namespace {

using namespace magneboots;

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

bool near(float a, float b) { return std::fabs(a - b) < 1e-6f; }

constexpr float kFrame = 1.0f / 60.0f;
constexpr SpeedSteps kTable{2.0f, 0.6f, 5.7f};

void centred_stick_keeps_the_games() {
    check(!ground_target(kTable, 0.0f, kFrame), "centred: none");
    check(!ground_target(kTable, -0.1f, kFrame), "negative: none");
    check(!ground_target(kTable, std::numeric_limits<float>::quiet_NaN(), kFrame), "NaN: none");
}

void walks_below_the_step_and_runs_from_it() {
    check(near(*ground_target(kTable, 0.3f, kFrame), 2.0f * kFrame), "small push walks");
    check(near(*ground_target(kTable, 0.6f, kFrame), 5.7f * kFrame), "at the step runs");
    check(near(*ground_target(kTable, 1.0f, kFrame), 5.7f * kFrame), "full push runs");
}

void a_bad_frame_time_keeps_the_games() {
    check(!ground_target(kTable, 1.0f, 0.0f), "zero frame time: none");
}

void only_a_speed_table_is_used() {
    check(plausible(kTable), "the table");
    check(!plausible({0.0f, 0.6f, 5.7f}), "no walk speed");
    check(!plausible({6.0f, 0.6f, 5.7f}), "walk above run");
    check(!plausible({2.0f, 0.6f, 1e9f}), "huge run");
    check(!plausible({std::numeric_limits<float>::quiet_NaN(), 0.6f, 5.7f}), "NaN");
}

}  // namespace

int main() {
    centred_stick_keeps_the_games();
    walks_below_the_step_and_runs_from_it();
    a_bad_frame_time_keeps_the_games();
    only_a_speed_table_is_used();
    if (failures == 0) {
        std::printf("speed: all passed\n");
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
