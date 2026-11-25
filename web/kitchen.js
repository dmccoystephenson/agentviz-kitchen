/* agentviz-kitchen page: polls /api/agents, lays the kitchen out, eases
   between snapshots and hands the Strata scene its rows every frame.
   Row layouts must match scene/main.cpp (kLineStride, kFigureStride). */
(function () {
  "use strict";
  const q = new URLSearchParams(location.search);
  const FIXTURE = q.get("fixture") === "1";
  const FEED = "/api/agents" + (FIXTURE ? "?fixture=1" : "");
  const LINE_STRIDE = 8, FIG_STRIDE = 16;
  const LINE_GAP = 7.5, STRIDE = 2.0, PASS_TO_STATION = 2.2, MIN_STATIONS = 2;
  const KIND = {chef: 0, cook: 1, empty: 2};
  const ACTION = {none: 0, edit: 1, run: 2, read: 3, web: 4, delegate: 5, think: 6, other: 7};
  const STATE = {offline: 0, dim: 1, idle: 2, busy: 3, waiting: 4, resting: 5, active: 6};
  const ICON = {edit: "🔪", run: "🔥", read: "🧾", web: "🚪", delegate: "📣", think: "🥄", other: "🧽", none: "☕"};
  const WORD = {edit: "chopping (edit)", run: "at the stove (run)", read: "reading the ticket (read)",
    web: "fetching from the walk-in (web)", delegate: "calling tickets (delegate)", think: "tasting (think)",
    other: "wiping down (other)", none: "—"};
  const STATE_ICON = {busy: "🔥", waiting: "🔔", idle: "🧊", offline: "🌑", dim: "🌫️", active: "🔥", resting: "☕"};

  let doc = null, kit = null, mod = null;
  let lines = [];                 // [{id,label,x0,x1,z,waiting,off}]
  const ents = new Map();         // id -> entity (chef, cook, empty station)
  const slots = new Map();        // session id -> Map(follower id -> slot)
  const labelsEl = document.getElementById("labels");
  const placeLabels = new Map();
  let selectedId = null, camGoal = null, firstLayout = true;
  let linePtr = 0, lineCap = 0, figPtr = 0, figCap = 0;

  function accent(seed) {  // hsl(seed) -> linear rgb
    const h = (seed * 360) % 360, s = 0.65, l = 0.5;
    const k = (n) => (n + h / 30) % 12, a = s * Math.min(l, 1 - l);
    const f = (n) => l - a * Math.max(-1, Math.min(k(n) - 3, Math.min(9 - k(n), 1)));
    return [f(0), f(8), f(4)].map((c) => Math.pow(c, 2.2));
  }
  function cssAccent(seed) { return `hsl(${Math.round((seed * 360) % 360)} 65% 50%)`; }

  function label(cls) {
    const el = document.createElement("div");
    el.className = "lbl " + cls;
    labelsEl.appendChild(el);
    return el;
  }

  function upsert(id, kind, props) {
    let e = ents.get(id);
    if (!e) {
      e = {id, kind, x: props.tx, z: props.tz, pres: 0, el: kind === KIND.empty ? null : label(kind === KIND.chef ? "chef" : "ticket")};
      ents.set(id, e);
    }
    Object.assign(e, props, {seen: true});
    return e;
  }

  // Stable station slots: a cook keeps its station while it is listed.
  function slotFor(sid, fids) {
    let m = slots.get(sid);
    if (!m) slots.set(sid, (m = new Map()));
    for (const k of [...m.keys()]) if (!fids.includes(k)) m.delete(k);
    const used = new Set(m.values());
    for (const f of fids) {
      if (m.has(f)) continue;
      let s = 0;
      while (used.has(s)) s++;
      m.set(f, s);
      used.add(s);
    }
    return m;
  }

  function layout(d) {
    doc = d;
    for (const e of ents.values()) e.seen = false;
    const places = [...d.places].sort((a, b) => a.id.localeCompare(b.id));
    const built = [];
    let maxLen = 0;
    places.forEach((p, i) => {
      const sess = d.sessions.filter((s) => s.place === p.id);
      let cursor = 0;
      const items = [];
      for (const s of sess) {
        const m = slotFor(s.id, s.followers.map((f) => f.id));
        const count = Math.max(MIN_STATIONS, ...[...m.values()].map((v) => v + 1));
        const passX = cursor + 1.1;
        const st0 = passX + PASS_TO_STATION;
        items.push({s, m, count, passX, st0});
        cursor = st0 + count * STRIDE - 0.4;
      }
      maxLen = Math.max(maxLen, cursor);
      built.push({p, items, len: cursor, z: i * LINE_GAP});
    });
    const ox = -maxLen / 2, oz = -((places.length - 1) * LINE_GAP) / 2;
    lines = built.map((b) => {
      const z = b.z + oz;
      const anyWaiting = b.items.some((it) => it.s.state === "waiting");
      const off = b.items.length > 0 && b.items.every((it) => it.s.state === "offline");
      for (const it of b.items) {
        const s = it.s, col = accent(s.seed);
        const state = STATE[s.state] != null ? STATE[s.state] : STATE.dim;
        upsert(s.id, KIND.chef, {tx: ox + it.passX, tz: z, action: ACTION[s.action] || 0, state, stalled: !!s.stalled,
          col, phase: s.seed, st0: ox + it.st0, count: it.count, data: s, lead: null});
        const byslot = new Map();
        for (const f of s.followers) byslot.set(it.m.get(f.id), f);
        for (let k = 0; k < it.count; k++) {
          const f = byslot.get(k), x = ox + it.st0 + k * STRIDE;
          if (f) {
            upsert(s.id + "/" + f.id, KIND.cook, {tx: x, tz: z, action: f.state === "active" ? (ACTION[f.action] || 0) : 0,
              state: f.state === "active" ? STATE.active : STATE.resting, stalled: false, col, phase: f.seed != null ? f.seed : (k * 0.37) % 1,
              data: f, lead: s});
          } else {
            upsert(s.id + "#e" + k, KIND.empty, {tx: x, tz: z, action: 0, state: STATE.idle, stalled: false, col,
              phase: 0, data: null, lead: s});
          }
        }
      }
      return {id: b.p.id, label: b.p.label, x0: ox, x1: ox + Math.max(b.len, 4), z, waiting: anyWaiting, off};
    });
    for (const l of lines) if (!placeLabels.has(l.id)) {
      const el = label("place");
      el.textContent = l.label;
      placeLabels.set(l.id, el);
    }
    for (const [id, el] of placeLabels) if (!lines.some((l) => l.id === id)) { el.remove(); placeLabels.delete(id); }
    if (firstLayout && kit) {
      firstLayout = false;
      const depth = places.length * LINE_GAP, w = maxLen;
      Object.assign(kit.cam, {dist: Math.max(16, Math.max(depth * 0.95, w * 1.0)), tx: 0, ty: 0.5, tz: 0});
    }
    renderHud();
    if (selectedId) renderSheet();
  }

  /* -------------------------------------------------------- per frame */
  let lastT = null;
  function frame(t) {
    const dt = lastT == null ? 0.016 : Math.min(0.25, Math.max(0, t - lastT));
    lastT = t;
    const k = 1 - Math.exp(-dt * 4);
    for (const [id, e] of ents) {
      e.x += (e.tx - e.x) * k;
      e.z += (e.tz - e.z) * k;
      e.pres += ((e.seen ? 1 : 0) - e.pres) * k;
      if (!e.seen && e.pres < 0.03) {
        if (e.el) e.el.remove();
        ents.delete(id);
        if (selectedId === id) select(null);
      }
    }
    if (camGoal) {
      const c = kit.cam;
      for (const key of Object.keys(camGoal)) c[key] += (camGoal[key] - c[key]) * Math.min(1, k * 1.5);
      if (Object.keys(camGoal).every((key) => Math.abs(camGoal[key] - c[key]) < 0.02)) camGoal = null;
    }
    // rows
    const ln = lines.length;
    if (ln * LINE_STRIDE > lineCap) { if (linePtr) mod._free(linePtr); lineCap = ln * LINE_STRIDE + 64; linePtr = mod._malloc(lineCap * 4); }
    let h = mod.HEAPF32, b = linePtr >> 2;
    lines.forEach((l, i) => {
      const o = b + i * LINE_STRIDE;
      h[o] = l.x0; h[o + 1] = l.x1; h[o + 2] = l.z; h[o + 3] = l.waiting ? 1 : 0; h[o + 4] = l.off ? 1 : 0;
      h[o + 5] = h[o + 6] = h[o + 7] = 0;
    });
    mod._kt_set_lines(linePtr, ln);
    const n = ents.size;
    if (n * FIG_STRIDE > figCap) { if (figPtr) mod._free(figPtr); figCap = n * FIG_STRIDE + 256; figPtr = mod._malloc(figCap * 4); }
    h = mod.HEAPF32; b = figPtr >> 2;
    let i = 0;
    for (const e of ents.values()) {
      const o = b + i++ * FIG_STRIDE;
      const chef = ents.get(e.kind === KIND.chef ? e.id : (e.lead && e.lead.id));
      h[o] = e.kind; h[o + 1] = e.x; h[o + 2] = e.z; h[o + 3] = e.action; h[o + 4] = e.state; h[o + 5] = e.stalled ? 1 : 0;
      h[o + 6] = e.col[0]; h[o + 7] = e.col[1]; h[o + 8] = e.col[2]; h[o + 9] = e.pres; h[o + 10] = e.id === selectedId ? 1 : 0;
      h[o + 11] = chef ? chef.st0 + (e.kind === KIND.chef ? e.x - e.tx : 0) : 0;
      h[o + 12] = chef ? chef.count : 0; h[o + 13] = STRIDE; h[o + 14] = e.phase || 0; h[o + 15] = 0;
    }
    mod._kt_set_figures(figPtr, n);
  }

  function after() {
    const items = [];
    for (const l of lines) items.push({el: placeLabels.get(l.id), q: kit.project(l.x0 + 0.3, 3.15, l.z + 0.4), rank: 1});
    for (const e of ents.values()) {
      if (!e.el) continue;
      if (e.kind === KIND.chef) {
        const s = e.data, w = s.state === "waiting";
        const txt = (w ? "🔔 " : s.stalled ? "💨 " : s.state === "offline" ? "🌑 " : "") + s.name;
        if (e.el.textContent !== txt) e.el.textContent = txt;
        e.el.className = "lbl chef " + s.state;
        e.el.style.borderColor = w ? "" : cssAccent(s.seed);
        items.push({el: e.el, q: e.pres > 0.3 ? kit.project(e.x, s.state === "offline" ? 2.4 : 2.75, e.z) : null,
          rank: w ? -1 : e.id === selectedId ? 0 : 1});
      } else {
        const f = e.data, txt = f.label || "subagent";
        if (e.el.textContent !== txt) e.el.textContent = txt;
        e.el.className = "lbl ticket" + (f.state === "resting" ? " resting" : "");
        items.push({el: e.el, q: e.pres > 0.3 ? kit.project(e.x, 2.45, e.z + 0.55) : null, rank: e.id === selectedId ? 0 : 2});
      }
    }
    StrataKit.placeLabels(items);
  }

  function pick(x, y, radius) {
    let best = null, bestD = radius * radius;
    for (const e of ents.values()) {
      if (e.kind === KIND.empty || e.pres < 0.3) continue;
      for (const yy of [0.6, 1.2, 1.7]) {
        const s = kit.project(e.x, yy, e.z);
        if (!s) continue;
        const d = (s.x - x) ** 2 + (s.y - y) ** 2;
        if (d < bestD) { bestD = d; best = e; }
      }
    }
    return best;
  }

  /* -------------------------------------------------------- UI */
  function esc(s) { return String(s == null ? "" : s).replace(/[&<>"]/g, (c) => ({"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;"}[c])); }

  function select(id, focus) {
    selectedId = id;
    const e = id && ents.get(id);
    if (e && focus) camGoal = {tx: e.tx, tz: e.tz, ty: 1, dist: Math.min(kit.cam.dist, 13)};
    renderSheet();
  }

  function renderSheet() {
    const sheet = document.getElementById("sheet"), body = document.getElementById("sheetBody");
    const e = selectedId && ents.get(selectedId);
    if (!e) { sheet.classList.remove("open"); return; }
    sheet.classList.add("open");
    const row = (ic, k, v) => `<div class="row"><span class="ic">${ic}</span><span class="k">${k}</span><span>${v}</span></div>`;
    let html;
    if (e.kind === KIND.chef) {
      const s = e.data, act = s.state === "busy" ? s.action : "none";
      const st = s.state === "waiting" ? `<span class="state-waiting">needs you</span>` : esc(s.state) + (s.stalled ? " · stalled (boiling over)" : "");
      html = `<h2>👨‍🍳 ${esc(s.name)}<button class="close" aria-label="Close">×</button></h2>` +
        row(STATE_ICON[s.state] || "·", "state", st) +
        row(ICON[act] || "·", "action", esc(WORD[act] || act)) +
        (s.activity && s.activity.label ? row("🏷️", "label", esc(s.activity.label)) : "") +
        row("🍽️", "line", esc(s.placeLabel)) +
        row("🌿", "branch", esc(s.branch || "—")) +
        row("🧑‍🍳", "cooks", String(s.followers.length)) +
        s.followers.map((f) => `<div class="follow" data-id="${esc(s.id + "/" + f.id)}"><span>${f.state === "active" ? ICON[f.action] || "·" : "☕"}</span><span class="t">${esc(f.label)}</span></div>`).join("");
    } else {
      const f = e.data, s = e.lead, act = f.state === "active" ? f.action : "none";
      html = `<h2>🧑‍🍳 ${esc(f.label)}<button class="close" aria-label="Close">×</button></h2>` +
        row(STATE_ICON[f.state] || "·", "state", f.state === "active" ? "working" : "resting") +
        row(ICON[act] || "·", "action", esc(WORD[act] || act)) +
        (f.toolLabel ? row("🏷️", "label", esc(f.toolLabel)) : "") +
        row("👨‍🍳", "chef", `<a href="#" data-id="${esc(s.id)}" style="color:inherit">${esc(s.name)}</a>`) +
        row("🌿", "branch", esc(s.branch || "—")) +
        row("🔢", "tools", String(f.toolCount != null ? f.toolCount : "—"));
    }
    body.innerHTML = html;
    body.querySelector(".close").onclick = () => select(null);
    body.querySelectorAll("[data-id]").forEach((el) => { el.onclick = (ev) => { ev.preventDefault(); select(el.dataset.id, true); }; });
  }

  function renderHud() {
    const c = doc.counts || {};
    document.getElementById("demo").hidden = !doc.fixture;
    document.getElementById("busy").textContent = "🔥 " + (c.busy || 0);
    document.getElementById("cooks").textContent = "🧑‍🍳 " + (c.activeFollowers || 0);
    const o = document.getElementById("order"), w = c.waiting || 0;
    o.hidden = w === 0;
    o.textContent = "🔔 " + w + " order" + (w === 1 ? "" : "s") + " up";
    o.classList.toggle("hot", w > 0);
  }
  let waitIdx = 0;
  document.getElementById("order").onclick = () => {
    const ws = doc.sessions.filter((s) => s.state === "waiting");
    if (ws.length) select(ws[waitIdx++ % ws.length].id, true);
  };
  document.getElementById("legendBtn").onclick = (ev) => {
    const l = document.getElementById("legend"), open = l.classList.toggle("open");
    ev.currentTarget.setAttribute("aria-expanded", String(open));
  };

  async function poll() {
    try {
      const r = await fetch(FEED, {cache: "no-store"});
      if (r.ok) layout(await r.json());
    } catch (e) { /* the server is gone for a moment: keep the last kitchen */ }
  }

  (async () => {
    try {
      mod = await StrataLoader.load({script: "kitchen-engine.js", factory: "createKitchenEngine",
        wasm: "kitchen-engine.wasm", title: "the kitchen"});
    } catch (e) { return; }
    const canvas = document.getElementById("view");
    try {
      kit = StrataKit.create({
        module: mod, canvas,
        camera: {yaw: -1.4, pitch: 0.78, dist: 34, tx: 0, ty: 0.5, tz: 0},
        limits: {maxDist: 90, minDist: 4},
        frame, after, pick,
        idOf: (e) => e && e.id,
        onTap(e) { select(e ? e.id : null, !!e); },
        onSlow: () => StrataLoader.slow(),
        onFatal: (code, msg) => StrataLoader.fallback(code, msg),
        targets: () => [...ents.values()].filter((e) => e.kind !== KIND.empty).map((e) => {
          const s = kit.project(e.x, 1.2, e.z), r = canvas.getBoundingClientRect();
          return s && {id: e.id, x: s.x + r.left, y: s.y + r.top};
        }).filter((t) => t && t.x > 0 && t.y > 60 && t.x < innerWidth && t.y < innerHeight - 10),
      });
    } catch (e) {
      return StrataLoader.fallback(e.code === "no-webgl2" ? "no-webgl2" : "engine");
    }
    kit.setSun(Date.UTC(2026, 5, 21, 18, 0, 0) / 1000, 40, -105);
    await poll();
    setInterval(poll, 2000);
    kit.start();
    if (kit.driven) kit.drive(0);
    StrataLoader.done();
    canvas.focus({preventScroll: true});
    // For tests and capture tools.
    window.kitchen = {
      get ready() { return !!doc; },
      ids: () => [...ents.keys()],
      select: (id, focus) => select(id, focus),
      settle() { for (const e of ents.values()) { e.x = e.tx; e.z = e.tz; e.pres = e.seen ? 1 : 0; } },
      view(o) { camGoal = null; Object.assign(kit.cam, o); },
      find: (name) => { for (const e of ents.values()) if (e.kind === KIND.chef && e.data.name === name) return e.id; return null; },
      entity: (id) => { const e = ents.get(id); return e && {x: e.tx, z: e.tz, kind: e.kind}; },
      legend(open) { document.getElementById("legend").classList.toggle("open", !!open); },
    };
  })();
})();
