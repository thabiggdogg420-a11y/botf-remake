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
    // Remaster rules: one full round disengaging under fire, then the jump out.
    const auto* disengaging = session.pending->getShip(withdrawing);
    check(disengaging->isAlive && !disengaging->retreated && disengaging->disengaging(),
        "Retreating ship escaped without spending a round disengaging");
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
    for (const auto& prefix : {"", "../Saved/BotfSnapshots/", "Saved/BotfSnapshots/", "game/Saved/BotfSnapshots/", "../../Saved/BotfSnapshots/"}) {
        std::ifstream file(prefix + filename, std::ios::binary);
        if (file) {
            return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        }
    }
    return {};
}

void testSavedPendingBattleSnapshotRegression() {
    const std::string bytes = readSnapshot("VISUAL-QA-PENDING-BATTLE-schema11-20260927.botfsave");
    if (bytes.empty()) {
        std::cout << "SKIP missing fixture VISUAL-QA-PENDING-BATTLE-schema11-20260927.botfsave\n";
        return;
    }
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

botf::TurnReport sessionTurn(botf::GameState& state) {
    botf::session::TurnSession session;
    botf::session::begin(session, state);
    for (int guard = 0; guard < 256; ++guard) {
        if (session.pending && !session.pending->isFinished) botf::session::autoResolve(session);
        if (auto report = botf::session::resume(session, state)) return *report;
    }
    throw std::runtime_error("Session turn did not complete");
}

const botf::Fleet& fleetById(const botf::GameState& state, int id) {
    const auto it = std::find_if(state.fleets.begin(), state.fleets.end(), [id](const botf::Fleet& f) { return f.id == id; });
    check(it != state.fleets.end(), "Fleet missing");
    return *it;
}

// Player ships the tactical AI flies with Charge (beam-heavy) and Circle (torpedo-heavy).
botf::tactical::Encounter chargeAndCircleEncounter() {
    using botf::tactical::Ship;
    botf::tactical::Encounter e;
    e.controllingOwnerId = 1;
    e.initialSeed = e.rngState = 2024;
    e.ships.push_back(Ship{1, 1, "Lancer", botf::Building::Destroyer, -1, 60, 60, 60, 10, 10, 10, 20, 0, 2, {1, 3}, true, true, false, 0});
    e.ships.push_back(Ship{2, 1, "Torpedo Boat", botf::Building::Destroyer, -1, 60, 60, 60, 10, 10, 10, 3, 18, 2, {1, 5}, true, true, false, 0});
    e.ships.push_back(Ship{3, 2, "Raider", botf::Building::Destroyer, -1, 150, 150, 150, 20, 20, 20, 16, 14, 2, {14, 4}, true, true, false, 0});
    e.ships[2].canRetreat = false;
    return e;
}

void testAutoResolvedAiManeuversReplay() {
    // Bug: AI decisions were not stored, so resolution treated AI ships as Engage
    // defensively while replay re-submitted them as explicit Charge/Circle orders.
    const auto initial = chargeAndCircleEncounter();
    auto recorded = initial;
    recorded.runUntilFinished();
    bool sawCharge = false, sawCircle = false;
    for (const auto& round : recorded.rounds) {
        const auto* lancer = round.getStatus(1);
        const auto* boat = round.getStatus(2);
        sawCharge |= lancer && lancer->hullBefore > 0 && lancer->aiControlled && lancer->maneuverExecuted == botf::tactical::TacticalManeuver::Charge;
        sawCircle |= boat && boat->hullBefore > 0 && boat->aiControlled && boat->maneuverExecuted == botf::tactical::TacticalManeuver::Circle;
    }
    check(sawCharge && sawCircle, "Fixture AI did not pick Charge and Circle");
    botf::session::verifyReplay(initial, recorded, 1);

    auto mixed = initial; // Player orders for one round, then AI control.
    mixed.submitOrder({1, 3, botf::tactical::TacticalManeuver::Evade});
    mixed.resolveRound();
    mixed.runUntilFinished();
    botf::session::verifyReplay(initial, mixed, 1);

    auto tampered = recorded;
    tampered.ships[2].hull = tampered.ships[2].hull > 1 ? tampered.ships[2].hull - 1 : 1;
    rejects([&] { botf::session::verifyReplay(initial, tampered, 1); }, "Tampered tactical result verified");

    // Schema-11 (V1) recordings used legacy rules and never marked AI decisions.
    auto legacyRun = initial;
    legacyRun.rulesVersion = botf::tactical::LegacyTacticalRules;
    legacyRun.runUntilFinished();
    auto v1 = botf::tactical::detail::writeEncounter(legacyRun, false);
    const std::string v2Header = "TACTICAL_ENCOUNTER_V2\nRULES 1\nRELATION 1 50\n";
    check(v1.starts_with(v2Header), "Unexpected legacy projection header");
    v1.replace(0, v2Header.size(), "TACTICAL_ENCOUNTER_V1\n");
    const auto legacyRecorded = botf::tactical::deserializeEncounter(v1);
    check(legacyRecorded.rulesVersion == botf::tactical::LegacyTacticalRules, "V1 recording did not keep legacy rules");
    botf::session::verifyReplay(initial, legacyRecorded, 1);
}

void testHeadlessAdvanceMatchesSession() {
    // One resolver: headless advance() and an auto-resolved session commit identical campaigns.
    for (auto state : {playerBattle(), playerBattle(true), rivalBattle()}) {
        check(state.unifiedCombat, "Fixture is not a unified-combat campaign");
        auto headless = state;
        const auto expected = botf::advance(headless);
        const auto actual = sessionTurn(state);
        check(botf::fingerprint(state) == botf::fingerprint(headless) && actual.stateHash == expected.stateHash &&
            actual.battles.size() == expected.battles.size() && !expected.battles.empty(),
            "Headless and session combat diverged");
    }
}

void testRetreatClearedAtDestination() {
    // Bug: the session combat path never cleared a completed strategic retreat.
    auto state = botf::generateCampaign(1999, 2, 16, 12, 28, 1);
    state.aiAggressionEnabled = false;
    auto& fleet = state.fleets.front();
    fleet.retreat = true;
    fleet.targetX = fleet.x; fleet.targetY = fleet.y;
    const int id = fleet.id;
    botf::validate(state);
    auto headless = state;
    sessionTurn(state);
    botf::advance(headless);
    check(!fleetById(state, id).retreat && !fleetById(headless, id).retreat,
        "Strategic retreat was not cleared on arrival");
}

void testThreeEmpireBattlesArePairwise() {
    auto state = botf::generateCampaign(1999, 3, 16, 12, 28, 1);
    state.aiAggressionEnabled = false;
    state.eventsEnabled = false;
    const auto home = state.systems.front();
    for (auto& fleet : state.fleets) {
        if (fleet.ownerId == 1) continue;
        fleet.x = fleet.targetX = home.x;
        fleet.y = fleet.targetY = home.y;
        fleet.attackTarget = 0;
    }
    reveal(state, 2, home.x, home.y);
    reveal(state, 3, home.x, home.y);
    declareWar(state, 1, 2);
    declareWar(state, 1, 3);
    declareWar(state, 2, 3);
    botf::validate(state);
    auto headless = state;
    const auto report = botf::advance(headless);
    check(!report.battles.empty(), "Three-way meeting produced no battle");
    std::pair<int, int> prior{0, 0};
    for (const auto& battle : report.battles) {
        check(battle.x == home.x && battle.y == home.y && battle.firstOwner < battle.secondOwner,
            "Battle is not a single-sector duel");
        const std::pair<int, int> pair{battle.firstOwner, battle.secondOwner};
        check(pair > prior, "Duels were not resolved in empire-id order");
        prior = pair;
    }
    check(report.battles.front().firstOwner == 1 && report.battles.front().secondOwner == 2,
        "The first duel is not between empires 1 and 2");
    botf::validate(headless);
    sessionTurn(state);
    check(botf::fingerprint(state) == botf::fingerprint(headless), "Three-way session diverged from headless");
}

void testStrategicOrdersBecomeStandingOrders() {
    auto state = playerBattle();
    auto& mine = *std::find_if(state.fleets.begin(), state.fleets.end(), [](const botf::Fleet& f) { return f.ownerId == 1; });
    const auto enemy = std::find_if(state.fleets.begin(), state.fleets.end(), [](const botf::Fleet& f) { return f.ownerId == 2; })->id;
    std::vector<int> ids;
    for (const auto& f : state.fleets) if (f.x == mine.x && f.y == mine.y && (f.ownerId == 1 || f.ownerId == 2)) ids.push_back(f.id);
    using botf::tactical::TacticalManeuver;
    const auto standing = [&](botf::TacticalStance stance, bool retreat, int target) {
        auto copy = state;
        auto& f = *std::find_if(copy.fleets.begin(), copy.fleets.end(), [&](const botf::Fleet& x) { return x.id == mine.id; });
        f.stance = stance; f.retreat = retreat; f.attackTarget = target;
        const auto encounter = botf::tactical::constructEncounter(copy, ids, 99);
        const auto it = encounter.standingOrders.find(mine.id);
        return it == encounter.standingOrders.end() ? std::optional<botf::tactical::Order>{} : std::optional{it->second};
    };
    check(!standing(botf::TacticalStance::Balanced, false, 0), "Balanced idle fleet should be AI-controlled");
    check(standing(botf::TacticalStance::Aggressive, false, 0)->maneuver == TacticalManeuver::Charge, "Aggressive stance");
    check(standing(botf::TacticalStance::Defensive, false, 0)->maneuver == TacticalManeuver::Evade, "Defensive stance");
    const auto attack = standing(botf::TacticalStance::Balanced, false, enemy);
    check(attack && attack->maneuver == TacticalManeuver::Engage && attack->targetFleetId == enemy, "Attack target");
    const auto retreat = standing(botf::TacticalStance::Balanced, true, 0);
    check(retreat && retreat->maneuver == TacticalManeuver::Retreat, "Strategic retreat");
}

int failed = 0;

// Tactical rules 3 (Remastered flight model): session and headless agree, player strafe orders
// replay-verify, mid-battle reload matches, and the event stream survives the campaign save.
botf::GameState flightBattle(bool grouped = false) {
    auto state = playerBattle(grouped);
    botf::applyRulesetPreset(state, botf::RulesetPreset::Remastered);
    botf::validate(state);
    return state;
}

void testFlightModelSessionMatchesHeadless() {
    for (auto state : {flightBattle(), flightBattle(true), rivalBattle()}) {
        if (!state.rules.flightModel) botf::applyRulesetPreset(state, botf::RulesetPreset::Remastered);
        auto headless = state;
        const auto expected = botf::advance(headless);
        const auto actual = sessionTurn(state);
        check(botf::fingerprint(state) == botf::fingerprint(headless) && actual.stateHash == expected.stateHash &&
            !expected.battles.empty(), "Flight-model headless and session combat diverged");
        bool fired = false;
        for (const auto& b : expected.battles) for (const auto& r : b.rounds)
            for (const auto& e : r.events) fired |= e.kind == botf::CombatEventKind::Fire;
        check(fired, "Flight-model battle recorded no weapon events");
        // COMBAT_EVENTS round-trips through the campaign save.
        const auto text = botf::serialize(state);
        check(text.find("\nTACTICAL_FLIGHT 1\n") != std::string::npos && text.find("\nCOMBAT_EVENTS ") != std::string::npos,
            "Flight-model save lacks its sections");
        const auto loaded = botf::deserialize(text);
        check(botf::serialize(loaded) == text && loaded.latestBattles.size() == state.latestBattles.size(),
            "Flight-model save did not round-trip");
        for (std::size_t i = 0; i < loaded.latestBattles.size(); ++i)
            for (std::size_t j = 0; j < loaded.latestBattles[i].rounds.size(); ++j)
                check(loaded.latestBattles[i].rounds[j].events == state.latestBattles[i].rounds[j].events, "Events lost in save");
    }
}

void testFlightModelPlayerOrdersReplay() {
    auto uninterrupted = flightBattle();
    botf::session::TurnSession live;
    beginPlayerBattle(live, uninterrupted);
    check(live.pending->rulesVersion == botf::tactical::WegoTacticalRules, "Remastered battle is not rules 4");
    const int player = firstShip(*live.pending, 1);
    const int enemy = firstShip(*live.pending, 2);
    botf::session::order(live, {player, enemy, botf::tactical::TacticalManeuver::Strafe});
    botf::session::resolveRound(live);
    auto reloadedState = botf::deserialize(botf::serialize(uninterrupted));
    auto reloaded = botf::session::deserialize(botf::session::serialize(live));
    check(botf::tactical::serializeEncounter(*reloaded.pending) == botf::tactical::serializeEncounter(*live.pending),
        "Mid-battle save lost flight state");
    const auto liveReport = finish(live, uninterrupted);
    const auto loadedReport = finish(reloaded, reloadedState);
    check(botf::serialize(uninterrupted) == botf::serialize(reloadedState) && liveReport.stateHash == loadedReport.stateHash,
        "Flight-model mid-battle reload diverged");
    bool strafed = false;
    for (const auto& b : liveReport.battles) for (const auto& r : b.rounds) for (const auto& e : r.events)
        strafed |= e.kind == botf::CombatEventKind::ManeuverStart && e.fleetId == player &&
            e.maneuver == static_cast<int>(botf::tactical::TacticalManeuver::Strafe);
    check(strafed, "Player strafe order missing from the event stream");
}

// Tactical rules 4 (WEGO): the Remastered preset switches it on, an aimed order replays, and the TACTICAL_WEGO
// section round-trips. A save without the section loads with it off and fights rules 3.
void testWegoCampaignBattle() {
    auto state = flightBattle();
    check(state.rules.wegoCombat && state.rules.flightModel, "Remastered preset does not set wegoCombat");
    const auto text0 = botf::serialize(state);
    check(text0.find("\nTACTICAL_WEGO 1\n") != std::string::npos, "Remastered save lacks TACTICAL_WEGO");
    check(botf::serialize(botf::deserialize(text0)) == text0, "TACTICAL_WEGO did not round-trip");

    // Old save: a campaign with the flag off writes no TACTICAL_WEGO line, loads with it off and fights rules 3.
    auto flightOnly = state;
    flightOnly.rules.wegoCombat = false;
    const auto oldText = botf::serialize(flightOnly);
    check(oldText.find("TACTICAL_WEGO") == std::string::npos && oldText.find("TACTICAL_FLIGHT 1") != std::string::npos,
        "Flag-off save wrote TACTICAL_WEGO");
    auto old = botf::deserialize(oldText);
    check(!old.rules.wegoCombat && old.rules.flightModel, "Old save did not load with WEGO off");
    botf::session::TurnSession oldLive;
    beginPlayerBattle(oldLive, old);
    check(oldLive.pending->rulesVersion == botf::tactical::FlightTacticalRules, "Old save battle is not rules 3");

    auto uninterrupted = state;
    botf::session::TurnSession live;
    beginPlayerBattle(live, uninterrupted);
    check(live.pending->rulesVersion == botf::tactical::WegoTacticalRules, "Campaign battle is not rules 4");
    const int player = firstShip(*live.pending, 1);
    const int enemy = firstShip(*live.pending, 2);
    botf::tactical::Order aimed{player, enemy, botf::tactical::TacticalManeuver::Engage, botf::tactical::Subsystem::Engines};
    botf::session::order(live, aimed);
    botf::session::resolveRound(live);
    bool aimRecorded = false;
    for (const auto& st : live.pending->rounds.back().shipStatuses)
        aimRecorded |= st.fleetId == player && st.subsystemTarget == botf::tactical::Subsystem::Engines;
    check(aimRecorded, "Aimed order not recorded in the round status");
    auto reloadedState = botf::deserialize(botf::serialize(uninterrupted));
    auto reloaded = botf::session::deserialize(botf::session::serialize(live));
    check(botf::tactical::serializeEncounter(*reloaded.pending) == botf::tactical::serializeEncounter(*live.pending),
        "Mid-battle save lost rules 4 state");
    const auto liveReport = finish(live, uninterrupted);
    const auto loadedReport = finish(reloaded, reloadedState);
    check(botf::serialize(uninterrupted) == botf::serialize(reloadedState) && liveReport.stateHash == loadedReport.stateHash,
        "Rules 4 mid-battle reload diverged (aimed order did not replay)");
}

void run(const char* name, const std::function<void()>& test, int& passed) {
    try {
        test();
        ++passed;
        std::cout << "PASS " << name << std::endl;
    } catch (const std::exception& error) {
        ++failed;
        std::cout << "FAIL " << name << ": " << error.what() << std::endl;
    } catch (...) {
        ++failed;
        std::cout << "FAIL " << name << ": unknown exception" << std::endl;
    }
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
        run("regression: auto-resolved AI Charge/Circle ships verify on replay (V2 and legacy V1)", testAutoResolvedAiManeuversReplay, passed);
        run("unified combat: headless advance matches the auto-resolved session", testHeadlessAdvanceMatchesSession, passed);
        run("regression: session combat clears strategic retreat on arrival", testRetreatClearedAtDestination, passed);
        run("unified combat: three empires in one sector fight pairwise duels in id order", testThreeEmpireBattlesArePairwise, passed);
        run("unified combat: strategic stance, target and retreat become standing orders", testStrategicOrdersBecomeStandingOrders, passed);
        run("rules 3: flight-model session matches headless advance; events survive the save", testFlightModelSessionMatchesHeadless, passed);
        run("rules 3: player strafe orders replay and reload mid-battle", testFlightModelPlayerOrdersReplay, passed);
        run("rules 4: Remastered campaign battle runs WEGO; aimed order replays; save/reload identical", testWegoCampaignBattle, passed);
    } catch (const std::exception& error) {
        std::cerr << "FAIL suite aborted after " << passed << " session scenarios: " << error.what() << '\n';
        return 1;
    }
    std::cout << (failed ? "FAILED: " : "PASS all ") << passed << " passed, " << failed << " failed campaign session scenarios\n";
    return failed ? 1 : 0;
}
