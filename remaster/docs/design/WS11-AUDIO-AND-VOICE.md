# Workstream 11: Audio and Voice

## 1. Reference Behaviour (BOTF and Mods)

In vanilla *Birth of the Federation*:
- **Audio Archives:** The original game contained four dedicated sound archives [AUDIO-PLAN:7-15]:
  - `english.snd` / `german.snd`: 1,364 voice lines (IMA ADPCM mono 22,050 Hz).
  - `music.snd`: 151 orchestral music cues composed by Ron Jones.
  - `sfx.snd`: 163 tactical and interface sound effects.
- **Voice Coverage:** Original voice acting spanned four primary functional areas [AUDIO-PLAN:43-50]:
  - *Advisors:* Colony founding, turn events, research milestones, enemy incursions (`vo.advisor.<fac>`).
  - *Diplomacy:* Hails, treaty offers, rejections, declarations of war (`vo.diplomacy.<fac>`).
  - *Combat Acknowledgements:* Tactical unit confirmations when setting ship maneuvers, targets, or retreat orders (`vo.combat.ack.<fac>`).
  - *End-game Speeches:* Full narrative speeches praising victory or lamenting defeat for each of the five empires.
- **Shortcomings in Current Build:**
  - Simon, 7 Nov 2026: *"whover is testing stuff, the game opens and runs sound dcts that shouldnt even be in the game, it sounds like the red alert klaxon has been replacd with a generic beep boop beep boop, there seems to be no sounds for weapons, or anything really"* (`CURRENT-STATE-AUDIT:197`).
  - Sound effects were originally mapped by algorithmic spectrum matching rather than curated by ear (`CURRENT-STATE-AUDIT:122`).
  - Order-acknowledgement voice clips remain unhooked to buttons; the cue scheduler's 4-voice salvo cap inadvertently drops ~70% of sound cues during fleet exchanges (`CURRENT-STATE-AUDIT:122`).

## 2. Modern Target

Folding Simon's explicit decision (§7 of `MASTER-PLAN.md` and `VOICE-PLAN.md`):

1. **Simon's Trek Library Audio Mining:**
   - Mine high-fidelity audio assets from Simon's extensive personal media collection on `D:\TV Shows` and `D:\Movies` (encompassing *Star Trek: The Next Generation*, *Deep Space Nine*, *Voyager*, *Enterprise*, and canon films) (`VOICE-PLAN.md:6-25`).
   - Clean dialogue extraction: Isolate speech tracks from 5.1 surround sound center channels; employ AI vocal separation (Demucs) where music or sound effects overlap.
   - Professional Mastering Pipeline: Dialogue normalized to -18 LUFS broadcast standard; silence trimmed; comm-link bandpass filtering applied to hail transmissions (`VOICE-PLAN.md:58-60`).
2. **Audio Curated by Ear:**
   - Every single weapon, engine, UI, and atmospheric sound effect must be selected and verified by ear against canon Trek sound design standards.
   - Replace generic synthesized tones (`botf_tones.gd`) with authentic studio sound effects:
     - *Red Alert:* Authentic faction klaxons (classic TNG oscillating siren for Federation; harsh pulsing horn for Klingons; klaxon alarm for Romulans).
     - *Weapons:* Distinctive beam crackles, heavy phaser hums, crisp disruptor discharges, and deep acoustic thuds for photon/quantum torpedo launches.
3. **Tactical Voice Acknowledgements:**
   - Every strategic and tactical command triggers immediate authentic bridge crew vocal confirmation (`vo.combat.ack.<order>.<fac>`):
     - Tactical Target Lock: *"Target locked on enemy command ship."*
     - Maneuver Execution: *"Pattern Sierra executed, all power to forward shields."*
     - Damage Reports: *"Shields down to forty percent, starboard nacelle venting plasma!"*
     - Cloaking: *"Cloaking device engaged, entering ambush posture."*
4. **Dynamic Adaptive Music Engine:**
   - Multi-tier interactive score that transitions smoothly across game states:
     - *Strategic Exploration:* Contemplative orchestral ambient themes.
     - *Diplomatic Tensions:* Suspenseful strings and militaristic percussion when negotiating with hostile empires.
     - *WEGO Tactical Battle:* High-tempo orchestral battle suites synchronized to round execution.
5. **Scheduler & Priority Queue Overhaul:**
   - Replace the arbitrary 4-voice salvo cap with an intelligent priority mixer:
     - Voice dialogue and weapon hit impacts are never dropped.
     - Background weapon hums dynamically duck when tactical voice lines or critical alarms play (`botf_cue_scheduler.gd`).

## 3. Player-Facing Surfaces (Screens and Scenes)

- Master Audio Systems:
  - `scripts/audio/botf_audio_system.gd`: Main audio bus mixer (Master, Music, SFX, Voice, Ambience).
  - `scripts/audio/botf_cue_scheduler.gd`: Priority queue and ducking coordinator.
  - `scripts/audio/botf_audio_library.gd`: Registry mapping asset keys to WAV/OGG streams.
- Client Interaction Surfaces:
  - `scenes/battle/battle_audio.gd`: 3D spatialized combat audio emitters for beams, torpedoes, and explosions.
  - `scenes/galaxy/orders.gd`: Voice acknowledgement triggers on strategic map clicks.
  - `scenes/ui/screens/diplomacy_screen.gd`: Hail Viewscreen dialogue audio.
  - `scenes/ui/display_settings.gd`: Audio volume sliders with live test tone feedback.

## 4. Core Changes (Naming Files)

- `remaster/game/Source/BotfSim/GameplayInternal.h`:
  - Expose structured audio event tags in `TurnReport` and `CombatReport` (e.g., `AUDIO_EVENT_RESEARCH_COMPLETE`, `AUDIO_EVENT_INVASION_REPULSED`).
- `remaster/game/Source/BotfSim/Diplomacy.h`:
  - Associate specific voice cue keys (`vo.diplomacy.<act>.<speaker>`) with outgoing AI treaty proposals and responses (`Diplomacy.h:450-480`).

## 5. Client Changes

- `remaster/godot/scripts/audio/botf_audio_library.gd`:
  - Populate complete cue tables for 261 standard cue keys and expand expansion voice keys (`VOICE-PLAN.md:28-44`).
- `remaster/godot/scripts/audio/botf_cue_scheduler.gd`:
  - Refactor salvo handling: implement priority queues allowing concurrent playback of up to 16 weapon channels with automated volume ducking, resolving the 70% drop rate bug.
- `remaster/godot/scenes/galaxy/orders.gd`:
  - Wire mouse button release events on strategic fleet orders to play matching `vo.combat.ack.<order>.<fac>` voice cues.
- `remaster/godot/scenes/battle/battle_audio.gd`:
  - Implement 3D audio listener positioning attached to the active tactical camera with Doppler effect on high-speed flybys.

## 6. Content and Art Needs (by Asset Key)

- Voice Dialogue Cues:
  - `vo.advisor.<event>.<fac>`: 10 event categories (turn, research, construction, colony, outpost, attack, fleet, contact, unrest, event) across all 6 factions.
  - `vo.diplomacy.<act>.<speaker>`: Greet, propose, accept, refuse, war, peace, alliance across major and minor civilizations.
  - `vo.combat.ack.<order>.<fac>`: Maneuver and target confirmations for all playable factions.
  - `vo.bark.<type>`: `shields.75`, `shields.50`, `shields.25`, `hull.critical`, `torpedo.incoming`, `kill.confirmed`.
- Weapon SFX:
  - `weapon.phaser.fire`, `weapon.disruptor.fire`, `weapon.spiral_wave.fire`, `weapon.polaron.fire`.
  - `weapon.torpedo.photon_launch`, `weapon.torpedo.quantum_launch`, `weapon.torpedo.plasma_launch`.
  - `sfx.shield.impact`, `sfx.hull.explosion_small`, `sfx.hull.explosion_catastrophic`.
  - `sfx.alarm.red_alert_fed`, `sfx.alarm.red_alert_kli`, `sfx.alarm.red_alert_rom`.
- Music Tracks:
  - `mus.ambient.galaxy_map`, `mus.battle.wego_resolve_loop`, `mus.theme.federation`, `mus.theme.klingon`, `mus.theme.dominion`.

## 7. Acceptance Tests

1. **Authentic Red Alert Klaxon:**
   - Trigger a tactical combat engagement involving the Federation fleet.
   - Verify the authentic Federation Red Alert alarm sounds (rich orchestral claxon), completely replacing the generic synthesized beep-boop tone.
2. **Order Acknowledgement Voice Playback:**
   - In the Galaxy map, select a Klingon battle group and issue an "Attack" order against a target system.
   - Verify an authentic Klingon commander voice bark plays immediately (e.g., *"Today is a good day to die! Setting attack vector."*).
3. **Salvo Cap Throughput:**
   - Simulate a heavy tactical exchange involving 12 starships firing simultaneous weapon volleys.
   - Verify all beam and torpedo sound effects trigger cleanly with dynamic volume ducking, and 0 critical impacts are dropped by the audio scheduler.
4. **Subsystem Voice Barks:**
   - In combat, suffer critical engine damage on a Federation ship.
   - Verify the bridge damage bark plays (*"Warning: Warp core containment breach imminent, impulse engines offline!"*) and audio priority ducks background music by -6 dB.
5. **End-Game Victory Speech:**
   - Trigger a Conquest Victory as the Klingon Empire.
   - Verify the full cinematic victory speech plays at -18 LUFS with pristine audio clarity and no clipping.

## 8. M1 Vertical-Slice Subset

For the M1 vertical slice:
- Complete ear-verified audio suite for Federation and Klingon.
- Authentic phaser, disruptor, and photon torpedo sound effects.
- Authentic Red Alert sirens for both Federation and Klingon.
- Strategic order acknowledgement voice clips active for Federation and Klingon task forces.
- Dynamic transition between galaxy ambient score and combat music.
