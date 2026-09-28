// Schema-13 victory conditions: the original conquest/domination rules stay in
// Simulation.cpp; this module adds the scientific megaproject victory, the
// diplomatic victory hook and the per-empire VictoryProgress query.
#include "GameplayInternal.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace botf::gameplay {
constexpr int MegaprojectFirstTurn = 50;

void noteVictory(const GameState& s, TurnReport& report, int empireId, int systemId, const std::string& text) {
    ChronicleEntry e;
    e.category = ChronicleCategory::Victory;
    e.text = text;
    e.empireId = empireId;
    e.systemId = systemId;
    if (const auto sys = systemById(s, systemId)) { e.x = sys->x; e.y = sys->y; }
    note(report, std::move(e));
}
std::string systemLabel(const GameState& s, int systemId) {
    const auto sys = systemById(s, systemId);
    return sys ? sys->name : "system " + std::to_string(systemId);
}

void validateMegaprojects(const GameState& s) {
    require(s.rules.alternativeVictories || s.megaprojects.empty(), "Megaprojects are disabled by the ruleset");
    const auto empires = internal::activeEmpires(s);
    int priorOwner = 0;
    const int cost = megaprojectIndustryCost(s);
    for (const auto& m : s.megaprojects) {
        require(m.ownerId > priorOwner && std::binary_search(empires.begin(), empires.end(), m.ownerId),
            "Megaprojects must be one per campaign empire, sorted by owner");
        priorOwner = m.ownerId;
        const auto colony = colonyAt(s, m.systemId);
        require(colony && colony->ownerId == m.ownerId, "Megaproject colony not owned by its empire");
        require(m.work >= 0 && m.work <= cost && (m.work < cost || s.outcome != CampaignOutcome::Ongoing),
            "Megaproject work out of bounds");
        require(m.startTurn >= 0 && m.startTurn <= s.turn, "Invalid megaproject start turn");
        for (const auto& u : s.stationUpgrades)
            require(u.fundingSystemId != m.systemId, "A colony cannot fund a megaproject and a starbase upgrade");
    }
}

void startMegaproject(GameState& s, int empireId, int systemId) {
    internal::mutableEmpire(s, empireId).credits -= megaprojectCreditCost(s);
    s.megaprojects.push_back({empireId, systemId, 0, s.turn});
    std::sort(s.megaprojects.begin(), s.megaprojects.end(), [](const Megaproject& a, const Megaproject& b) { return a.ownerId < b.ownerId; });
}

void applyMegaprojectCommand(GameState& state, const Command& c) {
    const int owner = state.playerEmpire;
    if (c.type == CommandType::StartMegaproject) {
        const auto blocker = megaprojectBlocker(state, owner, c.systemId);
        require(blocker.empty(), blocker);
        startMegaproject(state, owner, c.systemId);
        return;
    }
    const auto before = state.megaprojects.size();
    std::erase_if(state.megaprojects, [owner](const Megaproject& m) { return m.ownerId == owner; });
    require(state.megaprojects.size() != before, "No megaproject under way");
}

void planRivalMegaprojects(GameState& next, TurnReport& report) {
    for (const auto& rival : next.rivals) {
        if (empireMegaproject(next, rival.id) || rival.credits < 2 * megaprojectCreditCost(next)) continue;
        int best = 0, bestIndustry = 0;
        for (const auto& c : next.colonies) {
            if (c.ownerId != rival.id || !megaprojectBlocker(next, rival.id, c.systemId).empty()) continue;
            const int industry = economy(next, c).industry;
            if (industry > bestIndustry) { bestIndustry = industry; best = c.systemId; }
        }
        if (!best) continue;
        startMegaproject(next, rival.id, best);
        noteVictory(next, report, rival.id, best, std::string(empireName(rival.id)) + " began the " + megaprojectName(next) +
            " at " + systemLabel(next, best) + ". Its completion would win a scientific victory.");
    }
}

void resolveMegaprojectLosses(GameState& next, TurnReport& report) {
    std::erase_if(next.megaprojects, [&](const Megaproject& m) {
        const auto colony = colonyAt(next, m.systemId);
        if (colony && colony->ownerId == m.ownerId) return false;
        noteVictory(next, report, m.ownerId, m.systemId, std::string(empireName(m.ownerId)) + " lost the " + megaprojectName(next) +
            " when " + systemLabel(next, m.systemId) + " fell.");
        return true;
    });
}

void evaluateVictory(GameState& next, TurnReport& report) {
    if (!next.rules.alternativeVictories || next.outcome != CampaignOutcome::Ongoing) return;
    const int cost = megaprojectIndustryCost(next);
    int winner = 0;
    VictoryCondition how = VictoryCondition::Scientific;
    int where = 0;
    for (int id : internal::activeEmpires(next)) {
        const auto project = empireMegaproject(next, id);
        if (project && project->work >= cost) { winner = id; where = project->systemId; break; }
    }
    if (!winner) {
        for (int id : internal::activeEmpires(next)) {
            if (diplomaticVictoryAchieved(next, id)) { winner = id; how = VictoryCondition::Diplomatic; break; }
        }
    }
    if (!winner) return;
    next.winnerEmpire = winner;
    next.outcome = winner == next.playerEmpire ? CampaignOutcome::Victory : CampaignOutcome::Defeat;
    const std::string text = how == VictoryCondition::Scientific
        ? std::string(empireName(winner)) + " completed the " + megaprojectName(next) + " at " + systemLabel(next, where) +
            ": scientific victory. " + (winner == next.playerEmpire ? "Campaign victory." : "Campaign defeat.")
        : std::string(empireName(winner)) + " united the galaxy through diplomacy: diplomatic victory. " +
            (winner == next.playerEmpire ? "Campaign victory." : "Campaign defeat.");
    noteVictory(next, report, winner, where, text);
}
} // namespace botf::gameplay

namespace botf {
const char* victoryConditionName(VictoryCondition condition) {
    switch (condition) {
    case VictoryCondition::Conquest: return "Conquest";
    case VictoryCondition::Domination: return "Domination";
    case VictoryCondition::Scientific: return "Scientific";
    case VictoryCondition::Diplomatic: return "Diplomatic";
    default: return "Unknown";
    }
}

int scientificVictoryThreshold(const GameState& state) {
    return std::min(MaxTechLevel, eraBaseTechLevel(state.startingEra, state.ruleset) + 3);
}

const char* megaprojectName(const GameState& state) {
    switch (state.startingEra) {
    case StartingEra::Early: return "Genesis Initiative";
    case StartingEra::Developed: return "Subspace Relay Array";
    case StartingEra::Expanded: return "Transwarp Network";
    case StartingEra::Advanced: return "Iconian Gateway Restoration";
    case StartingEra::Supreme: return "Omega Stabilization Project";
    default: return "Empire Megaproject";
    }
}

int megaprojectIndustryCost(const GameState& state) {
    return 4000 + 1000 * (static_cast<int>(state.startingEra) - 1);
}

int megaprojectWorkPerTurn(const GameState& state, int empireId) {
    return 30 + 5 * empire(state, empireId).technologies[2].level; // Construction crews limit the pace.
}

std::int64_t megaprojectCreditCost(const GameState& state) {
    return (state.ruleset == Ruleset::Reference ? 500 : 150) * static_cast<std::int64_t>(state.startingEra);
}

const Megaproject* empireMegaproject(const GameState& state, int empireId) {
    for (const auto& m : state.megaprojects) if (m.ownerId == empireId) return &m;
    return nullptr;
}

std::string megaprojectBlocker(const GameState& state, int empireId, int systemId) {
    if (!state.rules.alternativeVictories) return "Alternative victories are disabled by the ruleset";
    if (state.outcome != CampaignOutcome::Ongoing) return "Campaign has ended";
    if (state.turn < gameplay::MegaprojectFirstTurn)
        return "The " + std::string(megaprojectName(state)) + " can begin on turn " + std::to_string(gameplay::MegaprojectFirstTurn);
    const auto& owner = empire(state, empireId);
    const int threshold = scientificVictoryThreshold(state);
    std::string missing;
    for (int field = 0; field < ResearchFields; ++field) {
        const int level = owner.technologies[static_cast<std::size_t>(field)].level;
        if (level < threshold) missing += (missing.empty() ? "" : ", ") + std::string(researchName(field)) + " " + std::to_string(level);
    }
    if (!missing.empty()) return "Requires every research field at level " + std::to_string(threshold) + " (now " + missing + ")";
    const auto colony = colonyAt(state, systemId);
    if (!colony || colony->ownerId != empireId) return "Choose one of our colonies";
    if (empireMegaproject(state, empireId)) return "The megaproject is already under way";
    for (const auto& u : state.stationUpgrades) if (u.fundingSystemId == systemId) return "Colony funds a starbase upgrade";
    if (owner.credits < megaprojectCreditCost(state)) return "Requires " + std::to_string(megaprojectCreditCost(state)) + " credits";
    return {};
}

// PLACEHOLDER (Lane B integration point): the diplomacy lane supplies the real
// predicate "this empire's alliances and memberships control a majority of the
// galactic population". Until it lands, diplomatic victory never triggers.
bool diplomaticVictoryAchieved(const GameState& state, int empireId) {
    (void)state;
    (void)empireId;
    return false;
}

std::vector<VictoryProgress> victoryProgress(const GameState& state, int empireId) {
    empire(state, empireId); // Throws for unknown empires.
    std::vector<VictoryProgress> result;
    int own = 0, total = 0;
    std::set<int> owners;
    for (const auto& c : state.colonies) {
        ++total;
        owners.insert(c.ownerId);
        if (c.ownerId == empireId) ++own;
    }
    VictoryProgress conquest;
    conquest.empireId = empireId;
    conquest.condition = VictoryCondition::Conquest;
    conquest.percent = total ? std::clamp(100 * own / total, 0, 100) : 0;
    conquest.achieved = own > 0 && owners.size() == 1 && !state.rivals.empty();
    conquest.explanation = "Holds " + std::to_string(own) + " of " + std::to_string(total) +
        " colonies; conquest requires holding every colony in the galaxy.";
    result.push_back(conquest);

    VictoryProgress domination;
    domination.empireId = empireId;
    domination.condition = VictoryCondition::Domination;
    const int systems = static_cast<int>(state.systems.size());
    const int required = (systems * 3 + 4) / 5;
    domination.percent = required ? std::clamp(100 * own / required, 0, 100) : 0;
    domination.achieved = own >= required && state.turn >= 20;
    domination.explanation = "Controls " + std::to_string(own) + " of " + std::to_string(systems) + " systems; domination needs " +
        std::to_string(required) + " (60%) after turn 20.";
    result.push_back(domination);

    VictoryProgress science;
    science.empireId = empireId;
    science.condition = VictoryCondition::Scientific;
    science.enabled = state.rules.alternativeVictories;
    const int threshold = scientificVictoryThreshold(state);
    const int cost = megaprojectIndustryCost(state);
    int techPoints = 0, fieldsReady = 0;
    for (const auto& t : empire(state, empireId).technologies) {
        techPoints += std::min(t.level, threshold);
        if (t.level >= threshold) ++fieldsReady;
    }
    const auto project = empireMegaproject(state, empireId);
    const int work = project ? project->work : 0;
    science.percent = threshold ? std::clamp(70 * techPoints / (ResearchFields * threshold) + 30 * work / cost, 0, 100) : 0;
    science.achieved = science.enabled && project && work >= cost;
    science.explanation = science.enabled
        ? std::to_string(fieldsReady) + " of 6 research fields at level " + std::to_string(threshold) + "; " + megaprojectName(state) + ": " +
            (project ? std::to_string(work) + "/" + std::to_string(cost) + " industry at " + gameplay::systemLabel(state, project->systemId)
                     : std::string("not started")) + "."
        : "Disabled by the Classic ruleset.";
    if (!science.enabled) science.percent = 0;
    result.push_back(science);

    VictoryProgress diplomacyProgress;
    diplomacyProgress.empireId = empireId;
    diplomacyProgress.condition = VictoryCondition::Diplomatic;
    diplomacyProgress.enabled = state.rules.alternativeVictories;
    diplomacyProgress.achieved = diplomacyProgress.enabled && diplomaticVictoryAchieved(state, empireId);
    diplomacyProgress.percent = diplomacyProgress.achieved ? 100 : 0;
    diplomacyProgress.explanation = diplomacyProgress.enabled
        ? "Alliances and memberships controlling a majority of the galactic population (awaiting the diplomacy integration)."
        : "Disabled by the Classic ruleset.";
    result.push_back(diplomacyProgress);
    return result;
}

Command makeStartMegaprojectCommand(std::uint64_t id, int systemId) {
    Command c; c.id = id; c.type = CommandType::StartMegaproject; c.systemId = systemId; return c;
}
Command makeCancelMegaprojectCommand(std::uint64_t id) {
    Command c; c.id = id; c.type = CommandType::CancelMegaproject; return c;
}
} // namespace botf
