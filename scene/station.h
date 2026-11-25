// station.h: a subagent as a line cook at its station, its lane's ticket
// clipped to the rail above.
#pragma once

#include "line.h"
#include "person.h"

namespace kitchen {

// The ticket on the rail above the station; it glows while the cook reads.
inline void railTicket(Host& h, float x, float z, float t, bool reading, float pulse) {
    const float sway = 0.05f * std::sin(t * 1.7f), hop = 0.12f * pulse;
    const float glow = reading ? 0.5f + 0.5f * std::sin(t * 4.0f) : 0.0f;
    h.add(STRATA_PRIMITIVE_CUBE, x + sway, kRailY - 0.24f + hop, z + kRailZ, 0.32f, 0.42f, 0.02f,
          mix(kPaper, {1.0f, 1.0f, 0.5f}, glow));
    h.add(STRATA_PRIMITIVE_CUBE, x + sway, kRailY - 0.03f, z + kRailZ, 0.1f, 0.06f, 0.05f, kSteelDark);
    if (reading) h.point(x, kRailY - 0.24f, z + kRailZ - 0.05f, 18.0f, 0.6f, {1.0f, 0.95f, 0.5f}, 0.5f);
}

/* ---------------------------------------------------------------- actions */
// Each poses the cook (p) and draws its props; cz is the working spot on the counter.

inline void cookEdit(Host& h, float x, float cz, float t, Pose& p) {
    const float chop = std::fabs(std::sin(t * 9.0f));
    h.add(STRATA_PRIMITIVE_CUBE, x - 0.1f, kCounterTop + 0.03f, cz, 0.75f, 0.05f, 0.45f, kWood);
    p.right = {x + 0.1f, 1.0f + 0.2f * chop, cz};
    p.left = {x - 0.3f, 0.98f, cz};
    h.add(STRATA_PRIMITIVE_CUBE, p.right.x - 0.13f, p.right.y - 0.06f, cz, 0.26f, 0.09f, 0.02f, kSteel);
    for (int k = 0; k < 5; ++k)
        h.add(STRATA_PRIMITIVE_SPHERE, x - 0.4f + k * 0.08f, kCounterTop + 0.08f, cz + 0.08f * (k % 2), 0.08f, 0.06f,
              0.08f, k % 2 ? Rgb{0.9f, 0.4f, 0.05f} : Rgb{0.2f, 0.6f, 0.1f});
    const float u = frac(t * 1.5f);  // a bit of veg flying off the board
    h.add(STRATA_PRIMITIVE_SPHERE, x + 0.05f + u * 0.2f, kCounterTop + 0.1f + std::sin(u * 3.14f) * 0.25f, cz, 0.06f,
          0.06f, 0.06f, {0.2f, 0.6f, 0.1f});
}

inline void cookRun(Host& h, float x, float z, float t, float phase, Pose& p) {
    const float px = x + 0.45f, pz = z + kCounterZ;
    h.add(STRATA_PRIMITIVE_CYLINDER, px, kCounterTop + 0.2f, pz, 0.5f, 0.18f, 0.5f, kSteel);
    burnerFlames(h, px, kCounterTop + 0.05f, pz, 0.22f, t);
    steam(h, px, kCounterTop + 0.4f, pz, t, phase, {0.92f, 0.92f, 0.92f}, 6, 1.1f, 0.45f);
    p.right = {px + 0.12f * std::cos(t * 5.0f), 1.3f, pz - 0.1f + 0.1f * std::sin(t * 5.0f)};
    h.wideLine(p.right.x, p.right.y, p.right.z, px, kCounterTop + 0.25f, pz, kWood, 2.0f, 0.03f);
    h.light(px, kCounterTop + 0.5f, pz, 2.5f, kFlame, 0.7f + 0.3f * std::sin(t * 11.0f), 1);
}

// Reading the ticket: a card held up beside the head, so it shows from behind too.
inline void cookRead(Host& h, float x, float z, float t, Pose& p) {
    const float bob = 0.04f * std::sin(t * 1.4f), rx = x + 0.42f;
    p.left = {rx - 0.14f, 1.55f + bob, z + 0.2f};
    p.right = {rx + 0.14f, 1.55f + bob, z + 0.2f};
    h.add(STRATA_PRIMITIVE_CUBE, rx, 1.62f + bob, z + 0.22f, 0.36f, 0.48f, 0.02f, kPaper);
    for (int k = 0; k < 4; ++k)
        h.add(STRATA_PRIMITIVE_CUBE, rx, 1.76f + bob - k * 0.09f, z + 0.2f, 0.24f, 0.025f, 0.01f, {0.2f, 0.2f, 0.25f});
}

// Fetching from the walk-in: a cold door behind the station, frost spilling
// out, crates coming over the counter.
inline void cookWeb(Host& h, float x, float z, float t, Pose& p) {
    const float dz = z + 1.75f;
    h.add(STRATA_PRIMITIVE_CUBE, x, 1.05f, dz, 1.0f, 2.1f, 0.12f, {0.45f, 0.62f, 0.78f});
    h.add(STRATA_PRIMITIVE_CUBE, x + 0.35f, 1.1f, dz - 0.08f, 0.06f, 0.4f, 0.06f, kSteel);
    for (int k = 0; k < 6; ++k) {
        const float v = frac(t * 0.4f + k / 6.0f);
        h.point(x - 0.4f + k * 0.16f, 0.2f + v * 0.5f, dz - 0.1f - v * 0.4f, 16.0f, 0.4f, {0.7f, 0.85f, 1.0f},
                0.6f * (1.0f - v));
    }
    const float u = frac(t * 0.6f);
    const float cy = 1.1f + std::sin(u * 3.14159f) * 0.6f, crateZ = dz - 0.2f - u * 1.4f;
    h.add(STRATA_PRIMITIVE_CUBE, x, cy, crateZ, 0.36f, 0.24f, 0.28f, kWood);
    h.add(STRATA_PRIMITIVE_SPHERE, x - 0.06f, cy + 0.14f, crateZ, 0.12f, 0.12f, 0.12f, {0.8f, 0.1f, 0.1f});
    h.add(STRATA_PRIMITIVE_SPHERE, x + 0.08f, cy + 0.14f, crateZ, 0.12f, 0.12f, 0.12f, {0.95f, 0.75f, 0.1f});
    p.left = {x - 0.2f, 1.2f, z + 0.45f};
    p.right = {x + 0.2f, 1.2f, z + 0.45f};
}

// Handing tickets on (rare for a cook): slips going up to the rail.
inline void cookDelegate(Host& h, float x, float z, float t, Pose& p) {
    flyingTicket(h, x, z + 0.4f, x + 0.6f, z + kRailZ, frac(t * 0.7f));
    p.right.y = 1.8f;
}

inline void cookThink(Host& h, float x, float cz, float t, Pose& p) {
    const float u = 0.5f - 0.5f * std::cos(t * 1.6f);
    h.add(STRATA_PRIMITIVE_CYLINDER, x, kCounterTop + 0.05f, cz, 0.36f, 0.05f, 0.36f, kWhite);
    p.right = {x + 0.05f, 1.0f + 0.5f * u, cz - 0.4f * u};
}

// Wiping down: a cloth swept along the counter.
inline void cookWipe(Host& h, float x, float cz, float t, Pose& p) {
    p.right = {x + 0.45f * std::sin(t * 3.0f), kCounterTop + 0.08f, cz};
    h.add(STRATA_PRIMITIVE_CUBE, p.right.x, kCounterTop + 0.04f, cz, 0.3f, 0.03f, 0.22f, {0.3f, 0.5f, 0.8f});
}

// Resting: leaning on the counter, elbows down, a slow sway, a mug.
inline void cookRest(Host& h, float x, float z, float cz, float t, Pose& p) {
    p.lean = 0.32f;
    p.drop = 0.2f;
    const float sway = 0.03f * std::sin(t * 0.8f);
    p.left = {x - 0.3f, kCounterTop + 0.04f, z + 0.5f + sway};
    p.right = {x + 0.3f, kCounterTop + 0.04f, z + 0.5f - sway};
    h.add(STRATA_PRIMITIVE_CUBE, x - 0.4f, kCounterTop + 0.03f, cz, 0.35f, 0.04f, 0.25f, kWhite);
    h.add(STRATA_PRIMITIVE_CYLINDER, x + 0.1f, kCounterTop + 0.1f, cz, 0.14f, 0.1f, 0.14f, {0.45f, 0.25f, 0.12f});
}

inline void cookAction(Host& h, const Figure& f, Action act, float t, Pose& p) {
    const float x = f.x, z = f.z, cz = z + kCounterZ - 0.15f;
    switch (act) {
        case Action::Edit: cookEdit(h, x, cz, t, p); break;
        case Action::Run: cookRun(h, x, z, t, f.phase, p); break;
        case Action::Read: cookRead(h, x, z, t, p); break;
        case Action::Web: cookWeb(h, x, z, t, p); break;
        case Action::Delegate: cookDelegate(h, x, z, t, p); break;
        case Action::Think: cookThink(h, x, cz, t, p); break;
        case Action::Other: cookWipe(h, x, cz, t, p); break;
        default: cookRest(h, x, z, cz, t, p); break;
    }
}

/* ---------------------------------------------------------------- the cook */

inline void drawCook(Host& h, const Figure& f, float seconds) {
    const float t = seconds + f.phase * 10.0f;
    const Action act = f.state == State::Busy ? f.action : Action::None;  // resting reads as idle
    counter(h, f.x, f.z, 1.95f, act == Action::Run);
    railTicket(h, f.x, f.z, t, act == Action::Read, f.pulse);
    Pose p{{f.x - 0.3f, 1.0f, f.z + 0.35f}, {f.x + 0.3f, 1.0f, f.z + 0.35f}};
    cookAction(h, f, act, t, p);
    // A stalled cook (quiet 5+ min, not finished) has a pot boiling over behind it.
    if (f.stalled) boilOver(h, f.x + 0.5f, kCounterTop + 0.02f, f.z + kCounterZ + 0.15f, 0.65f, t);
    flare(h, f.x + 0.45f, kCounterTop, f.z + kCounterZ, f.pulse);
    person(h, f.x, f.z, 0.92f, false, f.accent, kWhite, p);
    highlightRing(h, f, 0.8f);
}

}  // namespace kitchen
