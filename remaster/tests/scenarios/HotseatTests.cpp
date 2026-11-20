// Hotseat multiplayer core (MULTIPLAYER.md, decisions of 20 Nov 2026): several humans share one PC; the AI plays the rest.
// Checks: setHotseat validation, the HOTSEAT save section, switchSeat applying the seated human's orders and handing the chair
// over, the AI leaving every human's empire alone during the turn, and one-player games unchanged.
#include "CampaignSession.h"
#include "Simulation.h"

#include <algorithm>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace botf;

namespace {
int failures = 0, checks = 0;
void check(bool ok, const std::string& what) {
    ++checks;
    if (ok) std::cout << "  ok  " << what << '\n';
    else { ++failures; std::cout << "  FAIL " << what << '\n'; }
}
template <class F> bool throws(F f) { try { f(); } catch (const std::exception&) { return true; } return false; }

std::uint64_t nextId(const GameState& s) { return s.lastAcceptedCommand + 1; }

void allocate(GameState& s, std::array<int, ResearchFields> a) {
    Command c; c.id = nextId(s); c.type = CommandType::AllocateResearch; c.researchAllocation = a;
    submit(s, c);
}
const EmpireState& seat(const GameState& s, int id) { return empire(s, id); }

// Hotseat battles (21 Nov 2026): empires 2 and 3 at war with ships in one sector; empire 1 is in the chair.
GameState battleOf2And3(std::vector<int> humans) {
    auto state = generateCampaign(1999, 3, 16, 12, 28, 1);
    state.aiAggressionEnabled = false;
    state.eventsEnabled = false;
    auto first = std::find_if(state.fleets.begin(), state.fleets.end(), [](const Fleet& f) { return f.ownerId == 2; });
    auto second = std::find_if(state.fleets.begin(), state.fleets.end(), [](const Fleet& f) { return f.ownerId == 3; });
    second->x = second->targetX = first->x;
    second->y = second->targetY = first->y;
    second->attackTarget = 0;
    second->retreat = false;
    auto& known = std::find_if(state.rivals.begin(), state.rivals.end(), [](const EmpireState& e) { return e.id == 3; })->explored;
    known.push_back(first->y * state.width + first->x);
    std::sort(known.begin(), known.end());
    known.erase(std::unique(known.begin(), known.end()), known.end());
    for (auto& r : state.majorRelations)
        if (r.first == 2 && r.second == 3) { r.contact = true; r.atWar = true; r.tradeTreaty = false; r.warStartTurn = state.turn; }
    validate(state);
    if (!humans.empty()) setHotseat(state, humans);
    return state;
}
int shipOf(const tactical::Encounter& e, int owner) {
    for (const auto& s : e.ships) if (s.ownerId == owner && tactical::Encounter::shipActive(s)) return s.fleetId;
    return 0;
}
}

int main() {
    std::cout << "Hotseat tests\n";
    CampaignSetup setup;
    setup.seed = 21; setup.presetId = "remastered";
    auto s = newCampaign(setup);
    const int p1 = s.playerEmpire;
    int p2 = 0;
    for (const auto& r : s.rivals) { p2 = r.id; break; }
    check(p2 != 0, "the campaign has a rival to seat as a second human");

    const auto oneText = serialize(s);
    setHotseat(s, {});
    check(serialize(s) == oneText && oneText.find("HOTSEAT") == std::string::npos, "one-player game: no hotseat section, no change");
    check(throws([&] { auto t = s; setHotseat(t, {p1}); }), "a hotseat game needs two humans");
    check(throws([&] { auto t = s; setHotseat(t, {p2, 99}); }), "every human must be a seated empire and the player one of them");

    setHotseat(s, {p2, p1});
    check(s.hotseatHumans.size() == 2 && s.hotseatHumans[0] < s.hotseatHumans[1], "humans stored ascending");
    check(humanSeat(s, p1) && humanSeat(s, p2), "both are human seats");
    {
        const auto text = serialize(s);
        auto back = deserialize(text);
        check(text.find("HOTSEAT 2") != std::string::npos && back.hotseatHumans == s.hotseatHumans && fingerprint(back) == fingerprint(s),
              "the HOTSEAT section round-trips");
    }
    check(throws([&] { auto t = s; for (const auto& r : t.rivals) if (r.id != p2) { switchSeat(t, r.id); break; } }), "only a human can take the chair");

    // Seat 1 plans: everything into research field 0. Then the chair passes to seat 2, which plans field 5.
    allocate(s, {100, 0, 0, 0, 0, 0});
    switchSeat(s, p2);
    check(s.playerEmpire == p2 && s.id == p2, "the chair passes to the second human");
    check(s.pending.empty() && seat(s, p1).technologies[0].allocation == 100, "the first human's queued orders were applied on hand-over");
    allocate(s, {0, 0, 0, 0, 0, 100});
    // The turn resolves with seat 2 in the chair; the AI must not re-plan seat 1 (it sets every AI's research each turn).
    advance(s);
    check(seat(s, p1).technologies[0].allocation == 100, "the AI left the first human's research alone");
    check(seat(s, p2).technologies[5].allocation == 100, "the seated human's orders stand");
    check(s.hotseatHumans.size() == 2 && s.outcome == CampaignOutcome::Ongoing, "the game goes on with both humans");
    // Ten more turns, handing the chair back and forth each turn, and the game stays valid and deterministic.
    auto twin = s;
    for (int t = 0; t < 10; ++t) {
        switchSeat(s, s.playerEmpire == p1 ? p2 : p1);
        switchSeat(twin, twin.playerEmpire == p1 ? p2 : p1);
        advance(s); advance(twin);
    }
    check(fingerprint(s) == fingerprint(twin), "hotseat turns are deterministic");
    check(seat(s, p1).technologies[0].allocation == 100 && seat(s, p2).technologies[5].allocation == 100, "neither human was re-planned by the AI in ten turns");
    switchSeat(s, p1);
    check(s.playerEmpire == p1 && s.id == p1, "the chair comes back to the first human");
    setHotseat(s, {});
    check(s.hotseatHumans.empty() && !humanSeat(s, p2), "the game can go back to one player");

    // Battles for a human not in the chair wait for that human instead of being fought by the tactical AI.
    {
        auto solo = battleOf2And3({});
        session::TurnSession t;
        session::begin(t, solo);
        check(session::resume(t, solo).has_value(), "one player: a battle between two AIs is fought by the AI (no pause)");
    }
    {
        auto g = battleOf2And3({1, 2});
        session::TurnSession t;
        session::begin(t, g);
        const auto paused = session::resume(t, g);
        check(!paused && t.pending && t.pending->controllingOwnerId == 2, "hotseat: the battle of the human not in the chair waits for that human");
        const int mine = t.pending ? shipOf(*t.pending, 2) : 0, theirs = t.pending ? shipOf(*t.pending, 3) : 0;
        check(!throws([&] { session::order(t, {mine, theirs, tactical::TacticalManeuver::Engage}); }), "that human orders their own ships");
        check(throws([&] { session::order(t, {theirs, mine, tactical::TacticalManeuver::Engage}); }), "nobody orders the AI's ships");
        session::resolveRound(t);
        session::autoResolve(t);
        const auto done = session::resume(t, g);
        check(done.has_value() && !done->battles.empty() && g.turn == 1, "the battle commits with the turn after the human fought it");
        auto again = battleOf2And3({1, 2});
        advance(again);
        check(again.turn == 1, "headless advance still fights it by the tactical AI");
    }
    {
        auto g = battleOf2And3({1, 2, 3});
        session::TurnSession t;
        session::begin(t, g);
        check(!session::resume(t, g) && t.pending, "a battle between two humans waits for them");
        const int a = t.pending ? shipOf(*t.pending, 2) : 0, b = t.pending ? shipOf(*t.pending, 3) : 0;
        check(!throws([&] { session::order(t, {a, b, tactical::TacticalManeuver::Engage}); }) &&
              !throws([&] { session::order(t, {b, a, tactical::TacticalManeuver::Evade}); }), "each human orders their own ships in the same round");
        check(t.pending && t.pending->controllingOwnerId == 2, "the encounter's controller is unchanged by the other human's order");
        session::resolveRound(t);
        session::autoResolve(t);
        const auto done = session::resume(t, g);
        check(done.has_value() && g.turn == 1, "both humans' recorded orders replay and the turn commits");
    }

    std::cout << "hotseat: " << checks - failures << " passed, " << failures << " failed\n";
    return failures ? 1 : 0;
}
