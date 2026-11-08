# Workstream 12: UX, Onboarding and Platform

## 1. Reference Behaviour (BOTF and Mods)

In vanilla *Birth of the Federation*:
- **Interface & Ergonomics:** Fixed 800×600 resolution; clunky nested sub-menus created severe micromanagement strain as empires expanded beyond 15 systems [AFC-OV, WIKI]. Late-game turn calculations frequently bogged down, taking up to 10–15 minutes per turn on contemporary hardware [WIKI, AFC-CAL].
- **Tutorial & Onboarding:** Included an external printed handbook and a basic scripted tutorial scenario; however, the learning curve remained notoriously steep for new players [AFC-XAV, WIKI].
- **Encyclopedia:** Featured an in-game database detailing canonical Star Trek ship classes, alien civilizations, planetary phenomena, and historical lore [WIKI, TREKCORE].
- **Multiplayer:** Supported multiplayer over LAN (IPX/SPX and TCP/IP), direct serial cable, and modem dial-up, with AI filling vacant slots [AFC-OV].
- **Current Remaster State:**
  - The Godot UI is a direct, rigid port of the TypeScript/React web client ("Port of screens/X.tsx" in every UI header) (`docs/research/CURRENT-STATE-AUDIT.md:117`).
  - Zero tutorial code exists in the Godot client (`docs/research/CURRENT-STATE-AUDIT.md:125`).
  - The only exported executable (`BOTFRemastered.exe`) is dated 7 Nov 16:35, predating all modern rounds (`docs/research/CURRENT-STATE-AUDIT.md:36`).
  - Display settings lack a "confirm or revert" timer, risking video lockouts on unsupported refresh rates (`docs/research/CURRENT-STATE-AUDIT.md:116`).
  - Simon on 29 Sep 2026: *"redesign the entire games UI relative to the original to make it more modern and better working for today's tech"* (`docs/research/CURRENT-STATE-AUDIT.md:177`).

Mods and Remakes:
- **Ultimate Editor (UE):** The cornerstone community modding tool that permitted modifying ships, structures, galaxy maps, and textures outside the game [AFC-MAP].
- **Birth of the Empires (BotE):** Implemented modern hotseat and TCP/IP multiplayer alongside extensive in-game modding tables [BotE].

## 2. Modern Target

Folding Simon's explicit decisions (§7 of `docs/MASTER-PLAN.md`): **Private Multiplayer**, **Modern Redesigned UI**, and **Built-in Mod Tools**.

1. **Three-Layer Client UI Architecture:**
   - Retire the brittle "Port of screens/X.tsx" paradigm entirely (`docs/M0-LANES.md:84-93`):
     - *Layer 1: View Models (`remaster/godot/scripts/viewmodels/`):* Pure data and signal layer, decoupling screen presentation from raw core DLL pointers (`view_model.gd`, `shell_vm.gd`, `new_campaign_vm.gd`, `system_vm.gd`, `empire_vm.gd`, `research_vm.gd`, `fleets_vm.gd`, `combat_vm.gd`, `diplomacy_vm.gd`, `intel_vm.gd`, `database_vm.gd`, `saves_vm.gd`, `overlays_vm.gd`, `end_turn_vm.gd`).
     - *Layer 2: Presentation (`remaster/godot/scenes/ui/screens/`):* Pure scene composition and animation (`title_screen.gd`, `system_screen.gd`, `combat_screen.gd`, etc.).
     - *Layer 3: LCARS Widget Library (`remaster/godot/scenes/ui/`):* Reusable design-system components in `lcars.gd` (capsules, elbow brackets, sliders, data tables).
2. **Direct Strategic Map Interaction:**
   - Full keyboard navigation (F1 Galaxy, F2 System, F3 Empire, F4 Research, F5 Fleets, F6 Diplomacy, F7 Intel, F8 Database).
   - Drag-and-drop task force merging, radial right-click order menus, and mouse-wheel continuous zoom.
3. **First-Run Tutorial & Intelligent Advisor:**
   - "Starfleet Academy Cadet Simulation": An interactive, narrative-driven tutorial campaign guiding players through colony job allocation, technology research, 3D galaxy navigation, and WEGO tactical combat.
   - Contextual LCARS Advisor: Automated non-intrusive tips that pop up when economic deficits, starvation, or border incursions occur.
4. **Interactive 3D In-Game Encyclopedia:**
   - Spiritual successor to the vanilla database: Allows players to rotate, inspect, and analyze 3D models of all 223 ship classes, 9 planet types, and 318 structures, accompanied by complete canon Star Trek technical manuals and production history.
5. **Integrated In-Game Mod Tools:**
   - Internalized equivalent of the *Ultimate Editor*: An in-game Mod Suite allowing players to adjust ship stats, weapon mounts, structure ladders, tech prerequisites, and galaxy generation algorithms with instant JSON export to `remaster/data/` (`docs/M0-LANES.md:94-105`, `docs/DATA-FORMAT.md`).
6. **Private Multiplayer (Hotseat & Local LAN):**
   - Strictly private, non-commercial multiplayer supporting up to 6 human commanders over local network (LAN) or Hotseat on a single machine.
   - Simultaneous strategic turn planning with synchronized commit; tactical combat resolves through the deterministic WEGO resolver.
7. **Platform, Build Pipeline & Packaging:**
   - Automated nightly builds exporting version-stamped, icon-embedded Windows executables (`docs/M0-LANES.md:106-129`).
   - Display settings safety: 15-second confirm-or-revert countdown on all resolution, window mode, or refresh rate changes.
   - Rolling multi-slot autosave history (last 5 turns) preventing campaign loss.

## 3. Web-Build Consequences

In compliance with `docs/M0-LANES.md` ("Browser target") and Simon's ruling (8 Nov 2026: *"it all needs to be planned around being playable in a browser too"*):
- **Dual Export Target:** The project ships two ways from one unified Godot codebase: a Windows desktop executable and a web export served locally (never hosted publicly).
- **Renderer Fallback (Forward+ vs Compatibility/WebGL2):** Desktop uses Forward+; web uses Compatibility (WebGL2). Quality tiers live in project settings, never hard-coded per scene. The client presentation layer automatically handles WebGL2 shader limitations.
- **Platform Display Settings Restraints:** In web builds, the Display Settings panel automatically suppresses options that browsers cannot honour (exclusive fullscreen, native display refresh rate locks, arbitrary non-aspect desktop resolutions, or multiple OS windows). Window sizing adheres to browser canvas bounds.
- **Saves via IndexedDB:** All save games, preferences, and autosaves serialize through Godot's `user://` path, which maps directly to browser IndexedDB in web builds. The web client includes an in-game save export/import utility (base64 JSON string) to prevent loss during browser cache flushes.
- **Mod Tools & No Disk Paths:** Web builds cannot write directly to the local host operating system. The built-in Mod Tools operate by reading and writing JSON byte buffers in memory (`docs/DATA-FORMAT.md`), offering browser download triggers rather than direct disk I/O. At runtime, zero assets access host disks, `$BOTF_*` environment variables, or `LocalAssets`. Everything loads through `res://` packs or memory buffers.
- **Dual Verification Pipeline:** The automated build and verification pipeline exports and tests both Windows exe and Web builds, running test suites (`pc_smoke`) and capturing verification screenshots on both targets.

## 4. Player-Facing Surfaces (Screens and Scenes)

- Master Screens:
  - `remaster/godot/scenes/ui/screens/title_screen.gd`: Live 3D hero fleet background, Continue, New Campaign, Multiplayer Lobby, Mod Tools, Database, Settings, Quit (`title_vm.gd`).
  - `remaster/godot/scenes/ui/screens/saves_screen.gd`: Rolling autosave slots with visual campaign metadata and sector thumbnails (`saves_vm.gd`).
  - `remaster/godot/scenes/ui/display_settings.gd`: Resolution selector with 15-second confirm-or-revert modal.
- Planned Target Screens & Modals:
  - Multiplayer Lobby screen: Host/Join discovery, player slot assignments, faction selection, turn timer limits.
  - In-Game Mod Editor: Data inspection, table tuning, JSON pack export.
  - Tutorial Overlay: Interactive spotlight masks and narrative instruction cards.

## 5. Core Changes (Naming Files)

- `remaster/game/Source/BotfSim/Simulation.h`:
  - Extend `GameState` with `uint8_t humanPlayerMask` to support up to 6 local or networked human participants in simultaneous turn execution (`Simulation.h:50`).
  - Add `SaveMetadata` struct capturing turn, empire, active victories, and hash fingerprint for UI save slot rendering.
- `remaster/game/Source/BotfSim/Rules.cpp`:
  - Support hot-reloading of data tables loaded from external JSON files (`remaster/data/base/*.json`) without rebuilding C++ binaries (`docs/M0-LANES.md:94-105`, `docs/DATA-FORMAT.md`).
- `remaster/tests/scenarios/CampaignSessionTests.cpp`:
  - Add test fixtures validating multi-player turn commit sequencing and deterministic LAN synchronization.

## 6. Client Changes

- `remaster/godot/scripts/viewmodels/`:
  - Implement and maintain viewmodels for all primary screens: `shell_vm.gd`, `new_campaign_vm.gd`, `system_vm.gd`, `empire_vm.gd`, `research_vm.gd`, `fleets_vm.gd`, `combat_vm.gd`, `diplomacy_vm.gd`, `intel_vm.gd`, `database_vm.gd`, `saves_vm.gd`, `overlays_vm.gd`, `end_turn_vm.gd`.
- `remaster/godot/scenes/ui/display_settings.gd`:
  - Add modal countdown dialog (`Timer` node, 15 seconds) that automatically reverts video mode changes unless explicitly confirmed by the user.
- Planned Tutorial System:
  - Implement step-by-step tutorial director capable of disabling non-essential UI elements and focusing the player on required actions.

## 7. Content and Art Needs (by Asset Key)

- Application Executable Assets:
  - `remaster/godot/icon.ico`: High-resolution multi-layer Windows executable icon (256×256 down to 16×16) featuring the remaster emblem.
- Tutorial Scenario Assets:
  - Pre-configured small galaxy state for the Academy training simulation (planned scenario pack under `remaster/data/`).
- UI Indicator Badges:
  - `ui.badge.autosave`, `ui.icon.multiplayer_lan`, `ui.icon.mod_tools`.

## 8. Acceptance Tests

1. **First-Run Tutorial Progression:**
   - Launch game with fresh user preferences; verify tutorial prompt invites the player to Starfleet Academy.
   - Follow prompts: allocate population on Earth, queue a scout, and move a fleet; verify guidance arrows advance synchronously with completed objectives.
2. **Display Setting Revert Safety:**
   - Change screen resolution to an alternate mode (e.g., 2560×1440); observe the 15-second confirm modal.
   - Allow the timer to expire without clicking "Confirm"; verify the display automatically reverts to the original resolution and window posture.
3. **Private LAN Multiplayer Match:**
   - Host a LAN lobby on machine A as Federation; join from machine B as Klingon.
   - Commit strategic turns on both clients; verify simulation advances simultaneously and transitions both players into a synchronized WEGO tactical battle.
4. **Built-in Mod Tools Data Serialization:**
   - Open in-game Mod Tools; modify the weapon output of the *Defiant*-class pulse phasers from 40 to 60; save mod.
   - Start a new campaign; inspect the *Defiant* in the Database screen; verify updated weapon rating is immediately reflected from `remaster/data/base/roster.json`.
5. **Rolling Autosave Verification:**
   - Play 6 consecutive turns; inspect the Saves screen.
   - Verify 5 rolling autosave slots are preserved (`autosave_turn_N.sav`), each displaying accurate turn numbers, empire emblem, and timestamp.
6. **Web Build Export & IndexedDB Persistence:**
   - Export web package; serve locally and load in a WebGL2 browser.
   - Verify display settings automatically omit unsupported desktop video modes.
   - Create and save a campaign; refresh browser tab; verify save is loaded cleanly from IndexedDB without file I/O errors.

## 9. M1 Vertical-Slice Subset

For the M1 vertical slice:
- Three-layer viewmodel architecture implemented for Galaxy, System, and Combat screens.
- Starfleet Academy tutorial covering opening turn strategic basics (colony allocation, fleet movement, research focus).
- Display settings confirm-or-revert safety dialog fully functional.
- Nightly build pipeline exporting `BOTFRemastered.exe` with custom icon and version stamp.
