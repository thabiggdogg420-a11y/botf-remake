# Galaxy generator: shapes, minor-race density, difficulty

Lane M2-GEN (MASTER-PLAN §5 lanes 2 and 9). Outranked by `standing\simon\botf-rulings.md`. Private, not for distribution.
Simon, 10 Nov 2026: Medium is the default at about 50 systems; sizes are not bound by the original limits. Those stay.

## 1. Vanilla reference (sources in `research/BOTF-FEATURE-REFERENCE.md` 1.1)

- **Shapes:** Irregular, Spiral, Ring [AFC-OV, AFC-MAP]. Vanilla spiral arc values 1.5 / 1.0 / 0.5; a ring is a spiral whose inner and middle arc values are close. Elliptical is ours.
- **Minor-race density:** None / Few / Some / Many. Small 0/4/4/6, Medium 0/7/12/17, Large 0/10/16/24, hard maximum 29 [AFC-GAL]. The source gives Small Few and Some both as 4. That looks like a typo, and nothing here confirms it, so Small Some is 5 in our table so the four steps stay distinct. Flag for Simon.
- **Difficulty:** Simple up to Impossible, where Impossible cheats to compete [AFC-OV]. **[GAP]** No source states exactly what each vanilla level changes, so the numbers in section 2 are our design, not vanilla data.
- **Not in vanilla:** a setup preview, elliptical shape, sizes above 52x39.

## 2. Target

**Shapes** (integer maths only, so native, MSVC and wasm place every star identically; stars occupy distinct cells of the W x H grid, Sol is the first star placed):

| Shape | Distribution |
|---|---|
| Spiral | Three arms, twist 300 degrees across the radius, density rising to the core; 18% of stars in a bright core |
| Ring | Annulus between 56% and 100% of the radius, empty centre |
| Irregular | `3 + systems/12` seeded clumps with a triangular spread, 12% lone stars between them |
| Elliptical | Four-uniform (near-Gaussian) blob, dense in the middle, tapering out |
| Scatter | The old uniform shuffle. It is the default when a setup names no shape, so every existing seed, test and fixture is unchanged |

The 2D grid stays (`System` has no z). WS02's 3D coordinate is a separate change.

**Minor-race density.** `minorRaceTarget(density, systems)`: size picked by system count (up to 36 Small, 64 Medium, 100 Large, above that Huge). Small 0/4/5/6, Medium 0/7/12/17, Large 0/10/16/24, Huge (ours) 0/14/22/29; capped at 29 and at `systems/2 - 3`. None also removes the starter Bynars and returns their system to an ordinary star. Without a density the old `systems/5` count applies.

**Difficulty.** Applied only to rivals; the player's economy is never changed. `difficultyProfile()`:

| Level | Rival income / industry / research | Rival starting credits | War confidence | Spy-op threshold |
|---|---|---|---|---|
| Simple | 75% | 75% | 70% | +60 vs the player |
| Easy | 90% | 90% | 85% | +30 |
| Normal | 100% | 100% | 100% | 0 |
| Hard | 115% | 115% | 115% | -30 |
| Impossible | 135% | 140% | 130% | -60 |

War confidence scales the military power a rival sees in itself in its war assessment (`planAiWarDeclarations`). The spy-op threshold feeds `difficultyModifier` in the intelligence engine (it was 0): rival ops against the player are readier on Hard, and the player's ops against rivals move the opposite way. Impossible is the one level that cheats (more output than the player can match); the lower levels are handicaps.

**Expert (non-cheating, MASTER-PLAN lane 9): not built here.** It needs no economy change but better AI behaviour (best-ship builds, no piecemeal attacks), which belongs to the AI lane. Labelling it now would be a name with no effect. Note for the AI lane: add `Difficulty::Expert` between Hard and Impossible with all percentages at 100 once those behaviours land.

**Save (schema 15).** `GALAXY_SETUP <shape> <density> <difficulty>` is written only when the setup is not the default, so a campaign without it keeps its schema-14 text after the header. Schema-14 saves load with the default setup. The setup is in the fingerprint.

**Client.** The New Campaign screen's preview calls the core's `galaxyLayout` (bridge method `galaxyPreview`), so the preview shows the same star positions the campaign will have. The "generator pending" badges are removed for shape, minor races and difficulty. The other options on the screen (powers, era, ruleset, gameplay, events, aggressive rivals, seed) were already read by the core; interface look is client-only.

## 3. Acceptance tests

1. Each shape: system count exact, all cells distinct and inside the grid, same seed gives the same layout, a different seed differs, and a statistic that tells the shapes apart (ring: no star inside 40% of the radius; spiral: more stars in the inner half than the outer; elliptical: centroid near the middle; irregular: clumped, so the mean nearest-neighbour distance is below scatter's).
2. Density: None 0, Few/Some/Many equal the table for Small, Medium, Large and Huge, minors in a generated campaign match, never above 29.
3. Difficulty: after 30 turns of the same seed, a rival's research and credits are ordered Simple < Normal < Impossible; the player's economy is identical at every level; Normal matches the no-setup game exactly.
4. Determinism: same seed and options give the same fingerprint; recorded wasm fingerprints for each shape match the native core (`godot/fixtures/determinism-galaxy.json`).
5. Save: round trip keeps the setup and fingerprint; a schema-14 text loads; a malformed GALAXY_SETUP is refused.
6. Client: preview star count equals the generated campaign's; no "generator pending" badge remains.
