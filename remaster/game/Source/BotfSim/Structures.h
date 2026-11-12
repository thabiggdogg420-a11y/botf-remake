// BOTF Remastered: race structures (lane MOD-3; docs/design/DOMINION.md).
//
// A pack adds campaign structures as appended `buildings` records with `buildable: true` (the Dominion structure ladder is the
// first such pack). The core wires four things to them and nothing else, so a pack with no buildable record changes nothing:
//   * a colony may queue one (Building::Structure, Construction::referenceId = RefBase + record id) and holds the built ones;
//   * the ladder: `requires` names a record that must already stand in the same colony; `perEmpireLimit` caps the empire;
//   * effects, by `bonusType` (the vanilla meanings): 14 industry %, 30 research %, 42 food %, 5 credit income %, 20 morale,
//     37 ground defence %, 11 internal-security output %; and the two new ones, 90 Shapeshifter Network (sabotage output %, values[0],
//     sabotage damage bonus, values[1]) and 91 Ketracel-White (the roster classes in values[] need one in the empire);
//   * `excludedEmpireMask` bit N = race N may not use it (64 bits wide), `startsBuilt` = in every eligible capital at turn 0.
// A structure draws no colony energy; it costs credit upkeep of one per 400 industry of its cost. A captured colony keeps the
// structures it holds and they work for the new owner (they cannot be rebuilt by a race that may not use them).
//
// Save (schema 21): the optional section, written only while some colony holds a structure, after EMPIRE_RULES:
//     STRUCTURES 1
//     <n>                         then n rows: <systemId> <count> <record id> ...   (ascending ids)
// Integer arithmetic only; header-only like EmpireRules.h.
#pragma once

#include "Content.h"
#include "EmpireSlots.h"
#include "Simulation.h"
#include "StructureUpgrades.h"

#include <algorithm>
#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace botf::structures {

inline constexpr int RefBase = 100000;       // Construction::referenceId of a structure project = RefBase + record id.
inline constexpr int MaxPerColony = 12;
inline constexpr int MaxSabotagePercent = 150, MaxSecurityPercent = 150;

enum Effect : int { Credits = 5, Security = 11, Industry = 14, Morale = 20, Research = 30, GroundDefense = 37, Food = 42,
                    ShapeshifterNetwork = 90, KetracelWhite = 91,
                    // Schema 23 (EconomyRules.h): academy (values[0] experience points a turn for fleets in its system, values[1] the cap),
                    // shared morale (values[0] morale for every colony of the empire) and Protectorate HQ (values[0] morale for subjugated worlds).
                    Training = 16, Academy = 92, SharedMorale = 93, ProtectorateHq = 94 };

inline bool isRef(int referenceId) { return referenceId >= RefBase && referenceId < RefBase + 1000000; }
inline int refFor(int recordId) { return RefBase + recordId; }
inline int recordIdOf(int referenceId) { return referenceId - RefBase; }

inline const referenceData::BuildingRecord* record(int id) {
    const auto& all = content::tables::buildings();
    return id >= 0 && static_cast<std::size_t>(id) < all.size() ? &all[static_cast<std::size_t>(id)] : nullptr;
}
// Schema 23: the four per-race academies of the original game (Central Command 25, Starfleet Academy 37, Tactical College 43, Naval Academy 50)
// are campaign structures in the base pack with no overlay needed; they train crews (EconomyRules.h academyAt) and are refused while
// crew experience is off. They stay out of anyBuildable(): the structure engine (AI building, started-built records) is for packs.
inline bool isVanillaAcademy(const referenceData::BuildingRecord& r) { return r.bonusType == 16 && (r.id == 25 || r.id == 37 || r.id == 43 || r.id == 50); }
inline bool isCampaign(const referenceData::BuildingRecord& r) { return r.buildable || isVanillaAcademy(r); }
inline const referenceData::BuildingRecord* campaignRecord(int id) {
    const auto* r = record(id);
    return r && isCampaign(*r) ? r : nullptr;
}
inline bool anyBuildable() {
    for (const auto& r : content::tables::buildings()) if (r.buildable) return true;
    return false;
}
// True when the empire's race may use the record (the excluded mask is by RaceCatalogue id).
inline bool allowedFor(const referenceData::BuildingRecord& r, int empireId) {
    const int race = slots::catalogueRace(empireId);
    return race >= 0 && race < 64 && ((r.excludedEmpireMask >> race) & 1ULL) == 0;
}
inline bool built(const Colony& c, int id) { return std::binary_search(c.structures.begin(), c.structures.end(), id); }
inline int upkeepOf(const referenceData::BuildingRecord& r) { return std::max(1, r.industryCost / 400); }
// One-line player text for what the structure does (the same numbers the rules use).
inline std::string effectText(const referenceData::BuildingRecord& r) {
    const std::string v = std::to_string(r.bonusValues[0]);
    switch (r.bonusType) {
        case Industry: return "+" + v + "% industry here";
        case Research: return "+" + v + "% research here";
        case Food: return "+" + v + "% food here";
        case Credits: return "+" + v + "% credit income here";
        case Morale: return "+" + v + " morale here";
        case GroundDefense: return "+" + v + "% ground defence here";
        case Security: return "+" + v + "% internal security output (empire-wide)";
        case ShapeshifterNetwork: return "+" + v + "% sabotage output, +" + std::to_string(r.bonusValues[1]) + " sabotage damage (empire-wide)";
        case KetracelWhite: return "Jem'Hadar warships need one in the empire";
        case Academy: return "Fleets in this system train +" + v + " experience a turn up to " + std::to_string(r.bonusValues[1]) + "; new ships launch with " + std::to_string(r.bonusValues[0] * 4) + " extra";
        case SharedMorale: return "+" + v + " morale in every colony of the empire";
        case ProtectorateHq: return "Subjugated worlds hold order (+" + v + " morale) and pay tribute; built in a subjugated world";
        default: return "";
    }
}
inline int cost(int referenceId) {
    if (upgrades::isRecord(recordIdOf(referenceId))) return upgrades::cost(recordIdOf(referenceId)); // Schema 23: a structure upgrade project.
    const auto* r = campaignRecord(recordIdOf(referenceId));
    if (!r) throw std::runtime_error("Unknown campaign structure");
    return r->industryCost;
}
inline const char* name(int referenceId) {
    if (upgrades::isRecord(recordIdOf(referenceId))) return upgrades::name(recordIdOf(referenceId));
    const auto* r = record(recordIdOf(referenceId));
    return r ? r->name : "Structure";
}

struct ColonyEffects { int industryPct = 0, researchPct = 0, foodPct = 0, creditsPct = 0, morale = 0, groundPct = 0, upkeep = 0; };
inline ColonyEffects colonyEffects(const Colony& c) {
    ColonyEffects e;
    for (int id : c.structures) {
        const auto* r = record(id);
        if (!r) continue;
        const int v = r->bonusValues[0];
        e.upkeep += upkeepOf(*r);
        switch (r->bonusType) {
            case Industry: e.industryPct += v; break;
            case Research: e.researchPct += v; break;
            case Food: e.foodPct += v; break;
            case Credits: e.creditsPct += v; break;
            case Morale: e.morale += v; break;
            case GroundDefense: e.groundPct += v; break;
            default: break;
        }
    }
    return e;
}

// Empire-wide sums over every colony: internal-security output percent, sabotage output percent and the damage bonus.
struct EmpireIntel { int securityPct = 0, sabotagePct = 0, sabotageDamage = 0, networks = 0; };
inline EmpireIntel empireIntel(const GameState& s, int empireId) {
    EmpireIntel e;
    for (const auto& c : s.colonies) {
        if (c.ownerId != empireId) continue;
        for (int id : c.structures) {
            const auto* r = record(id);
            if (!r) continue;
            if (r->bonusType == Security) e.securityPct += r->bonusValues[0];
            else if (r->bonusType == ShapeshifterNetwork) { e.sabotagePct += r->bonusValues[0]; e.sabotageDamage = std::max(e.sabotageDamage, r->bonusValues[1]); ++e.networks; }
        }
    }
    e.securityPct = std::clamp(e.securityPct + slots::traitEffects(empireId).securityPct, 0, MaxSecurityPercent);
    e.sabotagePct = std::min(e.sabotagePct, MaxSabotagePercent);
    return e;
}

inline int heldInEmpire(const GameState& s, int empireId, int id) {
    int n = 0;
    for (const auto& c : s.colonies) {
        if (c.ownerId != empireId) continue;
        if (built(c, id)) ++n;
        for (const auto& p : c.queue) if (p.product == Building::Structure && recordIdOf(p.referenceId) == id) ++n;
    }
    return n;
}

// Schema 23: rule-flag gates of the economy records (a pack may carry them while the rule is off).
inline std::string recordFlagBlocker(const GameState& s, const referenceData::BuildingRecord& r) {
    if ((r.bonusType == Academy || r.bonusType == Training) && !s.rules.crewExperience) return "Crew experience is off in this ruleset";
    if (r.bonusType == SharedMorale && !s.rules.sharedMorale) return "Shared morale is off in this ruleset";
    if (r.bonusType == ProtectorateHq && !s.rules.protectorateHq) return "The Protectorate is off in this ruleset";
    return {};
}
// Empire-level reasons the owner cannot build the structure: not a campaign record, wrong race, technology. A queued project is
// re-checked with this every turn (it must not count itself against the empire limit).
inline std::string empireBlockerForProject(const GameState& s, int owner, int id) {
    if (upgrades::isRecord(id)) return upgrades::empireBlocker(s, owner, id);
    const auto* r = campaignRecord(id);
    if (!r) return "Not a campaign structure";
    if (!allowedFor(*r, owner)) return std::string(r->name) + " is not available to this empire";
    if (auto flag = recordFlagBlocker(s, *r); !flag.empty()) return flag;
    if (r->bonusType == Academy)   // The generic Fleet Academy is only for races with no academy of their own.
        for (const auto& o : content::tables::buildings())
            if (isCampaign(o) && o.bonusType == Training && allowedFor(o, owner)) return "This empire has its own academy";
    if (r->technologyField >= 0 && r->technologyField < ResearchFields && r->technologyLevel > 0 &&
        empire(s, owner).technologies[static_cast<std::size_t>(r->technologyField)].level < r->technologyLevel)
        return std::string("Requires ") + researchName(r->technologyField) + " level " + std::to_string(r->technologyLevel);
    return {};
}
// The same plus the empire limit: the reasons the owner cannot start another one.
inline std::string empireBlocker(const GameState& s, int owner, int id) {
    if (auto b = empireBlockerForProject(s, owner, id); !b.empty()) return b;
    const auto* r = campaignRecord(id);
    if (r->perEmpireLimit > 0 && heldInEmpire(s, owner, id) >= r->perEmpireLimit)
        return "Empire limit of " + std::to_string(r->perEmpireLimit) + " reached";
    return {};
}
// Colony-level reasons: already here, ladder prerequisite, room.
inline std::string colonyBlocker(const Colony& c, int id) {
    const auto* r = campaignRecord(id);
    if (!r) return "Not a campaign structure";
    if (r->bonusType == ProtectorateHq && !c.subjugated) return "Build it in a subjugated world";
    if (built(c, id)) return "Already built here";
    for (const auto& p : c.queue) if (p.product == Building::Structure && recordIdOf(p.referenceId) == id) return "Already queued here";
    if (r->requiresId >= 0 && !built(c, r->requiresId)) {
        const auto* need = record(r->requiresId);
        return std::string("Needs a ") + (need ? need->name : "prerequisite") + " here first";
    }
    if (static_cast<int>(c.structures.size()) >= MaxPerColony) return "No room for another structure here";
    return {};
}
inline std::string blocker(const GameState& s, const Colony& c, int id) {
    if (upgrades::isRecord(id)) return upgrades::blocker(s, c, id);
    if (auto b = empireBlocker(s, c.ownerId, id); !b.empty()) return b;
    return colonyBlocker(c, id);
}

// Ketracel-White: a roster class named in a buildable type-91 record's values needs one of those structures standing in the empire.
inline std::string shipGate(const GameState& s, int owner, int shipId) {
    if (shipId < content::ExpandedIdBase) return {};
    const referenceData::BuildingRecord* gate = nullptr;
    for (const auto& r : content::tables::buildings()) {
        if (!r.buildable || r.bonusType != KetracelWhite || !allowedFor(r, owner)) continue;
        for (int v : r.bonusValues) if (v == shipId) { gate = &r; break; }
        if (gate) break;
    }
    if (!gate) return {};
    for (const auto& c : s.colonies) if (c.ownerId == owner && built(c, gate->id)) return {};
    return std::string("Needs a ") + gate->name + " in the empire";
}

// AI: the cheapest next rung this colony can afford and the empire may start; -1 when none. Deterministic (record order).
inline int aiNext(const GameState& s, const Colony& c, int industry) {
    if (!anyBuildable() || c.shipyards == 0 || industry <= 0 || empire(s, c.ownerId).credits < 150) return -1;
    for (const auto& r : content::tables::buildings()) {
        if (!r.buildable || r.industryCost > 100 * industry) continue;
        if (blocker(s, c, r.id).empty()) return r.id;
    }
    return -1;
}

// Turn 0: every eligible capital gets the `startsBuilt` records (they only require earlier start records, so id order is the ladder).
inline void seedStart(GameState& s) {
    if (s.turn != 0 || !anyBuildable()) return;
    std::vector<int> empireIds{s.playerEmpire};
    for (const auto& r : s.rivals) empireIds.push_back(r.id);
    for (int empireId : empireIds) {
        for (auto& c : s.colonies) {
            if (c.ownerId != empireId) continue;
            for (const auto& r : content::tables::buildings())
                if (r.buildable && r.startsBuilt && allowedFor(r, empireId)) c.structures.push_back(r.id);
            std::sort(c.structures.begin(), c.structures.end());
            break;
        }
    }
}

inline void validate(const GameState& s) {
    for (const auto& c : s.colonies) {
        if (c.structures.empty()) continue;
        if (c.structures.size() > static_cast<std::size_t>(MaxPerColony)) throw std::runtime_error("Too many structures in a colony");
        int previous = -1;
        for (int id : c.structures) {
            if (id <= previous || !campaignRecord(id)) throw std::runtime_error("Invalid colony structure");
            previous = id;
            const auto* r = record(id);
            if (r->requiresId >= 0 && !built(c, r->requiresId)) throw std::runtime_error("Colony structure without its prerequisite");
        }
    }
}

inline void writeSection(std::ostream& out, const GameState& s) {
    const auto rows = std::count_if(s.colonies.begin(), s.colonies.end(), [](const Colony& c) { return !c.structures.empty(); });
    if (rows == 0) return;
    out << "STRUCTURES 1\n" << rows << '\n';
    for (const auto& c : s.colonies) {
        if (c.structures.empty()) continue;
        out << c.systemId << ' ' << c.structures.size();
        for (int id : c.structures) out << ' ' << id;
        out << '\n';
    }
}
// Reads the section body after the STRUCTURES tag.
inline void readSection(std::istream& in, GameState& s) {
    auto fail = [](const char* m) { throw std::runtime_error(m); };
    int revision = 0, rows = -1;
    if (!(in >> revision >> rows) || revision != 1 || rows < 1 || rows > 100000) fail("Malformed STRUCTURES section");
    for (int i = 0; i < rows; ++i) {
        int systemId = 0, count = 0;
        if (!(in >> systemId >> count) || count < 1 || count > MaxPerColony) fail("Malformed STRUCTURES row");
        Colony* colony = nullptr;
        for (auto& c : s.colonies) if (c.systemId == systemId) colony = &c;
        if (!colony || !colony->structures.empty()) fail("STRUCTURES row names an unknown or repeated colony");
        for (int k = 0; k < count; ++k) {
            int id = -1;
            if (!(in >> id) || id < 0) fail("Malformed STRUCTURES row");
            colony->structures.push_back(id);
        }
    }
}

} // namespace botf::structures
