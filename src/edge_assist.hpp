// SPDX-License-Identifier: MIT
//
// The edge assist: a hidden help on narrow magnetic tracks.
//
// With the Gravity Boots' run (gravity_boots.hpp) in place of the slow magnet walk, a narrow
// magnetic track is easy to run off: neither game stops its run at an edge. Each tick he runs on
// magnetic ground, the assist looks a few ticks ahead along his velocity with the game's own
// collision ray. Where the ground runs out there, it turns his velocity along the edge (the nearest
// direction that still has ground under it, losing only the outward part of his speed) or, with
// nowhere to go, slows him for that tick. Pushed nearly straight out for a moment, it lets go and
// he walks off. It never touches a jump: only his run on the ground, with a magnetic surface found
// under him.
//
// What it changes is the velocity the run's movement has just set (rac1.player.set_planar_velocity,
// before the run's own clipping, gravity and collision move him by it), so the game's collision has
// the last word. The look ahead is a callable, Ray: ray(from, to) returns the LineHit the game's
// collision finds on that line (openrac::Mod::collision_line in the game, a made-up world in the
// tests).
#pragma once

#include <cmath>
#include <cstdint>
#include <initializer_list>

#include "gravity_boots.hpp"
#include "layout.hpp"

namespace magneboots {

// How far ahead he looks: his velocity times this many ticks, and at least kMinLookAhead units
// (so a slow walk is covered too).
constexpr float kLookAheadTicks = 6.0f;
constexpr float kMinLookAhead = 0.4f;
// The ground under the point ahead: a ray from this far above the surface's plane to this far
// below it.
constexpr float kGroundProbeAbove = 0.5f;
constexpr float kGroundProbeBelow = 1.0f;
// A wall or a ramp ahead: a ray this high above the surface, from him to the point ahead. Anything
// on it is left to the game's own collision (a floor running up into a wall).
constexpr float kWallProbeHeight = 0.3f;
// The surface wrapping over a convex edge: a ray this deep under the surface's plane, from the
// point ahead back under him, finding a magnetic face (the edge's other side).
constexpr float kWrapProbeDepth = 0.3f;
// The directions tried at an edge: his own turned by this step, both ways, nearest first, up to
// kMaxTurnSteps steps (90 degrees).
constexpr float kTurnStepDegrees = 15.0f;
constexpr int kMaxTurnSteps = 6;
// With no direction that keeps ground under him: his speed along the surface times this, for the
// tick.
constexpr float kBlockedSpeedScale = 0.2f;
// A turn of this much or more (or no way at all) is the player pushing out over the edge...
constexpr float kPushOutDegrees = 60.0f;
// ...and this many ticks of it in a row (about 0.3 s at 60 a second) let him go.
constexpr int kReleaseTicks = 18;
// Let go, the assist is back once the way ahead has had ground under it for this many ticks in a
// row, the stick turns this far from where it pointed at the let go (or is let go of), or he
// leaves the ground or the magnetic surface.
constexpr int kRearmClearTicks = 6;
constexpr float kRearmStickDegrees = 45.0f;
constexpr float kStickDeadZone = 0.3f;
// Slower than this (units a tick) along the surface: nothing to assist.
constexpr float kMinAssistSpeed = 1e-4f;

constexpr float kDegrees = 3.14159265f / 180.0f;

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(const Vec3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// `v` (in the plane of unit `n`) turned by `angle` radians about `n`.
inline Vec3 turned_about(const Vec3& v, const Vec3& n, float angle) {
    return v * std::cos(angle) + cross(n, v) * std::sin(angle);
}

// What the game's collision found on a line.
struct LineHit {
    bool hit = false;
    // The face's collision type (kMagneticGround for a magnetic one), -1 for a creature's shape.
    std::int32_t surface = -1;
    // The face's normal.
    Vec3 normal{};
};

// Whether what the ray down at the point ahead found is ground he can go on to. Any face is: the
// magnetic surface; plain ground tilted past 45 degrees, which the hold goes on over while he walks
// on it (keeps_tilted); plain ground nearer upright, where the world's gravity holds him. A creature
// hides what is under it, so it is not the assist's to judge either.
inline bool ground_ahead(const LineHit& h) { return h.hit; }

// Whether going `dist` along unit `dir` (in the plane of the surface, unit normal `n`) from `pos`
// keeps ground under him, or meets something the game's collision deals with.
template <class Ray>
bool supported(Ray& ray, const Vec3& pos, const Vec3& n, const Vec3& dir, float dist) {
    const Vec3 ahead = pos + dir * dist;
    if (ground_ahead(ray(ahead + n * kGroundProbeAbove, ahead - n * kGroundProbeBelow))) {
        return true;
    }
    if (ray(pos + n * kWallProbeHeight, ahead + n * kWallProbeHeight).hit) {
        return true;
    }
    const LineHit wrap = ray(ahead - n * kWrapProbeDepth, pos - n * kWrapProbeDepth);
    return wrap.hit && wrap.surface == kMagneticGround;
}

// One tick of his, as the assist sees it.
struct AssistInput {
    // The setting.
    bool enabled = false;
    // The Gravity Boots found a magnetic surface under him (SurfaceHold::magnetic; not tilted plain
    // ground the hold goes on over).
    bool on_magnetic = false;
    // On the ground (no frames in the air) and running: no jump, fall or hurt.
    bool grounded = false;
    bool running = false;
    Vec3 position{};
    // The surface's normal (unit length).
    Vec3 normal{};
    // The velocity his run has just set (units a tick).
    Vec3 velocity{};
    // The stick (Hero.stick_x, stick_y).
    float stick_x = 0.0f;
    float stick_y = 0.0f;
};

// The assist from one tick to the next: the push out counted, and let go.
class EdgeAssist {
public:
    // The velocity to give him instead of `in.velocity`, or false to leave it as it is.
    template <class Ray>
    bool step(const AssistInput& in, Ray& ray, Vec3& out) {
        if (!in.enabled || !in.on_magnetic || !in.grounded || !in.running || !usable(in.normal)) {
            reset();
            return false;
        }
        const Vec3 n = in.normal * (1.0f / in.normal.length());
        const float along_normal = dot(in.velocity, n);
        const Vec3 planar = in.velocity - n * along_normal;
        const float speed = planar.length();
        if (!(speed > kMinAssistSpeed)) {
            return false;
        }
        const Vec3 dir = planar * (1.0f / speed);
        const float dist = std::fmax(speed * kLookAheadTicks, kMinLookAhead);
        const bool clear = supported(ray, in.position, n, dir, dist);

        if (m_released) {
            m_clear_ticks = clear ? m_clear_ticks + 1 : 0;
            if (m_clear_ticks >= kRearmClearTicks || stick_turned(in)) {
                m_released = false;
                m_push_ticks = 0;
            } else {
                return false;
            }
        }
        if (clear) {
            m_push_ticks = 0;
            return false;
        }

        // The nearest turn that keeps ground under him; at the same angle, the side of last time.
        for (int step = 1; step <= kMaxTurnSteps; ++step) {
            const float degrees = kTurnStepDegrees * static_cast<float>(step);
            for (const int side : {m_side, -m_side}) {
                const Vec3 turned = turned_about(dir, n, static_cast<float>(side) * degrees * kDegrees);
                if (supported(ray, in.position, n, turned, dist)) {
                    m_side = side;
                    if (pushed_out(degrees >= kPushOutDegrees, in)) {
                        return false;
                    }
                    out = turned * (speed * std::cos(degrees * kDegrees)) + n * along_normal;
                    return true;
                }
            }
        }
        if (pushed_out(true, in)) {
            return false;
        }
        out = planar * kBlockedSpeedScale + n * along_normal;
        return true;
    }

    void reset() {
        m_push_ticks = 0;
        m_clear_ticks = 0;
        m_released = false;
    }

    bool released() const { return m_released; }

private:
    static float stick_angle(const AssistInput& in) { return std::atan2(in.stick_y, in.stick_x); }
    static float stick_length(const AssistInput& in) { return std::hypot(in.stick_x, in.stick_y); }

    // Counts a tick pushed out over the edge (or not); true once that has gone on long enough to
    // let him go, this tick.
    bool pushed_out(bool hard, const AssistInput& in) {
        if (!hard) {
            m_push_ticks = 0;
            return false;
        }
        if (++m_push_ticks < kReleaseTicks) {
            return false;
        }
        m_released = true;
        m_clear_ticks = 0;
        m_release_stick = stick_angle(in);
        return true;
    }

    // Whether the stick has been let go of or turned well away from where it pointed at the let go.
    bool stick_turned(const AssistInput& in) const {
        if (stick_length(in) < kStickDeadZone) {
            return true;
        }
        const float diff = std::remainder(stick_angle(in) - m_release_stick, 2.0f * 3.14159265f);
        return std::fabs(diff) > kRearmStickDegrees * kDegrees;
    }

    int m_push_ticks = 0;
    int m_clear_ticks = 0;
    bool m_released = false;
    float m_release_stick = 0.0f;
    // The side (+1 or -1 about the normal) the last turn went to.
    int m_side = 1;
};

}  // namespace magneboots
