// Round 9 (Simon's delegation, 22 Nov 2026, botf-rulings.md E1-E6): the core rules that came with it.
//   E3 peace deals can cede worlds (war score, AI offers and accepts, schema 27 save and migration),
//   E4 the flagship can be renamed (saved, shown in battle names), E5 the science megaproject is named per power,
//   E2 the invasion odds shown before an assault match what the resolution then uses.
#include "Scenarios.h"
#include "Simulation.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

using namespace botf;
using diplomacy::ParticipantId;
using diplomacy::TreatyType;

namespace {
int failures = 0, checks = 0;
void check(bool ok, const std::string& what) {
    ++checks;
    if (ok) std::cout << "  ok  " << what << '\n';
    else { ++failures; std::cout << "  FAIL " << what << '\n'; }
}
std::string withChecksum(std::string body) {   // FNV-1a over the body, as the save writer seals it
    if (const auto pos = body.rfind("CHECKSUM "); pos != std::string::npos) body.erase(pos);
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
bool loads(const std::string& text) { try { (void)deserialize(text); return true; } catch (const std::exception&) { return false; } }

// A context for the pure engine: who holds which worlds, and how strong each side is.
struct WorldContext : diplomacy::SimpleDiplomacyContext {
    std::map<int, int> colonies;                 // empire -> world count
    std::map<int, std::set<int>> worlds;         // empire -> cedable worlds
    int colonyCount(ParticipantId p) const override { auto it = colonies.find(p.id); return it == colonies.end() ? -1 : it->second; }
    bool canCedeWorld(ParticipantId p, int id) const override { auto it = worlds.find(p.id); return it != worlds.end() && it->second.count(id) > 0; }
    std::vector<int> borderWorlds(ParticipantId p, ParticipantId, int maxCount) const override {
        std::vector<int> out;
        auto it = worlds.find(p.id);
        if (it != worlds.end()) for (int id : it->second) if (static_cast<int>(out.size()) < maxCount) out.push_back(id);
        return out;
    }
};
WorldContext warContext(int64_t powerA, int64_t powerB) {   // empire 1 (human) at war with empire 2 (AI)
    WorldContext c;
    c.setContact(ParticipantId::major(1), ParticipantId::major(2), true);
    c.setWar(ParticipantId::major(1), ParticipantId::major(2), true);
    c.setTrust(ParticipantId::major(1), ParticipantId::major(2), 60);
    c.setPower(ParticipantId::major(1), powerA, 100);
    c.setPower(ParticipantId::major(2), powerB, 100);
    c.majors = {1, 2};
    c.warDurations[{ParticipantId::major(1), ParticipantId::major(2)}] = 40;
    c.warDurations[{ParticipantId::major(2), ParticipantId::major(1)}] = 40;
    c.colonies = {{1, 8}, {2, 3}};
    c.worlds = {{1, {11, 12, 13, 14}}, {2, {21, 22}}};
    return c;
}

MajorRelation& relationOf(GameState& s, int a, int b) {
    if (a > b) std::swap(a, b);
    for (auto& r : s.majorRelations) if (r.first == a && r.second == b) return r;
    throw std::runtime_error("missing relation");
}
// Three empires, five colonies each; the player (1) and empire 2 at war, everyone in contact.
GameState warGame() {
    CampaignSetup setup;
    setup.seed = 2026; setup.empires = 3; setup.startSystems = 4; setup.systems = 40; setup.width = 20; setup.height = 14;
    auto s = newCampaign(setup);
    s.eventsEnabled = false; s.aiAggressionEnabled = false;
    for (auto& r : s.majorRelations) { r.contact = true; r.trust = 90; }
    auto& war = relationOf(s, 1, 2);
    war.atWar = true; war.tradeTreaty = false; war.warStartTurn = 0;
    s.turn = 12; s.diplomacyEngineState.currentTurn = 12;
    // Enough credits that nobody is in debt, and everybody has met everybody's worlds.
    return s;
}
int coloniesOf(const GameState& s, int owner) {
    return static_cast<int>(std::count_if(s.colonies.begin(), s.colonies.end(), [owner](const Colony& c) { return c.ownerId == owner; }));
}
int cap1Id(const GameState& s) { for (const auto& sys : s.systems) if (sys.name == slots::capital(1)) return sys.id; return 0; }
const System* systemNamed(const GameState& s, const std::string& name) { for (const auto& sys : s.systems) if (sys.name == name) return &sys; return nullptr; }
}

int main() {
    std::cout << std::unitbuf;
    const auto p1 = ParticipantId::major(1), p2 = ParticipantId::major(2);

    // ---- E3, engine level: war score, validation, the AI's weighing, offers and the serialised terms
    {
        auto strong = warContext(300, 100);   // empire 1 far ahead of empire 2
        check(diplomacy::warScore(strong, p1, p2) > 40 && diplomacy::warScore(strong, p2, p1) < -40, "war score: the stronger side with more worlds scores high, the other low and mirrored");
        auto even = warContext(100, 100);
        even.colonies = {{1, 4}, {2, 4}};
        check(diplomacy::warScore(even, p1, p2) == 0, "war score: equal fleets and worlds is 0");
        check(diplomacy::warScore(diplomacy::SimpleDiplomacyContext(), p1, p2) == 0, "war score: a context that knows neither fleets nor worlds scores 0");

        diplomacy::DiplomacyState ds; ds.currentTurn = 50;
        std::string why;
        diplomacy::Proposal peace;
        peace.id = 1; peace.proposer = p2; peace.target = p1; peace.treatyType = TreatyType::Peace; peace.submittedTurn = 50; peace.dueTurn = 52;
        check(diplomacy::validateProposalCreation(ds, strong, peace, &why), "plain peace validates");
        auto terms = peace; terms.cedeSystems = {21};
        check(diplomacy::validateProposalCreation(ds, strong, terms, &why), "peace with one ceded world validates");
        auto notMine = peace; notMine.cedeSystems = {99};
        check(!diplomacy::validateProposalCreation(ds, strong, notMine, &why) && contains(why, "cannot be ceded"), "a world the proposer does not hold cannot be ceded ('" + why + "')");
        auto demand = peace; demand.demandSystems = {11};
        check(diplomacy::validateProposalCreation(ds, strong, demand, &why), "peace demanding one of the target's worlds validates");
        auto tooMany = peace; tooMany.demandSystems = {11, 12, 13, 14};
        check(!diplomacy::validateProposalCreation(ds, strong, tooMany, &why), "more than three worlds is refused");
        auto dup = peace; dup.cedeSystems = {21, 21};
        check(!diplomacy::validateProposalCreation(ds, strong, dup, &why), "the same world twice is refused");
        auto notPeace = peace; notPeace.treatyType = TreatyType::NonAggression; notPeace.cedeSystems = {21};
        check(!diplomacy::validateProposalCreation(ds, strong, notPeace, &why), "only a peace deal can move worlds");
        auto allWorlds = peace; allWorlds.cedeSystems = {21, 22};   // empire 2 holds 3 worlds, two cedable: ceding both leaves one
        check(diplomacy::validateProposalCreation(ds, strong, allWorlds, &why), "ceding all but one world is allowed");
        strong.colonies[2] = 2;
        check(!diplomacy::validateProposalCreation(ds, strong, allWorlds, &why) && contains(why, "all its worlds"), "ceding every world is refused ('" + why + "')");
        strong.colonies[2] = 3;
        diplomacy::SimpleDiplomacyContext plain;   // a harness that cannot name worlds
        plain.setContact(p1, p2, true); plain.setWar(p1, p2, true);
        check(!diplomacy::validateProposalCreation(ds, plain, terms, &why), "a context with no worlds refuses world terms");

        // The AI weighs the worlds. Empire 1 is the strong side here; empire 2 (AI) is asked for peace by the human.
        const auto base = diplomacy::evaluateAiProposal(ds, strong, [&] { auto p = peace; p.proposer = p1; p.target = p2; return p; }(), 77, diplomacy::MajorPersonality::Federation);
        auto offer = peace; offer.proposer = p1; offer.target = p2; offer.cedeSystems = {11};
        const auto withOffer = diplomacy::evaluateAiProposal(ds, strong, offer, 77, diplomacy::MajorPersonality::Federation);
        check(withOffer.score > base.score, "the AI scores a peace higher when worlds are offered (" + std::to_string(base.score) + " -> " + std::to_string(withOffer.score) + ")");
        auto ask = peace; ask.proposer = p1; ask.target = p2; ask.demandSystems = {21};
        const auto losingAsked = diplomacy::evaluateAiProposal(ds, strong, ask, 77, diplomacy::MajorPersonality::Federation);   // empire 2 is losing badly
        auto winnerCtx = warContext(100, 300);   // now empire 2 is the strong one
        const auto winningAsked = diplomacy::evaluateAiProposal(ds, winnerCtx, ask, 77, diplomacy::MajorPersonality::Federation);
        check(losingAsked.score > winningAsked.score + 20, "asked for a world, a losing AI weighs it far kinder than a winning AI (" + std::to_string(losingAsked.score) + " vs " + std::to_string(winningAsked.score) + ")");
        check(winningAsked.decision != diplomacy::AiDecision::Accept, "a winning AI refuses to give up a world");

        // The AI offers by war score: a losing AI proposes peace with border worlds; a merely weary one does not.
        auto lose = warContext(500, 40);   // empire 2 (AI) is crushed
        diplomacy::DiplomacyState planState; planState.currentTurn = 80;
        const auto intents = diplomacy::planAiDiplomacy(planState, lose, {2}, 5);
        bool offered = false;
        for (const auto& in : intents) if (in.treatyType == TreatyType::Peace && in.proposer == p2 && !in.cedeSystems.empty()) offered = true;
        check(offered, "a crushed AI proposes peace and offers border worlds");
        auto mild = warContext(110, 100); mild.colonies = {{1, 4}, {2, 4}};
        const auto mildIntents = diplomacy::planAiDiplomacy(planState, mild, {2}, 5);
        bool mildOffered = false;
        for (const auto& in : mildIntents) if (!in.cedeSystems.empty()) mildOffered = true;
        check(!mildOffered, "an AI that is not losing offers no worlds");

        // The serialised terms round-trip, and a state without terms serialises as it always did.
        diplomacy::DiplomacyState saved; saved.currentTurn = 3; saved.nextProposalId = 3;
        diplomacy::Proposal a = peace; a.id = 1; a.proposer = p1; a.target = p2; a.cedeSystems = {11, 12}; a.demandSystems = {21}; a.submittedTurn = 3; a.dueTurn = 4;
        diplomacy::Proposal b = peace; b.id = 2; b.proposer = p2; b.target = p1; b.submittedTurn = 3; b.dueTurn = 4;
        saved.proposals = {a, b};
        const std::string text = diplomacy::serializeDiplomacyState(saved);
        check(contains(text, "proposalTerms 1\nw 1 2 11 12 1 21\n"), "the terms are written in their own block");
        diplomacy::DiplomacyState back; std::string err;
        check(diplomacy::deserializeDiplomacyState(text, back, &err) && back.proposals == saved.proposals, "the terms read back equal (" + err + ")");
        auto plainState = saved; plainState.proposals = {b};
        check(!contains(diplomacy::serializeDiplomacyState(plainState), "proposalTerms"), "no terms: no block, the old text");
        std::string forged = text; forged.replace(forged.find("w 1 2 11 12 1 21"), 16, "w 2 2 11 12 1 21");   // names the proposal with no terms... (peace, so legal) but then a second row would clash
        check(diplomacy::deserializeDiplomacyState(forged, back, &err), "terms may sit on any peace proposal");
        std::string bad = text; bad.replace(bad.find("w 1 2 11 12 1 21"), 16, "w 1 9 11 12 1 21");
        check(!diplomacy::deserializeDiplomacyState(bad, back, &err), "a forged world count is refused");
    }

    // ---- E3, campaign level: a peace deal moves worlds, through the real turn, the save and the migration
    try {
        auto s = warGame();
        const auto cedable = cedableWorlds(s, 1);
        const auto* cap = systemNamed(s, slots::capital(1));
        check(cap != nullptr && cedable.size() == 4 && std::none_of(cedable.begin(), cedable.end(), [&](int id) { return id == cap->id; }), "a player with five worlds can cede four: never the capital");
        check(std::abs(warScoreOf(s, 1, 2) + warScoreOf(s, 2, 1)) <= 1, "the war score is symmetric");
        const int worldToGive = cedable.front();
        const int before1 = coloniesOf(s, 1), before2 = coloniesOf(s, 2);
        for (auto& c : s.colonies) if (c.systemId == worldToGive) { c.queue.push_back(Construction(Building::Farm, 0, -1)); c.morale = 90; c.workforcePriority = GovernorPriority::Growth; }
        // The capital cannot be offered, and a world the player does not hold cannot be offered.
        {
            auto t = s; bool refused = false;
            try { submit(t, makeProposePeaceTermsCommand(t.lastAcceptedCommand + 1, 2, {cap->id}, {})); advance(t); } catch (const std::exception&) { refused = true; }
            check(refused, "the capital cannot be ceded in a peace deal");
            auto u = s; refused = false;
            const auto theirsList = cedableWorlds(s, 2);
            try { submit(u, makeProposePeaceTermsCommand(u.lastAcceptedCommand + 1, 2, {theirsList.front()}, {})); advance(u); } catch (const std::exception&) { refused = true; }
            check(refused, "a world the player does not hold cannot be ceded");
        }
        submit(s, makeProposePeaceTermsCommand(s.lastAcceptedCommand + 1, 2, {worldToGive}, {}));
        const std::string pendingText = serialize(s);
        check(pendingText.starts_with("BOTF_SIM 27 remaster-27\n"), "schema 27 header");
        const auto pendingBack = deserialize(pendingText);
        check(pendingBack.pending.size() == 1 && pendingBack.pending.front().proposalCedeSystems == std::vector<int>{worldToGive} &&
              serialize(pendingBack) == pendingText && fingerprint(pendingBack) == fingerprint(s), "a pending peace-with-worlds order saves and loads exactly");
        {   // The tail is a schema-27 feature: dressed as a schema-26 save it must be refused.
            std::string old = pendingText; old.replace(0, std::string("BOTF_SIM 27 remaster-27").size(), "BOTF_SIM 26 remaster-26");
            check(!loads(withChecksum(old)), "a schema-26 save carrying world terms is refused");
        }
        TurnReport report = advance(s);
        const auto* given = colonyAt(s, worldToGive);
        check(!relationOf(s, 1, 2).atWar, "the war ended");
        check(given && given->ownerId == 2, "the ceded world belongs to the other side now");
        check(coloniesOf(s, 1) == before1 - 1 && coloniesOf(s, 2) == before2 + 1, "one world moved, none made or lost");
        check(given && given->morale <= 45 && given->workforcePriority == GovernorPriority::Manual,
              "it arrives with shaken morale (" + std::to_string(given ? given->morale : -1) + ") and no automation (queue " + std::to_string(given ? given->queue.size() : 99) + ")");
        bool said = false;
        for (const auto& e : report.events) if (contains(e, "passed from") && contains(e, "peace terms")) said = true;
        check(said, "the turn report says the world changed hands");
        const std::string after = serialize(s);
        check(fingerprint(deserialize(after)) == fingerprint(s) && serialize(deserialize(after)) == after, "the game saves and loads exactly after the deal");

        // The same game, saved before any terms existed, loads from a schema-26 header and comes back out as 27.
        auto fresh = warGame();
        const std::string fresh27 = serialize(fresh);
        std::string as26 = fresh27; as26.replace(0, std::string("BOTF_SIM 27 remaster-27").size(), "BOTF_SIM 26 remaster-26");
        check(loads(withChecksum(as26)), "a schema-26 save with no world terms still loads");
        check(serialize(deserialize(withChecksum(as26))) == fresh27, "and is saved back as schema 27, unchanged otherwise");

        // Asked for a world, a crushed AI gives one. Empire 2 loses its fleet; the player gets a big one.
        auto d = warGame();
        std::erase_if(d.fleets, [](const Fleet& f) { return f.ownerId == 2; });
        const auto* home = systemNamed(d, slots::capital(1));
        for (int i = 0; i < 12; ++i) {
            Fleet f; f.id = d.nextFleetId++; f.ownerId = 1; f.x = f.targetX = home->x; f.y = f.targetY = home->y;
            f.shipClass = Building::Destroyer; f.hull = fleetMaxHull(f);
            d.fleets.push_back(f);
        }
        const int dBefore1 = coloniesOf(d, 1);
        const auto theirs = cedableWorlds(d, 2);
        check(!theirs.empty() && warScoreOf(d, 2, 1) <= -50, "setup: the AI is crushed (war score " + std::to_string(warScoreOf(d, 2, 1)) + ")");
        submit(d, makeProposePeaceTermsCommand(d.lastAcceptedCommand + 1, 2, {}, {theirs.front()}));
        advance(d);
        check(!relationOf(d, 1, 2).atWar && colonyAt(d, theirs.front()) && colonyAt(d, theirs.front())->ownerId == 1 && coloniesOf(d, 1) == dBefore1 + 1,
              "a crushed AI accepts peace and hands over the world the player asked for");
        // A winning AI does not.
        auto w = warGame();
        std::erase_if(w.fleets, [](const Fleet& f) { return f.ownerId == 1; });
        const auto* home2 = systemNamed(w, slots::capital(2));
        for (int i = 0; i < 12; ++i) {
            Fleet f; f.id = w.nextFleetId++; f.ownerId = 2; f.x = f.targetX = home2->x; f.y = f.targetY = home2->y;
            f.shipClass = Building::Destroyer; f.hull = fleetMaxHull(f);
            w.fleets.push_back(f);
        }
        const auto mine = cedableWorlds(w, 2);
        submit(w, makeProposePeaceTermsCommand(w.lastAcceptedCommand + 1, 2, {}, {mine.front()}));
        advance(w);
        check(relationOf(w, 1, 2).atWar && colonyAt(w, mine.front())->ownerId == 2, "a winning AI refuses to hand over a world: the war goes on and the world stays");
    } catch (const std::exception& e) { check(false, std::string("E3 campaign test threw: ") + e.what()); }

    // ---- E4: the flagship can be renamed; the name is saved and shows in battle names
    try {
        CampaignSetup setup; setup.scenarioId = "dominion-war";
        auto g = newCampaign(setup);
        g.eventsEnabled = false;
        const auto* flag = scenarios::flagshipFleet(g, g.playerEmpire);
        check(flag != nullptr, "setup: the Dominion War player starts with a flagship");
        const int fid = flag->id;
        const std::string defaultName = fleetName(*flag);
        check(scenarios::renameBlocker(g, g.playerEmpire, fid, "U.S.S. Valiant").empty(), "the flagship can be renamed");
        std::string other;
        for (const auto& f : g.fleets) if (f.ownerId == g.playerEmpire && f.id != fid && f.hull > 0) { other = scenarios::renameBlocker(g, g.playerEmpire, f.id, "Nope"); break; }
        check(!other.empty(), "an ordinary ship cannot be renamed ('" + other + "')");
        check(!scenarios::renameBlocker(g, g.playerEmpire, fid, std::string(25, 'x')).empty(), "a 25-character name is refused");
        check(!scenarios::renameBlocker(g, g.playerEmpire, fid, "Bad\"Name").empty() && !scenarios::renameBlocker(g, g.playerEmpire, fid, "Bad\\Name").empty() &&
              !scenarios::renameBlocker(g, g.playerEmpire, fid, std::string("Tab\there")).empty(), "quotes, backslashes and control characters are refused");
        submit(g, scenarios::makeRenameFlagshipCommand(g.lastAcceptedCommand + 1, fid, "  U.S.S. Valiant  "));
        advance(g);
        const auto* renamed = scenarios::flagshipFleet(g, g.playerEmpire);
        check(renamed && fleetName(*renamed).rfind("U.S.S. Valiant", 0) == 0 && scenarios::flagshipName(*renamed) == "U.S.S. Valiant", "the new name shows (trimmed): '" + (renamed ? fleetName(*renamed) : std::string("?")) + "'");
        const std::string text = serialize(g);
        check(contains(text, "\nFLAGSHIP_NAMES 1\n1 \"U.S.S. Valiant\"\n") && text.starts_with("BOTF_SIM 27 remaster-27\n"), "the name is saved in FLAGSHIP_NAMES");
        const auto back = deserialize(text);
        check(scenarios::flagshipFleet(back, back.playerEmpire) && fleetName(*scenarios::flagshipFleet(back, back.playerEmpire)).rfind("U.S.S. Valiant", 0) == 0 &&
              serialize(back) == text && fingerprint(back) == fingerprint(g), "and loads back: same name, same bytes, same fingerprint");
        {   // The section is a schema-27 feature.
            std::string old = text; old.replace(0, std::string("BOTF_SIM 27 remaster-27").size(), "BOTF_SIM 26 remaster-26");
            check(!loads(withChecksum(old)), "a schema-26 save with FLAGSHIP_NAMES is refused");
            std::string bad = text; bad.replace(bad.find("FLAGSHIP_NAMES 1\n1 "), 19, "FLAGSHIP_NAMES 1\n8 ");
            check(!loads(withChecksum(bad)), "a name for an empire with no flagship is refused");
        }
        {   // Schema 26 saves of flagship games load and keep the pack's names.
            auto plain = newCampaign(setup);
            std::string old = serialize(plain); old.replace(0, std::string("BOTF_SIM 27 remaster-27").size(), "BOTF_SIM 26 remaster-26");
            check(loads(withChecksum(old)), "a schema-26 flagship game loads (default names)");
        }
        submit(g, scenarios::makeRenameFlagshipCommand(g.lastAcceptedCommand + 1, fid, ""));
        advance(g);
        check(fleetName(*scenarios::flagshipFleet(g, g.playerEmpire)) == defaultName && !contains(serialize(g), "FLAGSHIP_NAMES"), "an empty name restores the pack's name and the section goes");
        // The loss line uses the name the player gave.
        submit(g, scenarios::makeRenameFlagshipCommand(g.lastAcceptedCommand + 1, fid, "Lost Cause"));
        advance(g);
        std::erase_if(g.fleets, [&](const Fleet& f) { return f.id == fid; });
        TurnReport lost;
        scenarios::step(g, lost);   // the per-turn upkeep that notices a flagship's ship is gone
        bool mourned = false;
        for (const auto& e : lost.events) if (contains(e, "Lost Cause has been lost")) mourned = true;
        for (const auto& c : lost.chronicle) if (contains(c.text, "Lost Cause has been lost")) mourned = true;
        check(mourned, "the loss of the flagship is reported under the player's name");
    } catch (const std::exception& e) { check(false, std::string("E4 test threw: ") + e.what()); }

    // ---- E5: the science megaproject is named per power, not per era
    {
        auto s = generateCampaign(5, 2);
        std::set<std::string> names;
        for (int id = 1; id <= 8; ++id) names.insert(megaprojectName(s, id));
        check(names.size() == 8, "eight powers, eight names");
        check(std::string(megaprojectName(s, 1)).find("Federation") != std::string::npos && std::string(megaprojectName(s, 1)).find("Genesis") != std::string::npos, "the Federation's is a Genesis-class project");
        check(std::string(megaprojectName(s, 2)).find("Klingon") != std::string::npos && std::string(megaprojectName(s, 7)).find("Borg") != std::string::npos, "the Klingon and Borg names carry their power");
        check(std::string(megaprojectName(s, 99)) == megaprojectName(s), "an unknown race keeps the era name");
        auto e5 = generateCampaignWithEra(5, StartingEra::Supreme, 2);
        check(std::string(megaprojectName(e5, 1)) == megaprojectName(s, 1), "the era does not change a power's name");
    }

    // ---- E2: the odds shown before an assault are the numbers the resolution uses
    try {
        auto s = warGame();
        const auto theirs = cedableWorlds(s, 2);
        const int target = theirs.front();
        const System* sys = nullptr;
        for (const auto& x : s.systems) if (x.id == target) sys = &x;
        std::erase_if(s.fleets, [&](const Fleet& f) { return f.ownerId == 2 && f.x == sys->x && f.y == sys->y; });
        std::erase_if(s.stations, [&](const Station& st) { return st.ownerId == 2 && st.x == sys->x && st.y == sys->y; });
        int firstId = 0;
        for (int i = 0; i < 2; ++i) {
            Fleet f; f.id = s.nextFleetId++; f.ownerId = 1; f.x = f.targetX = sys->x; f.y = f.targetY = sys->y;
            f.shipClass = Building::Transport; f.hull = fleetMaxHull(f);
            if (!i) firstId = f.id;
            s.fleets.push_back(f);
        }
        {   // Reveal the world to the player so the order is legal.
            auto& known = const_cast<EmpireState&>(empire(s, 1)).explored;
            const int cell = sys->y * s.width + sys->x;
            if (!std::binary_search(known.begin(), known.end(), cell)) known.insert(std::lower_bound(known.begin(), known.end(), cell), cell);
        }
        const auto single = invasionOdds(s, firstId, target);
        check(single.blocker.empty() && single.attack > 0 && single.defense > 0 && single.transports == 1 && single.percent > 0 && single.percent < 100, "one transport: attack " + std::to_string(single.attack) + " against defence " + std::to_string(single.defense) + " = " + std::to_string(single.percent) + "%");
        std::vector<int> transportIds;
        for (const auto& f : s.fleets) if (f.shipClass == Building::Transport && f.ownerId == 1) transportIds.push_back(f.id);
        for (int id : transportIds) {
            Command c; c.id = s.lastAcceptedCommand + 1; c.type = CommandType::Invade; c.fleetId = id; c.systemId = target;
            submit(s, c);
        }
        const auto drafted = [&] { auto d = s; return d; }();   // the draft view the UI reads
        (void)drafted;
        const auto report = advance(s);
        const InvasionReport* rec = nullptr;
        for (const auto& i : report.invasions) if (i.systemId == target) rec = &i;
        check(rec != nullptr, "the assault was resolved");
        // The preview, asked on the state before the turn, must equal the record. Rebuild that state: two transports ordered to the world.
        auto before = warGame();
        std::erase_if(before.fleets, [&](const Fleet& f) { return f.ownerId == 2 && f.x == sys->x && f.y == sys->y; });
        std::erase_if(before.stations, [&](const Station& st) { return st.ownerId == 2 && st.x == sys->x && st.y == sys->y; });
        int firstB = 0;
        for (int i = 0; i < 2; ++i) {
            Fleet f; f.id = before.nextFleetId++; f.ownerId = 1; f.x = f.targetX = sys->x; f.y = f.targetY = sys->y;
            f.shipClass = Building::Transport; f.hull = fleetMaxHull(f); f.invadeSystemId = target;
            if (!i) firstB = f.id;
            before.fleets.push_back(f);
        }
        const auto pooled = invasionOdds(before, firstB, target);
        check(pooled.transports == 2 && pooled.attack == 2 * single.attack, "two transports ordered to the same world pool: attack " + std::to_string(pooled.attack));
        check(invasionOdds(before, 999999, target).blocker == "No such ship" && !invasionOdds(before, firstB, cap1Id(before)).blocker.empty(), "odds for a missing ship, or for a world of our own, say why not");
        if (rec) check(rec->attackStrength == pooled.attack && rec->defenseStrength == pooled.defense, "preview and resolution agree: attack " + std::to_string(rec->attackStrength) + " / " + std::to_string(pooled.attack) + ", defence " + std::to_string(rec->defenseStrength) + " / " + std::to_string(pooled.defense));
    } catch (const std::exception& e) { check(false, std::string("E2 test threw: ") + e.what()); }

    // ---- E6, LAN roster: a player who dropped has the AI run their power, a rejoined one plays again, the chair follows
    try {
        auto g = warGame();
        setHotseat(g, {1, 2});
        auto dropped = g;
        setLanRoster(dropped, {1});
        check(dropped.hotseatHumans.empty() && dropped.playerEmpire == 1, "roster of one human: no hotseat list, that human in the chair");
        check(!humanSeat(dropped, 2) && humanSeat(dropped, 1), "the dropped player's power is not a human seat (the AI plans for it)");
        auto back = dropped;
        setLanRoster(back, {2, 1});
        check(back.hotseatHumans == std::vector<int>({1, 2}) && back.playerEmpire == 1 && humanSeat(back, 2), "a rejoined player is a human again (roster sorted, chair to the lowest)");
        auto other = g;
        setLanRoster(other, {2});
        check(other.playerEmpire == 2 && other.hotseatHumans.empty(), "the chair goes to the one remaining human even when it is not the seated one");
        bool threw = false;
        try { auto z = g; setLanRoster(z, {}); } catch (const std::exception&) { threw = true; }
        check(threw, "an empty roster is refused");
        const auto fpBefore = fingerprint(dropped);
        advance(dropped);
        check(fingerprint(dropped) != fpBefore, "a turn with a dropped player's power under AI command resolves");
    } catch (const std::exception& e) { check(false, std::string("E6 roster test threw: ") + e.what()); }

    std::cout << checks - failures << " passed, " << failures << " failed\n";
    return failures ? 1 : 0;
}
