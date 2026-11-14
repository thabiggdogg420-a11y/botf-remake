# Battle reference: measured from the shows

Outranked by Simon's recorded words in `AI_stuff\standing\simon\botf-rulings.md`, above all:

- 8 Nov 2026: "as close to photorealistic as possible, aiming to look exactly like the shows battle scenes from DS9 dominion war era".
- 10 Nov 2026: "Torpedoes have way too long tails, disruptor bursts seem to not exist, everyone seems to fire their weapons at the same time, different races don't have their proper torpedoes/disruptor textures visually ... Combat engine is still pretty basic. Go and watch some battle scenes from DS9, Voyager, TNG movies etc, to Sus it out".

## How this was measured

- Source: Simon's own library (`D:\TV Shows`, `D:\Movies\Star Trek`), video only (`ffmpeg -an`), nothing downloaded.
- Frames live outside every repo in `Documents\ChatGPT\battle-reference\<show>\<episode>\` (never committed): `top\top.png` is a contact sheet of the 48 strongest weapon-colour space frames found by a scan at 4 fps; `dense\d_<t>.png` is 3 s at 8 fps (0.125 s a frame) from second `<t>`, with `full_<t>_NN.png` the full-size frames.
- Sizes are read against a ship in the same frame (640-wide tiles). Times are frame counts x 0.125 s, so they are good to about +-0.1 s.
- Watched and measured: DS9 Sacrifice of Angels (SoA), The Way of the Warrior (WotW), Tears of the Prophets (Tears), What You Leave Behind (WYLB); Star Trek: First Contact (FC, film era). In the library but not reached this pass: DS9 Call to Arms, The Die Is Cast, A Time to Stand; Voyager Year of Hell, Scorpion, Equinox, Endgame; TNG Yesterday's Enterprise, The Best of Both Worlds; Nemesis. Romulan plasma torpedoes, Borg cutting beams, Breen weapons and Voyager-era looks are therefore thinly sourced (see the gaps).

## Weapons, per race (DS9 Dominion War era unless noted)

| Weapon | Measured look | Size | Timing | Frames |
|---|---|---|---|---|
| Fed phaser (Galaxy, Excelsior, Miranda) | Pale gold beam, near-white core, soft gold halo. Uniform along its length; no visible crackle at fleet range. | 1/15 to 1/25 of the target hull's length wide; 1-3 px at 640 when the shooter is across a fleet shot. | Holds 0.6-1.5 s (Excelsior 1.1 s+, SoA 1160 1.75-2.875). Lights instantly end to end (no visible travel). | SoA d_1160, d_782; WYLB d_2015 0.125-0.375 |
| DS9 station phaser | Amber-orange beam, white core, red-orange outer glow. | ~1/25 of a Vor'cha's length. | 0.5 s. | WotW d_4916 1.125-1.625 |
| Fed phaser, film era (Enterprise-E) | Red-orange (#FF4020) with a white core, thicker glow. | ~1/12 of the target's saucer. | 1.1 s, then a second 0.6 s burst. | FC d_682, d_692 |
| Fed photon torpedo | An orange 4-point star with a near-white core and a soft round halo. **No tail and no horizontal streak.** Spikes long and thin, roughly on the axes, slightly tilted, steady (they glint, they do not spin). | Star spans ~0.3-0.5 of a Vor'cha's width with its spikes; core ~1/10 of the ship. | Salvos of 4-5 in flight at once, launched ~0.1-0.15 s apart; cross a third of the frame in ~0.75 s. | WotW d_4891 2.125-2.875 |
| Fed photon, film era | Red-orange star with 2 long anamorphic glints, short smear. | | | FC d_692 2.5-2.875 |
| Photon impact (shields) | A small orange 4-point flare at the bubble, with a halo. No shockwave ring. | ~0.4 of the hull's width. | Gone in 1-2 frames (<=0.25 s). | WotW d_4916 0.125, 0.5, 1.625 |
| Defiant pulse phaser | Short gold-orange blobs, soft halo, ~2:1 long; from 4 emitters, two at a time. | Each ~1/12 of the Defiant's length. | 3 volleys 0.125 s apart (8 bolts/s per emitter pair), burst ~0.4 s. | Tears d_2053 1.875-2.125; FC d_581 1.375-1.875 |
| Klingon disruptor beam (Vor'cha, K'Vort) | **Red** beam, pink-white core, red emitter glow on the ship; a red halo on the target. | ~1/30 of the ship's length. | 0.25-0.6 s per beam; 5 beam starts in 1.5 s from 3 ships. | WotW d_4805 0.875-2.625 |
| Klingon disruptor cannon (bolts) | Green bolts: white-green head, short tapering tail, wide soft green halo; a green 4-point muzzle flare at the wing cannons. | Bolt ~1/4 of a Bird-of-Prey's length, 3-4:1 long. Impact halo up to half the frame in a close shot. | Pairs from both wing cannons, repeats ~0.125-0.25 s; cross a frame in ~0.4 s. | SoA d_1249 0.875-2.875; WotW d_4916 2.125-2.75; WotW d_4891 0.125-0.25 |
| Cardassian beam (Galor) | Yellow-gold beam strung with **bright beads** that run toward the target (the spiral seen side-on); orange fireball at the contact point. | Thin: ~0.6 of a phaser. | Held 1.4 s and more. Two beams 0.4 s apart. | Tears d_1712 1.375-2.875; SoA d_343 2.6, d_1186 1.25-2.0 |
| Dominion polaron beam (Jem'Hadar battleship / cruiser) | Pale lavender-white beam with a broad blue-violet halo. | Wider than a phaser (~1.5x). | 0.75 s+; several at once from one ship. | SoA d_1186 2.125-2.875 |
| Jem'Hadar fighter weapon | Short white-gold blobs in pairs, ~2:1 long, soft halo. | Small: ~1/6 of a fighter. | Pairs ~0.125 s apart during strafing runs. | SoA d_343 1.25-2.375, d_1160 1.0-1.375 |
| Romulan (Warbird) | A green ball with a glare (plasma torpedo or disruptor bolt), green bolts. Only one shot measured. | Ball ~1/10 of the Warbird. | | Tears d_1712 0.125-0.5 |
| Borg | Green-tinted hull fire and white-green explosions inside the cube; cutting beam not captured. | | | FC d_581 1.0-1.125, d_692 1.25-1.625 |
| Breen / unidentified blue beam | A blue beam with a broad soft blue glow (attribution not confirmed). | | 0.5 s+ | WYLB d_2015 1.5-1.875, d_2010 2.0-2.25 |
| Hull hit / kill | Orange fireball with a spray of sparks; small ships break up 0.1-0.4 s after the first visible hit; the fireball holds 1 s+. No rings. | Fireball ~1-1.5x the ship. | | SoA d_1160 1.375-2.875; WotW d_4891 0.75-1.25 |

## Choreography, measured

| Measure | Value | Frames |
|---|---|---|
| Shots starting per second in a fleet shot | 2-4 (beams and bursts); never a whole line at once. | SoA d_1160, WotW d_4805 |
| Gap between one ship's shots | 0.4-1.2 s between beams from one Klingon ship; Defiant bursts 0.125 s apart inside a burst. | WotW d_4805, Tears d_2053 |
| Beams on screen at once | 1-4 in a fleet shot, rarely more. | SoA d_1160, d_782; Tears d_1712 |
| Ships move while firing | Always. Escorts (Defiant, fighters, BoPs) cross the frame in 1-2 s on strafing runs and break away; capital ships (Galaxy, Galor, Vor'cha) drift and turn slowly while holding beams. | SoA d_343, d_1186; Tears d_2053; FC d_682 |
| How close they pass | Fighters pass within a ship length of a Galor; the Defiant passes under a Galaxy's saucer. | SoA d_343, WYLB d_2010 |
| How long a ship lives under focused fire | Small ships 0.3-1 s from first hit to fireball; big ships take several hits across shots. | SoA d_1160 |
| Cut length | 1-3 s per shot; the camera stays near a ship (close and medium shots), wide fleet shots are short. | every strip |

## Gaps: ours before this pass vs measured

| Item | Ours before (file:line at the stopped lane's v10) | Measured | Now |
|---|---|---|---|
| Torpedo tail | 1.6 head sizes plus a lens streak 2.6 S wide (`battle_fx.gd` TORPEDO_TAIL_HEADS, TORPEDO_ASPECT) | none | 0.3 S faint smear, no streak (`battle_fx.gd:24`, `:37`; `torpedo.gdshader` streak off except quantum) |
| Torpedo star | 8 spinning spikes | 4 steady spikes | 4, near-fixed (`torpedo.gdshader:57`) |
| Fed photon colour | (1.0, 0.42, 0.12) red-orange | orange, near-white core | (1.0, 0.55, 0.2) (`battle_timeline.gd:495`) |
| Torpedo impact | flash 1.8x radius + shockwave ring on every hit | small 4-point flare, no ring, <=0.25 s | 0.8x radius, exp(-9u), ring hidden (`battle_fx.gd:905-907`) |
| Phaser colour / width | orange #FF9A2E body, 18 px floor | pale gold, few px at range | #FFB447 body, 8 px floor (`battle_fx.gd:30`, `:386`) |
| Klingon disruptor | one green profile for beams and bolts | red beams, green bolts | split (`battle_fx.gd:401-405`) |
| Cardassian | orange-yellow twin-strand spiral, phaser width | thin yellow beam with running beads | beads, 0.6 width (`weapon_beam.gdshader:114`, `battle_fx.gd:395`) |
| Dominion polaron | purple #9A6BFF, phaser width | pale lavender-white, 1.5x | (`battle_fx.gd:390`) |
| Pulse bolts (Defiant, Jem'Hadar) | phaser-coloured capsules 6:1 | short blobs 2:1 | half length (`battle_fx.gd:414`, `:419`) |
| Fire timing | each core fire tick a window; shots in the first 60% of it; whole lines fire on one beat | 2-4 starts a second, overlapping | shots spread over the whole window, windows open 0.9 of a width early so ticks overlap (`battle_timeline.gd:88-89`, `:1153`) |
| Phaser hold | 1.1 s | 0.4-1.5 s | 0.9 s (`battle_timeline.gd:95`) |

## Engine proposals

What the playback and the core would need for a round to play like the footage. P = presentation only (no outcome changes); R = changes rules, goes to Simon.

1. **Per-ship fire cadence (P).** Replace "mount i fires at tick 5+i" as the draw order with a per-ship cadence: each ship's shots spaced 0.4-1.2 s apart through the whole round, seeded per ship, keeping the core's per-target resolution order. Started in this pass (`window_lead`); a full version schedules every ship's shots on its own clock.
2. **Attack runs during the round (P).** Escorts fly a pass (approach, strafe, break away) inside the round instead of easing between plotted cells; capitals turn to bring arrays to bear and drift. Needs a path generator that ends on the core's plotted cell.
3. **Pulse weapons as bursts of many projectiles (P for drawing, R for damage).** Draw a Defiant or Jem'Hadar volley as 3-4 pairs 0.125 s apart (presentation); making each projectile its own roll is a rules change.
4. **Beams that sweep and re-acquire (P).** One beam may cut out and re-fire a second shorter burst (FC d_682) when the core's event holds enough damage; drawn split, damage unchanged.
5. **Break-off and evasive turns after a hit (P).** A ship hit hard turns away within the round.
6. **Variable weapon recharge per class (R).** The footage shows big ships firing more often than the core's one-shot-per-mount-per-round allows; changing fire rate is a rules change.
7. **Torpedo spreads from one launcher (R if counted as more shots).** The show fires 4-5 torpedoes per salvo; the core decides projectile counts.
8. **Camera language (P).** Short cuts (1-3 s) near a firing ship, fleet-wide shots brief; an optional "cinematic camera" mode.

## M1 battlechoreo: what is built (presentation only)

Outcomes stay the core's: hits, damage, order on each target, kills and end poses. Rules calls (coordinator, 10 Nov, recorded in `standing\simon\botf-rulings.md`): pulse volleys, torpedo spreads and recharge stay presentation only.

| Proposal | Built | Where |
|---|---|---|
| 1 Per-ship fire clock | Each ship fires on its own seeded cadence (0.4-1.2 s; escorts 0.3-0.55 s bunched into their pass; capitals' many mounts spread evenly over the whole round as sustained fire; a ship that dies this round fires early). At most 4 beams held at once across the battle (a beam waits for one to end), re-applied after every order pass. | `battle_timeline.gd` `_ship_clocks`, `_beam_limit` |
| 2 Movement inside the round | Escorts fly an attack run timed on their own shots (approach, strafe past the target at about a hull length, break away, come about); cruisers swing to a broadside and slide 1.8-3.2 cells; capitals ease their bow round and creep 1.2-2.2 cells; all change height. Wingmen hold station on the leader's run. Zero at the round's start and from 0.94 of the round on, so every ship ends on the core's cell and facing. | `flown`, `_alive`, `_choreograph` |
| 3 Pulse bursts (draw) | One core shot = 3-4 pairs, 0.125 s apart; on a hit about a quarter of the later bolts go wide. Damage lands once. | `build` Fire/pulse |
| 3b Torpedo spreads (draw) | One core shot = a spread by class: escort 1-2, cruiser 2-3, capital 4-5, 0.12 s apart, fanned; earlier tubes spark on arrival, damage lands with the last. | `spread_tubes`, `battle_fx.gd` TORPEDO |
| 4 Beams cut out and re-fire | 45% of beams: 0.55 s, dark 0.2 s, 0.35 s. Both ends follow the moving ships. | `build` Fire/beam |
| 5 Reactions | A hull hit of a tenth of the hull or more rolls the ship up to 32 degrees and turns it away up to 24 degrees with a small shove. A killed ship keeps its way on, sinks, rolls and yaws for 1.25 s, venting fire and plasma, then breaks up into the fireball. | `kicks`, `_dying`, `battle_fx.gd` `_fireball` |
| 8 Camera | Cinematic mode cuts every 1-3 s: over the shooter's shoulder, chasing a torpedo salvo, a fixed-lens fly-by of an escort; every third cut a brief wide. Any camera-mode key takes manual control. | `battle_scene.gd` `_film_shot`, `battle_camera.gd` `_film_pose` |

Still listed for Simon, not built: none new (6 and 7 settled as presentation by the coordinator).

## Footage added in M1 battlechoreo (movement, camera, Romulan)

Frames in `battle-reference\ds9\die-is-cast\strip\` (never committed). `d_2052.png` is 16 s at 4 fps from 2052 s (tile k = 2052 + k/4 s); `overview_1900.png` is one frame per 5 s from 1900 s.

| Measure | Value | Frames |
|---|---|---|
| Romulan weapon, DS9 era (Warbird vs Jem'Hadar fighters) | A thin blue-white beam with a soft blue halo from the Warbird's dorsal hull; held ~0.5 s per beam, a second beam 0.25-0.5 s later. Thin: about 1/40 of the Warbird's width. (Our Romulan look is green bolts; this footage says the DS9-era Warbird beam reads blue-white.) | DIC d_2052 tiles 33-35, 40-41, 54-60 |
| Kill of a small craft | Hit, white flash, then an orange fireball about 2 fighter lengths, holding 1.5-2 s while debris spreads; the hulk is gone inside 0.25 s. | DIC d_2052 tiles 34-38, 42-46 |
| Pass distance | A Jem'Hadar fighter strafes the Warbird within about one fighter length of its dorsal hull and is destroyed in the pass. | DIC d_2052 tiles 46-51 |
| Shot length in a battle | One shot held 5.25 s (tiles 32-53) over a fighter pass; the next 2 s and more. Space shots in this episode alternate with bridge interiors (overview_1900: 10 of 128 tiles are space). Our 1-3 s cuts are shorter than this episode's; DS9 season 6-7 fleet battles (SoA, measured before) cut faster. | DIC d_2052, overview_1900 |

Not reached this pass (scanned but the colour scan picked interiors): Call to Arms, A Time to Stand, the Voyager and TNG episodes, Nemesis. The keyframe colour scanner (`bc_scan.py`, kept outside the repo) needs a better space-shot test before it is useful.

## M4 combat (13 Nov 2026): what changed, measured against the clips

Simon, 13 Nov 2026, after beta v5: "the combat engine is still a bit shit. the ships just fly around the screen a little aimlessley, move too fast at time, phaser beams dont get shown, torpedoes all get fired at once ... even my basic screensaver stuff looks like a better battle simulation?!" The floor is the LCARS battle screensaver (`lcars-theme\screensaver\v5`); the bar is the DS9 Dominion War battles above. Presentation only: the core, its events and the Classic fingerprint are untouched (no core change in this lane).

| Complaint | What caused it | What fixed it | Where |
|---|---|---|---|
| Aimless | Capitals and cruisers drifted along a random direction by a random amount and swayed; escorts ran to keyframes laid round a target with no turn rate; wingmen blended onto the leader's run and back. | A flight planner. Capitals and cruisers keep the core's path plus a slow drift that follows the round's order (Engage: broadside and slide; Charge, Assault, Ram: bow on, close; Circle, Harry, Strafe, Evade: broadside and slide round; Hold: stay; Retreat or under 30% hull: stern to the enemy, make for the edge). Escorts and strike craft are flown by a steering simulation: approach, a strafing pass inside about a hull length timed on their own shots, a break away, regroup onto the core's track; Circle and Harry orbit the target, Evade jinks, Charge and Ram close straight in, Hold advances with the line. Wingmen fly a formation-follow on the leader's flown run and break to their own cells. | `battle_timeline.gd` `_choreograph`, `_fly_escort` |
| Too fast | The escort run was timed to the round and ignored hull size: 16 cells/s peak (9.7 km/s, 100+ hull lengths a second), wingmen 20 cells/s with 500 cells/s2 jumps (teleports); capitals lurched 2-3 cells in 0.8 s at every round start and end (4.3 cells/s). | Class speed, turn rate and acceleration (below), a drift that swells with zero speed at both ends, the bow slewed at the class turn rate, docking onto the core's cell and facing with the last stretch a controlled slide, and a cap on how far a run may stray from the core's path (only what there is time to fly out and back). | `CLASS_SPEED`, `CLASS_TURN`, `CLASS_ACCEL`, `_aim_env` |
| Phasers not shown | Beams were drawn, but the Klingon disruptor was always drawn as bolts (so the footage's red beams never appeared), at most 4 beams were held at once however big the battle, 45% of beams were cut to 0.55 s then 0.35 s, and the camera sat a few radii behind one shooter so the beams were mostly off screen or behind a hull. | Klingon capitals fire red beams (escorts keep green bolts); beams are held 0.6-1.5 s by race (Federation 0.8-1.4, Klingon and Romulan 0.6-0.9, Cardassian 1.2-1.5, polaron 0.9-1.4); the beam cap grows by one for every six ships; tracking and wide shots that frame shooter and target. | `beam_hold`, `shows_as_bolts`, `_beam_limit`, `battle_camera.gd` `_film_pose` |
| Torpedoes all at once | A salvo was 1-2 tubes from escorts, 2-3 from cruisers and every ship launched on its own clock with nothing between ships, so clusters formed; one stagger for all. | Salvos of 4-5 from capitals and cruisers (2-3 from escorts), tubes 0.10-0.15 s apart seeded per ship, and a battle-wide gap between salvo starts (0.8 of the fire window over the salvo count, 0.05-0.25 s) so fire flows. The photon is an orange 4-point star with long thin spikes and a small halo. | `spread_tubes`, `torpedo_stagger`, `_salvo_limit`, `shaders/torpedo.gdshader` |

Class table (cells a second; one cell is 592 m as drawn). The screensaver's fleet table is the reference (`Battle.cs` MakeShip: capital 125-190 m/s, cruiser 210-290, escort 470-540, fighter 700-820; turn rates in rad/s capital 0.14-0.18, cruiser up to 0.55, escort 0.9-1.1, fighter 1.4-1.8).

| Class | Speed | Turn | Accel |
|---|---|---|---|
| Capital | 0.30 (178 m/s) | 10 deg/s | 0.10 |
| Cruiser | 0.50 (296 m/s) | 24 deg/s | 0.22 |
| Escort | 1.00 (592 m/s) | 60 deg/s | 0.80 |
| Strike craft (120 m and under) | 1.25 (740 m/s) | 90 deg/s | 1.10 |

Tools: `scenes/battle/tools/flight_check.gd` (per-class top speed, turn rate, hand-over to the core's poses, planner intent), `scenes/battle/tools/battle_clip.gd` (plays a sandbox battle round after round, for Godot's Movie Maker).

### M4 combat, second pass (14 Nov 2026)
- **Shields:** a hit shows a brief local flare (shader angular radius about 0.2 rad, about 0.4 of the hull width, bright part gone in under 0.25 s; job length capped at 0.45 s) with a faint partial shell. The whole-ship bubble no longer stays visible (sector glow and fresnel cut to a few percent; beam ripples 0.3 s every 0.3 s).
- **Fire timing:** ships open fire in turn across the round by seeded rank, torpedo ships first, the first at about 0.1 s and the last late; escorts fire from 0.1 s to flight_to minus 0.9 s. A weapon-free gap over 0.8 s is now a bug (the clip tool prints them per round).
- **Camera (battles of 14 ships or fewer):** every cut is the pair-fitted tracking shot. The torpedo-chase and fixed fly-by lenses are not used. A lull with nothing about to fire tracks the closest opposing pair. When the pair is too far apart to fit, the lens stays on the shooter. Sun and gas-giant avoidance run on every cut. A forced vista (tools, pc_smoke) holds over film cuts.
- **Races:** tactical owner ids 6, 7 and 8 map to ship.dom.*, ship.borg.* and ship.breen.* by class name when the encounter ship carries no assetKey (roster classes carry one).
