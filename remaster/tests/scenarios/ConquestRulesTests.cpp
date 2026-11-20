// Conquest rules (Simon's rulings of 20 Nov 2026 on BALANCE-PROPOSALS-F5.md round 4: "C1 yes, C2 yes, C3 yes, C4 keep playing").
// C1: transports landing together on one world add their troops and are all spent. C4: playing on past a domination win
// (PLAYING_ON save section, canPlayOn / playOn). C2 and C3 are AI planning changes measured by the observer soaks.
#include "Simulation.h"

#include <algorithm>
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
}

int main() {
    std::cout << std::unitbuf;
    // ---- C4: the PLAYING_ON section and the play-on gate
    {
        auto s = generateCampaign(2024, 3, 16, 12, 28, 1);
        check(!canPlayOn(s), "an ongoing campaign cannot be played on");
        check(throws([&] { playOn(s); }), "playOn refuses an ongoing campaign");
        const auto plain = fingerprint(s);
        s.playingOnFrom = 37;
        const auto text = serialize(s);
        check(text.find("PLAYING_ON 37") != std::string::npos, "PLAYING_ON is saved");
        const auto back = deserialize(text);
        check(back.playingOnFrom == 37, "PLAYING_ON loads back");
        check(fingerprint(back) != plain, "playing on is part of the fingerprint");
        s.playingOnFrom = 0;
        check(serialize(s).find("PLAYING_ON") == std::string::npos && fingerprint(s) == plain, "a game that never played on saves as before");
        // A human domination win: the player holds three fifths of the systems while a rival still stands.
        auto w = generateCampaign(2024, 3, 16, 12, 28, 1);
        w.outcome = CampaignOutcome::Victory; w.winnerEmpire = w.playerEmpire;
        int need = (static_cast<int>(w.systems.size()) * 3 + 4) / 5, own = 0;
        for (auto& c : w.colonies) if (c.ownerId == w.playerEmpire) ++own;
        const bool feasible = static_cast<int>(w.colonies.size()) > need;
        for (auto& c : w.colonies) if (own < need && c.ownerId != w.playerEmpire && feasible) {
            int left = 0; for (auto& o : w.colonies) if (o.ownerId == c.ownerId) ++left;
            if (left > 1) { c.ownerId = w.playerEmpire; ++own; }
        }
        if (own >= need) {
            check(canPlayOn(w), "a human domination win can be played on");
            playOn(w);
            check(w.outcome == CampaignOutcome::Ongoing && w.playingOnFrom == std::max(1, w.turn) && !canPlayOn(w), "playOn resumes the campaign once");
        } else {
            std::cout << "  note: this galaxy has too few colonies to stage a domination (" << w.colonies.size() << " for " << need << "); gate checked on the ongoing game only\n";
        }
    }
    // ---- C1: two transports at an undefended enemy world assault together
    try {
        auto s = generateCampaign(1999, 3, 16, 12, 28, 1);
        s.aiAggressionEnabled = false; s.eventsEnabled = false;
        auto target = std::find_if(s.colonies.begin(), s.colonies.end(), [&](const Colony& c) { return c.ownerId != s.playerEmpire; });
        const int enemy = target->ownerId, sid = target->systemId;
        const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == sid; });
        for (auto& r : s.majorRelations)
            if ((r.first == s.playerEmpire && r.second == enemy) || (r.second == s.playerEmpire && r.first == enemy)) { r.contact = true; r.atWar = true; r.tradeTreaty = false; r.warStartTurn = s.turn; }
        std::erase_if(s.fleets, [&](const Fleet& f) { return f.ownerId == enemy; });   // nothing guards the world
        s.explored.push_back(loc->y * s.width + loc->x);
        std::sort(s.explored.begin(), s.explored.end());
        s.explored.erase(std::unique(s.explored.begin(), s.explored.end()), s.explored.end());
        auto proto = *std::find_if(s.fleets.begin(), s.fleets.end(), [&](const Fleet& f) { return f.ownerId == s.playerEmpire; });
        for (int i = 0; i < 2; ++i) {
            Fleet t = proto;
            t.id = s.nextFleetId++; t.shipClass = Building::Transport; t.colonyShip = false; t.colonize = false;
            t.x = t.targetX = loc->x; t.y = t.targetY = loc->y; t.attackTarget = 0; t.retreat = false; t.taskForceId = 0;
            t.invadeSystemId = sid;
            s.fleets.push_back(t);
        }
        const int one = 8 + s.fleets.back().hull + 2 * empire(s, s.playerEmpire).technologies[5].level;
        validate(s);
        const auto r = advance(s);
        int left = 0;
        for (const auto& f : s.fleets) if (f.ownerId == s.playerEmpire && f.shipClass == Building::Transport) ++left;
        check(left == 0, "both transports land (and are spent)");
        int mine = 0, strength = 0;
        for (const auto& inv : r.invasions) if (inv.attackerOwner == s.playerEmpire && inv.systemId == sid) { ++mine; strength = inv.attackStrength; }
        check(mine == 1 && strength == 2 * one,
              "one assault with both transports' troops added (" + std::to_string(strength) + " = 2 x " + std::to_string(one) + ")");
    } catch (const std::exception& e) { check(false, std::string("C1 setup threw: ") + e.what()); }
    std::cout << checks - failures << " passed, " << failures << " failed\n";
    return failures ? 1 : 0;
}
