#pragma once
// Engine-independent view-model helpers for the Unreal client.
//
// Nothing here includes Unreal headers: the functions take core types and return plain values or UTF-8 text, so
// this header is compiled and unit-tested with an ordinary C++ compiler against the core (see
// remaster/docs/UNREAL-CLIENT.md). The Slate widgets only convert the results to FString/FText.
//
// Formulas marked MIRROR repeat a rule the core applies inside advance() but does not expose as a query. They are
// kept here, in one place, until the core offers the query named next to them.

#include "Simulation.h"
#include "TacticalCombat.h"
#include "ReferenceCatalogue.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace botf::view {

// ---------------------------------------------------------------------------------------------------------------
// Units and movement range
// ---------------------------------------------------------------------------------------------------------------

inline const Fleet* findFleet(const GameState& state, int fleetId) {
    for (const auto& fleet : state.fleets)
        if (fleet.id == fleetId) return &fleet;
    return nullptr;
}

// A task force can only go where every member can go (core validates SetTaskForceOrder member by member), so its
// range is its shortest-ranged member's. Ties keep the earliest member. nullptr when no member is in the state.
inline const Fleet* rangeLimitingFleet(const GameState& state, int taskForceId) {
    const TaskForce* group = taskForceAt(state, taskForceId);
    if (!group) return nullptr;
    const Fleet* best = nullptr;
    int bestRange = 0;
    for (const int id : group->fleetIds) {
        const Fleet* member = findFleet(state, id);
        if (!member) continue;
        const int range = shipRange(*member);
        if (!best || range < bestRange) { best = member; bestRange = range; }
    }
    return best;
}

// The fleet whose supply range governs a unit: a task force's shortest-ranged member, else the ship itself.
inline const Fleet* movementFleet(const GameState& state, int fleetId, int taskForceId) {
    if (taskForceId)
        if (const Fleet* limit = rangeLimitingFleet(state, taskForceId)) return limit;
    return findFleet(state, fleetId);
}

// The route a unit would take. For a task force the route is empty unless every member has a route (exactly what
// core requires of a group order); the returned path is the range-limiting member's.
inline std::vector<int> unitRoute(const GameState& state, int fleetId, int taskForceId, int x, int y) {
    if (taskForceId) {
        const TaskForce* group = taskForceAt(state, taskForceId);
        if (group && !group->fleetIds.empty()) {
            const Fleet* limit = rangeLimitingFleet(state, taskForceId);
            std::vector<int> governing;
            for (const int id : group->fleetIds) {
                const Fleet* member = findFleet(state, id);
                if (!member) return {};
                auto path = route(state, *member, x, y);
                if (path.empty()) return {};
                if (member == limit) governing = std::move(path);
            }
            return governing;
        }
    }
    const Fleet* ship = findFleet(state, fleetId);
    return ship ? route(state, *ship, x, y) : std::vector<int>{};
}

// Supply coverage for one fleet, row-major (width * height), 1 = supplied. Computed once per draft version by the
// chart instead of calling supplied() five times per sector per frame.
inline std::vector<unsigned char> supplyGrid(const GameState& state, const Fleet& fleet) {
    std::vector<unsigned char> grid(static_cast<std::size_t>(std::max(0, state.width * state.height)), 0);
    for (int y = 0; y < state.height; ++y)
        for (int x = 0; x < state.width; ++x)
            grid[static_cast<std::size_t>(y * state.width + x)] = supplied(state, fleet, x, y) ? 1 : 0;
    return grid;
}

// MIRROR of the distance test in supplied() (core query wanted: supplyRange(state, fleet)).
inline int supplyRange(const GameState& state, const Fleet& fleet) {
    return shipRange(fleet) + empire(state, fleet.ownerId).technologies[4].level / 2;
}

// ---------------------------------------------------------------------------------------------------------------
// Treasury forecast
// ---------------------------------------------------------------------------------------------------------------

// MIRROR of fleet maintenance in advance() (core query wanted: fleetUpkeep(fleet)).
inline int shipMaintenance(const Fleet& fleet) {
    const auto& catalogue = referenceData::ShipCatalogue;
    if (fleet.referenceShipId >= 0 && static_cast<std::size_t>(fleet.referenceShipId) < catalogue.size())
        return catalogue[static_cast<std::size_t>(fleet.referenceShipId)].maintenance;
    return fleet.colonyShip ? 2 : fleet.shipClass == Building::Destroyer ? 4 : fleet.shipClass == Building::Transport ? 3 : 1;
}

// MIRROR of trade route revenue in advance() (core query wanted: tradeRouteIncome(state, route)). 0 when idle.
inline int tradeRouteIncome(const GameState& state, const TradeRoute& tradeRoute) {
    const Colony* source = colonyAt(state, tradeRoute.fromSystemId);
    const Colony* target = colonyAt(state, tradeRoute.toSystemId);
    if (!source || !target || source->ownerId != tradeRoute.ownerId || target->ownerId == tradeRoute.ownerId) return 0;
    const MajorRelation* relation = majorRelation(state, tradeRoute.ownerId, target->ownerId);
    if (!relation || relation->atWar || !relation->tradeTreaty) return 0;
    return 5 + std::min(source->population, target->population) / 10;
}

// MIRROR of the treaty dividend in applyTreatyConsequences (modern campaigns; core query wanted:
// treatyDividend(state, empire)). Core adds it to TurnReport::tradeIncome.
inline int treatyDividend(const GameState& state, int empireId) {
    if (!(state.campaignExpansionEnabled && state.modernEnginesActive)) return 0;
    const auto& diplomacyState = campaignDiplomacyState(state);
    int total = 0;
    for (const auto& relation : state.majorRelations) {
        if (relation.first != empireId && relation.second != empireId) continue;
        if (!relation.contact || relation.atWar) continue;
        if (empireEliminated(state, relation.first) || empireEliminated(state, relation.second)) continue;
        const int rank = diplomacyState.highestTreatyRank(diplomacy::ParticipantId::major(relation.first),
                                                          diplomacy::ParticipantId::major(relation.second));
        total += rank == 4 ? 8 : rank == 3 ? 5 : rank == 2 ? 3 : 0;
    }
    return total;
}

struct IncomeForecast {
    int colonies = 0;        // colony income less structure upkeep
    int trade = 0;           // trade route revenue
    int treaties = 0;        // treaty dividends (reported by core as trade income)
    int ships = 0;           // ship maintenance
    std::int64_t spend = 0;  // credits this turn's orders already committed (purchases, gifts)
    int net() const { return colonies + trade + treaties - ships; }
    int tradeReported() const { return trade + treaties; }  // comparable with TurnReport::tradeIncome
};

// Expected treasury change at turn end, from the draft (committed campaign plus this turn's orders). Battles,
// events and rival actions during resolution can still change the result.
inline IncomeForecast forecastIncome(const GameState& committed, const GameState& draft, int empireId) {
    IncomeForecast out;
    for (const auto& colony : draft.colonies) {
        if (colony.ownerId != empireId) continue;
        const Economy e = economy(draft, colony);
        out.colonies += e.income - e.upkeep;
    }
    for (const auto& tradeRoute : draft.tradeRoutes)
        if (tradeRoute.ownerId == empireId) out.trade += tradeRouteIncome(draft, tradeRoute);
    out.treaties = treatyDividend(draft, empireId);
    for (const auto& fleet : draft.fleets)
        if (fleet.ownerId == empireId) out.ships += shipMaintenance(fleet);
    out.spend = empire(committed, empireId).credits - empire(draft, empireId).credits;
    return out;
}

// ---------------------------------------------------------------------------------------------------------------
// Tactical combat (rules 2: disengage, ambush, mutual ram, hail truce, AI-controlled orders)
// ---------------------------------------------------------------------------------------------------------------

using NameFn = std::function<std::string(int)>;

inline std::string shipLabel(const tactical::Encounter& encounter, int fleetId) {
    const tactical::Ship* ship = encounter.getShip(fleetId);
    if (!ship) return "#" + std::to_string(fleetId);
    return (ship->name.empty() ? std::string("Ship") : ship->name) + " #" + std::to_string(fleetId);
}

// Short state tags for a ship card: what the player must know before ordering it.
inline std::vector<std::string> shipTags(const tactical::Encounter& encounter, const tactical::Ship& ship) {
    std::vector<std::string> tags;
    if (!ship.isAlive) { tags.push_back("DESTROYED"); return tags; }
    if (ship.retreated) { tags.push_back("WITHDRAWN"); return tags; }
    if (ship.isCloaked) tags.push_back(encounter.remasterRules() ? "CLOAKED (cannot fire; attack to ambush)" : "CLOAKED");
    if (encounter.remasterRules() && ship.disengaging()) tags.push_back("DISENGAGING (escapes next round if still retreating)");
    // Schema-13 stations join as immobile units (fleet ids >= StationCombatantBase): they hold position whatever
    // their order, never retreat and brace for -20% incoming damage.
    if (ship.immobile) tags.push_back("STATION (immobile: holds position, cannot retreat)");
    else if (!ship.canRetreat) tags.push_back("NO RETREAT ROUTE");
    if (ship.beamAttack == 0 && ship.torpedoAttack == 0) tags.push_back("UNARMED");
    return tags;
}

inline std::string joinTags(const std::vector<std::string>& tags, const char* separator = "  |  ") {
    std::string out;
    for (const auto& tag : tags) {
        if (!out.empty()) out += separator;
        out += tag;
    }
    return out;
}

// What a ship did in the last resolved round, including orders the tactical AI chose. Empty before round 1.
inline std::string executedOrderText(const tactical::Encounter& encounter, int fleetId) {
    const auto order = encounter.lastExecutedOrder(fleetId);
    if (!order) return {};
    std::string text = tactical::maneuverName(order->maneuver);
    if (order->targetFleetId) text += " > #" + std::to_string(order->targetFleetId);
    if (const tactical::ShipRoundStatus* status = encounter.rounds.back().getStatus(fleetId)) {
        if (status->aiControlled) text += " (AI)";
        if (status->decloakedThisRound) text += ", ambush";
        if (status->hailedThisRound) text += ", hailed";
    }
    return text;
}

// One line per notable event of a resolved round: orders (AI-chosen ones marked), ambush volleys, disengages and
// escapes, both halves of a ram, destructions and a hail truce.
inline std::vector<std::string> roundDetail(const tactical::Encounter& encounter, const tactical::RoundResult& round, const NameFn& name) {
    const bool modern = encounter.remasterRules();
    std::vector<std::string> lines;
    const auto label = [&](int id) { return name ? name(id) : shipLabel(encounter, id); };
    if (round.hailedTruce) lines.push_back("TRUCE: the hail was accepted; the battle ends without a shot.");
    for (const auto& status : round.shipStatuses) {
        if (status.hullBefore <= 0) continue;
        std::string line = label(status.fleetId) + ": " + tactical::maneuverName(status.maneuverExecuted);
        if (status.targetFleetId) line += " > " + label(status.targetFleetId);
        if (status.aiControlled) line += " [AI]";
        if (status.decloakedThisRound) line += modern ? " [AMBUSH: decloaked, +25% volley]" : " [DECLOAKED]";
        if (status.hailedThisRound) line += " [HAILED]";
        if (status.retreatedThisRound) line += " [ESCAPED]";
        else if (modern && status.maneuverExecuted == tactical::TacticalManeuver::Retreat)
            line += " [DISENGAGING: weapons offline, -25% incoming]";
        const int dealt = status.beamDamageDealt + status.torpedoDamageDealt;
        if (dealt > 0)
            line += ", " + std::to_string(status.beamDamageDealt) + " beam + " + std::to_string(status.torpedoDamageDealt) + " torpedo";
        lines.push_back(line);
    }
    for (const auto& salvo : round.salvos) {
        if (!salvo.ram) continue;
        const tactical::ShipRoundStatus* attacker = round.getStatus(salvo.attackerFleetId);
        const bool impact = attacker && attacker->maneuverExecuted == tactical::TacticalManeuver::Ram;
        lines.push_back(impact
            ? "RAM: " + label(salvo.attackerFleetId) + " rams " + label(salvo.defenderFleetId) + " for " + std::to_string(salvo.beamDamage)
            : "RAM RECOIL: " + label(salvo.defenderFleetId) + " takes " + std::to_string(salvo.beamDamage) + " from the impact");
    }
    for (const int id : round.lostFleetIds) lines.push_back(label(id) + " destroyed");
    for (const int id : round.retreatedFleetIds) lines.push_back(label(id) + " withdrew");
    if (round.encounterEnded)
        lines.push_back(std::string("BATTLE OVER: ") + (round.winnerOwnerId ? std::string(empireName(round.winnerOwnerId)) + " hold the field" : std::string("no victor")));
    return lines;
}

// Whether the other side would accept the player's hail now (rules 2). Empty for legacy encounters.
inline std::string hailOutlook(const tactical::Encounter& encounter, int player) {
    if (!encounter.remasterRules() || encounter.isFinished) return {};
    int enemy = 0;
    for (const auto& ship : encounter.ships) if (ship.ownerId != player) { enemy = ship.ownerId; break; }
    if (!enemy) return {};
    if (!encounter.relationAtWar) return "Hail: they are not formally at war with you and will accept a truce.";
    const bool accepts = encounter.aiAcceptsHail(enemy, player);
    return std::string("Hail: strength ") + std::to_string(encounter.sideStrength(player)) + " against " +
        std::to_string(encounter.sideStrength(enemy)) + ", trust " + std::to_string(encounter.relationTrust) +
        (accepts ? ". They would accept a truce if every ship of yours hails." : ". They would refuse a truce.");
}

// The finished encounter behind a committed battle record, when this session fought it. Records alone only keep
// damage; the encounter adds orders, AI decisions, ambushes and rams. -1 when none matches.
inline int matchEncounter(const std::vector<tactical::Encounter>& encounters, const CombatReport& report) {
    for (std::size_t i = 0; i < encounters.size(); ++i) {
        const auto& e = encounters[i];
        if (e.strategicTurn != report.turn || e.sectorX != report.x || e.sectorY != report.y) continue;
        if (e.rounds.size() != report.rounds.size()) continue;
        bool first = false, second = false, other = false;
        for (const auto& ship : e.ships) {
            if (ship.ownerId == report.firstOwner) first = true;
            else if (ship.ownerId == report.secondOwner) second = true;
            else other = true;
        }
        if (first && second && !other) return static_cast<int>(i);
    }
    return -1;
}

// ---------------------------------------------------------------------------------------------------------------
// Diplomacy
// ---------------------------------------------------------------------------------------------------------------

inline std::string participantName(const GameState& state, diplomacy::ParticipantId participant) {
    if (!participant.isMinor) return empireName(participant.id);
    if (const Minor* minor = minorAt(state, participant.id)) return minor->name;
    return "System " + std::to_string(participant.id);
}

inline int turnsLeft(const GameState& state, int dueTurn) { return std::max(0, dueTurn - state.turn); }

// "respond by turn 12 (2 turns left)"
inline std::string deadlineText(const GameState& state, int dueTurn) {
    const int left = turnsLeft(state, dueTurn);
    return "respond by turn " + std::to_string(dueTurn) + " (" + (left == 0 ? std::string("this turn") :
        std::to_string(left) + (left == 1 ? " turn left" : " turns left")) + ")";
}

inline std::string proposalSummary(const GameState& state, const diplomacy::Proposal& proposal, int player) {
    const bool incoming = proposal.target == diplomacy::ParticipantId::major(player);
    std::string text = incoming ? "From " + participantName(state, proposal.proposer) : "To " + participantName(state, proposal.target);
    text += ": ";
    if (proposal.treatyType == diplomacy::TreatyType::None)
        text += proposal.creditDemand > 0 ? "tribute demand" : "gift";
    else
        text += diplomacy::treatyTypeName(proposal.treatyType);
    if (proposal.treatyType != diplomacy::TreatyType::None && proposal.treatyType != diplomacy::TreatyType::War)
        text += std::string(", ") + diplomacy::treatyDurationName(proposal.duration);
    text += std::string(", ") + diplomacy::toneName(proposal.tone) + " tone";
    if (proposal.creditOffer > 0) text += ", offers " + std::to_string(proposal.creditOffer) + " credits";
    if (proposal.creditDemand > 0) text += ", demands " + std::to_string(proposal.creditDemand) + " credits";
    return text;
}

inline std::string callToArmsText(const GameState& state, const diplomacy::DefenseCall& call) {
    return std::string(empireName(call.defender)) + " calls on you to join its war against " + empireName(call.aggressor) +
        ": declare war by turn " + std::to_string(call.dueTurn) + ". Refusing costs 20 trust and 2 grievances, and an ally "
        "whose trust falls below 40 dissolves the alliance." + (turnsLeft(state, call.dueTurn) == 0 ? " Due this turn." : "");
}

// Ended treaties involving the player, newest first: expired, broken by war, superseded or dissolved.
inline std::vector<std::string> treatyHistoryLines(const GameState& state, int player, std::size_t limit) {
    std::vector<std::string> lines;
    const auto self = diplomacy::ParticipantId::major(player);
    const auto& history = campaignDiplomacyState(state).treatyHistory;
    for (auto it = history.rbegin(); it != history.rend() && lines.size() < limit; ++it) {
        if (!it->involves(self)) continue;
        lines.push_back(std::string(diplomacy::treatyTypeName(it->type)) + " with " + participantName(state, it->getCounterparty(self)) +
            ": " + diplomacy::treatyStatusName(it->status) + " (signed turn " + std::to_string(it->startTurn) + ")");
    }
    return lines;
}

// What the other empire remembers about the player and the player's standing in the galaxy.
inline std::vector<std::string> relationMemoryLines(const GameState& state, int player, int other) {
    std::vector<std::string> lines;
    const auto& diplomacyState = campaignDiplomacyState(state);
    if (empireEliminated(state, other)) lines.push_back(std::string(empireName(other)) + " has been eliminated: no colonies remain.");
    if (const diplomacy::RelationMemory* memory = diplomacyState.findMemory(player, other)) {
        lines.push_back("Grievances: they hold " + std::to_string(memory->grievancesOf(other)) + " against you; you hold " +
            std::to_string(memory->grievancesOf(player)) + " against them.");
        const int refused = memory->refusalsFrom(player);
        if (refused > 0)
            lines.push_back("Your last " + std::to_string(refused) + " proposal" + (refused == 1 ? " was" : "s were") +
                " refused or ignored; the AI waits longer before proposing again after refusals of its own.");
        if (memory->lastWarTurn >= 0) lines.push_back("Last war declared turn " + std::to_string(memory->lastWarTurn) + ".");
        if (memory->lastPeaceTurn >= 0) lines.push_back("Last peace signed turn " + std::to_string(memory->lastPeaceTurn) + ".");
    }
    const int theirs = diplomacyState.betrayals(other), ours = diplomacyState.betrayals(player);
    if (theirs > 0) lines.push_back(std::string(empireName(other)) + " has betrayed " + std::to_string(theirs) + " treat" + (theirs == 1 ? "y." : "ies."));
    if (ours > 0) lines.push_back("You have betrayed " + std::to_string(ours) + " treat" + (ours == 1 ? "y" : "ies") + ": every proposal you make is weighed against it.");
    if (treatyForbidsAttack(state, player, other))
        lines.push_back("A treaty binds you: declaring war now is a betrayal (-15 trust with them, -10 with every other power).");
    return lines;
}

inline std::string diplomaticVictoryText(const GameState& state, int player) {
    if (!(state.campaignExpansionEnabled && state.modernEnginesActive)) return {};
    const diplomacy::DiplomaticVictoryStatus status = diplomaticVictoryStatus(state, player);
    if (status.galaxyPopulation <= 0) return {};
    const std::int64_t percent = status.blocPopulation * 100 / status.galaxyPopulation;
    return "Alliance bloc: " + std::to_string(status.blocPopulation) + " of " + std::to_string(status.galaxyPopulation) +
        " population (" + std::to_string(percent) + "%), " + std::to_string(status.allies.size()) + " full all" +
        (status.allies.size() == 1 ? "y" : "ies") + ", " + std::to_string(status.memberMinors.size()) + " member minor" +
        (status.memberMinors.size() == 1 ? "" : "s") + (status.achieved ? ". Diplomatic majority reached." :
        ". A strict majority with at least one ally or member minor is a diplomatic majority.");
}

} // namespace botf::view
