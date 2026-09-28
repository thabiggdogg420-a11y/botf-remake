// BOTF Remastered expanded ship roster (docs/SHIP-ROSTER.md).
//
// The legacy reference catalogue (ReferenceCatalogue.h, ids 0..124) is untouched. Expanded classes use ids from
// ExpandedIdBase upward, so every legacy save, fixture and Classic campaign keeps its ids and behaviour. Code that
// resolves a ship id should call findShip(); it covers both ranges and returns nullptr for an unknown id.
// Expanded classes are buildable only when GameState::rules.expandedRoster is on (Remastered preset).
#pragma once
#include "ReferenceCatalogue.h"
#include "ShipRosterData.h"
#include "ShipRosterTypes.h"

#include <array>
#include <cstddef>
#include <string>
#include <utility>

namespace botf::roster {

inline constexpr int ExpandedIdBase = 1000;
inline constexpr int ExpandedIdLimit = 2000; // Exclusive; ids are sparse inside [base, limit).

constexpr referenceData::ShipRecord toShipRecord(const RosterClass& c) {
    return referenceData::ShipRecord{c.id, c.raceId, c.roleName, c.className, c.technologyRequirements, c.industryCost,
        c.maintenance, c.hull, c.shields, c.attack, static_cast<double>(c.range), static_cast<double>(c.speed),
        c.engineRole, -1, c.description};
}
template <std::size_t... I>
constexpr std::array<referenceData::ShipRecord, sizeof...(I)> makeShipRecords(std::index_sequence<I...>) {
    return {{toShipRecord(ExpandedRoster[I])...}};
}
inline constexpr auto ExpandedShipRecords = makeShipRecords(std::make_index_sequence<ExpandedRoster.size()>{});

// Sparse id -> roster index table, built at compile time (-1 = unused id).
constexpr std::array<int, ExpandedIdLimit - ExpandedIdBase> makeIndex() {
    std::array<int, ExpandedIdLimit - ExpandedIdBase> index{};
    for (auto& v : index) v = -1;
    for (std::size_t i = 0; i < ExpandedRoster.size(); ++i) index[static_cast<std::size_t>(ExpandedRoster[i].id - ExpandedIdBase)] = static_cast<int>(i);
    return index;
}
inline constexpr auto ExpandedIndex = makeIndex();

constexpr bool rosterIdsValid() {
    int previous = ExpandedIdBase - 1;
    for (const auto& c : ExpandedRoster) {
        if (c.id <= previous || c.id >= ExpandedIdLimit) return false;
        previous = c.id;
    }
    return true;
}
static_assert(rosterIdsValid(), "Expanded roster ids must be ascending and inside [ExpandedIdBase, ExpandedIdLimit)");
static_assert(referenceData::ShipCatalogue.size() <= static_cast<std::size_t>(ExpandedIdBase), "Legacy ids must stay below the expanded range");

inline bool isExpandedId(int id) {
    return id >= ExpandedIdBase && id < ExpandedIdLimit && ExpandedIndex[static_cast<std::size_t>(id - ExpandedIdBase)] >= 0;
}
inline const RosterClass* findRosterClass(int id) {
    return isExpandedId(id) ? &ExpandedRoster[static_cast<std::size_t>(ExpandedIndex[static_cast<std::size_t>(id - ExpandedIdBase)])] : nullptr;
}
// Any ship id: legacy catalogue or expanded roster. nullptr when unknown.
inline const referenceData::ShipRecord* findShip(int id) {
    if (id >= 0 && static_cast<std::size_t>(id) < referenceData::ShipCatalogue.size()) return &referenceData::ShipCatalogue[static_cast<std::size_t>(id)];
    if (isExpandedId(id)) return &ExpandedShipRecords[static_cast<std::size_t>(ExpandedIndex[static_cast<std::size_t>(id - ExpandedIdBase)])];
    return nullptr;
}
inline bool validShipId(int id) { return findShip(id) != nullptr; }

inline bool torpedoWeapon(WeaponType t) {
    return t == WeaponType::SpatialTorpedo || t == WeaponType::PhotonicTorpedo || t == WeaponType::PhotonTorpedo ||
        t == WeaponType::QuantumTorpedo || t == WeaponType::PlasmaTorpedo || t == WeaponType::TricobaltDevice || t == WeaponType::Torpedo;
}
// Points per mount and arc factor used to derive attack (mirrors gen_ship_roster.py; RosterTests re-derives).
inline int weaponPoints(WeaponType t) {
    switch (t) {
    case WeaponType::PhaseCannon: return 5;
    case WeaponType::PhaserBank: return 7;
    case WeaponType::PhaserArray: return 10;
    case WeaponType::PulsePhaser: return 13;
    case WeaponType::Disruptor: return 9;
    case WeaponType::DisruptorCannon: return 11;
    case WeaponType::PlasmaCannon: return 6;
    case WeaponType::PolaronBeam: return 12;
    case WeaponType::CuttingBeam: return 20;
    case WeaponType::EnergyDampener: return 4;
    case WeaponType::TholianWeb: return 6;
    case WeaponType::BioEnergy: return 30;
    case WeaponType::TemporalWeapon: return 40;
    case WeaponType::SpatialTorpedo: return 4;
    case WeaponType::PhotonicTorpedo: return 6;
    case WeaponType::PhotonTorpedo: return 9;
    case WeaponType::QuantumTorpedo: return 14;
    case WeaponType::PlasmaTorpedo: return 12;
    case WeaponType::TricobaltDevice: return 18;
    case WeaponType::Torpedo: return 8;
    case WeaponType::None: break;
    }
    return 0;
}
inline int arcPercent(FiringArc a) {
    switch (a) {
    case FiringArc::Forward: return 100;
    case FiringArc::ForwardWide: return 110;
    case FiringArc::Broadside: return 90;
    case FiringArc::Aft: return 50;
    case FiringArc::All: return 120;
    }
    return 0;
}
inline int attackScale(int tier) { return 250 + 50 * tier + 25 * tier * tier; }

inline const char* factionName(Faction f) {
    static constexpr std::array<const char*, 28> names{"Federation", "Klingon Empire", "Romulan Star Empire",
        "Cardassian Union", "Ferengi Alliance", "Dominion", "Borg Collective", "Breen Confederacy", "Tholian Assembly",
        "Gorn Hegemony", "Orion Syndicate", "Hirogen", "Kazon sects", "Vidiian Sodality", "Suliban Cabal", "Xindi Council",
        "Species 8472", "Krenim Imperium", "Malon", "Son'a", "Maquis", "Reman", "Devore Imperium", "Andorians", "Vulcans",
        "Bajorans", "Nausicaans", "Talarians"};
    const auto i = static_cast<std::size_t>(f);
    return i < names.size() ? names[i] : "Unknown";
}
inline const char* weaponTypeName(WeaponType t) {
    static constexpr std::array<const char*, 21> names{"None", "PhaseCannon", "PhaserBank", "PhaserArray", "PulsePhaser",
        "Disruptor", "DisruptorCannon", "PlasmaCannon", "PolaronBeam", "CuttingBeam", "EnergyDampener", "TholianWeb",
        "BioEnergy", "TemporalWeapon", "SpatialTorpedo", "PhotonicTorpedo", "PhotonTorpedo", "QuantumTorpedo",
        "PlasmaTorpedo", "TricobaltDevice", "Torpedo"};
    const auto i = static_cast<std::size_t>(t);
    return i < names.size() ? names[i] : "Unknown";
}
// Tactical rules 3 weapon behaviour. Cannons and pulse weapons fire bursts; everything torpedoWeapon() travels.
inline WeaponFamily weaponFamily(WeaponType t) {
    if (torpedoWeapon(t)) return WeaponFamily::Torpedo;
    if (t == WeaponType::PhaseCannon || t == WeaponType::PulsePhaser || t == WeaponType::DisruptorCannon ||
        t == WeaponType::PlasmaCannon) return WeaponFamily::Pulse;
    return WeaponFamily::Beam;
}
inline const char* weaponFamilyName(WeaponFamily f) {
    switch (f) {
    case WeaponFamily::Beam: return "beam";
    case WeaponFamily::Pulse: return "pulse";
    case WeaponFamily::Torpedo: return "torpedo";
    }
    return "unknown";
}
// Asset/audio lookup key shared with the asset manifest template: "weapon.PhaserArray".
inline std::string weaponTypeKey(WeaponType t) { return std::string("weapon.") + weaponTypeName(t); }
inline const char* sizeClassName(SizeClass c) {
    static constexpr std::array<const char*, 5> names{"Small", "Medium", "Large", "Huge", "Station"};
    const auto i = static_cast<std::size_t>(c);
    return i < names.size() ? names[i] : "Unknown";
}
inline const char* eraName(Era e) {
    static constexpr std::array<const char*, 8> names{"Enterprise", "Original", "Movie", "NextGeneration", "DominionWar",
        "Nemesis", "Kelvin", "Future"};
    const auto i = static_cast<std::size_t>(e);
    return i < names.size() ? names[i] : "Unknown";
}
inline const char* sourceKindName(SourceKind k) {
    switch (k) {
    case SourceKind::Canon: return "canon";
    case SourceKind::ModDerived: return "mod-derived";
    case SourceKind::Kelvin: return "kelvin";
    }
    return "unknown";
}

} // namespace botf::roster
