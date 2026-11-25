// agentviz-kitchen: Claude Code sessions as a restaurant kitchen brigade.
//
// Each place (repo) is one kitchen line; each session is a head chef at the
// pass under a heat lamp; its subagents are line cooks at stations along the
// line, each with a ticket clipped on the rail above. The page lays the
// kitchen out and eases it between feed snapshots; this file only draws what
// the page hands it each frame (kt_set_lines, kt_set_figures).
#define STRATA_KIT_MAIN
#include "strata_kit.h"

#include <cmath>
#include <vector>

namespace {

using strata_kit::Host;
using strata_kit::Rgb;

// Rows from the page. Keep in step with web/kitchen.js.
constexpr int kLineStride = 8;     // x0, x1, z, anyWaiting, allOffline, -, -, -
constexpr int kFigureStride = 16;  // see Figure below

enum Kind { kChef = 0, kCook = 1, kEmpty = 2 };
enum Action { aNone = 0, aEdit, aRun, aRead, aWeb, aDelegate, aThink, aOther };
enum State { sOffline = 0, sDim, sIdle, sBusy, sWaiting, sResting, sActive };

struct Figure {
    int kind, action, state;
    float x, z;
    bool stalled;
    Rgb accent;
    float presence;  // 0..1, fades in and out
    bool selected;
    float stationX0;  // chef: the first station's x
    int stations;     // chef: how many stations its line has
    float stride;     // chef: station spacing
    float phase;      // 0..1 per agent, so animations do not march in step
};

std::vector<float> g_lines, g_figures;

constexpr float kTau = 6.2831853f;
constexpr float kCounterTop = 0.9f;
constexpr float kCounterZ = 0.85f;  // counter centre, relative to the line
constexpr float kRailY = 2.25f, kRailZ = 0.55f;

const Rgb kSteel{0.50f, 0.53f, 0.57f}, kSteelDark{0.20f, 0.21f, 0.23f}, kWhite{0.92f, 0.92f, 0.90f},
    kSkin{0.78f, 0.55f, 0.42f}, kTrousers{0.10f, 0.10f, 0.12f}, kWood{0.55f, 0.34f, 0.16f},
    kTile{0.72f, 0.74f, 0.72f}, kFloor{0.13f, 0.06f, 0.045f}, kRed{1.0f, 0.08f, 0.04f}, kGold{0.95f, 0.70f, 0.20f},
    kPaper{0.97f, 0.95f, 0.85f}, kFlame{1.0f, 0.45f, 0.08f};

Rgb mix(Rgb a, Rgb b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
Rgb scale(Rgb a, float k) { return {a.r * k, a.g * k, a.b * k}; }
float frac(float v) { return v - std::floor(v); }
float hash(float n) { return frac(std::sin(n * 12.9898f) * 43758.5453f); }

void ring(Host& h, float x, float y, float z, float r, Rgb c, float a, float px = 3.0f) {
    const int n = 32;
    for (int k = 0; k < n; ++k) {
        const float b0 = k * kTau / n, b1 = (k + 1) * kTau / n;
        h.wideLine(x + std::cos(b0) * r, y, z + std::sin(b0) * r, x + std::cos(b1) * r, y, z + std::sin(b1) * r, c,
                   px, 0.05f, a);
    }
}

// A person: legs, a jacket, a neckerchief in the brigade's colour, a head,
// a hat (a tall toque for the head chef) and two sleeves to the hands.
void person(Host& h, float x, float z, float s, bool chef, Rgb accent, Rgb jacket, float lean, float drop,
            const float* handL, const float* handR) {
    // Leaning: the body tips toward the counter, the head further and a
    // little to one side, so it reads from behind too.
    const float bz = z + lean * 0.5f, hz = z + lean * 1.3f, hxo = lean * 0.6f;
    h.add(STRATA_PRIMITIVE_CYLINDER, x - 0.1f * s, 0.27f * s - drop * 0.5f, z, 0.16f * s, 0.27f * s, 0.18f * s,
          kTrousers);
    h.add(STRATA_PRIMITIVE_CYLINDER, x + 0.1f * s, 0.27f * s - drop * 0.5f, z, 0.16f * s, 0.27f * s, 0.18f * s,
          kTrousers);
    h.add(STRATA_PRIMITIVE_CAPSULE, x, 0.95f * s - drop, bz, 0.5f * s, 0.42f * s, 0.38f * s, jacket);
    // apron in the brigade colour, a band across the front and the back
    h.add(STRATA_PRIMITIVE_CUBE, x, 0.72f * s - drop, bz, 0.44f * s, 0.34f * s, 0.40f * s, accent);
    h.add(STRATA_PRIMITIVE_SPHERE, x, 1.36f * s - drop, bz, 0.40f * s, 0.12f * s, 0.36f * s, accent);
    const float hy = 1.58f * s - drop;
    const float hx = x + hxo;
    h.add(STRATA_PRIMITIVE_SPHERE, hx, hy, hz, 0.34f * s, 0.36f * s, 0.34f * s, kSkin);
    if (chef) {
        h.add(STRATA_PRIMITIVE_CYLINDER, hx, hy + 0.32f * s, hz, 0.34f * s, 0.16f * s, 0.34f * s, kWhite);
        h.add(STRATA_PRIMITIVE_SPHERE, hx, hy + 0.50f * s, hz, 0.46f * s, 0.26f * s, 0.46f * s, kWhite);
    } else {
        h.add(STRATA_PRIMITIVE_CYLINDER, hx, hy + 0.17f * s, hz, 0.34f * s, 0.05f * s, 0.34f * s, accent);
    }
    const float sy = 1.25f * s - drop;
    if (handL) {
        h.wideLine(x - 0.24f * s, sy, bz, handL[0], handL[1], handL[2], jacket, 3.0f, 0.13f * s);
        h.add(STRATA_PRIMITIVE_SPHERE, handL[0], handL[1], handL[2], 0.11f * s, 0.11f * s, 0.11f * s, kSkin);
    }
    if (handR) {
        h.wideLine(x + 0.24f * s, sy, bz, handR[0], handR[1], handR[2], jacket, 3.0f, 0.13f * s);
        h.add(STRATA_PRIMITIVE_SPHERE, handR[0], handR[1], handR[2], 0.11f * s, 0.11f * s, 0.11f * s, kSkin);
    }
}

// A station's counter, and its burner, lit or not.
void counter(Host& h, float x, float z, float w, float dark, bool burnerLit) {
    const Rgb steel = scale(kSteel, dark);
    h.add(STRATA_PRIMITIVE_CUBE, x, kCounterTop * 0.5f, z + kCounterZ, w, kCounterTop, 0.9f, scale(kSteelDark, dark));
    h.add(STRATA_PRIMITIVE_CUBE, x, kCounterTop - 0.02f, z + kCounterZ, w + 0.02f, 0.05f, 0.92f, steel);
    // the burner ring at the back right of every station
    const Rgb burner = burnerLit ? Rgb{0.35f, 0.12f, 0.05f} : scale(Rgb{0.06f, 0.06f, 0.07f}, dark);
    h.add(STRATA_PRIMITIVE_CYLINDER, x + 0.45f, kCounterTop + 0.02f, z + kCounterZ + 0.15f, 0.42f, 0.02f, 0.42f,
          burner);
}

// Tickets flying from the pass to a station: a white slip on an arc.
void flyingTicket(Host& h, float x0, float z0, float x1, float z1, float u) {
    const float x = x0 + (x1 - x0) * u, z = z0 + (z1 - z0) * u;
    const float y = 1.9f + std::sin(u * 3.14159f) * 1.2f;
    h.add(STRATA_PRIMITIVE_CUBE, x, y, z, 0.22f, 0.28f, 0.03f, kPaper);
    h.point(x, y, z, 6.0f, 0.25f, {1.0f, 1.0f, 0.8f}, 0.5f);
}

void steam(Host& h, float x, float y, float z, float t, float phase, Rgb c, int n, float rise, float alpha) {
    for (int k = 0; k < n; ++k) {
        const float u = frac(t * 0.45f + k / static_cast<float>(n) + phase);
        const float wob = std::sin((t + k) * 2.1f) * 0.12f * u;
        h.point(x + wob + (hash(k + phase * 7) - 0.5f) * 0.25f, y + u * rise, z + (hash(k * 3.1f) - 0.5f) * 0.2f,
                14.0f + 18.0f * u, 0.35f + 0.5f * u, c, alpha * (1.0f - u));
    }
}

void drawChef(Host& h, const Figure& f, double td) {
    const float t = static_cast<float>(td) + f.phase * 10.0f;
    const float x = f.x, z = f.z, pres = f.presence;
    const bool offline = f.state == sOffline, waiting = f.state == sWaiting;
    const float dark = offline ? 0.35f : (f.state == sDim ? 0.6f : 1.0f);
    const float passZ = z + kCounterZ;

    // The pass: a higher steel shelf with a heat lamp hanging over it.
    h.add(STRATA_PRIMITIVE_CUBE, x, 0.5f, passZ, 1.8f, 1.0f, 0.9f, scale(kSteelDark, dark));
    h.add(STRATA_PRIMITIVE_CUBE, x, 1.0f, passZ, 1.9f, 0.05f, 0.95f, scale(kSteel, dark));
    h.add(STRATA_PRIMITIVE_CUBE, x, 1.5f, passZ + 0.3f, 1.9f, 0.04f, 0.4f, scale(kSteel, dark));  // upper shelf
    h.wideLine(x, 2.6f, passZ, x, 2.1f, passZ, kSteelDark, 2.0f, 0.03f);
    const float pulse = 0.5f + 0.5f * std::sin(t * 5.0f);
    Rgb lampC = waiting ? kRed : (offline ? Rgb{0.08f, 0.08f, 0.08f}
                                          : (f.state == sBusy ? Rgb{1.0f, 0.55f, 0.15f} : Rgb{0.5f, 0.3f, 0.12f}));
    h.add(STRATA_PRIMITIVE_CYLINDER, x, 2.05f, passZ, 0.7f, 0.08f, 0.5f, scale(kSteelDark, 1.0f));
    h.add(STRATA_PRIMITIVE_SPHERE, x, 1.98f, passZ, 0.5f, 0.12f, 0.35f, waiting ? scale(lampC, 0.6f + 0.6f * pulse) : lampC);
    if (!offline) {
        const float inten = waiting ? 2.5f + 2.5f * pulse : (f.state == sBusy ? 1.4f : 0.6f);
        h.light(x, 1.8f, passZ - 0.2f, waiting ? 7.0f : 4.5f, waiting ? kRed : Rgb{1.0f, 0.6f, 0.25f}, inten,
                waiting ? 20 : 4);
    }
    // The rail: from the pass along the brigade's stations, in its colour.
    const float railEnd = f.stationX0 + (static_cast<float>(f.stations) - 0.5f) * f.stride;
    const Rgb railC = scale(f.accent, offline ? 0.35f : 1.0f);
    h.wideLine(x - 0.9f, kRailY, z + kRailZ, railEnd, kRailY, z + kRailZ, railC, 4.0f, 0.06f, pres);
    h.add(STRATA_PRIMITIVE_CYLINDER, railEnd, (kRailY + 1.5f) * 0.5f, z + kRailZ + 0.3f, 0.06f, (kRailY - 1.5f) * 0.5f,
          0.06f, kSteelDark);

    if (offline) {
        // Nobody home: a hanging apron on the pass, the lamp off.
        h.wideLine(x - 0.3f, 1.5f, passZ - 0.45f, x - 0.3f, 1.42f, passZ - 0.47f, kSteelDark, 2.0f, 0.02f);
        h.add(STRATA_PRIMITIVE_CUBE, x - 0.3f, 1.12f, passZ - 0.5f, 0.42f, 0.6f, 0.03f, scale(f.accent, 0.5f));
        h.add(STRATA_PRIMITIVE_CUBE, x + 0.25f, 1.2f, passZ - 0.5f, 0.3f, 0.35f, 0.03f, scale(kWhite, 0.4f));
        if (f.selected) ring(h, x, 0.03f, z, 0.9f, {1, 1, 1}, 0.9f);
        return;
    }

    const float s = 1.12f;
    const Rgb jacket = scale(kWhite, dark);
    float hl[3] = {x - 0.35f, 0.95f, z + 0.25f}, hr[3] = {x + 0.35f, 0.95f, z + 0.25f};
    float drop = 0.0f;
    switch (f.state == sBusy ? f.action : aNone) {
        case aDelegate: {
            // Calling out: one arm up, tickets flying to every station.
            hr[0] = x + 0.35f;
            hr[1] = 2.0f + 0.15f * std::sin(t * 6.0f);
            hr[2] = z + 0.1f;
            for (int k = 0; k < std::max(f.stations, 1); ++k) {
                const float u = frac(t * 0.5f + k * 0.37f);
                flyingTicket(h, x + 0.3f, z + kRailZ, f.stationX0 + k * f.stride, z + kRailZ, u);
            }
            break;
        }
        case aThink: {
            // Tasting: a bowl on the pass, a spoon going to the mouth.
            const float u = 0.5f - 0.5f * std::cos(t * 1.6f);
            h.add(STRATA_PRIMITIVE_CYLINDER, x + 0.4f, 1.08f, passZ - 0.2f, 0.36f, 0.06f, 0.36f, kWhite);
            h.add(STRATA_PRIMITIVE_CYLINDER, x + 0.4f, 1.12f, passZ - 0.2f, 0.28f, 0.03f, 0.28f, {0.6f, 0.3f, 0.1f});
            hr[0] = x + 0.4f - 0.3f * u;
            hr[1] = 1.15f + 0.6f * u;
            hr[2] = passZ - 0.2f - 0.35f * u;
            h.add(STRATA_PRIMITIVE_SPHERE, hr[0], hr[1] + 0.08f, hr[2], 0.1f, 0.04f, 0.1f, kSteel);
            for (int k = 0; k < 3; ++k) {
                const float v = frac(t * 0.3f + k / 3.0f);
                h.point(x - 0.2f + 0.2f * k, 2.4f + v * 0.6f, z, 10.0f, 0.18f, {1.0f, 0.9f, 0.5f}, 0.7f * (1.0f - v));
            }
            break;
        }
        case aRun: {
            // At the stove: a pan on the pass burner, flames and steam.
            const float px = x + 0.45f, pz = passZ - 0.15f;
            h.add(STRATA_PRIMITIVE_CYLINDER, px, 1.14f, pz, 0.55f, 0.1f, 0.55f, kSteelDark);
            for (int k = 0; k < 6; ++k) {
                const float a = k * kTau / 6 + t * 3.0f;
                h.point(px + std::cos(a) * 0.25f, 1.04f, pz + std::sin(a) * 0.25f, 10.0f, 0.15f, kFlame,
                        0.7f + 0.3f * std::sin(t * 17.0f + k));
            }
            steam(h, px, 1.25f, pz, t, f.phase, {0.9f, 0.9f, 0.9f}, 6, 1.1f, 0.5f);
            hr[0] = px + 0.15f * std::cos(t * 4.0f);
            hr[1] = 1.3f;
            hr[2] = pz - 0.15f + 0.1f * std::sin(t * 4.0f);
            h.light(px, 1.3f, pz, 3.0f, kFlame, 0.8f + 0.3f * std::sin(t * 13.0f), 2);
            break;
        }
        case aEdit: {
            const float chop = std::fabs(std::sin(t * 8.0f));
            h.add(STRATA_PRIMITIVE_CUBE, x, 1.05f, passZ - 0.2f, 0.75f, 0.05f, 0.45f, kWood);
            hr[1] = 1.12f + 0.18f * chop;
            hr[2] = passZ - 0.2f;
            h.add(STRATA_PRIMITIVE_CUBE, hr[0] - 0.12f, hr[1] - 0.05f, hr[2], 0.22f, 0.08f, 0.02f, kSteel);
            hl[1] = 1.1f;
            hl[2] = passZ - 0.2f;
            break;
        }
        case aRead: {
            const float bob = 0.04f * std::sin(t * 1.5f);
            hl[0] = x - 0.15f; hl[1] = 1.55f + bob; hl[2] = z + 0.45f;
            hr[0] = x + 0.15f; hr[1] = 1.55f + bob; hr[2] = z + 0.45f;
            h.add(STRATA_PRIMITIVE_CUBE, x, 1.6f + bob, z + 0.5f, 0.32f, 0.42f, 0.02f, kPaper);
            break;
        }
        case aWeb:
        case aOther: {
            const float u = std::sin(t * 3.0f);
            hr[0] = x + 0.5f * u;
            hr[1] = 1.06f;
            hr[2] = passZ - 0.3f;
            h.add(STRATA_PRIMITIVE_CUBE, hr[0], 1.04f, hr[2], 0.3f, 0.03f, 0.22f, {0.3f, 0.5f, 0.8f});
            break;
        }
        default:
            break;
    }
    if (waiting) {
        // Order up: a plate under the red lamp, the service bell rung again
        // and again, a red ring on the floor and a red glow rising.
        h.add(STRATA_PRIMITIVE_CYLINDER, x - 0.3f, 1.05f, passZ - 0.1f, 0.6f, 0.02f, 0.6f, {1.0f, 1.0f, 0.97f});
        h.add(STRATA_PRIMITIVE_SPHERE, x - 0.3f, 1.12f, passZ - 0.1f, 0.3f, 0.14f, 0.3f, {0.85f, 0.35f, 0.1f});
        h.add(STRATA_PRIMITIVE_SPHERE, x - 0.18f, 1.17f, passZ - 0.05f, 0.1f, 0.1f, 0.1f, {0.2f, 0.6f, 0.15f});
        const float ring1 = std::fabs(std::sin(t * 4.0f));
        const float bx = x + 0.5f, bzz = passZ - 0.3f;
        h.add(STRATA_PRIMITIVE_CYLINDER, bx, 1.05f, bzz, 0.3f, 0.02f, 0.3f, kSteelDark);
        h.add(STRATA_PRIMITIVE_SPHERE, bx, 1.12f + 0.03f * ring1, bzz, 0.26f, 0.2f, 0.26f, kGold);
        h.add(STRATA_PRIMITIVE_SPHERE, bx, 1.24f + 0.03f * ring1, bzz, 0.05f, 0.06f, 0.05f, kGold);
        hr[0] = bx;
        hr[1] = 1.3f + 0.25f * (1.0f - ring1);
        hr[2] = bzz;
        // ring of sound from the bell
        const float w = frac(t * 1.3f);
        ring(h, bx, 1.2f + w * 0.2f, bzz, 0.2f + w * 0.6f, kGold, 1.0f - w, 2.0f);
        ring(h, x, 0.03f, z + 0.3f, 1.2f + 0.15f * pulse, kRed, 0.6f + 0.4f * pulse, 5.0f);
        for (int k = 0; k < 10; ++k) {
            const float v = frac(t * 0.35f + k * 0.1f);
            h.point(x + std::sin(k * 2.3f + t) * 0.3f, 2.2f + v * 3.0f, passZ, 26.0f, 0.6f, kRed, 0.55f * (1.0f - v));
        }
    }
    if (f.stalled) {
        // Boiling over: a stock pot on the pass, foam and red smoke.
        const float px = x - 0.62f, pz = passZ - 0.1f;
        h.add(STRATA_PRIMITIVE_CYLINDER, px, 1.3f, pz, 0.56f, 0.28f, 0.56f, kSteel);
        h.add(STRATA_PRIMITIVE_SPHERE, px, 1.6f, pz, 0.62f, 0.2f, 0.62f, {0.95f, 0.9f, 0.8f});
        h.add(STRATA_PRIMITIVE_SPHERE, px - 0.25f, 1.35f, pz - 0.22f, 0.16f, 0.32f, 0.12f, {0.95f, 0.9f, 0.8f});
        h.add(STRATA_PRIMITIVE_SPHERE, px - 0.25f, 1.02f, pz - 0.3f, 0.4f, 0.04f, 0.3f, {0.95f, 0.9f, 0.8f});
        for (int k = 0; k < 14; ++k) {  // thick red smoke
            const float u = frac(t * 0.5f + k / 14.0f);
            h.point(px + std::sin(k * 1.7f + t) * 0.25f * u, 1.75f + u * 2.8f, pz + std::cos(k * 2.3f) * 0.2f * u,
                    28.0f + 40.0f * u, 0.6f + 0.9f * u, {0.75f, 0.08f, 0.05f}, 0.85f * (1.0f - u));
        }
        h.light(px, 1.6f, pz, 3.5f, {1.0f, 0.2f, 0.1f}, 1.2f, 6);
    }
    if (f.state == sIdle || f.state == sDim) {
        hl[2] = z + 0.05f;
        hr[2] = z + 0.05f;
        hl[1] = hr[1] = 0.85f;
    }
    person(h, x, z, s, true, scale(f.accent, dark), jacket, 0.0f, drop, hl, hr);
    if (f.selected) ring(h, x, 0.03f, z, 0.9f, {1, 1, 1}, 0.9f);
}

void drawStation(Host& h, const Figure& f, double td) {
    const float t = static_cast<float>(td) + f.phase * 10.0f;
    const float x = f.x, z = f.z;
    const bool cook = f.kind == kCook;
    const bool working = cook && f.state == sActive;
    const int act = working ? f.action : aNone;
    const float cz = z + kCounterZ - 0.15f;  // where things go on the counter
    counter(h, x, z, 1.95f, 1.0f, act == aRun);

    // The ticket on the rail above the station: this cook's lane.
    if (cook) {
        const float sway = 0.05f * std::sin(t * 1.7f);
        const float glow = act == aRead ? 0.5f + 0.5f * std::sin(t * 4.0f) : 0.0f;
        h.add(STRATA_PRIMITIVE_CUBE, x + sway, kRailY - 0.24f, z + kRailZ, 0.32f, 0.42f, 0.02f,
              mix(kPaper, {1.0f, 1.0f, 0.5f}, glow));
        h.add(STRATA_PRIMITIVE_CUBE, x + sway, kRailY - 0.03f, z + kRailZ, 0.1f, 0.06f, 0.05f, kSteelDark);
        if (act == aRead) h.point(x, kRailY - 0.24f, z + kRailZ - 0.05f, 18.0f, 0.6f, {1.0f, 0.95f, 0.5f}, 0.5f);
    }
    if (!cook) {
        // An empty, clean station: a folded towel, the burner off.
        h.add(STRATA_PRIMITIVE_CUBE, x - 0.4f, kCounterTop + 0.03f, cz, 0.35f, 0.04f, 0.25f, kWhite);
        return;
    }

    float hl[3] = {x - 0.3f, 1.0f, z + 0.35f}, hr[3] = {x + 0.3f, 1.0f, z + 0.35f};
    float lean = 0.0f, drop = 0.0f;
    switch (act) {
        case aEdit: {
            // Chopping at the board.
            const float chop = std::fabs(std::sin(t * 9.0f));
            h.add(STRATA_PRIMITIVE_CUBE, x - 0.1f, kCounterTop + 0.03f, cz, 0.75f, 0.05f, 0.45f, kWood);
            hr[0] = x + 0.1f; hr[1] = 1.0f + 0.2f * chop; hr[2] = cz;
            hl[0] = x - 0.3f; hl[1] = 0.98f; hl[2] = cz;
            h.add(STRATA_PRIMITIVE_CUBE, hr[0] - 0.13f, hr[1] - 0.06f, cz, 0.26f, 0.09f, 0.02f, kSteel);
            for (int k = 0; k < 5; ++k)
                h.add(STRATA_PRIMITIVE_SPHERE, x - 0.4f + k * 0.08f, kCounterTop + 0.08f, cz + 0.08f * (k % 2), 0.08f,
                      0.06f, 0.08f, k % 2 ? Rgb{0.9f, 0.4f, 0.05f} : Rgb{0.2f, 0.6f, 0.1f});
            const float u = frac(t * 1.5f);
            h.add(STRATA_PRIMITIVE_SPHERE, x + 0.05f + u * 0.2f, kCounterTop + 0.1f + std::sin(u * 3.14f) * 0.25f, cz,
                  0.06f, 0.06f, 0.06f, {0.2f, 0.6f, 0.1f});
            break;
        }
        case aRun: {
            // Working the stove: a pot on a lit burner, flames and steam.
            const float px = x + 0.45f, pz = z + kCounterZ;
            h.add(STRATA_PRIMITIVE_CYLINDER, px, kCounterTop + 0.2f, pz, 0.5f, 0.18f, 0.5f, kSteel);
            for (int k = 0; k < 6; ++k) {
                const float a = k * kTau / 6 + t * 3.0f;
                h.point(px + std::cos(a) * 0.22f, kCounterTop + 0.05f, pz + std::sin(a) * 0.22f, 10.0f, 0.14f, kFlame,
                        0.7f + 0.3f * std::sin(t * 17.0f + k));
            }
            steam(h, px, kCounterTop + 0.4f, pz, t, f.phase, {0.92f, 0.92f, 0.92f}, 6, 1.1f, 0.45f);
            hr[0] = px + 0.12f * std::cos(t * 5.0f); hr[1] = 1.3f; hr[2] = pz - 0.1f + 0.1f * std::sin(t * 5.0f);
            h.wideLine(hr[0], hr[1], hr[2], px, kCounterTop + 0.25f, pz, kWood, 2.0f, 0.03f);
            h.light(px, kCounterTop + 0.5f, pz, 2.5f, kFlame, 0.7f + 0.3f * std::sin(t * 11.0f), 1);
            break;
        }
        case aRead: {
            // Reading the ticket: a card held up, looking from it to the rail.
            const float bob = 0.04f * std::sin(t * 1.4f);
            // held up beside the head, so it shows from behind too
            const float rx = x + 0.42f;
            hl[0] = rx - 0.14f; hl[1] = 1.55f + bob; hl[2] = z + 0.2f;
            hr[0] = rx + 0.14f; hr[1] = 1.55f + bob; hr[2] = z + 0.2f;
            h.add(STRATA_PRIMITIVE_CUBE, rx, 1.62f + bob, z + 0.22f, 0.36f, 0.48f, 0.02f, kPaper);
            for (int k = 0; k < 4; ++k)
                h.add(STRATA_PRIMITIVE_CUBE, rx, 1.76f + bob - k * 0.09f, z + 0.2f, 0.24f, 0.025f, 0.01f,
                      {0.2f, 0.2f, 0.25f});
            break;
        }
        case aWeb: {
            // Fetching from the walk-in: a cold door behind the station,
            // frost spilling out, crates coming over the counter.
            const float dz = z + 1.75f;
            h.add(STRATA_PRIMITIVE_CUBE, x, 1.05f, dz, 1.0f, 2.1f, 0.12f, {0.45f, 0.62f, 0.78f});
            h.add(STRATA_PRIMITIVE_CUBE, x + 0.35f, 1.1f, dz - 0.08f, 0.06f, 0.4f, 0.06f, kSteel);
            for (int k = 0; k < 6; ++k) {
                const float v = frac(t * 0.4f + k / 6.0f);
                h.point(x - 0.4f + k * 0.16f, 0.2f + v * 0.5f, dz - 0.1f - v * 0.4f, 16.0f, 0.4f, {0.7f, 0.85f, 1.0f},
                        0.6f * (1.0f - v));
            }
            const float u = frac(t * 0.6f);
            const float cx = x, cyy = 1.1f + std::sin(u * 3.14159f) * 0.6f, czz = dz - 0.2f - u * 1.4f;
            h.add(STRATA_PRIMITIVE_CUBE, cx, cyy, czz, 0.36f, 0.24f, 0.28f, kWood);
            h.add(STRATA_PRIMITIVE_SPHERE, cx - 0.06f, cyy + 0.14f, czz, 0.12f, 0.12f, 0.12f, {0.8f, 0.1f, 0.1f});
            h.add(STRATA_PRIMITIVE_SPHERE, cx + 0.08f, cyy + 0.14f, czz, 0.12f, 0.12f, 0.12f, {0.95f, 0.75f, 0.1f});
            hl[0] = x - 0.2f; hl[1] = 1.2f; hl[2] = z + 0.45f;
            hr[0] = x + 0.2f; hr[1] = 1.2f; hr[2] = z + 0.45f;
            break;
        }
        case aDelegate: {
            // A cook handing tickets on (rare): slips going up to the rail.
            const float u = frac(t * 0.7f);
            flyingTicket(h, x, z + 0.4f, x + 0.6f, z + kRailZ, u);
            hr[1] = 1.8f;
            break;
        }
        case aThink: {
            const float u = 0.5f - 0.5f * std::cos(t * 1.6f);
            h.add(STRATA_PRIMITIVE_CYLINDER, x, kCounterTop + 0.05f, cz, 0.36f, 0.05f, 0.36f, kWhite);
            hr[0] = x + 0.05f; hr[1] = 1.0f + 0.5f * u; hr[2] = cz - 0.4f * u;
            break;
        }
        case aOther: {
            // Wiping down: a cloth swept along the counter.
            const float u = std::sin(t * 3.0f);
            hr[0] = x + 0.45f * u; hr[1] = kCounterTop + 0.08f; hr[2] = cz;
            h.add(STRATA_PRIMITIVE_CUBE, hr[0], kCounterTop + 0.04f, cz, 0.3f, 0.03f, 0.22f, {0.3f, 0.5f, 0.8f});
            break;
        }
        default: {
            // Resting: leaning on the counter, elbows down, a slow sway.
            lean = 0.32f;
            drop = 0.2f;
            const float sway = 0.03f * std::sin(t * 0.8f);
            hl[0] = x - 0.3f; hl[1] = kCounterTop + 0.04f; hl[2] = z + 0.5f + sway;
            hr[0] = x + 0.3f; hr[1] = kCounterTop + 0.04f; hr[2] = z + 0.5f - sway;
            h.add(STRATA_PRIMITIVE_CUBE, x - 0.4f, kCounterTop + 0.03f, cz, 0.35f, 0.04f, 0.25f, kWhite);
            h.add(STRATA_PRIMITIVE_CYLINDER, x + 0.1f, kCounterTop + 0.1f, cz, 0.14f, 0.1f, 0.14f,
                  {0.45f, 0.25f, 0.12f});  // a mug
            break;
        }
    }
    person(h, x, z, 0.92f, false, f.accent, kWhite, lean, drop, hl, hr);
    if (f.selected) ring(h, x, 0.03f, z, 0.8f, {1, 1, 1}, 0.9f);
}

void drawLine(Host& h, const float* l, double t) {
    const float x0 = l[0], x1 = l[1], z = l[2];
    const bool waiting = l[3] > 0.5f, off = l[4] > 0.5f;
    const float len = x1 - x0, cx = (x0 + x1) * 0.5f;
    // The line's floor mat, the backsplash, a hood.
    h.add(STRATA_PRIMITIVE_CUBE, cx, 0.01f, z + 0.4f, len + 1.0f, 0.02f, 3.0f, off ? Rgb{0.1f, 0.05f, 0.04f}
                                                                                  : Rgb{0.32f, 0.12f, 0.07f});
    const Rgb tile = off ? scale(kTile, 0.4f) : kTile;
    h.add(STRATA_PRIMITIVE_CUBE, cx, 0.8f, z + 1.4f, len + 0.6f, 1.6f, 0.12f, tile);
    h.add(STRATA_PRIMITIVE_CUBE, cx, 2.95f, z + 1.45f, len + 0.6f, 0.12f, 0.25f, scale(kSteel, off ? 0.4f : 0.85f));
    // grout lines on the backsplash
    for (float gx = x0; gx <= x1; gx += 0.5f)
        h.line(gx, 0.0f, z + 1.33f, gx, 1.6f, z + 1.33f, {0.5f, 0.5f, 0.48f}, 0.5f);
    if (waiting) {
        const float p = 0.5f + 0.5f * std::sin(static_cast<float>(t) * 5.0f);
        h.wideLine(x0 - 0.3f, 2.9f, z + 0.3f, x1 + 0.3f, 2.9f, z + 0.3f, kRed, 4.0f, 0.06f, 0.5f + 0.5f * p);
    }
    if (!off) h.light(cx, 2.7f, z + 0.2f, std::max(len * 0.7f, 6.0f), {1.0f, 0.95f, 0.85f}, 0.6f, 3);
}

Figure unpack(const float* r) {
    Figure f{};
    f.kind = static_cast<int>(r[0] + 0.5f);
    f.x = r[1];
    f.z = r[2];
    f.action = static_cast<int>(r[3] + 0.5f);
    f.state = static_cast<int>(r[4] + 0.5f);
    f.stalled = r[5] > 0.5f;
    f.accent = {r[6], r[7], r[8]};
    f.presence = r[9];
    f.selected = r[10] > 0.5f;
    f.stationX0 = r[11];
    f.stations = static_cast<int>(r[12] + 0.5f);
    f.stride = r[13];
    f.phase = r[14];
    return f;
}

}  // namespace

void strata_kit_configure(strata_kit::Options& o) {
    o.name = "kitchen";
    o.tiers[0].thinPoints = true;
    o.fog = false;
}

void strata_kit_engine_ready(Host&) {}

void strata_kit_build(Host& h, double t) {
    h.clearFlags = STRATA_CLEAR_COLOR | STRATA_CLEAR_DEPTH;
    h.clearColor[0] = 0.035f;
    h.clearColor[1] = 0.03f;
    h.clearColor[2] = 0.03f;
    h.ambientTint = {1.0f, 0.92f, 0.82f};
    h.ambientLift = {0.10f, 0.09f, 0.08f};
    h.sunIntensity = 0.85f;
    h.sunTint = {1.0f, 0.95f, 0.88f};
    // The kitchen floor: red quarry tile.
    float minX = -10, maxX = 10, minZ = -6, maxZ = 6;
    for (size_t i = 0; i + kLineStride <= g_lines.size(); i += kLineStride) {
        minX = std::min(minX, g_lines[i] - 3.0f);
        maxX = std::max(maxX, g_lines[i + 1] + 3.0f);
        minZ = std::min(minZ, g_lines[i + 2] - 4.0f);
        maxZ = std::max(maxZ, g_lines[i + 2] + 4.0f);
    }
    h.add(STRATA_PRIMITIVE_CUBE, (minX + maxX) * 0.5f, -0.05f, (minZ + maxZ) * 0.5f, maxX - minX, 0.1f, maxZ - minZ,
          kFloor);
    for (size_t i = 0; i + kLineStride <= g_lines.size(); i += kLineStride) drawLine(h, &g_lines[i], t);
    for (size_t i = 0; i + kFigureStride <= g_figures.size(); i += kFigureStride) {
        const Figure f = unpack(&g_figures[i]);
        if (f.presence <= 0.02f) continue;
        if (f.kind == kChef)
            drawChef(h, f, t);
        else
            drawStation(h, f, t);
    }
    // A fill light so the lowest tier keeps its level.
    StrataPointLight fill{};
    fill.position[0] = (minX + maxX) * 0.5f;
    fill.position[1] = 8.0f;
    fill.position[2] = (minZ + maxZ) * 0.5f;
    fill.range = 40.0f;
    fill.color[0] = 1.0f;
    fill.color[1] = 0.95f;
    fill.color[2] = 0.85f;
    fill.intensity = 0.5f;
    h.fill(fill, {0.06f, 0.055f, 0.05f}, 5);
}

STRATA_KIT_EXPORT void kt_set_lines(const float* rows, int count) {
    g_lines.assign(rows, rows + static_cast<size_t>(std::max(count, 0)) * kLineStride);
}

STRATA_KIT_EXPORT void kt_set_figures(const float* rows, int count) {
    g_figures.assign(rows, rows + static_cast<size_t>(std::max(count, 0)) * kFigureStride);
}
