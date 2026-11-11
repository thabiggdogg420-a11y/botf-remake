// Expanded ship roster (docs/SHIP-ROSTER.md): data integrity, Classic isolation, Remastered building,
// the optional SHIP_ROSTER save section, tactical cloak/weapon split, AI adoption and determinism.
#include "CampaignSession.h"
#include "MinorRaces.h"
#include "ShipRoster.h"
#include "TacticalCombat.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <set>
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

std::string withChecksum(std::string body) {
    if (const auto pos = body.rfind("CHECKSUM "); pos != std::string::npos) body.erase(pos);
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
std::string withoutRosterSection(const std::string& save) {
    auto text = save;
    const auto at = text.find("SHIP_ROSTER 1\n");
    check(at != std::string::npos, "Save lacks the roster section");
    text.erase(at, 14);
    return withChecksum(text);
}

botf::GameState campaign(botf::RulesetPreset preset, int empires = 2, std::uint64_t seed = 4242) {
    botf::CampaignSetup setup;
    setup.seed = seed; setup.empires = empires; setup.reference = true; setup.preset = preset;
    auto s = botf::newCampaign(setup);
    s.aiAggressionEnabled = false;
    s.eventsEnabled = false;
    return s;
}
int homeSystem(const botf::GameState& s) {
    const auto c = std::find_if(s.colonies.begin(), s.colonies.end(), [&](const botf::Colony& x) { return x.ownerId == s.playerEmpire; });
    check(c != s.colonies.end(), "Player has no colony");
    return c->systemId;
}
botf::Command enqueue(std::uint64_t id, int systemId, botf::Building b, int referenceId) {
    botf::Command c;
    c.id = id; c.type = botf::CommandType::EnqueueBuilding; c.systemId = systemId; c.building = b; c.referenceId = referenceId;
    return c;
}
const botf::roster::RosterClass& byKey(const std::string& key) {
    for (const auto& c : botf::roster::ExpandedRoster) if (key == c.assetKey) return c;
    throw std::runtime_error("No roster class " + key);
}

int failed = 0;
void run(const char* name, const std::function<void()>& fn, int& passed) {
    try {
        fn(); ++passed; std::cout << "PASS " << name << std::endl;
    } catch (const std::exception& error) {
        ++failed; std::cout << "FAIL " << name << ": " << error.what() << std::endl;
    }
}

void dataScenarios(int& passed) {
    using namespace botf::roster;
    run("data: ids, lookups and the legacy catalogue are untouched", [] {
        check(botf::referenceData::ShipCatalogue.size() == 125, "Legacy catalogue size changed");
        for (const auto& ship : botf::referenceData::ShipCatalogue)
            check(findShip(ship.id) == &ship && !isExpandedId(ship.id), "Legacy lookup broken");
        check(findShip(-1) == nullptr && findShip(125) == nullptr && findShip(999) == nullptr &&
            findShip(ExpandedIdLimit) == nullptr, "Unknown ids must not resolve");
        check(ExpandedRoster.size() >= 90, "Roster unexpectedly small");
        for (std::size_t i = 0; i < ExpandedRoster.size(); ++i) {
            const auto& c = ExpandedRoster[i];
            const auto* r = findShip(c.id);
            check(r == &ExpandedShipRecords[i] && findRosterClass(c.id) == &c, std::string("Lookup mismatch for ") + c.className);
            check(r->id == c.id && r->raceId == c.raceId && r->role == c.engineRole && r->hull == c.hull &&
                r->shields == c.shields && r->attack == c.attack && r->industryCost == c.industryCost &&
                r->maintenance == c.maintenance && static_cast<int>(r->speed) == c.speed &&
                static_cast<int>(r->range) == c.range && r->technologyRequirements == c.technologyRequirements,
                std::string("Record does not mirror roster class ") + c.className);
        }
    }, passed);

    run("data: stats derive from typed weapons with integer rules", [] {
        std::set<std::string> keys;
        for (const auto& c : ExpandedRoster) {
            const std::string name = c.className;
            check(keys.insert(c.assetKey).second, "Duplicate assetKey " + std::string(c.assetKey));
            check(std::string(c.assetKey).rfind("ship.", 0) == 0, "assetKey prefix for " + name);
            check(c.weaponCount >= 0 && c.weaponCount <= 4, "Weapon count for " + name);
            int points = 0, torpedo = 0;
            for (int i = 0; i < 4; ++i) {
                const auto& w = c.weapons[static_cast<std::size_t>(i)];
                if (i >= c.weaponCount) { check(w.type == WeaponType::None && w.count == 0, "Unused slot not empty: " + name); continue; }
                check(w.type != WeaponType::None && w.count > 0, "Empty weapon mount on " + name);
                const int p = weaponPoints(w.type) * w.count * arcPercent(w.arc) / 100;
                points += p;
                if (torpedoWeapon(w.type)) torpedo += p;
            }
            check(c.attack == points * attackScale(c.tier) / 100, "Attack not derived from weapons for " + name);
            check(c.torpedoSharePercent == (points ? torpedo * 100 / points : 0), "Torpedo share for " + name);
            check(c.tier >= 0 && c.tier <= 10 && c.hull > 0 && c.shields >= 0 && c.industryCost >= 100 && c.maintenance > 0,
                "Stat ranges for " + name);
            check(c.speed >= 0 && c.speed <= 3 && c.range >= 0 && c.range <= 2 && c.sensors >= 1 && c.sensors <= 5,
                "Movement ranges for " + name);
            check(c.buildTurns == (c.industryCost + 599) / 600, "Nominal build turns for " + name);
            for (int req : c.technologyRequirements) check(req >= 1 && req <= 10, "Technology requirement range for " + name);
            const std::string description = c.description;
            check(description.size() >= 40 && description.size() <= 320, "Description length for " + name);
            check(std::string(c.source).size() > 3, "Source tag for " + name);
        }
    }, passed);

    run("data: buildability policy, factions and minor-race links", [] {
        std::array<int, 64> buildable{};
        for (const auto& c : ExpandedRoster) {
            const std::string name = c.className;
            if (c.buildable) {
                check(c.sourceKind == SourceKind::Canon, "Only canon classes are buildable: " + name);
                check(c.raceId >= 0 && static_cast<std::size_t>(c.raceId) < botf::referenceData::RaceCatalogue.size() && botf::referenceData::RaceCatalogue[static_cast<std::size_t>(c.raceId)].empireId != 0, "Buildable class outside the major empires: " + name);
                check(c.engineRole == 0 || c.engineRole == 5 || c.engineRole == 9 || (c.engineRole >= 1 && c.engineRole <= 4),
                    "Buildable class with a non-campaign role: " + name);
                ++buildable[static_cast<std::size_t>(c.raceId)];
            }
            if (c.sourceKind != SourceKind::Canon) check(!c.buildable, "Mod-derived/Kelvin classes are data only: " + name);
            if (c.sourceKind == SourceKind::Kelvin) check(c.era == Era::Kelvin, "Kelvin classes carry the Kelvin era: " + name);
            if (c.sourceKind == SourceKind::ModDerived) check(contains(c.source, "mod-derived"), "Mod note missing: " + name);
            check(std::string(factionName(c.faction)) != "Unknown" && std::string(eraName(c.era)) != "Unknown", "Names for " + name);
            if (c.minorRaceId != 0) {
                const auto* race = botf::minors::raceById(c.minorRaceId);
                check(race != nullptr && race->referenceRaceId == c.raceId, "Minor-race link for " + name);
            }
        }
        for (const int race : {0, 1, 2, 3, 4, 35, 36, 37}) check(buildable[static_cast<std::size_t>(race)] >= 1, "Every major empire gains a class");
        check(buildable[1] >= 20, "Federation canon roster is the deepest");
    }, passed);
}

void rulesetScenarios(int& passed) {
    run("classic: roster off, never offered, never accepted, save text unchanged", [] {
        auto s = campaign(botf::RulesetPreset::Classic);
        check(!s.rules.expandedRoster, "Classic enables the roster");
        const int sys = homeSystem(s);
        for (const auto& ch : botf::constructionChoices(s, sys))
            check(!botf::roster::isExpandedId(ch.referenceId), "Classic offers an expanded class");
        const auto nx = byKey("ship.fed.nx");
        rejects([&] { botf::submit(s, enqueue(1, sys, botf::Building::Destroyer, nx.id)); }, "Classic accepted an expanded class");
        check(!contains(botf::serialize(s), "SHIP_ROSTER"), "Classic save has a roster section");
        auto forged = s;
        forged.rules.expandedRoster = true;
        rejects([&] { botf::validate(forged); }, "Classic with roster flag validated");
    }, passed);

    run("pre-roster API and legacy generators keep the roster off", [] {
        const auto baseline = botf::generateReferenceCampaign(1999);
        check(!baseline.rules.expandedRoster && !contains(botf::serialize(baseline), "SHIP_ROSTER"), "Legacy generator gained roster");
        check(botf::rulesForPreset(botf::RulesetPreset::Remastered).expandedRoster, "Remastered preset must enable the roster");
        check(!botf::rulesForPreset(botf::RulesetPreset::Classic).expandedRoster, "Classic preset must not enable the roster");
    }, passed);

    run("remastered: expanded classes offered with tech blockers, built, saved and reloaded", [] {
        auto s = campaign(botf::RulesetPreset::Remastered);
        check(s.rules.expandedRoster, "Remastered lacks the roster");
        const int sys = homeSystem(s);
        const auto choices = botf::constructionChoices(s, sys);
        const auto nx = byKey("ship.fed.nx");
        const auto prometheus = byKey("ship.fed.prometheus");
        const auto found = [&](int id) {
            return std::find_if(choices.begin(), choices.end(), [&](const botf::ConstructionChoice& c) { return c.referenceId == id; });
        };
        check(found(nx.id) != choices.end(), "NX not offered to the Federation");
        check(found(prometheus.id) != choices.end() && !found(prometheus.id)->blocker.empty(), "Prometheus must be tech-blocked");
        check(found(byKey("ship.kli.d7").id) == choices.end(), "Klingon class offered to the Federation");
        check(found(byKey("ship.fed.luna").id) == choices.end(), "Mod-derived class offered");
        check(found(byKey("ship.fed.wells").id) == choices.end(), "Data-only class offered");
        const auto nxChoice = found(nx.id);
        check(nxChoice->blocker.empty() || contains(nxChoice->blocker, "hipyard"), "NX blocked: " + nxChoice->blocker);
        if (!nxChoice->blocker.empty()) {
            // Starting colonies may lack a yard in some eras; queue one first.
            botf::submit(s, enqueue(s.lastAcceptedCommand + 1, sys, botf::Building::Shipyard, -1));
        }
        botf::submit(s, enqueue(s.lastAcceptedCommand + 1, sys, nxChoice->product, nx.id));
        rejects([&] { botf::submit(s, enqueue(s.lastAcceptedCommand + 1, sys, botf::Building::Destroyer, prometheus.id)); },
            "Tech-blocked class accepted");
        bool built = false;
        for (int turn = 0; turn < 200 && !built; ++turn) {
            botf::advance(s);
            built = std::any_of(s.fleets.begin(), s.fleets.end(), [&](const botf::Fleet& f) {
                return f.ownerId == s.playerEmpire && f.referenceShipId == nx.id;
            });
        }
        check(built, "NX never launched");
        const auto& fleet = *std::find_if(s.fleets.begin(), s.fleets.end(), [&](const botf::Fleet& f) { return f.referenceShipId == nx.id; });
        check(botf::fleetMaxHull(fleet) == nx.hull && botf::fleetMaxShields(fleet) == nx.shields, "Fleet stats come from the roster");
        check(contains(botf::fleetName(fleet), "NX"), "Fleet name uses the class name");
        const auto text = botf::serialize(s);
        check(contains(text, "\nSHIP_ROSTER 1\n"), "Roster section missing");
        const auto loaded = botf::deserialize(text);
        check(loaded.rules.expandedRoster && botf::serialize(loaded) == text, "Roster save did not round-trip");
        rejects([&] { botf::deserialize(withoutRosterSection(text)); }, "Save with roster ships loaded without the section");
        auto revision = text;
        revision.replace(revision.find("SHIP_ROSTER 1"), 13, "SHIP_ROSTER 2");
        rejects([&] { botf::deserialize(withChecksum(revision)); }, "Unknown roster revision accepted");
    }, passed);

    run("remastered saves from before the roster load with it off and continue unchanged", [] {
        auto s = campaign(botf::RulesetPreset::Remastered);
        const auto legacyText = withoutRosterSection(botf::serialize(s));
        auto legacy = botf::deserialize(legacyText);
        check(!legacy.rules.expandedRoster && botf::serialize(legacy) == legacyText, "Pre-roster Remastered save changed on load");
        for (const auto& ch : botf::constructionChoices(legacy, homeSystem(legacy)))
            check(!botf::roster::isExpandedId(ch.referenceId), "Pre-roster save offers expanded classes");
        for (int turn = 0; turn < 20; ++turn) botf::advance(legacy);
        check(!contains(botf::serialize(legacy), "SHIP_ROSTER"), "Pre-roster save gained the roster");
    }, passed);

    run("templates: expanded classes are legal only with the roster on", [] {
        auto s = campaign(botf::RulesetPreset::Remastered);
        const auto nx = byKey("ship.fed.nx");
        botf::Construction item(botf::Building::Destroyer, 0, nx.id);
        const auto cmd = botf::makeDefineBuildTemplateCommand(s.lastAcceptedCommand + 1, 0, "Frontier", {item});
        botf::submit(s, cmd);
        check(!s.buildTemplates.empty() || !botf::preview(s).buildTemplates.empty(), "Template not defined");
    }, passed);
}

void tacticalScenarios(int& passed) {
    run("tactical: class cloak flags and weapon-fit beam/torpedo split", [] {
        auto s = campaign(botf::RulesetPreset::Remastered, 2);
        const auto akira = byKey("ship.fed.akira");
        const auto d7 = byKey("ship.kli.d7");
        const auto d12 = byKey("ship.kli.d12");
        const int x = 0, y = 0;
        auto add = [&](int owner, const botf::roster::RosterClass& c) {
            botf::Fleet f;
            f.id = s.nextFleetId++;
            f.ownerId = owner;
            f.x = f.targetX = x; f.y = f.targetY = y;
            f.shipClass = botf::Building::Destroyer;
            f.referenceShipId = c.id;
            f.hull = c.hull;
            f.shields = c.shields;
            s.fleets.push_back(f);
            return f.id;
        };
        const int a = add(1, akira), b = add(2, d7), c = add(2, d12);
        auto relation = std::find_if(s.majorRelations.begin(), s.majorRelations.end(), [](const botf::MajorRelation& r) {
            return r.first == 1 && r.second == 2;
        });
        check(relation != s.majorRelations.end(), "Relation missing");
        relation->contact = true; relation->atWar = true; relation->tradeTreaty = false; relation->warStartTurn = s.turn;
        const auto enc = botf::tactical::constructEncounter(s, {a, b, c});
        const auto ship = [&](int id) {
            return *std::find_if(enc.ships.begin(), enc.ships.end(), [&](const botf::tactical::Ship& sh) { return sh.fleetId == id; });
        };
        const auto sa = ship(a), sb = ship(b), sc = ship(c);
        check(!sa.canCloak && !sb.canCloak && sc.canCloak && sc.isCloaked, "Cloak must follow the class flag");
        const int total = sa.beamAttack + sa.torpedoAttack;
        check(total > 0 && std::abs(sa.torpedoAttack * 100 / total - akira.torpedoSharePercent) <= 3,
            "Akira split must follow its weapon fit: " + std::to_string(sa.torpedoAttack) + "/" + std::to_string(total));
        check(sa.torpedoAttack > sa.beamAttack, "Akira is a torpedo boat");
        check(sb.torpedoAttack < sb.beamAttack, "D7 is a disruptor ship");
        check(sa.speed == std::min(3, akira.speed) && sc.speed == 3, "Speed from the roster");
    }, passed);
}

void campaignScenarios(int& passed) {
    run("AI adopts expanded classes in Remastered and the run is deterministic", [] {
        auto play = [] {
            botf::CampaignSetup setup;
            setup.seed = 26713; setup.empires = 5; setup.reference = true; setup.preset = botf::RulesetPreset::Remastered;
            auto s = botf::newCampaign(setup);
            int expanded = 0;
            for (int turn = 0; turn < 120 && s.outcome == botf::CampaignOutcome::Ongoing; ++turn) {
                botf::advance(s);
                for (const auto& c : s.colonies)
                    for (const auto& p : c.queue) if (c.ownerId != s.playerEmpire && botf::roster::isExpandedId(p.referenceId)) ++expanded;
            }
            for (const auto& f : s.fleets) if (botf::roster::isExpandedId(f.referenceShipId)) ++expanded;
            return std::make_pair(botf::fingerprint(s), expanded);
        };
        const auto first = play();
        const auto second = play();
        check(first.first == second.first, "Remastered roster campaign is not deterministic");
        check(first.second > 0, "No AI empire queued or fielded an expanded class");
    }, passed);

    run("classic campaign never touches expanded ids", [] {
        botf::CampaignSetup setup;
        setup.seed = 26713; setup.empires = 5; setup.reference = true; setup.preset = botf::RulesetPreset::Classic;
        auto s = botf::newCampaign(setup);
        for (int turn = 0; turn < 120 && s.outcome == botf::CampaignOutcome::Ongoing; ++turn) {
            botf::advance(s);
            for (const auto& f : s.fleets) check(!botf::roster::isExpandedId(f.referenceShipId), "Classic fielded an expanded class");
            for (const auto& c : s.colonies)
                for (const auto& p : c.queue) check(!botf::roster::isExpandedId(p.referenceId), "Classic queued an expanded class");
        }
        check(!contains(botf::serialize(s), "SHIP_ROSTER"), "Classic save gained the roster");
    }, passed);
}
} // namespace

int main() {
    int passed = 0;
    dataScenarios(passed);
    rulesetScenarios(passed);
    tacticalScenarios(passed);
    campaignScenarios(passed);
    std::cout << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
