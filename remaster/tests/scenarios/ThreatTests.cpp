// Threat-actor scenarios (lane MOD-5, schema 20; docs/design/THREAT-ACTORS.md): spawn rules and switches, movement, targeting,
// combat through the tactical engine, despawn, Borg assimilation and morale, the THREATS save section and its migration,
// a player battle through a turn session, and 100-turn AI soaks with every actor on.
#include "CampaignSession.h"
#include "Simulation.h"
#include "Threats.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace botf;
using threats::def;
int failed = 0;
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void rejects(const std::function<void()>& fn, const std::string& message, const std::string& fragment = "") {
    try { fn(); } catch (const std::exception& e) {
        check(fragment.empty() || std::string(e.what()).find(fragment) != std::string::npos,
            message + ": message '" + e.what() + "' lacks '" + fragment + "'");
        return;
    }
    throw std::runtime_error(message);
}
void run(const char* name, const std::function<void()>& fn, int& passed) {
    try { fn(); ++passed; std::cout << "PASS " << name << std::endl; }
    catch (const std::exception& e) { ++failed; std::cout << "FAIL " << name << ": " << e.what() << std::endl; }
}
bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
std::string withChecksum(std::string body) {
    if (const auto pos = body.rfind("CHECKSUM "); pos != std::string::npos) body.erase(pos);
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
std::string headerOf(int version) { return "BOTF_SIM " + std::to_string(version) + " remaster-" + std::to_string(version) + "\n"; }
std::string withHeader(std::string text, int version) { text.replace(0, text.find('\n') + 1, headerOf(version)); return withChecksum(text); }

ThreatSetup everything(int frequency = 2) {
    ThreatSetup t;
    t.monsters = true; t.borg = true; t.frequency = frequency;
    return t;
}
GameState game(std::uint64_t seed, const ThreatSetup& t, const std::vector<std::string>& lineup = {}, int player = 1) {
    CampaignSetup setup;
    setup.seed = seed; setup.threats = t; setup.lineup = lineup; setup.playerEmpire = player;
    if (lineup.size() > 5) { setup.width = 22; setup.height = 15; setup.systems = 48; }
    return newCampaign(setup);
}
ThreatActor& inject(GameState& s, ThreatKind kind, int x, int y) {
    ThreatActor a;
    a.id = s.nextThreatId++;
    a.kind = static_cast<int>(kind);
    a.x = a.targetX = x; a.y = a.targetY = y;
    a.spawnTurn = s.turn;
    a.hull = threats::actorMaxHull(a);
    a.shields = threats::actorMaxShields(a);
    s.threatActors.push_back(a);
    return s.threatActors.back();
}
// By value: advance() replaces the state, so a reference into `systems` would dangle across a turn.
System home(const GameState& s) { return s.systems.front(); }
System systemOf(const GameState& s, int id) {
    for (const auto& x : s.systems) if (x.id == id) return x;
    throw std::runtime_error("no such system");
}
// No actor but the ones a test injects: every kind is switched off for spawning, the actors that exist keep moving and fighting.
void quiet(GameState& s) {
    s.threatActors.clear();
    s.nextThreatId = 1;
    s.threats.disabledMask = (1 << ThreatKinds) - 1;
}
// Keeps `n` copies of the player's first fleet (at home) and drops every other fleet.
void keepShips(GameState& s, int n) {
    Fleet proto;
    for (const auto& f : s.fleets) if (f.ownerId == s.playerEmpire && f.x == home(s).x && f.y == home(s).y && !f.colonyShip) { proto = f; break; }
    s.fleets.clear(); s.taskForces.clear();
    for (int i = 0; i < n; ++i) { Fleet f = proto; f.id = s.nextFleetId++; f.taskForceId = 0; s.fleets.push_back(f); }
}
// Colonies of the player at other systems (copies of the home colony), so the capital is not the last world.
void addColonies(GameState& s, int n) {
    for (const auto& sys : s.systems) {
        if (n == 0) break;
        if (colonyAt(s, sys.id) || minorAt(s, sys.id)) continue;
        Colony c = s.colonies.front();
        c.systemId = sys.id; c.queue.clear(); c.overflow = 0; c.shipyards = 0;
        s.colonies.push_back(c);
        const int cell = sys.y * s.width + sys.x;
        if (std::find(s.explored.begin(), s.explored.end(), cell) == s.explored.end()) { s.explored.push_back(cell); std::sort(s.explored.begin(), s.explored.end()); }
        --n;
    }
    std::sort(s.colonies.begin(), s.colonies.end(), [](const Colony& a, const Colony& b) { return a.systemId < b.systemId; });
}
int outerColony(const GameState& s) {
    for (const auto& c : s.colonies) if (c.systemId != home(s).id) return c.systemId;
    return 0;
}
int count(const GameState& s, ThreatKind k) {
    return static_cast<int>(std::count_if(s.threatActors.begin(), s.threatActors.end(), [k](const ThreatActor& a) { return a.kind == static_cast<int>(k); }));
}
std::uint64_t perTurnHash(GameState& s, int turns) {
    for (int t = 0; t < turns && s.outcome == CampaignOutcome::Ongoing; ++t) advance(s);
    return fingerprint(s);
}
bool hasLine(const TurnReport& r, const std::string& part) {
    return std::any_of(r.events.begin(), r.events.end(), [&](const std::string& e) { return contains(e, part); });
}
const ThreatKind AllKinds[] = {ThreatKind::BorgCube, ThreatKind::BorgSphere, ThreatKind::Species8472, ThreatKind::Husnock,
    ThreatKind::Calamarain, ThreatKind::Chodak, ThreatKind::CrystallineEntity, ThreatKind::EchoPapa, ThreatKind::EdoGuardian,
    ThreatKind::Gomtuu, ThreatKind::Tarellian, ThreatKind::DeepSpaceNine};
} // namespace

int main() {
    int passed = 0;

    run("default game: threats off, no section, no actor, the current header", [] {
        CampaignSetup setup; setup.seed = 3;
        auto s = newCampaign(setup);
        check(s.threats == ThreatSetup{} && s.threatActors.empty() && s.assimilated.empty() && s.nextThreatId == 1, "no threat state");
        auto text = serialize(s);
        check(text.starts_with(headerOf(SaveVersion)) && !contains(text, "THREATS"), "current header, no THREATS section");
        for (int t = 0; t < 12; ++t) advance(s);
        check(!contains(serialize(s), "THREATS") && s.threatActors.empty(), "twelve turns later still no threat state");
        for (const auto k : AllKinds) check(!threats::kindEnabled(s, static_cast<int>(k)), "no kind enabled");
        check(ContentVersion == "remaster-" + std::to_string(SaveVersion) && SaveVersion >= 20, "schema 20 or later (22 adds eras)");
    }, passed);

    run("switches: monsters, Borg-only off, sphere/8472/Edo replacements, mask, frequency, Borg seated", [] {
        auto s = game(1, ThreatSetup{});
        auto on = [&](const ThreatSetup& t, ThreatKind k) { s.threats = t; return threats::kindEnabled(s, static_cast<int>(k)); };
        ThreatSetup t = everything();
        check(on(t, ThreatKind::BorgCube) && !on(t, ThreatKind::BorgSphere) && on(t, ThreatKind::Husnock) && !on(t, ThreatKind::Species8472) &&
              on(t, ThreatKind::EdoGuardian) && !on(t, ThreatKind::DeepSpaceNine), "everything: the vanilla set");
        t.borg = false;
        check(!on(t, ThreatKind::BorgCube) && !on(t, ThreatKind::BorgSphere) && on(t, ThreatKind::Chodak), "Borg-only off switch leaves the monsters");
        t = everything(); t.monsters = false;
        check(on(t, ThreatKind::BorgCube) && !on(t, ThreatKind::Husnock) && !on(t, ThreatKind::Gomtuu), "monsters off leaves the Borg");
        t = everything(); t.borgSphere = true; t.species8472 = true; t.edoStation = true;
        check(!on(t, ThreatKind::BorgCube) && on(t, ThreatKind::BorgSphere) && !on(t, ThreatKind::Husnock) && on(t, ThreatKind::Species8472) &&
              !on(t, ThreatKind::EdoGuardian) && on(t, ThreatKind::DeepSpaceNine), "sphere, 8472 and DS9 replace cube, Husnock and Edo");
        t = everything(); t.disabledMask = (1 << static_cast<int>(ThreatKind::Calamarain)) | (1 << static_cast<int>(ThreatKind::BorgCube));
        check(!on(t, ThreatKind::Calamarain) && !on(t, ThreatKind::BorgCube) && on(t, ThreatKind::Chodak), "per-actor toggles");
        t = everything(0);
        for (const auto k : AllKinds) check(!on(t, k), "frequency none spawns nothing");
        auto seated = game(2, everything(), {"federation", "klingon", "romulan", "dominion", "borg"});
        check(!threats::kindEnabled(seated, 0) && !threats::kindEnabled(seated, 1) && threats::kindEnabled(seated, 3),
            "a seated Borg empire replaces the Borg threat; monsters stay");
    }, passed);

    run("spawn rules: min turns, caps, statics once, frequency orders spawns, replacements", [] {
        std::array<int, 4> totals{};
        for (int freq = 1; freq <= 3; ++freq) {
            for (std::uint64_t seed = 1; seed <= 4; ++seed) {
                auto s = game(seed, everything(freq));
                TurnReport report;
                std::map<int, int> spawnedAt;
                for (int turn = 1; turn <= 300; ++turn) {
                    s.turn = turn;
                    std::set<int> idsBefore;
                    for (const auto& a : s.threatActors) idsBefore.insert(a.id);
                    threats::step(s, report);
                    for (const auto& a : s.threatActors)
                        if (!idsBefore.count(a.id)) {
                            ++totals[static_cast<std::size_t>(freq)];
                            check(turn >= def(a.kind).minTurn, std::string(def(a.kind).name) + " spawned before its minimum turn");
                            check(a.spawnTurn == turn, "spawn turn recorded");
                            if (def(a.kind).speed == 0) check(turn == 1, "static actors appear at turn 1 only");
                            ++spawnedAt[a.kind];
                        }
                    for (int k = 0; k < ThreatKinds; ++k) {
                        check(count(s, static_cast<ThreatKind>(k)) <= def(k).cap * (freq >= 3 ? 2 : 1), "cap respected");
                        check(threats::kindEnabled(s, k) || count(s, static_cast<ThreatKind>(k)) == 0, "only enabled kinds exist");
                    }
                    threats::validate(s);
                }
                check(count(s, ThreatKind::EchoPapa) == 1 && count(s, ThreatKind::EdoGuardian) == 1, "the static actors stand somewhere");
                check(spawnedAt[static_cast<int>(ThreatKind::EchoPapa)] == 1, "one Echo Papa drone ever");
                check(!spawnedAt.count(static_cast<int>(ThreatKind::BorgSphere)) && !spawnedAt.count(static_cast<int>(ThreatKind::Species8472)),
                    "replaced kinds never spawn");
                check(s.nextThreatId > 3, "roamers spawned in 300 turns");
            }
        }
        check(totals[1] < totals[2] && totals[2] < totals[3], "frequency orders the number of spawns (" + std::to_string(totals[1]) + "/" +
            std::to_string(totals[2]) + "/" + std::to_string(totals[3]) + ")");
        ThreatSetup t = everything(); t.borgSphere = true; t.species8472 = true; t.edoStation = true;
        auto s = game(3, t);
        TurnReport report;
        std::set<int> seen;
        for (int turn = 1; turn <= 300; ++turn) { s.turn = turn; threats::step(s, report); for (const auto& a : s.threatActors) seen.insert(a.kind); }
        check(seen.count(static_cast<int>(ThreatKind::BorgSphere)) && seen.count(static_cast<int>(ThreatKind::Species8472)) &&
              seen.count(static_cast<int>(ThreatKind::DeepSpaceNine)) && !seen.count(static_cast<int>(ThreatKind::BorgCube)) &&
              !seen.count(static_cast<int>(ThreatKind::Husnock)) && !seen.count(static_cast<int>(ThreatKind::EdoGuardian)), "replacement set");
    }, passed);

    run("movement and targeting: hunters close on the nearest colony, guards stay, drifters crawl, black holes block", [] {
        auto s = game(5, everything());
        keepShips(s, 0);
        quiet(s);
        addColonies(s, 2);
        auto& cube = inject(s, ThreatKind::BorgCube, 0, 0);
        const int id = cube.id;
        TurnReport report;
        int last = 99;
        bool closed = false;
        for (int turn = 1; turn <= 40; ++turn) {
            s.turn = turn;
            threats::step(s, report);
            const auto* a = threats::actorById(s, id);
            check(a != nullptr, "the cube is still there");
            int nearest = 99;
            for (const auto& c : s.colonies) {
                const auto& sys = systemOf(s, c.systemId);
                nearest = std::min(nearest, std::max(std::abs(a->x - sys.x), std::abs(a->y - sys.y)));
            }
            check(nearest <= last, "never moves away from its nearest colony");
            last = nearest;
            if (nearest == 0) { closed = true; break; }
        }
        check(closed, "reached a colony");
        auto g = game(6, everything());
        keepShips(g, 0);
        quiet(g);
        auto& papa = inject(g, ThreatKind::EchoPapa, 3, 3);
        const int pid = papa.id;
        auto& gom = inject(g, ThreatKind::Gomtuu, 5, 5);
        const int gid = gom.id;
        int moves = 0, gx = 5, gy = 5;
        for (int turn = 1; turn <= 20; ++turn) {
            g.turn = turn; threats::step(g, report);
            const auto* gg = threats::actorById(g, gid);
            check(gg != nullptr, "Gomtuu present");
            check(std::max(std::abs(gg->x - gx), std::abs(gg->y - gy)) <= 1, "a drifter moves at most one sector");
            if (gg->x != gx || gg->y != gy) ++moves;
            gx = gg->x; gy = gg->y;
        }
        check(moves > 0 && moves <= 10, "a drifter moves on alternate turns at most");
        const auto* papaNow = threats::actorById(g, pid);
        check(papaNow && papaNow->x == 3 && papaNow->y == 3, "a guard never moves");
        auto b = game(7, everything());
        keepShips(b, 0);
        quiet(b);
        for (const auto& f : b.galaxyFeatures) if (f.type == GalaxyFeatureType::BlackHole) {
            inject(b, ThreatKind::Chodak, std::max(0, f.x - 1), f.y);
            for (int turn = 1; turn <= 30; ++turn) {
                b.turn = turn; threats::step(b, report);
                for (const auto& a : b.threatActors) check(sectorPassable(b, a.x, a.y), "no actor inside an impassable sector");
            }
            break;
        }
    }, passed);

    run("despawn: lifespan, a sated Borg cube withdraws, static actors stay", [] {
        auto s = game(8, everything());
        keepShips(s, 0);
        quiet(s);
        auto& hus = inject(s, ThreatKind::Husnock, 2, 2);
        const int hid = hus.id;
        TurnReport report;
        s.turn = def(static_cast<int>(ThreatKind::Husnock)).lifespan - 1;
        threats::step(s, report);
        check(threats::actorById(s, hid) != nullptr, "still here one turn early");
        s.turn += 1;
        threats::step(s, report);
        check(threats::actorById(s, hid) == nullptr, "left when its time was up");
        check(!report.events.empty(), "the departure is logged for a hostile actor");
        auto& cube = inject(s, ThreatKind::BorgCube, 1, 1);
        cube.kills = 3 + s.threats.frequency;
        const int cid = cube.id;
        TurnReport again;
        threats::step(s, again);
        check(threats::actorById(s, cid) == nullptr && !again.events.empty() && contains(again.events.front(), "withdrew"), "a sated cube withdraws");
        s.turn = 1;
        auto t = game(9, everything());
        TurnReport r;
        t.turn = 1; threats::step(t, r);
        t.turn = 400; threats::step(t, r);
        check(count(t, ThreatKind::EchoPapa) == 1 && count(t, ThreatKind::EdoGuardian) == 1, "static actors never leave");
    }, passed);

    run("each actor fights through the tactical engine and acts on a colony as its kind says", [] {
        for (const auto kind : AllKinds) {
            auto s = game(11, everything());
            keepShips(s, 1);
            quiet(s);
            const auto& sys = home(s);
            const int sid = sys.id;
            const int shipId = s.fleets.front().id;
            const ThreatActor snapshot = inject(s, kind, sys.x, sys.y);
            const int pop = s.colonies.front().population;
            const auto report = advance(s);
            const std::string label = def(static_cast<int>(kind)).name;
            const bool fights = threats::hostile(snapshot);
            const auto* now = threats::actorById(s, snapshot.id);
            const bool moved = now && (now->x != sys.x || now->y != sys.y);
            if (fights && !moved) {
                check(report.battles.size() == 1, label + " fought one battle");
                const auto& b = report.battles.front();
                check(b.firstOwner == 1 && b.secondOwner == ThreatOwner, label + ": the threat side is ThreatOwner");
                check(std::any_of(b.participants.begin(), b.participants.end(), [](const CombatParticipant& p) { return threats::isThreatCombatant(p.fleetId); }),
                    label + ": the actor is a recorded participant");
                check(b.lostFleetIds.size() == 1 && b.lostFleetIds[0] == shipId, label + ": our lone destroyer fell");
                check(hasLine(report, "against the " + label), label + ": the player's log carries the battle");
            } else if (!fights) {
                check(report.battles.empty(), label + " is peaceful: no battle");
            }
            if (!moved && now) {
                const auto* c = colonyAt(s, sid);
                switch (def(static_cast<int>(kind)).effect) {
                case threats::Effect::Assimilate: check(c != nullptr ? now->siege >= 1 : !s.assimilated.empty(), label + " besieges or assimilates"); break;
                case threats::Effect::Bombard: check(c != nullptr && c->population < pop && now->siege == 1, label + " bombards"); break;
                case threats::Effect::Strip: check(c != nullptr && c->population < pop && now->siege == 1, label + " strips"); break;
                case threats::Effect::Raid: check(c != nullptr && c->population < pop && now->siege == 1, label + " raids and the world stands"); break;
                case threats::Effect::None: check(c != nullptr && c->population >= pop && now->siege == 0, label + " leaves the world alone"); break;
                }
            }
        }
    }, passed);

    run("sieges: the cube assimilates a system after its siege, batteries slow it, the last world holds longest", [] {
        auto s = game(12, everything());
        keepShips(s, 0);
        quiet(s);
        addColonies(s, 2);
        const int sid = outerColony(s);
        const auto& sys = systemOf(s, sid);
        for (auto& c : s.colonies) if (c.systemId == sid) c.defenseBatteries = 2;
        inject(s, ThreatKind::BorgCube, sys.x, sys.y);
        int turns = 0;
        while (colonyAt(s, sid) && turns < 10) { advance(s); ++turns; }
        check(colonyAt(s, sid) == nullptr, "the colony was assimilated");
        check(turns == 2 + 2 / 2, "two batteries add a turn of siege (took " + std::to_string(turns) + ")");
        const auto* mark = threats::assimilatedAt(s, sid);
        check(mark && mark->ownerId == 0, "recorded as a Borg-held system");
        check(!s.threatActors.empty() && s.threatActors.front().kills == 1, "the kill is counted");
        check(contains(serialize(s), "THREATS"), "saved");
        s.threats.borg = false;
        for (int t = 0; t < threats::AssimilatedMemory + 2 && s.outcome == CampaignOutcome::Ongoing; ++t) advance(s);
        check(threats::assimilatedAt(s, sid) == nullptr, "the mark fades");
        auto last = game(13, everything());
        keepShips(last, 0);
        quiet(last);
        inject(last, ThreatKind::BorgCube, home(last).x, home(last).y);
        int held = 0;
        while (colonyAt(last, home(last).id) && held < 12 && last.outcome == CampaignOutcome::Ongoing) { advance(last); ++held; }
        check(held >= 5, "an empire's last world holds at least five turns (held " + std::to_string(held) + ")");
    }, passed);

    run("bombardment and raids: 8472 destroys a world, Husnock wears it down but leaves it, minors are spared", [] {
        auto s = game(13, everything());
        keepShips(s, 0);
        quiet(s);
        addColonies(s, 2);
        const int sid = outerColony(s);
        const auto& sys = systemOf(s, sid);
        inject(s, ThreatKind::Species8472, sys.x, sys.y);
        int turns = 0, popBefore = 99999;
        while (colonyAt(s, sid) && turns < 12) {
            const int p = colonyAt(s, sid)->population;
            check(p <= popBefore + 2, "population does not recover under bombardment");
            popBefore = p; advance(s); ++turns;
        }
        check(colonyAt(s, sid) == nullptr && s.assimilated.empty(), "8472 destroyed the world, no Borg mark");
        auto h = game(14, everything());
        keepShips(h, 0);
        quiet(h);
        addColonies(h, 1);
        const int hid = outerColony(h);
        const auto& hs = systemOf(h, hid);
        inject(h, ThreatKind::Husnock, hs.x, hs.y);
        for (int t = 0; t < 10; ++t) advance(h);
        check(colonyAt(h, hid) != nullptr && colonyAt(h, hid)->population >= 8, "a raid wears a world down but leaves it");
        auto m = game(15, everything());
        keepShips(m, 0);
        quiet(m);
        check(!m.minors.empty(), "a minor race exists");
        const auto& ms = systemOf(m, m.minors.front().systemId);
        inject(m, ThreatKind::BorgCube, ms.x, ms.y);
        for (int t = 0; t < 4; ++t) advance(m);
        check(m.assimilated.empty(), "no assimilation of a minor's world");
    }, passed);

    run("Borg: adaptation, wreckage feeds the cube, an idle cube mends", [] {
        auto s = game(16, everything());
        keepShips(s, 4);
        quiet(s);
        const auto& sys = home(s);
        auto& cube = inject(s, ThreatKind::BorgCube, sys.x, sys.y);
        cube.hull = threats::actorMaxHull(cube) / 2;
        const int id = cube.id, before = cube.hull;
        advance(s);
        const auto* a = threats::actorById(s, id);
        check(a != nullptr && a->adapt == 10, "adaptation after a battle");
        check(a->hull > before, "wreckage and the cube's repair raised its hull");
        ThreatActor fresh; fresh.kind = static_cast<int>(ThreatKind::BorgCube);
        check(threats::actorMaxShields(*a) == threats::actorMaxShields(fresh) * 110 / 100, "adaptation hardens the shields");
        check(threats::actorAttack(*a) > threats::actorAttack(fresh), "and the attack");
        auto& wounded = s.threatActors.front();
        wounded.hull = 10; wounded.x = wounded.targetX = 0; wounded.y = wounded.targetY = 0;
        for (int t = 0; t < 3; ++t) advance(s);
        const auto* healed = threats::actorById(s, id);
        check(healed && healed->hull > 10 + 3, "an idle cube mends itself");
    }, passed);

    run("Borg empire rules: morale, assimilating a captured colony, assimilated ships, regeneration", [] {
        auto s = game(17, ThreatSetup{}, {"federation", "klingon", "romulan", "dominion", "borg"});
        check(threats::borgSeated(s), "the Borg are seated");
        Colony c = s.colonies.front();
        check(threats::moraleTarget(s, c, 70) == 70, "other empires keep the minor-race morale");
        c.ownerId = threats::BorgEmpire;
        check(threats::moraleTarget(s, c, 70) == 60, "Borg colonies hold the Collective's morale");
        auto& col = *std::find_if(s.colonies.begin(), s.colonies.end(), [](const Colony& x) { return x.ownerId == threats::BorgEmpire; });
        const int sid = col.systemId;
        col.ownerId = threats::BorgEmpire; col.population = 40; col.defenseBatteries = 3; col.factories = 2; col.morale = 80;
        TurnReport report;
        threats::onColonyCaptured(s, col, report);
        check(col.population == 30 && col.defenseBatteries == 0 && col.factories == 5 && col.morale == 30 && col.queue.empty(),
            "a quarter lost, batteries retooled, restless");
        const auto* mark = threats::assimilatedAt(s, sid);
        check(mark && mark->ownerId == threats::BorgEmpire, "marked");
        check(fingerprint(deserialize(serialize(s))) == fingerprint(s), "saved and reloaded");
        CombatReport b;
        b.x = 1; b.y = 1; b.winnerOwner = threats::BorgEmpire; b.firstOwner = 1; b.secondOwner = threats::BorgEmpire;
        for (int i = 0; i < 5; ++i) {
            const int id = 900 + i;
            b.participants.push_back({id, 1, Building::Destroyer, -1, 10, 0});
            b.lostFleetIds.push_back(id);
        }
        std::vector<Fleet> fresh;
        const int nextId = s.nextFleetId;
        threats::assimilateShips(s, b, report, fresh);
        check(fresh.size() == 2 && fresh[0].ownerId == threats::BorgEmpire && fresh[0].id == nextId && fresh[1].id == nextId + 1, "five wrecks, two drones");
        b.winnerOwner = 1; fresh.clear();
        threats::assimilateShips(s, b, report, fresh);
        check(fresh.empty(), "only a Borg victory assimilates");
        Fleet f; f.id = s.nextFleetId++; f.ownerId = threats::BorgEmpire; f.x = f.targetX = 0; f.y = f.targetY = 0;
        f.shipClass = Building::Scout; f.referenceShipId = 1601; f.hull = 20;
        s.fleets.push_back(f);
        TurnReport idle;
        threats::afterCombat(s, idle);
        const auto it = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const Fleet& x) { return x.id == f.id; });
        check(it->hull == 20 + fleetMaxHull(*it) / 10, "an idle Borg ship mends a tenth of its hull");
    }, passed);

    run("a threat battle is recorded, validates and survives a save round trip", [] {
        auto s = game(18, everything());
        keepShips(s, 3);
        quiet(s);
        inject(s, ThreatKind::Husnock, home(s).x, home(s).y);
        auto report = advance(s);
        check(s.latestBattles.size() == 1 && report.battles.size() == 1, "one recorded battle");
        const auto text = serialize(s);
        check(contains(text, "THREATS 1"), "THREATS section written");
        const auto back = deserialize(text);
        check(serialize(back) == text && fingerprint(back) == fingerprint(s), "round trip is byte-stable");
        check(back.latestBattles.front().secondOwner == ThreatOwner, "the threat side survives");
        auto cont1 = s, cont2 = back;
        check(perTurnHash(cont1, 5) == perTurnHash(cont2, 5), "play continues identically after a reload");
    }, passed);

    run("saves: schemas 14 to 18 load, THREATS is refused where it cannot exist, forged sections are refused", [] {
        auto legacy = generateReferenceCampaign(5, 3);
        for (int i = 0; i < 4; ++i) advance(legacy);
        const auto text = serialize(legacy);
        check(text.starts_with(headerOf(SaveVersion)) && !contains(text, "THREATS"), "a legacy-shaped campaign saves without the section");
        for (int old : {14, 15, 16, 17, 18, 20, 21}) {
            const auto migrated = deserialize(withHeader(text, old));
            check(serialize(migrated) == text, "schema " + std::to_string(old) + " text migrates to the current text");
            auto playing = migrated; advance(playing);
            check(playing.turn == migrated.turn + 1, "a migrated schema-" + std::to_string(old) + " campaign plays on");
        }
        CampaignSetup setup; setup.seed = 21; setup.empires = 4;
        auto d = newCampaign(setup);
        for (int i = 0; i < 3; ++i) advance(d);
        const auto remastered = serialize(d);
        for (int old : {16, 17, 18, 20, 21}) {
            const auto m = deserialize(withHeader(remastered, old));
            check(serialize(m) == remastered && fingerprint(m) == fingerprint(d), "a schema-" + std::to_string(old) + " header over a four-empire Remastered text migrates");
        }
        auto t = game(22, everything());
        for (int i = 0; i < 3; ++i) advance(t);
        const auto threatText = serialize(t);
        check(contains(threatText, "THREATS 1"), "a threats-on game saves the section");
        rejects([&] { deserialize(withHeader(threatText, 18)); }, "THREATS under an 18 header", "Schema 20 section");
        rejects([&] { deserialize(withHeader(threatText, 17)); }, "THREATS under a 17 header");
        const auto cut = text.rfind("CHECKSUM ");
        rejects([&] { deserialize(withChecksum(text.substr(0, cut) + "THREATS 1 0 0 2 0 0 0 0 0 1 0 0\nLAST_SPAWN 0 0 0 0 0 0 0 0 0 0 0 0\n")); },
            "empty THREATS section");
        auto forge = [&](const std::string& from, const std::string& to) {
            auto copy = threatText;
            const auto at = copy.find(from);
            check(at != std::string::npos, "forge anchor " + from);
            copy.replace(at, from.size(), to);
            return withChecksum(copy);
        };
        const auto& a0 = t.threatActors.front();
        const std::string line = std::to_string(a0.id) + " " + std::to_string(a0.kind) + " " + std::to_string(a0.x) + " " + std::to_string(a0.y) + " ";
        rejects([&] { deserialize(forge(line, std::to_string(a0.id) + " 99 " + std::to_string(a0.x) + " " + std::to_string(a0.y) + " ")); }, "unknown kind");
        rejects([&] { deserialize(forge(line, std::to_string(a0.id) + " " + std::to_string(a0.kind) + " 999 " + std::to_string(a0.y) + " ")); }, "outside the galaxy");
        rejects([&] { deserialize(forge("THREATS 1 1 1 2", "THREATS 1 1 1 9")); }, "frequency out of range");
    }, passed);

    run("presets carry threat defaults; the setup overrides them", [] {
        auto of = [](const char* preset, const std::optional<ThreatSetup>& t = std::nullopt) {
            CampaignSetup setup; setup.seed = 4; setup.presetId = preset; setup.threats = t;
            return newCampaign(setup).threats;
        };
        const auto dw = of("dominion-war");
        check(dw.monsters && dw.borg && dw.borgSphere && dw.species8472 && dw.edoStation && dw.frequency == 2, "Dominion War: sphere, 8472, DS9");
        ThreatSetup mine; mine.borg = true; mine.frequency = 3;
        check(of("dominion-war", mine) == mine, "the new-game screen's switches win over the preset");
        check(of("dominion-war", ThreatSetup{}) == ThreatSetup{}, "an explicit all-off setup wins too");
        const auto gal = of("galaxies");
        check(gal.monsters && gal.borg && !gal.borgSphere && !gal.species8472, "Galaxies: vanilla actors");
        check(of("ages").frequency == 1 && of("ages").monsters, "Ages: rare");
        check(of("ultimate").monsters && of("ultimate").borg, "Ultimate: on");
        check(of("classic") == ThreatSetup{} && of("remastered") == ThreatSetup{}, "Classic and Remastered: off");
    }, passed);

    run("a player battle against a threat runs through a turn session, including a mid-battle save", [] {
        auto s = game(23, everything());
        keepShips(s, 3);
        quiet(s);
        inject(s, ThreatKind::Husnock, home(s).x, home(s).y);
        auto headless = s;
        const auto expected = advance(headless);
        session::TurnSession session;
        session::begin(session, s);
        const auto before = serialize(s);
        const auto paused = session::resume(session, s);
        check(!paused && session.active && session.pending.has_value(), "the player encounter pauses the turn");
        check(serialize(s) == before, "pausing changes nothing");
        const auto& enc = *session.pending;
        check(std::any_of(enc.ships.begin(), enc.ships.end(), [](const tactical::Ship& sh) { return sh.ownerId == ThreatOwner && threats::isThreatCombatant(sh.fleetId); }),
            "the actor is a ship of the encounter");
        auto reloaded = session::deserialize(session::serialize(session));
        check(reloaded.pending.has_value() && reloaded.pending->ships.size() == enc.ships.size(), "the encounter reloads mid-battle");
        session::autoResolve(session);
        const auto report = session::resume(session, s);
        check(report.has_value() && !session.active, "the turn commits");
        check(fingerprint(s) == fingerprint(headless) && report->stateHash == expected.stateHash, "a session turn equals a headless turn when the AI fights for the player");
    }, passed);

    run("sparesAi: actors ignore every empire but the player", [] {
        auto s = game(24, everything());
        s.threats.sparesAi = true;
        s.aiAggressionEnabled = false;   // The rivals keep to themselves, so only the actor acts on the map.
        keepShips(s, 0);
        quiet(s);
        int rid = 0;
        for (const auto& c : s.colonies) if (c.ownerId != s.playerEmpire) { rid = c.systemId; break; }
        check(rid != 0, "a rival colony");
        const auto& rs = systemOf(s, rid);
        inject(s, ThreatKind::BorgCube, rs.x, rs.y);
        for (int t = 0; t < 4; ++t) { advance(s); check(s.outcome == CampaignOutcome::Ongoing, "ongoing after turn " + std::to_string(s.turn)); }
        check(colonyAt(s, rid) != nullptr && s.assimilated.empty(), "the rival world is untouched");
        check(threats::actorById(s, s.threatActors.front().id) != nullptr, "the cube hunts the player instead");
        s.threats.sparesAi = false;
        auto& cube = s.threatActors.front();   // Back to the rival's world, now fair game.
        cube.x = cube.targetX = rs.x; cube.y = cube.targetY = rs.y; cube.siege = 0;
        for (int t = 0; t < 8 && colonyAt(s, rid); ++t) advance(s);
        check(colonyAt(s, rid) == nullptr, "with the switch off the rival world falls");
    }, passed);

    run("soak: 100 turns with every actor on, two seeds and a Borg-seated game: deterministic, save-stable, valid", [] {
        struct Case { std::uint64_t seed; std::vector<std::string> lineup; int frequency; };
        const std::vector<Case> cases{{31, {}, 3}, {32, {}, 2}, {33, {"federation", "klingon", "romulan", "dominion", "borg"}, 3}};
        int totalSpawns = 0, threatBattles = 0, lines = 0;
        for (const auto& c : cases) {
            auto a = game(c.seed, everything(c.frequency), c.lineup);
            auto b = a;
            for (int turn = 1; turn <= 100 && a.outcome == CampaignOutcome::Ongoing; ++turn) {
                const auto ra = advance(a);
                const auto rb = advance(b);
                check(ra.stateHash == rb.stateHash, "two runs agree at turn " + std::to_string(turn));
                for (const auto& battle : ra.battles)
                    if (battle.secondOwner == ThreatOwner || battle.firstOwner == ThreatOwner) ++threatBattles;
                for (const auto& e : ra.events) if (contains(e, "quadrant") || contains(e, "assimilated") || contains(e, "Borg")) ++lines;
                if (turn % 10 == 0) {
                    const auto text = serialize(a);
                    const auto back = deserialize(text);
                    check(serialize(back) == text && fingerprint(back) == fingerprint(a), "save round trip at turn " + std::to_string(turn));
                    if (turn == 50) {
                        auto cont = back, live = a;
                        check(perTurnHash(cont, 20) == perTurnHash(live, 20), "a reload at turn 50 plays on identically");
                    }
                }
            }
            check(a.nextThreatId > 3, "actors spawned");
            totalSpawns += a.nextThreatId - 1;
            threats::validate(a);
        }
        check(totalSpawns >= 10, "the soak saw spawns (" + std::to_string(totalSpawns) + ")");
        std::cout << "  soak: spawns " << totalSpawns << ", threat battles " << threatBattles << ", log lines " << lines << std::endl;
    }, passed);

    std::cout << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
