// BOTF Remastered: structure upgrade tiers (lane MOD-6; docs/design/ECONOMY-MECHANICS.md section 2).
//
// Farms, factories and laboratories can be upgraded in tiers as the matching technology advances. An upgrade is a queue project
// (Building::Structure with a reference id in the reserved record range below), so it shows in the build list, takes industry like
// any project, and is paid for before it acts. Header-only and free of the pack tables, so Structures.h and Simulation.cpp can both
// include it. Integer arithmetic only. Everything is gated by GameplayRules::structureUpgrades / autoUpgrades / noUpgrades.
#pragma once

#include "Simulation.h"

#include <algorithm>
#include <string>

namespace botf::upgrades {

inline constexpr int RecordBase = 900000;   // Structure record ids from here up are upgrade projects, never pack records.
inline constexpr int Kinds = 3;             // 0 farms (Biotech), 1 factories (Construction), 2 laboratories (Computers).
inline constexpr int MaxTier = 3;
inline constexpr int TechPerTier = 2;       // Tier n of a kind needs its technology at level 2n.
inline constexpr int StepPercent = 20;      // A kind whose every unit is at tier t gives +20 x t percent output.
inline constexpr int ManualCostPercent = 60, AutoCostPercent = 40; // Of the unit's own construction cost.

inline Building buildingOf(int kind) { return kind == 0 ? Building::Farm : kind == 1 ? Building::Factory : Building::Laboratory; }
inline int techField(int kind) { return kind == 0 ? 0 : kind == 1 ? 2 : 1; }   // ResearchField index: Biotech, Construction, Computers.
inline const char* kindName(int kind) { return kind == 0 ? "farms" : kind == 1 ? "factories" : "laboratories"; }
inline const char* unitName(int kind) { return kind == 0 ? "farm" : kind == 1 ? "factory" : "laboratory"; }

inline int unitsOf(const Colony& c, int kind) { return kind == 0 ? c.farms : kind == 1 ? c.factories : c.laboratories; }

// Reference ids: code = kind + 4 for the automatic (cheaper) variant.
inline int recordFor(int kind, bool automatic) { return RecordBase + kind + (automatic ? 4 : 0); }
inline bool isRecord(int recordId) { return recordId >= RecordBase && recordId < RecordBase + 8 && (recordId - RecordBase) % 4 < Kinds; }
inline int kindOfRecord(int recordId) { return (recordId - RecordBase) % 4; }
inline bool automaticRecord(int recordId) { return recordId - RecordBase >= 4; }
inline int cost(int recordId) {
    const int base = definition(buildingOf(kindOfRecord(recordId))).industryCost;
    return std::max(1, base * (automaticRecord(recordId) ? AutoCostPercent : ManualCostPercent) / 100);
}
inline const char* name(int recordId) {
    static constexpr const char* Manual[Kinds] = {"Upgrade farms", "Upgrade factories", "Upgrade laboratories"};
    static constexpr const char* Auto[Kinds] = {"Automatic farm upgrade", "Automatic factory upgrade", "Automatic laboratory upgrade"};
    const int k = kindOfRecord(recordId);
    return automaticRecord(recordId) ? Auto[k] : Manual[k];
}

// The best tier the empire's science allows for a kind.
inline int tierAvailable(const GameState& s, int owner, int kind) {
    return std::min(MaxTier, empire(s, owner).technologies[static_cast<std::size_t>(techField(kind))].level / TechPerTier);
}
// Steps still open: every unit may climb to the available tier.
inline int stepsOpen(const GameState& s, const Colony& c, int kind) {
    return std::max(0, unitsOf(c, kind) * tierAvailable(s, c.ownerId, kind) - c.upgradeSteps[static_cast<std::size_t>(kind)]);
}
inline int queuedOfKind(const Colony& c, int kind) {
    int n = 0;
    for (const auto& p : c.queue)
        if (p.product == Building::Structure && p.referenceId >= 0 && isRecord(p.referenceId - 100000) && kindOfRecord(p.referenceId - 100000) == kind) ++n;
    return n;
}
// Whole-tier level of the kind at this colony (every unit has reached it), for the player text.
inline int tierOf(const Colony& c, int kind) {
    const int units = unitsOf(c, kind);
    return units > 0 ? c.upgradeSteps[static_cast<std::size_t>(kind)] / units : 0;
}
// Percent added to the output of this kind of structure (all zero when no step is applied).
inline int percent(const Colony& c, int kind) {
    const int units = unitsOf(c, kind);
    return units > 0 ? StepPercent * c.upgradeSteps[static_cast<std::size_t>(kind)] / units : 0;
}
inline bool enabled(const GameState& s) { return (s.rules.structureUpgrades || s.rules.autoUpgrades) && !s.rules.noUpgrades; }

// Empire-level reasons an upgrade project cannot run (checked again every turn the project is at the head of a queue).
inline std::string empireBlocker(const GameState& s, int owner, int recordId) {
    if (s.rules.noUpgrades) return "This game plays without structure upgrades";
    if (!s.rules.structureUpgrades && !s.rules.autoUpgrades) return "Structure upgrades are off in this ruleset";
    if (!isRecord(recordId)) return "Not an upgrade";
    const int kind = kindOfRecord(recordId);
    if (tierAvailable(s, owner, kind) < 1)
        return std::string("Needs ") + researchName(techField(kind)) + " level " + std::to_string(TechPerTier) + " to upgrade " + kindName(kind);
    return {};
}
// Colony-level reasons the project at the head of a queue cannot finish (itself excluded from the count).
inline std::string projectBlocker(const GameState& s, const Colony& c, int recordId) {
    if (auto b = empireBlocker(s, c.ownerId, recordId); !b.empty()) return b;
    const int kind = kindOfRecord(recordId);
    if (unitsOf(c, kind) <= 0) return std::string("No ") + unitName(kind) + " here to upgrade";
    if (stepsOpen(s, c, kind) <= 0)
        return std::string("Every ") + unitName(kind) + " is at the best tier your science allows (tier " + std::to_string(tierAvailable(s, c.ownerId, kind)) + ")";
    return {};
}
// The same plus the queue: one project per open step.
inline std::string blocker(const GameState& s, const Colony& c, int recordId) {
    if (auto b = projectBlocker(s, c, recordId); !b.empty()) return b;
    const int kind = kindOfRecord(recordId);
    if (queuedOfKind(c, kind) >= stepsOpen(s, c, kind)) return "An upgrade is already queued for every open step";
    return {};
}
// A finished project lifts one unit one tier when a step is open (a scrap in the meantime may have closed it).
inline bool apply(const GameState& s, Colony& c, int recordId) {
    const int kind = kindOfRecord(recordId);
    if (unitsOf(c, kind) <= 0 || stepsOpen(s, c, kind) <= 0) return false;
    ++c.upgradeSteps[static_cast<std::size_t>(kind)];
    return true;
}
// Scrapping a unit takes away its share of the upgrades: no unit keeps a step the others paid for, and nothing is refunded.
inline void onScrap(Colony& c, int kind) {
    const int before = unitsOf(c, kind) + 1; // The unit was already removed by the caller.
    auto& steps = c.upgradeSteps[static_cast<std::size_t>(kind)];
    steps = before > 0 ? steps * (before - 1) / before : 0;
    steps = std::min(steps, unitsOf(c, kind) * MaxTier);
    if (unitsOf(c, kind) <= 0) steps = 0;
}

} // namespace botf::upgrades
