// Conquest rules (Simon's rulings of 20 Nov 2026 on BALANCE-PROPOSALS-F5.md round 4: "C1 yes, C2 yes, C3 yes, C4 keep playing").
// C1: transports landing together on one world add their troops and are all spent. C4: playing on past a domination win
// (PLAYING_ON save section, canPlayOn / playOn). C2 and C3 are AI planning changes measured by the observer soaks.
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
std::string withChecksum(std::string body) {   // FNV-1a over the body, as the save writer seals it
    if (const auto pos = body.rfind("CHECKSUM "); pos != std::string::npos) body.erase(pos);
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
// A campaign with one minor world held by the player as a member (colony on the minor's system, attitude 0, review turn).
GameState heldMinor(bool subjugated, int& sid) {
    auto s = generateCampaign(2024, 3, 16, 12, 28, 1);
    s.aiAggressionEnabled = false; s.eventsEnabled = false;
    s.campaignExpansionEnabled = true; s.modernEnginesActive = true;   // minor reviews run in the modern diplomacy pass
    if (s.minors.empty()) throw std::runtime_error("no minor in this galaxy");
    auto& m = s.minors.front();
    sid = m.systemId;
    m.memberOf = s.playerEmpire; m.subjugated = subjugated;
    m.contact[static_cast<std::size_t>(s.playerEmpire)] = true;
    m.attitude[static_cast<std::size_t>(s.playerEmpire)] = 0;
    m.friendshipTurn[static_cast<std::size_t>(s.playerEmpire)] = subjugated ? -1 : 0;
    Colony c; c.systemId = sid; c.ownerId = s.playerEmpire; c.population = 30; c.foodWorkers = 15; c.industryWorkers = 10; c.researchWorkers = 5; c.morale = 35;
    s.colonies.push_back(c);
    {   // the held world must be explored by its holder
        auto& ex = const_cast<EmpireState&>(empire(s, s.playerEmpire)).explored;
        const auto sys = std::find_if(s.systems.begin(), s.systems.end(), [&](const auto& y) { return y.id == sid; });
        const int cell = sys->y * s.width + sys->x;
        if (!std::binary_search(ex.begin(), ex.end(), cell)) ex.insert(std::lower_bound(ex.begin(), ex.end(), cell), cell);
    }
    std::sort(s.colonies.begin(), s.colonies.end(), [](const Colony& a, const Colony& b) { return a.systemId < b.systemId; });
    s.turn = 4; s.diplomacyEngineState.currentTurn = 4;   // the minor review runs on turns divisible by 5
    validate(s);
    return s;
}
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
    // ---- C7 (21 Nov 2026): the domination setting
    {
        auto s = generateCampaign(2024, 3, 16, 12, 28, 1);
        const auto plain = fingerprint(s);
        check(s.dominationEnabled && serialize(s).find("NO_DOMINATION") == std::string::npos, "domination is on by default and saves as before");
        s.dominationEnabled = false;
        const auto back = deserialize(serialize(s));
        check(!back.dominationEnabled, "NO_DOMINATION loads back");
        check(fingerprint(back) != plain, "the domination setting is part of the fingerprint (LAN seats agree on it)");
        const auto vp = victoryProgress(back, back.playerEmpire);
        const auto dom = std::find_if(vp.begin(), vp.end(), [](const VictoryProgress& v) { return v.condition == VictoryCondition::Domination; });
        check(dom != vp.end() && !dom->enabled && !dom->achieved, "victory progress shows domination off");
        // A staged domination (three fifths of the systems, a rival still standing) on turn 30: with the setting off it does not end the game.
        for (bool on : {true, false}) {
            auto w = generateCampaign(2024, 3, 16, 12, 28, 1);
            w.aiAggressionEnabled = false; w.eventsEnabled = false; w.turn = 30; w.dominationEnabled = on;
            const int need = (static_cast<int>(w.systems.size()) * 3 + 4) / 5;
            int own = 0;
            for (auto& c : w.colonies) if (c.ownerId == w.playerEmpire) ++own;
            for (auto& c : w.colonies) if (own < need && c.ownerId != w.playerEmpire) {
                int left = 0; for (auto& o : w.colonies) if (o.ownerId == c.ownerId) ++left;
                if (left > 1) { c.ownerId = w.playerEmpire; ++own; }
            }
            if (own < need) { std::cout << "  note: too few colonies to stage a domination (" << own << " of " << need << ")\n"; break; }
            advance(w);
            if (on) check(w.outcome == CampaignOutcome::Victory, "domination on: three fifths of the systems wins");
            else check(w.outcome == CampaignOutcome::Ongoing, "domination off: three fifths of the systems does not end the campaign");
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
    // ---- C11 (22 Nov 2026): a minor taken by invasion never secedes; a treaty member still does
    try {
        int sid = 0;
        auto treaty = heldMinor(false, sid);
        advance(treaty);
        check(colonyAt(treaty, sid) == nullptr, "a treaty member at attitude 0 secedes at the review (turn " + std::to_string(treaty.turn) + ", member " + std::to_string(treaty.minors.front().memberOf) + ", attitude " + std::to_string(treaty.minors.front().attitude[static_cast<std::size_t>(treaty.playerEmpire)]) + ", modern " + std::to_string(treaty.modernEnginesActive) + ")");
        auto held = heldMinor(true, sid);
        advance(held);
        const auto* c = colonyAt(held, sid);
        check(c && c->ownerId == held.playerEmpire && held.minors.front().subjugated, "a subjugated minor at attitude 0 stays");
        const auto text = serialize(held);
        check(text.find("\nSUBJUGATED 1 " + std::to_string(sid) + "\n") != std::string::npos, "SUBJUGATED is saved");
        check(deserialize(text).minors.front().subjugated, "SUBJUGATED loads back");
        check(text.starts_with("BOTF_SIM 27 remaster-27\n"), "schema 27 header");
        // Migration: the same game as a schema-25 save (no section). No friendship and no engine membership: conquered.
        std::string old = text;
        old.erase(old.find("SUBJUGATED "), old.find("\n", old.find("SUBJUGATED ")) - old.find("SUBJUGATED ") + 1);
        old.replace(0, std::string("BOTF_SIM 27 remaster-27").size(), "BOTF_SIM 25 remaster-25");
        check(deserialize(withChecksum(old)).minors.front().subjugated, "schema 25: a member with no friendship migrates as conquered");
        auto friendly = held; friendly.minors.front().friendshipTurn[static_cast<std::size_t>(friendly.playerEmpire)] = 2;
        std::string old2 = serialize(friendly);
        old2.erase(old2.find("SUBJUGATED "), old2.find("\n", old2.find("SUBJUGATED ")) - old2.find("SUBJUGATED ") + 1);
        old2.replace(0, std::string("BOTF_SIM 27 remaster-27").size(), "BOTF_SIM 25 remaster-25");
        check(!deserialize(withChecksum(old2)).minors.front().subjugated, "schema 25: a member with a friendship migrates as treaty-joined");
        auto none = heldMinor(false, sid);
        check(serialize(none).find("SUBJUGATED") == std::string::npos, "no subjugated minor: no section");
    } catch (const std::exception& e) { check(false, std::string("C11 setup threw: ") + e.what()); }
    // ---- D1 (22 Nov 2026): debt blocks credit purchases and lowers morale; the AI adds nothing with upkeep while in debt
    try {
        auto s = generateCampaign(2024, 3, 16, 12, 28, 1);
        s.aiAggressionEnabled = false; s.eventsEnabled = false;
        const auto cap = std::find_if(s.colonies.begin(), s.colonies.end(), [&](const Colony& c) { return c.ownerId == s.playerEmpire; });
        const int capId = cap->systemId;
        Command e; e.id = s.lastAcceptedCommand + 1; e.type = CommandType::EnqueueBuilding; e.systemId = capId; e.building = Building::Factory;
        submit(s, e);
        s.credits = -500;
        std::string why;
        try { submit(s, makeBuyConstructionCommand(s.lastAcceptedCommand + 1, capId)); } catch (const std::exception& x) { why = x.what(); }
        check(why == DebtBlocker, "rush-buy refused in debt with the debt reason ('" + why + "')");
        const int before = colonyAt(s, capId)->morale;
        for (int i = 0; i < 5; ++i) { s.credits = -500; advance(s); }
        check(colonyAt(s, capId)->morale < before && colonyAt(s, capId)->morale >= DebtMoraleTarget, "morale falls while in debt (" + std::to_string(before) + " -> " + std::to_string(colonyAt(s, capId)->morale) + ")");
        auto r = generateCampaign(2024, 3, 16, 12, 28, 1);
        r.eventsEnabled = false;
        for (auto& rv : r.rivals) rv.credits = -1000000;
        for (auto& c : r.colonies) if (c.ownerId != r.playerEmpire) c.queue.clear();
        advance(r);
        bool upkeepQueued = false;
        for (const auto& c : r.colonies) if (c.ownerId != r.playerEmpire)
            for (const auto& q : c.queue) if (q.product != Building::Farm && q.product != Building::PowerPlant) upkeepQueued = true;
        check(!upkeepQueued, "an AI in debt queues only farms and power plants");
    } catch (const std::exception& e) { check(false, std::string("D1 setup threw: ") + e.what()); }
    std::cout << checks - failures << " passed, " << failures << " failed\n";
    return failures ? 1 : 0;
}
