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
npm test             # bridge contract: new game, orders, 50 turns, save/load, battle drill, schema-13 layer
npm run test:e2e     # Playwright: campaign, combat (rules 2), diplomacy inbox, onboarding, settings, remaster layer,
                     # save export paths, screenshots (1280x720, 2560x1080)
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
- **Diplomacy drill** (title screen) sets up, through the core's diplomacy engine, a Ferengi trade
  proposal and a Klingon tribute demand awaiting your answer, a Klingon betrayal of the Romulans that
  calls you (their ally) to arms, and an eliminated Cardassia whose treaty with you dissolved.
- **First Contact** tutorial starts with a new campaign (skippable, remembered; replay it from
  Settings). **?** opens help for the current screen.
- **Settings** (menu or title): interface scale 90–130%, reduced motion, colour-blind-safe owner
  colours (Okabe–Ito), sound, autosave. Stored per browser; everything works without storage.

### Expanded ship roster, battle effects and sound

- The bridge exposes the 98-class expanded roster (docs/SHIP-ROSTER.md): every ship record carries a `roster`
  object (null for legacy ids) with `assetKey`, faction, hull role, era, tier, source kind/note, buildable, cloak,
  sensors, torpedo share and typed `weapons` (`type`, `key` = `weapon.<Type>`, `count`, `arc`, `torpedo`). It appears
  in `encyclopedia` (legacy + expanded), colony build `choices`, own fleets, and tactical ships (`weapons`, `assetKey`).
- **Database** ship filters: faction (majors, other powers, minors), role, era (plus "Legacy catalogue") and source
  (Legacy / Canon / Mod-derived / Kelvin). Build options show an era chip and the weapon fit; the fleet views show
  role, era, source and mounts.
- **Tactical arena** effects come from a `CombatPresentationSource` (`src/combat/presentation.ts`). The current
  `roundResultSource` derives typed shots from each round's salvos and the shooter's roster mounts (legacy hulls get
  their faction's classic fit): continuous beams coloured per faction, pulse volleys, torpedoes as glowing projectiles
  (photon red-orange, quantum blue, plasma green), shield-bubble flashes vs hull sparks, explosions with shockwaves,
  and ships that glide along a curve and turn smoothly. A core combat event stream can replace it through
  `setPresentationSource()` without touching the renderer or the audio.
- **Sound** (`src/audio/cues.ts`): WebAudio cues keyed `weapon.<WeaponType>` and the AUDIO-PLAN `sfx.*` categories,
  all synthesised at runtime (no recorded audio in the repo). Settings has a volume slider and an optional picker
  for your own local files named after a cue key (`weapon.PhotonTorpedo.wav`); they are decoded in memory only,
  never uploaded, stored or bundled.

### Remastered gameplay (save schema 13)

New campaigns go through `botf::newCampaign` with a **Classic / Remastered** choice (Remastered by
default), which also places the minor-race roster (docs/GAMEPLAY-REMASTER.md, docs/MINOR-RACES.md).

- **Stations.** A troop transport's sector menu offers *Build outpost* (the core's `outpostBlocker`
  explains refusals; `outpostBuildTurns` sets the ETA, shown as a ring on the transport). Click a station
  for its inspector: hull, shields, weapons, supply and sensor squares on the chart, and *Upgrade to
  starbase* with every colony in range dry-run as a funding option. Stations fight as immobile bases
  (fleet ids ≥ 2000000) in the tactical arena.
- **Anomalies.** Nebulae, black holes, neutron stars, drifting ion storms (with drift arrows), subspace
  rifts and wormhole pairs are drawn on the chart; hover for the effect and survey reward. Routes that
  transit a wormhole draw a jump arc between the mouths.
- **Galactic events.** Event cards (Events button, the card under the alerts, or the briefing) show the
  situation, each option's effect, the default and the deadline; choices are `ResolveEventChoice` orders.
- **Victory.** The Victory panel shows `victoryProgress` for conquest, domination, scientific and
  diplomatic victory; the System screen starts or abandons the era megaproject. The campaign ends on a
  Victory/Defeat screen with a summary (`window.__botf.previewOutcome()` previews it).
- **Turn briefing** is built from `TurnReport::chronicle`: entries keep the core's category and link to
  the system, fleet, station, event or battle they mention.
- **Build templates** (System screen): save the queue, apply it, make it the default for new colonies,
  update or delete it.
- **Minor races.** Minor systems show race and temperament; the diplomacy dossier shows homeworld,
  temperament, join bonus, tech level and population; the Empire screen lists member bonuses; the
  Database has a Minor races section.
- **Frontier drill** (title screen): a Developed-era Remastered campaign with charted anomalies, a
  transport ready to build an outpost, an outpost to upgrade and two events to decide. **Starbase drill**:
  the tactical drill with Sol's starbase in the fight.

### Tactical combat (rules 2)

Dashed lines and chips preview what every ship does next round, including AI-controlled ships
(`Encounter::aiOrderFor`, marked "AI"). Orders apply to all ships, a like-class group
(`submitGroupOrder`) or one ship. The arena animates each round: beams, torpedo tracers, cloak
ambushes (+25%), mutual rams, hails and truces, ships disengaging under fire and withdrawing. The hail
panel previews whether the enemy would accept a truce (`aiAcceptsHail`). Rounds resolve through the
plain `session::resolveRound` / `session::autoResolve`; the bridge test proves they verify in
`session::resume` and match headless `advance()`.

### Diplomacy

The Diplomacy screen opens on **Incoming** while anything awaits an answer: AI proposals and tribute
demands with deadlines and consequences, and calls to arms (honour = declare war; refusing lets the
call lapse with its penalties). Dossiers show relation memory (grievances, betrayals, refusals); the
treaty ledger lists ended treaties (expired, broken by war, superseded, dissolved); galactic news
tracks betrayals, wars and eliminations between known powers. Eliminated empires are greyed out.

### Save export inside the claude.ai artifact viewer

`src/platform/fileExport.ts`: when `window.claude.use('downloads')` offers a downloads capability,
exports go through it as `<name>.botfsave.txt`; a declined save is a no-op, and `unavailable`,
`not_granted`, unknown errors or no viewer fall back to the plain `.botfsave` download. Import accepts
both.

## Design rules

- The UI never re-implements game formulas. Economy, forecasts, routes, supply, construction options,
  treaty eligibility and every order's legality come from the core. Presentation-only estimates
  (queue ETA = remaining cost / current industry, route ETA = steps / unit speed) are computed once, in
  the bridge.
- Fog of war is enforced in the bridge: only surveyed systems, sensor-range contacts and colony owners,
  contacted races, the player's own intel and battles the player fought or saw leave the core.
- Turns run through `botf::session` (begin/resume) so battles involving the player pause the turn for
  tactical orders, exactly like the Unreal client.
