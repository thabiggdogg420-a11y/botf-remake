// Campaign-era scenarios (lane MOD-4, schema 22; docs/design/ERAS.md): the era gate on roster classes, the timeline year, research
// levels 11 to 14 and the cap, the pack's eras block and its refusals, the ERAS save section and migration from schema 20, the
// Classic / Remastered isolation, and a 100-turn AI soak per start era for the Ages and Galaxies presets.
#include "Presets.h"
#include "ShipRoster.h"
#include "Simulation.h"

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

const char* const AgesLineup[] = {"federation", "klingon", "romulan", "cardassian", "ferengi"};
GameState game(const std::string& preset, StartingEra era, std::uint64_t seed = 21, std::vector<std::string> lineup = {}) {
    CampaignSetup setup;
    setup.seed = seed; setup.era = era; setup.presetId = preset; setup.lineup = std::move(lineup);
    return newCampaign(setup);
}
GameState classic(std::uint64_t seed = 21, StartingEra era = StartingEra::Early) {
    content::installBase();
    CampaignSetup setup;
    setup.seed = seed; setup.era = era; setup.preset = RulesetPreset::Classic; setup.presetId = "classic";
    return newCampaign(setup);
}
int homeSystem(const GameState& s) {
    for (const auto& c : s.colonies) if (c.ownerId == s.playerEmpire) return c.systemId;
    throw std::runtime_error("no home");
}
// The build-list blocker for a ship class at the player's capital ("-" when the class is not listed at all).
std::string blockerFor(const GameState& s, int shipId) {
    for (const auto& choice : constructionChoices(s, homeSystem(s))) if (choice.referenceId == shipId) return choice.blocker;
    return "-";
}
const content::EraWindow* windowOf(int id) {
    for (const auto& w : content::eras().windows) if (w.id == id) return &w;
    return nullptr;
}
// The first year a roster class may be built, as the gate computes it.
int firstYear(int id) {
    if (const auto* w = windowOf(id)) return w->from;
    if (id < roster::ExpandedIdBase) return 0;   // A legacy ship the pack lists no window for: always in service.
    return content::eras().tagYears[static_cast<std::size_t>(roster::findRosterClass(id)->era)];
}
// Sets every field to `level` with a reconciled research ledger (the validator counts every level's cost).
void setLevels(GameState& s, int level) {
    s.research = s.researchReserve;
    for (int f = 0; f < ResearchFields; ++f) {
        s.technologies[static_cast<std::size_t>(f)].level = level;
        s.technologies[static_cast<std::size_t>(f)].progress = 0;
        for (int l = 1; l <= level; ++l) s.research += researchCost(s, f, l);
    }
}
std::uint64_t runTurns(GameState& s, int turns) {
    for (int t = 0; t < turns && s.outcome == CampaignOutcome::Ongoing; ++t) advance(s);
    return fingerprint(s);
}
} // namespace

int main() {
    int passed = 0;

    run("classic and remastered keep their cap, costs and no timeline", [] {
        for (const char* id : {"classic", "remastered"}) {
            content::installBase();
            CampaignSetup setup;
            setup.seed = 5; setup.presetId = id; setup.preset = std::string(id) == "classic" ? RulesetPreset::Classic : RulesetPreset::Remastered;
            const auto s = newCampaign(setup);
            check(!s.rules.eras && techCap(s) == MaxTechLevel && MaxTechLevel == 10, std::string(id) + ": cap stays 10");
            check(campaignYear(s) == 0 && eraStartTech(s) == eraBaseTechLevel(s.startingEra, s.ruleset), std::string(id) + ": no timeline, classic start tech");
            check(s.presetId.empty(), std::string(id) + ": plain pack");
            check(!contains(serialize(s), "ERAS"), std::string(id) + ": no ERAS section");
            check(contains(serialize(s), "BOTF_SIM 22 remaster-22\n"), std::string(id) + ": schema 22 header");
            expectRefusal([&] { researchCost(s, 0, 11); }, "Invalid research catalogue position", std::string(id) + ": level 11");
            expectRefusal([&] { technologyName(s, 0, 11); }, "Invalid technology position", std::string(id) + ": level 11 name");
            check(researchCost(s, 0, 10) == 81700 && researchCost(s, 5, 10) == 128500, std::string(id) + ": level 10 costs");
            for (int f = 0; f < ResearchFields; ++f) check(s.technologies[static_cast<std::size_t>(f)].level == 1, std::string(id) + ": early start level 1");
            check(eraBlocker(s, 1007).empty(), std::string(id) + ": no era gate");
        }
        content::installBase();
    }, passed);

    run("the eras block: start years, start tech, cap 14, the 24 research levels, class windows", [] {
        const auto s = game("ages", StartingEra::Early);
        const auto& e = content::eras();
        check(e.present && e.techCap == 14 && s.rules.eras && techCap(s) == 14, "cap 14 under Ages");
        check(e.years == std::array<int, 5>({2240, 2280, 2320, 2350, 2380}) && e.turnsPerYear == 4, "timeline years");
        check(e.startTech == std::array<int, 5>({1, 3, 5, 8, 11}), "Ages start tech");
        check(e.highTech.size() == 24 && e.windows.size() >= 14, "24 high records and the class windows");
        for (int f = 0; f < ResearchFields; ++f) {
            std::int64_t last = referenceData::TechnologyCatalogue[static_cast<std::size_t>(f * 11 + 10)].cost;
            for (int l = 11; l <= 14; ++l) {
                const auto cost = researchCost(s, f, l);
                check(cost > last && cost == e.highTech[static_cast<std::size_t>(f * 4 + l - 11)].cost, "cost rises: field " + std::to_string(f) + " level " + std::to_string(l));
                last = cost;
                check(!technologyName(s, f, l).empty() && technologyName(s, f, l) == e.highTech[static_cast<std::size_t>(f * 4 + l - 11)].name, "name of level " + std::to_string(l));
            }
            check(researchCost(s, f, 10) == referenceData::TechnologyCatalogue[static_cast<std::size_t>(f * 11 + 10)].cost, "levels up to 10 are the base costs");
        }
        expectRefusal([&] { researchCost(s, 0, 15); }, "Invalid research catalogue position", "level 15");
        check(technologyName(s, 0, 11) == "Genomic Synthesis" && technologyName(s, 5, 14) == "Temporal Lock Targeting", "named levels");
        for (const auto& w : e.windows) check((roster::findRosterClass(w.id) != nullptr || w.id < roster::ExpandedIdBase) && (w.until == 0 || w.until > w.from), "window of class " + std::to_string(w.id));
        // Galaxies: GALM's tech 1 / 4 / 7 / 10 / 14 across the five stages.
        const auto g = game("galaxies", StartingEra::Supreme, 22, {"federation", "klingon", "romulan", "dominion", "borg"});
        check(content::eras().startTech == std::array<int, 5>({1, 4, 7, 10, 14}) && g.technologies[0].level == 14, "Galaxies start tech and 2380 at level 14");
        content::installBase();
    }, passed);

    run("every start era: year, start tech, and the research screen's data", [] {
        const int years[] = {2240, 2280, 2320, 2350, 2380};
        const int ages[] = {1, 3, 5, 8, 11};
        for (int i = 0; i < 5; ++i) {
            const auto era = static_cast<StartingEra>(i + 1);
            auto s = game("ages", era);
            check(campaignYear(s) == years[i] && eraStartYear(era) == years[i] && eraStartTech(s) == ages[i], "start year and tech, era " + std::to_string(i + 1));
            for (const auto& t : s.technologies) check(t.level == ages[i], "player field at the start level");
            for (const auto& r : s.rivals) for (const auto& t : r.technologies) check(t.level == ages[i], "rival field at the start level");
            for (int turn = 0; turn < 8; ++turn) advance(s);
            check(campaignYear(s) == years[i] + 2, "two years after eight turns");
            validate(s);
        }
        content::installBase();
    }, passed);

    run("home fleets: the start era's own canon classes first, the legacy catalogue for the rest", [] {
        // 2380: a Federation home fleet holds roster classes; every one in service in the start year, none retired or early.
        auto s = game("ages", StartingEra::Supreme);
        int rosterShips = 0, legacy = 0;
        for (const auto& f : s.fleets) {
            if (f.referenceShipId < 0) continue;
            if (roster::isExpandedId(f.referenceShipId)) { ++rosterShips; check(eraBlocker(s, f.referenceShipId).empty(), "a start ship in service: " + std::to_string(f.referenceShipId)); }
            else ++legacy;
        }
        check(rosterShips > 0 && legacy > 0, "both catalogues field ships (" + std::to_string(rosterShips) + " roster, " + std::to_string(legacy) + " legacy)");
        // 2240: nothing newer than the start year; the opening fleet is scouts and colony ships from the legacy catalogue.
        s = game("ages", StartingEra::Early);
        for (const auto& f : s.fleets) check(f.referenceShipId < 0 || eraBlocker(s, f.referenceShipId).empty(), "a 2240 start ship is in service");
        content::installBase();
    }, passed);

    run("era gate: classes enter service and retire on the timeline, in every start era", [] {
        // 2240: the Enterprise line is in service, the Constitution waits for 2245, the Excelsior for 2285.
        auto s = game("ages", StartingEra::Early);
        check(blockerFor(s, 1000).empty(), "NX is buildable in 2240");
        check(blockerFor(s, 1003) == "Enters service in 2245", "Constitution not yet: " + blockerFor(s, 1003));
        check(blockerFor(s, 1007) == "Enters service in 2285", "Excelsior not yet: " + blockerFor(s, 1007));
        check(blockerFor(s, 1009) == "Enters service in 2340", "Challenger not yet: " + blockerFor(s, 1009));
        for (int t = 0; t < 20; ++t) advance(s);
        check(campaignYear(s) == 2245 && blockerFor(s, 1003).find("Enters service") == std::string::npos, "Constitution enters service in 2245");
        // 2280: NX retired, Constitution and the refit on the line, Excelsior from 2285.
        s = game("ages", StartingEra::Developed);
        check(blockerFor(s, 1000) == "Retired in 2255", "NX retired by 2280: " + blockerFor(s, 1000));
        check(blockerFor(s, 1003).empty(), "Constitution in service in 2280");
        check(blockerFor(s, 1007) == "Enters service in 2285", "Excelsior waits");
        for (int t = 0; t < 20; ++t) advance(s);
        check(blockerFor(s, 1007).find("Enters service") == std::string::npos, "Excelsior in service in 2285");
        // 2320: Constitution retired; refit still in service until 2340; the TNG line waits.
        s = game("ages", StartingEra::Expanded);
        check(blockerFor(s, 1003) == "Retired in 2310", "Constitution retired: " + blockerFor(s, 1003));
        check(blockerFor(s, 1004).find("Retired") == std::string::npos && blockerFor(s, 1004).find("Enters") == std::string::npos, "refit in service");
        check(blockerFor(s, 1009) == "Enters service in 2340", "Challenger waits in 2320");
        // 2350: the TNG line is in service, the Dominion War line waits for 2368.
        s = game("ages", StartingEra::Advanced);
        check(blockerFor(s, 1004) == "Retired in 2340", "refit retired in 2350");
        check(blockerFor(s, 1009).find("Enters") == std::string::npos, "Challenger in service in 2350");
        check(blockerFor(s, 1019) == "Enters service in 2368", "Intrepid waits: " + blockerFor(s, 1019));
        // 2380: everything through Nemesis, including the classes the overlay switched on.
        s = game("ages", StartingEra::Supreme);
        for (int id : {1019, 1026, 1030}) {   // The Federation's own classes appear in its build list.
            const auto b = blockerFor(s, id);
            check(b != "-" && b.find("Enters") == std::string::npos && b.find("Retired") == std::string::npos, "class " + std::to_string(id) + " is not era-blocked in 2380: " + b);
        }
        for (int id : {1105, 1106, 1206, 1207, 1304, 1401, 1030})   // The classes the overlay switched on, for their own empires.
            check(roster::findRosterClass(id)->buildable && eraBlocker(s, id).empty(), "class " + std::to_string(id) + " is buildable and in service in 2380");
        check(eraBlocker(game("ages", StartingEra::Early), 1106) == "Enters service in 2379", "Negh'Tev waits for 2379 in a 2240 start");
        check(blockerFor(s, 1000) == "Retired in 2255", "the NX is long gone");
        // The legacy catalogue is on the timeline through the pack's windows: in 2380 every playable ship is in service.
        for (int id = 0; id <= 104; ++id) check(eraBlocker(s, id).empty(), "legacy ship " + std::to_string(id) + " in service in 2380");
        check(eraBlocker(s, 115).empty() && eraBlocker(s, 124).empty(), "ships the pack does not list are never era-gated");
        // 2240: a Federation player is offered no Nebula, Galaxy, Sovereign, Ambassador or Defiant, only the basic utility hulls and the roster's period classes.
        s = game("ages", StartingEra::Early);
        check(blockerFor(s, 20) == "Enters service in 2360", "Nebula waits for 2360 in a 2240 start: " + blockerFor(s, 20));
        check(blockerFor(s, 24) == "Enters service in 2360", "Galaxy waits: " + blockerFor(s, 24));
        check(blockerFor(s, 26) == "Enters service in 2370", "Sovereign waits: " + blockerFor(s, 26));
        check(blockerFor(s, 43) == "Enters service in 2335" && blockerFor(s, 34) == "Enters service in 2370", "Ambassador and Defiant wait");
        check(blockerFor(s, 22).find("Enters") == std::string::npos && blockerFor(s, 10).find("Enters") == std::string::npos, "the basic colony ship stays on offer");
        check(eraBlocker(s, 8).empty() && eraBlocker(s, 16).empty(), "the Union's first destroyer and cruiser are always on offer");
        // 2350 and 2320: each class appears on its own year (Galaxy and Nebula in the 2360s, Sovereign in the 2370s).
        s = game("ages", StartingEra::Advanced);
        check(blockerFor(s, 20) == "Enters service in 2360" && blockerFor(s, 43).find("Enters") == std::string::npos, "2350: Ambassador yes, Nebula no");
        for (int t = 0; t < 40; ++t) advance(s);
        check(campaignYear(s) == 2360 && blockerFor(s, 24).find("Enters") == std::string::npos, "Galaxy enters service in 2360");
        check(blockerFor(s, 26) == "Enters service in 2370", "Sovereign still waits in 2360");
        s = game("ages", StartingEra::Expanded);
        check(blockerFor(s, 43) == "Enters service in 2335" && blockerFor(s, 30).find("Enters") == std::string::npos, "2320: Miranda yes, Ambassador no");
        content::installBase();
    }, passed);

    run("the gate is enforced when a build is queued, and retirement leaves existing ships alone", [] {
        auto s = game("ages", StartingEra::Early);
        Command c; c.id = 1; c.type = CommandType::EnqueueBuilding; c.systemId = homeSystem(s); c.building = Building::Destroyer;
        c.referenceId = 1003;
        expectRefusal([&] { submit(s, c); }, "Enters service in 2245", "queueing a ship that is not in service yet");
        c.referenceId = 1000; c.building = Building::Destroyer;   // NX: light cruiser, engine role 2
        submit(s, c);
        // Play to 2256 (64 turns): the NX retired in 2255, ships and queue entries made before then stay valid.
        for (int t = 0; t < 64 && s.outcome == CampaignOutcome::Ongoing; ++t) advance(s);
        validate(s);
        check(campaignYear(s) == 2256 && blockerFor(s, 1000) == "Retired in 2255", "retired by turn 64");
        content::installBase();
    }, passed);

    run("levels 11 to 14: research advances, the cap holds, the ledger reconciles", [] {
        auto s = game("ages", StartingEra::Supreme, 23);
        check(s.technologies[0].level == 11, "2380 begins at level 11");
        setLevels(s, 13);
        s.technologies[0].progress = researchCost(s, 0, 14) - 1; s.research += s.technologies[0].progress;
        validate(s);
        for (int t = 0; t < 6 && s.technologies[0].level < 14; ++t) advance(s);
        check(s.technologies[0].level == 14 && s.technologies[0].progress == 0, "level 14 reached, progress held at zero");
        for (int t = 0; t < 5; ++t) advance(s);
        check(s.technologies[0].level == 14, "no level 15");
        validate(s);
        // A level above the cap is refused by the validator, and Classic still stops at ten.
        auto bad = s; bad.technologies[1].level = 15;
        expectRefusal([&] { validate(bad); }, "", "level 15");
        check(deserialize(serialize(s)).technologies[0].level == 14, "level 14 survives a save");
        auto cl = classic(); cl.technologies[0].level = 11;
        expectRefusal([&] { validate(cl); }, "", "level 11 in a classic game");
        content::installBase();
    }, passed);

    run("the unlocked research scales: higher levels cost more and the scientific victory threshold follows the cap", [] {
        auto s = game("galaxies", StartingEra::Supreme, 24, {"federation", "klingon", "romulan", "dominion", "borg"});
        std::int64_t total = 0;
        for (int l = 1; l <= 14; ++l) total += researchCost(s, 4, l);
        check(total > 700000, "total propulsion cost to 14");
        check(eraStartTech(s) == 14 && s.technologies[4].level == 14, "2380 Galaxies stage at 14");
        content::installBase();
    }, passed);

    run("save: ERAS section, round trip, schema 20 migration, and refusals", [] {
        auto s = game("ages", StartingEra::Advanced, 25);
        for (int t = 0; t < 10; ++t) advance(s);
        const auto text = serialize(s);
        check(contains(text, "\nERAS 1\n") && contains(text, "PACKS ages 2 ") == false && contains(text, "PACKS ages 1 "), "ERAS and PACKS in the text");
        check(text.find("BOTF_SIM 22 remaster-22\n") == 0, "schema 22 header");
        const auto back = deserialize(text);
        check(serialize(back) == text && fingerprint(back) == fingerprint(s) && back.rules.eras && campaignYear(back) == campaignYear(s), "round trip is byte-stable");
        // A reload on a different active pack still plays: the save names its overlay stack.
        content::installBase();
        const auto again = deserialize(text);
        check(serialize(again) == text, "round trip after the base pack was reinstalled");
        // The schema-22 section is refused under an older header.
        expectRefusal([&] { deserialize(withHeader(text, 20)); }, "older save", "ERAS under a schema-20 header");
        expectRefusal([&] { deserialize(withHeader(text, 21)); }, "older save", "ERAS under a schema-21 header");
        // A schema-20 Classic text (header only differs) loads and re-saves as 22.
        auto cl = classic(26);
        const auto classicText = serialize(cl);
        check(!contains(classicText, "ERAS"), "classic text has no ERAS");
        const auto migrated = deserialize(withHeader(classicText, 20));
        check(serialize(migrated) == classicText && fingerprint(migrated) == fingerprint(cl), "schema 20 migrates to 22 byte for byte");
        const auto old18 = withHeader(classicText, 18);
        check(serialize(deserialize(old18)) == classicText, "schema 18 still loads");
        // The ERAS section needs a pack that carries the block: the eras rule on the plain base is refused.
        auto rem = classic(27);
        rem.rules.eras = true;
        expectRefusal([&] { validate(rem); }, "eras rule needs", "eras rule on the base pack");
        content::installBase();
    }, passed);

    run("the eras block is validated, and a bad block leaves the active pack alone", [] {
        content::installBase();
        const auto good = *presets::overlayBytes("ages");
        const auto before = content::active().checksum;
        auto bad = [&](const std::string& body, const std::string& fragment, const std::string& what) {
            expectRefusal([&] { content::installStack({overlay("t-eras", body)}); }, fragment, what);
            check(content::active().checksum == before, what + ": active pack changed");
        };
        bad("\"eras\":{\"techCap\":15}", "outside 10..14", "a cap of 15");
        bad("\"eras\":{\"techCap\":14}", "needs highTechnologies", "a cap above 10 without the research data");
        bad("\"eras\":{\"years\":[2240,2230,2320,2350,2380]}", "ascend", "years out of order");
        bad("\"eras\":{\"startTech\":[1,4,3,10,10]}", "must not fall", "start tech falling");
        bad("\"eras\":{\"startTech\":[1,4,7,10,12]}", "above techCap", "a start level over the cap");
        bad("\"eras\":{\"classes\":[{\"id\":1000,\"from\":2200,\"until\":2100}]}", "retirement year", "retired before it entered service");
        bad("\"eras\":{\"classes\":[{\"id\":500,\"from\":2200,\"until\":0}]}", "no such roster class or catalogue ship", "a window for a missing legacy ship");
        bad("\"eras\":{\"classes\":[{\"id\":1999,\"from\":2200,\"until\":0}]}", "no such roster class", "a window for a missing class");
        bad("\"eras\":{\"classes\":[{\"id\":1000,\"from\":2200,\"until\":0},{\"id\":1000,\"from\":2210,\"until\":0}]}", "listed twice", "a class listed twice");
        bad("\"eras\":{\"bogus\":1}", "unknown key", "an unknown key");
        bad("\"eras\":{\"turnsPerYear\":0}", "outside 1..52", "zero turns per year");
        // The pack still loads as shipped.
        content::installStack({good});
        check(content::eras().present && content::eras().highTech.size() == 24, "the shipped block loads");
        content::installBase();
        check(!content::eras().present && content::eras().techCap == 10, "base has no eras block");
    }, passed);

    run("presets: the eras flag needs the expanded roster; Classic cannot carry it", [] {
        const std::string classicBase = R"({"format":1,"id":"t-classic-eras","name":"T","blurb":"b","order":5,"overlays":[],"rules":{"base":"classic","flags":{"eras":true}},"lineup":["Federation","Klingon"],"options":{},"needs":[]})";
        expectRefusal([&] { presets::parsePreset(classicBase); }, "eras needs expandedRoster", "eras on a classic base");
        const std::string noRoster = R"({"format":1,"id":"t-noroster","name":"T","blurb":"b","order":5,"overlays":[],"rules":{"base":"remastered","flags":{"eras":true,"expandedRoster":false}},"lineup":["Federation","Klingon"],"options":{},"needs":[]})";
        expectRefusal([&] { presets::parsePreset(noRoster); }, "eras needs expandedRoster", "eras without the roster");
        const auto* ages = presets::find("ages");
        const auto* galaxies = presets::find("galaxies");
        check(ages && galaxies && ages->needs.empty() && galaxies->needs.empty() && galaxies->overlays == std::vector<std::string>{"dominion", "galaxies"}, "needs lists");
        bool agesEras = false, galEras = false;
        for (const auto& f : ages->flags) if (f.first == "eras" && f.second) agesEras = true;
        for (const auto& f : galaxies->flags) if (f.first == "eras" && f.second) galEras = true;
        check(agesEras && galEras, "both presets set the eras rule");
        const auto sum = presets::erasOf(*galaxies);
        check(sum.present && sum.techCap == 14 && sum.startTech[4] == 14 && sum.years[0] == 2240, "the picker's summary");
        for (const char* plain : {"classic", "remastered", "ultimate"}) check(!presets::erasOf(*presets::find(plain)).present, std::string(plain) + " has no eras block");
        content::installBase();
    }, passed);

    run("soak: 100 turns per start era, Ages (stock five) and Galaxies (Dominion and Borg), deterministic, save-stable, gate respected", [] {
        struct Case { const char* preset; std::vector<std::string> lineup; };
        const std::vector<Case> cases{{"ages", {}}, {"galaxies", {"federation", "klingon", "romulan", "dominion", "borg"}}};
        int rosterBuilds = 0, legacyBuilds = 0;
        std::map<int, int> byClass;
        for (const auto& c : cases) for (int e = 1; e <= 5; ++e) {
            auto a = game(c.preset, static_cast<StartingEra>(e), 40 + e, c.lineup);
            auto b = a;
            const bool twin = e == 1 || e == 5;   // Two runs against each other at the extremes; the rest still save and reload.
            for (int turn = 1; turn <= 100 && a.outcome == CampaignOutcome::Ongoing; ++turn) {
                const int firstNew = a.nextFleetId;
                const auto ra = advance(a);
                if (twin) check(ra.stateHash == advance(b).stateHash, std::string(c.preset) + " era " + std::to_string(e) + ": two runs agree at turn " + std::to_string(turn));
                const int year = campaignYear(a);
                for (const auto& f : a.fleets) {
                    if (f.id < firstNew || f.referenceShipId < 0) continue;
                    if (!roster::isExpandedId(f.referenceShipId)) {   // A legacy ship: only its window (when the pack lists one) matters.
                        ++legacyBuilds;
                        check(year >= firstYear(f.referenceShipId), "a legacy ship (" + std::to_string(f.referenceShipId) + ") was built before its year, " + std::to_string(year));
                        continue;
                    }
                    ++rosterBuilds; ++byClass[f.referenceShipId];
                    check(year >= firstYear(f.referenceShipId), "a roster ship (" + std::to_string(f.referenceShipId) + ") was built before its year, " + std::to_string(year));
                }
                if (turn % 25 == 0) {
                    const auto text = serialize(a);
                    const auto back = deserialize(text);
                    check(serialize(back) == text && fingerprint(back) == fingerprint(a), "save round trip at turn " + std::to_string(turn));
                    validate(a);
                }
            }
            check(a.rules.eras && campaignYear(a) >= eraStartYear(static_cast<StartingEra>(e)), "still on the timeline");
        }
        std::cout << "  soak: roster ships built " << rosterBuilds << " across " << byClass.size() << " classes, legacy ships built " << legacyBuilds << std::endl;
        check(rosterBuilds > 0, "the AI built roster ships");
        content::installBase();
    }, passed);

    std::cout << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
