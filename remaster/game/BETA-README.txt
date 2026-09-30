BIRTH OF THE FEDERATION REMASTERED - PRIVATE BETA
=================================================

Unofficial fan reconstruction. Private build: not for distribution. Packages built with local content
(LocalAssets / Content/Local) contain third-party models and sounds and must never be shared.


START
-----
Run BOTFRemake.exe in this folder. The main menu shows a live flight of one empire's hero fleet.
NEW CAMPAIGN: choose a power, galaxy size, number of powers, era and ruleset (Remastered or Classic), then
BEGIN CAMPAIGN (Enter).

Press ? in game for the full controls reference. The essentials:
  Galaxy map    click selects, double-click opens, right-click orders the selected unit, drag pans, wheel zooms
  Screens       F1 Galaxy  F2 System  F3 Research  F4 Diplomacy  F5 Intel  F6 Empire  F7 Battles
  Turn          Ctrl+Enter ends the turn, Ctrl+S saves, Esc closes panels and then opens the game menu
  Battle        F fight / A auto-resolve at contact, 1-0 manoeuvres, Space next round,
                wheel zooms toward the cursor, double-click a ship to follow it, F / G follow ship / target


WHAT TO TRY
-----------
1. A full early game: explore with the science vessel, colonise with the colony ship, build up Sol.
2. Research, diplomacy with the minor races, trade routes (M switches the map to economic).
3. A battle under tactical command: the camera, the manoeuvres, the hero ship models at true scale.
4. Save, quit to the menu, load, and continue.
5. Settings: resolution, window mode, quality preset, audio levels.


WHERE THINGS ARE
----------------
  Saves      BOTFRemake\Saved\BotfSnapshots\
  Logs       BOTFRemake\Saved\Logs\BOTFRemake.log   (attach this with any bug report)
  Settings   BOTFRemake\Saved\Config\Windows\GameUserSettings.ini
  Build      build-manifest.json (version, commit, tests run)


KNOWN ISSUES IN THIS BETA
-------------------------
- Voice lines (advisor and combat barks) are mostly absent: the voice packs need account logins not yet made.
  Effects and interface sounds are present.
- Classes without a downloaded model use a generated hull; 16 hero classes have full models.
- System names are real star names for new campaigns; campaigns saved before this build keep "System-N" names.
- Turn numbers count completed turns: a new campaign starts on turn 0.


REPORTING
---------
Say what you did, what you expected and what happened, with the time. Attach the log above and, for a
campaign problem, the save from Saved\BotfSnapshots.
