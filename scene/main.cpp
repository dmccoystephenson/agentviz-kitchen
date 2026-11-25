// agentviz-kitchen: Claude Code sessions as a restaurant kitchen brigade.
//
// The page (web/kitchen-*.js, web/page.js) owns the data and the layout: it
// reads the agent feed, lays out one kitchen line per place, a pass per
// session and a station per subagent, and eases them between snapshots.
// Every frame it packs the scene in the agentviz wire format
// (vendor/agentviz-kit, docs/KIT.md in the agentviz repo) and calls
// agentviz_frame(); this file only draws what the frame says:
//   scene floats  the floor's bounds (line.h)
//   zones         kitchen lines and empty stations (line.h)
//   agents        head chefs at their passes (chef.h), line cooks at stations (station.h)
#define STRATA_KIT_MAIN
#include "strata_kit.h"

#include "chef.h"
#include "line.h"
#include "station.h"

namespace {

agentviz::Frame frame;

// A fill light so the lowest quality tier keeps its level.
void fillLight(strata_kit::Host& h, const float* scene) {
    StrataPointLight fill{};
    fill.position[0] = (scene[0] + scene[1]) * 0.5f;
    fill.position[1] = 8.0f;
    fill.position[2] = (scene[2] + scene[3]) * 0.5f;
    fill.range = 40.0f;
    fill.color[0] = 1.0f;
    fill.color[1] = 0.95f;
    fill.color[2] = 0.85f;
    fill.intensity = 0.5f;
    h.fill(fill, {0.06f, 0.055f, 0.05f}, 5);
}

// A warm, dark kitchen whatever the sun is doing.
void kitchenLighting(strata_kit::Host& h) {
    h.clearFlags = STRATA_CLEAR_COLOR | STRATA_CLEAR_DEPTH;
    h.clearColor[0] = 0.035f;
    h.clearColor[1] = 0.03f;
    h.clearColor[2] = 0.03f;
    h.ambientTint = {1.0f, 0.92f, 0.82f};
    h.ambientLift = {0.10f, 0.09f, 0.08f};
    h.sunIntensity = 0.85f;
    h.sunTint = {1.0f, 0.95f, 0.88f};
}

}  // namespace

AGENTVIZ_FRAME_EXPORT(frame)

void strata_kit_configure(strata_kit::Options& o) {
    o.name = "kitchen";
    o.tiers[0].thinPoints = true;
    o.fog = false;
}

void strata_kit_engine_ready(strata_kit::Host&) {}

void strata_kit_build(strata_kit::Host& h, double seconds) {
    kitchenLighting(h);
    if (!frame.ok) return;
    const float t = static_cast<float>(seconds);
    kitchen::drawFloor(h, frame.scene);
    kitchen::drawZones(h, frame.zones, t);
    for (const auto& agent : frame.agents) {
        if (agent.appear <= 0.02f) continue;
        const kitchen::Figure f = kitchen::figureOf(agent);
        if (agent.lead()) kitchen::drawChef(h, f, t);
        else kitchen::drawCook(h, f, t);
    }
    fillLight(h, frame.scene);
}
