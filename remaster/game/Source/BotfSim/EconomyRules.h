// BOTF Remastered: economy and rules mechanics (lane MOD-6; docs/design/ECONOMY-MECHANICS.md).
//
// Structure upgrades (StructureUpgrades.h holds the tier maths), shared empire-wide morale, the Protectorate HQ for subjugated worlds,
// dilithium on every minor homeworld, the intercept and raid orders (with the Orion and Maquis raid bonuses), crew experience with
// academies, and the exploit fixes. Header-only like EmpireRules.h; integer arithmetic only. Every rule is one GameplayRules flag and
// every flag is off in the Classic preset and in every save older than schema 23, so those games play exactly as before.
//
// Save support (schema 23): the optional section
//     ECONOMY_RULES 1 <flag bits>
//     COLONIES <n>   then n rows: <systemId> <farm steps> <factory steps> <laboratory steps> <subjugated>
//     FLEETS <n>     then n rows: <fleetId> <order> <postX> <postY> <raidSystemId> <experience>
// is written after STRUCTURES and before THREATS, only when at least one flag is on. Rows list only non-default colonies and fleets.
#pragma once

#include "Crew.h"
#include "EmpireRules.h"
#include "MinorRaces.h"
#include "ShipRoster.h"
#include "Structures.h"
#include "StructureUpgrades.h"

#include <algorithm>
#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace botf::economy_rules {

enum Order : int { OrderNone = 0, OrderIntercept = 1, OrderRaid = 2 };
inline constexpr int StartExperience = crew::StartExperience, MaxExperience = crew::MaxExperience;
inline constexpr int SubjugatedUnrest = 10;       // Morale target penalty of a subjugated world with no Protectorate HQ.
inline constexpr int RaidMoraleHit = 3;
inline constexpr int RaidBonusHullPercent = 50, RaidBonusMemberPercent = 25;

namespace detail {
inline void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
inline Colony* colonyOf(GameState& s, int systemId) {
    for (auto& c : s.colonies) if (c.systemId == systemId) return &c;
    return nullptr;
}
inline Fleet* fleetOf(GameState& s, int fleetId) {
    for (auto& f : s.fleets) if (f.id == fleetId) return &f;
    return nullptr;
}
inline EmpireState& empireRef(GameState& s, int id) {
    if (id == s.playerEmpire) return s;
    for (auto& e : s.rivals) if (e.id == id) return e;
    throw std::runtime_error("Empire does not exist");
}
inline bool knownBy(const GameState& s, int owner, int x, int y) {
    const auto& cells = empire(s, owner).explored;
    return std::binary_search(cells.begin(), cells.end(), y * s.width + x);
}
inline bool armedShip(const Fleet& f) { return f.hull > 0 && !f.colonyShip && f.shipClass != Building::Transport && fleetAttack(f) > 0; }
inline int distance(int ax, int ay, int bx, int by) { return std::max(std::abs(ax - bx), std::abs(ay - by)); }
} // namespace detail

// ---------------------------------------------------------------------------
// Flags
// ---------------------------------------------------------------------------
inline int flagBits(const GameplayRules& r) {
    return (r.structureUpgrades ? 1 : 0) | (r.autoUpgrades ? 2 : 0) | (r.noUpgrades ? 4 : 0) | (r.sharedMorale ? 8 : 0) |
        (r.protectorateHq ? 16 : 0) | (r.allDilithium ? 32 : 0) | (r.interceptOrders ? 64 : 0) | (r.raidOrders ? 128 : 0) |
        (r.raidBonus ? 256 : 0) | (r.crewExperience ? 512 : 0) | (r.exploitFixes ? 1024 : 0);
}
inline void setFlagBits(GameplayRules& r, int bits) {
    r.structureUpgrades = bits & 1; r.autoUpgrades = bits & 2; r.noUpgrades = bits & 4; r.sharedMorale = bits & 8;
    r.protectorateHq = bits & 16; r.allDilithium = bits & 32; r.interceptOrders = bits & 64; r.raidOrders = bits & 128;
    r.raidBonus = bits & 256; r.crewExperience = bits & 512; r.exploitFixes = bits & 1024;
}
inline bool anyFlag(const GameplayRules& r) { return flagBits(r) != 0; }
inline constexpr int AllFlagBits = 2047;

// Sets one flag by its preset name; false when the name is not an economy flag.
inline bool setFlagByName(GameplayRules& r, const std::string& name, bool value) {
    if (name == "structureUpgrades") r.structureUpgrades = value;
    else if (name == "autoUpgrades") r.autoUpgrades = value;
    else if (name == "noUpgrades") r.noUpgrades = value;
    else if (name == "sharedMorale") r.sharedMorale = value;
    else if (name == "protectorateHq") r.protectorateHq = value;
    else if (name == "allDilithium") r.allDilithium = value;
    else if (name == "interceptOrders") r.interceptOrders = value;
    else if (name == "raidOrders") r.raidOrders = value;
    else if (name == "raidBonus") r.raidBonus = value;
    else if (name == "crewExperience") r.crewExperience = value;
    else if (name == "exploitFixes") r.exploitFixes = value;
    else return false;
    return true;
}

// ---------------------------------------------------------------------------
// Shared morale and the Protectorate HQ (X01, X02)
// ---------------------------------------------------------------------------
// Morale every colony of the empire gains from its shared morale structures (the largest one counts; they do not stack).
inline int sharedMorale(const GameState& s, int empireId) {
    if (!s.rules.sharedMorale) return 0;
    int best = 0;
    for (const auto& c : s.colonies) {
        if (c.ownerId != empireId) continue;
        for (int id : c.structures)
            if (const auto* r = structures::record(id); r && r->bonusType == structures::SharedMorale) best = std::max(best, r->bonusValues[0]);
    }
    return best;
}
// Morale a Protectorate HQ gives the empire's subjugated worlds, or -1 when the empire has none standing.
inline int protectorateHq(const GameState& s, int empireId) {
    if (!s.rules.protectorateHq) return -1;
    int best = -1;
    for (const auto& c : s.colonies) {
        if (c.ownerId != empireId) continue;
        for (int id : c.structures)
            if (const auto* r = structures::record(id); r && r->bonusType == structures::ProtectorateHq) best = std::max(best, r->bonusValues[0]);
    }
    return best;
}
inline int subjugatedWorlds(const GameState& s, int empireId) {
    int n = 0;
    for (const auto& c : s.colonies) if (c.ownerId == empireId && c.subjugated) ++n;
    return n;
}
// Added to the colony's morale target.
inline int moraleBonus(const GameState& s, const Colony& c) {
    if (!anyFlag(s.rules)) return 0;
    int b = sharedMorale(s, c.ownerId);
    if (s.rules.protectorateHq && c.subjugated) {
        const int hq = protectorateHq(s, c.ownerId);
        b += hq >= 0 ? hq : -SubjugatedUnrest;
    }
    return b;
}
// Credits a subjugated world pays each turn once the empire holds a Protectorate HQ.
inline int tribute(const GameState& s, const Colony& c) {
    if (!s.rules.protectorateHq || !c.subjugated || protectorateHq(s, c.ownerId) < 0) return 0;
    return std::max(1, c.population / 8);
}
// ---------------------------------------------------------------------------
// Dilithium on every minor (N06)
// ---------------------------------------------------------------------------
inline bool minorHomeworld(const GameState& s, int systemId) { return s.rules.allDilithium && minorAt(s, systemId) != nullptr; }

// ---------------------------------------------------------------------------
// Crew experience (X19)
// ---------------------------------------------------------------------------
inline int crewRating(int experience) { return crew::rating(experience); }
inline const char* crewRatingName(int rating) { return crew::ratingName(rating); }
inline int attackPercent(int experience) { return crew::attackPercent(experience); }
// The academy standing in a colony: experience a turn for fleets in its system, the cap training reaches.
struct Academy { int perTurn = 0, cap = 0; };
inline Academy academyAt(const Colony& c) {
    Academy a;
    for (int id : c.structures)
        if (const auto* r = structures::record(id); r && r->bonusType == structures::Academy) {
            a.perTurn = std::max(a.perTurn, r->bonusValues[0]);
            a.cap = std::max(a.cap, r->bonusValues[1]);
        } else if (const auto* t = structures::record(id); t && t->bonusType == structures::Training) {
            // The original per-race academies (Starfleet Academy, Tactical College, Naval Academy, Central Command): the record's training
            // percent sets the pace (percent / 12, at least 1); Klingon crews may train to Legendary (100), the rest to Elite (80).
            int pct = 0;
            for (int v : t->bonusValues) pct = std::max(pct, v);
            a.perTurn = std::max(a.perTurn, std::max(1, pct / 12));
            a.cap = std::max(a.cap, c.ownerId == crew::KlingonEmpire ? crew::MaxExperience : 80);
        }
    return a;
}
// A ship launched from a colony with an academy leaves with graduates aboard (the "training fix": crews are trained at the yard, not
// only by waiting in orbit).
inline int graduateExperience(const GameState& s, const Colony& c) {
    if (!s.rules.crewExperience) return StartExperience;
    const auto a = academyAt(c);
    return std::min(std::max(StartExperience, a.cap), StartExperience + a.perTurn * 4);
}
inline void gainExperience(Fleet& f, int points, int cap = MaxExperience) {
    if (points > 0 && f.experience < cap) f.experience = std::min(cap, f.experience + points);
}
// After a turn's combat: survivors of each battle train, the winners more; every fleet in an academy system trains up to its cap.
inline void trainCrews(GameState& s, const TurnReport& report) {
    if (!s.rules.crewExperience) return;
    for (const auto& b : report.battles) {
        auto lost = [&](int id) { return std::find(b.lostFleetIds.begin(), b.lostFleetIds.end(), id) != b.lostFleetIds.end(); };
        for (const auto* ids : {&b.firstFleetIds, &b.secondFleetIds})
            for (int id : *ids) {
                if (lost(id)) continue;
                if (auto* f = detail::fleetOf(s, id)) gainExperience(*f, 4 + (f->ownerId == b.winnerOwner ? 4 : 0));
            }
    }
    for (auto& f : s.fleets) {
        if (f.hull <= 0) continue;
        for (const auto& c : s.colonies) {
            if (c.ownerId != f.ownerId) continue;
            const auto a = academyAt(c);
            if (a.perTurn <= 0) continue;
            const auto* sys = empire_rules::systemOf(s, c.systemId);
            if (sys && sys->x == f.x && sys->y == f.y) { gainExperience(f, a.perTurn, a.cap); break; }
        }
    }
}

// ---------------------------------------------------------------------------
// Intercept and raid orders (X18)
// ---------------------------------------------------------------------------
inline const char* orderName(int order) { return order == OrderIntercept ? "Intercept" : order == OrderRaid ? "Raid" : "None"; }
inline void clearOrder(Fleet& f) { f.order = OrderNone; f.postX = f.postY = 0; f.raidSystemId = 0; }

// Orion and Maquis hulls (and the minors themselves) raid better under the raidBonus rule.
inline bool raidHull(const Fleet& f) {
    const auto* rc = roster::findRosterClass(f.referenceShipId);
    if (!rc || rc->minorRaceId <= 0 || rc->minorRaceId > minors::minorRaceCount()) return false;
    const std::string name = minors::Roster[static_cast<std::size_t>(rc->minorRaceId - 1)].name;
    return name.find("Orion") != std::string::npos || name.find("Maquis") != std::string::npos;
}
inline int raidMembers(const GameState& s, int empireId) {
    int n = 0;
    for (const auto& m : s.minors)
        if (m.memberOf == empireId && (m.name.find("Orion") != std::string::npos || m.name.find("Maquis") != std::string::npos)) ++n;
    return n;
}
// Percent the raider's plunder is multiplied by (100 = no bonus).
inline int raidPercent(const GameState& s, const Fleet& f) {
    if (!s.rules.raidBonus) return 100;
    return 100 + (raidHull(f) ? RaidBonusHullPercent : 0) + raidMembers(s, f.ownerId) * RaidBonusMemberPercent;
}

// What a defended colony does to a raider: nothing steals from a colony that has powered batteries, a station, or a hostile warship in
// its sector.
inline bool raidDefended(const GameState& s, const Fleet& raider, const Colony& c) {
    if (s.advancedRulesEnabled && economy(s, c).poweredBatteries > 0) return true;
    for (const auto& other : s.fleets)
        if (other.hull > 0 && other.ownerId == c.ownerId && other.x == raider.x && other.y == raider.y && fleetAttack(other) > 0 &&
            other.shipClass != Building::Transport && !other.colonyShip) return true;
    if (const auto* st = stationAt(s, raider.x, raider.y); st && st->ownerId == c.ownerId) return true;
    return false;
}
// The plunder of one turn before the raid bonus and the victim's purse cap.
inline int raidLoot(const GameState& s, const Colony& c) {
    int loot = c.population / 4 + 3 * c.factories + 5;
    for (const auto& route : s.tradeRoutes)
        if (route.ownerId == c.ownerId && (route.fromSystemId == c.systemId || route.toSystemId == c.systemId))
            loot += tradeRouteRevenue(s, route) / 2;
    return loot;
}

inline std::string orderBlocker(const GameState& s, const Fleet& f, int order, int systemId) {
    if (order == OrderNone) return {};
    if (!detail::armedShip(f)) return "Only armed warships take this order";
    if (f.taskForceId > 0) return "Take the ship out of its task force first";
    if (f.buildingOutpost || f.terraforming) return "The ship is busy";
    if (order == OrderIntercept) return s.rules.interceptOrders ? std::string() : std::string("Intercept orders are off in this ruleset");
    if (order != OrderRaid) return "Unknown order";
    if (!s.rules.raidOrders) return "Raid orders are off in this ruleset";
    const auto* colony = colonyAt(s, systemId);
    if (!colony || colony->ownerId == f.ownerId) return "Raid an enemy colony";
    if (!combatAllowed(s, f.ownerId, colony->ownerId)) return "Declare war before raiding";
    const auto* sys = empire_rules::systemOf(s, systemId);
    if (!sys || !detail::knownBy(s, f.ownerId, sys->x, sys->y)) return "That system is not charted";
    if (route(s, f, sys->x, sys->y).empty() && !(f.x == sys->x && f.y == sys->y)) return "Target is outside supplied range";
    return {};
}

// The player's SetFleetOrder: fleetId, eventOption 0 clear / 1 intercept / 2 raid, systemId the raid target.
inline void applyOrderCommand(GameState& s, const Command& cmd) {
    auto* f = detail::fleetOf(s, cmd.fleetId);
    detail::require(f != nullptr && f->ownerId == s.playerEmpire, "Fleet is not owned");
    if (cmd.eventOption != OrderRaid) detail::require(cmd.systemId == 0, "Only a raid names a target colony");
    if (const auto problem = orderBlocker(s, *f, cmd.eventOption, cmd.systemId); !problem.empty()) throw std::runtime_error(problem);
    f->attackTarget = 0; f->retreat = false;
    if (cmd.eventOption == OrderNone) { clearOrder(*f); f->targetX = f->x; f->targetY = f->y; return; }
    clearOrder(*f);
    f->order = cmd.eventOption;
    if (cmd.eventOption == OrderIntercept) { f->postX = f->x; f->postY = f->y; f->targetX = f->x; f->targetY = f->y; }
    else {
        f->raidSystemId = cmd.systemId;
        const auto* sys = empire_rules::systemOf(s, cmd.systemId);
        f->targetX = sys->x; f->targetY = sys->y;
    }
}

// Start of the movement phase: an interceptor goes after the nearest hostile ship inside its radius of the post (and comes home when
// none is), a raider heads for its colony. An order whose reason is gone is cleared.
inline void beforeMovement(GameState& s, TurnReport& report) {
    for (auto& f : s.fleets) {
        if (f.order == OrderNone) continue;
        const bool mine = f.ownerId == s.playerEmpire;
        auto drop = [&](const std::string& why) {
            if (mine) report.events.push_back(fleetName(f) + " #" + std::to_string(f.id) + ": " + orderName(f.order) + " order ended. " + why);
            clearOrder(f);
        };
        if (f.hull <= 0 || !detail::armedShip(f)) { clearOrder(f); continue; }
        if (fleetMaxHull(f) > 0 && f.hull * 2 < fleetMaxHull(f)) { drop("The ship is badly damaged."); f.attackTarget = 0; continue; }
        if (f.order == OrderRaid) {
            const auto* colony = colonyAt(s, f.raidSystemId);
            if (!colony || colony->ownerId == f.ownerId || !combatAllowed(s, f.ownerId, colony->ownerId)) { drop("The target is no longer an enemy colony."); continue; }
            const auto* sys = empire_rules::systemOf(s, f.raidSystemId);
            f.attackTarget = 0; f.targetX = sys->x; f.targetY = sys->y;
            continue;
        }
        // Intercept.
        const int radius = std::min(4, 1 + shipSpeed(s, f));
        const Fleet* best = nullptr; int bestDistance = 1000;
        for (const auto& other : s.fleets) {
            if (other.hull <= 0 || other.ownerId == f.ownerId || other.ownerId == ThreatOwner || !combatAllowed(s, f.ownerId, other.ownerId)) continue;
            if (!visible(s, f.ownerId, other.x, other.y)) continue;
            const int toPost = detail::distance(f.postX, f.postY, other.x, other.y);
            if (toPost > radius) continue;
            const int toShip = detail::distance(f.x, f.y, other.x, other.y);
            if (toShip < bestDistance || (toShip == bestDistance && best && other.id < best->id)) { bestDistance = toShip; best = &other; }
        }
        if (best && !route(s, f, best->x, best->y).empty()) { f.attackTarget = best->id; f.targetX = best->x; f.targetY = best->y; f.retreat = false; }
        else { f.attackTarget = 0; f.targetX = f.postX; f.targetY = f.postY; }
    }
}

// After combat and invasions: raiders that reached an undefended enemy colony plunder it. The purse moves, it is not created.
inline void resolveRaids(GameState& s, TurnReport& report) {
    if (!s.rules.raidOrders) return;
    for (auto& f : s.fleets) {
        if (f.order != OrderRaid || f.hull <= 0) continue;
        auto* colony = detail::colonyOf(s, f.raidSystemId);
        if (!colony || colony->ownerId == f.ownerId || !combatAllowed(s, f.ownerId, colony->ownerId)) continue;
        const auto* sys = empire_rules::systemOf(s, f.raidSystemId);
        if (!sys || sys->x != f.x || sys->y != f.y) continue;
        const bool involved = f.ownerId == s.playerEmpire || colony->ownerId == s.playerEmpire;
        const std::string where = sys->name;
        if (raidDefended(s, f, *colony)) {
            if (involved) report.events.push_back(std::string("Raid on ") + where + " repelled: the colony's defences hold.");
            continue;
        }
        auto& victim = detail::empireRef(s, colony->ownerId);
        auto& raider = detail::empireRef(s, f.ownerId);
        int loot = raidLoot(s, *colony);
        loot = static_cast<int>(static_cast<std::int64_t>(loot) * raidPercent(s, f) / 100);
        loot = static_cast<int>(std::min<std::int64_t>(loot, std::max<std::int64_t>(0, victim.credits)));
        victim.credits -= loot; raider.credits += loot;
        colony->morale = std::max(0, colony->morale - RaidMoraleHit);
        if (s.rules.crewExperience) gainExperience(f, 2);
        if (involved)
            report.events.push_back(f.ownerId == s.playerEmpire ? fleetName(f) + " raided " + where + ": " + std::to_string(loot) + " credits taken."
                : std::string(empireName(f.ownerId)) + " raiders plundered " + where + ": " + std::to_string(loot) + " credits lost.");
    }
}

// AI: at war, every third warship holds an intercept post at a home colony and every third (and any Orion or Maquis hull) raids the
// nearest known enemy colony that has no powered batteries. Deterministic: fleet id, then system id.
inline void planOrders(GameState& s, int owner, bool atWar) {
    if (!s.rules.interceptOrders && !s.rules.raidOrders) return;
    if (!atWar) return;
    for (auto& f : s.fleets) {
        if (f.ownerId != owner || f.order != OrderNone || f.taskForceId > 0 || f.colonize || f.retreat || !detail::armedShip(f)) continue;
        const int max = fleetMaxHull(f);
        if (max <= 0 || f.hull * 4 < max * 3) continue;
        if (s.rules.raidOrders && (f.id % 3 == 1 || (s.rules.raidBonus && raidHull(f)))) {
            const Colony* best = nullptr; int bestDistance = 1000;
            for (const auto& c : s.colonies) {
                if (c.ownerId == owner || !combatAllowed(s, owner, c.ownerId)) continue;
                const auto* sys = empire_rules::systemOf(s, c.systemId);
                if (!sys || !detail::knownBy(s, owner, sys->x, sys->y)) continue;
                if (s.advancedRulesEnabled && economy(s, c).poweredBatteries > 0) continue;
                const int d = detail::distance(f.x, f.y, sys->x, sys->y);
                if (d >= bestDistance || route(s, f, sys->x, sys->y).empty()) continue;
                bestDistance = d; best = &c;
            }
            if (best) {
                const auto* sys = empire_rules::systemOf(s, best->systemId);
                f.order = OrderRaid; f.raidSystemId = best->systemId; f.targetX = sys->x; f.targetY = sys->y; f.attackTarget = 0;
                continue;
            }
        }
        if (s.rules.interceptOrders && f.id % 3 == 0) {
            for (const auto& c : s.colonies) {
                if (c.ownerId != owner) continue;
                const auto* sys = empire_rules::systemOf(s, c.systemId);
                if (sys && sys->x == f.x && sys->y == f.y) { f.order = OrderIntercept; f.postX = f.x; f.postY = f.y; f.attackTarget = 0; break; }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Structure upgrades: the automatic variant and the AI (E10, X08)
// ---------------------------------------------------------------------------
// End of turn: with autoUpgrades a colony with an empty queue queues the cheaper automatic upgrade for its first open step.
inline void autoUpgrade(GameState& s) {
    if (!s.rules.autoUpgrades || s.rules.noUpgrades) return;
    for (auto& c : s.colonies) {
        if (!c.queue.empty() || c.queue.size() >= 8) continue;
        for (int kind = 0; kind < upgrades::Kinds; ++kind) {
            const int record = upgrades::recordFor(kind, true);
            if (!upgrades::blocker(s, c, record).empty()) continue;
            c.queue.emplace_back(Building::Structure, 0, structures::refFor(record));
            break;
        }
    }
}
// AI (manual upgrades only): the record id of the first upgrade the colony may queue, or -1.
inline int aiUpgrade(const GameState& s, const Colony& c) {
    if (!s.rules.structureUpgrades || s.rules.autoUpgrades || s.rules.noUpgrades) return -1;
    if (c.shipyards == 0 || empire(s, c.ownerId).credits < 120) return -1;
    for (int kind = 0; kind < upgrades::Kinds; ++kind) {
        const int record = upgrades::recordFor(kind, false);
        if (upgrades::blocker(s, c, record).empty()) return record;
    }
    return -1;
}
// A finished upgrade project: lifts a unit one tier. Returns the player-facing text.
inline std::string finishUpgrade(GameState& s, Colony& c, int referenceId) {
    const int record = structures::recordIdOf(referenceId);
    const bool ok = upgrades::apply(s, c, record);
    return ok ? std::string(upgrades::name(record)) + " finished at " + std::to_string(c.systemId) + "."
              : std::string(upgrades::name(record)) + " at " + std::to_string(c.systemId) + " had nothing left to upgrade.";
}

// ---------------------------------------------------------------------------
// Terraform cap (X09)
// ---------------------------------------------------------------------------
// Most work any number of colony ships can add to one planet in one turn: two turns at least, 300 at most.
inline int terraformCap(const Planet& p) { return std::max(1, std::min(300, (p.terraformNeeded + 1) / 2)); }

// ---------------------------------------------------------------------------
// Aftermath: Subjugate marks the world for the Protectorate
// ---------------------------------------------------------------------------
inline void onSubjugated(GameState& s, int systemId) {
    if (!s.rules.protectorateHq) return;
    if (auto* c = detail::colonyOf(s, systemId)) c->subjugated = true;
}

// ---------------------------------------------------------------------------
// Validation and save
// ---------------------------------------------------------------------------
inline void validate(const GameState& s) {
    const auto& r = s.rules;
    detail::require(!(r.noUpgrades && r.autoUpgrades), "noUpgrades excludes autoUpgrades");
    detail::require(!r.raidBonus || r.raidOrders, "raidBonus needs raidOrders");
    const bool on = anyFlag(r);
    for (const auto& c : s.colonies) {
        for (int k = 0; k < upgrades::Kinds; ++k) {
            const int steps = c.upgradeSteps[static_cast<std::size_t>(k)];
            detail::require(steps >= 0 && steps <= upgrades::unitsOf(c, k) * upgrades::MaxTier, "Upgrade steps out of bounds");
            detail::require(steps == 0 || r.structureUpgrades || r.autoUpgrades || r.noUpgrades, "Upgrade state without upgrade rules");
        }
        detail::require(!c.subjugated || r.protectorateHq, "Subjugated world without the Protectorate rule");
        for (const auto& p : c.queue)
            if (p.product == Building::Structure && upgrades::isRecord(structures::recordIdOf(p.referenceId)))
                detail::require(r.structureUpgrades || r.autoUpgrades, "Upgrade project without upgrade rules");
    }
    for (const auto& f : s.fleets) {
        detail::require(f.order >= 0 && f.order <= OrderRaid && f.experience >= 0 && f.experience <= MaxExperience, "Fleet order or experience out of bounds");
        if (f.order == OrderIntercept) detail::require(r.interceptOrders && f.postX >= 0 && f.postY >= 0 && f.postX < s.width && f.postY < s.height, "Invalid intercept post");
        if (f.order == OrderRaid) detail::require(r.raidOrders && f.raidSystemId > 0, "Invalid raid order");
        if (f.order == OrderNone) detail::require(f.raidSystemId == 0 && f.postX == 0 && f.postY == 0, "Order fields without an order");
        detail::require(r.crewExperience || f.experience == StartExperience, "Experience without crew experience");
    }
    (void)on;
}

inline void writeSection(std::ostream& out, const GameState& s) {
    if (!anyFlag(s.rules)) return;
    out << "ECONOMY_RULES 1 " << flagBits(s.rules) << '\n';
    auto colonyRow = [](const Colony& c) { return c.upgradeSteps[0] || c.upgradeSteps[1] || c.upgradeSteps[2] || c.subjugated; };
    out << "COLONIES " << std::count_if(s.colonies.begin(), s.colonies.end(), colonyRow) << '\n';
    for (const auto& c : s.colonies)
        if (colonyRow(c)) out << c.systemId << ' ' << c.upgradeSteps[0] << ' ' << c.upgradeSteps[1] << ' ' << c.upgradeSteps[2] << ' ' << (c.subjugated ? 1 : 0) << '\n';
    auto fleetRow = [](const Fleet& f) { return f.order != 0 || f.experience != StartExperience; };
    out << "FLEETS " << std::count_if(s.fleets.begin(), s.fleets.end(), fleetRow) << '\n';
    for (const auto& f : s.fleets)
        if (fleetRow(f)) out << f.id << ' ' << f.order << ' ' << f.postX << ' ' << f.postY << ' ' << f.raidSystemId << ' ' << f.experience << '\n';
}
// Reads the section body after the ECONOMY_RULES tag.
inline void readSection(std::istream& in, GameState& s) {
    int revision = 0, bits = -1;
    detail::require(static_cast<bool>(in >> revision >> bits) && revision == 1 && bits > 0 && bits <= AllFlagBits, "Malformed ECONOMY_RULES header");
    setFlagBits(s.rules, bits);
    std::string tag; int n = -1;
    auto header = [&](const char* expected) {
        detail::require(static_cast<bool>(in >> tag >> n) && tag == expected && n >= 0 && n <= 100000, "Malformed ECONOMY_RULES section");
    };
    header("COLONIES");
    for (int i = 0; i < n; ++i) {
        int systemId = 0, a = 0, b = 0, c = 0, sub = 0;
        detail::require(static_cast<bool>(in >> systemId >> a >> b >> c >> sub) && (sub == 0 || sub == 1), "Malformed ECONOMY_RULES colony row");
        auto* colony = detail::colonyOf(s, systemId);
        detail::require(colony != nullptr, "ECONOMY_RULES row names an unknown colony");
        colony->upgradeSteps = {a, b, c}; colony->subjugated = sub == 1;
    }
    header("FLEETS");
    for (int i = 0; i < n; ++i) {
        int id = 0, order = 0, px = 0, py = 0, raid = 0, exp = 0;
        detail::require(static_cast<bool>(in >> id >> order >> px >> py >> raid >> exp), "Malformed ECONOMY_RULES fleet row");
        auto* f = detail::fleetOf(s, id);
        detail::require(f != nullptr, "ECONOMY_RULES row names an unknown fleet");
        f->order = order; f->postX = px; f->postY = py; f->raidSystemId = raid; f->experience = exp;
    }
}

} // namespace botf::economy_rules
