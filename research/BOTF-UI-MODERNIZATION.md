# BOTF interface modernization: research and design direction

**Date:** 28 September 2026  
**Scope:** Research, source grounding, and modernization design choices.  
**Preserved Baseline:** The [parked playable beta](../planning/milestones/BOTF-20260928-PLAYABLE-BETA.md) remains byte-for-byte untouched.

---

## 1. Executive Summary & Findings

Modernizing the *workflows across the game* while retaining BOTF's recognizable information density, empire identity, and command-and-watch combat is the core objective. Historical review and successor discussions indicate friction around locating an issue, understanding its consequences, taking legal action on the target object, and returning seamlessly to strategic context.

Rather than a superficial restyle, the evidence supports an integrated command hub, connected navigation between galaxy map, systems, fleets, reports, and empire sheets, and actionable status feedback. Proposed interface patterns and automation features represent **project engineering choices** designed to streamline gameplay, not historical community consensus.

---

## 2. Primary Evidence by Source & Tier

To avoid conflating distinct eras of game design and feedback, evidence is separated into distinct tiers:

### A. Original BOTF (1999–2000)

1. **GameSpot Review by Elliott Chin (2 May 2000):**
   - **Repetitive Colony Management:** Criticizes the turn-by-turn management burden that compounds as empires expand to dozens of systems, requiring repetitive manual maintenance every turn.
   - **Color & Icon Ambiguity:** Identifies confusing color palettes and ambiguous icons that hinder rapid scanning and interface readability.
   - **Hidden Troop-Transport Mechanic:** Notes that troop transports possess the undocumented ability to establish outposts and starbases, a mechanic unexplained in the documentation.
   - **Documentation & Tutorial Omissions:** Highlights that the tutorial guide and manual omit critical gameplay rules, race capabilities, and technology progression paths.
   - **Tactical Combat Ship Visibility:** Finds that small starships (e.g. scouts) are difficult to see, track, and select in 3D combat when positioned near large capital ships.
   - **Link:** [GameSpot Review (2 May 2000)](https://www.gamespot.com/reviews/star-trek-the-next-generation-birth-of-the-federat/1900-2535046/).

2. **Tutorial Guide (MicroProse / Hasbro Interactive, 1999):**
   - Documents the lack of native colony automation in the tutorial walk-through: population units are manually assigned to food, industry, and energy structures.
   - Instructs that food output must satisfy colony population requirements to sustain growth and prevent deficits.
   - Citizens in the tutorial require manual assignment across resource structures.
   - *Scope Note:* Claims from the physical retail manual have been omitted as no direct accessible manual link was verified in this session; this item is supported solely by the inspected tutorial PDF.
   - **Link:** [TrekCore Tutorial Guide PDF](https://gaming.trekcore.com/birthofthefederation/downloads/tutorial.pdf).

3. **Armada Fleet Command "View all ships" Thread (March–June 2023):**
   - **Single User Request:** One player asked whether a screen or keyboard shortcut existed to view a consolidated list of all owned ships across the galaxy.
   - **Direct Answer:** A forum respondent replied confirming that no global ship list exists in vanilla BOTF.
   - **Coordinates Inquiry:** A user asked how to view sector coordinates on the colony screen.
   - **Moderator Note:** A community note pointed out that certain third-party mods expose coordinates in the colony UI.
   - **Scope:** This thread represents a single user's inquiry and specific forum replies; it does not constitute a measured community consensus.
   - **Link:** [AFC Thread (2023)](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=4268).

---

### B. Successor Experience: Star Trek: Supremacy (2021–2024)

1. **Empire Management Tips (Armada Fleet Command, 2021):**
   - A *Supremacy* maintainer advised players managing large empires to review an empire-wide colonies table each turn for next-turn completions, free energy, and unassigned labour, jumping directly to colonies needing attention.
   - **Link:** [AFC Supremacy Tips (2021)](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=4084).

2. **UI & Display Scaling Discussion (Armada Fleet Command, 2024):**
   - A player reported an ambassador control being clipped when running on a 13.3-inch 4K display at 300% Windows scaling.
   - The maintainer confirmed high-DPI layout clipping issues on compact 4K screens and discussed UI layout constraints for colony labour and descriptions.
   - **Link:** [AFC UI Thread (2024)](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?p=63010).

---

### C. Remake Play Feedback (September 2026)

1. **Simon's Playtest Report (28 September 2026):**
   - Reported friction from having to manually assign newly grown population to farms nearly every turn to avoid starvation on growing colonies.
   - Suggested optional colony automation with independent workforce and queue priorities (Balanced, Growth, Research, Manufacturing) and manual override capability.

---

### D. Modernization Design Choices (Project Engineering Decisions)

The following solutions are **deliberate project choices**, not historical vanilla features or surveyed community consensus:

1. **Connected Strategic Command Shell:**
   - The galaxy map remains the strategic command hub.
   - Selecting a system or fleet updates contextual sidebars; double-clicking opens full sheets.
   - A dedicated `BACK` button and `Backspace` shortcut return the player directly to their previous strategic context.
   - `Escape` cleanly unwinds open sheets and overlays back to the galaxy hub.
   - Full keyboard navigation: `F1`–`F7` direct sheet shortcuts, `Q`/`E` colony cycling, `Tab` fleet cycling, and arrow-key map navigation.

2. **Turn Inbox & Actionable Reports:**
   - Turn events, battles, proposals, shortages, and idle alerts are indexed with direct clickable links to the affected system, fleet, or diplomacy dossier.
   - Pre-turn warning dialog flags unapplied research changes, starving colonies, and unassigned queues before committing the turn.

3. **Colony Workforce & Queue Automation:**
   - Independent dual toggles for Workforce and Queue governors (`Manual`, `Balanced`, `Growth`, `Research`, `Manufacturing`).
   - Workforce governor reserves food using the exact simulation economy formula (`economy(state, colony)`), accounting for technology multipliers and powered facility caps.
   - If maximum farm capacity cannot feed the population, a visible warning is surfaced (`governorWarning`) explaining the deficit and suggesting remediation.
   - Touching workforce sliders or buttons pauses automation (`workforceManualOverride = true`) until explicitly resumed (`RESUME GOVERNOR`).
   - Queue governor auto-queues legal choices only, prioritizing structural bottlenecks (food shortages, energy deficits) before priority infrastructure, with transparent decision logging (`queueExplanation`).

4. **Queue Pin Protection:**
   - Players can pin queue items to guarantee project order and survival across governor updates and queue manipulations.
   - Pinned projects reject `CancelFirstConstruction`, `PrioritizeLastConstruction`, `RemoveConstruction`, and `MoveConstruction` across pinned indices unless explicitly unpinned.
   - Actionable disabled states and status tooltips explain why a command is blocked.

5. **Accurate Research Descriptions:**
   - UI domain effect readouts display actual simulation mechanics (Biotech/Computers/Construction/Energy ladder bonuses, Propulsion +1 sector route range per 2 levels, Weapons +1 combat attack and +2 ground invasion strength) rather than generic placeholders.
   - Verified building and ship catalogue unlocks and multi-field prerequisites are displayed dynamically.

---

## 3. Research Scope & Limitations Note

- **Source Scope:** This research is strictly limited to the specific public sources cited above: Elliott Chin's May 2000 GameSpot review, the 1999 TrekCore tutorial guide PDF, the March–June 2023 AFC ship list thread, the 2021 and 2024 AFC *Supremacy* forum threads, and Simon's September 2026 playtest report.
- **Verification Boundaries:**
  - Retail manual claims have been completely removed as no direct accessible retail manual link was available for verification; only the TrekCore tutorial guide PDF was directly inspected.
  - No claim of broad or universal community consensus is made; all modernization features and UI interactions are explicitly classified as project design choices.
  - Sourced claims are restricted to the exact citations and documented behaviors.
