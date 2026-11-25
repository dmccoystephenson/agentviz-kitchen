"""Serves the fixture feed and checks the page, its assets and the feed."""
import json
import os
import socket
import subprocess
import sys
import time
import unittest
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = int(os.environ.get("KITCHEN_TEST_PORT", "8942"))
BASE = "http://127.0.0.1:%d" % PORT
ASSETS = ["/", "/index.html", "/kitchen.js", "/strata-kit.js", "/strata-loader.js",
          "/kitchen-engine.js", "/kitchen-engine.wasm"]


def get(path):
    with urllib.request.urlopen(BASE + path, timeout=10) as r:
        return r.status, r.headers.get("Content-Type", ""), r.read()


class ServeFixture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.proc = subprocess.Popen(
            [sys.executable, "-m", "vendor.agentfeed", "serve", "--web", "web", "--port", str(PORT), "--fixture"],
            cwd=REPO, stdout=subprocess.DEVNULL)
        for _ in range(100):
            try:
                socket.create_connection(("127.0.0.1", PORT), timeout=0.2).close()
                return
            except OSError:
                time.sleep(0.1)
        cls.proc.kill()
        raise RuntimeError("server did not start")

    @classmethod
    def tearDownClass(cls):
        cls.proc.kill()
        cls.proc.wait()

    def test_assets(self):
        for path in ASSETS:
            status, ctype, body = get(path)
            self.assertEqual(status, 200, path)
            self.assertTrue(body, path)
        self.assertIn("wasm", get("/kitchen-engine.wasm")[1])

    def test_page_wires_the_scene(self):
        html = get("/")[2].decode()
        for s in ("kitchen.js", "strata-kit.js", "strata-loader.js", 'id="sheet"', 'id="order"'):
            self.assertIn(s, html)
        js = get("/kitchen.js")[2].decode()
        for s in ("/api/agents", "_kt_set_lines", "_kt_set_figures", "createKitchenEngine", "fixture=1"):
            self.assertIn(s, js)

    def test_feed_parses(self):
        status, ctype, body = get("/api/agents")
        self.assertEqual(status, 200)
        doc = json.loads(body)
        self.assertEqual(doc["version"], 1)
        self.assertTrue(doc["fixture"])
        states = {s["state"] for s in doc["sessions"]}
        self.assertTrue({"busy", "waiting", "idle", "offline"} <= states)
        places = {p["id"] for p in doc["places"]}
        for s in doc["sessions"]:
            self.assertIn(s["place"], places)
            for f in s["followers"]:
                self.assertIn(f["state"], ("active", "resting"))


if __name__ == "__main__":
    unittest.main()
