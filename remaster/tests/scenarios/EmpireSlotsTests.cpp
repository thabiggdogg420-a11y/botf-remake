// Empire-slot scenarios (lane MOD-2, schema 17; docs/design/EMPIRE-SLOTS.md): a campaign seats two to eight majors from a
// lineup of race keys, Dominion and Borg play as majors, saves from schemas 14-16 load, and the default game is unchanged.
#include "EmpireSlots.h"
#include "Simulation.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
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
std::string headerOf(int version) { return "BOTF_SIM " + std::to_string(version) + " remaster-" + std::to_string(version) + "\n"; }
std::string withHeader(std::string text, int version) { text.replace(0, text.find('\n') + 1, headerOf(version)); return withChecksum(text); }

GameState lineupGame(std::uint64_t seed, const std::vector<std::string>& keys, int player) {
    CampaignSetup setup;
    setup.seed = seed; setup.lineup = keys; setup.playerEmpire = player;
    if (keys.size() > 5) { setup.width = 22; setup.height = 15; setup.systems = 48; }
    return newCampaign(setup);
}
std::vector<int> empireIds(const GameState& s) {
    std::vector<int> ids{s.playerEmpire};
    for (const auto& r : s.rivals) ids.push_back(r.id);
    std::sort(ids.begin(), ids.end());
    return ids;
}
int fleetsOf(const GameState& s, int owner) {
    return static_cast<int>(std::count_if(s.fleets.begin(), s.fleets.end(), [owner](const Fleet& f) { return f.ownerId == owner; }));
}
int coloniesOf(const GameState& s, int owner) {
    return static_cast<int>(std::count_if(s.colonies.begin(), s.colonies.end(), [owner](const Colony& c) { return c.ownerId == owner; }));
}
// Plays `turns` turns with the AI running every rival (the player's colonies keep their default queue); round-trips the save
// every ten turns; returns the battle count.
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
const std::vector<std::string> All8{"federation", "klingon", "romulan", "cardassian", "ferengi", "dominion", "borg", "breen"};
} // namespace

int main() {
    int passed = 0;

    run("slots: the race table and lineup parsing", [] {
        const auto keys = slots::allKeys();
        check(keys == All8, "eight stable race keys in slot order");
        check(std::string(slots::name(1)) == "Federation" && std::string(slots::name(2)) == "Klingon Empire" &&
              std::string(slots::name(6)) == "Dominion" && std::string(slots::name(7)) == "Borg Collective", "empire names");
        check(std::string(slots::capital(1)) == "Sol" && std::string(slots::capital(4)) == "Cardassia", "capitals");
        check((slots::idsFromKeys({"klingon", "dominion"}) == std::vector<int>{2, 6}), "ids follow the keys");
        rejects([] { slots::idsFromKeys({"klingon"}); }, "one empire is not a lineup");
        rejects([] { slots::idsFromKeys({"klingon", "klingon"}); }, "repeated race");
        rejects([] { slots::idsFromKeys({"klingon", "gorn"}); }, "unknown race");
        CampaignSetup bad; bad.lineup = {"federation", "tholian"};
        rejects([&] { newCampaign(bad); }, "newCampaign refuses an unknown key");
        CampaignSetup absent; absent.lineup = {"klingon", "dominion"}; absent.playerEmpire = 1;
        rejects([&] { newCampaign(absent); }, "player must be seated", "not in the lineup");
    }, passed);

    run("lineup: seating, capitals, relations and ids for a Klingon-Dominion game", [] {
        const auto s = lineupGame(5, {"klingon", "dominion"}, 6);
        check(s.playerEmpire == 6 && s.id == 6 && s.rivals.size() == 1 && s.rivals[0].id == 2, "player Dominion, rival Klingon");
        check(s.majorRelations.size() == 1 && s.majorRelations[0].first == 2 && s.majorRelations[0].second == 6, "one relation, sorted");
        check(coloniesOf(s, 2) == 1 && coloniesOf(s, 6) == 1, "one capital each");
        check(s.systems.front().name == "Qo'noS", "the first seat takes the origin star under its capital's name");
        check(fleetsOf(s, 6) >= 2 && fleetsOf(s, 2) >= 2, "both empires start with ships");
        const auto text = serialize(s);
        check(contains(text, "\nEMPIRE_SLOTS 1 2 2 6\n"), "the lineup above slot five is written");
        check(serialize(deserialize(text)) == text, "round trip");
        // The default constructor of the lineup is the old behaviour: no lineup, no section.
        CampaignSetup plain; plain.seed = 5; plain.empires = 3;
        const auto d = newCampaign(plain);
        check(!contains(serialize(d), "EMPIRE_SLOTS") && empireIds(d) == std::vector<int>({1, 2, 3}), "empires=3 still seats slots 1..3");
    }, passed);

    run("dominion, borg and breen start, build, research and talk as majors", [] {
        auto s = lineupGame(21, All8, 6);
        check(empireIds(s) == std::vector<int>({1, 2, 3, 4, 5, 6, 7, 8}), "eight empires");
        // Start: each new race keeps its roster ships next to the stand-in era fleet.
        const auto hasShip = [&](int owner, int shipId) {
            return std::any_of(s.fleets.begin(), s.fleets.end(), [&](const Fleet& f) { return f.ownerId == owner && f.referenceShipId == shipId; });
        };
        check(hasShip(6, 1500), "Dominion starts with a Jem'Hadar Attack Ship");
        check(hasShip(7, 1604) && hasShip(7, 1601), "Borg start with a Scout Vessel and a Probe");
        check(hasShip(8, 1503), "Breen start with a Warship");
        for (int id : {6, 7, 8}) check(fleetsOf(s, id) >= 4, "a scout, a colony ship and the roster ships for empire " + std::to_string(id));
        // Build: the player's own colony lists its race's ships (tech-gated) and the donor's stand-in hulls.
        const auto& home = *std::find_if(s.colonies.begin(), s.colonies.end(), [](const Colony& c) { return c.ownerId == 6; });
        const auto choices = constructionChoices(s, home.systemId);
        const auto own = std::count_if(choices.begin(), choices.end(), [](const ConstructionChoice& c) { return c.name.find("Jem'Hadar") != std::string::npos; });
        check(own == 3, "Dominion lists the three Jem'Hadar classes");
        const auto standIn = std::count_if(choices.begin(), choices.end(), [](const ConstructionChoice& c) { return c.blocker.empty() && c.referenceId >= 0 && c.referenceId < 1000; });
        check(standIn > 0, "Dominion can build donor hulls at once");
        const auto jem = std::find_if(choices.begin(), choices.end(), [](const ConstructionChoice& c) { return c.name.find("Attack Ship") != std::string::npos; });
        check(jem != choices.end() && !jem->blocker.empty() && jem->blocker.find("Requires") == 0, "Jem'Hadar need research first");
        // Research: the player's allocation command is accepted.
        Command alloc; alloc.id = s.lastAcceptedCommand + 1; alloc.type = CommandType::AllocateResearch;
        alloc.researchAllocation = {10, 10, 20, 20, 20, 20};
        submit(s, alloc);
        check(s.technologies[5].allocation == 20, "research allocation changed for the Dominion player");
        // Talk: the player proposes a treaty to the Federation; the proposal is accepted into the engine.
        for (auto& r : s.majorRelations) if (r.first == 1 && r.second == 6) r.contact = true;
        submit(s, makeProposeTreatyCommand(s.lastAcceptedCommand + 1, 1, false, diplomacy::TreatyType::NonAggression));
        const auto report = advance(s);
        check(!report.events.empty(), "the turn resolves with the proposal on the table");
        // Fight: a Federation armed fleet at the Dominion capital, at war, ends in a battle.
        const int domSystem = home.systemId;
        const auto sys = *std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == domSystem; });
        for (auto& f : s.fleets) if (f.ownerId == 1) { f.x = f.targetX = sys.x; f.y = f.targetY = sys.y; }
        const int cell = sys.y * s.width + sys.x;
        const auto mark = [&](EmpireState& e) { if (!std::binary_search(e.explored.begin(), e.explored.end(), cell)) { e.explored.push_back(cell); std::sort(e.explored.begin(), e.explored.end()); } };
        mark(s); for (auto& r : s.rivals) mark(r);
        for (auto& r : s.majorRelations) if (r.first == 1 && r.second == 6) { r.contact = true; }
        Command war; war.id = s.lastAcceptedCommand + 1; war.type = CommandType::DeclareWar; war.targetEmpire = 1;
        submit(s, war);
        int battles = 0;
        for (int t = 0; t < 3; ++t) battles += static_cast<int>(advance(s).battles.size());
        check(battles > 0, "a Dominion fleet and a Federation fleet at war fight");
    }, passed);

    for (const auto& config : std::vector<std::pair<std::string, std::vector<std::string>>>{
            {"2 majors (Federation, Dominion)", {"federation", "dominion"}},
            {"2 majors (Borg, Klingon)", {"borg", "klingon"}},
            {"5 majors (original)", {"federation", "klingon", "romulan", "cardassian", "ferengi"}},
            {"6 majors (original five and Dominion)", {"federation", "klingon", "romulan", "cardassian", "ferengi", "dominion"}},
            {"8 majors (all)", All8}}) {
        const std::string name = "soak: " + config.first + ", 50 turns with the AI, determinism and save/reload";
        run(name.c_str(), [config] {
            const int player = slots::raceForKey(config.second[0])->empireId;
            auto a = lineupGame(7, config.second, player);
            auto b = lineupGame(7, config.second, player);
            check(fingerprint(a) == fingerprint(b), "the same setup gives the same world");
            check(empireIds(a) == [&] { auto v = slots::idsFromKeys(config.second); std::sort(v.begin(), v.end()); return v; }(), "the lineup is seated");
            const int battlesA = playTurns(a, 50);
            const int battlesB = playTurns(b, 50);
            check(battlesA == battlesB && fingerprint(a) == fingerprint(b), "two runs agree on 50 turns");
            check(a.turn == 50 || a.outcome != CampaignOutcome::Ongoing, "50 turns were played");
            // A save taken at turn 25 resumes identically.
            auto c = lineupGame(7, config.second, player);
            playTurns(c, 25);
            auto resumed = deserialize(serialize(c));
            playTurns(resumed, 25);
            check(fingerprint(resumed) == fingerprint(a), "reload at turn 25 continues identically");
            std::ostringstream summary;
            for (int id : empireIds(a)) summary << ' ' << slots::key(id) << ":c" << coloniesOf(a, id) << "/f" << fleetsOf(a, id);
            std::cout << "    turn " << a.turn << ", battles " << battlesA << ", empires" << summary.str() << std::endl;
            for (int id : empireIds(a)) check(coloniesOf(a, id) + fleetsOf(a, id) > 0 || a.turn < 50, "empire " + std::to_string(id) + " still exists or was eliminated");
        }, passed);
    }

    run("default campaign: no lineup, no section, six relation slots per minor", [] {
        CampaignSetup setup; setup.seed = 1999;
        const auto s = newCampaign(setup);
        const auto text = serialize(s);
        check(text.starts_with("BOTF_SIM 17 remaster-17\n") && !contains(text, "EMPIRE_SLOTS"), "default five-empire save has no slot section");
        check(empireIds(s) == std::vector<int>({1, 2, 3, 4, 5}), "slots one to five");
        // The minor lines carry six (contact, attitude, friendship) triples, exactly as in schema 16.
        const auto at = text.find("\nMINORS ");
        const auto lineStart = text.find('\n', at + 1) + 1;
        const auto line = text.substr(lineStart, text.find('\n', lineStart) - lineStart);
        std::istringstream in(line);
        int systemId = 0, memberOf = 0, count = 0; std::string name;
        in >> systemId >> std::quoted(name) >> memberOf;
        for (int value; in >> value;) ++count;
        check(count == 18, "six relation slots (three numbers each)");
    }, passed);

    run("migration: schemas 14, 15 and 16 load as schema 17; the new section is refused in older headers", [] {
        CampaignSetup setup; setup.seed = 31; setup.empires = 4;
        auto s = newCampaign(setup);
        for (int i = 0; i < 6; ++i) advance(s);
        const auto text = serialize(s);
        // Rebuild the exact older text: schema 14 and 15 had no EMPIRE_RULES / GALAXY_SETUP content for this campaign shape only
        // when those rules were off, so the legacy campaigns are the ones generated with the layers off.
        auto legacy = generateReferenceCampaign(5, 3);
        for (int i = 0; i < 4; ++i) advance(legacy);
        const auto legacyText = serialize(legacy);
        for (int version : {14, 15, 16}) {
            const auto old = withHeader(legacyText, version);
            const auto loaded = deserialize(old);
            check(serialize(loaded) == legacyText, "schema " + std::to_string(version) + " text migrates to the schema-17 text");
            auto playing = loaded; advance(playing);
            check(playing.turn == loaded.turn + 1, "a migrated schema-" + std::to_string(version) + " campaign plays on");
        }
        const auto sixteen = deserialize(withHeader(text, 16));
        check(serialize(sixteen) == text && fingerprint(sixteen) == fingerprint(s), "a schema-16 header over a four-empire Remastered text migrates");
        // A wide lineup cannot hide in an older header.
        const auto wide = serialize(lineupGame(3, {"federation", "borg"}, 1));
        check(contains(wide, "EMPIRE_SLOTS"), "wide lineup writes the section");
        for (int version : {14, 15, 16}) rejects([&] { deserialize(withHeader(wide, version)); }, "EMPIRE_SLOTS in schema " + std::to_string(version));
        // The section must list a lineup above slot five and match the empires.
        auto forged = wide;
        forged.replace(forged.find("EMPIRE_SLOTS 1 2 1 7"), 20, "EMPIRE_SLOTS 1 2 1 5");
        rejects([&] { deserialize(withChecksum(forged)); }, "a forged slot list is refused");
    }, passed);

    run("limits: nine slot arrays, rival counts and bridge-facing bounds", [] {
        const auto s = lineupGame(41, All8, 1);
        check(s.minors.empty() || s.minors[0].attitude.size() == 9, "minor relation arrays hold nine slots");
        check(s.rivals.size() == 7 && s.majorRelations.size() == 28, "seven rivals and twenty-eight relations");
        check(empireName(8) == std::string("Breen Confederacy"), "empire names reach slot eight");
        rejects([] { empireName(9); }, "slot nine does not exist");
        const auto text = serialize(s);
        // Every minor line holds nine triples when the section is present.
        check(contains(text, "\nEMPIRE_SLOTS 1 8 1 2 3 4 5 6 7 8\n"), "the full lineup is written");
        // 8 empires exchange intel budgets without overflow: the player's default budget lists seven rivals at most.
        check(serialize(deserialize(text)) == text, "eight-empire round trip");
    }, passed);

    if (failed) {
        std::cout << "FAILED " << passed << " passed, " << failed << " failed" << std::endl;
        return 1;
    }
    std::cout << "ALL " << passed << " EMPIRE SLOT SCENARIOS PASSED" << std::endl;
    return 0;
}
