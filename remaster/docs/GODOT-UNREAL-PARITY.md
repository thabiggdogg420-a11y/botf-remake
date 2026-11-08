# Godot vs Unreal parity (R3.5, 8 Nov 2026)

Outranked by `docs/BATTLE-DESIGN.md` (Simon, 8 Nov 2026), where the battle direction departs from the Unreal client.
The reference is the frozen Unreal client (`remaster/game/Source`, mainly BotfCombatScene.cpp and BotfSettingsPanel.cpp). Status: **have**, **weaker** or **missing**.

| Area | Unreal had | Godot status after R3.5 | Owner |
|---|---|---|---|
| Battle camera modes | overview, chase, ship, target, top-down, cinematic, free | have (BattleCamera, 67 rig checks) | - |
| Ship motion | poses, banking, station-keeping bob | have; bob removed on Simon's word (steady heading and altitude) | R3.5 |
| Beams | cylinder 2.5-unit radius, faction colours | have: same colours, core plus halo, sized against the hulls, minimum on-screen width | R3.5 |
| Pulse bolts | 70-unit segment | have (sized to Unreal) | R3.5 |
| Torpedoes | orb with halo, arcing flight | have: brighter head, short trail, slower flight, shallower curve | R3.5 |
| Shield impact | flattened bubble on the shell | have: hex-lattice shell flash, capped so a close camera does not white out | R3.5 |
| Hull impact | orb, scorch, light flash, sparks | have: plus decals and smoke | - |
| Explosion | core, two noise fireballs (T_FxNoise), shockwave ring, debris | weaker: particle fireball and ring, no noise-textured fireball mesh | R4 |
| Debris | pooled chunks | weaker: particle streaks, no mesh chunks | R4 |
| Warp out | stretched orb | have (stretch plus flare) | - |
| Warp in | none on arrival | missing in both | R4 |
| Cloak shimmer | bubble shimmer | have (cloak overlay shader) | - |
| Niagara VFX per weapon | optional per-key systems | missing (no equivalent; procedural only) | R4 |
| Skybox | T_SpaceNebula equirect | have (procedural sky shader, volumetric nebula) | - |
| Planet / moon | gas giant 8200 units, moon 5600 | weaker: gas giant only, now a far backdrop at true scale; no moon | R4 |
| Stations | disc footprint, starbase model | have (2-cell footprint, hero or procedural) | R4 for art |
| Lighting | sun, rim, fill lights | have (sun, nebula rim, ambient) | - |
| Settings: display | window mode, resolution, quality preset | have: resolution, window mode, vsync, FPS cap, render scale, AA, shadows (persist and apply at start) | R3.5 |
| Settings: confirm-or-revert on display change | yes | missing | R6 |
| Settings: quality preset | single preset combo | missing (individual options instead) | R6 |
| Title | live fleet scene, menu | have, plus Quit with confirm | R3.5 |
| Quit to title | yes | have, now with confirm | R3.5 |
| UI screens | all strategic screens | have (UI 158 checks) | - |
| Tutorial / onboarding | partial | missing | R5 |

## Audio (items 6-7)

- **Red alert:** remapped `sfx.alert.red` from sfx_0075 to **sfx_0132**. A spectrogram of all 163 effects showed that sfx_0075 is a 1.2 s run of stepped UI beeps (the "beep boop"). sfx_0132 is the only rising harmonic sweep: a 1.0 s whoop with a 0.6 s tail, the klaxon shape. None of the 163 effects is a two-tone loop; the klaxon repeats by replaying the file. **Still a guess: Simon should judge it by ear.**
- **Torpedo:** `weapon.generic.torpedo` changed from sfx_0146/0162 (short beepy blips) to **sfx_0010**, a 1.3 s descending harmonic sweep. Still a guess.
- **Beam:** sfx_0132 was removed from `weapon.generic.beam`, which keeps sfx_0131 and sfx_0135. Still a guess.
- These edits are in the local, uncommitted `game/LocalAssets/audio/cues.json`; the original is archived at `~\_archive\2026-11\botf-audio-cues-pre-r35\`. `tools/build_audio_cues.py` lines 90 and 100 still hold the old numbers, and that file is outside R3.5 ownership.
- **Trace:** `BOTF_CUE_TRACE=1` with `scenes/battle/tools/cue_trace.gd` logs one line per cue. Every beam, torpedo, shield hit, hull hit, explosion, cloak and decloak fires a cue, and disengage now does too. The scheduler's per-key cooldown drops about 70% of them in salvos, because 8 torpedoes launched in the same instant play once. That behaviour lives in `scripts/audio`, outside this round.
- **Guesses for Simon's ear:** every `sfx.*` and `weapon.*` pick (build_audio_cues.py chose them by length and spectrum), plus music tracks 11-15.

## R3.5 pass 3 (8 Nov 2026)
- Camera: tactical fit by default; validated cutaways; outside-shooter preference (Godot goes beyond Unreal's free director here).
- Glare and bloom capped (Godot only).
- Event-to-cue trace after the salvo change:
  - every event resolves to a file;
  - 24 of 42 torpedo launches play, the rest are dropped by the 4-voice salvo cap;
  - 29 of 41 shield hits play;
  - 19 of 20 hull hits play.

## R3.5 pass 4 (8 Nov 2026)
- The round-start refresh is now incremental (combat_screen.refresh_round updates the cards in place): about 30 ms, down from 220-370 ms. In 24v24 the worst frame is about 117 ms in rounds 2-4 and 370 ms in round 1.
- Cloak and decloak play for every ship: 4 of 4 each in the trace.
- The drill takes `foeStation`, which gives the rival the starbase. WebBridge.cpp is changed and the debug DLL is rebuilt in the worktree.
- Torpedoes have empire colours (Federation red-orange, Klingon green-gold), longer trails on a 60 px floor, and brighter heads. Beam, flare and impact floors are raised.
- Damaged ships show fires and plasma venting. Explosions last 3.5 ticks, with more debris and smoke.
- The tactical camera fits the screen box to about 86% of the frame and drifts slowly. Formation spacing uses full spheres, so hulls no longer overlap.
