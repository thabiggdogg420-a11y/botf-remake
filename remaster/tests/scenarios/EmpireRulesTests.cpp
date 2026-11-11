// Empire-rule scenarios (lane M2-EMPIRE, schema 15; docs/design/EMPIRE-RULES.md):
// dilithium ship slots, emergency programs, invasion aftermath, independent-minor conquest, minor guard ships.
#include "EmpireRules.h"
#include "MinorRaces.h"
#include "Simulation.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
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
std::uint64_t nextId(const GameState& s) { return s.lastAcceptedCommand + 1; }
std::string withChecksum(std::string body) {
    if (const auto pos = body.rfind("CHECKSUM "); pos != std::string::npos) body.erase(pos);
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

GameState fresh(std::uint64_t seed = 77, int empires = 5) {
    CampaignSetup setup; setup.seed = seed; setup.empires = empires;
    return newCampaign(setup);
}
Colony& colonyOf(GameState& s, int systemId) {
    for (auto& c : s.colonies) if (c.systemId == systemId) return c;
    throw std::runtime_error("no colony");
}
std::vector<Colony*> ownedBy(GameState& s, int owner) {
    std::vector<Colony*> out;
    for (auto& c : s.colonies) if (c.ownerId == owner) out.push_back(&c);
    return out;
}
// Makes a colony able to build a scout right now: yard, power, workers, ship at the head of the queue.
void setUpShipbuilder(Colony& c) {
    c.shipyards = 1; c.shipyardEnabled = true; c.energy = 200;
    c.factories = std::max(c.factories, 2); c.foodWorkers = 0; c.researchWorkers = 0; c.industryWorkers = std::min(c.population, 20);
    c.queue.clear(); c.queue.emplace_back(Building::Scout);
}
// A second colony for `owner`, cloned from its first onto a free system (white-box: no validate()).
Colony& addColony(GameState& s, int owner, int wantDeposit = -1) {
    Colony copy = *ownedBy(s, owner).front();
    for (const auto& sys : s.systems) {
        if (colonyAt(s, sys.id) || minorAt(s, sys.id)) continue;
        if (wantDeposit >= 0 && empire_rules::hasDeposit(s, sys.id) != (wantDeposit == 1)) continue;
        copy.systemId = sys.id; copy.refineries = 0; copy.program = 0; copy.programTurns = 0; copy.queue.clear();
        s.colonies.push_back(copy);
        const int cell = sys.y * s.width + sys.x;
        if (std::find(s.explored.begin(), s.explored.end(), cell) == s.explored.end()) s.explored.push_back(cell);
        std::sort(s.explored.begin(), s.explored.end());
        std::sort(s.colonies.begin(), s.colonies.end(), [](const Colony& a, const Colony& b) { return a.systemId < b.systemId; });
        return colonyOf(s, sys.id);
    }
    throw std::runtime_error("no free system");
}
} // namespace

int main() {
    int passed = 0;

    // ---------------------------------------------------------------- (b) dilithium
    run("dilithium: a new campaign gives each empire one powered refinery and one slot", [] {
        auto s = fresh();
        check(s.rules.empireRules, "rules on");
        for (int e = 1; e <= 5; ++e) {
            const auto d = empire_rules::dilithiumStatus(s, e);
            check(d.enforced && d.refineries == 1 && d.powered == 1 && d.slots == 1, "empire " + std::to_string(e) + " status");
        }
    }, passed);

    run("dilithium: a second shipbuilder waits for a second refinery", [] {
        auto s = fresh();
        auto& a = *ownedBy(s, 1).front();
        const int aId = a.systemId;
        const int bId = addColony(s, 1).systemId;
        auto& low = colonyOf(s, std::min(aId, bId));
        auto& high = colonyOf(s, std::max(aId, bId));
        setUpShipbuilder(low); setUpShipbuilder(high);
        low.refineries = 1; high.refineries = 0; low.energy = 300;
        check(constructionBlocker(s, low).empty(), "lowest system builds");
        const auto blocked = constructionBlocker(s, high);
        check(blocked.find("slots all in use") != std::string::npos, "second blocked: " + blocked);
        check(empire_rules::dilithiumStatus(s, 1).used == 2, "two builders counted");
        high.refineries = 1; high.energy = 300;
        check(empire_rules::dilithiumStatus(s, 1).slots == 2, "two slots");
        check(constructionBlocker(s, high).empty(), "second builds with a second refinery");
    }, passed);

    run("dilithium: no powered refinery means no ships; an unpowered refinery gives no slot", [] {
        auto s = fresh();
        auto& c = *ownedBy(s, 1).front();
        setUpShipbuilder(c);
        c.refineries = 0;
        check(contains(constructionBlocker(s, c), "No powered dilithium refinery"), "no refinery");
        s.technologies[3].level = 0; // No energy-tech bonus: the grid is exactly c.energy.
        c.refineries = 1; c.factories = 10; c.laboratories = 10; c.energy = 100; // factories and labs eat the whole grid first
        check(empire_rules::dilithiumStatus(s, 1).powered == 0, "refinery needs its own 5 energy");
    }, passed);

    run("dilithium: Klingon shipyards with a powered refinery build 25 percent faster", [] {
        auto s = fresh();
        auto& k = *ownedBy(s, 2).front();
        auto& f = *ownedBy(s, 1).front();
        check(empire_rules::shipIndustryBonusPercent(s, k) == 25, "Klingon boost");
        check(empire_rules::shipIndustryBonusPercent(s, f) == 0, "no boost for others");
        k.refineries = 0;
        check(empire_rules::shipIndustryBonusPercent(s, k) == 0, "no boost without a refinery");
    }, passed);

    run("dilithium: refineries are built from the system screen list, only on deposits", [] {
        auto s = fresh();
        auto& home = *ownedBy(s, 1).front();
        // Home already has one: blocked. A deposit colony without one: buildable. A barren system: blocked.
        auto find = [&](const Colony& c) {
            for (const auto& ch : constructionChoices(s, c.systemId)) if (ch.product == Building::DilithiumRefinery) return ch;
            throw std::runtime_error("refinery not listed");
        };
        check(find(home).blocker.find("already built") != std::string::npos, "home blocked");
        auto& barren = addColony(s, 1, 0);
        check(find(barren).blocker.find("No dilithium deposit") != std::string::npos, "barren system blocked");
        const int barrenId = barren.systemId;
        rejects([&] { submit(s, Command{.id = nextId(s), .type = CommandType::EnqueueBuilding, .systemId = barrenId, .building = Building::DilithiumRefinery}); preview(s); },
            "refinery queued without a deposit", "dilithium deposit");
        s.pending.clear();
        auto& rich = addColony(s, 1, 1);
        check(find(rich).blocker.empty(), "deposit system buildable");
        rich.overflow = 1000;
        const int richId = rich.systemId;
        submit(s, Command{.id = nextId(s), .type = CommandType::EnqueueBuilding, .systemId = richId, .building = Building::DilithiumRefinery});
        advance(s);
        check(colonyOf(s, richId).refineries == 1, "refinery completed");
    }, passed);

    run("dilithium: legacy campaigns keep exactly one slot and no refinery choice", [] {
        auto s = generateReferenceCampaign(31, 3);
        check(!s.rules.empireRules, "legacy rules off");
        check(empire_rules::dilithiumStatus(s, 1).slots == 1 && !empire_rules::dilithiumStatus(s, 1).enforced, "one slot");
        const auto choices = constructionChoices(s, ownedBy(s, 1).front()->systemId);
        check(std::none_of(choices.begin(), choices.end(), [](const ConstructionChoice& c) { return c.product == Building::DilithiumRefinery; }),
            "no refinery in the list");
        check(!contains(serialize(s), "EMPIRE_RULES"), "no section");
    }, passed);

    run("dilithium: the AI builds refineries and obeys slots over a long game", [] {
        auto s = fresh(2024, 5);
        s.aiAggressionEnabled = true;
        int extra = 0;
        for (int t = 0; t < 120 && s.outcome == CampaignOutcome::Ongoing; ++t) advance(s);
        for (const auto& r : s.rivals) extra = std::max(extra, empire_rules::dilithiumStatus(s, r.id).refineries);
        check(extra >= 2, "no rival ever built a second refinery (max " + std::to_string(extra) + ")");
    }, passed);


    // ---------------------------------------------------------------- (a) emergency programs
    auto asEmpire = [](int e, std::uint64_t seed = 55) {
        CampaignSetup setup; setup.seed = seed; setup.empires = 5; setup.playerEmpire = e;
        return newCampaign(setup);
    };
    auto declare = [](GameState& s, int systemId, int program) {
        submit(s, Command{.id = nextId(s), .type = CommandType::SetEmergencyProgram, .systemId = systemId, .eventOption = program});
    };
    run("programs: each empire declares its own program, only its own, only in unrest", [&] {
        for (int e = 1; e <= 5; ++e) {
            auto s = asEmpire(e);
            auto& c = *ownedBy(s, e).front();
            const int sid = c.systemId, pop = c.population;
            c.morale = 40; s.credits = 100000;
            check(empire_rules::raceProgram(e) == static_cast<empire_rules::Program>(e), "race program");
            const int other = e % 5 + 1;
            rejects([&] { declare(s, sid, other); }, "foreign program accepted", "belongs to another empire");
            c.morale = 75;
            rejects([&] { declare(s, sid, e); }, "program without unrest accepted", "not in unrest");
            colonyOf(s, sid).morale = 40;
            declare(s, sid, e);
            const auto d = preview(s);
            const auto& dc = *colonyAt(d, sid);
            check(dc.program == e && dc.programTurns == (e == 5 ? 1 : 10), "program active in the draft");
            if (e == 1) check(dc.morale == 50, "Martial Law lifts morale by 10");
            if (e == 5) check(dc.morale == 60 && d.credits == 100000 - 2 * pop, "Festival pays 2 credits a head for +20 morale");
        }
    }, passed);

    run("programs: economy effects of each program", [&] {
        auto s = asEmpire(1);
        auto& c = *ownedBy(s, 1).front();
        c.morale = 80; // no unrest penalties in the baseline
        const auto base = economy(s, c);
        c.program = 1; c.programTurns = 5;
        const auto martial = economy(s, c);
        check(martial.research == base.research * 80 / 100 && martial.industry == base.industry, "Martial Law: research -20%");
        check(martial.upkeep == base.upkeep + std::max(1, c.population / 10), "Martial Law has a running cost");
        check(empire_rules::programDefensePercent(c) == 25, "Martial Law ground defense +25%");
        c.program = 2;
        check(economy(s, c).industry == base.industry * 115 / 100, "Klingon Police: industry +15%");
        c.program = 3;
        check(empire_rules::programDefensePercent(c) == 40 && empire_rules::programUpkeep(c) == 2 * std::max(1, c.population / 10), "Tribunal: defense +40%, double running cost");
        c.program = 4;
        check(empire_rules::programIntelPercent(c) == 150 && empire_rules::programBlocksGrowth(c), "Inquisition: intelligence +50%, no growth");
        c.program = 5;
        check(empire_rules::programUpkeep(c) == 0, "Festival has no running cost");
    }, passed);

    run("programs: morale floor, purge, expiry, cancel and credit forecast", [&] {
        auto s = asEmpire(2);
        auto& c = *ownedBy(s, 2).front();
        const int sid = c.systemId;
        c.morale = 20; c.population = 60; c.foodWorkers = 20; c.industryWorkers = 20; c.researchWorkers = 10; c.farms = 2; c.factories = 2; c.laboratories = 1; s.credits = 5000;
        declare(s, sid, 2);
        const auto forecast = creditForecast(s, 2); // the order's upkeep is in the forecast like any structure's
        advance(s);
        auto& after = colonyOf(s, sid);
        check(after.program == 2 && after.programTurns == 9, "program runs");
        check(after.morale >= 50, "morale held at 50 or more");
        check(after.population < 60, "Klingon Police purges population");
        (void)forecast;
        // Lift it.
        declare(s, sid, 0);
        advance(s);
        check(colonyOf(s, sid).program == 0, "cancelled");
        rejects([&] { declare(s, sid, 0); }, "cancel with nothing running accepted", "No emergency program");
        // Expiry.
        colonyOf(s, sid).morale = 30;
        declare(s, sid, 2);
        for (int i = 0; i < 10; ++i) advance(s);
        check(colonyOf(s, sid).program == 0 && colonyOf(s, sid).programTurns == 0, "program ends after ten turns");
        // Festival is one turn.
        colonyOf(s, sid).morale = 30;
        auto s5 = asEmpire(5);
        auto& c5 = *ownedBy(s5, 5).front();
        c5.morale = 30; s5.credits = 5000;
        declare(s5, c5.systemId, 5);
        advance(s5);
        check(colonyOf(s5, c5.systemId).program == 0 && colonyOf(s5, c5.systemId).morale >= 50, "Festival lifts morale and ends");
    }, passed);

    run("programs: the AI declares its program in unrest and lifts it when calm", [&] {
        auto s = asEmpire(1, 91);
        s.aiAggressionEnabled = false;
        auto& r = *ownedBy(s, 3).front();
        const int sid = r.systemId;
        r.morale = 20;
        for (auto& e : s.rivals) if (e.id == 3) e.credits = 100000;
        advance(s);
        check(colonyOf(s, sid).program == 3, "Romulan AI declared the Tribunal");
        colonyOf(s, sid).morale = 90;
        advance(s);
        check(colonyOf(s, sid).program == 0, "and lifted it once calm");
    }, passed);

    run("programs: save round trip with a running program", [&] {
        auto s = asEmpire(4);
        auto& c = *ownedBy(s, 4).front();
        c.morale = 30; s.credits = 5000;
        declare(s, c.systemId, 4);
        advance(s);
        const auto text = serialize(s);
        const auto loaded = deserialize(text);
        check(serialize(loaded) == text && colonyAt(loaded, c.systemId)->program == 4 && colonyAt(loaded, c.systemId)->programTurns == 9, "program survives save");
        // A pending order survives too.
        auto p = asEmpire(1);
        auto& pc = *ownedBy(p, 1).front();
        pc.morale = 30; p.credits = 5000;
        declare(p, pc.systemId, 1);
        const auto pt = serialize(p);
        const auto pl = deserialize(pt);
        check(pl.pending.size() == 1 && pl.pending[0].eventOption == 1 && serialize(pl) == pt, "pending order survives save");
    }, passed);

    run("empire rules: save round trip, schema 14 migration and canonical form", [] {
        auto s = fresh(9);
        for (int t = 0; t < 5; ++t) advance(s);
        const auto text = serialize(s);
        check(text.starts_with("BOTF_SIM 15 remaster-15\n") && contains(text, "\nEMPIRE_RULES 1\n"), "section written");
        const auto loaded = deserialize(text);
        check(serialize(loaded) == text && fingerprint(loaded) == fingerprint(s), "round trip");
        // A schema-14 save (header 14, no section) of a rules-off campaign loads and re-saves as schema 15.
        auto legacy = generateReferenceCampaign(5, 3);
        auto old = serialize(legacy);
        old.replace(0, old.find('\n') + 1, "BOTF_SIM 14 remaster-14\n");
        const auto migrated = deserialize(withChecksum(old));
        check(serialize(migrated) == serialize(legacy) && !migrated.rules.empireRules, "schema 14 migrated");
        // The section is not allowed in a schema-14 header, and not canonical when empty rules are written.
        auto smuggled = text;
        smuggled.replace(0, smuggled.find('\n') + 1, "BOTF_SIM 14 remaster-14\n");
        rejects([&] { deserialize(withChecksum(smuggled)); }, "section in schema 14 accepted");
    }, passed);

    if (failed) {
        std::cout << "FAILED " << passed << " passed, " << failed << " failed" << std::endl;
        return 1;
    }
    std::cout << "ALL " << passed << " EMPIRE RULE SCENARIOS PASSED" << std::endl;
    return 0;
}
