// Dominion content scenarios (lane MOD-3, schema 21; docs/design/DOMINION.md): the Dominion overlay (structure ladder, Founders and
// Vorta traits, Jem'Hadar line), race structures in the core (ladder, Ketracel-White gate, effects, Shapeshifter Network feeding the
// intel sabotage output), the new minors and the Bajor-for-Edo swap, the schema-21 STRUCTURES section and migration from 18, and a
// 50-turn AI soak of a Dominion War game. The default Classic game must stay identical (no section, no new rule).
#include "EmpireSlots.h"
#include "Intelligence.h"
#include "MinorRaces.h"
#include "Presets.h"
#include "ShipRoster.h"
#include "Simulation.h"
#include "Structures.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace botf;
int failed = 0;
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void rejects(const std::function<void()>& fn, const std::string& message, const std::string& fragment = "") {
    try { fn(); } catch (const std::exception& e) {
        check(fragment.empty() || std::string(e.what()).find(fragment) != std::string::npos,
            message + ": message '" + e.what() + "' lacks '" + fragment + "'");
        return;
    }
    throw std::runtime_error(message);
}
void run(const char* name, const std::function<void()>& fn, int& passed) {
    try { fn(); ++passed; std::cout << "PASS " << name << std::endl; }
    catch (const std::exception& e) { ++failed; std::cout << "FAIL " << name << ": " << e.what() << std::endl; }
}
bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
std::string withChecksum(std::string body) {
    if (const auto pos = body.rfind("CHECKSUM "); pos != std::string::npos) body.erase(pos);
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
std::string withHeader(std::string text, int version) {
    text.replace(0, text.find('\n') + 1, "BOTF_SIM " + std::to_string(version) + " remaster-" + std::to_string(version) + "\n");
    return withChecksum(text);
}

// Sets every research track of the player to `level` and keeps the research ledger reconciled.
void setTech(GameState& s, int level) {
    std::int64_t total = s.researchReserve;
    for (int f = 0; f < ResearchFields; ++f) {
        auto& t = s.technologies[static_cast<std::size_t>(f)];
        t.level = level; t.progress = 0;
        for (int l = 1; l <= level; ++l) total += researchCost(s, f, l);
    }
    s.research = total;
}

const std::vector<std::string> WarLineup{"federation", "klingon", "romulan", "cardassian", "dominion"};

GameState warGame(std::uint64_t seed, int player, const std::vector<std::string>& lineup = WarLineup, MinorDensity density = MinorDensity::Many) {
    CampaignSetup setup;
    setup.seed = seed; setup.presetId = "dominion-war"; setup.lineup = lineup; setup.playerEmpire = player;
    setup.width = 22; setup.height = 15; setup.systems = 48;
    setup.galaxy.density = density;
    return newCampaign(setup);
}
Colony& colonyOf(GameState& s, int owner) {
    return *std::find_if(s.colonies.begin(), s.colonies.end(), [owner](const Colony& c) { return c.ownerId == owner; });
}
int coloniesOf(const GameState& s, int owner) {
    return static_cast<int>(std::count_if(s.colonies.begin(), s.colonies.end(), [owner](const Colony& c) { return c.ownerId == owner; }));
}
Command enqueue(const GameState& s, int systemId, int recordId) {
    Command c; c.id = s.lastAcceptedCommand + 1; c.type = CommandType::EnqueueBuilding; c.systemId = systemId;
    c.building = Building::Structure; c.referenceId = structures::refFor(recordId);
    return c;
}
const ConstructionChoice* choiceFor(const std::vector<ConstructionChoice>& all, int recordId) {
    for (const auto& c : all) if (c.product == Building::Structure && c.referenceId == structures::refFor(recordId)) return &c;
    return nullptr;
}
bool minorPlaced(const GameState& s, const std::string& name) {
    return std::any_of(s.minors.begin(), s.minors.end(), [&](const Minor& m) { return m.name == name; });
}
int playTurns(GameState& s, int turns) {
    int battles = 0;
    for (int t = 0; t < turns && s.outcome == CampaignOutcome::Ongoing; ++t) {
        const auto report = advance(s);
        battles += static_cast<int>(report.battles.size());
        if (s.turn % 10 == 0) {
            const auto text = serialize(s);
            const auto back = deserialize(text);
            check(fingerprint(back) == fingerprint(s) && serialize(back) == text, "save round trip at turn " + std::to_string(s.turn));
        }
    }
    return battles;
}
constexpr int Center = 318, Ketracel = 319, Barracks = 320, Bureau = 321, Foundry = 322, Vats = 323, Genetics = 324, Directorate = 325, Sanctuary = 326, Network = 327;
} // namespace

int main() {
    int passed = 0;

    run("overlay: the Dominion stack loads and the base pack is untouched", [] {
        presets::activate(*presets::find("remastered"));
        check(content::tables::buildings().size() == 318 && !structures::anyBuildable(), "the base pack has the 318 vanilla structures and none is a campaign structure");
        const auto baseMinors = content::tables::minors().size();
        check(baseMinors == 30, "thirty vanilla minors");
        presets::activate(*presets::find("dominion-war"));
        const auto& b = content::tables::buildings();
        check(b.size() == 328 && b[Center].buildable && b[Network].buildable && !b[100].buildable, "ten Dominion structures are appended");
        check(std::string(b[Network].name) == "Changeling Shapeshifter Network" && b[Network].bonusType == structures::ShapeshifterNetwork, "the network record");
        check(b[Barracks].requiresId == Center && b[Sanctuary].requiresId == Genetics && b[Network].requiresId == Sanctuary, "the ladder rungs");
        check(b[Network].excludedEmpireMask >> 35 << 35 != 0 && ((b[Network].excludedEmpireMask >> 35) & 1) == 0, "the 64-bit mask leaves only race 35 (Dominion) out of the exclusion");
        const auto* dom = slots::raceForEmpire(6);
        check(dom && dom->traitMorale == 5 && dom->traitCreditsPct == 10 && dom->traitGroundPct == 25, "Founders and Vorta trait effects");
        check(std::string(dom->traits).find("Founders") != std::string::npos && std::string(dom->traits).find("Vorta") != std::string::npos, "trait text");
        check(content::tables::minors().size() == 38, "eight new minors");
        check(!content::tables::minors()[13].available && std::string(content::tables::minors()[13].name) == "Edo", "Edo is switched off");
        check(content::tables::minors()[5].featured && std::string(content::tables::minors()[5].name) == "Bajorans", "Bajor is featured");
        check(content::tables::races().size() == 44, "six race records for the new minors");
        // Roster line: the Jem'Hadar tiers.
        const auto* attack = roster::findShip(1500);
        check(attack && attack->technologyRequirements[5] == 5 && attack->industryCost == 3600, "Jem'Hadar Attack Ship patched");
        presets::activate(*presets::find("remastered"));
        check(content::tables::buildings().size() == 318 && content::tables::minors().size() == 30, "picking Remastered puts the base back");
    }, passed);

    run("overlay: refusals name the field (forward requires, start without prerequisite, bad minor)", [] {
        const auto* bytes = presets::overlayBytes("dominion");
        check(bytes != nullptr, "the dominion overlay is registered");
        const auto tryStack = [&](const std::string& text) {
            content::detail::buildStack(content::embeddedBaseBundle(), content::detail::baseChecksumValue(), {text});
        };
        std::string forward = *bytes; forward.replace(forward.find("\"requires\":318"), 14, "\"requires\":400");
        rejects([&] { tryStack(forward); }, "a structure cannot require a later record", "earlier record");
        std::string noBuild = *bytes; noBuild.replace(noBuild.find("\"buildable\":true"), 16, "\"buildable\":false");
        rejects([&] { tryStack(noBuild); }, "a ladder field without buildable is refused", "need buildable");
        std::string lateStart = *bytes;  // a start rung that requires a rung which does not start built is refused
        lateStart.replace(lateStart.find("\"startsBuilt\":true"), 18, "\"startsBuilt\":false");
        rejects([&] { tryStack(lateStart); }, "a start rung needs a start prerequisite", "start");
        const auto* war = presets::overlayBytes("dominion-war");
        check(war != nullptr, "the dominion-war overlay is registered");
        std::string unknownKey = *war; unknownKey.replace(unknownKey.find("\"featured\":true"), 15, "\"fetured\":true");
        rejects([&] { content::detail::buildStack(content::embeddedBaseBundle(), content::detail::baseChecksumValue(), {*bytes, unknownKey}); },
            "a mistyped minor key is refused", "fetured");
        presets::activate(*presets::find("remastered"));
    }, passed);

    run("setup: Dominion War seats the Dominion with its start structures; other empires have none", [] {
        auto s = warGame(5, 6);
        check(s.playerEmpire == 6 && s.presetId == "dominion-war", "the player is the Dominion in the preset");
        const auto& home = colonyOf(s, 6);
        check((home.structures == std::vector<int>{Center, Ketracel}), "the Dominion capital starts with the Vorta centre and the Ketracel-White facility");
        for (int id : {1, 2, 3, 4}) check(colonyOf(s, id).structures.empty(), "empire " + std::to_string(id) + " starts with no race structure");
        const auto choices = constructionChoices(s, home.systemId);
        check(choiceFor(choices, Center) && choiceFor(choices, Center)->blocker == "Already built here", "the built rung says so");
        check(choiceFor(choices, Barracks) && (choiceFor(choices, Barracks)->blocker.find("Requires") == 0), "Barracks wait for Weapons research");
        check(choiceFor(choices, Network) && !choiceFor(choices, Network)->blocker.empty(), "the network is far up the ladder");
        check(serialize(s).find("\nSTRUCTURES 1\n1\n") != std::string::npos, "the save carries the section");
        // A Federation player sees none of the Dominion's structures.
        auto fed = warGame(5, 1);
        const auto fedChoices = constructionChoices(fed, colonyOf(fed, 1).systemId);
        check(!choiceFor(fedChoices, Center) && !choiceFor(fedChoices, Network), "the Federation is not offered Dominion structures");
        rejects([&] { submit(fed, enqueue(fed, colonyOf(fed, 1).systemId, Center)); preview(fed); }, "a Federation colony cannot queue a Dominion structure", "not available");
    }, passed);

    run("ladder: prerequisite in the colony, one per colony, empire limit, technology", [] {
        auto s = warGame(8, 6);
        auto& home = colonyOf(s, 6);
        setTech(s, 8);
        const int sys = home.systemId;
        // Needs the prerequisite in this colony.
        rejects([&] { submit(s, enqueue(s, sys, Sanctuary)); preview(s); }, "the Sanctuary needs the Genetic Research Station", "Needs a Genetic Research Station");
        // Not twice.
        rejects([&] { submit(s, enqueue(s, sys, Center)); preview(s); }, "the Vorta centre already stands", "Already built");
        // Build the rungs one by one with banked industry.
        for (int id : {Barracks, Genetics}) {
            colonyOf(s, 6).overflow = 20000;
            submit(s, enqueue(s, sys, id));
            advance(s);
            check(structures::built(colonyOf(s, 6), id), "structure " + std::to_string(id) + " completes");
        }
        auto& after = colonyOf(s, 6);
        check(std::is_sorted(after.structures.begin(), after.structures.end()), "the colony list stays sorted");
        // Empire limit: two Sanctuaries only.
        after.overflow = 20000; submit(s, enqueue(s, sys, Sanctuary)); advance(s);
        check(structures::built(colonyOf(s, 6), Sanctuary), "the Sanctuary completes once its rung stands");
        const auto text = serialize(s);
        check(deserialize(text).colonies.size() == s.colonies.size() && serialize(deserialize(text)) == text, "round trip with a built ladder");
        // Empire limit: two Sanctuaries and two networks. Spread them over two colonies (state edited directly).
        Colony second = colonyOf(s, 6);
        int freeSystem = -1;
        for (const auto& sysRec : s.systems) if (!colonyAt(s, sysRec.id) && !minorAt(s, sysRec.id)) { freeSystem = sysRec.id; break; }
        check(freeSystem >= 0, "a free system exists");
        second.systemId = freeSystem; second.queue.clear(); second.overflow = 0;
        second.structures = {Center, Ketracel, Genetics, Sanctuary};
        s.colonies.push_back(second);
        std::sort(s.colonies.begin(), s.colonies.end(), [](const Colony& a, const Colony& b) { return a.systemId < b.systemId; });
        auto& cap = *std::find_if(s.colonies.begin(), s.colonies.end(), [&](const Colony& c) { return c.ownerId == 6 && c.systemId == sys; });
        cap.structures = {Center, Ketracel, Genetics, Sanctuary, Network};
        check(structures::empireBlocker(s, 6, Network).empty(), "one network: the second may start (limit two)");
        check(structures::empireBlocker(s, 6, Sanctuary).find("limit of 2") != std::string::npos, "two Sanctuaries reach the empire limit");
        auto& other = *std::find_if(s.colonies.begin(), s.colonies.end(), [&](const Colony& c) { return c.systemId == freeSystem; });
        other.structures = {Center, Ketracel, Genetics, Sanctuary, Network};
        check(structures::empireBlocker(s, 6, Network).find("limit of 2") != std::string::npos, "two networks reach the empire limit");
    }, passed);

    run("gate: Jem'Hadar warships need a Ketracel-White Facility in the empire", [] {
        auto s = warGame(9, 6);
        setTech(s, 10);
        auto& home = colonyOf(s, 6);
        auto choices = constructionChoices(s, home.systemId);
        const auto jem = std::find_if(choices.begin(), choices.end(), [](const ConstructionChoice& c) { return c.referenceId == 1500; });
        check(jem != choices.end() && jem->blocker.empty(), "with the facility standing and the tech in hand the Attack Ship is open: " + (jem == choices.end() ? "" : jem->blocker));
        home.structures = {Center};
        choices = constructionChoices(s, home.systemId);
        const auto gated = std::find_if(choices.begin(), choices.end(), [](const ConstructionChoice& c) { return c.referenceId == 1500; });
        check(gated != choices.end() && gated->blocker == "Needs a Ketracel-White Facility in the empire", "without it the class is blocked: " + gated->blocker);
        const auto donor = std::find_if(choices.begin(), choices.end(), [](const ConstructionChoice& c) { return c.referenceId >= 0 && c.referenceId < 1000 && c.blocker.empty(); });
        check(donor != choices.end(), "donor hulls stay open");
        // A Cardassian ship is unaffected.
        auto car = warGame(9, 4);
        setTech(car, 10);
        check(structures::shipGate(car, 4, 1300).empty() && structures::shipGate(car, 6, 1500) == "", "gate is named only for the listed classes");
    }, passed);

    run("effects: structures change economy, upkeep, morale target, ground defence and intel output", [] {
        auto s = warGame(11, 6);
        auto& home = colonyOf(s, 6);
        const auto base = economy(s, home);
        home.structures = {Center, Ketracel, Bureau, Foundry, Vats, Genetics};
        const auto with = economy(s, home);
        check(with.industry > base.industry && with.research >= base.research && with.food > base.food, "industry and food rise (" + std::to_string(base.industry) + " -> " + std::to_string(with.industry) + ")");
        check(with.income > base.income, "the Vorta Commerce Bureau lifts credit income");
        check(with.upkeep > base.upkeep, "structures cost credit upkeep");
        const auto fx = structures::colonyEffects(home);
        check(fx.morale == 6 && fx.industryPct == 30 && fx.creditsPct == 25 && fx.foodPct == 40 && fx.researchPct == 40, "effect sums");
        // Traits: Founders' will, Vorta commerce, Jem'Hadar ground troops.
        const auto b = minors::memberBonuses(s, 6);
        check(b.morale == 5 && b.creditsPct == 10 && b.groundDefensePct == 25, "trait bonuses for the Dominion");
        const auto fed = minors::memberBonuses(s, 1);
        check(fed.morale == 0 && fed.creditsPct == 0 && fed.groundDefensePct == 0, "no trait bonus for the Federation");
        // Morale settles toward 70 + trait + structures.
        home.morale = 70;
        for (int i = 0; i < 12; ++i) advance(s);
        check(colonyOf(s, 6).morale >= 78, "Dominion capital morale settles toward 70 + 5 + 6 = 81, got " + std::to_string(colonyOf(s, 6).morale));
        // Ground defence.
        auto& again = colonyOf(s, 6);
        const int noBarracks = minors::invasionDefense(s, again, 100);
        again.structures.push_back(Barracks); std::sort(again.structures.begin(), again.structures.end());
        check(minors::invasionDefense(s, again, 100) > noBarracks, "Jem'Hadar Barracks raise ground defence");
        // Remastered Dominion without the overlay: no traits, no structures.
        presets::activate(*presets::find("remastered"));
        CampaignSetup plain; plain.seed = 11; plain.lineup = WarLineup; plain.playerEmpire = 6;
        auto flat = newCampaign(plain);
        check(flat.presetId.empty() && colonyOf(flat, 6).structures.empty(), "a plain Dominion game has no start structures");
        const auto fb = minors::memberBonuses(flat, 6);
        check(fb.morale == 0 && fb.creditsPct == 0 && fb.groundDefensePct == 0, "a plain Dominion game has no trait effects");
        presets::activate(*presets::find("dominion-war"));
    }, passed);

    run("intel: the Shapeshifter Network feeds sabotage output and the Vorta Directorate security output", [] {
        auto s = warGame(13, 6);
        check(structures::empireIntel(s, 6).networks == 0 && structures::empireIntel(s, 6).sabotagePct == 0, "no network at the start");
        auto& home = colonyOf(s, 6);
        home.structures = {Center, Ketracel, Genetics, Directorate, Sanctuary, Network};
        const auto intel = structures::empireIntel(s, 6);
        check(intel.networks == 1 && intel.sabotagePct == 30 && intel.sabotageDamage == 1 && intel.securityPct == 40, "network and directorate sums");
        check(structures::empireIntel(s, 1).sabotagePct == 0, "a rival is unaffected");
        // The same budget produces more sabotage stockpile with the bonus, and the bonus touches no other area.
        const auto output = [](int sabotageBonus, int securityBonus) {
            intelligence::EmpireIntelState state;
            state.empireId = 6;
            state.budget.setDefault({1}, 6);
            state.budget.setRivalAllocation(1, intelligence::IntelArea::Sabotage, 50);
            intelligence::TurnContext ctx;
            ctx.currentTurn = 1; ctx.totalEmpireBaseIntelOutput = 200;
            ctx.sabotageBonusPercent = sabotageBonus; ctx.securityBonusPercent = securityBonus;
            intelligence::RivalContext r; r.rivalId = 1; r.currentCredits = 1000;
            ctx.knownRivals.push_back(r);
            std::map<int, intelligence::DefenderSecurityContext> defenders;
            intelligence::IntelligenceRules rules;
            std::uint64_t seed = 99;
            intelligence::advanceTurn(state, ctx, defenders, rules, seed);
            const auto* rs = state.getRivalState(1);
            return std::make_pair(rs ? rs->sabotageStockpile : -1, state.internalSecurityStockpile);
        };
        const auto plain = output(0, 0), net = output(30, 0), dir = output(0, 40);
        check(plain.first > 0 && net.first == plain.first * 130 / 100, "+30% on the sabotage area: " + std::to_string(plain.first) + " -> " + std::to_string(net.first));
        check(net.second == plain.second && dir.first == plain.first, "each bonus leaves the other area alone");
        check(dir.second > plain.second || plain.second == 0, "the directorate raises internal security output");
    }, passed);

    run("minors: Bajor for the Edo, the new minors, and a Breen or Ferengi minor only while that race is not a major", [] {
        std::set<std::string> seen;
        for (std::uint64_t seed = 1; seed <= 10; ++seed) {
            const auto s = warGame(seed, 1, {"federation", "klingon", "romulan", "cardassian"});
            check(!minorPlaced(s, "Edo"), "the Edo are never placed under the preset");
            check(minorPlaced(s, "Bajorans"), "Bajor is always placed (featured), seed " + std::to_string(seed));
            for (const auto& m : s.minors) seen.insert(m.name);
        }
        for (const char* name : {"Breen", "Ferengi", "Son'a", "Maquis", "Miradorn", "Karemma", "Kazon", "Orion Syndicate"})
            check(seen.count(name) > 0, std::string(name) + " turns up in some galaxy");
        // Seat Breen and Ferengi as majors: their minors stay home.
        for (std::uint64_t seed = 1; seed <= 6; ++seed) {
            const auto s = warGame(seed, 1, {"federation", "ferengi", "breen", "dominion"});
            check(!minorPlaced(s, "Breen") && !minorPlaced(s, "Ferengi"), "no Breen or Ferengi minor beside the major, seed " + std::to_string(seed));
        }
        // Small galaxy with few minors still holds Bajor.
        const auto few = warGame(3, 1, {"federation", "klingon"}, MinorDensity::Few);
        check(minorPlaced(few, "Bajorans"), "Bajor is placed first even at low density");
        // A Son'a, Maquis or Kazon guard uses a roster ship as its signature.
        bool guardSeen = false;
        for (std::uint64_t seed = 1; seed <= 10 && !guardSeen; ++seed) {
            const auto s = warGame(seed, 1, {"federation", "klingon", "romulan", "cardassian"});
            for (const auto& g : s.minorGuards) guardSeen = guardSeen || g.shipId >= 1000;
        }
        check(guardSeen, "a new minor's roster ship guards its homeworld");
        // Membership benefits work for the Dominion (slot 6) as well.
        auto s = warGame(2, 6);
        check(!s.minors.empty(), "minors exist");
        s.minors[0].memberOf = 6;
        check(minors::memberBonuses(s, 6).members == 1, "a member minor counts for the Dominion");
    }, passed);

    run("save: STRUCTURES round trip, schemas 18 and 20 load, the section is refused below 21, forged rows fail", [] {
        auto s = warGame(15, 6);
        for (int i = 0; i < 4; ++i) advance(s);
        const auto text = serialize(s);
        check(contains(text, "\nSTRUCTURES 1\n") && text.starts_with("BOTF_SIM " + std::to_string(SaveVersion) + " remaster-" + std::to_string(SaveVersion) + "\n"), "current schema with a section");
        check(serialize(deserialize(text)) == text && fingerprint(deserialize(text)) == fingerprint(s), "round trip");
        for (int version : {14, 15, 16, 17, 18, 20})
            rejects([&] { deserialize(withHeader(text, version)); }, "STRUCTURES under a schema-" + std::to_string(version) + " header", "older save");
        auto forged = text;
        forged.replace(forged.find("STRUCTURES 1\n1\n") , 15, "STRUCTURES 1\n2\n");
        rejects([&] { deserialize(withChecksum(forged)); }, "a forged row count");
        auto badId = text;
        const auto at = badId.find("STRUCTURES 1\n1\n") + 15;
        badId.replace(at, badId.find('\n', at) - at, std::to_string(colonyOf(s, 6).systemId) + " 2 5 6");
        rejects([&] { deserialize(withChecksum(badId)); }, "a vanilla record is not a campaign structure");
        auto rung = text;
        const auto at2 = rung.find("STRUCTURES 1\n1\n") + 15;
        rung.replace(at2, rung.find('\n', at2) - at2, std::to_string(colonyOf(s, 6).systemId) + " 1 " + std::to_string(Barracks));
        rejects([&] { deserialize(withChecksum(rung)); }, "a rung without its prerequisite", "prerequisite");
        // A save made on the Dominion stack needs it.
        presets::activate(*presets::find("remastered"));
        const auto reloaded = deserialize(text);
        check(content::active().id == "botf-base+dominion+dominion-war" && serialize(reloaded) == text, "the loader rebuilds the stack the save names");
        // Default game: no section, a schema-18 or schema-20 header over the same text loads and re-saves as 21.
        presets::activate(*presets::find("remastered"));
        CampaignSetup plain; plain.seed = 1999;
        auto d = newCampaign(plain);
        for (int i = 0; i < 5; ++i) advance(d);
        const auto dt = serialize(d);
        check(!contains(dt, "STRUCTURES") && !contains(dt, "EMPIRE_SLOTS") && !contains(dt, "PACKS"), "the default five-empire game has no new section");
        const auto old = deserialize(withHeader(dt, 18));
        check(serialize(old) == dt, "a schema-18 default save loads and re-saves as 21");
        check(serialize(deserialize(withHeader(dt, 20))) == dt, "a schema-20 default save loads and re-saves as 21");
        check(serialize(deserialize(withHeader(dt, 21))) == dt, "a schema-21 default save loads and re-saves as the current schema");
        auto playing = old; advance(playing);
        check(playing.turn == old.turn + 1, "and plays on");
        presets::activate(*presets::find("dominion-war"));
    }, passed);

    run("soak: Dominion War, 50 turns with the AI (Federation player), determinism and reload", [] {
        auto a = warGame(7, 1), b = warGame(7, 1);
        check(fingerprint(a) == fingerprint(b), "same setup, same world");
        const int battlesA = playTurns(a, 50), battlesB = playTurns(b, 50);
        check(battlesA == battlesB && fingerprint(a) == fingerprint(b), "two runs agree on 50 turns");
        auto c = warGame(7, 1);
        playTurns(c, 25);
        auto resumed = deserialize(serialize(c));
        playTurns(resumed, 25);
        check(fingerprint(resumed) == fingerprint(a), "reload at turn 25 continues identically");
        int domColonies = 0, domStructures = 0;
        std::set<int> kinds;
        for (const auto& col : a.colonies) if (col.ownerId == 6) { ++domColonies; domStructures += static_cast<int>(col.structures.size()); for (int id : col.structures) kinds.insert(id); }
        std::cout << "    turn " << a.turn << ", battles " << battlesA << ", Dominion colonies " << domColonies << ", structures " << domStructures
                  << ", kinds " << kinds.size() << std::endl;
        check(a.turn == 50 || a.outcome != CampaignOutcome::Ongoing, "50 turns were played");
        int anywhere = 0;
        for (const auto& col : a.colonies) anywhere += static_cast<int>(col.structures.size());
        std::cout << "    structures standing anywhere: " << anywhere << " (two at the start)" << std::endl;
        check(anywhere >= 4, "the Dominion AI built at least two structures beyond the start rungs");
    }, passed);

    run("soak: Dominion War, 50 turns with the Dominion as the player's rival set and Dominion player", [] {
        auto a = warGame(12, 6), b = warGame(12, 6);
        const int battlesA = playTurns(a, 50), battlesB = playTurns(b, 50);
        check(battlesA == battlesB && fingerprint(a) == fingerprint(b), "two runs agree");
        auto c = warGame(12, 6);
        playTurns(c, 25);
        auto resumed = deserialize(serialize(c));
        playTurns(resumed, 25);
        check(fingerprint(resumed) == fingerprint(a), "reload at turn 25 continues identically");
        check(a.turn == 50 || a.outcome != CampaignOutcome::Ongoing, "50 turns were played");
        std::cout << "    turn " << a.turn << ", battles " << battlesA << ", Dominion colonies " << coloniesOf(a, 6) << std::endl;
    }, passed);

    run("soak: eight majors with the overlay (Dominion, Borg, Breen with Ferengi), 30 turns", [] {
        const std::vector<std::string> all8{"federation", "klingon", "romulan", "cardassian", "ferengi", "dominion", "borg", "breen"};
        auto a = warGame(21, 1, all8), b = warGame(21, 1, all8);
        playTurns(a, 30); playTurns(b, 30);
        check(fingerprint(a) == fingerprint(b), "two eight-empire runs agree on 30 turns");
        check(!minorPlaced(a, "Breen") && !minorPlaced(a, "Ferengi"), "no duplicate minors");
    }, passed);

    if (failed) {
        std::cout << "FAILED " << passed << " passed, " << failed << " failed" << std::endl;
        return 1;
    }
    std::cout << "ALL " << passed << " DOMINION SCENARIOS PASSED" << std::endl;
    return 0;
}
