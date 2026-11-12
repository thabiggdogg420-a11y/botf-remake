// Lane H integration scenarios (schema 14): treaty effects in the turn loop
// (shared supply, shared sensors, treaties forbidding attacks, refusing a call to
// arms), the client queries (credit forecast, ship speed, supply and task-force
// ranges and routes), per-ship orders in recorded battle rounds, the save size
// cap, schema-13 migration, and AI-vs-AI wars in long Remastered campaigns.
// See docs/GAMEPLAY-REMASTER.md section 13 and docs/DIPLOMACY.md section 4.
#include "CampaignSession.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
std::string rejection(const std::function<void()>& fn) {
    try { fn(); } catch (const std::exception& error) { return error.what(); }
    return {};
}
void rejects(const std::function<void()>& fn, const std::string& message) {
    check(!rejection(fn).empty(), message);
}
void run(const char* name, const std::function<void()>& fn, int& passed) {
    fn(); ++passed; std::cout << "PASS " << name << std::endl;
}
bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
bool hasEvent(const botf::TurnReport& r, const std::string& needle) {
    return std::any_of(r.events.begin(), r.events.end(), [&](const std::string& e) { return contains(e, needle); });
}
using botf::diplomacy::ParticipantId;
using botf::diplomacy::TreatyDuration;
using botf::diplomacy::TreatyType;

int chebyshev(int ax, int ay, int bx, int by) { return std::max(std::abs(ax - bx), std::abs(ay - by)); }
botf::EmpireState& empireOf(botf::GameState& s, int id) {
    if (id == s.playerEmpire) return s;
    for (auto& r : s.rivals) if (r.id == id) return r;
    throw std::runtime_error("missing empire");
}
botf::MajorRelation& relation(botf::GameState& s, int a, int b) {
    if (a > b) std::swap(a, b);
    for (auto& r : s.majorRelations) if (r.first == a && r.second == b) return r;
    throw std::runtime_error("missing relation");
}
void meetEveryone(botf::GameState& s) { for (auto& r : s.majorRelations) r.contact = true; }
void establish(botf::GameState& s, int a, int b, TreatyType type, TreatyDuration duration = TreatyDuration::Indefinite) {
    botf::diplomacy::TurnResolutionDelta delta;
    s.diplomacyEngineState.currentTurn = s.turn;
    check(botf::diplomacy::DiplomacyEngine::establishTreaty(s.diplomacyEngineState, ParticipantId::major(a),
        ParticipantId::major(b), type, duration, delta), "setup treaty");
}
void revealAround(botf::GameState& s, int owner, int x, int y, int radius = 1) {
    auto& known = empireOf(s, owner).explored;
    for (int dy = -radius; dy <= radius; ++dy) for (int dx = -radius; dx <= radius; ++dx) {
        const int px = x + dx, py = y + dy;
        if (px < 0 || py < 0 || px >= s.width || py >= s.height) continue;
        known.push_back(py * s.width + px);
    }
    std::sort(known.begin(), known.end());
    known.erase(std::unique(known.begin(), known.end()), known.end());
}
int addFleet(botf::GameState& s, int owner, botf::Building cls, int x, int y) {
    botf::Fleet f;
    f.id = s.nextFleetId++;
    f.ownerId = owner;
    f.x = f.targetX = x; f.y = f.targetY = y;
    f.shipClass = cls;
    f.colonyShip = cls == botf::Building::ColonyShip;
    f.hull = botf::shipMaxHull(cls);
    s.fleets.push_back(f);
    revealAround(s, owner, x, y);
    return f.id;
}
botf::Fleet& fleetRef(botf::GameState& s, int id) {
    const auto it = std::find_if(s.fleets.begin(), s.fleets.end(), [id](const botf::Fleet& f) { return f.id == id; });
    check(it != s.fleets.end(), "missing fleet " + std::to_string(id));
    return *it;
}
const botf::Fleet* findFleet(const botf::GameState& s, int id) {
    const auto it = std::find_if(s.fleets.begin(), s.fleets.end(), [id](const botf::Fleet& f) { return f.id == id; });
    return it == s.fleets.end() ? nullptr : &*it;
}
int addStation(botf::GameState& s, int owner, int x, int y, botf::StationType type = botf::StationType::Outpost) {
    botf::Station st;
    st.id = s.nextStationId++;
    st.ownerId = owner;
    st.x = x; st.y = y;
    st.type = type;
    const auto stats = botf::stationStats(s, owner, type);
    st.hull = stats.maxHull; st.shields = stats.maxShields;
    st.builtTurn = s.turn;
    s.stations.push_back(st);
    revealAround(s, owner, x, y);
    return st.id;
}
botf::System homeOf(const botf::GameState& s, int owner) {
    const auto colony = std::find_if(s.colonies.begin(), s.colonies.end(), [&](const botf::Colony& c) { return c.ownerId == owner; });
    check(colony != s.colonies.end(), "empire has no colony");
    return *std::find_if(s.systems.begin(), s.systems.end(), [&](const botf::System& x) { return x.id == colony->systemId; });
}
bool systemAt(const botf::GameState& s, int x, int y) {
    return std::any_of(s.systems.begin(), s.systems.end(), [&](const botf::System& sys) { return sys.x == x && sys.y == y; });
}
std::uint64_t nextId(const botf::GameState& s) { return s.lastAcceptedCommand + 1; }
// A modern (reference, schema-14) campaign with the AI and events quiet.
botf::GameState quietCampaign(botf::RulesetPreset preset, std::uint64_t seed, int empires = 3) {
    botf::CampaignSetup setup;
    setup.seed = seed; setup.empires = empires; setup.reference = true; setup.preset = preset;
    auto s = botf::newCampaign(setup);
    s.aiAggressionEnabled = false;
    s.eventsEnabled = false;
    s.galaxyFeatures.clear();
    meetEveryone(s);
    return s;
}
std::string withChecksum(std::string body) {
    if (const auto pos = body.rfind("CHECKSUM "); pos != std::string::npos) body.erase(pos);
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
std::string header() { return "BOTF_SIM " + std::to_string(botf::SaveVersion) + " " + botf::ContentVersion + "\n"; }
// The schema-13 text of a campaign without schema-14 content.
std::string asSchemaThirteen(const botf::GameState& state) {
    auto text = botf::serialize(state);
    check(!contains(text, "\nCOMBAT_ORDERS "), "schema-13 forgery needs a campaign without recorded orders");
    text.replace(0, text.find('\n') + 1, "BOTF_SIM 13 remaster-13\n");
    return withChecksum(text);
}
int battlesBetween(const botf::TurnReport& r, int a, int b) {
    return static_cast<int>(std::count_if(r.battles.begin(), r.battles.end(), [&](const botf::CombatReport& c) {
        return (c.firstOwner == a && c.secondOwner == b) || (c.firstOwner == b && c.secondOwner == a);
    }));
}

// ---------------------------------------------------------------------------
// 1. Treaty effects
// ---------------------------------------------------------------------------
void supplyScenarios(int& passed) {
    run("alliance: an ally's powered shipyards supply our fleets out to our own supply range", [] {
        auto s = quietCampaign(botf::RulesetPreset::Classic, 4242);
        const auto ally = homeOf(s, 2);
        const int scout = addFleet(s, 1, botf::Building::Scout, ally.x, ally.y);
        const int reach = botf::supplyRange(s, fleetRef(s, scout));
        check(reach == botf::shipRange(fleetRef(s, scout)) + s.technologies[4].level / 2, "supplyRange formula");
        // Sectors inside the ally's yard radius that our own yards do not reach.
        std::vector<std::pair<int, int>> allyOnly;
        for (int y = 0; y < s.height; ++y) for (int x = 0; x < s.width; ++x)
            if (chebyshev(x, y, ally.x, ally.y) <= reach && !botf::supplied(s, fleetRef(s, scout), x, y)) allyOnly.emplace_back(x, y);
        check(!allyOnly.empty(), "fixture: the ally's space is unsupplied for us");
        auto affiliated = s;
        establish(affiliated, 1, 2, TreatyType::Affiliation);
        for (const auto& [x, y] : allyOnly)
            check(!botf::supplied(affiliated, fleetRef(affiliated, scout), x, y), "Affiliation must not share supply");
        establish(s, 1, 2, TreatyType::Alliance);
        check(botf::alliedEmpires(s, 1) == std::vector<int>({2}), "alliance helper");
        for (const auto& [x, y] : allyOnly)
            check(botf::supplied(s, fleetRef(s, scout), x, y), "Allied yard does not supply at (" + std::to_string(x) + "," + std::to_string(y) + ")");
        // Exactly our supply range from the allied yard: one sector further is not supplied by it.
        bool edgeChecked = false;
        for (int y = 0; y < s.height; ++y) for (int x = 0; x < s.width; ++x) {
            if (chebyshev(x, y, ally.x, ally.y) != reach + 1) continue;
            // Only the allied home yard could supply here if the pre-alliance state did not.
            if (botf::supplied(affiliated, fleetRef(affiliated, scout), x, y)) continue;
            bool otherYard = false;
            for (const auto& c : s.colonies) {
                if (c.ownerId != 2 && c.ownerId != 1) continue;
                const auto sys = std::find_if(s.systems.begin(), s.systems.end(), [&](const botf::System& v) { return v.id == c.systemId; });
                if (sys->x != ally.x || sys->y != ally.y) otherYard |= chebyshev(x, y, sys->x, sys->y) <= reach;
            }
            if (otherYard) continue;
            check(!botf::supplied(s, fleetRef(s, scout), x, y), "Allied supply reaches beyond our range");
            edgeChecked = true;
        }
        check(edgeChecked, "fixture: no sector at the supply edge");
        // Orders use it: a move into the ally's space is now accepted.
        const auto target = allyOnly.front();
        botf::Command move; move.type = botf::CommandType::MoveFleet; move.fleetId = scout; move.targetX = target.first; move.targetY = target.second;
        move.id = nextId(affiliated);
        rejects([&] { botf::submit(affiliated, move); }, "Move outside supply accepted without an alliance");
        move.id = nextId(s);
        botf::submit(s, move);
        check(!botf::route(s, fleetRef(s, scout), target.first, target.second).empty(), "No route through allied supply");
    }, passed);

    run("alliance: allied stations and wormholes supplied through them extend our supply", [] {
        auto s = quietCampaign(botf::RulesetPreset::Remastered, 1999);
        const auto sol = homeOf(s, 1);
        const int scout = addFleet(s, 1, botf::Building::Transport, sol.x, sol.y); // Shortest range.
        const int range = botf::supplyRange(s, fleetRef(s, scout));
        // A far, empty entry sector outside our and the ally's yards' reach, and a far exit.
        const auto farFromYards = [&](int x, int y, int margin) {
            for (const auto& c : s.colonies) {
                if (c.ownerId != 1 && c.ownerId != 2) continue;
                const auto sys = std::find_if(s.systems.begin(), s.systems.end(), [&](const botf::System& v) { return v.id == c.systemId; });
                if (chebyshev(x, y, sys->x, sys->y) <= range + margin) return false;
            }
            return !systemAt(s, x, y);
        };
        std::pair<int, int> entry{-1, -1}, exit{-1, -1};
        for (int y = 0; y < s.height && entry.first < 0; ++y) for (int x = 0; x < s.width; ++x)
            if (farFromYards(x, y, 0)) { entry = {x, y}; break; }
        check(entry.first >= 0, "fixture: no unsupplied entry sector");
        for (int y = 0; y < s.height && exit.first < 0; ++y) for (int x = 0; x < s.width; ++x)
            if (farFromYards(x, y, 1) && chebyshev(x, y, entry.first, entry.second) >= 5) { exit = {x, y}; break; }
        check(exit.first >= 0, "fixture: no exit sector");
        addStation(s, 2, entry.first, entry.second);
        const int a = s.nextFeatureId++, b = s.nextFeatureId++;
        s.galaxyFeatures.push_back({a, botf::GalaxyFeatureType::Wormhole, entry.first, entry.second, b});
        s.galaxyFeatures.push_back({b, botf::GalaxyFeatureType::Wormhole, exit.first, exit.second, a});
        revealAround(s, 1, entry.first, entry.second, 0);
        botf::validate(s);
        const auto& f = fleetRef(s, scout);
        check(!botf::supplied(s, f, entry.first, entry.second) && !botf::supplied(s, f, exit.first, exit.second),
            "fixture: station and exit supplied before the alliance");
        establish(s, 1, 2, TreatyType::Alliance);
        const auto& g = fleetRef(s, scout);
        check(botf::supplied(s, g, entry.first, entry.second), "Allied outpost does not supply");
        check(botf::supplied(s, g, exit.first, exit.second), "Wormhole exit supplied by an allied outpost is not supplied");
    }, passed);
}

void sensorScenarios(int& passed) {
    run("affiliation: partners share sensor coverage of ships, colonies and stations", [] {
        auto s = quietCampaign(botf::RulesetPreset::Remastered, 1999);
        const auto sol = homeOf(s, 1);
        // An empire-2 outpost far from us, and an empire-3 station next to it.
        std::pair<int, int> site{-1, -1};
        for (int y = 0; y < s.height && site.first < 0; ++y) for (int x = 0; x < s.width; ++x)
            if (chebyshev(x, y, sol.x, sol.y) >= 6 && !systemAt(s, x, y) && !botf::visible(s, 1, x, y) &&
                x + 2 < s.width && !systemAt(s, x + 2, y) && !botf::visible(s, 1, x + 2, y)) { site = {x, y}; break; }
        check(site.first >= 0, "fixture: no unseen site");
        addStation(s, 2, site.first, site.second);
        const int foreign = addStation(s, 3, site.first + 2, site.second);
        check(!botf::visible(s, 1, site.first + 2, site.second), "fixture: seen before any treaty");
        auto pact = s;
        establish(pact, 1, 2, TreatyType::Friendship);
        check(!botf::visible(pact, 1, site.first + 2, site.second), "Friendship must not share sensors");
        establish(s, 1, 2, TreatyType::Affiliation);
        check(botf::visible(s, 1, site.first + 2, site.second), "Partner outpost sensors (radius 2) not shared");
        const auto stations = botf::visibleStations(s, 1);
        check(std::any_of(stations.begin(), stations.end(), [&](const botf::Station& st) { return st.id == foreign; }),
            "Station seen by a partner missing from visibleStations");
        // Ships and colonies of the partner see too.
        const auto theirs = homeOf(s, 2);
        check(botf::visible(s, 1, theirs.x, theirs.y), "Partner colony coverage not shared");
        check(botf::sensorSharingEmpires(s, 2) == std::vector<int>({1}), "Sharing is mutual");
    }, passed);
}

void treatyGateScenarios(int& passed) {
    run("treaties: no battle, pursuit, invasion or boarding while a pact is in force", [] {
        auto base = quietCampaign(botf::RulesetPreset::Remastered, 26713, 2);
        const auto sol = homeOf(base, 1);
        std::pair<int, int> site{-1, -1};
        for (int y = 0; y < base.height && site.first < 0; ++y) for (int x = 0; x < base.width; ++x)
            if (chebyshev(x, y, sol.x, sol.y) == 2 && !systemAt(base, x, y)) { site = {x, y}; break; }
        const int stationId = addStation(base, 2, site.first, site.second);
        base.stations.back().hull = botf::stationStats(base, base.stations.back()).maxHull / 2;
        const int ours = addFleet(base, 1, botf::Building::Destroyer, site.first, site.second);
        const int boarder = addFleet(base, 1, botf::Building::Transport, site.first, site.second);
        const int theirs = addFleet(base, 2, botf::Building::Destroyer, site.first, site.second);
        (void)ours; (void)theirs;
        relation(base, 1, 2).atWar = true;
        relation(base, 1, 2).warStartTurn = base.turn;
        botf::validate(base);
        check(botf::combatAllowed(base, 1, 2), "at war without a treaty");

        auto bound = base;
        establish(bound, 1, 2, TreatyType::NonAggression, TreatyDuration::Turns50);
        check(!botf::combatAllowed(bound, 1, 2) && botf::treatyForbidsAttack(bound, 1, 2), "combatAllowed ignores the pact");
        // Player orders are refused while the pact holds.
        botf::Command attack; attack.type = botf::CommandType::AttackFleet; attack.fleetId = ours; attack.targetFleetId = theirs;
        attack.id = nextId(bound);
        check(contains(rejection([&] { botf::submit(bound, attack); }), "treaty"), "Attack order accepted under a pact");
        const auto report = botf::advance(bound);
        check(battlesBetween(report, 1, 2) == 0, "Battle fought under a non-aggression pact");
        const auto* kept = botf::stationById(bound, stationId);
        check(kept && kept->ownerId == 2 && findFleet(bound, boarder), "Station boarded under a pact");

        // Control: the same state without the pact fights and boards.
        auto war = base;
        const auto fought = botf::advance(war);
        check(battlesBetween(fought, 1, 2) >= 1, "Control battle missing");
    }, passed);

    run("treaties: war must be declared first, and a declaration breaks the pact", [] {
        auto s = quietCampaign(botf::RulesetPreset::Classic, 77, 2);
        const auto sol = homeOf(s, 1);
        const int ours = addFleet(s, 1, botf::Building::Destroyer, sol.x, sol.y);
        const int theirs = addFleet(s, 2, botf::Building::Destroyer, sol.x, sol.y);
        (void)ours; (void)theirs;
        establish(s, 1, 2, TreatyType::NonAggression, TreatyDuration::Turns50);
        auto peaceful = s;
        check(battlesBetween(botf::advance(peaceful), 1, 2) == 0, "Battle without war");
        botf::Command war; war.id = nextId(s); war.type = botf::CommandType::DeclareWar; war.targetEmpire = 2;
        botf::submit(s, war);
        const auto report = botf::advance(s);
        check(!botf::treatyForbidsAttack(s, 1, 2) && botf::combatAllowed(s, 1, 2), "Declaration did not break the pact");
        check(battlesBetween(report, 1, 2) >= 1, "No battle after the declaration");
    }, passed);

    run("treaties: rival AI neither targets nor invades an empire it has a pact with", [] {
        auto base = quietCampaign(botf::RulesetPreset::Classic, 90210, 2);
        const auto rivalHome = homeOf(base, 2), sol = homeOf(base, 1);
        // A rival warship and transport next to our home, which is a colony at war.
        int near = -1;
        for (int y = 0; y < base.height && near < 0; ++y) for (int x = 0; x < base.width; ++x)
            if (chebyshev(x, y, sol.x, sol.y) == 1 && !systemAt(base, x, y)) { near = y * base.width + x; break; }
        const int raider = addFleet(base, 2, botf::Building::Destroyer, near % base.width, near / base.width);
        const int troops = addFleet(base, 2, botf::Building::Transport, sol.x, sol.y);
        const int bait = addFleet(base, 1, botf::Building::Scout, sol.x, sol.y);
        (void)rivalHome;
        relation(base, 1, 2).atWar = true;
        relation(base, 1, 2).warStartTurn = base.turn;
        auto bound = base;
        establish(bound, 1, 2, TreatyType::NonAggression, TreatyDuration::Turns50);
        const auto report = botf::advance(bound);
        const auto* r = findFleet(bound, raider);
        check(!r || r->attackTarget != bait, "Rival AI targets a ship it has a pact with");
        check(report.invasions.empty() && botf::colonyAt(bound, sol.id)->ownerId == 1, "Rival AI invaded under a pact");
        check(battlesBetween(report, 1, 2) == 0, "Battle under a pact");
        (void)troops;
        // Control: without the pact the rival engages.
        auto war = base;
        const auto fought = botf::advance(war);
        const auto* w = findFleet(war, raider);
        check(battlesBetween(fought, 1, 2) >= 1 || (w && w->attackTarget == bait) || !fought.invasions.empty(),
            "Control: the rival AI ignores an enemy at war");
    }, passed);
}

void callToArmsScenarios(int& passed) {
    run("calls to arms: RefuseCallToArms applies the refusal at once with a clear event", [] {
        auto p = botf::generateReferenceCampaign(82, 3);
        p.eventsEnabled = false;
        p.aiAggressionEnabled = false;
        meetEveryone(p);
        p.turn = 30; p.diplomacyEngineState.currentTurn = 30;
        for (auto& r : p.majorRelations) r.trust = 50;
        establish(p, 1, 2, TreatyType::Alliance);
        relation(p, 2, 3).atWar = true;
        relation(p, 2, 3).warStartTurn = 30;
        p.diplomacyEngineState.defenseCalls.push_back({1, 2, 3, 30, 33}); // Due in three turns.
        botf::validate(p);
        check(botf::openCallsToArms(p, 1).size() == 1, "open call");
        // Shape and existence are checked at submission.
        rejects([&] { botf::submit(p, botf::makeRefuseCallToArmsCommand(nextId(p), 2, 2)); }, "Defender == aggressor accepted");
        rejects([&] { botf::submit(p, botf::makeRefuseCallToArmsCommand(nextId(p), 3, 2)); }, "Refusing a call that does not exist accepted");
        auto legacy = botf::generateCampaign(82, 3);
        rejects([&] { botf::submit(legacy, botf::makeRefuseCallToArmsCommand(nextId(legacy), 2, 3)); }, "Legacy campaign accepted a refusal");
        botf::submit(p, botf::makeRefuseCallToArmsCommand(nextId(p), 2, 3));
        // The pending order survives a save.
        const auto text = botf::serialize(p);
        auto loaded = botf::deserialize(text);
        check(botf::serialize(loaded) == text && loaded.pending.back().callAggressor == 3, "Refusal order lost in a save");
        const int trustBefore = relation(p, 1, 2).trust;
        const auto report = botf::advance(p); // Turn 31, two turns before the call would fall due.
        check(botf::openCallsToArms(p, 1).empty(), "Call still open after the refusal");
        check(relation(p, 1, 2).trust <= trustBefore - 20 + 2, "Refusal did not cost trust at once");
        check(hasEvent(report, "We refused Klingon Empire's call to arms against Romulan Star Empire"), "Refusal not reported clearly");
        check(!relation(p, 1, 3).atWar, "Refusing must not declare war");
        check(botf::alliedEmpires(p, 1).empty(), "An AI defender left with trust below 40 keeps the alliance");
        bool logged = false;
        for (const auto& e : p.diplomacyEngineState.eventHistory)
            logged |= e.type == botf::diplomacy::DiplomaticEventType::AllianceRefused && e.turn == 31;
        check(logged, "Refusal missing from the diplomatic log");
        botf::advance(loaded);
        check(botf::fingerprint(loaded) == botf::fingerprint(p), "Reloaded refusal resolved differently");
    }, passed);

    run("calls to arms: a trusted ally keeps the alliance after one refusal", [] {
        auto p = botf::generateReferenceCampaign(83, 3);
        p.eventsEnabled = false;
        p.aiAggressionEnabled = false;
        meetEveryone(p);
        p.turn = 30; p.diplomacyEngineState.currentTurn = 30;
        for (auto& r : p.majorRelations) r.trust = 50;
        relation(p, 1, 2).trust = 90;
        establish(p, 1, 2, TreatyType::Alliance);
        relation(p, 2, 3).atWar = true;
        relation(p, 2, 3).warStartTurn = 30;
        p.diplomacyEngineState.defenseCalls.push_back({1, 2, 3, 30, 33});
        botf::submit(p, botf::makeRefuseCallToArmsCommand(nextId(p), 2, 3));
        botf::advance(p);
        check(botf::alliedEmpires(p, 1) == std::vector<int>({2}), "A trusted alliance ended after one refusal");
        check(relation(p, 1, 2).trust < 90 - 15, "Trust unchanged");
    }, passed);
}

// ---------------------------------------------------------------------------
// 2. Client queries
// ---------------------------------------------------------------------------
struct ForecastTally { int turns = 0, exactTotals = 0, treatyTurns = 0; };

// Advances `s` for `turns` turns (a scripted player buys and gifts now and then) and
// checks every forecast line against the ledger advance() records.
void checkForecasts(botf::GameState s, int turns, ForecastTally& tally, const std::string& label) {
    std::uint64_t rng = s.seed;
    for (int t = 0; t < turns && s.outcome == botf::CampaignOutcome::Ongoing; ++t) {
        rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
        // Exercise order costs: rush a build, gift a minor.
        for (const auto& c : s.colonies) {
            if (c.ownerId != s.playerEmpire || c.queue.empty() || (rng >> 33) % 5 != 0) continue;
            const int cost = botf::rushCost(s, c.systemId);
            if (cost > 0 && cost < s.credits) {
                try { botf::submit(s, botf::makeBuyConstructionCommand(nextId(s), c.systemId)); } catch (const std::exception&) {}
            }
            break;
        }
        const auto before = s;
        const auto forecast = botf::creditForecast(s, s.playerEmpire);
        for (const auto& rival : s.rivals) (void)botf::creditForecast(s, rival.id);
        const auto report = botf::advance(s);
        const auto& l = report.ledger;
        const std::string where = label + " turn " + std::to_string(report.turn);
        check(l.treasury == report.creditsBefore && l.nextTreasury == report.creditsAfter, where + ": ledger endpoints");
        check(l.fleetUpkeep + l.stationUpkeep == report.fleetUpkeep, where + ": ledger military upkeep");
        check(l.tradeRouteIncome + l.treatyDividends == report.tradeIncome, where + ": ledger trade income");
        check(forecast.treasury == l.treasury && forecast.orders == l.orders, where + ": orders");
        check(forecast.colonyIncome == l.colonyIncome && forecast.buildingUpkeep == l.buildingUpkeep, where + ": colonies");
        check(forecast.fleetUpkeep == l.fleetUpkeep, where + ": fleet upkeep " + std::to_string(forecast.fleetUpkeep) + " vs " + std::to_string(l.fleetUpkeep));
        check(forecast.stationUpkeep == l.stationUpkeep, where + ": station upkeep");
        check(forecast.net == forecast.income - forecast.upkeep && forecast.other == 0, where + ": forecast totals");
        // Trade, treaties and events are forecast from today's treaties and wars; a
        // turn that changes them (or an AI declaration) may differ.
        bool diplomacyChanged = false;
        for (const auto& r : s.majorRelations) {
            if (r.first != s.playerEmpire && r.second != s.playerEmpire) continue;
            const auto* old = botf::majorRelation(before, r.first, r.second);
            diplomacyChanged |= !old || old->atWar != r.atWar || old->tradeTreaty != r.tradeTreaty;
        }
        const auto treaties = [](const botf::GameState& g) {
            std::vector<std::tuple<int, int, int>> list;
            for (const auto& tr : g.diplomacyEngineState.activeTreaties)
                if (tr.status == botf::diplomacy::TreatyStatus::Active && !tr.partyA.isMinor && !tr.partyB.isMinor)
                    list.emplace_back(tr.partyA.id, tr.partyB.id, static_cast<int>(tr.type));
            std::sort(list.begin(), list.end());
            return list;
        };
        diplomacyChanged |= treaties(before) != treaties(s);
        if (diplomacyChanged) { ++tally.treatyTurns; continue; }
        check(forecast.tradeRouteIncome == l.tradeRouteIncome, where + ": trade routes");
        check(forecast.treatyDividends == l.treatyDividends, where + ": treaty dividends " + std::to_string(forecast.treatyDividends) +
            " vs " + std::to_string(l.treatyDividends));
        const bool laborUnrest = hasEvent(report, "demands met");
        if (!laborUnrest) check(forecast.eventIncome == l.eventIncome && forecast.eventCosts == l.eventCosts,
            where + ": events " + std::to_string(forecast.eventIncome) + "/" + std::to_string(forecast.eventCosts) + " vs " +
            std::to_string(l.eventIncome) + "/" + std::to_string(l.eventCosts));
        ++tally.turns;
        if (l.other == 0 && !laborUnrest) {
            check(forecast.nextTreasury == report.creditsAfter, where + ": next treasury " + std::to_string(forecast.nextTreasury) +
                " vs " + std::to_string(report.creditsAfter));
            ++tally.exactTotals;
        }
    }
}

void forecastScenarios(int& passed) {
    run("creditForecast equals what advance() charges across varied campaigns", [] {
        ForecastTally tally;
        struct Setup { std::uint64_t seed; int player; botf::StartingEra era; bool reference; botf::RulesetPreset preset; };
        const std::vector<Setup> setups{
            {26713, 4, botf::StartingEra::Early, true, botf::RulesetPreset::Remastered},
            {77, 3, botf::StartingEra::Developed, true, botf::RulesetPreset::Remastered},
            {4242, 1, botf::StartingEra::Advanced, false, botf::RulesetPreset::Remastered},
            {1999, 2, botf::StartingEra::Early, true, botf::RulesetPreset::Classic},
            {31337, 5, botf::StartingEra::Supreme, true, botf::RulesetPreset::Remastered},
        };
        for (const auto& c : setups) {
            botf::CampaignSetup setup;
            setup.seed = c.seed; setup.empires = 5; setup.playerEmpire = c.player; setup.era = c.era;
            setup.reference = c.reference; setup.preset = c.preset;
            checkForecasts(botf::newCampaign(setup), 120, tally, "seed " + std::to_string(c.seed));
        }
        checkForecasts(botf::generateReferenceCampaign(5150, 4), 60, tally, "schema-12 style reference");
        checkForecasts(botf::generateCampaign(8086, 3), 60, tally, "prototype legacy");
        check(tally.turns > 400 && tally.exactTotals * 10 >= tally.turns * 9,
            "Too few exact forecasts: " + std::to_string(tally.exactTotals) + " of " + std::to_string(tally.turns));
        std::cout << "  forecasts: " << tally.turns << " comparable turns, " << tally.exactTotals << " exact next-turn treasuries, "
                  << tally.treatyTurns << " turns with treaty or war changes" << std::endl;
    }, passed);

    run("creditForecast: pending orders, treaty dividends and a trade boom are itemised", [] {
        auto s = quietCampaign(botf::RulesetPreset::Remastered, 1999);
        establish(s, 1, 2, TreatyType::Alliance);
        establish(s, 1, 3, TreatyType::Friendship);
        botf::GalacticEvent boom;
        boom.id = s.nextEventId++;
        boom.kind = botf::GalacticEventKind::TradeBoom;
        boom.empireId = 1;
        boom.startTurn = s.turn; boom.endTurn = s.turn + 5; boom.magnitude = 10;
        s.galacticEvents.push_back(boom);
        botf::validate(s);
        const auto home = homeOf(s, 1);
        if (botf::colonyAt(s, home.id)->queue.empty()) {
            botf::Command build; build.id = nextId(s); build.type = botf::CommandType::EnqueueBuilding;
            build.systemId = home.id; build.building = botf::Building::Farm;
            botf::submit(s, build);
        }
        s.credits = 100000;
        auto costed = s;
        botf::submit(costed, botf::makeBuyConstructionCommand(nextId(costed), home.id));
        const int rush = botf::rushCost(botf::preview(s), home.id);
        check(rush > 0, "fixture: nothing to rush");
        s = costed;
        const auto f = botf::creditForecast(s, 1);
        check(f.orders == -rush, "Rush purchase not itemised as an order");
        check(f.treatyDividends == 8 + 3, "Alliance (8) and friendship (3) dividends");
        int colonies = 0;
        for (const auto& c : s.colonies) colonies += c.ownerId == 1;
        check(f.eventIncome == std::min(150, 10 * colonies), "Trade boom income");
        check(f.nextTreasury == f.treasury + f.orders + f.net, "Next treasury");
        const auto report = botf::advance(s);
        check(report.ledger.treatyDividends == f.treatyDividends && report.ledger.eventIncome == f.eventIncome &&
              report.ledger.orders == f.orders && report.creditsAfter == f.nextTreasury, "Ledger differs from the forecast");
        check(botf::fleetUpkeep(s.fleets.front()) > 0, "fleetUpkeep");
    }, passed);
}

void movementScenarios(int& passed) {
    run("shipSpeed and taskForceRoute predict movement; the route is empty exactly when orders are refused", [] {
        auto s = quietCampaign(botf::RulesetPreset::Remastered, 26713, 2);
        const auto sol = homeOf(s, 1);
        // A task force of two player ships with different reference speeds and ranges.
        std::vector<int> mine;
        for (const auto& f : s.fleets) if (f.ownerId == 1 && f.x == sol.x && f.y == sol.y && f.hull > 0) mine.push_back(f.id);
        const int extra = addFleet(s, 1, botf::Building::Destroyer, sol.x, sol.y);
        mine.push_back(extra);
        for (int id : mine) {
            const auto& f = fleetRef(s, id);
            const int expected = f.referenceShipId >= 0 ? std::clamp(static_cast<int>(
                botf::referenceData::ShipCatalogue[static_cast<std::size_t>(f.referenceShipId)].speed), 1, 3) : 1;
            check(botf::shipSpeed(s, f) == expected, "shipSpeed of a free ship");
        }
        botf::submit(s, botf::makeCreateTaskForceCommand(nextId(s), mine, "Probe"));
        botf::advance(s);
        const auto forces = botf::empireTaskForces(s, 1);
        check(!forces.empty(), "task force");
        const int tf = forces.front().id;
        int minRange = 1000;
        for (const auto& m : botf::taskForceFleets(s, tf)) {
            minRange = std::min(minRange, botf::supplyRange(s, m));
            check(botf::shipSpeed(s, m) == botf::taskForceSpeed(s, tf), "Members move at the task force pace");
        }
        check(botf::taskForceRange(s, tf) == minRange, "taskForceRange is the shortest member range");
        check(botf::taskForceRange(s, 99999) == 0 && botf::taskForceRoute(s, 99999, 0, 0).empty(), "Unknown task force");
        int accepted = 0, refused = 0;
        std::pair<int, int> far{-1, -1};
        int farthest = 0;
        for (int y = 0; y < s.height; ++y) for (int x = 0; x < s.width; ++x) {
            const auto path = botf::taskForceRoute(s, tf, x, y);
            auto draft = s;
            const bool ok = rejection([&] {
                botf::submit(draft, botf::makeTaskForceOrderCommand(nextId(draft), tf, x, y));
            }).empty();
            check(ok == !path.empty(), "taskForceRoute disagrees with validation at (" + std::to_string(x) + "," + std::to_string(y) + ")");
            (ok ? accepted : refused)++;
            if (ok) {
                check(path.front() == std::make_pair(sol.x, sol.y) && path.back() == std::make_pair(x, y), "Route endpoints");
                if (static_cast<int>(path.size()) > farthest) { farthest = static_cast<int>(path.size()); far = {x, y}; }
            }
        }
        check(accepted > 1 && refused > 0, "fixture: both reachable and unreachable sectors");
        const auto path = botf::taskForceRoute(s, tf, far.first, far.second);
        botf::submit(s, botf::makeTaskForceOrderCommand(nextId(s), tf, far.first, far.second));
        const int speed = botf::taskForceSpeed(s, tf);
        botf::advance(s);
        const auto step = path[static_cast<std::size_t>(std::min<int>(speed, static_cast<int>(path.size()) - 1))];
        for (const auto& m : botf::taskForceFleets(s, tf))
            check(m.x == step.first && m.y == step.second, "Task force did not follow taskForceRoute at shipSpeed");
    }, passed);
}

// ---------------------------------------------------------------------------
// 3. Recorded round orders, saves and the size cap
// ---------------------------------------------------------------------------
void combatRecordScenarios(int& passed) {
    run("battle records: every round lists each active ship's order and flags", [] {
        using namespace botf::tactical;
        Encounter e;
        e.controllingOwnerId = 1;
        e.rngState = 7;
        e.strategicTurn = 1;
        auto ship = [](int id, int owner, int hull, int beam, int x) {
            Ship s;
            s.fleetId = id; s.ownerId = owner; s.name = "Ship"; s.shipClass = botf::Building::Destroyer;
            s.hull = s.maxHull = s.initialHull = hull; s.beamAttack = beam; s.speed = 2; s.position = {x, 5};
            return s;
        };
        e.ships.push_back(ship(1, 1, 150, 10, 4));   // Rams.
        auto anchored = ship(2, 2, 300, 5, 5);       // AI-controlled ram target that holds its cell.
        anchored.immobile = true;
        e.ships.push_back(anchored);
        auto cloaked = ship(3, 2, 80, 30, 9);
        cloaked.canCloak = cloaked.isCloaked = true; // AI opens from the cloak: an ambush.
        e.ships.push_back(cloaked);
        auto runner = ship(4, 1, 60, 5, 2);
        e.ships.push_back(runner);
        e.submitOrder({1, 2, TacticalManeuver::Ram});
        e.submitOrder({4, 0, TacticalManeuver::Retreat});
        e.resolveRound();
        const auto report = toCombatReport(e);
        check(report.rounds.size() == 1, "one round");
        const auto& orders = report.rounds.front().orders;
        check(orders.size() == 4, "every active ship has an order");
        const auto find = [&](int id) {
            const auto it = std::find_if(orders.begin(), orders.end(), [id](const botf::CombatShipOrder& o) { return o.fleetId == id; });
            check(it != orders.end(), "missing order");
            return *it;
        };
        const auto ram = find(1), target = find(2), ambush = find(3), run = find(4);
        check(ram.maneuver == static_cast<int>(TacticalManeuver::Ram) && ram.targetFleetId == 2 && ram.rammed && !ram.aiControlled,
            "Ram order (maneuver " + std::to_string(ram.maneuver) + ", rammed " + std::to_string(ram.rammed) + ")");
        check(target.aiControlled && !target.rammed, "AI flag");
        check(ambush.aiControlled && ambush.decloaked, "Decloak ambush flag");
        check(run.maneuver == static_cast<int>(TacticalManeuver::Retreat) && run.disengaging, "Disengaging flag");
        // A hail round.
        Encounter h;
        h.controllingOwnerId = 1;
        h.ships.push_back(ship(1, 1, 100, 5, 3));
        h.ships.push_back(ship(2, 2, 100, 5, 9));
        h.submitOrder({1, 0, TacticalManeuver::Hail});
        h.standingOrders[2] = Order{2, 0, TacticalManeuver::Hail};
        h.resolveRound();
        const auto truce = toCombatReport(h);
        check(truce.rounds.front().orders.size() == 2 && truce.rounds.front().orders.front().hailed, "Hail flag");
    }, passed);

    run("battle records: unified-resolver campaign battles keep orders through saves; schema 13 migrates", [] {
        botf::CampaignSetup setup;
        setup.seed = 8086; setup.empires = 5; setup.playerEmpire = 1;
        auto s = botf::newCampaign(setup);
        // The schema-13 forgeries below need a campaign without schema-15 content: switch the empire rules off.
        s.rules.empireRules = false;
        s.rules.interceptOrders = s.rules.raidOrders = s.rules.crewExperience = false; // pre-schema-23 text for the forgeries
        s.minorGuards.clear();
        for (auto& c : s.colonies) { c.energy -= 5 * c.refineries; c.refineries = 0; }
        bool withOrders = false;
        for (int t = 0; t < 300 && !withOrders && s.outcome == botf::CampaignOutcome::Ongoing; ++t) {
            botf::advance(s);
            for (const auto& b : s.latestBattles)
                for (const auto& r : b.rounds) withOrders |= !r.orders.empty();
        }
        check(withOrders, "No recorded battle carries orders");
        const auto text = botf::serialize(s);
        check(text.starts_with(header()) && contains(text, "\nCOMBAT_ORDERS "), "Schema 14 section missing");
        const auto loaded = botf::deserialize(text);
        check(botf::serialize(loaded) == text, "Orders did not round-trip");
        for (std::size_t i = 0; i < s.latestBattles.size(); ++i)
            for (std::size_t r = 0; r < s.latestBattles[i].rounds.size(); ++r)
                check(loaded.latestBattles[i].rounds[r].orders == s.latestBattles[i].rounds[r].orders, "Orders changed in the save");
        // Tampering is rejected: an unknown maneuver, and an order for a ship not in the battle.
        const auto firstOrder = [](botf::GameState& g) -> botf::CombatShipOrder& {
            for (auto& b : g.latestBattles) for (auto& r : b.rounds) if (!r.orders.empty()) return r.orders.front();
            throw std::runtime_error("no recorded order");
        };
        auto bad = s;
        firstOrder(bad).maneuver = 12;
        rejects([&] { botf::validate(bad); }, "Unknown maneuver accepted");
        auto stranger = s;
        firstOrder(stranger).fleetId = 999999;
        rejects([&] { botf::validate(stranger); }, "Order for a non-participant accepted");
        auto twice = s;
        for (auto& b : twice.latestBattles) for (auto& r : b.rounds) if (!r.orders.empty()) r.orders.push_back(r.orders.front());
        rejects([&] { botf::validate(twice); }, "Two orders for one ship in a round accepted");
        // A schema-13 save (no orders) loads and re-saves as schema 14.
        auto plain = s;
        for (auto& b : plain.latestBattles) for (auto& r : b.rounds) { r.orders.clear(); r.events.clear(); }
        const auto old = asSchemaThirteen(plain);
        const auto migrated = botf::deserialize(old);
        check(botf::serialize(migrated) == botf::serialize(plain), "Schema 13 did not migrate losslessly");
        auto a = migrated, b = plain;
        for (int t = 0; t < 5 && a.outcome == botf::CampaignOutcome::Ongoing; ++t) { botf::advance(a); botf::advance(b); }
        check(botf::fingerprint(a) == botf::fingerprint(b), "Migrated schema-13 campaign diverged");
        // Schema 14 content in a schema-13 header is rejected.
        auto smuggled = text;
        smuggled.replace(0, smuggled.find('\n') + 1, "BOTF_SIM 13 remaster-13\n");
        rejects([&] { botf::deserialize(withChecksum(smuggled)); }, "Schema 13 save with COMBAT_ORDERS accepted");
        // A schema-14 save must not carry an empty COMBAT_ORDERS section (canonical form).
        auto empty = botf::serialize(plain);
        empty.insert(empty.rfind("CHECKSUM "), "COMBAT_ORDERS " + std::to_string(plain.latestBattles.size()) + "\n");
        rejects([&] { botf::deserialize(withChecksum(empty)); }, "Non-canonical empty COMBAT_ORDERS accepted");
    }, passed);

    run("saves: schema 13 rejects the RefuseCallToArms order; the size cap is MaxSaveBytes", [] {
        auto p = botf::generateReferenceCampaign(84, 3);
        p.aiAggressionEnabled = false;
        meetEveryone(p);
        establish(p, 1, 2, TreatyType::Alliance);
        relation(p, 2, 3).atWar = true;
        p.diplomacyEngineState.defenseCalls.push_back({1, 2, 3, p.turn, p.turn + 3});
        botf::submit(p, botf::makeRefuseCallToArmsCommand(nextId(p), 2, 3));
        auto old = botf::serialize(p);
        old.replace(0, old.find('\n') + 1, "BOTF_SIM 13 remaster-13\n");
        rejects([&] { botf::deserialize(withChecksum(old)); }, "Schema 13 save with a schema-14 order accepted");
        check(botf::MaxSaveBytes == 8u * 1024u * 1024u, "cap");
        const auto tooLarge = rejection([] { botf::deserialize(std::string(botf::MaxSaveBytes + 1, 'x')); });
        check(contains(tooLarge, "too large"), "Oversized save not rejected for its size");
        const auto atCap = rejection([] { botf::deserialize(std::string(botf::MaxSaveBytes, 'x')); });
        check(!atCap.empty() && !contains(atCap, "too large"), "A save at the cap was rejected for its size");
        const auto oldCap = rejection([] { botf::deserialize(std::string(1024 * 1024 + 1, 'x')); });
        check(!contains(oldCap, "too large"), "The old 1 MiB cap still applies");
        // A session embeds the campaign through the same function and cap.
        botf::session::TurnSession session;
        botf::session::begin(session, p);
        const auto envelope = botf::session::serialize(session);
        check(botf::serialize(botf::session::deserialize(envelope).baseline) == botf::serialize(p), "Session round trip");
        const std::string bloated = "BOTF_TURN_SESSION 1\n" + std::to_string(botf::MaxSaveBytes + 1) + "\n" +
            std::string(botf::MaxSaveBytes + 1, 'x') + "\n0\n0\n";
        check(contains(rejection([&] { botf::session::deserialize(bloated); }), "too large"), "Session accepted an oversized campaign");
    }, passed);
}

// ---------------------------------------------------------------------------
// 4. AI-vs-AI wars in a long Remastered campaign
// ---------------------------------------------------------------------------
void warScenarios(int& passed) {
    run("AI empires go to war with each other, fight and respect their pacts (300-turn Remastered)", [] {
        int aiWars = 0, aiBattles = 0, pactBattles = 0, campaigns = 0;
        for (std::uint64_t seed : {26713ULL, 4242ULL}) {
            botf::CampaignSetup setup;
            setup.seed = seed; setup.empires = 5; setup.playerEmpire = 3;
            auto s = botf::newCampaign(setup);
            ++campaigns;
            for (int t = 0; t < 300 && s.outcome == botf::CampaignOutcome::Ongoing; ++t) {
                const auto before = s;
                const auto report = botf::advance(s);
                for (const auto& r : s.majorRelations) {
                    const auto* old = botf::majorRelation(before, r.first, r.second);
                    if (r.atWar && old && !old->atWar && r.first != 3 && r.second != 3) ++aiWars;
                }
                for (const auto& b : report.battles) {
                    if (b.firstOwner != 3 && b.secondOwner != 3) ++aiBattles;
                    if (botf::treatyForbidsAttack(before, b.firstOwner, b.secondOwner)) ++pactBattles;
                }
            }
        }
        std::cout << "  " << campaigns << " campaigns: " << aiWars << " AI-vs-AI war declarations, " << aiBattles << " AI-vs-AI battles" << std::endl;
        check(aiWars >= 2 && aiBattles >= 2, "AI empires never fight each other");
        check(pactBattles == 0, "A battle was fought under a pact");
    }, passed);
}
} // namespace

int main() {
    int passed = 0;
    try {
        supplyScenarios(passed);
        sensorScenarios(passed);
        treatyGateScenarios(passed);
        callToArmsScenarios(passed);
        forecastScenarios(passed);
        movementScenarios(passed);
        combatRecordScenarios(passed);
        warScenarios(passed);
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << passed << " scenarios: " << error.what() << '\n';
        return 1;
    }
    std::cout << "ALL " << passed << " INTEGRATION SCENARIOS PASSED\n";
    return 0;
}
