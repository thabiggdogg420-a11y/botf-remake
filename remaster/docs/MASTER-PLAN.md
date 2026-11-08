# BOTF Remastered: master plan

Written 8 Nov 2026, on Simon's direction to "pull back, refactor, and compare what we currently have against the original BOTF and all its modded features ... assume you have unlimited resources, time and a full team of expert games software engineers".

Sources that outrank this plan:
- Simon's own words, collected in `research/CURRENT-STATE-AUDIT.md` §3.
- Simon's decisions recorded in REMASTER-ROADMAP.md (Godot is the one engine).

Inputs:
- `research/BOTF-FEATURE-REFERENCE.md` (the original game, its mods and remakes, and modern 4X games);
- `research/CURRENT-STATE-AUDIT.md` (what we actually have, system by system).

---

## 1. The vision, in one page

Each line below traces to something Simon said.

1. **A faithful Birth of the Federation first, then better.** Every vanilla system, option, ship and minor race is present, and so is the best of what the mods added. The original is the floor, not the ceiling: "original-or-better for every section".
2. **AAA presentation everywhere,** not only in combat: "ship models, sounds, UI, all of the features of the game should be WAAAAYYY better than the current state ... 500x better than the original".
3. **A redesigned modern UI,** "relative to the original ... more modern and better working for today's tech", with direct map interaction.
4. **Real 3D hulls for every class,** at true scale and readable at any zoom.
5. **Combat is "a modernized, updated, smartly designed 3d turn based space battle"**, not a copy of BOTF's weakest feature. A real-time mode and a Bridge-Commander-style helm mode stay parked as a later option.
6. **Original BOTF voice and sound,** plus lines from the wider Trek library.
7. **Private and solo.** It is never distributed.
8. **Simon sees milestones, never work in progress** that shows no visible progress.

## 2. Where we really are

**Strong.** The C++ rules core (`game/Source/BotfSim`):
- deterministic, with save schema 14;
- deep diplomacy (3.6k lines plus 1.7k of tests), intelligence, tactical rules, victory, galactic events and a 223-class ship roster;
- 44 of 47 commands reachable from a screen.

This is a better foundation than the original game had, and it stays.

**Weak.**
- **The Godot client is a plain shell.** It has eight text-and-panel screens ported from the web client and has had no design pass of its own.
- **There is almost no art in the project:**
  - zero images in it;
  - planets drawn as one coloured circle;
  - no race portraits;
  - 126 of 159 ship classes on procedural hulls.
- **Sound effects** were picked by spectrum, not by ear.
- **No tutorial.**
- **The only exported exe predates every round since 7 Nov.**

**Missing against vanilla BOTF and its mods:**
- galaxy shapes (spiral, ring, irregular);
- minor-race density;
- difficulty;
- crew experience ranks;
- per-race structure ladders and minor-race "wonder" structures as the player sees them;
- emergency programs;
- dilithium as a real constraint;
- the strategic orders intercept, raid, defend and redeploy;
- invasion aftermath (liberate, subjugate, exterminate);
- monsters and the Borg as actors;
- vendetta victory;
- race portraits and hail scenes;
- end-game speeches;
- Dominion-era content (Ultimate Dominion Mod).

**The process failure.** A single builder ran six passes on battle cameras, with no design lead, no art direction and no independent reviewer. The rest of the game stood still. A studio would never organise like that.

## 3. What unlimited resources change

1. **Studio structure, not one builder.** Each discipline below gets its own lead and its own lane, and the lanes run in parallel. Two roles sit across all lanes:
   - an **art director** who judges every frame;
   - a **QA lead** who plays every build.

   Neither builds anything they judge. The orchestrator (me) owns the plan, merges and integration.
2. **Design before code.** Every system gets a one-to-three-page design doc that sets the BOTF reference behaviour, the mod additions, our modern target and the acceptance test. Then it gets built. BATTLE-DESIGN.md is the model.
3. **A vertical slice before breadth.** We prove AAA quality on a thin slice of the whole game before scaling it. That way quality is set once and copied, not discovered six times over.
4. **Content at full scale.** We do not settle for 33 hero hulls with procedural hulls for the rest. Every class, planet type, race and structure gets real art.
5. **No scoping down.** Multiplayer, mod tools, an encyclopedia with 3D viewers, cinematic hail scenes and an era progression are all in scope, sequenced rather than cut.

## 4. Refactor first (milestone M0)

| Area | Change | Why |
|---|---|---|
| Client architecture | Split the Godot client into three layers: **game-state view models** (one per screen, fed by the core), **presentation** (scenes, 3D, VFX) and **UI widgets** (a design-system component library). Retire the "port of screens/X.tsx" structure. | The current screens mix all three, so every redesign means rewriting a whole screen. |
| Data and content | Move ships, structures, minors, techs, events and text from compiled headers into data files (JSON or Godot resources) that the core loads, with checksums kept for determinism. | It enables the mod tools, balancing without rebuilding the C++ core, and parallel content work. |
| Asset pipeline | Use one manifest for every asset key (ship, planet, portrait, structure icon, VFX, sound). Track the status of each key (procedural / placeholder / final). Reviewing a key means rendering it with a turntable on a contact sheet. | Today art status is invisible. A manifest makes the gap a number that shrinks. |
| Presentation director | One shared camera and staging system for the galaxy, the system and battle, with framing rules (whole hulls, minimum size, empty-space cap) built in and unit-tested. | The six camera passes taught us these rules. Writing them once stops us repeating that. |
| Build and verification | Add a nightly export and an automatic frame capture of a fixed script of scenes (title, galaxy, system, every screen, five battles), with the contact sheets reviewed by the art director lane. Every handover carries a fresh exe. | This prevents a stale exe and a build nobody has looked at. |
| Known bugs | Fix the bugs carried over from R1 to R3.5: the empire option at new campaign is ignored, the round-1 hull preload, blocky ion storms, untested trade routes, and the audio cue builder. | These are clean-up before scaling. |

## 5. Workstreams

Twelve lanes, each with an owner and a design doc. "Ref" means vanilla BOTF or the mods; "Target" means our modern version.

1. **Game design and balance.**
   - *Ref:* the full vanilla ruleset. Recover the tech unlocks and the victory thresholds from the original data files we already parse.
   - *Target:*
     - a parity matrix for every vanilla rule, with each row marked faithful, corrected or modernised;
     - AI-vs-AI balance soaks to each victory type;
     - Classic and Remastered presets.
2. **Galaxy and strategic map.**
   - *Ref:* irregular, spiral and ring shapes, sizes up to 52×39, minor-race density, tech level, difficulty, events on or off.
   - *Target:*
     - all of those options, plus a live preview of the galaxy at setup;
     - a modern **3D galaxy map**: zoom smoothly from galaxy to sector to system, with territory volumes, supply and trade routes drawn in space, and anomalies as volumetrics;
     - the right-click flow kept.
3. **Systems, planets and economy.**
   - *Ref:*
     - five work areas;
     - race-specific structure ladders;
     - dilithium refinery limits;
     - emergency programs;
     - morale tiers;
     - one purchase per system per turn.
   - *Target:*
     - a 3D system view with the star, orbiting textured planets and the structures visible;
     - all vanilla structures and dilithium;
     - emergency programs;
     - governors and automation that answer BOTF's micromanagement complaint;
     - every number with its explanation, already in the core.
4. **Research.**
   - *Ref:* six fields of ten levels, unlock lists and minor-race bonuses.
   - *Target:*
     - an expanded tree with original-text descriptions;
     - a visual tree with 3D previews of the unlocks;
     - era starts.
5. **Ships, fleets and logistics.**
   - *Ref:*
     - fixed classes upgraded by tech;
     - crew ranks from Green to Legendary;
     - training academies;
     - intercept, raid, evade and avoid orders;
     - task forces.
   - *Target:*
     - every vanilla order plus the Rise of the UFP ones (defend, idle, redeploy);
     - crew experience;
     - refits;
     - a ship detail screen with a 3D viewer;
     - wings for small craft.
6. **Combat.**
   - *Ref:* BOTF's simultaneous group orders, the ~15 s turn, cloak first strike, hailing, auto-resolve, replays.
   - *Target:* BATTLE-DESIGN.md, plus WEGO plan-then-resolve (Simon's call, §7):
     - 3D firing arcs and range rings in the plan phase;
     - shield facings and subsystem targeting;
     - damage feedback;
     - a director with group shots;
     - planetary assault with ground combat;
     - invasion aftermath choices.

     The real-time and helm modes come later.
7. **Diplomacy, intelligence and minors.**
   - *Ref:*
     - the eight attitude tiers;
     - the treaty ladder (friendship, affiliation, membership);
     - gifts, bribes and demands;
     - framing other empires;
     - 30 minor races, each with its own structure.
   - *Target:*
     - **hail scenes**: an animated race portrait on a bridge backdrop, with the original voice lines;
     - every minor-race structure buildable;
     - attackable independent minors;
     - the extended roster (Breen, Son'a, Maquis and others) from Ultimate Dominion.
8. **Majors, eras and threats.**
   - *Target:*
     - era progression (Enterprise, TOS/movies, TNG/DS9, post-Nemesis);
     - the Dominion as an optional sixth major, as in Ultimate Dominion Mod;
     - the Borg and species 8472 as tunable galactic threats;
     - the full vanilla monster list (Calamarain, crystalline entity, Husnock and others);
     - vendetta victory.
9. **AI.**
   - *Target:*
     - a personality-faithful AI that builds its best ships and doesn't attack piecemeal;
     - difficulty levels from easy to "impossible", plus a non-cheating Expert;
     - AI explanations shown in diplomacy.
10. **Art.**
    - Every ship class at hero quality (all 159, with BOTF-only classes modelled from the original's references).
    - Planets: every class with texture sets.
    - Race and leader portraits, structure icons and illustrations.
    - A VFX library (fireball meshes, debris, warp, cloak, shields).
    - An art bible fixing scale, palette, lighting and LCARS style.
11. **Audio and voice.**
    - Every effect and music cue picked by ear.
    - The original voice lines hooked to orders, plus order acknowledgements.
    - Expand the voice from Simon's Trek library (TNG/DS9/VOY), as he offered on 30 Sep.
    - Adaptive music.
12. **UX, onboarding, platform.**
    - A full UI redesign from a design system, not a port.
    - A tutorial campaign and advisor hints.
    - An encyclopedia with 3D viewers.
    - Accessibility, display settings and an exported exe with an icon and autosave slots.
    - **Mod tools** (the Ultimate Editor's job inside the game).
    - Hotseat and LAN multiplayer, kept private.

## 6. Milestones

| Milestone | Content | What Simon gets |
|---|---|---|
| **M0 Foundation** | The §4 refactor, the parity matrix, design docs for all twelve lanes, the art bible, nightly export and frame capture | A short read: the parity matrix and art bible to approve. No build. |
| **M1 Vertical slice** | One complete small-galaxy Federation vs Klingon campaign at final quality on every surface it touches. That covers the 3D galaxy map, the 3D system view, every screen redesigned, the hail scene, a WEGO battle, final hulls for both factions, planet art, sound by ear, a tutorial opening and a fresh exe. | The first build he plays: proof that the whole game can look and feel AAA. |
| **M2 Faithful parity** | Every vanilla feature, option, ship, structure, minor race, event, monster and victory type, at slice quality, for all five majors | A full BOTF campaign, original-or-better in every section |
| **M3 Mods and modern** | Ultimate Dominion and Supremacy content, eras, the Dominion, Borg and 8472, the expanded tech tree, AI difficulty, automation, mod tools | The remaster |
| **M4 Polish and extras** | Balance soaks, performance, cinematics and speeches, the encyclopedia, multiplayer, the real-time and helm combat modes | The finished game |

Within each milestone the lanes run in parallel. The art director and QA review every merge from captured frames and play-throughs before anything reaches Simon.

## 7. Decisions only Simon can make

**Decided by Simon on 8 Nov 2026:** "yes start M0, all decisions yes". Questions 1 to 5 below are all a yes:
1. WEGO combat with facings and subsystems.
2. A 3D galaxy map.
3. A playable Dominion.
4. Private multiplayer.
5. His Trek library as a voice source.

Question 6 is still open.

His limit on how the work runs: at most 6 lanes at once ("coz youll destroy my usage"). Gemini and Antigravity (the Tuvok seat) may take lanes.

1. **Combat rules.** Adopt WEGO plan-then-resolve with shield facings and subsystem targeting (BATTLE-DESIGN.md proposals 9-12)?
2. **3D galaxy map.** Replace the 2D star chart with a zoomable 3D galaxy?
3. **A sixth major.** Make the Dominion (and perhaps a Borg mode) playable, or keep them as threats only?
4. **Multiplayer.** Include it (hotseat or LAN, private), or drop it?
5. **His Trek library.** Use it as a voice source (clips cut from the episodes on his drive)?
6. **Mods.** Which mods "Ultimate Mod" and "Galaxies" refer to, if he means specific ones. The research found Ultimate Dominion Mod III, Supremacy, Rise of the UFP and Birth of the Empires.
