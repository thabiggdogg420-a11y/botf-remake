// Schema 20: threat actors (Threats.h, docs/design/THREAT-ACTORS.md).
#include "Threats.h"

#include "GameplayInternal.h"
#include "TacticalCombat.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <sstream>

namespace botf::threats {
namespace {
using gameplay::chebyshev;
using gameplay::require;

int clampi(int v, int lo, int hi) { return std::min(hi, std::max(lo, v)); }

const System* cellSystem(const GameState& s, int x, int y) { return gameplay::systemAtCell(s, x, y); }
std::string systemName(const GameState& s, int id) {
    const auto* sys = gameplay::systemById(s, id);
    return sys ? sys->name : "system " + std::to_string(id);
}
Colony* colonyOf(GameState& s, int systemId) {
    for (auto& c : s.colonies) if (c.systemId == systemId) return &c;
    return nullptr;
}
bool seated(const GameState& s, int id) {
    return s.playerEmpire == id || std::any_of(s.rivals.begin(), s.rivals.end(), [id](const EmpireState& e) { return e.id == id; });
}
// A line only the player's log should carry when the player has a stake in the place.
bool playerKnows(const GameState& s, int x, int y) { return internal::known(s, s.playerEmpire, x, y); }
void say(TurnReport& report, const std::string& text, int x, int y, int systemId = 0, int empireId = 0) {
    ChronicleEntry e;
    e.category = ChronicleCategory::Events;
    e.text = text;
    e.x = x; e.y = y; e.systemId = systemId;
    e.empireId = empireId;
    gameplay::note(report, std::move(e));
}

// ---- Placement and movement ------------------------------------------------------------------------------------------
bool freeCell(const GameState& s, int x, int y) {
    return x >= 0 && y >= 0 && x < s.width && y < s.height && sectorPassable(s, x, y);
}
// A cell for a static actor: an empty deep-space sector away from every colony (falls back to any passable cell).
void staticCell(GameState& s, int& ox, int& oy) {
    int bx = -1, by = -1, bestScore = -1;
    for (int attempt = 0; attempt < 24; ++attempt) {
        const int x = static_cast<int>(internal::bounded(s.rngState, static_cast<std::uint64_t>(s.width)));
        const int y = static_cast<int>(internal::bounded(s.rngState, static_cast<std::uint64_t>(s.height)));
        if (!freeCell(s, x, y) || cellSystem(s, x, y)) continue;
        int nearest = 99;
        for (const auto& c : s.colonies)
            if (const auto* sys = gameplay::systemById(s, c.systemId)) nearest = std::min(nearest, chebyshev(x, y, sys->x, sys->y));
        for (const auto& a : s.threatActors) nearest = std::min(nearest, chebyshev(x, y, a.x, a.y));
        if (nearest > bestScore) { bestScore = nearest; bx = x; by = y; }
        if (nearest >= 4) break;
    }
    if (bx < 0) { bx = s.width / 2; by = s.height / 2; }
    ox = bx; oy = by;
}
void edgeCell(GameState& s, int& ox, int& oy) {
    for (int attempt = 0; attempt < 16; ++attempt) {
        const int side = static_cast<int>(internal::bounded(s.rngState, 4));
        const int along = static_cast<int>(internal::bounded(s.rngState, static_cast<std::uint64_t>(side < 2 ? s.width : s.height)));
        const int x = side == 0 ? along : side == 1 ? along : side == 2 ? 0 : s.width - 1;
        const int y = side == 0 ? 0 : side == 1 ? s.height - 1 : along;
        if (freeCell(s, x, y)) { ox = x; oy = y; return; }
    }
    ox = 0; oy = 0;
}
void anyCell(GameState& s, int& ox, int& oy) {
    for (int attempt = 0; attempt < 16; ++attempt) {
        const int x = static_cast<int>(internal::bounded(s.rngState, static_cast<std::uint64_t>(s.width)));
        const int y = static_cast<int>(internal::bounded(s.rngState, static_cast<std::uint64_t>(s.height)));
        if (freeCell(s, x, y)) { ox = x; oy = y; return; }
    }
    ox = s.width / 2; oy = s.height / 2;
}
bool oneStep(const GameState& s, ThreatActor& a, int tx, int ty) {
    const int dx = (tx > a.x) - (tx < a.x), dy = (ty > a.y) - (ty < a.y);
    if (dx == 0 && dy == 0) return false;
    const std::array<std::pair<int, int>, 4> options{{{dx, dy}, {dx, 0}, {0, dy}, {dx != 0 ? 0 : 1, dy != 0 ? 0 : 1}}};
    for (const auto& [ox, oy] : options) {
        if (ox == 0 && oy == 0) continue;
        if (freeCell(s, a.x + ox, a.y + oy)) { a.x += ox; a.y += oy; return true; }
    }
    return false;
}
bool eligibleOwner(const GameState& s, int owner) { return !s.threats.sparesAi || owner == s.playerEmpire; }
bool targetable(const GameState& s, const Colony& c) {
    return eligibleOwner(s, c.ownerId) && !minorAt(s, c.systemId);
}
// The nearest colony the actor may attack (ties by system id), or nullptr.
const Colony* nearestColony(const GameState& s, const ThreatActor& a) {
    const Colony* best = nullptr;
    int bestDist = std::numeric_limits<int>::max();
    for (const auto& c : s.colonies) {
        if (!targetable(s, c)) continue;
        const auto* sys = gameplay::systemById(s, c.systemId);
        if (!sys) continue;
        const int d = chebyshev(a.x, a.y, sys->x, sys->y);
        if (d < bestDist) { bestDist = d; best = &c; }
    }
    return best;
}

int count(const GameState& s, int kind) {
    return static_cast<int>(std::count_if(s.threatActors.begin(), s.threatActors.end(), [kind](const ThreatActor& a) { return a.kind == kind; }));
}
void spawn(GameState& s, TurnReport& report, int kind, bool atTurnOne) {
    const auto& d = def(kind);
    ThreatActor a;
    a.id = s.nextThreatId++;
    a.kind = kind;
    a.spawnTurn = s.turn;
    if (d.behaviour == Behaviour::Guard) staticCell(s, a.x, a.y);
    else if (d.effect == Effect::Assimilate) edgeCell(s, a.x, a.y);
    else if (d.effect == Effect::Bombard) anyCell(s, a.x, a.y);
    else edgeCell(s, a.x, a.y);
    a.targetX = a.x; a.targetY = a.y;
    a.hull = actorMaxHull(a);
    a.shields = actorMaxShields(a);
    s.threatActors.push_back(a);
    s.threatLastSpawn[static_cast<std::size_t>(kind)] = s.turn;
    if (atTurnOne) return;
    if (d.hostility == Hostility::Aggressive && d.behaviour != Behaviour::Guard)
        say(report, std::string("Starfleet Intelligence: ") + d.name + " detected entering the quadrant.", -1, -1);
    else if (playerKnows(s, a.x, a.y))
        say(report, std::string("Long-range sensors: ") + d.name + " detected at sector " + std::to_string(a.x) + "," + std::to_string(a.y) + ".", a.x, a.y);
}

void retarget(GameState& s, ThreatActor& a) {
    const auto& d = def(a.kind);
    if (d.behaviour == Behaviour::Guard) return;
    if (d.behaviour == Behaviour::Hunt) {
        if (const Colony* c = nearestColony(s, a)) {
            if (const auto* sys = gameplay::systemById(s, c->systemId)) { a.targetX = sys->x; a.targetY = sys->y; return; }
        }
    }
    if (a.x == a.targetX && a.y == a.targetY) {
        anyCell(s, a.targetX, a.targetY);
    }
}
void move(GameState& s, ThreatActor& a) {
    const auto& d = def(a.kind);
    if (d.behaviour == Behaviour::Guard || d.speed <= 0) return;
    if (d.behaviour == Behaviour::Drift && (s.turn + a.id) % 2 != 0) return;
    // A besieging actor holds position while it works on the colony.
    if (a.x == a.targetX && a.y == a.targetY) return;
    for (int i = 0; i < d.speed; ++i) {
        if (a.x == a.targetX && a.y == a.targetY) break;
        if (!oneStep(s, a, a.targetX, a.targetY)) { a.targetX = a.x; a.targetY = a.y; break; }
    }
}
} // namespace

// ---- Queries -------------------------------------------------------------------------------------------------------
bool borgSeated(const GameState& s) { return seated(s, BorgEmpire); }
bool kindEnabled(const GameState& s, int kind) {
    if (!kindValid(kind) || s.threats.frequency <= 0) return false;
    if ((s.threats.disabledMask >> kind) & 1) return false;
    const auto& d = def(kind);
    if (d.borg) {
        if (!s.threats.borg || borgSeated(s)) return false;
        return (kind == static_cast<int>(ThreatKind::BorgSphere)) == s.threats.borgSphere;
    }
    if (!s.threats.monsters) return false;
    switch (static_cast<ThreatKind>(kind)) {
    case ThreatKind::Husnock: return !s.threats.species8472;
    case ThreatKind::Species8472: return s.threats.species8472;
    case ThreatKind::EdoGuardian: return !s.threats.edoStation;
    case ThreatKind::DeepSpaceNine: return s.threats.edoStation;
    default: return true;
    }
}
bool anyEnabled(const GameState& s) {
    for (int k = 0; k < ThreatKinds; ++k) if (kindEnabled(s, k)) return true;
    return false;
}
std::string actorName(const ThreatActor& a) { return def(a.kind).name; }
int actorMaxHull(const ThreatActor& a) { return std::max(1, internal::sourceShip(def(a.kind).shipId).hull); }
int actorMaxShields(const ThreatActor& a) {
    const int base = std::max(0, internal::sourceShip(def(a.kind).shipId).shields);
    return static_cast<int>(static_cast<std::int64_t>(base) * (100 + a.adapt) / 100);
}
int actorAttack(const ThreatActor& a) {
    const std::int64_t base = std::max(0, internal::sourceShip(def(a.kind).shipId).attack);
    return static_cast<int>(base * def(a.kind).attackPct / 100 * (100 + a.adapt) / 100);
}
bool hostile(const ThreatActor& a) { return def(a.kind).hostility != Hostility::Peaceful && actorAttack(a) > 0; }
bool canSee(const GameState& s, int viewer, const ThreatActor& a) { return visible(s, viewer, a.x, a.y); }
const ThreatActor* actorById(const GameState& s, int id) {
    for (const auto& a : s.threatActors) if (a.id == id) return &a;
    return nullptr;
}
bool isThreatCombatant(int fleetId) { return fleetId > ThreatCombatantBase && fleetId < ThreatCombatantBase + 1000000; }
int threatIdForCombatant(int fleetId) { return fleetId - ThreatCombatantBase; }
const AssimilatedSystem* assimilatedAt(const GameState& s, int systemId) {
    for (const auto& a : s.assimilated) if (a.systemId == systemId) return &a;
    return nullptr;
}

// ---- Validation and persistence --------------------------------------------------------------------------------------
void validate(const GameState& s) {
    const auto& t = s.threats;
    require(t.frequency >= 0 && t.frequency <= 3 && t.disabledMask >= 0 && t.disabledMask < (1 << ThreatKinds), "Invalid threat setup");
    require(s.nextThreatId >= 1 && s.nextThreatId < 1000000, "Invalid next threat id");
    require(s.threatActors.size() <= 64 && s.assimilated.size() <= 4096, "Too many threat records");
    for (int v : s.threatLastSpawn) require(v >= 0 && v <= s.turn, "Invalid threat spawn turn");
    int previous = 0;
    for (const auto& a : s.threatActors) {
        require(a.id > previous && a.id < s.nextThreatId && kindValid(a.kind), "Invalid threat actor identity");
        previous = a.id;
        require(a.x >= 0 && a.x < s.width && a.y >= 0 && a.y < s.height && a.targetX >= 0 && a.targetX < s.width &&
            a.targetY >= 0 && a.targetY < s.height, "Threat actor outside the galaxy");
        require(a.adapt >= 0 && a.adapt <= AdaptCap && (a.adapt == 0 || def(a.kind).borg), "Invalid threat adaptation");
        require(a.hull >= 1 && a.hull <= actorMaxHull(a) && a.shields >= 0 && a.shields <= actorMaxShields(a), "Invalid threat actor strength");
        require(a.spawnTurn >= 0 && a.spawnTurn <= s.turn && a.siege >= 0 && a.siege <= 100 && a.kills >= 0 && a.kills <= 10000, "Invalid threat actor state");
    }
    std::set<int> seen;
    for (const auto& m : s.assimilated) {
        require(gameplay::systemById(s, m.systemId) != nullptr && m.turn >= 0 && m.turn <= s.turn &&
            (m.ownerId == 0 || m.ownerId == BorgEmpire) && seen.insert(m.systemId).second, "Invalid assimilated system");
    }
}
bool sectionNeeded(const GameState& s) {
    return !(s.threats == ThreatSetup{}) || !s.threatActors.empty() || !s.assimilated.empty() || s.nextThreatId != 1 ||
        std::any_of(s.threatLastSpawn.begin(), s.threatLastSpawn.end(), [](int v) { return v != 0; });
}
void writeSection(std::ostream& out, const GameState& s) {
    if (!sectionNeeded(s)) return;
    const auto& t = s.threats;
    out << "THREATS 1 " << t.monsters << ' ' << t.borg << ' ' << t.frequency << ' ' << t.disabledMask << ' ' << t.borgSphere << ' '
        << t.species8472 << ' ' << t.edoStation << ' ' << t.sparesAi << ' ' << s.nextThreatId << ' ' << s.threatActors.size() << ' '
        << s.assimilated.size() << '\n';
    out << "LAST_SPAWN";
    for (int v : s.threatLastSpawn) out << ' ' << v;
    out << '\n';
    for (const auto& a : s.threatActors)
        out << a.id << ' ' << a.kind << ' ' << a.x << ' ' << a.y << ' ' << a.targetX << ' ' << a.targetY << ' ' << a.hull << ' '
            << a.shields << ' ' << a.spawnTurn << ' ' << a.adapt << ' ' << a.siege << ' ' << a.kills << '\n';
    for (const auto& m : s.assimilated) out << m.systemId << ' ' << m.turn << ' ' << m.ownerId << '\n';
}
void readSection(std::istream& in, GameState& s) {
    int revision = 0, monsters = 0, borg = 0, sphere = 0, eight = 0, edo = 0, spares = 0;
    std::size_t actors = 0, assimilated = 0;
    auto& t = s.threats;
    require(static_cast<bool>(in >> revision >> monsters >> borg >> t.frequency >> t.disabledMask >> sphere >> eight >> edo >> spares >>
        s.nextThreatId >> actors >> assimilated) && revision == 1 && actors <= 64 && assimilated <= 4096 &&
        (monsters | borg | sphere | eight | edo | spares) >= 0 && monsters <= 1 && borg <= 1 && sphere <= 1 && eight <= 1 && edo <= 1 && spares <= 1,
        "Malformed THREATS section");
    t.monsters = monsters; t.borg = borg; t.borgSphere = sphere; t.species8472 = eight; t.edoStation = edo; t.sparesAi = spares;
    std::string tag;
    require(static_cast<bool>(in >> tag) && tag == "LAST_SPAWN", "Malformed THREATS section");
    for (auto& v : s.threatLastSpawn) require(static_cast<bool>(in >> v), "Malformed THREATS section");
    for (std::size_t i = 0; i < actors; ++i) {
        ThreatActor a;
        require(static_cast<bool>(in >> a.id >> a.kind >> a.x >> a.y >> a.targetX >> a.targetY >> a.hull >> a.shields >> a.spawnTurn >>
            a.adapt >> a.siege >> a.kills), "Malformed THREATS actor");
        s.threatActors.push_back(a);
    }
    for (std::size_t i = 0; i < assimilated; ++i) {
        AssimilatedSystem m;
        require(static_cast<bool>(in >> m.systemId >> m.turn >> m.ownerId), "Malformed THREATS assimilated system");
        s.assimilated.push_back(m);
    }
    require(sectionNeeded(s), "Non-canonical empty THREATS section");
}

// ---- Turn step: spawn, retarget, move, despawn ---------------------------------------------------------------------------
void step(GameState& s, TurnReport& report) {
    if (!s.unifiedCombat) return;
    if (!anyEnabled(s) && s.threatActors.empty()) return;
    // Existing actors first (id order): leave when their time is up, otherwise retarget and move.
    for (std::size_t i = 0; i < s.threatActors.size();) {
        auto& a = s.threatActors[i];
        const auto& d = def(a.kind);
        const bool timedOut = d.lifespan > 0 && s.turn - a.spawnTurn >= d.lifespan;
        const bool sated = d.borg && a.kills >= 3 + s.threats.frequency;
        if (timedOut || sated) {
            if (d.hostility == Hostility::Aggressive || playerKnows(s, a.x, a.y))
                say(report, std::string(d.name) + (d.borg ? " withdrew from the quadrant." : " left the quadrant."), a.x, a.y);
            s.threatActors.erase(s.threatActors.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        retarget(s, a);
        move(s, a);
        ++i;
    }
    // Spawns, one kind at a time in kind order, so the draws are a fixed sequence.
    if (s.threats.frequency <= 0) return;
    static const int Multiplier[4] = {0, 50, 100, 200};
    for (int kind = 0; kind < ThreatKinds; ++kind) {
        if (!kindEnabled(s, kind)) continue;
        const auto& d = def(kind);
        if (s.turn < d.minTurn) continue;
        const int last = s.threatLastSpawn[static_cast<std::size_t>(kind)];
        if (d.speed == 0) {   // Static: once, at the first turn.
            if (last == 0 && count(s, kind) == 0) spawn(s, report, kind, true);
            continue;
        }
        if (count(s, kind) >= d.cap * (s.threats.frequency >= 3 ? 2 : 1)) continue;
        if (last != 0 && s.turn - last < d.gap) continue;
        const int chance = d.spawnPer1000 * Multiplier[s.threats.frequency] / 100;
        if (static_cast<int>(internal::bounded(s.rngState, 1000)) < chance) spawn(s, report, kind, false);
    }
}

// ---- Battles -----------------------------------------------------------------------------------------------------------
namespace {
std::string fleetsLine(int lost, int withdrew, bool dead, const ThreatActor& a) {
    return ": " + std::to_string(lost) + " ships lost, " + std::to_string(withdrew) + " withdrew" +
        (dead ? ". The " + actorName(a) + " was destroyed." : ".");
}
} // namespace

void battles(GameState& state, TurnReport& report, std::set<int>& withdrawn, std::vector<CombatReport>& out,
             const std::function<void(tactical::Encounter&)>& playerEncounter) {
    if (state.threatActors.empty()) return;
    std::vector<int> ids;
    for (const auto& a : state.threatActors) ids.push_back(a.id);
    for (int actorId : ids) {
        // F5 (20 Nov 2026): each empire meets the actor once a turn. Before, an empire whose ships neither died nor withdrew
        // (a duel that ran to its round limit) was picked again forever: dominion-war seed 7655 turn 61 grew past 19 GB here.
        std::set<int> fought;
        for (;;) {   // One duel per empire present, in empire-id order; the actor carries its wounds from duel to duel.
            auto it = std::find_if(state.threatActors.begin(), state.threatActors.end(), [actorId](const ThreatActor& a) { return a.id == actorId; });
            if (it == state.threatActors.end() || !hostile(*it)) break;
            const ThreatActor actor = *it;
            int owner = 0;
            for (const auto& f : state.fleets) {
                if (f.hull <= 0 || f.x != actor.x || f.y != actor.y || withdrawn.count(f.id) || !eligibleOwner(state, f.ownerId) || fought.count(f.ownerId)) continue;
                if (owner == 0 || f.ownerId < owner) owner = f.ownerId;
            }
            if (owner == 0) break;
            fought.insert(owner);
            std::vector<int> fleetIds;
            for (const auto& f : state.fleets)
                if (f.hull > 0 && f.x == actor.x && f.y == actor.y && f.ownerId == owner && !withdrawn.count(f.id)) fleetIds.push_back(f.id);
            // The actor is a proxy fleet for construction only; its real values are written below (as Stations.cpp does for a station).
            Fleet proxy;
            proxy.id = ThreatCombatantBase + actor.id;
            proxy.ownerId = ThreatOwner;
            proxy.x = proxy.targetX = actor.x;
            proxy.y = proxy.targetY = actor.y;
            proxy.shipClass = Building::Destroyer;
            proxy.hull = 1;
            proxy.referenceShipId = def(actor.kind).shipId;
            auto all = fleetIds;
            all.push_back(proxy.id);
            const auto seed = state.rngState ^ (static_cast<std::uint64_t>(state.turn) << 32) ^
                static_cast<std::uint64_t>(actor.y * state.width + actor.x + 1) ^ (static_cast<std::uint64_t>(ThreatOwner * 7 + owner) << 16) ^
                (static_cast<std::uint64_t>(actor.id) << 24);
            state.fleets.push_back(proxy);
            tactical::Encounter encounter;
            try { encounter = tactical::constructEncounter(state, all, seed); }
            catch (...) { state.fleets.pop_back(); throw; }
            state.fleets.pop_back();
            auto* ship = encounter.getShip(proxy.id);
            require(ship != nullptr, "Threat actor missing from its encounter");
            ship->name = actorName(actor);
            ship->maxHull = actorMaxHull(actor);
            ship->hull = ship->initialHull = clampi(actor.hull, 1, ship->maxHull);
            ship->maxShields = actorMaxShields(actor);
            ship->shields = ship->initialShields = clampi(actor.shields, 0, ship->maxShields);
            const std::int64_t original = static_cast<std::int64_t>(ship->beamAttack) + ship->torpedoAttack;
            if (original > 0) {
                const std::int64_t target = actorAttack(actor);
                ship->beamAttack = static_cast<int>(ship->beamAttack * target / original);
                ship->torpedoAttack = static_cast<int>(ship->torpedoAttack * target / original);
            }
            ship->canRetreat = false;
            ship->canCloak = false;
            ship->isCloaked = false;
            const bool still = def(actor.kind).speed == 0;
            if (still) { ship->immobile = true; ship->speed = 1; }
            if (encounter.wegoRules()) { tactical::splitShieldFacings(*ship); if (still) ship->elevation = 0; }
            if (encounter.flightRules()) tactical::equipFlight(*ship, owner);
            if (still) {
                encounter.standingOrders[proxy.id] = tactical::Order{proxy.id, 0, tactical::TacticalManeuver::Hold};
                int nextGroup = 1;
                for (auto& group : encounter.groups) {
                    std::erase(group.fleetIds, proxy.id);
                    nextGroup = std::max(nextGroup, group.groupId + 1);
                }
                std::erase_if(encounter.groups, [](const tactical::TacticalGroup& g) { return g.fleetIds.empty(); });
                tactical::TacticalGroup group;
                group.groupId = nextGroup;
                group.ownerId = ThreatOwner;
                group.groupClass = tactical::TacticalGroupClass::Command;
                group.name = "Threat Group";
                group.fleetIds = {proxy.id};
                group.standingManeuver = tactical::TacticalManeuver::Hold;
                encounter.groups.push_back(group);
                ship->groupId = group.groupId;
            }
            const bool playerInvolved = owner == state.playerEmpire || humanSeat(state, owner);   // hotseat: any human's battle waits for them
            if (playerInvolved && owner != state.playerEmpire) encounter.controllingOwnerId = owner;
            if (playerInvolved && playerEncounter) playerEncounter(encounter);
            else encounter.runUntilFinished();
            require(encounter.isFinished, "Tactical encounter did not finish");
            auto fleetsOnly = encounter;
            std::erase_if(fleetsOnly.ships, [](const tactical::Ship& s) { return isThreatCombatant(s.fleetId); });
            if (!fleetsOnly.ships.empty()) tactical::applyEncounterResults(state, fleetsOnly);
            encounter.resultsApplied = true;
            for (const auto& s : encounter.ships) if (s.retreated && !isThreatCombatant(s.fleetId)) withdrawn.insert(s.fleetId);
            internal::reconcileTaskForces(state);
            auto battle = tactical::toCombatReport(encounter);
            // The actor's outcome.
            const auto* result = encounter.getShip(proxy.id);
            it = std::find_if(state.threatActors.begin(), state.threatActors.end(), [actorId](const ThreatActor& a) { return a.id == actorId; });
            require(result != nullptr && it != state.threatActors.end(), "Threat actor lost during its battle");
            int lostShips = 0;
            for (const auto& s : encounter.ships)
                if (!isThreatCombatant(s.fleetId) && (!s.isAlive || s.hull <= 0) && !s.retreated) ++lostShips;
            const bool dead = !result->isAlive || result->hull <= 0;
            if (dead) {
                const ThreatActor gone = *it;
                state.threatActors.erase(it);
                if (playerInvolved || playerKnows(state, gone.x, gone.y))
                    say(report, std::string("The ") + actorName(gone) + " was destroyed at sector " + std::to_string(gone.x) + "," + std::to_string(gone.y) + ".", gone.x, gone.y);
            } else {
                it->hull = clampi(result->hull, 1, actorMaxHull(*it));
                it->shields = clampi(result->shields, 0, actorMaxShields(*it));
                if (def(it->kind).borg) {
                    // Assimilation: wreckage feeds the cube; adaptation: it hardens against what it has met.
                    it->hull = std::min(actorMaxHull(*it), it->hull + lostShips * std::max(1, actorMaxHull(*it) / 12));
                    it->adapt = std::min(AdaptCap, it->adapt + 10);
                    it->shields = std::min(actorMaxShields(*it), it->shields);
                }
            }
            if (playerInvolved) {
                int withdrew = 0;
                for (const auto& s : encounter.ships) if (s.retreated && !isThreatCombatant(s.fleetId)) ++withdrew;
                report.events.push_back("Battle at sector " + std::to_string(actor.x) + "," + std::to_string(actor.y) + " against the " + actorName(actor) +
                    fleetsLine(lostShips, withdrew, dead, actor));
            }
            out.push_back(std::move(battle));
            if (dead) break;
        }
    }
}

// ---- After combat: siege, assimilation, bombardment, regeneration --------------------------------------------------------
namespace {
void removeColony(GameState& s, int systemId) {
    std::erase_if(s.colonies, [systemId](const Colony& c) { return c.systemId == systemId; });
    std::erase_if(s.tradeRoutes, [systemId](const TradeRoute& r) { return r.fromSystemId == systemId || r.toSystemId == systemId; });
    std::erase_if(s.stationUpgrades, [systemId](const StationUpgrade& u) { return u.fundingSystemId == systemId; });
    std::erase_if(s.megaprojects, [systemId](const Megaproject& p) { return p.systemId == systemId; });
    std::erase_if(s.aftermaths, [systemId](const AftermathChoice& p) { return p.systemId == systemId; });
}
void trimWorkers(Colony& c) {
    int excess = std::max(0, c.foodWorkers + c.industryWorkers + c.researchWorkers - c.population);
    for (int* w : {&c.industryWorkers, &c.researchWorkers, &c.foodWorkers}) {
        const int cut = std::min(*w, excess);
        *w -= cut; excess -= cut;
    }
}
void destroyStations(GameState& s, int x, int y) {
    std::vector<int> gone;
    for (const auto& st : s.stations) if (st.x == x && st.y == y) gone.push_back(st.id);
    for (int id : gone) {
        std::erase_if(s.stations, [id](const Station& st) { return st.id == id; });
        std::erase_if(s.stationUpgrades, [id](const StationUpgrade& u) { return u.stationId == id; });
    }
}
void heal(ThreatActor& a, int pct) {
    a.hull = std::min(actorMaxHull(a), a.hull + std::max(1, actorMaxHull(a) * pct / 100));
}
} // namespace

void afterCombat(GameState& s, TurnReport& report) {
    // Old assimilation marks fade, and a recolonised system loses its mark.
    std::erase_if(s.assimilated, [&](const AssimilatedSystem& m) {
        if (m.ownerId == 0) return s.turn - m.turn > AssimilatedMemory || colonyAt(s, m.systemId) != nullptr;
        const auto* c = colonyAt(s, m.systemId);
        return c == nullptr || c->ownerId != BorgEmpire;
    });
    // Borg empire fleets mend themselves (the cube's repair, at fleet scale) unless they fought this turn.
    if (borgSeated(s)) {
        std::set<int> fought;
        for (const auto& b : report.battles) for (const auto& p : b.participants) fought.insert(p.fleetId);
        for (auto& f : s.fleets) {
            if (f.ownerId != BorgEmpire || f.hull <= 0 || fought.count(f.id)) continue;
            const int maxHull = fleetMaxHull(f);
            if (f.hull < maxHull) f.hull = std::min(maxHull, f.hull + std::max(1, maxHull / 10));
        }
    }
    if (s.threatActors.empty()) return;
    std::set<int> fought;
    for (const auto& b : report.battles) for (const auto& p : b.participants) if (isThreatCombatant(p.fleetId)) fought.insert(threatIdForCombatant(p.fleetId));
    std::vector<int> ids;
    for (const auto& a : s.threatActors) ids.push_back(a.id);
    for (int id : ids) {
        auto it = std::find_if(s.threatActors.begin(), s.threatActors.end(), [id](const ThreatActor& a) { return a.id == id; });
        if (it == s.threatActors.end()) continue;
        ThreatActor& a = *it;
        const auto& d = def(a.kind);
        if (!fought.count(a.id) && d.regenPct > 0) {
            heal(a, d.regenPct);
            a.shields = actorMaxShields(a);
        }
        if (d.hostility != Hostility::Aggressive || d.effect == Effect::None || actorAttack(a) <= 0) { a.siege = 0; continue; }
        // Stations in the sector wear down (Borg: absorbed).
        for (auto& st : s.stations) {
            if (st.x != a.x || st.y != a.y || !eligibleOwner(s, st.ownerId)) continue;
            st.hull -= std::max(1, actorAttack(a) / 20);
        }
        std::vector<int> deadStations;
        for (const auto& st : s.stations) if (st.hull <= 0) deadStations.push_back(st.id);
        for (int sid : deadStations) {
            std::erase_if(s.stations, [sid](const Station& st) { return st.id == sid; });
            std::erase_if(s.stationUpgrades, [sid](const StationUpgrade& u) { return u.stationId == sid; });
            if (playerKnows(s, a.x, a.y)) say(report, std::string("A station at sector ") + std::to_string(a.x) + "," + std::to_string(a.y) + " fell to the " + actorName(a) + ".", a.x, a.y);
        }
        const auto* sys = cellSystem(s, a.x, a.y);
        Colony* colony = sys ? colonyOf(s, sys->id) : nullptr;
        if (!colony || !targetable(s, *colony)) { a.siege = 0; continue; }
        const std::string place = systemName(s, colony->systemId);
        const int owner = colony->ownerId;
        const bool shown = owner == s.playerEmpire || playerKnows(s, a.x, a.y);
        ++a.siege;
        if (d.effect == Effect::Assimilate) {
            // Two turns of siege, one more per two batteries and for a starbase; an empire's last world holds three turns longer.
            const int worlds = static_cast<int>(std::count_if(s.colonies.begin(), s.colonies.end(), [owner](const Colony& c) { return c.ownerId == owner; }));
            const int needed = 2 + colony->defenseBatteries / 2 + (stationAt(s, a.x, a.y) ? 1 : 0) + (worlds <= 1 ? 3 : 0);
            if (a.siege < needed) {
                if (shown) say(report, "The " + actorName(a) + " besieges " + place + ".", a.x, a.y, colony->systemId, owner);
                continue;
            }
            const int sysId = colony->systemId;
            removeColony(s, sysId);
            destroyStations(s, a.x, a.y);
            s.assimilated.push_back({sysId, s.turn, 0});
            a.siege = 0;
            ++a.kills;
            heal(a, 20);
            if (shown) say(report, "The Borg have assimilated " + place + " (" + empireName(owner) + ").", a.x, a.y, sysId, owner);
        } else if (d.effect == Effect::Bombard || d.effect == Effect::Strip) {
            const bool strip = d.effect == Effect::Strip;
            const int loss = std::max(strip ? 5 : 8, colony->population / (strip ? 6 : 3));
            if (colony->defenseBatteries > 0) --colony->defenseBatteries;
            colony->morale = std::max(0, colony->morale - 15);
            colony->population -= loss;
            if (colony->population <= (strip ? 5 : 0)) {
                const int sysId = colony->systemId;
                removeColony(s, sysId);
                destroyStations(s, a.x, a.y);
                ++a.kills;
                if (shown) say(report, place + " was " + (strip ? "stripped of life" : "destroyed") + " by the " + actorName(a) + " (" + empireName(owner) + ").", a.x, a.y, sysId, owner);
            } else {
                trimWorkers(*colony);
                if (shown) say(report, "The " + actorName(a) + " " + (strip ? "strips" : "bombards") + " " + place + ": population -" + std::to_string(loss) + ".", a.x, a.y, colony->systemId, owner);
            }
        } else if (d.effect == Effect::Raid) {
            if (colony->defenseBatteries > 0) --colony->defenseBatteries;
            const int loss = colony->population > 12 ? std::max(2, colony->population / 10) : 0;
            colony->population -= loss;
            colony->morale = std::max(0, colony->morale - 10);
            trimWorkers(*colony);
            if (shown) say(report, "The " + actorName(a) + " raids " + place + (loss ? ": population -" + std::to_string(loss) + "." : "."), a.x, a.y, colony->systemId, owner);
        }
    }
}

// ---- Borg rules shared by the threat and a playable Borg empire ----------------------------------------------------------
int moraleTarget(const GameState&, const Colony& colony, int minorMorale) {
    // The Collective has no unrest and no minor-race sympathies: its colonies settle at one value.
    return colony.ownerId == BorgEmpire ? 60 : minorMorale;
}
void onColonyCaptured(GameState& s, Colony& colony, TurnReport& report) {
    if (colony.ownerId != BorgEmpire) return;
    // Assimilation: a quarter of the population is lost in conversion; batteries are retooled as factories; the world
    // starts restless and settles to the Collective's morale.
    colony.population = std::max(1, colony.population * 3 / 4);
    colony.factories += colony.defenseBatteries;
    colony.defenseBatteries = 0;
    colony.morale = 30;
    colony.queue.clear();
    colony.overflow = 0;
    trimWorkers(colony);
    std::erase_if(s.assimilated, [&](const AssimilatedSystem& m) { return m.systemId == colony.systemId; });
    s.assimilated.push_back({colony.systemId, s.turn, BorgEmpire});
    const auto* sys = gameplay::systemById(s, colony.systemId);
    if (sys && playerKnows(s, sys->x, sys->y))
        say(report, "The Borg Collective assimilated " + sys->name + ".", sys->x, sys->y, colony.systemId, BorgEmpire);
}
void assimilateShips(GameState& s, const CombatReport& battle, TurnReport& report, std::vector<Fleet>& fresh) {
    if (!s.rules.expandedRoster || battle.winnerOwner != BorgEmpire || !seated(s, BorgEmpire)) return;
    int lost = 0;
    for (int id : battle.lostFleetIds) {
        for (const auto& p : battle.participants) {
            if (p.fleetId != id || p.ownerId == BorgEmpire || p.ownerId == ThreatOwner || isThreatCombatant(id)) continue;
            const bool armed = p.referenceShipId >= 0 ? internal::sourceShip(p.referenceShipId).attack > 0 : p.shipClass == Building::Destroyer;
            if (armed) ++lost;
        }
    }
    const int drones = lost / 2;
    if (drones <= 0) return;
    const auto& probe = internal::sourceShip(1601);
    for (int i = 0; i < drones; ++i) {
        Fleet f{s.nextFleetId++, BorgEmpire, battle.x, battle.y, battle.x, battle.y, false, false, Building::Scout, std::max(1, probe.hull / 2)};   // The Probe is a role-0 hull, so its fleet class is Scout.
        f.referenceShipId = 1601;
        f.shields = s.advancedRulesEnabled ? std::max(0, probe.shields) / 2 : 0;
        fresh.push_back(f);
    }
    if (internal::known(s, s.playerEmpire, battle.x, battle.y))
        say(report, "The Borg assimilated " + std::to_string(drones) + " wrecked ship(s) at sector " + std::to_string(battle.x) + "," + std::to_string(battle.y) + ".", battle.x, battle.y);
}
} // namespace botf::threats
