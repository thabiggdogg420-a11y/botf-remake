#include "CampaignSession.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void rejects(const std::function<void()>& action, const std::string& message) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    check(rejected, message);
}

void reveal(botf::GameState& state, int owner, int x, int y) {
    auto& known = owner == state.playerEmpire ? state.explored :
        std::find_if(state.rivals.begin(), state.rivals.end(),
            [owner](const botf::EmpireState& e) { return e.id == owner; })->explored;
    known.push_back(y * state.width + x);
    std::sort(known.begin(), known.end());
    known.erase(std::unique(known.begin(), known.end()), known.end());
}

void declareWar(botf::GameState& state, int a, int b) {
    auto it = std::find_if(state.majorRelations.begin(), state.majorRelations.end(),
        [a, b](const botf::MajorRelation& r) {
            return r.first == std::min(a, b) && r.second == std::max(a, b);
        });
    check(it != state.majorRelations.end(), "War fixture lacks relation");
    it->contact = true;
    it->atWar = true;
    it->tradeTreaty = false;
    it->warStartTurn = state.turn;
}

botf::GameState playerBattle(bool grouped = false) {
    auto state = botf::generateCampaign(1999, 2, 16, 12, 28, 1);
    state.aiAggressionEnabled = false;
    state.eventsEnabled = false;
    const auto home = state.systems.front();
    auto rival = std::find_if(state.fleets.begin(), state.fleets.end(),
        [](const botf::Fleet& f) { return f.ownerId == 2; });
    check(rival != state.fleets.end(), "Battle fixture lacks rival fleet");
    rival->x = rival->targetX = home.x;
    rival->y = rival->targetY = home.y;
    rival->attackTarget = 0;
    rival->retreat = false;
    reveal(state, 2, home.x, home.y);
    declareWar(state, 1, 2);

    if (grouped) {
        std::vector<int> members;
        for (auto& fleet : state.fleets) {
            if (fleet.ownerId == 1 && fleet.x == home.x && fleet.y == home.y)
                members.push_back(fleet.id);
        }
        check(members.size() >= 2, "Group fixture lacks friendly ships");
        botf::TaskForce group;
        group.id = state.nextTaskForceId++;
        group.ownerId = 1;
        group.name = "Session Group";
        group.fleetIds = members;
        group.targetX = home.x;
        group.targetY = home.y;
        for (auto& fleet : state.fleets)
            if (std::find(members.begin(), members.end(), fleet.id) != members.end())
                fleet.taskForceId = group.id;
        state.taskForces.push_back(std::move(group));
    }
    botf::validate(state);
    return state;
}

botf::GameState rivalBattle() {
    auto state = botf::generateCampaign(1999, 3, 16, 12, 28, 1);
    state.aiAggressionEnabled = false;
    state.eventsEnabled = false;
    auto first = std::find_if(state.fleets.begin(), state.fleets.end(),
        [](const botf::Fleet& f) { return f.ownerId == 2; });
    auto second = std::find_if(state.fleets.begin(), state.fleets.end(),
        [](const botf::Fleet& f) { return f.ownerId == 3; });
    check(first != state.fleets.end() && second != state.fleets.end(), "Rival fixture lacks fleets");
    second->x = second->targetX = first->x;
    second->y = second->targetY = first->y;
    second->attackTarget = 0;
    second->retreat = false;
    reveal(state, 3, first->x, first->y);
    declareWar(state, 2, 3);
    botf::validate(state);
    return state;
}

int firstShip(const botf::tactical::Encounter& encounter, int owner) {
    const auto it = std::find_if(encounter.ships.begin(), encounter.ships.end(),
        [owner](const botf::tactical::Ship& s) { return s.ownerId == owner; });
    check(it != encounter.ships.end(), "Encounter lacks expected owner");
    return it->fleetId;
}

void beginPlayerBattle(botf::session::TurnSession& session, botf::GameState& committed) {
    botf::session::begin(session, committed);
    const auto before = botf::serialize(committed);
    const auto report = botf::session::resume(session, committed);
    check(!report && session.active && session.pending.has_value(), "Player encounter did not pause");
    check(botf::serialize(committed) == before, "Pause changed the committed campaign");
}

botf::TurnReport finish(botf::session::TurnSession& session, botf::GameState& committed) {
    if (session.pending && !session.pending->isFinished)
        botf::session::autoResolve(session);
    const auto report = botf::session::resume(session, committed);
    check(report.has_value() && !session.active && !session.pending,
        "Resolved player encounter did not commit exactly one turn");
    botf::validate(committed);
    return *report;
}

void testOrdinaryTurnAndRivalOnlyBattle() {
    auto baseline = botf::generateCampaign(1999, 2, 16, 12, 28, 1);
    baseline.aiAggressionEnabled = false;
    auto reference = baseline;
    const auto expected = botf::advanceWithCombat(reference,
        [](botf::GameState&, botf::TurnReport&) {});
    botf::session::TurnSession session;
    botf::session::begin(session, baseline);
    const auto actual = botf::session::resume(session, baseline);
    check(actual.has_value() && !session.active && !session.pending,
        "Ordinary turn unexpectedly paused");
    check(botf::serialize(baseline) == botf::serialize(reference) &&
        actual->stateHash == expected.stateHash && actual->appliedCommands == expected.appliedCommands,
        "Session replay changed an ordinary turn or applied commands twice");
    check(baseline.turn == reference.turn, "Ordinary turn incremented twice");

    auto rivals = rivalBattle();
    botf::session::begin(session, rivals);
    const auto rivalReport = botf::session::resume(session, rivals);
    check(rivalReport.has_value() && !session.active && !session.pending,
        "Rival-only battle incorrectly requested player orders");
    check(rivals.turn == 1 && !rivalReport->battles.empty(),
        "Rival-only battle did not commit once with a combat report");
    check(rivals.latestBattles.size() == rivalReport->battles.size(),
        "Rival-only combat history was duplicated");
    botf::validate(rivals);
}

void testPlayerPauseAutoResolveAndAuthorization() {
    auto state = playerBattle();
    const auto baseline = botf::serialize(state);
    botf::session::TurnSession session;
    beginPlayerBattle(session, state);
    check(botf::serialize(state) == baseline, "Player pause changed the baseline");
    const int enemy = firstShip(*session.pending, 2);
    const auto beforeOrders = botf::tactical::serializeEncounter(*session.pending);
    rejects([&] { botf::session::order(session,
        {enemy, 0, botf::tactical::TacticalManeuver::Hold}); },
        "Player could command an enemy ship");
    check(botf::tactical::serializeEncounter(*session.pending) == beforeOrders,
        "Rejected enemy order altered pending combat");
    const auto report = finish(session, state);
    check(state.turn == 1 && report.battles.size() == 1 &&
        state.latestBattles.size() == 1, "Combat commit or history occurred more than once");
}

void testMidRoundSaveReloadMatchesUninterrupted() {
    auto uninterrupted = playerBattle();
    botf::session::TurnSession live;
    beginPlayerBattle(live, uninterrupted);
    const int player = firstShip(*live.pending, 1);
    botf::session::order(live, {player, 0, botf::tactical::TacticalManeuver::Hold});
    botf::session::resolveRound(live);
    check(live.pending && !live.pending->isFinished && live.pending->currentRound == 1,
        "Fixture did not reach a live mid-battle round");
    check(uninterrupted.turn == 0, "Tactical round prematurely committed a strategic turn");

    auto reloadedState = botf::deserialize(botf::serialize(uninterrupted));
    auto reloaded = botf::session::deserialize(botf::session::serialize(live));
    check(botf::tactical::serializeEncounter(*reloaded.pending) ==
        botf::tactical::serializeEncounter(*live.pending), "Mid-battle save lost tactical state");
    const auto liveReport = finish(live, uninterrupted);
    const auto loadedReport = finish(reloaded, reloadedState);
    check(botf::serialize(uninterrupted) == botf::serialize(reloadedState) &&
        liveReport.stateHash == loadedReport.stateHash,
        "Mid-battle reload diverged from uninterrupted deterministic replay");
}

void testChangedBaselineAndInvalidReplayAreAtomic() {
    auto state = playerBattle();
    botf::session::TurnSession session;
    beginPlayerBattle(session, state);
    auto changed = state;
    changed.credits += 1;
    const auto changedBefore = botf::serialize(changed);
    const auto sessionBefore = botf::session::serialize(session);
    rejects([&] { (void)botf::session::resume(session, changed); },
        "Changed committed campaign was accepted as session baseline");
    check(botf::serialize(changed) == changedBefore &&
        botf::session::serialize(session) == sessionBefore,
        "Rejected baseline mismatch changed state or session");

    botf::session::autoResolve(session);
    check(session.pending && session.pending->isFinished, "Fixture combat did not finish");
    session.pending->winnerOwnerId = session.pending->winnerOwnerId == 1 ? 2 : 1;
    const auto stateBefore = botf::serialize(state);
    const auto corruptBefore = botf::session::serialize(session);
    rejects([&] { (void)botf::session::resume(session, state); },
        "Altered tactical result passed deterministic replay");
    check(botf::serialize(state) == stateBefore &&
        botf::session::serialize(session) == corruptBefore,
        "Rejected replay mismatch partially mutated session or campaign");
}

void testPartialGroupRetreat() {
    auto state = playerBattle(true);
    const auto originalGroup = state.taskForces.front();
    botf::session::TurnSession session;
    beginPlayerBattle(session, state);
    const int withdrawing = originalGroup.fleetIds.front();
    botf::session::order(session,
        {withdrawing, 0, botf::tactical::TacticalManeuver::Retreat});
    botf::session::resolveRound(session);
    check(session.pending->getShip(withdrawing)->retreated,
        "Fixture ship did not withdraw from the battle");
    const auto report = finish(session, state);
    check(std::find(report.battles[0].retreatedFleetIds.begin(),
        report.battles[0].retreatedFleetIds.end(), withdrawing) !=
        report.battles[0].retreatedFleetIds.end(), "Withdrawal missing from combat report");
    const auto survivor = std::find_if(state.fleets.begin(), state.fleets.end(),
        [withdrawing](const botf::Fleet& f) { return f.id == withdrawing; });
    check(survivor != state.fleets.end(), "Tactical withdrawal destroyed campaign fleet");
    check(survivor->x != session.baseline.fleets.front().x ||
        survivor->y != session.baseline.fleets.front().y,
        "Withdrawn fleet stayed in the hostile sector");
    for (const auto& group : state.taskForces) {
        for (int id : group.fleetIds) {
            const auto member = std::find_if(state.fleets.begin(), state.fleets.end(),
                [id](const botf::Fleet& f) { return f.id == id; });
            check(member != state.fleets.end() && member->taskForceId == group.id,
                "Task force retained a lost or detached fleet");
        }
        check(std::find(group.fleetIds.begin(), group.fleetIds.end(), withdrawing) ==
            group.fleetIds.end(), "Independent retreater remained in its task force");
    }
    botf::validate(state);
}

void testPartialGroupLoss() {
    auto state = playerBattle(true);
    const int vulnerable = state.taskForces.front().fleetIds.front();
    auto ally = std::find_if(state.fleets.begin(), state.fleets.end(),
        [vulnerable](const botf::Fleet& f) { return f.id == vulnerable; });
    auto enemy = std::find_if(state.fleets.begin(), state.fleets.end(),
        [](const botf::Fleet& f) { return f.ownerId == 2; });
    check(ally != state.fleets.end() && enemy != state.fleets.end(),
        "Loss fixture lacks participants");
    ally->hull = 1;
    enemy->shipClass = botf::Building::Destroyer;
    enemy->colonyShip = false;
    enemy->hull = botf::fleetMaxHull(*enemy);
    botf::validate(state);

    botf::session::TurnSession session;
    beginPlayerBattle(session, state);
    for (int round = 0; round < session.pending->maxRounds &&
         !session.pending->isFinished; ++round) {
        const auto* ship = session.pending->getShip(vulnerable);
        if (!ship || !ship->isAlive || ship->retreated) break;
        botf::session::order(session,
            {vulnerable, 0, botf::tactical::TacticalManeuver::Hold});
        botf::session::resolveRound(session);
    }
    const auto* fallen = session.pending->getShip(vulnerable);
    check(fallen && !fallen->isAlive && !fallen->retreated,
        "Loss fixture failed to destroy its selected group member");
    const auto report = finish(session, state);
    check(std::find(report.battles[0].lostFleetIds.begin(),
        report.battles[0].lostFleetIds.end(), vulnerable) !=
        report.battles[0].lostFleetIds.end(), "Group loss missing from combat report");
    check(std::none_of(state.fleets.begin(), state.fleets.end(),
        [vulnerable](const botf::Fleet& f) { return f.id == vulnerable; }),
        "Destroyed group member survived in campaign state");
    for (const auto& group : state.taskForces)
        check(std::find(group.fleetIds.begin(), group.fleetIds.end(), vulnerable) ==
            group.fleetIds.end(), "Task force retained a destroyed member");
    botf::validate(state);
}

std::string readSnapshot(const std::string& filename) {
    for (const auto& prefix : {"../Saved/BotfSnapshots/", "Saved/BotfSnapshots/", "game/Saved/BotfSnapshots/", "../../Saved/BotfSnapshots/"}) {
        std::ifstream file(prefix + filename, std::ios::binary);
        if (file) {
            return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        }
    }
    throw std::runtime_error("Cannot locate snapshot file: " + filename);
}

void testSavedPendingBattleSnapshotRegression() {
    const std::string bytes = readSnapshot("VISUAL-QA-PENDING-BATTLE-schema11-20260927.botfsave");
    check(bytes.starts_with("BOTF_TURN_SESSION"), "Pending battle snapshot lacks turn session header");

    auto session = botf::session::deserialize(bytes);
    check(session.active, "Pending battle snapshot is not active");
    check(session.pending.has_value(), "Pending battle snapshot lacks pending tactical encounter");
    check(session.pending->currentRound == 1, "Pending battle snapshot expected round 1");
    check(!session.pending->isFinished, "Pending battle snapshot should not be finished");

    auto committed = session.baseline;
    const int beforeTurn = committed.turn;
    int steps = 0;
    while (session.active && steps++ < 64) {
        if (session.pending && !session.pending->isFinished) {
            botf::session::autoResolve(session);
        }
        const auto report = botf::session::resume(session, committed);
        if (report) break;
    }
    check(!session.active, "Session remained active after resuming pending battle snapshot");
    check(committed.turn == beforeTurn + 1, "Turn did not advance by 1 after resuming pending battle");
    botf::validate(committed);
}

void run(const char* name, const std::function<void()>& test, int& passed) {
    test();
    ++passed;
    std::cout << "PASS " << name << '\n';
}
}

int main() {
    int passed = 0;
    try {
        run("ordinary turn and rival-only combat commit once", testOrdinaryTurnAndRivalOnlyBattle, passed);
        run("player encounter pauses, rejects enemy orders, and auto-resolves", testPlayerPauseAutoResolveAndAuthorization, passed);
        run("mid-round save/reload matches uninterrupted combat", testMidRoundSaveReloadMatchesUninterrupted, passed);
        run("changed baseline and invalid replay reject atomically", testChangedBaselineAndInvalidReplayAreAtomic, passed);
        run("partial task-force retreat preserves campaign invariants", testPartialGroupRetreat, passed);
        run("partial task-force loss removes the destroyed member", testPartialGroupLoss, passed);
        run("saved pending battle snapshot resumes deterministically", testSavedPendingBattleSnapshotRegression, passed);
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << passed << " session scenarios: " << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS all " << passed << " campaign session scenarios\n";
    return 0;
}
