// Regenerates every README image in docs/media/ from the FIXTURE feed only.
//   NODE_PATH=<dir with playwright> node tests/screenshots.cjs
// Uses ?drive=1 so each still is drawn at a fixed scene time, and Pillow
// (python3) to assemble the GIF from its frames.
const {chromium} = require("playwright");
const {spawn, execFileSync} = require("child_process");
const fs = require("fs");
const path = require("path");

const repo = path.resolve(__dirname, "..");
const media = path.join(repo, "docs/media");
const port = Number(process.env.PORT || 8942);
const ARGS = ["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader"];

function serve() {
  const cmd = ["-m", "vendor.agentfeed", "serve", "--web", "web", "--port", String(port), "--fixture"];
  const p = spawn("python3", cmd, {cwd: repo, stdio: ["ignore", "pipe", "inherit"]});
  return new Promise((resolve, reject) => {
    p.stdout.on("data", (d) => { if (String(d).includes("serving")) resolve(p); });
    p.on("exit", (c) => reject(new Error("server exited " + c)));
  });
}

async function open(browser, viewport, dpr = 1) {
  const page = await browser.newPage({viewport, deviceScaleFactor: dpr, isMobile: dpr > 1, hasTouch: dpr > 1});
  page.on("pageerror", (e) => console.error("pageerror:", String(e)));
  await page.goto(`http://127.0.0.1:${port}/?fixture=1&drive=1`, {waitUntil: "commit"});
  await page.waitForFunction(() => window.kitchen && window.kitchen.ready && window.__strataKit && window.__strataKit.ready,
    null, {timeout: 180000});
  await page.evaluate(() => window.kitchen.settle());
  return page;
}

// A busy machine can starve a screenshot: try again a few times.
async function shot(page, file) {
  for (let i = 0; ; i++) {
    try { return await page.screenshot({path: file, timeout: 120000}); }
    catch (e) { if (i >= 3) throw e; console.log("retrying", path.basename(file)); }
  }
}

// Draw scene time t a few times with real time between, so the labels'
// show/hide hysteresis settles, then save.
async function still(page, name, t, setup) {
  if (setup) await page.evaluate(setup);
  for (let i = 0; i < 3; i++) {
    await page.evaluate((tt) => window.__strataKit.drive(tt), t + i * 0.001);
    await page.waitForTimeout(260);
  }
  await shot(page, path.join(media, name));
  console.log("saved", name);
}

// Point the camera at a named chef (offset along the line by dx).
const at = ([name, o]) => {
  const k = window.kitchen, e = k.entity(k.find(name));
  k.view(Object.assign({tx: e.x + (o.dx || 0), ty: 1.0, tz: e.z + (o.dz || 0)}, o.cam));
  k.select(o.select ? k.find(name) : null);
};

(async () => {
  fs.mkdirSync(media, {recursive: true});
  const server = await serve();
  const browser = await chromium.launch({args: ARGS});
  try {
    let page = await open(browser, {width: 1600, height: 900});
    const whole = await page.evaluate(() => Object.assign({}, window.__strataKit.cam));
    await still(page, "hero.png", 3.0);
    await still(page, "angle-side.png", 3.5, () => window.kitchen.view({yaw: -0.55, pitch: 0.42, dist: 34, tx: 0, ty: 1, tz: 2}));
    await still(page, "angle-top.png", 4.0, () => window.kitchen.view({yaw: -1.5708, pitch: 1.4, dist: 40, tx: 0, ty: 0, tz: 0}));
    const close = {yaw: -1.35, pitch: 0.5, dist: 11};
    await page.evaluate(at, ["Otto", {dx: 5.5, cam: close}]);
    await still(page, "closeup-brigade.png", 5.0);
    await page.evaluate(at, ["Thistle", {dx: 1, cam: {yaw: -1.25, pitch: 0.45, dist: 7.5}}]);
    await still(page, "closeup-waiting.png", 5.3);
    await page.evaluate(at, ["Lantern", {dx: -1.8, cam: {yaw: -1.25, pitch: 0.5, dist: 9.5}}]);
    await still(page, "closeup-idle-offline.png", 5.6);
    await page.evaluate(at, ["Wren", {dx: 9.2, cam: {yaw: -1.6, pitch: 0.45, dist: 7}}]);
    await still(page, "closeup-resting.png", 5.9);
    await page.evaluate(at, ["Otto", {dx: 4, cam: {yaw: -1.3, pitch: 0.6, dist: 14}, select: true}]);
    await still(page, "panel-desktop.png", 6.2);
    await page.evaluate(() => { window.kitchen.select(null); window.kitchen.view({yaw: -1.4, pitch: 0.78, dist: 30, tx: 0, ty: 0.5, tz: 0}); window.kitchen.legend(true); });
    await still(page, "legend.png", 6.5);
    await page.evaluate(() => window.kitchen.legend(false));

    // The GIF: Wren's brigade at work, 5 s at 10 fps.
    const frames = path.join(repo, ".strata", "gif-frames");
    fs.rmSync(frames, {recursive: true, force: true});
    fs.mkdirSync(frames, {recursive: true});
    await page.setViewportSize({width: 960, height: 540});
    await page.evaluate(at, ["Wren", {dx: 4.5, cam: {yaw: -1.3, pitch: 0.5, dist: 10.5}}]);
    for (let i = 0; i < 3; i++) { await page.evaluate((t) => window.__strataKit.drive(t), 10 + i * 0.001); await page.waitForTimeout(260); }
    for (let i = 0; i < 50; i++) {
      await page.evaluate((t) => window.__strataKit.drive(t), 10 + i * 0.1);
      await shot(page, path.join(frames, `f${String(i).padStart(3, "0")}.png`));
    }
    execFileSync("python3", ["-c", `
import glob, sys
from PIL import Image
fs = sorted(glob.glob(sys.argv[1] + "/f*.png"))
im = [Image.open(f).convert("RGB").resize((800, 450), Image.LANCZOS) for f in fs]
pal = im[0].quantize(colors=128, method=Image.Quantize.MEDIANCUT)
q = [i.quantize(palette=pal, dither=Image.Dither.NONE) for i in im]
q[0].save(sys.argv[2], save_all=True, append_images=q[1:], duration=100, loop=0, optimize=True)
`, frames, path.join(media, "kitchen.gif")]);
    console.log("saved kitchen.gif");
    await page.close();

    // Phone portrait, DPR 3, bottom sheet open on the chef who needs you.
    page = await open(browser, {width: 390, height: 844}, 3);
    await page.evaluate(at, ["Thistle", {dx: 0.5, cam: {yaw: -1.3, pitch: 0.6, dist: 12}, select: true}]);
    await still(page, "phone.png", 7.0);
    await page.close();
    // A stalled chef: Scout (busy, last tool call 400 s ago in the demo feed).
    page = await open(browser, {width: 1600, height: 900});
    await page.evaluate(at, ["Scout", {dx: -0.4, cam: {yaw: -1.15, pitch: 0.35, dist: 7.5}}]);
    await still(page, "closeup-stalled.png", 8.0);
    await page.close();
    void whole;
  } finally {
    await browser.close();
    server.kill();
  }
})().catch((e) => { console.error(e); process.exit(1); });
