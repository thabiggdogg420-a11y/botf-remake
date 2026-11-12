// BOTF Remastered content packs (docs/DATA-FORMAT.md).
//
// Ships, structures, technologies, races, minor races and galactic-event text are data: a JSON bundle the host hands
// to the core as bytes (content::install). The core never opens a file (Browser target, M0-LANES.md). When no pack is
// installed, the first table access installs the base pack embedded from data/base.pack.json
// (ContentBase.generated.h, compiled into Simulation.cpp only).
//
// The tables keep their old names and shape (ReferenceCatalogue.h, ShipRoster.h, MinorRaces.h): Table<T> supports
// range-for, operator[], size(), front() and back(), so consumers did not change.
#pragma once
#include "ShipRosterTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace botf::referenceData {
struct TechnologyRecord { int id; const char* name; int field; int level; std::int64_t cost; const char* description; };
struct ShipRecord { int id; int raceId; const char* name; const char* shipClass; std::array<int, 6> technologyRequirements; int industryCost; int maintenance; int hull; int shields; int attack; double range; double speed; int role; int troopCapacity; const char* description; };
// excludedEmpireMask: bit N set = race N (RaceCatalogue id) cannot use the structure (64 bits since MOD-3; the vanilla records use the low five).
// buildable..perEmpireLimit are the MOD-3 campaign fields (docs/design/DOMINION.md); a vanilla record has none of them and stays reference data.
struct BuildingRecord { int id; const char* name; int technologyField; int technologyLevel; int industryCost; int energyCost; int populationCost; int moraleBonus; std::uint64_t excludedEmpireMask; std::uint64_t residentRaceMask; int bonusType; std::array<int, 6> bonusValues; std::array<int, 6> unrestBonuses; const char* description;
    bool buildable = false;        // A campaign structure: a colony may queue it (structures::).
    bool startsBuilt = false;      // Built in the capital of every empire that may use it, at turn 0.
    int requiresId = -1;             // Record id that must stand in the same colony first (the ladder), -1 none.
    int perEmpireLimit = 0;        // Most the empire may hold (built or queued), 0 unlimited.
};
// A race with empireId 1..8 is a playable major (docs/design/EMPIRE-SLOTS.md); every other record is portrait/icon data only.
struct RaceRecord {
    int id; const char* name; const char* icon;
    int empireId = 0;                 // Campaign slot 1..8, 0 = not a major.
    const char* key = "";             // Stable race key: "federation", "dominion", ...
    const char* empireName = "";      // "Klingon Empire".
    const char* capital = "";         // Home world name given to the empire's first colony.
    const char* traits = "";          // '|' separated display traits.
    int shipDonor = -1;               // Catalogue race whose utility and escort hulls this race may also build, -1 none.
    std::array<int, 4> startingFleet{-1, -1, -1, -1}; // Expanded-roster class ids added to the home fleet (-1 unused).
    // MOD-3 trait effects (race `major.traitEffects`; all 0 in the base pack): morale added to the level the empire's colonies settle toward,
    // percent added to colony credit income, percent added to ground defence, percent added to internal-security output.
    int traitMorale = 0, traitCreditsPct = 0, traitGroundPct = 0, traitSecurityPct = 0;
};
} // namespace botf::referenceData

namespace botf::minors {
enum class Temperament : int { Peaceful, Scientific, Mercantile, Spiritual, Xenophobic, Warlike };
// What a minor contributes to the empire it joins. Percentages apply to every colony of the empire;
// Morale raises the morale level its colonies settle toward.
enum class JoinBonus : int { Research, Industry, Food, Credits, Morale, GroundDefense };

struct MinorRaceRecord {
    int id;                        // Stable roster id (1-based, never reused); saved as Minor::raceId.
    const char* name;              // Matches Minor::name.
    const char* homeworld;         // Name given to the home system on placement.
    int referenceRaceId;           // referenceData::RaceCatalogue id (portrait/icon), -1 if none.
    Temperament temperament;
    int techLevel;                 // 1..10 in an Early-era campaign; later eras add (era - 1).
    int population;                // Starting population, and the colony size it brings on joining.
    JoinBonus bonus;
    int bonusValue;                // Percent, or morale points for JoinBonus::Morale.
    int signatureShipId;           // referenceData::ShipCatalogue id of its own design, -1 if none.
    std::array<int, 9> affinity;   // By major slot 1..8 (1 Fed, 2 Klingon, 3 Romulan, 4 Cardassian, 5 Ferengi, 6 Dominion, 7 Borg, 8 Breen); -2..+2. Data gives slots 0..5; the rest load as 0.
    const char* description;       // Original remaster text.
    // MOD-3 placement fields (optional in data): an unavailable minor is never placed (a pack replaces Edo by Bajor), a featured one is
    // placed first, and a minor with majorRaceId >= 0 is skipped while that race is seated as a major (Breen minor, Breen major).
    bool available = true;
    bool featured = false;
    int majorRaceId = -1;
};
} // namespace botf::minors

namespace botf::content {

inline constexpr int FormatVersion = 1;
inline constexpr int EventKinds = 7;          // GalacticEventKind count (Simulation.h).
inline constexpr int LegacyShipLimit = 1000;  // roster::ExpandedIdBase.
inline constexpr int ExpandedIdBase = 1000;
inline constexpr int ExpandedIdLimit = 2000;

struct EventChoiceText { std::string label, effect; };
struct EventDefinition {
    std::string name;
    int defaultChoice = -1;
    std::vector<EventChoiceText> choices;
    std::string description;  // Template: {where}, {magnitude}, {endTurn}.
};

// One overlay in a stack: its id and the FNV-1a 64 checksum of its exact bytes (docs/design/PACKS-AND-PRESETS.md).
struct PartRef { std::string id; std::uint64_t checksum = 0; };

struct PackInfo {
    std::string id, name;
    int format = 0, revision = 0;
    std::uint64_t checksum = 0;
    std::size_t bytes = 0;
    bool base = false;        // Byte-identical to the embedded base pack.
    std::vector<PartRef> stack; // Overlays applied over the base pack, in order; empty for a plain pack.
};

// Pack-wide balance values (the optional "balance" object). Percentages of the base figure; 100 changes nothing.
struct Balance {
    int researchCostPercent = 100;  // Every research cost (Reference ruleset): cost * percent / 100.
    int shipCostPercent = 100;      // Every ship's industry cost in a build queue.
    bool operator==(const Balance&) const = default;
};

// Schema 22 (docs/design/ERAS.md): the optional "eras" object. A pack that carries it defines the campaign timeline, the tech
// level each start era begins at, the highest tech level (10..14) and the research data for levels 11 to 14, and gives roster
// classes their years in service. Absent in the base pack, so base bytes and checksum are unchanged.
struct EraWindow { int id = 0, from = 0, until = 0; };   // Years in service for one roster class; 0 = no bound.
struct Eras {
    bool present = false;
    int turnsPerYear = 4;                                  // Campaign turns per timeline year.
    std::array<int, 5> years{2240, 2280, 2320, 2350, 2380};    // Start year of each StartingEra (Early..Supreme).
    std::array<int, 5> startTech{1, 3, 5, 7, 10};          // Tech level every field starts at, per StartingEra.
    int techCap = 10;                                      // Highest research level (10..14).
    std::array<int, 8> tagYears{2150, 2245, 2270, 2350, 2370, 2379, 2380, 2400}; // First year of service per roster era tag (EraNames order).
    std::vector<EraWindow> windows;                        // Per-class overrides of the tag year, with an optional retirement year.
    std::vector<referenceData::TechnologyRecord> highTech; // Levels 11..14 of the six fields: index = field * 4 + (level - 11).
};

struct Pack {
    PackInfo info;
    std::deque<std::string> strings; // Owns every const char* in the records (deque: stable addresses).
    std::vector<referenceData::TechnologyRecord> technologies;
    std::vector<referenceData::ShipRecord> ships;
    std::vector<referenceData::BuildingRecord> buildings;
    std::vector<referenceData::RaceRecord> races;
    std::vector<roster::RosterClass> roster;
    std::vector<referenceData::ShipRecord> rosterShips;   // ShipRecord view of roster, same order.
    std::vector<int> rosterIndex;                         // id - ExpandedIdBase -> roster index, -1 unused.
    std::vector<minors::MinorRaceRecord> minors;
    std::vector<EventDefinition> events;
    Balance balance;
    Eras eras;
    const char* keep(std::string s) { strings.push_back(std::move(s)); return strings.back().c_str(); }
};

// The base bundle, assembled from ContentBase.generated.h (defined in Simulation.cpp).
std::string_view embeddedBaseBundle();

inline std::uint64_t fnv1a64(std::string_view bytes) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : bytes) { hash ^= byte; hash *= 1099511628211ULL; }
    return hash;
}
inline std::string checksumHex(std::uint64_t value) {
    static const char* digits = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) { out[static_cast<std::size_t>(i)] = digits[value & 15]; value >>= 4; }
    return out;
}

// ---- JSON reader: exact integers, decimals without strtod or locale (docs/DATA-FORMAT.md section 5) ----
namespace json {
struct Value {
    enum class Kind { Null, Bool, Number, String, Array, Object } kind = Kind::Null;
    bool boolean = false;
    bool negative = false;
    std::uint64_t mantissa = 0;
    int scale = 0;            // Digits after the decimal point.
    std::string text;
    std::vector<Value> items;
    std::vector<std::pair<std::string, Value>> members;
};

class Parser {
public:
    explicit Parser(std::string_view s) : s_(s) {}
    Value document() {
        Value v = value(0);
        ws();
        if (p_ != s_.size()) fail("trailing data after the document");
        return v;
    }
private:
    std::string_view s_;
    std::size_t p_ = 0;
    [[noreturn]] void fail(const std::string& what) const {
        std::size_t line = 1;
        for (std::size_t i = 0; i < p_ && i < s_.size(); ++i) if (s_[i] == '\n') ++line;
        throw std::runtime_error("content: JSON line " + std::to_string(line) + ": " + what);
    }
    void ws() { while (p_ < s_.size() && (s_[p_] == ' ' || s_[p_] == '\n' || s_[p_] == '\r' || s_[p_] == '\t')) ++p_; }
    bool eat(char c) { ws(); if (p_ < s_.size() && s_[p_] == c) { ++p_; return true; } return false; }
    void expect(char c) { if (!eat(c)) fail(std::string("expected '") + c + "'"); }
    bool word(std::string_view w) { if (s_.substr(p_, w.size()) == w) { p_ += w.size(); return true; } return false; }
    Value value(int depth) {
        if (depth > 32) fail("nesting too deep");
        ws();
        if (p_ >= s_.size()) fail("unexpected end of data");
        Value v;
        const char c = s_[p_];
        if (c == '{') {
            ++p_; v.kind = Value::Kind::Object;
            if (eat('}')) return v;
            do {
                ws();
                if (p_ >= s_.size() || s_[p_] != '"') fail("expected a key");
                std::string key = string();
                for (const auto& m : v.members) if (m.first == key) fail("duplicate key \"" + key + "\"");
                expect(':');
                v.members.emplace_back(std::move(key), value(depth + 1));
            } while (eat(','));
            expect('}');
        } else if (c == '[') {
            ++p_; v.kind = Value::Kind::Array;
            if (eat(']')) return v;
            do v.items.push_back(value(depth + 1)); while (eat(','));
            expect(']');
        } else if (c == '"') {
            v.kind = Value::Kind::String; v.text = string();
        } else if (word("true")) { v.kind = Value::Kind::Bool; v.boolean = true; }
        else if (word("false")) { v.kind = Value::Kind::Bool; v.boolean = false; }
        else if (word("null")) { v.kind = Value::Kind::Null; }
        else if (c == '-' || (c >= '0' && c <= '9')) number(v);
        else fail(std::string("unexpected character '") + c + "'");
        return v;
    }
    void number(Value& v) {
        v.kind = Value::Kind::Number;
        if (s_[p_] == '-') { v.negative = true; ++p_; }
        const std::size_t start = p_;
        auto digit = [&](bool fraction) {
            const auto d = static_cast<std::uint64_t>(s_[p_] - '0');
            if (v.mantissa > (UINT64_MAX - d) / 10) fail("number out of range");
            v.mantissa = v.mantissa * 10 + d; ++p_;
            if (fraction) ++v.scale;
        };
        while (p_ < s_.size() && s_[p_] >= '0' && s_[p_] <= '9') digit(false);
        if (p_ == start) fail("malformed number");
        if (p_ - start > 1 && s_[start] == '0') fail("leading zero in number");
        if (p_ < s_.size() && s_[p_] == '.') {
            ++p_;
            const std::size_t fs = p_;
            while (p_ < s_.size() && s_[p_] >= '0' && s_[p_] <= '9') digit(true);
            if (p_ == fs) fail("malformed number");
        }
        if (p_ < s_.size() && (s_[p_] == 'e' || s_[p_] == 'E')) fail("exponents are not allowed in content numbers");
    }
    static void utf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    }
    std::uint32_t hex4() {
        if (p_ + 4 > s_.size()) fail("short \\u escape");
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            const char h = s_[p_++];
            v <<= 4;
            if (h >= '0' && h <= '9') v |= static_cast<std::uint32_t>(h - '0');
            else if (h >= 'a' && h <= 'f') v |= static_cast<std::uint32_t>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') v |= static_cast<std::uint32_t>(h - 'A' + 10);
            else fail("bad \\u escape");
        }
        return v;
    }
    std::string string() {
        ++p_; // opening quote
        std::string out;
        for (;;) {
            if (p_ >= s_.size()) fail("unterminated string");
            const char c = s_[p_++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') { out += c; continue; }
            if (p_ >= s_.size()) fail("unterminated escape");
            const char e = s_[p_++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                std::uint32_t cp = hex4();
                if (cp >= 0xD800 && cp < 0xDC00) {
                    if (!word("\\u")) fail("unpaired surrogate");
                    const std::uint32_t lo = hex4();
                    if (lo < 0xDC00 || lo >= 0xE000) fail("unpaired surrogate");
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                } else if (cp >= 0xDC00 && cp < 0xE000) fail("unpaired surrogate");
                utf8(out, cp);
                break;
            }
            default: fail(std::string("bad escape \\") + e);
            }
        }
    }
};
} // namespace json

// ---- Typed record reader with path-carrying errors ----
namespace detail {
[[noreturn]] inline void bad(const std::string& path, const std::string& what) {
    throw std::runtime_error("content: " + path + ": " + what);
}
inline const char* kindName(json::Value::Kind k) {
    switch (k) {
    case json::Value::Kind::Null: return "null";
    case json::Value::Kind::Bool: return "boolean";
    case json::Value::Kind::Number: return "number";
    case json::Value::Kind::String: return "string";
    case json::Value::Kind::Array: return "array";
    case json::Value::Kind::Object: return "object";
    }
    return "?";
}
inline std::int64_t integer(const json::Value& v, const std::string& path, std::int64_t lo, std::int64_t hi) {
    if (v.kind != json::Value::Kind::Number || v.scale != 0) bad(path, std::string("expected an integer, got ") + kindName(v.kind));
    if (v.mantissa > static_cast<std::uint64_t>(INT64_MAX)) bad(path, "integer out of range");
    const auto value = v.negative ? -static_cast<std::int64_t>(v.mantissa) : static_cast<std::int64_t>(v.mantissa);
    if (value < lo || value > hi) bad(path, std::to_string(value) + " outside " + std::to_string(lo) + ".." + std::to_string(hi));
    return value;
}
inline std::uint64_t unsignedInteger(const json::Value& v, const std::string& path) {
    if (v.kind != json::Value::Kind::Number || v.scale != 0 || v.negative) bad(path, "expected an unsigned integer");
    return v.mantissa;
}
inline double decimal(const json::Value& v, const std::string& path, double lo, double hi) {
    static constexpr double Pow10[] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15};
    if (v.kind != json::Value::Kind::Number) bad(path, std::string("expected a number, got ") + kindName(v.kind));
    if (v.scale > 15 || v.mantissa > 999999999999999ULL) bad(path, "decimal needs at most 15 digits");
    // Both operands are exact doubles, so one IEEE division gives the same correctly rounded value everywhere.
    double value = static_cast<double>(v.mantissa) / Pow10[v.scale];
    if (v.negative) value = -value;
    if (!(value >= lo && value <= hi)) bad(path, "decimal out of range");
    return value;
}

class Object {
public:
    Object(const json::Value& v, std::string path) : v_(v), path_(std::move(path)) {
        if (v.kind != json::Value::Kind::Object) bad(path_, std::string("expected an object, got ") + kindName(v.kind));
        used_.assign(v.members.size(), false);
    }
    const json::Value& get(const char* key) {
        for (std::size_t i = 0; i < v_.members.size(); ++i)
            if (v_.members[i].first == key) { used_[i] = true; return v_.members[i].second; }
        bad(path_, std::string("missing key \"") + key + "\"");
    }
    bool has(const char* key) const {
        for (const auto& m : v_.members) if (m.first == key) return true;
        return false;
    }
    std::string at(const char* key) const { return path_ + "." + key; }
    int i32(const char* key, int lo, int hi) { return static_cast<int>(integer(get(key), at(key), lo, hi)); }
    std::int64_t i64(const char* key, std::int64_t lo, std::int64_t hi) { return integer(get(key), at(key), lo, hi); }
    std::uint64_t u64(const char* key) { return unsignedInteger(get(key), at(key)); }
    double dec(const char* key, double lo, double hi) { return decimal(get(key), at(key), lo, hi); }
    bool flag(const char* key) {
        const auto& v = get(key);
        if (v.kind != json::Value::Kind::Bool) bad(at(key), "expected true or false");
        return v.boolean;
    }
    std::string str(const char* key, std::size_t maxBytes = 4096) {
        const auto& v = get(key);
        if (v.kind != json::Value::Kind::String) bad(at(key), std::string("expected a string, got ") + kindName(v.kind));
        if (v.text.size() > maxBytes) bad(at(key), "string too long");
        if (v.text.find('\0') != std::string::npos) bad(at(key), "NUL in string");
        return v.text;
    }
    std::array<int, 6> six(const char* key, int lo, int hi) {
        const auto& v = get(key);
        if (v.kind != json::Value::Kind::Array || v.items.size() != 6) bad(at(key), "expected an array of 6 integers");
        std::array<int, 6> out{};
        for (std::size_t i = 0; i < 6; ++i) out[i] = static_cast<int>(integer(v.items[i], at(key) + "[" + std::to_string(i) + "]", lo, hi));
        return out;
    }
    template <std::size_t N> int enumeration(const char* key, const char* const (&names)[N], const char* typeName) {
        const std::string s = str(key, 64);
        for (std::size_t i = 0; i < N; ++i) if (s == names[i]) return static_cast<int>(i);
        bad(at(key), std::string("unknown ") + typeName + " \"" + s + "\"");
    }
    const json::Value& array(const char* key) {
        const auto& v = get(key);
        if (v.kind != json::Value::Kind::Array) bad(at(key), "expected an array");
        return v;
    }
    void done() const {
        for (std::size_t i = 0; i < used_.size(); ++i) if (!used_[i]) bad(path_, "unknown key \"" + v_.members[i].first + "\"");
    }
private:
    const json::Value& v_;
    std::string path_;
    std::vector<bool> used_;
};

// Enumerator names, in enum order (ShipRosterTypes.h, this file). Data names enums by these strings.
inline constexpr const char* FactionNames[] = {"Federation", "Klingon", "Romulan", "Cardassian", "Ferengi", "Dominion",
    "Borg", "Breen", "Tholian", "Gorn", "Orion", "Hirogen", "Kazon", "Vidiian", "Suliban", "Xindi", "Species8472",
    "Krenim", "Malon", "SonA", "Maquis", "Reman", "Devore", "Andorian", "Vulcan", "Bajoran", "Nausicaan", "Talarian"};
inline constexpr const char* HullRoleNames[] = {"Scout", "Science", "Escort", "Frigate", "Destroyer", "Raider",
    "LightCruiser", "Cruiser", "HeavyCruiser", "StrikeCruiser", "Carrier", "Battlecruiser", "Explorer", "Battleship",
    "Dreadnought", "Colony", "Transport", "Station", "Special"};
inline constexpr const char* EraNames[] = {"Enterprise", "Original", "Movie", "NextGeneration", "DominionWar", "Nemesis", "Kelvin", "Future"};
inline constexpr const char* SourceKindNames[] = {"Canon", "ModDerived", "Kelvin"};
inline constexpr const char* WeaponTypeNames[] = {"None", "PhaseCannon", "PhaserBank", "PhaserArray", "PulsePhaser",
    "Disruptor", "DisruptorCannon", "PlasmaCannon", "PolaronBeam", "CuttingBeam", "EnergyDampener", "TholianWeb",
    "BioEnergy", "TemporalWeapon", "SpatialTorpedo", "PhotonicTorpedo", "PhotonTorpedo", "QuantumTorpedo",
    "PlasmaTorpedo", "TricobaltDevice", "Torpedo"};
inline constexpr const char* FiringArcNames[] = {"Forward", "ForwardWide", "Broadside", "Aft", "All"};
inline constexpr const char* SizeClassNames[] = {"Small", "Medium", "Large", "Huge", "Station"};
inline constexpr const char* TemperamentNames[] = {"Peaceful", "Scientific", "Mercantile", "Spiritual", "Xenophobic", "Warlike"};
inline constexpr const char* JoinBonusNames[] = {"Research", "Industry", "Food", "Credits", "Morale", "GroundDefense"};
inline constexpr const char* EventKindNames[] = {"Plague", "SupernovaWarning", "SubspaceRift", "TradeBoom",
    "DerelictVessel", "LaborUnrest", "ScientificBreakthrough"};
inline constexpr bool EventHasChoice[] = {true, true, false, false, true, true, true};

constexpr int Big = 1000000000;

inline const json::Value& section(Object& root, const char* key, std::size_t minCount, std::size_t maxCount) {
    const auto& a = root.array(key);
    if (a.items.size() < minCount || a.items.size() > maxCount)
        bad(key, std::to_string(a.items.size()) + " records, expected " + std::to_string(minCount) + ".." + std::to_string(maxCount));
    return a;
}

// Validates a parsed bundle document (a base bundle, or a base merged with overlays) into typed tables. The caller sets
// info.checksum and info.bytes.
inline std::unique_ptr<Pack> parsePackValue(const json::Value& doc) {
    auto pack = std::make_unique<Pack>();
    Pack& p = *pack;
    Object root(doc, "pack");
    p.info.format = root.i32("format", 0, Big);
    if (p.info.format != FormatVersion) bad("pack.format", "format " + std::to_string(p.info.format) + " is not supported (expected " + std::to_string(FormatVersion) + ")");
    p.info.id = root.str("id", 40);
    if (p.info.id.empty()) bad("pack.id", "empty id");
    for (char c : p.info.id)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) bad("pack.id", "id must be [a-z0-9-]");
    p.info.name = root.str("name", 200);
    p.info.revision = root.i32("revision", 0, Big);

    {   const auto& a = section(root, "technologies", 77, 77);
        for (std::size_t i = 0; i < a.items.size(); ++i) {
            Object o(a.items[i], "technologies[" + std::to_string(i) + "]");
            referenceData::TechnologyRecord r{};
            r.id = o.i32("id", 0, Big);
            r.name = p.keep(o.str("name"));
            r.field = o.i32("field", 0, 6);
            r.level = o.i32("level", 0, 10);
            r.cost = o.i64("cost", 0, INT64_MAX / 4);
            r.description = p.keep(o.str("description"));
            o.done();
            if (r.id != static_cast<int>(i) || r.field != r.id / 11 || r.level != r.id % 11)
                bad(o.at("id"), "technology ids must equal their index, with field = id / 11 and level = id % 11");
            p.technologies.push_back(r);
        }
    }
    {   const auto& a = section(root, "ships", 1, LegacyShipLimit - 1);
        for (std::size_t i = 0; i < a.items.size(); ++i) {
            Object o(a.items[i], "ships[" + std::to_string(i) + "]");
            referenceData::ShipRecord r{};
            r.id = o.i32("id", 0, LegacyShipLimit - 1);
            r.raceId = o.i32("raceId", -1, 1000);
            r.name = p.keep(o.str("name"));
            r.shipClass = p.keep(o.str("shipClass"));
            r.technologyRequirements = o.six("technologyRequirements", 0, 64);
            r.industryCost = o.i32("industryCost", 0, Big);
            r.maintenance = o.i32("maintenance", 0, Big);
            r.hull = o.i32("hull", 0, Big);
            r.shields = o.i32("shields", 0, Big);
            r.attack = o.i32("attack", 0, Big);
            r.range = o.dec("range", 0, 100);
            r.speed = o.dec("speed", 0, 100);
            r.role = o.i32("role", 0, 64);
            r.troopCapacity = o.i32("troopCapacity", -1, Big);
            r.description = p.keep(o.str("description"));
            o.done();
            if (r.id != static_cast<int>(i)) bad(o.at("id"), "legacy ship ids must equal their index");
            p.ships.push_back(r);
        }
    }
    {   const auto& a = section(root, "buildings", 1, 100000);
        for (std::size_t i = 0; i < a.items.size(); ++i) {
            Object o(a.items[i], "buildings[" + std::to_string(i) + "]");
            referenceData::BuildingRecord r{};
            r.id = o.i32("id", 0, Big);
            r.name = p.keep(o.str("name"));
            r.technologyField = o.i32("technologyField", -1, 64);
            r.technologyLevel = o.i32("technologyLevel", -1, 64);
            r.industryCost = o.i32("industryCost", 0, Big);
            r.energyCost = o.i32("energyCost", -Big, Big);
            r.populationCost = o.i32("populationCost", -Big, Big);
            r.moraleBonus = o.i32("moraleBonus", -Big, Big);
            r.excludedEmpireMask = o.u64("excludedEmpireMask");
            r.residentRaceMask = o.u64("residentRaceMask");
            r.bonusType = o.i32("bonusType", -Big, Big);
            r.bonusValues = o.six("bonusValues", -Big, Big);
            r.unrestBonuses = o.six("unrestBonuses", -Big, Big);
            r.description = p.keep(o.str("description"));
            if (o.has("buildable")) r.buildable = o.flag("buildable");
            if (o.has("startsBuilt")) r.startsBuilt = o.flag("startsBuilt");
            if (o.has("requires")) r.requiresId = o.i32("requires", -1, Big);
            if (o.has("perEmpireLimit")) r.perEmpireLimit = o.i32("perEmpireLimit", 0, 1000);
            o.done();
            if (r.id != static_cast<int>(i)) bad(o.at("id"), "building ids must equal their index");
            if (r.requiresId >= r.id) bad(o.at("requires"), "a structure can only require an earlier record");
            if ((r.startsBuilt || r.requiresId >= 0 || r.perEmpireLimit > 0) && !r.buildable) bad(o.at("buildable"), "startsBuilt, requires and perEmpireLimit need buildable");
            if (r.startsBuilt && r.requiresId >= 0 && !p.buildings[static_cast<std::size_t>(r.requiresId)].startsBuilt) bad(o.at("startsBuilt"), "a start structure cannot require one that does not start built");
            p.buildings.push_back(r);
        }
    }
    {   const auto& a = section(root, "races", 1, 64);
        for (std::size_t i = 0; i < a.items.size(); ++i) {
            Object o(a.items[i], "races[" + std::to_string(i) + "]");
            referenceData::RaceRecord r{};
            r.id = o.i32("id", 0, 63);
            r.name = p.keep(o.str("name"));
            r.icon = p.keep(o.str("icon"));
            if (o.has("major")) {
                Object m(o.get("major"), o.at("major"));
                r.empireId = m.i32("empireId", 1, 8);
                r.key = p.keep(m.str("key", 32));
                r.empireName = p.keep(m.str("empireName", 64));
                r.capital = p.keep(m.str("capital", 64));
                r.shipDonor = m.i32("shipDonor", -1, 63);
                std::string traits;
                const auto& tv = m.array("traits");
                for (std::size_t k = 0; k < tv.items.size(); ++k) {
                    if (tv.items[k].kind != json::Value::Kind::String) bad(m.at("traits"), "expected strings");
                    traits += (k ? "|" : "") + tv.items[k].text;
                }
                r.traits = p.keep(traits);
                const auto& fv = m.array("startingFleet");
                if (fv.items.size() > r.startingFleet.size()) bad(m.at("startingFleet"), "at most 4 starting ships");
                for (std::size_t k = 0; k < fv.items.size(); ++k)
                    r.startingFleet[k] = static_cast<int>(integer(fv.items[k], m.at("startingFleet") + "[" + std::to_string(k) + "]", 1000, 1999));
                if (m.has("traitEffects")) {
                    Object te(m.get("traitEffects"), m.at("traitEffects"));
                    if (te.has("morale")) r.traitMorale = te.i32("morale", -30, 30);
                    if (te.has("creditsPct")) r.traitCreditsPct = te.i32("creditsPct", -50, 100);
                    if (te.has("groundDefensePct")) r.traitGroundPct = te.i32("groundDefensePct", -50, 200);
                    if (te.has("securityPct")) r.traitSecurityPct = te.i32("securityPct", -50, 200);
                    te.done();
                }
                m.done();
                if (r.key[0] == '\0') bad(m.at("key"), "empty race key");
                for (const auto& other : p.races) {
                    if (other.empireId == r.empireId) bad(m.at("empireId"), "duplicate empire slot");
                    if (other.empireId != 0 && std::string_view(other.key) == r.key) bad(m.at("key"), "duplicate race key");
                }
            }
            o.done();
            if (r.id != static_cast<int>(i)) bad(o.at("id"), "race ids must equal their index");
            p.races.push_back(r);
        }
    }
    {   const auto& a = section(root, "roster", 0, static_cast<std::size_t>(ExpandedIdLimit - ExpandedIdBase));
        p.rosterIndex.assign(static_cast<std::size_t>(ExpandedIdLimit - ExpandedIdBase), -1);
        int previous = ExpandedIdBase - 1;
        for (std::size_t i = 0; i < a.items.size(); ++i) {
            const std::string path = "roster[" + std::to_string(i) + "]";
            Object o(a.items[i], path);
            roster::RosterClass c{};
            c.id = o.i32("id", ExpandedIdBase, ExpandedIdLimit - 1);
            if (c.id <= previous) bad(o.at("id"), "roster ids must be strictly ascending");
            previous = c.id;
            c.assetKey = p.keep(o.str("assetKey", 128));
            c.faction = static_cast<roster::Faction>(o.enumeration("faction", FactionNames, "Faction"));
            c.raceId = o.i32("raceId", -1, 1000);
            c.minorRaceId = o.i32("minorRaceId", 0, 1000);
            c.roleName = p.keep(o.str("roleName"));
            c.className = p.keep(o.str("className"));
            c.hullRole = static_cast<roster::HullRole>(o.enumeration("hullRole", HullRoleNames, "HullRole"));
            c.engineRole = o.i32("engineRole", 0, 64);
            c.era = static_cast<roster::Era>(o.enumeration("era", EraNames, "Era"));
            c.tier = o.i32("tier", 0, 10);
            c.technologyRequirements = o.six("technologyRequirements", 0, 64);
            c.industryCost = o.i32("industryCost", 0, Big);
            c.buildTurns = o.i32("buildTurns", 0, Big);
            c.maintenance = o.i32("maintenance", 0, Big);
            c.hull = o.i32("hull", 0, Big);
            c.shields = o.i32("shields", 0, Big);
            c.attack = o.i32("attack", 0, Big);
            c.torpedoSharePercent = o.i32("torpedoSharePercent", 0, 100);
            const auto& w = o.array("weapons");
            if (w.items.size() != 4) bad(o.at("weapons"), "expected exactly 4 weapon mounts");
            for (std::size_t k = 0; k < 4; ++k) {
                Object m(w.items[k], o.at("weapons") + "[" + std::to_string(k) + "]");
                c.weapons[k].type = static_cast<roster::WeaponType>(m.enumeration("type", WeaponTypeNames, "WeaponType"));
                c.weapons[k].count = m.i32("count", 0, 1000);
                c.weapons[k].arc = static_cast<roster::FiringArc>(m.enumeration("arc", FiringArcNames, "FiringArc"));
                m.done();
            }
            c.weaponCount = o.i32("weaponCount", 0, 4);
            c.speed = o.i32("speed", 0, 10);
            c.range = o.i32("range", 0, 10);
            c.sensors = o.i32("sensors", 0, 10);
            c.cloak = o.flag("cloak");
            c.sourceKind = static_cast<roster::SourceKind>(o.enumeration("sourceKind", SourceKindNames, "SourceKind"));
            c.source = p.keep(o.str("source"));
            c.buildable = o.flag("buildable");
            c.description = p.keep(o.str("description"));
            Object f(o.get("flight"), o.at("flight"));
            c.flight.turnRate = f.i32("turnRate", 0, 360);
            c.flight.acceleration = f.i32("acceleration", 0, 100);
            c.flight.lateral = f.i32("lateral", 0, 100);
            c.flight.size = static_cast<roster::SizeClass>(f.enumeration("size", SizeClassNames, "SizeClass"));
            f.done();
            o.done();
            p.rosterIndex[static_cast<std::size_t>(c.id - ExpandedIdBase)] = static_cast<int>(p.roster.size());
            p.roster.push_back(c);
            p.rosterShips.push_back(referenceData::ShipRecord{c.id, c.raceId, c.roleName, c.className, c.technologyRequirements,
                c.industryCost, c.maintenance, c.hull, c.shields, c.attack, static_cast<double>(c.range),
                static_cast<double>(c.speed), c.engineRole, -1, c.description});
        }
    }
    {   const auto& a = section(root, "minors", 1, 1000);
        for (std::size_t i = 0; i < a.items.size(); ++i) {
            Object o(a.items[i], "minors[" + std::to_string(i) + "]");
            minors::MinorRaceRecord r{};
            r.id = o.i32("id", 1, 1000);
            r.name = p.keep(o.str("name", 200));
            r.homeworld = p.keep(o.str("homeworld", 200));
            r.referenceRaceId = o.i32("referenceRaceId", -1, 63);
            r.temperament = static_cast<minors::Temperament>(o.enumeration("temperament", TemperamentNames, "Temperament"));
            r.techLevel = o.i32("techLevel", 0, 10);
            r.population = o.i32("population", 1, 1000);
            r.bonus = static_cast<minors::JoinBonus>(o.enumeration("bonus", JoinBonusNames, "JoinBonus"));
            r.bonusValue = o.i32("bonusValue", 0, 100);
            r.signatureShipId = o.i32("signatureShipId", -1, ExpandedIdLimit - 1);
            {   // Six values (slots 0..5) or all nine (slots 0..8, with the Dominion, Borg and Breen at 6..8).
                const auto& av = o.array("affinity");
                if (av.items.size() != 6 && av.items.size() != 9) bad(o.at("affinity"), "expected an array of 6 or 9 integers");
                r.affinity = {};
                for (std::size_t k = 0; k < av.items.size(); ++k)
                    r.affinity[k] = static_cast<int>(integer(av.items[k], o.at("affinity") + "[" + std::to_string(k) + "]", -2, 2));
            }
            r.description = p.keep(o.str("description"));
            if (o.has("available")) r.available = o.flag("available");
            if (o.has("featured")) r.featured = o.flag("featured");
            if (o.has("majorRaceId")) r.majorRaceId = o.i32("majorRaceId", -1, 63);
            o.done();
            if (r.id != static_cast<int>(i) + 1) bad(o.at("id"), "minor race ids must equal their index + 1");
            if (r.referenceRaceId >= static_cast<int>(p.races.size())) bad(o.at("referenceRaceId"), "no such race");
            if (r.signatureShipId >= 0 && r.signatureShipId < ExpandedIdBase && r.signatureShipId >= static_cast<int>(p.ships.size())) bad(o.at("signatureShipId"), "no such legacy ship");
            if (r.signatureShipId >= ExpandedIdBase && p.rosterIndex[static_cast<std::size_t>(r.signatureShipId - ExpandedIdBase)] < 0) bad(o.at("signatureShipId"), "no such roster class");
            if (r.majorRaceId >= static_cast<int>(p.races.size())) bad(o.at("majorRaceId"), "no such race");
            if (r.featured && !r.available) bad(o.at("featured"), "a minor cannot be featured and unavailable");
            for (std::size_t j = 0; j < i; ++j)
                if (std::string_view(p.minors[j].name) == r.name) bad(o.at("name"), "duplicate minor race name");
            p.minors.push_back(r);
        }
    }
    {   const auto& a = section(root, "events", EventKinds, EventKinds);
        for (std::size_t i = 0; i < a.items.size(); ++i) {
            Object o(a.items[i], "events[" + std::to_string(i) + "]");
            if (o.enumeration("kind", EventKindNames, "GalacticEventKind") != static_cast<int>(i))
                bad(o.at("kind"), std::string("events must be in GalacticEventKind order; expected ") + EventKindNames[i]);
            EventDefinition e;
            e.name = o.str("name", 200);
            e.defaultChoice = o.i32("defaultChoice", -1, 2);
            const auto& choices = o.array("choices");
            for (std::size_t k = 0; k < choices.items.size(); ++k) {
                Object c(choices.items[k], o.at("choices") + "[" + std::to_string(k) + "]");
                e.choices.push_back({c.str("label", 200), c.str("effect", 1000)});
                c.done();
            }
            e.description = o.str("description", 2000);
            o.done();
            const bool hasChoice = EventHasChoice[i];
            if (hasChoice ? (e.choices.size() != 3 || e.defaultChoice < 0) : (!e.choices.empty() || e.defaultChoice != -1))
                bad(o.at("choices"), hasChoice ? "this event kind needs exactly 3 choices and a default 0..2"
                                               : "this event kind has no choices; defaultChoice must be -1");
            for (std::size_t at = e.description.find('{'); at != std::string::npos; at = e.description.find('{', at + 1)) {
                const auto rest = std::string_view(e.description).substr(at);
                if (!rest.starts_with("{where}") && !rest.starts_with("{magnitude}") && !rest.starts_with("{endTurn}"))
                    bad(o.at("description"), "unknown placeholder; use {where}, {magnitude} or {endTurn}");
            }
            p.events.push_back(std::move(e));
        }
    }
    {   // Majors: the five original slots must exist; donors and starting ships must resolve (roster is parsed by now).
        int found = 0;
        for (const auto& r : p.races) {
            if (r.empireId == 0) continue;
            if (r.empireId <= 5) ++found;
            if (r.shipDonor >= static_cast<int>(p.races.size())) bad("races[" + std::to_string(r.id) + "].major.shipDonor", "no such race");
            for (int ship : r.startingFleet) {
                if (ship < 0) continue;
                const auto at = static_cast<std::size_t>(ship - ExpandedIdBase);
                if (ship < ExpandedIdBase || ship >= ExpandedIdLimit || p.rosterIndex[at] < 0 || !p.roster[static_cast<std::size_t>(p.rosterIndex[at])].buildable)
                    bad("races[" + std::to_string(r.id) + "].major.startingFleet", "must name a buildable roster class");
            }
        }
        if (found != 5) bad("races", "the pack must define the five original majors (empireId 1..5)");
    }
    if (root.has("balance")) {
        Object b(root.get("balance"), "pack.balance");
        if (b.has("researchCostPercent")) p.balance.researchCostPercent = b.i32("researchCostPercent", 10, 1000);
        if (b.has("shipCostPercent")) p.balance.shipCostPercent = b.i32("shipCostPercent", 10, 1000);
        b.done();
    }
    if (root.has("eras")) {
        Object e(root.get("eras"), "pack.eras");
        Eras& er = p.eras;
        er.present = true;
        const auto ints = [&](const char* key, std::size_t n, int lo, int hi, auto& out) {
            const auto& v = e.get(key);
            if (v.kind != json::Value::Kind::Array || v.items.size() != n) bad(e.at(key), "expected an array of " + std::to_string(n) + " integers");
            for (std::size_t i = 0; i < n; ++i) out[i] = static_cast<int>(integer(v.items[i], e.at(key) + "[" + std::to_string(i) + "]", lo, hi));
        };
        if (e.has("turnsPerYear")) er.turnsPerYear = e.i32("turnsPerYear", 1, 52);
        if (e.has("years")) ints("years", 5, 1900, 3000, er.years);
        if (e.has("startTech")) ints("startTech", 5, 0, 14, er.startTech);
        if (e.has("techCap")) er.techCap = e.i32("techCap", 10, 14);
        if (e.has("tagYears")) ints("tagYears", 8, 1900, 3000, er.tagYears);
        for (std::size_t i = 1; i < 5; ++i) {
            if (er.years[i] <= er.years[i - 1]) bad(e.at("years"), "start years must ascend");
            if (er.startTech[i] < er.startTech[i - 1]) bad(e.at("startTech"), "start tech levels must not fall");
        }
        for (int t : er.startTech) if (t > er.techCap) bad(e.at("startTech"), "a start level above techCap");
        if (e.has("classes")) {
            const auto& a = e.array("classes");
            for (std::size_t i = 0; i < a.items.size(); ++i) {
                Object o(a.items[i], "pack.eras.classes[" + std::to_string(i) + "]");
                EraWindow w;
                w.id = o.i32("id", 0, ExpandedIdLimit - 1);   // A roster class (1000..1999) or a legacy catalogue ship (0..124).
                w.from = o.i32("from", 0, 3000);
                w.until = o.i32("until", 0, 3000);
                o.done();
                if (w.until != 0 && w.until <= w.from) bad(o.at("until"), "retirement year must follow the first year");
                bool known = false;
                for (const auto& c : p.roster) known = known || c.id == w.id;
                if (w.id < ExpandedIdBase) known = w.id < static_cast<int>(p.ships.size());
                if (!known) bad(o.at("id"), "no such roster class or catalogue ship");
                for (const auto& other : er.windows) if (other.id == w.id) bad(o.at("id"), "class listed twice");
                er.windows.push_back(w);
            }
        }
        if (e.has("highTechnologies")) {
            const auto& a = e.array("highTechnologies");
            if (a.items.size() != 24) bad(e.at("highTechnologies"), "expected 24 records (six fields, levels 11 to 14)");
            for (std::size_t i = 0; i < a.items.size(); ++i) {
                Object o(a.items[i], "pack.eras.highTechnologies[" + std::to_string(i) + "]");
                referenceData::TechnologyRecord r{};
                r.field = o.i32("field", 0, 5);
                r.level = o.i32("level", 11, 14);
                r.name = p.keep(o.str("name", 80));
                r.cost = o.i64("cost", 1, INT64_MAX / 4);
                r.description = p.keep(o.str("description"));
                o.done();
                r.id = static_cast<int>(77 + i);
                if (static_cast<std::size_t>(r.field * 4 + (r.level - 11)) != i) bad(o.at("level"), "records run field by field, level 11 to 14");
                if (i % 4 == 0 && r.cost <= p.technologies[static_cast<std::size_t>(r.field * 11 + 10)].cost)
                    bad(o.at("cost"), "level 11 must cost more than level 10");
                if (i % 4 != 0 && r.cost <= er.highTech.back().cost) bad(o.at("cost"), "costs must rise with the level");
                er.highTech.push_back(r);
            }
        }
        if (er.techCap > 10 && er.highTech.size() != 24) bad(e.at("techCap"), "a cap above 10 needs highTechnologies");
        e.done();
    }
    root.done();
    return pack;
}

inline std::unique_ptr<Pack> parsePack(std::string_view bytes) {
    if (bytes.size() > 64u * 1024u * 1024u) bad("pack", "bundle larger than 64 MB");
    auto pack = parsePackValue(json::Parser(bytes).document());
    pack->info.checksum = fnv1a64(bytes);
    pack->info.bytes = bytes.size();
    return pack;
}

// ---- Overlay packs: a base pack plus N overlays (docs/design/PACKS-AND-PRESETS.md section 1) ----
// {"format":1,"kind":"overlay","id":"..","name":"..","revision":1,"requires":["botf-base"],
//  "balance":{..}, "<section>":{"patch":[{"id":N,<changed fields>}],"append":[<whole records>]}}
// Merging happens on the parsed document, before the strict typed parse above, so every merged record is validated by
// exactly the rules a base record is (unknown keys, ranges, id == index, ...). Nothing is ever removed: an overlay
// switches a roster class off with {"id":N,"buildable":false}.
struct Overlay {
    std::string id, name;
    int revision = 0;
    std::vector<std::string> requires_;
    std::uint64_t checksum = 0;
    std::size_t bytes = 0;
    json::Value doc;
};

inline bool validPackId(const std::string& id) {
    if (id.empty() || id.size() > 40) return false;
    for (char c : id) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    return true;
}

inline Overlay parseOverlay(std::string_view bytes) {
    if (bytes.size() > 16u * 1024u * 1024u) bad("overlay", "overlay larger than 16 MB");
    Overlay ov;
    ov.doc = json::Parser(bytes).document();
    ov.checksum = fnv1a64(bytes);
    ov.bytes = bytes.size();
    Object root(ov.doc, "overlay");
    if (root.i32("format", 0, Big) != FormatVersion) bad("overlay.format", "format is not supported (expected " + std::to_string(FormatVersion) + ")");
    if (root.str("kind", 16) != "overlay") bad("overlay.kind", "expected \"overlay\"");
    ov.id = root.str("id", 40);
    if (!validPackId(ov.id) || ov.id == "botf-base") bad("overlay.id", "id must be [a-z0-9-] and not botf-base");
    ov.name = root.str("name", 200);
    ov.revision = root.i32("revision", 0, Big);
    const auto& req = root.array("requires");
    for (std::size_t i = 0; i < req.items.size(); ++i) {
        if (req.items[i].kind != json::Value::Kind::String) bad("overlay.requires[" + std::to_string(i) + "]", "expected a pack id");
        ov.requires_.push_back(req.items[i].text);
    }
    return ov; // Section and balance keys are checked while merging; unknown keys fail there.
}

inline constexpr const char* OverlaySections[] = {"technologies", "ships", "buildings", "races", "roster", "minors", "events"};

// Optional record keys (absent from the base pack's records): a patch may add them to a record that lacks them. The strict typed parse
// after the merge still checks name, type and range, so a stray key anywhere else stays an error.
inline bool optionalField(const std::string& key) {
    return key == "available" || key == "featured" || key == "majorRaceId" || key == "traitEffects" ||
        key == "buildable" || key == "perEmpireLimit"; // Schema 23: the economy overlay makes vanilla records buildable
}

inline json::Value* member(json::Value& obj, const std::string& key) {
    for (auto& m : obj.members) if (m.first == key) return &m.second;
    return nullptr;
}
inline bool sameKey(const json::Value& a, const json::Value& b) {
    if (a.kind != b.kind) return false;
    if (a.kind == json::Value::Kind::String) return a.text == b.text;
    return a.kind == json::Value::Kind::Number && a.mantissa == b.mantissa && a.negative == b.negative && a.scale == b.scale;
}

// Applies one overlay to the merged document. Rules, per section (the record key is "id", or "kind" for events):
//  - patch: each entry names an existing record (earlier overlays' appended records count) and replaces only the fields
//    it lists. A nested object ("flight") merges field by field; arrays and scalars are replaced whole. An entry that
//    names no record, or names one twice in the same overlay, is an error.
//  - append: whole records added after the existing ones. Ids must continue the section's rule (id == index, or
//    ascending in the roster's range); the strict parse checks that. technologies and events have a fixed record count
//    and take patch only.
//  - balance: each listed value replaces the pack's (the last overlay wins).
inline void mergeOverlay(json::Value& doc, const Overlay& ov) {
    const std::string where = "overlay " + ov.id;
    for (const auto& m : ov.doc.members) {
        const std::string& key = m.first;
        if (key == "format" || key == "kind" || key == "id" || key == "name" || key == "revision" || key == "requires") continue;
        if (key == "balance" || key == "eras") {   // "eras" (schema 22) merges the same way: each listed key replaces the pack's.
            if (m.second.kind != json::Value::Kind::Object) bad(where + "." + key, "expected an object");
            json::Value* target = member(doc, key);
            if (!target) {
                json::Value fresh;
                fresh.kind = json::Value::Kind::Object;
                doc.members.emplace_back(key, std::move(fresh));
                target = &doc.members.back().second;
            }
            for (const auto& b : m.second.members) {
                if (json::Value* t = member(*target, b.first)) *t = b.second; else target->members.push_back(b);
            }
            continue;
        }
        bool known = false;
        for (const char* name : OverlaySections) known = known || key == name;
        if (!known) bad(where, "unknown key \"" + key + "\"");
        if (m.second.kind != json::Value::Kind::Object) bad(where + "." + key, "expected an object with patch and/or append");
        json::Value* section = member(doc, key);
        if (!section || section->kind != json::Value::Kind::Array) bad(where + "." + key, "the base pack has no such section");
        const char* idKey = key == "events" ? "kind" : "id";
        const bool fixedCount = key == "technologies" || key == "events";
        for (const auto& part : m.second.members) {
            const std::string path = where + "." + key + "." + part.first;
            if (part.second.kind != json::Value::Kind::Array) bad(path, "expected an array");
            if (part.first == "append") {
                if (fixedCount) bad(path, "this section has a fixed record count; use patch");
                for (const auto& rec : part.second.items) section->items.push_back(rec);
            } else if (part.first == "patch") {
                std::vector<const json::Value*> seen;
                for (std::size_t i = 0; i < part.second.items.size(); ++i) {
                    const auto& entry = part.second.items[i];
                    const std::string epath = path + "[" + std::to_string(i) + "]";
                    if (entry.kind != json::Value::Kind::Object) bad(epath, "expected an object");
                    const json::Value* want = nullptr;
                    for (const auto& f : entry.members) if (f.first == idKey) want = &f.second;
                    if (!want) bad(epath, std::string("missing key \"") + idKey + "\"");
                    for (const auto* prior : seen) if (sameKey(*prior, *want)) bad(epath, "record patched twice in one overlay");
                    seen.push_back(want);
                    json::Value* target = nullptr;
                    for (auto& rec : section->items) {
                        const json::Value* k = member(rec, idKey);
                        if (k && sameKey(*k, *want)) { target = &rec; break; }
                    }
                    if (!target) bad(epath, "no such record to patch");
                    for (const auto& f : entry.members) {
                        if (f.first == idKey) continue;
                        json::Value* t = member(*target, f.first);
                        if (!t && optionalField(f.first)) { target->members.push_back(f); continue; }
                        if (!t) bad(epath, "unknown field \"" + f.first + "\"");
                        if (t->kind == json::Value::Kind::Object && f.second.kind == json::Value::Kind::Object) {
                            for (const auto& nf : f.second.members) {
                                json::Value* nt = member(*t, nf.first);
                                if (!nt && optionalField(nf.first)) { t->members.push_back(nf); continue; }
                                if (!nt) bad(epath + "." + f.first, "unknown field \"" + nf.first + "\"");
                                *nt = nf.second;
                            }
                        } else {
                            *t = f.second;
                        }
                    }
                }
            } else {
                bad(path, "expected patch or append");
            }
        }
    }
}

// The stack's checksum covers the base and every overlay's exact bytes in order, so two peers with the same stack agree
// and any changed byte shows. A pure function of the bytes: nothing reads a clock or the campaign RNG.
inline std::uint64_t stackChecksum(std::uint64_t baseSum, const std::vector<PartRef>& parts) {
    std::string text = "botf-base:" + checksumHex(baseSum) + "\n";
    for (const auto& p : parts) text += p.id + ":" + checksumHex(p.checksum) + "\n";
    return fnv1a64(text);
}
inline std::string stackId(const std::vector<PartRef>& parts) {
    std::string id = "botf-base";
    for (const auto& p : parts) id += "+" + p.id;
    return id;
}
// The base bundle with each overlay merged in order, validated as one pack.
inline std::unique_ptr<Pack> buildStack(std::string_view baseBytes, std::uint64_t baseSum, const std::vector<std::string_view>& overlays) {
    json::Value doc = json::Parser(baseBytes).document();
    std::vector<PartRef> parts;
    std::size_t total = baseBytes.size();
    for (const auto& bytes : overlays) {
        const Overlay ov = parseOverlay(bytes);
        for (const auto& p : parts) if (p.id == ov.id) bad("overlay " + ov.id, "listed twice in the stack");
        for (const auto& need : ov.requires_) {
            bool met = need == "botf-base";
            for (const auto& p : parts) met = met || p.id == need;
            if (!met) bad("overlay " + ov.id, "requires " + need + ", which must come earlier in the stack");
        }
        mergeOverlay(doc, ov);
        parts.push_back({ov.id, ov.checksum});
        total += ov.bytes;
    }
    auto pack = parsePackValue(doc);
    pack->info.id = stackId(parts);
    pack->info.name = "BOTF Remastered base content with " + std::to_string(parts.size()) + " overlay(s)";
    pack->info.stack = parts;
    pack->info.checksum = stackChecksum(baseSum, parts);
    pack->info.bytes = total;
    pack->info.base = false;
    return pack;
}

struct State {
    std::vector<std::unique_ptr<Pack>> packs;  // Every pack ever installed; the last is active. Never freed.
    const Pack* active = nullptr;
    std::uint64_t baseChecksum = 0;
    bool baseKnown = false;
};
inline State& state() { static State s; return s; }
inline std::uint64_t baseChecksumValue() {
    auto& s = state();
    if (!s.baseKnown) { s.baseChecksum = fnv1a64(embeddedBaseBundle()); s.baseKnown = true; }
    return s.baseChecksum;
}
inline const Pack& activate(std::unique_ptr<Pack> pack) {
    auto& s = state();
    pack->info.base = pack->info.checksum == baseChecksumValue();
    if (pack->info.base != (pack->info.id == "botf-base"))
        bad("pack.id", pack->info.base ? "the base bundle must carry id botf-base" : "id botf-base is reserved for the base pack");
    s.packs.push_back(std::move(pack));
    s.active = s.packs.back().get();
    return *s.active;
}
inline const Pack& activateStack(std::unique_ptr<Pack> pack) {
    auto& s = state();
    s.packs.push_back(std::move(pack));
    s.active = s.packs.back().get();
    return *s.active;
}
inline const Pack& pack() {
    auto& s = state();
    if (!s.active) activate(parsePack(embeddedBaseBundle()));
    return *s.active;
}
} // namespace detail

// Parse, validate and activate a bundle. Atomic: on error the active pack is unchanged.
inline const PackInfo& install(std::string_view bundleBytes) { return detail::activate(detail::parsePack(bundleBytes)).info; }
inline const PackInfo& installBase() { return install(embeddedBaseBundle()); }
inline const PackInfo& active() { return detail::pack().info; }
// Installs the embedded base pack with these overlays merged over it, in order (no overlays = the base pack). Atomic: on
// any error the active pack is unchanged. Idempotent: the stack that is already active is left in place.
inline const PackInfo& installStack(const std::vector<std::string_view>& overlays) {
    if (overlays.empty()) return installBase();
    auto pack = detail::buildStack(embeddedBaseBundle(), detail::baseChecksumValue(), overlays);
    const auto& now = active();
    if (now.id == pack->info.id && now.checksum == pack->info.checksum) return now;
    return detail::activateStack(std::move(pack)).info;
}
inline const Balance& balance() { return detail::pack().balance; }
inline const Eras& eras() { return detail::pack().eras; }
inline std::uint64_t baseChecksum() { return detail::baseChecksumValue(); }

// Event text for GalacticEventKind index 0..6.
inline const EventDefinition& eventDefinition(int kind) {
    const auto& events = detail::pack().events;
    return events.at(static_cast<std::size_t>(kind));
}

// Read-only view of a loaded table with the interface of the old std::array tables.
template <class T> class Table {
public:
    using Getter = const std::vector<T>& (*)();
    constexpr explicit Table(Getter get) : get_(get) {}
    const T* begin() const { return get_().data(); }
    const T* end() const { return get_().data() + get_().size(); }
    std::size_t size() const { return get_().size(); }
    bool empty() const { return get_().empty(); }
    const T& operator[](std::size_t i) const { return get_()[i]; }
    const T& front() const { return get_().front(); }
    const T& back() const { return get_().back(); }
private:
    Getter get_;
};

namespace tables {
inline const std::vector<referenceData::TechnologyRecord>& technologies() { return detail::pack().technologies; }
inline const std::vector<referenceData::ShipRecord>& ships() { return detail::pack().ships; }
inline const std::vector<referenceData::BuildingRecord>& buildings() { return detail::pack().buildings; }
inline const std::vector<referenceData::RaceRecord>& races() { return detail::pack().races; }
inline const std::vector<roster::RosterClass>& roster() { return detail::pack().roster; }
inline const std::vector<referenceData::ShipRecord>& rosterShips() { return detail::pack().rosterShips; }
inline const std::vector<int>& rosterIndex() { return detail::pack().rosterIndex; }
inline const std::vector<minors::MinorRaceRecord>& minors() { return detail::pack().minors; }
} // namespace tables

} // namespace botf::content
