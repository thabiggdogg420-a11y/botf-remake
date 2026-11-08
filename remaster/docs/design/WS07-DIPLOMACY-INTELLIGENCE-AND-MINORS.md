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
   - Fully voiced greetings, proposal acceptances, rejections, and war declarations powered by Simon's Trek audio library (`vo.diplomacy.<act>.<speaker>`) (`VOICE-PLAN.md:35-37`).
2. **Minor Race Wonder Structures:**
   - All 30 minor race signature structures from `ReferenceCatalogue.h` and `MinorRaces.h` (Vulcan Science Academy, Andorian War College, Bynar Planetary Computer, etc.) become buildable across all empire colonies once Affiliation or Membership is ratified (`MinorRaces.h:20-60`).
3. **Attackable Independent Minors:**
   - Independent (neutral/unaligned) minor races are no longer invulnerable to direct military action (`CURRENT-STATE-AUDIT:91`).
   - Empires can establish naval blockades, execute orbital bombardments, or invade minor worlds. Unprovoked attacks trigger severe diplomatic trust penalties across all honorable major powers (`Diplomacy.h:grievances`).
4. **Extended Minor Roster:**
   - Integrate DS9 and late-era minor races from UDM3: Bajorans (morale bonus), Breen (energy-dampening weapon tech), Son'a (metaphasic biotech), Maquis (raider escorts), and Miradorn (smuggling commerce).
5. **Deep Intelligence & Multi-Tier Operations:**
   - Maintain the deterministic integer-only espionage math in `Intelligence.h:180-320`.
   - Dedicated budget sliders: Internal Security vs Rival Espionage vs Rival Sabotage.
   - Department Focus: General, Economy, Science, Military.
   - High-tier covert operations: *Steal Technology*, *Sabotage Shipyard*, *Assassinate Governor*, and *Frame Third-Party Empire* (generating fabricated battle reports in target empire's chronicle).

## 3. Player-Facing Surfaces (Screens and Scenes)

- `remaster/godot/scenes/ui/screens/diplomacy_screen.gd`:
  - Major and minor empire dossiers, relationship matrix, treaty ledger, and tribute demand builder.
  - "Hail" button opening the 3D Hail Viewscreen modal with real-time dialogue options.
- `remaster/godot/scenes/ui/screens/intel_screen.gd`:
  - Budget allocation sliders (Security, Espionage, Sabotage) with Apply/Revert controls.
  - Covert Operations Planner: select target empire, select department, assign agents.
  - Intelligence Dossiers and Agent Chronicle reports.
- `remaster/godot/scenes/ui/screens/empire_screen.gd`:
  - Member Minor Civilizations tab with active racial bonuses, wonder structure status, and secession risk meters.

## 4. Core Changes (Naming Files)

- `remaster/game/Source/BotfSim/Diplomacy.h`:
  - Implement `getDiplomacyAudioCue(empireId, act, tone)` returning exact asset keys for voice playback (`Diplomacy.h:450-480`).
  - Add Dominion diplomatic personality profile and acceptance matrices (`Diplomacy.h:20-55`).
  - Wire severe trust penalties across all known majors when unprovoked war is declared on an independent minor.
- `remaster/game/Source/BotfSim/MinorRaces.h`:
  - Remove attack restrictions on independent minors (`canAttackMinor()` returns true when unallied).
  - Add Wonder Structure unlock query `getUnlockedMinorStructure(minorId)`.
- `remaster/game/Source/BotfSim/Intelligence.h`:
  - Enhance "Frame Empire" operation to inject a synthetic war incident into `TurnReport::chronicle` of the framed parties (`Intelligence.h:410-475`).
- `remaster/tests/scenarios/DiplomacyTests.cpp` and `IntelligenceTests.cpp`:
  - Add tests validating minor race attack consequences, wonder structure unlocking, and framing operation determinism.

## 5. Client Changes

- `remaster/godot/scenes/ui/screens/diplomacy_screen.gd`:
  - Build `HailViewscreen` scene embedding 3D character portrait rig, bridge backdrop texture, and typewriter dialogue subtitles.
  - Synchronize dialogue text with audio playback from `botf_audio_system.gd`.
- `remaster/godot/scenes/ui/screens/intel_screen.gd`:
  - Redesign interface using LCARS security styling; bind budget sliders directly to `res://scripts/viewmodels/intel_viewmodel.gd`.

## 6. Content and Art Needs (by Asset Key)

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

## 7. Acceptance Tests

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

## 8. M1 Vertical-Slice Subset

For the M1 vertical slice:
- Federation and Klingon major diplomacy with complete Hail Viewscreen scenes and authentic voice barks.
- Vulcans (research affinity) and Andorians (military affinity) present as minor races.
- Vulcan Science Academy and Andorian War College structures buildable upon membership.
- Basic Espionage and Security budget sliders active.
