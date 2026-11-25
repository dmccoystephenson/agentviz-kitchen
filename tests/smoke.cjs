// Headless smoke test: serve the fixture, load /?fixture=1, wait for the kit
// to draw, check there are no console errors, a tap selects an agent, and
// save docs/media/screenshot.png.
//   NODE_PATH=/root/arcade-watch/node_modules node tests/smoke.cjs
const {chromium} = require("playwright");
const {spawn} = require("child_process");
const path = require("path");

const repo = path.resolve(__dirname, "..");
const port = Number(process.env.PORT || 8942);

function serve() {
  const p = spawn("python3", ["-m", "vendor.agentfeed", "serve", "--web", "web", "--port", String(port), "--fixture"],
    {cwd: repo, stdio: ["ignore", "pipe", "inherit"]});
  return new Promise((resolve, reject) => {
    p.stdout.on("data", (d) => { if (String(d).includes("serving")) resolve(p); });
    p.on("exit", (c) => reject(new Error("server exited " + c)));
  });
}

(async () => {
  const server = await serve();
  const browser = await chromium.launch({args: ["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader"]});
  const errors = [];
  try {
    const page = await browser.newPage({viewport: {width: 1280, height: 760}});
    page.on("console", (m) => { if (m.type() === "error") errors.push(m.text()); });
    page.on("pageerror", (e) => errors.push(String(e)));
    await page.goto(`http://127.0.0.1:${port}/?fixture=1&drive=1`, {waitUntil: "commit"});
    await page.waitForFunction(() => window.kitchen && window.kitchen.ready && window.__strataKit && window.__strataKit.ready,
      null, {timeout: 120000});
    // ?drive=1: the frame loop does not run; draw scene times ourselves.
    await page.evaluate(() => { window.kitchen.settle(); for (let i = 1; i <= 3; i++) window.__strataKit.drive(2 + i * 0.1); });
    const ids = await page.evaluate(() => window.kitchen.ids());
    if (ids.length < 15) throw new Error("expected the fixture's agents, got " + ids.length);
    const targets = await page.evaluate(() => window.__strataKit.targets());
    if (!targets.length) throw new Error("no tap targets");
    const t = targets[0];
    const hit = await page.evaluate(([x, y]) => window.__strataKit.pickAt(x, y), [t.x, t.y]);
    if (!hit) throw new Error("pick at a target found nothing");
    await page.evaluate((id) => window.kitchen.select(id), hit);
    await page.waitForSelector("#sheet.open", {timeout: 5000});
    await page.evaluate(() => window.kitchen.select(null));
    await page.evaluate(() => window.__strataKit.drive(3));
    await page.screenshot({path: path.join(repo, "docs/media/screenshot.png")});
    if (errors.length) throw new Error("console errors:\n" + errors.join("\n"));
    console.log("smoke OK:", ids.length, "entities,", targets.length, "targets; picked", hit);
  } finally {
    await browser.close();
    server.kill();
  }
})().catch((e) => { console.error(e); process.exit(1); });
