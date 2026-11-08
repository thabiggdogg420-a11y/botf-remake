# UI design system (M1-UI)

Outranked by, in order: Simon's words in `standing\simon\botf-rulings.md` and the quotes in `research/CURRENT-STATE-AUDIT.md` §3 ("redesign the entire games UI relative to the original to make it more modern and better working for today's tech", 29 Sep 2026; "AAA quality ATLEAST", same day); MASTER-PLAN.md §1 items 2 and 3 and §7; M0-LANES.md "Browser target"; ART-BIBLE.md §5 (LCARS UI style) and §12. This document turns those into tokens and components. Code: `godot/scenes/ui/widgets/` (paths below are relative to `remaster/godot/`).

Source tags: `[file:line]` is the repo; `[sys]` is a rule this document sets for the first time.

## 1. Principles

1. **Pictures and numbers lead, paragraphs follow.** Every screen opens with a visual (planet, emblem, galaxy, portrait, gauge) and a row of key figures. Prose is kept for tooltips and the database. A screen that is mostly sentences fails review. [sys, from Simon's "text-heavy and plain" judgement of 8 Nov]
2. **Every number explains itself.** Hovering or focusing any figure shows: what it is, its parts, and what changes it (cause and effect), with the core's forecast where one exists (§8). [ART-BIBLE §5 "Every number has its explanation"]
3. **Two clicks from the galaxy.** Every screen, overlay and manager is reachable in at most two clicks from the galaxy screen, and in one keystroke or the command palette (§9). [sys, Simon's "better working for today's tech"]
4. **One look, five empires, five eras.** Shape and surface come from the era; accent comes from the player's empire (§2). [ART-BIBLE §3, §5; Simon 8 Nov "era chosen to play in" sets the look]
5. **Both renderers.** Only Control nodes, StyleBoxFlat, CanvasItem drawing and `canvas_item` shaders with at most 4 loop iterations; no screen or depth textures in UI shaders. Every UI shader has a CanvasItem fallback. [M0-LANES "Browser target"; ART-BIBLE §12.2]
6. **Data in view models, look in widgets.** Screens draw; view models compute; widgets never read game state. [CLIENT-ARCHITECTURE.md §2]

## 2. Colour tokens

### 2.1 Surfaces and status (all eras) [`widgets/lcars.gd:7-22`]

| Token | Value | Use |
|---|---|---|
| `BG` | #04060c | window background |
| `BG2` | #070b16 | insets, inputs |
| `PANEL` / `PANEL_SOLID` | #0a1022 at 0.9 / 1.0 | panels |
| `PANEL2`, `PANEL3` | #0e1630, #131d3c | raised levels, hover |
| `LINE`, `LINE2` | (0.55,0.67,0.9) at 13 % and 26 % | hairlines |
| `TEXT`, `TEXT2`, `TEXT3` | #e7edf9, #b2bfd8, #7c8aa8 | primary, secondary, muted text |
| `GOOD`, `WARN`, `ALERT`, `INFO`, `SHIELD` | #5fd98f, #f5c451, #ff5c50, #7fb3ff, #6fd3ff | status; always paired with an icon (§6) |

### 2.2 Empire accents [`widgets/lcars.gd:25-40`, ART-BIBLE §3]

| Empire | primary | secondary | tertiary | map owner | colour-blind primary |
|---|---|---|---|---|---|
| Neutral 0 | #8fa6c8 | #c3cfe0 | #5b6b86 | #9aa6b8 | n/a |
| Federation 1 | #6fa8ff | #f2b45a | #a8b8ff | #5b9dff | #56b4e9 |
| Klingon 2 | #e0484a | #c98a4b | #b86b52 | #e2463f | #e0662a |
| Romulan 3 | #3fc98a | #93dcbf | #59a88a | #35c37f | #16b38a |
| Cardassian 4 | #d9a44a | #e9d6a6 | #b88a55 | #d6a13f | #f0e442 |
| Ferengi 5 | #f08a2c | #f5c16c | #c8743d | #f28c28 | #d98cbb |

Rules: accent = the player's primary (rails, frame titles, active control); secondary = highlights and the call-to-action (End turn, Launch); map owner colour only for ownership. The Okabe-Ito set replaces them when "Colour-blind palette" is on.

### 2.3 Era tokens [sys; `widgets/ui_tokens.gd` `ERAS`]

The era sets surface tint, corner radius, line weight, header style and two era colours; the empire accent is applied on top. The era is a setting (`settings.uiEra`), chosen on the New campaign screen and in Settings. Default: `tng`.

| Era | Shows (ART-BIBLE "Combat visual benchmark") | Surface tint | Radius (panel / pill) | Header | Era colours (hi, lo) |
|---|---|---|---|---|---|
| `tos` | TOS-R | #0b0a12, warm black | 4 / 6 | flat colour bar, no elbow | #f2c14e gold, #d6453d red |
| `movie` | TMP to TUC | #070a10, grey-blue | 6 / 8 | thin rule with bracket ends | #9fb4c8 steel, #c06a4a rust |
| `tng` | TNG and DS9 (Dominion War) | #04060c, navy black | 14 / 18 | LCARS elbow cap | #f2b45a amber, #9d8cff lilac |
| `voy` | Voyager | #050a12, cooler | 12 / 16 | LCARS elbow cap, slimmer | #7fb3ff sky, #d9a36a sand |
| `late` | Picard season 3 | #03050a, glass | 6 / 10 | hairline with glow dot | #5fe0ff cyan, #ff8a5c coral |

## 3. Type scale and spacing [`widgets/ui_tokens.gd` `TYPE`, `SPACE`]

Families: Antonio (display, all caps), Inter (body), Share Tech Mono (numbers and tables) [ART-BIBLE §5, `fonts/LICENSE-*.txt`]. Minimum body 14 px at UI scale 1.0; every numeral in a table is mono.

| Token | Family, weight | Size px | Use |
|---|---|---|---|
| `display` | Antonio 700 | 64 | screen hero name (planet, empire) |
| `h1` | Antonio 600 | 40 | screen title |
| `h2` | Antonio 600 | 22 | panel title |
| `h3` | Antonio 500 | 17 | sub-panel, tab |
| `kpi` | Antonio 560 | 30 | key figure in a KPI tile |
| `stat` | Antonio 560 | 22 | figure in a stat row |
| `body` | Inter 420 | 15 | sentences |
| `small` | Inter 420 | 13 | secondary lines |
| `caption` | Antonio 500, caps | 12 | labels above figures |
| `mono` | Share Tech Mono | 14 | table cells, ratios |

Spacing is a 4 px grid: `xs 4, sm 8, md 12, lg 16, xl 24, xxl 32`. Screen margin 16 (in game) and 48 to 64 (title, new campaign). Panel padding 16 by 14. Gap between panels 14. Row height 44 (lists), 36 (dense tables).

## 4. Motion [`widgets/ui_tokens.gd` `MOTION`, `widgets/motion.gd`]

| Token | Duration | Curve | Use |
|---|---|---|---|
| `fast` | 120 ms | ease-out cubic | hover, press |
| `panel` | 180 ms | ease-out cubic | panel enter: fade 0 to 1 and slide 12 px up, 30 ms stagger per panel, at most 6 staggered |
| `screen` | 200 ms | ease-out cubic | screen enter (fade) |
| `count` | 400 ms | ease-out | KPI figure counts up from its previous value |
| `idle` | 40 s per turn | linear | galaxy preview rotation, planet spin |

Rules: nothing loops in a way that competes with the map [ART-BIBLE §5]; "Reduced motion" sets every duration to 0 and stops idle rotation; motion never delays input (controls are live from frame one).

## 5. LCARS components [`widgets/`]

Each component is a builder on `Lcars` or its own class. Every one takes only plain data.

| Component | Builder / class | Spec |
|---|---|---|
| Frame (panel) | `Lcars.frame(title, sub, tone, icon)` | panel stylebox from the era (radius, tint, 1 px LINE border, 6 px top edge in accent at 0.9); header per era style (§2.3): LCARS cap = rounded left pill 28 x 22 in tone colour, title h2 in tone colour, sub caption, trailing rule at 35 %; optional icon at the left of the title |
| KPI tile | `Lcars.kpi(icon, caption, value, delta, tone, explain)` / `KpiTile` | 1 icon (20 px) + caption over `kpi` figure + signed delta in status colour; rich tooltip from `explain` (§8); min width 140 |
| Stat | `Lcars.stat(...)` | caption over `stat` figure; tooltip required |
| Ring gauge | `RingGauge` | donut 64 to 120 px, 10 px stroke, track LINE2, fill tone, centre figure mono; segments mode for shares (workforce, research split) |
| Segment bar | `SegmentBar` | stacked horizontal bar of parts with legend dots; for income, workforce, energy |
| Meter | `Lcars.meter` | 8 px rounded bar, fill tone |
| Sparkline | `Sparkline` | 120 x 28 polyline of up to 20 points with an end dot; for history the view models keep |
| Pill button | `Lcars.pill(text, tone, on, reason, hint, key)` | radius per era; disabled carries "Unavailable: reason" as its tooltip; key hint shown in the tooltip |
| Tab bar | `Lcars.tab_bar` | ghost toggles; active = accent fill |
| Row button | `Lcars.row_button` + `row_inner` | 44 to 64 px list row; active = accent 20 % fill with accent border |
| Data table | `DataTable` | sortable header, mono numerals, row hover, activates on click |
| Badge | `Lcars.badge(text, tone)` | 18 px pill, 18 % tone fill, 70 % tone border |
| Planet orb | `PlanetOrb` | lit sphere: L7 texture when present (§7), else procedural `planet_orb.gdshader` by class; terminator from the battle sun direction (ART-BIBLE §4), atmosphere rim per class (ART-BIBLE §8 table); CanvasItem fallback |
| Portrait slot | `PortraitSlot` | race card or hail portrait (ART-BIBLE §7): L7 image when present; else the empire emblem large on a gradient of the empire panel colour with scan lines and a frame |
| Galaxy preview | `GalaxyPreview` | spiral, ring, irregular or elliptical star field from the seed, size and minor-density options, with homeworld markers in empire colours; slow idle spin |
| Icon | `UiIcon` | the icon set (§6) drawn as vectors |
| Rich tooltip | `RichTip`, `RichTip.Explained` | panel: title (h3), figure, part lines "label ... value" with tone, a cause and effect line, a forecast line |
| Command palette | `CommandPalette` | Ctrl+K modal: search field and a filtered list of every screen, overlay, colony, fleet and command; Up, Down, Enter |
| Hail panel | diplomacy screen `HailPanel` | 16:9 viewscreen frame: portrait slot left, leader name, relation badge, trust ring, three last lines of correspondence, Hail and Propose buttons |

## 6. Iconography [`widgets/ui_icon.gd`]

One set, 24 px grid, 2 px stroke, squared ends [ART-BIBLE §5]. Drawn in code from line segments and arcs so it scales with UI scale and needs no texture in either renderer (the SVG set named in the manifest, `ui.icon_set`, can replace the drawing later with the same names).

Names: `credits, research, industry, food, population, intel, morale, energy, growth, ship, fleet, shield, attack, speed, range, hull, build, colony, planet, star, galaxy, system, empire, fleets, diplomacy, database, treaty, war, peace, trade, alert, warn, good, info, clock, turn, search, settings, save, menu, events, victory, chronicle, combat, close, plus, minus, up, down, arrow_right, lock, eye`.

Every status colour is paired with an icon (`good` check, `warn` triangle, `alert` octagon, `info` circle-i) [ART-BIBLE §5 accessibility].

## 7. Art slots (planets and portraits)

The UI reads images only through `AppLoader.art(key)` [`scripts/app_loader.gd`], which looks for the manifest key [`assets/asset-manifest.json`] under `res://art/<category>/<key>.<ext>` with ext in `webp, png, jpg`, inside the base pack or a mounted asset pack:
- planets: `res://art/planet/planet.m.webp` ... `planet.gasgiant`, `planet.asteroids` (equirectangular 2:1, ART-BIBLE §8);
- portraits: `res://art/portrait/portrait.fed.webp`, `portrait.minor.bajorans.webp` (square, ART-BIBLE §7).

When the file is there the widget uses it; when it is not, the procedural planet and the emblem portrait stand in. No disk path is read (M0-LANES "No disk paths at runtime"). Lane L7 delivers to these paths or tells the UI lane its own; the key names do not change.

## 8. Numbers: value plus explanation

Standard for every figure on screen [sys, ART-BIBLE §5]:
1. **Value** in the type token for its role, mono in tables, with a unit glyph or icon (¢ credits, /t per turn, T turns).
2. **Delta** where the core gives a change (net income, food balance, growth), signed and coloured: green up, red down, muted at zero.
3. **Explanation on hover and focus**, as a `RichTip` with:
   - title: what the figure is;
   - parts: how it is made, each line "source ... amount" (for credits: colonies, trade, ships, spending);
   - cause and effect: what raises or lowers it and by how much, from the core where it says ("+1 farm worker: +2.5 food", "Food short by 1: population shrinks");
   - forecast: the core's forecast where one exists (`forecast` turn summary, `workforcePreview`, research `forecast` ETA).
4. View models build explanations (`ViewModel.explain`-style dictionaries `{title, value, parts: [[label, value, tone]], effect, forecast}`); widgets only render them; the plain-text fallback (`tooltip_text`) is the same content as lines, so tests and screen readers see it.

## 9. Information density and navigation

- **Density:** a screen at 1920 x 1080 shows its hero visual, 4 to 8 KPI tiles, and at most three primary panels without scrolling; long lists scroll inside their panel, never the page. No line of prose wider than 80 characters. No panel shows more than 12 rows before scrolling. Empty states say what to do next in one line, with the button to do it.
- **Two-click rule:** galaxy to any screen: 1 click (rail) or F1 to F8; galaxy to any overlay (events, victory, chronicle, saves, settings, help): 1 click (rail extras, top-bar buttons); galaxy to any colony's management: 2 (rail System, then the colony strip); galaxy to any empire manager tab (trade, governors, templates, stations, members): 2 (rail Empire, then the tab).
- **Hotkeys:** F1 to F8 screens; F9 saves; F10 settings; Enter end turn; Esc back or menu; Ctrl+K command palette; Ctrl+S quicksave; ? help; Tab and Shift+Tab next unit (galaxy); E events; V victory; C chronicle (not in battle, where C is the camera); Q and E, or `[` and `]`, previous and next colony on the System screen (there E cycles colonies instead of opening events); Alt+Left back. Code: `scenes/ui/app.gd` `_unhandled_key_input`, `screens/system_screen.gd`.
- **Command palette:** Ctrl+K anywhere in game; type to filter screens, overlays, colonies (opens System on it), fleets (selects it on the galaxy), and commands (end turn, preview turn, quicksave, settings, help).

## 10. Screen specs (what each leads with)

| Screen | Hero visual | KPI row | Panels |
|---|---|---|---|
| Title | live 3D fleet (TitleFleet) | latest save card | menu stack with icons, drills, era and version footer |
| New campaign | galaxy preview | powers, systems, sectors, minors | empire cards with emblem and traits; galaxy (shape, size, minor density, powers, seed); rules (difficulty, era, ruleset, preset, events, AI aggression, UI era); launch summary |
| Galaxy HUD (host) | the map (lane L8) | top bar KPI tiles with icons | rail, command palette, toasts; the map and its panels are L8's |
| System | the colony's planets as lit orbs, largest first | population, growth, morale, food, industry, research, energy, credits | workforce with ring gauge and sliders; construction; build queue; governor; energy; victory project |
| Empire | empire emblem banner | treasury, population, industry, research, food, intelligence, ships, contacts | colony cards (planet orb, pop ring, output bars); income segment bar; fleet strength; contacts strip; member worlds; manager tabs |
| Research | six field rings | output per turn, reserve, field levels | fields with progress, ETA and next tech; allocation; focus |
| Fleets | ship silhouette of the selected unit | ships, task forces, total attack, upkeep | unit list; unit card; orders |
| Diplomacy | hail panel (portrait slot) | contacts, at war, treaties, reputation | powers list with emblems and trust; dossier; negotiate; ledger |
| Intel | budget ring | intel per turn, security, missions | targets; missions; reports |
| Database | entry illustration (silhouette, planet or emblem) | entries, filters | search and facets; table; entry detail |
| Combat HUD | the 3D battle (lane L3/combat) | round, ships each side, odds | ship card, target, maneuvers, log |
| Saves | slot cards with empire emblem and turn | slots | load, delete |
| Settings | n/a | n/a | display, audio, interface (scale, colour-blind, reduced motion, UI era), gameplay |
| Overlays | per kind: chronicle categories, event art, victory rings, end-of-game summary | per kind | as today |

## 11. Acceptance (judged by frame, ART-BIBLE §11)

- Each screen captured at 1920 x 1080 on Forward+ and on Compatibility (`screenshot.gd`, `run_ui_compat.ps1`) and looked at, frame by frame, against §1 and §10.
- A screen fails if: it leads with prose; a figure has no tooltip; a status shows by colour alone; any panel scrolls the page; anything is more than two clicks from the galaxy.
- Tests: the UI suite and the view-model suite (both renderers), `pc_smoke` windowed, `verify.ps1 -Target windows`.
