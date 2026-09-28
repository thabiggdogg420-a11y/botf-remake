# Modern BOTF beta acceptance

Authority: Simon's attached execution brief, 27 September 2026, supersedes the former build plan. Preserve working systems; vanilla BOTF is the reference, documented corrections and deliberate modernizations must be identified.

Latest delivery priority: [UI-FIRST-DELIVERY.md](UI-FIRST-DELIVERY.md). Simon rejected the old packaged interface. A new, verified galaxy/system interaction build is the next delivery gate; backend module completion must not postpone it. The full beta scope below remains mandatory.

| Area | Delivery requirement | Current evidence / gap | Owner |
|---|---|---|---|
| Campaign | Five empires, deterministic generation, exploration, expansion, conclusion | Independent long runs:14/15 passed determinism/validation; seed26713/player4/turn171 group-supply blocker. Player-driven victory verification outstanding | Gemini campaign; Sol review |
| Systems | Planets, terraforming, workforce, growth, morale, energy, construction | Planet data extracted and header independently compiled; campaign integration, Buy/Scrap and source work rates under correction | Gemini campaign |
| Research/content | Six fields, prerequisites, functional faction unlocks | Reference catalogue present; many bonuses inactive | Gemini campaign |
| Diplomacy/intelligence | Major/minor relations, treaties, trade, membership, espionage | Standalone modules built; independent tests found payment, membership, parser and formula defects. Repairs and campaign integration pending | Gemini builders; Sol/Luna review |
| Fleets/logistics | Grouping, movement, supply, repairs, roles | Task-force backend and UI implemented; mixed movement/consumption/retreat regressions under correction | Gemini campaign; Claude UI |
| Tactical combat | Player-controlled rounds, targeting, maneuvers, retreat, consequences | Six independent session scenarios passed on recorded unchanged core hashes; live packaged pointer/UI flow unverified | Gemini tactical; Astra integration; Sol tests |
| Interface | Original-derived screen architecture and workflows, modern usability | Source revisions unverified; current right-click only screen selector, no map double-click, persistent command-column composition still needs redesign. Old package rejected by Simon | Claude UI; Sol review |
| Presentation | Representative faction art, weapons, shields, credits | Provisional art; oversized stars | Astra integration; Luna provenance |
| Persistence | Compatible saves, recovery, settings and exit | Prior schema10 checkpoint and shell pass; schema11 and tactical save envelope implemented, integration tests pending | Builders preserve; reviewers test |
| Delivery | Packaged game, guide, regression evidence, known gaps | Prior CommandPreview retained as checkpoint | Astra |

No row is complete merely because code exists. Acceptance requires independent live-file tests and packaged visual checks where relevant. Known original behavior, deliberate modernization, uncertain reconstruction and incomplete implementation are separate claims.

Ownership: Gemini campaign exclusively owns Simulation.h/.cpp and SimulationTests.cpp; Gemini tactical exclusively owns new TacticalCombat.h and TacticalCombatTests.cpp. Claude exclusively owns game-mode/settings UI and UI helper files. Astra owns integration/build scripts and acceptance records. Reviewers read production, write only isolated review artifacts. Interface extensions are communicated before integration; no concurrent writes to shared files.

Current independent gates: standalone planet header passed MSVC /W4 /WX; diplomacy corrections remain unverified after interrupted final pass; intelligence14suites and malformed-parse atomicity independently passed, with extreme runtime input bounds required in adapter. These are module results, not evidence of a completed campaign. The default packaged CommandPreview remains the earlier checkpoint until a new package passes integration and visual checks.

## Standalone beta distribution — Simon's explicit requirement

Each beta milestone must include a self-contained Windows package and ZIP that a reviewer can extract and launch without Unreal Editor or the source tree. Include a clear EXE entry point, build identity, credits, controls, known limitations and any prerequisite instructions. Exclude personal saves, logs, developer debug symbols and project source. Verify archive integrity and the packaged executable; do not substitute an older preview without stating it. Editor-only execution is not completion of this delivery gate.

