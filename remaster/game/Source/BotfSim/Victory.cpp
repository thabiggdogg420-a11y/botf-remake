// Schema-13 victory conditions: the original conquest/domination rules stay in
// Simulation.cpp; this module adds the scientific megaproject victory, the
// diplomatic victory hook and the per-empire VictoryProgress query.
#include "GameplayInternal.h"
#include "Scenarios.h"

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
    for (const EmpireState* planner : internal::aiPlanners(next)) {
        const auto& rival = *planner;
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

// Campaign diplomatic victory: the diplomacy engine's bloc-majority predicate plus
// remaster pacing rules, so a single early alliance or member race cannot end the game.
// Balance (schema 14, GAMEPLAY-REMASTER.md "Balance notes"): with turn 80 an AI
// grand alliance claimed the galaxy on exactly turn 80 in 16 of 24 five-empire
// campaigns, before most wars were fought. The earliest turn is now 150 and the claimant must lead its
// bloc (hold more population than any of its allies), so one bloc has one winner.
constexpr int DiplomaticVictoryEarliestTurn = 150;
constexpr int DiplomaticVictoryMinimumPartners = 2;
// P1 (Simon's yes, 20 Nov 2026; BALANCE-PROPOSALS-F5.md): the bloc must meet every condition for this many consecutive turns
// before it wins. In the F5 soaks 21 of 27 AI diplomatic wins came on turn 150 or 151, the rule's first legal turn.
constexpr int DiplomaticVictoryHoldTurns = 10;
bool diplomaticSupermajority(const diplomacy::DiplomaticVictoryStatus& bloc) {
    return bloc.galaxyPopulation > 0 && bloc.blocPopulation * 3 >= bloc.galaxyPopulation * 2;
}
std::int64_t empirePopulation(const GameState& state, int empireId) {
    std::int64_t total = 0;
    for (const auto& c : state.colonies) if (c.ownerId == empireId) total += c.population;
    return total;
}
bool leadsBloc(const GameState& state, int empireId, const diplomacy::DiplomaticVictoryStatus& bloc) {
    const auto own = empirePopulation(state, empireId);
    for (int ally : bloc.allies) {
        const auto theirs = empirePopulation(state, ally);
        if (theirs > own || (theirs == own && ally < empireId)) return false;
    }
    return true;
}
bool diplomaticConditionsMet(const GameState& state, int empireId) {
    if (state.turn < DiplomaticVictoryEarliestTurn || !diplomaticVictoryAchieved(state, empireId)) return false;
    const auto bloc = diplomaticVictoryStatus(state, empireId);
    const auto partners = static_cast<int>(bloc.allies.size() + bloc.memberMinors.size());
    return partners >= DiplomaticVictoryMinimumPartners && diplomaticSupermajority(bloc) && leadsBloc(state, empireId, bloc);
}
int diplomaticHoldTurns(const GameState& state, int empireId) {
    for (const auto& [id, turns] : state.diplomaticHold) if (id == empireId) return turns;
    return 0;
}
bool diplomaticVictoryClaimed(const GameState& state, int empireId) {
    return diplomaticHoldTurns(state, empireId) >= DiplomaticVictoryHoldTurns && diplomaticConditionsMet(state, empireId);
}
// Once a turn, before the victory checks: an empire meeting every condition adds a turn to its hold; any other loses it.
void updateDiplomaticHold(GameState& next) {
    std::vector<std::pair<int, int>> held;
    for (int id : internal::activeEmpires(next))
        if (diplomaticConditionsMet(next, id)) held.emplace_back(id, diplomaticHoldTurns(next, id) + 1);
    std::sort(held.begin(), held.end());
    next.diplomaticHold = std::move(held);
}

// Schema 25: the vanilla vendetta victory. An empire wins by eliminating both of its designated racial rivals (scenarios::vendettaRivals),
// once turn 20 is past; the rule is live only while both are seated, and it is part of the alternative victories.
constexpr int VendettaEarliestTurn = 20;
bool colonyOwnedBy(const GameState& state, int empireId) {
    return std::any_of(state.colonies.begin(), state.colonies.end(), [empireId](const Colony& c) { return c.ownerId == empireId; });
}
bool vendettaLive(const GameState& state, int empireId, std::vector<int>& rivals) {
    rivals = scenarios::vendettaRivals(empireId);
    if (rivals.size() != 2) return false;
    const auto empires = internal::activeEmpires(state);
    for (int r : rivals) if (!std::binary_search(empires.begin(), empires.end(), r)) return false;
    return true;
}
bool vendettaClaimed(const GameState& state, int empireId) {
    std::vector<int> rivals;
    if (state.turn < VendettaEarliestTurn || !vendettaLive(state, empireId, rivals) || !colonyOwnedBy(state, empireId)) return false;
    for (int r : rivals) if (colonyOwnedBy(state, r)) return false;
    return true;
}

void evaluateVictory(GameState& next, TurnReport& report) {
    if (!next.rules.alternativeVictories || next.outcome != CampaignOutcome::Ongoing) return;
    updateDiplomaticHold(next);
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
            if (diplomaticVictoryClaimed(next, id)) { winner = id; how = VictoryCondition::Diplomatic; break; }
        }
    }
    if (!winner) {
        for (int id : internal::activeEmpires(next)) {
            if (vendettaClaimed(next, id)) { winner = id; how = VictoryCondition::Vendetta; break; }
        }
    }
    if (!winner) return;
    next.winnerEmpire = winner;
    // Hotseat: any human's win is a human victory (the client names the winner and, for a diplomatic win, the allied humans).
    next.outcome = winner == next.playerEmpire || humanSeat(next, winner) ? CampaignOutcome::Victory : CampaignOutcome::Defeat;
    // Hotseat (MULTIPLAYER.md decision 1, Simon 20 Nov 2026): allies share a diplomatic win; when the winning bloc holds two or
    // more humans every one of them wins, even under an AI's lead. Humans outside the bloc lose. A one-human game is unchanged.
    if (how == VictoryCondition::Diplomatic && !next.hotseatHumans.empty() && next.outcome == CampaignOutcome::Defeat) {
        int humans = 0;
        for (int ally : diplomaticVictoryStatus(next, winner).allies) humans += humanSeat(next, ally);
        if (humans >= 2) next.outcome = CampaignOutcome::Victory;
    }
    const std::string text = how == VictoryCondition::Vendetta
        ? std::string(empireName(winner)) + " settled its vendetta: both of its sworn rivals are finished. Vendetta victory. " +
            (winner == next.playerEmpire ? "Campaign victory." : "Campaign defeat.")
        : how == VictoryCondition::Scientific
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
    case VictoryCondition::Vendetta: return "Vendetta";
    default: return "Unknown";
    }
}

int scientificVictoryThreshold(const GameState& state) {
    return std::min(techCap(state), eraStartTech(state) + 3);
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
    domination.achieved = own >= required && state.turn >= 20 && state.playingOnFrom == 0; // C4: not after playing on
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
    diplomacyProgress.achieved = diplomacyProgress.enabled && gameplay::diplomaticVictoryClaimed(state, empireId);
    if (diplomacyProgress.enabled) {
        const auto bloc = diplomaticVictoryStatus(state, empireId);
        const auto partners = bloc.allies.size() + bloc.memberMinors.size();
        // Progress toward the two-thirds supermajority of galactic population.
        const std::int64_t goal = std::max<std::int64_t>(1, (bloc.galaxyPopulation * 2 + 2) / 3);
        diplomacyProgress.percent = diplomacyProgress.achieved ? 100
            : static_cast<int>(std::clamp<std::int64_t>(bloc.blocPopulation * 100 / goal, 0, 99));
        diplomacyProgress.explanation = "Alliance bloc holds " + std::to_string(bloc.blocPopulation) + " of " +
            std::to_string(bloc.galaxyPopulation) + " galactic population with " + std::to_string(partners) +
            " partner(s). Victory needs two-thirds of the population, at least " +
            std::to_string(gameplay::DiplomaticVictoryMinimumPartners) + " allied empires or member races, the largest population "
            "among the allies, and turn " + std::to_string(gameplay::DiplomaticVictoryEarliestTurn) + " or later, all held for " +
            std::to_string(gameplay::DiplomaticVictoryHoldTurns) + " turns in a row (held " +
            std::to_string(gameplay::diplomaticHoldTurns(state, empireId)) + ").";
    } else {
        diplomacyProgress.percent = 0;
        diplomacyProgress.explanation = "Disabled by the Classic ruleset.";
    }
    result.push_back(diplomacyProgress);

    VictoryProgress vendetta;   // Schema 25: the vanilla vendetta victory.
    vendetta.empireId = empireId;
    vendetta.condition = VictoryCondition::Vendetta;
    std::vector<int> rivals;
    const bool live = gameplay::vendettaLive(state, empireId, rivals);
    vendetta.enabled = state.rules.alternativeVictories && live;
    if (!state.rules.alternativeVictories) {
        vendetta.explanation = "Disabled by the Classic ruleset.";
    } else if (!live) {
        vendetta.explanation = "Needs both sworn rivals in the game; they are not both seated here.";
    } else {
        int down = 0;
        std::string names;
        for (int r : rivals) {
            const bool gone = !gameplay::colonyOwnedBy(state, r);
            down += gone ? 1 : 0;
            names += (names.empty() ? "" : " and ") + std::string(empireName(r)) + (gone ? " (finished)" : "");
        }
        vendetta.achieved = gameplay::vendettaClaimed(state, empireId);
        vendetta.percent = vendetta.achieved ? 100 : 50 * down;
        vendetta.explanation = "Sworn rivals: " + names + ". Eliminating both wins the vendetta (from turn " + std::to_string(gameplay::VendettaEarliestTurn) + ").";
    }
    result.push_back(vendetta);
    return result;
}

Command makeStartMegaprojectCommand(std::uint64_t id, int systemId) {
    Command c; c.id = id; c.type = CommandType::StartMegaproject; c.systemId = systemId; return c;
}
Command makeCancelMegaprojectCommand(std::uint64_t id) {
    Command c; c.id = id; c.type = CommandType::CancelMegaproject; return c;
}
} // namespace botf
