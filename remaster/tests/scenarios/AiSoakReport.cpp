// F5 balance evidence (20 Nov 2026): AI-vs-AI soaks on every registered preset. Not a test suite (no add_test): a report.
// The player slot is passive (no orders), so the rival AIs decide the campaign; every turn it reads victoryProgress for every
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
            const auto sz = sizes.find(p->size.empty() ? std::string("medium") : p->size);
            if (sz != sizes.end()) { setup.width = sz->second[0]; setup.height = sz->second[1]; setup.systems = sz->second[2]; }
            if (!p->shape.empty()) parseGalaxyShape(p->shape, setup.galaxy.shape);
            if (!p->minorDensity.empty()) parseMinorDensity(p->minorDensity, setup.galaxy.density);
            if (!p->difficulty.empty()) parseDifficulty(p->difficulty, setup.galaxy.difficulty);
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
            std::map<int, EmpireTrack> track;
            for (int id : internal::activeEmpires(state)) { track[id].id = id; track[id].name = empireName(id); }
            std::string ending;
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
                    if (turnSecs > 20.0 || state.fleets.size() > 20000) {
                        std::cout << "RUNAWAY " << p->id << " seed " << setup.seed << " turn " << state.turn << ": " << turnSecs << " s, fleets "
                                  << state.fleets.size() << ", colonies " << state.colonies.size() << ", stations "
                                  << state.stations.size() << ", events this turn " << report.events.size() << std::endl;
                        throw std::runtime_error("runaway");
                    }
                    if (state.turn % 50 == 0) std::cout << "TICK " << p->id << " seed " << setup.seed << " turn " << state.turn << " fleets " << state.fleets.size() << std::endl;
                    for (auto& [id, tr] : track) {
                        int n = 0;
                        for (const auto& c : state.colonies) n += c.ownerId == id;
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
                            if (line.find("victory") != std::string::npos || line.find("defeat") != std::string::npos) ending = line;
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
            else if (state.outcome == CampaignOutcome::Defeat && empireEliminated(state, state.playerEmpire)) { kind = "player-eliminated"; ++playerOut; }
            else kind = "conquest/domination";
            ++endings[kind];
            const std::string winner = state.winnerEmpire ? empireName(state.winnerEmpire) : "-";
            if (state.winnerEmpire && kind != "player-eliminated") ++winsBy[winner];
            std::cout << "GAME " << p->id << " seed " << setup.seed << " turns " << state.turn << " end " << kind << " winner " << winner
                      << (ending.empty() ? "" : " | " + ending) << std::endl;
            for (const auto& [id, tr] : track) {
                ++appearances[tr.name];
                std::cout << "EMP " << p->id << " seed " << setup.seed << " " << tr.name << (id == state.playerEmpire ? " (passive)" : "")
                          << " colonies " << tr.colonies << " peak " << tr.peakColonies
                          << (tr.eliminatedTurn ? " out@" + std::to_string(tr.eliminatedTurn) : std::string());
                for (int c = 0; c < 5; ++c) {
                    std::cout << " " << condName(c) << " " << tr.best[c] << "%" << (tr.first[c] ? "@" + std::to_string(tr.first[c]) : std::string());
                    if (tr.first[c] && id != state.playerEmpire) firstReach[tr.name + " " + condName(c)].push_back(tr.first[c]);
                }
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
