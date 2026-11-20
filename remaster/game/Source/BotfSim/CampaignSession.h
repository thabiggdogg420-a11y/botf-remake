#pragma once
#include "TacticalCombat.h"
#include <functional>
#include <optional>
#include <set>

namespace botf {
// Implemented in Simulation.cpp: identical strategic phases, with a supplied combat phase.
TurnReport advanceWithCombat(GameState&, const std::function<void(GameState&, TurnReport&)>&);
// resolveTacticalBattles (Simulation.h) is the single battle resolver shared by
// headless advance() and this session; the session only intercepts player encounters.
}

namespace botf::session {
// A strategic turn is transactional. While a battle is pending the committed
// campaign remains unchanged. Replaying deterministic phases avoids persisting
// partially applied production, diplomacy or invasion side effects.
struct TurnSession {
    GameState baseline;
    std::vector<tactical::Encounter> completed;
    std::optional<tactical::Encounter> pending;
    bool active = false;
};
struct AwaitingPlayer {};

inline void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
inline void begin(TurnSession& session, const GameState& state) {
    require(!session.active, "A strategic turn is already in progress");
    validate(state);
    require(state.outcome == CampaignOutcome::Ongoing, "Campaign has ended");
    session = TurnSession{state, {}, {}, true};
}
inline void order(TurnSession& session, const tactical::Order& command) {
    require(session.active && session.pending.has_value(), "No tactical encounter awaits orders");
    const auto* ship = session.pending->getShip(command.fleetId);
    require(ship && humanSeat(session.baseline, ship->ownerId), "Cannot command an enemy ship");
    // Hotseat (21 Nov 2026): in a battle between two humans each commands their own ships, so the encounter's controller is
    // that ship's owner for the order and is restored afterwards (it decides only who may order and the AI's hail answer).
    auto& encounter = *session.pending;
    const int controller = encounter.controllingOwnerId;
    encounter.controllingOwnerId = ship->ownerId;
    try { encounter.submitOrder(command); } catch (...) { encounter.controllingOwnerId = controller; throw; }
    encounter.controllingOwnerId = controller;
}
inline void resolveRound(TurnSession& session) {
    require(session.active && session.pending.has_value(), "No tactical encounter awaits orders");
    auto next = *session.pending;
    next.resolveRound();
    session.pending = std::move(next);
}
inline void autoResolve(TurnSession& session) {
    require(session.active && session.pending.has_value(), "No tactical encounter awaits resolution");
    auto next = *session.pending;
    next.runUntilFinished();
    session.pending = std::move(next);
}

namespace detail {
inline tactical::Encounter prepareReplay(const tactical::Encounter& initial, const tactical::Encounter& recorded) {
    auto replay = initial;
    if (recorded.groups.empty()) {
        replay.groups.clear();
        for (auto& s : replay.ships) s.groupId = 0;
    }
    bool recordedHasCloak = false;
    for (const auto& s : recorded.ships) {
        if (s.canCloak || s.isCloaked) { recordedHasCloak = true; break; }
    }
    if (!recordedHasCloak) {
        for (auto& s : replay.ships) {
            s.canCloak = false;
            s.isCloaked = false;
        }
    }
    if (recorded.rulesVersion < tactical::RemasterTacticalRules) {
        // Schema-11 encounters: legacy rules, no strategic standing orders or relation data.
        replay.rulesVersion = recorded.rulesVersion;
        replay.standingOrders.clear();
        replay.relationAtWar = recorded.relationAtWar;
        replay.relationTrust = recorded.relationTrust;
    }
    return replay;
}

// Group standing orders are UI bookkeeping (member orders carry the effect), so
// they are taken from the recording once the group structure matches.
inline void adoptGroupStanding(tactical::Encounter& replay, const tactical::Encounter& recorded) {
    if (replay.groups.size() != recorded.groups.size()) return;
    for (std::size_t i = 0; i < replay.groups.size(); ++i) {
        const auto& g = replay.groups[i];
        const auto& r = recorded.groups[i];
        if (g.groupId != r.groupId || g.ownerId != r.ownerId || g.groupClass != r.groupClass ||
            g.fleetIds != r.fleetIds || g.name != r.name) return;
    }
    for (std::size_t i = 0; i < replay.groups.size(); ++i) {
        replay.groups[i].standingManeuver = recorded.groups[i].standingManeuver;
        replay.groups[i].standingTargetFleetId = recorded.groups[i].standingTargetFleetId;
    }
}

// Replays recorded rounds. inferAi=true (legacy recordings only) treats a player
// ship as AI-driven until the first round whose recorded order differs from the
// AI's decision, because V1 recordings did not mark AI decisions.
inline bool replayMatches(const tactical::Encounter& initial, const tactical::Encounter& recorded, const std::set<int>& humans, bool inferAi) {
    const bool remaster = recorded.rulesVersion >= tactical::RemasterTacticalRules;
    auto replay = prepareReplay(initial, recorded);
    std::set<int> explicitShips;
    try {
        for (const auto& round : recorded.rounds) {
            if (replay.isFinished) return false;
            for (const auto& status : round.shipStatuses) {
                const auto* ship = replay.getShip(status.fleetId);
                if (!ship || !humans.count(ship->ownerId) || !tactical::Encounter::shipActive(*ship)) continue;
                const tactical::Order recordedOrder{status.fleetId, status.targetFleetId, status.maneuverExecuted, status.subsystemTarget};
                if (remaster) {
                    if (status.aiControlled) continue; // The AI re-derives the same decision.
                } else if (inferAi && !explicitShips.count(status.fleetId)) {
                    if (!replay.standingOrders.count(status.fleetId) && replay.aiOrderFor(*ship) == recordedOrder) continue;
                    explicitShips.insert(status.fleetId);
                }
                const int controller = replay.controllingOwnerId;   // hotseat: a human's order under its own command
                replay.controllingOwnerId = ship->ownerId;
                replay.submitOrder(recordedOrder);
                replay.controllingOwnerId = controller;
            }
            replay.resolveRound();
        }
    } catch (const std::exception&) {
        return false;
    }
    adoptGroupStanding(replay, recorded);
    if (remaster) return tactical::serializeEncounter(replay) == tactical::serializeEncounter(recorded);
    return tactical::detail::writeEncounter(replay, false) == tactical::detail::writeEncounter(recorded, false);
}
} // namespace detail

inline void verifyReplay(const tactical::Encounter& initial, const tactical::Encounter& recorded, const std::set<int>& humans) {
    bool matches = detail::replayMatches(initial, recorded, humans, false);
    if (!matches && recorded.rulesVersion < tactical::RemasterTacticalRules)
        matches = detail::replayMatches(initial, recorded, humans, true);
    require(matches, "Tactical result does not match deterministic order replay");
}

inline void verifyReplay(const tactical::Encounter& initial, const tactical::Encounter& recorded, int player) {
    verifyReplay(initial, recorded, std::set<int>{player});
}

inline std::optional<TurnReport> resumeWorking(TurnSession& session, GameState& committed) {
    require(session.active, "No strategic turn in progress");
    require(fingerprint(committed) == fingerprint(session.baseline), "Campaign changed while resolving tactical combat");
    if (session.pending) {
        if (!session.pending->isFinished) return {};
        session.completed.push_back(*session.pending);
        session.pending.reset();
    }
    auto working = session.baseline;
    std::set<int> humans{working.playerEmpire};   // hotseat: every human's ships carry recorded orders
    humans.insert(working.hotseatHumans.begin(), working.hotseatHumans.end());
    std::size_t encounterIndex = 0;
    // The same resolver as headless advance(); only player encounters are routed here.
    const auto playerEncounter = [&](tactical::Encounter& encounter) {
        if (encounterIndex == session.completed.size()) {
            session.pending = std::move(encounter);
            throw AwaitingPlayer{};
        }
        const auto& recorded = session.completed.at(encounterIndex++);
        require(recorded.isFinished && recorded.initialSeed == encounter.initialSeed &&
            recorded.strategicTurn == encounter.strategicTurn && recorded.sectorX == encounter.sectorX &&
            recorded.sectorY == encounter.sectorY && recorded.ships.size() == encounter.ships.size(),
            "Tactical result does not match strategic encounter");
        for (const auto& ship : encounter.ships) {
            const auto* saved = recorded.getShip(ship.fleetId);
            require(saved && saved->ownerId == ship.ownerId, "Tactical result participant mismatch");
        }
        verifyReplay(encounter, recorded, humans);
        encounter = recorded;
    };
    const auto combat = [&](GameState& state, TurnReport& report) {
        resolveTacticalBattles(state, report, playerEncounter);
        require(encounterIndex == session.completed.size(), "Unexpected extra tactical results");
    };
    try {
        auto report = advanceWithCombat(working, combat);
        committed = std::move(working);
        session.active = false;
        return report;
    } catch (const AwaitingPlayer&) { return {}; }
}

inline std::optional<TurnReport> resume(TurnSession& session, GameState& committed) {
    auto nextSession = session;
    auto nextState = committed;
    auto report = resumeWorking(nextSession, nextState);
    session = std::move(nextSession);
    committed = std::move(nextState);
    return report;
}

inline std::string serialize(const TurnSession& session) {
    require(session.active, "No active turn to save");
    std::ostringstream out;
    const auto write = [&](const std::string& value) { out << value.size() << '\n' << value; };
    out << "BOTF_TURN_SESSION 1\n";
    write(botf::serialize(session.baseline));
    out << '\n' << session.completed.size() << '\n';
    for (const auto& encounter : session.completed) { write(tactical::serializeEncounter(encounter)); out << '\n'; }
    out << (session.pending ? 1 : 0) << '\n';
    if (session.pending) write(tactical::serializeEncounter(*session.pending));
    return out.str();
}
inline TurnSession deserialize(const std::string& text) {
    require(text.size() <= 32 * 1024 * 1024, "Turn session exceeds size limit");
    std::istringstream in(text);
    std::string magic; int version = 0;
    require(static_cast<bool>(in >> magic >> version) && magic == "BOTF_TURN_SESSION" && version == 1, "Invalid turn session header");
    const auto read = [&]() {
        std::size_t size = 0;
        require(static_cast<bool>(in >> size) && size <= 16 * 1024 * 1024, "Invalid turn session block size");
        require(in.get() == '\n', "Invalid turn session separator");
        std::string block(size, '\0');
        in.read(block.data(), static_cast<std::streamsize>(size));
        require(static_cast<std::size_t>(in.gcount()) == size, "Truncated turn session");
        return block;
    };
    TurnSession session;
    // The baseline is a campaign save: botf::deserialize enforces botf::MaxSaveBytes on it.
    session.baseline = botf::deserialize(read()); session.active = true;
    std::size_t count = 0;
    require(static_cast<bool>(in >> count) && count <= 1024, "Invalid encounter count");
    for (std::size_t i = 0; i < count; ++i) {
        auto encounter = tactical::deserializeEncounter(read());
        require(encounter.isFinished, "Stored completed encounter is unfinished");
        session.completed.push_back(std::move(encounter));
    }
    int pending = 0;
    require(static_cast<bool>(in >> pending) && (pending == 0 || pending == 1), "Invalid pending encounter marker");
    if (pending) session.pending = tactical::deserializeEncounter(read());
    in >> std::ws;
    require(in.eof(), "Unexpected trailing turn-session data");
    return session;
}
} // namespace botf::session
