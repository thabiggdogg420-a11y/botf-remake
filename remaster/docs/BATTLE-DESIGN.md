# Battle design direction

Outranked by Simon's recorded words in `standing\simon\` and by his words below.

Simon, 8 Nov 2026, verbatim:

> "scale is off, torpedos are way wrong, the ships look huge compared to the nearby planet, the battle should be a little more spread out. it looks like your trying to replica oringinal BOTF combat, but that was arguably the original games weakest points. it needs to be a modernized, updated, smartly designed 3d turn based space battle."

Earlier the same day: "the 3d battling is a bit shit, the ships bob up and down like boats, and theres no weapon textures to fire?"

## Direction

The battle is a modern 3D turn-based space battle. BOTF supplies the rules core and the fleets, not the presentation. Each round has a plan phase, where the player gives orders with clear 3D feedback, and a resolve phase that plays back as a short directed sequence. Scale follows real proportions: hulls keep their true relative lengths, planets and stations dwarf ships, and fleets fight at a range you can read.

## Proposals

Each proposal is marked P (presentation, Godot only, can be built now) or R (rules, needs C++ core changes; Simon decides).

| # | Proposal | Kind | R3.5 status |
|---|---|---|---|
| 1 | **Steady flight.** No idle bobbing, roll or nodding. Ships bank only into turns and hold altitude. | P | Built (station_keeping, capital climb 0) |
| 2 | **True-scale world.** Planets sit far back as a backdrop and never beside a hull. Formations are drawn 1.7x further apart than the grid (`BattleScene.SPREAD`), so ships are several lengths apart. | P | Built |
| 3 | **Readable fire.** Beams, bolts and torpedoes sized to the hulls, with a minimum on-screen width so a wide shot still shows them. Torpedoes are glowing heads with short tails, about 0.8 s in flight on a shallow curve, with an impact flash. | P | Built |
| 4 | **Uncluttered markers.** In a crowd, only the selected, targeted and hovered ships get brackets; the others get a pip. | P | Built |
| 5 | **Plan-phase overlays in 3D.** Movement path preview, weapon firing arcs as translucent wedges, and range rings on the selected ship, drawn on the plane. | P | Not built (next presentation round) |
| 6 | **Cinematic resolve.** The director cuts between establishing, exchange and kill shots. Cuts must never park the lens inside a small hull's shield shell (the whiteout seen in the close strip). | P | Partly built (glare caps); the camera rule is still open |
| 7 | **Hit and damage feedback.** Floating damage numbers, shield-facing flash colours, venting and fire on damaged hulls that persists across rounds. | P | Not built |
| 8 | **Formations as orders.** Line, wedge and screen presets that set the spacing the scene draws. | P for the look, R for the rule effect | Not built |
| 9 | **Shield facings.** Fore, aft, port and starboard shields that are damaged separately. | R | Simon decides |
| 10 | **Subsystem targeting.** Engines, weapons and shields targeted as subsystems, each with its own effect. | R | Simon decides |
| 11 | **Simultaneous plan then resolve.** Orders from both sides commit, then resolve together. | R (the core already resolves per round; this changes order timing) | Simon decides |
| 12 | **Range bands and arcs as rules.** Firing arcs and range bands that change to-hit and damage. | R | Simon decides |

## R3.5 pass 3 build notes (8 Nov 2026)
- P built: tactical camera is the default. It fits every live ship from a raised three-quarter angle with about 10% margin, re-fits every second and eases. A station only drops out of the fit when it would push the ships under about 2.2% of frame height.
- P built: cutaways are occasional, on round 2, then every fourth round, or on a kill. A candidate shot is accepted only when:
  - shooter and target are whole in frame;
  - neither fills more than 40% of frame height;
  - one of them fills at least 6%;
  - no other hull over 10% of frame height is cut by the frame edge.

  Rejected shots fall back to tactical. The director prefers shooters on the outside of their formation.
- P built: beams on one target share one glare budget. Bloom: intensity 0.4, HDR threshold 1.8, scale 1.2; tonemap white 8. Brightest strip frames measured 0.07-0.78% near-white pixels.
