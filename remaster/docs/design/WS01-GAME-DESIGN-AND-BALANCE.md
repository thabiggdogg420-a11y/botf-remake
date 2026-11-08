# Workstream 01: Game Design and Balance

## 1. Reference Behaviour (BOTF and Mods)

In vanilla *Birth of the Federation* (1999), campaigns operated on a rigid turn-based model with five playable major empires: Federation, Klingon, Romulan, Cardassian, and Ferengi [AFC-OV]. Starting parameters permitted selecting galaxy size (Small, Medium, Large [WIKI, AFC-MAP]), tech level (Levels 1 through 8, where "Advanced" placed all six research fields at Level 8 [AFC-OV, AFC-CAL]), starting difficulty (Simple to Impossible [AFC-OV]), and random event toggles [AFC-OV, LPA].

Vanilla victory operated on two primary conditions [WIKI, AFC-OV]:
- **Domination Victory:** Control 60% of galaxy population and systems alone, or 75% in a permanent alliance (conflicting documentation also cites 70% alone / 50% allied [AFC-OV, WIKI]).
- **Vendetta Victory:** Completely eliminate two specified historical rival empires (for the Federation: Romulans and Cardassians; for Klingons: Romulans and Cardassians; for Romulans: Federation and Klingons; for Cardassians: Federation and Klingons; for Ferengi: Federation and Cardassians) [WIKI].

Balance in vanilla was severely skewed:
- Colony rush and light raider spam dominated early turns [AFC-CAL].
- Minor race diplomacy allowed wealthier empires (notably Ferengi) to buy alliances prematurely without substantial trust investment [AFC-CAL, AFC-XAV].
- AI difficulty scaling relied on raw resource cheating rather than enhanced tactical decision-making [AFC-OV].

Modifications introduced by the community altered several dynamics:
- **Ultimate Dominion Mod III (UDM3):** Standardized AI build priorities, drastically raised minor race bribe and alliance costs, balanced late-game weapon damage, and integrated dilithium as a strategic hard gate [UDM3].
- **Star Trek: Supremacy & Birth of the Empires (BotE):** Implemented multi-path victory conditions (Scientific, Cultural, Diplomatic, Conquest), granular difficulty sliders with a non-cheating AI baseline, and distinct campaign pacing options [Supremacy, BotE].

## 2. Modern Target

The modern ruleset preserves the integer-only deterministic simulation core implemented in `remaster/game/Source/BotfSim/Rules.cpp:44-58` and `Simulation.cpp:3900-4200`, ensuring cross-platform lockstep equivalence between Native MSVC and WebAssembly targets (`tests/scenarios/SimulationTests.cpp`, `test_fixture_determinism`).

Key enhancements:
1. **Campaign Presets:**
   - **Classic Preset:** Enforces vanilla 1999 mechanics, original random event rates, standard tech ladders, and legacy victory rules (`Rules.cpp:49-51`).
   - **Remastered Preset (Default):** Activates outposts/starbases, galactic anomalies, 14-card curated event deck, and expanded alternative victories (`Rules.cpp:46-48`).
2. **Settled Victory Thresholds:**
   - **Domination:** Solo control of 60% of total galaxy population and colonized systems, or 75% for an allied bloc, evaluated after turn 150 (`Victory.cpp:15-40`).
   - **Conquest:** Complete military subjugation or elimination of all rival major powers.
   - **Scientific Victory:** Reach Level 10 across all six research disciplines and construct the empire-specific megaproject (e.g., Federation Daystrom Nexus, Klingon Ty'Gokor Tactical Array) (`Victory.cpp:42-78`).
   - **Diplomatic Victory:** Lead a diplomatic coalition controlling ≥66% of the galactic population post-turn 150 (`Victory.cpp:80-112`).
   - **Vendetta Victory:** Eliminate two assigned historical rivals; for the Dominion, assigned rivals are Federation and Klingons.
3. **Difficulty Modifiers:**
   - *Easy:* AI production and research multiplier at 0.8x.
   - *Normal:* 1.0x baseline.
   - *Hard:* 1.25x multiplier to AI production and research.
   - *Impossible:* 1.5x multiplier to AI production and research.
   - *Expert (Non-cheating):* 1.0x baseline production with advanced heuristic decision trees and aggressive tactical fleet assembly (`Simulation.h:difficultyModifier`).
4. **Pacing:** Standard campaign calibrated to 250–350 turns with anti-stall mechanics in late-game diplomacy and conquest.

## 3. Player-Facing Surfaces (Screens and Scenes)

- `remaster/godot/scenes/ui/screens/new_campaign_screen.gd`:
  - Ruleset selector: `Classic` vs `Remastered`.
  - Difficulty selector: `Easy`, `Normal`, `Hard`, `Impossible`, `Expert`.
  - Victory conditions checklist: toggles for Domination, Conquest, Scientific, Diplomatic, and Vendetta.
  - Campaign pacing setting: Quick (0.75x research/build costs), Standard (1.0x), Epic (1.5x).
- `remaster/godot/scenes/ui/screens/overlays.gd`:
  - Victory Progress Modal: Live percentage gauges and requirements breakdown for each victory path (`Victory.cpp:victoryProgress`).
  - Turn Chronicle Modal: Historical milestone tracking.
- `remaster/godot/scenes/ui/screens/database_screen.gd`:
  - Rules Encyclopedia: In-game manual describing exact formulas for morale, trade, growth, and victory conditions.

## 4. Core Changes (Naming Files)

- `remaster/game/Source/BotfSim/Simulation.h`:
  - Add `DifficultyLevel` enum (`DIFFICULTY_EASY`, `DIFFICULTY_NORMAL`, `DIFFICULTY_HARD`, `DIFFICULTY_IMPOSSIBLE`, `DIFFICULTY_EXPERT`).
  - Add `vendettaTargets[2]` array to `EmpireState` struct.
  - Extend `CampaignSetup` struct to include `difficulty`, `enabledVictoriesMask`, and `pacingMultiplier`.
- `remaster/game/Source/BotfSim/Rules.cpp`:
  - In `newCampaign(CampaignSetup)` (around line 120), initialize `vendettaTargets` based on chosen empire and map setup.
  - Register difficulty modifiers into `GameState::difficultyModifier`.
- `remaster/game/Source/BotfSim/Victory.cpp`:
  - Implement `checkVendettaVictory(const GameState& state, int empireId)` (`Victory.cpp:85-130`).
  - Calibrate `checkScientificVictory()` and `checkDiplomaticVictory()` turn 150 threshold gating (`Victory.cpp:32-85`).
- `remaster/game/Source/BotfSim/Simulation.cpp`:
  - Apply `difficultyModifier` to AI yield calculations in `economy()` (line 2850) and `research()` (line 2450).
- `remaster/tests/scenarios/RemasterGameplayTests.cpp`:
  - Add unit tests verifying Vendetta victory triggers, Expert non-cheating yield parity, and difficulty multiplier integrity.

## 5. Client Changes

- `remaster/godot/scenes/ui/screens/new_campaign_screen.gd`:
  - Bind UI controls for difficulty, victory toggles, and campaign preset.
  - Forward selected parameters to `app.start_campaign()`.
- `remaster/godot/scenes/ui/app.gd`:
  - Fix bug where `start_campaign` ignored the `empire` option (`CURRENT-STATE-AUDIT:245`).
  - Marshall `difficulty` and `ruleset` arguments through GDExtension `botf_core.cpp`.
- `remaster/godot/scenes/ui/screens/overlays.gd`:
  - Display Vendetta rival portraits, alive/eliminated status, and completion meters in `victory_overlay`.

## 6. Content and Art Needs (by Asset Key)

- `ui.icon.victory.domination`: Vector/texture icon depicting galaxy conquest map.
- `ui.icon.victory.conquest`: Vector/texture icon depicting crossed phasers / military victory.
- `ui.icon.victory.scientific`: Vector/texture icon depicting atom / galaxy research breakthrough.
- `ui.icon.victory.diplomatic`: Vector/texture icon depicting council / handshake federation seal.
- `ui.icon.victory.vendetta`: Vector/texture icon depicting targeting reticle over rival insignia.
- `ui.badge.difficulty.<level>`: LCARS difficulty badge indicators for Easy, Normal, Hard, Impossible, Expert.

## 7. Acceptance Tests

1. **Empire Setup Passthrough:**
   - Open New Campaign screen, select Klingon Empire, Hard difficulty, and Remastered preset.
   - Start campaign. Verify via F3 Empire screen and save game inspection that the player is Klingon, AI production has 1.25x scaling, and curated events are active.
2. **Vendetta Victory Verification:**
   - Launch Federation campaign with Vendetta enabled (targets: Romulan and Cardassian).
   - Use debug console/drill to eliminate the Romulan and Cardassian empires while Klingons and Ferengi remain active.
   - Advance turn; verify Vendetta Victory triggers immediately, halting the campaign with the victory screen.
3. **Scientific Victory Progression:**
   - Advance all six research fields to Level 10; verify empire megaproject becomes buildable on the capital world.
   - Complete construction of the megaproject; advance turn; verify Scientific Victory triggers.
4. **Expert Difficulty Yield Verification:**
   - Start game on Expert difficulty; inspect rival AI colony turn reports.
   - Verify AI receives exactly 1.0x baseline resource income with zero unearned credits or research points.
5. **Classic Preset Parity:**
   - Start game with Classic preset; verify anomalous hazards, outposts, and alternative victories are disabled in `GameState::rules`.

## 8. M1 Vertical-Slice Subset

For the M1 vertical slice (Federation vs Klingon duel on a Small map):
- Preset locked to `Remastered`.
- Difficulty set to `Normal`.
- Victory conditions enabled: `Conquest` and `Domination`.
- Full validation of victory check triggering at turn boundary when either Qo'noS or Earth falls.
