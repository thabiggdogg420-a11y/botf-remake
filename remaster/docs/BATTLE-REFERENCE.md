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
