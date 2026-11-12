// Empire slots (schema 17, docs/design/EMPIRE-SLOTS.md). A campaign empire id is a race slot 1..8; which slots a game
// seats is its lineup. The race table (key, empire name, capital, traits, ship donor, starting ships) is data:
// the `major` block of the races in data/base/races.json. Header-only, no state.
#pragma once
#include "Content.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace botf::slots {

inline constexpr int SlotCount = 8;

inline const referenceData::RaceRecord* raceForEmpire(int empireId) {
    for (const auto& r : content::tables::races()) if (r.empireId == empireId && empireId != 0) return &r;
    return nullptr;
}
inline const referenceData::RaceRecord* raceForKey(std::string_view key) {
    for (const auto& r : content::tables::races()) if (r.empireId != 0 && key == r.key) return &r;
    return nullptr;
}
inline bool validSlot(int empireId) { return empireId >= 1 && empireId <= SlotCount && raceForEmpire(empireId) != nullptr; }
// Reference-catalogue race id of the slot's own ships (ShipRecord::raceId), -1 for an unknown slot.
inline int catalogueRace(int empireId) { const auto* r = raceForEmpire(empireId); return r ? r->id : -1; }
// Catalogue race whose utility and escort hulls the slot may also build (stand-in until the race has its own), -1 none.
inline int shipDonor(int empireId) { const auto* r = raceForEmpire(empireId); return r ? r->shipDonor : -1; }
inline const char* key(int empireId) { const auto* r = raceForEmpire(empireId); return r ? r->key : ""; }
inline const char* name(int empireId) { const auto* r = raceForEmpire(empireId); return r ? r->empireName : ""; }
// MOD-3 trait effects of the slot's race (the `major.traitEffects` data; all zero unless a pack sets them).
struct TraitEffects {
    int morale = 0, creditsPct = 0, groundDefensePct = 0, securityPct = 0;
    int industryPct = 0, researchPct = 0, foodPct = 0, shipCostPct = 0, intelPct = 0; // MOD-7 (BALANCE-AI.md section 3)
};
inline TraitEffects traitEffects(int empireId) {
    const auto* r = raceForEmpire(empireId);
    return r ? TraitEffects{r->traitMorale, r->traitCreditsPct, r->traitGroundPct, r->traitSecurityPct,
                            r->traitIndustryPct, r->traitResearchPct, r->traitFoodPct, r->traitShipCostPct, r->traitIntelPct} : TraitEffects{};
}
inline const char* capital(int empireId) { const auto* r = raceForEmpire(empireId); return r ? r->capital : ""; }

// Race keys in slot order ("federation", ... "breen").
inline std::vector<std::string> allKeys() {
    std::vector<std::string> out;
    for (int id = 1; id <= SlotCount; ++id) if (const auto* r = raceForEmpire(id)) out.push_back(r->key);
    return out;
}
// A lineup of race keys -> empire ids, in the given order. Throws on an unknown or repeated key or a size outside 2..8.
inline std::vector<int> idsFromKeys(const std::vector<std::string>& keys) {
    if (keys.size() < 2 || keys.size() > static_cast<std::size_t>(SlotCount)) throw std::invalid_argument("A lineup seats two to eight majors");
    std::vector<int> ids;
    for (const auto& k : keys) {
        const auto* r = raceForKey(k);
        if (!r) throw std::invalid_argument("Unknown race key \"" + k + "\"");
        if (std::find(ids.begin(), ids.end(), r->empireId) != ids.end()) throw std::invalid_argument("Race \"" + k + "\" appears twice in the lineup");
        ids.push_back(r->empireId);
    }
    return ids;
}

} // namespace botf::slots
