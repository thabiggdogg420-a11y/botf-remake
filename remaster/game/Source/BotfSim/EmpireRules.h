// BOTF Remastered: empire rules (lane M2-EMPIRE; docs/design/EMPIRE-RULES.md).
//
// Dilithium ship slots, emergency programs, invasion aftermath, independent-minor conquest and minor guard ships.
// Header-only like MinorRaces.h, so the web, Godot and Unreal builds need no new source file. Integer arithmetic only.
// Everything is gated by GameplayRules::empireRules; schema-14 and older saves and generateCampaign*() keep it off.
//
// Save support (schema 15): the optional section
//     EMPIRE_RULES 1
//     COLONIES <n>        then n rows: <systemId> <refineries> <program> <programTurns>
//     AFTERMATHS <n>      then n rows: <systemId> <conqueror> <formerOwner> <turn> <populationBefore>
//     GUARDS <n>          then n rows: <systemId> <shipId> <hull> <shields>
// is written after TACTICAL_WEGO and before COMBAT_ORDERS, only when rules.empireRules is set.
#pragma once

#include "GameplayInternal.h"
#include "MinorRaces.h"
#include "ReferenceCatalogue.h"
#include "Simulation.h"

#include <algorithm>
#include <cstdint>
#include <istream>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace botf::empire_rules {

// ---- Constants (design numbers, see EMPIRE-RULES.md) ----
inline constexpr int RefineryDepositPercent = 45;    // Share of systems with a dilithium deposit.
inline constexpr int KlingonYardBonusPercent = 25;   // Klingon ship industry in a colony with a powered refinery.
inline constexpr int MaxProgramTurns = 10;
inline constexpr int ProgramMoraleLimit = 70;        // A program starts only while colony morale is below this.
inline constexpr int AftermathLapseTurns = 3;

enum class Program : int { None = 0, MartialLaw = 1, PoliceState = 2, Tribunal = 3, Inquisition = 4, FestivalOfFun = 5 };
enum class Aftermath : int { Liberate = 0, Subjugate = 1, Exterminate = 2 };

namespace detail {
inline void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
inline std::uint64_t mix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
inline Colony* colonyOf(GameState& s, int systemId) {
    for (auto& c : s.colonies) if (c.systemId == systemId) return &c;
    return nullptr;
}
} // namespace detail

inline bool enabled(const GameState& s) { return s.rules.empireRules; }

// ---------------------------------------------------------------------------
// (b) Dilithium
// ---------------------------------------------------------------------------
// A system has a dilithium deposit (refinery site) by a seeded hash; a colony that already has a refinery always does.
inline bool hasDeposit(const GameState& s, int systemId) {
    for (const auto& c : s.colonies) if (c.systemId == systemId && c.refineries > 0) return true;
    return static_cast<int>(detail::mix(s.seed ^ 0x646c697468697561ULL ^ static_cast<std::uint64_t>(systemId)) % 100) < RefineryDepositPercent;
}

struct DilithiumStatus {
    bool enforced = false;
    int refineries = 0, powered = 0;  // Built / with power.
    int slots = 0, used = 0;          // Ship construction slots and how many are in use this turn.
    bool operator==(const DilithiumStatus&) const = default;
};

// True when the colony is building a ship it can actually progress this turn (powered yard, industry, ship at the head).
inline bool buildingShip(const GameState& s, const Colony& c) {
    if (c.queue.empty() || !isShip(c.queue.front())) return false;
    const auto e = economy(s, c);
    return e.shipyardPowered && e.industry > 0;
}

inline DilithiumStatus dilithiumStatus(const GameState& s, int empireId) {
    DilithiumStatus d;
    d.enforced = enabled(s);
    for (const auto& c : s.colonies) {
        if (c.ownerId != empireId) continue;
        d.refineries += c.refineries;
        if (c.refineries > 0 && economy(s, c).refineryPowered) ++d.powered;
        if (buildingShip(s, c)) ++d.used;
    }
    d.slots = d.enforced ? d.powered : 1; // Before schema 15 an empire had exactly one slot.
    return d;
}

// Non-empty when the colony's head-of-queue ship has no free dilithium slot. Slots go to colonies in system-id order.
inline std::string slotBlocker(const GameState& s, const Colony& c) {
    if (!enabled(s)) return {};
    const int powered = dilithiumStatus(s, c.ownerId).powered;
    if (powered == 0) return "No powered dilithium refinery: ships cannot be built";
    int earlier = 0;
    for (const auto& other : s.colonies)
        if (other.ownerId == c.ownerId && other.systemId < c.systemId && buildingShip(s, other)) ++earlier;
    if (earlier >= powered)
        return "Empire shipbuilding slots all in use (" + std::to_string(powered) + " of " + std::to_string(powered) + "); build another dilithium refinery";
    return {};
}

// Percent added to the colony's industry while it builds a ship (Klingon yards with a powered refinery).
inline int shipIndustryBonusPercent(const GameState& s, const Colony& c) {
    if (!enabled(s) || c.ownerId != 2 || c.refineries == 0) return 0;
    return economy(s, c).refineryPowered ? KlingonYardBonusPercent : 0;
}

inline std::string refineryBlocker(const GameState& s, const Colony& c) {
    if (!enabled(s)) return "Dilithium refineries need a campaign with empire rules";
    if (c.refineries > 0) return "Dilithium refinery already built";
    for (const auto& p : c.queue) if (p.product == Building::DilithiumRefinery) return "Dilithium refinery already queued";
    if (!hasDeposit(s, c.systemId)) return "No dilithium deposit in this system";
    return {};
}

// AI: build a refinery on a deposit colony while slots lag behind the empire's size.
inline bool aiWantsRefinery(const GameState& s, int owner, const Colony& c) {
    if (!enabled(s) || c.shipyards == 0 || !refineryBlocker(s, c).empty()) return false;
    int colonies = 0, refineries = 0;
    for (const auto& o : s.colonies) {
        if (o.ownerId != owner) continue;
        ++colonies;
        refineries += o.refineries;
        for (const auto& p : o.queue) if (p.product == Building::DilithiumRefinery) ++refineries;
    }
    return refineries < std::min(3, colonies / 2 + 1);
}

// ---------------------------------------------------------------------------
// New game, validation, save
// ---------------------------------------------------------------------------
// Turn-0 setup when rules.empireRules is on: each empire's most populous colony gets a powered refinery.
inline void initialize(GameState& s) {
    std::set<int> done;
    std::vector<Colony*> best;
    for (auto& c : s.colonies) {
        auto it = std::find_if(best.begin(), best.end(), [&](Colony* o) { return o->ownerId == c.ownerId; });
        if (it == best.end()) best.push_back(&c);
        else if (c.population > (*it)->population) *it = &c;
    }
    for (auto* c : best) if (c->refineries == 0) { c->refineries = 1; c->energy += 5; }
}

inline void validate(const GameState& s) {
    if (!enabled(s)) {
        for (const auto& c : s.colonies)
            detail::require(c.refineries == 0 && c.program == 0 && c.programTurns == 0, "Empire-rule colony state without empire rules");
        detail::require(s.aftermaths.empty() && s.minorGuards.empty(), "Empire-rule state without empire rules");
        return;
    }
    for (const auto& c : s.colonies) {
        detail::require(c.refineries >= 0 && c.refineries <= 1, "Refinery count out of bounds");
        detail::require(c.program >= 0 && c.program <= 5 && c.programTurns >= 0 && c.programTurns <= MaxProgramTurns &&
            (c.program != 0 || c.programTurns == 0), "Invalid emergency program state");
    }
}

inline void writeSection(std::ostream& out, const GameState& s) {
    if (!enabled(s)) return;
    out << "EMPIRE_RULES 1\n";
    const auto rows = std::count_if(s.colonies.begin(), s.colonies.end(), [](const Colony& c) {
        return c.refineries || c.program || c.programTurns; });
    out << "COLONIES " << rows << '\n';
    for (const auto& c : s.colonies)
        if (c.refineries || c.program || c.programTurns) out << c.systemId << ' ' << c.refineries << ' ' << c.program << ' ' << c.programTurns << '\n';
    out << "AFTERMATHS " << s.aftermaths.size() << '\n';
    for (const auto& a : s.aftermaths)
        out << a.systemId << ' ' << a.conqueror << ' ' << a.formerOwner << ' ' << a.turn << ' ' << a.populationBefore << '\n';
    out << "GUARDS " << s.minorGuards.size() << '\n';
    for (const auto& g : s.minorGuards) out << g.systemId << ' ' << g.shipId << ' ' << g.hull << ' ' << g.shields << '\n';
}

// Reads the section body after the EMPIRE_RULES tag.
inline void readSection(std::istream& in, GameState& s) {
    int revision = 0;
    detail::require(static_cast<bool>(in >> revision) && revision == 1, "Unsupported empire rules revision");
    s.rules.empireRules = true;
    std::string tag; int n = -1;
    auto header = [&](const char* expected) {
        detail::require(static_cast<bool>(in >> tag >> n) && tag == expected && n >= 0 && n <= 100000, "Malformed EMPIRE_RULES section");
    };
    header("COLONIES");
    for (int i = 0; i < n; ++i) {
        int systemId = 0, refineries = 0, program = 0, turns = 0;
        detail::require(static_cast<bool>(in >> systemId >> refineries >> program >> turns), "Malformed EMPIRE_RULES colony row");
        auto* c = detail::colonyOf(s, systemId);
        detail::require(c != nullptr, "EMPIRE_RULES row names an unknown colony");
        c->refineries = refineries; c->program = program; c->programTurns = turns;
    }
    header("AFTERMATHS");
    for (int i = 0; i < n; ++i) {
        AftermathChoice a;
        detail::require(static_cast<bool>(in >> a.systemId >> a.conqueror >> a.formerOwner >> a.turn >> a.populationBefore),
            "Malformed EMPIRE_RULES aftermath row");
        s.aftermaths.push_back(a);
    }
    header("GUARDS");
    for (int i = 0; i < n; ++i) {
        MinorGuard g;
        detail::require(static_cast<bool>(in >> g.systemId >> g.shipId >> g.hull >> g.shields), "Malformed EMPIRE_RULES guard row");
        s.minorGuards.push_back(g);
    }
}

} // namespace botf::empire_rules
