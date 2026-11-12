// Difficulty tiers, AI against a fixed baseline (lane MOD-7, schema 24; docs/design/BALANCE-AI.md section 5). Every tier plays the same
// seeds for 100 turns: three AI rivals against a passive baseline empire that only runs colony governors, so the only thing that differs
// between the rows is how the AI plays (income, industry, research, treasury, war confidence, spy readiness, and from Hard the AI policy).
// A rival "wins" a trial when its strength score beats the baseline empire's. The table is printed and the order is checked.
// BOTF_SOAK_SCALE=N (environment) multiplies the seed count.
#include "Content.h"
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
void check(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }

std::int64_t strength(const GameState& s, int empireId) {
    const auto& e = empireId == s.playerEmpire ? static_cast<const EmpireState&>(s) : *std::find_if(s.rivals.begin(), s.rivals.end(), [&](const EmpireState& r) { return r.id == empireId; });
    std::int64_t score = 0;
    for (const auto& t : e.technologies) score += 10 * t.level;
    for (const auto& c : s.colonies) if (c.ownerId == empireId) score += 25 + c.population;
    for (const auto& f : s.fleets) if (f.ownerId == empireId) score += fleetAttack(f) / 20 + f.hull / 10;
    for (const auto& st : s.stations) if (st.ownerId == empireId) score += st.type == StationType::Starbase ? 60 : 25;
    return score;
}
void governors(GameState& s) {
    std::vector<int> mine;
    for (const auto& c : s.colonies) if (c.ownerId == s.playerEmpire) mine.push_back(c.systemId);
    for (int sys : mine) submit(s, makeSetColonyGovernorCommand(s.lastAcceptedCommand + 1, sys, GovernorPriority::Balanced, GovernorPriority::Manufacturing));
}
struct Row { double rivalScore = 0, playerScore = 0, wins = 0, colonies = 0, ships = 0, tech = 0; int trials = 0; };
Row play(Difficulty d, int seeds, int turns) {
    Row row;
    for (int k = 0; k < seeds; ++k) {
        CampaignSetup setup;
        setup.seed = 300 + static_cast<std::uint64_t>(k) * 7; setup.empires = 4; setup.width = 16; setup.height = 12; setup.systems = 30;
        setup.galaxy.difficulty = d;
        auto s = newCampaign(setup);
        governors(s);
        for (int t = 0; t < turns && s.outcome == CampaignOutcome::Ongoing; ++t) advance(s);
        const auto player = strength(s, s.playerEmpire);
        row.playerScore += static_cast<double>(player);
        for (const auto& r : s.rivals) {
            const auto score = strength(s, r.id);
            row.rivalScore += static_cast<double>(score); ++row.trials;
            if (score > player) row.wins += 1;
            for (const auto& c : s.colonies) if (c.ownerId == r.id) row.colonies += 1;
            for (const auto& f : s.fleets) if (f.ownerId == r.id) row.ships += 1;
            for (const auto& t : r.technologies) row.tech += t.level;
        }
    }
    row.rivalScore /= row.trials; row.playerScore /= seeds; row.wins = 100.0 * row.wins / row.trials;
    row.colonies /= row.trials; row.ships /= row.trials; row.tech /= row.trials;
    return row;
}
}  // namespace

int main() {
    int failed = 0;
    const char* scale = std::getenv("BOTF_SOAK_SCALE");
    const int seeds = 8 * (scale ? std::max(1, std::atoi(scale)) : 1), turns = 100;
    try {
        content::installBase();
        std::vector<Row> rows;
        std::cout << "tier        rival score  rival win %  colonies  ships  tech sum  (baseline player score)   [" << seeds << " seeds x 3 rivals, " << turns << " turns]" << std::endl;
        for (Difficulty d : DifficultyOrder) {
            const auto r = play(d, seeds, turns);
            rows.push_back(r);
            std::cout << std::left << std::setw(10) << difficultyName(d) << std::right << std::fixed << std::setprecision(1)
                      << std::setw(12) << r.rivalScore << std::setw(13) << r.wins << std::setw(10) << r.colonies << std::setw(8) << r.ships << std::setw(10) << r.tech
                      << std::setw(14) << r.playerScore << std::endl;
        }
        // Order of play: Simple, Easy, Normal, Hard, Expert, Impossible.
        const auto& simple = rows[0]; const auto& easy = rows[1]; const auto& normal = rows[2]; const auto& hard = rows[3]; const auto& expert = rows[4]; const auto& impossible = rows[5];
        check(simple.rivalScore < easy.rivalScore && easy.rivalScore < normal.rivalScore && normal.rivalScore < hard.rivalScore, "rival strength climbs from Simple to Hard");
        check(hard.rivalScore < impossible.rivalScore, "Impossible is above Hard");
        check(expert.rivalScore > normal.rivalScore && expert.rivalScore > hard.rivalScore * 0.97 && expert.rivalScore < impossible.rivalScore, "Expert sits between Normal and Impossible, at or above Hard, with no bonus");
        for (std::size_t i = 1; i < rows.size(); ++i) check(rows[i].wins + 0.001 >= rows[i - 1].wins, "rival win rate never falls along the tiers");
        check(simple.wins < impossible.wins || impossible.wins >= 99.9, "win rate rises from Simple to Impossible");
        std::cout << "PASS difficulty tiers ordered" << std::endl;
    } catch (const std::exception& e) {
        ++failed;
        std::cout << "FAIL difficulty tiers: " << e.what() << std::endl;
    }
    content::installBase();
    std::cout << (failed ? 0 : 1) << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
