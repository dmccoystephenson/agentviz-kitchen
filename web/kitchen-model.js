/* kitchen-model.js: lays the kitchen out from the feed and eases it.

   Each place (repo) is one kitchen line, a row along x; lines stack along z,
   sorted by place id. Each session is a head chef at a pass on its line;
   its subagents are cooks at stations after the pass, and every cook keeps
   its station slot while it stays in the feed. Lines with fewer than two
   cooks per chef get clean, empty stations so a brigade always reads as one.

   Entities are keyed by id: a session's id, a follower's "<session>/<follower>"
   (AgentvizFeed.followerKey), an empty station's "<session>#e<slot>". They
   ease to their targets and fade in and out (`appear`).

   Classic script: window.Kitchen. Needs AgentvizFeed. */
(function () {
  "use strict";
  const {ease, followerKey} = window.AgentvizFeed;
  const LINE_GAP = 7.5, STRIDE = 2.0, PASS_TO_STATION = 2.2, MIN_STATIONS = 2;
  const EASE_RATE = 4, GONE_BELOW = 0.03;
  const KIND = {chef: "chef", cook: "cook", empty: "empty"};

  class Kitchen {
    constructor() {
      this.lines = [];             // [{id, label, x0, x1, z, waiting, offline, toolCount}]
      this.entities = new Map();   // id -> {id, kind, session, follower, x, z, tx, tz, appear, seen, ...}
      this.slots = new Map();      // session id -> Map(follower id -> station slot)
      this.lastT = null;
    }

    /* ------------------------------------------------------------ layout */

    // Stable station slots: a cook keeps its slot while it is listed; new
    // cooks take the lowest free one.
    slotsFor(session) {
      const ids = session.followers.map((f) => f.id);
      let slots = this.slots.get(session.id);
      if (!slots) this.slots.set(session.id, (slots = new Map()));
      for (const id of [...slots.keys()]) if (!ids.includes(id)) slots.delete(id);
      const used = new Set(slots.values());
      for (const id of ids) {
        if (slots.has(id)) continue;
        let slot = 0;
        while (used.has(slot)) slot++;
        slots.set(id, slot);
        used.add(slot);
      }
      return slots;
    }

    // One line's brigades, left to right, in line-local x.
    brigadesOf(sessions) {
      let cursor = 0;
      const brigades = sessions.map((session) => {
        const slots = this.slotsFor(session);
        const stations = Math.max(MIN_STATIONS, ...[...slots.values()].map((slot) => slot + 1));
        const passX = cursor + 1.1, firstStation = passX + PASS_TO_STATION;
        cursor = firstStation + stations * STRIDE - 0.4;
        return {session, slots, stations, passX, firstStation};
      });
      return {brigades, length: cursor};
    }

    upsert(id, kind, props) {
      let e = this.entities.get(id);
      if (!e) {
        e = {id, kind, x: props.tx, z: props.tz, appear: 0};
        this.entities.set(id, e);
      }
      return Object.assign(e, props, {seen: true});
    }

    placeBrigade(b, ox, z) {
      const s = b.session;
      this.upsert(s.id, KIND.chef, {session: s, follower: null, tx: ox + b.passX, tz: z,
        firstStation: ox + b.firstStation, stations: b.stations});
      const bySlot = new Map(s.followers.map((f) => [b.slots.get(f.id), f]));
      for (let slot = 0; slot < b.stations; slot++) {
        const f = bySlot.get(slot), x = ox + b.firstStation + slot * STRIDE;
        if (f) this.upsert(followerKey(s, f), KIND.cook, {session: s, follower: f, tx: x, tz: z});
        else this.upsert(s.id + "#e" + slot, KIND.empty, {session: s, follower: null, tx: x, tz: z});
      }
    }

    update(doc) {
      for (const e of this.entities.values()) e.seen = false;
      const places = [...(doc.places || [])].sort((a, b) => a.id.localeCompare(b.id));
      const built = places.map((place) =>
        Object.assign({place}, this.brigadesOf(doc.sessions.filter((s) => s.place === place.id))));
      const longest = Math.max(0, ...built.map((b) => b.length));
      const ox = -longest / 2, oz = -((places.length - 1) * LINE_GAP) / 2;
      this.lines = built.map((line, i) => {
        const z = oz + i * LINE_GAP, sessions = line.brigades.map((b) => b.session);
        for (const b of line.brigades) this.placeBrigade(b, ox, z);
        return {id: line.place.id, label: line.place.label, x0: ox, x1: ox + Math.max(line.length, 4), z,
          waiting: sessions.some((s) => s.state === "waiting"),
          offline: sessions.length > 0 && sessions.every((s) => s.state === "offline"),
          toolCount: line.place.toolCount || 0};
      });
    }

    /* ------------------------------------------------------------ easing */

    step(t) {
      const dt = this.lastT == null ? 0.016 : Math.min(0.25, Math.max(0, t - this.lastT));
      this.lastT = t;
      for (const [id, e] of this.entities) {
        e.x = ease(e.x, e.tx, dt, EASE_RATE);
        e.z = ease(e.z, e.tz, dt, EASE_RATE);
        e.appear = ease(e.appear, e.seen ? 1 : 0, dt, EASE_RATE);
        if (!e.seen && e.appear < GONE_BELOW) this.entities.delete(id);
      }
    }

    // Jump every entity to where it is going (captures and tests).
    settle() {
      for (const e of this.entities.values()) { e.x = e.tx; e.z = e.tz; e.appear = e.seen ? 1 : 0; }
    }

    /* ------------------------------------------------------------ queries */

    agents() { return [...this.entities.values()].filter((e) => e.kind !== KIND.empty); }

    findChef(name) {
      for (const e of this.entities.values()) if (e.kind === KIND.chef && e.session.name === name) return e;
      return null;
    }

    // The floor under every line, with a margin.
    bounds() {
      const b = {x0: -10, x1: 10, z0: -6, z1: 6};
      for (const l of this.lines) {
        b.x0 = Math.min(b.x0, l.x0 - 3); b.x1 = Math.max(b.x1, l.x1 + 3);
        b.z0 = Math.min(b.z0, l.z - 4); b.z1 = Math.max(b.z1, l.z + 4);
      }
      return b;
    }
  }

  Kitchen.KIND = KIND;
  Kitchen.STRIDE = STRIDE;
  window.Kitchen = Kitchen;
})();
