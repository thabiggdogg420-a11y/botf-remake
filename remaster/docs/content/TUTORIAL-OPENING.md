# M1 Tutorial Opening: Starfleet Academy Simulation (Federation Start)

This document defines the 16-step onboarding script for the Federation small-galaxy opening scenario (Starfleet Academy Cadet Simulation), as specified in `docs/design/WS12-UX-ONBOARDING-PLATFORM.md` and architected around the Godot strategic client in `docs/CLIENT-ARCHITECTURE.md`.

---

## Tutorial Script

### Step 1: Galaxy Map Navigation
- **id**: `TUT-01`
- **trigger**: `screen_changed("galaxy")` following `App.start_campaign`
- **advisor_line**: "Welcome to Starfleet Command, Cadet. Pan and zoom the star chart to survey our local sector and sensor perimeter." (19 words)
- **ui_highlight**: `GalaxyMap` (star chart viewport in `galaxy` screen)
- **completion_condition**: Camera pan or zoom detected on `GalaxyMap`

### Step 2: Selecting Sol System
- **id**: `TUT-02`
- **trigger**: `galaxy` screen active, map navigation detected
- **advisor_line**: "Sector zero-zero contains our home system, Sol. Select Sol to inspect planetary assets and infrastructure in the inspector." (18 words)
- **ui_highlight**: Sol star system sector on `GalaxyMap` / `Inspector` sector card
- **completion_condition**: `selection_changed` with Sol selected (`AppSession.selection.systemId == Sol_ID`)

### Step 3: Opening System Management
- **id**: `TUT-03`
- **trigger**: `galaxy` screen, Sol system selected
- **advisor_line**: "Access Sol's administrative deck to manage planetary facilities, population assignments, and orbital facilities. Engage the management console." (17 words)
- **ui_highlight**: `OpenSystem` button in `SectorCard` (or `Rail_system` on LCARS rail)
- **completion_condition**: `screen_changed("system")` (navigated to `system` screen / F2)

### Step 4: System Workforce Allocation
- **id**: `TUT-04`
- **trigger**: `screen_changed("system")` with Sol active
- **advisor_line**: "Earth's population sustains our economy. Adjust workforce sliders to balance food supplies, energy production, and industrial output." (17 words)
- **ui_highlight**: `Workforce` frame (sliders `wf_food`, `wf_industry` and `ApplyWorkers` button)
- **completion_condition**: Workforce draft adjusted and confirmed via `ApplyWorkers` (`SystemVM.apply_workers()`)

### Step 5: Queueing Starship Construction
- **id**: `TUT-05`
- **trigger**: `system` screen, workforce applied (`view_changed`)
- **advisor_line**: "Our shipyards stand idle. Select a scout vessel from the construction roster to expand our deep-space reconnaissance." (17 words)
- **ui_highlight**: `Construction` frame starship choice (`Choice_Ship_Scout` in `Construction` panel)
- **completion_condition**: Scout starship added to colony build queue (`SystemVM.queue()` contains new item)

### Step 6: Navigating to Research
- **id**: `TUT-06`
- **trigger**: `system` screen, starship queued in `BuildQueue`
- **advisor_line**: "Scientific progress powers Federation development. Open the Research division from the navigation rail to inspect our technology ladder." (18 words)
- **ui_highlight**: `Rail_research` on LCARS navigation rail (or F4 key)
- **completion_condition**: `screen_changed("research")` (navigated to `research` screen / F4)

### Step 7: Technology Research Allocation
- **id**: `TUT-07`
- **trigger**: `screen_changed("research")`
- **advisor_line**: "Science labs pool research quadrant-wide. Adjust allocation toward propulsion and energy systems, then transmit the directive." (16 words)
- **ui_highlight**: Field allocation sliders (`Alloc0`–`Alloc5`) and `ApplyResearch` button
- **completion_condition**: Research allocation modified and submitted via `ApplyResearch` (`ResearchVM.apply()`)

### Step 8: Returning to Galaxy Map
- **id**: `TUT-08`
- **trigger**: `research` screen, allocation applied (`view_changed`)
- **advisor_line**: "Research priorities are locked. Return to the primary star chart to review our active fleet deployments." (16 words)
- **ui_highlight**: `Rail_galaxy` on LCARS navigation rail (or F1 key)
- **completion_condition**: `screen_changed("galaxy")` (navigated to `galaxy` screen / F1)

### Step 9: Selecting Sol Scout Vessel
- **id**: `TUT-09`
- **trigger**: `screen_changed("galaxy")`
- **advisor_line**: "A scout starship is stationed in Sol. Select the vessel to review warp range and issue navigational orders." (18 words)
- **ui_highlight**: Scout starship unit in Sol sector on `GalaxyMap` / `UnitCard` in `Inspector`
- **completion_condition**: `selection_changed` with player's scout fleet selected (`AppSession.selection.fleetId > 0`)

### Step 10: Plotting Reconnaissance Course
- **id**: `TUT-10`
- **trigger**: `galaxy` screen, scout fleet selected
- **advisor_line**: "Set course for an adjacent uncharted sector beyond sensor range. Right-click the destination to draft warp travel orders." (18 words)
- **ui_highlight**: Adjacent unexplored sector on `GalaxyMap` / `GalaxyOrders` context action `Action_move`
- **completion_condition**: Move course plotted (`AppSession.issue(MoveFleet)` / order drafted in `OrdersCard`)

### Step 11: End-Turn Initiation
- **id**: `TUT-11`
- **trigger**: `galaxy` screen, move order drafted in `OrdersCard`
- **advisor_line**: "All operational directives are drafted for this turn. Signal Starfleet Command to initiate turn progression." (16 words)
- **ui_highlight**: `EndTurn` button in top bar (`_topbar`)
- **completion_condition**: Player activates `EndTurn` button (`App.request_end_turn()`)

### Step 12: Reviewing End-Turn Check
- **id**: `TUT-12`
- **trigger**: `end_turn_check` modal opened (`EndTurnCheck.open()`)
- **advisor_line**: "The LCARS end-turn diagnostic audits production forecasts and idle assets. Review the projection and confirm turn resolution." (17 words)
- **ui_highlight**: `EndTurnCheck` modal panel and `EndTurnAnyway` button
- **completion_condition**: Player confirms `EndTurnAnyway` (`App.end_turn()`)

### Step 13: First Contact Briefing
- **id**: `TUT-13`
- **trigger**: `overlays` (`Briefing`) modal opened with diplomatic first contact event
- **advisor_line**: "Long-range sensors report contact with an unidentified alien vessel. Starfleet Command has logged first contact in our diplomatic records." (19 words)
- **ui_highlight**: `Briefing` modal overlay and `Acknowledge` button
- **completion_condition**: Player clicks `Acknowledge` button (`App.close_modal()`)

### Step 14: Tactical Encounter Alert
- **id**: `TUT-14`
- **trigger**: `combat` screen opened with live encounter (`EncounterChoice` modal active)
- **advisor_line**: "Red Alert! Hostile vessels have intercepted our scout. Assume tactical command to direct ship maneuvers personally." (16 words)
- **ui_highlight**: `EncounterChoice` modal and `TakeCommand` button
- **completion_condition**: Player selects `TakeCommand` (`choice = false`, live tactical grid active)

### Step 15: Tactical Combat Planning
- **id**: `TUT-15`
- **trigger**: `combat` screen live view (`choice == false`, `round == 0`)
- **advisor_line**: "Assess weapons envelopes and shield integrity. Designate an evasive or attack maneuver from the tactical console." (16 words)
- **ui_highlight**: `TacticalOrders` panel maneuver buttons (`Maneuver_Evade` or `Maneuver_Engage`)
- **completion_condition**: Tactical maneuver order issued (`CombatVM.order()` / `tacticalOrder`)

### Step 16: Resolving Tactical Round & Aftermath
- **id**: `TUT-16`
- **trigger**: `combat` screen, tactical maneuver order assigned
- **advisor_line**: "Orders logged. Authorize round resolution to execute maneuvers and weapons discharge in WEGO synchronization." (15 words)
- **ui_highlight**: `ResolveRound` button in combat header (or `ContinueTurn` upon battle resolution)
- **completion_condition**: Round resolved (`CombatVM.resolve()` / `_round()` executed, round log updated)

---

## Needs UI hooks

The following element node names, signals, and modal hooks are required by the tutorial director to drive spotlight masks and condition checks beyond the top-level screen routes in `CLIENT-ARCHITECTURE.md` and `WS12`:

1. **`TutorialOverlay` Director Hooks (WS12 §4, §6):**
   - Central tutorial director overlay node in `App` (`_overlay`) capable of drawing spotlight cutouts over designated child nodes and intercepting clicks outside spotlight zones.
   - Event trigger hook `tutorial_step_advanced(step_id: String)`.

2. **Galaxy Screen Hooks (`scenes/galaxy/galaxy_screen.gd`):**
   - `GalaxyMap.camera_moved`: Signal emitted on user pan/orbit/zoom gestures.
   - `OpenSystem`: Unique node path/name for the "Manage system" pill in `SectorCard`.
   - `GalaxyOrders.Action_move`: Queryable identifier on dynamically generated context menu buttons for movement orders.

3. **System Screen Hooks (`scenes/ui/screens/system_screen.gd`):**
   - `Workforce`: Container frame node for workforce sliders.
   - `wf_food`, `wf_industry`: Slider node names inside `Workforce`.
   - `ApplyWorkers`: Confirmation pill button node name in `_forecast`.
   - `Choice_Ship_Scout`: Deterministic name / tag on the scout vessel item in the `Construction` roster (`Choice_Ship_<id>`).

4. **Research Screen Hooks (`scenes/ui/screens/research_screen.gd`):**
   - `Alloc0`–`Alloc5`: Allocation slider node names.
   - `ApplyResearch`: Confirmation pill button node name.

5. **Top Bar & End-Turn Modal Hooks (`scenes/ui/app.gd`, `scenes/ui/end_turn_check.gd`):**
   - `EndTurn`: Pill button node name in `_topbar`.
   - `EndTurnCheck`: Modal root node name.
   - `EndTurnAnyway`: Confirmation pill button node name in `EndTurnCheck`.

6. **Briefing Overlay Hooks (`scenes/ui/screens/overlays.gd`):**
   - `Briefing`: Modal root node name.
   - `Acknowledge`: Dismissal pill button node name.

7. **Combat Screen Hooks (`scenes/ui/screens/combat_screen.gd`):**
   - `EncounterChoice`: Initial choice modal node name.
   - `TakeCommand`: Pill button node name in `EncounterChoice`.
   - `TacticalOrders`: Tactical order frame node name.
   - `Maneuver_Evade`, `Maneuver_Engage`: Maneuver pill button node names.
   - `ResolveRound`: Round execution pill button node name in header.
   - `ContinueTurn`: Post-combat exit pill button node name in header.
