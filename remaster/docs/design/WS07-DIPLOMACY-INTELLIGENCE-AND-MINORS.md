# Workstream 07: Diplomacy, Intelligence and Minors

## 1. Reference Behaviour (BOTF and Mods)

In vanilla *Birth of the Federation*:
- **Attitude Ladder:** Diplomatic relations progressed across eight tiers: Enraged, Icy, Uncooperative, Neutral, Receptive, Cordial, Enthusiastic, and Worshipful [AFC-CAL].
- **Treaty Progression:** Defined a hierarchical ladder: Non-Aggression -> Friendship (permitting trade treaties) -> Affiliation (shared yard access and sensor ranges) -> Alliance (military defense pacts) -> Membership (complete integration of a minor civilization) [AFC-CAL, AFC-STR].
- **Diplomatic Levers:** Credit gifts and bribes influenced minor race attitudes; tribute demands allowed extorting weaker powers [AFC-CAL]. Bribery resistance could be boosted via structures such as the Communications Grid (+500% resistance) [AFC-CAL].
- **Minor Civilizations:** Vanilla featured 30 minor races [AFC-MIN]. Each minor civilization provided exactly one buildable "wonder" structure upon full membership (e.g., Vulcan Science Academy, Zakdorn Military Academy, Acamarian Clan Hall, Trill Research Committee) [AFC-MIN]. Minors could construct defensive ships but could not expand or colonize beyond their home sector [LPA].
- **Intelligence System:** Staffed by population in the Intel work area and intelligence buildings [AFC-CAL]. Budget was split between internal security and external espionage/sabotage against rivals [TREKCORE]. Actions included stealing tech, sabotaging structures, poisoning population, and framing another empire to provoke wars [AFC-OV, AFC-CAL]. Cardassians and Romulans held dominant racial advantages in intelligence operations [AFC-OV, AFC-CAL].

Mods and Remakes:
- **UDM3:** Added DS9-era minors (Bajorans, Breen, Son'a, Maquis, Miradorn), rebalanced minor race bribe costs to prevent early-game exploitation, and introduced shapeshifter infiltration mechanics [UDM3].
- **Supremacy & BotE:** Expanded diplomatic proposals with denouncements, trade embargoes, non-aggression pact timers, and independent minor subjugation [Supremacy, BotE].

## 2. Modern Target

1. **Interactive Hail Viewscreen Scenes:**
   - In place of static text panels, clicking "Hail" opens a dynamic **Hail Viewscreen** modal.
   - Features an animated 3D race portrait set against a faction-authentic bridge backdrop (Federation bridge, Klingon Great Hall / bird-of-prey bridge, Romulan command deck, Cardassian Central Command, Ferengi tower, Dominion Great Link).
   - Fully voiced greetings, proposal acceptances, rejections, and war declarations powered by Simon's Trek audio library (`vo.diplomacy.<act>.<speaker>`) (`docs/VOICE-PLAN.md:35-37`).
2. **Minor Race Wonder Structures:**
   - All 30 minor race signature structures from `remaster/data/base/minors.json` and `remaster/data/base/buildings.json` (Vulcan Science Academy, Andorian War College, Bynar Planetary Computer, etc.) become buildable across all empire colonies once Affiliation or Membership is ratified (`MinorRaces.h:37-70`, `docs/DATA-FORMAT.md`).
3. **Attackable Independent Minors:**
   - Independent (neutral/unaligned) minor races are no longer invulnerable to direct military action (`docs/research/CURRENT-STATE-AUDIT.md:91`).
   - Empires can establish naval blockades, execute orbital bombardments, or invade minor worlds. Unprovoked attacks trigger severe diplomatic trust penalties across all honorable major powers (`Diplomacy.h:720-725`).
4. **Extended Minor Roster:**
   - Integrate DS9 and late-era minor races from UDM3: Bajorans (morale bonus), Breen (energy-dampening weapon tech), Son'a (metaphasic biotech), Maquis (raider escorts), and Miradorn (smuggling commerce).
5. **Deep Intelligence & Multi-Tier Operations:**
   - Maintain the deterministic integer-only espionage math in `Intelligence.h:13-25, 160-230`.
   - Dedicated budget sliders: Internal Security vs Rival Espionage vs Rival Sabotage.
   - Department Focus: General, Economy, Science, Military.
   - High-tier covert operations: *Steal Technology*, *Sabotage Shipyard*, *Assassinate Governor*, and *Frame Third-Party Empire* (generating fabricated battle reports in target empire's chronicle).

## 3. Web-Build Consequences

In compliance with `docs/M0-LANES.md` ("Browser target") and `docs/ART-BIBLE.md` §12:
- **Renderer Fallback (Forward+ vs Compatibility/WebGL2):** The Hail Viewscreen modal and animated portraits render under Compatibility (WebGL2). Bridge backdrops are rendered using lightweight 2D canvas planes without depth-texture post-processing. If high-poly 3D character rigs cannot be initialized, the viewmodel falls back seamlessly to 2D vector emblems and lossy WebP portraits.
- **Budgets:** Character portraits for major leaders and 30 minor civilizations are budgeted at 512x512 lossy WebP (2 animation layers, ≤150 KB each); hail backdrops are 1280x720 WebP. Voice dialogue cues use Ogg q2 32kHz mono (≤120 KB per clip, normalized to -18 LUFS). Audio files are grouped into per-empire packs (≤100 MB per empire) fetched lazily upon initial diplomatic contact.
- **No Disk Paths at Runtime:** Minor race definitions, homeworld coordinates, traits, and affinities load from `data/base/minors.json` handed to the core via memory buffer (`minors::Roster`, `MinorRaces.h:44`). Voice cues stream through `res://` packs. Zero runtime disk reads or `$BOTF_*` lookups.
- **Saves:** Treaties, historical relations memory, accumulated grievances, and intelligence budget allocations serialize to `user://`, persisting to IndexedDB on web builds.

## 4. Player-Facing Surfaces (Screens and Scenes)

- `remaster/godot/scenes/ui/screens/diplomacy_screen.gd`:
  - Major and minor empire dossiers, relationship matrix, treaty ledger, and tribute demand builder (`diplomacy_vm.gd`).
  - "Hail" button opening the 3D Hail Viewscreen modal with real-time dialogue options.
- `remaster/godot/scenes/ui/screens/intel_screen.gd`:
  - Budget allocation sliders (Security, Espionage, Sabotage) with Apply/Revert controls (`intel_vm.gd`).
  - Covert Operations Planner: select target empire, select department, assign agents.
  - Intelligence Dossiers and Agent Chronicle reports.
- `remaster/godot/scenes/ui/screens/empire_screen.gd`:
  - Member Minor Civilizations tab with active racial bonuses, wonder structure status, and secession risk meters.

## 5. Core Changes (Naming Files)

- `remaster/game/Source/BotfSim/Diplomacy.h`:
  - Implement `getDiplomacyAudioCue(empireId, act, tone)` returning exact asset keys for voice playback (`Diplomacy.h:441-480`).
  - Add Dominion diplomatic personality profile and acceptance matrices (`Diplomacy.h:416-440`).
  - Wire severe trust penalties across all known majors when unprovoked war is declared on an independent minor (`Diplomacy.h:720-725`).
- `remaster/game/Source/BotfSim/MinorRaces.h`:
  - Remove attack restrictions on independent minors (`canAttackMinor()` returns true when unallied, `MinorRaces.h:36-100`).
  - Add Wonder Structure unlock query `getUnlockedMinorStructure(minorId)` reading `remaster/data/base/minors.json`.
- `remaster/game/Source/BotfSim/Intelligence.h`:
  - Enhance "Frame Empire" operation to inject a synthetic war incident into `TurnReport::chronicle` of the framed parties (`Intelligence.h:430-560`).
- `remaster/tests/scenarios/DiplomacyTests.cpp` and `remaster/tests/scenarios/IntelligenceTests.cpp`:
  - Add tests validating minor race attack consequences, wonder structure unlocking, and framing operation determinism.

## 6. Client Changes

- `remaster/godot/scenes/ui/screens/diplomacy_screen.gd`:
  - Build `HailViewscreen` scene embedding 3D character portrait rig, bridge backdrop texture, and typewriter dialogue subtitles consuming `res://scripts/viewmodels/diplomacy_vm.gd`.
  - Synchronize dialogue text with audio playback from `botf_audio_system.gd`.
- `remaster/godot/scenes/ui/screens/intel_screen.gd`:
  - Redesign interface using LCARS security styling; bind budget sliders directly to `res://scripts/viewmodels/intel_vm.gd`.

## 7. Content and Art Needs (by Asset Key)

- Major Leader Portraits:
  - `portrait.major.fed_president`: Federation President.
  - `portrait.major.kli_chancellor`: Klingon Chancellor.
  - `portrait.major.rom_praetor`: Romulan Praetor.
  - `portrait.major.car_gul`: Cardassian Central Command Gul.
  - `portrait.major.fer_nagus`: Grand Nagus.
  - `portrait.major.dom_founder`: Dominion Female Changeling.
- Minor Race Portraits:
  - 30+ keys: `portrait.minor.<race>` (e.g., `portrait.minor.vulcan`, `portrait.minor.andorian`, `portrait.minor.bajoran`, `portrait.minor.breen`).
- Bridge Backdrops:
  - `bg.bridge.fed`, `bg.bridge.kli`, `bg.bridge.rom`, `bg.bridge.car`, `bg.bridge.fer`, `bg.bridge.dom`, `bg.bridge.alien`.
- Voice Lines:
  - `vo.diplomacy.greet.<fac>`, `vo.diplomacy.propose.<fac>`, `vo.diplomacy.war.<fac>`, `vo.minor.<race>`.

## 8. Acceptance Tests

1. **Hail Viewscreen Activation:**
   - In Diplomacy screen, select the Klingon Empire and click "Hail".
   - Verify Hail Viewscreen opens displaying Klingon bridge backdrop, Chancellor portrait, and authentic Klingon greeting audio plays (`vo.diplomacy.greet.kli`).
2. **Minor Race Wonder Unlocking:**
   - Achieve Membership status with the Andorian civilization.
   - Open System screen construction menu on Earth; verify "Andorian War College" is now available to build and provides +25 ship combat experience upon completion.
3. **Independent Minor Conquest:**
   - Order a fleet of troop transports to invade an independent, unaligned minor system.
   - Conquer the system; verify world is annexed, but Federation attitude toward the aggressor drops by -35 trust with grievance "Attacked neutral world".
4. **Framing Covert Operation:**
   - Fund a high-tier Sabotage operation against the Romulans, selecting "Frame Federation" as the mission modifier.
   - On mission success, inspect Romulan diplomatic dossier; verify Romulan trust toward the Federation drops and a grievance is recorded against the Federation.
5. **Bribery & Secession:**
   - Bribe a rival's affiliated minor race with credits; verify minor attitude increases and secession meter rises until the minor breaks its treaty with the rival.
6. **Web Compatibility & Lazy Fetching Test:**
   - Launch diplomacy screen in a WebGL2 browser export.
   - Hail a foreign power; verify portrait and bridge backdrop render under Compatibility mode, and dialogue audio loads from the lazy-fetched empire pack via `res://` without disk access faults.
   - Save game, reload from IndexedDB, and verify treaty standings and intelligence slider positions persist accurately.

## 9. M1 Vertical-Slice Subset

For the M1 vertical slice:
- Federation and Klingon major diplomacy with complete Hail Viewscreen scenes and authentic voice barks.
- Vulcans (research affinity) and Andorians (military affinity) present as minor races.
- Vulcan Science Academy and Andorian War College structures buildable upon membership.
- Basic Espionage and Security budget sliders active.
