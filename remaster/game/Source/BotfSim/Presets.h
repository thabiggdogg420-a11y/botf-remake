// BOTF Remastered presets and the pack registry (docs/design/PACKS-AND-PRESETS.md).
//
// A preset is a new-game choice: which overlay packs sit on the base pack, which rule flags are on, which empires
// are in play (the lineup) and the defaults for the new-game screen. Presets and overlays are data: the core embeds the
// shipped ones (ContentPresets.generated.h, built by tools/content/build_presets.py) and a host may register more as
// bytes. The core never opens a file.
#pragma once
#include "Content.h"
#include "Threats.h"

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace botf::presets {

// The cap on majors and the empires the core can seat. Lane MOD-2 (empire slots, EmpireSlots.h) lifted MajorCap to eight and
// named the Dominion, Borg and Breen; a lineup is any distinct subset of them, in any order (the seating order of the preset).
inline constexpr int MajorCap = 8;
inline constexpr int LineupMax = 8;  // Structural sanity bound for a lineup field, independent of the cap.
inline constexpr const char* StockEmpires[] = {"Federation", "Klingon", "Romulan", "Cardassian", "Ferengi", "Dominion", "Borg", "Breen"};
inline constexpr int StockEmpireCount = 8;

inline constexpr const char* FlagNames[] = {"stations", "galaxyFeatures", "curatedEvents", "alternativeVictories",
    "expandedRoster", "flightModel", "wegoCombat", "empireRules",
    // Schema 23 (EconomyRules.h): the economy and rules mechanics.
    "structureUpgrades", "autoUpgrades", "noUpgrades", "sharedMorale", "protectorateHq", "allDilithium", "interceptOrders",
    "raidOrders", "raidBonus", "crewExperience", "exploitFixes"};
inline constexpr const char* SizeIds[] = {"small", "medium", "large", "huge"};
inline constexpr const char* ShapeIds[] = {"legacy", "irregular", "spiral", "ring", "elliptical"};
inline constexpr const char* DensityIds[] = {"legacy", "none", "few", "some", "many"};
inline constexpr const char* DifficultyIds[] = {"simple", "easy", "normal", "hard", "impossible"};

struct Preset {
    std::string id, name, blurb;
    int order = 0;
    std::vector<std::string> overlays;                // Overlay ids over the base pack, in merge order.
    std::string rulesBase;                            // "classic" or "remastered": the flag set before the overrides.
    std::vector<std::pair<std::string, bool>> flags;  // Rule-flag overrides (FlagNames).
    std::vector<std::string> lineup;                  // Major empires in slot order.
    bool lineupPlayable = false;                      // The core can seat this lineup today.
    std::string lineupNote;                           // Why not, when it cannot.
    // New-game defaults. 0, "" and -1 mean "leave the screen's own default".
    int empires = 0, era = 0;
    std::string size, shape, minorDensity, difficulty;
    int events = -1, aiAggression = -1;
    std::optional<ThreatSetup> threats;                // Schema 20: threat-switch defaults (absent = threats off).
    std::vector<std::string> needs;                   // Later lanes whose content this preset is waiting for.
};

// Checks a lineup against the cap and the seatable empires. Structural faults throw; a lineup the core cannot seat yet
// comes back as playable = false with the reason, so the preset still loads and says what it is waiting for.
struct LineupCheck { bool playable = true; std::string note; };
inline LineupCheck checkLineup(const std::vector<std::string>& lineup, const std::string& path) {
    if (lineup.size() < 2 || lineup.size() > static_cast<std::size_t>(LineupMax))
        content::detail::bad(path, "a lineup holds 2.." + std::to_string(LineupMax) + " empires");
    for (std::size_t i = 0; i < lineup.size(); ++i) {
        if (lineup[i].empty() || lineup[i].size() > 32) content::detail::bad(path + "[" + std::to_string(i) + "]", "empty or oversized empire key");
        for (char c : lineup[i]) if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) content::detail::bad(path + "[" + std::to_string(i) + "]", "an empire key is letters only");
        for (std::size_t j = 0; j < i; ++j) if (lineup[j] == lineup[i]) content::detail::bad(path + "[" + std::to_string(i) + "]", "empire listed twice");
    }
    LineupCheck out;
    if (lineup.size() > static_cast<std::size_t>(MajorCap)) {
        out.playable = false;
        out.note = std::to_string(lineup.size()) + " majors exceed the cap of " + std::to_string(MajorCap) + " (lane MOD-2).";
        return out;
    }
    for (std::size_t i = 0; i < lineup.size(); ++i) {
        bool known = false;
        for (const char* e : StockEmpires) known = known || lineup[i] == e;
        if (!known) {
            out.playable = false;
            out.note = lineup[i] + " is not an empire the core can seat (lane MOD-2 seats " + std::to_string(StockEmpireCount) + ").";
            return out;
        }
    }
    return out;
}

namespace detail {
template <std::size_t N> bool oneOf(const std::string& s, const char* const (&list)[N]) {
    for (const char* x : list) if (s == x) return true;
    return false;
}
inline std::vector<std::string> stringList(const content::json::Value& v, const std::string& path) {
    if (v.kind != content::json::Value::Kind::Array) content::detail::bad(path, "expected an array of strings");
    std::vector<std::string> out;
    for (std::size_t i = 0; i < v.items.size(); ++i) {
        if (v.items[i].kind != content::json::Value::Kind::String) content::detail::bad(path + "[" + std::to_string(i) + "]", "expected a string");
        out.push_back(v.items[i].text);
    }
    return out;
}
} // namespace detail

// The flag set a preset ends with must be one validate() (Rules.cpp) accepts: a Remastered base keeps the four layers on;
// a Classic base keeps every layer but stations and empire rules off; WEGO combat needs the flight model. Checked at load
// so a bad preset is refused when it registers, not when someone starts a game with it.
inline void checkFlags(const std::string& base, const std::vector<std::pair<std::string, bool>>& overrides, const std::string& path) {
    const bool remastered = base == "remastered";
    auto value = [&](const char* name) {
        bool v = remastered || std::string(name) == "stations" || std::string(name) == "empireRules";
        const std::string n(name);
        const bool economyFlag = n == "structureUpgrades" || n == "autoUpgrades" || n == "noUpgrades" || n == "sharedMorale" || n == "protectorateHq" ||
            n == "allDilithium" || n == "interceptOrders" || n == "raidOrders" || n == "raidBonus" || n == "crewExperience" || n == "exploitFixes";
        if (economyFlag) {
            v = n == "interceptOrders" || n == "raidOrders" || n == "crewExperience"; // vanilla rules, on in both bases
        }
        for (const auto& o : overrides) if (o.first == name) v = o.second;
        return v;
    };
    if (remastered) {
        for (const char* must : {"stations", "galaxyFeatures", "curatedEvents", "alternativeVictories"})
            if (!value(must)) content::detail::bad(path, std::string("flag ") + must + " cannot be off under the remastered base");
    } else {
        for (const char* off : {"galaxyFeatures", "curatedEvents", "alternativeVictories", "expandedRoster", "flightModel", "wegoCombat"})
            if (value(off)) content::detail::bad(path, std::string("flag ") + off + " cannot be on under the classic base");
    }
    if (value("wegoCombat") && !value("flightModel")) content::detail::bad(path, "wegoCombat needs flightModel");
    if (value("noUpgrades") && value("autoUpgrades")) content::detail::bad(path, "noUpgrades excludes autoUpgrades");
    if (value("raidBonus") && !value("raidOrders")) content::detail::bad(path, "raidBonus needs raidOrders");
}

inline Preset parsePreset(std::string_view bytes) {
    using content::detail::bad;
    if (bytes.size() > 1024u * 1024u) bad("preset", "preset larger than 1 MB");
    const auto doc = content::json::Parser(bytes).document();
    content::detail::Object root(doc, "preset");
    Preset p;
    if (root.i32("format", 0, content::detail::Big) != content::FormatVersion) bad("preset.format", "format is not supported");
    p.id = root.str("id", 40);
    if (!content::detail::validPackId(p.id)) bad("preset.id", "id must be [a-z0-9-]");
    p.name = root.str("name", 80);
    p.blurb = root.str("blurb", 400);
    if (p.name.empty()) bad("preset.name", "empty name");
    p.order = root.i32("order", 0, 1000);
    p.overlays = detail::stringList(root.get("overlays"), "preset.overlays");
    for (std::size_t i = 0; i < p.overlays.size(); ++i) {
        if (!content::detail::validPackId(p.overlays[i])) bad("preset.overlays[" + std::to_string(i) + "]", "not a pack id");
        for (std::size_t j = 0; j < i; ++j) if (p.overlays[j] == p.overlays[i]) bad("preset.overlays[" + std::to_string(i) + "]", "overlay listed twice");
    }
    {   content::detail::Object r(root.get("rules"), "preset.rules");
        p.rulesBase = r.str("base", 16);
        if (p.rulesBase != "classic" && p.rulesBase != "remastered") bad(r.at("base"), "expected \"classic\" or \"remastered\"");
        const auto& f = r.get("flags");
        if (f.kind != content::json::Value::Kind::Object) bad(r.at("flags"), "expected an object");
        for (const auto& m : f.members) {
            if (!detail::oneOf(m.first, FlagNames)) bad(r.at("flags") + "." + m.first, "unknown rule flag");
            if (m.second.kind != content::json::Value::Kind::Bool) bad(r.at("flags") + "." + m.first, "expected true or false");
            p.flags.emplace_back(m.first, m.second.boolean);
        }
        checkFlags(p.rulesBase, p.flags, r.at("flags"));
        r.done();
    }
    p.lineup = detail::stringList(root.get("lineup"), "preset.lineup");
    const auto check = checkLineup(p.lineup, "preset.lineup");
    p.lineupPlayable = check.playable;
    p.lineupNote = check.note;
    {   content::detail::Object o(root.get("options"), "preset.options");
        if (o.has("empires")) p.empires = o.i32("empires", 2, MajorCap);
        if (o.has("era")) p.era = o.i32("era", 1, 5);
        auto pick = [&](const char* key, std::string& out, auto& list, const char* what) {
            if (!o.has(key)) return;
            out = o.str(key, 16);
            if (!detail::oneOf(out, list)) bad(o.at(key), std::string("unknown ") + what + " \"" + out + "\"");
        };
        pick("size", p.size, SizeIds, "galaxy size");
        pick("shape", p.shape, ShapeIds, "galaxy shape");
        pick("minorDensity", p.minorDensity, DensityIds, "minor-race density");
        pick("difficulty", p.difficulty, DifficultyIds, "difficulty");
        if (o.has("events")) p.events = o.flag("events") ? 1 : 0;
        if (o.has("aiAggression")) p.aiAggression = o.flag("aiAggression") ? 1 : 0;
        if (o.has("threats")) {   // Schema 20 (docs/design/THREAT-ACTORS.md section 4).
            content::detail::Object t(o.get("threats"), o.at("threats"));
            ThreatSetup ts;
            if (t.has("monsters")) ts.monsters = t.flag("monsters");
            if (t.has("borg")) ts.borg = t.flag("borg");
            if (t.has("borgSphere")) ts.borgSphere = t.flag("borgSphere");
            if (t.has("species8472")) ts.species8472 = t.flag("species8472");
            if (t.has("edoStation")) ts.edoStation = t.flag("edoStation");
            if (t.has("sparesAi")) ts.sparesAi = t.flag("sparesAi");
            if (t.has("frequency")) {
                const std::string f = t.str("frequency", 16);
                int idx = -1;
                for (int i = 0; i < 4; ++i) if (f == threats::frequencyName(i)) idx = i;
                if (idx < 0) bad(t.at("frequency"), "expected none, rare, normal or frequent");
                ts.frequency = idx;
            }
            if (t.has("disabled")) {
                for (const auto& key : detail::stringList(t.get("disabled"), t.at("disabled"))) {
                    int idx = -1;
                    for (int i = 0; i < ThreatKinds; ++i) if (key == threats::Defs[i].key) idx = i;
                    if (idx < 0) bad(t.at("disabled"), "unknown threat actor \"" + key + "\"");
                    ts.disabledMask |= 1 << idx;
                }
            }
            t.done();
            p.threats = ts;
        }
        o.done();
    }
    p.needs = detail::stringList(root.get("needs"), "preset.needs");
    root.done();
    return p;
}

// Defined in Simulation.cpp from ContentPresets.generated.h: the shipped overlay and preset files, as text.
const std::vector<std::string>& embeddedOverlayFiles();
const std::vector<std::string>& embeddedPresetFiles();

namespace detail {
struct Registry {
    std::map<std::string, std::string> overlays;  // id -> exact bytes (map nodes are stable, so views stay valid)
    std::vector<Preset> presets;
    std::size_t embeddedOverlays = 0, embeddedPresets = 0;
};
inline void addOverlay(Registry& r, std::string bytes) {
    const auto ov = content::detail::parseOverlay(bytes);
    const auto at = r.overlays.find(ov.id);
    if (at != r.overlays.end()) {
        if (at->second == bytes) return;
        content::detail::bad("overlay " + ov.id, "already registered with different bytes");
    }
    r.overlays.emplace(ov.id, std::move(bytes));
}
inline void addPreset(Registry& r, std::string_view bytes) {
    Preset p = parsePreset(bytes);
    for (const auto& have : r.presets) if (have.id == p.id) content::detail::bad("preset " + p.id, "already registered");
    for (const auto& id : p.overlays)
        if (!r.overlays.count(id)) content::detail::bad("preset " + p.id, "overlay " + id + " is not registered");
    r.presets.push_back(std::move(p));
}
inline Registry& registry() {
    static Registry r = [] {
        Registry fresh;
        for (const auto& text : embeddedOverlayFiles()) addOverlay(fresh, text);
        for (const auto& text : embeddedPresetFiles()) addPreset(fresh, text);
        fresh.embeddedOverlays = fresh.overlays.size();
        fresh.embeddedPresets = fresh.presets.size();
        return fresh;
    }();
    return r;
}
} // namespace detail

// Host registration of a user overlay or preset (bytes the host read). Atomic: a bad file registers nothing.
inline std::string registerOverlay(std::string_view bytes) {
    auto& r = detail::registry();
    const auto ov = content::detail::parseOverlay(bytes);
    detail::addOverlay(r, std::string(bytes));
    return ov.id;
}
inline std::string registerPreset(std::string_view bytes) {
    auto& r = detail::registry();
    detail::addPreset(r, bytes);
    return r.presets.back().id;
}

// Every preset, in screen order (order, then id).
inline std::vector<const Preset*> all() {
    std::vector<const Preset*> out;
    for (const auto& p : detail::registry().presets) out.push_back(&p);
    std::stable_sort(out.begin(), out.end(), [](const Preset* a, const Preset* b) { return a->order != b->order ? a->order < b->order : a->id < b->id; });
    return out;
}
inline const Preset* find(const std::string& id) {
    for (const auto& p : detail::registry().presets) if (p.id == id) return &p;
    return nullptr;
}
inline const std::string* overlayBytes(const std::string& id) {
    const auto& o = detail::registry().overlays;
    const auto at = o.find(id);
    return at == o.end() ? nullptr : &at->second;
}

// Makes the preset's content the active pack: the base pack with its overlays. A preset without overlays leaves a plain
// (base or host-loaded) pack alone and drops a stack back to base, so choosing Classic after Dominion War restores base.
inline const content::PackInfo& activate(const Preset& p) {
    if (p.overlays.empty()) return content::active().stack.empty() ? content::active() : content::installBase();
    std::vector<std::string_view> bytes;
    const auto& now = content::active();
    bool same = now.stack.size() == p.overlays.size();
    for (std::size_t i = 0; i < p.overlays.size(); ++i) {
        bytes.push_back(*overlayBytes(p.overlays[i]));
        same = same && now.stack[i].id == p.overlays[i] && now.stack[i].checksum == content::fnv1a64(bytes.back());
    }
    return same ? now : content::installStack(bytes);  // The stack that is already active is kept: no rebuild per galaxy preview.
}

// Makes the stack a save names the active pack. Each overlay must be registered and byte-identical to the one the save
// was made with; a missing or changed overlay is an error that names it.
inline void ensureStack(const std::vector<content::PartRef>& refs) {
    std::vector<std::string_view> bytes;
    for (const auto& ref : refs) {
        const std::string* b = overlayBytes(ref.id);
        if (!b) content::detail::bad("save", "needs overlay " + ref.id + ", which is not installed");
        if (content::fnv1a64(*b) != ref.checksum)
            content::detail::bad("save", "overlay " + ref.id + " changed since the save (" + content::checksumHex(ref.checksum) + " then, " + content::checksumHex(content::fnv1a64(*b)) + " now)");
        bytes.push_back(*b);
    }
    content::installStack(bytes);
}

} // namespace botf::presets
