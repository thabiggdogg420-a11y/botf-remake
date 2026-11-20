// Hotseat multiplayer core (MULTIPLAYER.md, decisions of 20 Nov 2026): several humans share one PC; the AI plays the rest.
// Checks: setHotseat validation, the HOTSEAT save section, switchSeat applying the seated human's orders and handing the chair
// over, the AI leaving every human's empire alone during the turn, and one-player games unchanged.
#include "Simulation.h"

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

    std::cout << "hotseat: " << checks - failures << " passed, " << failures << " failed\n";
    return failures ? 1 : 0;
}
