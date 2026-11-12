// BOTF Remastered scenarios (lane MOD-8; docs/design/SCENARIOS.md).
//
// A scenario is a new-game choice that sits on a preset: it names the preset (overlays, rule flags), the lineup, the galaxy (seeded or a fixed
// star list), the player's default empire, the start era and year, a scripted start (relations, colonies, fleets, minors, wormholes, threats),
// deterministic triggers (a turn, a war, a lost capital ... then a list of actions, secession among them) and its own victory conditions.
// Scenarios are data (data/scenarios/<id>.scenario.json, embedded like presets by tools/content/build_presets.py); the core never opens a file.
//
// Also here, because they share the scenario save section: flagships (a pack's `flagships` block, the CommissionFlagship command) and the
// vendetta victory table. Every rule is inert in a free game of a pack without a flagships block, so Classic and Remastered play and save
// exactly as before apart from the header.
//
// Save (schema 25): the optional section, written directly after ECONOMY_RULES and before THREATS only when a scenario is running, a flagship
// was ever commissioned or a house has seceded:
//     SCENARIO 1 <id|-> <fired n> <trigger index>...
//     FLAGSHIPS <n>      then n rows: <empireId> <fleetId> <serial> <lostTurn>
//     SECESSIONS <n>     then n rows: <parent> <house> <turn>
#pragma once

#include "Presets.h"
#include "Simulation.h"

#include <algorithm>
#include <cstdint>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace botf::scenarios {

inline constexpr int MaxActions = 64, MaxTriggers = 32, MaxVictories = 8, MaxFixedSystems = 160, MaxBriefing = 12;

enum class ActionKind : int { War, Peace, Alliance, Contact, Colony, Fleet, Flagship, PlaceMinor, Wormhole, PlaceThreat, Secede, Credits };
inline constexpr const char* ActionNames[] = {"war", "peace", "alliance", "contact", "colony", "fleet", "flagship", "placeMinor", "wormhole",
    "placeThreat", "secede", "credits"};

struct Action {
    ActionKind kind = ActionKind::War;
    std::string a, b;                 // Empire references ("player" or a race key); secede: parent and house
    std::string at;                   // System reference (see docs/design/SCENARIOS.md section 3)
    std::string name;                 // placeMinor: minor race name; placeThreat: threat actor key; wormhole: the second reference; flagship: ship class
    std::vector<std::string> ships;   // fleet: roster class names ("Defiant") or "id:1022"
    int count = 1, amount = 0, pct = 50, population = 30;
};
struct When {
    int turn = -1;                    // This turn or later
    std::string eliminated;           // Empire with no colonies left
    std::string warA, warB;           // The two empires are at war
    std::string colEmpire;            // Colony count test for this empire ...
    int colAtLeast = -1, colAtMost = -1;
    std::string capitalLost;          // The empire no longer holds the system that was its capital
    bool empty() const { return turn < 0 && eliminated.empty() && warA.empty() && colEmpire.empty() && capitalLost.empty(); }
};
struct Trigger {
    std::string id, text;
    When when;
    std::vector<Action> actions;
};
enum class VictoryKind : int { Eliminate, Hold, Survive };
inline constexpr const char* VictoryKindNames[] = {"eliminate", "hold", "survive"};
struct Victory {
    std::string id, name, text;
    VictoryKind kind = VictoryKind::Eliminate;
    std::vector<std::string> empires;   // eliminate: every one of them has no colony left
    std::vector<std::string> systems;   // hold: the player holds every one (capital:, minor: or system: references)
    int turn = 0;                       // survive: the player still has a colony on this turn
};
struct FixedSystem { std::string name; int x = 0, y = 0; };
struct Map {
    int width = 0, height = 0, systems = 0;
    std::string shape, density;         // "" = leave to the setup
    std::optional<std::uint64_t> seed;  // absent: the setup's seed
    std::vector<FixedSystem> fixed;     // present: exactly these stars (system 1 is the first lineup empire's capital)
    std::vector<std::string> homes;     // fixed maps: the capital system of lineup seat 2, 3, ... by system name
};
struct Palette { int empire = 0; std::string color; };
struct Scenario {
    std::string id, name, blurb;
    int order = 0;
    std::string preset;
    std::vector<std::string> overlays;  // Added to the preset's stack
    Map map;
    std::vector<std::string> lineup;    // Race keys, seating order
    std::string player;                 // Default player race key
    int era = 0, year = 0;
    std::vector<std::string> briefing;
    std::vector<Action> setup;
    std::vector<Trigger> triggers;
    std::vector<Victory> victories;
};

namespace detail {
using content::detail::bad;
using content::detail::Object;
inline std::string refText(Object& o, const char* key) {
    const std::string v = o.str(key, 64);
    if (v.empty()) bad(o.at(key), "empty reference");
    return v;
}
inline bool validKey(const std::string& v) {
    if (v == "player") return true;
    if (v.empty() || v.size() > 32) return false;
    for (char c : v) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return false;
    return true;
}
inline void empireRef(Object& o, const char* key, std::string& out) {
    out = refText(o, key);
    if (!validKey(out)) bad(o.at(key), "expected \"player\" or a race key (letters only)");
}
inline void systemRef(Object& o, const char* key, std::string& out) {
    out = refText(o, key);
    const bool ok = out.rfind("capital:", 0) == 0 || out.rfind("minor:", 0) == 0 || out.rfind("system:", 0) == 0 || out.rfind("near:", 0) == 0;
    if (!ok) bad(o.at(key), "expected capital:<race>, minor:<name>, system:<name> or near:<n>:<reference>");
    if (out.rfind("near:", 0) == 0) {
        const auto second = out.find(':', 5);
        if (second == std::string::npos || second == 5 || second > 7) bad(o.at(key), "near needs a count: near:<n>:<reference>");
        for (std::size_t i = 5; i < second; ++i) if (out[i] < '0' || out[i] > '9') bad(o.at(key), "near needs a count: near:<n>:<reference>");
        if (out[5] == '0') bad(o.at(key), "near counts from 1");
    }
}
inline std::vector<std::string> strings(const content::json::Value& v, const std::string& path, std::size_t max) {
    if (v.kind != content::json::Value::Kind::Array) bad(path, "expected an array of strings");
    if (v.items.size() > max) bad(path, "too many entries");
    std::vector<std::string> out;
    for (std::size_t i = 0; i < v.items.size(); ++i) {
        if (v.items[i].kind != content::json::Value::Kind::String || v.items[i].text.empty() || v.items[i].text.size() > 200)
            bad(path + "[" + std::to_string(i) + "]", "expected a non-empty string");
        out.push_back(v.items[i].text);
    }
    return out;
}
inline Action parseAction(const content::json::Value& v, const std::string& path) {
    Object o(v, path);
    Action a;
    const std::string kind = o.str("do", 24);
    int k = -1;
    for (int i = 0; i < 12; ++i) if (kind == ActionNames[i]) k = i;
    if (k < 0) bad(o.at("do"), "unknown action \"" + kind + "\"");
    a.kind = static_cast<ActionKind>(k);
    switch (a.kind) {
    case ActionKind::War: case ActionKind::Peace: case ActionKind::Alliance: case ActionKind::Contact:
        empireRef(o, "a", a.a); empireRef(o, "b", a.b);
        if (a.a == a.b) bad(o.at("b"), "an empire cannot relate to itself");
        break;
    case ActionKind::Colony:
        empireRef(o, "a", a.a); systemRef(o, "at", a.at);
        if (o.has("population")) a.population = o.i32("population", 5, 200);
        break;
    case ActionKind::Fleet:
        empireRef(o, "a", a.a); systemRef(o, "at", a.at);
        a.ships = strings(o.get("ships"), o.at("ships"), 8);
        if (a.ships.empty()) bad(o.at("ships"), "name at least one ship class");
        if (o.has("count")) a.count = o.i32("count", 1, 12);
        break;
    case ActionKind::Flagship:
        empireRef(o, "a", a.a); systemRef(o, "at", a.at); a.name = o.str("ship", 64);
        if (a.name.empty()) bad(o.at("ship"), "name the ship class");
        break;
    case ActionKind::PlaceMinor:
        a.name = o.str("name", 64); systemRef(o, "at", a.at);
        if (a.name.empty()) bad(o.at("name"), "name the minor race");
        break;
    case ActionKind::Wormhole:
        systemRef(o, "at", a.at); {
            const std::string second = o.str("to", 64);
            a.name = second;
            const bool ok = second.rfind("capital:", 0) == 0 || second.rfind("minor:", 0) == 0 || second.rfind("system:", 0) == 0 || second.rfind("near:", 0) == 0;
            if (!ok) bad(o.at("to"), "expected a system reference");
        }
        break;
    case ActionKind::PlaceThreat:
        a.name = o.str("name", 40); systemRef(o, "at", a.at);
        {   bool known = false;
            for (int i = 0; i < ThreatKinds; ++i) known = known || a.name == threats::Defs[i].key;
            if (!known) bad(o.at("name"), "unknown threat actor key");
        }
        break;
    case ActionKind::Secede:
        empireRef(o, "a", a.a); empireRef(o, "b", a.b);
        if (a.a == a.b) bad(o.at("b"), "the house must be another empire");
        if (o.has("pct")) a.pct = o.i32("pct", 10, 90);
        break;
    case ActionKind::Credits:
        empireRef(o, "a", a.a); a.amount = o.i32("amount", -100000, 1000000);
        break;
    }
    o.done();
    return a;
}
inline std::vector<Action> parseActions(const content::json::Value& v, const std::string& path) {
    if (v.kind != content::json::Value::Kind::Array) bad(path, "expected an array of actions");
    if (v.items.size() > static_cast<std::size_t>(MaxActions)) bad(path, "too many actions");
    std::vector<Action> out;
    for (std::size_t i = 0; i < v.items.size(); ++i) out.push_back(parseAction(v.items[i], path + "[" + std::to_string(i) + "]"));
    return out;
}
} // namespace detail

inline Scenario parseScenario(std::string_view bytes) {
    using content::detail::bad;
    using detail::Object;
    if (bytes.size() > 1024u * 1024u) bad("scenario", "scenario larger than 1 MB");
    const auto doc = content::json::Parser(bytes).document();
    Object root(doc, "scenario");
    Scenario s;
    if (root.i32("format", 0, content::detail::Big) != content::FormatVersion) bad("scenario.format", "format is not supported");
    s.id = root.str("id", 40);
    if (!content::detail::validPackId(s.id)) bad("scenario.id", "id must be [a-z0-9-]");
    s.name = root.str("name", 80);
    if (s.name.empty()) bad("scenario.name", "empty name");
    s.blurb = root.str("blurb", 600);
    s.order = root.i32("order", 0, 1000);
    s.preset = root.str("preset", 40);
    if (!content::detail::validPackId(s.preset)) bad("scenario.preset", "not a preset id");
    if (root.has("overlays")) {
        s.overlays = presets::detail::stringList(root.get("overlays"), "scenario.overlays");
        for (std::size_t i = 0; i < s.overlays.size(); ++i) {
            if (!content::detail::validPackId(s.overlays[i])) bad("scenario.overlays[" + std::to_string(i) + "]", "not a pack id");
            for (std::size_t j = 0; j < i; ++j) if (s.overlays[j] == s.overlays[i]) bad("scenario.overlays[" + std::to_string(i) + "]", "overlay listed twice");
        }
    }
    {   Object m(root.get("map"), "scenario.map");
        Map& map = s.map;
        map.width = m.i32("width", 2, 64); map.height = m.i32("height", 2, 64);
        if (m.has("systems")) map.systems = m.i32("systems", 2, 400);
        if (m.has("seed")) map.seed = m.u64("seed");
        if (m.has("shape")) {
            map.shape = m.str("shape", 16);
            if (!presets::detail::oneOf(map.shape, presets::ShapeIds)) bad(m.at("shape"), "unknown galaxy shape");
        }
        if (m.has("minorDensity")) {
            map.density = m.str("minorDensity", 16);
            if (!presets::detail::oneOf(map.density, presets::DensityIds)) bad(m.at("minorDensity"), "unknown minor-race density");
        }
        if (m.has("fixed")) {
            const auto& a = m.array("fixed");
            if (a.items.size() < 4 || a.items.size() > static_cast<std::size_t>(MaxFixedSystems)) bad(m.at("fixed"), "a fixed map holds 4.." + std::to_string(MaxFixedSystems) + " stars");
            for (std::size_t i = 0; i < a.items.size(); ++i) {
                Object f(a.items[i], m.at("fixed") + "[" + std::to_string(i) + "]");
                FixedSystem fs;
                fs.name = f.str("name", 40); fs.x = f.i32("x", 0, 63); fs.y = f.i32("y", 0, 63);
                f.done();
                if (fs.name.empty()) bad(f.at("name"), "empty star name");
                if (fs.x >= map.width || fs.y >= map.height) bad(f.at("x"), "star outside the map");
                for (const auto& other : map.fixed) {
                    if (other.name == fs.name) bad(f.at("name"), "star named twice");
                    if (other.x == fs.x && other.y == fs.y) bad(f.at("x"), "two stars in one sector");
                }
                map.fixed.push_back(fs);
            }
            map.systems = static_cast<int>(map.fixed.size());
            if (m.has("homes")) map.homes = presets::detail::stringList(m.get("homes"), m.at("homes"));
        } else {
            if (map.systems == 0) bad(m.at("systems"), "a seeded map needs a system count");
            if (map.systems > map.width * map.height) bad(m.at("systems"), "more stars than sectors");
            if (m.has("homes")) bad(m.at("homes"), "homes belong to fixed maps");
        }
        m.done();
    }
    s.lineup = presets::detail::stringList(root.get("lineup"), "scenario.lineup");
    if (s.lineup.size() < 2 || s.lineup.size() > static_cast<std::size_t>(presets::LineupMax)) bad("scenario.lineup", "a lineup holds 2.." + std::to_string(presets::LineupMax) + " empires");
    for (std::size_t i = 0; i < s.lineup.size(); ++i) {
        if (!detail::validKey(s.lineup[i]) || s.lineup[i] == "player") bad("scenario.lineup[" + std::to_string(i) + "]", "expected a race key (letters only)");
        for (std::size_t j = 0; j < i; ++j) if (s.lineup[j] == s.lineup[i]) bad("scenario.lineup[" + std::to_string(i) + "]", "empire listed twice");
    }
    s.player = root.str("player", 32);
    if (std::find(s.lineup.begin(), s.lineup.end(), s.player) == s.lineup.end()) bad("scenario.player", "the default player is not in the lineup");
    if (!s.map.fixed.empty()) {
        if (s.map.homes.size() + 1 != s.lineup.size()) bad("scenario.map.homes", "a fixed map names one home star for every lineup seat after the first");
        for (const auto& h : s.map.homes) {
            bool found = false;
            for (const auto& f : s.map.fixed) found = found || f.name == h;
            if (!found) bad("scenario.map.homes", "home star \"" + h + "\" is not on the map");
        }
    }
    s.era = root.i32("era", 1, 5);
    s.year = root.i32("year", 2150, 2500);
    s.briefing = detail::strings(root.get("briefing"), "scenario.briefing", static_cast<std::size_t>(MaxBriefing));
    s.setup = detail::parseActions(root.get("setup"), "scenario.setup");
    {   const auto& a = root.array("triggers");
        if (a.items.size() > static_cast<std::size_t>(MaxTriggers)) bad(root.at("triggers"), "too many triggers");
        for (std::size_t i = 0; i < a.items.size(); ++i) {
            const std::string path = "scenario.triggers[" + std::to_string(i) + "]";
            Object t(a.items[i], path);
            Trigger tr;
            tr.id = t.str("id", 40);
            if (!content::detail::validPackId(tr.id)) bad(t.at("id"), "id must be [a-z0-9-]");
            for (const auto& o : s.triggers) if (o.id == tr.id) bad(t.at("id"), "trigger id used twice");
            tr.text = t.str("text", 400);
            {   Object w(t.get("when"), t.at("when"));
                if (w.has("turn")) tr.when.turn = w.i32("turn", 1, 10000);
                if (w.has("eliminated")) detail::empireRef(w, "eliminated", tr.when.eliminated);
                if (w.has("warA") || w.has("warB")) { detail::empireRef(w, "warA", tr.when.warA); detail::empireRef(w, "warB", tr.when.warB); }
                if (w.has("colonies")) {
                    detail::empireRef(w, "colonies", tr.when.colEmpire);
                    if (w.has("atLeast")) tr.when.colAtLeast = w.i32("atLeast", 0, 1000);
                    if (w.has("atMost")) tr.when.colAtMost = w.i32("atMost", 0, 1000);
                    if (tr.when.colAtLeast < 0 && tr.when.colAtMost < 0) bad(w.at("colonies"), "name atLeast or atMost");
                } else if (w.has("atLeast") || w.has("atMost")) bad(w.at("atLeast"), "atLeast and atMost belong to a colonies test");
                if (w.has("capitalLost")) detail::empireRef(w, "capitalLost", tr.when.capitalLost);
                w.done();
                if (tr.when.empty()) bad(t.at("when"), "a trigger needs at least one condition");
            }
            tr.actions = detail::parseActions(t.get("do"), t.at("do"));
            t.done();
            s.triggers.push_back(std::move(tr));
        }
    }
    {   const auto& a = root.array("victory");
        if (a.items.size() > static_cast<std::size_t>(MaxVictories)) bad(root.at("victory"), "too many victory conditions");
        for (std::size_t i = 0; i < a.items.size(); ++i) {
            Object o(a.items[i], "scenario.victory[" + std::to_string(i) + "]");
            Victory v;
            v.id = o.str("id", 40);
            if (!content::detail::validPackId(v.id)) bad(o.at("id"), "id must be [a-z0-9-]");
            v.name = o.str("name", 80); v.text = o.str("text", 400);
            const int k = o.enumeration("kind", VictoryKindNames, "victory kind");
            v.kind = static_cast<VictoryKind>(k);
            if (v.kind == VictoryKind::Eliminate) {
                v.empires = presets::detail::stringList(o.get("empires"), o.at("empires"));
                if (v.empires.empty()) bad(o.at("empires"), "name at least one empire");
                for (const auto& e : v.empires) if (!detail::validKey(e) || e == "player") bad(o.at("empires"), "expected race keys");
            } else if (v.kind == VictoryKind::Hold) {
                v.systems = presets::detail::stringList(o.get("systems"), o.at("systems"));
                if (v.systems.empty()) bad(o.at("systems"), "name at least one system");
                for (const auto& r : v.systems)
                    if (r.rfind("capital:", 0) != 0 && r.rfind("minor:", 0) != 0 && r.rfind("system:", 0) != 0) bad(o.at("systems"), "hold needs capital:, minor: or system: references");
            } else {
                v.turn = o.i32("turn", 1, 10000);
            }
            o.done();
            for (const auto& other : s.victories) if (other.id == v.id) bad(o.at("id"), "victory id used twice");
            s.victories.push_back(std::move(v));
        }
    }
    root.done();
    return s;
}

// Defined in Simulation.cpp from ContentPresets.generated.h.
const std::vector<std::string>& embeddedScenarioFiles();

namespace detail {
struct Registry {
    std::vector<Scenario> scenarios;
    std::size_t embedded = 0;
};
inline void addScenario(Registry& r, std::string_view bytes) {
    Scenario s = parseScenario(bytes);
    for (const auto& have : r.scenarios) if (have.id == s.id) content::detail::bad("scenario " + s.id, "already registered");
    const auto* preset = presets::find(s.preset);
    if (!preset) content::detail::bad("scenario " + s.id, "preset " + s.preset + " is not registered");
    for (const auto& id : s.overlays)
        if (!presets::overlayBytes(id)) content::detail::bad("scenario " + s.id, "overlay " + id + " is not registered");
    r.scenarios.push_back(std::move(s));
}
inline Registry& registry() {
    static Registry r = [] {
        Registry fresh;
        for (const auto& text : embeddedScenarioFiles()) addScenario(fresh, text);
        fresh.embedded = fresh.scenarios.size();
        return fresh;
    }();
    return r;
}
} // namespace detail

// Host registration of a user scenario (bytes the host read; its preset and overlays must already be registered). Atomic.
inline std::string registerScenario(std::string_view bytes) {
    auto& r = detail::registry();
    detail::addScenario(r, bytes);
    return r.scenarios.back().id;
}
inline std::vector<const Scenario*> all() {
    std::vector<const Scenario*> out;
    for (const auto& s : detail::registry().scenarios) out.push_back(&s);
    std::stable_sort(out.begin(), out.end(), [](const Scenario* a, const Scenario* b) { return a->order != b->order ? a->order < b->order : a->id < b->id; });
    return out;
}
inline const Scenario* find(const std::string& id) {
    for (const auto& s : detail::registry().scenarios) if (s.id == id) return &s;
    return nullptr;
}
// The preset a scenario plays on, with the scenario's own overlays appended to its stack.
inline presets::Preset effectivePreset(const Scenario& s) {
    presets::Preset p = *presets::find(s.preset);
    for (const auto& o : s.overlays) if (std::find(p.overlays.begin(), p.overlays.end(), o) == p.overlays.end()) p.overlays.push_back(o);
    return p;
}

// ---- Flagships ----------------------------------------------------------------------------------------------------------------
inline const content::FlagshipDef* flagshipDef(int empireId) {
    for (const auto& d : content::flagships().defs) if (d.empireId == empireId) return &d;
    return nullptr;
}
inline bool flagshipsOn() { return content::flagships().present && !content::flagships().defs.empty(); }
// Percent bonuses for a ship that carries a flagship mark (zero for any other ship, or an owner the pack lists no flagship for).
struct Bonus { int hull = 0, attack = 0, shields = 0; };
inline Bonus bonusFor(const Fleet& fleet) {
    if (fleet.flagship <= 0) return {};
    const auto* d = flagshipDef(fleet.ownerId);
    return d ? Bonus{d->hullPct, d->attackPct, d->shieldsPct} : Bonus{};
}
inline std::string flagshipName(const Fleet& fleet) {
    if (fleet.flagship <= 0) return {};
    const auto* d = flagshipDef(fleet.ownerId);
    if (!d) return {};
    return d->names[static_cast<std::size_t>(fleet.flagship - 1) % d->names.size()];
}
const Flagship* flagshipOf(const GameState& state, int empireId);          // The record (alive or lost), or nullptr
const Fleet* flagshipFleet(const GameState& state, int empireId);          // The living flagship ship, or nullptr
int flagshipMorale(const GameState& state, int empireId);                  // Morale added to the empire's colonies while its flagship lives
std::string commissionBlocker(const GameState& state, int empireId, int fleetId); // Empty when the empire may commission this ship
Command makeCommissionFlagshipCommand(std::uint64_t id, int fleetId);
void commission(GameState& state, int empireId, int fleetId);              // Pays, marks the ship, records it (the blocker must be empty)
void planFlagships(GameState& next, int empireId);      // AI: commissions its strongest warship when it can afford the refit

// ---- Vendetta (the vanilla victory) --------------------------------------------------------------------------------------------
// Each major designates two racial rivals; defeating both (every one of them seated in the game) wins the vendetta once turn 20 is past.
std::vector<int> vendettaRivals(int empireId);

// ---- Scenario script ------------------------------------------------------------------------------------------------------------
int resolveEmpire(const GameState& state, const std::string& ref);          // "player" or a race key -> empire id (0 if not seated)
int resolveSystem(const GameState& state, const std::string& ref);          // A system reference -> system id (0 if none)
void applySetup(GameState& state, const Scenario& scenario);                 // The scripted start; the state is a new campaign at turn 0
void step(GameState& next, TurnReport& report);                              // Fire due triggers, keep the flagship records straight
void runAction(GameState& state, const Action& action, TurnReport* report); // One action (setup and triggers)
bool secede(GameState& state, TurnReport* report, int parent, int house, int pct); // The secession rule (docs/design/SCENARIOS.md section 5)
bool victoryAchieved(const GameState& state, const Victory& v);
struct ObjectiveStatus { std::string id, name, text; bool achieved = false; int percent = 0; };
std::vector<ObjectiveStatus> objectives(const GameState& state);             // Scenario victory conditions for the player
int yearOf(const GameState& state);                                          // Scenario year at the current turn (0 outside a scenario)

// Rule hooks and the save section.
void validate(const GameState& state);
bool sectionNeeded(const GameState& state);
void writeSection(std::ostream& out, const GameState& state);
void readSection(std::istream& in, GameState& state);
void evaluateVictories(GameState& next, TurnReport& report);                 // Scenario victory conditions (the player wins when any holds)

// Map overrides for a fixed-map scenario, installed around campaign generation (Simulation.cpp).
struct MapOverride { const std::vector<FixedSystem>* systems = nullptr; const std::vector<std::string>* homes = nullptr; };

} // namespace botf::scenarios
