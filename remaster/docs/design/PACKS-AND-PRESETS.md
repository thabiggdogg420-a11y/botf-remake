# Packs, overlays and presets (lane MOD-1)

Sources that outrank this file: `C:\Users\BiggDogg\AI_stuff\standing\simon\botf-rulings.md` (11 Nov 2026: every vanilla feature plus the five most popular mod packs, each as a new-game preset, "Yes, presets"; the Borg playable and a threat), then `docs/research/MOD-PARITY.md` (§4 conflicts, §5 lane plan) and `docs/DATA-FORMAT.md` (the base pack and the content checksum, which this file extends and does not replace). Private; not for distribution.

One superset core. A mod pack is not a second game: it is a **preset** (which overlays sit on the base pack, which rule flags are on, which empires play, defaults for the new-game screen). The code is `game/Source/BotfSim/Content.h` (overlay merge), `Presets.h` (preset registry), `Rules.cpp` (`newCampaign`), `Simulation.cpp` (the save section), tests in `tests/scenarios/PresetTests.cpp`.

## 1. Overlay packs

The base pack (`botf-base`, `data/base.pack.json`) is unchanged. An **overlay** is a small JSON file, `data/overlays/<id>.overlay.json`:

```json
{"format":1,"kind":"overlay","id":"ultimate","name":"…","revision":1,"requires":["botf-base"],
 "balance":{"researchCostPercent":85},
 "technologies":{"patch":[{"id":13,"cost":220}]},
 "minors":{"append":[{ …a whole minor record… }]}}
```

**Merge rules**, applied to the parsed document before the strict typed parse, so a merged record is validated exactly as a base record is (unknown keys, ranges, `id == index`, enum names):

| Section | patch (change listed fields of an existing record) | append (whole new records) |
|---|---|---|
| `technologies`, `events` | yes, by `id` / `kind` | no (fixed count) |
| `ships`, `buildings`, `races`, `minors` | yes, by `id` | yes; ids continue the section (`id == index`, minors `index + 1`) |
| `roster` | yes, by `id` | yes; ids ascending in 1000..1999 |
| `balance` | each listed value replaces the pack's | n/a |

- A patch names an existing record (an earlier overlay's appended record counts) and replaces only the fields it lists. A nested object (`flight`) merges field by field; arrays and scalars are replaced whole. A missing target, an unknown field or the same record patched twice in one overlay is an error that names the overlay, section and entry.
- **Nothing is removed**: ids are save data and never renumber. An overlay switches a class off with `{"id":N,"buildable":false}`.
- `requires` lists pack ids that must come earlier in the stack (`botf-base` is always present). The same overlay twice is an error. Order is meaning: **later overlays win per field**.
- `balance` is a new optional object in a bundle (absent in the base pack, so the base bytes and checksum are unchanged). Wired values: `researchCostPercent` (every research cost, `researchCost()`) and `shipCostPercent` (every ship's industry cost in a queue), integers 10..1000, 100 is the identity. More values join with MOD-6/MOD-7.
- A refusal leaves the active pack untouched (atomic, as `install` is).

**Checksum and determinism.** A stack's checksum is FNV-1a 64 over the text `botf-base:<base checksum>\n` then `<overlay id>:<overlay checksum>\n` per overlay in order, each overlay checksum being FNV-1a 64 over the overlay's exact bytes (whitespace counts, as for a bundle). So the same stack gives the same checksum on every host, and a changed byte, a reorder or a different membership shows. The stack id is `botf-base+<id>+<id>`. Merging is a pure function of the bytes: it reads no clock and never touches the campaign RNG; the JSON reader is the loader's own, so native, MSVC and wasm agree. An empty overlay still makes a distinct stack.

**Where the bytes come from.** The core never opens a file. Shipped overlays and presets are embedded (`ContentPresets.generated.h`, built by `tools/content/build_presets.py`, `--check` for staleness, same pattern as `ContentBase.generated.h`). A host adds more with the bridge calls `registerOverlay` / `registerPreset {"bytes"}`; a same-id overlay with different bytes is refused.

**How a save records its packs (schema 17).** One new optional section, written only when the active pack is a stack, directly before `CONTENT`:

```
PACKS <presetId|-> <n> <overlayId> <checksum16> …
CONTENT botf-base+ultimate <stack checksum16>
```

Both lines are inside the fingerprint. Base-content saves (Classic, Remastered) carry neither, so their text and fingerprints differ from schema 16 only by the header. Loading: the host need not install anything; `PACKS` names each overlay and the checksum of its bytes, the core rebuilds the stack from the registry and the existing strict `CONTENT` rule then holds. A missing overlay, or one whose bytes changed since the save, is refused by name, and the active pack stays as it was. A schema-16 save loads and re-saves as 17; `PACKS` in a file with a 16 header is refused. `GameState.presetId` holds the preset (empty for Classic, Remastered and any plain-pack game). Schema 17 is also what MOD-2 plans; the migration here is self-contained (header, `legacy16`, one section) and Simon renumbers at merge.

## 2. Presets

A preset is `data/presets/<id>.preset.json`, validated at load:

```json
{"format":1,"id":"dominion-war","name":"Dominion War","blurb":"…","order":30,
 "overlays":["dominion-war"],
 "rules":{"base":"remastered","flags":{"expandedRoster":true}},
 "lineup":["Federation","Klingon","Romulan","Cardassian","Dominion"],
 "options":{"empires":5,"era":3,"size":"large","shape":"irregular","minorDensity":"many","difficulty":"hard","events":true,"aiAggression":true},
 "needs":["MOD-2","MOD-3","MOD-8"]}
```

- **Data overlays**: `overlays`, in merge order. No overlays means the plain base (or host-loaded) pack; choosing such a preset after a mod preset puts the base pack back.
- **Rule flags**: `rules.base` (`classic` or `remastered`, the existing `RulesetPreset`) then `flags` overrides from the `GameplayRules` set (`stations, galaxyFeatures, curatedEvents, alternativeVictories, expandedRoster, flightModel, wegoCombat, empireRules, eras`; `eras` (MOD-4, schema 22) needs `expandedRoster` and an overlay carrying an `eras` block: docs/design/ERAS.md). The result must pass `validate()` (a Remastered base keeps its four layers; a Classic base keeps every layer but stations and empire rules off; WEGO needs the flight model); the preset is refused at load otherwise. Later lanes add flags to that list, not a second mechanism. Flags are saved by the existing rule sections.
- **Lineup**: the major empires in slot order, validated against `presets::MajorCap` (5) and `presets::StockEmpires` (the five the core seats): 2..8 distinct letter-only keys are structural, anything else loads but reports `lineupPlayable:false` with the reason (over the cap; a slot the core cannot seat yet). A not-playable preset still starts: the game seats the stock empires and the picker says what it waits for. **MOD-2 lifts the cap by raising `MajorCap` and extending `StockEmpires` in `Presets.h`; no preset file changes.**
- **Defaults** for the new-game options (all optional): `empires` (2..cap), `era` (1..5), `size` (small, medium, large, huge), `shape`, `minorDensity`, `difficulty`, `events`, `aiAggression`. The screen fills them when the preset is picked; the player may change any afterwards. `needs` lists the lanes whose content the preset is still waiting for (shown to the player).

`newCampaign` with `CampaignSetup.presetId` set: finds the preset, makes its overlay stack the active content, generates the galaxy (research costs and the roster feed the generator), applies the flags, records the preset. The bridge's `preset` option takes any registered id (`classic` and `remastered` still work); `meta.presets` and `listPresets` give the picker its list.

## 3. The presets

All seven use the five stock empires today. "Needs" is content that arrives with later lanes; the overlays below are the manifests those lanes fill.

| Preset | Overlay | Base rules and flags | Lineup (target) | Defaults | Waiting for |
|---|---|---|---|---|---|
| **Classic** | none | classic | Fed, Klingon, Romulan, Cardassian, Ferengi | medium, elliptical, some minors, normal, era 1 | nothing |
| **Remastered** (default) | none | remastered | stock five | medium, spiral, some, normal, era 1 | nothing |
| **Dominion War** (UDM III) | `dominion-war` (empty) | remastered | Fed, Klingon, Romulan, Cardassian, **Dominion** (replaces Ferengi) | large, irregular, many, hard, era 3 | MOD-2, MOD-3, MOD-8 |
| **Ages** (AAM) | `ages`: eras block (2240 to 2380, start tech 1/3/5/8/11, cap 14), canon windows, 7 classes switched on | remastered, `expandedRoster`, `eras` | stock five | medium, spiral, some, normal, era 1 | nothing |
| **Galaxies** (GALM) | `galaxies`: eras block (start tech 1/4/7/10/14, cap 14), Dominion, Breen and Borg windows | remastered, `eras` | Fed, Klingon, Romulan, **Dominion, Borg** | huge, spiral, many, normal, era 1 | MOD-3 |
| **Balance of Power** | `balance-of-power` (empty) | classic | stock five | large, spiral, many, normal, era 1 | MOD-6, MOD-7 |
| **Ultimate** (UM5 Redux) | `ultimate`: `balance.researchCostPercent` 85 | remastered | stock five | large, irregular, many, hard, era 1 | MOD-7 |

The 85% research scale is a placeholder for UM5's "research costs scaled back to help the AI"; MOD-7 sets the real curve. Dominion War and Galaxies report `lineupPlayable:false` until MOD-2 and MOD-3 land; the Borg being playable (Simon, 11 Nov) is in the Galaxies lineup from day one so the validation has something to lift.

## 4. Screen

New-campaign Rules panel: a **Preset** row (one LCARS toggle per preset from `meta.presets`, tooltip = description) above the other options, and under it the chosen preset's text: blurb, the powers, a "Not seated yet" line when the lineup is pending, a "Content still to come" line. Picking a preset fills era, size, shape, minor races, difficulty, events, rival aggression and powers in play (never below the chosen faction's place). Godot: `NewCampaignVM.pick_preset`, `preset_text`, `PRESET_FALLBACK` (the two built-ins when no core list exists); web: `web/src/screens/NewCampaign.tsx` (`pickPreset`). Both read the same registry through the core / wasm.

## 5. Not in this lane

Real content for any overlay (MOD-3, 4, 5, 7, 8); lifting the cap and the empire arrays (MOD-2); a preset editor (MOD-10); more `balance` values and rule flags (MOD-6/7). A user pack replacing the base bundle (`loadContent`) and a preset stack do not combine: a preset stacks on the embedded base.
