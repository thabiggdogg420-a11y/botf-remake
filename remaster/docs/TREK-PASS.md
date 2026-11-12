# BOTF Remastered: Trekkification pass (assessment)

Date: 10 Nov 2026. Lane "trekpass", author Barclay. Assessment only: no game code changed, no Godot run, nothing built.

Sources that outrank this file: Simon's words in `standing\simon\botf-rulings.md`, in particular 10 Nov: "So far as UI goes, there's ALOT that can be updated though, to trekkify it. Currently we have little arrow/triangle icons instead of the Starfleet insignia etc for federation, and needs the different empire insignias for other powers etc"; "do a trekkification pass of the game, compare to other popular trek games and 4x games and assess how to best trekify it"; and "The current UI is fine" (the LCARS frame stays). Then `docs/UI-DESIGN-SYSTEM.md` and `docs/ART-BIBLE.md`.

Tags used below: **[repo file:line]** = read in the repo today. **[frame]** = seen in a screenshot. **[web]** = read on a page I fetched or in a search result (URLs in section F). **[memory]** = from my own knowledge of the shows and games, not checked online; treat as a brief for the artist to verify against Simon's own library, not as fact.

Paths are relative to `remaster/godot/` unless they start `game/`, `data/` or `docs/`.

---

## A. Verdict

Today the game is a competent generic 4X with a Trek coat, and the coat is mostly colour and nouns. What is Trek: the LCARS frame and rail [frame], real ship classes and canon-named stars, the 98-class canon roster from Enterprise to Dominion War plus Borg, Dominion, Breen, Hirogen and others (`data/base/roster.json`), 30 minor races with good short in-universe descriptions (`data/base/minors.json`), real Trek-correct planet classes (K, Y, M, H) [frame], red alert on battle entry, and an audio pack with per-empire voice, music and era-twinned weapon sounds that is far ahead of the visuals (`scripts/audio/pack`, 5 empire packs plus shared). What is generic: every owner is a flat polygon (the "triangle" Simon sees is `Lcars.EMBLEMS[1]`, `scenes/ui/widgets/lcars.gd:64`), ships on the chart are a chevron, stations a hexagon, contacts a diamond; a Klingon player gets the Federation LCARS recoloured red, down to a label that still says "LCARS" (`lcars_elbow.gd:25`); the writing is a spreadsheet ("Sol completed Laboratory.", "Klingon Empire declared war on Federation.", "Turn 12 report") [frame, `game/Source/BotfSim/Simulation.cpp:2292`]; there are no captain's logs, no ranks, no registries, no leader viewscreens, no intro, no per-empire voice in text, and the stardate is computed two different ways. Honest score: about 3 out of 10 on feeling like Star Trek (6 out of 10 on sound, 2 out of 10 on screens). The cheapest large gains are the insignia and ship glyphs (Simon's own ask), a stardate and captain's-log voice for every report, and a real per-empire console family; the sound and data foundations already exist and mostly need wiring and filling.

---

## B. Screen by screen

Effort S = a day-scale lane change in one or two files, M = a lane, L = needs more than one lane or an art deliverable. Impact 1 to 5 for "reads as Star Trek".

| Screen | What's generic (file:line) | What would make it Trek | Effort | Impact |
|---|---|---|---|---|
| Title [frame `pc-01-title`] | Five tiny flat emblems over the name (`scenes/ui/screens/title_screen.gd:29-32`); "BOTF REMASTERED" wordmark with no Trek mark; tagline "Five powers. One quadrant." (`:~39`); drill buttons "Tactical/Diplomacy/Frontier/Starbase drill" (`:67-72`); static UI over a live fleet (nice: `title_fleet.gd`). No intro film, no VideoStream anywhere (grep found none). | Insignia row replaced by the real set; Starfleet-style title card ("Stardate" line, "These are the voyages" is NOT ours to quote, write an original line); per-empire title fleet and music already switch on `title_empire()` (`:105`), add the per-empire insignia and one-line empire motto; drills renamed "Kobayashi Maru"-style scenario names (original, not quotes). | M | 4 |
| New campaign [frame `pc-02`] | Faction cards are a flat polygon on a gradient (`new_campaign_screen.gd:170,219` via `Lcars.emblem`); traits are bullets ("Balanced economy"); "Minor race" legend is a text "○" (`:~176`); "Interface look" row offers TOS/Movie/TNG/Voyager/Late (`:209`) as the only identity control; "Powers in play" 2-5 only. | Faction card with insignia, flagship silhouette, government name (Federation Council, High Council, Senate and Tal Shiar, Detapa Council, Ferengi Commerce Authority) [memory], leader name and a one-line canon pitch; "Interface look" becomes "Era" (kept) plus an automatic per-empire console family; Dominion card greyed "Coming" until M3. | M | 4 |
| Galaxy (2D and 3D map) [frame `pc-03`, `pc-07`, Simon's shot] | Top-left emblem is the Federation arrowhead (`lcars_elbow.gd:24`); inspector card heads use the same polygon (`galaxy_screen.gd:328,596`); own fleets a chevron (`galaxy_overlay.gd:262`, def `:450`); stations a hexagon (`:184-199`); contacts a diamond (`:222-232`); minor races a diamond ring (`:155`); anomalies a "?" (`:177-182`); legend "Your ships / Contacts / Station / Anomaly / Unexplored" (`galaxy_vm.gd:83-84`); layer chips "Territory, Supply, Routes, Trade, Fog, Grid, Labels" (`galaxy_vm.gd:9-10`); alerts "All systems nominal" (`galaxy_vm.gd:35`, already Trek-ish); system labels are `SYS-xx` in the test strip (`scenes/galaxy/tools/galaxy_strip.gd:92`, the real game uses star names). No neutral zone, no warp trails, no sensor sweep; "Sector 7.10" coordinates are grid numbers, not quadrant names. | Ship-class glyph per fleet; station glyph per type and empire; owner insignia on territory edges and capital markers; legend renamed "Starfleet vessels / Foreign contacts / Starbase / Spatial anomaly / Uncharted"; layers renamed "Territory, Supply range, Courses, Trade lanes, Sensor coverage, Grid, Names"; neutral-zone border style, warp-trail dashes, sensor sweep from owned bases; Stellar Cartography title strip with stardate. | M | 5 |
| System / colony [frame `pc-05`] | Frames "Workforce, Governor, Energy & shipyard, Construction, Build queue, Build templates" (`system_screen.gd:196,311,343,366,434,494`); "Governor" manual/balanced/growth; build icons are a generic clipboard in a circle (`widgets/choice_icon.gd:13-16`, used `system_screen.gd:410`); building names are original BOTF ("Class 1 Hunting Ground", "Matter Furnace"), which is fine; planet orbs are procedural and already carry Trek classes. | Per-empire building skins in name and icon (Federation "Starfleet shipyard", Klingon "Qo'noS warrior academy" [from BOTF data, check]); colony "Governor" -> "Colonial administrator" (Fed), "Warden"/"Subadar" per empire [memory, verify]; construction icons from the new set (see C1); "Build queue" -> "Fabrication queue" is optional. | M | 3 |
| Empire [frame `pc-06-empire`] | Banner is the emblem card (`empire_screen.gd:108-130`); "Powers" list uses the same polygons (`:219`); "Registry · Colonies" and column headers are table-speak; "Member worlds: None yet - befriend minor races in Diplomacy" is the only flavour. | Banner = insignia plus government plaque and head of state; "Registry" -> per-empire ("Federation Council roll", "Imperial Fleet roster"); Powers list with real insignia. | S | 3 |
| Research [frame `pc-06-research`] | Six field cards with ring "L1 OF 10" and no field icon (`research_screen.gd:150` only uses the generic "research" icon); field names Biotech/Computers/Construction/Energy/Propulsion/Weapons are original BOTF and fine; tech names are original BOTF too and good ("Warp Drive", "Hydroponics"); descriptions are flat prose (`data/base/technologies.json`). | Six tech-field icons (C1); one in-character quote per tech from an empire's scientist or officer (Alpha Centauri device [web]) shown under "NEXT:"; "Details" opens a Starfleet-science-department style note. | M | 4 |
| Fleets [frame `pc-06-fleets`] | Row icon is the kite "ship" glyph (`fleets_screen.gd:82`, `ui_icon.gd:70`); detail hero is a procedural saucer-or-wedge picked by random seed per design id (`widgets/ship_silhouette.gd:15-31`, used `fleets_screen.gd:129`), so a Klingon ship can draw a saucer; ships are "Oberth - Science Vessel #1" (class, role, numeric id; no name, no registry, no captain; `combat_vm.gd:102`, `shell_vm.gd:99`); "Task force" is fine. | Real silhouette per class from the roster (C1); ship names and registries (NCC-1701-style for Federation, IKS for Klingon, IRW for Romulan, CU/"Galor-class" numbers for Cardassian [memory]); commander rank and name on the row; "Retreat to Sol" -> "Fall back to Sol". | M | 4 |
| Diplomacy [frame `pc-06-diplomacy`] | The Hail panel is the emblem on a gradient (`portrait_slot.gd:43-57`, `diplomacy_screen.gd:287-318`); all 35 portrait keys are `placeholder`/`missing` in the manifest (`assets/asset-manifest.json`: `portrait.*` placeholder, `portrait.leader.*` and `hail.bg.*` missing); tabs "Dossier / Treaty ledger / Galactic news" (`:63-64,273`); news lines are the core's flat sentences (`Simulation.cpp:2292-2299`); proposals "X proposes a Y" (`:2270`); "Reputation: BLOC 11%" (`:135`). | Viewscreen: comm static frame, leader name and title, empire insignia, the leader's opening line per relation state, treaty text in the empire's voice; "Treaty ledger" -> "Accords"; Galactic news in a news-bulletin frame (Master of Orion GNN device [web]); minor races get their own plate. | L | 5 |
| Intel [frame `pc-06-intel`] | Cards "Internal security / Espionage / Sabotage / Gather intelligence" (`intel_screen.gd:72,96`); generic eye icon. | Per-empire agency name and mark: Starfleet Intelligence/Section 31 (hidden), Obsidian Order, Tal Shiar, Klingon Intelligence, Ferengi "Commerce Authority informants" [memory]; wording "Gather intelligence" -> "Run an operation". | S | 3 |
| Database [frame `pc-06-database`] | Table list, stat tiles, wireframe wedge silhouette (`database_screen.gd:203`), "LEGACY CATALOGUE" badge; the best raw material in the game (descriptions in `ships.json`, `minors.json`). | Library-computer style "Starfleet Records" frame; real silhouette; faction crest per entry (`database_screen.gd:197,278`); keep the descriptions, they are good. | S | 3 |
| Events [frame `pc-08-events`] | Modal "Galactic events", empty state "No galactic events concern your empire right now" (`overlays.gd:154`); spinning arc glyph for every event (`widgets/event_glyph.gd:9-15`, used `overlays.gd:167`); only 7 event kinds exist (`data/base/events.json`: Plague, Supernova warning, Subspace rift, Trade boom, Derelict vessel, Labor unrest, Scientific breakthrough). | Event cards as a "Priority transmission" with speaker (advisor, officer) and a per-kind glyph; add events from canon (Kobayashi-style incidents, Tholian web, Q [careful], temporal anomaly, Orion raiders, Maquis, Founders' warning) as data. | M | 4 |
| Victory [frame `pc-08-victory`] | Paths "Conquest, Domination, Scientific, Diplomatic", megaproject "Genesis Initiative" (`overlays.gd`, `Victory.cpp:141`); rings and bars. Genesis is canon-named but it is Federation flavour shown to every empire. | Per-empire path names and a per-empire megaproject name (candidates are a naming decision, not yet proposed here); title "Mission objectives" / "Objectives of the Council" per the C3 table. | M | 3 |
| Chronicle / turn report [frame `pc-08-chronicle`] | "TURN 12 REPORT", "Turn briefing · chronicle", "Sol completed Laboratory.", "ACKNOWLEDGE" (`overlays.gd:37-47`); category headers "CONSTRUCTION". | "Captain's log, stardate 2363.50" header; each line phrased as a log or officer report; "Acknowledge" -> "Make it so" is a quote, use "Log filed"; per-empire: "Captain's log" (Fed), "Warrior's log"/"Fleet commander's report" (Klingon), "Praetor's dispatch" (Romulan), "Central Command briefing" (Cardassian), "Ledger" (Ferengi) [memory]. | S | 5 |
| Battle [frame `pc-09`..`pc-12`] | "RED ALERT" header is good (`combat_vm.gd:97`); "Our forces / Hostiles / Orders / Ship systems / Combat log" are generic; order verbs (Engage, Assault, Charge, Strafe, Flyby, Harry, Circle, Evade, Hold, Ram, Hail, Retreat) are tactical-game words (`combat_screen.gd:385`); tiny owner polygons in lists (`:215,248,505`); combat log "Galaxy -> Negh'Var: 26 damage" (`combat_vm.gd`); end state "Engagement concluded". Hostile cloaked contacts are labelled "Cloaked contact" (`combat_vm.gd:43`). Bridge chatter and per-empire acknowledgements exist in audio (`botf_bridge_chatter.gd`, `botf_audio_cues.gd:100-135`). | Orders in bridge phrasing ("Fire at will" is a quote, so write original: "Open fire", "All ahead full", "Evasive pattern", "Hold position", "Open hailing frequencies" is a quote, use "Open a channel"); log lines in officer voice ("Tactical: Galaxy's phasers hit Negh'Var, shields at 74%"); red-alert transition; ship registry and name instead of "#3"; sub-system names already fine (WPN, ENG, SHD, SNS, CORE). | M | 4 |
| Menus, saves, settings [`overlays.gd:299-420`, `saves_screen.gd`] | "Game menu", "Slots", "stored in user://saves" (`saves_screen.gd:31`) | "Log entries" for saves ("Captain's log, stardate ..."), computer voice prompts; settings "Interface look" row stays. | S | 2 |
| Loading / transitions | None: screens fade 0.2 s (`ui_tokens.gd:25`); the web build downloads an empire audio pack silently (`app.gd:441-456`). No tips, no warp effect, no red alert wipe. | Warp-streak transition for turn end and sector change; per-empire loading tips (Rules of Acquisition for Ferengi, Klingon proverbs, etc., original wording). | M | 3 |
| End turn / end-turn check | "End turn" pill, "Preview turn" (`app.gd:~225`), orders counter "ORDERS 3". | Per-empire phrase ("Proceed" / "Engage" is a quote; use "Advance stardate" / "Next watch"). Turn counter shown with stardate (`shell_vm.gd:10`). | S | 2 |
| Top bar and rail [frame] | Elbow text "LCARS" on every empire (`lcars_elbow.gd:25`); rail labels Galaxy, System, Empire, Research, Fleets, Diplomacy, Intel, Database, Events, Victory, Chronicle (`app_routes.gd:7-14`, `shell_vm.gd:73-75`); KPIs Turn/Credits/Research/Industry/Food/Pop/Intel (`shell_vm.gd`). | Rail relabelled per empire (C3 table); elbow label per console family; top bar shows stardate large. | S | 4 |

---

## C. Cross-cutting systems

### C0. How the game decides "who you are" today (the structural cause)

- Empire identity is one thing only: `Lcars.accent_empire` picks a palette (`lcars.gd:25-40`), `Lcars.build_theme(empire)` rebuilds the Theme with that accent (`lcars.gd:181-183`, `app.gd:181-196`), and the five `lcars_<empire>.tres` files in `scenes/ui/theme/` are the same theme recoloured [repo]. Era (`UiTokens.ERAS`, `ui_tokens.gd:7-13`) is a separate user setting that changes radius, header style and surface tint, not per empire, and the Interface look row offers only Federation-show eras.
- Result: Klingon, Romulan, Cardassian and Ferengi players all play inside the Federation's LCARS. Shapes, type, labels, music identity of the UI chrome, rail names and elbow text are identical.
- The sound side is better: voice, music and acknowledgements resolve per faction through `FACTION_PREFIX` (`botf_audio_cues.gd:17`), and the rule "voice keys never cross factions" is enforced (`:25`).

### C1. Insignia and iconography

**Where the glyphs live now** (all of these are the "arrow/triangle" Simon means):

| What | Where drawn | Now |
|---|---|---|
| Empire emblem polygons | `lcars.gd:61-69` `EMBLEMS` (0 neutral diamond; 1 Federation arrowhead; 2 Klingon 8-point cross-star; 3 Romulan bird-ish 10-point; 4 Cardassian inverted wedge; 5 Ferengi empty list -> ring), drawn by `lcars.gd:559-571` `draw_emblem`, wrapped by `lcars.gd:573-586` `emblem()` / `EmblemView` | flat filled polygons |
| Elbow badge (top-left in Simon's shot) | `lcars_elbow.gd:24` `draw_emblem`, `:25` "LCARS" | Federation arrowhead |
| Inspector card heads on the galaxy screen | `galaxy_screen.gd:328` (`_card_head`), `:596` (station card) | emblem 30 px and 18 px |
| Title row | `title_screen.gd:29-32` | five 30 px |
| Faction cards and summary | `new_campaign_screen.gd:170` (16 px legend), `:219` (44 px summary), cards via `_faction_card` | flat |
| Empire banner and Powers list | `empire_screen.gd:108+`, `:219` | flat |
| Diplomacy list, proposals, ledger | `diplomacy_screen.gd:93,165,208` | flat |
| Intel cards | `intel_screen.gd:72,96` | flat |
| Combat lists | `combat_screen.gd:215,248,505` | 14 to 30 px |
| Database | `database_screen.gd:197` (54 px), `:278` | flat |
| End screen | `overlays.gd:265,286` | 84 px |
| Portrait slot fallback | `portrait_slot.gd:57` | emblem on gradient |
| Own fleet marker | `galaxy_overlay.gd:262` `_chevron`, def `:450-459` | chevron |
| Station marker | `galaxy_overlay.gd:184-199` | hexagon, dot for Starbase |
| Contact marker | `galaxy_overlay.gd:222-232` | diamond |
| Minor-race marker | `galaxy_overlay.gd:155` | diamond ring |
| Kite "ship" and "fleet" icon | `ui_icon.gd:70-73` | kite and pair of kites |
| Construction choice icon | `widgets/choice_icon.gd:13-16`, used `system_screen.gd:410` | arrow for ships, box for structures |
| Procedural hull picture | `widgets/ship_silhouette.gd:15-31` | random saucer or wedge by `seed_id` |
| Battle marker polygons | `scenes/battle/battle_markers.gd:236-243` | square, kite, dart, pentagon by hull role |
| Event glyph | `widgets/event_glyph.gd:9-15` | spinning arcs for every event |
| Planet-class and "?" | fine already (Trek classes) | n/a |

**Icons needed.** Drawn in-house as vector, one path per icon with a 24 px simplified and a 96 px detailed variant, each in a "tint" version (single colour, for lists) and a "plate" version (with the empire's frame). Names follow `UiIcon` style. I specify; nothing is pulled from outside. Reference designs are **[memory]**; the artist checks each against Simon's own library (`D:\TV Shows`, `D:\Movies`, per `docs/VOICE-PLAN.md` §1) before drawing.

Empire and power insignia (`ins.*`):

1. `ins.fed` Starfleet delta arrowhead (the combadge shape; keep the simple filled delta for 14 px use) and `ins.fed.ufp` the UFP seal (circle with laurel branches and star field) for treaties and diplomacy.
2. `ins.kli` Klingon Empire trefoil (three-bladed, the "tri-lobed" imperial emblem) and `ins.kli.tlh` the Klingon-script monogram for small sizes (optional).
3. `ins.rom` Romulan Star Empire bird of prey (wings spread, two small spheres for Romulus and Remus in talons).
4. `ins.car` Cardassian Union emblem (angular arrowhead/trident with chamfered frame; the current inverted wedge at `lcars.gd:67` is a placeholder for this).
5. `ins.fer` Ferengi Alliance seal (ornate round seal; a plain ring today, `lcars.gd:561-563`).
6. `ins.dom` Dominion emblem (three-armed asymmetric mark) for the Dominion once playable (M3) and for the roster ships already present (`roster.json`, 3 classes).
7. `ins.borg` Borg (no insignia exists; use a cube-over-circuit mark; used for threat contacts, 5 roster classes).
8. `ins.s8472` Species 8472 (organic tri-prong; 1 roster class).
9. `ins.breen`, `ins.hirogen`, `ins.kazon`, `ins.xindi`, `ins.krenim`, `ins.tholian`, `ins.gorn`, `ins.orion`, `ins.maquis`, `ins.devore` (roster factions with ships but no UI presence; one small mark each, can be monogram-on-shape until a lane has time).
10. `ins.neutral` unknown contact (keep the diamond but with a "?" notch).
11. Minor-race plates: one `ins.minor.<temperament>` plate per temperament (Mercantile, Warlike, Peaceful, Xenophobic and the others used in `minors.json`; confirm the set with a one-line query of the data) plus a 2-letter monogram, then specific marks for the 12 most recognisable minors: `ins.minor.vulcan` (IDIC/Vulcan mark), `bajoran` (Bajoran earring symbol), `andorian`, `betazoid`, `trill`, `bolian`, `tamarian`, `pakled`, `talarian`, `nausicaan`, `yridian`, `zakdorn` [memory for each reference].

Ship-class silhouettes (`hull.*`, top-down, tint and plate): a **family** per empire plus a role pip row, instead of 98 individual drawings.

- Families: `hull.fed` (saucer plus twin nacelles; sub-variants Miranda/Excelsior/Galaxy/Defiant by proportions), `hull.kli` (Bird-of-Prey wing, K't'inga forward boom, Vor'cha), `hull.rom` (warbird crescent), `hull.car` (Galor spade), `hull.fer` (D'Kora/marauder), `hull.dom` (Jem'Hadar beetle wedge), `hull.borg` (cube, sphere, diamond), `hull.s8472` (bioship), `hull.generic` (minor raider).
- Role pips (reads at 12 px): scout, colony ship, outpost ship, troop transport, destroyer, light cruiser, cruiser, heavy cruiser, battle cruiser, battleship/dreadnought, science vessel/explorer, raider, escort, starbase, orbital battery (15, from `ships.json` role names).
- Replace: `ui_icon.gd:70-73` ("ship", "combat", "fleet", "fleets") for list rows; `galaxy_overlay.gd:262,450-459` (map fleet marker: draw the family silhouette rotated to `heading`); `widgets/ship_silhouette.gd:15-31` (stop using `seed_id` randomness; pick by `raceId` and `hullRole` from the data); `widgets/choice_icon.gd:13-16`; `battle_markers.gd:236-243`.

Station and place glyphs (`st.*`, plate per empire): `st.outpost`, `st.starbase`, `st.shipyard`, `st.listening_post`, `st.science` for each of Federation, Klingon, Romulan, Cardassian, Ferengi (10 minimum: outpost and starbase per empire). Replace `galaxy_overlay.gd:184-199`. Also: `map.capital` (homeworld marker using the empire insignia on a ring), `map.colony`, `map.minor` (replaces `galaxy_overlay.gd:155`), `map.contact` (replaces `:222-232`, insignia of the owner), `map.anomaly.<kind>` for the `GalaxyFeatureType` set: nebula, wormhole, black hole, neutron star, ion storm, subspace rift (`game/Source/BotfSim/GalaxyFeatures.cpp:452-456`), `map.derelict`, `map.neutral_zone` (border style, not an icon).

Resource and tech-field icons (24 px, replace the generic `UiIcon` arms at `ui_icon.gd:44-64`): `credits` (Federation credit chip; Ferengi variant a gold-pressed latinum strip `credits.latinum`), `research` (tricorder), `industry` (foundry/replicator ring), `food` (keep grain; Federation could use a replicator pattern, optional), `population` (crew/combadge), `intel` (Okudagram bars), `morale`, `energy` (dilithium crystal), and the six tech fields which have no icon today (`research_screen.gd:150`): `tech.biotech` (DNA helix), `tech.computers` (isolinear chip), `tech.construction` (structural frame/girder), `tech.energy` (warp core or dilithium), `tech.propulsion` (nacelle with warp field), `tech.weapons` (torpedo or phaser array). Also `event.<kind>` x 7 now (plague, supernova, rift, trade, derelict, unrest, breakthrough) to replace the single spinning glyph at `event_glyph.gd:9-15`.

Delivery: all as `static func` path tables beside `UiIcon` (same pattern as `ui_icon.gd` `match` arms, which already scale on both renderers) or as SVG per the manifest key `ui.icon_set` named in `docs/UI-DESIGN-SYSTEM.md` §6. Total about 80 insignia/hull/station vectors and about 40 resource and event icons. Colour-blind mode (`lcars.gd:34-40`) is already satisfied because the silhouettes carry identity (ART-BIBLE §3).

### C2. Per-empire console identity

What the shows give each empire (all **[memory]**; the web fetches returned no console-design page for the aliens, only that the Okudagram was adapted for Klingon, Cardassian, Romulan and other alien displays [web: treknews.net, Wikipedia LCARS search results]). The artist should freeze-frame the actual shows (Simon has the library locally per `docs/VOICE-PLAN.md`) before drawing.

| Empire | On screen in the shows (brief) | Shapes | Colours | Type | Panel geometry |
|---|---|---|---|---|---|
| Federation | TNG/DS9 LCARS; TOS button-boxes; Picard S3 glass | rounded elbows, pills, rails | amber, lilac, sky blue on black (Enterprise-D warm, -E cool) | rounded grotesque (Swiss 911 style), already Antonio here | the current layout; TOS/movie/voy/late already in `UiTokens.ERAS` |
| Klingon | red-lit, hard-edged, ridged, little text, script glyphs | angular chevrons, notched corners, jagged lines, asymmetry | blood red, burnt orange, black, steel | blocky with angled cuts; pIqaD script for decoration | heavy bordered panels, bars with sawtooth ends |
| Romulan | green, curved, symmetrical, wide arcs (warbird panels) | long arcs, tapered wings, nested crescents | green with blue-grey, black | thin wide sans | symmetrical, centre-weighted, thin double rules |
| Cardassian | brown/ochre, text-dense Okudagram with chamfered rectangles (DS9 Ops) | stepped chamfers, trapezoids, hatching | ochre, rust, amber on dark brown-black | tight, dense, small caps | stacked horizontal bars, many small rectangles |
| Ferengi | gold, ornate, rounded, trade-chart grids (shows use mostly Federation/Cardassian hardware on DS9) | rounded bevels, coin circles | gold, orange, ivory | ornate italics | tabular ledgers |
| Dominion (M3) | organic, purple-teal, Jem'Hadar utilitarian | curved, biological | violet, teal | none/own script | sparse |

**How far the existing theme carries.** The seams are all present: `UiTokens.ERAS` already holds `radius`, `pill`, `header` (`bar | bracket | cap | hairline`), tint, panel and two colours, and `Lcars.frame` switches on `header_style()` (`lcars.gd:424-470`); `build_theme(empire)` already rebuilds per empire (`lcars.gd:181`); the elbow is drawn by `LcarsElbow` (`lcars_elbow.gd`) from a handful of rects and an arc. So:

- **S, no new engine work:** add a `UiTokens.FAMILIES` dict beside `ERAS`, keyed by empire, with `radius`, `pill`, `header`, `tint`, `panel`, a `rail_label` map, an `elbow_text`, and a corner style, and have `UiTokens.radius()` etc. read family first and era second. This gives Klingon square corners and a "chevron" header, Romulan "arc" header, Cardassian "chamfer" header, Ferengi "ornate" header with no new widget.
- **M:** two or three new header styles in `Lcars.frame` (`chevron`, `arc`, `chamfer`) and per-family elbow drawing in `lcars_elbow.gd` (Klingon notched bar, Romulan arc, Cardassian stepped), plus the `Button`, `Panel`, `Tabs` StyleBoxes in `build_theme` reading a corner shape (Godot `StyleBoxFlat` cannot chamfer; use a small `StyleBoxTexture`/`StyleBoxFlat` + custom draw or `Polygon`-backed panel for those families; the Compatibility renderer rule in UI-DESIGN-SYSTEM §1.5 allows only Control and CanvasItem drawing, which is fine).
- **L, needs art and a licence decision:** per-empire display fonts. Only Antonio, Inter and Share Tech Mono are bundled (`scenes/ui/fonts/`). A Klingon and a Romulan decorative script and a Cardassian dense face need Simon's word on adding font files (see E4); fall back to glyph art (insignia-drawn headings) if not.
- Era stays orthogonal: Federation can still be TOS/TNG/DS9/VOY/late; non-Federation families get an era tint only (tint and surface), not a different shape.

Precedent [web]: Starfleet Command gave each faction "an individualized HUD, with controls written in alien script and unique panel layouts", reviewers called it a high point, and SFC III removed them (Ex Astris Scientia). Star Trek: New Horizons ships Borg, Romulan, Klingon, LCARS, Dominion, Cardassian and Ferengi UI as separate add-on UI mods for Stellaris (Paradox forum search result). Both support doing it, and the lesson from SFC III is to keep it in the design rather than cut it later.

### C3. Language and writing

Strings that read as generic 4X, with where they live:

| Now | Where | Trek replacement (examples, original wording) |
|---|---|---|
| "Turn 12 report" | `overlays.gd:39` | "Captain's log, stardate 2363.50" (Fed); Klingon "Warrior's log, stardate ..."; Romulan "Praetor's dispatch"; Cardassian "Central Command briefing"; Ferengi "Ledger, close of day 12" [memory on terms] |
| "Sol completed Laboratory." | core `Simulation.cpp` report events (e.g. `:2686` "Terraforming of X completed."), `Chronicle.cpp` | "Chief Engineer's report: the Sol laboratory is operational." Done as a Godot-side phrase table keyed by event category, so no core change |
| "Klingon Empire declared war on Federation." | `Simulation.cpp:2292` `describeDiplomaticEvent` | Per-speaker line: "The Klingon High Council has declared war on the Federation. Starfleet is at red alert." |
| "X proposes a Y" / "Diplomacy: ..." | `Simulation.cpp:2270,2273` | "Ambassador X transmits a proposal for a trade agreement. Respond by stardate ..." |
| "Galactic events" / "No galactic events concern your empire" | `overlays.gd:154,156` | "Subspace bulletin" / "No priority transmissions" |
| "Enemy contact" modal and "Cloaked contact" | `combat_screen.gd:462`, `combat_vm.gd:43` | "Sensors detect hostile vessels", "Unidentified vessel" |
| "Retreat to Sol" | galaxy inspector, `orders.gd` | "Fall back to Sol" |
| "Acknowledge" | `overlays.gd:47` | "Log filed" |
| "Credits", "Industry", "Food", "Pop" top bar | `shell_vm.gd` stats | Keep "Credits" (Federation credits are canon); Ferengi bar shows "Latinum"; Klingon "Honour" no (invented); keep numeric words generic, fix only Ferengi |
| "Governor" (colony) | `system_screen.gd:311` | "Colonial administrator" (Fed); Klingon "Governor-warrior" no; Cardassian "Gul/Legate" mapping is a rank, see below |
| Rail: Galaxy, System, Empire, Research, Fleets, Diplomacy, Intel, Database | `app_routes.gd:7-14` | per empire table below |
| "Stardate 2360.0" and "Stardate %.1f" | `shell_vm.gd:10` (`2360 + turn/4`, remainder `(turn%4)*25` printed with `%d`, giving "2360.0, 2360.25, 2362.50") and `overlays.gd:289` (`41000 + turn*25.7`) | one `Stardate.format(turn, era)` helper: era-correct format (TOS "2260.1", TNG "41153.7" style, DS9 "52000s") [memory]; both call sites use it |

Rail and frame names per empire (Federation = base; originals in brackets):

| Screen | Federation | Klingon | Romulan | Cardassian | Ferengi |
|---|---|---|---|---|---|
| Galaxy | Stellar cartography | Star chart | Strategic chart | Sector map | Trade chart |
| System | Colony | Holdfast | Province | Prefecture | Holding |
| Empire | Federation Council | High Council | Senate | Detapa Council | Commerce Authority |
| Research | Science | Weapons forge | Research directorate | Science ministry | Acquisitions research |
| Fleets | Starfleet | Imperial fleet | Fleet of the Empire | Central Command | Fleet of the Alliance |
| Diplomacy | Diplomatic corps | Embassy | Envoy corps | Foreign office | Negotiations |
| Intel | Starfleet Intelligence | Klingon Intelligence | Tal Shiar | Obsidian Order | Informants |
| Database | Starfleet records | Archive | Archive | Cardassian archive | Rules of Acquisition |
| Events | Subspace bulletin | Messengers | Messengers | Bulletins | Market news |
| Victory | Mission objectives | Path to glory | Destiny | Union ascendancy | Profit |
| Chronicle | Captain's log | Warrior's log | Praetor's dispatch | Briefing | Ledger |

These organisation and office names are **[memory]**; the data file should be reviewed by a Trek-literate pass (Odo-style audit by another seat, not the writer).

Ranks, ships and names: no ranks exist in the UI (grep found none); ships are "<class> - <role> #<id>" with no name, registry or captain. Propose: ship registry prefixes by empire (NCC-xxxx with "USS" for Federation, "IKS" Klingon, "IRW" Romulan [Romulan IRW is from the Star Trek canon-adjacent; verify], Cardassian "Gul" captains and unit numbers, Ferengi "FCA" ships [memory]); ranks Ensign to Fleet Admiral (Fed), Klingon "Commander (Captain)/General", Romulan "Commander/Admiral", Cardassian "Gul/Glinn/Legate". Name pools per class and empire (Enterprise, Defiant, Excelsior, Yamato...) in a data file. The data to hold them is new; the core's `Fleet` has an id and a design only (`Simulation.h`), so ship names are a presentation-layer map keyed by fleet id, with a core field only if Simon wants names in saves (see E5).

Flavour writing that already exists and is good: `minors.json` descriptions, `ships.json`/`roster.json` descriptions ("The Union's standoff bombardment ship, built around a deep torpedo magazine", etc.), `technologies.json` and `buildings.json` descriptions, faction mottos (`lcars.gd:42-46`: "Glory is earned in fire." etc.). The gap is at turn-time writing, not in the catalogue.

Alpha Centauri device [web]: each tech comes with a spoken quote from a faction leader, tied to the leader's philosophy; the quotes were written to carry faction politics (fan discussion). Proposal: add `quote` and `speaker` fields per technology and one per structure completed, written per empire (the four BOTF empires disagree about the same tech), shown under "NEXT:" on the research ladder (`research_screen.gd`) and on the completion toast. 77 technologies times 5 empires is 385 lines; first pass: the 66 named techs for Federation and Klingon (132), the others later. Writing lane, original text, checked for quotation by the proof seat.

### C4. Sound and voice (manifest and code only; nothing played)

What exists:

- Pack `scripts/audio/pack` (71 MB): `shared/` (75 keys: explosions, warp, generic weapons, `sfx.ui.*` x 8, alerts x 4, shield, transporter, 5 music keys) and five empire packs each with `cues.json`, voice, sfx, music: `fed` 80 keys, `kli` 55, `rom` 49, `car` 47, `fer` 47 (counted from the `cues.json` files). Per empire: `vo.advisor.*` (research, construction, colony, attack, fleet, contact, diplomacy, war, intel, terraform, treaty), `vo.diplomacy.*` x 7, `vo.victory.*` x 5, `vo.station.*` x 2, and combat acknowledgements `vo.combat.*` (Federation 52, Klingon 29, others 21). Era twins (`.tos .ent .tng .ds9 .voy`) exist for weapons in `shared` (51 twin keys) and for Federation and Klingon voice only.
- Code: the cue registry (`botf_audio_cues.gd:30-77`), era twins (`:84-110`), bridge chatter (`botf_bridge_chatter.gd`), a faction-keyed music table (`:138+`, `mus.botf.galaxy.{fac}`), a cue scheduler and mix buses (`botf_audio_mix.gd`). It is attached at app start (`app.gd:76`), end turn plays (`:488`).
- Sources are Simon's own library clips and the original BOTF bank, private, gitignored (`pack/README.md`, `docs/VOICE-PLAN.md`).

What is missing:

- No empire-specific UI sound sets: `sfx.ui.*` has only 8 shared keys; a Klingon click is a Federation LCARS beep. No per-empire alarm: `sfx.alert.red` is shared (4 keys); a Federation red alert klaxon is used for every empire.
- Only `rom` carries `sfx.cloak`/`sfx.decloak` (one key each); Klingon cloak, Cardassian, Ferengi have none.
- No ship-computer voice (VOICE-PLAN §1 source E, "last resort", not built): no "Working", "Acknowledged", "Red alert", "Incoming transmission". Hails use `vo.diplomacy.greet.{other}`, which has 7 lines per empire in the pack but is not wired to a viewscreen UI.
- Non-Federation empires have 8 advisor keys (one clip each, `vo.advisor.attack` 1 etc.), so repeated events repeat the same line.
- Shared pack is over its size budget: 19.4 MB against 15.7 MB (`pack_manifest.json` `packs.shared`; `violations` lists it). Not a Trek issue but it blocks web export of more sound.
- Music: `mus.botf.galaxy.{fac}` resolves per empire where files exist; no battle-per-empire distinction I could verify from the manifest alone, and no era-specific score keys (`with_era` skips `mus.` keys: `:97`).

Proposals: (1) `sfx.ui.<empire>.click/open/close/endturn/turn` per empire (five LCARS-vs-alien sets; Federation keeps the LCARS beeps) from the library's actual UI sounds; (2) `sfx.alert.red.<empire>` (klaxon variants: Klingon alarm, Romulan and Cardassian alert tones, a Ferengi bell) and wire `encounter_opens`, `war_declared`, `system_under_attack` to them; (3) a ship-computer set (about 30 lines: acknowledgements, "incoming transmission", "red alert", "stardate"), original synthetic voice per VOICE-PLAN §1 E; (4) widen advisor lines to 3 per event type per empire where the library has them; (5) bridge ambience per empire (engine hum, Klingon drums, Romulan, DS9 Ops) as a very quiet loop on the Galaxy screen; (6) turn-report read-aloud in the empire's advisor voice (see viewscreen below). Never play audio during tests (`--audio-driver Dummy`, pack README).

Precedent [web]: Armada gave unit acknowledgement lines per faction, with named flagships (Enterprise, Defiant) getting special lines (Ex Astris Scientia, Armada reviews). Master of Orion (2016) gave each race its own advisor and a GNN news anchor (Wargaming cast announcement). Civilization VI gives each civilization several music tracks of its own (CivFanatics thread, anecdotal). Here the pack already beats most of that; it needs the UI sound layer and the viewscreen to be heard.

### C5. Moments

| Moment | Now | Proposal |
|---|---|---|
| Intro / title | static UI over live fleet (`title_screen.gd`), no film | 15 to 20 s procedural title sequence on the existing 3D scene: stardate caption, fleet warps in, per-empire insignia fades up. No third-party clip. Per-empire title card using the player's last empire (`title_empire()` already exists) |
| First contact | audio cue exists (`first_contact`, `botf_audio_cues.gd:46`), UI: new contact appears in lists | viewscreen hail: "Incoming transmission from the ...", leader on comm static, first-contact line in the empire's voice; a log entry |
| Declaration of war | red alert sound and line in news | full-screen red-alert overlay, leader viewscreen with the declaration, fleet status card; per-empire wording |
| Treaty signing | beep and a ledger line | "Accord signed" card with both insignia and a seal (UFP seal for Federation treaties), advisor voice line |
| Victory / defeat | big "VICTORY"/"DEFEAT" with a 84 px emblem (`overlays.gd:286-296`), stardate line uses a different formula than the top bar | per-victory-type card in empire voice ("The Federation has ... Admiral, this log ends"); end-of-game speech audio from the original BOTF bank is already listed as a source in `VOICE-PLAN.md` §1 A |
| Ship launch / commissioning | Chronicle line "X completed Y" | "Commissioning" toast with the ship name and registry (needs ship names), launch whoosh |
| Red alert on battle entry | present: header "RED ALERT" and `encounter_opens` sfx | add the red-alert wipe and per-empire alarm; "Yellow alert" for fleets in contact range |
| Map details | "Sector 7.10"; no neutral zone; features exist in the core (`GalaxyFeatures.cpp`: nebula, wormhole, black hole, neutron star, ion storm, subspace rift) | canon labels on them ("Nebula", "Wormhole (Bajoran-style) / Subspace rift", "Ion storm"), Neutral Zone border between Federation and Romulan at start (needs a rule; see E3), warp-trail dashes (dashes already flow in `galaxy_overlay.gd:432-447`), sensor sweep from bases, "uncharted" shimmer |
| Leader portraits | all 35 portraits and all leader/hail backdrops placeholder or missing (`assets/asset-manifest.json`) | in-house vector comm-viewscreen: silhouette bust plus insignia plus lines; if Simon wants real likenesses that is a rights question (E7) |

Precedents [web]: Master of Orion 2016's GNN news anchors and per-race advisor voices; Civ VI's diplomacy screen with agendas and gossip (fandom Diplomacy page search result); Alpha Centauri's leader monologues and secret-project videos (Wikipedia). All of these are viewscreen-and-voice devices, which is exactly what BOTF's diplomacy screen lacks today.

### C6. Canon furniture

- **Stars:** generated systems take a list of 120 real star names then "Gliese N" (`Simulation.cpp:3865-3884`). Many are real stars used in Trek charts (Tau Ceti, Wolf 359, Epsilon Eridani, Alpha Centauri, Barnard's Star, Proxima, Rigel, Vega, Altair, Algol, Betelgeuse...). That is the right base; add canon-only names where the stars are fiction (Qo'noS, Romulus, Cardassia, Ferenginar are capitals already: `lcars.gd:42-46`), plus canon sites placed deterministically by a seed rule: Vulcan/40 Eridani, Andoria, Tellar, Risa, Betazed, Bajor, Deneb IV, Khitomer, Cestus III, Ceti Alpha V, Organia, Praxis, Genesis/Regula, Narendra III, Starbase 74, Deep Space 9 (as a station name), Wolf 359 as a battle site. **[memory]** for canon placements. Core change (one list), plus a table of "hint planets and flavour tags" in `data/`.
- **Species:** 30 minor races exist with canon names (`minors.json`: Acamarians through Zakdorn) and signature ships. Missing from minors but present as roster factions: Breen, Gorn, Tholian, Orion, Hirogen, Kazon, Xindi, Maquis, Krenim, Devore; Borg and Species 8472 as threat powers are planned (MASTER-PLAN M3), not yet present as UI actors.
- **Ships:** the roster is the strongest canon asset: 98 classes across eras (`roster.json`: Enterprise, Original, Movie, NextGeneration, DominionWar, Nemesis, Kelvin, Future) with era-matching and source tags (`sourceKind`, `source`). Federation 36, Romulan 9, Klingon 8, Cardassian 5, Borg 5, Xindi 5, Dominion 3. Gaps: Ferengi 2, Breen 1; none per-empire for the original five at all eras (the legacy catalogue has 20 each).
- **Events:** 7 event kinds (`events.json`), all generic disasters. Trek-flavoured additions proposed as data (no new rules needed for the first batch because the schema is kind/choices/effect): "Temporal anomaly", "Tholian web", "Orion raiders", "Plasma storm", "Distress call", "Q-like entity" [needs Simon: it is an iconic character, original wording only], "Founders' infiltrator" (Dominion era), "Maquis rising" (Cardassian border), "Pakled request", "Bajoran wormhole" (era-gated), "Cardassian-Klingon war" background (events that reference the factions rather than the colony). Star Trek: Infinite does exactly this with scripted and emergent faction events tied to canon plots, and mission trees tied to the shows (Enterprise-D focus tree, recruiting Data, Worf, Troi) [web: Wikipedia, search results]. Our equivalent is a small canon "arc" per empire (see E6).

Comparison summary of devices found (all [web] unless marked [memory]), with what to take:

| Game | Concrete device | Take |
|---|---|---|
| Birth of the Federation (1999) | Five playable races with in-lore strengths (Federation diplomacy, Klingon military, Romulan espionage, Ferengi trade, Cardassian production); minor races as bonuses; "attractively presented"; IGN found menus "really cool-looking" but tedious late game; criticised for diplomacy not matching the shows and AI Federation demanding bribes (Wikipedia, Ex Astris Scientia). Per-race advisors and intro films I could not confirm online; from memory the original had per-race interface skins and intro movies, verify against Simon's copy. | Keep the strengths; fix diplomacy voice. Confirm original skin and film details from the installed game before copying them |
| Ultimate Dominion Mod | Dominion replaces Ferengi as playable; many minors replaced with new ones, each with a unique special structure; new ships; new intro video and race intro for the Dominion (Armada Fleet Command forum) | Dominion as optional sixth; unique structure per minor; per-race intro text |
| Star Trek: Armada I/II | Named hero ships with special voice lines (Picard calls Red Alert; Worf "good day to die" [Ex Astris Scientia: reviewer's note]); Klingons "sound like Klingons"; Borg and Armada II species 8472 with different economy and no shipyards, Ferengi traders (trekcore) | Named flagships with unique lines; per-empire economy flavour; 8472 as an alien non-shipyard threat |
| Starfleet Command series | Individualised HUD per faction with alien script and unique panel layouts (high point); SFC III removed them (Ex Astris Scientia) | Per-empire console family; do not cut it later |
| Bridge Commander | Crew at stations call out status; mission logs read by crew; Data guest on the Sovereign (GOG listing, GameSpot, PC Gamer search results) | Officer-voice report lines; named crew on events |
| Star Trek Online | Factions share story but differ in NPCs, ships and starting zones; Dominion limited to few species; faction allegiances (search results from forum and wiki) | Per-faction start region flavour; ship access differences |
| Star Trek: Infinite (2023) | Faction mission trees tied to canon (Federation: Enterprise-D, recruit Data, Worf, Troi; Klingon war; Cardassian spying; Romulan stealth); ship designer; scripted events referencing the shows; bugs and "85% Stellaris" criticism (Wikipedia, PC Gamer search, reviews) | Canon arcs and crew; avoid generic Stellaris UI and reference guides missing |
| Stellaris: ST New Horizons | Total conversion with 90+ pre-scripted Trek races with animated portraits, district economy, 1300 techs, per-empire UI add-ons (Klingon UI, Romulan UI, Cardassian UI, LCARS UI, Dominion UI, Borg UI, Ferengi UI) (Paradox forum search results) | Per-empire UI is an established community need |
| Star Trek: New Civilisations | I could not retrieve a page for this one; not assessed | n/a |
| Star Trek Fleet Command, Resurgence | Resurgence leans on dialogue and relationships, captures '90s tone (PC Gamer, Empire search results); Fleet Command not retrieved | Tone and dialogue as the reference for writing; Fleet Command not assessed |
| Stellaris | Species packs started as portraits, ship sets, city sets and advisor voices, later events and civics; ~100 animated portraits at launch; event chains per species (Paradox dev diaries search result) | Portrait plus voice plus event chain per race |
| Endless Space 2 | Faction campaigns with their own stories delivered by quests and exclusive artwork; per-faction music as DLC (e.g. 7 Harmony tracks); faction tech variants (PC Gamer, pcinvasion, forum search results). I found no faction-specific UI theming | Per-faction quest-line and exclusive music |
| Civilization V/VI | Per-region or per-civ music, agendas, gossip, leader diplomacy screens (fandom/CivFanatics search results); leader scene art not retrieved | Leader diplomacy with a visible personality and agenda line |
| Sins of a Solar Empire 2 | Three factions with distinct identity (TEC, Advent, Vasari); I found no UI/music detail | not assessed |
| Master of Orion (2016) | Per-race advisors, GNN news anchors, narration; diplomatic writing criticised as repetitive (Wargaming cast announcement, imperium.news review) | News anchor frame; vary the lines |
| Galactic Civilizations IV, Distant Worlds 2 | Not retrieved | not assessed |
| Alpha Centauri | Leader monologues per tech discovery, secret-project videos, Datalinks encyclopedia, faction philosophies (Wikipedia, TV Tropes, CivFanatics) | Tech quotes per empire; the benchmark for writing |

Not assessed because I could not retrieve a source: ST: New Civilisations (the search returned a forum link but I fetched nothing from it), Fleet Command, GalCiv IV, Distant Worlds 2, Sins 2 audio/UI, Civ leader scenes.

---

## D. Ranked build list (top 15, impact per effort)

Rule for lanes: three per round, each owning a separate set of files. Round order is dependency order. Each line is what / where / done-when.

**Round 1 (UI atoms, in parallel)**

1. **Insignia set** (impact 5, M). New `widgets/insignia.gd` with vector path tables for the section C1 list (majors, Dominion, Borg, 8472, roster factions, minor plates); `lcars.gd:61-69,559-586` `EMBLEMS`/`draw_emblem`/`emblem()` call it, so all 40 call sites change at once without edits. Files: `widgets/insignia.gd` (new), `lcars.gd` (emblem region only), `lcars_elbow.gd:24`. Done when every empire emblem call renders its own insignia at 14, 30 and 84 px in both renderers, the Federation's top-left elbow shows the delta (Simon's galaxy shot), and a smoke frame of title, new campaign, galaxy and diplomacy shows no polygon placeholder.
2. **Ship, station, contact and resource glyphs** (impact 5, M). New `widgets/ship_glyph.gd` (hull families, role pips, station glyphs, anomaly marks, tech-field and resource icons from C1); replace `galaxy_overlay.gd:155,184-199,222-232,262,450-459`, `ui_icon.gd:70-73` and the credit/energy/research arms, `choice_icon.gd:13-16`, `ship_silhouette.gd:15-31`, `battle_markers.gd:236-243`, `event_glyph.gd`, `research_screen.gd:150` (six field icons). Done when the chart shows a Starfleet-vs-Klingon silhouette per fleet, stations read by type, the six fields have icons, and `test_galaxy_map.gd` and `test_campaign_ui.gd` still pass. Files: those listed only; does not touch `lcars.gd` emblem region.
3. **Stardate, captain's log and Trek retitling** (impact 5, S to M). New `scripts/trek_text.gd` (one `Stardate.format(turn, era)`, empire phrase tables from C3, rail names); `shell_vm.gd:10`, `overlays.gd:37-47,154-156,289`, `app_routes.gd:7-14`, `shell_vm.gd:73-75`, `lcars_elbow.gd:25` text only, `saves_screen.gd:31`. Done when top bar, chronicle and end screen print the same stardate, the turn report reads "Captain's log, stardate ..." for the Federation and the empire's own title for the others, the rail shows the per-empire names, and `test_campaign_ui.gd` expectations are updated for the new strings. Files: `trek_text.gd`, `shell_vm.gd`, `overlays.gd`, `app_routes.gd`, `saves_screen.gd`.

**Round 2 (identity foundations)**

4. **Per-empire console family** (impact 5, L). `UiTokens.FAMILIES` plus new header styles `chevron`, `arc`, `chamfer`, `ornate` in `Lcars.frame` (`lcars.gd:424-470`), per-family elbow in `lcars_elbow.gd`, `build_theme` corner shapes, elbow text per family, rail shape. Files: `ui_tokens.gd`, `lcars.gd` (frame/theme regions), `lcars_elbow.gd`, `app.gd` chrome (`:191-260`). Done when a Klingon, Romulan, Cardassian and Ferengi campaign each show a visibly different frame set in smoke frames (title, galaxy, system), the Federation view is unchanged pixel for pixel, and the era setting still works. Needs E4 (fonts).
5. **Per-empire UI sounds, alarms, computer voice** (impact 4, M to L). Add `sfx.ui.<empire>.*` and `sfx.alert.red.<empire>` keys to `botf_audio_cues.gd` registry with fallback to the current shared keys, extend `tools/mine_trek_audio.py` and the pack build, ship-computer set (about 30 lines) from original synthetic voice. Files: `scripts/audio/*`, `tools/*audio*`, pack. Done when the cue trace (`test_cue_trace.gd`, run with `--audio-driver Dummy`) shows every UI click/alert for each empire resolving to its own key or the shared fallback and the listen sheet is regenerated for Simon.
6. **Canon furniture in the data and core** (impact 3, S to M). Canon system names placed by seed rule (`Simulation.cpp:3865-3884`), canon anomaly names and a Neutral Zone feature (`GalaxyFeatures.cpp:452-456`), ten Trek events (`data/base/events.json`, `GalacticEvents.cpp`), roster faction presence for Breen, Gorn, Tholian, Orion. Files: `game/Source/BotfSim/Simulation.cpp`, `GalaxyFeatures.cpp`, `GalacticEvents.cpp`, `data/base/events.json`. Done when a fresh Medium galaxy lists named canon sites, the Neutral Zone appears in the preview, the new events fire in the Frontier drill, and the C++ suites and the core tests still pass with updated name fixtures (core text change: sim lane to confirm no determinism golden depends on the names).

**Round 3 (moments)**

7. **Diplomacy viewscreen and per-empire voice** (impact 5, L). New `widgets/hail_viewscreen.gd` (comm static frame, leader name, insignia, vector bust), per-empire diplomacy phrase table `scripts/trek_voice.gd` mapping core event types and treaty names to empire-voiced lines (do not change `Simulation.cpp:2292-2299`), wire `vo.diplomacy.greet.{other}` audio. Files: `diplomacy_screen.gd:83-318`, `portrait_slot.gd`, `trek_voice.gd`, `widgets/hail_viewscreen.gd`. Done when contacting an empire opens the viewscreen with that empire's insignia, name, a relation-state line and its greeting sound, the news tab reads in voice, and `test_campaign_ui.gd` passes. Needs E7.
8. **Galaxy-map Trek furniture** (impact 4, M). Neutral Zone border style, warp-trail and sensor-sweep layers, anomaly marks per kind with canon names, legend and layer renames, quadrant-style sector labels. Files: `galaxy_overlay.gd`, `galaxy_map.gd`, `galaxy_vm.gd:9-10,83-84`. Done when the 3D disc map (Simon's reference shot) shows the new layers with the existing zoom levels and `test_galaxy_map.gd` passes.
9. **Title, victory and commissioning moments** (impact 4, M). Title sequence and per-empire title card (`title_screen.gd:15-110`), end screen per victory type in empire voice (`overlays.gd:272-330`, the end-screen region only), commissioning and red-alert wipe toasts (`app.gd` toast hooks, conflict-free if the console lane has closed). Done when the title shows the player's empire insignia, motto and stardate, victory and defeat read in the winning empire's voice, and a built ship fires a commissioning toast. Files: `title_screen.gd`, `overlays.gd` end-screen region, `title_fleet.gd`.

**Round 4 (names and quotes)**

10. **Ship names, registries, commanders** (impact 4, M). Presentation-layer name pools per class and empire and registry formats (NCC / IKS / IRW / etc.) keyed by fleet id; new `data/ship_names.json`; `combat_vm.gd:43,102`, `shell_vm.gd:99`, `fleets_screen.gd:82-129`, `database_screen.gd` unchanged. Core `Fleet` field only if E5 says yes. Done when fleets, combat and palette show "USS Oberth NCC-xxxx" style names and the same ship keeps its name across save and load.
11. **Tech and structure quotes, building skins** (impact 4, M). `quote` and `speaker` fields per tech and per empire in `data/base/technologies.json`; show in `research_screen.gd` ladder and completion toast; per-empire structure display names in `buildings.json` where BOTF already differs. Done when each Federation and Klingon tech has an in-voice quote, the research screen shows it, and a proof seat has checked there is no quoted show dialogue.
12. **Minor-race treatment** (impact 3, M). Race plates and signature marks from C1 item 11, per-race join and refusal lines (`minors.json` extension), minor-race hail viewscreen reuse. Files: `minors.json`, `diplomacy_screen.gd` minor branch, `database_screen.gd` minors tab, `portrait_slot.gd:25-27` keys.

**Round 5 (battle voice, advisors, flavour)**

13. **Battle Trek layer** (impact 4, M). Bridge-phrase order labels and log lines, red-alert wipe, per-empire alarm hook, registry names in lists (uses item 10 names), per-empire bridge chatter selection. Files: `combat_screen.gd`, `combat_vm.gd`, `scenes/battle/battle_markers.gd` (silhouettes from item 2), `botf_bridge_chatter.gd`.
14. **Advisors** (impact 4, M). A named advisor per empire (Fed science officer, Klingon weapons master, Romulan Tal Shiar liaison, Cardassian Legate, Ferengi accountant) delivering event cards in speaker voice with the existing `vo.advisor.*` audio; replaces the spinning glyph card layout (`overlays.gd:150-215`). Done when an event card shows the advisor's insignia, name, line and plays the advisor clip.
15. **Loading, tips and tooltip flavour** (impact 3, S to M). Warp-streak transition, per-empire tip lines (Ferengi acquisition rules, Klingon sayings, Cardassian slogans, Starfleet protocols, all original wording), event-card footers. Files: `scenes/ui/screens/*` loading overlay (new), `trek_text.gd` extension.

---

## E. Open calls for Simon (answerable from a phone)

1. **Dominion and Borg as full powers in the UI?** The roster already has Dominion, Borg and 8472 ships. (A) Dominion playable as a sixth power with its own insignia and console (matches the 8 Nov approved "a playable Dominion"); Borg and 8472 as threats only. (B) Insignia drawn for all three now, playable later. Which: A or B?
2. **Real show insignia, drawn in-house, OK?** The Starfleet delta, Klingon trefoil, Romulan bird, Cardassian and Ferengi marks are all from the shows. Drawing them as our own vectors from memory of the shows, for your private copy only, is what you asked for. Confirm: yes or no? (If no, I would use invented per-empire marks instead.)
3. **Neutral Zone as a rule?** A Federation-Romulan neutral zone that blocks entry or carries a treaty penalty changes play. (A) Visual border only, no rules. (B) Rule: crossing triggers a diplomatic incident. A or B?
4. **Per-empire fonts.** The Klingon and Romulan scripts and a denser Cardassian face need new free font files in the repo, which is third-party content. (A) Add free fonts you approve. (B) Draw all alien lettering as in-house glyph art, no new fonts. A or B?
5. **Ship names in saves.** (A) Names and registries are presentation only, generated from the fleet id, never saved. (B) Add a name field to the core so each ship keeps a name you can see and rename. A or B? (B is a rules-core change.)
6. **Canon arcs per empire.** Star Trek: Infinite uses faction mission trees tied to canon episodes. (A) Small event arcs (3 to 5 linked events per empire, e.g. Khitomer, the Maquis, Dukat's rise, a Ferengi trade war). (B) Only stand-alone events. A or B?
7. **Leader portraits.** All portraits are placeholder. (A) In-house vector comm-viewscreen with a silhouette bust, no likenesses. (B) Source real likenesses from your own screen-grabs for private use. A or B?
8. **Writing voice.** Should captain's-log style speak in your own voice as the captain (first person) or a generic Starfleet clerk? A: first person ("Captain's log... we have ..."). B: neutral ship's computer or officer report.
9. **Era versus empire look.** For non-Federation empires, keep the era choice (TOS, TNG, DS9, Voyager, late) as a tint only, or give each empire a single fixed look? A: tint only. B: fixed.

---

## F. Sources read

Repo (read today): `standing\simon\botf-rulings.md`; `docs/UI-DESIGN-SYSTEM.md`, `ART-BIBLE.md`, `VOICE-PLAN.md`, `research/CURRENT-STATE-AUDIT.md`, `design/WS11-AUDIO-AND-VOICE.md` headings; code and data listed inline; frames `remaster\captures\2026-11-09\134817\smoke\*.png`: opened 19 of 24 (`pc-01`, `02`, `03`, `04`, `05`, `06-empire`, `06-research`, `06-diplomacy`, `06-fleets`, `06-intel`, `06-database`, `07`, `08-events`, `08-victory`, `08-chronicle`, `09-enemy-contact`, `09b-plan-phase`, `11-battle-cinematic`, `12-battle-end`, plus the galaxy upload); NOT opened: `pc-09c-aimed-round`, `pc-10-battle-round2`, `pc-11a-battle-overview`, `pc-11b-planet-vista`, `pc-13-24v24-cinematic` (battle camera variants; the battle frames I did see cover the HUD), and Simon's upload `...\uploads\399fcc84-...\cd8ea117-image.jpg`.

Web (fetched or in search results; pages that returned a stub or truncated text are noted):
- Wikipedia, Star Trek: Birth of the Federation: https://en.wikipedia.org/wiki/Star_Trek:_Birth_of_the_Federation
- Ex Astris Scientia game reviews (BOTF, Armada, Starfleet Command): https://www.ex-astris-scientia.org/reviews/games.htm
- Game Revolution BOTF review (search result): https://www.gamerevolution.com/review/33289-star-trek-birth-of-the-federation-review
- Armada Fleet Command, Ultimate Dominion Mod III thread (search result): https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?p=16019
- Unity One Wiki Ultimate Dominion (search result): https://unityone.fandom.com/wiki/File:Birth_of_the_Federation_alternate_cover.jpg
- Armada II empires, TrekCore: https://gaming.trekcore.com/armada2/empires.html
- Wikipedia Star Trek: Armada II: https://en.wikipedia.org/wiki/Star_Trek:_Armada_II
- Combat Sim Armada II review (search result): https://combatsim.com/memb123/htm/2002/02/sta2
- Wikipedia Star Trek: Infinite: https://en.wikipedia.org/wiki/Star_Trek:_Infinite
- PC Gamer Star Trek Infinite review (page returned truncated): https://www.pcgamer.com/star-trek-infinite-review
- Windows Central, Infinite review (search result): https://www.windowscentral.com/gaming/pc-gaming/star-trek-infinite-review
- Paradox forum, ST: New Horizons features (search result; the fetch returned only a validation page): https://forum.paradoxplaza.com/forum/threads/mod-st-new-horizons-a-stellaris-total-conversion.954925/post-21530699
- PC Gamer, ST: New Horizons (search result): https://pcgamer.com/star-trek-new-horizons-mod-boldly-revamps-stellaris
- Star Trek Online player species, Allegiance (search results): https://sto.fandom.com/wiki/Player_species , https://forum.arcgames.com/startrekonline/discussion/1170409/what-differences-in-story-between-factions
- Star Trek Online Legacy of Romulus dev blog #8 (fetched, body empty): https://www.playstartrekonline.com/en/news/article/1013170
- Bridge Commander, GameSpot and GOG listing (search results): https://www.gamespot.com/reviews/star-trek-bridge-commander-review/1900-2854221
- PC Gamer Bridge Commander retrospective (search result): https://www.pcgamer.com/lets-revisit-the-greatest-star-trek-game-ever
- Starfleet Command: Orion Pirates, StrategyWiki (search result): https://strategywiki.org/wiki/Star_Trek:_Starfleet_Command:_Orion_Pirates
- PC Gamer, Star Trek: Resurgence (search result): https://www.pcgamer.com/star-trek-resurgence-review/
- Wikipedia, Sid Meier's Alpha Centauri: https://en.wikipedia.org/wiki/Sid_Meier%27s_Alpha_Centauri
- TV Tropes Alpha Centauri quotes (search result): https://tvtropes.org/pmwiki/pmwiki.php/Quotes/SidMeiersAlphaCentauri
- CivFanatics Alpha Centauri thread (search result): https://forums.civfanatics.com/threads/why-do-the-franchises-veterans-love-alpha-centauri.526529/page-3
- Endless Space 2, PC Gamer (fetched, truncated) and community threads (search results): https://pcgamer.com/endless-space-2-takes-endless-legends-quests-and-varied-factions-to-the-stars
- Civ VI Diplomacy, fandom (search result): https://civ6.fandom.com/wiki/Diplomacy
- Master of Orion 2016 voice cast, Wargaming (search result): https://wargaming.com/en/news/voice_cast_revealed
- Master of Orion 2016 review (search result): https://imperium.news/review-master-of-orion/
- Stellaris dev diary 5, empires and species (search result): https://www.moddb.com/games/stellaris/features/dev-diary-5-empires-and-species
- Sins of a Solar Empire II, StrategyWiki (search result; no UI or audio detail found): https://strategywiki.org/wiki/Sins_of_a_Solar_Empire_II
- Wikipedia LCARS and the Okudagram piece on alien adaptation (search results): https://en.wikipedia.org/wiki/LCARS , https://treknews.net/2015/07/23/ode-to-okudagram/amp/

What I could not inspect: the game running (no Godot, no builds, by instruction), so everything about animation, transitions and in-game audio is from code and manifests, not seen or heard; the 3D galaxy reference strip in the other worktree (`wt-m0-galaxy3d`) and any other lane's uncommitted work; the original BOTF's own per-race skins, advisors and intro films (not retrievable online in this pass; they need a look in Simon's installed copy); the show-accurate shapes of alien consoles and insignia (from memory, flagged); the C++ core beyond targeted greps (event text templates, star names, features); web pages for ST: New Civilisations, Fleet Command, GalCiv IV, Distant Worlds 2, and several truncated or empty fetches noted above.


## G. Round 2 (M4-TREKIFY, 13 Nov 2026)

Trigger (Simon, 13 Nov 2026, v5 playtest): "it needs another Trekify round to be ran, alot of little icons or chosen shapes in some menus are generic and not trekky." Scope: the whole client except `scenes/battle/` and the battle HUD (M4-COMBAT). Everything below is drawn in code (`_draw` vector shapes on a 24 px grid and `StyleBoxFlat` shapes). Nothing was downloaded and no image asset was added.

Captures (all 1920x1080): `Documents\ChatGPT\m4-trekify-captures\before\before-<look>-<shot>.png` and `...\after\after-<look>-<shot>.png`, five looks (tos, movie, tng, voy, late) by 41 shots before and 46 after. The five shots added in this round (`confirm`, `toast`, `aftermath`, `frontier`, `events-drill`) have no "before" frame: the capture tool did not have them at the start, and they show screens whose icons are covered by the same widgets as the other shots. Side-by-side sheets: `contact-tng\compare-01..16.png`, `contact-voy\compare-01..16.png` (BEFORE | AFTER, three screens per sheet), plus after-only grids `contact-tos`, `contact-movie`, `contact-late` (twelve screens per sheet).

### G1. What was generic, and what replaced it

| Generic item (found) | Trek replacement | Where |
|---|---|---|
| Plain round/dot badges and bullets | Starfleet delta bullets, octagon alert plates, rank-pip tiers | badges, events, new campaign |
| Text glyphs standing for icons (arrow, plus, lightning, tick, cross, search, close marks) | Drawn delta arrows and `delta_right`, energy cell, lamp tick and cross, sensor sweep for search | system, research, empire, intel, palette, toast |
| Round progress bars and ring gauges | Segmented Okudagram bars (`LcarsMeter`) and segmented arc gauges (`ring_gauge`) | every screen with a meter or ring |
| Stock Godot checkbox, option arrow, spin arrows, slider grabber | LCARS lamp checkboxes, rocker switches, delta dropdown arrows, bar-and-notch slider grabber (theme icons rasterised at runtime) | settings, fleets, new campaign, intel, packs |
| Generic rounded-rectangle buttons, cards and tabs | Tab-shaped (elbow-ended) buttons, asymmetric ghost, card and option boxes | all screens, via the theme |
| Layer bar icons on the chart (plain words and dots) | Territory seal, supply spoke, course line, trade slab, sensor sweep, grid, label glyphs; legend entries drawn per meaning | galaxy |
| Chart range rings and selection ring | Segmented rings, corner-bracket reticle, octagon chips | galaxy overlay |
| Minor race marks (one generic dot) | Six temperament marks (`minor_mercantile`, `minor_warlike`, `minor_peaceful`, `minor_scientific`, `minor_spiritual`, `minor_xenophobic`) | new campaign, database, galaxy, diplomacy |
| Structure and choice icons (a box for everything) | `choice_icon.glyph_for(name)` keyword table: farm, factory, lab, power, shipyard, defence and so on | system, events |
| Sort marker on tables | Starfleet delta | empire tables |
| Back button | Back pill with delta | new campaign, packs, saves |
| Left rail and elbow identical in every era | TOS: boxed gold console, lamp row, colour push-button rail ("STARFLEET"). Movie: chamfered grey-blue plate, outlined flat rail ("FLEET OPS"). TNG, Voyager, Late: sweep elbow | rail and elbow |
| Toast and modal frames | Tone icon on the toast; elbow-capped LCARS frame on toast and modal | all |

Stroke weight follows the look (`UiTokens.icon_stroke()`: TOS 2.8, TNG and Voyager 2.0, Movie and Late 1.5), so TOS icons read heavier and Movie icons read thinner.

### G2. Count per screen (found and fixed)

Approximate count, tallied by hand from the before frames: each distinct icon, glyph or shape that was generic on that screen. Shared chrome (rail, elbow, top-bar stats, theme controls) is counted once under Chrome and is visible on every screen.

| Screen (shot) | Found | Fixed | Before frame (tng) |
|---|---|---|---|
| Chrome: rail, elbow, top bar, toast, modal frame | 9 | 9 | `before-tng-galaxy.png` |
| Galaxy chart (galaxy, galaxy-whole, galaxy-menu, station) | 18 | 18 | `before-tng-galaxy.png` |
| System (system-installed, system-ships, system-structures) | 9 | 9 | `before-tng-system-ships.png` |
| Empire (colonies, fleets, governors, members, programs, stations, templates, trade) | 11 | 11 | `before-tng-empire-colonies.png` |
| Research | 6 | 6 | `before-tng-research.png` |
| Diplomacy (dossier, history, ledger, news, propose) | 7 | 7 | `before-tng-diplomacy.png` |
| Fleets | 6 | 6 | `before-tng-fleets.png` |
| Intel | 5 | 5 | `before-tng-intel.png` |
| Database (ships, tech, structures, races, minors) | 8 | 8 | `before-tng-db-ships.png` |
| New campaign (new, new-preset) | 8 | 8 | `before-tng-new.png` |
| Packs (pack editor) | 3 | 3 | `before-tng-packs.png` |
| Saves | 3 | 3 | `before-tng-saves.png` |
| Settings | 5 | 5 | `before-tng-settings.png` |
| Events, briefing, end, victory, help, menu, palette | 9 | 9 | `before-tng-events.png`, `before-tng-briefing.png`, `before-tng-victory.png` |
| Title | 2 | 2 | `before-tng-title.png` |
| Total | 109 | 109 | |

The icon set itself has 94 named icons (`UiIcon.NAMES`), covered by `scenes/ui/tests/test_trek_icons.gd`.

### G3. Left generic, and why

- Close, plus and minus glyphs on dialog corners: they are universal controls and the same in every LCARS reference. Kept.
- The wheat shape for food: no canon food icon exists; the Federation replicator or a plain leaf were weaker than the current mark. Kept.
- Torpedo and combat icons on the battle screen and battle HUD: M4-COMBAT owns `scenes/battle/`.
- The "•" separators and the infinity sign in tooltip text: text punctuation, not icons.
- Buttons that are already LCARS pills (END TURN, MENU, RESUME and similar): these were already on-theme.
- The cut-off back button and bottom row on the Title, New campaign, Packs and Saves captures: this is the capture tool's scroll framing, identical in the before frames. Not a layout change.
- Pre-existing, not touched: `test_viewmodels.gd` has 12 failures, identical with and without this round's changes (checked by stashing). It is not in the verify suite list.

### G4. Proof

See the lane report for commit hashes, suite counts and the `verify.ps1 -Target both` output.
