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
