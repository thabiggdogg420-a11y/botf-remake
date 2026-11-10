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

## M1 battleplay build notes (9 Nov 2026)
Simon, 9 Nov 2026, verbatim: "the torpedoes still never got fixed, and all ships seem to fire their weapons at the same time and only seem to move between turns, I want the combat to play out naturally similar to how ships fight in the shows, turn based still, but with a better animation of the actual battles".

- P built: **cinematic round playback** (`BattleTimeline.Config.cinematic`, on in the battle view). A round plays as one 8 s timeline. Ships fly their whole path through the round (a cruise ease, not ticks 0-4) and bank into turns. The core fires mount k of every ship on tick 5+k, so the playback gives each fire tick a window sized by its shots (an empty tick is a short beat). Each shot launches inside its tick's window at the moment the flown paths put the target best in the mount's arc, pulled toward a deterministic jitter.
- **Outcomes stay the core's.** Impacts on one ship land in the core's resolution order: rules 4 events carry the bridge's read-only `resolveOrder`, and other events fall back to (impact tick, phase, event index). A destroyed ship fires everything it fired that round before the killing blow lands, and explodes on that blow. The hull and shield readouts fall as each impact lands, and the round ends on the core's figures.
- P built: **torpedoes** are white-hot billboard heads with a coloured glow and four-point flare (`shaders/torpedo.gdshader`, at least 44 px on screen), a short trail and a launch flare. They curve from the tube to the target and fly at least 1.2 s. A spread leaves its tubes 0.16 s apart. Colours: photon red-orange, quantum blue-white, Klingon lime, Romulan green. Phasers and polarons hold and sweep across the target; Klingon, Romulan and Dominion disruptors fire as bolts.
- P built: **impacts.** The shield bubble flares at the facing hit; hull hits throw sparks, debris and a venting plume; a damaged subsystem sparks at its spot.
- P built: **1x, 2x and SKIP** on the camera bar.
- P built: **web and Compatibility parity.** Effect nodes (beams, flares, torpedo heads, shock rings) carry a material each with plain uniforms. With instance uniforms, torpedo heads and impact flares spawned mid-battle drew nothing in the web build while the plain-uniform trails beside them drew; with plain uniforms they draw. Compatibility also gives instance uniforms a single 4,096-slot pool that the ships' nav lights overrun at 24v24 (2 lights miss a slot). The additive effect shaders and the shield also lift their output with an sRGB-like curve under Compatibility, where it was about 16 times dimmer on black than Forward+ and the shield bubble vanished.

## M1 weaponfx build notes (10 Nov 2026)
Simon, 9 Nov 2026, verbatim: "torpedoes still havent been done properly, phaser either, despite having proper benchmarks to work towards". The benchmark is the DS9 Dominion War battles (ruling, 8 Nov). The spec is `docs/WEAPON-FX.md`.

- P built: **phasers** draw as a camera-facing ribbon (`shaders/weapon_beam.gdshader`): 1.3 wu wide with an 18 px floor applied per vertex, a warm white core, an amber body and a red-orange glow into the bloom, crackle and flicker. A 0.3 s charge runs along a strip on the hull to the emitter before the beam lights. On a shield the contact point splashes and ripples the bubble every 0.25 s; on the hull it burns with a spark stream.
- P built: **torpedoes** are a pulsing spiked star (8 thin twinkling spikes) with a screen-horizontal lens streak, a short motion smear and a launch flash. A hit makes a big flash and an expanding shockwave ring. Federation photon red-orange, Klingon deep red (was lime), quantum blue-white with a ring halo, Romulan plasma a churning green ball.
- P built: **bolts** (Klingon and Romulan disruptors) are capsules with a bright head, at least 13 by 60 px. Cardassian beams twist with two strands; polarons are violet with a stutter.
- **Outcomes, timing and the core are unchanged.** Only the look changed: the timeline's jobs and times are the same; the phaser charge plays in the 0.3 s before a beam's start.
- Glare: beams converging on one target dim their last stretch to 1/sqrt(n). Close-ups of ten-beam focus fire and of a kill explosion still go over 1% pure white (measured 1.5 to 4%); wide and salvo shots stay under 0.6%.
- Proof tool: `scenes/battle/tools/weaponfx_capture.gd` (close-ups, frame strips and `--fps`).
