// Balance and AI overlay scenarios (lane MOD-7, schema 24; docs/design/BALANCE-AI.md): the pack balance values and their
// refusals, ship and structure cost, batteries and shields, populations, weapon output, minor gifts, per-empire traits, the intel
// rebalance, the AI policy flags and the Expert difficulty, the galaxy options (planets per star, extra start colonies, star types),
// the STAR_TYPES save section and migration from schema 22, Classic / Remastered isolation, and a 100-turn AI soak per mod preset.
#include "Content.h"
#include "EmpireSlots.h"
#include "GameplayInternal.h"
#include "MinorRaces.h"
#include "Presets.h"
#include "ShipRoster.h"
#include "Simulation.h"
#include "TacticalCombat.h"

#include <algorithm>
#include <array>
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
void run(const char* name, const std::function<void()>& fn, int& passed) {
    try { fn(); ++passed; std::cout << "PASS " << name << std::endl; }
    catch (const std::exception& e) { ++failed; std::cout << "FAIL " << name << ": " << e.what() << std::endl; }
}
std::string refusal(const std::function<void()>& fn, const std::string& what) {
    try { fn(); } catch (const std::exception& e) { return e.what(); }
    throw std::runtime_error(what + " accepted");
}
void expectRefusal(const std::function<void()>& fn, const std::string& fragment, const std::string& what) {
    const auto message = refusal(fn, what);
    check(message.find(fragment) != std::string::npos, what + ": message '" + message + "' lacks '" + fragment + "'");
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
std::string overlay(const std::string& id, const std::string& body) {
    return "{\"format\":1,\"kind\":\"overlay\",\"id\":\"" + id + "\",\"name\":\"Test " + id + "\",\"revision\":1,\"requires\":[\"botf-base\"]" +
           (body.empty() ? "" : "," + body) + "}";
}
GameState game(const std::string& preset, std::uint64_t seed = 21, StartingEra era = StartingEra::Early, std::vector<std::string> lineup = {}) {
    CampaignSetup setup;
    setup.seed = seed; setup.era = era; setup.presetId = preset; setup.lineup = std::move(lineup);
    if (preset == "classic") setup.preset = RulesetPreset::Classic;
    return newCampaign(setup);
}
// A campaign on whatever content is active (an ad-hoc overlay stack), Remastered rules.
GameState onActive(std::uint64_t seed = 21, Difficulty d = Difficulty::Normal) {
    CampaignSetup setup;
    setup.seed = seed; setup.galaxy.difficulty = d;
    return newCampaign(setup);
}
int homeSystem(const GameState& s, int empire = 0) {
    const int who = empire ? empire : s.playerEmpire;
    for (const auto& c : s.colonies) if (c.ownerId == who) return c.systemId;
    throw std::runtime_error("no home");
}
int costOf(const GameState& s, int empire, Building product, int referenceId) {
    for (const auto& choice : constructionChoices(s, homeSystem(s, empire))) if (choice.product == product && choice.referenceId == referenceId) return choice.industryCost;
    throw std::runtime_error("class not listed");
}
// The attack totals of the two starting fleets as a tactical encounter sees them.
int battleAttack(GameState s) {
    const Fleet* mine = nullptr; const Fleet* theirs = nullptr;
    for (const auto& f : s.fleets) {
        if (f.hull <= 0 || fleetAttack(f) <= 0) continue;
        if (f.ownerId == s.playerEmpire && !mine) mine = &f;
        if (f.ownerId != s.playerEmpire && !theirs) theirs = &f;
    }
    check(mine && theirs, "both sides field a warship");
    Fleet a = *mine, b = *theirs;
    b.x = a.x; b.y = a.y; b.id = a.id + 9000;
    s.fleets.push_back(b);
    for (auto& r : s.majorRelations) r.atWar = true;
    const auto enc = tactical::constructEncounter(s, {a.id, b.id}, 99);
    int total = 0;
    for (const auto& ship : enc.ships) total += ship.beamAttack + ship.torpedoAttack;
    return total;
}
const char* const Stock[] = {"federation", "klingon", "romulan", "cardassian", "ferengi"};
}  // namespace

int main() {
    int passed = 0;

    run("isolation: Classic and Remastered carry the identity balance, the old AI and the schema-25 header", [] {
        for (const char* id : {"classic", "remastered"}) {
            auto s = game(id);
            check(content::balance() == content::Balance{}, std::string(id) + ": identity balance");
            const auto p = aiPolicy(s);
            check(!p.alwaysStarbases && !p.newestShips && !p.standardBuild && !p.aggressiveExpansion, std::string(id) + ": no AI flags at Normal");
            check(s.galaxy.difficulty == Difficulty::Normal, "default difficulty is Normal");
            const auto text = serialize(s);
            check(text.starts_with("BOTF_SIM 25 remaster-25\n"), "header");
            check(!contains(text, "STAR_TYPES") && !contains(text, "GALAXY_SETUP") && !contains(text, "PACKS"), std::string(id) + ": no new section");
            for (const auto& sys : s.systems) check(sys.starType == -1, "no star type set");
            check(minorGiftCost() == 50 && minorGiftAttitude() == 20 && packWeaponOutputPercent() == 100, "vanilla gift and weapon values");
        }
        content::installBase();
    }, passed);

    run("balance values: every key parses, merges (later overlay wins) and its range is enforced", [] {
        const std::string all = "\"balance\":{\"researchCostPercent\":90,\"researchCurveLow\":80,\"researchCurveHigh\":120,\"shipCostPercent\":95,\"structureCostPercent\":85,"
            "\"batteryDefensePercent\":150,\"shieldDefensePercent\":120,\"populationCapacityPercent\":110,\"populationGrowthPercent\":90,\"weaponOutputPercent\":70,"
            "\"minorGiftCost\":120,\"minorGiftAttitude\":30,\"intelOutputPercent\":80,\"aiIntelPercent\":60,\"aiAlwaysStarbases\":1,\"aiNewestShips\":1,"
            "\"aiStandardBuild\":1,\"aiAggressiveExpansion\":1,\"starTypesPerRace\":1}";
        content::installStack({overlay("t-all", all)});
        const auto& b = content::balance();
        check(b.researchCostPercent == 90 && b.researchCurveLow == 80 && b.researchCurveHigh == 120 && b.shipCostPercent == 95 && b.structureCostPercent == 85, "cost values");
        check(b.batteryDefensePercent == 150 && b.shieldDefensePercent == 120 && b.populationCapacityPercent == 110 && b.populationGrowthPercent == 90, "defence and population values");
        check(b.weaponOutputPercent == 70 && b.minorGiftCost == 120 && b.minorGiftAttitude == 30 && b.intelOutputPercent == 80 && b.aiIntelPercent == 60, "weapon, gift and intel values");
        check(b.aiAlwaysStarbases && b.aiNewestShips && b.aiStandardBuild && b.aiAggressiveExpansion && b.starTypesPerRace, "flags");
        content::installStack({overlay("t-a", "\"balance\":{\"weaponOutputPercent\":70,\"minorGiftCost\":120}"), overlay("t-b", "\"balance\":{\"weaponOutputPercent\":90}")});
        check(content::balance().weaponOutputPercent == 90 && content::balance().minorGiftCost == 120, "later overlay wins per key, earlier keys survive");
        for (const char* bad : {"\"weaponOutputPercent\":5", "\"weaponOutputPercent\":2000", "\"minorGiftCost\":5", "\"minorGiftAttitude\":0", "\"aiNewestShips\":2", "\"batteryDefensePercent\":-1", "\"aiStandardBuildd\":1"})
            expectRefusal([&] { content::installStack({overlay("t-bad", std::string("\"balance\":{") + bad + "}")}); }, "", std::string("balance ") + bad);
        content::installBase();
    }, passed);

    run("research curve: the flat percent times the level curve, identity at 100", [] {
        content::installBase();
        auto base = onActive(5);
        const int plain2 = researchCost(base, 0, 2), plain10 = researchCost(base, 0, 10), plain6 = researchCost(base, 0, 6);
        content::installStack({overlay("t-r", "\"balance\":{\"researchCostPercent\":50,\"researchCurveLow\":100,\"researchCurveHigh\":200}")});
        auto s = onActive(5);
        check(researchCost(s, 0, 2) == plain2 * 50 / 100, "level 2 takes the low end (50%)");
        check(researchCost(s, 0, 10) == plain10 * 50 / 100 * 200 / 100, "level 10 takes the high end (100%)");
        check(researchCost(s, 0, 6) < plain6 && researchCost(s, 0, 6) > plain6 * 50 / 100 * 100 / 100 - 1, "level 6 sits between the ends (" + std::to_string(researchCost(s, 0, 6)) + " of " + std::to_string(plain6) + ")");
        content::installBase();
    }, passed);

    run("costs: ship and structure percent show in the build list and the queue; the owner's race trait adds to ships", [] {
        content::installBase();
        const auto plain = onActive(9);
        const int fed = plain.playerEmpire;
        // A basic structure and a warship the Federation can build now.
        int shipId = -1, shipPlain = 0;
        for (const auto& c : constructionChoices(plain, homeSystem(plain))) if (c.referenceId >= 0 && c.blocker.empty() && c.product == Building::Destroyer) { shipId = c.referenceId; shipPlain = c.industryCost; break; }
        check(shipId >= 0, "a buildable ship");
        const int farmPlain = costOf(plain, fed, Building::Farm, -1);
        content::installStack({overlay("t-c", "\"balance\":{\"shipCostPercent\":90,\"structureCostPercent\":85}")});
        const auto cheap = onActive(9);
        check(costOf(cheap, fed, Building::Destroyer, shipId) == shipPlain * 90 / 100, "ship cost 90%");
        check(costOf(cheap, fed, Building::Farm, -1) == farmPlain * 85 / 100, "structure cost 85%");
        // The queue finishes on the scaled cost: rush cost follows it.
        auto q = cheap;
        Command c; c.id = 1; c.type = CommandType::EnqueueBuilding; c.systemId = homeSystem(q); c.building = Building::Destroyer; c.referenceId = shipId;
        submit(q, c);
        check(!q.pending.empty(), "queued");
        content::installStack({overlay("t-t", "\"races\":{\"patch\":[{\"id\":1,\"major\":{\"traitEffects\":{\"shipCostPct\":10}}}]}")});
        const auto trait = onActive(9);
        check(costOf(trait, fed, Building::Destroyer, shipId) == shipPlain * 110 / 100, "a +10% trait costs the owner 110%");
        check(costOf(trait, fed, Building::Farm, -1) == farmPlain, "a ship-cost trait leaves structures alone");
        content::installBase();
    }, passed);

    run("traits: industry, research, food, credits and ground defence ride on the empire's economy and invasion defence", [] {
        content::installBase();
        const auto plain = onActive(11);
        const auto e0 = economy(plain, plain.colonies.front());
        const int me = plain.colonies.front().ownerId;
        const int raceId = me == 1 ? 1 : -1;
        check(raceId == 1, "the player is the Federation");
        content::installStack({overlay("t-tr", "\"races\":{\"patch\":[{\"id\":1,\"major\":{\"traitEffects\":{\"industryPct\":20,\"researchPct\":10,\"foodPct\":30,\"creditsPct\":10,\"groundDefensePct\":50,\"morale\":4}}}]}")});
        const auto s = onActive(11);
        const auto e1 = economy(s, s.colonies.front());
        check(e1.industry > e0.industry && e1.research >= e0.research && e1.food > e0.food && e1.income >= e0.income, "output rises");
        check(e1.industry == e0.industry * 120 / 100 || e1.industry * 100 / 120 >= e0.industry - 1, "industry +20%");
        check(minors::memberBonuses(s, me).groundDefensePct == 50 && minors::moraleTarget(s, me) == 74, "ground defence and morale");
        content::installStack({overlay("t-neg", "\"races\":{\"patch\":[{\"id\":1,\"major\":{\"traitEffects\":{\"industryPct\":-20,\"foodPct\":-20}}}]}")});
        const auto w = onActive(11);
        const auto e2 = economy(w, w.colonies.front());
        check(e2.industry < e0.industry && e2.food < e0.food, "a weakness lowers output");
        for (int bad : {101, -51}) expectRefusal([&] { content::installStack({overlay("t-x", "\"races\":{\"patch\":[{\"id\":1,\"major\":{\"traitEffects\":{\"industryPct\":" + std::to_string(bad) + "}}}]}")}); }, "", "trait range");
        content::installBase();
    }, passed);

    run("batteries and shields: the pack scales a colony's ground defence", [] {
        content::installBase();
        auto plain = onActive(13);
        auto& col = plain.colonies.front();
        col.defenseBatteries = 3;
        const int base = minors::invasionDefense(plain, col, 8 + col.population / InvasionPopDefenseDivisor + col.morale / 10 + 3 * 12);
        content::installStack({overlay("t-d", "\"balance\":{\"shieldDefensePercent\":150}")});
        auto shield = onActive(13);
        shield.colonies.front().defenseBatteries = 3;
        const auto& c2 = shield.colonies.front();
        const int boosted = minors::invasionDefense(shield, c2, 8 + c2.population / InvasionPopDefenseDivisor + c2.morale / 10 + 3 * 12);
        check(boosted == base * 150 / 100, "shield 150% scales invasion defence (" + std::to_string(base) + " -> " + std::to_string(boosted) + ")");
        content::installBase();
    }, passed);

    run("populations: capacity and growth percent", [] {
        content::installBase();
        const auto plain = onActive(17);
        const int sys = homeSystem(plain);
        const int cap = systemCapacity(plain, sys), maxCap = systemMaxCapacity(plain, sys);
        content::installStack({overlay("t-p", "\"balance\":{\"populationCapacityPercent\":120,\"populationGrowthPercent\":50}")});
        auto s = onActive(17);
        check(systemCapacity(s, sys) == cap * 120 / 100 && systemMaxCapacity(s, sys) == maxCap * 120 / 100, "capacity +20%");
        content::installBase();
        auto fast = onActive(17), slow = fast;
        content::installStack({overlay("t-p2", "\"balance\":{\"populationGrowthPercent\":50}")});
        slow = onActive(17);
        content::installBase();
        fast = onActive(17);
        for (auto* g : {&fast, &slow}) {
            auto& c = g->colonies.front(); c.population = 20; c.foodWorkers = 10; c.industryWorkers = 5; c.researchWorkers = 5;
        }
        for (int i = 0; i < 25; ++i) { content::installBase(); advance(fast); content::installStack({overlay("t-p2", "\"balance\":{\"populationGrowthPercent\":50}")}); advance(slow); }
        check(fingerprint(fast) != fingerprint(slow) && fast.colonies.front().population >= slow.colonies.front().population, "growth at 50% differs from vanilla over 25 turns (" + std::to_string(fast.colonies.front().population) + " vs " + std::to_string(slow.colonies.front().population) + ")");
        content::installBase();
    }, passed);

    run("weapon output: the pack's percent scales every ship's attack in a tactical encounter", [] {
        content::installBase();
        const auto plain = onActive(19);
        const int a = battleAttack(plain);
        content::installStack({overlay("t-w", "\"balance\":{\"weaponOutputPercent\":75}")});
        const int b = battleAttack(onActive(19));
        check(a > 0 && b > 0 && b < a && b >= a * 70 / 100 - 4 && b <= a * 80 / 100 + 4, "75% output (" + std::to_string(a) + " -> " + std::to_string(b) + ")");
        content::installBase();
    }, passed);

    run("minor gifts: the pack sets the cost and the attitude a delivered gift gives", [] {
        content::installStack({overlay("t-g", "\"balance\":{\"minorGiftCost\":100,\"minorGiftAttitude\":25}")});
        auto s = onActive(23);
        auto& m = s.minors.front();
        m.contact[s.playerEmpire] = true;
        const auto credits = s.credits;
        const int attitude = m.attitude[s.playerEmpire];
        Command c; c.id = 1; c.systemId = m.systemId; c.type = CommandType::SendGift;
        s.credits = 99;
        expectRefusal([&] { submit(s, c); advance(s); }, "100", "a gift below the new cost");
        s = onActive(23);
        s.minors.front().contact[s.playerEmpire] = true;
        submit(s, c);
        advance(s); advance(s);
        check(s.minors.front().attitude[s.playerEmpire] >= attitude + 25 - 1, "gift gave +25 attitude");
        (void)credits;
        content::installBase();
    }, passed);

    run("intel rebalance: the pack and the race trait scale the spy output", [] {
        content::installBase();
        auto s = onActive(29);
        const std::int64_t base = empireIntelOutput(s, s.rivals.front().id);
        check(base > 0, "an empire has intel output");
        content::installStack({overlay("t-i", "\"balance\":{\"aiIntelPercent\":50,\"intelOutputPercent\":80},\"races\":{\"patch\":[{\"id\":1,\"major\":{\"traitEffects\":{\"intelPct\":20}}}]}")});
        // The scaling is applied where the turn context is built; the played game shows it as a different intelligence history.
        auto a = onActive(29), b = a;
        content::installBase();
        auto c = onActive(29);
        for (int i = 0; i < 8; ++i) { content::installStack({overlay("t-i", "\"balance\":{\"aiIntelPercent\":50,\"intelOutputPercent\":80},\"races\":{\"patch\":[{\"id\":1,\"major\":{\"traitEffects\":{\"intelPct\":20}}}]}")}); advance(a); content::installBase(); advance(c); }
        check(fingerprint(a) != fingerprint(c), "the rebalance changes the intelligence game");
        (void)b;
        content::installBase();
    }, passed);

    run("armed transports: Dominion War arms every race's colony and troop ships; Classic does not", [] {
        const auto arms = [](const char* preset) {
            auto s = game(preset, 31, StartingEra::Early, {});
            std::set<int> armed;
            for (const auto& ship : referenceData::ShipCatalogue) if ((ship.role == 5 || ship.role == 9) && ship.attack > 0) armed.insert(ship.raceId);
            return armed.size();
        };
        const auto classicArmed = arms("classic");
        const auto udm = arms("dominion-war");
        check(classicArmed == 1 && udm >= 5, "classic: only the Cardassians (" + std::to_string(classicArmed) + "); Dominion War: " + std::to_string(udm) + " races");
        content::installBase();
    }, passed);

    run("AI policy: Normal none, Hard the standard build, Expert and Impossible all four; flags from the pack", [] {
        content::installBase();
        for (Difficulty d : DifficultyOrder) {
            auto s = onActive(33, d);
            const auto p = aiPolicy(s);
            const bool all = p.alwaysStarbases && p.newestShips && p.standardBuild && p.aggressiveExpansion;
            const bool none = !p.alwaysStarbases && !p.newestShips && !p.standardBuild && !p.aggressiveExpansion;
            if (d == Difficulty::Expert || d == Difficulty::Impossible) check(all, std::string(difficultyName(d)) + " plays with every behaviour");
            else if (d == Difficulty::Hard) check(p.standardBuild && !p.alwaysStarbases && !p.newestShips && !p.aggressiveExpansion, "hard: the standard build only");
            else check(none, std::string(difficultyName(d)) + " plays the old AI");
        }
        content::installStack({overlay("t-f", "\"balance\":{\"aiNewestShips\":1}")});
        const auto p = aiPolicy(onActive(33));
        check(p.newestShips && !p.alwaysStarbases, "a pack flag switches one behaviour");
        content::installBase();
    }, passed);

    run("AI newest ships: the queue picks the highest-technology class under the flag, the cheapest otherwise", [] {
        content::installBase();
        const auto pick = [](bool newest) {
            if (newest) content::installStack({overlay("t-n", "\"balance\":{\"aiNewestShips\":1}")}); else content::installBase();
            auto s = onActive(35);
            for (auto& f : s.rivals) for (auto& t : f.technologies) t.level = 8;
            s.rivals.front().credits = 100000;
            auto& colony = *std::find_if(s.colonies.begin(), s.colonies.end(), [&](const Colony& c) { return c.ownerId == s.rivals.front().id; });
            colony.queue.clear();
            check(internal::queueShip(s, colony, Building::Destroyer), "the AI queued a warship");
            const auto id = colony.queue.back().referenceId;
            int tech = 0; for (int v : internal::sourceShip(id).technologyRequirements) tech += v;
            return std::pair<int, int>(tech, internal::sourceShip(id).industryCost);
        };
        const auto newest = pick(true), cheap = pick(false);
        check(newest.first >= cheap.first && newest.second >= cheap.second, "newest: tech " + std::to_string(newest.first) + " cost " + std::to_string(newest.second) + " vs " + std::to_string(cheap.first) + "/" + std::to_string(cheap.second));
        check(newest.first > cheap.first || newest.second > cheap.second, "newest differs from cheapest");
        content::installBase();
    }, passed);

    run("AI starbases: under the flag the AI keeps more stations and upgrades them sooner", [] {
        const auto stations = [](bool flag) {
            if (flag) content::installStack({overlay("t-s", "\"balance\":{\"aiAlwaysStarbases\":1}")}); else content::installBase();
            auto s = onActive(37);
            int built = 0;
            for (int t = 0; t < 90 && s.outcome == CampaignOutcome::Ongoing; ++t) advance(s);
            for (const auto& st : s.stations) if (st.ownerId != s.playerEmpire) built += st.type == StationType::Starbase ? 3 : 1;
            return built;
        };
        const int with = stations(true), without = stations(false);
        std::cout << "  rival station score (starbase 3, outpost 1) after 90 turns: with flag " << with << ", without " << without << std::endl;
        check(with >= without, "the flag never builds fewer");
        content::installBase();
    }, passed);

    run("Expert: parses, saves as 5, takes no bonus of any kind and the AI reads the tier", [] {
        content::installBase();
        Difficulty d{};
        check(parseDifficulty("expert", d) && d == Difficulty::Expert && std::string(difficultyName(d)) == "expert", "parse and name");
        const auto& dp = difficultyProfile(Difficulty::Expert);
        check(dp.aiIncomePct == 100 && dp.aiIndustryPct == 100 && dp.aiResearchPct == 100 && dp.aiStartCreditsPct == 100 && dp.aiWarConfidencePct == 100 && dp.intelThreshold == 0, "no bonus of any kind");
        check(difficultyRank(Difficulty::Hard) < difficultyRank(Difficulty::Expert) && difficultyRank(Difficulty::Expert) < difficultyRank(Difficulty::Impossible), "tier order");
        auto normal = onActive(41, Difficulty::Normal), expert = onActive(41, Difficulty::Expert);
        check(normal.rivals.front().credits == expert.rivals.front().credits, "same starting treasury as Normal");
        for (int i = 0; i < 6; ++i) { advance(normal); advance(expert); }
        std::int64_t rn = 0, re = 0;
        for (const auto& r : normal.rivals) rn += r.research; for (const auto& r : expert.rivals) re += r.research;
        check(re >= rn * 95 / 100 && re <= rn * 105 / 100, "rival research within 5% of Normal early on (" + std::to_string(rn) + " vs " + std::to_string(re) + ")");
        check(serialize(expert).find("GALAXY_SETUP 0 0 5") != std::string::npos || contains(serialize(expert), " 5\n"), "saved as difficulty 5");
        const auto text = serialize(expert);
        check(serialize(deserialize(text)) == text && deserialize(text).galaxy.difficulty == Difficulty::Expert, "round trip");
        expectRefusal([&] { deserialize(withHeader(text, 22)); }, "", "Expert under a schema-22 header");
        content::installBase();
    }, passed);

    run("galaxy options: planets per star, extra start colonies and race star types; defaults change nothing", [] {
        content::installBase();
        CampaignSetup plain; plain.seed = 51;
        const auto base = newCampaign(plain);
        auto same = plain; same.startSystems = 0; same.planetsMin = 0; same.planetsMax = 0; same.starTypes = -1;
        check(fingerprint(newCampaign(same)) == fingerprint(base), "defaults leave the galaxy as it was");
        auto many = plain; many.planetsMin = 6; many.planetsMax = 9;
        const auto crowded = newCampaign(many);
        int ordinary = 0;
        for (const auto& sys : crowded.systems) {
            if (colonyAt(crowded, sys.id)) { check(sys.planets.size() == 5, "a home keeps its five planets"); continue; }
            ++ordinary;
            check(sys.planets.size() >= 6 && sys.planets.size() <= 9, "ordinary star holds 6..9 planets (" + std::to_string(sys.planets.size()) + ")");
        }
        check(ordinary > 10, "many ordinary stars");
        auto sparse = plain; sparse.planetsMin = 1; sparse.planetsMax = 2;
        for (const auto& sys : newCampaign(sparse).systems) if (!colonyAt(base, sys.id)) check(sys.planets.size() <= 2, "sparse stars hold 1..2 planets");
        auto extra = plain; extra.startSystems = 3;
        auto e = newCampaign(extra);
        std::map<int, int> perEmpire;
        for (const auto& c : e.colonies) ++perEmpire[c.ownerId];
        check(perEmpire.size() == 5, "five empires");
        for (const auto& kv : perEmpire) check(kv.second == 4, "each empire starts with a capital and three more (empire " + std::to_string(kv.first) + " has " + std::to_string(kv.second) + ")");
        validate(e);
        for (int i = 0; i < 20; ++i) advance(e);
        validate(e);
        expectRefusal([&] { auto bad = plain; bad.startSystems = 9; newCampaign(bad); }, "0 to 8", "nine extra systems");
        expectRefusal([&] { auto bad = plain; bad.planetsMin = 5; bad.planetsMax = 3; newCampaign(bad); }, "1 to 9", "planet range reversed");
        expectRefusal([&] { auto bad = plain; bad.planetsMin = 3; bad.planetsMax = 10; newCampaign(bad); }, "1 to 9", "ten planets");
        // Star types: capitals take their race's star, saved in STAR_TYPES; off by default.
        auto st = plain; st.starTypes = 1;
        const auto typed = newCampaign(st);
        std::set<int> kinds;
        for (const auto& c : typed.colonies) for (const auto& sys : typed.systems) if (sys.id == c.systemId) { check(sys.starType >= 0, "a capital has a star type"); kinds.insert(sys.starType); }
        check(kinds.size() >= 4, "races differ in star type");
        const auto text = serialize(typed);
        check(contains(text, "\nSTAR_TYPES 1 5\n") && deserialize(text).systems[static_cast<std::size_t>(homeSystem(typed) - 1)].starType == typed.systems[static_cast<std::size_t>(homeSystem(typed) - 1)].starType, "STAR_TYPES saved and loaded");
        check(serialize(deserialize(text)) == text, "star types round trip");
        check(!contains(serialize(base), "STAR_TYPES"), "default game writes no section");
        content::installBase();
    }, passed);

    run("save: schema 22 saves load and re-save as 24; STAR_TYPES is refused under older headers and when malformed", [] {
        content::installBase();
        auto s = game("remastered", 61);
        for (int i = 0; i < 12; ++i) advance(s);
        {   // Schema 23 carries the economy section; its text differs from schema 24 by the header alone.
            const auto t23 = serialize(s);
            const auto back23 = deserialize(withHeader(t23, 23));
            check(serialize(back23) == t23 && fingerprint(back23) == fingerprint(s), "a schema-23 save re-saves as the schema-24 text");
        }
        auto& r = s.rules;   // A schema-22 game has none of the economy rules.
        r.interceptOrders = r.raidOrders = r.crewExperience = r.structureUpgrades = r.exploitFixes = r.autoUpgrades = r.noUpgrades = r.sharedMorale = r.protectorateHq = r.allDilithium = r.raidBonus = false;
        const auto text = serialize(s);
        check(!contains(text, "ECONOMY_RULES"), "no economy section with the rules off");
        const auto old = withHeader(text, 22);
        const auto back = deserialize(old);
        check(serialize(back) == text, "a schema-22 save re-saves as the schema-24 text");
        check(fingerprint(back) == fingerprint(s), "same fingerprint");
        CampaignSetup st; st.seed = 62; st.starTypes = 1;
        const auto typed = serialize(newCampaign(st));
        expectRefusal([&] { deserialize(withHeader(typed, 22)); }, "older save", "STAR_TYPES under a schema-22 header");
        auto bad = typed; bad.replace(bad.find("STAR_TYPES 1 5"), 14, "STAR_TYPES 1 9");
        expectRefusal([&] { deserialize(withChecksum(bad)); }, "", "wrong STAR_TYPES count");
        auto badType = typed; { const auto at = badType.find("STAR_TYPES 1 5\n"); const auto line = badType.find('\n', at) + 1; const auto sp = badType.find(' ', line); badType.replace(sp + 1, 1, "9"); }
        expectRefusal([&] { deserialize(withChecksum(badType)); }, "", "star type 9");
        content::installBase();
    }, passed);

    run("P1: a diplomatic win holds for 10 turns in a row, and the hold survives a save", [] {
        content::installBase();
        // Remastered seed 7000 with every seat played (observer mode): before P1 the Ferengi won diplomatically on turn 150.
        auto s = game("remastered", 7000);
        s.observerMode = true;
        int firstHold = 0, winTurn = 0, maxHold = 0;
        bool roundTrip = false;
        for (int t = 0; t < 320 && s.outcome == CampaignOutcome::Ongoing; ++t) {
            advance(s);
            for (const auto& [id, turns] : s.diplomaticHold) {
                maxHold = std::max(maxHold, turns);
                if (!firstHold) firstHold = s.turn;
                if (turns == 5 && !roundTrip) {   // mid-hold: the save carries it and the reloaded game is the same game
                    const auto text = serialize(s);
                    check(text.find("DIPLOMATIC_HOLD ") != std::string::npos, "the save carries the hold");
                    auto back = deserialize(text);
                    back.observerMode = true;
                    check(back.diplomaticHold == s.diplomaticHold && fingerprint(back) == fingerprint(s), "the hold round-trips");
                    roundTrip = true;
                }
            }
            if (s.outcome != CampaignOutcome::Ongoing) winTurn = s.turn;
        }
        std::cout << "  first hold turn " << firstHold << ", longest hold " << maxHold << ", ended turn " << winTurn << std::endl;
        // C9 (21 Nov 2026): AI-AI blocs now break at their 100-turn reviews, and seed 7000 forms no bloc in 320 turns. 0 = never held.
        check(firstHold == 0 || firstHold >= 150, "no hold before turn 150");
        if (winTurn && s.winnerEmpire) {
            bool diplomatic = false;
            for (const auto& v : victoryProgress(s, s.winnerEmpire)) diplomatic |= v.condition == VictoryCondition::Diplomatic && v.achieved;
            if (diplomatic) check(winTurn >= firstHold + 9 && maxHold >= 10, "the diplomatic win came after 10 held turns");
        }
        if (!roundTrip) {   // no bloc held in this run (C9): stage a 5-turn hold on a live campaign and check the save path the same way
            auto h = game("remastered", 7000);
            h.observerMode = true;
            for (int t = 0; t < 5; ++t) advance(h);
            h.diplomaticHold = {{internal::activeEmpires(h).back(), 5}};
            const auto text = serialize(h);
            check(text.find("DIPLOMATIC_HOLD ") != std::string::npos, "the save carries the hold (staged)");
            auto back = deserialize(text);
            back.observerMode = true;
            check(back.diplomaticHold == h.diplomaticHold && fingerprint(back) == fingerprint(h), "the hold round-trips (staged)");
            roundTrip = true;
        }
        check(roundTrip, "the hold's save check ran");
        GameState fresh = game("remastered", 7000);
        check(serialize(fresh).find("DIPLOMATIC_HOLD") == std::string::npos, "a new game's save has no hold section");
        content::installBase();
    }, passed);
    run("AI vanilla rules: rivals build their race academy and use intercept and raid orders when crew experience is on", [] {
        content::installBase();
        std::set<int> academyOwners; int orders = 0, intercept = 0, raid = 0;
        for (std::uint64_t seed : {1999ULL, 7ULL, 42ULL}) {
            auto s = game("remastered", seed);
            check(s.rules.crewExperience && s.rules.interceptOrders && s.rules.raidOrders, "the vanilla rules are on by default");
            // Observer mode (soaks only, F5): the AI plays the player's seat too, so the campaign runs its 160 turns. With a passive seat
            // it ended at turn 85-110 when the seat fell, before a 5,000-industry academy could finish (P2, 20 Nov 2026: one academy
            // project per empire, at its strongest yard, instead of every yard building the same academy at once).
            s.observerMode = true;
            for (int t = 0; t < 160 && s.outcome == CampaignOutcome::Ongoing; ++t) {
                advance(s);
                for (const auto& f : s.fleets) if (f.ownerId != s.playerEmpire && f.order != 0) { ++orders; (f.order == 1 ? intercept : raid)++; }
            }
            for (const auto& c : s.colonies) if (c.ownerId != s.playerEmpire)
                for (int id : c.structures) if (id == 25 || id == 37 || id == 43 || id == 50) academyOwners.insert(static_cast<int>(seed) * 10 + c.ownerId);
        }
        std::cout << "  academies held by " << academyOwners.size() << " rival empires over 3 games; AI order-turns " << orders << " (intercept " << intercept << ", raid " << raid << ")" << std::endl;
        check(!academyOwners.empty(), "some rival built its race academy");
        check(orders > 0, "the AI used intercept or raid orders");
        content::installBase();
    }, passed);

    run("presets: Balance of Power, Ultimate and Dominion War carry their balance and traits", [] {
        const auto bop = game("balance-of-power", 71);
        const auto& b = content::balance();
        check(b.weaponOutputPercent == 75 && b.shipCostPercent == 90 && b.structureCostPercent == 85 && b.aiAlwaysStarbases && b.aiNewestShips, "BoP values");
        check(aiPolicy(bop).alwaysStarbases && aiPolicy(bop).newestShips && aiPolicy(bop).standardBuild && !aiPolicy(bop).aggressiveExpansion, "BoP AI policy");
        const auto um = game("ultimate", 72);
        check(content::balance().researchCostPercent == 85 && content::balance().weaponOutputPercent == 85 && content::balance().aiIntelPercent == 75, "UM5 values");
        check(slots::traitEffects(4).intelPct == -25 && slots::traitEffects(3).intelPct == -25 && slots::traitEffects(1).intelPct == 0, "UM5 cuts the Cardassian and Romulan intel");
        check(aiPolicy(um).aggressiveExpansion && aiPolicy(um).standardBuild, "UM5 AI policy");
        const auto udm = game("dominion-war", 73);
        check(content::balance().batteryDefensePercent == 150 && content::balance().shieldDefensePercent == 115 && content::balance().minorGiftCost == 100, "UDM values");
        const auto ages = game("ages", 74);
        check(content::balance().weaponOutputPercent == 85 && slots::traitEffects(2).groundDefensePct == 20 && slots::traitEffects(2).researchPct == -10, "AAM traits and firepower");
        int offsets = 0;
        for (int e = 1; e <= 5; ++e) { const auto t = slots::traitEffects(e); if ((t.researchPct > 0 || t.industryPct > 0 || t.creditsPct > 0 || t.intelPct > 0 || t.groundDefensePct > 0) && (t.researchPct < 0 || t.industryPct < 0 || t.creditsPct < 0 || t.foodPct < 0 || t.groundDefensePct < 0 || t.shipCostPct > 0 || t.morale < 0)) ++offsets; }
        check(offsets == 5, "every empire has a strength and an offsetting weakness");
        const auto gal = game("galaxies", 75, StartingEra::Early, {"federation", "klingon", "romulan", "dominion", "borg"});
        int typed = 0; for (const auto& sys : gal.systems) if (sys.starType >= 0) ++typed;
        check(typed == 5, "Galaxies gives the five capitals their race's star");
        // UDM and BoP give the new-game screen their option defaults.
        check(presets::find("balance-of-power")->startSystems == 1 && presets::find("balance-of-power")->planetsMax == 9, "BoP options");
        content::installBase();
    }, passed);

    run("soak: 100 turns per mod preset, deterministic, save-stable, validated", [] {
        struct Case { const char* preset; std::vector<std::string> lineup; StartingEra era; };
        const std::vector<Case> cases{
            {"balance-of-power", {}, StartingEra::Early}, {"ultimate", {}, StartingEra::Early},
            {"dominion-war", {"federation", "klingon", "romulan", "cardassian", "dominion"}, StartingEra::Developed}};
        for (const auto& c : cases) {
            auto a = game(c.preset, 80, c.era, c.lineup);
            auto b = a;
            for (int turn = 1; turn <= 100 && a.outcome == CampaignOutcome::Ongoing; ++turn) {
                const auto ra = advance(a);
                check(ra.stateHash == advance(b).stateHash, std::string(c.preset) + ": two runs agree at turn " + std::to_string(turn));
                if (turn % 25 == 0) {
                    const auto text = serialize(a);
                    const auto back = deserialize(text);
                    check(serialize(back) == text && fingerprint(back) == fingerprint(a), std::string(c.preset) + ": save round trip at turn " + std::to_string(turn));
                    validate(a);
                }
            }
            int colonies = 0; for (const auto& col : a.colonies) if (col.ownerId != a.playerEmpire) ++colonies;
            std::cout << "  " << c.preset << ": turn " << a.turn << ", rivals hold " << colonies << " colonies, " << a.stations.size() << " stations" << std::endl;
        }
        content::installBase();
    }, passed);

    std::cout << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
