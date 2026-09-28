# BOTF remake research dossier

## Authority, scope and status

Simon's instructions in this chat outrank this document: research the original game and community technical work first; build a modern-looking personal-use remake with the original feeling and a similar gameplay loop, allowing later upgrades and features. This is research, not approval of a final ruleset or architecture.

Research started 26 September 2026. Owner: this Codex chat. Initial source-discovery pass completed; original-game runtime validation, comprehensive code inspection and independent review remain outstanding. No original game executable, mod or third-party code was installed or run during this pass. No authors were contacted.

Evidence labels:

- **SOURCE VERIFIED:** the referenced page/file was read. This verifies what it reports, not that every reported mechanic is correct.
- **COMMUNITY TECHNICAL FINDING:** first-hand tests, offsets, assembly analysis or patches published by community researchers; not reproduced locally yet.
- **INFERRED:** our proposed interpretation or design consequence.
- **OPEN:** insufficient evidence, inaccessible material or a test still needed.

## Initial findings

### 1. There is a substantial technical knowledge base

**SOURCE VERIFIED.** Armada Fleet Command's Structured Modding Index is the best starting map found so far. It connects work on galaxy generation, population, buildings, trade, morale, research, minor races, ships, AI, intelligence, combat and bug fixes. It is a navigation aid, not one internally consistent specification. Some destinations are category pages, and old threads mix discoveries with proposed modifications. [S01]

**INFERRED.** Research should proceed system by system, following each thread through corrections and recording which executable/mod it concerns. We should not reverse-engineer everything from zero, nor implement the first formula found in a search result.

### 2. A documented executable analysis exists; it is not original source code

**SOURCE VERIFIED.** QuasarDonkey's IDA database thread describes named functions, structures and subsystem categorisation. Its stated reference executable is 1,781,248 bytes, MD5 `483448abf30f84d19c7a5b59b0536935`, from a modified installer. These identify that research target only; they do not identify Simon's game. The same thread cautions that an older contributed database contained unreliable labels. Later discussion includes newer database conversions. [S02, S03]

**INFERRED.** Before using offsets, hash our reference executable and record its patch lineage. A disassembly label is a research lead, not proof of function semantics. No official original source release was established in this pass.

### 3. Ultimate Editor is a useful extraction and schema reference

**SOURCE VERIFIED.** The tool documentation describes `stbof.res` as a compressed resource collection alongside `trek.exe`, with editable ship/building/race data and graphics. It supports building-stat overview exports and resource extraction. In BOTF discussions, “UE” often means Ultimate Editor, not Unreal Engine. [S04]

The public GitLab project has Java source, tests and a Gradle build. Its README identifies GPL2; its licence file starts with GPL version 2. Observed master commit: `7ee45c4b89942c324aa8ff0b9cb4334defbc667e`. Repository metadata, file listing, README and licence header were inspected; parser implementation has not yet been reviewed. [S05]

**INFERRED.** Prefer documented data extraction to manually transcribing tables. Keep tool code and extracted game content separately attributed. The tool's licence is not evidence of permission for every game asset it can extract.

### 4. Original behaviour and bug-fixed behaviour need separate records

**SOURCE VERIFIED.** Error Correction Mod 5 presents itself as an attempt to correct errors and omissions rather than broadly rebalance the game. Its description nevertheless shows changes with significant economic and strategic effects, including scrapping revenue, terraforming and minor development. That stated intent does not prove every change is neutral. [S06]

**INFERRED.** Use three explicit columns in future specifications: original observed behaviour, community fix, and our selected remake behaviour. Do not quietly use a mod's rules as the original baseline.

### 5. Morale is an early high-value research target

**COMMUNITY TECHNICAL FINDING.** The morale thread describes event changes followed by ongoing building-related effects, with base values and boundary behaviour affecting results. Later replies correct earlier boundary interpretations. It also identifies a branch error associated with morale effects from disabled buildings and supplies a patch location. [S07]

**INFERRED.** Avoid implementing morale as a simple label or one linear bonus. Candidate tests: powered versus disabled structures; event plus building effect in the same turn; values around 99–101 and 119–120; starting, colonised, member and conquered systems; difficulty differences. Expected numbers remain provisional until tested on a pinned executable.

### 6. Research is not necessarily simple point accumulation

**COMMUNITY TECHNICAL FINDING.** The research thread identifies `Sub_4529F0`, a reported decay constant of 0.05 at file offset `0x177328`, and a progress-related bonus limit of 0.1 at `0x177330`. A later post identifies a test of allocation percentage instead of actual contributed points as a decay bug. The thread also contains optional AI modifications, which must not be mistaken for baseline behaviour. [S08]

**INFERRED.** Candidate tests should include zero allocation, nonzero allocation with zero output, nearing completion, bonuses, rounding and excess points. These constants alone are not a complete research formula.

### 7. Supremacy has multiple lineages

**SOURCE VERIFIED.** Public `BotF2/BotF2` describes itself as Rise of the UFP and a version of Supremacy. Its repository includes C# simulation-related directories; the README says some graphics live separately and describes older framework dependencies. The root `LICENSE.md` was fetched directly and identifies Microsoft Reciprocal License (MS-RL). Observed master commit: `dace2dd3f7334974b2b694579438e35c223b8a8c`. No build or implementation audit was performed. [S09, S10]

**SOURCE VERIFIED.** In an April 2026 reply, current Supremacy team member Iceman says their source is not public and distinguishes a version with Unity combat from a development version without it. This is a dated maintainer statement, not proof about all historical forks or future availability. [S11]

**INFERRED.** Public forks can inform architecture, but cannot be treated as the current game's source or a faithful implementation of original BOTF. Record code licensing and art provenance independently before reuse.

### 8. A new comparable project is worth following

**SOURCE VERIFIED.** Dafedz announced Rebirth of the Federation in August 2026 as a Unity 6 URP/C# spiritual successor, describing BOTF-like play with additions. The announcement reports a pre-release state and an intended later alpha; that is an author's plan, not a tested release. [S12]

**INFERRED.** Useful comparison topics include galaxy readability, fleet orders, diplomacy scope and AI development. Its chosen engine is not evidence that we should change ours. No reusable source or asset licence was established here.

## Initial system map and evidence gaps

This is a research work breakdown, not a verified reconstruction of original turn-processing order.

| System | What we need to establish | Current evidence / gap |
|---|---|---|
| Galaxy and starts | Sector geometry, distributions, system capacity, starting advantages | Index routes found; formulas not inspected |
| Population and colonies | Growth, food shortages, labour, terraforming, starting facilities | Ultimate Editor capabilities read; numeric rules pending |
| Economy | Industry, power, credits, maintenance, trade, buying/scrapping | ECM shows why baseline matters; exact accounting pending |
| Morale | Events, ongoing effects, boundaries, ownership and difficulty | Technical thread read; runtime tests pending |
| Research | Allocation, progression, decay, bonuses, unlocks | Technical thread read; rounding/overflow/order unresolved |
| Diplomacy and minors | Contact, attitudes, gifts, treaties, membership, conquest | Source routes found; thresholds and AI decisions pending |
| Intelligence | Allocation, defence, espionage, sabotage, attribution | Index route found; implementation unreviewed |
| Fleets and exploration | Orders, range, speed, scanning, stations, interception | Index routes found; path/range rules unreviewed |
| Tactical combat | Orders, round resolution, accuracy, shields, hull, retreat | Dedicated community routes found; formulas unreviewed |
| Strategic AI | Economic goals, fleet composition, diplomacy, difficulty bonuses | Index routes found; no behavioural model verified |
| Events and victory | Triggers, probabilities, monsters, victory predicates | Manual baseline located; exact executable conditions pending |
| Saves and turn order | State layout, random numbers, sequencing, replay | Highest-priority structural gap; no pinned saves yet |
| Presentation and feel | Faction interfaces, sound cues, feedback, pacing | Manual located; reference capture and Simon's priorities pending |

The original manual is available in a web transcription. It distinguishes strategic turn timing from tactical combat and manual from automatic combat handling. It provides intended player-facing behaviour, not executable proof. Its numerical victory description should be checked against actual code before adoption. Executable-analysis evidence now explains an alliance-threshold discrepancy; see the additional findings below. This is not yet locally reproduced. [S13]

## Recommended next research sequence

1. **Specify the first playable loop from existing evidence.** Record colony economy, growth, production, exploration, research and minor diplomacy contracts, including rounding and exceptions.
2. **Trace turn dependencies.** Establish when payments, movement, ownership, production, completion, growth and morale apply. Keep unresolved ordering explicit.
3. **Inspect Ultimate Editor readers.** Catalogue building, ship, race, technology and environment schemas before selecting an extraction approach.
4. **Resolve contradictions selectively.** Compare manual intent, technical findings and mod changes. Use a pinned reference installation only for precise questions the documentary evidence cannot settle.
5. **Extend diplomacy, AI and combat research.** Record source corrections and version limitations; do not substitute a conquest-only game for BOTF's broader loop.
6. **Build against bounded contracts.** Follow [the beta acceptance matrix](../planning/BETA-ACCEPTANCE.md). Simulation scaffolding can proceed with named provisional policies; historical accuracy claims require stronger evidence.

Simon confirmed that only the installer collection is present. No installation or additional answer from him is required to continue documentary research and planning.
Suggested rule-record fields: ID; subsystem; claim; source/post; source date; executable hash; original/modded status; inputs; outputs; rounding; ordering dependencies; confidence; reproduction steps; result; remake decision. No rule is labelled runtime-verified until its test is actually run.

## Research tools and access

Immediate research can proceed with web access and file inspection. Ultimate Editor is the strongest candidate for later table extraction. IDA databases are a candidate for targeted code tracing; compatibility and provenance should be checked before choosing an IDA version. No extra analysis software was installed in this pass.

Several raw-file pages failed in the web reader; direct read-only HTTPS requests successfully retrieved the BOTF2 licence and Ultimate Editor metadata/README/licence header. Some AFC attachments require forum access. Do not interpret inaccessible attachments as inspected evidence. Web snapshots have differing crawl dates, so status statements are explicitly dated.

## Existing local work discovered

**SOURCE VERIFIED, filesystem inventory only.** `C:\Users\BiggDogg\AI_stuff\botf-remake` exists. Its `package.json` declares a private TypeScript/Vite project and simulation/self-test commands. The file inventory includes economy, combat, AI, galaxy, game and RNG modules plus faction, ship, technology, building and balance data. This does not establish that it works or faithfully implements BOTF.

The console board's `b-botfassess` entry describes a deferred assessment of this older prototype. The current research does not restart that assessment. No files in that project were modified and no tests were run. Record it as prior work to consider later, not a foundation silently adopted for the Unreal remake.

## Source register

All sources accessed 26 September 2026. Read depth is stated to prevent source discovery being mistaken for code verification.

| ID | Source | Inspection and intended use |
|---|---|---|
| S01 | [Structured Modding Index](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?p=40658) | Index read; routing to subsystem research |
| S02 | [Standard Trek.exe IDA Database](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=2175) | Initial project description, executable identity and caveats read |
| S03 | [IDA database files](https://sourceforge.net/projects/botftrekidb/files/) and [later discussion](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?start=60&style=3&t=2175) | Listing/discussion inspected; database not downloaded/opened |
| S04 | [Ultimate Editor Basics](https://www.armadafleetcommand.com/botf/ultimate-editor-basics) | Resource roles and editor/export capabilities read |
| S05 | [Ultimate Editor repository](https://gitlab.com/stbotf/ultimate-editor) | API metadata/tree, README and GPL2 header inspected; parsers pending |
| S06 | [Error Correction Mod 5](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=4234) | Opening description read; package untested |
| S07 | [Morale](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=62) | Initial analysis, boundary corrections and disabled-building patch read |
| S08 | [Research Progress and Decay](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=105) | Observations, constants and decay bug analysis read |
| S09 | [BOTF2 repository](https://github.com/BotF2/BotF2) and [SupremacyCore tree](https://github.com/BotF2/BotF2/tree/master/SupremacyCore) | README and directory organisation inspected, not implementation |
| S10 | [BOTF2 licence](https://github.com/BotF2/BotF2/blob/master/LICENSE.md) | Full root licence read through raw HTTPS endpoint |
| S11 | [Current Supremacy source availability](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=4634) | Maintainer reply of 13 April 2026 read |
| S12 | [Rebirth announcement](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?p=66165) | Author's announcement inspected; not runtime verification |
| S13 | [Original manual transcription](https://manualmachine.com/gamespc/birthofthefederation/1118235-user-manual/) | Initial targeted reading; full manual extraction/cross-check pending |

## Outstanding inputs

- Specific mod preferences may inform later playtesting; they do not block research. Simon confirmed only the installer collection is present.
- No engine change, final feature selection or reuse of third-party code has been decided by this research.

## Additional mechanics findings — 26 September 2026

These are community technical findings, not local runtime verification. Opening analyses were inspected; multipage topics have not been exhaustively reconciled. The initial system-map statuses above describe the first pass; the entries here record subsequent progress.

### Economy and population

**S14 — Trade.** Gowron reports base credits as `floor(0.05 * source population + 0.1 * destination population)`. Direction therefore matters. Trade, local-credit and global-credit bonuses apply, with bonuses of the same kind added together. Reported route thresholds differ by empire, and a route can become available based on projected growth before the displayed population reaches its threshold. A shrinking population reportedly does not remove existing routes. [Trade and repair analysis](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=59).

Candidate base-only fixture: population 100 to 200 produces 25; reversing direction produces 20. This is arithmetic from the reported formula, not an observed game result. Remake implication: calculate UI estimates from the same rule as actual income; retain directionality unless explicitly changed. Bonus order and route entitlement persistence still need full contracts.

**S15 — Growth.** The population analysis reports growth on the colonization turn, food-shortage losses of `0.1 * sqrt(P * (P - food))` with remaining population rounded up, and growth constraints from insufficient employment capacity. With sufficient food, reported growth uses population and effective food, including capacity limits; fractional results can use probabilistic rounding. It also reports trade goods as industry square root rounded up. [Population, terraforming and trade goods](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=58).

Candidate fixtures: isolate shortage versus adequate food, employment limits, capacity and newly colonized systems. Trade goods at industry 5 gives 3 under the reported rule. Do not implement population using a single displayed percentage. Probabilistic rounding requires a saved random state in the remake, without implying reproduction of the original RNG algorithm.

### Intelligence and invasion

**S16 — Intelligence.** The technical account distinguishes accumulated area points controlling attack frequency from a history of the last 20 turns' area output affecting success. Department preference changes over time. It identifies a building-bonus allocation bug where nonzero sliders receive bonuses as if allocated fully. [Intel mechanisms](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=2311).

This is enough to reject a specification consisting only of a current-turn attack percentage. It is not a complete intelligence algorithm: thresholds, defenses, targeting, attribution and later corrections remain to be reconciled. Proposed data design must accommodate rolling history and per-target state; save/load tests must preserve both.

**S17 — Invasion.** Gowron's analysis describes probability `A² * B / (A² * B + D²)`, where A is combined attacking ground strength after the racial factor, B is the attacking empire bonus factor, and D is defending strength including bonuses. The author expresses uncertainty about the racial-factor variable and processing of multiple task forces. [Ground combat analysis](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=1632).

Mathematical examples with B=1: equal nonzero A and D gives 50%; twice the attacking strength gives 80%. These are candidate fixtures, not measured outcomes. Zero denominators, multiple forces, casualties and later replies must be resolved before adopting this as a full invasion contract.

### Victory discrepancy resolved at source level

**S18 — Domination.** Executable analysis reports checks after turn 60, single-empire thresholds over 60% of current empire population and map systems, and an alliance multiplier of 0.67 rather than the manual's 0.75. It also reports an omitted alliance system-count check and possible inconsistencies in total system counting. Later discussion includes fixes and a Borg-spawn defect. [Domination analysis and corrections](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=1331).

The manual and executable claims can now be recorded separately rather than treated as one rule. Proposed remake policy: explicit configurable victory predicates, checked against the authoritative current galaxy, with boundary tests. Exact thresholds remain a design decision; no accidental adoption of the reported bug.

### First contract backlog

| Contract | Evidence now available | Still required before fidelity claim |
|---|---|---|
| ECO-01 trade base income | S14 formula and directional example | Bonus sequencing and version comparison |
| POP-01 growth and shortages | S15 conditions and rounding account | Full branch order, caps, employment and RNG boundaries |
| ECO-02 trade goods | S15 reported conversion | Minimum-output interaction and original/mod distinction |
| INT-01 intelligence state | S16 history/stockpile distinction | Complete thresholds, attribution and corrected behaviour choice |
| INV-01 invasion probability | S17 formula and caveats | Multi-force order, effective strengths and edge cases |
| VIC-01 domination | S18 manual/code discrepancy | Selected remake predicates and boundary scenarios |
| TURN-01 phase dependencies | S07/S08/S15 identify ordering-sensitive effects | End-to-end turn trace; largest structural gap |

Next research should turn ECO-01, POP-01 and TURN-01 into explicit contracts, then complete the minor-diplomacy contract needed for the first playable slice. The [beta acceptance matrix](../planning/BETA-ACCEPTANCE.md) sets implementation order and milestone acceptance criteria. No additional software installation is needed for those documentary tasks.

## Local reference installer identified — 26 September 2026

Simon confirmed playing both vanilla and modded BOTF for multiple hours each, and identified `F:\Game Files\BOTF_All_in_One_1.0.1` as the installer collection he last used. This establishes his historical reference, not a preference for one ruleset over the other.

**SOURCE VERIFIED, read-only inspection:**

| File | Bytes | Finding |
|---|---:|---|
| `BOTF_All_in_One_1.0.1.exe` | 1,173,026,311 | Installer present; not executed |
| `BOTF_All_in_One_1.0.1.zip` | 1,160,259,244 | ZIP listing contains the installer and readme; not extracted |
| `All_in_One_readme.htm` | 10,728 | Bundled documentation read |
| `Multi_1080_installer.exe` | 132,804,659 | Separate executable present; not run or analysed |
| `turn_processing.patch` | 22,606 | Header and initial patch entries inspected; attributes heap-processing optimisation to QuasarDonkey; not applied |

Installer MD5: `BA143E1C3BE25EB5FD9135616A9BD584`, matching the value in the bundled readme. This is consistency with accompanying documentation, not independent publisher authentication.

Installer SHA-256: `D29B4CF0BE6BC20FCAFE04B1453966EE247614159A6513B49FD6E29BDE8F73E7`.

The readme labels the release date `6-12-2017` (date order not resolved), describes up to 26 mods, and lists Ultimate Editor `0.7.2dev5c`, DxWnd `2.04.32`, QD's Patcher and EdificePatcher. It mentions patch folders for BOP, UM5, ECM4.0, UCW and UDM. These are documented package capabilities, not verified installed components or a complete mod inventory: the component list is a remotely linked image.

It describes selectable resolutions, optional fixes and special vanilla options. Therefore “vanilla from this installer” must be recorded with its selected options and resulting file hashes rather than assumed byte-identical to the original retail game. The separately stored turn-processing and display patches are additional provenance questions; nothing here proves they were applied to Simon's previous installation.

Initial checks found no BOTF/Federation uninstall entry in the queried registry views and no game at `C:\BOTF` or `F:\BOTF`. This was a limited search, not evidence that no installed copy exists elsewhere. Source files on F: were unchanged.

Next: continue documentary mechanics research and the implementation plan. If a targeted runtime comparison becomes necessary, prepare an isolated reference installation, record options and hashes, and inspect mods separately.


## First-slice contracts and parser inspection — continuation, 26 September 2026

Status: documentary contracts and proposed policies, not executable tests. This section advances the earlier backlog. Source evidence is separated from choices made to keep the prototype implementable.

### New evidence register

| ID | Source | Read depth and finding |
|---|---|---|
| S19 | [Minor gifts](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=12) | Opening analysis, 2008 correction and later multiplier caveat inspected. Gift effects reportedly arrive two turns after submission; base attitude gain is 4% per 1,000 credits, before modifiers. Costs and effects have separate modifiers. Later corrections exempt some personalities from membership multipliers; another post questions an additional effect multiplier. Exact effective gift strength is not fully settled. |
| S20 | [AI diplomacy](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=4317) | First-page discussion inspected. Researchers identify random outcomes, a saved timer-queue problem and incomplete delay knowledge. This is not a complete minor-membership acceptance formula. Replies about fixes/modifications are not vanilla rules. |
| S21 | [Original tutorial](https://gaming.trekcore.com/birthofthefederation/downloads/tutorial.pdf) | Targeted construction, energy and scrap passages inspected. Player orders can be revised before Turn; construction and salvage resolve through turn processing. Powered shipyards can extend range, and stations provide another range source. This supports command/commit separation, but does not establish complete internal phase order. |
| S22 | [Turn-processing investigation](https://www.armadafleetcommand.com/onscreen/botf/viewtopic.php?t=3533) | Published tests and code discussion inspected. Ship range/speed and repeated AI fleet evaluation are implicated in turn lag. Disabling a call or reducing iterations can also change behaviour; reported timing improvements are not a safe algorithm specification. |
| S23 | [Edifice reader](https://gitlab.com/stbotf/ultimate-editor/-/blob/7ee45c4b89942c324aa8ff0b9cb4334defbc667e/source/ue/edit/res/stbof/files/bst/Edifice.java) | Header/constants, entry access, ID relocation and executable segment dependencies inspected; not every method read. |
| S24 | [Building record](https://gitlab.com/stbotf/ultimate-editor/-/blob/7ee45c4b89942c324aa8ff0b9cb4334defbc667e/source/ue/edit/res/stbof/files/bst/data/Building.java) | Record fields, portions of loading/writing and validation inspected. |
| S25 | [TechTree](https://gitlab.com/stbotf/ultimate-editor/-/blob/7ee45c4b89942c324aa8ff0b9cb4334defbc667e/source/ue/edit/res/stbof/files/tec/TechTree.java) and [TechTreeEntry](https://gitlab.com/stbotf/ultimate-editor/-/blob/7ee45c4b89942c324aa8ff0b9cb4334defbc667e/source/ue/edit/res/stbof/files/tec/data/TechTreeEntry.java) | Container loading/sorting and entry constructor inspected at pinned commit. No code copied or executed. |

Public repository tree requests found source paths; source reads above were pinned to the previously recorded commit. One large response was truncated, so only visible sections and subsequent targeted reads count as inspected. No claim of comprehensive parser review is made.

### ECO-01: base trade income

- Evidence: S14. Scope is one eligible route with bonuses disabled; route entitlement and treaty eligibility are separate policies.
- Inputs: source/destination population, both nonnegative integers. Output: base integer credits plus a calculation breakdown.
- Proposed implementation: `(source + 2 * destination) / 20`, using sufficiently wide integer arithmetic and floor division. This is an algebraic expression of the documented base formula, not original code reuse.
- Candidate checks: (100,200) -> 25; reverse -> 20; (101,201) -> 25; (0,0) -> 0. Negative population is invalid input, not a credit-producing route.
- Open: mixed bonus order, earliest population snapshot, route persistence and treaty changes. Keep the base calculator independent of those decisions. Status: ready for an isolated arithmetic implementation, not complete trade-system fidelity.

### ECO-02: construction, energy and spending contract

Evidence S21 establishes player-facing deferred resolution. The policies below are **P-01 prototype choices**, not extracted BOTF algorithms:

- A colony has a queue of stable content IDs, stored progress, assigned workforce and an explicit power priority list. Costs, capacity, prerequisites and output types come from content definitions.
- Commands are drafts until End Turn. A proposed buy/gift reserves funds in the draft ledger; cancelling releases the reservation. Validate the complete order set so individually affordable orders cannot collectively overspend.
- On commit, reject invalid orders without partial charges. A committed valid transaction is charged once and gets a unique transaction ID. Save/load cannot duplicate it.
- Compute production from the eligible buildings at the start of the colony-economy phase. Complete at most one queued item per turn initially; preserve excess progress for the next item. A completed building first contributes output next turn. These two choices need comparison with original behaviour before a fidelity claim.
- Allocate energy deterministically by the player's priority list and then stable ID. A building must receive its full requirement to be active; show inactive structures and missing energy. Do not silently lower all outputs proportionally.
- Workforce assignment cannot exceed population or installed capacity. Zero assigned workers means zero workforce-dependent output. Exact legacy partial-staffing rounding remains a separate research item.

Acceptance scenarios (synthetic, not original-game measurements):

| Case | Setup | Expected under P-01 |
|---|---|---|
| No double spending | Treasury 100, two proposed purchases of 70 | Second order cannot be committed unless first is cancelled or funds change |
| Completion boundary | Cost 100, progress 90, industry 10 | One item completes; progress resets; new output starts following turn |
| Overflow | Cost 100, progress 90, industry 25 | One item completes, 15 retained; next item does not also complete this turn |
| Short power | Supply 15, two buildings require 10 each | Higher-priority building active, other inactive; explain 5 short |
| Repeated commit | Same transaction ID submitted twice | At most one charge and one resulting effect |

### POP-01: population contract boundary

Use the S15 evidence already recorded; do not label the whole growth model resolved. Split food shortage, employment constraint, capacity, growth calculation and rounding into named functions so they can be independently compared. Required fixtures include adequate food versus shortage, employment just below/at its threshold, a colony at capacity and the colonization turn.

**P-02 prototype policy:** inject the random draw into the rounding function, persist the stream state, and use a documented half-open draw domain [0,1). Exact integer increments never draw randomness; fractional increments round upward only when the draw is less than the fractional part. This is an intentional stable implementation choice; original equality/end-point behaviour is not asserted.

A synthetic increment 2.25 gives 3 with draw 0.10 and 2 with draw 0.80. Save immediately before the draw and resume: the same result must occur. These checks validate our rounding contract, not the original game's random engine.

### DIP-01: diplomatic state and pending actions

Separate five concepts: contact/knowledge, attitude, treaty rights, membership/ownership and a pending action. A gift is not a treaty and attitude is not ownership. A minor can have separate attitudes to several empires but at most one membership owner. Preserve native identity when control changes.

Proposed state fields: target ID, proposer ID, action ID/type, submitted turn, due turn, reserved/charged credits, resolution status and result reason. Save all pending actions and diplomacy timers. Use explicit proposed-turn and resolved-turn labels in the UI; never infer delivery solely from a disappearing message.

S19 supports testing a delayed gift, but its timing language requires a reference comparison to determine the exact UI turn numbering. **P-03 prototype convention:** an action submitted while the displayed turn is T has its gift effect due when the resulting state reaches T+2. Costs are charged at commit, and the due action is resolved once. Effects use a named policy; do not claim every gift earns the unmodified base rate.

### DIP-02: provisional minor membership policy

The following **P-04 policy is for the small authored prototype only**. It is not a discovery about vanilla BOTF and is not a settled final balance decision:

- Start with one independent minor, contact and normalized attitude in [0,100]. No major-empire war or rival membership transfer in this test scenario.
- Friendship requires contact, peace and attitude at least 50. Membership requires an existing friendship from a prior turn and attitude at least 80. Resolve proposals at the next turn boundary using the attitude then; report the failed condition on rejection.
- A successful membership transfers control of the minor's system, retains its native identity and unlocks its configured benefit. Do not retroactively collect past production. Later contracts must cover ships, buildings, diplomacy visibility and benefit scope during transfers.
- Concurrent eligible membership offers do not pick an owner by container iteration order. For the prototype, choose the proposer with highest current attitude; break an exact tie using the saved diplomacy RNG and record that resolution.
- Thresholds and action delays are data settings. The test scenario can set initial attitudes directly; it must not depend on a guessed gift formula to prove membership code works.

Candidate checks: 49/50 friendship boundary; 79/80 membership boundary; absent friendship; contact made this turn; cancelled action; duplicate resolution; two offers on one minor; save before delivery; target already controlled when an action becomes due. Inapplicable offers must produce explicit results rather than silently overwrite ownership. More sophisticated acceptance, rival poaching and personality effects require the remaining research.

### TURN-01: dependency contract

There is still no source-verified total phase order. Preserve the distinction between known local dependencies and proposed sequencing:

| Relationship | Evidence/status | First-slice action |
|---|---|---|
| Draft orders precede resolution | S21 player-facing tutorial | Draft/commit separation, test cancellation |
| Colonization and growth interact in the same turn | S15 community report | Keep colony creation visible to the later population phase; compare initialization separately |
| Transient morale effects precede ongoing modifier application | S07 reported local sequence | Separate event list from ongoing modifier calculation; do not infer every event's global position |
| Gifts have a delivery delay | S19 | Serialized pending-action queue with explicit due-turn semantics |
| Diplomacy timers may survive incorrectly in legacy saves | S20 report | Save timers as authoritative state and test uninterrupted versus resumed runs |
| New construction output timing | Open | P-01 next-turn activation, clearly labelled |
| Ownership versus income timing | Open | P-05 ownership snapshot below |

**P-05 implemented provisional phase sequence (schema 8, 27 September 2026):** validate and copy the decision state; increment the resolving turn; commit player drafts and plan rival orders; charge existing-fleet upkeep; freeze production ownership, facilities, power and workforce; settle colony production/construction, trade income and research; resolve movement/contact, combat and invasions using the committed snapshot; merge ownership transfers and append newly launched ships; resolve due minor diplomacy; resolve population, morale and events; determine campaign outcome, validate and atomically publish the next state. Newly settled, conquered or joined colonies produce for their new owner on the following turn. Newly launched ships act and pay upkeep on the following turn. This is an explicit provisional policy, not a source-verified vanilla total ordering. A failure preserves the pre-turn state and input orders. The earlier proposal to transfer ownership before production is superseded by this implemented contract.

The sequence replaces the build plan's earlier informal placeholder. It is an executable design candidate, not the original turn trace. Membership benefits use the ownership snapshot; future conquests require explicit rules for native morale, captured infrastructure and destroyed colonies. Do not parallelize state-changing phases until their dependencies and deterministic reductions are proven.

### Parser/schema catalogue

**SOURCE VERIFIED, S23/S24.** Edifice describes a building count/address header, prerequisite maps, empire exclusions, 176-byte records and an address map. Code also maintains building IDs in executable segments, so resource records alone are not the whole relationship model. Building fields include names, upgrade reference, requirements, build/energy/population costs, morale and per-race bonus arrays; unknown fields remain. The reader uses a special no-upgrade value. Treat comments as aids and byte-reading code as the stronger source when they disagree.

**SOURCE VERIFIED, S25.** TechTree loads entries, groups by field, sorts them and reports 72 bytes per entry. The entry constructor reads a name, index, image reference, field, level, unknown bytes, 8-byte cost and description address. This establishes a content-record route, not the algorithm that earns research points.

| Resource | Reader path relative to source/ue/edit/res/stbof/files | Inspection |
|---|---|---|
| Buildings | bst/Edifice.java; bst/data/Building.java | Selected implementation sections read |
| Technology | tec/TechTree.java; tec/data/TechTreeEntry.java | Loader/entry constructor read |
| Ships | sst/ShipList.java; sst/data/ShipDefinition.java | Tree entries found; implementation pending |
| Races | rst/RaceRst.java | Tree entry found; implementation pending |
| Planets | pst/Planet.java; pst/data/PlanetEntry.java | Tree entries found; implementation pending |
| AI/minors | wtf/Aiempire.java; wtf/Aiminor.java | Tree entries found; implementation pending |

Extraction pipeline: preserve a source manifest; export raw tables read-only; keep original numeric IDs and unknown values in staging; map to stable remake IDs; resolve cross-references; validate; emit normalized content with provenance. Do not reshape extracted binary records directly into the authoritative save format. The first local extraction is recorded below; no third-party implementation code has been adopted.

### Readiness after this pass

Ready for isolated implementation: base trade arithmetic, command transactions, deterministic rounding wrapper, pending-action storage, content schema skeleton and a membership state machine under P-04. Not ready for historical-accuracy claims: complete economy modifiers, growth branches, gift multipliers, membership acceptance, whole-turn order and tactical combat.

Next bounded work: complete ships/races parser inspection; reconcile economy bonus/partial-staffing rules; inspect treaty mechanics or explicitly retain provisional acceptance for the first slice. A later controlled installation should target these gaps, not replace the documentary research.

### 27 September 2026 — shipyard reference check and parity requirement

Simon requires every section to be as good as or better than original BOTF. The
build plan now carries the comparison gate; prototype tests alone cannot pass it.
S21 (original tutorial, pages 3, 14–17 and 24) was reopened: powered shipyards and
stations support range; ship classes have different ranges; route/ETA feedback is
shown; available dilithium constrains simultaneous ship construction. Exact costs,
distances, routing ties and priority rules remain unverified.

Implementation `prototype-shipyards-3` uses declared synthetic fixtures: scout range
6, colony-ship range 4, square distance, shortest supplied routes, one construction
slot with stable system-ID priority, and fixed power allocation. Stations, actual
dilithium production, ship tiers and original runtime comparisons remain open.

### 27 September 2026 — local installer reference pipeline

VERIFIED from local bytes: Simon's All-in-One 1.0.1 installer is Inno Setup 5.5.7.
Project-local innoextract 1.9 extracted its English Vanilla components without
executing the installer. Extracted files remain under ignored `reference-local/`.
The common `app/Vanilla/stbof.res` ZIP and `tmp/vanilla/mod` overlays must be treated
together; the common ZIP alone is not the assembled Vanilla installation.
Collision variants were retained. No retail-1.0.2 identity claim is established.

`export-reference.py` reads the technology and ship formats independently, using
Ultimate Editor commit `7ee45c4b89942c324aa8ff0b9cb4334defbc667e` as the format
reference. The normalized local export has 77 technology records, 125 ship records,
and SHA-256 fingerprints for 20 source tables. It preserves numeric IDs, unknown
bytes, description pointers, raw ship race and the separate shiptech race mapping.
The seventh technology group is Sociology, levels 0–10; its presence in the file
is not evidence that the remake should expose a seventh research slider.

ShipDefinition uses 288-byte records; ShipTech uses 16-byte records containing
race, ship ID, parent and six technology requirements. ShipList explicitly takes
race from ShipTech. These readers were inspected directly at the pinned revision.
The exporter rejects partial records, duplicate/missing IDs and non-finite ship
values. Two independent binary-fixture test groups pass, including corruption
cases (`tests/scenarios/test_reference_export.py`). Outputs are reference evidence,
not active gameplay content. Building/race parsing, description decoding, executable
variant selection and runtime comparisons remain open.

P-03 implementation correction: a gift drafted at displayed turn T commits at T+1
and arrives at T+2. The first diplomacy build incorrectly scheduled T+3; a regression
asserting the due turn reproduced this, then passed after correcting the scheduler.
Previously saved due dates remain explicit and unchanged. Current prototype membership
and settlement produce on the following turn; P-05 above now records this implemented
provisional timing choice, not a claim of vanilla fidelity.

Reference exporter follow-up: 318 building records now parse from `edifice.bst`.
The header count and address map are checked against every record's original ID;
upgrade targets must resolve or equal the documented 32767 sentinel. Export keeps
technology requirements, owner/resident masks, costs, bonus arrays, and the full
raw record plus prerequisite header. Three binary-fixture test groups pass,
including bad addresses and unresolved upgrades. Ignore-mask interpretation,
bonus-type semantics and executable-linked special building IDs are not yet
normalized. The latest ignored export is `reference-local/normalized-buildings`.

## Community asset sourcing — 27 September 2026

Simon directed asset reuse ahead of bespoke replacement modelling. The acquisition
pipeline is: catalogue a specific source and author, retain its terms and original
archive, inspect actual meshes/textures, convert in Blender where necessary, then
verify materials, scale, orientation and fleet performance in Unreal. A download
listing alone does not count as an integrated asset.

- [Bridge Commander Remastered 1.2](https://www.gamefront.com/games/bridge-commander/file/bridge-commander-remastered)
  lists 57 additional ships, replacement stock ships, textures and effects. Its
  page explicitly says third-party reuse needs the original author's permission;
  no permission has been obtained and no pack assets have been integrated.
- [Trekmeshes catalogue](https://trekmeshes.eu/download/all) lists 270 models,
  spanning Federation, Klingon, Romulan, Cardassian, Ferengi and other subjects.
  [David Metlesits' Enterprise-D](https://trekmeshes.eu/download/dl196/USSEnterpriseD)
  has a Blender conversion credited to Fabio A. Correa (46.94 MB).
  [Raul Mamoru's Marauder](https://trekmeshes.eu/download/dl215/FerengiMarauder)
  offers OBJ and a 3.41 MB low-poly variant. The download endpoint returned the
  site's terms page rather than an archive: terms limit use to rendering images
  and prohibit redistributing meshes without author permission. These remain
  candidates, not acquired game assets.
- [Chris Kuhn's Defiant](https://blendswap.com/blend/13098) is listed as a 6.88 MB
  Blender 2.7 model under CC-BY-NC. Actual download access and modern material
  conversion remain to be verified. BlendSwap's current API requires an account
  key; no account, paid credits or connector settings were created or changed.

Five original procedural silhouette studies were generated and imported as
temporary scene-integration fixtures. They do not satisfy the final ship-art
quality gate and must not be confused with downloaded community models.

The reference catalogue now contains 77 technologies, 125 ships, 318 buildings
and 35 races. It retains unresolved ship race ID 36 and the Guardian Weapons 11
requirement. Planet records are fingerprinted only; structured planet semantics
are not yet established. The compiled catalogue is ReferenceCatalogue.h; exporting
that data does not establish that every record is playable in the simulation.

Acquisition proof: Icewolf132 Mesh Pack 1 was downloaded from
https://philharris.co.uk/Armada2Files/armadafiles.com/files/armada-2/ships/3d-meshes/icewolfs-mesh-pack-1/details.html
and extracted locally. Its included README expressly permits conversion and use
in other games with credit. It separately credits Taldren for Ulysses, Yamato and
Lyran Tiger designs. This pack is not a complete five-faction set.

setup/convert-community-models.py converted static Ranger (677 triangles),
Ulysses (1076) and Lyran Tiger (1130) to textured GLB. Three joint-bearing Reaver
variants were skipped by the static importer. Original readme/source hashes
accompany game/ArtSource/Community/Icewolf132. Ranger geometry and UV textures
were verified in a Blender render. Unreal imported all three successfully.
Their dated art does not meet the final modern-presentation bar; Ranger is an
interim Federation illustration, not an exact representation of every ship class.

Conversion follow-up: independent inspection confirmed all three Reaver variants
have only unbound static vertices and 26 unanimated helper joints. The converter
now accepts these static meshes while retaining all source joint names, parents,
positions and rotations in the manifest for future hardpoints. All six models
imported successfully. Authored per-corner normals and material emissive colours
are preserved; metallic=0 and roughness derived from Phong shininess are documented
rendering approximations. This supersedes the initial three-model/skip status.

Asset provenance follow-up: conversion manifests now include texture SHA256
fingerprints, the source-to-Blender axis mapping, centering offset and glTF Y-up
export setting. Helper coordinates remain source-local; their parent transforms
must be composed before applying that conversion for weapon placement. Six-model
Blender conversion succeeds; this metadata change does not alter rendered geometry.
