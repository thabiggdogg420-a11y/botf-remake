#pragma once
#include "TacticalCombat.h"
#include <functional>
#include <optional>
#include <set>

namespace botf {
// Implemented in Simulation.cpp: identical strategic phases, with a supplied combat phase.
TurnReport advanceWithCombat(GameState&, const std::function<void(GameState&, TurnReport&)>&);
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
    require(ship && ship->ownerId == session.baseline.playerEmpire, "Cannot command an enemy ship");
    session.pending->submitOrder(command);
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

inline void verifyReplay(const tactical::Encounter& initial, const tactical::Encounter& recorded, int player) {
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
    for (const auto& round : recorded.rounds) {
        for (const auto& status : round.shipStatuses) {
            const auto* ship = replay.getShip(status.fleetId);
            if (ship && ship->ownerId == player && ship->isAlive && !ship->retreated)
                replay.submitOrder({status.fleetId, status.targetFleetId, status.maneuverExecuted});
        }
        replay.resolveRound();
    }
    require(tactical::serializeEncounter(replay) == tactical::serializeEncounter(recorded),
            "Tactical result does not match deterministic order replay");
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
    std::size_t encounterIndex = 0;
    const auto combat = [&](GameState& state, TurnReport& report) {
        std::set<int> withdrawn;
        for (int y = 0; y < state.height; ++y) for (int x = 0; x < state.width; ++x) {
            for (const auto& relation : state.majorRelations) {
                if (!relation.atWar) continue;
                std::vector<int> ids;
                bool first = false, second = false;
                for (const auto& fleet : state.fleets) {
                    if (fleet.hull <= 0 || fleet.x != x || fleet.y != y || withdrawn.count(fleet.id)) continue;
                    if (fleet.ownerId != relation.first && fleet.ownerId != relation.second) continue;
                    ids.push_back(fleet.id);
                    first |= fleet.ownerId == relation.first;
                    second |= fleet.ownerId == relation.second;
                }
                if (!first || !second) continue;
                const auto seed = state.rngState ^ (static_cast<std::uint64_t>(state.turn) << 32)
                    ^ static_cast<std::uint64_t>(y * state.width + x + 1)
                    ^ (static_cast<std::uint64_t>(relation.first * 7 + relation.second) << 16);
                auto encounter = tactical::constructEncounter(state, ids, seed);
                const bool playerInvolved = relation.first == state.playerEmpire || relation.second == state.playerEmpire;
                if (playerInvolved) {
                    if (encounterIndex == session.completed.size()) {
                        session.pending = std::move(encounter);
                        throw AwaitingPlayer{};
                    }
                    const auto& recorded = session.completed.at(encounterIndex++);
                    require(recorded.isFinished && recorded.initialSeed == encounter.initialSeed &&
                        recorded.strategicTurn == encounter.strategicTurn && recorded.sectorX == x && recorded.sectorY == y &&
                        recorded.ships.size() == encounter.ships.size(), "Tactical result does not match strategic encounter");
                    for (const auto& ship : encounter.ships) {
                        const auto* saved = recorded.getShip(ship.fleetId);
                        require(saved && saved->ownerId == ship.ownerId, "Tactical result participant mismatch");
                    }
                    verifyReplay(encounter, recorded, state.playerEmpire);
                    encounter = recorded;
                } else encounter.runUntilFinished();
                tactical::applyEncounterResults(state, encounter);
                for (const auto& ship : encounter.ships) if (ship.retreated) {
                    withdrawn.insert(ship.fleetId);
                    const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& f) { return f.id == ship.fleetId; });
                    require(fleet != state.fleets.end() && fleet->hull > 0, "Tactical withdrawal removed a surviving ship");
                    require(fleet->x != x || fleet->y != y, "No safe strategic withdrawal sector exists");
                }
                // A destroyed or independently withdrawing ship leaves its task force.
                for (auto& group : state.taskForces) {
                    std::erase_if(group.fleetIds, [&](int id) {
                        return withdrawn.count(id) || std::none_of(state.fleets.begin(), state.fleets.end(),
                            [&](const Fleet& fleet) { return fleet.id == id; });
                    });
                }
                std::erase_if(state.taskForces, [](const TaskForce& group) { return group.fleetIds.empty(); });
                for (auto& fleet : state.fleets) if (fleet.taskForceId > 0) {
                    const auto* group = taskForceAt(state, fleet.taskForceId);
                    if (!group || std::find(group->fleetIds.begin(), group->fleetIds.end(), fleet.id) == group->fleetIds.end())
                        fleet.taskForceId = 0;
                }
                report.battles.push_back(tactical::toCombatReport(encounter));
            }
        }
        require(encounterIndex == session.completed.size(), "Unexpected extra tactical results");
        state.latestBattles.insert(state.latestBattles.end(), report.battles.begin(), report.battles.end());
        if (state.latestBattles.size() > 16) state.latestBattles.erase(state.latestBattles.begin(), state.latestBattles.end() - 16);
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
