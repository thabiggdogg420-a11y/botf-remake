// Schema-13 outposts and starbases: construction by troop transports, starbase
// upgrades funded by a colony, supply/sensor extension, upkeep, repairs,
// tactical combat as immobile units, destruction and capture, and rival AI use.
#include "GameplayInternal.h"
#include "TacticalCombat.h"

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>

namespace botf::gameplay {
constexpr int StationLimit = 128;
constexpr int StationLimitPerEmpire = 12;
constexpr int OutpostBuildTurnsMax = 3;
constexpr int StationUpgradeRange = 6;           // Funding colony within this many sectors.
constexpr int StationUpgradeConstructionLevel = 2;

Station* mutableStation(GameState& s, int id) {
    const auto it = std::find_if(s.stations.begin(), s.stations.end(), [id](const Station& st) { return st.id == id; });
    return it == s.stations.end() ? nullptr : &*it;
}
int stationsOwnedOrBuilding(const GameState& s, int owner) {
    int total = 0;
    for (const auto& st : s.stations) if (st.ownerId == owner) ++total;
    for (const auto& f : s.fleets) if (f.ownerId == owner && f.buildingOutpost) ++total;
    return total;
}
int totalStationsAndBuilds(const GameState& s) {
    int total = static_cast<int>(s.stations.size());
    for (const auto& f : s.fleets) if (f.buildingOutpost) ++total;
    return total;
}
// The best catalogue station record (role 6 outpost, role 7 starbase) of the
// owner's race: the upgraded "II" design once its technology is met.
const referenceData::ShipRecord* catalogueStation(const GameState& s, int owner, StationType type) {
    const int race = internal::catalogueRace(owner);
    const int role = type == StationType::Outpost ? 6 : 7;
    const referenceData::ShipRecord* basic = nullptr;
    const referenceData::ShipRecord* best = nullptr;
    const auto& tech = empire(s, owner).technologies;
    for (const auto& ship : referenceData::ShipCatalogue) {
        if (ship.raceId != race || ship.role != role) continue;
        if (!basic || ship.industryCost < basic->industryCost) basic = &ship;
        bool met = true;
        for (int field = 0; field < ResearchFields; ++field) met &= tech[field].level >= ship.technologyRequirements[field];
        if (met && (!best || ship.industryCost > best->industryCost)) best = &ship;
    }
    return best ? best : basic;
}
void noteStation(TurnReport& report, const Station& st, const std::string& text, int otherEmpire = 0) {
    ChronicleEntry e;
    e.category = ChronicleCategory::Stations;
    e.text = text;
    e.x = st.x; e.y = st.y;
    e.stationId = st.id;
    e.empireId = st.ownerId;
    e.otherEmpireId = otherEmpire;
    note(report, std::move(e));
}
bool hostileArmedPresent(const GameState& s, int owner, int x, int y) {
    return std::any_of(s.fleets.begin(), s.fleets.end(), [&](const Fleet& f) {
        return f.hull > 0 && f.x == x && f.y == y && empiresAtWar(s, owner, f.ownerId);
    });
}

// ---------------------------------------------------------------------------
// Validation and commands
// ---------------------------------------------------------------------------
void validateStations(const GameState& s) {
    const bool enabled = s.rules.stations;
    require(s.nextStationId >= 1 && s.nextStationId <= 1000000, "Invalid next station ID");
    require(s.stations.size() <= StationLimit, "Too many stations");
    require(enabled || (s.stations.empty() && s.stationUpgrades.empty()), "Stations are disabled by the ruleset");
    const auto empires = internal::activeEmpires(s);
    int prior = 0;
    std::set<int> cells;
    std::map<int, int> perOwner;
    for (const auto& st : s.stations) {
        require(st.id > prior && st.id < s.nextStationId, "Station IDs must be unique, positive and sorted");
        prior = st.id;
        require(std::binary_search(empires.begin(), empires.end(), st.ownerId), "Station owner is not a campaign empire");
        require(st.x >= 0 && st.x < s.width && st.y >= 0 && st.y < s.height, "Station outside galaxy");
        require(st.type == StationType::Outpost || st.type == StationType::Starbase, "Invalid station type");
        require(cells.insert(st.y * s.width + st.x).second, "Only one station may occupy a sector");
        require(internal::known(s, st.ownerId, st.x, st.y), "Station sector must be explored by its owner");
        require(sectorPassable(s, st.x, st.y), "Station inside an impassable anomaly");
        require(st.builtTurn >= 0 && st.builtTurn <= s.turn, "Invalid station construction turn");
        const auto stats = stationStats(s, st);
        require(st.hull >= 1 && st.hull <= stats.maxHull && st.shields >= 0 && st.shields <= stats.maxShields,
            "Station integrity out of bounds");
        require(++perOwner[st.ownerId] <= 32, "Too many stations for one empire");
    }
    std::set<int> upgradedStations, fundingColonies;
    for (const auto& u : s.stationUpgrades) {
        const auto station = stationById(s, u.stationId);
        require(station && station->type == StationType::Outpost, "Starbase upgrade needs an outpost");
        require(upgradedStations.insert(u.stationId).second && fundingColonies.insert(u.fundingSystemId).second,
            "Duplicate starbase upgrade or funding colony");
        const auto colony = colonyAt(s, u.fundingSystemId);
        require(colony && colony->ownerId == station->ownerId, "Starbase upgrade funding colony not owned");
        require(u.work >= 0 && u.work < stationUpgradeCost(s, station->ownerId).industry, "Starbase upgrade work out of bounds");
    }
    require(std::is_sorted(s.stationUpgrades.begin(), s.stationUpgrades.end(), [](const StationUpgrade& a, const StationUpgrade& b) {
        return a.stationId < b.stationId;
    }), "Starbase upgrades must be sorted");
    for (const auto& f : s.fleets) {
        if (!f.buildingOutpost) { require(f.outpostProgress == 0, "Idle fleet has outpost progress"); continue; }
        require(enabled && f.shipClass == Building::Transport, "Only troop transports assemble outposts");
        require(f.outpostProgress >= 0 && f.outpostProgress < OutpostBuildTurnsMax, "Outpost progress out of bounds");
    }
}

void applyStationCommand(GameState& state, const Command& command, std::set<int>& orderedFleets) {
    if (command.type == CommandType::BuildOutpost) {
        require(orderedFleets.insert(command.fleetId).second, "Cancel existing fleet draft before replacing it");
        const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& f) { return f.id == command.fleetId; });
        require(fleet != state.fleets.end() && fleet->ownerId == state.playerEmpire, "Fleet is not owned");
        const auto blocker = outpostBlocker(state, command.fleetId);
        require(blocker.empty(), blocker);
        internal::detachFromTaskForce(state, *fleet);
        fleet->buildingOutpost = true;
        fleet->outpostProgress = 0;
        fleet->targetX = fleet->x; fleet->targetY = fleet->y;
        fleet->attackTarget = 0; fleet->retreat = false; fleet->terraforming = false;
        return;
    }
    const auto station = stationById(state, command.stationId);
    require(station && station->ownerId == state.playerEmpire, "Station is not owned");
    const auto blocker = stationUpgradeBlocker(state, command.stationId, command.systemId);
    require(blocker.empty(), blocker);
    internal::mutableEmpire(state, state.playerEmpire).credits -= stationUpgradeCost(state, state.playerEmpire).credits;
    state.stationUpgrades.push_back({command.stationId, command.systemId, 0});
    std::sort(state.stationUpgrades.begin(), state.stationUpgrades.end(), [](const StationUpgrade& a, const StationUpgrade& b) {
        return a.stationId < b.stationId;
    });
}

// ---------------------------------------------------------------------------
// Turn phases
// ---------------------------------------------------------------------------
void chargeStationUpkeep(GameState& next, TurnReport& report) {
    for (const auto& st : next.stations) {
        const int upkeep = stationStats(next, st).upkeep;
        internal::mutableEmpire(next, st.ownerId).credits -= upkeep;
        if (st.ownerId == next.playerEmpire) {
            report.fleetUpkeep += upkeep; // Military upkeep includes stations.
            report.ledger.stationUpkeep += upkeep;
        }
    }
}

int divertIndustry(GameState& next, const Colony& colony, int industry, TurnReport& report) {
    if (industry <= 0) return 0;
    for (auto& u : next.stationUpgrades) {
        if (u.fundingSystemId != colony.systemId) continue;
        const auto station = stationById(next, u.stationId);
        if (!station || station->ownerId != colony.ownerId) return 0;
        const int cost = stationUpgradeCost(next, colony.ownerId).industry;
        const int used = std::clamp(cost - u.work, 0, industry);
        u.work += used;
        if (colony.ownerId == next.playerEmpire && used > 0) {
            ChronicleEntry e;
            e.category = ChronicleCategory::Stations;
            e.text = "System " + std::to_string(colony.systemId) + ": " + std::to_string(used) +
                " industry to the starbase at " + sectorText(station->x, station->y) + " (" + std::to_string(u.work) + "/" +
                std::to_string(cost) + ").";
            e.systemId = colony.systemId; e.stationId = station->id; e.x = station->x; e.y = station->y;
            e.empireId = colony.ownerId;
            note(report, std::move(e));
        }
        return used;
    }
    for (auto& m : next.megaprojects) {
        if (m.systemId != colony.systemId || m.ownerId != colony.ownerId) continue;
        const int cost = megaprojectIndustryCost(next);
        const int used = std::clamp(std::min(cost - m.work, megaprojectWorkPerTurn(next, colony.ownerId)), 0, industry);
        m.work += used;
        if (colony.ownerId == next.playerEmpire && used > 0) {
            ChronicleEntry e;
            e.category = ChronicleCategory::Victory;
            e.text = "System " + std::to_string(colony.systemId) + ": " + std::to_string(used) + " industry to the " +
                megaprojectName(next) + " (" + std::to_string(m.work) + "/" + std::to_string(cost) + ").";
            e.systemId = colony.systemId; e.empireId = colony.ownerId;
            if (const auto sys = systemById(next, colony.systemId)) { e.x = sys->x; e.y = sys->y; }
            note(report, std::move(e));
        }
        return used;
    }
    return 0;
}

void cancelInterruptedOutposts(GameState& next, TurnReport& report) {
    for (auto& f : next.fleets) {
        if (!f.buildingOutpost) continue;
        if (f.x == f.targetX && f.y == f.targetY && !f.retreat && !f.attackTarget && !f.invadeSystemId && !f.colonize &&
            !f.taskForceId && !f.terraforming) continue;
        f.buildingOutpost = false;
        f.outpostProgress = 0;
        if (f.ownerId != next.playerEmpire) continue;
        ChronicleEntry e;
        e.category = ChronicleCategory::Stations;
        e.text = "Fleet " + std::to_string(f.id) + ": outpost assembly at " + sectorText(f.x, f.y) +
            " abandoned (the transport received new orders).";
        e.x = f.x; e.y = f.y; e.fleetId = f.id; e.empireId = f.ownerId;
        note(report, std::move(e));
    }
}

void advanceOutpostConstruction(GameState& moving, TurnReport& report) {
    for (auto& f : moving.fleets) {
        if (!f.buildingOutpost || f.hull <= 0) continue;
        const bool player = f.ownerId == moving.playerEmpire;
        auto abandon = [&](const std::string& why) {
            f.buildingOutpost = false; f.outpostProgress = 0;
            if (player) {
                ChronicleEntry e;
                e.category = ChronicleCategory::Stations;
                e.text = "Fleet " + std::to_string(f.id) + ": outpost assembly at " + sectorText(f.x, f.y) + " abandoned (" + why + ").";
                e.x = f.x; e.y = f.y; e.fleetId = f.id; e.empireId = f.ownerId;
                note(report, std::move(e));
            }
        };
        if (f.x != f.targetX || f.y != f.targetY || f.retreat || f.attackTarget || f.invadeSystemId || f.colonize) {
            abandon("the transport received new orders"); continue;
        }
        if (stationAt(moving, f.x, f.y)) { abandon("another station occupies the sector"); continue; }
        if (static_cast<int>(moving.stations.size()) >= StationLimit || !sectorPassable(moving, f.x, f.y)) {
            abandon("the site is no longer usable"); continue;
        }
        ++f.outpostProgress;
        if (f.outpostProgress < outpostBuildTurns(moving, f.ownerId)) continue;
        Station st;
        st.id = moving.nextStationId++;
        st.ownerId = f.ownerId;
        st.x = f.x; st.y = f.y;
        st.type = StationType::Outpost;
        const auto stats = stationStats(moving, f.ownerId, StationType::Outpost);
        st.hull = stats.maxHull; st.shields = stats.maxShields;
        st.builtTurn = moving.turn;
        moving.stations.push_back(st);
        f.buildingOutpost = false; f.outpostProgress = 0;
        f.hull = 0; // The transport becomes the outpost.
        if (player) noteStation(report, st, "Outpost completed at " + sectorText(st.x, st.y) + ". Supply and sensor range extended.");
        else if (visible(moving, moving.playerEmpire, st.x, st.y))
            noteStation(report, st, std::string(empireName(st.ownerId)) + " outpost detected at " + sectorText(st.x, st.y) + ".");
    }
    std::erase_if(moving.fleets, [](const Fleet& f) { return f.hull <= 0; });
    std::sort(moving.stations.begin(), moving.stations.end(), [](const Station& a, const Station& b) { return a.id < b.id; });
    internal::reconcileTaskForces(moving);
}

void stationsAfterCombat(GameState& moving, TurnReport& report, const std::set<int>& inBattle) {
    // Report stations destroyed this turn (recorded as battle losses).
    for (const auto& battle : report.battles) {
        for (int lost : battle.lostFleetIds) {
            if (!isStationCombatant(lost)) continue;
            const int owner = std::find_if(battle.participants.begin(), battle.participants.end(),
                [lost](const CombatParticipant& p) { return p.fleetId == lost; })->ownerId;
            const int other = owner == battle.firstOwner ? battle.secondOwner : battle.firstOwner;
            if (owner != moving.playerEmpire && other != moving.playerEmpire) continue;
            ChronicleEntry e;
            e.category = ChronicleCategory::Stations;
            e.text = std::string(empireName(owner)) + " station at " + sectorText(battle.x, battle.y) + " destroyed by " +
                empireName(other) + ".";
            e.x = battle.x; e.y = battle.y; e.stationId = stationIdForCombatant(lost);
            e.empireId = owner; e.otherEmpireId = other;
            note(report, std::move(e));
        }
    }
    for (auto& st : moving.stations) {
        const auto stats = stationStats(moving, st);
        if (!inBattle.count(st.id)) {
            st.hull = std::min(stats.maxHull, st.hull + std::max(1, stats.maxHull / 10));
            st.shields = stats.maxShields;
        }
        if (stats.repairPercent <= 0 || hostileArmedPresent(moving, st.ownerId, st.x, st.y)) continue;
        for (auto& f : moving.fleets) {
            if (f.ownerId != st.ownerId || f.x != st.x || f.y != st.y || f.hull <= 0) continue;
            const bool fought = std::any_of(report.battles.begin(), report.battles.end(), [&](const CombatReport& b) {
                return std::find(b.firstFleetIds.begin(), b.firstFleetIds.end(), f.id) != b.firstFleetIds.end() ||
                    std::find(b.secondFleetIds.begin(), b.secondFleetIds.end(), f.id) != b.secondFleetIds.end();
            });
            if (fought) continue;
            const int maxHull = fleetMaxHull(f);
            const int oldHull = f.hull;
            f.hull = std::min(maxHull, f.hull + std::max(1, maxHull * stats.repairPercent / 100));
            f.shields = moving.advancedRulesEnabled ? fleetMaxShields(f) : f.shields;
            if (f.ownerId == moving.playerEmpire && f.hull > oldHull) {
                ChronicleEntry e;
                e.category = ChronicleCategory::Stations;
                e.text = fleetName(f) + " repaired at the starbase at " + sectorText(st.x, st.y) + ".";
                e.x = st.x; e.y = st.y; e.fleetId = f.id; e.stationId = st.id; e.empireId = f.ownerId;
                note(report, std::move(e));
            }
        }
    }
}

void resolveStationCaptures(GameState& moving, TurnReport& report) {
    for (auto& st : moving.stations) {
        const auto stats = stationStats(moving, st);
        if (st.hull * 2 > stats.maxHull) continue;
        const bool defended = std::any_of(moving.fleets.begin(), moving.fleets.end(), [&](const Fleet& f) {
            return f.ownerId == st.ownerId && f.hull > 0 && f.x == st.x && f.y == st.y && fleetAttack(f) > 0;
        });
        if (defended) continue;
        Fleet* capturer = nullptr;
        for (auto& f : moving.fleets) {
            if (f.hull > 0 && f.x == st.x && f.y == st.y && f.shipClass == Building::Transport && !f.buildingOutpost &&
                empiresAtWar(moving, f.ownerId, st.ownerId)) { capturer = &f; break; }
        }
        if (!capturer) continue;
        const int oldOwner = st.ownerId;
        st.ownerId = capturer->ownerId;
        st.shields = 0;
        st.hull = std::min(st.hull, stationStats(moving, st).maxHull);
        capturer->hull = 0; // Boarding parties garrison the station.
        std::erase_if(moving.stationUpgrades, [&](const StationUpgrade& u) { return u.stationId == st.id; });
        if (oldOwner == moving.playerEmpire || st.ownerId == moving.playerEmpire)
            noteStation(report, st, std::string(stationTypeName(st.type)) + " at " + sectorText(st.x, st.y) + " captured by " +
                empireName(st.ownerId) + " from " + empireName(oldOwner) + ".", oldOwner);
    }
    std::erase_if(moving.fleets, [](const Fleet& f) { return f.hull <= 0; });
    internal::reconcileTaskForces(moving);
}

void completeStationUpgrades(GameState& next, TurnReport& report) {
    std::erase_if(next.stationUpgrades, [&](const StationUpgrade& u) {
        auto* st = mutableStation(next, u.stationId);
        const auto colony = colonyAt(next, u.fundingSystemId);
        if (!st || st->type != StationType::Outpost || !colony || colony->ownerId != st->ownerId) return true;
        if (u.work < stationUpgradeCost(next, st->ownerId).industry) return false;
        st->type = StationType::Starbase;
        const auto stats = stationStats(next, *st);
        st->hull = stats.maxHull; st->shields = stats.maxShields;
        if (st->ownerId == next.playerEmpire)
            noteStation(report, *st, "Starbase commissioned at " + sectorText(st->x, st->y) + ". It repairs our ships and anchors the frontier.");
        else if (visible(next, next.playerEmpire, st->x, st->y))
            noteStation(report, *st, std::string(empireName(st->ownerId)) + " starbase commissioned at " + sectorText(st->x, st->y) + ".");
        return true;
    });
}

// ---------------------------------------------------------------------------
// Map and combat integration
// ---------------------------------------------------------------------------
bool stationSupplies(const GameState& s, int owner, int x, int y) {
    for (const auto& st : s.stations)
        if (st.ownerId == owner && chebyshev(st.x, st.y, x, y) <= stationSupplyRadiusFor(st.type)) return true;
    return false;
}

bool stationDefends(const GameState& s, int ownerId, int x, int y) {
    const auto st = stationAt(s, x, y);
    return st && st->ownerId == ownerId && st->hull > 0;
}

const Station* combatStation(const GameState& s, int x, int y, int first, int second) {
    const auto st = stationAt(s, x, y);
    return st && st->hull > 0 && (st->ownerId == first || st->ownerId == second) ? st : nullptr;
}

tactical::Encounter constructStationEncounter(GameState& state, const std::vector<int>& fleetIds,
                                              std::uint64_t seed, const Station& station) {
    // The tactical engine is built from campaign fleets, so the station joins as a
    // proxy fleet for construction only; its real values are written below.
    Fleet proxy;
    proxy.id = StationCombatantBase + station.id;
    proxy.ownerId = station.ownerId;
    proxy.x = proxy.targetX = station.x;
    proxy.y = proxy.targetY = station.y;
    proxy.shipClass = Building::Destroyer;
    proxy.hull = 1;
    std::vector<int> ids = fleetIds;
    ids.push_back(proxy.id);
    const auto stats = stationStats(state, station);
    const std::string name = stationName(station);
    state.fleets.push_back(proxy);
    tactical::Encounter encounter;
    try {
        encounter = tactical::constructEncounter(state, ids, seed);
    } catch (...) {
        state.fleets.pop_back();
        throw;
    }
    state.fleets.pop_back();
    auto* ship = encounter.getShip(proxy.id);
    require(ship != nullptr, "Station missing from its encounter");
    ship->name = name;
    ship->maxHull = stats.maxHull;
    ship->hull = ship->initialHull = std::clamp(station.hull, 1, stats.maxHull);
    ship->maxShields = stats.maxShields;
    ship->shields = ship->initialShields = std::clamp(station.shields, 0, stats.maxShields);
    ship->beamAttack = stats.beam;
    ship->torpedoAttack = stats.torpedo;
    ship->speed = 1;
    ship->canRetreat = false;
    ship->canCloak = false;
    ship->isCloaked = false;
    ship->immobile = true;
    int firstOwner = station.ownerId;
    for (const auto& s : encounter.ships) firstOwner = std::min(firstOwner, s.ownerId);
    ship->position.x = station.ownerId == firstOwner ? 1 : encounter.arenaWidth - 2;
    ship->position.y = encounter.arenaHeight / 2;
    encounter.standingOrders[proxy.id] = tactical::Order{proxy.id, 0, tactical::TacticalManeuver::Hold};
    // Stations form their own tactical group (group orders never move them).
    int nextGroup = 1;
    for (auto& group : encounter.groups) {
        std::erase(group.fleetIds, proxy.id);
        nextGroup = std::max(nextGroup, group.groupId + 1);
    }
    std::erase_if(encounter.groups, [](const tactical::TacticalGroup& g) { return g.fleetIds.empty(); });
    tactical::TacticalGroup group;
    group.groupId = nextGroup;
    group.ownerId = station.ownerId;
    group.groupClass = tactical::TacticalGroupClass::Command;
    group.name = "Station Group " + std::to_string(nextGroup);
    group.fleetIds = {proxy.id};
    group.standingManeuver = tactical::TacticalManeuver::Hold;
    encounter.groups.push_back(group);
    ship->groupId = group.groupId;
    return encounter;
}

void applyStationEncounter(GameState& state, const tactical::Encounter& encounter) {
    auto fleetsOnly = encounter;
    std::erase_if(fleetsOnly.ships, [](const tactical::Ship& s) { return isStationCombatant(s.fleetId); });
    if (!fleetsOnly.ships.empty()) tactical::applyEncounterResults(state, fleetsOnly);
    encounter.resultsApplied = true;
    for (const auto& ship : encounter.ships) {
        if (!isStationCombatant(ship.fleetId)) continue;
        const int id = stationIdForCombatant(ship.fleetId);
        auto* st = mutableStation(state, id);
        require(st != nullptr && st->ownerId == ship.ownerId, "Station changed during its battle");
        if (!ship.isAlive || ship.hull <= 0) {
            std::erase_if(state.stations, [id](const Station& s) { return s.id == id; });
            std::erase_if(state.stationUpgrades, [id](const StationUpgrade& u) { return u.stationId == id; });
        } else {
            st->hull = ship.hull;
            st->shields = ship.shields;
        }
    }
}

// ---------------------------------------------------------------------------
// Rival AI
// ---------------------------------------------------------------------------
// Deterministic outpost site choice for one transport: extend supply into
// unsupplied space, claim empty systems, guard wormholes and hostile frontiers.
bool chooseOutpostSite(const GameState& s, const Fleet& transport, std::set<int>& claimed, int& siteX, int& siteY) {
    const int owner = transport.ownerId;
    const int w = s.width, h = s.height;
    std::vector<char> suppliedGrid(static_cast<std::size_t>(w * h), 0);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
        suppliedGrid[static_cast<std::size_t>(y * w + x)] = supplied(s, transport, x, y) ? 1 : 0;
    struct Candidate { int cell, score; };
    std::vector<Candidate> candidates;
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const int cell = y * w + x;
        if (!suppliedGrid[static_cast<std::size_t>(cell)] || claimed.count(cell) || !internal::known(s, owner, x, y)) continue;
        if (!sectorPassable(s, x, y) || sectorHazardous(s, x, y) || stationAt(s, x, y)) continue;
        if (chebyshev(x, y, transport.x, transport.y) > 10) continue;
        const auto sys = systemAtCell(s, x, y);
        if (sys && (minorAt(s, sys->id) || (colonyAt(s, sys->id) && colonyAt(s, sys->id)->ownerId != owner))) continue;
        bool nearOwn = false;
        for (const auto& st : s.stations) if (st.ownerId == owner && chebyshev(st.x, st.y, x, y) <= 3) nearOwn = true;
        for (const auto& c : s.colonies) {
            if (c.ownerId != owner) continue;
            const auto home = systemById(s, c.systemId);
            if (home && chebyshev(home->x, home->y, x, y) <= 2) nearOwn = true;
        }
        if (nearOwn) continue;
        int fresh = 0;
        for (int dy = -3; dy <= 3; ++dy) for (int dx = -3; dx <= 3; ++dx) {
            const int px = x + dx, py = y + dy;
            if (px < 0 || py < 0 || px >= w || py >= h) continue;
            if (!suppliedGrid[static_cast<std::size_t>(py * w + px)]) ++fresh;
        }
        int score = 10 * fresh - 2 * chebyshev(x, y, transport.x, transport.y);
        if (sys && !colonyAt(s, sys->id)) score += 30;
        for (const auto& f : s.galaxyFeatures)
            if (f.type == GalaxyFeatureType::Wormhole && chebyshev(f.x, f.y, x, y) <= 1) score += 25;
        for (const auto& c : s.colonies) {
            if (c.ownerId == owner) continue;
            const auto sysc = systemById(s, c.systemId);
            const auto rel = majorRelation(s, owner, c.ownerId);
            if (sysc && rel && rel->contact && chebyshev(sysc->x, sysc->y, x, y) <= 5) { score += 15; break; }
        }
        if (score > 0) candidates.push_back({cell, score});
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.score == b.score ? a.cell < b.cell : a.score > b.score;
    });
    int checked = 0;
    for (const auto& c : candidates) {
        if (++checked > 6) break; // Route checks are the expensive part.
        const int x = c.cell % w, y = c.cell / w;
        if (route(s, transport, x, y).empty()) continue;
        siteX = x; siteY = y;
        claimed.insert(c.cell);
        return true;
    }
    return false;
}

// At war: strike visible hostile stations with enough firepower, and send a
// troop transport to board a weakened, undefended one.
void planStationRaids(GameState& next, int owner) {
    std::set<int> committed;
    const int weapons = empire(next, owner).technologies[5].level;
    for (const auto& st : visibleStations(next, owner)) {
        if (st.ownerId == owner || !empiresAtWar(next, owner, st.ownerId)) continue;
        const auto stats = stationStats(next, st);
        const bool defended = std::any_of(next.fleets.begin(), next.fleets.end(), [&](const Fleet& f) {
            return f.ownerId == st.ownerId && f.x == st.x && f.y == st.y && fleetAttack(f) > 0;
        });
        const auto byDistance = [&](const Fleet* a, const Fleet* b) {
            const int da = chebyshev(a->x, a->y, st.x, st.y), db = chebyshev(b->x, b->y, st.x, st.y);
            return da == db ? a->id < b->id : da < db;
        };
        if (st.hull * 2 <= stats.maxHull && !defended) {
            std::vector<Fleet*> transports;
            for (auto& f : next.fleets)
                if (f.ownerId == owner && f.shipClass == Building::Transport && f.hull > 0 && !f.buildingOutpost &&
                    !f.taskForceId && !f.invadeSystemId && !f.retreat && !committed.count(f.id) &&
                    chebyshev(f.x, f.y, st.x, st.y) <= 8) transports.push_back(&f);
            std::sort(transports.begin(), transports.end(), byDistance);
            for (auto* f : transports) {
                if (!(f->x == st.x && f->y == st.y) && route(next, *f, st.x, st.y).empty()) continue;
                f->targetX = st.x; f->targetY = st.y; f->attackTarget = 0;
                committed.insert(f->id);
                break;
            }
        }
        std::vector<Fleet*> warships;
        for (auto& f : next.fleets)
            if (f.ownerId == owner && f.hull > 0 && fleetAttack(f) > 0 && f.shipClass != Building::Transport && !f.colonyShip &&
                !f.taskForceId && !f.attackTarget && !f.retreat && !committed.count(f.id) && f.hull * 4 >= fleetMaxHull(f) * 3 &&
                chebyshev(f.x, f.y, st.x, st.y) <= 8) warships.push_back(&f);
        std::sort(warships.begin(), warships.end(), byDistance);
        std::vector<Fleet*> group;
        int power = 0;
        const int needed = st.hull + st.shields;
        for (auto* f : warships) {
            if (group.size() >= 5 || power >= needed) break;
            if (!(f->x == st.x && f->y == st.y) && route(next, *f, st.x, st.y).empty()) continue;
            group.push_back(f);
            power += (fleetAttack(*f) + weapons) * 6;
        }
        if (power < needed) continue;
        for (auto* f : group) {
            f->targetX = st.x; f->targetY = st.y;
            committed.insert(f->id);
        }
    }
}

void planRivalStations(GameState& next) {
    for (const auto& rival : next.rivals) {
        const int owner = rival.id;
        const bool war = empireAtAnyWar(next, owner);
        // Builders keep their post whatever the general fleet plan says.
        for (auto& f : next.fleets) {
            if (f.ownerId != owner || !f.buildingOutpost) continue;
            internal::detachFromTaskForce(next, f);
            f.targetX = f.x; f.targetY = f.y;
            f.attackTarget = 0; f.invadeSystemId = 0; f.retreat = false;
        }
        int colonies = 0;
        for (const auto& c : next.colonies) if (c.ownerId == owner) ++colonies;
        if (colonies == 0) continue;
        const int desired = std::min(4, 1 + colonies / 2);
        int have = stationsOwnedOrBuilding(next, owner);
        std::set<int> claimed;
        int idleTransports = 0;
        if (!war && have < desired && totalStationsAndBuilds(next) < StationLimit) {
            for (auto& f : next.fleets) {
                if (have >= desired) break;
                if (f.ownerId != owner || f.shipClass != Building::Transport || f.buildingOutpost || f.hull <= 0 ||
                    f.taskForceId || f.invadeSystemId || f.retreat) continue;
                ++idleTransports;
                int sx = 0, sy = 0;
                if (!chooseOutpostSite(next, f, claimed, sx, sy)) continue;
                if (f.x == sx && f.y == sy) {
                    if (!outpostBlocker(next, f.id).empty()) continue;
                    f.buildingOutpost = true; f.outpostProgress = 0;
                    f.targetX = f.x; f.targetY = f.y; f.attackTarget = 0;
                    ++have;
                } else {
                    f.targetX = sx; f.targetY = sy; f.attackTarget = 0;
                    ++have; // Committed to a site.
                }
            }
            // Order a transport for outpost duty when none is available or queued.
            if (have < desired && idleTransports == 0 && stationsOwnedOrBuilding(next, owner) < StationLimitPerEmpire &&
                technologyBlocker(next, Building::Transport, owner).empty()) {
                // One ship is built per empire at a time and the lowest-numbered powered
                // yard holds that slot, so the outpost transport goes to the front of that
                // yard's queue (and moves there if a newer colony took the slot).
                Colony* slot = nullptr;
                for (auto& c : next.colonies)
                    if (c.ownerId == owner && economy(next, c).shipyardPowered) { slot = &c; break; }
                std::optional<Construction> pending;
                for (auto& c : next.colonies) {
                    if (c.ownerId != owner) continue;
                    for (std::size_t i = 0; i < c.queue.size() && !pending; ++i) {
                        if (c.queue[i].product != Building::Transport || c.queue[i].pinned) continue;
                        if (&c == slot && i == 0) { pending = c.queue[i]; break; }
                        pending = c.queue[i];
                        c.queue.erase(c.queue.begin() + static_cast<std::ptrdiff_t>(i));
                        if (!slot || slot->queue.size() >= static_cast<std::size_t>(internal::QueueLimit)) { c.queue.insert(c.queue.begin() + static_cast<std::ptrdiff_t>(i), *pending); pending.reset(); break; }
                        slot->queue.insert(slot->queue.begin(), *pending);
                    }
                }
                if (!pending && slot && next.fleets.size() < 240 && slot->queue.size() < 4) {
                    const auto before = slot->queue.size();
                    if (internal::queueShip(next, *slot, Building::Transport)) {
                        const Construction project = slot->queue.back();
                        slot->queue.resize(before);
                        if (internal::constructionTechBlocker(next, project, owner).empty())
                            slot->queue.insert(slot->queue.begin(), project);
                    }
                }
            }
        }
        if (war) planStationRaids(next, owner);
        // Upgrade one frontier outpost at a time once the treasury allows it.
        const auto cost = stationUpgradeCost(next, owner);
        const bool upgrading = std::any_of(next.stationUpgrades.begin(), next.stationUpgrades.end(), [&](const StationUpgrade& u) {
            const auto st = stationById(next, u.stationId);
            return st && st->ownerId == owner;
        });
        if (upgrading || rival.credits < cost.credits * 3) continue;
        const Station* bestStation = nullptr;
        int bestColony = 0, bestScore = std::numeric_limits<int>::min();
        for (const auto& st : next.stations) {
            if (st.ownerId != owner || st.type != StationType::Outpost) continue;
            int threat = 0;
            for (const auto& c : next.colonies) {
                if (c.ownerId == owner) continue;
                const auto sys = systemById(next, c.systemId);
                if (sys) threat = std::max(threat, 20 - chebyshev(sys->x, sys->y, st.x, st.y));
            }
            int colonyId = 0, colonyIndustry = -1;
            for (const auto& c : next.colonies) {
                if (c.ownerId != owner || !stationUpgradeBlocker(next, st.id, c.systemId).empty()) continue;
                const int industry = economy(next, c).industry;
                if (industry > colonyIndustry) { colonyIndustry = industry; colonyId = c.systemId; }
            }
            if (!colonyId || colonyIndustry <= 0) continue;
            const int score = threat * 10 + colonyIndustry;
            if (score > bestScore) { bestScore = score; bestStation = &st; bestColony = colonyId; }
        }
        if (!bestStation) continue;
        internal::mutableEmpire(next, owner).credits -= cost.credits;
        next.stationUpgrades.push_back({bestStation->id, bestColony, 0});
        std::sort(next.stationUpgrades.begin(), next.stationUpgrades.end(), [](const StationUpgrade& a, const StationUpgrade& b) {
            return a.stationId < b.stationId;
        });
    }
}
} // namespace botf::gameplay

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
namespace botf {
const Station* stationById(const GameState& state, int stationId) {
    const auto it = std::find_if(state.stations.begin(), state.stations.end(), [stationId](const Station& s) { return s.id == stationId; });
    return it == state.stations.end() ? nullptr : &*it;
}
const Station* stationAt(const GameState& state, int x, int y) {
    const auto it = std::find_if(state.stations.begin(), state.stations.end(), [&](const Station& s) { return s.x == x && s.y == y; });
    return it == state.stations.end() ? nullptr : &*it;
}

StationStats stationStats(const GameState& state, int ownerId, StationType type) {
    const auto& tech = empire(state, ownerId).technologies;
    const int construction = tech[2].level, energy = tech[3].level, weapons = tech[5].level;
    const bool outpost = type == StationType::Outpost;
    StationStats stats;
    int attack = 0;
    if (state.ruleset == Ruleset::Reference) {
        // Hull, shields and upkeep come from the race's catalogue station; the
        // remaster maps its armament onto the tactical scale (outposts rival a
        // few destroyers, starbases a heavy squadron).
        const auto* record = gameplay::catalogueStation(state, ownerId, type);
        gameplay::require(record != nullptr, "Race has no catalogue station");
        stats.maxHull = record->hull;
        stats.maxShields = record->shields;
        stats.upkeep = record->maintenance;
        attack = outpost ? 14 + 2 * weapons : 40 + 4 * weapons;
    } else {
        stats.maxHull = outpost ? 40 + 4 * construction : 120 + 10 * construction;
        stats.maxShields = outpost ? 10 + 3 * energy : 40 + 6 * energy;
        stats.upkeep = outpost ? 3 : 8;
        attack = outpost ? 7 + weapons : 18 + 2 * weapons;
    }
    stats.beam = attack * 55 / 100;
    stats.torpedo = attack - stats.beam;
    stats.supplyRadius = gameplay::stationSupplyRadiusFor(type);
    stats.sensorRadius = gameplay::stationSensorRadiusFor(type);
    stats.repairPercent = outpost ? 0 : 20;
    return stats;
}
StationStats stationStats(const GameState& state, const Station& station) {
    return stationStats(state, station.ownerId, station.type);
}
const char* stationTypeName(StationType type) {
    switch (type) {
    case StationType::Outpost: return "Outpost";
    case StationType::Starbase: return "Starbase";
    default: return "Unknown";
    }
}
std::string stationName(const Station& station) {
    return std::string(stationTypeName(station.type)) + " " + std::to_string(station.id);
}
std::vector<Station> visibleStations(const GameState& state, int viewerEmpire) {
    std::vector<Station> result;
    for (const auto& st : state.stations)
        if (st.ownerId == viewerEmpire || visible(state, viewerEmpire, st.x, st.y)) result.push_back(st);
    return result;
}
int outpostBuildTurns(const GameState& state, int ownerId) {
    return empire(state, ownerId).technologies[2].level >= 3 ? gameplay::OutpostBuildTurnsMax - 1 : gameplay::OutpostBuildTurnsMax;
}
std::string outpostBlocker(const GameState& state, int fleetId) {
    if (!state.rules.stations) return "Stations are disabled by the ruleset";
    const auto fleet = std::find_if(state.fleets.begin(), state.fleets.end(), [fleetId](const Fleet& f) { return f.id == fleetId; });
    if (fleet == state.fleets.end() || fleet->hull <= 0) return "Fleet does not exist";
    if (fleet->shipClass != Building::Transport) return "Only troop transports can assemble outposts";
    if (fleet->buildingOutpost) return "This transport is already assembling an outpost";
    if (fleet->colonize || fleet->retreat || fleet->invadeSystemId) return "Transport is busy";
    if (!internal::known(state, fleet->ownerId, fleet->x, fleet->y)) return "Sector is unexplored";
    if (!sectorPassable(state, fleet->x, fleet->y)) return "Sector is impassable";
    if (stationAt(state, fleet->x, fleet->y)) return "A station already occupies this sector";
    if (const auto sys = gameplay::systemAtCell(state, fleet->x, fleet->y)) {
        if (minorAt(state, sys->id)) return "Cannot build inside a minor race's system";
        const auto colony = colonyAt(state, sys->id);
        if (colony && colony->ownerId != fleet->ownerId) return "Cannot build inside a foreign colony's system";
    }
    if (gameplay::stationsOwnedOrBuilding(state, fleet->ownerId) >= gameplay::StationLimitPerEmpire)
        return "Station limit reached (" + std::to_string(gameplay::StationLimitPerEmpire) + " per empire)";
    if (gameplay::totalStationsAndBuilds(state) >= gameplay::StationLimit) return "Galactic station limit reached";
    return {};
}
StationUpgradeCost stationUpgradeCost(const GameState& state, int ownerId) {
    (void)ownerId;
    return state.ruleset == Ruleset::Reference ? StationUpgradeCost{1500, 400} : StationUpgradeCost{300, 240};
}
std::string stationUpgradeBlocker(const GameState& state, int stationId, int fundingSystemId) {
    if (!state.rules.stations) return "Stations are disabled by the ruleset";
    const auto st = stationById(state, stationId);
    if (!st) return "Station does not exist";
    if (st->type != StationType::Outpost) return "Only outposts can be upgraded";
    if (stationUpgradeFor(state, stationId)) return "Starbase upgrade already under way";
    const auto colony = colonyAt(state, fundingSystemId);
    if (!colony || colony->ownerId != st->ownerId) return "Funding colony must belong to the station's owner";
    const auto sys = gameplay::systemById(state, fundingSystemId);
    if (!sys || gameplay::chebyshev(sys->x, sys->y, st->x, st->y) > gameplay::StationUpgradeRange)
        return "Funding colony must be within " + std::to_string(gameplay::StationUpgradeRange) + " sectors of the outpost";
    for (const auto& u : state.stationUpgrades)
        if (u.fundingSystemId == fundingSystemId) return "Colony already funds a starbase upgrade";
    for (const auto& m : state.megaprojects)
        if (m.systemId == fundingSystemId) return "Colony is building the empire megaproject";
    if (empire(state, st->ownerId).technologies[2].level < gameplay::StationUpgradeConstructionLevel)
        return "Requires Construction level " + std::to_string(gameplay::StationUpgradeConstructionLevel);
    const auto cost = stationUpgradeCost(state, st->ownerId);
    if (empire(state, st->ownerId).credits < cost.credits) return "Requires " + std::to_string(cost.credits) + " credits";
    return {};
}
const StationUpgrade* stationUpgradeFor(const GameState& state, int stationId) {
    const auto it = std::find_if(state.stationUpgrades.begin(), state.stationUpgrades.end(), [stationId](const StationUpgrade& u) {
        return u.stationId == stationId;
    });
    return it == state.stationUpgrades.end() ? nullptr : &*it;
}
bool isStationCombatant(int fleetId) { return fleetId > StationCombatantBase && fleetId <= StationCombatantBase + 1000000; }
int stationIdForCombatant(int fleetId) { return isStationCombatant(fleetId) ? fleetId - StationCombatantBase : 0; }

Command makeBuildOutpostCommand(std::uint64_t id, int fleetId) {
    Command c; c.id = id; c.type = CommandType::BuildOutpost; c.fleetId = fleetId; return c;
}
Command makeUpgradeStationCommand(std::uint64_t id, int stationId, int fundingSystemId) {
    Command c; c.id = id; c.type = CommandType::UpgradeStation; c.stationId = stationId; c.systemId = fundingSystemId; return c;
}
} // namespace botf
