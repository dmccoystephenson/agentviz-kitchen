// chef.h: a session as a head chef at the pass, under a heat lamp, with the
// rail in its brigade's colour running along its cooks' stations.
#pragma once

#include "person.h"

namespace kitchen {

inline float darkness(State s) { return s == State::Offline ? 0.35f : (s == State::Dim ? 0.6f : 1.0f); }

// The pass: a higher steel shelf with a heat lamp hanging over it; red and
// pulsing when the session waits on you.
inline void drawPass(Host& h, const Figure& f, float t) {
    const float x = f.x, passZ = f.z + kCounterZ, dark = darkness(f.state);
    const bool offline = f.state == State::Offline, waiting = f.state == State::Waiting;
    h.add(STRATA_PRIMITIVE_CUBE, x, 0.5f, passZ, 1.8f, 1.0f, 0.9f, scale(kSteelDark, dark));
    h.add(STRATA_PRIMITIVE_CUBE, x, 1.0f, passZ, 1.9f, 0.05f, 0.95f, scale(kSteel, dark));
    h.add(STRATA_PRIMITIVE_CUBE, x, 1.5f, passZ + 0.3f, 1.9f, 0.04f, 0.4f, scale(kSteel, dark));  // upper shelf
    h.wideLine(x, 2.6f, passZ, x, 2.1f, passZ, kSteelDark, 2.0f, 0.03f);
    const float pulse = agentviz::pulse(t, 0.8f);
    const Rgb lamp = waiting ? scale(kRed, 0.6f + 0.6f * pulse)
                     : offline ? Rgb{0.08f, 0.08f, 0.08f}
                     : f.state == State::Busy ? Rgb{1.0f, 0.55f, 0.15f} : Rgb{0.5f, 0.3f, 0.12f};
    h.add(STRATA_PRIMITIVE_CYLINDER, x, 2.05f, passZ, 0.7f, 0.08f, 0.5f, kSteelDark);
    h.add(STRATA_PRIMITIVE_SPHERE, x, 1.98f, passZ, 0.5f, 0.12f, 0.35f, lamp);
    if (offline) return;
    const float intensity = waiting ? 2.5f + 2.5f * pulse : (f.state == State::Busy ? 1.4f : 0.6f);
    h.light(x, 1.8f, passZ - 0.2f, waiting ? 7.0f : 4.5f, waiting ? kRed : Rgb{1.0f, 0.6f, 0.25f}, intensity,
            waiting ? 20 : 4);
}

inline void drawRail(Host& h, const Figure& f) {
    const float railEnd = f.stationX0 + (static_cast<float>(f.stations) - 0.5f) * f.stride;
    const Rgb rail = scale(f.accent, f.state == State::Offline ? 0.35f : 1.0f);
    h.wideLine(f.x - 0.9f, kRailY, f.z + kRailZ, railEnd, kRailY, f.z + kRailZ, rail, 4.0f, 0.06f, f.appear);
    h.add(STRATA_PRIMITIVE_CYLINDER, railEnd, (kRailY + 1.5f) * 0.5f, f.z + kRailZ + 0.3f, 0.06f,
          (kRailY - 1.5f) * 0.5f, 0.06f, kSteelDark);
}

// Nobody home: an apron hanging on the pass, the lamp off.
inline void drawEmptyPass(Host& h, const Figure& f) {
    const float x = f.x, passZ = f.z + kCounterZ;
    h.wideLine(x - 0.3f, 1.5f, passZ - 0.45f, x - 0.3f, 1.42f, passZ - 0.47f, kSteelDark, 2.0f, 0.02f);
    h.add(STRATA_PRIMITIVE_CUBE, x - 0.3f, 1.12f, passZ - 0.5f, 0.42f, 0.6f, 0.03f, scale(f.accent, 0.5f));
    h.add(STRATA_PRIMITIVE_CUBE, x + 0.25f, 1.2f, passZ - 0.5f, 0.3f, 0.35f, 0.03f, scale(kWhite, 0.4f));
}

/* ---------------------------------------------------------------- actions */

// Calling out: one arm up, tickets flying along the rail to every station.
inline void chefDelegate(Host& h, const Figure& f, float t, Pose& p) {
    p.right = {f.x + 0.35f, 2.0f + 0.15f * std::sin(t * 6.0f), f.z + 0.1f};
    for (int k = 0; k < std::max(f.stations, 1); ++k)
        flyingTicket(h, f.x + 0.3f, f.z + kRailZ, f.stationX0 + k * f.stride, f.z + kRailZ, frac(t * 0.5f + k * 0.37f));
}

// Tasting: a bowl on the pass, a spoon going to the mouth.
inline void chefThink(Host& h, const Figure& f, float t, Pose& p) {
    const float u = 0.5f - 0.5f * std::cos(t * 1.6f), bx = f.x + 0.4f, bz = f.z + kCounterZ - 0.2f;
    h.add(STRATA_PRIMITIVE_CYLINDER, bx, 1.08f, bz, 0.36f, 0.06f, 0.36f, kWhite);
    h.add(STRATA_PRIMITIVE_CYLINDER, bx, 1.12f, bz, 0.28f, 0.03f, 0.28f, {0.6f, 0.3f, 0.1f});
    p.right = {bx - 0.3f * u, 1.15f + 0.6f * u, bz - 0.35f * u};
    h.add(STRATA_PRIMITIVE_SPHERE, p.right.x, p.right.y + 0.08f, p.right.z, 0.1f, 0.04f, 0.1f, kSteel);
    for (int k = 0; k < 3; ++k) {
        const float v = frac(t * 0.3f + k / 3.0f);
        h.point(f.x - 0.2f + 0.2f * k, 2.4f + v * 0.6f, f.z, 10.0f, 0.18f, {1.0f, 0.9f, 0.5f}, 0.7f * (1.0f - v));
    }
}

// At the stove: a pan on the pass burner, flames and steam.
inline void chefRun(Host& h, const Figure& f, float t, Pose& p) {
    const float px = f.x + 0.45f, pz = f.z + kCounterZ - 0.15f;
    h.add(STRATA_PRIMITIVE_CYLINDER, px, 1.14f, pz, 0.55f, 0.1f, 0.55f, kSteelDark);
    burnerFlames(h, px, 1.04f, pz, 0.25f, t);
    steam(h, px, 1.25f, pz, t, f.phase, {0.9f, 0.9f, 0.9f}, 6, 1.1f, 0.5f);
    p.right = {px + 0.15f * std::cos(t * 4.0f), 1.3f, pz - 0.15f + 0.1f * std::sin(t * 4.0f)};
    h.light(px, 1.3f, pz, 3.0f, kFlame, 0.8f + 0.3f * std::sin(t * 13.0f), 2);
}

inline void chefEdit(Host& h, const Figure& f, float t, Pose& p) {
    const float bz = f.z + kCounterZ - 0.2f;
    h.add(STRATA_PRIMITIVE_CUBE, f.x, 1.05f, bz, 0.75f, 0.05f, 0.45f, kWood);
    p.right = {p.right.x, 1.12f + 0.18f * std::fabs(std::sin(t * 8.0f)), bz};
    h.add(STRATA_PRIMITIVE_CUBE, p.right.x - 0.12f, p.right.y - 0.05f, bz, 0.22f, 0.08f, 0.02f, kSteel);
    p.left = {p.left.x, 1.1f, bz};
}

inline void chefRead(Host& h, const Figure& f, float t, Pose& p) {
    const float bob = 0.04f * std::sin(t * 1.5f);
    p.left = {f.x - 0.15f, 1.55f + bob, f.z + 0.45f};
    p.right = {f.x + 0.15f, 1.55f + bob, f.z + 0.45f};
    h.add(STRATA_PRIMITIVE_CUBE, f.x, 1.6f + bob, f.z + 0.5f, 0.32f, 0.42f, 0.02f, kPaper);
}

// web and other: a cloth swept along the pass.
inline void chefWipe(Host& h, const Figure& f, float t, Pose& p) {
    p.right = {f.x + 0.5f * std::sin(t * 3.0f), 1.06f, f.z + kCounterZ - 0.3f};
    h.add(STRATA_PRIMITIVE_CUBE, p.right.x, 1.04f, p.right.z, 0.3f, 0.03f, 0.22f, {0.3f, 0.5f, 0.8f});
}

inline void chefAction(Host& h, const Figure& f, float t, Pose& p) {
    switch (f.state == State::Busy ? f.action : Action::None) {
        case Action::Delegate: chefDelegate(h, f, t, p); break;
        case Action::Think: chefThink(h, f, t, p); break;
        case Action::Run: chefRun(h, f, t, p); break;
        case Action::Edit: chefEdit(h, f, t, p); break;
        case Action::Read: chefRead(h, f, t, p); break;
        case Action::Web:
        case Action::Other: chefWipe(h, f, t, p); break;
        default: break;
    }
}

/* ---------------------------------------------------------------- waiting */

// A plate up under the lamp, waiting for your sign-off.
inline void platedUp(Host& h, float x, float passZ) {
    h.add(STRATA_PRIMITIVE_CYLINDER, x - 0.3f, 1.05f, passZ - 0.1f, 0.6f, 0.02f, 0.6f, {1.0f, 1.0f, 0.97f});
    h.add(STRATA_PRIMITIVE_SPHERE, x - 0.3f, 1.12f, passZ - 0.1f, 0.3f, 0.14f, 0.3f, {0.85f, 0.35f, 0.1f});
    h.add(STRATA_PRIMITIVE_SPHERE, x - 0.18f, 1.17f, passZ - 0.05f, 0.1f, 0.1f, 0.1f, {0.2f, 0.6f, 0.15f});
}

// A ticket held high with a red "?" on both faces: the chef has a question.
inline void questionTicket(Host& h, const Figure& f, float t, Pose& p) {
    const float bob = 0.05f * std::sin(t * 3.0f);
    const float cx = f.x - 0.45f, cy = 2.25f + bob, cz = f.z + 0.2f;
    p.left = {cx, cy - 0.28f, cz};
    h.add(STRATA_PRIMITIVE_CUBE, cx, cy, cz, 0.42f, 0.56f, 0.02f, kPaper);
    for (float side : {-0.02f, 0.02f}) {
        const float z = cz + side;
        for (int k = 0; k < 7; ++k) {  // the hook: an arc from the left round to the lower right
            const float a0 = 3.4f - k * 0.62f, a1 = 3.4f - (k + 1) * 0.62f;
            h.wideLine(cx + std::cos(a0) * 0.1f, cy + 0.1f + std::sin(a0) * 0.1f, z, cx + std::cos(a1) * 0.1f,
                       cy + 0.1f + std::sin(a1) * 0.1f, z, kRed, 3.0f, 0.035f);
        }
        h.wideLine(cx + 0.02f, cy + 0.01f, z, cx, cy - 0.08f, z, kRed, 3.0f, 0.035f);
        h.add(STRATA_PRIMITIVE_SPHERE, cx, cy - 0.17f, z, 0.06f, 0.06f, 0.02f, kRed);
    }
}

// Order up: the service bell rung again and again, rings of sound, a red ring
// on the floor and a red glow rising. A plate for permission; a "?" ticket
// for a question.
inline void drawWaiting(Host& h, const Figure& f, float t, Pose& p) {
    const float x = f.x, passZ = f.z + kCounterZ, pulse = agentviz::pulse(t, 0.8f);
    if (f.reason == agentviz::WaitingReason::Question) questionTicket(h, f, t, p);
    else platedUp(h, x, passZ);
    const float strike = std::fabs(std::sin(t * 4.0f)), bx = x + 0.5f, bz = passZ - 0.3f;
    h.add(STRATA_PRIMITIVE_CYLINDER, bx, 1.05f, bz, 0.3f, 0.02f, 0.3f, kSteelDark);
    h.add(STRATA_PRIMITIVE_SPHERE, bx, 1.12f + 0.03f * strike, bz, 0.26f, 0.2f, 0.26f, kGold);
    h.add(STRATA_PRIMITIVE_SPHERE, bx, 1.24f + 0.03f * strike, bz, 0.05f, 0.06f, 0.05f, kGold);
    p.right = {bx, 1.3f + 0.25f * (1.0f - strike), bz};
    const float w = frac(t * 1.3f);
    ring(h, bx, 1.2f + w * 0.2f, bz, 0.2f + w * 0.6f, kGold, 1.0f - w, 2.0f);
    ring(h, x, 0.03f, f.z + 0.3f, 1.2f + 0.15f * pulse, kRed, 0.6f + 0.4f * pulse, 5.0f);
    for (int k = 0; k < 10; ++k) {
        const float v = frac(t * 0.35f + k * 0.1f);
        h.point(x + std::sin(k * 2.3f + t) * 0.3f, 2.2f + v * 3.0f, passZ, 26.0f, 0.6f, kRed, 0.55f * (1.0f - v));
    }
}

/* ---------------------------------------------------------------- the chef */

inline void drawChef(Host& h, const Figure& f, float seconds) {
    const float t = seconds + f.phase * 10.0f;
    drawPass(h, f, t);
    drawRail(h, f);
    if (f.state == State::Offline) {
        drawEmptyPass(h, f);
        highlightRing(h, f, 0.9f);
        return;
    }
    Pose p{{f.x - 0.35f, 0.95f, f.z + 0.25f}, {f.x + 0.35f, 0.95f, f.z + 0.25f}};
    chefAction(h, f, t, p);
    if (f.state == State::Waiting) drawWaiting(h, f, t, p);
    if (f.stalled) boilOver(h, f.x - 0.62f, 1.02f, f.z + kCounterZ - 0.1f, 1.0f, t);
    if (f.state == State::Idle || f.state == State::Dim) {
        p.left = {p.left.x, 0.85f, f.z + 0.05f};
        p.right = {p.right.x, 0.85f, f.z + 0.05f};
    }
    flare(h, f.x + 0.45f, 1.05f, f.z + kCounterZ - 0.15f, f.pulse);
    person(h, f.x, f.z, 1.12f, true, scale(f.accent, darkness(f.state)), scale(kWhite, darkness(f.state)), p);
    highlightRing(h, f, 0.9f);
}

}  // namespace kitchen
