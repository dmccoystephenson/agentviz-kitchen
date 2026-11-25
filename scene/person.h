// person.h: a chef or a cook. A pose is where the two hands are, how far
// the body leans toward the counter and how far it sinks (resting cooks).
#pragma once

#include "draw_util.h"

namespace kitchen {

struct Vec3 {
    float x, y, z;
};

struct Pose {
    Vec3 left, right;
    float lean = 0.0f, drop = 0.0f;
};

inline void arm(Host& h, float shoulderX, float shoulderY, float bodyZ, Vec3 hand, Rgb sleeve, float s) {
    h.wideLine(shoulderX, shoulderY, bodyZ, hand.x, hand.y, hand.z, sleeve, 3.0f, 0.13f * s);
    h.add(STRATA_PRIMITIVE_SPHERE, hand.x, hand.y, hand.z, 0.11f * s, 0.11f * s, 0.11f * s, kSkin);
}

// Legs, a jacket, an apron and neckerchief in the brigade's colour, a head,
// a hat (a tall toque for the head chef, a cap for a cook) and two sleeves to
// the hands. Leaning tips the body toward the counter, the head further and
// a little to one side, so it reads from behind too.
inline void person(Host& h, float x, float z, float s, bool chef, Rgb accent, Rgb jacket, const Pose& p) {
    const float bz = z + p.lean * 0.5f, hz = z + p.lean * 1.3f, hx = x + p.lean * 0.6f, drop = p.drop;
    for (float side : {-1.0f, 1.0f})
        h.add(STRATA_PRIMITIVE_CYLINDER, x + 0.1f * side * s, 0.27f * s - drop * 0.5f, z, 0.16f * s, 0.27f * s,
              0.18f * s, kTrousers);
    h.add(STRATA_PRIMITIVE_CAPSULE, x, 0.95f * s - drop, bz, 0.5f * s, 0.42f * s, 0.38f * s, jacket);
    h.add(STRATA_PRIMITIVE_CUBE, x, 0.72f * s - drop, bz, 0.44f * s, 0.34f * s, 0.40f * s, accent);
    h.add(STRATA_PRIMITIVE_SPHERE, x, 1.36f * s - drop, bz, 0.40f * s, 0.12f * s, 0.36f * s, accent);
    const float hy = 1.58f * s - drop;
    h.add(STRATA_PRIMITIVE_SPHERE, hx, hy, hz, 0.34f * s, 0.36f * s, 0.34f * s, kSkin);
    if (chef) {
        h.add(STRATA_PRIMITIVE_CYLINDER, hx, hy + 0.32f * s, hz, 0.34f * s, 0.16f * s, 0.34f * s, kWhite);
        h.add(STRATA_PRIMITIVE_SPHERE, hx, hy + 0.50f * s, hz, 0.46f * s, 0.26f * s, 0.46f * s, kWhite);
    } else {
        h.add(STRATA_PRIMITIVE_CYLINDER, hx, hy + 0.17f * s, hz, 0.34f * s, 0.05f * s, 0.34f * s, accent);
    }
    const float sy = 1.25f * s - drop;
    arm(h, x - 0.24f * s, sy, bz, p.left, jacket, s);
    arm(h, x + 0.24f * s, sy, bz, p.right, jacket, s);
}

}  // namespace kitchen
