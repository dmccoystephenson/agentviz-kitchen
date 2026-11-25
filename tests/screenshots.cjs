// Regenerates every README image in docs/media/ from the DEMO feed (never live):
//   npm run screenshots [-- name ...]
// Stills are drawn with ?drive=1 (the kit draws one frame on demand at a
// fixed scene time), so they repeat. The GIF steps scene time frame by frame
// through the page's ?demo=1 script (the fixture with scripted changes, so
// actions switch and new tool calls flare) and is encoded with Pillow.
const fs = require("fs");
const os = require("os");
const path = require("path");
const h = require("../vendor/agentviz-kit/tests/harness.cjs");
const screens = require("../vendor/agentviz-kit/tests/screens.cjs");

const REPO = path.resolve(__dirname, "..");
const MEDIA = path.join(REPO, "docs/media");
const PORT = Number(process.env.KITCHEN_TEST_PORT || 9101);
const READY = "window.__page && window.__page.polls > 0 && window.__page.kitchen.entities.size > 0";
const only = new Set(process.argv.slice(2));
const want = (name) => only.size === 0 || only.has(name);
const DESKTOP = {viewport: {width: 1600, height: 900}};
const PHONE = {viewport: {width: 390, height: 844}, dpr: 3, mobile: true};

// Camera helpers run in the page: frame a chef's line (offset along it by dx).
const HELPERS = `
window.__shots = {
  t: 3,
  at(name, o) {
    const p = window.__page, e = p.kitchen.findChef(name);
    p.view(Object.assign({tx: e.tx + (o.dx || 0), ty: 1.0, tz: e.tz + (o.dz || 0)}, o.cam));
    p.select(o.select ? e.id : null);
  },
  draw() { window.__page.kit.drive(this.t); },
};`;

async function openPage(browser, server, options) {
  const opened = await h.open(browser, `${server.base}/?fixture=1&drive=1`, Object.assign({ready: READY}, options));
  await h.stillAnimations(opened.page);
  await opened.page.evaluate(() => { window.__page.stopPolling(); window.__page.kitchen.settle(); });
  await opened.page.evaluate(HELPERS);
  return opened;
}

async function shoot(page, name, t, setup) {
  if (!want(name)) return;
  await page.evaluate(([time, js]) => { window.__shots.t = time; (0, eval)(js); }, [t, setup]);
  await screens.still(page, path.join(MEDIA, name + ".png"), () => window.__shots.draw());
}

const CLOSE = "{yaw: -1.35, pitch: 0.5, dist: 11}";
const DESKTOP_SHOTS = [
  ["hero", 3.0, "window.__page.fit(true)"],
  ["angle-side", 3.5, "window.__page.fit(true); window.__page.view({yaw: -0.85, pitch: 0.42, dist: 44})"],
  ["angle-top", 4.0, "window.__page.fit(true); window.__page.view({yaw: -1.5708, pitch: 1.4, dist: 62})"],
  ["closeup-brigade", 5.0, `window.__shots.at("Otto", {dx: 7.2, cam: {yaw: -1.35, pitch: 0.5, dist: 13.5}})`],
  ["closeup-waiting", 5.3, `window.__shots.at("Thistle", {dx: 0.8, cam: {yaw: -1.25, pitch: 0.45, dist: 7.5}})`],
  ["closeup-question", 5.4, `window.__shots.at("Juniper", {dx: 0.2, cam: {yaw: -1.3, pitch: 0.35, dist: 7}})`],
  ["closeup-idle-offline", 5.6, `window.__shots.at("Lantern", {dx: 0.2, cam: {yaw: -1.25, pitch: 0.5, dist: 13}})`],
  ["closeup-resting", 5.9, `window.__shots.at("Wren", {dx: 9.2, cam: {yaw: -1.6, pitch: 0.45, dist: 7}})`],
  ["closeup-stalled", 8.0, `window.__shots.at("Patch", {dx: -0.4, cam: {yaw: -1.15, pitch: 0.35, dist: 7.5}})`],
  ["closeup-stalled-cook", 8.2, `window.__shots.at("Otto", {dx: 14.4, cam: {yaw: -1.35, pitch: 0.42, dist: 7.5}})`],
  ["panel-desktop", 6.2, `window.__shots.at("Otto", {dx: 6, cam: {yaw: -1.3, pitch: 0.6, dist: 16}, select: true})`],
  ["legend", 6.5, "window.__page.select(null); window.__page.fit(true); window.__page.legend(true)"],
];

async function desktopStills(browser, server) {
  const {ctx, page, errors} = await openPage(browser, server, DESKTOP);
  for (const [name, t, setup] of DESKTOP_SHOTS) await shoot(page, name, t, setup);
  h.assertNoErrors(errors);
  await ctx.close();
}

async function phone(browser, server) {
  if (!want("phone")) return;
  const {ctx, page, errors} = await openPage(browser, server, PHONE);
  await shoot(page, "phone", 7.0, `window.__page.fit(true); window.__page.select(window.__page.kitchen.findChef("Thistle").id)`);
  h.assertNoErrors(errors);
  await ctx.close();
}

// The GIF: Wren's line under the demo script, a new snapshot every second,
// stepped at 10 frames a second.
async function kitchenGif(browser, server) {
  if (!want("kitchen")) return;
  const {ctx, page, errors} = await openPage(browser, server, {viewport: {width: 960, height: 540}});
  await page.evaluate(async () => {
    const base = await (await fetch("/api/agents?fixture=1")).json();
    window.__feedDemo = (k) => window.__page.feed(window.__page.demoMutate(JSON.parse(JSON.stringify(base)), k));
    window.__shots.t = 10;
    window.__feedDemo(0);
    window.__page.kitchen.settle();
    window.__shots.at("Wren", {dx: 4.5, cam: {yaw: -1.3, pitch: 0.5, dist: 10.5}});
    for (let i = 0; i < 3; i++) window.__shots.draw();
  });
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "kitchen-gif-"));
  const FPS = 10;
  await screens.frames(page, tmp, FPS * 5, (i) => {
    window.__shots.t = 10 + i / 10;
    if (i % 10 === 0) window.__feedDemo(1 + i / 10);
    window.__shots.draw();
  });
  screens.gif(tmp, path.join(MEDIA, "kitchen.gif"), FPS, 800);
  fs.rmSync(tmp, {recursive: true, force: true});
  h.assertNoErrors(errors);
  await ctx.close();
}

h.run("screenshots", {repo: REPO, port: PORT}, async ({server, browser}) => {
  fs.mkdirSync(MEDIA, {recursive: true});
  await desktopStills(browser, server);
  await phone(browser, server);
  await kitchenGif(browser, server);
});
