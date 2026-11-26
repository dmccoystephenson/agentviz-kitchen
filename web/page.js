/* page.js: boots the kitchen. The feed client, HUD, labels and wire come
   from the agentviz kit (agentviz-*.js); this file wires them to the Kitchen
   model and the Strata kit, and owns the camera and the world labels. */
(function () {
  "use strict";
  const query = new URLSearchParams(location.search);
  const DEMO = query.get("demo") === "1";
  const WIDE_CAM = {yaw: -0.35, pitch: 0.85}, TALL_CAM = {yaw: -1.4, pitch: 0.78};
  const REASON = {question: "has a question", permission: "needs permission"};
  const REASON_ICON = {question: "❓ ", permission: "🔔 "};
  const STATION = {edit: "chopping", run: "at the stove", read: "reading the ticket", web: "fetching from the walk-in",
    delegate: "calling tickets", think: "tasting", other: "wiping down", none: "resting"};
  const {KIND} = window.Kitchen;
  const canvas = document.getElementById("view");
  const kitchen = new window.Kitchen();
  const wire = AgentvizWire.create();
  const labels = AgentvizScreen.labels(document.getElementById("labels"));
  let kit = null, mod = null, feed = null, selectedId = null, hoveredId = null, camAnim = null, fitted = false;
  let sceneT = 0;

  /* ------------------------------------------------------------ demo */
  // ?demo=1: the fixture with a scripted change every poll, so action changes
  // and per-call flares can be seen and captured. Synthetic only.
  function demoMutate(d, k) {
    const ACTS = ["edit", "run", "read", "web", "edit", "other", "read"];
    for (const s of d.sessions) {
      (s.followers || []).forEach((f, i) => {
        if (f.state !== "active") return;
        f.action = ACTS[(i * 2 + Math.floor(k / 2)) % ACTS.length];
        if ((i + k) % 3 === 0) f.toolCount = (f.toolCount || 0) + k;
      });
      if (s.state === "busy" && s.lastToolAt != null && k % 2 === 0) s.lastToolAt += k;
      if (s.name === "Wren") s.action = Math.floor(k / 4) % 2 ? "delegate" : "read";
    }
    return d;
  }

  /* ------------------------------------------------------------ camera */
  // Lines run along x and stack along z. On a wide screen the camera looks
  // down the stack from the side, so the lines stand side by side; on a tall
  // one it looks along them, so they stack top to bottom.
  function fitView() {
    const b = kitchen.bounds();
    const w = b.x1 - b.x0, d = b.z1 - b.z0;
    const aspect = canvas.clientWidth / Math.max(1, canvas.clientHeight);
    const wide = aspect >= 1;
    const across = wide ? d : w, along = wide ? w : d;
    const span = Math.max(across / Math.min(aspect, 1.9) * (wide ? 1.55 : 1.1), along * (wide ? 1.3 : 0.95));
    return Object.assign({dist: Math.min(110, Math.max(16, span)), tx: (b.x0 + b.x1) / 2, ty: 0.5,
      tz: (b.z0 + b.z1) / 2}, wide ? WIDE_CAM : TALL_CAM);
  }
  function fit(instant) { if (kitchen.lines.length) flyTo(fitView(), instant); }
  function flyTo(to, instant) {
    if (!kit) return;
    if (instant || kit.driven) { Object.assign(kit.cam, to); camAnim = null; return; }
    camAnim = {from: Object.assign({}, kit.cam), to, t0: performance.now(), ms: 700};
  }
  function stepCam() {
    if (!camAnim) return;
    const k = Math.min(1, (performance.now() - camAnim.t0) / camAnim.ms), e = AgentvizFeed.smoothstep(k);
    for (const key of Object.keys(camAnim.to)) kit.cam[key] = AgentvizFeed.lerp(camAnim.from[key], camAnim.to[key], e);
    if (k >= 1) camAnim = null;
  }
  // Moves in on an agent. On a phone the card is a bottom sheet over the
  // lower half, so the agent is framed in the part above it.
  const SHEET = matchMedia("(max-width: 640px)");
  function focusOn(e) {
    const dist = Math.min(kit.cam.dist, 13);
    let tx = e.tx, tz = e.tz;
    if (SHEET.matches) {
      ({tx, tz} = AgentvizScreen.reveal(kit.cam, [e.tx, 1, e.tz], hud.freeRect(canvas), {set: {dist, ty: 1}, force: true}));
    }
    flyTo({tx, tz, ty: 1, dist});
  }

  /* ------------------------------------------------------------ labels */
  function accentCss(seed) { return "hsl(" + Math.round((seed * 360) % 360) + " 65% 50%)"; }

  function chefLabel(e) {
    const s = e.session, waiting = s.state === "waiting", offline = s.state === "offline";
    const text = waiting ? (REASON_ICON[s.waitingReason] || "🔔 ") + s.name + " " + (REASON[s.waitingReason] || "needs you")
      : (s.stalled ? "💨 " : "") + (s.name || s.id);
    const el = labels.set("a:" + e.id, {cls: "chef " + s.state, text, state: waiting ? null : s.stalled ? "stalled" : s.state,
      action: s.state === "busy" ? s.action : null});
    el.style.borderColor = waiting ? "" : accentCss(s.seed);
    return {el, q: kit.project(e.x, offline ? 2.4 : 2.75, e.z), rank: waiting ? -1 : e.id === selectedId ? 0 : 1};
  }
  function ticketLabel(e) {
    const f = e.follower;
    const cls = "ticket" + (f.state === "resting" ? " resting" : "") + (f.stalled ? " stalled" : "");
    const el = labels.set("a:" + e.id, {cls, text: (f.stalled ? "💨 " : "") + (f.label || "subagent"),
      action: f.state === "active" ? f.action : null});
    return {el, q: kit.project(e.x, 2.45, e.z + 0.55), rank: e.id === selectedId ? 0 : 2};
  }
  function placeLabels() {
    const items = [];
    labels.begin();
    for (const l of kitchen.lines) {
      const el = labels.set("p:" + l.id, {cls: "place", text: l.label});
      items.push({el, q: kit.project(l.x0 + 0.3, 3.15, l.z + 0.4), rank: 1});
    }
    for (const e of kitchen.agents()) {
      if (e.appear < 0.3) continue;
      items.push(e.kind === KIND.chef ? chefLabel(e) : ticketLabel(e));
    }
    labels.end();
    StrataKit.placeLabels(clearOfHud(items));
  }

  // Labels never sit under the top bar or the bottom sheet: one whose anchor
  // falls there is hidden (a waiting chef's too; the chip names it).
  function clearOfHud(items) {
    const r = canvas.getBoundingClientRect(), {top} = hud.clearArea(canvas), p = hud.panelRect();
    const bottom = (p && p.sheet ? p.y0 : innerHeight) - r.top;
    for (const it of items) if (it.q && (it.q.y - 18 < top || it.q.y > bottom)) it.q = null;
    return items;
  }

  /* ------------------------------------------------------------ picking + selection */
  function pick(x, y, radius) {
    const candidates = kitchen.agents().filter((e) => e.appear >= 0.3)
      .map((e) => ({item: e, points: [0.6, 1.2, 1.7].map((h) => [e.x, h, e.z])}));
    return AgentvizScreen.pick(kit.project, candidates, x, y, radius);
  }
  const targetOf = (e) => (e ? {session: e.session, follower: e.follower} : null);
  const idOf = (target) => (target ? (target.follower ? AgentvizFeed.followerKey(target.session, target.follower)
    : target.session.id) : null);

  function onSelect(target, how) {
    selectedId = idOf(target);
    const e = selectedId && kitchen.entities.get(selectedId);
    kit.selected = e || null;  // the idle orbit pauses while something is selected
    if (e && how !== "api") focusOn(e);
  }

  // The kitchen's word for what the selected agent is doing.
  function sessionDoing(s) {
    if (s.state === "waiting") return s.waitingReason === "question" ? "holding up a question" : "plate up, ringing for you";
    if (s.state === "offline") return "pass dark, apron hung up";
    return s.state === "busy" ? STATION[s.action] || s.action : "standing by";
  }
  function stationRows({session, follower}) {
    if (!follower) return [["at the pass", sessionDoing(session)]];
    return [["station", follower.state === "active" ? STATION[follower.action] || follower.action : STATION.none]];
  }

  const hud = AgentvizHud.create({
    title: "Kitchen brigade",
    oneRow: true,
    legend: document.getElementById("legend"),
    buttons: [{icon: "home", label: "Recentre the view", onClick: () => fit(false)}],
    onSelect,
    rows: stationRows,
  });

  function accept(doc) {
    kitchen.update(doc);
    hud.update(doc);
    if (!fitted && kitchen.lines.length) { fitted = true; fit(true); }
  }

  /* ------------------------------------------------------------ boot */
  function frame(t) {
    if (!feed) return;
    sceneT = t;
    stepCam();
    kitchen.step(t);
    const highlight = (id) => (id === selectedId ? 1 : id === hoveredId ? 2 : 0);
    window.KitchenFrame.write(wire, kitchen, feed.doc, highlight, feed.pulse).send(mod);
  }

  function screenTargets() {
    const r = canvas.getBoundingClientRect();
    return kitchen.agents().filter((e) => e.appear > 0.5).map((e) => {
      const s = kit.project(e.x, 1.2, e.z);
      return s && {id: e.id, x: s.x + r.left, y: s.y + r.top};
    }).filter((t) => t && t.x > 0 && t.y > 60 && t.x < innerWidth && t.y < innerHeight - 10);
  }

  function createKit() {
    return StrataKit.create({
      module: mod, canvas,
      camera: Object.assign({dist: 46, tx: 0, ty: 0.5, tz: 0}, WIDE_CAM),
      limits: {maxDist: 120, minDist: 4},
      frame,
      after: placeLabels,
      pick,
      idOf: (e) => e.id,
      onTap: (e) => hud.select(targetOf(e), "tap"),
      onHover(e) { hoveredId = e ? e.id : null; canvas.style.cursor = e ? "pointer" : ""; },
      onSlow: () => StrataLoader.slow(),
      onFatal: (code, msg) => StrataLoader.fallback(code, msg),
      targets: screenTargets,
    });
  }

  function exposeForTests() {
    window.__page = {kit, kitchen, hud, fit, flyTo, demoMutate,
      select: (id, focus) => hud.select(targetOf(kitchen.entities.get(id)), focus ? "list" : "api"),
      view(cam) { camAnim = null; Object.assign(kit.cam, cam); },
      legend(open) { document.querySelector(".av-legend").hidden = !open; },
      get doc() { return feed.doc; }, get polls() { return feed.polls; },
      stopPolling: () => feed.stop(), pollOnce: () => feed.pollOnce(), feed: (d) => feed.push(d)};
  }

  async function boot() {
    try {
      mod = await StrataLoader.load({script: "kitchen-engine.js", factory: "createKitchenEngine",
        wasm: "kitchen-engine.wasm", title: "the kitchen"});
    } catch (error) {
      console.error("kitchen: the engine did not load", error);
      return;  // StrataLoader has already shown its fallback
    }
    try {
      kit = createKit();
    } catch (error) {
      StrataLoader.fallback(error.code === "no-webgl2" ? "no-webgl2" : "engine");
      return;
    }
    // Under ?drive=1 scene time is the clock, so per-call flares fade with the frames drawn.
    feed = AgentvizFeed.create({onUpdate: accept, onStatus: hud.setStatus, transform: DEMO ? demoMutate : null,
      now: () => (kit.driven ? sceneT : Date.now() / 1000)});
    exposeForTests();
    kit.setSun(Date.UTC(2026, 5, 21, 18, 0, 0) / 1000, 40, -105);
    feed.start();
    kit.start();
    if (kit.driven) kit.drive(0);
    StrataLoader.done();
    canvas.focus({preventScroll: true});
  }

  boot();
})();
