# Onboarding and mobile plan (draft, 25 Nov 2026)

Outranked by `AI_stuff\standing\simon\botf-rulings.md`. Nothing here is agreed until Simon answers the questions in section 3. His answers go into the rulings file and into this file in the same session.

Source: the playtest feedback in `PLAYTEST-FEEDBACK.md`, 24 Nov. Points from that feedback:
- A tester asked for simpler onboarding.
- A visitor asked about mobile.
- The open item "wordiness of the UI" is still unresolved.

## 1. Simpler onboarding

### Today
- **Starting a game.** Title, then New Campaign, then about 6 options: empire, era, rules, galaxy size, preset, AI. Only then the galaxy.
- **The tutorial** (`scenes/ui/advisor.gd` STEPS):
  - It is 8 text cards: welcome, home system, build, research, send a ship, end turn, captain's log, handover.
  - Each card is 1-2 sentences plus a button.
  - The cards point at screens but do not show *where* to click.
- **The first screen** a player sees has a lot on it at once:
  - 10 side tabs and 7 top-bar figures;
  - alerts, the system panel and the orders strip;
  - about 50 stars labelled FGC-nnn.
- **The tester's experience:** they could tell what the scout could reach, but they found the start heavy going.

### Proposal
1. **Quick Start on the title.**
   - Tap an empire emblem, then "Quick Start", and you are in the galaxy.
   - It uses fixed beginner defaults:
     - Small galaxy;
     - Early era;
     - classic rules;
     - normal AI, with its first war held back to turn 40 or later;
     - tutorial on.
   - New Campaign stays as "Custom game" with every option.
2. **Show, don't tell. The tutorial becomes 5 hands-on beats across the first 3 turns.**
   - Each beat dims the screen except the one control it wants, pulses that control, and gives one short line of at most 12 words.
   - It moves on when the player *does* the thing, not when they press Next. The beats:

   | Beat | Player does | Line (draft) |
   |---|---|---|
   | 1 | taps the scout, then a star | "Send your scout to explore. Tap it, then a star." |
   | 2 | opens Sol, queues a build | "Your home system. Pick something to build." |
   | 3 | presses End Turn | "Orders set. End the turn." |
   | 4 | reads and closes the log (shown short) | "What happened this turn. Close it when done." |
   | 5 | turn 2-3: sends the colony ship to a free world | "A free world in reach. Send the colony ship." |

   - Research, diplomacy, battles and first contact are taught **when they first happen**. One beat fires on the event, such as the first contact or the first battle, never up front.
   - "Skip tutorial" and "Restart tutorial" stay.
3. **Gradual reveal in the tutorial game only.**
   - Side tabs with nothing to show yet are dimmed: Council, Diplomats, Fleet Intel and Records before first contact. They still open.
   - Each one lights with a small "new" mark when it first has something to show.
   - Custom games show everything as now.
4. **Fewer words everywhere.** One pass over every advisor and HUD string:
   - one idea per line, with a 20-word cap on advisor text;
   - every top-bar figure explains itself on hover or long-press.
   This closes "wordiness of the UI".
5. **Measured, not judged by eye.** A scripted first-time drive records:
   - clicks from title to the first End Turn: target 6 or fewer (today about 15);
   - words shown before the first End Turn: target under 60;
   - time to the first End Turn for a scripted player: under 60 s.
   It becomes a verify step, so the start can't drift back to heavy.

### Files
- `advisor.gd` (STEPS becomes beats with a target control and a "done when" check).
- `advisor_panel.gd`.
- A new spotlight overlay.
- `title_screen.gd` (Quick Start).
- `new_campaign_screen.gd` (relabel to Custom).
- `app.gd` (tab dim state).
- The string pass across the screens.

## 2. Mobile / portrait mode, with a website toggle

### Today
- The game is laid out for 1920x1080 landscape (`canvas_items` stretch). On a phone it shrinks to tiny text.
- It needs hover for tooltips, and has no touch gestures.
- The website (Web-Latest) is one WebGL build with Godot's standard `index.html`.

### Proposal
1. **The website toggle.**
   - The host page gets a "Desktop | Mobile" switch.
   - It picks automatically: touch screen plus a narrow width means Mobile.
   - The choice is remembered on that device, and `?mode=mobile` forces it.
   - Mobile starts the **same build** with a mobile flag passed in, so one upload serves both and there are no duplicate downloads.
   - Mobile always uses WebGL.
2. **The portrait layout (1080x1920 base), screen by screen.**
   - **Top bar:** 3 key figures (credits, research, turn). Tap them for the full list.
   - **Galaxy map:** fills the screen. Pinch to zoom, drag to pan, tap a star to select it.
   - **Bottom tab bar** with 5 tabs: Chart, Systems, Research, Fleets, More. More holds Council, Diplomacy, Intel, Records and Log.
   - **Panels:** the side panels (system, fleet, alerts) become bottom sheets that slide up.
   - **Advisor and tutorial:** a slim banner above the tab bar.
   - **Battle:**
     - the 3D view takes the top 60%;
     - orders sit in a bottom sheet;
     - camera modes cut to 3 (overview, follow, cinematic);
     - landscape is allowed in battle.
   - **Title and Custom game:** a single column.
3. **Touch rules.**
   - Tap means click.
   - Long-press means tooltip; nothing may be shown by hover alone.
   - Minimum tap target 44 px at phone scale.
   - Drag and pinch work on the map and in battle.
   - Sound starts on the first tap, which browsers require.
4. **Phone performance and memory.**
   - Render scale auto-drops to 0.6-0.75 on slow frames.
   - Bloom is off on mobile.
   - Lower-resolution packs for ships and planets.
   - The tab stays under the iPhone Safari memory ceiling (about 1-1.5 GB).
   - Targets: 30 fps on the galaxy map and in battle on a mid-range phone, and a first load under 25 MB.
5. **Testing.**
   - Layout and touch: checked in Chrome's phone emulation, every screen, both orientations.
   - Real check: Simon's phone, from a short check sheet, plus one other model if a tester offers.
   - The Android emulator belongs to another project, so it is not used.

### Order of work
| Stage | What | Simon sees |
|---|---|---|
| M-A | Website toggle and mobile flag; landscape phone made usable (bigger UI scale, touch taps) | Phone in landscape works, small |
| M-B | Portrait layout for title, galaxy, system, research, fleets, log | Portrait strategy game |
| M-C | Portrait battle, diplomacy, council, intel, records | Whole game in portrait |
| M-D | Gestures, long-press tooltips, phone performance and packs | Smooth on a phone |
| M-E | Real-phone check sheet and fixes | Release to the site |

Onboarding (section 1) goes first. It is smaller, helps every player, and its tutorial beats and spotlight are reused by the mobile layout.

## 3. Decisions (25 Nov). Simon: "You decide using your judgement". The lead decided:
1. **Quick Start:** yes, with the beginner settings as listed.
2. **Tutorial game:** advanced tabs are dimmed until they first have something to show. They still open when tapped. Custom games are unchanged.
3. **Portrait battle:** playable in portrait. Turning the phone sideways also works, but the game never asks for it.
4. **Order:** two lanes at once.
   - Lane 1: all of onboarding (section 1).
   - Lane 2: mobile stage M-A only (website switch, mobile flag, a usable landscape phone UI).
   - Once onboarding has merged, M-B to M-D follow and reuse the spotlight overlay. M-E (the phone check) is Simon's.

Questions as originally put:
1. **Quick Start:** should Quick Start use the beginner defaults listed above (Small galaxy, Early era, classic rules, normal AI, tutorial on), or something else?
2. **Gradual reveal:** should advanced tabs be dimmed in the tutorial game until they first have something to show?
3. **Portrait battle:** should battle work in portrait, or should the game ask the player to turn the phone sideways for battles?
4. **Order:** should onboarding come first and then mobile, or both at once in parallel lanes?

## 4. Stage M-A as built (25 Nov, branch lane/mobile-ma)
- **Mode:** `scripts/platform_mode.gd` (`PlatformMode.is_mobile()` from anywhere). Web: the page's `botf-mode.js` decides before the engine starts (`?mode=` in the address, else the remembered switch, else touch plus a screen whose shorter side is 640 px or less) and sets `globalThis.BOTF_MODE`; the game reads it at boot (and makes the same decision itself, `PlatformMode.decide`, if an older page did not). PC exe: `--mobile` / `--desktop` / `--platform-mode=<m>`, the `BOTF_MODE` environment variable, or Settings, Touch layout (Auto / Mobile / Desktop; Auto is Desktop).
- **Website:** the Desktop | Mobile switch is in the export's own page (Web and WebGPU presets' `head_include`) and in the Web-Next loader (`godot/web-host/loader.html`, now in git); Mobile always goes to the WebGL build. `tools/web/embed_mode_js.py` keeps the copies in step with `web-host/botf-mode.js`; the build scripts refuse a page without it.
- **Landscape phone:** the content scale rises so a phone window reads like a 1080-line layout at about 66% (`PlatformMode.ui_factor`, device-pixel-ratio aware) and the extra width goes to the layout (aspect expand, no bars); a tap is a click (engine touch-to-mouse); a held finger opens the control's tooltip (`PlatformRuntime`), or the order menu on a star; one finger pans and two pinch-zoom the star chart; bloom and glow are off; the 3D views step their render scale down to 0.6 while frames stay over 42 ms.
- **Not in M-A:** portrait layouts and the bottom tab bar (M-B to M-D), battle gestures and pinch in battle (M-D), lower-resolution packs (M-D), the real-phone check (M-E).
