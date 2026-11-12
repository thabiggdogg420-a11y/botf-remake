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
#include <array>
#include <map>
#include <cstdio>
#include <optional>
#include <set>
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
    "expandedRoster", "flightModel", "wegoCombat", "empireRules", "eras",
    // Schema 23 (EconomyRules.h): the economy and rules mechanics.
    "structureUpgrades", "autoUpgrades", "noUpgrades", "sharedMorale", "protectorateHq", "allDilithium", "interceptOrders",
    "raidOrders", "raidBonus", "crewExperience", "exploitFixes"};
inline constexpr const char* SizeIds[] = {"small", "medium", "large", "huge"};
inline constexpr const char* ShapeIds[] = {"legacy", "irregular", "spiral", "ring", "elliptical"};
inline constexpr const char* DensityIds[] = {"legacy", "none", "few", "some", "many"};
inline constexpr const char* DifficultyIds[] = {"simple", "easy", "normal", "hard", "expert", "impossible"};

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
    int startSystems = -1, planetsMin = 0, planetsMax = 0, starTypes = -1; // Schema 24: extra start colonies, planets per star, race star types.
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
        bool v = (remastered && std::string(name) != "eras") || std::string(name) == "stations" || std::string(name) == "empireRules";
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
    if (value("eras") && !value("expandedRoster")) content::detail::bad(path, "eras needs expandedRoster");
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
        if (o.has("startSystems")) p.startSystems = o.i32("startSystems", 0, 8);
        if (o.has("planetsMin")) p.planetsMin = o.i32("planetsMin", 1, 9);
        if (o.has("planetsMax")) p.planetsMax = o.i32("planetsMax", 1, 9);
        if ((p.planetsMin > 0) != (p.planetsMax > 0) || p.planetsMin > p.planetsMax) bad(o.at("planetsMin"), "planetsMin and planetsMax come as a pair, minimum first");
        if (o.has("starTypes")) p.starTypes = o.flag("starTypes") ? 1 : 0;
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
    std::set<std::string> embeddedOverlayIds;      // The shipped overlays: a user pack never replaces one.
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
        for (const auto& kv : fresh.overlays) fresh.embeddedOverlayIds.insert(kv.first);
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

// ---- User packs (lane MOD-10, docs/design/MOD-TOOLS.md) -------------------------------------------------------------
// A user pack is one overlay plus one preset the host read from the player's data folder. Unlike registerOverlay (which refuses
// a same-id overlay with other bytes), a user pack may be replaced by its author: the editor saves, the registry takes the new
// bytes, and a save made on the old bytes is refused by its checksum (ensureStack). A shipped overlay or preset is never
// replaced. Nothing here touches the rules or the campaign RNG.
namespace detail {
inline void writeNumber(const content::json::Value& v, std::string& out) {
    std::string digits = std::to_string(v.mantissa);
    if (v.scale > 0) {
        while (digits.size() <= static_cast<std::size_t>(v.scale)) digits.insert(digits.begin(), '0');
        digits.insert(digits.size() - static_cast<std::size_t>(v.scale), 1, '.');
    }
    if (v.negative) out += '-';
    out += digits;
}
inline void writeString(const std::string& text, std::string& out) {
    out += '"';
    for (unsigned char c : text) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
        else out += static_cast<char>(c);
    }
    out += '"';
}
inline void writeValue(const content::json::Value& v, std::string& out) {
    using K = content::json::Value::Kind;
    switch (v.kind) {
        case K::Null: out += "null"; break;
        case K::Bool: out += v.boolean ? "true" : "false"; break;
        case K::Number: writeNumber(v, out); break;
        case K::String: writeString(v.text, out); break;
        case K::Array:
            out += '[';
            for (std::size_t i = 0; i < v.items.size(); ++i) { if (i) out += ','; writeValue(v.items[i], out); }
            out += ']';
            break;
        case K::Object:
            out += '{';
            for (std::size_t i = 0; i < v.members.size(); ++i) {
                if (i) out += ',';
                writeString(v.members[i].first, out);
                out += ':';
                writeValue(v.members[i].second, out);
            }
            out += '}';
            break;
    }
}
} // namespace detail

// The merged JSON of one top-level pack key ("roster", "buildings", "balance", "eras" ...) under the base pack plus the
// registered overlays `under`, in order: what a pack editor shows as the current value. "null" when there is no such key.
inline std::string packPart(const std::vector<std::string>& under, const std::string& key) {
    auto doc = content::json::Parser(content::embeddedBaseBundle()).document();
    for (const auto& id : under) {
        const std::string* bytes = overlayBytes(id);
        if (!bytes) content::detail::bad("under", "overlay " + id + " is not registered");
        content::detail::mergeOverlay(doc, content::detail::parseOverlay(*bytes));
    }
    for (const auto& m : doc.members) {
        if (m.first != key) continue;
        std::string out;
        detail::writeValue(m.second, out);
        return out;
    }
    return "null";
}

struct UserPackCheck { std::string id, stackId, checksum; };
// Everything a user pack must satisfy to register: the overlay parses and merges over the base and the preset's other overlays
// exactly as at game start (so the typed parse reports a bad record), the preset parses (flags, lineup, options), names the
// overlay last in its stack, and an eras flag has an eras block under it. Throws the core's own message; registers nothing.
inline UserPackCheck validateUserPack(std::string_view ovBytes, std::string_view presetBytes) {
    using content::detail::bad;
    const auto& r = detail::registry();
    const auto ov = content::detail::parseOverlay(ovBytes);
    if (r.embeddedOverlayIds.count(ov.id)) bad("overlay id", "\"" + ov.id + "\" is a shipped pack and cannot be replaced; save the pack under another id");
    const Preset p = parsePreset(presetBytes);
    for (std::size_t i = 0; i < r.embeddedPresets && i < r.presets.size(); ++i)
        if (r.presets[i].id == p.id) bad("preset " + p.id, "is a shipped preset and cannot be replaced; save the pack under another id");
    if (p.overlays.empty() || p.overlays.back() != ov.id) bad("preset " + p.id, "must list the pack's overlay \"" + ov.id + "\" last in its overlays");
    std::vector<std::string_view> stack;
    for (const auto& id : p.overlays) {
        if (id == ov.id) { stack.push_back(ovBytes); continue; }
        const std::string* b = overlayBytes(id);
        if (!b) bad("preset " + p.id, "overlay " + id + " is not registered");
        stack.push_back(*b);
    }
    const auto pack = content::detail::buildStack(content::embeddedBaseBundle(), content::baseChecksum(), stack);
    bool erasOn = false;
    for (const auto& f : p.flags) if (f.first == "eras") erasOn = f.second;
    if (erasOn && !pack->eras.present) bad("preset " + p.id, "the eras flag needs an overlay with an eras block in the stack");
    return {ov.id, pack->info.id, content::checksumHex(pack->info.checksum)};
}

// Validates, then registers (or replaces) the user pack's overlay and preset. Atomic: a refusal changes nothing.
inline std::string installUserPack(std::string_view ovBytes, std::string_view presetBytes) {
    const auto check = validateUserPack(ovBytes, presetBytes);
    auto& r = detail::registry();
    Preset p = parsePreset(presetBytes);
    r.overlays[check.id] = std::string(ovBytes);
    for (auto& have : r.presets) if (have.id == p.id) { have = std::move(p); return have.id; }
    r.presets.push_back(std::move(p));
    return r.presets.back().id;
}

// Removes a user pack: its preset and its overlay (never a shipped one). False when nothing user-made had that id.
inline bool removeUserPack(const std::string& id) {
    auto& r = detail::registry();
    bool removed = false;
    for (std::size_t i = r.embeddedPresets; i < r.presets.size(); ++i)
        if (r.presets[i].id == id) { r.presets.erase(r.presets.begin() + static_cast<std::ptrdiff_t>(i)); removed = true; break; }
    if (!r.embeddedOverlayIds.count(id) && r.overlays.count(id)) {
        for (const auto& p : r.presets) for (const auto& o : p.overlays) if (o == id) return removed;  // another preset still lists it
        r.overlays.erase(id);
        removed = true;
    }
    return removed;
}

inline bool isUserPreset(const std::string& id) {
    const auto& r = detail::registry();
    for (std::size_t i = r.embeddedPresets; i < r.presets.size(); ++i) if (r.presets[i].id == id) return true;
    return false;
}

struct UserPackEntry { std::string id; std::uint64_t checksum = 0; bool hasPreset = false; };
// The overlays a host registered beyond the shipped ones, with the checksum of their exact bytes.
inline std::vector<UserPackEntry> userPacks() {
    const auto& r = detail::registry();
    std::vector<UserPackEntry> out;
    for (const auto& kv : r.overlays) {
        if (r.embeddedOverlayIds.count(kv.first)) continue;
        UserPackEntry e{kv.first, content::fnv1a64(kv.second), false};
        for (std::size_t i = r.embeddedPresets; i < r.presets.size(); ++i) e.hasPreset = e.hasPreset || r.presets[i].id == kv.first;
        out.push_back(e);
    }
    return out;
}

// Schema 22: what a preset's overlay stack says about eras (docs/design/ERAS.md), for the new-game screen. Builds the stack
// without activating it; nothing is cached because only the picker asks, once per meta call.
struct EraSummary {
    bool present = false;
    int techCap = 10;
    std::array<int, 5> years{}, startTech{};
};
inline EraSummary erasOf(const Preset& p) {
    EraSummary out;
    if (p.overlays.empty()) return out;
    std::vector<std::string_view> bytes;
    for (const auto& id : p.overlays) bytes.push_back(*overlayBytes(id));
    const auto pack = content::detail::buildStack(content::embeddedBaseBundle(), content::baseChecksum(), bytes);
    if (!pack->eras.present) return out;
    out.present = true; out.techCap = pack->eras.techCap; out.years = pack->eras.years; out.startTech = pack->eras.startTech;
    return out;
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
