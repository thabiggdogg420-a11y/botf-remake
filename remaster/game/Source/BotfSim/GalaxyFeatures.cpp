// Schema-13 galaxy features and anomalies: generation, pathfinding (impassable
// black holes and rifts, hazard avoidance, wormhole links), supply through known
// wormholes, sensor-blind nebulae, sector hazards, drifting ion storms and
// one-time scout survey rewards.
#include "GameplayInternal.h"

#include <algorithm>
#include <array>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>

namespace botf::gameplay {
constexpr std::array<std::pair<int, int>, 8> FeatureDirections{{
    {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}}};
constexpr int FeatureLimit = 256;

bool featureAtCell(const GameState& s, GalaxyFeatureType type, int x, int y) {
    for (const auto& f : s.galaxyFeatures) if (f.type == type && f.x == x && f.y == y) return true;
    return false;
}
bool insideIonStorm(const GameState& s, int x, int y) {
    for (const auto& f : s.galaxyFeatures)
        if (f.type == GalaxyFeatureType::IonStorm && ((f.x == x && std::abs(f.y - y) <= 1) || (f.y == y && std::abs(f.x - x) <= 1)))
            return true;
    return false;
}
const GalaxyFeature* featureById(const GameState& s, int id) {
    for (const auto& f : s.galaxyFeatures) if (f.id == id) return &f;
    return nullptr;
}

int addFeature(GameState& s, GalaxyFeatureType type, int x, int y, int expiresTurn) {
    require(s.nextFeatureId < 10000000 && s.galaxyFeatures.size() < FeatureLimit, "Galaxy feature capacity reached");
    GalaxyFeature f;
    f.id = s.nextFeatureId++;
    f.type = type;
    f.x = x; f.y = y;
    f.expiresTurn = expiresTurn;
    s.galaxyFeatures.push_back(f);
    return f.id;
}

void generateGalaxyFeatures(GameState& s) {
    // A dedicated stream keeps the base galaxy identical to a Classic game of the same seed.
    std::uint64_t g = s.seed ^ 0xa0761d6478bd642fULL;
    const int w = s.width, h = s.height, area = w * h;
    std::vector<char> blocked(static_cast<std::size_t>(area), 0);
    for (const auto& sys : s.systems) blocked[static_cast<std::size_t>(sys.y * w + sys.x)] = 1;
    auto nearHome = [&](int x, int y) {
        for (const auto& c : s.colonies) {
            const auto sys = systemById(s, c.systemId);
            if (sys && chebyshev(sys->x, sys->y, x, y) <= 1) return true;
        }
        return false;
    };
    auto freeCell = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < w && y < h && !blocked[static_cast<std::size_t>(y * w + x)] && !nearHome(x, y);
    };
    auto border = [&](int x, int y) { return x == 0 || y == 0 || x == w - 1 || y == h - 1; };
    auto pick = [&](const std::function<bool(int, int)>& ok, int& ox, int& oy) {
        for (int attempt = 0; attempt < 400; ++attempt) {
            const int cell = static_cast<int>(internal::bounded(g, static_cast<std::uint64_t>(area)));
            if (ok(cell % w, cell / w)) { ox = cell % w; oy = cell / w; return true; }
        }
        return false;
    };
    auto claim = [&](int x, int y) { blocked[static_cast<std::size_t>(y * w + x)] = 1; };
    int x = 0, y = 0, cluster = 0;
    // Anomaly counts keep pace with the galaxy: the original steps (area 150, 250, 300) hold up to a 22x16 Medium, and the
    // bigger grids (Large 28x20, Huge 34x24) add one and two more of each kind. No change below area 500.
    const int extra = area >= 700 ? 2 : (area >= 500 ? 1 : 0);
    // Nebula clouds: short random walks of 3-5 cells.
    const int clouds = (area >= 150 ? 2 : 1) + extra;
    for (int cloud = 0; cloud < clouds; ++cloud) {
        if (!pick(freeCell, x, y)) break;
        ++cluster;
        const int cells = 3 + static_cast<int>(internal::bounded(g, 3));
        int added = 0, cx = x, cy = y;
        for (int attempt = 0; attempt < 24 && added < cells; ++attempt) {
            if (freeCell(cx, cy)) {
                const int id = addFeature(s, GalaxyFeatureType::Nebula, cx, cy, 0);
                s.galaxyFeatures.back().clusterId = cluster;
                (void)id;
                claim(cx, cy);
                ++added;
            }
            const auto [dx, dy] = FeatureDirections[static_cast<std::size_t>(internal::bounded(g, 8))];
            if (cx + dx >= 0 && cy + dy >= 0 && cx + dx < w && cy + dy < h) { cx += dx; cy += dy; }
        }
    }
    // Black holes: interior cells clear of systems and other anomalies, so the
    // 8-connected map stays connected.
    const int blackHoles = (area >= 300 ? 2 : 1) + (extra > 1 ? 1 : 0);
    for (int i = 0; i < blackHoles; ++i) {
        const auto ok = [&](int px, int py) {
            if (!freeCell(px, py) || border(px, py)) return false;
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
                if (blocked[static_cast<std::size_t>((py + dy) * w + px + dx)]) return false;
            return true;
        };
        if (!pick(ok, x, y)) break;
        addFeature(s, GalaxyFeatureType::BlackHole, x, y, 0);
        claim(x, y);
    }
    const int neutronStars = (area >= 250 ? 2 : 1) + extra;
    for (int i = 0; i < neutronStars; ++i) {
        if (!pick(freeCell, x, y)) break;
        addFeature(s, GalaxyFeatureType::NeutronStar, x, y, 0);
        claim(x, y);
    }
    const int wormholes = (area >= 300 ? 2 : 1) + extra;
    const int span = std::max(5, std::max(w, h) / 2);
    for (int i = 0; i < wormholes; ++i) {
        int ax = 0, ay = 0, bx = 0, by = 0;
        if (!pick(freeCell, ax, ay)) break;
        claim(ax, ay);
        if (!pick([&](int px, int py) { return freeCell(px, py) && chebyshev(px, py, ax, ay) >= span; }, bx, by)) {
            blocked[static_cast<std::size_t>(ay * w + ax)] = 0;
            break;
        }
        claim(bx, by);
        const int first = addFeature(s, GalaxyFeatureType::Wormhole, ax, ay, 0);
        const int second = addFeature(s, GalaxyFeatureType::Wormhole, bx, by, 0);
        s.galaxyFeatures[s.galaxyFeatures.size() - 2].pairId = second;
        s.galaxyFeatures.back().pairId = first;
    }
    const int storms = (area >= 300 ? 2 : 1) + extra;
    for (int i = 0; i < storms; ++i) {
        if (!pick([&](int px, int py) { return freeCell(px, py) && !border(px, py); }, x, y)) break;
        addFeature(s, GalaxyFeatureType::IonStorm, x, y, 0);
        const auto [dx, dy] = FeatureDirections[static_cast<std::size_t>(internal::bounded(g, 8))];
        s.galaxyFeatures.back().driftX = dx;
        s.galaxyFeatures.back().driftY = dy;
    }
}

void validateFeatures(const GameState& s) {
    require(s.rules.galaxyFeatures || s.galaxyFeatures.empty(), "Galaxy features are disabled by the ruleset");
    require(s.nextFeatureId >= 1 && s.nextFeatureId <= 10000000 && s.galaxyFeatures.size() <= FeatureLimit,
        "Invalid galaxy feature identity state");
    int prior = 0;
    std::set<int> cells;
    for (const auto& f : s.galaxyFeatures) {
        require(f.id > prior && f.id < s.nextFeatureId, "Galaxy feature IDs must be unique, positive and sorted");
        prior = f.id;
        require(static_cast<int>(f.type) >= 0 && static_cast<int>(f.type) <= 5, "Invalid galaxy feature type");
        require(f.x >= 0 && f.x < s.width && f.y >= 0 && f.y < s.height, "Galaxy feature outside galaxy");
        require((f.type == GalaxyFeatureType::Nebula) == (f.clusterId > 0) && f.clusterId >= 0 && f.clusterId <= 1000,
            "Invalid nebula cluster");
        if (f.type == GalaxyFeatureType::Wormhole) {
            const auto partner = featureById(s, f.pairId);
            require(partner && partner->type == GalaxyFeatureType::Wormhole && partner->pairId == f.id &&
                (partner->x != f.x || partner->y != f.y), "Wormhole endpoints must pair up");
        } else {
            require(f.pairId == 0, "Only wormholes have partners");
        }
        if (f.type == GalaxyFeatureType::IonStorm)
            require(std::abs(f.driftX) <= 1 && std::abs(f.driftY) <= 1 && (f.driftX || f.driftY), "Invalid ion storm drift");
        else
            require(f.driftX == 0 && f.driftY == 0, "Only ion storms drift");
        if (f.type == GalaxyFeatureType::SubspaceRift)
            require(f.expiresTurn >= s.turn && f.expiresTurn <= s.turn + 100, "Invalid subspace rift lifetime");
        else
            require(f.expiresTurn == 0, "Only subspace rifts expire");
        require(f.surveyedMask >= 0 && f.surveyedMask < 64 && (f.surveyedMask & 1) == 0, "Invalid survey mask");
        if (f.type != GalaxyFeatureType::IonStorm)
            require(cells.insert(f.y * s.width + f.x).second, "Two anomalies share a sector");
        if (f.type == GalaxyFeatureType::BlackHole || f.type == GalaxyFeatureType::SubspaceRift) {
            require(!systemAtCell(s, f.x, f.y), "A star system lies inside an impassable anomaly");
            for (const auto& fleet : s.fleets)
                require(fleet.x != f.x || fleet.y != f.y, "A fleet lies inside an impassable anomaly");
        }
    }
}

bool extendedSupply(const GameState& s, int owner, int x, int y, const std::function<bool(int, int)>& colonySupplied) {
    if (x < 0 || y < 0 || x >= s.width || y >= s.height) return false;
    if (stationSupplies(s, owner, x, y)) return true;
    for (const auto& f : s.galaxyFeatures) {
        if (f.type != GalaxyFeatureType::Wormhole) continue;
        const auto exit = featureById(s, f.pairId);
        if (!exit || chebyshev(exit->x, exit->y, x, y) > 1 || !internal::known(s, owner, f.x, f.y)) continue;
        if (colonySupplied(f.x, f.y) || stationSupplies(s, owner, f.x, f.y)) return true;
    }
    return false;
}

bool extendedVisible(const GameState& s, int owner, int x, int y) {
    const bool targetNebula = sectorInNebula(s, x, y);
    const auto sees = [&](int sx, int sy, int radius) {
        if (targetNebula) return sx == x && sy == y; // Nebula interiors are sensor-blind from outside.
        if (radius > 0 && !s.galaxyFeatures.empty() && sectorInNebula(s, sx, sy)) radius = 0;
        return chebyshev(sx, sy, x, y) <= radius;
    };
    for (const auto& f : s.fleets) if (f.ownerId == owner && sees(f.x, f.y, 1)) return true;
    for (const auto& c : s.colonies) {
        if (c.ownerId != owner) continue;
        const auto sys = systemById(s, c.systemId);
        if (sys && sees(sys->x, sys->y, 1)) return true;
    }
    for (const auto& st : s.stations) if (st.ownerId == owner && sees(st.x, st.y, stationSensorRadiusFor(st.type))) return true;
    return false;
}

std::vector<int> findPath(const GameState& s, int owner, int start, int goal, const std::function<bool(int, int)>& supplied) {
    const int w = s.width, h = s.height, area = w * h;
    if (start < 0 || goal < 0 || start >= area || goal >= area) return {};
    if (start == goal) return {start};
    std::vector<char> blocked(static_cast<std::size_t>(area), 0), hazard(static_cast<std::size_t>(area), 0);
    std::map<int, int> links;
    bool anyHazard = false;
    for (const auto& f : s.galaxyFeatures) {
        const int cell = f.y * w + f.x;
        if (f.type == GalaxyFeatureType::BlackHole || f.type == GalaxyFeatureType::SubspaceRift) blocked[static_cast<std::size_t>(cell)] = 1;
        else if (f.type == GalaxyFeatureType::NeutronStar) { hazard[static_cast<std::size_t>(cell)] = 1; anyHazard = true; }
        else if (f.type == GalaxyFeatureType::IonStorm) {
            for (const auto& [cx, cy] : ionStormCells(s, f)) hazard[static_cast<std::size_t>(cy * w + cx)] = 1;
            anyHazard = true;
        } else if (f.type == GalaxyFeatureType::Wormhole && internal::known(s, owner, f.x, f.y)) {
            if (const auto exit = featureById(s, f.pairId)) links[cell] = exit->y * w + exit->x;
        }
    }
    if (blocked[static_cast<std::size_t>(goal)]) return {};
    std::vector<signed char> supplyCache(static_cast<std::size_t>(area), -1);
    const auto isSupplied = [&](int cell) {
        auto& cached = supplyCache[static_cast<std::size_t>(cell)];
        if (cached < 0) cached = supplied(cell % w, cell / w) ? 1 : 0;
        return cached == 1;
    };
    for (int pass = 0; pass < 2; ++pass) {
        const bool avoidHazards = pass == 0;
        if (!avoidHazards && !anyHazard) break;
        std::vector<int> previous(static_cast<std::size_t>(area), -1);
        std::queue<int> open;
        open.push(start);
        previous[static_cast<std::size_t>(start)] = start;
        while (!open.empty() && previous[static_cast<std::size_t>(goal)] == -1) {
            const int cell = open.front(); open.pop();
            const auto consider = [&](int next) {
                if (previous[static_cast<std::size_t>(next)] != -1 || blocked[static_cast<std::size_t>(next)]) return;
                if (avoidHazards && hazard[static_cast<std::size_t>(next)] && next != goal) return;
                if (!isSupplied(next)) return;
                previous[static_cast<std::size_t>(next)] = cell;
                open.push(next);
            };
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                if (!dx && !dy) continue;
                const int nx = cell % w + dx, ny = cell / w + dy;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                consider(ny * w + nx);
            }
            if (const auto link = links.find(cell); link != links.end()) consider(link->second);
        }
        if (previous[static_cast<std::size_t>(goal)] == -1) continue;
        std::vector<int> path;
        for (int cell = goal; cell != start; cell = previous[static_cast<std::size_t>(cell)]) path.push_back(cell);
        path.push_back(start);
        std::reverse(path.begin(), path.end());
        return path;
    }
    return {};
}

std::int64_t researchGrantFor(const GameState& s, int empireId, int field, int percent, std::int64_t floor) {
    const auto& tech = empire(s, empireId).technologies[static_cast<std::size_t>(field)];
    if (tech.level >= MaxTechLevel) return floor;
    return std::max<std::int64_t>(floor, static_cast<std::int64_t>(researchCost(s, field, tech.level + 1)) * percent / 100);
}

void grantResearch(GameState& s, int empireId, int field, std::int64_t amount, TurnReport* report) {
    require(field >= 0 && field < ResearchFields, "Invalid research field grant");
    auto& e = internal::mutableEmpire(s, empireId);
    amount = std::min(amount, internal::BalanceLimit - std::max(e.research, e.researchReserve));
    if (amount <= 0) return;
    e.research += amount;
    auto& tech = e.technologies[static_cast<std::size_t>(field)];
    if (tech.level >= MaxTechLevel) { e.researchReserve += amount; return; }
    tech.progress += amount;
    while (tech.level < MaxTechLevel && tech.progress >= researchCost(s, field, tech.level + 1)) {
        tech.progress -= researchCost(s, field, ++tech.level);
        if (report && empireId == s.playerEmpire) {
            ChronicleEntry entry;
            entry.category = ChronicleCategory::Research;
            entry.text = std::string(researchName(field)) + " advanced to level " + std::to_string(tech.level) + ". Benefits apply next turn.";
            entry.empireId = empireId;
            note(*report, std::move(entry));
        }
    }
    if (tech.level == MaxTechLevel) { e.researchReserve += tech.progress; tech.progress = 0; }
}

void surveyAnomalies(GameState& moving, TurnReport& report, TurnEffects& effects) {
    for (const auto& fleet : moving.fleets) {
        if (fleet.hull <= 0 || fleet.shipClass != Building::Scout) continue;
        const int bit = 1 << fleet.ownerId;
        for (std::size_t i = 0; i < moving.galaxyFeatures.size(); ++i) {
            const auto feature = moving.galaxyFeatures[i];
            if ((feature.surveyedMask & bit) || chebyshev(feature.x, feature.y, fleet.x, fleet.y) > 1) continue;
            for (auto& f : moving.galaxyFeatures) {
                const bool sameCloud = feature.type == GalaxyFeatureType::Nebula && f.type == GalaxyFeatureType::Nebula &&
                    f.clusterId == feature.clusterId;
                if (f.id == feature.id || f.id == feature.pairId || sameCloud) f.surveyedMask |= bit;
            }
            int field = 3, percent = 25; std::int64_t floor = 40, intel = 0;
            std::string extra;
            switch (feature.type) {
            case GalaxyFeatureType::Nebula: field = 3; percent = 25; floor = 40; break;
            case GalaxyFeatureType::Wormhole: {
                field = 4; percent = 20; floor = 40; intel = 60;
                if (const auto exit = featureById(moving, feature.pairId)) {
                    internal::reveal(moving, exit->x, exit->y, fleet.ownerId);
                    extra = "; its far end lies at " + sectorText(exit->x, exit->y);
                }
                break;
            }
            case GalaxyFeatureType::BlackHole: field = 4; percent = 30; floor = 50; break;
            case GalaxyFeatureType::NeutronStar: field = 2; percent = 25; floor = 40; break;
            case GalaxyFeatureType::IonStorm: field = 3; percent = 20; floor = 30; break;
            case GalaxyFeatureType::SubspaceRift: field = 1; percent = 35; floor = 60; break;
            }
            const auto amount = researchGrantFor(moving, fleet.ownerId, field, percent, floor);
            effects.deltas.push_back({fleet.ownerId, 0, field, amount, intel});
            if (fleet.ownerId == moving.playerEmpire) {
                ChronicleEntry e;
                e.category = ChronicleCategory::Exploration;
                e.text = fleetName(fleet) + " surveyed the " + galaxyFeatureName(feature.type) + " at " +
                    sectorText(feature.x, feature.y) + ": +" + std::to_string(amount) + " " + researchName(field) +
                    " research" + (intel ? " and +" + std::to_string(intel) + " intelligence" : std::string()) + extra + ".";
                e.x = feature.x; e.y = feature.y; e.fleetId = fleet.id; e.empireId = fleet.ownerId;
                note(report, std::move(e));
            }
        }
    }
}

void applySectorHazards(GameState& moving, TurnReport& report) {
    std::set<int> noted;
    const auto hazardNote = [&](int x, int y, const std::string& text) {
        if (!noted.insert(y * moving.width + x).second) return;
        ChronicleEntry e;
        e.category = ChronicleCategory::Exploration;
        e.text = text;
        e.x = x; e.y = y; e.empireId = moving.playerEmpire;
        note(report, std::move(e));
    };
    for (auto& f : moving.fleets) {
        if (f.hull <= 0) continue;
        const bool nebula = sectorInNebula(moving, f.x, f.y);
        const bool storm = insideIonStorm(moving, f.x, f.y);
        const bool neutron = featureAtCell(moving, GalaxyFeatureType::NeutronStar, f.x, f.y);
        if (!nebula && !storm && !neutron) continue;
        const int maxHull = fleetMaxHull(f);
        const int oldHull = f.hull, oldShields = f.shields;
        if (nebula || storm) f.shields = 0;
        int damage = 0;
        if (storm) damage += std::max(1, maxHull * 5 / 100);
        if (neutron) { damage += std::max(1, maxHull * 10 / 100); f.shields /= 2; }
        f.hull = std::max(1, f.hull - damage);
        if (f.ownerId != moving.playerEmpire || (f.hull == oldHull && f.shields == oldShields)) continue;
        if (storm) hazardNote(f.x, f.y, "The ion storm at " + sectorText(f.x, f.y) + " knocked out our shields and damaged our hulls.");
        else if (neutron) hazardNote(f.x, f.y, "Neutron star radiation at " + sectorText(f.x, f.y) + " is damaging our ships.");
        else hazardNote(f.x, f.y, "Our shields cannot hold inside the nebula at " + sectorText(f.x, f.y) + ".");
    }
    for (auto& st : moving.stations) {
        const bool storm = insideIonStorm(moving, st.x, st.y);
        if (sectorInNebula(moving, st.x, st.y) || storm) st.shields = 0;
        const int maxHull = stationStats(moving, st).maxHull;
        int damage = 0;
        if (storm) damage += std::max(1, maxHull * 5 / 100);
        if (featureAtCell(moving, GalaxyFeatureType::NeutronStar, st.x, st.y)) damage += std::max(1, maxHull * 10 / 100);
        st.hull = std::max(1, st.hull - damage);
    }
}

void driftIonStorms(GameState& next) {
    if (next.turn % IonStormDriftTurns != 0) return;
    for (auto& f : next.galaxyFeatures) {
        if (f.type != GalaxyFeatureType::IonStorm) continue;
        auto valid = [&](int x, int y) { return x >= 0 && y >= 0 && x < next.width && y < next.height && sectorPassable(next, x, y); };
        int nx = f.x + f.driftX, ny = f.y + f.driftY;
        if (nx < 0 || nx >= next.width) { f.driftX = -f.driftX; nx = f.x + f.driftX; }
        if (ny < 0 || ny >= next.height) { f.driftY = -f.driftY; ny = f.y + f.driftY; }
        if (!valid(nx, ny)) {
            f.driftX = -f.driftX; f.driftY = -f.driftY;
            nx = f.x + f.driftX; ny = f.y + f.driftY;
        }
        if (valid(nx, ny)) { f.x = nx; f.y = ny; }
    }
}

void expireFeatures(GameState& next, TurnReport& report) {
    std::vector<GalaxyFeature> expired;
    std::erase_if(next.galaxyFeatures, [&](const GalaxyFeature& f) {
        if (f.expiresTurn == 0 || next.turn < f.expiresTurn) return false;
        expired.push_back(f);
        return true;
    });
    for (const auto& f : expired) {
        for (auto& e : next.galacticEvents) if (e.featureId == f.id) e.featureId = 0;
        if (!internal::known(next, next.playerEmpire, f.x, f.y)) continue;
        ChronicleEntry entry;
        entry.category = ChronicleCategory::Events;
        entry.text = "The " + std::string(galaxyFeatureName(f.type)) + " at " + sectorText(f.x, f.y) + " has collapsed; the sector is passable again.";
        entry.x = f.x; entry.y = f.y;
        note(report, std::move(entry));
    }
}

void planAnomalySurveys(GameState& next) {
    static constexpr std::array<std::pair<int, int>, 8> Approach{{
        {-1, -1}, {1, -1}, {-1, 1}, {1, 1}, {0, -1}, {-1, 0}, {1, 0}, {0, 1}}};
    for (const auto& rival : next.rivals) {
        const int owner = rival.id, bit = 1 << owner;
        std::set<int> reserved;
        for (auto& fleet : next.fleets) {
            if (fleet.ownerId != owner || fleet.shipClass != Building::Scout || fleet.hull <= 0 || fleet.taskForceId ||
                fleet.retreat || fleet.attackTarget) continue;
            std::vector<const GalaxyFeature*> candidates;
            for (const auto& f : next.galaxyFeatures)
                if (!(f.surveyedMask & bit) && !reserved.count(f.clusterId ? -f.clusterId : f.id) && internal::known(next, owner, f.x, f.y))
                    candidates.push_back(&f);
            std::sort(candidates.begin(), candidates.end(), [&](const GalaxyFeature* a, const GalaxyFeature* b) {
                const int da = chebyshev(a->x, a->y, fleet.x, fleet.y), db = chebyshev(b->x, b->y, fleet.x, fleet.y);
                return da == db ? a->id < b->id : da < db;
            });
            int routeChecks = 0;
            for (const auto* f : candidates) {
                if (routeChecks >= 3) break;
                int tx = -1, ty = -1;
                if (sectorPassable(next, f->x, f->y) && !sectorHazardous(next, f->x, f->y)) { tx = f->x; ty = f->y; }
                else for (const auto& [dx, dy] : Approach) {
                    const int px = f->x + dx, py = f->y + dy;
                    if (px < 0 || py < 0 || px >= next.width || py >= next.height) continue;
                    if (sectorPassable(next, px, py) && !sectorHazardous(next, px, py)) { tx = px; ty = py; break; }
                }
                if (tx < 0) continue;
                ++routeChecks;
                if (!(fleet.x == tx && fleet.y == ty) && route(next, fleet, tx, ty).empty()) continue;
                fleet.targetX = tx; fleet.targetY = ty;
                reserved.insert(f->clusterId ? -f->clusterId : f->id);
                break;
            }
        }
    }
}
} // namespace botf::gameplay

namespace botf {
const char* galaxyFeatureName(GalaxyFeatureType type) {
    switch (type) {
    case GalaxyFeatureType::Nebula: return "Nebula";
    case GalaxyFeatureType::Wormhole: return "Wormhole";
    case GalaxyFeatureType::BlackHole: return "Black hole";
    case GalaxyFeatureType::NeutronStar: return "Neutron star";
    case GalaxyFeatureType::IonStorm: return "Ion storm";
    case GalaxyFeatureType::SubspaceRift: return "Subspace rift";
    default: return "Unknown anomaly";
    }
}
std::vector<std::pair<int, int>> ionStormCells(const GameState& state, const GalaxyFeature& storm) {
    std::vector<std::pair<int, int>> cells;
    for (const auto& [dx, dy] : {std::pair{0, 0}, std::pair{0, -1}, std::pair{-1, 0}, std::pair{1, 0}, std::pair{0, 1}}) {
        const int x = storm.x + dx, y = storm.y + dy;
        if (x >= 0 && y >= 0 && x < state.width && y < state.height) cells.push_back({x, y});
    }
    return cells;
}
std::vector<GalaxyFeature> featuresAt(const GameState& state, int x, int y) {
    std::vector<GalaxyFeature> result;
    for (const auto& f : state.galaxyFeatures) {
        if (f.type == GalaxyFeatureType::IonStorm) {
            const auto cells = ionStormCells(state, f);
            if (std::find(cells.begin(), cells.end(), std::pair{x, y}) != cells.end()) result.push_back(f);
        } else if (f.x == x && f.y == y) {
            result.push_back(f);
        }
    }
    return result;
}
std::vector<GalaxyFeature> knownFeatures(const GameState& state, int viewerEmpire) {
    std::vector<GalaxyFeature> result;
    for (const auto& f : state.galaxyFeatures)
        if (internal::known(state, viewerEmpire, f.x, f.y)) result.push_back(f);
    return result;
}
bool sectorPassable(const GameState& state, int x, int y) {
    if (x < 0 || y < 0 || x >= state.width || y >= state.height) return false;
    return !gameplay::featureAtCell(state, GalaxyFeatureType::BlackHole, x, y) &&
        !gameplay::featureAtCell(state, GalaxyFeatureType::SubspaceRift, x, y);
}
bool sectorHazardous(const GameState& state, int x, int y) {
    return gameplay::featureAtCell(state, GalaxyFeatureType::NeutronStar, x, y) || gameplay::insideIonStorm(state, x, y);
}
bool sectorInNebula(const GameState& state, int x, int y) {
    return gameplay::featureAtCell(state, GalaxyFeatureType::Nebula, x, y);
}
} // namespace botf
