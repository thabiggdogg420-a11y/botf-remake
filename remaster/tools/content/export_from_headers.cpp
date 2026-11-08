// One-time M0 migration exporter and equality probe (docs/DATA-FORMAT.md sections 2 and 9).
//
//   export_from_headers export <dir>   write the section files (technologies.json ... events.json) from the
//                                       compiled tables (only meaningful against the pre-M0 headers, f2581a7)
//   export_from_headers dump  <file>   write every field of every loaded record as flat text (doubles in hex)
//
// Build the same source twice: against the f2581a7 headers and against the data-driven core. Identical dumps
// prove the JSON pack reproduces the compiled tables field for field. Not part of any build.
#include "MinorRaces.h"
#include "ReferenceCatalogue.h"
#include "ShipRoster.h"
#include "Simulation.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace botf;

namespace {
const char* const FactionNames[] = {"Federation", "Klingon", "Romulan", "Cardassian", "Ferengi", "Dominion", "Borg",
    "Breen", "Tholian", "Gorn", "Orion", "Hirogen", "Kazon", "Vidiian", "Suliban", "Xindi", "Species8472", "Krenim",
    "Malon", "SonA", "Maquis", "Reman", "Devore", "Andorian", "Vulcan", "Bajoran", "Nausicaan", "Talarian"};
const char* const HullRoleNames[] = {"Scout", "Science", "Escort", "Frigate", "Destroyer", "Raider", "LightCruiser",
    "Cruiser", "HeavyCruiser", "StrikeCruiser", "Carrier", "Battlecruiser", "Explorer", "Battleship", "Dreadnought",
    "Colony", "Transport", "Station", "Special"};
const char* const EraNames[] = {"Enterprise", "Original", "Movie", "NextGeneration", "DominionWar", "Nemesis", "Kelvin", "Future"};
const char* const SourceKindNames[] = {"Canon", "ModDerived", "Kelvin"};
const char* const WeaponTypeNames[] = {"None", "PhaseCannon", "PhaserBank", "PhaserArray", "PulsePhaser", "Disruptor",
    "DisruptorCannon", "PlasmaCannon", "PolaronBeam", "CuttingBeam", "EnergyDampener", "TholianWeb", "BioEnergy",
    "TemporalWeapon", "SpatialTorpedo", "PhotonicTorpedo", "PhotonTorpedo", "QuantumTorpedo", "PlasmaTorpedo",
    "TricobaltDevice", "Torpedo"};
const char* const ArcNames[] = {"Forward", "ForwardWide", "Broadside", "Aft", "All"};
const char* const SizeNames[] = {"Small", "Medium", "Large", "Huge", "Station"};
const char* const TemperamentNames[] = {"Peaceful", "Scientific", "Mercantile", "Spiritual", "Xenophobic", "Warlike"};
const char* const JoinBonusNames[] = {"Research", "Industry", "Food", "Credits", "Morale", "GroundDefense"};
const char* const EventKindNames[] = {"Plague", "SupernovaWarning", "SubspaceRift", "TradeBoom", "DerelictVessel",
    "LaborUnrest", "ScientificBreakthrough"};

std::string q(const char* s) {
    std::string out = "\"";
    for (const char* p = s; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
        else out += static_cast<char>(c);
    }
    return out + "\"";
}
std::string q(const std::string& s) { return q(s.c_str()); }
template <class A> std::string arr(const A& a) {
    std::string out = "[";
    for (std::size_t i = 0; i < a.size(); ++i) out += (i ? "," : "") + std::to_string(a[i]);
    return out + "]";
}
std::string num(double d) { // Shortest form; the values are small decimals.
    char buf[64];
    for (int p = 1; p <= 17; ++p) {
        std::snprintf(buf, sizeof buf, "%.*g", p, d);
        if (std::stod(buf) == d) break;
    }
    return buf;
}
std::string hex(double d) { char buf[64]; std::snprintf(buf, sizeof buf, "%a", d); return buf; }

void writeSection(const std::string& path, const std::string& key, const std::vector<std::string>& rows) {
    std::ofstream out(path, std::ios::binary);
    out << "{\"" << key << "\":[\n";
    for (std::size_t i = 0; i < rows.size(); ++i) out << rows[i] << (i + 1 < rows.size() ? ",\n" : "\n");
    out << "]}\n";
}

void exportAll(const std::string& dir) {
    std::vector<std::string> rows;
    for (const auto& t : referenceData::TechnologyCatalogue)
        rows.push_back("{\"id\":" + std::to_string(t.id) + ",\"name\":" + q(t.name) + ",\"field\":" + std::to_string(t.field) +
            ",\"level\":" + std::to_string(t.level) + ",\"cost\":" + std::to_string(t.cost) + ",\"description\":" + q(t.description) + "}");
    writeSection(dir + "/technologies.json", "technologies", rows); rows.clear();
    for (const auto& s : referenceData::ShipCatalogue)
        rows.push_back("{\"id\":" + std::to_string(s.id) + ",\"raceId\":" + std::to_string(s.raceId) + ",\"name\":" + q(s.name) +
            ",\"shipClass\":" + q(s.shipClass) + ",\"technologyRequirements\":" + arr(s.technologyRequirements) +
            ",\"industryCost\":" + std::to_string(s.industryCost) + ",\"maintenance\":" + std::to_string(s.maintenance) +
            ",\"hull\":" + std::to_string(s.hull) + ",\"shields\":" + std::to_string(s.shields) + ",\"attack\":" + std::to_string(s.attack) +
            ",\"range\":" + num(s.range) + ",\"speed\":" + num(s.speed) + ",\"role\":" + std::to_string(s.role) +
            ",\"troopCapacity\":" + std::to_string(s.troopCapacity) + ",\"description\":" + q(s.description) + "}");
    writeSection(dir + "/ships.json", "ships", rows); rows.clear();
    for (const auto& b : referenceData::BuildingCatalogue)
        rows.push_back("{\"id\":" + std::to_string(b.id) + ",\"name\":" + q(b.name) + ",\"technologyField\":" + std::to_string(b.technologyField) +
            ",\"technologyLevel\":" + std::to_string(b.technologyLevel) + ",\"industryCost\":" + std::to_string(b.industryCost) +
            ",\"energyCost\":" + std::to_string(b.energyCost) + ",\"populationCost\":" + std::to_string(b.populationCost) +
            ",\"moraleBonus\":" + std::to_string(b.moraleBonus) + ",\"excludedEmpireMask\":" + std::to_string(b.excludedEmpireMask) +
            ",\"residentRaceMask\":" + std::to_string(b.residentRaceMask) + ",\"bonusType\":" + std::to_string(b.bonusType) +
            ",\"bonusValues\":" + arr(b.bonusValues) + ",\"unrestBonuses\":" + arr(b.unrestBonuses) + ",\"description\":" + q(b.description) + "}");
    writeSection(dir + "/buildings.json", "buildings", rows); rows.clear();
    for (const auto& r : referenceData::RaceCatalogue)
        rows.push_back("{\"id\":" + std::to_string(r.id) + ",\"name\":" + q(r.name) + ",\"icon\":" + q(r.icon) + "}");
    writeSection(dir + "/races.json", "races", rows); rows.clear();
    for (const auto& c : roster::ExpandedRoster) {
        std::string w = "[";
        for (std::size_t i = 0; i < c.weapons.size(); ++i)
            w += std::string(i ? "," : "") + "{\"type\":" + q(WeaponTypeNames[static_cast<int>(c.weapons[i].type)]) +
                ",\"count\":" + std::to_string(c.weapons[i].count) + ",\"arc\":" + q(ArcNames[static_cast<int>(c.weapons[i].arc)]) + "}";
        w += "]";
        rows.push_back("{\"id\":" + std::to_string(c.id) + ",\"assetKey\":" + q(c.assetKey) +
            ",\"faction\":" + q(FactionNames[static_cast<int>(c.faction)]) + ",\"raceId\":" + std::to_string(c.raceId) +
            ",\"minorRaceId\":" + std::to_string(c.minorRaceId) + ",\"roleName\":" + q(c.roleName) + ",\"className\":" + q(c.className) +
            ",\"hullRole\":" + q(HullRoleNames[static_cast<int>(c.hullRole)]) + ",\"engineRole\":" + std::to_string(c.engineRole) +
            ",\"era\":" + q(EraNames[static_cast<int>(c.era)]) + ",\"tier\":" + std::to_string(c.tier) +
            ",\"technologyRequirements\":" + arr(c.technologyRequirements) + ",\"industryCost\":" + std::to_string(c.industryCost) +
            ",\"buildTurns\":" + std::to_string(c.buildTurns) + ",\"maintenance\":" + std::to_string(c.maintenance) +
            ",\"hull\":" + std::to_string(c.hull) + ",\"shields\":" + std::to_string(c.shields) + ",\"attack\":" + std::to_string(c.attack) +
            ",\"torpedoSharePercent\":" + std::to_string(c.torpedoSharePercent) + ",\"weapons\":" + w +
            ",\"weaponCount\":" + std::to_string(c.weaponCount) + ",\"speed\":" + std::to_string(c.speed) +
            ",\"range\":" + std::to_string(c.range) + ",\"sensors\":" + std::to_string(c.sensors) +
            ",\"cloak\":" + (c.cloak ? "true" : "false") + ",\"sourceKind\":" + q(SourceKindNames[static_cast<int>(c.sourceKind)]) +
            ",\"source\":" + q(c.source) + ",\"buildable\":" + (c.buildable ? "true" : "false") + ",\"description\":" + q(c.description) +
            ",\"flight\":{\"turnRate\":" + std::to_string(c.flight.turnRate) + ",\"acceleration\":" + std::to_string(c.flight.acceleration) +
            ",\"lateral\":" + std::to_string(c.flight.lateral) + ",\"size\":" + q(SizeNames[static_cast<int>(c.flight.size)]) + "}}");
    }
    writeSection(dir + "/roster.json", "roster", rows); rows.clear();
    for (const auto& r : minors::Roster)
        rows.push_back("{\"id\":" + std::to_string(r.id) + ",\"name\":" + q(r.name) + ",\"homeworld\":" + q(r.homeworld) +
            ",\"referenceRaceId\":" + std::to_string(r.referenceRaceId) + ",\"temperament\":" + q(TemperamentNames[static_cast<int>(r.temperament)]) +
            ",\"techLevel\":" + std::to_string(r.techLevel) + ",\"population\":" + std::to_string(r.population) +
            ",\"bonus\":" + q(JoinBonusNames[static_cast<int>(r.bonus)]) + ",\"bonusValue\":" + std::to_string(r.bonusValue) +
            ",\"signatureShipId\":" + std::to_string(r.signatureShipId) + ",\"affinity\":" + arr(r.affinity) + ",\"description\":" + q(r.description) + "}");
    writeSection(dir + "/minors.json", "minors", rows); rows.clear();
    // Event text: names, choices and defaults come from the code; description templates are written by hand
    // in events.json (describeEvent built them with string concatenation) and checked by the dump below.
    for (int k = 0; k < 7; ++k) {
        const auto kind = static_cast<GalacticEventKind>(k);
        std::string ch = "[";
        for (const auto& c : eventChoices(kind)) ch += std::string(c.index ? "," : "") + "{\"label\":" + q(c.label) + ",\"effect\":" + q(c.effect) + "}";
        ch += "]";
        rows.push_back("{\"kind\":" + q(EventKindNames[k]) + ",\"name\":" + q(galacticEventName(kind)) +
            ",\"defaultChoice\":" + std::to_string(eventDefaultChoice(kind)) + ",\"choices\":" + ch + ",\"description\":\"\"}");
    }
    writeSection(dir + "/events.json", "events", rows);
}

void dumpAll(const std::string& path) {
    std::ostringstream o;
    for (const auto& t : referenceData::TechnologyCatalogue) o << "T " << t.id << '|' << t.name << '|' << t.field << '|' << t.level << '|' << t.cost << '|' << t.description << '\n';
    for (const auto& s : referenceData::ShipCatalogue) {
        o << "S " << s.id << '|' << s.raceId << '|' << s.name << '|' << s.shipClass << '|' << arr(s.technologyRequirements) << '|' << s.industryCost << '|'
          << s.maintenance << '|' << s.hull << '|' << s.shields << '|' << s.attack << '|' << hex(s.range) << '|' << hex(s.speed) << '|' << s.role << '|'
          << s.troopCapacity << '|' << s.description << '\n';
    }
    for (const auto& b : referenceData::BuildingCatalogue)
        o << "B " << b.id << '|' << b.name << '|' << b.technologyField << '|' << b.technologyLevel << '|' << b.industryCost << '|' << b.energyCost << '|'
          << b.populationCost << '|' << b.moraleBonus << '|' << b.excludedEmpireMask << '|' << b.residentRaceMask << '|' << b.bonusType << '|'
          << arr(b.bonusValues) << '|' << arr(b.unrestBonuses) << '|' << b.description << '\n';
    for (const auto& r : referenceData::RaceCatalogue) o << "R " << r.id << '|' << r.name << '|' << r.icon << '\n';
    for (const auto& c : roster::ExpandedRoster) {
        o << "X " << c.id << '|' << c.assetKey << '|' << static_cast<int>(c.faction) << '|' << c.raceId << '|' << c.minorRaceId << '|' << c.roleName << '|'
          << c.className << '|' << static_cast<int>(c.hullRole) << '|' << c.engineRole << '|' << static_cast<int>(c.era) << '|' << c.tier << '|'
          << arr(c.technologyRequirements) << '|' << c.industryCost << '|' << c.buildTurns << '|' << c.maintenance << '|' << c.hull << '|' << c.shields << '|'
          << c.attack << '|' << c.torpedoSharePercent << '|';
        for (const auto& w : c.weapons) o << static_cast<int>(w.type) << ':' << w.count << ':' << static_cast<int>(w.arc) << ',';
        o << '|' << c.weaponCount << '|' << c.speed << '|' << c.range << '|' << c.sensors << '|' << c.cloak << '|' << static_cast<int>(c.sourceKind) << '|'
          << c.source << '|' << c.buildable << '|' << c.description << '|' << c.flight.turnRate << ':' << c.flight.acceleration << ':' << c.flight.lateral << ':'
          << static_cast<int>(c.flight.size) << '\n';
    }
    for (const auto& s : roster::ExpandedShipRecords)
        o << "XS " << s.id << '|' << s.name << '|' << s.shipClass << '|' << hex(s.range) << '|' << hex(s.speed) << '|' << s.role << '|' << s.troopCapacity << '\n';
    for (const auto& r : minors::Roster)
        o << "M " << r.id << '|' << r.name << '|' << r.homeworld << '|' << r.referenceRaceId << '|' << static_cast<int>(r.temperament) << '|' << r.techLevel << '|'
          << r.population << '|' << static_cast<int>(r.bonus) << '|' << r.bonusValue << '|' << r.signatureShipId << '|' << arr(r.affinity) << '|' << r.description << '\n';
    for (int k = 0; k < 7; ++k) {
        const auto kind = static_cast<GalacticEventKind>(k);
        o << "E " << k << '|' << galacticEventName(kind) << '|' << eventDefaultChoice(kind) << '\n';
        for (const auto& c : eventChoices(kind)) o << "EC " << c.index << '|' << c.label << '|' << c.effect << '|' << c.isDefault << '\n';
        GameState s;
        GalacticEvent e; e.kind = kind; e.systemId = 12; e.magnitude = 7; e.endTurn = 31;
        o << "ED1 " << describeEvent(s, e) << '\n';
        e.systemId = 0; e.x = 4; e.y = 9;
        o << "ED2 " << describeEvent(s, e) << '\n';
        e.x = -1; e.y = -1;
        o << "ED3 " << describeEvent(s, e) << '\n';
    }
    for (int id : {0, 104, 124, 125, 999, 1000, 1001, 1097, 1999, 2000, -1}) {
        const auto* r = roster::findShip(id);
        o << "F " << id << '|' << (r ? r->id : -9) << '|' << roster::isExpandedId(id) << '|' << (roster::findRosterClass(id) != nullptr) << '\n';
    }
    for (int id : {0, 1, 15, 30, 31}) o << "MR " << id << '|' << (minors::raceById(id) ? minors::raceById(id)->name : "-") << '\n';
    std::ofstream(path, std::ios::binary) << o.str();
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 3) { std::cerr << "usage: export_from_headers export <dir> | dump <file>\n"; return 2; }
    const std::string mode = argv[1];
    if (mode == "export") exportAll(argv[2]);
    else if (mode == "dump") dumpAll(argv[2]);
    else { std::cerr << "unknown mode\n"; return 2; }
    return 0;
}
