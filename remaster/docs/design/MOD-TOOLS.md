# Mod tools: the in-game pack editor (lane MOD-10)

Sources that outrank this file: `C:\Users\BiggDogg\AI_stuff\standing\simon\botf-rulings.md` (11 Nov 2026: every vanilla feature plus the popular mod packs, each a preset; 12 Nov: at most two lanes at once), then `docs/research/MOD-PARITY.md` (§5 MOD-10 "Simon can author his own pack", row A06 and the Ultimate Editor's role), `docs/design/PACKS-AND-PRESETS.md` (the overlay and preset formats this lane writes) and `docs/DATA-FORMAT.md`. Private; not for distribution.

The Ultimate Editor let a player change the game's data files. Here the same job is an in-game screen that writes a **user pack**: one overlay and one preset, in the player's own data folder, stacked and checksummed by exactly the rules MOD-1 gave the shipped packs. No rules change; the core only learned to accept, replace and list user packs.

## 1. What a user pack is

Two text files in the user data folder, laid out like `data/overlays` and `data/presets` (so a finished pack can be copied into the repository to ship):

```
user://mods/<id>.overlay.json    data patches and appended records, balance values, an eras block
user://mods/<id>.preset.json     rule base and flags, lineup, new-game defaults; lists the overlay last
```

`user://` is `%APPDATA%\Godot\app_userdata\BOTF Remastered\` on Windows and IndexedDB in the browser. The pack id is the id of both files and of the preset. The overlay's **exact bytes** are what the core checksums, so the editor writes and reads plain text and never re-serialises it: the file on disk is the byte string the core was given.

A pack may **build on** a shipped preset or another user pack: that preset's overlays go under the user's overlay (`requires` lists them) and its rules, lineup and options become the starting point. Later overlays win per field, so the user pack's changes sit on top.

## 2. What the core gained (host registry only)

`game/Source/BotfSim/Presets.h` and `web/bridge/WebBridge.cpp`. Nothing in the rules, the campaign RNG, the save format or the shipped pack bytes changed; Classic and Remastered games are bit-identical (fingerprints compared in `ModToolsTests`).

| Call | Does |
|---|---|
| `packPart {under:[ids], key}` | The merged JSON of one top-level pack key (`roster`, `buildings`, `balance`, `eras` ...) under the base pack plus those registered overlays: the current values the editor shows. |
| `validatePack {overlay, preset}` | The core's own validators on the exact file text, registers nothing. The overlay merges over the base and the preset's other overlays and goes through the strict typed parse (unknown fields, ranges, `id == index`); the preset goes through `parsePreset` (flags, lineup, options, `checkFlags`); the preset must list the overlay last; an `eras` flag needs an eras block in the stack. A refusal is the core's message. |
| `installUserPack {overlay, preset}` | Validate, then register or **replace** the user pack. Atomic. A shipped overlay id or preset id is refused ("is a shipped pack and cannot be replaced"). |
| `removeUserPack {id}` | Removes a user pack's preset and overlay (never a shipped one). |
| `userPacks` | The user overlays with the checksum of their bytes. The editor probes this to know the core can take user packs. |
| `listPresets` / `meta.presets` | A user preset carries `"user": true` (written only for user packs, so shipped output is unchanged). |

`registerOverlay` keeps its MOD-1 rule (same id, other bytes: refused); `installUserPack` is the one path that replaces, because an author edits their pack. A save made on the old bytes is then refused by its checksum.

## 3. Saves and checksums

Unchanged from MOD-1 (schema 17 `PACKS` section): a save on a stack records each overlay's id and the checksum of its bytes. The client now registers the folder's packs at boot (`UserPacks.register_all`, before any save loads), so a save made on a user pack loads in a later session. `ensureStack` refuses a save whose pack is **missing** ("needs overlay X, which is not installed") or **changed** ("overlay X changed since the save"); `UserPacks.friendly_load_error` turns both into plain words ("This save was made with the pack "X", which is not installed ..."; "... an earlier version of the pack "X"; the pack's files have changed since ..."). A refused load leaves the active pack and the running game as they were. Restoring the exact bytes restores the save.

## 4. The editor (`scenes/ui/screens/pack_editor_screen.gd`, model `scripts/viewmodels/pack_editor_vm.gd`)

Reached from the title menu ("Pack editor"; hidden when the core cannot take user packs). Toolbar: open a saved pack, new pack, **Save pack**, **New game with this pack** (opens the new-campaign screen with the preset picked), delete pack. Ten tabs:

| Tab | Edits |
|---|---|
| Pack | id, name, blurb; "Build on" any registered preset. |
| Ship classes | The expanded roster (98 classes): name, cost, build turns, maintenance, hull, shields, attack, torpedo share, speed, range, sensors, cloak, buildable, description. "Add as a new record" copies a class under the next free id (1000..1999). |
| Legacy ships | The 125 catalogue ships (Classic): name, cost, maintenance, hull, shields, attack, range, speed, troops, description; copy-to-add. |
| Structures | The 318 structures: name, industry, energy and population cost, morale, tech level, description. |
| Technologies | The 77 techs: name, cost, description. |
| Races | Race names and the nine trait effects of the majors (research, industry, food, credits, ship cost, intel, security, ground defence, morale). |
| Minor races | Name, homeworld, tech level, population, bonus value, description; copy-to-add. |
| Balance | All 19 pack-wide balance values (research and ship cost, curve, structure cost, defence, population, weapon damage, espionage, gifts, the AI policy flags, race star types). |
| Eras | Switch a timeline on (sets `expandedRoster` and `eras`), turns per year, tech cap, start years and start tech per era, per-class windows (first and last year in service). Under a pack that already has a timeline (Ages, Galaxies) the values edit in place. |
| Rules and lineup | Rule base (Classic or Remastered), every rule flag, the powers in play and their seating order (any of the eight), and the new-game defaults: powers, start era, size, shape, minor density, difficulty, events, rival aggression. |

The draft keeps only what the author changed: patches (a value equal to the one underneath drops out), appended records, balance and era overrides. Current values come from `packPart`, so the editor shows what the game will run, including the packs under it. Every change re-validates through `validatePack`; the core's message is shown **inline**: a red banner at the foot, a red "!" on the tab it is about, and the message at the top of that tab. A value that is not a number is refused by the editor itself before the core sees it. An invalid pack cannot be saved.

**Not editable here (deferred):** the 77 + 24 event and high-tech text records (`events` patches and an eras `highTechnologies` block are kept as written if a hand-edited pack has them, not shown); weapon mounts, flight data, tech requirements and 64-bit race masks of a record (a copied record keeps the source's); new races, new structures, new technologies (the core's fixed counts and slot rules); threat-actor defaults (kept from the preset you build on, not shown); the scenario format (MOD-8 owns it; scenario editing waits until it lands); a visual 3D ship-model picker. A shipped pack is never edited: the editor builds a new pack on top of it.

## 5. Browser build

The editor is plain GDScript on the same LCARS widgets, the bridge calls are in the shared `WebBridge.cpp`, and `user://` is IndexedDB in the web export, so the editor is built into both targets. It is **shown only when the core answers `userPacks`**: an older wasm core, or the recorded-fixture backend, hides the title button and the screen says so if reached. See the verify result in the lane report for which the final web build did.

## 6. Tests

- `tests/scenarios/ModToolsTests.cpp` (`BotfModToolsTests`): `packPart`, `validatePack` refusals (bad range, unknown field, missing class, balance range, preset not listing the pack, unregistered overlay under it, eras flag without eras, layer switched off, shipped overlay and preset ids, bad JSON) leaving the registry untouched; create, replace, remove; the round trip (new game on a user pack, 20 turns, save, reload, lockstep play-on), the changed-pack and missing-pack refusals, and Classic and Remastered fingerprints unchanged.
- `godot/scripts/viewmodels/tests/test_pack_editor_vm.gd`: the viewmodel's helpers, then on the native core the whole route (edit, inline core errors, save to a scratch folder, preset picker, 20 turns, save, reload, tamper, missing pack, delete).
- verify.ps1 runs the Godot suite as `pack-editor-vm`.
