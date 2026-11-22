// SPDX-License-Identifier: MIT
//
// The edge assist (edge_assist.hpp) in a made-up world of flat faces standing in for the game's
// collision.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "edge_assist.hpp"

namespace {

using namespace magneboots;

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

bool near(float a, float b, float within = 1e-4f) { return std::fabs(a - b) <= within; }

// A face across one axis (0 x, 1 y, 2 z) at `at`, between lo and hi on the other two (in order).
struct Face {
    int axis;
    float at;
    float lo[2];
    float hi[2];
    Vec3 normal;
    std::int32_t surface;
};

float coord(const Vec3& v, int axis) { return axis == 0 ? v.x : axis == 1 ? v.y : v.z; }

// The nearest face on the line, as the game's collision_line answers.
struct World {
    std::vector<Face> faces;
    int rays = 0;

    LineHit operator()(const Vec3& from, const Vec3& to) {
        ++rays;
        LineHit best;
        float best_t = 2.0f;
        for (const Face& f : faces) {
            const float a = coord(from, f.axis);
            const float b = coord(to, f.axis);
            if (a == b) {
                continue;
            }
            const float t = (f.at - a) / (b - a);
            if (t < 0.0f || t > 1.0f || t >= best_t) {
                continue;
            }
            const Vec3 p = from + (to - from) * t;
            bool inside = true;
            for (int k = 0, other = 0; k < 3; ++k) {
                if (k == f.axis) {
                    continue;
                }
                const float c = coord(p, k);
                inside = inside && c >= f.lo[other] && c <= f.hi[other];
                ++other;
            }
            if (inside) {
                best_t = t;
                best = {true, f.surface, f.normal};
            }
        }
        return best;
    }
};

constexpr Vec3 kUp{0.0f, 0.0f, 1.0f};

// A magnetic track 2 wide (x from -1 to 1) along y, over a pit.
World track() { return {{{2, 0.0f, {-1.0f, -100.0f}, {1.0f, 100.0f}, kUp, kMagneticGround}}}; }

AssistInput running(const Vec3& pos, const Vec3& velocity) {
    AssistInput in;
    in.enabled = true;
    in.on_magnetic = true;
    in.grounded = true;
    in.running = true;
    in.position = pos;
    in.normal = kUp;
    in.velocity = velocity;
    // The stick along the velocity.
    in.stick_x = velocity.x;
    in.stick_y = velocity.y;
    const float l = std::hypot(in.stick_x, in.stick_y);
    if (l > 0.0f) {
        in.stick_x /= l;
        in.stick_y /= l;
    }
    return in;
}

constexpr float kSpeed = 0.15f;  // about a run, in units a tick

void along_the_track_nothing_changes() {
    World w = track();
    EdgeAssist a;
    Vec3 out;
    check(!a.step(running({0.5f, 0.0f, 0.0f}, {0.0f, kSpeed, -0.01f}), w, out),
          "straight along the track: left alone");
}

void at_an_edge_he_is_turned_along_it() {
    World w = track();
    EdgeAssist a;
    Vec3 out;
    // 30 degrees off the track's line, toward its edge at x = 1, 0.4 from it: 0.45 out ahead.
    const float s = std::sin(30.0f * kDegrees), c = std::cos(30.0f * kDegrees);
    const Vec3 v{kSpeed * s, kSpeed * c, -0.01f};
    check(a.step(running({0.6f, 0.0f, 0.0f}, v), w, out), "at the edge: turned");
    const float planar = std::hypot(out.x, out.y);
    // Turned 15 degrees (the nearest that keeps ground ahead): toward the track's line.
    check(near(planar, kSpeed * std::cos(15.0f * kDegrees)), "losing only the outward part: |v| cos 15");
    check(out.x < v.x && out.x > 0.0f && out.y > 0.0f, "along the edge, still forward");
    check(near(std::atan2(out.x, out.y), 15.0f * kDegrees, 1e-3f), "heading 15 degrees off the line");
    check(near(out.z, -0.01f), "the part along the normal kept");
    check(!a.released(), "not let go");
}

void on_a_wall_track_too() {
    // The track on a wall facing +x: x = 0, y from -1 to 1, along z.
    World w{{{0, 0.0f, {-1.0f, -100.0f}, {1.0f, 100.0f}, {1.0f, 0.0f, 0.0f}, kMagneticGround}}};
    EdgeAssist a;
    AssistInput in = running({0.0f, 0.6f, 0.0f}, {0.0f, kSpeed * 0.5f, kSpeed * 0.8660254f});
    in.normal = {1.0f, 0.0f, 0.0f};
    Vec3 out;
    check(a.step(in, w, out), "on a wall: turned at its edge");
    check(out.y < in.velocity.y && out.z > 0.0f, "on a wall: along the edge");
}

void pushing_straight_out_lets_him_go() {
    World w = track();
    EdgeAssist a;
    Vec3 out;
    const AssistInput in = running({0.6f, 0.0f, 0.0f}, {kSpeed, 0.0f, 0.0f});
    int held = 0;
    for (int tick = 0; tick < kReleaseTicks + 5; ++tick) {
        if (a.step(in, w, out)) {
            ++held;
        } else {
            break;
        }
    }
    check(held == kReleaseTicks - 1, "held for kReleaseTicks - 1 ticks, let go on the next");
    check(a.released(), "let go");
    check(!a.step(in, w, out), "let go: still pushing out, left alone");
    check(!a.step(in, w, out), "and the next tick");
}

void a_turned_stick_brings_it_back() {
    World w = track();
    EdgeAssist a;
    Vec3 out;
    const AssistInput out_over = running({0.6f, 0.0f, 0.0f}, {kSpeed, 0.0f, 0.0f});
    for (int tick = 0; tick < kReleaseTicks; ++tick) {
        a.step(out_over, w, out);
    }
    check(a.released(), "let go");
    const float s = std::sin(30.0f * kDegrees), c = std::cos(30.0f * kDegrees);
    AssistInput along = running({0.6f, 0.0f, 0.0f}, {kSpeed * s, kSpeed * c, 0.0f});
    check(a.step(along, w, out), "the stick turned away from the edge: assisted again");
    check(!a.released(), "back");
}

void clear_ground_brings_it_back() {
    World w = track();
    EdgeAssist a;
    Vec3 out;
    AssistInput in = running({0.6f, 0.0f, 0.0f}, {kSpeed, 0.0f, 0.0f});
    for (int tick = 0; tick < kReleaseTicks; ++tick) {
        a.step(in, w, out);
    }
    // Back in the middle, the same stick, the way ahead clear.
    in.position = {-0.9f, 0.0f, 0.0f};
    for (int tick = 0; tick < kRearmClearTicks; ++tick) {
        a.step(in, w, out);
    }
    check(!a.released(), "clear ground for a while: back");
}

void landing_brings_it_back() {
    World w = track();
    EdgeAssist a;
    Vec3 out;
    AssistInput in = running({0.6f, 0.0f, 0.0f}, {kSpeed, 0.0f, 0.0f});
    for (int tick = 0; tick < kReleaseTicks; ++tick) {
        a.step(in, w, out);
    }
    AssistInput air = in;
    air.grounded = false;
    a.step(air, w, out);
    check(!a.released(), "off the ground: back for the landing");
}

void jumps_and_the_setting_are_left_alone() {
    World w = track();
    EdgeAssist a;
    Vec3 out;
    AssistInput in = running({0.6f, 0.0f, 0.0f}, {kSpeed, 0.0f, 0.1f});
    in.grounded = false;
    check(!a.step(in, w, out), "in the air: left alone");
    check(w.rays == 0, "in the air: no rays");
    in = running({0.6f, 0.0f, 0.0f}, {kSpeed, 0.0f, 0.0f});
    in.running = false;
    check(!a.step(in, w, out), "not running (a jump, a hurt): left alone");
    in = running({0.6f, 0.0f, 0.0f}, {kSpeed, 0.0f, 0.0f});
    in.enabled = false;
    check(!a.step(in, w, out), "setting off: left alone");
    in = running({0.6f, 0.0f, 0.0f}, {kSpeed, 0.0f, 0.0f});
    in.on_magnetic = false;
    check(!a.step(in, w, out), "plain ground: left alone");
}

void a_wall_ahead_is_the_games() {
    World w = track();
    // A wall across the track's end at y = 1.
    w.faces.push_back({1, 1.0f, {-1.0f, -1.0f}, {1.0f, 5.0f}, {0.0f, -1.0f, 0.0f}, 0});
    w.faces[0].hi[1] = 1.0f;
    EdgeAssist a;
    Vec3 out;
    check(!a.step(running({0.0f, 0.6f, 0.0f}, {0.0f, kSpeed, 0.0f}), w, out), "a wall ahead: left to the game");
}

void plain_floor_beside_the_track_is_ground() {
    World w = track();
    // Plain floor (collision type 0) beside the track, level with it: x from 1 to 10.
    w.faces.push_back({2, 0.0f, {1.0f, -100.0f}, {10.0f, 100.0f}, kUp, 0});
    EdgeAssist a;
    Vec3 out;
    check(!a.step(running({0.6f, 0.0f, 0.0f}, {kSpeed, 0.0f, 0.0f}), w, out),
          "off the track onto plain floor: left alone");
}

void a_plain_wall_beside_a_wall_track_is_ground() {
    // A magnetic strip up a plain wall (both facing +x): the hold goes on over the wall
    // (keeps_tilted), so it is no edge.
    World w{{{0, 0.0f, {-1.0f, -100.0f}, {1.0f, 100.0f}, {1.0f, 0.0f, 0.0f}, kMagneticGround},
             {0, -0.01f, {-100.0f, -100.0f}, {100.0f, 100.0f}, {1.0f, 0.0f, 0.0f}, 0}}};
    EdgeAssist a;
    AssistInput in = running({0.0f, 0.6f, 0.0f}, {0.0f, kSpeed, 0.0f});
    in.normal = {1.0f, 0.0f, 0.0f};
    Vec3 out;
    check(!a.step(in, w, out), "a plain wall beside a wall track: ground, left alone");
}

void the_edge_wrapping_over_is_ground() {
    // A slab: magnetic top (z = 0, x up to 1) and a magnetic side (x = 1, facing +x, down to -2).
    World w = track();
    w.faces.push_back({0, 1.0f, {-100.0f, -2.0f}, {100.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, kMagneticGround});
    EdgeAssist a;
    Vec3 out;
    check(!a.step(running({0.6f, 0.0f, 0.0f}, {kSpeed, 0.0f, 0.0f}), w, out),
          "magnetic over the edge: left alone");
}

void nowhere_to_go_slows_him() {
    // A small magnetic pad: 0.5 a side around him, nothing ahead in any direction.
    World w{{{2, 0.0f, {-0.25f, -0.25f}, {0.25f, 0.25f}, kUp, kMagneticGround}}};
    EdgeAssist a;
    Vec3 out;
    check(a.step(running({0.0f, 0.0f, 0.0f}, {kSpeed, 0.0f, -0.01f}), w, out), "a pad: slowed");
    check(near(out.x, kSpeed * kBlockedSpeedScale) && near(out.y, 0.0f) && near(out.z, -0.01f),
          "slowed by kBlockedSpeedScale, the normal part kept");
}

void slow_walking_is_covered() {
    World w = track();
    EdgeAssist a;
    Vec3 out;
    // A walk 0.25 from the edge: 6 ticks of it are 0.06, the look ahead kMinLookAhead.
    check(a.step(running({0.75f, 0.0f, 0.0f}, {0.01f, 0.0f, 0.0f}), w, out), "a slow walk at the edge: turned");
}

}  // namespace

int main() {
    along_the_track_nothing_changes();
    at_an_edge_he_is_turned_along_it();
    on_a_wall_track_too();
    pushing_straight_out_lets_him_go();
    a_turned_stick_brings_it_back();
    clear_ground_brings_it_back();
    landing_brings_it_back();
    jumps_and_the_setting_are_left_alone();
    a_wall_ahead_is_the_games();
    plain_floor_beside_the_track_is_ground();
    a_plain_wall_beside_a_wall_track_is_ground();
    the_edge_wrapping_over_is_ground();
    nowhere_to_go_slows_him();
    slow_walking_is_covered();
    if (failures == 0) {
        std::printf("edge_assist: all passed\n");
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
