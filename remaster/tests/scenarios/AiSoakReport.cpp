// F5 balance evidence (20 Nov 2026): AI-vs-AI soaks on every registered preset. Not a test suite (no add_test): a report.
// F5 round 2: observer mode (GameState::observerMode) by default, so the AI plays the player's seat too and a campaign runs
// to a victory of any type; BOTF_SOAK_PASSIVE=1 restores round 1's passive player seat (no orders), where the rival AIs decide the campaign; every turn it reads victoryProgress for every
// empire still standing and records, per empire and condition, the best percent and the first turn it reached 100 or was
// achieved. A campaign stops at its outcome or the turn cap.
//   BotfAiSoak [turns=300] [seeds=6] [preset-id ...]
// Output: one "GAME" line per campaign, one "EMP" line per empire, then a "PRESET" summary per preset. Rules are untouched:
// this reads the core, it changes nothing.
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "GameplayInternal.h"
#include "Presets.h"
#include "Simulation.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
using namespace botf;

struct EmpireTrack {
    int id = 0;
    std::string name;
    int best[5] = {0, 0, 0, 0, 0};
    int first[5] = {0, 0, 0, 0, 0};
    int colonies = 0, peakColonies = 0, eliminatedTurn = 0;
    // P2 breakdown (BOTF_SOAK_GAINS): how the empire's colonies came, and its economy at turn 100.
    std::array<int, 14> queueFront{}; std::map<int, int> structureFront; int warTurns = 0, warsDeclaredOn = 0, settled = 0, joined = 0, taken = 0, lost = 0, colonyShipsSeen = 0, members100 = 0, colonies100 = 0, pop100 = 0;
    long long credits100 = 0;
};

const char* condName(int c) {
    static const char* n[5] = {"conquest", "domination", "scientific", "diplomatic", "vendetta"};
    return n[c];
}
}

int main(int argc, char** argv) {
    const int turns = argc > 1 ? std::atoi(argv[1]) : 300;
    const int seeds = argc > 2 ? std::atoi(argv[2]) : 6;
    std::vector<std::string> only;
    for (int i = 3; i < argc; ++i) only.emplace_back(argv[i]);
    for (const auto* p : presets::all()) {
        if (!only.empty() && std::find(only.begin(), only.end(), p->id) == only.end()) continue;
        if (!p->lineupPlayable && !p->lineup.empty()) {
            std::cout << "SKIP " << p->id << ": lineup not playable (" << p->lineupNote << ")" << std::endl;
            continue;
        }
        std::map<std::string, int> endings, winsBy;
        std::map<std::string, std::vector<int>> firstReach;   // "<empire> <condition>" -> first turns
        std::map<std::string, int> appearances;
        int games = 0, totalTurns = 0, capped = 0, playerOut = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < seeds; ++k) {
            CampaignSetup setup;
            setup.seed = 7000 + 131 * static_cast<std::uint64_t>(k);
            if (const char* pick = std::getenv("BOTF_SOAK_SEED"); pick && std::to_string(setup.seed) != pick) continue;
            setup.presetId = p->id;
            if (p->rulesBase == "classic") setup.preset = RulesetPreset::Classic;
            // The preset's new-game defaults, as the New campaign screen fills them (newCampaign itself applies only the
            // overlays and rule flags). Sizes are NewCampaignVM.SIZES.
            for (const auto& race : p->lineup) {
                std::string key = race;
                std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                setup.lineup.push_back(key);
            }
            if (setup.lineup.empty() && p->empires > 0) setup.empires = p->empires;
            if (p->era > 0) setup.era = static_cast<StartingEra>(p->era);
            const std::map<std::string, std::array<int, 3>> sizes{{"small", {16, 13, 30}}, {"medium", {22, 16, 50}}, {"large", {28, 20, 80}}, {"huge", {34, 24, 120}}};
            // Round 4 (22 Nov 2026): BOTF_SOAK_SIZE=<small|medium|large|huge> overrides the preset's galaxy size.
            const char* sizeEnv = std::getenv("BOTF_SOAK_SIZE");
            const auto sz = sizes.find(sizeEnv ? std::string(sizeEnv) : (p->size.empty() ? std::string("medium") : p->size));
            if (sz != sizes.end()) { setup.width = sz->second[0]; setup.height = sz->second[1]; setup.systems = sz->second[2]; }
            // Round 4: BOTF_SOAK_PLAYER=<empire id> seats the observer AI on that empire (2 = Klingon, the aggressive profile).
            if (const char* seat = std::getenv("BOTF_SOAK_PLAYER")) { setup.playerEmpire = std::atoi(seat); setup.playerSet = true; }
            if (!p->shape.empty()) parseGalaxyShape(p->shape, setup.galaxy.shape);
            if (!p->minorDensity.empty()) parseMinorDensity(p->minorDensity, setup.galaxy.density);
            if (!p->difficulty.empty()) parseDifficulty(p->difficulty, setup.galaxy.difficulty);
            if (const char* dEnv = std::getenv("BOTF_SOAK_DIFF")) parseDifficulty(dEnv, setup.galaxy.difficulty);   // round 4: e.g. easy, simple
            if (p->startSystems >= 0) setup.startSystems = p->startSystems;
            setup.planetsMin = p->planetsMin; setup.planetsMax = p->planetsMax;
            if (p->starTypes >= 0) setup.starTypes = p->starTypes;
            if (p->threats) setup.threats = p->threats;
            GameState state;
            try {
                state = newCampaign(setup);
            } catch (const std::exception& e) {
                std::cout << "FAIL " << p->id << " seed " << setup.seed << ": newCampaign: " << e.what() << std::endl;
                continue;
            }
            state.observerMode = !std::getenv("BOTF_SOAK_PASSIVE");
            // C4 (20 Nov 2026): BOTF_SOAK_PLAYON=1 plays on past domination (as a winner who chose "Continue playing"), so a
            // campaign ends only by conquest or another victory type.
            if (std::getenv("BOTF_SOAK_PLAYON")) state.playingOnFrom = 1;
            // C7 (21 Nov 2026): BOTF_SOAK_NODOM=1 plays with the New Campaign domination setting off (conquest is the military win).
            if (std::getenv("BOTF_SOAK_NODOM")) state.dominationEnabled = false;
            std::map<int, EmpireTrack> track;
            for (int id : internal::activeEmpires(state)) { track[id].id = id; track[id].name = empireName(id); }
            std::string ending;
            double slowestTurn = 0;
            const bool gains = std::getenv("BOTF_SOAK_GAINS") != nullptr;
            const int snapTurn = std::getenv("BOTF_SOAK_SNAP") ? std::atoi(std::getenv("BOTF_SOAK_SNAP")) : 100;
            std::map<int, int> ownerAt;   // system id -> colony owner last turn
            std::set<int> shipsSeen;
            std::map<int, int> ownerPrev, ownerNow; std::map<std::pair<int, int>, int> captured, pairWarTurns; long long parkedTransportTurns = 0;
            for (const auto& c : state.colonies) ownerPrev[c.systemId] = c.ownerId;
            // Round 4 (22 Nov 2026): always on. Worlds that change hands between empires, and the longest run of turns with no
            // war between any two surviving majors (AI only in observer mode).
            std::map<int, int> churnPrev; for (const auto& c : state.colonies) churnPrev[c.systemId] = c.ownerId;
            long long handsChanged = 0; int peaceRun = 0, longestPeace = 0, longestPeaceEnd = 0;
            for (const auto& c : state.colonies) ownerAt[c.systemId] = c.ownerId;
            try {
                for (int t = 0; t < turns && state.outcome == CampaignOutcome::Ongoing; ++t) {
                    if (const char* dump = std::getenv("BOTF_SOAK_DUMP_TURN"); dump && std::atoi(dump) == state.turn) {
                        std::ofstream(std::string("soak-") + p->id + "-" + std::to_string(setup.seed) + "-turn" + dump + ".sav", std::ios::binary) << serialize(state);
                    }
                    if (std::getenv("BOTF_SOAK_TRACE")) std::cout << "TURN " << state.turn << " fleets " << state.fleets.size() << " colonies " << state.colonies.size() << std::endl;
                    const auto tg = std::chrono::steady_clock::now();
                    const auto report = advance(state);
                    // Runaway guard (dominion-war seed 7655 grew to 10 GB in the first run): a turn over 20 s, or a campaign
                    // with an absurd fleet count, is recorded with its sizes and the campaign stops there.
                    const double turnSecs = std::chrono::duration<double>(std::chrono::steady_clock::now() - tg).count();
                    slowestTurn = std::max(slowestTurn, turnSecs);
                    if (turnSecs > 20.0 || state.fleets.size() > 20000) {
                        std::cout << "RUNAWAY " << p->id << " seed " << setup.seed << " turn " << state.turn << ": " << turnSecs << " s, fleets "
                                  << state.fleets.size() << ", colonies " << state.colonies.size() << ", stations "
                                  << state.stations.size() << ", events this turn " << report.events.size() << std::endl;
                        throw std::runtime_error("runaway");
                    }
                    {
                        std::map<int, int> nowOwners; for (const auto& c : state.colonies) nowOwners[c.systemId] = c.ownerId;
                        for (const auto& [sys, owner] : nowOwners) { auto w = churnPrev.find(sys); if (w != churnPrev.end() && w->second != owner && w->second > 0 && owner > 0) ++handsChanged; }
                        std::set<int> alive; for (const auto& [sys, owner] : nowOwners) alive.insert(owner);
                        churnPrev = std::move(nowOwners);
                        const bool anyWar = std::any_of(state.majorRelations.begin(), state.majorRelations.end(), [&](const MajorRelation& r) {
                            return r.atWar && alive.count(r.first) && alive.count(r.second); });
                        peaceRun = anyWar ? 0 : peaceRun + 1;
                        if (peaceRun > longestPeace) { longestPeace = peaceRun; longestPeaceEnd = state.turn; }
                    }
                    if (gains) {
                        std::map<int, int> now;
                        for (const auto& c : state.colonies) now[c.systemId] = c.ownerId;
                        for (const auto& [sys, owner] : now) {
                            const auto was = ownerAt.find(sys);
                            const bool minorWorld = std::any_of(state.minors.begin(), state.minors.end(), [&](const Minor& m) { return m.systemId == sys; });
                            if (was == ownerAt.end()) { if (track.count(owner)) ++(minorWorld ? track[owner].joined : track[owner].settled); }
                            else if (was->second != owner) { if (track.count(owner)) ++track[owner].taken; if (track.count(was->second)) ++track[was->second].lost; }
                        }
                        for (const auto& [sys, owner] : ownerAt) if (!now.count(sys) && track.count(owner)) ++track[owner].lost;
                        ownerAt = std::move(now);
                        for (const auto& f : state.fleets)
                            if (f.colonyShip && track.count(f.ownerId) && shipsSeen.insert(f.id).second) ++track[f.ownerId].colonyShipsSeen;
                        for (auto& [id, tr] : track)
                            if (std::any_of(state.majorRelations.begin(), state.majorRelations.end(), [&, id = id](const MajorRelation& r) {
                                    return r.atWar && (r.first == id || r.second == id); })) ++tr.warTurns;
                        for (const auto& c : state.colonies)
                            if (track.count(c.ownerId) && !c.queue.empty()) {
                                ++track[c.ownerId].queueFront[std::clamp(static_cast<int>(c.queue.front().product), 0, 13)];
                                if (c.queue.front().product == Building::Structure) ++track[c.ownerId].structureFront[c.queue.front().referenceId];
                            }
                        if (state.turn == snapTurn)
                            for (auto& [id, tr] : track) {
                                tr.credits100 = empire(state, id).credits;
                                for (const auto& c : state.colonies) if (c.ownerId == id) { ++tr.colonies100; tr.pop100 += c.population; }
                                for (const auto& m : state.minors) tr.members100 += m.memberOf == id;
                            }
                    }
                    // Conquest diagnosis (21 Nov 2026, BOTF_SOAK_WARS=<every n turns>): who is left, how strong, and what stands
                    // between each surviving pair (war and since when, active treaties, worlds taken from each other so far).
                    if (const char* wars = std::getenv("BOTF_SOAK_WARS")) {
                        for (const auto& c : state.colonies) ownerNow[c.systemId] = c.ownerId;
                        for (const auto& [sys, owner] : ownerNow) {
                            const auto was = ownerPrev.find(sys);
                            if (was != ownerPrev.end() && was->second != owner && was->second > 0 && owner > 0) ++captured[{owner, was->second}];
                        }
                        ownerPrev = ownerNow; ownerNow.clear();
                        for (const auto& r : state.majorRelations) if (r.atWar) ++pairWarTurns[{std::min(r.first, r.second), std::max(r.first, r.second)}];
                        // Transports idle at an enemy colony they do not assault (the stuck-transport bug), counted per turn.
                        for (const auto& f : state.fleets) {
                            if (f.shipClass != Building::Transport || f.hull <= 0 || f.invadeSystemId || f.x != f.targetX || f.y != f.targetY) continue;
                            for (const auto& c : state.colonies) {
                                const auto* sys = gameplay::systemById(state, c.systemId);
                                if (!sys || sys->x != f.x || sys->y != f.y || c.ownerId == f.ownerId) continue;
                                const bool war = std::any_of(state.majorRelations.begin(), state.majorRelations.end(), [&](const MajorRelation& r) {
                                    return r.atWar && ((r.first == f.ownerId && r.second == c.ownerId) || (r.second == f.ownerId && r.first == c.ownerId)); });
                                if (war) ++parkedTransportTurns;
                            }
                        }
                        const int every = std::max(1, std::atoi(wars));
                        if (state.turn % every == 0 || state.outcome != CampaignOutcome::Ongoing) {
                            std::map<int, int> cols, fleets;
                            for (const auto& c : state.colonies) ++cols[c.ownerId];
                            for (const auto& f : state.fleets) { ++fleets[f.ownerId]; }
                            std::cout << "WARS " << p->id << " seed " << setup.seed << " turn " << state.turn << " systems " << state.systems.size() << " colonies " << state.colonies.size()
                                      << " parked-transport-turns " << parkedTransportTurns << " |";
                            for (const auto& [id, n] : cols) std::cout << " " << empireName(id) << " c" << n << " f" << fleets[id];
                            std::cout << std::endl;
                            // Stuck sites (21 Nov 2026): every hostile world where an empire's transports sit at war without
                            // landing: how many, their troops as the AI reckons them, the world's real ground defence, and what
                            // guards it (a station or armed ships of its owner block any landing; enemy warships make the AI wait).
                            std::map<std::pair<int, int>, std::array<int, 2>> stuck;   // {owner, system} -> {transports, troops}
                            for (const auto& f : state.fleets) {
                                if (f.shipClass != Building::Transport || f.hull <= 0 || f.invadeSystemId) continue;
                                const auto* c = colonyAt(state, [&] { for (const auto& sy : state.systems) if (sy.x == f.x && sy.y == f.y) return sy.id; return -1; }());
                                if (!c || c->ownerId == f.ownerId || !combatAllowed(state, f.ownerId, c->ownerId)) continue;
                                auto& e = stuck[{f.ownerId, c->systemId}];
                                ++e[0];
                                e[1] += 8 + f.hull + 2 * empire(state, f.ownerId).technologies[5].level;
                            }
                            for (const auto& [site, v] : stuck) {
                                const auto* c = colonyAt(state, site.second);
                                const auto* sy = gameplay::systemById(state, site.second);
                                const int bat = state.advancedRulesEnabled ? economy(state, *c).poweredBatteries : 0;
                                const int aiDef = (8 + c->population / InvasionPopDefenseDivisor + c->morale / 10 + bat * 12 * content::balance().batteryDefensePercent / 100) * content::balance().shieldDefensePercent / 100;
                                int guards = 0, foes = 0;
                                for (const auto& o : state.fleets) if (o.hull > 0 && o.x == sy->x && o.y == sy->y && fleetAttack(o) > 0 && o.shipClass != Building::Transport) {
                                    if (o.ownerId == c->ownerId) ++guards;
                                    else if (combatAllowed(state, site.first, o.ownerId)) ++foes;
                                }
                                int own = 0;
                                for (const auto& o : state.fleets) if (o.hull > 0 && o.x == sy->x && o.y == sy->y && o.ownerId == site.first && fleetAttack(o) > 0 && o.shipClass != Building::Transport) ++own;
                                const auto* st = stationAt(state, sy->x, sy->y);
                                std::cout << "  STUCK " << empireName(site.first) << " at " << sy->name << " (" << empireName(c->ownerId) << " pop " << c->population
                                          << " morale " << c->morale << " bat " << bat << "): transports " << v[0] << " troops " << v[1] << " ai-def " << aiDef
                                          << " station " << (st && st->hull > 0 ? (st->ownerId == c->ownerId ? "theirs" : "other") : "none")
                                          << " their-armed " << guards << " other-armed " << foes << " our-armed " << own << std::endl;
                            }
                            // Last worlds (21 Nov 2026): an empire down to two colonies, each world's defence, and what each enemy
                            // at war with it has: transports (troops, at the world, heading there), warships, task forces.
                            for (const auto& [id, n] : cols) {
                                if (n > 2 || id <= 0) continue;
                                for (const auto& c : state.colonies) {
                                    if (c.ownerId != id) continue;
                                    const auto* sy = gameplay::systemById(state, c.systemId);
                                    const auto* st = stationAt(state, sy->x, sy->y);
                                    int guards = 0;
                                    for (const auto& o : state.fleets) if (o.hull > 0 && o.x == sy->x && o.y == sy->y && o.ownerId == id && fleetAttack(o) > 0) ++guards;
                                    std::cout << "  LAST " << empireName(id) << " " << sy->name << " pop " << c.population << " morale " << c.morale
                                              << " station " << (st && st->hull > 0 ? std::to_string(st->hull) + (st->ownerId == id ? " own" : " other") : std::string("none"))
                                              << " guards " << guards << " |";
                                    for (const auto& r : state.majorRelations) {
                                        if (!r.atWar || (r.first != id && r.second != id)) continue;
                                        const int foe = r.first == id ? r.second : r.first;
                                        int tr = 0, troops = 0, here = 0, heading = 0, war = 0, tfs = 0;
                                        for (const auto& f : state.fleets) {
                                            if (f.ownerId != foe || f.hull <= 0) continue;
                                            if (f.shipClass == Building::Transport) {
                                                ++tr; troops += 8 + f.hull + 2 * empire(state, foe).technologies[5].level;
                                                if (f.x == sy->x && f.y == sy->y) ++here;
                                                else if (f.targetX == sy->x && f.targetY == sy->y) ++heading;
                                            } else if (fleetAttack(f) > 0) ++war;
                                        }
                                        for (const auto& tf : state.taskForces) if (tf.ownerId == foe) ++tfs;
                                        std::cout << " " << empireName(foe) << " transports " << tr << " (troops " << troops << ", here " << here << ", heading " << heading
                                                  << ") warships " << war << " taskforces " << tfs << ";";
                                    }
                                    std::cout << std::endl;
                                }
                            }
                            const auto& dip = campaignDiplomacyState(state);
                            for (auto a = cols.begin(); a != cols.end(); ++a)
                                for (auto b = std::next(a); b != cols.end(); ++b) {
                                    const MajorRelation* rel = nullptr;
                                    for (const auto& r : state.majorRelations)
                                        if ((r.first == a->first && r.second == b->first) || (r.first == b->first && r.second == a->first)) rel = &r;
                                    std::cout << "  PAIR " << empireName(a->first) << " / " << empireName(b->first) << ": "
                                              << (rel ? (rel->atWar ? "WAR since " + std::to_string(rel->warStartTurn) : std::string(rel->contact ? "peace" : "no-contact")) : std::string("no-relation"))
                                              << " war-turns " << pairWarTurns[{a->first, b->first}] << " taken " << captured[{a->first, b->first}] << "/" << captured[{b->first, a->first}] << " treaties";
                                    for (const auto& tr8 : dip.activeTreaties)
                                        if (tr8.status == diplomacy::TreatyStatus::Active && !tr8.partyA.isMinor && !tr8.partyB.isMinor &&
                                            ((tr8.partyA.id == a->first && tr8.partyB.id == b->first) || (tr8.partyA.id == b->first && tr8.partyB.id == a->first)))
                                            std::cout << " " << static_cast<int>(tr8.type) << "@" << tr8.startTurn;
                                    std::cout << std::endl;
                                }
                        }
                    }
                    if (state.turn % 50 == 0) std::cout << "TICK " << p->id << " seed " << setup.seed << " turn " << state.turn << " fleets " << state.fleets.size() << std::endl;
                    for (auto& [id, tr] : track) {
                        int n = 0;
                        for (const auto& c : state.colonies) n += c.ownerId == id;
                        if (n > 0 && tr.colonies == 0 && tr.peakColonies > 0 && std::getenv("BOTF_SOAK_WARS")) {
                            // 21 Nov: an empire that had lost every world holds one again; which world, and how.
                            std::cout << "REVIVE " << p->id << " seed " << setup.seed << " turn " << state.turn << " " << tr.name << ":";
                            for (const auto& c : state.colonies) if (c.ownerId == id) {
                                const bool minorWorld = std::any_of(state.minors.begin(), state.minors.end(), [&](const Minor& m) { return m.systemId == c.systemId; });
                                std::cout << " " << gameplay::systemById(state, c.systemId)->name << (minorWorld ? " (minor world)" : "") << " pop " << c.population;
                            }
                            int fl = 0, cs = 0, tp = 0;
                            for (const auto& f : state.fleets) if (f.ownerId == id) { ++fl; cs += f.colonyShip; tp += f.shipClass == Building::Transport; }
                            std::cout << " | fleets " << fl << " colony ships " << cs << " transports " << tp << std::endl;
                        }
                        tr.colonies = n;
                        tr.peakColonies = std::max(tr.peakColonies, n);
                        if (!n) { if (!tr.eliminatedTurn) tr.eliminatedTurn = state.turn; continue; }
                        for (const auto& v : victoryProgress(state, id)) {
                            const int c = static_cast<int>(v.condition);
                            if (c < 0 || c > 4 || !v.enabled) continue;
                            tr.best[c] = std::max(tr.best[c], v.percent);
                            if (!tr.first[c] && (v.achieved || v.percent >= 100)) tr.first[c] = state.turn;
                        }
                    }
                    if (state.outcome != CampaignOutcome::Ongoing)
                        for (const auto& line : report.events)
                            if (ending.empty() && (line.find("victory") != std::string::npos || line.find("defeat") != std::string::npos)) ending = line;
                }
            } catch (const std::exception& e) {
                std::cout << "FAIL " << p->id << " seed " << setup.seed << " turn " << state.turn << ": " << e.what() << std::endl;
                continue;
            }
            ++games;
            totalTurns += state.turn;
            std::string kind = "cap";
            if (state.outcome == CampaignOutcome::Ongoing) ++capped;
            else if (ending.find("Vendetta") != std::string::npos) kind = "vendetta";
            else if (ending.find("scientific") != std::string::npos) kind = "scientific";
            else if (ending.find("diplomatic") != std::string::npos) kind = "diplomatic";
            else if (ending.find("conquest victory") != std::string::npos) kind = "conquest";
            else if (ending.find("domination victory") != std::string::npos) kind = "domination";
            else if (state.outcome == CampaignOutcome::Defeat && empireEliminated(state, state.playerEmpire)) { kind = "player-eliminated"; ++playerOut; }
            else kind = "conquest/domination";
            ++endings[kind];
            const std::string winner = state.winnerEmpire ? empireName(state.winnerEmpire) : "-";
            if (state.winnerEmpire && kind != "player-eliminated") ++winsBy[winner];
            std::cout << "CHURN " << p->id << " seed " << setup.seed << " turns " << state.turn << " hands-changed " << handsChanged << " per-100 "
                      << (state.turn > 0 ? 100.0 * handsChanged / state.turn : 0.0) << " longest-no-war " << longestPeace << " ending-turn " << longestPeaceEnd << std::endl;
            std::cout << "GAME " << p->id << " seed " << setup.seed << " turns " << state.turn << " end " << kind << " winner " << winner << " slowest-turn " << slowestTurn << "s"
                      << (ending.empty() ? "" : " | " + ending) << std::endl;
            for (const auto& [id, tr] : track) {
                ++appearances[tr.name];
                std::cout << "EMP " << p->id << " seed " << setup.seed << " " << tr.name << (id == state.playerEmpire ? (state.observerMode ? " (observer seat)" : " (passive)") : "")
                          << " colonies " << tr.colonies << " peak " << tr.peakColonies
                          << (tr.eliminatedTurn ? " out@" + std::to_string(tr.eliminatedTurn) : std::string());
                for (int c = 0; c < 5; ++c) {
                    std::cout << " " << condName(c) << " " << tr.best[c] << "%" << (tr.first[c] ? "@" + std::to_string(tr.first[c]) : std::string());
                    if (tr.first[c] && (id != state.playerEmpire || state.observerMode)) firstReach[tr.name + " " + condName(c)].push_back(tr.first[c]);
                }
                std::cout << std::endl;
                if (gains)
                    std::cout << "GAINS " << p->id << " seed " << setup.seed << " " << tr.name << " settled " << tr.settled << " joined " << tr.joined << " taken " << tr.taken
                              << " lost " << tr.lost << " war-turns " << tr.warTurns << " colony-ships " << tr.colonyShipsSeen << " t" << snapTurn << ": colonies " << tr.colonies100 << " pop "
                              << tr.pop100 << " credits " << tr.credits100 << " member-minors " << tr.members100 << " queue-front";
                    for (int b = 0; b < 14; ++b) std::cout << ' ' << tr.queueFront[b];
                    std::cout << " structure-refs";
                    for (const auto& [ref, n] : tr.structureFront) std::cout << ' ' << ref << ':' << n;
                    std::cout << std::endl;
            }
        }
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::cout << "PRESET " << p->id << ": " << games << " games, avg " << (games ? totalTurns / games : 0) << " turns, " << secs << " s; endings";
        for (const auto& [k, n] : endings) std::cout << " " << k << "=" << n;
        std::cout << "; wins";
        for (const auto& [k, n] : winsBy) std::cout << " " << k << "=" << n;
        std::cout << "; AI first reached 100%";
        for (const auto& [k, v] : firstReach) {
            auto s = v;
            std::sort(s.begin(), s.end());
            std::cout << " [" << k << " " << s.size() << "/" << appearances[k.substr(0, k.rfind(' '))] << " median t" << s[s.size() / 2] << "]";
        }
        std::cout << std::endl;
    }
    return 0;
}
