// C4 end-to-end fixture (21 Nov 2026, Barclay). Not a rule change: builds saves for the Godot play-on drive.
//   c4_fixture dom <out.sav>            classic seed 7131, Federation human; AIs play 150 turns (observer), then enough rival
//                                       non-minor worlds pass to the Federation that it holds the domination share (60%).
//   c4_fixture conquer <in.sav> <out>   every colony passes to the player (the next end of turn is a conquest win).
#include "Simulation.h"
#include "GameplayInternal.h"
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
using namespace botf;
static std::string slurp(const char* p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }
static void give(GameState& s, Colony& c, int to) {
    c.ownerId = to; c.queue.clear(); c.overflow = 0; c.morale = 50;
    c.workforcePriority = GovernorPriority::Manual; c.queuePriority = GovernorPriority::Manual; c.workforceManualOverride = false;
    for (auto& m : s.minors) if (m.systemId == c.systemId) m.memberOf = to;
}
static void tidy(GameState& s) {
    std::erase_if(s.tradeRoutes, [&](const TradeRoute& r) {
        const auto a = colonyAt(s, r.fromSystemId), b = colonyAt(s, r.toSystemId);
        return !a || !b || a->ownerId != r.ownerId || b->ownerId == r.ownerId; });
}
int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "dom" && argc == 3) {
        CampaignSetup setup; setup.seed = 7131; setup.presetId = "classic"; setup.preset = RulesetPreset::Classic;
        setup.width = 22; setup.height = 16; setup.systems = 50;
        for (const char* r : {"federation", "klingon", "romulan", "cardassian", "ferengi"}) setup.lineup.push_back(r);
        GameState s = newCampaign(setup);
        s.observerMode = true;
        while (s.turn < 150 && s.outcome == CampaignOutcome::Ongoing) advance(s);
        s.observerMode = false;
        if (s.outcome != CampaignOutcome::Ongoing) { std::cerr << "campaign ended early\n"; return 1; }
        const int need = (static_cast<int>(s.systems.size()) * 3 + 4) / 5;
        auto own = [&] { int n = 0; for (auto& c : s.colonies) n += c.ownerId == s.playerEmpire; return n; };
        for (int pass = 0; pass < 2; ++pass)
        for (auto& c : s.colonies) {
            if (own() >= need) break;
            if (c.ownerId == s.playerEmpire) continue;
            int left = 0; for (auto& o : s.colonies) left += o.ownerId == c.ownerId;
            if (left <= 1) continue;   // every rival keeps a foothold: the win is domination, and play-on has someone to fight
            bool minor = false; for (auto& m : s.minors) minor |= m.systemId == c.systemId;
            if (minor != (pass == 1)) continue;   // ordinary worlds first, then member worlds
            give(s, c, s.playerEmpire);
        }
        tidy(s);
        std::set<int> owners; for (auto& c : s.colonies) owners.insert(c.ownerId);
        std::cout << "colonies " << s.colonies.size() << "; turn " << s.turn << " player holds " << own() << " of " << s.systems.size() << " systems (needs " << need << "), " << owners.size() << " owners\n";
        std::ofstream(argv[2], std::ios::binary) << serialize(s);
        return own() >= need ? 0 : 2;
    }
    if (mode == "conquer" && argc == 4) {
        GameState s = deserialize(slurp(argv[2]));
        for (auto& c : s.colonies) if (c.ownerId != s.playerEmpire) give(s, c, s.playerEmpire);
        tidy(s);
        std::cout << "turn " << s.turn << " playingOnFrom " << s.playingOnFrom << ": player holds all " << s.colonies.size() << " colonies\n";
        std::ofstream(argv[3], std::ios::binary) << serialize(s);
        return 0;
    }
    std::cerr << "usage: c4_fixture dom <out> | conquer <in> <out>\n"; return 1;
}
