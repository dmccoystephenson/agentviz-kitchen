// draw_util.h: the kitchen's palette, its view of a wire agent, and the small
// effects every part of the scene shares (rings, steam, flying tickets, a
// pot boiling over, the per-call flare).
#pragma once

#include <algorithm>
#include <cmath>

#include "../vendor/agentviz-kit/scene/agentviz.h"

namespace kitchen {

using strata_kit::Host;
using strata_kit::Rgb;
using agentviz::Action;
using agentviz::State;

constexpr float kTau = 6.2831853f;
constexpr float kCounterTop = 0.9f;
constexpr float kCounterZ = 0.85f;  // counter centre, relative to the line
constexpr float kRailY = 2.25f, kRailZ = 0.55f;

const Rgb kSteel{0.50f, 0.53f, 0.57f}, kSteelDark{0.20f, 0.21f, 0.23f}, kWhite{0.92f, 0.92f, 0.90f},
    kSkin{0.78f, 0.55f, 0.42f}, kTrousers{0.10f, 0.10f, 0.12f}, kWood{0.55f, 0.34f, 0.16f},
    kTile{0.72f, 0.74f, 0.72f}, kFloor{0.13f, 0.06f, 0.045f}, kRed{1.0f, 0.08f, 0.04f}, kGold{0.95f, 0.70f, 0.20f},
    kPaper{0.97f, 0.95f, 0.85f}, kFlame{1.0f, 0.45f, 0.08f}, kFoam{0.95f, 0.9f, 0.8f};

inline Rgb mix(Rgb a, Rgb b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
inline Rgb scale(Rgb a, float k) { return {a.r * k, a.g * k, a.b * k}; }
inline float frac(float v) { return v - std::floor(v); }
inline float hash(float n) { return frac(std::sin(n * 12.9898f) * 43758.5453f); }

// The brigade colour: hsl(seed * 360, 65%, 50%) in linear RGB, the same hue
// as the page's label borders.
inline Rgb accentOf(float seed) {
    const float hue = std::fmod(seed * 360.0f, 360.0f), s = 0.65f, l = 0.5f;
    const float a = s * std::min(l, 1.0f - l);
    auto channel = [&](float n) {
        const float k = std::fmod(n + hue / 30.0f, 12.0f);
        const float c = l - a * std::max(-1.0f, std::min({k - 3.0f, 9.0f - k, 1.0f}));
        return std::pow(c, 2.2f);
    };
    return {channel(0.0f), channel(8.0f), channel(4.0f)};
}

// What the scene needs to know about one chef or cook, from its wire record.
struct Figure {
    float x, z, appear, phase, pulse;
    State state;
    Action action;
    agentviz::WaitingReason reason;
    bool stalled;
    Rgb accent;
    agentviz::Highlight highlight;
    float stationX0;  // chef: the first station's x
    int stations;     // chef: how many stations its line has
    float stride;     // chef: station spacing
};

inline Figure figureOf(const agentviz::Agent& a) {
    return {a.x, a.z, a.appear, a.seed, a.pulse, a.state, a.action, a.waitingReason, a.stalled,
            accentOf(a.parentSeed), a.highlight, a.extra[0], agentviz::toInt(a.extra[1]), a.extra[2]};
}

inline void ring(Host& h, float x, float y, float z, float r, Rgb c, float a, float px = 3.0f) {
    const int n = 32;
    for (int k = 0; k < n; ++k) {
        const float b0 = k * kTau / n, b1 = (k + 1) * kTau / n;
        h.wideLine(x + std::cos(b0) * r, y, z + std::sin(b0) * r, x + std::cos(b1) * r, y, z + std::sin(b1) * r, c,
                   px, 0.05f, a);
    }
}

// White ring on the floor when selected, a fainter one when hovered.
inline void highlightRing(Host& h, const Figure& f, float r) {
    if (f.highlight == agentviz::Highlight::Selected) ring(h, f.x, 0.03f, f.z, r, {1, 1, 1}, 0.9f);
    if (f.highlight == agentviz::Highlight::Hovered) ring(h, f.x, 0.03f, f.z, r, {1, 1, 1}, 0.4f, 2.0f);
}

// A ticket on its way from the pass to a station: a white slip on an arc.
inline void flyingTicket(Host& h, float x0, float z0, float x1, float z1, float u) {
    const float x = x0 + (x1 - x0) * u, z = z0 + (z1 - z0) * u;
    const float y = 1.9f + std::sin(u * 3.14159f) * 1.2f;
    h.add(STRATA_PRIMITIVE_CUBE, x, y, z, 0.22f, 0.28f, 0.03f, kPaper);
    h.point(x, y, z, 6.0f, 0.25f, {1.0f, 1.0f, 0.8f}, 0.5f);
}

inline void steam(Host& h, float x, float y, float z, float t, float phase, Rgb c, int n, float rise, float alpha) {
    for (int k = 0; k < n; ++k) {
        const float u = frac(t * 0.45f + k / static_cast<float>(n) + phase);
        const float wob = std::sin((t + k) * 2.1f) * 0.12f * u;
        h.point(x + wob + (hash(k + phase * 7) - 0.5f) * 0.25f, y + u * rise, z + (hash(k * 3.1f) - 0.5f) * 0.2f,
                14.0f + 18.0f * u, 0.35f + 0.5f * u, c, alpha * (1.0f - u));
    }
}

// Flames licking round a pot on a lit burner.
inline void burnerFlames(Host& h, float x, float y, float z, float r, float t) {
    for (int k = 0; k < 6; ++k) {
        const float a = k * kTau / 6 + t * 3.0f;
        h.point(x + std::cos(a) * r, y, z + std::sin(a) * r, 10.0f, 0.15f, kFlame,
                0.7f + 0.3f * std::sin(t * 17.0f + k));
    }
}

// Stalled: a stock pot boiling over, foam down its side and thick red smoke.
// `size` 1 is the chef's pot at the pass; cooks get a smaller one.
inline void boilOver(Host& h, float px, float y, float pz, float size, float t) {
    const float s = size;
    h.add(STRATA_PRIMITIVE_CYLINDER, px, y + 0.28f * s, pz, 0.56f * s, 0.28f * s, 0.56f * s, kSteel);
    h.add(STRATA_PRIMITIVE_SPHERE, px, y + 0.58f * s, pz, 0.62f * s, 0.2f * s, 0.62f * s, kFoam);
    h.add(STRATA_PRIMITIVE_SPHERE, px - 0.25f * s, y + 0.33f * s, pz - 0.22f * s, 0.16f * s, 0.32f * s, 0.12f * s,
          kFoam);
    h.add(STRATA_PRIMITIVE_SPHERE, px - 0.25f * s, y, pz - 0.3f * s, 0.4f * s, 0.04f, 0.3f * s, kFoam);
    for (int k = 0; k < 14; ++k) {
        const float u = frac(t * 0.5f + k / 14.0f);
        h.point(px + std::sin(k * 1.7f + t) * 0.25f * u, y + 0.73f * s + u * 2.8f * s,
                pz + std::cos(k * 2.3f) * 0.2f * u, (28.0f + 40.0f * u) * s, (0.6f + 0.9f * u) * s,
                {0.75f, 0.08f, 0.05f}, 0.85f * (1.0f - u));
    }
    h.light(px, y + 0.58f * s, pz, 3.5f * s, {1.0f, 0.2f, 0.1f}, 1.2f, 6);
}

// A new tool call: a flare of sparks jumps off the burner and fades
// (`pulse` is 1 at the call, easing to 0).
inline void flare(Host& h, float x, float y, float z, float pulse) {
    if (pulse <= 0.01f) return;
    const float u = 1.0f - pulse;
    for (int k = 0; k < 9; ++k) {
        const float a = k * kTau / 9;
        h.point(x + std::cos(a) * (0.15f + 0.5f * u), y + 0.2f + 1.1f * u * (0.6f + 0.4f * hash(k * 1.3f)),
                z + std::sin(a) * (0.15f + 0.5f * u), 12.0f, 0.18f, {1.0f, 0.8f, 0.3f}, pulse);
    }
    h.light(x, y + 0.4f, z, 3.0f, {1.0f, 0.7f, 0.3f}, 2.0f * pulse, 3);
}

}  // namespace kitchen
