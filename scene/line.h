// line.h: the room. The quarry-tile floor, one kitchen line per place (a
// floor mat, tiled backsplash and hood, a spike of spent tickets at its
// head) and the station counters, including the clean empty ones.
#pragma once

#include <vector>

#include "draw_util.h"

namespace kitchen {

enum ZoneKind { kZoneLine = 0, kZoneStation = 1 };

// A station's counter, and its burner, lit or not.
inline void counter(Host& h, float x, float z, float w, bool burnerLit) {
    h.add(STRATA_PRIMITIVE_CUBE, x, kCounterTop * 0.5f, z + kCounterZ, w, kCounterTop, 0.9f, kSteelDark);
    h.add(STRATA_PRIMITIVE_CUBE, x, kCounterTop - 0.02f, z + kCounterZ, w + 0.02f, 0.05f, 0.92f, kSteel);
    const Rgb burner = burnerLit ? Rgb{0.35f, 0.12f, 0.05f} : Rgb{0.06f, 0.06f, 0.07f};
    h.add(STRATA_PRIMITIVE_CYLINDER, x + 0.45f, kCounterTop + 0.02f, z + kCounterZ + 0.15f, 0.42f, 0.02f, 0.42f,
          burner);
}

// An empty, clean station: the counter, a folded towel, the burner off.
inline void emptyStation(Host& h, const agentviz::Zone& zone) {
    if (zone.appear <= 0.02f) return;
    const float x = zone.extra[0], z = zone.extra[1];
    counter(h, x, z, 1.95f, false);
    h.add(STRATA_PRIMITIVE_CUBE, x - 0.4f, kCounterTop + 0.03f, z + kCounterZ - 0.15f, 0.35f, 0.04f, 0.25f, kWhite);
}

// The spike at the head of the line: one spent ticket per doubling of the
// tool calls the place has seen, so a busy repo's spike is crowded.
inline void ticketSpike(Host& h, float x, float z, int toolCount) {
    const int tickets = std::min(12, static_cast<int>(std::lround(std::log2(1.0f + toolCount) * 1.3f)));
    const float top = kCounterTop + 0.15f + 0.04f * tickets;
    h.add(STRATA_PRIMITIVE_CUBE, x, kCounterTop * 0.5f, z, 0.6f, kCounterTop, 0.6f, kSteelDark);
    h.add(STRATA_PRIMITIVE_CYLINDER, x, kCounterTop + 0.01f, z, 0.24f, 0.02f, 0.24f, kSteel);
    h.wideLine(x, kCounterTop, z, x, top + 0.12f, z, kSteel, 2.0f, 0.015f);
    for (int k = 0; k < tickets; ++k) {
        const float y = kCounterTop + 0.04f + 0.04f * k, twist = hash(k * 2.7f) - 0.5f;
        h.add(STRATA_PRIMITIVE_CUBE, x + twist * 0.06f, y, z + twist * 0.04f, 0.26f + 0.06f * twist, 0.012f, 0.34f,
              mix(kPaper, {0.85f, 0.8f, 0.68f}, hash(k * 5.1f)));
    }
}

inline void drawLine(Host& h, const agentviz::Zone& zone, float t) {
    const float x0 = zone.x0, x1 = zone.x1, z = zone.extra[0];
    const bool waiting = zone.extra[1] > 0.5f, off = zone.extra[2] > 0.5f;
    const float len = x1 - x0, cx = (x0 + x1) * 0.5f;
    h.add(STRATA_PRIMITIVE_CUBE, cx - 0.3f, 0.01f, z + 0.4f, len + 1.6f, 0.02f, 3.0f,
          off ? Rgb{0.1f, 0.05f, 0.04f} : Rgb{0.32f, 0.12f, 0.07f});
    h.add(STRATA_PRIMITIVE_CUBE, cx - 0.3f, 0.8f, z + 1.4f, len + 1.2f, 1.6f, 0.12f, off ? scale(kTile, 0.4f) : kTile);
    h.add(STRATA_PRIMITIVE_CUBE, cx - 0.3f, 2.95f, z + 1.45f, len + 1.2f, 0.12f, 0.25f,
          scale(kSteel, off ? 0.4f : 0.85f));
    for (float gx = x0 - 0.9f; gx <= x1; gx += 0.5f)  // grout lines on the backsplash
        h.line(gx, 0.0f, z + 1.33f, gx, 1.6f, z + 1.33f, {0.5f, 0.5f, 0.48f}, 0.5f);
    ticketSpike(h, x0 - 0.45f, z + kCounterZ, agentviz::toInt(zone.extra[3]));
    if (waiting) {
        const float p = agentviz::pulse(t, 0.8f);
        h.wideLine(x0 - 0.9f, 2.9f, z + 0.3f, x1 + 0.3f, 2.9f, z + 0.3f, kRed, 4.0f, 0.06f, 0.5f + 0.5f * p);
    }
    if (!off) h.light(cx, 2.7f, z + 0.2f, std::max(len * 0.7f, 6.0f), {1.0f, 0.95f, 0.85f}, 0.6f, 3);
}

// The red quarry-tile floor under the whole kitchen (bounds in the scene floats).
inline void drawFloor(Host& h, const float* scene) {
    const float x0 = scene[0], x1 = scene[1], z0 = scene[2], z1 = scene[3];
    h.add(STRATA_PRIMITIVE_CUBE, (x0 + x1) * 0.5f, -0.05f, (z0 + z1) * 0.5f, x1 - x0, 0.1f, z1 - z0, kFloor);
    // Grout between the quarry tiles, a metre apart, so the floor reads as a
    // kitchen's and gives the room its scale. Tiles line up with world x / z.
    const Rgb grout{0.42f, 0.24f, 0.17f};
    for (float gx = std::ceil(x0); gx <= x1; gx += 1.0f) h.line(gx, 0.004f, z0, gx, 0.004f, z1, grout, 0.8f);
    for (float gz = std::ceil(z0); gz <= z1; gz += 1.0f) h.line(x0, 0.004f, gz, x1, 0.004f, gz, grout, 0.8f);
}

inline void drawZones(Host& h, const std::vector<agentviz::Zone>& zones, float t) {
    for (const auto& zone : zones) {
        if (zone.kind == kZoneLine) drawLine(h, zone, t);
        else emptyStation(h, zone);
    }
}

}  // namespace kitchen
