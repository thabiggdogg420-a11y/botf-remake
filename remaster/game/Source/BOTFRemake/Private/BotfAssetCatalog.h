#pragma once
// Engine-independent asset-key logic for the Unreal client: which key a ship or weapon asks for, and how a key is
// resolved through LocalAssets/resolved-manifest.json (written by Import-Assets.ps1) down to a /Game object path.
//
// UBotfAssetRegistry (BotfAssetRegistry.h) only reads the file and turns the resulting strings into soft object
// paths; everything else lives here so it is compiled and unit-tested without Unreal (Tools/client-logic-tests.cpp).
//
// Key scheme (docs/SHIP-ROSTER.md, assets/check_asset_keys.py):
//   ship.<fac>.<class>         expanded roster assetKey, or ship.<fac>.<slug(className)> for legacy catalogue ids
//   weapon.<Type>[.<fac>]      roster WeaponType names; the faction variant is tried first
//   study.<fac>                in-repo placeholder meshes

#include "BotfJson.h"
#include "ShipRoster.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace botf::assets {

// ---------------------------------------------------------------------------------------------------------------
// Keys
// ---------------------------------------------------------------------------------------------------------------

// Lower-case, apostrophes dropped, every other run of non-alphanumerics becomes one '_' (MIRROR of slug() in
// assets/check_asset_keys.py): "K'T'Inga" -> "ktinga", "Galaxy-X" -> "galaxy_x", "Outpost " -> "outpost".
inline std::string slug(const std::string& name) {
    std::string out;
    bool pendingSeparator = false;
    for (const char raw : name) {
        const char c = (raw >= 'A' && raw <= 'Z') ? static_cast<char>(raw - 'A' + 'a') : raw;
        if (c == '\'') continue;
        const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (!alnum) { pendingSeparator = !out.empty(); continue; }
        if (pendingSeparator) out.push_back('_');
        pendingSeparator = false;
        out.push_back(c);
    }
    return out.empty() ? std::string("unknown") : out;
}

// Legacy RaceCatalogue ids 0..4 (Cardassians, Federation, Ferengi, Klingons, Romulans). Empty for anything else.
inline std::string legacyRacePrefix(int raceId) {
    switch (raceId) {
    case 0: return "car";
    case 1: return "fed";
    case 2: return "fer";
    case 3: return "kli";
    case 4: return "rom";
    default: return {};
    }
}

// Combat/owner faction as used by the tactical scene (1 Federation, 2 Klingon, 3 Romulan, 4 Cardassian, 5 Ferengi).
inline std::string ownerFactionPrefix(int ownerFaction) {
    switch (ownerFaction) {
    case 1: return "fed";
    case 2: return "kli";
    case 3: return "rom";
    case 4: return "car";
    case 5: return "fer";
    default: return {};
    }
}

inline std::string placeholderShipKey(const std::string& prefix) { return prefix.empty() ? std::string() : "study." + prefix; }

// The asset key for a ship id (legacy catalogue or expanded roster). Empty when the id is unknown or the class
// belongs to a race without art keys.
inline std::string shipAssetKey(int referenceShipId) {
    if (const auto* rosterClass = roster::findRosterClass(referenceShipId)) return rosterClass->assetKey ? rosterClass->assetKey : "";
    if (referenceShipId == 115) return "ship.borg.cube"; // Legacy "Borg Cube" (race 36).
    const auto* record = roster::findShip(referenceShipId);
    if (!record || !record->shipClass) return {};
    const std::string prefix = legacyRacePrefix(record->raceId);
    if (prefix.empty()) return {};
    return "ship." + prefix + "." + slug(record->shipClass);
}

// Faction prefix embedded in a ship/weapon/study key ("ship.kli.d7" -> "kli", "weapon.Disruptor.rom" -> "rom").
inline std::string keyFactionPrefix(const std::string& key) {
    const auto first = key.find('.');
    if (first == std::string::npos) return {};
    const std::string head = key.substr(0, first);
    if (head == "study") return key.substr(first + 1);
    if (head == "ship") {
        const auto second = key.find('.', first + 1);
        return second == std::string::npos ? key.substr(first + 1) : key.substr(first + 1, second - first - 1);
    }
    if (head == "weapon") {
        const auto second = key.find('.', first + 1);
        return second == std::string::npos ? std::string() : key.substr(second + 1);
    }
    return {};
}

// Keys to try, in order, for a weapon's fire sound/VFX.
inline std::vector<std::string> weaponAssetKeys(roster::WeaponType type, const std::string& factionPrefix) {
    std::vector<std::string> keys;
    if (type == roster::WeaponType::None) return keys;
    const std::string base = std::string("weapon.") + roster::weaponTypeName(type);
    if (!factionPrefix.empty()) keys.push_back(base + "." + factionPrefix);
    keys.push_back(base);
    keys.push_back(roster::torpedoWeapon(type) ? "weapon.generic.torpedo" : "weapon.generic.beam");
    return keys;
}

// The dominant beam and torpedo types of a ship (first mount of each kind), for classic ships the faction default.
struct ShipWeaponKeys {
    roster::WeaponType beam = roster::WeaponType::None;
    roster::WeaponType torpedo = roster::WeaponType::None;
};
inline ShipWeaponKeys shipWeapons(int referenceShipId, int ownerFaction) {
    ShipWeaponKeys out;
    if (const auto* rosterClass = roster::findRosterClass(referenceShipId)) {
        for (int i = 0; i < rosterClass->weaponCount && i < static_cast<int>(rosterClass->weapons.size()); ++i) {
            const auto type = rosterClass->weapons[static_cast<std::size_t>(i)].type;
            if (type == roster::WeaponType::None) continue;
            if (roster::torpedoWeapon(type)) { if (out.torpedo == roster::WeaponType::None) out.torpedo = type; }
            else if (out.beam == roster::WeaponType::None) out.beam = type;
        }
        return out;
    }
    switch (ownerFaction) {
    case 2: case 3: case 4: out.beam = roster::WeaponType::Disruptor; break;
    default: out.beam = roster::WeaponType::PhaserArray; break;
    }
    out.torpedo = ownerFaction == 3 ? roster::WeaponType::PlasmaTorpedo : roster::WeaponType::PhotonTorpedo;
    return out;
}

// ---------------------------------------------------------------------------------------------------------------
// Scale normalisation
// ---------------------------------------------------------------------------------------------------------------

// On-screen bounding radius (scene units) for a ship of the given canon length. Unknown length keeps the interim 80.
// Square-root compression keeps a 3 km station and a 30 m fighter both readable in one battle.
inline double displayRadiusForLength(double lengthMeters) {
    if (!(lengthMeters > 0.0)) return 80.0;
    const double radius = 80.0 * std::sqrt(lengthMeters / 350.0);
    return std::clamp(radius, 40.0, 170.0);
}
inline double uniformScaleFor(double lengthMeters, double meshBoundsRadius) {
    return displayRadiusForLength(lengthMeters) / std::max(1.0, meshBoundsRadius);
}

// ---------------------------------------------------------------------------------------------------------------
// Resolved manifest
// ---------------------------------------------------------------------------------------------------------------

struct Entry {
    std::string key;
    std::string kind;     // model, texture, sfx, voice, music
    std::string status;   // resolved, in-repo, missing, convert-failed, unsupported-format
    std::string faction;
    std::string fallback; // Manifest fallback key ("" = none)
    std::vector<std::string> objectPaths; // /Game/... object paths (ueAssets, or the in-repo ueAsset)
    std::vector<std::string> botfClasses;
    double targetLengthMeters = 0.0;

    bool usable() const { return (status == "resolved" || status == "in-repo") && !objectPaths.empty(); }
};

struct Resolution {
    std::string requestedKey;
    std::string resolvedKey;              // Empty when nothing in the chain resolved (placeholder).
    std::vector<std::string> objectPaths; // Every variant (voice lines have several).
    std::vector<std::string> chain;       // Keys visited, requested first.
    double targetLengthMeters = 0.0;      // Canon length of the requested class when known, else of the resolved one.
    bool placeholder = true;
    std::string factionPrefix;            // For the built-in placeholder.

    const std::string& primaryPath() const {
        static const std::string empty;
        return objectPaths.empty() ? empty : objectPaths.front();
    }
};

// Object path of the ueAsset string: "/Game/X/Name" becomes "/Game/X/Name.Name"; full paths are kept.
inline std::string normaliseObjectPath(const std::string& path) {
    if (path.empty()) return path;
    const auto slash = path.rfind('/');
    const std::string leaf = slash == std::string::npos ? path : path.substr(slash + 1);
    if (leaf.find('.') != std::string::npos) return path;
    return path + "." + leaf;
}

// Built-in placeholder meshes that ship with the project (Content/Presentation), by faction prefix. Empty when
// there is none; the caller then uses an engine basic shape.
inline std::string builtInShipPlaceholder(const std::string& prefix) {
    const char* name = prefix == "fed" ? "Federation" : prefix == "kli" ? "Klingon" : prefix == "rom" ? "Romulan" :
        prefix == "car" ? "Cardassian" : prefix == "fer" ? "Ferengi" : nullptr;
    if (!name) return {};
    return std::string("/Game/Presentation/") + name + "/StaticMeshes/" + name + "." + name;
}

class Catalog {
public:
    // Parses resolved-manifest.json text. On failure the catalogue stays empty (every key resolves to a placeholder).
    bool loadFromJson(const std::string& text, std::string* error = nullptr) {
        entries.clear();
        json::Value root;
        if (!json::parse(text, root, error)) return false;
        const json::Value* assets = root.find("assets");
        if (!assets || !assets->isObject()) {
            if (error) *error = "no \"assets\" object";
            return false;
        }
        for (std::size_t m = 0; m < assets->keys.size() && m < assets->values.size(); ++m) {
            const json::Value& v = assets->values[m];
            if (!v.isObject()) continue;
            Entry e;
            e.key = assets->keys[m];
            e.kind = v.stringOr("kind");
            e.status = v.stringOr("status", "missing");
            e.faction = v.stringOr("faction");
            e.fallback = v.stringOr("fallback");
            e.botfClasses = v.stringsAt("botfClasses");
            e.targetLengthMeters = v.numberOr("targetLengthMeters", 0.0);
            for (const auto& path : v.stringsAt("ueAssets"))
                if (path.rfind("/Game/", 0) == 0) e.objectPaths.push_back(normaliseObjectPath(path));
            const std::string ueAsset = v.stringOr("ueAsset");
            if (e.objectPaths.empty() && ueAsset.rfind("/Game/", 0) == 0) e.objectPaths.push_back(normaliseObjectPath(ueAsset));
            entries[e.key] = std::move(e);
        }
        return true;
    }

    void add(Entry e) { entries[e.key] = std::move(e); }
    std::size_t size() const { return entries.size(); }
    const Entry* find(const std::string& key) const {
        const auto it = entries.find(key);
        return it == entries.end() ? nullptr : &it->second;
    }

    // Walks key -> manifest fallback -> ... until a usable entry. An unknown key is retried without its last
    // segment while that still names a key ("weapon.Disruptor.gorn" -> "weapon.Disruptor"). Cycles stop the walk.
    Resolution resolve(const std::string& key) const {
        Resolution out;
        out.requestedKey = key;
        out.factionPrefix = keyFactionPrefix(key);
        std::set<std::string> visited;
        std::string current = key;
        for (int hop = 0; hop < 32 && !current.empty(); ++hop) {
            if (!visited.insert(current).second) break;
            out.chain.push_back(current);
            const Entry* e = find(current);
            if (!e) {
                const auto dot = current.rfind('.');
                const std::string parent = dot == std::string::npos ? std::string() : current.substr(0, dot);
                current = parent.find('.') == std::string::npos ? std::string() : parent;
                continue;
            }
            if (out.targetLengthMeters <= 0.0 && e->targetLengthMeters > 0.0) out.targetLengthMeters = e->targetLengthMeters;
            if (e->usable()) {
                out.resolvedKey = e->key;
                out.objectPaths = e->objectPaths;
                out.placeholder = false;
                return out;
            }
            current = e->fallback;
        }
        return out;
    }

    // First usable key of an ordered list (weapon variants); a placeholder resolution of the first key otherwise.
    Resolution resolveFirst(const std::vector<std::string>& keys) const {
        for (const auto& key : keys) {
            Resolution r = resolve(key);
            if (!r.placeholder) return r;
        }
        return keys.empty() ? Resolution{} : resolve(keys.front());
    }

    // The model for a ship id, down to the faction's built-in mesh. objectPaths is empty only when not even a
    // built-in placeholder exists (non-major factions); the caller then uses an engine shape.
    Resolution resolveShip(int referenceShipId, int ownerFaction) const {
        std::string key = shipAssetKey(referenceShipId);
        if (key.empty()) key = placeholderShipKey(ownerFactionPrefix(ownerFaction));
        Resolution r = resolve(key);
        if (r.factionPrefix.empty()) r.factionPrefix = ownerFactionPrefix(ownerFaction);
        if (r.placeholder) {
            const std::string builtIn = builtInShipPlaceholder(r.factionPrefix.empty() ? ownerFactionPrefix(ownerFaction) : r.factionPrefix);
            if (!builtIn.empty()) r.objectPaths = {builtIn};
        }
        return r;
    }

private:
    std::map<std::string, Entry> entries;
};

} // namespace botf::assets
