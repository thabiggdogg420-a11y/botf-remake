// Schema-13 gameplay layer scenarios: ruleset presets, outposts and starbases,
// galaxy features, curated events, victory conditions, build templates, the
// turn chronicle, save schema 13 and its migration from schema 12.
#include "CampaignSession.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void rejects(const std::function<void()>& fn, const std::string& message) {
    bool rejected = false;
    try { fn(); } catch (const std::exception&) { rejected = true; }
    check(rejected, message);
}
bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

botf::EmpireState& empireOf(botf::GameState& s, int id) {
    if (id == s.playerEmpire) return s;
    return *std::find_if(s.rivals.begin(), s.rivals.end(), [id](const botf::EmpireState& e) { return e.id == id; });
}
void reveal(botf::GameState& s, int owner, int x, int y) {
    auto& known = empireOf(s, owner).explored;
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
        const int px = x + dx, py = y + dy;
        if (px < 0 || py < 0 || px >= s.width || py >= s.height) continue;
        known.push_back(py * s.width + px);
    }
    std::sort(known.begin(), known.end());
    known.erase(std::unique(known.begin(), known.end()), known.end());
}
void declareWar(botf::GameState& s, int a, int b) {
    auto it = std::find_if(s.majorRelations.begin(), s.majorRelations.end(), [a, b](const botf::MajorRelation& r) {
        return r.first == std::min(a, b) && r.second == std::max(a, b);
    });
    check(it != s.majorRelations.end(), "Relation missing");
    it->contact = true; it->atWar = true; it->tradeTreaty = false; it->warStartTurn = s.turn;
}
// Raises a technology while keeping the research ledger reconciled.
void raiseTech(botf::GameState& s, int owner, int field, int level) {
    auto& e = empireOf(s, owner);
    auto& tech = e.technologies[static_cast<std::size_t>(field)];
    while (tech.level < level) {
        e.research += botf::researchCost(s, field, tech.level + 1);
        ++tech.level;
    }
    if (tech.level == botf::MaxTechLevel) { e.researchReserve += tech.progress; tech.progress = 0; }
}
int addFleet(botf::GameState& s, int owner, botf::Building cls, int x, int y) {
    botf::Fleet f;
    f.id = s.nextFleetId++;
    f.ownerId = owner;
    f.x = f.targetX = x; f.y = f.targetY = y;
    f.shipClass = cls;
    f.colonyShip = cls == botf::Building::ColonyShip;
    f.hull = botf::shipMaxHull(cls);
    s.fleets.push_back(f);
    reveal(s, owner, x, y);
    return f.id;
}
botf::Fleet* fleetById(botf::GameState& s, int id) {
    const auto it = std::find_if(s.fleets.begin(), s.fleets.end(), [id](const botf::Fleet& f) { return f.id == id; });
    return it == s.fleets.end() ? nullptr : &*it;
}
int addStation(botf::GameState& s, int owner, int x, int y, botf::StationType type = botf::StationType::Outpost) {
    botf::Station st;
    st.id = s.nextStationId++;
    st.ownerId = owner;
    st.x = x; st.y = y;
    st.type = type;
    const auto stats = botf::stationStats(s, owner, type);
    st.hull = stats.maxHull; st.shields = stats.maxShields;
    st.builtTurn = s.turn;
    s.stations.push_back(st);
    reveal(s, owner, x, y);
    return st.id;
}
// By value: submit() replaces the state, so references into it would dangle.
botf::System home(const botf::GameState& s) {
    const auto colony = std::find_if(s.colonies.begin(), s.colonies.end(), [&](const botf::Colony& c) { return c.ownerId == s.playerEmpire; });
    return *std::find_if(s.systems.begin(), s.systems.end(), [&](const botf::System& x) { return x.id == colony->systemId; });
}
bool systemAt(const botf::GameState& s, int x, int y) {
    return std::any_of(s.systems.begin(), s.systems.end(), [&](const botf::System& sys) { return sys.x == x && sys.y == y; });
}
// First empty (no system/feature/station) cell at the given distance from (cx,cy).
std::pair<int, int> emptyCellAt(const botf::GameState& s, int cx, int cy, int distance,
                                const std::function<bool(int, int)>& extra = {}) {
    for (int y = 0; y < s.height; ++y) for (int x = 0; x < s.width; ++x) {
        if (std::max(std::abs(x - cx), std::abs(y - cy)) != distance || systemAt(s, x, y)) continue;
        if (!botf::featuresAt(s, x, y).empty() || botf::stationAt(s, x, y)) continue;
        if (extra && !extra(x, y)) continue;
        return {x, y};
    }
    throw std::runtime_error("No empty cell at the requested distance");
}
// A prototype-rules campaign (plain fleets) switched to a preset, AI and events quiet.
botf::GameState campaign(botf::RulesetPreset preset, std::uint64_t seed = 1999, int empires = 2, bool reference = false) {
    botf::CampaignSetup setup;
    setup.seed = seed; setup.empires = empires; setup.reference = reference; setup.preset = preset;
    auto s = botf::newCampaign(setup);
    s.aiAggressionEnabled = false;
    s.eventsEnabled = false;
    return s;
}
// Remastered rules but with no generated anomalies, for controlled fixtures.
botf::GameState cleanRemastered(std::uint64_t seed = 1999, int empires = 2, bool reference = false) {
    auto s = campaign(botf::RulesetPreset::Remastered, seed, empires, reference);
    s.galaxyFeatures.clear();
    return s;
}
std::uint64_t nextId(const botf::GameState& s) { return s.lastAcceptedCommand + 1; }
std::string roundTrip(const botf::GameState& s) {
    const auto text = botf::serialize(s);
    const auto loaded = botf::deserialize(text);
    check(botf::serialize(loaded) == text, "Save did not round-trip exactly");
    return text;
}
std::string withChecksum(std::string body) {
    if (const auto pos = body.rfind("CHECKSUM "); pos != std::string::npos) body.erase(pos);
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
std::string asSchemaTwelve(const botf::GameState& state) {
    auto text = botf::serialize(state);
    text.erase(text.rfind("CHECKSUM "));
    text.replace(0, text.find('\n') + 1, "BOTF_SIM 12 remaster-12\n");
    const auto start = text.find("\nGAMEPLAY_LAYER ");
    const auto end = text.find("\nEND_GAMEPLAY_LAYER\n", start);
    check(start != std::string::npos && end != std::string::npos, "Gameplay section missing");
    text.erase(start + 1, end + 20 - (start + 1));
    return withChecksum(text);
}
std::string sessionTurn(botf::GameState& state) {
    botf::session::TurnSession session;
    botf::session::begin(session, state);
    for (int guard = 0; guard < 256; ++guard) {
        if (session.pending && !session.pending->isFinished) botf::session::autoResolve(session);
        if (botf::session::resume(session, state)) return "done";
    }
    throw std::runtime_error("Session turn did not finish");
}

int failed = 0;
void run(const char* name, const std::function<void()>& fn, int& passed) {
    try {
        fn(); ++passed; std::cout << "PASS " << name << std::endl;
    } catch (const std::exception& error) {
        ++failed; std::cout << "FAIL " << name << ": " << error.what() << std::endl;
    }
}

void rulesetScenarios(int& passed) {
    run("presets: Remastered enables every layer, Classic keeps outposts only, old API stays baseline", [] {
        const auto remastered = campaign(botf::RulesetPreset::Remastered);
        check(remastered.rules == botf::rulesForPreset(botf::RulesetPreset::Remastered), "Remastered rules");
        check(remastered.rules.stations && remastered.rules.galaxyFeatures && remastered.rules.curatedEvents &&
            remastered.rules.alternativeVictories, "Remastered must enable all layers");
        check(!remastered.galaxyFeatures.empty() && remastered.eventDeck.size() == 14, "Features and deck generated");
        const auto classic = campaign(botf::RulesetPreset::Classic);
        check(classic.rules.stations && !classic.rules.galaxyFeatures && !classic.rules.curatedEvents &&
            !classic.rules.alternativeVictories, "Classic is original-like");
        check(classic.galaxyFeatures.empty() && classic.eventDeck.empty(), "Classic has no anomalies or deck");
        const auto baseline = botf::generateReferenceCampaign(1999);
        check(baseline.rules == botf::GameplayRules{} && !baseline.rules.stations, "Pre-13 API keeps every layer off");
        botf::RulesetPreset parsed = botf::RulesetPreset::Classic;
        check(botf::parseRulesetPreset("REMASTERED", parsed) && parsed == botf::RulesetPreset::Remastered, "Parse remastered");
        check(botf::parseRulesetPreset("classic", parsed) && parsed == botf::RulesetPreset::Classic, "Parse classic");
        check(!botf::parseRulesetPreset("modern", parsed), "Unknown preset rejected");
        check(std::string(botf::rulesetPresetName(botf::RulesetPreset::Remastered)) == "Remastered", "Preset name");
    }, passed);

    run("presets: the base galaxy is identical between Classic and Remastered for one seed", [] {
        const auto classic = campaign(botf::RulesetPreset::Classic, 4242, 5);
        const auto remastered = campaign(botf::RulesetPreset::Remastered, 4242, 5);
        check(classic.systems.size() == remastered.systems.size(), "System count differs");
        for (std::size_t i = 0; i < classic.systems.size(); ++i)
            check(classic.systems[i].x == remastered.systems[i].x && classic.systems[i].y == remastered.systems[i].y &&
                classic.systems[i].name == remastered.systems[i].name, "System layout differs");
        check(classic.fleets.size() == remastered.fleets.size() && classic.colonies.size() == remastered.colonies.size(),
            "Starting assets differ");
        check(botf::serialize(campaign(botf::RulesetPreset::Remastered, 4242, 5)) == botf::serialize(remastered),
            "Remastered generation is not deterministic");
    }, passed);

    run("presets: generated anomalies avoid systems and homes; black holes keep the map connected", [] {
        for (std::uint64_t seed : {1ULL, 1999ULL, 26713ULL, 90210ULL}) {
            const auto s = campaign(botf::RulesetPreset::Remastered, seed, 5, true);
            int wormholes = 0, blackHoles = 0, nebulae = 0;
            for (const auto& f : s.galaxyFeatures) {
                if (f.type != botf::GalaxyFeatureType::IonStorm) check(!systemAt(s, f.x, f.y), "Anomaly on a star system");
                if (f.type == botf::GalaxyFeatureType::Wormhole) ++wormholes;
                if (f.type == botf::GalaxyFeatureType::Nebula) ++nebulae;
                if (f.type == botf::GalaxyFeatureType::BlackHole) {
                    ++blackHoles;
                    check(f.x > 0 && f.y > 0 && f.x < s.width - 1 && f.y < s.height - 1, "Black hole on the border");
                    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
                        check(!systemAt(s, f.x + dx, f.y + dy), "Black hole next to a system");
                }
                for (const auto& c : s.colonies) {
                    const auto& sys = *std::find_if(s.systems.begin(), s.systems.end(), [&](const botf::System& x) { return x.id == c.systemId; });
                    check(std::max(std::abs(sys.x - f.x), std::abs(sys.y - f.y)) > 1 || f.type == botf::GalaxyFeatureType::IonStorm,
                        "Anomaly crowds a home system");
                }
            }
            check(wormholes % 2 == 0 && wormholes >= 2 && blackHoles >= 1 && nebulae >= 3, "Feature mix incomplete");
        }
    }, passed);

    run("presets: applyRulesetPreset only applies to fresh campaigns", [] {
        auto s = campaign(botf::RulesetPreset::Classic);
        botf::advance(s);
        rejects([&] { botf::applyRulesetPreset(s, botf::RulesetPreset::Remastered); }, "Preset change mid-campaign accepted");
        auto fresh = botf::generateCampaign(7, 3);
        botf::applyRulesetPreset(fresh, botf::RulesetPreset::Remastered);
        check(fresh.rules.galaxyFeatures && !fresh.galaxyFeatures.empty(), "Preset not applied");
        rejects([&] { auto bad = fresh; bad.rules.curatedEvents = false; botf::validate(bad); },
            "Remastered with a layer switched off validated");
        rejects([&] { auto bad = campaign(botf::RulesetPreset::Classic); bad.rules.galaxyFeatures = true; botf::validate(bad); },
            "Classic with anomalies validated");
        rejects([&] { auto bad = campaign(botf::RulesetPreset::Classic); bad.unifiedCombat = false; botf::validate(bad); },
            "Stations without unified combat validated");
    }, passed);

    run("gameplay command fields are validated and never leak onto other commands", [] {
        auto s = campaign(botf::RulesetPreset::Remastered);
        botf::Command leak; leak.id = nextId(s); leak.type = botf::CommandType::AssignWorkers;
        leak.systemId = home(s).id; leak.foodWorkers = 20; leak.industryWorkers = 10; leak.researchWorkers = 10; leak.stationId = 3;
        rejects([&] { botf::submit(s, leak); }, "Station field leaked onto a workforce command");
        auto badChoice = botf::makeResolveEventChoiceCommand(nextId(s), 1, 5);
        rejects([&] { botf::submit(s, badChoice); }, "Out-of-range event option accepted");
        auto noName = botf::makeDefineBuildTemplateCommand(nextId(s), 0, "", {botf::Construction(botf::Building::Farm)});
        rejects([&] { botf::submit(s, noName); }, "Unnamed template accepted");
        auto newline = botf::makeDefineBuildTemplateCommand(nextId(s), 0, "a\nb", {botf::Construction(botf::Building::Farm)});
        rejects([&] { botf::submit(s, newline); }, "Template name with a newline accepted");
        auto stray = botf::makeBuildOutpostCommand(nextId(s), 1); stray.systemId = 4;
        rejects([&] { botf::submit(s, stray); }, "BuildOutpost with a system field accepted");
    }, passed);
}

void stationScenarios(int& passed) {
    run("stations: a transport assembles an outpost over three turns, extending supply and sensors", [] {
        auto s = campaign(botf::RulesetPreset::Classic);
        const auto sol = home(s);
        // A site three sectors out whose far side lies beyond the colony supply for transports.
        const auto site = emptyCellAt(s, sol.x, sol.y, 3, [&](int x, int y) {
            for (int dy = -3; dy <= 3; ++dy) for (int dx = -3; dx <= 3; ++dx) {
                const int px = x + dx, py = y + dy;
                if (px >= 0 && py >= 0 && px < s.width && py < s.height && std::max(std::abs(px - sol.x), std::abs(py - sol.y)) >= 6)
                    return true;
            }
            return false;
        });
        const int transport = addFleet(s, 1, botf::Building::Transport, site.first, site.second);
        std::pair<int, int> far{-1, -1};
        for (int dy = -3; dy <= 3 && far.first < 0; ++dy) for (int dx = -3; dx <= 3 && far.first < 0; ++dx) {
            const int px = site.first + dx, py = site.second + dy;
            if (px >= 0 && py >= 0 && px < s.width && py < s.height && std::max(std::abs(px - sol.x), std::abs(py - sol.y)) >= 6)
                far = {px, py};
        }
        check(!botf::supplied(s, *fleetById(s, transport), far.first, far.second), "Far sector unexpectedly supplied");
        check(botf::outpostBlocker(s, transport).empty(), "Outpost blocked: " + botf::outpostBlocker(s, transport));
        botf::submit(s, botf::makeBuildOutpostCommand(nextId(s), transport));
        const auto previewed = botf::preview(s);
        check(fleetById(const_cast<botf::GameState&>(previewed), transport)->buildingOutpost, "Preview lacks the build order");
        botf::advance(s);
        check(fleetById(s, transport) && fleetById(s, transport)->outpostProgress == 1 && s.stations.empty(), "Turn 1 progress");
        roundTrip(s);
        botf::advance(s);
        const auto report = botf::advance(s);
        check(!fleetById(s, transport) && s.stations.size() == 1, "Outpost not completed after three turns");
        const auto& st = s.stations.front();
        check(st.ownerId == 1 && st.type == botf::StationType::Outpost && st.x == site.first && st.y == site.second, "Outpost identity");
        check(st.hull == botf::stationStats(s, st).maxHull, "Outpost not at full integrity");
        check(std::any_of(report.chronicle.begin(), report.chronicle.end(), [&](const botf::ChronicleEntry& e) {
            return e.category == botf::ChronicleCategory::Stations && e.stationId == st.id && contains(e.text, "Outpost completed");
        }), "Completion missing from the chronicle");
        botf::Fleet probe; probe.ownerId = 1; probe.shipClass = botf::Building::Transport;
        check(botf::supplied(s, probe, far.first, far.second), "Outpost did not extend supply");
        const auto sensor = emptyCellAt(s, st.x, st.y, 2);
        check(botf::visible(s, 1, sensor.first, sensor.second), "Outpost sensors do not reach two sectors");
        roundTrip(s);
    }, passed);

    run("stations: new orders abandon assembly; illegal sites are refused with reasons", [] {
        auto s = campaign(botf::RulesetPreset::Classic);
        const auto sol = home(s);
        const auto site = emptyCellAt(s, sol.x, sol.y, 2);
        const int transport = addFleet(s, 1, botf::Building::Transport, site.first, site.second);
        botf::submit(s, botf::makeBuildOutpostCommand(nextId(s), transport));
        botf::advance(s);
        botf::Command move; move.id = nextId(s); move.type = botf::CommandType::MoveFleet; move.fleetId = transport;
        move.targetX = sol.x; move.targetY = sol.y;
        botf::submit(s, move);
        const auto report = botf::advance(s);
        check(!fleetById(s, transport)->buildingOutpost && s.stations.empty(), "Moving transport kept building");
        check(std::any_of(report.events.begin(), report.events.end(), [](const std::string& e) { return contains(e, "abandoned"); }),
            "Abandonment not reported");
        const int scout = addFleet(s, 1, botf::Building::Scout, site.first, site.second);
        check(contains(botf::outpostBlocker(s, scout), "troop transports"), "Scouts may not build outposts");
        const auto rivalHome = std::find_if(s.colonies.begin(), s.colonies.end(), [](const botf::Colony& c) { return c.ownerId == 2; });
        const auto& rivalSys = *std::find_if(s.systems.begin(), s.systems.end(), [&](const botf::System& x) { return x.id == rivalHome->systemId; });
        const int intruder = addFleet(s, 1, botf::Building::Transport, rivalSys.x, rivalSys.y);
        check(contains(botf::outpostBlocker(s, intruder), "foreign colony"), "Outpost in a foreign system allowed");
        addStation(s, 2, site.first, site.second);
        const int second = addFleet(s, 1, botf::Building::Transport, site.first, site.second);
        check(contains(botf::outpostBlocker(s, second), "already occupies"), "Two stations in one sector allowed");
        rejects([&] { botf::submit(s, botf::makeBuildOutpostCommand(nextId(s), second)); }, "Occupied sector accepted");
        auto legacy = botf::generateCampaign(1999, 2);
        const int old = addFleet(legacy, 1, botf::Building::Transport, sol.x, sol.y);
        check(contains(botf::outpostBlocker(legacy, old), "disabled"), "Baseline campaigns must not build stations");
    }, passed);

    run("stations: upkeep is charged and a funded upgrade commissions a starbase that repairs ships", [] {
        auto s = campaign(botf::RulesetPreset::Classic);
        const auto sol = home(s);
        const auto site = emptyCellAt(s, sol.x, sol.y, 2);
        const int stationId = addStation(s, 1, site.first, site.second);
        const auto cost = botf::stationUpgradeCost(s, 1);
        check(contains(botf::stationUpgradeBlocker(s, stationId, sol.id), "Construction level"), "Upgrade needs Construction 2");
        raiseTech(s, 1, 2, 2);
        s.credits = 5000;
        check(botf::stationUpgradeBlocker(s, stationId, sol.id).empty(), "Upgrade blocked: " + botf::stationUpgradeBlocker(s, stationId, sol.id));
        botf::submit(s, botf::makeUpgradeStationCommand(nextId(s), stationId, sol.id));
        const auto upkeep = botf::stationStats(s, 1, botf::StationType::Outpost).upkeep;
        const auto report = botf::advance(s);
        check(report.fleetUpkeep >= upkeep, "Station upkeep not charged");
        check(s.stationUpgrades.size() == 1 && s.stationUpgrades.front().work > 0, "Industry not diverted to the upgrade");
        const auto colonyReport = std::find_if(report.colonies.begin(), report.colonies.end(), [&](const botf::ColonyReport& c) { return c.systemId == sol.id; });
        check(colonyReport != report.colonies.end() && colonyReport->economy.industry == 0, "Funding colony still building");
        check(s.credits < 5000 - cost.credits + 1000, "Upgrade credits not deducted");
        rejects([&] { botf::submit(s, botf::makeUpgradeStationCommand(nextId(s), stationId, sol.id)); }, "Double upgrade accepted");
        s.stationUpgrades.front().work = cost.industry - 1;
        const auto done = botf::advance(s);
        const auto* st = botf::stationById(s, stationId);
        check(st && st->type == botf::StationType::Starbase && s.stationUpgrades.empty(), "Starbase not commissioned");
        check(st->hull == botf::stationStats(s, *st).maxHull, "Starbase not at full integrity");
        check(std::any_of(done.chronicle.begin(), done.chronicle.end(), [](const botf::ChronicleEntry& e) {
            return contains(e.text, "Starbase commissioned"); }), "Commission not reported");
        const int damaged = addFleet(s, 1, botf::Building::Destroyer, st->x, st->y);
        fleetById(s, damaged)->hull = 5;
        botf::advance(s);
        check(fleetById(s, damaged)->hull > 5, "Starbase did not repair a docked ship");
        roundTrip(s);
    }, passed);

    run("stations: an outpost fights as an immobile unit, is damaged and can be destroyed", [] {
        auto s = campaign(botf::RulesetPreset::Classic);
        const auto sol = home(s);
        const auto site = emptyCellAt(s, sol.x, sol.y, 2);
        const int stationId = addStation(s, 1, site.first, site.second);
        declareWar(s, 1, 2);
        const int raider = addFleet(s, 2, botf::Building::Destroyer, site.first, site.second);
        auto report = botf::advance(s);
        check(!report.battles.empty(), "No battle at the outpost");
        const auto& battle = report.battles.front();
        check(std::any_of(battle.participants.begin(), battle.participants.end(), [&](const botf::CombatParticipant& p) {
            return botf::isStationCombatant(p.fleetId) && botf::stationIdForCombatant(p.fleetId) == stationId && p.ownerId == 1;
        }), "Outpost did not join the battle");
        const auto* st = botf::stationById(s, stationId);
        check(!st || st->hull < botf::stationStats(s, *st).maxHull || !fleetById(s, raider), "Battle had no effect");
        roundTrip(s);
        // A squadron overwhelms the outpost.
        auto overwhelmed = campaign(botf::RulesetPreset::Classic);
        const int target = addStation(overwhelmed, 1, site.first, site.second);
        declareWar(overwhelmed, 1, 2);
        raiseTech(overwhelmed, 2, 5, 4);
        for (int i = 0; i < 8; ++i) addFleet(overwhelmed, 2, botf::Building::Destroyer, site.first, site.second);
        report = botf::advance(overwhelmed);
        check(!botf::stationById(overwhelmed, target), "Outpost survived eight destroyers");
        check(std::any_of(report.battles.front().lostFleetIds.begin(), report.battles.front().lostFleetIds.end(),
            [](int id) { return botf::isStationCombatant(id); }), "Station loss not recorded");
        check(std::any_of(report.chronicle.begin(), report.chronicle.end(), [](const botf::ChronicleEntry& e) {
            return e.category == botf::ChronicleCategory::Stations && contains(e.text, "destroyed"); }), "Destruction not reported");
        roundTrip(overwhelmed);
    }, passed);

    run("stations: a player station battle pauses the session, never moves, serializes and matches headless advance", [] {
        auto s = campaign(botf::RulesetPreset::Classic);
        const auto sol = home(s);
        const auto site = emptyCellAt(s, sol.x, sol.y, 2);
        const int stationId = addStation(s, 1, site.first, site.second);
        declareWar(s, 1, 2);
        addFleet(s, 2, botf::Building::Destroyer, site.first, site.second);
        auto headless = s;
        botf::advance(headless);

        botf::session::TurnSession session;
        botf::session::begin(session, s);
        check(!botf::session::resume(session, s), "Player station battle did not pause the session");
        check(session.pending.has_value(), "No pending encounter");
        const int combatant = botf::StationCombatantBase + stationId;
        const auto* ship = session.pending->getShip(combatant);
        check(ship && ship->immobile && !ship->canRetreat && !ship->canCloak, "Station is not an immobile combatant");
        const auto start = ship->position;
        const auto text = botf::tactical::serializeEncounter(*session.pending);
        check(contains(text, "\nFIXED 1 " + std::to_string(combatant) + "\n"), "FIXED section missing");
        check(botf::tactical::serializeEncounter(botf::tactical::deserializeEncounter(text)) == text, "Encounter did not round-trip");
        botf::session::order(session, {combatant, 0, botf::tactical::TacticalManeuver::Charge});
        botf::session::resolveRound(session);
        const auto* moved = session.pending->getShip(combatant);
        check(moved->position.x == start.x && moved->position.y == start.y, "Station moved");
        auto reloaded = botf::session::deserialize(botf::session::serialize(session));
        botf::session::autoResolve(reloaded);
        check(botf::session::resume(reloaded, s).has_value(), "Session did not finish");
        auto base2 = campaign(botf::RulesetPreset::Classic);
        addStation(base2, 1, site.first, site.second);
        declareWar(base2, 1, 2);
        addFleet(base2, 2, botf::Building::Destroyer, site.first, site.second);
        sessionTurn(base2);
        check(botf::fingerprint(base2) == botf::fingerprint(headless), "Session and headless advance diverged with a station");
    }, passed);

    run("stations: a transport boards a weakened, undefended station; defenders prevent it", [] {
        auto s = campaign(botf::RulesetPreset::Classic);
        const auto sol = home(s);
        const auto site = emptyCellAt(s, sol.x, sol.y, 2);
        const int stationId = addStation(s, 1, site.first, site.second);
        auto& st = s.stations.back();
        st.hull = botf::stationStats(s, st).maxHull / 2;
        declareWar(s, 1, 2);
        auto defended = s;
        const int boarders = addFleet(s, 2, botf::Building::Transport, site.first, site.second);
        const auto report = botf::advance(s);
        const auto* captured = botf::stationById(s, stationId);
        check(captured && captured->ownerId == 2 && !fleetById(s, boarders), "Station not captured by boarding");
        check(std::any_of(report.chronicle.begin(), report.chronicle.end(), [](const botf::ChronicleEntry& e) {
            return contains(e.text, "captured"); }), "Capture not reported to the former owner");
        roundTrip(s);
        addFleet(defended, 1, botf::Building::Destroyer, site.first, site.second);
        addFleet(defended, 2, botf::Building::Transport, site.first, site.second);
        botf::advance(defended);
        const auto* kept = botf::stationById(defended, stationId);
        check(!kept || kept->ownerId == 1, "Defended station was captured");
    }, passed);

    run("stations: rivals only see stations inside their sensor coverage", [] {
        auto s = campaign(botf::RulesetPreset::Classic);
        const auto sol = home(s);
        const auto site = emptyCellAt(s, sol.x, sol.y, 3);
        addStation(s, 2, site.first, site.second);
        const auto seen = [&]() { return botf::visibleStations(s, 1).size(); };
        bool playerAdjacent = false;
        for (const auto& f : s.fleets)
            if (f.ownerId == 1 && std::max(std::abs(f.x - site.first), std::abs(f.y - site.second)) <= 1) playerAdjacent = true;
        check(playerAdjacent || seen() == 0, "Unobserved rival station visible");
        addFleet(s, 1, botf::Building::Scout, site.first, site.second);
        check(seen() == 1, "Adjacent rival station invisible");
        check(botf::visibleStations(s, 2).size() == 1, "Owner cannot see its own station");
    }, passed);

    run("stations: reference campaigns use catalogue hulls and the rival AI builds and upgrades outposts", [] {
        auto s = campaign(botf::RulesetPreset::Classic, 1999, 5, true);
        // Keep this long AI run on its pre-roster fixture: the idle player otherwise falls before a rival
        // starbase appears. Roster minors are covered by ContentTests (MINOR-RACES.md).
        std::erase_if(s.minors, [](const botf::Minor& m) { return m.name != "Bynars"; });
        // Schema 15 empire rules speed the idle player's fall (AI conquest, dilithium); this stations fixture stays on the
        // pre-schema-15 rules so a rival starbase can appear. The empire rules have their own suite (EmpireRulesTests).
        s.rules.empireRules = false;
        s.minorGuards.clear();
        for (auto& c : s.colonies) { c.energy -= 5 * c.refineries; c.refineries = 0; c.program = 0; c.programTurns = 0; }
        std::erase_if(s.diplomacyEngineState.minorStates, [&](const auto& ms) { return !botf::minorAt(s, ms.minorId); });
        botf::validate(s);
        const auto stats = botf::stationStats(s, 1, botf::StationType::Outpost);
        check(stats.maxHull == 127 && stats.maxShields == 330 && stats.upkeep == 12, "Federation outpost not from the catalogue");
        const auto starbase = botf::stationStats(s, 1, botf::StationType::Starbase);
        check(starbase.maxHull > stats.maxHull && starbase.repairPercent > 0 && starbase.supplyRadius > stats.supplyRadius,
            "Starbase is not stronger than an outpost");
        s.aiAggressionEnabled = true;
        bool rivalStation = false, rivalStarbase = false;
        for (int turn = 0; turn < 200 && s.outcome == botf::CampaignOutcome::Ongoing; ++turn) {
            botf::advance(s);
            for (const auto& st : s.stations) {
                rivalStation |= st.ownerId != 1;
                rivalStarbase |= st.ownerId != 1 && st.type == botf::StationType::Starbase;
            }
        }
        check(rivalStation && rivalStarbase, "Rival AI never built and upgraded an outpost (turn " + std::to_string(s.turn) + ", outcome " + std::to_string(static_cast<int>(s.outcome)) + ", station " + std::to_string(rivalStation) + ")");
    }, passed);
}

void featureScenarios(int& passed) {
    run("anomalies: black holes are impassable and routes bend around them", [] {
        auto s = cleanRemastered();
        const auto sol = home(s);
        const auto hole = emptyCellAt(s, sol.x, sol.y, 2, [&](int x, int y) {
            return x > 0 && y > 0 && x < s.width - 1 && y < s.height - 1 && x != sol.x && y != sol.y;
        });
        s.galaxyFeatures.push_back({s.nextFeatureId++, botf::GalaxyFeatureType::BlackHole, hole.first, hole.second});
        botf::validate(s);
        const int scout = addFleet(s, 1, botf::Building::Scout, sol.x, sol.y);
        check(!botf::sectorPassable(s, hole.first, hole.second), "Black hole passable");
        check(botf::route(s, *fleetById(s, scout), hole.first, hole.second).empty(), "Route into a black hole");
        botf::Command move; move.id = nextId(s); move.type = botf::CommandType::MoveFleet; move.fleetId = scout;
        move.targetX = hole.first; move.targetY = hole.second;
        rejects([&] { botf::submit(s, move); }, "Order into a black hole accepted");
        const int beyondX = hole.first + (hole.first - sol.x > 0 ? 1 : hole.first - sol.x < 0 ? -1 : 0);
        const int beyondY = hole.second + (hole.second - sol.y > 0 ? 1 : hole.second - sol.y < 0 ? -1 : 0);
        const auto path = botf::route(s, *fleetById(s, scout), beyondX, beyondY);
        check(!path.empty(), "No route past the black hole");
        for (int cell : path) check(cell != hole.second * s.width + hole.first, "Route crosses the black hole");
    }, passed);

    run("anomalies: a known wormhole links its endpoints for routing and supply", [] {
        auto s = cleanRemastered(1999, 2);
        const auto sol = home(s);
        const auto entry = emptyCellAt(s, sol.x, sol.y, 2);
        std::pair<int, int> exit{-1, -1};
        int farthest = 0;
        for (int y = 0; y < s.height; ++y) for (int x = 0; x < s.width; ++x) {
            const int d = std::max(std::abs(x - sol.x), std::abs(y - sol.y));
            if (d > farthest && !systemAt(s, x, y)) { farthest = d; exit = {x, y}; }
        }
        check(farthest >= 6, "Map too small for the wormhole fixture");
        const int a = s.nextFeatureId++, b = s.nextFeatureId++;
        s.galaxyFeatures.push_back({a, botf::GalaxyFeatureType::Wormhole, entry.first, entry.second, b});
        s.galaxyFeatures.push_back({b, botf::GalaxyFeatureType::Wormhole, exit.first, exit.second, a});
        botf::validate(s);
        const int transport = addFleet(s, 1, botf::Building::Transport, sol.x, sol.y);
        auto unknown = s;
        auto& explored = unknown.explored;
        std::erase(explored, entry.second * s.width + entry.first);
        check(!botf::supplied(unknown, *fleetById(unknown, transport), exit.first, exit.second), "Unknown wormhole supplies");
        reveal(s, 1, entry.first, entry.second);
        check(botf::supplied(s, *fleetById(s, transport), exit.first, exit.second), "Known wormhole exit not supplied");
        const auto path = botf::route(s, *fleetById(s, transport), exit.first, exit.second);
        check(!path.empty() && path.back() == exit.second * s.width + exit.first, "No route through the wormhole");
        const auto link = std::adjacent_find(path.begin(), path.end(), [&](int from, int to) {
            return std::max(std::abs(from % s.width - to % s.width), std::abs(from / s.width - to / s.width)) > 1;
        });
        check(link != path.end() && *link == entry.second * s.width + entry.first, "Route does not jump through the wormhole");
        check(static_cast<int>(path.size()) <= 4, "Wormhole route is not a shortcut");
    }, passed);

    run("anomalies: nebulae hide ships and drain shields; storms and neutron stars damage hulls but never destroy", [] {
        auto s = cleanRemastered(1999, 2, true);
        const auto sol = home(s);
        const auto cloud = emptyCellAt(s, sol.x, sol.y, 3);
        auto nebula = botf::GalaxyFeature{s.nextFeatureId++, botf::GalaxyFeatureType::Nebula, cloud.first, cloud.second};
        nebula.clusterId = 1;
        s.galaxyFeatures.push_back(nebula);
        auto& rival = *std::find_if(s.fleets.begin(), s.fleets.end(), [](const botf::Fleet& f) { return f.ownerId == 2; });
        rival.x = rival.targetX = cloud.first; rival.y = rival.targetY = cloud.second;
        reveal(s, 2, cloud.first, cloud.second);
        auto& scout = *std::find_if(s.fleets.begin(), s.fleets.end(), [](const botf::Fleet& f) {
            return f.ownerId == 1 && f.shipClass == botf::Building::Scout; });
        const auto beside = emptyCellAt(s, cloud.first, cloud.second, 1);
        scout.x = scout.targetX = beside.first; scout.y = scout.targetY = beside.second;
        reveal(s, 1, beside.first, beside.second);
        botf::validate(s);
        check(!botf::visible(s, 1, cloud.first, cloud.second), "Nebula interior visible from outside");
        scout.x = scout.targetX = cloud.first; scout.y = scout.targetY = cloud.second;
        check(botf::visible(s, 1, cloud.first, cloud.second), "Nebula interior invisible from inside");
        check(!botf::visible(s, 1, beside.first, beside.second), "Ships inside a nebula still see out");
        const int scoutId = scout.id;
        botf::advance(s);
        check(fleetById(s, scoutId)->shields == 0, "Nebula did not drain shields");

        auto hazard = cleanRemastered(1999, 2, true);
        const auto home2 = home(hazard);
        const auto stormCell = emptyCellAt(hazard, home2.x, home2.y, 3, [&](int x, int y) { return x > 0 && y > 0 && x < hazard.width - 1 && y < hazard.height - 1; });
        auto storm = botf::GalaxyFeature{hazard.nextFeatureId++, botf::GalaxyFeatureType::IonStorm, stormCell.first, stormCell.second};
        storm.driftX = 1;
        hazard.galaxyFeatures.push_back(storm);
        const auto star = emptyCellAt(hazard, home2.x, home2.y, 2, [&](int x, int y) {
            return std::max(std::abs(x - stormCell.first), std::abs(y - stormCell.second)) > 2; });
        hazard.galaxyFeatures.push_back({hazard.nextFeatureId++, botf::GalaxyFeatureType::NeutronStar, star.first, star.second});
        auto& inStorm = *std::find_if(hazard.fleets.begin(), hazard.fleets.end(), [](const botf::Fleet& f) {
            return f.ownerId == 1 && f.shipClass == botf::Building::Scout; });
        inStorm.x = inStorm.targetX = stormCell.first; inStorm.y = inStorm.targetY = stormCell.second;
        reveal(hazard, 1, stormCell.first, stormCell.second);
        const int stormShip = inStorm.id, stormHull = inStorm.hull;
        auto& onStar = *std::find_if(hazard.fleets.begin(), hazard.fleets.end(), [](const botf::Fleet& f) {
            return f.ownerId == 1 && f.shipClass == botf::Building::ColonyShip; });
        onStar.x = onStar.targetX = star.first; onStar.y = onStar.targetY = star.second;
        reveal(hazard, 1, star.first, star.second);
        const int starShip = onStar.id, starHull = onStar.hull;
        check(botf::sectorHazardous(hazard, stormCell.first, stormCell.second) && botf::sectorHazardous(hazard, star.first, star.second),
            "Hazards not reported");
        botf::advance(hazard);
        check(fleetById(hazard, stormShip)->hull < stormHull && fleetById(hazard, stormShip)->shields == 0, "Ion storm had no effect");
        check(fleetById(hazard, starShip)->hull < starHull, "Neutron star had no effect");
        for (int i = 0; i < 40; ++i) botf::advance(hazard);
        if (auto* f = fleetById(hazard, starShip)) check(f->hull >= 1, "Hazard destroyed a ship");
    }, passed);

    run("anomalies: ion storms drift every two turns and bounce off the galaxy edge", [] {
        auto s = cleanRemastered();
        auto storm = botf::GalaxyFeature{s.nextFeatureId++, botf::GalaxyFeatureType::IonStorm, s.width - 2, 1};
        storm.driftX = 1; storm.driftY = 0;
        s.galaxyFeatures.push_back(storm);
        std::vector<int> xs;
        for (int i = 0; i < 6; ++i) { botf::advance(s); xs.push_back(s.galaxyFeatures.back().x); }
        // Turn 2 reaches the edge, turn 4 bounces back.
        check(xs[1] == s.width - 1 && xs[3] == s.width - 2 && s.galaxyFeatures.back().driftX == -1,
            "Unexpected storm track");
        check(xs[0] == xs[1] - 1 || xs[0] == s.width - 2, "Storm moved on an odd turn");
    }, passed);

    run("anomalies: a scout survey pays once per empire, reveals a wormhole's exit and keeps the research ledger", [] {
        auto s = cleanRemastered(1999, 2);
        const auto sol = home(s);
        const auto entry = emptyCellAt(s, sol.x, sol.y, 2);
        std::pair<int, int> exit{-1, -1};
        int farthest = 0;
        for (int y = 0; y < s.height; ++y) for (int x = 0; x < s.width; ++x) {
            const int d = std::max(std::abs(x - sol.x), std::abs(y - sol.y));
            if (d > farthest && !systemAt(s, x, y)) { farthest = d; exit = {x, y}; }
        }
        const int a = s.nextFeatureId++, b = s.nextFeatureId++;
        s.galaxyFeatures.push_back({a, botf::GalaxyFeatureType::Wormhole, entry.first, entry.second, b});
        s.galaxyFeatures.push_back({b, botf::GalaxyFeatureType::Wormhole, exit.first, exit.second, a});
        const int scout = addFleet(s, 1, botf::Building::Scout, entry.first, entry.second);
        (void)scout;
        const auto researchBefore = s.research;
        const auto intelBefore = s.intelligence;
        const auto report = botf::advance(s);
        check(s.research > researchBefore + report.researchGained, "Survey research not granted");
        check(s.intelligence > intelBefore, "Survey intelligence not granted");
        check((s.galaxyFeatures[0].surveyedMask & 2) && (s.galaxyFeatures[1].surveyedMask & 2), "Survey marks missing");
        check(botf::explored(s, exit.first, exit.second), "Wormhole exit not charted");
        check(std::any_of(report.chronicle.begin(), report.chronicle.end(), [](const botf::ChronicleEntry& e) {
            return e.category == botf::ChronicleCategory::Exploration && contains(e.text, "surveyed the Wormhole"); }),
            "Survey not in the chronicle");
        const auto second = botf::advance(s);
        check(std::none_of(second.chronicle.begin(), second.chronicle.end(), [](const botf::ChronicleEntry& e) {
            return contains(e.text, "surveyed"); }), "Survey paid twice");
        roundTrip(s);
    }, passed);

    run("anomalies: tampered feature records are rejected", [] {
        auto s = campaign(botf::RulesetPreset::Remastered);
        botf::validate(s);
        rejects([&] { auto bad = s; bad.galaxyFeatures.front().surveyedMask = 1; botf::validate(bad); }, "Bit zero survey mask");
        rejects([&] {
            auto bad = s;
            auto hole = std::find_if(bad.galaxyFeatures.begin(), bad.galaxyFeatures.end(), [](const botf::GalaxyFeature& f) {
                return f.type == botf::GalaxyFeatureType::BlackHole; });
            hole->x = bad.systems.front().x; hole->y = bad.systems.front().y;
            botf::validate(bad);
        }, "Black hole on a star system");
        rejects([&] {
            auto bad = s;
            auto worm = std::find_if(bad.galaxyFeatures.begin(), bad.galaxyFeatures.end(), [](const botf::GalaxyFeature& f) {
                return f.type == botf::GalaxyFeatureType::Wormhole; });
            worm->pairId = worm->id;
            botf::validate(bad);
        }, "Self-paired wormhole");
        auto text = botf::serialize(s);
        const auto features = text.find("\nFEATURES ");
        auto broken = text;
        broken.replace(broken.find('\n', features + 1) + 1, 0, "");
        const auto line = broken.find('\n', features + 1) + 1;
        const auto firstSpace = broken.find(' ', line);
        broken.replace(firstSpace + 1, 1, "9");
        rejects([&] { botf::deserialize(withChecksum(broken)); }, "Unknown feature type loaded");
    }, passed);
}

void eventScenarios(int& passed) {
    run("events: the seeded deck deals events on schedule; Classic keeps the original random events", [] {
        auto s = campaign(botf::RulesetPreset::Remastered, 26713, 5);
        s.eventsEnabled = true;
        auto twin = s;
        int created = 0;
        for (int turn = 0; turn < 60; ++turn) {
            botf::advance(s);
            botf::advance(twin);
        }
        created = s.nextEventId - 1;
        check(created >= 6, "Too few curated events in 60 turns: " + std::to_string(created));
        check(botf::fingerprint(s) == botf::fingerprint(twin), "Event deck is not deterministic");
        check(s.lastRandomEventTurn == 0, "Original random events ran alongside the deck");
        auto classic = campaign(botf::RulesetPreset::Classic, 26713, 5);
        classic.eventsEnabled = true;
        for (int turn = 0; turn < 30; ++turn) botf::advance(classic);
        check(classic.lastRandomEventTurn > 0 && classic.galacticEvents.empty() && classic.nextEventId == 1,
            "Classic events changed");
    }, passed);

    run("events: the player decides by command, invalid decisions are refused, defaults apply after the window", [] {
        auto s = cleanRemastered();
        const auto sol = home(s);
        botf::GalacticEvent plague;
        plague.id = s.nextEventId++;
        plague.kind = botf::GalacticEventKind::Plague;
        plague.empireId = 1; plague.systemId = sol.id; plague.x = sol.x; plague.y = sol.y;
        plague.startTurn = 0; plague.endTurn = 4; plague.choiceDeadline = 2; plague.magnitude = 6;
        s.galacticEvents.push_back(plague);
        auto defaulted = s;
        botf::validate(s);
        check(botf::empireEvents(s, 1).size() == 1 && botf::eventChoices(plague.kind).size() == 3, "Event not listed");
        rejects([&] { botf::submit(s, botf::makeResolveEventChoiceCommand(nextId(s), 99, 0)); }, "Unknown event accepted");
        rejects([&] { botf::submit(s, botf::makeResolveEventChoiceCommand(nextId(s), plague.id, 3)); }, "Invalid option accepted");
        s.credits = 1000;
        auto quarantine = s;
        botf::submit(quarantine, botf::makeResolveEventChoiceCommand(nextId(quarantine), plague.id, 0));
        botf::submit(s, botf::makeResolveEventChoiceCommand(nextId(s), plague.id, 1));
        roundTrip(s);
        rejects([&] { botf::submit(s, botf::makeResolveEventChoiceCommand(nextId(s), plague.id, 0)); }, "Second decision accepted");
        const auto report = botf::advance(s);
        botf::advance(quarantine);
        check(s.galacticEvents.empty(), "Cure did not end the plague");
        check(quarantine.credits - s.credits == 60, "Cure did not cost 60 credits (prototype rules)");
        check(quarantine.galacticEvents.size() == 1 && quarantine.galacticEvents.front().endTurn == quarantine.turn + 1,
            "Quarantine did not shorten the plague");
        check(std::any_of(report.chronicle.begin(), report.chronicle.end(), [](const botf::ChronicleEntry& e) {
            return e.category == botf::ChronicleCategory::Events && contains(e.text, "cure"); }), "Cure not reported");

        const auto population = std::find_if(defaulted.colonies.begin(), defaulted.colonies.end(), [&](const botf::Colony& c) { return c.systemId == sol.id; })->population;
        bool defaultNoted = false;
        for (int turn = 0; turn < 5; ++turn) {
            const auto r = botf::advance(defaulted);
            defaultNoted |= std::any_of(r.events.begin(), r.events.end(), [](const std::string& e) { return contains(e, "default applied"); });
        }
        check(defaultNoted, "Default decision not reported");
        check(defaulted.galacticEvents.empty(), "Plague did not end");
        const auto after = std::find_if(defaulted.colonies.begin(), defaulted.colonies.end(), [&](const botf::Colony& c) { return c.systemId == sol.id; })->population;
        check(after < population + 5, "Plague killed nobody");
    }, passed);

    run("events: supernova evacuation moves colonists and the detonation strikes the system", [] {
        auto s = cleanRemastered();
        const auto sol = home(s);
        // A second player colony to receive evacuees.
        const auto otherIt = std::find_if(s.systems.begin(), s.systems.end(), [&](const botf::System& x) {
            return !botf::colonyAt(s, x.id) && !botf::minorAt(s, x.id); });
        const auto other = &*otherIt;
        const botf::System otherCopy = *other;
        botf::Colony outpost; outpost.systemId = otherCopy.id; outpost.ownerId = 1; outpost.population = 30;
        outpost.foodWorkers = 15; outpost.industryWorkers = 10; outpost.researchWorkers = 5;
        s.colonies.push_back(outpost);
        std::sort(s.colonies.begin(), s.colonies.end(), [](const botf::Colony& a, const botf::Colony& b) { return a.systemId < b.systemId; });
        reveal(s, 1, otherCopy.x, otherCopy.y);
        botf::GalacticEvent nova;
        nova.id = s.nextEventId++;
        nova.kind = botf::GalacticEventKind::SupernovaWarning;
        nova.empireId = 1; nova.systemId = otherCopy.id; nova.x = otherCopy.x; nova.y = otherCopy.y;
        nova.startTurn = 0; nova.endTurn = 2; nova.choiceDeadline = 2; nova.magnitude = 75;
        s.galacticEvents.push_back(nova);
        botf::validate(s);
        botf::submit(s, botf::makeResolveEventChoiceCommand(nextId(s), nova.id, 0));
        const auto homeBefore = botf::colonyAt(s, sol.id)->population;
        botf::advance(s);
        check(botf::colonyAt(s, otherCopy.id)->population <= 16, "Evacuation left the colonists behind");
        check(botf::colonyAt(s, sol.id)->population > homeBefore, "Evacuees did not arrive");
        const auto beforeBlast = botf::colonyAt(s, otherCopy.id)->population;
        botf::advance(s);
        check(botf::colonyAt(s, otherCopy.id)->population <= std::max(1, beforeBlast - beforeBlast * 75 / 100) + 1, "Detonation spared the colony");
        check(s.galacticEvents.empty(), "Supernova event lingered");
    }, passed);

    run("events: rival AIs decide immediately and deterministically", [] {
        auto s = campaign(botf::RulesetPreset::Remastered, 4242, 5);
        s.eventsEnabled = true;
        bool rivalDecided = false;
        for (int turn = 0; turn < 120 && !rivalDecided; ++turn) {
            botf::advance(s);
            for (const auto& e : s.galacticEvents)
                if (e.empireId != 0 && e.empireId != s.playerEmpire && e.choiceDeadline > 0) {
                    check(e.choice >= 0, "Rival left an event undecided");
                    rivalDecided = true;
                }
        }
        check(rivalDecided, "No rival decision event in 120 turns");
    }, passed);
}

void victoryScenarios(int& passed) {
    run("victory: the megaproject needs the research threshold, completes for a scientific victory", [] {
        auto s = cleanRemastered();
        const auto sol = home(s);
        check(contains(botf::megaprojectBlocker(s, 1, sol.id), "turn 50"), "Megaproject open too early");
        s.turn = 60;
        s.diplomacyEngineState.currentTurn = s.turn;
        const int threshold = botf::scientificVictoryThreshold(s);
        check(contains(botf::megaprojectBlocker(s, 1, sol.id), "research field"), "Research threshold not required");
        for (int field = 0; field < botf::ResearchFields; ++field) raiseTech(s, 1, field, threshold);
        s.credits = 100000;
        check(botf::megaprojectBlocker(s, 1, sol.id).empty(), "Megaproject blocked: " + botf::megaprojectBlocker(s, 1, sol.id));
        botf::submit(s, botf::makeStartMegaprojectCommand(nextId(s), sol.id));
        botf::advance(s);
        const auto* project = botf::empireMegaproject(s, 1);
        check(project && project->work > 0 && project->work <= botf::megaprojectWorkPerTurn(s, 1), "Industry not diverted within the cap");
        roundTrip(s);
        const auto progress = botf::victoryProgress(s, 1);
        check(progress.size() == 4 && progress[2].condition == botf::VictoryCondition::Scientific && progress[2].percent >= 70,
            "Scientific progress not reported");
        s.megaprojects.front().work = botf::megaprojectIndustryCost(s) - 1;
        const auto report = botf::advance(s);
        check(s.outcome == botf::CampaignOutcome::Victory && s.winnerEmpire == 1 && report.outcome == botf::CampaignOutcome::Victory,
            "Scientific victory not awarded");
        check(std::any_of(report.chronicle.begin(), report.chronicle.end(), [](const botf::ChronicleEntry& e) {
            return e.category == botf::ChronicleCategory::Victory && contains(e.text, "scientific victory"); }), "Victory not announced");
        roundTrip(s);
    }, passed);

    run("victory: a rival's megaproject defeats the player; Classic disables alternative victories", [] {
        auto s = cleanRemastered(1999, 3);
        s.turn = 60;
        s.diplomacyEngineState.currentTurn = s.turn;
        const auto rivalColony = std::find_if(s.colonies.begin(), s.colonies.end(), [](const botf::Colony& c) { return c.ownerId == 3; });
        s.megaprojects.push_back({3, rivalColony->systemId, botf::megaprojectIndustryCost(s) - 1, 55});
        botf::validate(s);
        botf::advance(s);
        check(s.outcome == botf::CampaignOutcome::Defeat && s.winnerEmpire == 3, "Rival scientific victory not applied");
        auto classic = campaign(botf::RulesetPreset::Classic);
        check(contains(botf::megaprojectBlocker(classic, 1, home(classic).id), "disabled"), "Classic allows megaprojects");
        const auto progress = botf::victoryProgress(classic, 1);
        check(!progress[2].enabled && !progress[3].enabled && progress[0].enabled && progress[1].enabled, "Classic victory flags");
        check(!botf::diplomaticVictoryAchieved(classic, 1), "Diplomatic placeholder must return false");
        rejects([&] { botf::submit(classic, botf::makeCancelMegaprojectCommand(nextId(classic))); }, "Cancelling nothing accepted");
    }, passed);
}

void templateScenarios(int& passed) {
    run("templates: define, apply with reasons, redefine, delete and default", [] {
        auto s = campaign(botf::RulesetPreset::Classic);
        const auto sol = home(s);
        rejects([&] { botf::submit(s, botf::makeDefineBuildTemplateCommand(nextId(s), 0, "Forts",
            {botf::Construction(botf::Building::DefenseBattery)})); }, "Prototype template with defense batteries accepted");
        botf::submit(s, botf::makeDefineBuildTemplateCommand(nextId(s), 0, "Core World",
            {botf::Construction(botf::Building::Farm), botf::Construction(botf::Building::Shipyard),
             botf::Construction(botf::Building::Laboratory), botf::Construction(botf::Building::Scout)}));
        botf::advance(s);
        check(s.buildTemplates.size() == 1 && s.buildTemplates.front().name == "Core World", "Template not stored");
        const int id = s.buildTemplates.front().id;
        auto& colony = *std::find_if(s.colonies.begin(), s.colonies.end(), [&](const botf::Colony& c) { return c.systemId == sol.id; });
        const auto queueBefore = colony.queue.size();
        botf::submit(s, botf::makeApplyBuildTemplateCommand(nextId(s), id, sol.id));
        const auto previewed = botf::preview(s);
        const auto& pc = *botf::colonyAt(previewed, sol.id);
        check(pc.queue.size() == queueBefore + 3, "Template did not queue three projects");
        check(contains(pc.queueExplanation, "skipped") && contains(pc.queueExplanation, "Shipyard"), "Skip reason missing");
        roundTrip(s);
        botf::advance(s);
        botf::submit(s, botf::makeDefineBuildTemplateCommand(nextId(s), id, "Farms", {botf::Construction(botf::Building::Farm)}));
        botf::submit(s, botf::makeSetDefaultBuildTemplateCommand(nextId(s), id));
        botf::advance(s);
        check(s.buildTemplates.front().items.size() == 1 && s.buildTemplates.front().autoApply, "Redefine/default failed");
        rejects([&] { botf::submit(s, botf::makeApplyBuildTemplateCommand(nextId(s), 99, sol.id)); }, "Unknown template applied");
        botf::submit(s, botf::makeDeleteBuildTemplateCommand(nextId(s), id));
        botf::advance(s);
        check(s.buildTemplates.empty(), "Template not deleted");
    }, passed);

    run("templates: the default template fills newly founded colonies", [] {
        auto s = campaign(botf::RulesetPreset::Classic);
        botf::submit(s, botf::makeDefineBuildTemplateCommand(nextId(s), 0, "Frontier",
            {botf::Construction(botf::Building::Farm), botf::Construction(botf::Building::Factory)}));
        botf::advance(s);
        botf::submit(s, botf::makeSetDefaultBuildTemplateCommand(nextId(s), s.buildTemplates.front().id));
        botf::advance(s);
        auto& ship = *std::find_if(s.fleets.begin(), s.fleets.end(), [](const botf::Fleet& f) { return f.ownerId == 1 && f.colonyShip; });
        const auto target = std::find_if(s.systems.begin(), s.systems.end(), [&](const botf::System& x) {
            return !botf::colonyAt(s, x.id) && !botf::minorAt(s, x.id) &&
                std::max(std::abs(x.x - ship.x), std::abs(x.y - ship.y)) <= 3; });
        check(target != s.systems.end(), "No nearby colonization target");
        const int targetId = target->id; // submit() replaces the state and its vectors.
        ship.x = ship.targetX = target->x; ship.y = ship.targetY = target->y;
        reveal(s, 1, target->x, target->y);
        botf::Command colonize; colonize.id = nextId(s); colonize.type = botf::CommandType::Colonize; colonize.fleetId = ship.id;
        botf::submit(s, colonize);
        const auto report = botf::advance(s);
        const auto* founded = botf::colonyAt(s, targetId);
        check(founded && founded->queue.size() == 2 && founded->queue.front().product == botf::Building::Farm, "Default template not applied");
        check(std::any_of(report.chronicle.begin(), report.chronicle.end(), [](const botf::ChronicleEntry& e) {
            return e.category == botf::ChronicleCategory::Construction && contains(e.text, "Frontier"); }), "Adoption not reported");
    }, passed);

    run("templates: reference templates need the empire's own named ship classes", [] {
        auto s = campaign(botf::RulesetPreset::Classic, 1999, 2, true);
        rejects([&] { botf::submit(s, botf::makeDefineBuildTemplateCommand(nextId(s), 0, "Scouts",
            {botf::Construction(botf::Building::Scout)})); }, "Unnamed reference ship accepted");
        const auto choices = botf::constructionChoices(s, home(s).id);
        const auto own = std::find_if(choices.begin(), choices.end(), [](const botf::ConstructionChoice& c) {
            return c.product == botf::Building::Scout && c.referenceId >= 0; });
        check(own != choices.end(), "No own scout class");
        botf::submit(s, botf::makeDefineBuildTemplateCommand(nextId(s), 0, "Scouts",
            {botf::Construction(botf::Building::Scout, 0, own->referenceId), botf::Construction(botf::Building::DefenseBattery)}));
        int foreign = -1;
        for (const auto& ship : botf::referenceData::ShipCatalogue)
            if (ship.raceId != 1 && ship.role == 0) { foreign = ship.id; break; }
        rejects([&] { botf::submit(s, botf::makeDefineBuildTemplateCommand(nextId(s), 0, "Stolen",
            {botf::Construction(botf::Building::Scout, 0, foreign)})); }, "Foreign ship class accepted");
        botf::advance(s);
        roundTrip(s);
    }, passed);
}

void persistenceScenarios(int& passed) {
    run("saves: the current schema round-trips a live Remastered campaign and reloads continue identically", [] {
        auto s = campaign(botf::RulesetPreset::Remastered, 26713, 5, true);
        s.eventsEnabled = true; s.aiAggressionEnabled = true;
        for (int turn = 0; turn < 160 && s.outcome == botf::CampaignOutcome::Ongoing; ++turn) botf::advance(s);
        check(!s.stations.empty() && !s.galaxyFeatures.empty(), "Campaign lacks gameplay-layer content");
        const auto text = roundTrip(s);
        check(text.starts_with("BOTF_SIM " + std::to_string(botf::SaveVersion) + " " + botf::ContentVersion + "\n") &&
            contains(text, "\nGAMEPLAY_LAYER 1 1 1 1 1\n"), "Current schema header or gameplay section");
        auto reloaded = botf::deserialize(text);
        // Schema 15 empire rules change how fast a passive player falls; a campaign may be decided before turn 170.
        check(reloaded.outcome == s.outcome && reloaded.turn == s.turn, "Reload lost the outcome or turn: turn " + std::to_string(s.turn) + " outcome " + std::to_string(static_cast<int>(s.outcome)) + " vs " + std::to_string(static_cast<int>(reloaded.outcome)));
        for (int turn = 0; turn < 10 && s.outcome == botf::CampaignOutcome::Ongoing && reloaded.outcome == botf::CampaignOutcome::Ongoing; ++turn) { botf::advance(s); botf::advance(reloaded); }
        check(botf::fingerprint(s) == botf::fingerprint(reloaded), "Reloaded campaign diverged");
    }, passed);

    run("saves: pending gameplay commands survive save and load", [] {
        auto s = campaign(botf::RulesetPreset::Classic);
        const auto sol = home(s);
        const auto site = emptyCellAt(s, sol.x, sol.y, 2);
        const int transport = addFleet(s, 1, botf::Building::Transport, site.first, site.second);
        botf::submit(s, botf::makeBuildOutpostCommand(nextId(s), transport));
        botf::submit(s, botf::makeDefineBuildTemplateCommand(nextId(s), 0, "Quoted \"name\" here",
            {botf::Construction(botf::Building::Farm), botf::Construction(botf::Building::Factory)}));
        const auto text = roundTrip(s);
        const auto loaded = botf::deserialize(text);
        check(loaded.pending.size() == 2 && loaded.pending[1].templateItems.size() == 2 &&
            loaded.pending[1].templateName == "Quoted \"name\" here", "Pending template command lost fields");
        auto a = s, b = loaded;
        botf::advance(a); botf::advance(b);
        check(botf::fingerprint(a) == botf::fingerprint(b), "Pending commands resolved differently after load");
    }, passed);

    run("saves: schema 12 loads as Classic with every layer off and continues exactly", [] {
        auto baseline = botf::generateReferenceCampaign(1999, 3);
        for (int turn = 0; turn < 8; ++turn) botf::advance(baseline);
        const auto old = asSchemaTwelve(baseline);
        auto loaded = botf::deserialize(old);
        check(loaded.rules == botf::GameplayRules{} && loaded.stations.empty() && loaded.galaxyFeatures.empty(), "Schema 12 gained layers");
        check(botf::serialize(loaded) == botf::serialize(baseline), "Schema 12 did not migrate losslessly");
        for (int turn = 0; turn < 20; ++turn) { botf::advance(loaded); botf::advance(baseline); }
        check(botf::fingerprint(loaded) == botf::fingerprint(baseline), "Migrated campaign diverged");
        auto smuggled = botf::serialize(baseline);
        smuggled.replace(0, smuggled.find('\n') + 1, "BOTF_SIM 12 remaster-12\n");
        rejects([&] { botf::deserialize(withChecksum(smuggled)); }, "Schema 12 save with a schema 13 section accepted");
        auto remastered = campaign(botf::RulesetPreset::Remastered);
        auto missing = botf::serialize(remastered);
        missing.erase(missing.find("GAMEPLAY_LAYER"), missing.find("END_GAMEPLAY_LAYER\n") + 19 - missing.find("GAMEPLAY_LAYER"));
        rejects([&] { botf::deserialize(withChecksum(missing)); }, "Schema 13 save without its section accepted");
    }, passed);

    run("saves: the fingerprint covers stations, anomalies, events, templates and megaprojects", [] {
        auto s = campaign(botf::RulesetPreset::Remastered);
        const auto base = botf::fingerprint(s);
        auto changed = s; changed.galaxyFeatures.front().surveyedMask = 2;
        check(botf::fingerprint(changed) != base, "Feature not fingerprinted");
        changed = s; changed.eventDeckPosition = 1;
        check(botf::fingerprint(changed) != base, "Deck not fingerprinted");
        changed = s; addStation(changed, 1, home(s).x, home(s).y);
        check(botf::fingerprint(changed) != base, "Station not fingerprinted");
        changed = s; changed.buildTemplates.push_back({changed.nextBuildTemplateId++, 1, "T", {botf::Construction(botf::Building::Farm)}});
        check(botf::fingerprint(changed) != base, "Template not fingerprinted");
        auto tampered = botf::serialize(s);
        const auto pos = tampered.find("\nSTATIONS ");
        tampered.replace(pos, 11, "\nSTATIONS 0");
        rejects([&] { botf::deserialize(withChecksum(tampered)); }, "Invalid next station id loaded");
    }, passed);
}

void chronicleScenarios(int& passed) {
    run("chronicle: every event line has one categorized entry in order, with sector data for battles", [] {
        auto s = campaign(botf::RulesetPreset::Remastered, 26713, 5, true);
        s.eventsEnabled = true; s.aiAggressionEnabled = true;
        bool sawBattle = false;
        std::set<botf::ChronicleCategory> categories;
        for (int turn = 0; turn < 120 && s.outcome == botf::CampaignOutcome::Ongoing; ++turn) {
            const auto report = botf::advance(s);
            check(report.chronicle.size() == report.events.size(), "Chronicle does not cover every event");
            for (std::size_t i = 0; i < report.chronicle.size(); ++i) {
                const auto& e = report.chronicle[i];
                check(e.eventIndex == static_cast<int>(i) && e.text == report.events[i], "Chronicle out of order");
                categories.insert(e.category);
                if (contains(e.text, "Battle at sector")) {
                    check(e.category == botf::ChronicleCategory::Combat && e.x >= 0 && e.y >= 0, "Battle entry lacks its sector");
                    sawBattle = true;
                }
                if (contains(e.text, "advanced to level")) check(e.category == botf::ChronicleCategory::Research, "Research miscategorized");
                if (contains(e.text, "Discovered ")) check(e.category == botf::ChronicleCategory::Exploration && e.systemId > 0, "Discovery lacks its system");
            }
            check(!botf::describeChronicle(report).empty(), "Briefing text empty");
        }
        check(categories.count(botf::ChronicleCategory::Events) && categories.count(botf::ChronicleCategory::Colonization), "Categories missing");
        (void)sawBattle;
    }, passed);

    run("chronicle: rival internal events and unseen stations stay hidden from the player", [] {
        auto s = campaign(botf::RulesetPreset::Remastered, 4242, 5, true);
        s.eventsEnabled = true; s.aiAggressionEnabled = true;
        for (int turn = 0; turn < 100 && s.outcome == botf::CampaignOutcome::Ongoing; ++turn) {
            const auto report = botf::advance(s);
            for (const auto& e : report.chronicle) {
                if (e.category == botf::ChronicleCategory::Events && e.eventId) {
                    const bool mine = e.empireId == s.playerEmpire || e.empireId == 0;
                    check(mine, "Rival event leaked into the briefing: " + e.text);
                }
                if (e.category == botf::ChronicleCategory::Stations && e.empireId != s.playerEmpire && e.stationId && e.x >= 0 &&
                    contains(e.text, "detected"))
                    check(botf::visible(s, s.playerEmpire, e.x, e.y) || botf::explored(s, e.x, e.y), "Unseen station reported");
            }
        }
    }, passed);
}
} // namespace

int main() {
    int passed = 0;
    rulesetScenarios(passed);
    stationScenarios(passed);
    featureScenarios(passed);
    eventScenarios(passed);
    victoryScenarios(passed);
    templateScenarios(passed);
    persistenceScenarios(passed);
    chronicleScenarios(passed);
    std::cout << (failed ? "FAILED " : "ALL PASSED ") << passed << " passed, " << failed << " failed" << std::endl;
    return failed ? 1 : 0;
}
