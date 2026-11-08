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
struct BuildingRecord { int id; const char* name; int technologyField; int technologyLevel; int industryCost; int energyCost; int populationCost; int moraleBonus; int excludedEmpireMask; std::uint64_t residentRaceMask; int bonusType; std::array<int, 6> bonusValues; std::array<int, 6> unrestBonuses; const char* description; };
struct RaceRecord { int id; const char* name; const char* icon; };
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
    std::array<int, 6> affinity;   // By major slot 1..5 (1 Fed, 2 Klingon, 3 Romulan, 4 Cardassian, 5 Ferengi); -2..+2.
    const char* description;       // Original remaster text.
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

struct PackInfo {
    std::string id, name;
    int format = 0, revision = 0;
    std::uint64_t checksum = 0;
    std::size_t bytes = 0;
    bool base = false;        // Byte-identical to the embedded base pack.
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

inline std::unique_ptr<Pack> parsePack(std::string_view bytes) {
    if (bytes.size() > 64u * 1024u * 1024u) bad("pack", "bundle larger than 64 MB");
    const json::Value doc = json::Parser(bytes).document();
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
            r.excludedEmpireMask = o.i32("excludedEmpireMask", 0, Big);
            r.residentRaceMask = o.u64("residentRaceMask");
            r.bonusType = o.i32("bonusType", -Big, Big);
            r.bonusValues = o.six("bonusValues", -Big, Big);
            r.unrestBonuses = o.six("unrestBonuses", -Big, Big);
            r.description = p.keep(o.str("description"));
            o.done();
            if (r.id != static_cast<int>(i)) bad(o.at("id"), "building ids must equal their index");
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
            r.signatureShipId = o.i32("signatureShipId", -1, LegacyShipLimit - 1);
            r.affinity = o.six("affinity", -2, 2);
            r.description = p.keep(o.str("description"));
            o.done();
            if (r.id != static_cast<int>(i) + 1) bad(o.at("id"), "minor race ids must equal their index + 1");
            if (r.referenceRaceId >= static_cast<int>(p.races.size())) bad(o.at("referenceRaceId"), "no such race");
            if (r.signatureShipId >= static_cast<int>(p.ships.size())) bad(o.at("signatureShipId"), "no such legacy ship");
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
    root.done();
    p.info.checksum = fnv1a64(bytes);
    p.info.bytes = bytes.size();
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
