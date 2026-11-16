// AI war and early-food proof (PLAYTEST-FEEDBACK 16 Nov 2026). Two problems from the browser playtest:
//  1. No war: every AI sat on a non-aggression pact. Each shipped preset is played against a player who ACCEPTS every pact an AI offers
//     (the friend's play, the worst case for war) on the browser's default galaxy for 10 seeds; the turn of the first war on the player is
//     reported and must fall in the window the feedback asked for (turn 30-60), and not every AI may be friendly at turn 40.
//  2. Early food trap: a Growth-governor home system must keep a minimum share of workers on industry for its first 100 turns, and the
//     starting colony must always be able to afford to build something on any governor.
// BOTF_SEEDS=N overrides the seed count (10 for the war half, at most 4 for the food half), BOTF_PRESET=id limits the presets, argument "war" or "food" runs one half.
#define _CRT_SECURE_NO_WARNINGS
#include "Content.h"
#include "Presets.h"
#include "Simulation.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace botf;

CampaignSetup browserDefault(const std::string& presetId, std::uint64_t seed) {
    CampaignSetup setup;
    setup.seed = seed; setup.empires = 5; setup.width = 22; setup.height = 16; setup.systems = 50;   // Medium, the browser default
    setup.galaxy.shape = GalaxyShape::Spiral; setup.galaxy.density = MinorDensity::Some; setup.galaxy.difficulty = Difficulty::Normal;
    setup.presetId = presetId;
    return setup;
}

void governors(GameState& s, GovernorPriority work, GovernorPriority queue) {
    std::vector<int> mine;
    for (const auto& c : s.colonies) if (c.ownerId == s.playerEmpire && (c.workforcePriority != work || c.queuePriority != queue)) mine.push_back(c.systemId);
    for (int sys : mine) submit(s, makeSetColonyGovernorCommand(s.lastAcceptedCommand + 1, sys, work, queue));
}


// A player who plays the early game the way the friend did: explores with every idle ship, settles the nearest charted free system with a
// colony ship, builds with the colony governors, and accepts every offer. Refused orders are simply skipped.
void explorerOrders(GameState& s) {
    struct Cand { int cell, score; };
    const std::vector<Fleet> fleets = s.fleets;
    std::vector<int> claimed;
    for (const auto& f : fleets) {
        if (f.ownerId != s.playerEmpire || f.hull <= 0) continue;
        const auto submitMove = [&](int x, int y) {
            Command c; c.id = s.lastAcceptedCommand + 1; c.type = CommandType::MoveFleet; c.fleetId = f.id; c.targetX = x; c.targetY = y;
            try { submit(s, c); return true; } catch (const std::exception&) { return false; }
        };
        if (f.colonyShip) {
            const auto here = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.x == f.x && x.y == f.y; });
            if (here != s.systems.end() && !colonyAt(s, here->id) && f.x == f.targetX && f.y == f.targetY && f.colonize == false) {
                Command c; c.id = s.lastAcceptedCommand + 1; c.type = CommandType::Colonize; c.fleetId = f.id;
                try { submit(s, c); continue; } catch (const std::exception&) {}
            }
            if (f.x != f.targetX || f.y != f.targetY) continue;
            const System* best = nullptr; std::size_t bestLen = 1u << 30;
            for (const auto& sys : s.systems) {
                if (!explored(s, sys.x, sys.y) || colonyAt(s, sys.id) || (sys.x == f.x && sys.y == f.y && false)) continue;
                if (std::find(claimed.begin(), claimed.end(), sys.id) != claimed.end()) continue;
                bool minor = false; for (const auto& m : s.minors) if (m.systemId == sys.id) minor = true;
                if (minor) continue;
                const auto path = route(s, f, sys.x, sys.y);
                if (path.empty() && !(sys.x == f.x && sys.y == f.y)) continue;
                if (path.size() < bestLen) { bestLen = path.size(); best = &sys; }
            }
            if (best) { claimed.push_back(best->id); if (best->x == f.x && best->y == f.y) { Command c; c.id = s.lastAcceptedCommand + 1; c.type = CommandType::Colonize; c.fleetId = f.id; try { submit(s, c); } catch (const std::exception&) {} } else submitMove(best->x, best->y); }
            continue;
        }
        if (f.x != f.targetX || f.y != f.targetY) continue;
        std::vector<Cand> cands;
        for (int y = 0; y < s.height; ++y) for (int x = 0; x < s.width; ++x) {
            if (!supplied(s, f, x, y)) continue;
            int unknown = 0;
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
                if (x + dx >= 0 && y + dy >= 0 && x + dx < s.width && y + dy < s.height && !explored(s, x + dx, y + dy)) ++unknown;
            if (unknown) cands.push_back({y * s.width + x, unknown * 100 - std::max(std::abs(f.x - x), std::abs(f.y - y))});
        }
        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.score == b.score ? a.cell < b.cell : a.score > b.score; });
        int tries = 0;
        for (const auto& c : cands) { if (++tries > 12) break; if (submitMove(c.cell % s.width, c.cell / s.width)) break; }
    }
}

struct WarRun { int firstWar = -1, firstWarBy = 0, friendsAt40 = 0, aisAt40 = 0, firstContact = -1, endedTurn = -1, firstBattle = -1; };  // endedTurn: the campaign ended first (a threat took the player)

// The friend's player: accepts every offer, proposes nothing, runs balanced governors.
WarRun playWar(const std::string& presetId, std::uint64_t seed, int turns) {
    auto s = newCampaign(browserDefault(presetId, seed));
    WarRun out;
    for (int t = 0; t < turns && s.outcome == CampaignOutcome::Ongoing; ++t) {
        governors(s, GovernorPriority::Balanced, GovernorPriority::Balanced);
        explorerOrders(s);
        for (const auto& p : incomingProposals(s, s.playerEmpire))
            submit(s, makeRespondProposalCommand(s.lastAcceptedCommand + 1, p.id, true));
        const auto rep = advance(s);
        for (const auto& b : rep.battles) if ((b.firstOwner == s.playerEmpire || b.secondOwner == s.playerEmpire) && out.firstBattle < 0) out.firstBattle = s.turn;
        for (const auto& r : s.majorRelations) {
            if (r.first != s.playerEmpire && r.second != s.playerEmpire) continue;
            const int other = r.first == s.playerEmpire ? r.second : r.first;
            if (r.contact && out.firstContact < 0) out.firstContact = s.turn;
            if (r.atWar && out.firstWar < 0) { out.firstWar = s.turn; out.firstWarBy = other; }
        }
        if (s.turn == 40) {
            const auto self = diplomacy::ParticipantId::major(s.playerEmpire);
            for (const auto& r : s.majorRelations) {
                if (r.first != s.playerEmpire && r.second != s.playerEmpire) continue;
                const int other = r.first == s.playerEmpire ? r.second : r.first;
                ++out.aisAt40;
                if (s.diplomacyEngineState.highestTreatyRank(self, diplomacy::ParticipantId::major(other)) > 0) ++out.friendsAt40;
            }
        }
        if (out.firstWar >= 0 && s.turn >= 40 && (out.firstBattle >= 0 || s.turn >= 100)) break;
    }
    if (s.outcome != CampaignOutcome::Ongoing && out.firstWar < 0) out.endedTurn = s.turn;
    return out;
}

// Home system under one governor: the industry workers, every turn of the first `turns` turns.
struct FoodRun { int minIndustry = 1 << 30, minIndustryTurn = 0, turnsAtZero = 0, popStart = 0, popEnd = 0, turnsAllFood = 0, stalledTurns = 0; int minSharePct = 100; };
FoodRun playFood(const std::string& presetId, std::uint64_t seed, int turns, GovernorPriority work) {
    auto s = newCampaign(browserDefault(presetId, seed));
    FoodRun out;
    for (int t = 0; t < turns && s.outcome == CampaignOutcome::Ongoing; ++t) {
        governors(s, work, GovernorPriority::Balanced);
        for (const auto& p : incomingProposals(s, s.playerEmpire)) submit(s, makeRespondProposalCommand(s.lastAcceptedCommand + 1, p.id, true));
        advance(s);
        for (const auto& c : s.colonies) {
            if (c.ownerId != s.playerEmpire) continue;
            if (out.popStart == 0) out.popStart = c.population;
            out.popEnd = c.population;
            if (c.population < 20) continue;   // a colony a threat has bombarded to nothing is not a governor question
            if (c.industryWorkers < out.minIndustry) { out.minIndustry = c.industryWorkers; out.minIndustryTurn = s.turn; }
            if (c.industryWorkers == 0 && c.factories > 0) ++out.turnsAtZero;
            if (c.industryWorkers == 0 && !c.queue.empty()) ++out.stalledTurns;
            if (c.factories > 0) out.minSharePct = std::min(out.minSharePct, c.industryWorkers * 100 / c.population);
            if (c.foodWorkers >= c.population) ++out.turnsAllFood;
            if (std::getenv("BOTF_FOODTRACE") && c.systemId == s.colonies.front().systemId && s.turn % 5 == 0) std::cout << "  t" << s.turn << " sys" << c.systemId << " pop " << c.population << " food " << c.foodWorkers << " ind " << c.industryWorkers << " res " << c.researchWorkers << " farms " << c.farms << " fac " << c.factories << " labs " << c.laboratories << " q" << c.queue.size() << std::endl;
        }
    }
    return out;
}

// Diagnostic: `AiWarFoodTests probe <preset> <seedIndex>` prints every AI's standing towards the player every 5 turns.
void probe(const std::string& presetId, int k) {
    auto s = newCampaign(browserDefault(presetId, 500 + 37ULL * static_cast<std::uint64_t>(k)));
    for (int t = 0; t < 100 && s.outcome == CampaignOutcome::Ongoing; ++t) {
        governors(s, GovernorPriority::Balanced, GovernorPriority::Balanced);
        explorerOrders(s);
        for (const auto& p : incomingProposals(s, s.playerEmpire)) submit(s, makeRespondProposalCommand(s.lastAcceptedCommand + 1, p.id, true));
        const auto rep = advance(s);
        for (const auto& e : rep.events) if (std::getenv("BOTF_ALLEV") || e.find("iplomacy") != std::string::npos || e.find("war") != std::string::npos) std::cout << "  t" << s.turn << " " << e << std::endl;
        if (s.turn % 10) continue;
        std::cout << "turn " << s.turn << ":";
        const auto self = diplomacy::ParticipantId::major(s.playerEmpire);
        for (const auto& r : s.majorRelations) {
            if (r.first != s.playerEmpire && r.second != s.playerEmpire) continue;
            const int other = r.first == s.playerEmpire ? r.second : r.first;
            std::int64_t atk = 0; int ships = 0;
            for (const auto& f : s.fleets) if (f.ownerId == other) { atk += fleetAttack(f); ++ships; }
            std::cout << "  e" << other << "[contact " << r.contact << " war " << r.atWar << " trust " << r.trust << " rank "
                      << s.diplomacyEngineState.highestTreatyRank(self, diplomacy::ParticipantId::major(other)) << " ships " << ships << " atk " << atk << "]";
        }
        std::int64_t atk = 0; int ships = 0;
        for (const auto& f : s.fleets) if (f.ownerId == s.playerEmpire) { atk += fleetAttack(f); ++ships; }
        std::cout << "  me[ships " << ships << " atk " << atk << "]" << std::endl;
        if (std::getenv("BOTF_FLEETS")) {
            for (const auto& f : s.fleets) std::cout << "    f" << f.id << " o" << f.ownerId << " (" << f.x << "," << f.y << ")->(" << f.targetX << "," << f.targetY << ") atk " << fleetAttack(f) << " hull " << f.hull << " class " << static_cast<int>(f.shipClass) << (f.colonyShip ? " colony" : "") << std::endl;
            for (const auto& c : s.colonies) { const auto sy = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == c.systemId; }); std::cout << "    col o" << c.ownerId << " (" << sy->x << "," << sy->y << ") pop " << c.population << std::endl; }
        }
    }
}
}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "all";
    const char* seedEnv = std::getenv("BOTF_SEEDS");
    const int seeds = seedEnv ? std::max(1, std::atoi(seedEnv)) : 10;
    const char* presetEnv = std::getenv("BOTF_PRESET");
    int failed = 0;
    try {
        content::installBase();
        if (mode == "probe") { probe(argc > 2 ? argv[2] : "remastered", argc > 3 ? std::atoi(argv[3]) : 0); return 0; }
        std::vector<std::string> ids;
        for (const auto* p : presets::all()) if (p->lineupPlayable && (!presetEnv || p->id == presetEnv)) ids.push_back(p->id);
        if (mode == "all" || mode == "war") {
            std::cout << "FIRST WAR on the player (turn e<empire>), player accepts every pact, " << seeds << " seeds, medium galaxy, normal difficulty" << std::endl;
            for (const auto& id : ids) {
                std::cout << std::left << std::setw(18) << id << std::right;
                int worst = 0, none = 0, early = 0, pactSeeds = 0;
                for (int k = 0; k < seeds; ++k) {
                    const auto r = playWar(id, 500 + 37ULL * static_cast<std::uint64_t>(k), 100);
                    std::cout << std::setw(12) << (r.firstWar < 0 ? (r.endedTurn >= 0 ? "end@" + std::to_string(r.endedTurn) : std::string("none")) : std::to_string(r.firstWar) + "e" + std::to_string(r.firstWarBy)) << "(c" << r.firstContact << "b" << r.firstBattle << ")";
                    if (r.firstWar < 0) { if (r.endedTurn < 0) ++none; } else { worst = std::max(worst, r.firstWar); if (r.firstWar < 30) ++early; }
                    if (r.friendsAt40 > 0) ++pactSeeds;
                    if (r.aisAt40 > 0 && r.friendsAt40 >= r.aisAt40) { std::cout << "!"; ++failed; }  // every AI friendly at turn 40
                }
                std::cout << "   no-war " << none << ", latest " << worst << ", seeds with a pact at turn 40: " << pactSeeds << std::endl;
                if (pactSeeds == 0) { std::cout << "FAIL " << id << ": no seed signed any pact by turn 40 (pacts must stay possible)" << std::endl; ++failed; }
                if (none > 0) { std::cout << "FAIL " << id << ": a seed saw no war within 100 turns" << std::endl; ++failed; }
                if (worst > 60) { std::cout << "FAIL " << id << ": a first war came after turn 60" << std::endl; ++failed; }
                if (early > 0) { std::cout << "FAIL " << id << ": a war before turn 30" << std::endl; ++failed; }
            }
        }
        if (mode == "all" || mode == "food") {
            std::cout << "GOVERNORS, first 100 turns: lowest industry workers on the player's colonies (turn), pop start->end, first 3 seeds" << std::endl;
            for (const auto& id : ids) {
                for (GovernorPriority g : {GovernorPriority::Growth, GovernorPriority::Balanced, GovernorPriority::Research, GovernorPriority::Manufacturing}) {
                    int zero = 0, lowest = 1 << 30, stalled = 0, share = 100;
                    std::cout << std::left << std::setw(18) << id << std::setw(14) << governorPriorityName(g) << std::right;
                    for (int k = 0; k < std::min(seeds, 4); ++k) {
                        const auto r = playFood(id, 500 + 37ULL * static_cast<std::uint64_t>(k), 100, g);
                        zero += r.turnsAtZero; lowest = std::min(lowest, r.minIndustry); stalled += r.stalledTurns; share = std::min(share, r.minSharePct);
                        if (k < 3) std::cout << "  min " << r.minIndustry << "@" << r.minIndustryTurn << " pop " << r.popStart << "->" << r.popEnd;
                    }
                    std::cout << "   zero-turns " << zero << " stalled-queue-turns " << stalled << " lowest " << lowest << " min share " << share << "%" << std::endl;
                    if (zero > 0 || stalled > 0) { std::cout << "FAIL " << id << " " << governorPriorityName(g) << ": industry hit zero or a build stalled" << std::endl; ++failed; }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cout << "ERROR " << e.what() << std::endl;
        return 2;
    }
    std::cout << (failed ? "AiWarFood: FAILED " + std::to_string(failed) : std::string("AiWarFood: all passed")) << std::endl;
    return failed ? 1 : 0;
}
