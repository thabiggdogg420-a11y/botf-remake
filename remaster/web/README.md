# BOTF Remastered — browser client

The deterministic C++20 simulation core (`remaster/game/Source/BotfSim`) compiled to WebAssembly and
driven by a Preact + TypeScript interface. The production build is a single self-contained
`dist/index.html` (JavaScript, CSS, fonts and the wasm core are all inlined) that can be opened from
disk or published to any static host.

```
remaster/web/
  bridge/WebBridge.cpp   JSON facade over the core: botf_call(method, argsJson)
  bridge/Json.h          dependency-free JSON writer/reader
  build-wasm.sh          Emscripten build -> src/generated/botf-core.mjs (committed)
  src/core/              typed engine wrapper + view types
  src/state/             signals store, orders/context actions, saves
  src/map/               Canvas 2D galaxy renderer and interaction
  src/screens/           title, setup, galaxy, system, empire, research, fleets,
                         diplomacy, intel, database, combat, briefing, saves
  test/bridge.test.mjs   Node contract test of the wasm bridge
  e2e/                   Playwright end-to-end + screenshot specs
```

## Run

```bash
cd remaster/web
npm install
npm run dev          # http://localhost:5173
npm run build        # typecheck + single-file build in dist/
npm run preview      # serve dist/ on http://127.0.0.1:4173
```

The generated core module is committed, so none of this needs Emscripten.

## Rebuild the core (after changing the simulation or the bridge)

```bash
source /opt/emsdk/emsdk_env.sh    # or set EMSDK; the script also tries /opt/emsdk
./build-wasm.sh                   # incremental; --clean for a full rebuild (~30 s)
```

Flags: `-std=c++20 -O2 -fexceptions` (the core throws for invalid orders; the bridge returns those
messages as JSON errors), `-sSINGLE_FILE -sMODULARIZE -sEXPORT_ES6 -sALLOW_MEMORY_GROWTH`.
`remaster/CMakeLists.txt` also compiles the bridge natively (`botf_web_bridge`) so core API changes
that break it fail CI.

## Test

```bash
npm test             # bridge contract: new game, orders, 50 turns, save/load fingerprint, battle drill
npm run test:e2e     # Playwright: campaign workflow, commanded battle, screenshots (1280x720, 2560x1080)
```

Screenshots go to `$BOTF_SHOTS_DIR` (default: the session scratchpad `web-shots/`). Playwright uses
the Chromium in `PLAYWRIGHT_BROWSERS_PATH` when set.

## Playing

- **F1–F8** Galaxy, System, Empire, Research, Fleets, Diplomacy, Intel, Database · **Enter** end turn
  (with confirmation) · **Esc** back / menu · **Tab** next ship · **Q/E** previous/next colony.
- Galaxy: drag or arrow keys to pan, wheel zooms about the cursor, **F** fits the galaxy, **H** home.
  Click selects; right-click a sector for orders (with a ship selected, right-click open space sets
  course immediately; **Shift+right-click** always opens the menu). Double-click your system to manage it.
- Disabled actions explain themselves: every order is dry-run through the core and the tooltip shows
  the core's reason.
- Saves: quick save from the menu, named slots on the Save/Load screen, autosave every turn, and
  `.botfsave` export/import. A save made mid-battle resumes the open turn.
- **Tactical drill** (title screen) sets up a war with a Klingon squadron at Sol to try combat.

## Design rules

- The UI never re-implements game formulas. Economy, forecasts, routes, supply, construction options,
  treaty eligibility and every order's legality come from the core. Presentation-only estimates
  (queue ETA = remaining cost / current industry, route ETA = steps / unit speed) are computed once, in
  the bridge.
- Fog of war is enforced in the bridge: only surveyed systems, sensor-range contacts and colony owners,
  contacted races, the player's own intel and battles the player fought or saw leave the core.
- Turns run through `botf::session` (begin/resume) so battles involving the player pause the turn for
  tactical orders, exactly like the Unreal client.
