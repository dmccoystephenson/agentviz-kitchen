// Headless smoke test on the demo feed (vendor/agentviz-kit/tests/harness.cjs):
// every fixture agent is in the kitchen; the "needs you" chip cycles both
// waiting sessions and the panel says why each waits; a tap on a chef opens
// its panel; a stalled cook and a cook's own branch show; feed strings never
// become HTML; the empty state and the unreachable banner show; the frame is
// not flat; no console errors. Expected counts come from the feed document.
//   npm ci && npx playwright install chromium && npm run smoke
const path = require("path");
const h = require("../vendor/agentviz-kit/tests/harness.cjs");

const REPO = path.resolve(__dirname, "..");
const PORT = Number(process.env.KITCHEN_TEST_PORT || 9101);
const READY = "window.__page && window.__page.polls > 0 && window.__page.kitchen.entities.size > 0";

function check(cond, message) { if (!cond) throw new Error(message); }

async function checkScene(page) {
  const info = await page.evaluate(() => {
    const k = window.__page.kitchen, doc = window.__page.doc, agents = k.agents();
    return {agents: agents.length, chefs: agents.filter((e) => e.kind === "chef").length, lines: k.lines.length,
      want: {agents: doc.sessions.length + doc.sessions.reduce((n, s) => n + s.followers.length, 0),
        chefs: doc.sessions.length, lines: doc.places.length},
      waiting: doc.sessions.filter((s) => s.state === "waiting").map((s) => ({name: s.name, reason: s.waitingReason}))};
  });
  console.log("scene:", JSON.stringify(info));
  check(info.agents === info.want.agents && info.chefs === info.want.chefs && info.lines === info.want.lines,
    "the kitchen does not hold every fixture agent");
  check(info.waiting.length >= 2, "the fixture should have at least two sessions waiting");
  return info;
}

const panel = (page) => page.evaluate(() => {
  const p = document.querySelector(".av-panel");
  if (p.hidden) return null;
  const needs = p.querySelector(".av-needs");
  return {title: p.querySelector("h2").textContent.trim(), needs: needs && needs.textContent.trim(),
    rows: [...p.querySelectorAll("dd")].map((d) => d.textContent)};
});

const REASON = {question: "has a question", permission: "needs permission"};

async function checkWaitingChip(page, waiting) {
  const chip = page.locator(".av-wait");
  check(await chip.isVisible(), "the needs-you chip is hidden");
  check(Number(await chip.locator("b").textContent()) === waiting.length, "the needs-you chip miscounts");
  const seen = [];
  for (let i = 0; i < waiting.length; i++) { await chip.click(); seen.push(await panel(page)); }
  console.log("needs-you chip cycles:", seen.map((p) => p.title + " (" + p.needs + ")").join(" -> "));
  for (const w of waiting) {
    const p = seen.find((s) => s.title === w.name);
    check(p, "the chip did not reach " + w.name);
    check(p.needs === REASON[w.reason], w.name + "'s panel does not say why it waits");
  }
}

async function checkTap(page) {
  await page.evaluate(() => window.__page.select(null));
  const hit = await page.evaluate(() => {
    const p = window.__page, e = p.kitchen.findChef("Otto");
    p.view({tx: e.x + 3, ty: 1, tz: e.z, yaw: -1.35, pitch: 0.6, dist: 14});
    p.kit.drive(3);
    const q = p.kit.project(e.x, 1.2, e.z), r = p.kit.canvas.getBoundingClientRect();
    return q && {x: q.x + r.left, y: q.y + r.top};
  });
  check(hit, "Otto is off screen");
  await page.mouse.click(hit.x, hit.y);
  await page.waitForTimeout(300);
  const p = await panel(page);
  console.log("tap:", p && p.title);
  check(p && p.title === "Otto", "a tap on Otto did not open his panel");
}

// The stalled subagent and the one on its own branch, found in the feed.
async function checkFollowers(page) {
  const found = await page.evaluate(() => {
    const doc = window.__page.doc;
    let stalled = null, branched = null;
    for (const s of doc.sessions) for (const f of s.followers) {
      if (f.stalled && !stalled) stalled = {key: s.id + "/" + f.id, label: f.label};
      if (f.branch && f.branch !== s.branch && !branched) branched = {key: s.id + "/" + f.id, branch: f.branch};
    }
    return {stalled, branched};
  });
  check(found.stalled && found.branched, "the fixture should have a stalled subagent and one on its own branch");
  const stalledLabel = await page.evaluate((key) => {
    window.__page.select(key, false);
    const e = window.__page.kitchen.entities.get(key);
    return {entity: !!e, ticket: [...document.querySelectorAll(".av-label.ticket.stalled")].map((l) => l.textContent)};
  }, found.stalled.key);
  console.log("stalled cook:", JSON.stringify(stalledLabel));
  check(stalledLabel.entity && stalledLabel.ticket.some((t) => t.includes(found.stalled.label)),
    "the stalled subagent has no stalled ticket");
  const p = await panel(page);
  check(p.rows.includes("stalled") || (await page.locator(".av-panel .av-s-stalled").count()) > 0,
    "the stalled subagent's panel does not say stalled");
  await page.evaluate((key) => window.__page.select(key, false), found.branched.key);
  const rows = (await panel(page)).rows;
  console.log("branched cook rows:", rows.join(" | "));
  check(rows.includes(found.branched.branch), "the subagent panel does not show its own branch");
}

async function checkTextOnly(page) {
  const result = await page.evaluate(() => {
    const doc = JSON.parse(JSON.stringify(window.__page.doc));
    const lead = doc.sessions.find((s) => s.followers.length > 0);
    lead.followers[0].label = "<img src=x id=pwned>";
    window.__page.feed(doc);
    window.__page.select(lead.id, false);
    window.__page.kit.drive(4);
    const asText = (nodes) => [...nodes].some((n) => n.textContent.includes("<img src=x id=pwned>"));
    return {img: !!document.getElementById("pwned"), panel: asText(document.querySelectorAll(".av-panel .av-lanes button")),
      label: asText(document.querySelectorAll(".av-label"))};
  });
  console.log("text only:", JSON.stringify(result));
  check(!result.img && result.panel && result.label, "a feed label was parsed as HTML");
}

async function checkEmptyState(page) {
  const shown = await page.evaluate(() => {
    const doc = JSON.parse(JSON.stringify(window.__page.doc));
    const sessions = doc.sessions;
    window.__page.feed(Object.assign(doc, {sessions: []}));
    const visible = !document.querySelector(".av-empty").hidden;
    window.__page.feed(Object.assign(doc, {sessions}));
    return visible && document.querySelector(".av-empty").hidden;
  });
  check(shown, "no empty-state hint with zero sessions");
}

async function checkUnreachable(page) {
  await page.route("**/api/agents*", (route) => route.fulfill({status: 503, body: "down"}));
  await page.evaluate(async () => { await window.__page.pollOnce(); await window.__page.pollOnce(); });
  const banner = await page.evaluate(() => !document.querySelector(".av-banner").hidden && document.querySelector(".av-banner").textContent);
  console.log("banner:", banner);
  check(banner && banner.includes("HTTP 503"), "no unreachable banner after failed polls");
}

h.run("smoke", {repo: REPO, port: PORT}, async ({server, browser}) => {
  const {page, errors} = await h.open(browser, server.base + "/?fixture=1&drive=1", {ready: READY});
  await h.stillAnimations(page);
  await page.evaluate(() => { window.__page.stopPolling(); window.__page.kitchen.settle(); window.__page.kit.drive(2); });
  const info = await checkScene(page);
  await checkWaitingChip(page, info.waiting);
  await checkTap(page);
  await checkFollowers(page);
  await checkTextOnly(page);
  await checkEmptyState(page);
  await page.evaluate(() => { const p = window.__page; p.select(null); p.kitchen.settle(); p.fit(true); p.kit.drive(5); });
  const png = await h.shot(page, path.join(REPO, "docs/media/screenshot.png"));
  console.log("distinct colours:", await h.assertNotFlat(page, png));
  h.assertNoErrors(errors);
  await checkUnreachable(page);  // last: the failed fetches log console errors of their own
});
