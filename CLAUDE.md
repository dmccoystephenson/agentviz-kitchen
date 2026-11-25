# agentviz-kitchen

A Strata web-kit scene: Claude Code sessions as a restaurant kitchen.

- Data comes only from `/api/agents` (see `vendor/agentfeed`, schema in
  https://github.com/dmccoystephenson/agentviz docs/FEED.md). Never edit
  `vendor/agentfeed/`; re-vendor with agentviz's `tools/vendor-feed.sh`.
- Scene: `scene/main.cpp`; build with `scene/build.sh` (needs emsdk 6.0.11);
  commit the built `web/kitchen-engine.{js,wasm}`.
- Run: `python3 -m vendor.agentfeed serve --web web --port 8842 --fixture`.
- Test: `python3 -m unittest discover -s tests` and
  `NODE_PATH=/root/arcade-watch/node_modules node tests/smoke.cjs`.
- Screenshots come from the fixture feed only, never live sessions.
