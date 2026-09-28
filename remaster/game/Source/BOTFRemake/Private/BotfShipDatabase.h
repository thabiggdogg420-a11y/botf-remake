#pragma once
// Engine-independent rows for the Object Database ship list: the legacy catalogue (ids 0..124) followed by the
// expanded roster (ids from roster::ExpandedIdBase), both read through the core's findShip()/findRosterClass().
// Returns UTF-8 text; the Slate screen only converts it. Tested in Tools/client-logic-tests.cpp.

#include "ShipRoster.h"

#include <cstddef>
#include <string>
#include <vector>

namespace botf::view {

// Every ship id the database lists, legacy first, then the expanded roster in id order.
inline const std::vector<int>& shipDatabaseIds() {
    static const std::vector<int> ids = [] {
        std::vector<int> out;
        out.reserve(referenceData::ShipCatalogue.size() + roster::ExpandedRoster.size());
        for (const auto& s : referenceData::ShipCatalogue) out.push_back(s.id);
        for (const auto& c : roster::ExpandedRoster) out.push_back(c.id);
        return out;
    }();
    return ids;
}

inline std::string shipDatabaseOwner(int shipId) {
    if (const auto* c = roster::findRosterClass(shipId)) return roster::factionName(c->faction);
    const auto* s = roster::findShip(shipId);
    if (!s) return "Unknown";
    if (s->raceId >= 0 && static_cast<std::size_t>(s->raceId) < referenceData::RaceCatalogue.size())
        return referenceData::RaceCatalogue[static_cast<std::size_t>(s->raceId)].name;
    return "Race " + std::to_string(s->raceId);
}

// "Galaxy / Command Cruiser / Federation"; expanded classes are marked so they are easy to find by search ("roster").
inline std::string shipDatabaseLabel(int shipId) {
    const auto* s = roster::findShip(shipId);
    if (!s) return "Unknown ship " + std::to_string(shipId);
    const bool expanded = roster::isExpandedId(shipId);
    std::string label = std::string(s->name ? s->name : "") + "  /  " + shipDatabaseOwner(shipId);
    if (expanded) label = std::string(s->shipClass ? s->shipClass : "") + " (" + (s->name ? s->name : "") + ")  /  " + shipDatabaseOwner(shipId) + "  /  roster";
    return label;
}

inline std::string shipDatabaseWeapons(const roster::RosterClass& c) {
    static const char* arcs[] = {"forward", "forward-wide", "broadside", "aft", "all-round"};
    std::string out;
    for (int i = 0; i < c.weaponCount && i < static_cast<int>(c.weapons.size()); ++i) {
        const auto& w = c.weapons[static_cast<std::size_t>(i)];
        if (w.type == roster::WeaponType::None || w.count <= 0) continue;
        if (!out.empty()) out += ", ";
        const int arc = static_cast<int>(w.arc);
        out += std::to_string(w.count) + "x " + roster::weaponTypeName(w.type) + " (" + (arc >= 0 && arc < 5 ? arcs[arc] : "?") + ")";
    }
    return out.empty() ? std::string("none") : out;
}

// Full entry text. researchNames: the six field names in order (the caller passes the core's names).
inline std::string shipDatabaseDetails(int shipId, const std::vector<std::string>& researchNames) {
    const auto* s = roster::findShip(shipId);
    if (!s) return "Select an entry.";
    std::string requirements;
    for (std::size_t f = 0; f < s->technologyRequirements.size(); ++f) {
        if (!requirements.empty()) requirements += "  |  ";
        requirements += (f < researchNames.size() ? researchNames[f] : "Field " + std::to_string(f)) + " " + std::to_string(s->technologyRequirements[f]);
    }
    std::string out = std::string(s->name ? s->name : "") + "  /  " + (s->shipClass ? s->shipClass : "") + "\n" + shipDatabaseOwner(shipId) + "\n\n";
    out += "Industry " + std::to_string(s->industryCost) + "  |  upkeep " + std::to_string(s->maintenance) + "  |  hull " +
        std::to_string(s->hull) + "  |  shields " + std::to_string(s->shields) + "  |  attack " + std::to_string(s->attack) + "\n";
    out += "Requires: " + requirements + "\n";
    if (const auto* c = roster::findRosterClass(shipId)) {
        out += std::string("Era: ") + roster::eraName(c->era) + "  |  tier " + std::to_string(c->tier) + "  |  build turns " +
            std::to_string(c->buildTurns) + "\n";
        out += "Speed " + std::to_string(c->speed) + "  |  range " + std::to_string(c->range) + "  |  sensors " +
            std::to_string(c->sensors) + (c->cloak ? "  |  cloaking device" : "") + "\n";
        out += "Weapons: " + shipDatabaseWeapons(*c) + "\n";
        out += std::string("Source: ") + (c->source ? c->source : "") + " (" + roster::sourceKindName(c->sourceKind) + ")\n";
        out += std::string("Expanded roster class: ") + (c->buildable ? "buildable by its empire under the Remastered ruleset" : "not buildable (special or non-player vessel)") + ".\n";
        out += std::string("Asset key: ") + (c->assetKey ? c->assetKey : "") + "\n";
    }
    out += std::string("\n") + (s->description ? s->description : "");
    out += "\n\nOnly ships listed in a system's Ship List can currently be built.";
    return out;
}

} // namespace botf::view
