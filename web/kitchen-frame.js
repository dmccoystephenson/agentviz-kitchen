/* kitchen-frame.js: packs the kitchen for the scene, once a frame, in the
   agentviz wire format (vendor/agentviz-kit, docs/KIT.md in agentviz).
   scene/line.h, chef.h and station.h read what is written here.

     scene floats  floor x0, x1, z0, z1
     zones         kind 0 a kitchen line: extras line z, anyone waiting, all offline, place toolCount
                   kind 1 an empty station: extras station x, z
     agents        chefs: extras first station x, stations, station stride
                   cooks: no extras (position, seed, state and action are the common fields)
     links         none

   Classic script: window.KitchenFrame. */
(function () {
  "use strict";
  const {KIND, STRIDE} = window.Kitchen;
  const ZONE = {line: 0, station: 1};

  function line(wire, l) {
    wire.zone({id: "line:" + l.id, kind: ZONE.line, x0: l.x0, z0: l.z - 1.1, x1: l.x1, z1: l.z + 1.9,
      extra: [l.z, l.waiting ? 1 : 0, l.offline ? 1 : 0, l.toolCount]});
  }

  function emptyStation(wire, e) {
    wire.zone({id: "station:" + e.id, kind: ZONE.station, x0: e.x - STRIDE / 2, z0: e.z, x1: e.x + STRIDE / 2,
      z1: e.z + 1.3, appear: e.appear, extra: [e.x, e.z]});
  }

  // A chef's rail and stations slide with it while it eases to a new spot.
  function chefExtras(e) { return [e.firstStation + (e.x - e.tx), e.stations, STRIDE]; }

  // highlightOf(id) -> 0 none, 1 selected, 2 hovered; pulseOf(id) -> 0..1.
  function write(wire, kitchen, doc, highlightOf, pulseOf) {
    const b = kitchen.bounds();
    wire.begin({doc, scene: [b.x0, b.x1, b.z0, b.z1]});
    for (const l of kitchen.lines) line(wire, l);
    const entities = [...kitchen.entities.values()];
    for (const e of entities) if (e.kind === KIND.empty) emptyStation(wire, e);
    for (const e of entities) {
      if (e.kind === KIND.empty) continue;
      wire.agent({session: e.session, follower: e.follower, x: e.x, z: e.z, appear: e.appear,
        highlight: highlightOf(e.id), pulse: pulseOf(e.id), extra: e.kind === KIND.chef ? chefExtras(e) : []});
    }
    return wire;
  }

  window.KitchenFrame = {write, ZONE};
})();
