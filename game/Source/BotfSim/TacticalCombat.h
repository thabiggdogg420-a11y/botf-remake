#pragma once

#include "Simulation.h"
#include "ReferenceCatalogue.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace botf::tactical {

// -----------------------------------------------------------------------------
// Tactical Coordinate & Enums
// -----------------------------------------------------------------------------

struct TacticalCoord {
    int x = 0;
    int y = 0;
    constexpr bool operator==(const TacticalCoord& other) const = default;
};

// Original-inspired tactical maneuvers (Manual pp. 64-65, Tutorial pp. 25-27)
enum class TacticalManeuver : int {
    Engage = 0,    // Standard advance to optimal weapon range and fire
    Charge = 1,    // High-speed closing to close/point-blank range (+beam burst, +incoming vulnerability)
    Circle = 2,    // Stand-off evasive orbit (-incoming damage evasion, -beam accuracy)
    Hold = 3,      // Stationary shield focus (recharge/reinforce shields)
    Retreat = 4,   // Disengage toward tactical jump boundary, weapons offline
    Assault = 5,   // Original Assault: disciplined advance to medium beam/torpedo envelope
    Harry = 6,     // Original Harry: close-range evasive circling and harassment
    Flyby = 7,     // Original Flyby: high-speed passing run at medium range
    Strafe = 8,    // Original Strafe: high-speed passing run at close range
    Evade = 9,     // Original Evade: defensive evasion, firing only on targets of opportunity
    Ram = 10,     // Original Ram: desperate kinetic collision attack
    Hail = 11      // Original Hail: offer mutual disengagement / truce
};

inline const char* maneuverName(TacticalManeuver maneuver) {
    switch (maneuver) {
        case TacticalManeuver::Engage: return "Engage";
        case TacticalManeuver::Charge: return "Charge";
        case TacticalManeuver::Circle: return "Circle/Evade";
        case TacticalManeuver::Hold: return "Hold";
        case TacticalManeuver::Retreat: return "Retreat";
        case TacticalManeuver::Assault: return "Assault";
        case TacticalManeuver::Harry: return "Harry";
        case TacticalManeuver::Flyby: return "Flyby";
        case TacticalManeuver::Strafe: return "Strafe";
        case TacticalManeuver::Evade: return "Evade";
        case TacticalManeuver::Ram: return "Ram";
        case TacticalManeuver::Hail: return "Hail";
        default: return "Unknown";
    }
}

// Like-class tactical group categories (Manual pp. 63-64)
enum class TacticalGroupClass : int {
    Command = 0,      // Cruisers, Command ships
    FastAttack = 1,   // Destroyers, Strike craft
    Scout = 2,        // Scouts, reconnaissance
    Support = 3       // Transports, Colony ships, noncombatants
};

inline const char* tacticalGroupClassName(TacticalGroupClass c) {
    switch (c) {
        case TacticalGroupClass::Command: return "Command";
        case TacticalGroupClass::FastAttack: return "Fast Attack";
        case TacticalGroupClass::Scout: return "Scout";
        case TacticalGroupClass::Support: return "Support";
        default: return "Unknown";
    }
}

struct TacticalGroup {
    int groupId = 0;
    int ownerId = 0;
    TacticalGroupClass groupClass = TacticalGroupClass::Command;
    std::string name;
    std::vector<int> fleetIds;
    TacticalManeuver standingManeuver = TacticalManeuver::Engage;
    int standingTargetFleetId = 0;
    constexpr bool operator==(const TacticalGroup& other) const = default;
};

// -----------------------------------------------------------------------------
// Core Structs: Ship, Order, RoundResult, Encounter
// -----------------------------------------------------------------------------

struct Ship {
    int fleetId = 0;               // Campaign fleet ID
    int ownerId = 0;               // Campaign empire/owner ID
    std::string name;              // Vessel / task force name
    Building shipClass = Building::Scout;
    int referenceShipId = -1;      // ID in ReferenceCatalogue (-1 for prototype/generic)
    int hull = 0;                  // Current hull integrity
    int maxHull = 0;               // Maximum hull integrity
    int initialHull = 0;           // Starting hull when encounter began
    int shields = 0;               // Current shield capacity
    int maxShields = 0;            // Maximum shield capacity
    int initialShields = 0;        // Starting shields when encounter began
    int beamAttack = 0;            // Beam weapon attack power (phasers/disruptors)
    int torpedoAttack = 0;         // Torpedo weapon attack power (photons/plasma)
    int speed = 1;                 // Tactical movement speed in grid cells/round
    TacticalCoord position{0, 0};  // Current coordinates on tactical battlefield
    bool isAlive = true;           // True if vessel is physically intact (hull > 0, not destroyed)
    bool activeInCombat = true;    // True if still actively engaged on the tactical map
    bool retreated = false;        // True if successfully disengaged and retreated
    int retreatSteps = 0;          // Rounds spent moving toward retreat edge
    bool canRetreat = true;        // True if a safe supplied retreat sector exists
    bool canCloak = false;         // True if equipped with cloaking device
    bool isCloaked = false;        // True if currently in cloaked stealth state
    int groupId = 0;               // Tactical like-class group assignment
};

struct Order {
    int fleetId = 0;               // ID of the ship receiving the order
    int targetFleetId = 0;         // Target enemy fleet ID (0 for auto-targeting)
    TacticalManeuver maneuver = TacticalManeuver::Engage;
    constexpr bool operator==(const Order& other) const = default;
};

struct ShipRoundStatus {
    int fleetId = 0;
    int ownerId = 0;
    int hullBefore = 0;
    int hullAfter = 0;
    int shieldBefore = 0;
    int shieldAfter = 0;
    int beamDamageDealt = 0;
    int torpedoDamageDealt = 0;
    int totalDamageTaken = 0;
    TacticalCoord positionBefore{0, 0};
    TacticalCoord positionAfter{0, 0};
    TacticalManeuver maneuverExecuted = TacticalManeuver::Engage;
    int targetFleetId = 0;
    bool destroyedThisRound = false;
    bool retreatedThisRound = false;
    bool decloakedThisRound = false;
    bool hailedThisRound = false;
    TacticalCoord intermediatePosition{0, 0};
    int facingYaw = 0;             // Heading in degrees [0, 359] for 3D orientation
    constexpr bool operator==(const ShipRoundStatus& other) const = default;
};

struct WeaponSalvoRecord {
    int attackerFleetId = 0;
    int defenderFleetId = 0;
    int beamDamage = 0;
    int torpedoDamage = 0;
    bool absorbedByShields = false;
    constexpr bool operator==(const WeaponSalvoRecord& other) const = default;
};

struct RoundResult {
    int roundIndex = 0;
    std::vector<ShipRoundStatus> shipStatuses;
    std::vector<FleetDamage> damage; // Directly maps to Simulation.h FleetDamage
    std::vector<int> lostFleetIds;
    std::vector<int> retreatedFleetIds;
    std::vector<WeaponSalvoRecord> salvos;
    bool encounterEnded = false;
    int winnerOwnerId = 0; // 0 if draw or ongoing
    bool hailedTruce = false;

    const ShipRoundStatus* getStatus(int fid) const {
        for (const auto& st : shipStatuses) {
            if (st.fleetId == fid) return &st;
        }
        return nullptr;
    }
};

// -----------------------------------------------------------------------------
// Deterministic RNG (64-bit LCG)
// -----------------------------------------------------------------------------

struct DeterministicRng {
    std::uint64_t state = 0;

    explicit DeterministicRng(std::uint64_t seed = 0)
        : state(seed == 0 ? 0x853c49e6748fea9bULL : seed) {}

    std::uint64_t next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    }

    int range(int minVal, int maxVal) {
        if (minVal >= maxVal) return minVal;
        const std::uint64_t span = static_cast<std::uint64_t>(maxVal - minVal + 1);
        return minVal + static_cast<int>(next() % span);
    }
};

// -----------------------------------------------------------------------------
// Tactical Encounter State
// -----------------------------------------------------------------------------

struct Encounter {
    int arenaWidth = 16;
    int arenaHeight = 12;
    int currentRound = 0;
    int maxRounds = 50;            // Hard limit to avoid infinite tactical loops
    bool isFinished = false;
    int winnerOwnerId = 0;
    std::uint64_t initialSeed = 0;
    std::uint64_t rngState = 0;
    int sectorX = 0;
    int sectorY = 0;
    int strategicTurn = 0;
    int controllingOwnerId = 1;    // Player empire ID authorized to issue orders
    mutable bool resultsApplied = false; // Guard against applying results twice

    std::vector<Ship> ships;
    std::vector<TacticalGroup> groups;
    std::vector<Order> pendingOrders;
    std::map<int, Order> standingOrders;
    std::vector<RoundResult> rounds;

    const Ship* getShip(int fleetId) const {
        for (const auto& s : ships) {
            if (s.fleetId == fleetId) return &s;
        }
        return nullptr;
    }

    Ship* getShip(int fleetId) {
        for (auto& s : ships) {
            if (s.fleetId == fleetId) return &s;
        }
        return nullptr;
    }

    const TacticalGroup* getGroup(int groupId) const {
        for (const auto& g : groups) {
            if (g.groupId == groupId) return &g;
        }
        return nullptr;
    }

    TacticalGroup* getGroup(int groupId) {
        for (auto& g : groups) {
            if (g.groupId == groupId) return &g;
        }
        return nullptr;
    }

    const TacticalGroup* getGroupForShip(int fleetId) const {
        const Ship* s = getShip(fleetId);
        if (!s || s->groupId == 0) return nullptr;
        return getGroup(s->groupId);
    }

    void submitOrder(const Order& order) {
        if (isFinished) {
            throw std::invalid_argument("Cannot submit order: tactical encounter is already finished.");
        }
        Ship* s = getShip(order.fleetId);
        if (!s) {
            throw std::invalid_argument("Fleet ID " + std::to_string(order.fleetId) + " not found in encounter.");
        }
        if (!s->isAlive || s->retreated || !s->activeInCombat) {
            throw std::invalid_argument("Cannot issue order to destroyed or retreated ship " + std::to_string(order.fleetId));
        }
        if (s->ownerId != controllingOwnerId) {
            throw std::invalid_argument("Cannot issue order to non-controlled/enemy fleet ID " + std::to_string(order.fleetId));
        }
        if (static_cast<int>(order.maneuver) < 0 || static_cast<int>(order.maneuver) > 11) {
            throw std::invalid_argument("Invalid maneuver enum value: " + std::to_string(static_cast<int>(order.maneuver)));
        }
        if (order.maneuver == TacticalManeuver::Retreat && !s->canRetreat) {
            throw std::invalid_argument("Fleet ID " + std::to_string(order.fleetId) +
                " cannot retreat: no safe supplied withdrawal sector exists.");
        }
        if (order.targetFleetId != 0) {
            const Ship* target = getShip(order.targetFleetId);
            if (!target) {
                throw std::invalid_argument("Target fleet ID " + std::to_string(order.targetFleetId) + " not found in encounter.");
            }
            if (!target->isAlive || target->retreated || !target->activeInCombat) {
                throw std::invalid_argument("Target fleet ID " + std::to_string(order.targetFleetId) + " is already destroyed or retreated.");
            }
            if (target->ownerId == s->ownerId) {
                throw std::invalid_argument("Cannot target friendly ship " + std::to_string(order.targetFleetId));
            }
        }

        // Replace existing order or append
        auto it = std::find_if(pendingOrders.begin(), pendingOrders.end(),
            [&](const Order& o) { return o.fleetId == order.fleetId; });
        if (it != pendingOrders.end()) {
            *it = order;
        } else {
            pendingOrders.push_back(order);
        }

        // Update standing orders map so order persists into subsequent rounds
        standingOrders[order.fleetId] = order;
    }

    void submitOrders(const std::vector<Order>& orders) {
        if (isFinished) {
            throw std::invalid_argument("Cannot submit orders: tactical encounter is already finished.");
        }
        // Atomic validation: validate all orders before applying any
        for (const auto& o : orders) {
            const Ship* s = getShip(o.fleetId);
            if (!s) {
                throw std::invalid_argument("Fleet ID " + std::to_string(o.fleetId) + " not found in encounter.");
            }
            if (!s->isAlive || s->retreated || !s->activeInCombat) {
                throw std::invalid_argument("Cannot issue order to destroyed or retreated ship " + std::to_string(o.fleetId));
            }
            if (s->ownerId != controllingOwnerId) {
                throw std::invalid_argument("Cannot issue order to non-controlled/enemy fleet ID " + std::to_string(o.fleetId));
            }
            if (static_cast<int>(o.maneuver) < 0 || static_cast<int>(o.maneuver) > 11) {
                throw std::invalid_argument("Invalid maneuver enum value: " + std::to_string(static_cast<int>(o.maneuver)));
            }
            if (o.maneuver == TacticalManeuver::Retreat && !s->canRetreat) {
                throw std::invalid_argument("Fleet ID " + std::to_string(o.fleetId) +
                    " cannot retreat: no safe supplied withdrawal sector exists.");
            }
            if (o.targetFleetId != 0) {
                const Ship* target = getShip(o.targetFleetId);
                if (!target) {
                    throw std::invalid_argument("Target fleet ID " + std::to_string(o.targetFleetId) + " not found in encounter.");
                }
                if (!target->isAlive || target->retreated || !target->activeInCombat) {
                    throw std::invalid_argument("Target fleet ID " + std::to_string(o.targetFleetId) + " is already destroyed or retreated.");
                }
                if (target->ownerId == s->ownerId) {
                    throw std::invalid_argument("Cannot target friendly ship " + std::to_string(o.targetFleetId));
                }
            }
        }

        for (const auto& o : orders) {
            submitOrder(o);
        }
    }

    // Submit group order to all member ships in a like-class tactical group
    void submitGroupOrder(int groupId, TacticalManeuver maneuver, int targetFleetId = 0) {
        TacticalGroup* g = getGroup(groupId);
        if (!g) {
            throw std::invalid_argument("Tactical group ID " + std::to_string(groupId) + " not found.");
        }
        if (g->ownerId != controllingOwnerId) {
            throw std::invalid_argument("Cannot order group belonging to other owner.");
        }
        g->standingManeuver = maneuver;
        g->standingTargetFleetId = targetFleetId;

        for (int fid : g->fleetIds) {
            Ship* s = getShip(fid);
            if (s && s->isAlive && !s->retreated && s->activeInCombat) {
                if (maneuver == TacticalManeuver::Retreat && !s->canRetreat) {
                    continue; // Skip retreat for ships lacking retreat route
                }
                submitOrder(Order{s->fleetId, targetFleetId, maneuver});
            }
        }
    }

    void clearOrders() {
        pendingOrders.clear();
    }

    Order getOrder(int fleetId) const {
        for (const auto& o : pendingOrders) {
            if (o.fleetId == fleetId) return o;
        }
        auto it = standingOrders.find(fleetId);
        if (it != standingOrders.end()) return it->second;
        return Order{fleetId, 0, TacticalManeuver::Engage};
    }

    // -------------------------------------------------------------------------
    // Round Resolution
    // -------------------------------------------------------------------------
    RoundResult resolveRound() {
        if (isFinished) {
            throw std::runtime_error("Encounter is already finished; cannot resolve further rounds.");
        }

        currentRound++;
        DeterministicRng rng(rngState);

        // First owner is used to determine deployment edges for retreats
        const int firstOwner = ships.empty() ? 0 : ships[0].ownerId;

        // 1. Gather orders for all active combat ships (pending orders, standing orders, or AI decisions)
        std::vector<Order> activeOrders;
        for (const auto& ship : ships) {
            if (!ship.isAlive || ship.retreated || !ship.activeInCombat) continue;

            auto it = std::find_if(pendingOrders.begin(), pendingOrders.end(),
                [&](const Order& o) { return o.fleetId == ship.fleetId; });

            if (it != pendingOrders.end()) {
                activeOrders.push_back(*it);
            } else {
                auto standIt = standingOrders.find(ship.fleetId);
                if (standIt != standingOrders.end()) {
                    Order stand = standIt->second;
                    // Validate target
                    const Ship* curTarget = getShip(stand.targetFleetId);
                    if (curTarget && (!curTarget->isAlive || curTarget->retreated || !curTarget->activeInCombat)) {
                        stand.targetFleetId = 0; // Clear destroyed target for retargeting
                    }
                    activeOrders.push_back(stand);
                } else {
                    // Autonomous AI decision for ships without explicit order
                    Order aiOrder;
                    aiOrder.fleetId = ship.fleetId;

                    // AI Pick maneuver
                    if (ship.hull * 4 <= ship.maxHull && ship.canRetreat) {
                        aiOrder.maneuver = TacticalManeuver::Retreat;
                    } else if (ship.beamAttack == 0 && ship.torpedoAttack == 0 && ship.canRetreat) {
                        aiOrder.maneuver = TacticalManeuver::Retreat;
                    } else if (ship.torpedoAttack > ship.beamAttack * 2) {
                        aiOrder.maneuver = TacticalManeuver::Circle;
                    } else if (ship.beamAttack > ship.torpedoAttack * 2) {
                        aiOrder.maneuver = TacticalManeuver::Charge;
                    } else {
                        aiOrder.maneuver = TacticalManeuver::Engage;
                    }

                    // Pick target: living active hostile with lowest hull + shields (tie break closest)
                    // Disregard cloaked targets unless no other hostiles exist
                    const Ship* bestTarget = nullptr;
                    int bestTotalEhp = std::numeric_limits<int>::max();
                    int bestDist = std::numeric_limits<int>::max();

                    for (const auto& other : ships) {
                        if (!other.isAlive || other.retreated || !other.activeInCombat || other.ownerId == ship.ownerId) continue;
                        if (other.isCloaked) continue; // Cannot auto-target cloaked vessel
                        const int ehp = other.hull + other.shields;
                        const int dist = std::max(std::abs(ship.position.x - other.position.x),
                                                  std::abs(ship.position.y - other.position.y));
                        if (ehp < bestTotalEhp || (ehp == bestTotalEhp && dist < bestDist)) {
                            bestTotalEhp = ehp;
                            bestDist = dist;
                            bestTarget = &other;
                        }
                    }

                    // Fallback to cloaked if all hostiles cloaked
                    if (!bestTarget) {
                        for (const auto& other : ships) {
                            if (!other.isAlive || other.retreated || !other.activeInCombat || other.ownerId == ship.ownerId) continue;
                            const int ehp = other.hull + other.shields;
                            const int dist = std::max(std::abs(ship.position.x - other.position.x),
                                                      std::abs(ship.position.y - other.position.y));
                            if (ehp < bestTotalEhp || (ehp == bestTotalEhp && dist < bestDist)) {
                                bestTotalEhp = ehp;
                                bestDist = dist;
                                bestTarget = &other;
                            }
                        }
                    }

                    aiOrder.targetFleetId = bestTarget ? bestTarget->fleetId : 0;
                    activeOrders.push_back(aiOrder);
                }
            }
        }

        RoundResult round;
        round.roundIndex = currentRound;

        std::vector<ShipRoundStatus> statuses;
        statuses.reserve(ships.size());
        for (const auto& s : ships) {
            ShipRoundStatus st;
            st.fleetId = s.fleetId;
            st.ownerId = s.ownerId;
            st.hullBefore = s.hull;
            st.shieldBefore = s.shields;
            st.positionBefore = s.position;
            statuses.push_back(st);
        }

        auto getStatus = [&](int fleetId) -> ShipRoundStatus* {
            for (auto& st : statuses) {
                if (st.fleetId == fleetId) return &st;
            }
            return nullptr;
        };

        // 1b. Check for Cloak Decloak & Hail triggers
        int hailCountOwner1 = 0, totalActiveOwner1 = 0;
        int hailCountOwner2 = 0, totalActiveOwner2 = 0;

        for (const auto& ord : activeOrders) {
            Ship* s = getShip(ord.fleetId);
            if (!s || !s->isAlive || s->retreated || !s->activeInCombat) continue;

            if (s->ownerId == firstOwner) totalActiveOwner1++;
            else totalActiveOwner2++;

            if (ord.maneuver == TacticalManeuver::Hail) {
                if (s->ownerId == firstOwner) hailCountOwner1++;
                else hailCountOwner2++;
                ShipRoundStatus* st = getStatus(s->fleetId);
                if (st) st->hailedThisRound = true;
            }

            // Decloak trigger: assigning an offensive tactic decloaks the ship (Manual p. 64)
            if (s->isCloaked) {
                if (ord.maneuver == TacticalManeuver::Engage || ord.maneuver == TacticalManeuver::Charge ||
                    ord.maneuver == TacticalManeuver::Assault || ord.maneuver == TacticalManeuver::Harry ||
                    ord.maneuver == TacticalManeuver::Flyby || ord.maneuver == TacticalManeuver::Strafe ||
                    ord.maneuver == TacticalManeuver::Ram) {
                    s->isCloaked = false;
                    ShipRoundStatus* st = getStatus(s->fleetId);
                    if (st) st->decloakedThisRound = true;
                }
            }
        }

        // Check mutual hail truce
        if (totalActiveOwner1 > 0 && totalActiveOwner2 > 0 &&
            hailCountOwner1 == totalActiveOwner1 && hailCountOwner2 == totalActiveOwner2) {
            round.hailedTruce = true;
            isFinished = true;
            winnerOwnerId = 0;
            round.encounterEnded = true;
            round.winnerOwnerId = 0;
        }

        // 2. Maneuver & Movement Phase
        for (const auto& ord : activeOrders) {
            Ship* s = getShip(ord.fleetId);
            if (!s || !s->isAlive || s->retreated || !s->activeInCombat) continue;

            ShipRoundStatus* st = getStatus(s->fleetId);
            if (st) {
                st->maneuverExecuted = ord.maneuver;
                st->targetFleetId = ord.targetFleetId;
                st->intermediatePosition = s->position;
            }

            if (ord.maneuver == TacticalManeuver::Retreat) {
                s->retreatSteps++;
                // Move towards deployment edge
                const int retreatTargetX = (s->ownerId == firstOwner) ? 0 : (arenaWidth - 1);
                const int dx = (retreatTargetX > s->position.x) - (retreatTargetX < s->position.x);
                s->position.x += dx * s->speed;
                s->position.x = std::clamp(s->position.x, 0, arenaWidth - 1);

                if (st) {
                    st->facingYaw = (s->ownerId == firstOwner) ? 180 : 0;
                }

                // If reached boundary or retreated for at least 2 rounds, disengage!
                if (s->position.x == retreatTargetX || s->retreatSteps >= 2) {
                    s->retreated = true;
                    s->activeInCombat = false;
                    round.retreatedFleetIds.push_back(s->fleetId);
                    if (st) st->retreatedThisRound = true;
                }
            } else if (ord.maneuver == TacticalManeuver::Hold) {
                // Stationary: shield recharge boost (+10% max shields)
                const int recharge = std::max(1, s->maxShields / 10);
                s->shields = std::min(s->maxShields, s->shields + recharge);
                if (st) {
                    st->facingYaw = (s->ownerId == firstOwner) ? 0 : 180;
                }
            } else if (ord.maneuver == TacticalManeuver::Hail) {
                // Stationary defensive stance for peaceful hail
                if (st) {
                    st->facingYaw = (s->ownerId == firstOwner) ? 0 : 180;
                }
            } else {
                // Find effective movement target
                const Ship* target = getShip(ord.targetFleetId);
                if (!target || !target->isAlive || target->retreated || !target->activeInCombat) {
                    // Auto-target closest living active hostile
                    int minD = std::numeric_limits<int>::max();
                    for (const auto& other : ships) {
                        if (!other.isAlive || other.retreated || !other.activeInCombat || other.ownerId == s->ownerId) continue;
                        const int d = std::max(std::abs(s->position.x - other.position.x),
                                               std::abs(s->position.y - other.position.y));
                        if (d < minD) { minD = d; target = &other; }
                    }
                }

                if (target) {
                    int desiredDist = 2; // Default Engage optimal range
                    if (ord.maneuver == TacticalManeuver::Charge || ord.maneuver == TacticalManeuver::Strafe) {
                        desiredDist = 1; // Point-blank / close
                    } else if (ord.maneuver == TacticalManeuver::Circle) {
                        desiredDist = 4; // Standoff orbit
                    } else if (ord.maneuver == TacticalManeuver::Assault) {
                        desiredDist = 2; // Disciplined medium range
                    } else if (ord.maneuver == TacticalManeuver::Harry) {
                        desiredDist = 1; // Close harassment
                    } else if (ord.maneuver == TacticalManeuver::Flyby) {
                        desiredDist = 2; // Passing medium run
                    } else if (ord.maneuver == TacticalManeuver::Evade) {
                        desiredDist = 5; // Long range standoff
                    } else if (ord.maneuver == TacticalManeuver::Ram) {
                        desiredDist = 0; // Collision course
                    }

                    int steps = s->speed;
                    // Flyby / Strafe speed surge (+1 step during passing attack)
                    if (ord.maneuver == TacticalManeuver::Flyby || ord.maneuver == TacticalManeuver::Strafe) {
                        steps += 1;
                    }

                    while (steps > 0) {
                        const int curDist = std::max(std::abs(s->position.x - target->position.x),
                                                     std::abs(s->position.y - target->position.y));
                        if (curDist > desiredDist) {
                            // Advance toward target
                            if (s->position.x < target->position.x) s->position.x++;
                            else if (s->position.x > target->position.x) s->position.x--;
                            if (s->position.y < target->position.y) s->position.y++;
                            else if (s->position.y > target->position.y) s->position.y--;
                        } else if (curDist < desiredDist) {
                            // Back up / stand off
                            if (s->position.x < target->position.x && s->position.x > 0) s->position.x--;
                            else if (s->position.x > target->position.x && s->position.x < arenaWidth - 1) s->position.x++;
                            if (s->position.y < target->position.y && s->position.y > 0) s->position.y--;
                            else if (s->position.y > target->position.y && s->position.y < arenaHeight - 1) s->position.y++;
                        } else {
                            // Orbit / pass / harry
                            if (ord.maneuver == TacticalManeuver::Circle || ord.maneuver == TacticalManeuver::Harry) {
                                if (s->position.y + 1 < arenaHeight) s->position.y++;
                                else if (s->position.y - 1 >= 0) s->position.y--;
                            } else if (ord.maneuver == TacticalManeuver::Flyby || ord.maneuver == TacticalManeuver::Strafe) {
                                // Continue past target along heading
                                const int forwardDir = (s->ownerId == firstOwner) ? 1 : -1;
                                s->position.x = std::clamp(s->position.x + forwardDir, 0, arenaWidth - 1);
                            }
                            break;
                        }
                        s->position.x = std::clamp(s->position.x, 0, arenaWidth - 1);
                        s->position.y = std::clamp(s->position.y, 0, arenaHeight - 1);
                        steps--;
                    }

                    if (st) {
                        const int dx = target->position.x - s->position.x;
                        const int dy = target->position.y - s->position.y;
                        const double angle = std::atan2(static_cast<double>(dy), static_cast<double>(dx)) * 180.0 / 3.141592653589793;
                        st->facingYaw = static_cast<int>(angle + 360.0) % 360;
                    }
                }
            }
        }

        // 3. Firing & Salvo Phase (Simultaneous fire)
        struct Salvo {
            int attackerFleetId = 0;
            int defenderFleetId = 0;
            int beamDamage = 0;
            int torpedoDamage = 0;
            bool isRam = false;
        };
        std::vector<Salvo> plannedSalvos;

        for (const auto& ord : activeOrders) {
            Ship* attacker = getShip(ord.fleetId);
            if (!attacker || !attacker->isAlive || attacker->retreated || !attacker->activeInCombat ||
                ord.maneuver == TacticalManeuver::Retreat || ord.maneuver == TacticalManeuver::Hail) {
                continue;
            }

            // Find target
            const Ship* target = getShip(ord.targetFleetId);
            if (!target || !target->isAlive || target->retreated || !target->activeInCombat) {
                // Auto-retarget closest active enemy
                int minD = std::numeric_limits<int>::max();
                for (const auto& other : ships) {
                    if (!other.isAlive || other.retreated || !other.activeInCombat || other.ownerId == attacker->ownerId) continue;
                    if (other.isCloaked) continue; // Cloaked ships cannot be targeted
                    const int d = std::max(std::abs(attacker->position.x - other.position.x),
                                           std::abs(attacker->position.y - other.position.y));
                    if (d < minD) { minD = d; target = &other; }
                }
            }
            if (!target) continue;

            const int dist = std::max(std::abs(attacker->position.x - target->position.x),
                                      std::abs(attacker->position.y - target->position.y));

            // Ram execution: if reached point-blank / collision cell, inflict kinetic ramming damage
            if (ord.maneuver == TacticalManeuver::Ram && dist <= 1) {
                const int ramDmg = attacker->hull; // Impact damage proportional to attacker hull
                Salvo s;
                s.attackerFleetId = attacker->fleetId;
                s.defenderFleetId = target->fleetId;
                s.beamDamage = ramDmg;
                s.torpedoDamage = 0;
                s.isRam = true;
                plannedSalvos.push_back(s);

                ShipRoundStatus* st = getStatus(attacker->fleetId);
                if (st) st->beamDamageDealt = ramDmg;
                continue;
            }

            // Beam weapon calculation (optimal at range 1-2, dropoff beyond)
            int beamDmg = 0;
            if (attacker->beamAttack > 0) {
                if (dist == 1) {
                    beamDmg = attacker->beamAttack;
                } else if (dist == 2) {
                    beamDmg = (attacker->beamAttack * 85) / 100;
                } else if (dist == 3) {
                    beamDmg = (attacker->beamAttack * 60) / 100;
                } else if (dist == 4) {
                    beamDmg = (attacker->beamAttack * 35) / 100;
                } else {
                    beamDmg = 0; // Out of beam range
                }

                // Attacker maneuver modifiers
                if (ord.maneuver == TacticalManeuver::Charge) {
                    beamDmg = (beamDmg * 120) / 100; // +20% beam burst on charge
                } else if (ord.maneuver == TacticalManeuver::Strafe) {
                    beamDmg = (beamDmg * 125) / 100; // +25% beam burst on close strafe
                } else if (ord.maneuver == TacticalManeuver::Circle) {
                    beamDmg = (beamDmg * 85) / 100;  // -15% beam penalty while circling
                } else if (ord.maneuver == TacticalManeuver::Harry) {
                    beamDmg = (beamDmg * 90) / 100;  // -10% beam penalty while harrying
                } else if (ord.maneuver == TacticalManeuver::Evade) {
                    beamDmg = (beamDmg * 70) / 100;  // -30% defensive posture penalty
                }

                // Decloak ambush bonus (+25% surprise strike)
                ShipRoundStatus* st = getStatus(attacker->fleetId);
                if (st && st->decloakedThisRound) {
                    beamDmg = (beamDmg * 125) / 100;
                }
            }

            // Torpedo weapon calculation (optimal at range 2-5, arming penalty at range 1)
            int torpDmg = 0;
            if (attacker->torpedoAttack > 0) {
                if (dist <= 1) {
                    torpDmg = (attacker->torpedoAttack * 40) / 100; // Minimum arming range penalty
                } else if (dist <= 5) {
                    torpDmg = attacker->torpedoAttack;
                } else if (dist == 6) {
                    torpDmg = (attacker->torpedoAttack * 80) / 100;
                } else if (dist == 7) {
                    torpDmg = (attacker->torpedoAttack * 50) / 100;
                } else {
                    torpDmg = 0; // Out of torpedo range
                }

                if (ord.maneuver == TacticalManeuver::Strafe) {
                    torpDmg = (torpDmg * 80) / 100; // -20% high speed tracking penalty
                } else if (ord.maneuver == TacticalManeuver::Evade) {
                    torpDmg = (torpDmg * 70) / 100;
                }

                ShipRoundStatus* st = getStatus(attacker->fleetId);
                if (st && st->decloakedThisRound) {
                    torpDmg = (torpDmg * 125) / 100;
                }
            }

            // Deterministic dispersion variation (+/- 5%)
            if (beamDmg > 0) {
                const int var = rng.range(-5, 5);
                beamDmg = std::max(1, (beamDmg * (100 + var)) / 100);
            }
            if (torpDmg > 0) {
                const int var = rng.range(-5, 5);
                torpDmg = std::max(1, (torpDmg * (100 + var)) / 100);
            }

            Salvo s;
            s.attackerFleetId = attacker->fleetId;
            s.defenderFleetId = target->fleetId;
            s.beamDamage = beamDmg;
            s.torpedoDamage = torpDmg;
            s.isRam = false;
            plannedSalvos.push_back(s);

            ShipRoundStatus* st = getStatus(attacker->fleetId);
            if (st) {
                st->beamDamageDealt = beamDmg;
                st->torpedoDamageDealt = torpDmg;
            }
        }

        // 4. Apply damage simultaneously to defenders (Shields before Hull)
        std::map<int, int> incomingDamage;
        for (const auto& salvo : plannedSalvos) {
            incomingDamage[salvo.defenderFleetId] += (salvo.beamDamage + salvo.torpedoDamage);
            round.salvos.push_back({salvo.attackerFleetId, salvo.defenderFleetId, salvo.beamDamage, salvo.torpedoDamage, false});
        }

        for (auto& ship : ships) {
            if (!ship.isAlive || ship.retreated || !ship.activeInCombat) continue;

            auto it = incomingDamage.find(ship.fleetId);
            int rawDamage = (it != incomingDamage.end()) ? it->second : 0;

            // Defender maneuver defensive modifier
            Order defOrder = getOrder(ship.fleetId);
            if (defOrder.maneuver == TacticalManeuver::Circle) {
                rawDamage = (rawDamage * 75) / 100; // -25% incoming damage due to evasion
            } else if (defOrder.maneuver == TacticalManeuver::Charge) {
                rawDamage = (rawDamage * 115) / 100; // +15% incoming damage due to direct approach
            } else if (defOrder.maneuver == TacticalManeuver::Hold) {
                rawDamage = (rawDamage * 90) / 100;  // -10% incoming damage (reinforced shields)
            } else if (defOrder.maneuver == TacticalManeuver::Harry) {
                rawDamage = (rawDamage * 80) / 100;  // -20% incoming damage (evasive close circling)
            } else if (defOrder.maneuver == TacticalManeuver::Flyby) {
                rawDamage = (rawDamage * 85) / 100;  // -15% incoming damage (high-speed pass)
            } else if (defOrder.maneuver == TacticalManeuver::Strafe) {
                rawDamage = (rawDamage * 110) / 100; // +10% exposure on close strafe
            } else if (defOrder.maneuver == TacticalManeuver::Evade) {
                rawDamage = (rawDamage * 65) / 100;  // -35% incoming damage (maximum evasive stance)
            }

            // Cloak defense: cloaked ships take -50% area damage
            if (ship.isCloaked) {
                rawDamage = (rawDamage * 50) / 100;
            }

            ShipRoundStatus* st = getStatus(ship.fleetId);
            if (st) {
                st->totalDamageTaken = rawDamage;
            }

            // Shields absorb first
            const int absorbed = std::min(ship.shields, rawDamage);
            ship.shields -= absorbed;
            const int remainingDamage = rawDamage - absorbed;

            // Remainder penetrates to hull
            ship.hull = std::max(0, ship.hull - remainingDamage);

            if (ship.hull == 0) {
                ship.isAlive = false;
                ship.activeInCombat = false;
                round.lostFleetIds.push_back(ship.fleetId);
                if (st) st->destroyedThisRound = true;
            }
        }

        // 5. Finalize status records & round damages
        for (auto& st : statuses) {
            const Ship* s = getShip(st.fleetId);
            if (s) {
                st.hullAfter = s->hull;
                st.shieldAfter = s->shields;
                st.positionAfter = s->position;

                if (st.hullBefore != st.hullAfter || st.shieldBefore != st.shieldAfter) {
                    round.damage.push_back({
                        st.fleetId, st.hullBefore, st.hullAfter, st.shieldBefore, st.shieldAfter
                    });
                }
            }
        }
        round.shipStatuses = statuses;

        // 6. Check termination conditions
        if (!round.hailedTruce) {
            std::map<int, int> activeShipsByOwner;
            for (const auto& s : ships) {
                if (s.isAlive && !s.retreated && s.activeInCombat) {
                    activeShipsByOwner[s.ownerId]++;
                }
            }

            if (activeShipsByOwner.size() == 1) {
                isFinished = true;
                winnerOwnerId = activeShipsByOwner.begin()->first;
                round.encounterEnded = true;
                round.winnerOwnerId = winnerOwnerId;
            } else if (activeShipsByOwner.empty()) {
                isFinished = true;
                winnerOwnerId = 0; // Mutual destruction or all retreated
                round.encounterEnded = true;
                round.winnerOwnerId = 0;
            } else if (currentRound >= maxRounds) {
                // Hard round limit reached: disengage stalemate
                isFinished = true;
                round.encounterEnded = true;
                int bestEhp = -1;
                int leaderOwner = 0;
                bool tie = false;
                for (const auto& [owner, count] : activeShipsByOwner) {
                    int totalEhp = 0;
                    for (const auto& s : ships) {
                        if (s.ownerId == owner && s.isAlive && !s.retreated && s.activeInCombat) {
                            totalEhp += s.hull + s.shields;
                        }
                    }
                    if (totalEhp > bestEhp) {
                        bestEhp = totalEhp;
                        leaderOwner = owner;
                        tie = false;
                    } else if (totalEhp == bestEhp) {
                        tie = true;
                    }
                }
                winnerOwnerId = tie ? 0 : leaderOwner;
                round.winnerOwnerId = winnerOwnerId;
            }
        }

        rngState = rng.state;
        rounds.push_back(round);
        pendingOrders.clear();

        return round;
    }

    RoundResult runUntilFinished() {
        RoundResult lastRound;
        while (!isFinished) {
            lastRound = resolveRound();
        }
        return lastRound;
    }
};

// -----------------------------------------------------------------------------
// Construction & Integration Functions
// -----------------------------------------------------------------------------

inline EmpireState& mutableEmpireState(GameState& s, int id) {
    if (id == s.playerEmpire) return s;
    for (auto& r : s.rivals) {
        if (r.id == id) return r;
    }
    throw std::invalid_argument("Empire ID " + std::to_string(id) + " not found in GameState.");
}

inline void revealSector(GameState& s, int x, int y, int ownerId) {
    auto& knowledge = mutableEmpireState(s, ownerId).explored;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int px = x + dx, py = y + dy;
            if (px < 0 || py < 0 || px >= s.width || py >= s.height) continue;
            const int cell = py * s.width + px;
            const auto it = std::lower_bound(knowledge.begin(), knowledge.end(), cell);
            if (it == knowledge.end() || *it != cell) {
                knowledge.insert(it, cell);
            }
        }
    }
}

inline Encounter constructEncounter(const GameState& state,
                                    const std::vector<int>& participantFleetIds,
                                    std::uint64_t seed = 0) {
    if (participantFleetIds.size() < 2) {
        throw std::invalid_argument("Tactical encounter requires at least two participant fleet IDs.");
    }

    std::vector<int> sortedIds = participantFleetIds;
    std::sort(sortedIds.begin(), sortedIds.end());
    if (std::adjacent_find(sortedIds.begin(), sortedIds.end()) != sortedIds.end()) {
        throw std::invalid_argument("Duplicate participant fleet IDs are not permitted.");
    }

    std::vector<const Fleet*> matched;
    matched.reserve(participantFleetIds.size());
    for (int id : participantFleetIds) {
        auto it = std::find_if(state.fleets.begin(), state.fleets.end(),
            [id](const Fleet& f) { return f.id == id; });
        if (it == state.fleets.end()) {
            throw std::invalid_argument("Fleet ID " + std::to_string(id) + " not found in GameState.");
        }
        if (it->hull <= 0) {
            throw std::invalid_argument("Fleet ID " + std::to_string(id) + " has 0 hull (destroyed).");
        }
        matched.push_back(&*it);
    }

    const int sectorX = matched[0]->x;
    const int sectorY = matched[0]->y;
    for (const Fleet* f : matched) {
        if (f->x != sectorX || f->y != sectorY) {
            throw std::invalid_argument("All participant fleets must occupy the same sector.");
        }
    }

    // Determine unique owners
    std::set<int> uniqueOwners;
    for (const Fleet* f : matched) {
        uniqueOwners.insert(f->ownerId);
    }
    if (uniqueOwners.size() != 2) {
        throw std::invalid_argument("Tactical encounter requires exactly 2 distinct empire owners (found " +
            std::to_string(uniqueOwners.size()) + ").");
    }

    auto ownerIt = uniqueOwners.begin();
    const int ownerA = *ownerIt++;
    const int ownerB = *ownerIt;

    // Requirement 2: Must be at war. Peaceful combat is rejected.
    const auto* relation = majorRelation(state, ownerA, ownerB);
    if (!relation || !relation->atWar) {
        throw std::invalid_argument("Empires " + std::to_string(ownerA) + " and " + std::to_string(ownerB) +
            " are not at war; peaceful tactical combat is rejected.");
    }

    // Canonicalize deterministic deployment:
    const int firstOwner = std::min(ownerA, ownerB);

    // Canonicalize fleet ordering: owner side first, then fleetId
    std::sort(matched.begin(), matched.end(), [&](const Fleet* a, const Fleet* b) {
        if (a->ownerId != b->ownerId) {
            return a->ownerId < b->ownerId;
        }
        return a->id < b->id;
    });

    Encounter enc;
    enc.arenaWidth = 16;
    enc.arenaHeight = 12;
    enc.currentRound = 0;
    enc.maxRounds = 50;
    enc.isFinished = false;
    enc.winnerOwnerId = 0;
    enc.initialSeed = (seed != 0) ? seed : (state.rngState ^ (static_cast<std::uint64_t>(sectorX) << 16) ^ sectorY);
    enc.rngState = enc.initialSeed;
    enc.sectorX = sectorX;
    enc.sectorY = sectorY;
    enc.strategicTurn = state.turn;
    enc.controllingOwnerId = state.playerEmpire;
    enc.resultsApplied = false;

    // Helper to evaluate if a safe supplied retreat sector exists
    auto hasSafeEscape = [&](const Fleet& f) {
        for (const auto [dx, dy] : {std::pair{-1,-1}, std::pair{1,-1}, std::pair{-1,1}, std::pair{1,1},
                                    std::pair{-1,0}, std::pair{1,0}, std::pair{0,-1}, std::pair{0,1}}) {
            const int px = f.x + dx, py = f.y + dy;
            if (px < 0 || py < 0 || px >= state.width || py >= state.height || !supplied(state, f, px, py)) continue;
            const bool hostile = std::any_of(state.fleets.begin(), state.fleets.end(), [&](const Fleet& other) {
                const auto rel = majorRelation(state, f.ownerId, other.ownerId);
                return other.hull > 0 && other.x == px && other.y == py && rel && rel->atWar;
            });
            if (!hostile) return true;
        }
        return false;
    };

    int side1Index = 0;
    int side2Index = 0;

    for (const Fleet* f : matched) {
        Ship s;
        s.fleetId = f->id;
        s.ownerId = f->ownerId;
        s.name = fleetName(*f);
        s.shipClass = f->shipClass;
        s.referenceShipId = f->referenceShipId;
        s.maxHull = fleetMaxHull(*f);
        s.hull = std::min(f->hull, s.maxHull);
        s.initialHull = s.hull; // Capture actual initial starting hull
        s.maxShields = fleetMaxShields(*f);
        s.shields = std::min(f->shields, s.maxShields);
        s.initialShields = s.shields; // Capture actual initial starting shields
        s.canRetreat = hasSafeEscape(*f);

        // Attack derivation & beam/torpedo split
        int totalAtk = fleetAttack(*f);
        const auto& emp = empire(state, f->ownerId);
        totalAtk += emp.technologies[5].level;
        totalAtk = std::max(0, totalAtk);

        if (f->shipClass == Building::Transport || f->shipClass == Building::ColonyShip) {
            s.beamAttack = totalAtk;
            s.torpedoAttack = 0;
        } else if (f->shipClass == Building::Scout) {
            s.beamAttack = (totalAtk * 75) / 100;
            s.torpedoAttack = totalAtk - s.beamAttack;
        } else if (f->shipClass == Building::Destroyer) {
            s.beamAttack = (totalAtk * 65) / 100;
            s.torpedoAttack = totalAtk - s.beamAttack;
        } else {
            // Check ReferenceCatalogue for strike cruiser (role == 3)
            if (f->referenceShipId >= 0 &&
                f->referenceShipId < static_cast<int>(referenceData::ShipCatalogue.size()) &&
                referenceData::ShipCatalogue[f->referenceShipId].role == 3) {
                s.torpedoAttack = (totalAtk * 65) / 100;
                s.beamAttack = totalAtk - s.torpedoAttack;
            } else {
                s.beamAttack = (totalAtk * 50) / 100;
                s.torpedoAttack = totalAtk - s.beamAttack;
            }
        }

        // Speed determination
        if (f->shipClass == Building::Scout) s.speed = 3;
        else if (f->shipClass == Building::Destroyer) s.speed = 2;
        else if (f->shipClass == Building::Transport || f->shipClass == Building::ColonyShip) s.speed = 1;
        else s.speed = 2;

        if (f->referenceShipId >= 0 &&
            f->referenceShipId < static_cast<int>(referenceData::ShipCatalogue.size())) {
            const double catSpd = referenceData::ShipCatalogue[f->referenceShipId].speed;
            if (catSpd >= 3.0) s.speed = 3;
            else if (catSpd >= 2.0) s.speed = 2;
            else s.speed = 1;
        }

        // Deterministic deployment positioning
        if (f->ownerId == firstOwner) {
            s.position.x = 1;
            s.position.y = 2 + (side1Index * 2) % (enc.arenaHeight - 4);
            side1Index++;
        } else {
            s.position.x = enc.arenaWidth - 2;
            s.position.y = 2 + (side2Index * 2) % (enc.arenaHeight - 4);
            side2Index++;
        }

        s.isAlive = (s.hull > 0);
        s.activeInCombat = s.isAlive;
        s.retreated = false;
        s.retreatSteps = 0;

        // Cloak capability: Klingon (ownerId == 2) and Romulan (ownerId == 3) combat vessels start cloaked
        if ((f->ownerId == 2 || f->ownerId == 3) &&
            f->shipClass != Building::Transport && f->shipClass != Building::ColonyShip) {
            s.canCloak = true;
            s.isCloaked = true;
        }

        enc.ships.push_back(std::move(s));
    }

    // Regroup strategic task forces into like-class tactical groups (Manual pp. 63-65)
    std::map<std::pair<int, TacticalGroupClass>, std::vector<int>> classGroups;
    for (const auto& s : enc.ships) {
        TacticalGroupClass gc = TacticalGroupClass::Command;
        if (s.shipClass == Building::Transport || s.shipClass == Building::ColonyShip) {
            gc = TacticalGroupClass::Support;
        } else if (s.shipClass == Building::Scout) {
            gc = TacticalGroupClass::Scout;
        } else if (s.shipClass == Building::Destroyer) {
            gc = TacticalGroupClass::FastAttack;
        } else {
            gc = TacticalGroupClass::Command;
        }
        classGroups[{s.ownerId, gc}].push_back(s.fleetId);
    }

    int nextGroupId = 1;
    for (auto& [key, fids] : classGroups) {
        TacticalGroup grp;
        grp.groupId = nextGroupId++;
        grp.ownerId = key.first;
        grp.groupClass = key.second;
        grp.fleetIds = fids;
        grp.name = std::string(tacticalGroupClassName(key.second)) + " Group " + std::to_string(grp.groupId);
        grp.standingManeuver = (key.second == TacticalGroupClass::Support) ? TacticalManeuver::Retreat : TacticalManeuver::Engage;

        for (int fid : fids) {
            Ship* s = enc.getShip(fid);
            if (s) s->groupId = grp.groupId;
        }
        enc.groups.push_back(std::move(grp));
    }

    return enc;
}

inline CombatReport toCombatReport(const Encounter& encounter) {
    // Existing report supports exactly 2 owners
    std::set<int> owners;
    for (const auto& s : encounter.ships) owners.insert(s.ownerId);
    if (owners.size() > 2) {
        throw std::invalid_argument("CombatReport does not support 3+ empire owners.");
    }

    CombatReport report;
    report.turn = encounter.strategicTurn;
    report.x = encounter.sectorX;
    report.y = encounter.sectorY;
    report.winnerOwner = encounter.winnerOwnerId;

    if (!encounter.ships.empty()) {
        report.firstOwner = encounter.ships[0].ownerId;
        for (const auto& s : encounter.ships) {
            if (s.ownerId != report.firstOwner) {
                report.secondOwner = s.ownerId;
                break;
            }
        }
    }

    for (const auto& s : encounter.ships) {
        if (s.ownerId == report.firstOwner) {
            report.firstFleetIds.push_back(s.fleetId);
        } else {
            report.secondFleetIds.push_back(s.fleetId);
        }

        // Capture initialHull / initialShields per ship rather than maximum
        report.participants.push_back({
            s.fleetId, s.ownerId, s.shipClass, s.referenceShipId, s.initialHull, s.initialShields
        });

        // Retreated ships must NOT be reported as lost!
        if (s.retreated) {
            report.retreatedFleetIds.push_back(s.fleetId);
        } else if (!s.isAlive || s.hull <= 0) {
            report.lostFleetIds.push_back(s.fleetId);
        }

        report.damage.push_back({
            s.fleetId, s.initialHull, s.hull, s.initialShields, s.shields
        });
    }

    for (const auto& r : encounter.rounds) {
        CombatRound cr;
        cr.index = r.roundIndex;
        cr.damage = r.damage;
        cr.lostFleetIds = r.lostFleetIds;
        cr.retreatedFleetIds = r.retreatedFleetIds;
        report.rounds.push_back(std::move(cr));
    }

    return report;
}

inline void applyEncounterResults(GameState& state, const Encounter& encounter) {
    if (!encounter.isFinished) {
        throw std::invalid_argument("Cannot apply tactical results: encounter is not finished.");
    }
    if (encounter.resultsApplied) {
        throw std::invalid_argument("Cannot apply tactical results: encounter results have already been consumed/applied.");
    }

    // Stale turn check
    if (state.turn != encounter.strategicTurn) {
        throw std::invalid_argument("Stale turn: GameState turn " + std::to_string(state.turn) +
            " does not match encounter strategic turn " + std::to_string(encounter.strategicTurn));
    }

    // Atomic validation of location, owner, ship identity, and initial HP
    for (const auto& ship : encounter.ships) {
        auto it = std::find_if(state.fleets.begin(), state.fleets.end(),
            [&](const Fleet& f) { return f.id == ship.fleetId; });
        if (it == state.fleets.end()) {
            throw std::invalid_argument("Fleet ID " + std::to_string(ship.fleetId) + " not found in GameState.");
        }
        if (it->ownerId != ship.ownerId) {
            throw std::invalid_argument("Owner mismatch for fleet ID " + std::to_string(ship.fleetId) +
                ": expected " + std::to_string(ship.ownerId) + ", found " + std::to_string(it->ownerId));
        }
        if (it->x != encounter.sectorX || it->y != encounter.sectorY) {
            throw std::invalid_argument("Stale location for fleet ID " + std::to_string(ship.fleetId) +
                ": fleet moved since encounter was constructed.");
        }
        if (it->hull != ship.initialHull || it->shields != ship.initialShields) {
            throw std::invalid_argument("Stale initial HP for fleet ID " + std::to_string(ship.fleetId) +
                ": fleet was repaired or damaged concurrently in GameState.");
        }
    }

    // Helper for finding safe withdrawal sector
    auto safeSector = [&](const Fleet& f, int px, int py) {
        if (px < 0 || py < 0 || px >= state.width || py >= state.height || !supplied(state, f, px, py)) return false;
        return std::none_of(state.fleets.begin(), state.fleets.end(), [&](const Fleet& other) {
            const auto enemy = majorRelation(state, f.ownerId, other.ownerId);
            return other.hull > 0 && other.x == px && other.y == py && enemy && enemy->atWar;
        });
    };

    // Update fleet states
    for (const auto& ship : encounter.ships) {
        auto it = std::find_if(state.fleets.begin(), state.fleets.end(),
            [&](const Fleet& f) { return f.id == ship.fleetId; });
        if (it == state.fleets.end()) continue;

        if (ship.retreated) {
            it->hull = ship.hull;
            it->shields = ship.shields;
            it->attackTarget = 0;
            it->invadeSystemId = 0;

            // Strategic withdrawal relocation towards nearest owned colony or adjacent safe sector
            const System* home = nullptr;
            int bestDist = std::numeric_limits<int>::max();
            for (const auto& c : state.colonies) {
                if (c.ownerId == it->ownerId) {
                    auto loc = std::find_if(state.systems.begin(), state.systems.end(),
                        [&](const System& s) { return s.id == c.systemId; });
                    if (loc != state.systems.end()) {
                        const int dist = std::max(std::abs(it->x - loc->x), std::abs(it->y - loc->y));
                        if (dist > 0 && dist < bestDist) {
                            bestDist = dist;
                            home = &*loc;
                        }
                    }
                }
            }

            int retreatX = it->x, retreatY = it->y;
            if (home) {
                const int candX = it->x + (home->x > it->x) - (home->x < it->x);
                const int candY = it->y + (home->y > it->y) - (home->y < it->y);
                if (safeSector(*it, candX, candY)) {
                    retreatX = candX;
                    retreatY = candY;
                }
            }
            if (retreatX == it->x && retreatY == it->y) {
                for (const auto [dx, dy] : {std::pair{-1,-1}, std::pair{1,-1}, std::pair{-1,1}, std::pair{1,1},
                                            std::pair{-1,0}, std::pair{1,0}, std::pair{0,-1}, std::pair{0,1}}) {
                    if (safeSector(*it, it->x + dx, it->y + dy)) {
                        retreatX = it->x + dx;
                        retreatY = it->y + dy;
                        break;
                    }
                }
            }

            // Reject/cancel retreat when no safe supplied adjacent sector exists rather than reporting into same hostile tile
            if (retreatX == it->x && retreatY == it->y) {
                it->retreat = false; // Cannot withdraw; retreat rejected/cancelled
            } else {
                it->x = retreatX;
                it->y = retreatY;
                it->retreat = true;
                if (home && (retreatX != home->x || retreatY != home->y)) {
                    it->targetX = home->x;
                    it->targetY = home->y;
                } else {
                    it->targetX = retreatX;
                    it->targetY = retreatY;
                }
                // Reveal relocated destination through campaign explored knowledge
                revealSector(state, retreatX, retreatY, it->ownerId);
            }
        } else if (!ship.isAlive || ship.hull <= 0) {
            it->hull = 0;
        } else {
            it->hull = ship.hull;
            it->shields = ship.shields;
        }
    }

    // Destroyed ships (hull <= 0) are removed from campaign fleets
    std::erase_if(state.fleets, [](const Fleet& f) { return f.hull <= 0; });

    // Mark encounter consumed
    encounter.resultsApplied = true;
}

// -----------------------------------------------------------------------------
// Serialization & Deserialization (Mid-battle save and replay)
// -----------------------------------------------------------------------------

inline std::string escapeString(const std::string& str) {
    std::ostringstream ss;
    for (char c : str) {
        if (c == ' ') ss << "\\s";
        else if (c == '\\') ss << "\\\\";
        else if (c == '\n') ss << "\\n";
        else ss << c;
    }
    return ss.str();
}

inline std::string unescapeString(const std::string& str) {
    std::string out;
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            char next = str[i + 1];
            if (next == 's') { out.push_back(' '); i++; }
            else if (next == '\\') { out.push_back('\\'); i++; }
            else if (next == 'n') { out.push_back('\n'); i++; }
            else { out.push_back(next); i++; }
        } else {
            out.push_back(str[i]);
        }
    }
    return out;
}

inline std::string serializeEncounter(const Encounter& encounter) {
    std::ostringstream ss;
    ss << "TACTICAL_ENCOUNTER_V1\n";
    ss << "ARENA " << encounter.arenaWidth << " " << encounter.arenaHeight << "\n";
    ss << "ROUNDS " << encounter.currentRound << " " << encounter.maxRounds << " "
       << (encounter.isFinished ? 1 : 0) << " " << encounter.winnerOwnerId << "\n";
    ss << "SECTOR " << encounter.sectorX << " " << encounter.sectorY << " " << encounter.strategicTurn << "\n";
    ss << "RNG " << encounter.initialSeed << " " << encounter.rngState << "\n";
    ss << "CONTROLLER " << encounter.controllingOwnerId << " " << (encounter.resultsApplied ? 1 : 0) << "\n";

    ss << "SHIPS " << encounter.ships.size() << "\n";
    for (const auto& s : encounter.ships) {
        ss << s.fleetId << " "
           << s.ownerId << " "
           << s.hull << " "
           << s.maxHull << " "
           << s.initialHull << " "
           << s.shields << " "
           << s.maxShields << " "
           << s.initialShields << " "
           << s.beamAttack << " "
           << s.torpedoAttack << " "
           << s.speed << " "
           << s.position.x << " "
           << s.position.y << " "
           << (s.isAlive ? 1 : 0) << " "
           << (s.activeInCombat ? 1 : 0) << " "
           << (s.retreated ? 1 : 0) << " "
           << s.retreatSteps << " "
           << (s.canRetreat ? 1 : 0) << " "
           << static_cast<int>(s.shipClass) << " "
           << s.referenceShipId << " "
           << escapeString(s.name) << "\n";
    }

    ss << "ORDERS " << encounter.pendingOrders.size() << "\n";
    for (const auto& o : encounter.pendingOrders) {
        ss << o.fleetId << " " << o.targetFleetId << " " << static_cast<int>(o.maneuver) << "\n";
    }

    ss << "ROUND_RESULTS " << encounter.rounds.size() << "\n";
    for (const auto& r : encounter.rounds) {
        ss << "ROUND " << r.roundIndex << " " << (r.encounterEnded ? 1 : 0) << " " << r.winnerOwnerId << "\n";
        ss << "STATUSES " << r.shipStatuses.size() << "\n";
        for (const auto& st : r.shipStatuses) {
            ss << st.fleetId << " "
               << st.ownerId << " "
               << st.hullBefore << " "
               << st.hullAfter << " "
               << st.shieldBefore << " "
               << st.shieldAfter << " "
               << st.beamDamageDealt << " "
               << st.torpedoDamageDealt << " "
               << st.totalDamageTaken << " "
               << st.positionBefore.x << " "
               << st.positionBefore.y << " "
               << st.positionAfter.x << " "
               << st.positionAfter.y << " "
               << static_cast<int>(st.maneuverExecuted) << " "
               << st.targetFleetId << " "
               << (st.destroyedThisRound ? 1 : 0) << " "
               << (st.retreatedThisRound ? 1 : 0) << "\n";
        }
        ss << "DAMAGE " << r.damage.size() << "\n";
        for (const auto& d : r.damage) {
            ss << d.fleetId << " " << d.hullBefore << " " << d.hullAfter << " "
               << d.shieldBefore << " " << d.shieldAfter << "\n";
        }
        ss << "LOST " << r.lostFleetIds.size();
        for (int id : r.lostFleetIds) ss << " " << id;
        ss << "\n";
        ss << "RETREATED " << r.retreatedFleetIds.size();
        for (int id : r.retreatedFleetIds) ss << " " << id;
        ss << "\n";
    }

    // Extended section: groups and cloak states
    if (!encounter.groups.empty()) {
        ss << "GROUPS " << encounter.groups.size() << "\n";
        for (const auto& g : encounter.groups) {
            ss << g.groupId << " " << g.ownerId << " " << static_cast<int>(g.groupClass) << " "
               << static_cast<int>(g.standingManeuver) << " " << g.standingTargetFleetId << " "
               << g.fleetIds.size();
            for (int fid : g.fleetIds) ss << " " << fid;
            ss << " " << escapeString(g.name) << "\n";
        }
    }

    ss << "CLOAK_STATE " << encounter.ships.size() << "\n";
    for (const auto& s : encounter.ships) {
        ss << s.fleetId << " " << (s.canCloak ? 1 : 0) << " " << (s.isCloaked ? 1 : 0) << " " << s.groupId << "\n";
    }

    ss << "END\n";
    return ss.str();
}

inline Encounter deserializeEncounter(const std::string& text) {
    std::istringstream ss(text);
    std::string tag;
    if (!(ss >> tag) || (tag != "TACTICAL_ENCOUNTER_V1" && tag != "TACTICAL_ENCOUNTER_V2")) {
        throw std::invalid_argument("Invalid or corrupt tactical encounter serialization header.");
    }

    Encounter enc;
    bool hasArena = false, hasRounds = false, hasSector = false, hasRng = false;
    bool hasController = false, hasShips = false, hasOrders = false, hasRoundResults = false, hasEnd = false;
    bool hasGroups = false, hasCloakState = false;

    while (ss >> tag) {
        if (tag == "ARENA") {
            if (!(ss >> enc.arenaWidth >> enc.arenaHeight)) {
                throw std::invalid_argument("Failed to parse ARENA dimensions.");
            }
            if (enc.arenaWidth <= 0 || enc.arenaWidth > 100 || enc.arenaHeight <= 0 || enc.arenaHeight > 100) {
                throw std::invalid_argument("ARENA dimensions out of valid bounds [1, 100].");
            }
            hasArena = true;
        } else if (tag == "ROUNDS") {
            int fin = 0;
            if (!(ss >> enc.currentRound >> enc.maxRounds >> fin >> enc.winnerOwnerId)) {
                throw std::invalid_argument("Failed to parse ROUNDS section.");
            }
            if (fin != 0 && fin != 1) {
                throw std::invalid_argument("Invalid boolean for isFinished in ROUNDS (expected 0 or 1).");
            }
            if (enc.currentRound < 0 || enc.maxRounds <= 0 || enc.maxRounds > 500) {
                throw std::invalid_argument("ROUNDS counts out of valid bounds.");
            }
            enc.isFinished = (fin == 1);
            hasRounds = true;
        } else if (tag == "SECTOR") {
            if (!(ss >> enc.sectorX >> enc.sectorY >> enc.strategicTurn)) {
                throw std::invalid_argument("Failed to parse SECTOR section.");
            }
            if (enc.strategicTurn < 0) {
                throw std::invalid_argument("Negative strategic turn in SECTOR.");
            }
            hasSector = true;
        } else if (tag == "RNG") {
            if (!(ss >> enc.initialSeed >> enc.rngState)) {
                throw std::invalid_argument("Failed to parse RNG section.");
            }
            hasRng = true;
        } else if (tag == "CONTROLLER") {
            int applied = 0;
            if (!(ss >> enc.controllingOwnerId >> applied)) {
                throw std::invalid_argument("Failed to parse CONTROLLER section.");
            }
            if (applied != 0 && applied != 1) {
                throw std::invalid_argument("Invalid boolean for resultsApplied in CONTROLLER (expected 0 or 1).");
            }
            enc.resultsApplied = (applied == 1);
            hasController = true;
        } else if (tag == "SHIPS") {
            std::int64_t count = 0;
            if (!(ss >> count)) {
                throw std::invalid_argument("Failed to parse SHIPS count.");
            }
            if (count < 2 || count > 256) {
                throw std::invalid_argument("SHIPS count out of valid bounds [2, 256]: " + std::to_string(count));
            }
            enc.ships.clear();
            std::set<int> seenFleetIds;
            std::set<int> seenOwners;

            for (std::int64_t i = 0; i < count; ++i) {
                Ship s;
                int alive = 0, active = 0, ret = 0, canRet = 0, sc = 0;
                std::string escName;
                if (!(ss >> s.fleetId >> s.ownerId
                         >> s.hull >> s.maxHull >> s.initialHull
                         >> s.shields >> s.maxShields >> s.initialShields
                         >> s.beamAttack >> s.torpedoAttack >> s.speed
                         >> s.position.x >> s.position.y
                         >> alive >> active >> ret >> s.retreatSteps
                         >> canRet >> sc >> s.referenceShipId >> escName)) {
                    throw std::invalid_argument("Failed to parse ship record " + std::to_string(i));
                }

                // Strict boolean verification
                if ((alive != 0 && alive != 1) || (active != 0 && active != 1) ||
                    (ret != 0 && ret != 1) || (canRet != 0 && canRet != 1)) {
                    throw std::invalid_argument("Invalid boolean value in ship record " + std::to_string(i));
                }

                // Check duplicate fleet IDs
                if (!seenFleetIds.insert(s.fleetId).second) {
                    throw std::invalid_argument("Duplicate fleet ID in SHIPS section: " + std::to_string(s.fleetId));
                }
                seenOwners.insert(s.ownerId);

                // Bounds & consistency validation
                if (s.maxHull <= 0 || s.hull < 0 || s.hull > s.maxHull || s.initialHull < 0 || s.initialHull > s.maxHull) {
                    throw std::invalid_argument("Invalid hull values for ship " + std::to_string(s.fleetId));
                }
                if (s.maxShields < 0 || s.shields < 0 || s.shields > s.maxShields || s.initialShields < 0 || s.initialShields > s.maxShields) {
                    throw std::invalid_argument("Invalid shield values for ship " + std::to_string(s.fleetId));
                }
                if (s.position.x < 0 || s.position.x >= enc.arenaWidth || s.position.y < 0 || s.position.y >= enc.arenaHeight) {
                    throw std::invalid_argument("Ship coordinates out of arena bounds: (" +
                        std::to_string(s.position.x) + "," + std::to_string(s.position.y) + ")");
                }

                s.isAlive = (alive == 1);
                s.activeInCombat = (active == 1);
                s.retreated = (ret == 1);
                s.canRetreat = (canRet == 1);
                s.shipClass = static_cast<Building>(sc);
                s.name = unescapeString(escName);
                enc.ships.push_back(s);
            }

            // Exactly 2 owners required
            if (seenOwners.size() != 2) {
                throw std::invalid_argument("SHIPS section must contain exactly 2 distinct owners (found " +
                    std::to_string(seenOwners.size()) + ")");
            }
            hasShips = true;
        } else if (tag == "ORDERS") {
            std::int64_t count = 0;
            if (!(ss >> count)) {
                throw std::invalid_argument("Failed to parse ORDERS count.");
            }
            if (count < 0 || count > 256) {
                throw std::invalid_argument("ORDERS count out of valid bounds [0, 256]: " + std::to_string(count));
            }
            enc.pendingOrders.clear();
            for (std::int64_t i = 0; i < count; ++i) {
                Order o;
                int man = 0;
                if (!(ss >> o.fleetId >> o.targetFleetId >> man)) {
                    throw std::invalid_argument("Failed to parse order " + std::to_string(i));
                }
                if (man < 0 || man > 11) {
                    throw std::invalid_argument("Invalid maneuver value in ORDERS: " + std::to_string(man));
                }
                o.maneuver = static_cast<TacticalManeuver>(man);
                enc.pendingOrders.push_back(o);
                enc.standingOrders[o.fleetId] = o;
            }
            hasOrders = true;
        } else if (tag == "ROUND_RESULTS") {
            std::int64_t count = 0;
            if (!(ss >> count)) {
                throw std::invalid_argument("Failed to parse ROUND_RESULTS count.");
            }
            if (count < 0 || count > 1024) {
                throw std::invalid_argument("ROUND_RESULTS count out of valid bounds: " + std::to_string(count));
            }
            enc.rounds.clear();
            for (std::int64_t i = 0; i < count; ++i) {
                RoundResult r;
                std::string subTag;
                int fin = 0;
                if (!(ss >> subTag >> r.roundIndex >> fin >> r.winnerOwnerId) || subTag != "ROUND") {
                    throw std::invalid_argument("Malformed ROUND tag in ROUND_RESULTS.");
                }
                if (fin != 0 && fin != 1) {
                    throw std::invalid_argument("Invalid boolean value for encounterEnded in ROUND.");
                }
                r.encounterEnded = (fin == 1);

                std::int64_t statusCount = 0;
                if (!(ss >> subTag >> statusCount) || subTag != "STATUSES" || statusCount < 0 || statusCount > 256) {
                    throw std::invalid_argument("Malformed STATUSES tag in ROUND_RESULTS.");
                }
                for (std::int64_t sIdx = 0; sIdx < statusCount; ++sIdx) {
                    ShipRoundStatus st;
                    int man = 0, dest = 0, ret = 0;
                    if (!(ss >> st.fleetId >> st.ownerId
                             >> st.hullBefore >> st.hullAfter
                             >> st.shieldBefore >> st.shieldAfter
                             >> st.beamDamageDealt >> st.torpedoDamageDealt
                             >> st.totalDamageTaken
                             >> st.positionBefore.x >> st.positionBefore.y
                             >> st.positionAfter.x >> st.positionAfter.y
                             >> man >> st.targetFleetId
                             >> dest >> ret)) {
                        throw std::invalid_argument("Failed to parse ShipRoundStatus in ROUND_RESULTS.");
                    }
                    if (man < 0 || man > 11) {
                        throw std::invalid_argument("Invalid maneuverExecuted in ShipRoundStatus: " + std::to_string(man));
                    }
                    if ((dest != 0 && dest != 1) || (ret != 0 && ret != 1)) {
                        throw std::invalid_argument("Invalid boolean value in ShipRoundStatus.");
                    }
                    st.maneuverExecuted = static_cast<TacticalManeuver>(man);
                    st.destroyedThisRound = (dest == 1);
                    st.retreatedThisRound = (ret == 1);
                    r.shipStatuses.push_back(st);
                }

                std::int64_t dmgCount = 0;
                if (!(ss >> subTag >> dmgCount) || subTag != "DAMAGE" || dmgCount < 0 || dmgCount > 256) {
                    throw std::invalid_argument("Malformed DAMAGE tag in ROUND_RESULTS.");
                }
                for (std::int64_t j = 0; j < dmgCount; ++j) {
                    FleetDamage d;
                    if (!(ss >> d.fleetId >> d.hullBefore >> d.hullAfter >> d.shieldBefore >> d.shieldAfter)) {
                        throw std::invalid_argument("Failed to parse FleetDamage record.");
                    }
                    r.damage.push_back(d);
                }

                std::int64_t lostCount = 0;
                if (!(ss >> subTag >> lostCount) || subTag != "LOST" || lostCount < 0 || lostCount > 256) {
                    throw std::invalid_argument("Malformed LOST tag in ROUND_RESULTS.");
                }
                for (std::int64_t j = 0; j < lostCount; ++j) {
                    int id = 0;
                    if (!(ss >> id)) throw std::invalid_argument("Failed to parse lost fleet ID.");
                    r.lostFleetIds.push_back(id);
                }

                std::int64_t retCount = 0;
                if (!(ss >> subTag >> retCount) || subTag != "RETREATED" || retCount < 0 || retCount > 256) {
                    throw std::invalid_argument("Malformed RETREATED tag in ROUND_RESULTS.");
                }
                for (std::int64_t j = 0; j < retCount; ++j) {
                    int id = 0;
                    if (!(ss >> id)) throw std::invalid_argument("Failed to parse retreated fleet ID.");
                    r.retreatedFleetIds.push_back(id);
                }
                enc.rounds.push_back(r);
            }
            hasRoundResults = true;
        } else if (tag == "GROUPS") {
            hasGroups = true;
            std::int64_t count = 0;
            if (!(ss >> count) || count < 0 || count > 256) {
                throw std::invalid_argument("Failed to parse GROUPS count.");
            }
            enc.groups.clear();
            for (std::int64_t i = 0; i < count; ++i) {
                TacticalGroup g;
                int gc = 0, man = 0;
                std::int64_t fCount = 0;
                std::string escName;
                if (!(ss >> g.groupId >> g.ownerId >> gc >> man >> g.standingTargetFleetId >> fCount)) {
                    throw std::invalid_argument("Failed to parse GROUPS header record.");
                }
                g.groupClass = static_cast<TacticalGroupClass>(gc);
                g.standingManeuver = static_cast<TacticalManeuver>(man);
                for (std::int64_t j = 0; j < fCount; ++j) {
                    int fid = 0;
                    if (!(ss >> fid)) throw std::invalid_argument("Failed to parse group fleet ID.");
                    g.fleetIds.push_back(fid);
                }
                if (!(ss >> escName)) throw std::invalid_argument("Failed to parse group name.");
                g.name = unescapeString(escName);
                enc.groups.push_back(g);
            }
        } else if (tag == "CLOAK_STATE") {
            hasCloakState = true;
            std::int64_t count = 0;
            if (!(ss >> count) || count < 0 || count > 256) {
                throw std::invalid_argument("Failed to parse CLOAK_STATE count.");
            }
            for (std::int64_t i = 0; i < count; ++i) {
                int fid = 0, canC = 0, isC = 0, gid = 0;
                if (!(ss >> fid >> canC >> isC >> gid)) {
                    throw std::invalid_argument("Failed to parse CLOAK_STATE ship record.");
                }
                Ship* s = enc.getShip(fid);
                if (s) {
                    s->canCloak = (canC == 1);
                    s->isCloaked = (isC == 1);
                    s->groupId = gid;
                }
            }
        } else if (tag == "END") {
            hasEnd = true;
            break;
        } else {
            throw std::invalid_argument("Unknown or unexpected section tag in serialization: " + tag);
        }
    }

    if (!hasArena || !hasRounds || !hasSector || !hasRng || !hasShips || !hasOrders || !hasRoundResults || !hasEnd) {
        throw std::invalid_argument("Incomplete tactical encounter serialization: missing required sections.");
    }

    // Reject trailing data after END tag
    std::string trailing;
    if (ss >> trailing) {
        throw std::invalid_argument("Unexpected trailing data after END tag: " + trailing);
    }

    // Reconstruct standing orders for controlling player from latest round statuses for mid-battle continuation
    if (!enc.rounds.empty()) {
        for (const auto& st : enc.rounds.back().shipStatuses) {
            if (st.ownerId == enc.controllingOwnerId && enc.standingOrders.find(st.fleetId) == enc.standingOrders.end()) {
                enc.standingOrders[st.fleetId] = Order{st.fleetId, st.targetFleetId, st.maneuverExecuted};
            }
        }
    }

    return enc;
}

} // namespace botf::tactical
