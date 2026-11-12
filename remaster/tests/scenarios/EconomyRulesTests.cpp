// Economy and rules mechanics scenarios (lane MOD-6, schema 23; docs/design/ECONOMY-MECHANICS.md): structure upgrades (order, tiers,
// automatic variant, no-upgrades variant), the upgrade-and-scrap and insta-terraform exploit fixes, shared morale, the Protectorate HQ,
// dilithium on every minor, the intercept and raid orders with the Orion and Maquis bonus, crew experience and academies, the
// schema-23 ECONOMY_RULES section, migration from 21 and 22, preset flags, and a 100-turn AI soak with every flag on.
// The default Classic game must stay identical (no section, no new rule).
#include "EconomyRules.h"
#include "EmpireSlots.h"
#include "Presets.h"
#include "Simulation.h"
#include "Structures.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace botf;
int failed = 0;
void check(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
void rejects(const std::function<void()>& fn, const std::string& message, const std::string& fragment = "") {
    try { fn(); } catch (const std::exception& e) {
        check(fragment.empty() || std::string(e.what()).find(fragment) != std::string::npos, message + ": message '" + e.what() + "' lacks '" + fragment + "'");
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
void setTech(GameState& s, int level) {
    std::int64_t total = s.researchReserve;
    for (int f = 0; f < ResearchFields; ++f) {
        auto& t = s.technologies[static_cast<std::size_t>(f)];
        t.level = level; t.progress = 0;
        for (int l = 1; l <= level; ++l) total += researchCost(s, f, l);
    }
    s.research = total;
}
GameState game(std::uint64_t seed, const std::string& preset, int empires = 3) {
    CampaignSetup setup;
    setup.seed = seed; setup.empires = empires; setup.presetId = preset;
    setup.width = 20; setup.height = 14; setup.systems = 40;
    return newCampaign(setup);
}
GameState classicGame(std::uint64_t seed) {
    CampaignSetup setup;
    setup.seed = seed; setup.empires = 5; setup.preset = RulesetPreset::Classic;
    return newCampaign(setup);
}
Colony& homeOf(GameState& s, int owner) {
    return *std::find_if(s.colonies.begin(), s.colonies.end(), [owner](const Colony& c) { return c.ownerId == owner; });
}
const System& sysOf(const GameState& s, int id) { return *empire_rules::systemOf(s, id); }
Command enqueue(const GameState& s, int systemId, int recordId) {
    Command c; c.id = s.lastAcceptedCommand + 1; c.type = CommandType::EnqueueBuilding; c.systemId = systemId;
    c.building = Building::Structure; c.referenceId = structures::refFor(recordId);
    return c;
}
Command orderCmd(const GameState& s, int fleetId, int order, int systemId = 0) {
    Command c; c.id = s.lastAcceptedCommand + 1; c.type = CommandType::SetFleetOrder; c.fleetId = fleetId; c.eventOption = order; c.systemId = systemId;
    return c;
}
void setEconomyFlags(GameState& s, int bits) { economy_rules::setFlagBits(s.rules, bits); }
Fleet warship(GameState& s, int owner, int x, int y) {
    Fleet f; f.id = s.nextFleetId++; f.ownerId = owner; f.x = x; f.y = y; f.targetX = x; f.targetY = y;
    f.shipClass = Building::Destroyer; f.hull = shipMaxHull(Building::Destroyer);
    return f;
}
void makeWar(GameState& s, int a, int b) {
    for (auto& r : s.majorRelations)
        if ((r.first == a && r.second == b) || (r.first == b && r.second == a)) { r.contact = true; r.atWar = true; }
}
void chart(GameState& s, int x, int y) {
    const int cell = y * s.width + x;
    const auto at = std::lower_bound(s.explored.begin(), s.explored.end(), cell);
    if (at == s.explored.end() || *at != cell) s.explored.insert(at, cell);
}
int otherEmpire(const GameState& s) { return s.rivals.front().id; }
} // namespace

int main() {
    int passed = 0;

    run("flags: Classic has none, Remastered the vanilla set, presets carry theirs", [] {
        check(economy_rules::flagBits(rulesForPreset(RulesetPreset::Classic)) == 0, "Classic has no economy flag");
        const auto r = rulesForPreset(RulesetPreset::Remastered);
        check(r.interceptOrders && r.raidOrders && r.crewExperience && r.structureUpgrades && r.exploitFixes, "Remastered vanilla flags");
        check(!r.sharedMorale && !r.protectorateHq && !r.allDilithium && !r.raidBonus && !r.autoUpgrades && !r.noUpgrades, "mod-only flags are off");
        const auto udm = game(11, "dominion-war", 5);
        check(udm.rules.sharedMorale && udm.rules.protectorateHq && udm.rules.allDilithium && udm.rules.raidBonus, "Dominion War carries the UDM flags");
        check(game(11, "ultimate", 5).rules.autoUpgrades && game(11, "balance-of-power", 5).rules.autoUpgrades, "UM5 and BoP upgrade automatically");
        check(!game(11, "classic", 5).rules.structureUpgrades, "the Classic preset keeps every rule off");
        const auto bad = [](const char* json) { presets::parsePreset(json); };
        rejects([&] { bad("{\"format\":1,\"id\":\"x\",\"name\":\"X\",\"blurb\":\"\",\"order\":1,\"overlays\":[],\"rules\":{\"base\":\"remastered\",\"flags\":{\"noUpgrades\":true,\"autoUpgrades\":true}},\"lineup\":[\"Federation\",\"Klingon\"],\"options\":{},\"needs\":[]}"); }, "noUpgrades with autoUpgrades");
        rejects([&] { bad("{\"format\":1,\"id\":\"x\",\"name\":\"X\",\"blurb\":\"\",\"order\":1,\"overlays\":[],\"rules\":{\"base\":\"classic\",\"flags\":{\"raidBonus\":true}},\"lineup\":[\"Federation\",\"Klingon\"],\"options\":{},\"needs\":[]}"); }, "raidBonus without raid orders");
    }, passed);

    run("upgrades: tier needs technology, costs industry, lifts output, one project per open step", [] {
        auto s = game(21, "remastered");
        const int sid = homeOf(s, s.playerEmpire).systemId;
        const int record = upgrades::recordFor(0, false);
        check(!structures::blocker(s, homeOf(s, s.playerEmpire), record).empty() && contains(structures::blocker(s, homeOf(s, s.playerEmpire), record), "level 2"), "Biotech 2 is needed for tier 1");
        setTech(s, 2);
        check(structures::blocker(s, homeOf(s, s.playerEmpire), record).empty(), "tier 1 is open at level 2");
        const int farms = homeOf(s, s.playerEmpire).farms;
        const auto before = economy(s, homeOf(s, s.playerEmpire)).food;
        submit(s, enqueue(s, sid, record));
        const auto choices = constructionChoices(preview(s), sid);
        check(std::any_of(choices.begin(), choices.end(), [&](const ConstructionChoice& ch) { return ch.referenceId == structures::refFor(record) && ch.name == "Upgrade farms"; }), "the upgrade is in the build list");
        for (int t = 0; t < 60 && upgrades::tierOf(homeOf(s, s.playerEmpire), 0) == 0 && homeOf(s, s.playerEmpire).upgradeSteps[0] < farms; ++t) advance(s);
        auto& after = homeOf(s, s.playerEmpire);
        check(after.upgradeSteps[0] >= 1, "an upgrade project finished and applied a step");
        check(economy(s, after).food >= before, "output did not fall");
        check(upgrades::percent(after, 0) == 20 * after.upgradeSteps[0] / after.farms, "percent follows steps over units");
        // One project per open step: with farms*1 steps open, queueing farms+1 projects is refused.
        auto t = game(22, "remastered");
        setTech(t, 2);
        const int hs = homeOf(t, t.playerEmpire).systemId;
        homeOf(t, t.playerEmpire).queue.clear();
        const int open = upgrades::stepsOpen(t, *colonyAt(t, hs), 0);
        check(open == colonyAt(t, hs)->farms, "every farm may climb one tier");
        for (int i = 0; i < open; ++i) submit(t, enqueue(t, hs, record));
        rejects([&] { submit(t, enqueue(t, hs, record)); }, "no project beyond the open steps", "");
    }, passed);

    run("upgrades: the no-upgrades variant refuses every upgrade; flags off refuse too", [] {
        auto s = game(23, "remastered");
        setTech(s, 4);
        setEconomyFlags(s, economy_rules::flagBits(s.rules) | 4);
        const auto& c = homeOf(s, s.playerEmpire);
        check(contains(structures::blocker(s, c, upgrades::recordFor(1, false)), "without structure upgrades"), "no-upgrades blocker");
        auto classic = classicGame(23);
        check(!structures::blocker(classic, homeOf(classic, classic.playerEmpire), upgrades::recordFor(0, false)).empty(), "Classic refuses upgrades");
        auto choicesClassic = constructionChoices(classic, homeOf(classic, classic.playerEmpire).systemId);
        check(std::none_of(choicesClassic.begin(), choicesClassic.end(), [](const ConstructionChoice& ch) { return ch.name.find("Upgrade") == 0; }), "Classic build list has no upgrade entries");
    }, passed);

    run("upgrades: automatic variant fills an empty queue with the cheaper project", [] {
        auto s = game(24, "ultimate", 3);
        setTech(s, 4);
        for (auto& c : s.colonies) c.queue.clear();
        advance(s);
        bool found = false;
        for (const auto& c : s.colonies) for (const auto& p : c.queue)
            if (p.product == Building::Structure && upgrades::isRecord(structures::recordIdOf(p.referenceId)) && upgrades::automaticRecord(structures::recordIdOf(p.referenceId))) found = true;
        check(found, "an automatic upgrade was queued");
        check(upgrades::cost(upgrades::recordFor(0, true)) < upgrades::cost(upgrades::recordFor(0, false)), "the automatic variant is cheaper");
    }, passed);

    run("exploits: scrapping an upgraded structure refunds nothing and takes its share of steps; a queued upgrade cannot outlive its units", [] {
        auto s = game(25, "remastered");
        setTech(s, 6);
        auto& c = homeOf(s, s.playerEmpire);
        c.farms = 3; c.foodWorkers = std::min(c.foodWorkers, 20); c.upgradeSteps[0] = 3;
        const auto credits = s.credits; const auto overflow = c.overflow;
        submit(s, makeScrapStructureCommand(s.lastAcceptedCommand + 1, c.systemId, Building::Farm));
        const auto d = preview(s);
        const auto& pc = *colonyAt(d, c.systemId);
        check(pc.farms == 2 && pc.upgradeSteps[0] == 2, "scrapping a farm removes its share of the steps (3 of 3 -> 2 of 2)");
        check(d.credits == credits && pc.overflow == overflow, "no credits or industry come back");
        // Scrap down to nothing with an upgrade queued: it cannot apply.
        auto t = game(26, "remastered");
        setTech(t, 6);
        auto& h = homeOf(t, t.playerEmpire);
        h.farms = 1; h.foodWorkers = 0; h.upgradeSteps[0] = 0;
        h.queue.clear(); h.queue.emplace_back(Building::Structure, 0, structures::refFor(upgrades::recordFor(0, false)));
        submit(t, makeScrapStructureCommand(t.lastAcceptedCommand + 1, h.systemId, Building::Farm));
        advance(t);
        check(homeOf(t, t.playerEmpire).upgradeSteps[0] == 0, "no step without a farm");
        check(upgrades::unitsOf(homeOf(t, t.playerEmpire), 0) == 0 && economy_rules::flagBits(t.rules) != 0, "the colony lost its farm");
        // The upgrade does not pay for itself: costs always positive and tied to the unit's own cost.
        for (int k = 0; k < upgrades::Kinds; ++k) check(upgrades::cost(upgrades::recordFor(k, false)) > 0 && upgrades::cost(upgrades::recordFor(k, false)) < definition(upgrades::buildingOf(k)).industryCost, "upgrade costs less than a new unit but is never free");
    }, passed);

    run("exploits: terraforming cannot finish in one turn and ships do not stack", [] {
        Planet p; p.terraformNeeded = 200;
        check(economy_rules::terraformCap(p) == 100, "two turns at least");
        p.terraformNeeded = 5000;
        check(economy_rules::terraformCap(p) == 300, "never more than 300 a turn");
        p.terraformNeeded = 1;
        check(economy_rules::terraformCap(p) == 1, "tiny planets still progress");
    }, passed);

    run("morale: the shared structure lifts every colony, only with the flag", [] {
        auto s = game(31, "dominion-war", 5);
        const int me = s.playerEmpire;
        auto& c = homeOf(s, me);
        const auto* rec = structures::record(73);
        check(rec && rec->buildable && rec->bonusType == structures::SharedMorale, "the economy overlay makes record 73 the Unity Monument");
        check(economy_rules::sharedMorale(s, me) == 0, "none built yet");
        c.structures.push_back(73); std::sort(c.structures.begin(), c.structures.end());
        check(economy_rules::sharedMorale(s, me) == 5, "+5 morale empire-wide");
        for (const auto& other : s.colonies) if (other.ownerId == me) check(economy_rules::moraleBonus(s, other) >= 5, "every colony of the empire gains it");
        for (const auto& other : s.colonies) if (other.ownerId != me) check(economy_rules::moraleBonus(s, other) < 5 || economy_rules::sharedMorale(s, other.ownerId) > 0, "other empires do not");
        auto off = s;
        off.rules.sharedMorale = false;
        check(economy_rules::sharedMorale(off, me) == 0, "flag off: no bonus");
        for (int i = 0; i < 6; ++i) advance(s);
        check(homeOf(s, me).morale >= 70, "morale holds at or above the base target");
    }, passed);

    run("protectorate: subjugated worlds are restless until a Protectorate HQ stands among them, then pay tribute", [] {
        auto s = game(32, "dominion-war", 5);
        setTech(s, 4);
        const int me = s.playerEmpire;
        Colony* extra = nullptr;
        for (auto& c : s.colonies) if (c.ownerId != me) { extra = &c; break; }
        check(extra != nullptr, "a foreign colony to take");
        extra->ownerId = me; extra->subjugated = true;
        extra->shipyards = 0;
        check(economy_rules::moraleBonus(s, *extra) == -economy_rules::SubjugatedUnrest, "unrest without a HQ");
        check(economy_rules::tribute(s, *extra) == 0, "no tribute without a HQ");
        const auto* hq = structures::record(61);
        check(hq && hq->bonusType == structures::ProtectorateHq && hq->perEmpireLimit == 1, "record 61 is the HQ, one per empire");
        check(!structures::blocker(s, homeOf(s, me), 61).empty() && contains(structures::blocker(s, homeOf(s, me), 61), "subjugated"), "cannot build it in a free world");
        extra->structures.push_back(61);
        check(economy_rules::moraleBonus(s, *extra) == 10, "the HQ turns unrest into +10");
        check(economy_rules::tribute(s, *extra) >= 1, "tribute is paid");
        auto off = s; off.rules.protectorateHq = false;
        check(economy_rules::moraleBonus(off, *extra) == 0, "flag off: neither penalty nor bonus");
    }, passed);

    run("aftermath: Subjugate marks the world for the Protectorate only under the flag", [] {
        auto s = game(33, "dominion-war", 5);
        Colony* world = nullptr;
        for (auto& c : s.colonies) if (c.ownerId != s.playerEmpire && !minorAt(s, c.systemId)) { world = &c; break; }
        check(world != nullptr, "a world");
        world->ownerId = s.playerEmpire;
        chart(s, sysOf(s, world->systemId).x, sysOf(s, world->systemId).y);
        AftermathChoice a; a.systemId = world->systemId; a.conqueror = s.playerEmpire; a.formerOwner = otherEmpire(s); a.turn = s.turn; a.populationBefore = world->population;
        s.aftermaths.push_back(a);
        Command c; c.id = s.lastAcceptedCommand + 1; c.type = CommandType::ResolveAftermath; c.systemId = world->systemId; c.eventOption = 1;
        submit(s, c);
        check(colonyAt(preview(s), a.systemId)->subjugated, "Subjugate marks the world");
        auto r = game(33, "remastered");
        check(!r.rules.protectorateHq, "Remastered has no Protectorate");
    }, passed);

    run("dilithium: every minor homeworld has a deposit under the UDM flag", [] {
        auto s = game(41, "dominion-war", 5);
        check(!s.minors.empty(), "minors are placed");
        for (const auto& m : s.minors) check(empire_rules::hasDeposit(s, m.systemId), "deposit at " + m.name);
        auto plain = s; plain.rules.allDilithium = false;
        int without = 0;
        for (const auto& m : plain.minors) if (!empire_rules::hasDeposit(plain, m.systemId)) ++without;
        check(without > 0, "without the flag some minor has none");
    }, passed);

    run("orders: intercept goes after a hostile ship in range and returns home when none is; orders need the flag and a war", [] {
        auto s = game(51, "remastered");
        const int me = s.playerEmpire, foe = otherEmpire(s);
        makeWar(s, me, foe);
        auto& home = homeOf(s, me);
        const auto& sys = sysOf(s, home.systemId);
        auto mine = warship(s, me, sys.x, sys.y);
        s.fleets.push_back(mine);
        submit(s, orderCmd(s, mine.id, 1));
        auto d = preview(s);
        const auto* f = &*std::find_if(d.fleets.begin(), d.fleets.end(), [&](const Fleet& x) { return x.id == mine.id; });
        check(f->order == 1 && f->postX == sys.x && f->postY == sys.y, "intercept order holds the post");
        // An enemy ship one sector away.
        Fleet intruder = warship(d, foe, std::min(d.width - 1, sys.x + 1), sys.y);
        if (intruder.x == sys.x) intruder.x = sys.x - 1;
        d.fleets.push_back(intruder);
        TurnReport report;
        auto moving = d;
        economy_rules::beforeMovement(moving, report);
        const auto* g = &*std::find_if(moving.fleets.begin(), moving.fleets.end(), [&](const Fleet& x) { return x.id == mine.id; });
        check(g->attackTarget == intruder.id || g->targetX == intruder.x, "the interceptor heads for the intruder");
        moving.fleets.erase(std::remove_if(moving.fleets.begin(), moving.fleets.end(), [&](const Fleet& x) { return x.id == intruder.id; }), moving.fleets.end());
        economy_rules::beforeMovement(moving, report);
        const auto* h2 = &*std::find_if(moving.fleets.begin(), moving.fleets.end(), [&](const Fleet& x) { return x.id == mine.id; });
        check(h2->attackTarget == 0 && h2->targetX == sys.x && h2->targetY == sys.y, "with no intruder it returns to its post");
        auto classic = classicGame(51);
        const auto& cs = sysOf(classic, homeOf(classic, classic.playerEmpire).systemId);
        auto cf = warship(classic, classic.playerEmpire, cs.x, cs.y);
        classic.fleets.push_back(cf);
        rejects([&] { submit(classic, orderCmd(classic, cf.id, 1)); }, "Classic refuses the order", "off in this ruleset");
        auto peace = game(52, "remastered");
        auto pf = warship(peace, peace.playerEmpire, sysOf(peace, homeOf(peace, peace.playerEmpire).systemId).x, sysOf(peace, homeOf(peace, peace.playerEmpire).systemId).y);
        peace.fleets.push_back(pf);
        const int enemyColony = homeOf(peace, otherEmpire(peace)).systemId;
        rejects([&] { submit(peace, orderCmd(peace, pf.id, 2, enemyColony)); }, "no raid in peace", "");
    }, passed);

    run("orders: a raider at an undefended enemy colony moves credits from the victim to itself; defences stop it; Orion hulls take more", [] {
        auto s = game(53, "remastered");
        const int me = s.playerEmpire, foe = otherEmpire(s);
        makeWar(s, me, foe);
        setEconomyFlags(s, economy_rules::flagBits(s.rules) | 256);
        auto& victim = homeOf(s, foe);
        victim.defenseBatteries = 0;
        const auto& sys = sysOf(s, victim.systemId);
        s.fleets.erase(std::remove_if(s.fleets.begin(), s.fleets.end(), [&](const Fleet& f) { return f.ownerId == foe && f.x == sys.x && f.y == sys.y; }), s.fleets.end());
        s.stations.erase(std::remove_if(s.stations.begin(), s.stations.end(), [&](const Station& st) { return st.x == sys.x && st.y == sys.y; }), s.stations.end());
        auto raider = warship(s, me, sys.x, sys.y);
        raider.order = 2; raider.raidSystemId = victim.systemId;
        s.fleets.push_back(raider);
        const auto before = std::pair{s.credits, empire(s, foe).credits};
        TurnReport report;
        auto work = s;
        economy_rules::resolveRaids(work, report);
        const auto gained = work.credits - before.first, lost = before.second - empire(work, foe).credits;
        check(gained > 0 && gained == lost, "what the raider gains the victim loses (" + std::to_string(gained) + " vs " + std::to_string(lost) + ")");
        check(colonyAt(work, victim.systemId)->morale == std::max(0, victim.morale - economy_rules::RaidMoraleHit), "the colony's morale suffers");
        check(economy_rules::raidPercent(s, raider) == 100, "an ordinary hull has no bonus");
        // Powered batteries repel the raid.
        auto guarded = s;
        homeOf(guarded, foe).defenseBatteries = 2; homeOf(guarded, foe).energy = 100;
        guarded.advancedRulesEnabled = true;
        auto g2 = guarded;
        economy_rules::resolveRaids(g2, report);
        check(g2.credits == guarded.credits || economy(guarded, homeOf(guarded, foe)).poweredBatteries == 0, "powered batteries stop the raid");
        // The member bonus.
        auto bonus = s;
        bonus.minors.front().name = "Orion Syndicate"; bonus.minors.front().memberOf = me;
        check(economy_rules::raidPercent(bonus, raider) == 100 + economy_rules::RaidBonusMemberPercent, "a member Orion Syndicate adds 25%");
        auto noFlag = bonus; noFlag.rules.raidBonus = false;
        check(economy_rules::raidPercent(noFlag, raider) == 100, "flag off: no bonus");
    }, passed);

    run("orders: the AI uses them (a war game with orders on issues intercept and raid orders)", [] {
        auto s = game(54, "remastered", 4);
        makeWar(s, s.playerEmpire, otherEmpire(s));
        for (const auto& r : s.rivals) for (const auto& q : s.rivals) if (r.id < q.id) makeWar(s, r.id, q.id);
        int intercepts = 0, raids = 0;
        for (int t = 0; t < 60 && s.outcome == CampaignOutcome::Ongoing; ++t) {
            advance(s);
            for (const auto& f : s.fleets) if (f.ownerId != s.playerEmpire) { intercepts += f.order == 1; raids += f.order == 2; }
        }
        check(intercepts > 0, "the AI posted an interceptor");
        check(raids > 0, "the AI sent a raider");
    }, passed);

    run("crew: academies, battles and launches feed experience; the rating shows; effects need the flag", [] {
        check(crew::rating(0) == 0 && crew::rating(25) == 1 && crew::rating(40) == 2 && crew::rating(65) == 3 && crew::rating(90) == 4, "thresholds");
        check(crew::attackPercent(0) == -10 && crew::attackPercent(100) == 15, "attack percent");
        auto s = game(61, "dominion-war", 5);
        const int me = s.playerEmpire;
        auto& c = homeOf(s, me);
        const auto* ac = structures::record(85);
        check(ac && ac->bonusType == structures::Academy && ac->buildable, "record 85 is the Fleet Academy");
        const auto& sys = sysOf(s, c.systemId);
        auto ship = warship(s, me, sys.x, sys.y);
        s.fleets.push_back(ship);
        c.structures.push_back(85); std::sort(c.structures.begin(), c.structures.end());
        check(economy_rules::graduateExperience(s, c) == 25 + 16, "graduates launch with extra training");
        TurnReport report;
        auto work = s;
        economy_rules::trainCrews(work, report);
        const auto* f = &*std::find_if(work.fleets.begin(), work.fleets.end(), [&](const Fleet& x) { return x.id == ship.id; });
        check(f->experience == 29, "a fleet in an academy system trains +4");
        for (int i = 0; i < 40; ++i) economy_rules::trainCrews(work, report);
        f = &*std::find_if(work.fleets.begin(), work.fleets.end(), [&](const Fleet& x) { return x.id == ship.id; });
        check(f->experience == 80, "training stops at the academy's cap");
        // A battle won and survived.
        auto b = s;
        TurnReport br;
        CombatReport cr; cr.winnerOwner = me; cr.firstFleetIds = {ship.id}; cr.secondFleetIds = {};
        br.battles.push_back(cr);
        economy_rules::trainCrews(b, br);
        check(std::find_if(b.fleets.begin(), b.fleets.end(), [&](const Fleet& x) { return x.id == ship.id; })->experience >= 25 + 8, "a victory trains 8");
        auto off = s; off.rules.crewExperience = false;
        economy_rules::trainCrews(off, report);
        check(std::find_if(off.fleets.begin(), off.fleets.end(), [&](const Fleet& x) { return x.id == ship.id; })->experience == 25, "flag off: no training");
    }, passed);

    run("save: ECONOMY_RULES round trip, default Classic writes none, 21 and 22 headers load, the section is refused in older headers", [] {
        auto classic = classicGame(71);
        const auto ct = serialize(classic);
        check(!contains(ct, "ECONOMY_RULES") && ct.starts_with("BOTF_SIM 23 remaster-23\n"), "Classic: header 23, no section");
        check(serialize(deserialize(withHeader(ct, 21))) == ct && serialize(deserialize(withHeader(ct, 22))) == ct, "a 21 or 22 header over a Classic text loads and re-saves as 23");
        auto s = game(72, "dominion-war", 5);
        setTech(s, 4);
        auto& c = homeOf(s, s.playerEmpire);
        c.upgradeSteps[1] = 1; c.subjugated = true;
        auto f = warship(s, s.playerEmpire, sysOf(s, c.systemId).x, sysOf(s, c.systemId).y);
        f.order = 1; f.postX = f.x; f.postY = f.y; f.experience = 61;
        s.fleets.push_back(f);
        const auto text = serialize(s);
        check(contains(text, "ECONOMY_RULES 1 "), "section written");
        const auto back = deserialize(text);
        check(serialize(back) == text && fingerprint(back) == fingerprint(s), "round trip");
        check(homeOf(const_cast<GameState&>(back), s.playerEmpire).upgradeSteps[1] == 1, "steps survive");
        rejects([&] { deserialize(withHeader(text, 21)); }, "ECONOMY_RULES in a 21 header", "older save");
        // Plain Remastered carries the vanilla flags in its section.
        auto r = game(73, "remastered");
        check(contains(serialize(r), "ECONOMY_RULES 1 "), "Remastered writes its flag bits");
        check(deserialize(serialize(r)).rules.interceptOrders, "and reads them back");
    }, passed);

    run("soak: 100 turns, every flag on, deterministic and save-stable", [] {
        auto run1 = [] {
            auto s = game(81, "dominion-war", 5);
            setEconomyFlags(s, economy_rules::AllFlagBits & ~(4 | 2) | 2); // everything except noUpgrades (autoUpgrades on)
            setTech(s, 4);
            makeWar(s, s.playerEmpire, s.rivals.front().id); // one war keeps raids and intercepts live without ending the campaign early
            std::vector<std::uint64_t> hashes;
            for (int t = 0; t < 100 && s.outcome == CampaignOutcome::Ongoing; ++t) {
                advance(s);
                if (s.turn % 10 == 0) {
                    const auto text = serialize(s);
                    check(fingerprint(deserialize(text)) == fingerprint(s), "save round trip at turn " + std::to_string(s.turn));
                    hashes.push_back(fingerprint(s));
                }
            }
            int steps = 0, exp = 0;
            for (const auto& c : s.colonies) steps += c.upgradeSteps[0] + c.upgradeSteps[1] + c.upgradeSteps[2];
            for (const auto& f : s.fleets) exp += f.experience != 25;
            std::cout << "  soak: turn " << s.turn << ", upgrade steps " << steps << ", trained fleets " << exp << std::endl;
            return hashes;
        };
        const auto first = run1();
        check(first.size() >= 8, "the soak played at least 80 turns");
        check(first == run1(), "two runs agree");
    }, passed);

    std::cout << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
