// BOTF Remastered scenarios, flagships and the vendetta table (lane MOD-8; docs/design/SCENARIOS.md).
// Deterministic: nothing here reads a clock, and the only random draws are the ones the existing systems make themselves.
#include "Scenarios.h"

#include "EconomyRules.h"
#include "EmpireSlots.h"
#include "GameplayInternal.h"
#include "Threats.h"

#include <cctype>
#include <set>
#include <stdexcept>

namespace botf::scenarios {
namespace scn_detail {   // Named, not anonymous: the Unreal bridge compiles every core .cpp as one translation unit.
using gameplay::require;

std::string lowered(const std::string& text) {
    std::string out;
    for (char c : text) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}
bool seated(const GameState& s, int id) {
    if (id == s.playerEmpire) return true;
    return std::any_of(s.rivals.begin(), s.rivals.end(), [id](const EmpireState& e) { return e.id == id; });
}
System* systemMut(GameState& s, int id) {
    for (auto& sys : s.systems) if (sys.id == id) return &sys;
    return nullptr;
}
int coloniesOf(const GameState& s, int empire) {
    return static_cast<int>(std::count_if(s.colonies.begin(), s.colonies.end(), [empire](const Colony& c) { return c.ownerId == empire; }));
}
bool eliminatedEmpire(const GameState& s, int empire) { return seated(s, empire) && coloniesOf(s, empire) == 0; }
bool systemFree(const GameState& s, int systemId) {
    return !colonyAt(s, systemId) && !minorAt(s, systemId);
}
// A reference in the scripted start must resolve (an authoring error throws). In a running game (a trigger) an empire that is not
// seated or a system that is gone makes the action a no-op instead, so a secession that did not happen cannot stop the turn.
bool g_soft = false;
int empireOrThrow(const GameState& s, const std::string& ref) {
    const int id = resolveEmpire(s, ref);
    if (!id && !g_soft) require(false, "Scenario: empire \"" + ref + "\" is not seated in this campaign");
    return id;
}
int systemOrThrow(const GameState& s, const std::string& ref) {
    const int id = resolveSystem(s, ref);
    if (!id && !g_soft) {
        const auto colon = ref.rfind("capital:");
        if (colon != std::string::npos && !resolveEmpire(s, ref.substr(colon + 8)))
            require(false, "Scenario: an empire that is not seated in this game: \"" + ref + "\"");
        require(false, "Scenario: no system matches \"" + ref + "\"");
    }
    return id;
}

// A free cell next to a system for a wormhole mouth: row-major scan at growing Chebyshev distance.
bool freeCellNear(const GameState& s, const System& sys, int& ox, int& oy) {
    std::set<int> taken;
    for (const auto& other : s.systems) taken.insert(other.y * s.width + other.x);
    for (const auto& f : s.galaxyFeatures) taken.insert(f.y * s.width + f.x);
    for (int d = 1; d <= 6; ++d)
        for (int y = std::max(0, sys.y - d); y <= std::min(s.height - 1, sys.y + d); ++y)
            for (int x = std::max(0, sys.x - d); x <= std::min(s.width - 1, sys.x + d); ++x) {
                if (std::max(std::abs(x - sys.x), std::abs(y - sys.y)) != d || taken.count(y * s.width + x)) continue;
                ox = x; oy = y;
                return true;
            }
    return false;
}

int findShipId(const std::string& text, int owner) {
    if (text.rfind("id:", 0) == 0) {
        int id = -1;
        try { id = std::stoi(text.substr(3)); } catch (...) { id = -1; }
        const auto* ship = roster::findShip(id);
        require(ship && internal::playableShip(*ship) && internal::shipUsable(*ship, owner), "Scenario: ship " + text + " is not available to the empire");
        return id;
    }
    const std::string want = lowered(text);
    auto scan = [&](const referenceData::ShipRecord& ship) {
        return (lowered(ship.shipClass) == want || lowered(ship.name) == want) && internal::playableShip(ship) && internal::shipUsable(ship, owner);
    };
    for (const auto& ship : referenceData::ShipCatalogue) if (scan(ship)) return ship.id;
    for (const auto& ship : roster::ExpandedShipRecords) if (scan(ship)) return ship.id;
    throw std::runtime_error("Scenario: no ship class \"" + text + "\" is available to " + std::string(slots::name(owner)));
}

int spawnShip(GameState& s, int owner, int systemId, int shipId) {
    const auto* sys = gameplay::systemById(s, systemId);
    require(sys != nullptr, "Scenario: fleet placed at an unknown system");
    const auto& ship = internal::sourceShip(shipId);
    Fleet f;
    f.id = s.nextFleetId++;
    f.ownerId = owner;
    f.x = f.targetX = sys->x; f.y = f.targetY = sys->y;
    f.stance = TacticalStance::Balanced;
    f.referenceShipId = ship.id;
    f.shipClass = internal::roleProduct(ship.role);
    f.colonyShip = f.shipClass == Building::ColonyShip;
    f.hull = ship.hull;
    f.shields = s.advancedRulesEnabled ? fleetMaxShields(f) : 0;
    s.fleets.push_back(f);
    internal::reveal(s, f.x, f.y, owner);
    return f.id;
}

Fleet* fleetMut(GameState& s, int id) {
    for (auto& f : s.fleets) if (f.id == id) return &f;
    return nullptr;
}
bool armed(const Fleet& f) { return f.hull > 0 && !f.colonyShip && f.shipClass != Building::Transport && fleetAttack(f) > 0; }

void say(TurnReport* report, ChronicleCategory category, const std::string& text, int empire = 0, int other = 0, int systemId = 0) {
    if (!report) return;
    ChronicleEntry e;
    e.category = category; e.text = text; e.empireId = empire; e.otherEmpireId = other; e.systemId = systemId;
    gameplay::note(*report, std::move(e));
}
} // namespace scn_detail
using namespace scn_detail;

// ---- References ---------------------------------------------------------------------------------------------------------------
int resolveEmpire(const GameState& state, const std::string& ref) {
    if (ref == "player") return state.playerEmpire;
    const auto* race = slots::raceForKey(ref);
    return race && seated(state, race->empireId) ? race->empireId : 0;
}

int resolveSystem(const GameState& state, const std::string& ref) {
    const auto colon = ref.find(':');
    if (colon == std::string::npos) return 0;
    const std::string head = ref.substr(0, colon), rest = ref.substr(colon + 1);
    if (head == "capital") {
        const int empire = resolveEmpire(state, rest);
        if (!empire) return 0;
        const std::string capital = slots::capital(empire);
        for (const auto& sys : state.systems) if (sys.name == capital) return sys.id;
        return 0;
    }
    if (head == "minor") {
        for (const auto& m : state.minors) if (m.name == rest) return m.systemId;
        return 0;
    }
    if (head == "system") {
        for (const auto& sys : state.systems) if (sys.name == rest) return sys.id;
        return 0;
    }
    if (head == "near") {
        const auto second = rest.find(':');
        if (second == std::string::npos) return 0;
        int n = 0;
        try { n = std::stoi(rest.substr(0, second)); } catch (...) { return 0; }
        const int base = resolveSystem(state, rest.substr(second + 1));
        const auto* from = base ? gameplay::systemById(state, base) : nullptr;
        if (!from || n < 1) return 0;
        std::vector<const System*> candidates;
        for (const auto& sys : state.systems) if (sys.id != base && systemFree(state, sys.id)) candidates.push_back(&sys);
        auto key = [&](const System* x) {
            const int dx = x->x - from->x, dy = x->y - from->y;
            return std::tuple<int, int, int>(std::max(std::abs(dx), std::abs(dy)), dx * dx + dy * dy, x->id);
        };
        std::sort(candidates.begin(), candidates.end(), [&](const System* a, const System* b) { return key(a) < key(b); });
        return static_cast<std::size_t>(n) <= candidates.size() ? candidates[static_cast<std::size_t>(n) - 1]->id : 0;
    }
    return 0;
}

int yearOf(const GameState& state) {
    const auto* sc = state.scenarioId.empty() ? nullptr : find(state.scenarioId);
    return sc ? sc->year + state.turn / 4 : 0;
}

// ---- Flagships ----------------------------------------------------------------------------------------------------------------
const Flagship* flagshipOf(const GameState& state, int empireId) {
    for (const auto& f : state.flagships) if (f.empireId == empireId) return &f;
    return nullptr;
}
const Fleet* flagshipFleet(const GameState& state, int empireId) {
    const auto* rec = flagshipOf(state, empireId);
    if (!rec || rec->fleetId == 0) return nullptr;
    for (const auto& f : state.fleets) if (f.id == rec->fleetId) return &f;
    return nullptr;
}
int flagshipMorale(const GameState& state, int empireId) {
    if (state.flagships.empty() || !flagshipFleet(state, empireId)) return 0;
    const auto* d = flagshipDef(empireId);
    return d ? d->morale : 0;
}
std::string commissionBlocker(const GameState& state, int empireId, int fleetId) {
    if (!flagshipsOn()) return "This game has no flagships";
    if (state.outcome != CampaignOutcome::Ongoing) return "Campaign has ended";
    const auto* def = flagshipDef(empireId);
    if (!def) return "This empire has no flagship";
    const Fleet* fleet = nullptr;
    for (const auto& f : state.fleets) if (f.id == fleetId) fleet = &f;
    if (!fleet || fleet->ownerId != empireId) return "Choose one of our ships";
    if (!armed(*fleet)) return "Only an armed warship can be a flagship";
    if (fleet->flagship > 0) return "That ship is already the flagship";
    if (flagshipFleet(state, empireId)) return "The empire already has a flagship";
    if (const auto* rec = flagshipOf(state, empireId))
        if (rec->lostTurn > 0 && state.turn < rec->lostTurn + def->cooldown)
            return "A new flagship can be commissioned on turn " + std::to_string(rec->lostTurn + def->cooldown);
    if (empire(state, empireId).credits < def->cost) return "Commissioning needs " + std::to_string(def->cost) + " credits";
    return {};
}
Command makeCommissionFlagshipCommand(std::uint64_t id, int fleetId) {
    Command c; c.id = id; c.type = CommandType::CommissionFlagship; c.fleetId = fleetId; return c;
}
namespace scn_detail {
void markFlagship(GameState& state, int empireId, Fleet& fleet) {
    const int oldHull = std::max(1, fleetMaxHull(fleet)), oldShields = std::max(1, fleetMaxShields(fleet));
    const bool hadShields = fleetMaxShields(fleet) > 0;
    Flagship* rec = nullptr;
    for (auto& f : state.flagships) if (f.empireId == empireId) rec = &f;
    if (!rec) {
        state.flagships.push_back({empireId, 0, 0, 0});
        std::sort(state.flagships.begin(), state.flagships.end(), [](const Flagship& a, const Flagship& b) { return a.empireId < b.empireId; });
        for (auto& f : state.flagships) if (f.empireId == empireId) rec = &f;
    }
    rec->serial += 1; rec->fleetId = fleet.id; rec->lostTurn = 0;
    fleet.flagship = rec->serial;
    // The refit scales the ship's current hull and shields with the new maxima, so a damaged ship stays equally damaged.
    fleet.hull = std::min(fleetMaxHull(fleet), std::max(1, fleet.hull * fleetMaxHull(fleet) / oldHull));
    if (hadShields) fleet.shields = std::min(fleetMaxShields(fleet), fleet.shields * fleetMaxShields(fleet) / oldShields);
    if (state.rules.crewExperience) fleet.experience = std::max(fleet.experience, 75);
}
} // namespace scn_detail
void commission(GameState& state, int empireId, int fleetId) {
    const auto blocker = commissionBlocker(state, empireId, fleetId);
    require(blocker.empty(), blocker);
    const auto* def = flagshipDef(empireId);
    internal::mutableEmpire(state, empireId).credits -= def->cost;
    markFlagship(state, empireId, *fleetMut(state, fleetId));
}
void planFlagships(GameState& next, int empireId) {
    if (!flagshipsOn() || empireId == next.playerEmpire || flagshipFleet(next, empireId)) return;
    const auto* def = flagshipDef(empireId);
    if (!def || empire(next, empireId).credits < 2 * static_cast<std::int64_t>(def->cost)) return;
    const Fleet* best = nullptr;
    for (const auto& f : next.fleets)
        if (f.ownerId == empireId && armed(f) && f.flagship == 0 && (!best || fleetAttack(f) * 10 + f.hull > fleetAttack(*best) * 10 + best->hull)) best = &f;
    if (!best || !commissionBlocker(next, empireId, best->id).empty()) return;
    commission(next, empireId, best->id);
}

// ---- Vendetta -----------------------------------------------------------------------------------------------------------------
std::vector<int> vendettaRivals(int empireId) {
    // Design call (Simon can overturn): the vanilla pairings are not in the data we hold; the Federation's pair is the wiki's own example
    // (UFP defeats the Romulans and the Cardassians). Each other race names its two historical enemies among the original majors.
    switch (empireId) {
    case 1: return {3, 4};
    case 2: return {1, 3};
    case 3: return {1, 2};
    case 4: return {1, 2};
    case 5: return {1, 2};
    case 6: return {1, 2};
    case 7: return {1, 3};
    case 8: return {1, 2};
    default: return {};
    }
}

// ---- Secession ----------------------------------------------------------------------------------------------------------------
bool secede(GameState& s, TurnReport* report, int parent, int house, int pct) {
    require(seated(s, parent), "Secession: the parent empire is not seated");
    require(slots::validSlot(house) && !seated(s, house), "Secession: the house must be a free empire slot");
    require(pct >= 10 && pct <= 90, "Secession: share outside 10..90 percent");
    const auto* parentSys = [&]() -> const System* {
        const std::string capital = slots::capital(parent);
        for (const auto& sys : s.systems) if (sys.name == capital) return &sys;
        return nullptr;
    }();
    // Colonies other than the capital, farthest first.
    std::vector<int> pool;
    for (const auto& c : s.colonies) if (c.ownerId == parent && (!parentSys || c.systemId != parentSys->id)) pool.push_back(c.systemId);
    if (pool.size() < 2) return false;   // Too small to split: the loyalists keep everything.
    auto dist = [&](int id) {
        const auto* sys = gameplay::systemById(s, id);
        if (!sys || !parentSys) return 0;
        return std::max(std::abs(sys->x - parentSys->x), std::abs(sys->y - parentSys->y));
    };
    std::sort(pool.begin(), pool.end(), [&](int a, int b) { return dist(a) != dist(b) ? dist(a) > dist(b) : a < b; });
    const std::size_t take = std::clamp<std::size_t>(pool.size() * static_cast<std::size_t>(pct) / 100, 1, pool.size() - 1);
    pool.resize(take);
    std::set<int> moved(pool.begin(), pool.end());

    EmpireState houseState = empire(s, parent);
    houseState.id = house;
    const std::int64_t share = std::max<std::int64_t>(0, empire(s, parent).credits) * pct / 100;
    internal::mutableEmpire(s, parent).credits -= share;
    houseState.credits = share; houseState.intelligence = 0;   // The house keeps its parent's technology, research ledger and charts.
    internal::seatEmpire(s, houseState);

    // Colonies: the far provinces rise under the house. Its first world becomes its capital (renamed, with a shipyard).
    for (int id : pool) {
        auto* c = [&]() -> Colony* { for (auto& col : s.colonies) if (col.systemId == id) return &col; return nullptr; }();
        c->ownerId = house;
        c->queue.clear(); c->overflow = 0; c->program = 0; c->programTurns = 0; c->subjugated = false;
        c->workforceManualOverride = false;
        if (const auto* sys = gameplay::systemById(s, id)) internal::reveal(s, sys->x, sys->y, house);
    }
    {
        auto* capital = [&]() -> Colony* { for (auto& col : s.colonies) if (col.systemId == pool.front()) return &col; return nullptr; }();
        capital->shipyards = 1; capital->shipyardEnabled = true;
        if (auto* sys = systemMut(s, pool.front())) sys->name = slots::capital(house);
    }
    // Ships: everything in a rebel sector, and every other warship besides, if the house can crew its class. The flagship stays loyal.
    int alternate = 0;
    for (auto& f : s.fleets) {
        if (f.ownerId != parent || f.flagship > 0) continue;
        bool rebel = false;
        for (int id : moved) {
            const auto* sys = gameplay::systemById(s, id);
            if (sys && sys->x == f.x && sys->y == f.y) rebel = true;
        }
        if (!rebel && armed(f)) rebel = (alternate++ % 2) == 0;
        if (!rebel) continue;
        if (f.referenceShipId >= 0) {
            const auto* ship = roster::findShip(f.referenceShipId);
            if (!ship || !internal::shipUsable(*ship, house)) continue;
        }
        internal::detachFromTaskForce(s, f);
        f.ownerId = house; f.attackTarget = 0; f.retreat = false; f.invadeSystemId = 0;
        economy_rules::clearOrder(f);
    }
    internal::reconcileTaskForces(s);
    for (auto& st : s.stations) {
        if (st.ownerId != parent) continue;
        for (int id : moved) {
            const auto* sys = gameplay::systemById(s, id);
            if (sys && sys->x == st.x && sys->y == st.y) st.ownerId = house;
        }
    }
    std::erase_if(s.megaprojects, [&](const Megaproject& m) { return moved.count(m.systemId) > 0; });
    std::erase_if(s.tradeRoutes, [&](const TradeRoute& r) { return moved.count(r.fromSystemId) > 0 || moved.count(r.toSystemId) > 0; });
    std::erase_if(s.stationUpgrades, [&](const StationUpgrade& u) { return moved.count(u.fundingSystemId) > 0; });
    // Relations: the house sees what its parent saw, owes nothing to its treaties and is at war with it.
    for (int other : internal::activeEmpires(s)) {
        if (other == house || other == parent) continue;
        const auto& pr = internal::relation(s, parent, other);
        const bool contact = pr.contact, atWar = pr.atWar;
        const int trust = pr.trust;
        auto& r = internal::relation(s, house, other);
        r.contact = contact; r.trust = trust;
        if (atWar) { r.atWar = true; r.warStartTurn = s.turn; }
    }
    {   auto& r = internal::relation(s, parent, house);
        r.contact = true; r.atWar = true; r.warStartTurn = s.turn; r.trust = 0; r.tradeTreaty = false;
    }
    for (auto& m : s.minors) {
        m.contact[static_cast<std::size_t>(house)] = m.contact[static_cast<std::size_t>(parent)];
        m.attitude[static_cast<std::size_t>(house)] = m.attitude[static_cast<std::size_t>(parent)];
    }
    internal::syncDiplomacy(s);
    s.secessions.push_back({parent, house, s.turn});
    say(report, ChronicleCategory::Diplomacy, std::string("Civil war: ") + slots::name(house) + " has split from the " + slots::name(parent) + " with " +
        std::to_string(take) + " worlds and a share of its fleets. The two are at war.", parent, house, pool.front());
    return true;
}

// ---- Actions ------------------------------------------------------------------------------------------------------------------
void runAction(GameState& s, const Action& a, TurnReport* report) {
    struct Soft { bool old; explicit Soft(bool on) : old(g_soft) { g_soft = on; } ~Soft() { g_soft = old; } } soft(report != nullptr);
    switch (a.kind) {
    case ActionKind::War: {
        const int x = empireOrThrow(s, a.a), y = empireOrThrow(s, a.b);
        if (!x || !y) break;
        internal::relation(s, x, y).contact = true;
        internal::war(s, x, y, report != nullptr);
        break;
    }
    case ActionKind::Peace: {
        const int x = empireOrThrow(s, a.a), y = empireOrThrow(s, a.b);
        if (!x || !y) break;
        internal::relation(s, x, y).contact = true;
        internal::peace(s, x, y);
        break;
    }
    case ActionKind::Alliance: {
        const int x = empireOrThrow(s, a.a), y = empireOrThrow(s, a.b);
        if (!x || !y) break;
        internal::alliance(s, x, y);
        break;
    }
    case ActionKind::Contact: {
        const int x = empireOrThrow(s, a.a), y = empireOrThrow(s, a.b);
        if (!x || !y) break;
        internal::relation(s, x, y).contact = true;
        for (const auto& pair : {std::pair<int, int>{x, y}, std::pair<int, int>{y, x}}) {
            const std::string capital = slots::capital(pair.second);
            for (const auto& sys : s.systems) if (sys.name == capital) internal::reveal(s, sys.x, sys.y, pair.first);
        }
        break;
    }
    case ActionKind::Colony: {
        const int owner = empireOrThrow(s, a.a), id = systemOrThrow(s, a.at);
        if (!owner || !id) break;
        if (!systemFree(s, id) && g_soft) break;
        require(systemFree(s, id), "Scenario: colony placed on an occupied system");
        Colony c;
        c.systemId = id; c.ownerId = owner;
        c.population = std::min(a.population, 50);
        c.foodWorkers = std::min(20, (c.population * 2 + 4) / 5);
        c.industryWorkers = std::min(20, c.population * 2 / 5);
        c.researchWorkers = std::min(10, c.population - c.foodWorkers - c.industryWorkers);
        s.colonies.push_back(c);
        std::sort(s.colonies.begin(), s.colonies.end(), [](const Colony& l, const Colony& r) { return l.systemId < r.systemId; });
        if (const auto* sys = gameplay::systemById(s, id)) internal::reveal(s, sys->x, sys->y, owner);
        break;
    }
    case ActionKind::Fleet: {
        const int owner = empireOrThrow(s, a.a), id = systemOrThrow(s, a.at);
        if (!owner || !id) break;
        for (const auto& name : a.ships) {
            const int shipId = findShipId(name, owner);
            for (int i = 0; i < a.count; ++i) spawnShip(s, owner, id, shipId);
        }
        break;
    }
    case ActionKind::Flagship: {
        require(flagshipsOn(), "Scenario: a flagship needs a pack with flagships");
        const int owner = empireOrThrow(s, a.a), id = systemOrThrow(s, a.at);
        if (!owner || !id) break;
        require(flagshipDef(owner) != nullptr, "Scenario: this empire has no flagship definition");
        if (g_soft && flagshipFleet(s, owner)) break;
        require(!flagshipFleet(s, owner), "Scenario: the empire already has a flagship");
        const int fleetId = spawnShip(s, owner, id, findShipId(a.name, owner));
        Fleet* f = fleetMut(s, fleetId);
        require(armed(*f), "Scenario: a flagship must be an armed ship");
        markFlagship(s, owner, *f);
        break;
    }
    case ActionKind::PlaceMinor: {
        const int target = systemOrThrow(s, a.at);
        if (!target) break;
        auto it = std::find_if(s.minors.begin(), s.minors.end(), [&](const Minor& m) { return m.name == a.name; });
        if (it == s.minors.end() && g_soft) break;
        require(it != s.minors.end(), "Scenario: minor race \"" + a.name + "\" is not in this galaxy");
        if (it->systemId == target) break;
        require(systemFree(s, target), "Scenario: the minor race's new home is occupied");
        System* from = systemMut(s, it->systemId);
        System* to = systemMut(s, target);
        std::swap(from->name, to->name);
        std::swap(from->planets, to->planets);
        for (auto& p : from->planets) p.systemId = from->id;
        for (auto& p : to->planets) p.systemId = to->id;
        const int old = it->systemId;
        it->systemId = target;
        for (auto& ms : s.diplomacyEngineState.minorStates) if (ms.minorId == old) { ms.minorId = target; ms.systemId = target; }
        for (auto& g : s.minorGuards) if (g.systemId == old) g.systemId = target;
        std::sort(s.minors.begin(), s.minors.end(), [](const Minor& l, const Minor& r) { return l.systemId < r.systemId; });
        break;
    }
    case ActionKind::Wormhole: {
        require(s.rules.galaxyFeatures, "Scenario: a wormhole needs galaxy features");
        const int one = systemOrThrow(s, a.at), two = systemOrThrow(s, a.name);
        if (!one || !two) break;
        const auto* sa = gameplay::systemById(s, one);
        const auto* sb = gameplay::systemById(s, two);
        int ax = 0, ay = 0, bx = 0, by = 0;
        require(freeCellNear(s, *sa, ax, ay), "Scenario: no free sector for a wormhole mouth");
        const int first = gameplay::addFeature(s, GalaxyFeatureType::Wormhole, ax, ay, 0);
        require(freeCellNear(s, *sb, bx, by), "Scenario: no free sector for a wormhole mouth");
        const int second = gameplay::addFeature(s, GalaxyFeatureType::Wormhole, bx, by, 0);
        for (auto& f : s.galaxyFeatures) {
            if (f.id == first) f.pairId = second;
            if (f.id == second) f.pairId = first;
        }
        break;
    }
    case ActionKind::PlaceThreat: {
        const int id = systemOrThrow(s, a.at);
        if (!id) break;
        const auto* sys = gameplay::systemById(s, id);
        int kind = -1;
        for (int i = 0; i < ThreatKinds; ++i) if (a.name == threats::Defs[i].key) kind = i;
        auto it = std::find_if(s.threatActors.begin(), s.threatActors.end(), [kind](const ThreatActor& t) { return t.kind == kind; });
        if (it != s.threatActors.end()) { it->x = it->targetX = sys->x; it->y = it->targetY = sys->y; break; }
        ThreatActor actor;
        actor.id = s.nextThreatId++; actor.kind = kind; actor.spawnTurn = s.turn;
        actor.x = actor.targetX = sys->x; actor.y = actor.targetY = sys->y;
        actor.hull = threats::actorMaxHull(actor); actor.shields = threats::actorMaxShields(actor);
        s.threatActors.push_back(actor);
        s.threatLastSpawn[static_cast<std::size_t>(kind)] = s.turn;
        break;
    }
    case ActionKind::Secede: {
        const int parent = empireOrThrow(s, a.a);
        if (!parent) break;
        const auto* race = slots::raceForKey(a.b);
        require(race != nullptr, "Scenario: unknown house \"" + a.b + "\"");
        if (g_soft && (seated(s, race->empireId) || !slots::validSlot(race->empireId))) break;
        secede(s, report, parent, race->empireId, a.pct);
        break;
    }
    case ActionKind::Credits: {
        const int owner = empireOrThrow(s, a.a);
        if (!owner) break;
        auto& e = internal::mutableEmpire(s, owner);
        e.credits = std::max<std::int64_t>(0, e.credits + a.amount);
        break;
    }
    }
}

void applySetup(GameState& s, const Scenario& sc) {
    require(s.turn == 0 && s.pending.empty(), "A scenario starts a new campaign");
    s.scenarioId = sc.id;
    for (const auto& a : sc.setup) runAction(s, a, nullptr);
    internal::syncDiplomacy(s);
}

// ---- Turn step ----------------------------------------------------------------------------------------------------------------
namespace scn_detail {
bool conditionHolds(const GameState& s, const When& w) {
    if (w.turn >= 0 && s.turn < w.turn) return false;
    if (!w.eliminated.empty()) { const int e = resolveEmpire(s, w.eliminated); if (!e || !eliminatedEmpire(s, e)) return false; }
    if (!w.warA.empty()) {
        const int x = resolveEmpire(s, w.warA), y = resolveEmpire(s, w.warB);
        if (!x || !y) return false;
        const auto* r = majorRelation(s, x, y);
        if (!r || !r->atWar) return false;
    }
    if (!w.colEmpire.empty()) {
        const int e = resolveEmpire(s, w.colEmpire);
        if (!e) return false;
        const int n = coloniesOf(s, e);
        if (w.colAtLeast >= 0 && n < w.colAtLeast) return false;
        if (w.colAtMost >= 0 && n > w.colAtMost) return false;
    }
    if (!w.capitalLost.empty()) {
        const int e = resolveEmpire(s, w.capitalLost);
        if (!e) return false;
        const int sysId = resolveSystem(s, "capital:" + w.capitalLost);
        const auto* c = sysId ? colonyAt(s, sysId) : nullptr;
        if (c && c->ownerId == e) return false;
    }
    return true;
}
} // namespace scn_detail

void step(GameState& next, TurnReport& report) {
    // Flagship upkeep: a flagship whose ship is gone is marked lost (and may be replaced after the cooldown).
    for (auto& rec : next.flagships) {
        if (rec.fleetId == 0) continue;
        const bool alive = std::any_of(next.fleets.begin(), next.fleets.end(), [&](const Fleet& f) { return f.id == rec.fleetId && f.hull > 0; });
        if (alive) continue;
        const std::string name = [&] {
            const auto* d = flagshipDef(rec.empireId);
            return d ? d->names[static_cast<std::size_t>(rec.serial - 1) % d->names.size()] : std::string("The flagship");
        }();
        rec.fleetId = 0; rec.lostTurn = next.turn;
        say(&report, ChronicleCategory::Combat, name + " has been lost. " + slots::name(rec.empireId) + " mourns its flagship.", rec.empireId);
    }
    if (next.scenarioId.empty()) return;
    const auto* sc = find(next.scenarioId);
    if (!sc) return;
    for (std::size_t i = 0; i < sc->triggers.size(); ++i) {
        const int index = static_cast<int>(i);
        if (std::binary_search(next.scenarioFired.begin(), next.scenarioFired.end(), index)) continue;
        const auto& t = sc->triggers[i];
        if (!conditionHolds(next, t.when)) continue;
        next.scenarioFired.push_back(index);
        std::sort(next.scenarioFired.begin(), next.scenarioFired.end());
        say(&report, ChronicleCategory::Events, t.text);
        for (const auto& a : t.actions) runAction(next, a, &report);
    }
}

// ---- Scenario victories -------------------------------------------------------------------------------------------------------
bool victoryAchieved(const GameState& s, const Victory& v) {
    switch (v.kind) {
    case VictoryKind::Eliminate:
        for (const auto& e : v.empires) { const int id = resolveEmpire(s, e); if (!id || !eliminatedEmpire(s, id)) return false; }
        return true;
    case VictoryKind::Hold:
        for (const auto& r : v.systems) {
            const int id = resolveSystem(s, r);
            const auto* c = id ? colonyAt(s, id) : nullptr;
            if (!c || c->ownerId != s.playerEmpire) return false;
        }
        return true;
    case VictoryKind::Survive:
        return s.turn >= v.turn && coloniesOf(s, s.playerEmpire) > 0;
    }
    return false;
}
std::vector<ObjectiveStatus> objectives(const GameState& s) {
    std::vector<ObjectiveStatus> out;
    const auto* sc = s.scenarioId.empty() ? nullptr : find(s.scenarioId);
    if (!sc) return out;
    for (const auto& v : sc->victories) {
        ObjectiveStatus o;
        o.id = v.id; o.name = v.name; o.text = v.text; o.achieved = victoryAchieved(s, v);
        int have = 0, total = 1;
        switch (v.kind) {
        case VictoryKind::Eliminate:
            total = static_cast<int>(v.empires.size());
            for (const auto& e : v.empires) { const int id = resolveEmpire(s, e); if (id && eliminatedEmpire(s, id)) ++have; }
            break;
        case VictoryKind::Hold:
            total = static_cast<int>(v.systems.size());
            for (const auto& r : v.systems) { const int id = resolveSystem(s, r); const auto* c = id ? colonyAt(s, id) : nullptr; if (c && c->ownerId == s.playerEmpire) ++have; }
            break;
        case VictoryKind::Survive:
            total = v.turn; have = std::min(s.turn, v.turn);
            break;
        }
        o.percent = o.achieved ? 100 : std::clamp(100 * have / std::max(1, total), 0, 99);
        out.push_back(std::move(o));
    }
    return out;
}
void evaluateVictories(GameState& next, TurnReport& report) {
    if (next.scenarioId.empty() || next.outcome != CampaignOutcome::Ongoing) return;
    const auto* sc = find(next.scenarioId);
    if (!sc) return;
    for (const auto& v : sc->victories) {
        if (!victoryAchieved(next, v)) continue;
        next.outcome = CampaignOutcome::Victory;
        next.winnerEmpire = next.playerEmpire;
        say(&report, ChronicleCategory::Victory, std::string(slots::name(next.playerEmpire)) + ": " + v.name + ". " + v.text + " Campaign victory.", next.playerEmpire);
        return;
    }
}

// ---- Validation and save ------------------------------------------------------------------------------------------------------
void validate(const GameState& s) {
    if (!s.scenarioId.empty()) {
        const auto* sc = find(s.scenarioId);
        require(sc != nullptr, "Save needs scenario " + s.scenarioId + ", which is not installed");
        int previous = -1;
        for (int i : s.scenarioFired) {
            require(i > previous && i < static_cast<int>(sc->triggers.size()), "Invalid fired trigger list");
            previous = i;
        }
    } else {
        require(s.scenarioFired.empty(), "Fired triggers without a scenario");
    }
    int priorEmpire = 0;
    std::set<int> marked;
    for (const auto& f : s.flagships) {
        require(f.empireId > priorEmpire && seated(s, f.empireId), "Flagship records must be one per seated empire, ascending");
        priorEmpire = f.empireId;
        require(flagshipsOn(), "Flagships without a flagships pack");
        require(f.serial >= 1 && f.serial < 10000 && f.lostTurn >= 0 && f.lostTurn <= s.turn, "Invalid flagship record");
        if (f.fleetId == 0) {
            require(f.lostTurn > 0, "A flagship record without a ship was never lost");
        } else {
            require(f.lostTurn == 0, "A living flagship was lost");
            const Fleet* fleet = nullptr;
            for (const auto& fl : s.fleets) if (fl.id == f.fleetId) fleet = &fl;
            require(fleet && fleet->ownerId == f.empireId && fleet->flagship == f.serial && fleet->hull > 0, "Flagship record does not match its ship");
            marked.insert(f.fleetId);
        }
    }
    for (const auto& fl : s.fleets) require(fl.flagship == 0 || marked.count(fl.id) > 0, "A ship carries a flagship mark with no record");
    for (const auto& sec : s.secessions) {
        require(seated(s, sec.parent) && seated(s, sec.house) && sec.parent != sec.house && sec.turn >= 0 && sec.turn <= s.turn, "Invalid secession record");
    }
    require(s.secessions.size() <= 8, "Too many secessions");
}
bool sectionNeeded(const GameState& s) { return !s.scenarioId.empty() || !s.flagships.empty() || !s.secessions.empty(); }
void writeSection(std::ostream& out, const GameState& s) {
    if (!sectionNeeded(s)) return;
    out << "SCENARIO 1 " << (s.scenarioId.empty() ? std::string("-") : s.scenarioId) << ' ' << s.scenarioFired.size();
    for (int i : s.scenarioFired) out << ' ' << i;
    out << '\n';
    out << "FLAGSHIPS " << s.flagships.size() << '\n';
    for (const auto& f : s.flagships) out << f.empireId << ' ' << f.fleetId << ' ' << f.serial << ' ' << f.lostTurn << '\n';
    out << "SECESSIONS " << s.secessions.size() << '\n';
    for (const auto& c : s.secessions) out << c.parent << ' ' << c.house << ' ' << c.turn << '\n';
}
void readSection(std::istream& in, GameState& s) {
    int revision = 0; std::string id; std::size_t fired = 0;
    require(static_cast<bool>(in >> revision >> id >> fired) && revision == 1 && !id.empty() && fired <= static_cast<std::size_t>(MaxTriggers), "Malformed SCENARIO section");
    require(id == "-" || content::detail::validPackId(id), "Malformed SCENARIO section");
    s.scenarioId = id == "-" ? std::string() : id;
    for (std::size_t i = 0; i < fired; ++i) {
        int v = -1;
        require(static_cast<bool>(in >> v), "Malformed SCENARIO section");
        s.scenarioFired.push_back(v);
    }
    std::string tag; std::size_t n = 0;
    require(static_cast<bool>(in >> tag >> n) && tag == "FLAGSHIPS" && n <= 8, "Malformed FLAGSHIPS rows");
    for (std::size_t i = 0; i < n; ++i) {
        Flagship f;
        require(static_cast<bool>(in >> f.empireId >> f.fleetId >> f.serial >> f.lostTurn), "Malformed FLAGSHIPS rows");
        s.flagships.push_back(f);
        if (f.fleetId > 0) for (auto& fl : s.fleets) if (fl.id == f.fleetId) fl.flagship = f.serial;
    }
    require(static_cast<bool>(in >> tag >> n) && tag == "SECESSIONS" && n <= 8, "Malformed SECESSIONS rows");
    for (std::size_t i = 0; i < n; ++i) {
        Secession c;
        require(static_cast<bool>(in >> c.parent >> c.house >> c.turn), "Malformed SECESSIONS rows");
        s.secessions.push_back(c);
    }
    require(sectionNeeded(s), "Non-canonical empty SCENARIO section");
}
} // namespace botf::scenarios
