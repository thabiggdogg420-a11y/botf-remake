// BOTF Remastered: minor-race roster and minor-race life rules (Lane G).
//
// Everything here is original remaster content and deterministic integer logic. The roster is data
// only; the functions below are the whole rule set and are called from Simulation.cpp through a few
// one-line hooks (generation, turn review, economy, morale, invasion, membership, save, validate).
//
// Managed minors: a Minor with raceId != 0 carries roster identity plus progression state
// (techLevel, population, progress). New campaigns (generateCampaign*) place managed minors;
// minors from older saves and hand-built fixtures keep raceId 0 and the legacy rules, and are
// matched to the roster by name only for display and for their join bonus.
//
// Save support: the optional section
//     MINOR_RACES <count>
//     <systemId> <raceId> <techLevel> <population> <progress>     (one line per managed minor)
// is written after COLONY_GOVERNORS and before CHECKSUM, and only when at least one minor is managed.
// Saves without it load exactly as before.
#pragma once

#include "Simulation.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <istream>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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

inline constexpr int MinorRaceCount = 30;
inline constexpr int ReviewInterval = 5;       // Same cadence as the modern minor councils (DIPLOMACY.md 6).
inline constexpr int MaxMinorPopulation = 1000;

inline constexpr std::array<MinorRaceRecord, MinorRaceCount> Roster{{
    {1, "Acamarians", "Acamar", 5, Temperament::Mercantile, 5, 45, JoinBonus::Credits, 10, 105, {0, 1, 0, 0, 0, 1},
     "A clan society healing from centuries of blood feuds, with some clans still roaming the stars as raiders. Membership brings their trade networks and a steady flow of credits."},
    {2, "Andorians", "Andoria", 6, Temperament::Warlike, 6, 60, JoinBonus::GroundDefense, 20, -1, {0, 2, 1, -1, 0, 0},
     "Proud, antennaed warriors from an icy moon and old partners of the Vulcans and humans. Their soldiers harden the ground defenses of every world in the empire they join."},
    {3, "Angosians", "Angosia", 7, Temperament::Peaceful, 5, 40, JoinBonus::GroundDefense, 15, -1, {0, 1, 0, 0, -1, 0},
     "A peaceful society that once engineered its soldiers into living weapons, then struggled to live with them. Their veterans stiffen the defenses of allied worlds."},
    {4, "Antedeans", "Antede", 8, Temperament::Xenophobic, 3, 35, JoinBonus::Food, 10, -1, {0, -1, 0, 0, 0, 1},
     "Ichthyoid travelers who fall into a stupor aboard starships and rarely trust outsiders. Their ocean harvests add to the food supply of any empire they join."},
    {5, "Anticans", "Antica", 9, Temperament::Warlike, 3, 40, JoinBonus::GroundDefense, 15, -1, {0, 1, 1, 0, 0, 0},
     "Carnivorous hunters locked in an ancient feud with the neighboring Selay. Their appetite for battle strengthens the garrisons of any empire they join."},
    {6, "Bajorans", "Bajor", 10, Temperament::Spiritual, 4, 70, JoinBonus::Morale, 5, 106, {0, 2, 0, 0, -2, 0},
     "A deeply spiritual people who endured a brutal occupation with their faith intact. They despise Cardassian rule, and their devotion lifts morale across the empire they join."},
    {7, "Bandi", "Deneb", 11, Temperament::Mercantile, 2, 35, JoinBonus::Industry, 10, -1, {0, 1, 0, 0, 0, 1},
     "Gracious hosts and gifted builders whose showpiece station charmed visitors until its secret came out. Their architects lift industry across any empire they join."},
    {8, "Benzites", "Benzar", 12, Temperament::Scientific, 6, 45, JoinBonus::Industry, 10, -1, {0, 2, 0, 0, 0, 0},
     "Methodical perfectionists who never solve the same problem twice. Their efficiency experts raise industry throughout the empire they join."},
    {9, "Betazoids", "Betazed", 13, Temperament::Peaceful, 6, 55, JoinBonus::Morale, 5, -1, {0, 2, 0, -1, -1, 0},
     "Telepaths who prize honesty because little can be hidden from them for long. Their counselors ease tension on every world of the empire they join."},
    {10, "Bolians", "Bolarus", 14, Temperament::Mercantile, 5, 60, JoinBonus::Credits, 10, -1, {0, 1, 0, 0, 0, 1},
     "Cheerful blue-skinned bankers whose financial houses reach across the quadrant. Membership opens their banks and raises credit income empire-wide."},
    {11, "Bynars", "Bynaus", 15, Temperament::Scientific, 7, 40, JoinBonus::Research, 10, -1, {0, 0, 0, 0, 0, 0},
     "Paired beings who think in binary and live linked to their planetary computer. Their network adds 10% to the research of any empire they join."},
    {12, "Caldonians", "Caldos", 16, Temperament::Scientific, 6, 35, JoinBonus::Research, 10, -1, {0, 1, 0, 0, 0, -1},
     "Tall, gentle scholars who value pure research far above commerce. Their think tanks add to the research of any empire they join."},
    {13, "Chalnoth", "Chalna", 17, Temperament::Warlike, 3, 45, JoinBonus::GroundDefense, 20, -1, {0, -1, 2, 0, 0, 0},
     "Anarchist hunters who recognize no authority but strength. If persuaded to join an empire, they make its worlds very costly to invade."},
    {14, "Edo", "Rubicun", 18, Temperament::Peaceful, 2, 30, JoinBonus::Morale, 5, -1, {0, 1, -1, 0, 0, 0},
     "A carefree, pleasure-loving people watched over by a powerful guardian entity. Their easygoing culture raises morale across the empire they join."},
    {15, "Ktarians", "Ktaria", 19, Temperament::Xenophobic, 5, 50, JoinBonus::Industry, 5, 107, {0, -1, 0, 1, 1, 0},
     "Ambitious schemers who once tried to subvert a starship crew with an addictive game. Wary of outsiders, they drive a hard bargain but put industrious hands to work."},
    {16, "Malcorians", "Malcor", 20, Temperament::Xenophobic, 4, 50, JoinBonus::Industry, 10, -1, {0, 0, -1, -1, -1, -1},
     "A people on the verge of warp flight who are not yet ready to learn they are not alone. Suspicious of every empire, they bring a busy industrial base to one that wins them over."},
    {17, "Mintakans", "Mintaka", 21, Temperament::Peaceful, 1, 30, JoinBonus::Food, 10, -1, {0, 1, 0, 0, 0, 0},
     "A gentle, rational proto-Vulcan people who once briefly mistook a starship captain for a god. Their communal farms add to the food supply of any empire they join."},
    {18, "Mizarians", "Mizar", 22, Temperament::Peaceful, 5, 50, JoinBonus::Morale, 5, -1, {0, 1, 0, 0, 0, 0},
     "Pacifists who have outlasted repeated conquests by never resisting any of them. Their calm steadies morale in the empire they join."},
    {19, "Nausicaans", "Nausicaa", 23, Temperament::Warlike, 4, 50, JoinBonus::GroundDefense, 20, -1, {0, -2, 2, 0, 0, 1},
     "Towering brawlers and hired muscle with a taste for violence. Their young warriors swell the garrisons of any empire that can hold their loyalty."},
    {20, "Pakleds", "Pakled", 24, Temperament::Mercantile, 3, 45, JoinBonus::Research, 5, 108, {0, -1, 0, 0, 0, 2},
     "Slow-spoken scavengers who acquire technology by trade, trickery or theft. What they collect adds a little to the research of any empire they join."},
    {21, "Selay", "Selay", 25, Temperament::Warlike, 3, 40, JoinBonus::GroundDefense, 15, -1, {0, 1, 0, 1, 0, 0},
     "Reptilian rivals of the Anticans, just as eager to settle old scores. Their soldiers strengthen the garrisons of any empire they join."},
    {22, "Sheliak", "Sheliak", 26, Temperament::Xenophobic, 8, 60, JoinBonus::Food, 15, 109, {0, -1, -1, 0, -1, -1},
     "Legalistic non-humanoids who consider most species barely worth addressing and enforce every clause of every treaty. Their mastery of environments boosts food output empire-wide."},
    {23, "Takarans", "Takara", 27, Temperament::Scientific, 6, 40, JoinBonus::Research, 5, -1, {0, 1, 0, 0, 0, 0},
     "A people whose decentralized bodies shrug off wounds that would kill others. Their curiosity about physical forces adds to the research of any empire they join."},
    {24, "Talarians", "Talar", 28, Temperament::Warlike, 5, 55, JoinBonus::GroundDefense, 25, 110, {0, -1, 1, 0, 1, 0},
     "A patriarchal military society that treasures its homeworld above all else. Their fortification doctrine makes every world of their empire harder to invade."},
    {25, "Tamarians", "Tama", 29, Temperament::Spiritual, 6, 50, JoinBonus::Research, 15, 111, {0, 1, 0, 0, 0, 0},
     "The Children of Tama speak only in metaphors drawn from their myths and history. Once understood, those stories hold insights that raise the research of any empire they join."},
    {26, "Trill", "Trill", 30, Temperament::Scientific, 7, 50, JoinBonus::Research, 10, 112, {0, 2, 0, 0, 0, 0},
     "Humanoids who share their lives with long-lived symbionts that carry centuries of memory from host to host. Their joined scientists raise the research of any empire they join."},
    {27, "Ullians", "Ullia", 31, Temperament::Peaceful, 6, 30, JoinBonus::Research, 5, -1, {0, 1, 0, 0, 0, 0},
     "Telepathic historians who collect the memories of other peoples as living archives. Their knowledge adds a little to the research of any empire they join."},
    {28, "Vulcans", "Vulcan", 32, Temperament::Scientific, 8, 80, JoinBonus::Research, 15, 113, {0, 2, -1, -1, 0, 0},
     "Disciples of logic with a scientific tradition older than their warp drive. Vulcan scholars add 15% to the research of any empire they join."},
    {29, "Yridians", "Yridia", 33, Temperament::Mercantile, 5, 35, JoinBonus::Credits, 15, 114, {0, 0, 0, 1, 1, 2},
     "Information dealers who will sell any secret to anyone who can pay. Their brokerage houses raise credit income for any empire they join."},
    {30, "Zakdorn", "Zakdorn", 34, Temperament::Scientific, 6, 45, JoinBonus::GroundDefense, 15, -1, {0, 1, 1, 0, 0, 0},
     "Celebrated as the finest strategic minds in the galaxy, though they rarely put the reputation to the test. Their war-planners strengthen the defenses of any empire they join."},
}};

inline const char* temperamentName(Temperament t) {
    switch (t) {
        case Temperament::Peaceful: return "Peaceful";
        case Temperament::Scientific: return "Scientific";
        case Temperament::Mercantile: return "Mercantile";
        case Temperament::Spiritual: return "Spiritual";
        case Temperament::Xenophobic: return "Xenophobic";
        case Temperament::Warlike: return "Warlike";
    }
    return "Unknown";
}

inline const char* joinBonusName(JoinBonus b) {
    switch (b) {
        case JoinBonus::Research: return "Research";
        case JoinBonus::Industry: return "Industry";
        case JoinBonus::Food: return "Food";
        case JoinBonus::Credits: return "Credits";
        case JoinBonus::Morale: return "Morale";
        case JoinBonus::GroundDefense: return "Ground defense";
    }
    return "Unknown";
}

// Player-facing summary of a join bonus, e.g. "+10% research".
inline std::string describeJoinBonus(const MinorRaceRecord& r) {
    const std::string value = "+" + std::to_string(r.bonusValue);
    switch (r.bonus) {
        case JoinBonus::Research: return value + "% research";
        case JoinBonus::Industry: return value + "% industry";
        case JoinBonus::Food: return value + "% food";
        case JoinBonus::Credits: return value + "% credit income";
        case JoinBonus::Morale: return value + " morale";
        case JoinBonus::GroundDefense: return value + "% ground defense";
    }
    return value;
}

inline const MinorRaceRecord* raceById(int id) {
    return id >= 1 && id <= MinorRaceCount ? &Roster[static_cast<std::size_t>(id - 1)] : nullptr;
}

inline const MinorRaceRecord* raceByName(std::string_view name) {
    for (const auto& r : Roster) if (name == r.name) return &r;
    return nullptr;
}

// Roster profile for a minor: its saved raceId, or (legacy minors) a match by name.
inline const MinorRaceRecord* raceFor(const Minor& m) {
    return m.raceId != 0 ? raceById(m.raceId) : raceByName(m.name);
}

inline bool managed(const Minor& m) { return m.raceId != 0; }

// Number of minors a new campaign of this size should hold (including the starter minor).
inline int rosterTarget(int systemCount) {
    return std::clamp(systemCount / 7, 1, MinorRaceCount);
}

// Technology progress needed to reach the next level.
inline int techThreshold(int techLevel) { return techLevel + 2; }

namespace detail {
inline std::uint64_t mix(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
inline void fail(const char* message) { throw std::runtime_error(message); }
inline int chebyshev(const System& a, const System& b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }
inline const System* systemById(const GameState& s, int id) {
    const auto it = std::find_if(s.systems.begin(), s.systems.end(), [id](const System& x) { return x.id == id; });
    return it == s.systems.end() ? nullptr : &*it;
}
inline bool atWar(const GameState& s, int empireId) {
    return std::any_of(s.majorRelations.begin(), s.majorRelations.end(), [&](const MajorRelation& r) {
        return r.atWar && (r.first == empireId || r.second == empireId) && !empireEliminated(s, r.first == empireId ? r.second : r.first);
    });
}
inline bool campaignEmpire(const GameState& s, int id) {
    return id == s.playerEmpire || std::any_of(s.rivals.begin(), s.rivals.end(), [id](const EmpireState& e) { return e.id == id; });
}
} // namespace detail

inline int startingTechLevel(const MinorRaceRecord& r, StartingEra era) {
    return std::clamp(r.techLevel + static_cast<int>(era) - 1, 1, MaxTechLevel);
}

// Gives a minor its roster identity and starting progression (does not move or rename it).
inline void adopt(Minor& m, const MinorRaceRecord& r, StartingEra era) {
    m.raceId = r.id;
    m.techLevel = startingTechLevel(r, era);
    m.population = r.population;
    m.progress = 0;
}

// New-campaign placement. Adopts minors already present that match the roster by name (the starter
// Bynars), then places a seeded subset of the roster on unclaimed systems until rosterTarget() minors
// exist. Deterministic in (seed, map); never consumes the campaign RNG. `renameSystem` is called for
// each newly claimed system after its name changes, so the caller can regenerate its planets.
inline void placeRosterMinors(GameState& s, std::uint64_t seed, StartingEra era,
                              const std::function<void(System&)>& renameSystem = {}) {
    std::set<int> usedRaces;
    for (auto& m : s.minors) {
        if (const auto* r = raceByName(m.name); r && !usedRaces.count(r->id)) { adopt(m, *r, era); usedRaces.insert(r->id); }
    }
    const int target = rosterTarget(static_cast<int>(s.systems.size()));
    if (static_cast<int>(s.minors.size()) >= target) return;

    std::vector<const MinorRaceRecord*> races;
    for (const auto& r : Roster) if (!usedRaces.count(r.id)) races.push_back(&r);
    std::sort(races.begin(), races.end(), [&](const MinorRaceRecord* a, const MinorRaceRecord* b) {
        const auto ha = detail::mix(seed ^ 0x6d696e6f72726163ULL ^ static_cast<std::uint64_t>(a->id));
        const auto hb = detail::mix(seed ^ 0x6d696e6f72726163ULL ^ static_cast<std::uint64_t>(b->id));
        return ha != hb ? ha < hb : a->id < b->id;
    });
    std::vector<System*> candidates;
    for (auto& sys : s.systems) candidates.push_back(&sys);
    std::sort(candidates.begin(), candidates.end(), [&](const System* a, const System* b) {
        const auto ha = detail::mix(seed ^ 0x686f6d65776f726cULL ^ static_cast<std::uint64_t>(a->id));
        const auto hb = detail::mix(seed ^ 0x686f6d65776f726cULL ^ static_cast<std::uint64_t>(b->id));
        return ha != hb ? ha < hb : a->id < b->id;
    });
    auto eligible = [&](const System& sys, int homeSpacing) {
        if (colonyAt(s, sys.id) || minorAt(s, sys.id)) return false;
        for (const auto& c : s.colonies)
            if (const auto* home = detail::systemById(s, c.systemId); home && detail::chebyshev(*home, sys) < homeSpacing) return false;
        for (const auto& m : s.minors)
            if (const auto* other = detail::systemById(s, m.systemId); other && detail::chebyshev(*other, sys) < 2) return false;
        return true;
    };
    std::size_t nextRace = 0;
    for (int homeSpacing : {3, 2}) {
        for (auto* sys : candidates) {
            if (static_cast<int>(s.minors.size()) >= target || nextRace >= races.size()) break;
            if (!eligible(*sys, homeSpacing)) continue;
            const auto& race = *races[nextRace++];
            sys->name = race.homeworld;
            if (renameSystem) renameSystem(*sys);
            Minor m;
            m.systemId = sys->id;
            m.name = race.name;
            adopt(m, race, era);
            const int xenophobia = race.temperament == Temperament::Xenophobic ? 5 : 0;
            for (int e = 1; e <= 5; ++e) m.attitude[static_cast<std::size_t>(e)] = std::clamp(60 + 5 * race.affinity[static_cast<std::size_t>(e)] - xenophobia, 0, 100);
            s.minors.push_back(m);
        }
    }
    std::sort(s.minors.begin(), s.minors.end(), [](const Minor& a, const Minor& b) { return a.systemId < b.systemId; });
}

// Aggregate benefits an empire draws from its member minors. Derived from current membership only,
// so every bonus disappears the moment a minor secedes or is conquered away.
struct MemberBonuses {
    int researchPct = 0, industryPct = 0, foodPct = 0, creditsPct = 0, morale = 0, groundDefensePct = 0;
    int members = 0;
};

inline MemberBonuses memberBonuses(const GameState& s, int empireId) {
    MemberBonuses b;
    if (empireId < 1 || empireId > 5) return b;
    for (const auto& m : s.minors) {
        if (m.memberOf != empireId) continue;
        ++b.members;
        const auto* r = raceFor(m);
        if (!r) { b.researchPct += 10; continue; } // Legacy membership dividend for unknown minors.
        switch (r->bonus) {
            case JoinBonus::Research: b.researchPct += r->bonusValue; break;
            case JoinBonus::Industry: b.industryPct += r->bonusValue; break;
            case JoinBonus::Food: b.foodPct += r->bonusValue; break;
            case JoinBonus::Credits: b.creditsPct += r->bonusValue; break;
            case JoinBonus::Morale: b.morale += r->bonusValue; break;
            case JoinBonus::GroundDefense: b.groundDefensePct += r->bonusValue; break;
        }
    }
    b.morale = std::min(b.morale, 20);
    return b;
}

// Level colonies of this empire settle toward each turn (70 baseline).
inline int moraleTarget(const GameState& s, int empireId) { return std::min(100, 70 + memberBonuses(s, empireId).morale); }

// Ground defense of a colony facing invasion: member ground-defense bonuses, plus a home guard when the
// colony is the homeworld of a Warlike member minor (5 + 2 per minor tech level).
inline int invasionDefense(const GameState& s, const Colony& target, int baseDefense) {
    int defense = baseDefense;
    if (const auto* m = minorAt(s, target.systemId); m && managed(*m) && m->memberOf == target.ownerId) {
        if (const auto* r = raceFor(*m); r && r->temperament == Temperament::Warlike) defense += 5 + 2 * m->techLevel;
    }
    return defense * (100 + memberBonuses(s, target.ownerId).groundDefensePct) / 100;
}

// Shapes the colony a managed minor brings when it joins: its own population and a workforce that
// fits the default facilities. Legacy minors keep the caller's defaults.
inline void configureJoinedColony(const Minor& m, Colony& colony) {
    if (!managed(m)) return;
    colony.population = std::clamp(m.population, 10, MaxMinorPopulation);
    colony.foodWorkers = std::min(colony.farms * 10, colony.population / 2);
    colony.industryWorkers = std::min(colony.factories * 10, colony.population / 3);
    colony.researchWorkers = std::min(colony.laboratories * 10, colony.population - colony.foodWorkers - colony.industryWorkers);
}

// Every ReviewInterval turns, managed minors grow and advance, and their temperament and affinities
// pull on their relations. Runs before the diplomacy resolution of the turn (legacy and modern), so
// the modern engine sees the result through its usual Minor -> engine synchronization.
inline void reviewMinorRaces(GameState& s, std::vector<std::string>& events) {
    if (s.turn <= 0 || s.turn % ReviewInterval != 0) return;
    for (auto& m : s.minors) {
        if (!managed(m)) continue;
        const auto* r = raceById(m.raceId);
        if (!r) continue;
        // Technology: one step per review, two for Scientific cultures.
        if (m.techLevel < MaxTechLevel) {
            m.progress += r->temperament == Temperament::Scientific ? 2 : 1;
            if (m.progress >= techThreshold(m.techLevel)) {
                m.progress = 0;
                ++m.techLevel;
                if (m.techLevel == MaxTechLevel) m.progress = 0;
                if (m.contact[static_cast<std::size_t>(s.playerEmpire)])
                    events.push_back("The " + m.name + " have advanced to technology level " + std::to_string(m.techLevel) + ".");
            }
        } else {
            m.progress = 0;
        }
        // Population of an independent minor grows toward twice its founding size.
        if (m.memberOf == 0) {
            const int growthPct = r->temperament == Temperament::Xenophobic ? 2
                : (r->temperament == Temperament::Peaceful || r->temperament == Temperament::Spiritual) ? 4 : 3;
            const int cap = std::min(MaxMinorPopulation, 2 * r->population);
            if (m.population < cap) m.population = std::min(cap, m.population + std::max(1, m.population * growthPct / 100));
        }
        for (int e = 1; e <= 5; ++e) {
            const auto slot = static_cast<std::size_t>(e);
            if (!m.contact[slot] || !detail::campaignEmpire(s, e) || empireEliminated(s, e)) continue;
            int& att = m.attitude[slot];
            if (m.memberOf == e) {
                // Temperament shapes loyalty to the empire it belongs to.
                switch (r->temperament) {
                    case Temperament::Warlike: if (detail::atWar(s, e)) ++att; break;
                    case Temperament::Peaceful: if (detail::atWar(s, e)) --att; break;
                    case Temperament::Scientific: if (empire(s, e).technologies[1].level >= m.techLevel) ++att; break;
                    case Temperament::Mercantile: if (empire(s, e).credits >= 1000) ++att; break;
                    case Temperament::Spiritual: if (const auto* c = colonyAt(s, m.systemId); c && c->morale >= 70) ++att; break;
                    case Temperament::Xenophobic: --att; break;
                }
            } else if (m.memberOf == 0) {
                // Affinity pulls an independent minor toward its natural view of each empire; aversion
                // yields once any treaty is signed.
                const int affinity = r->affinity[slot];
                const int xenophobia = r->temperament == Temperament::Xenophobic ? 1 : 0;
                const int goal = 50 + 10 * affinity - 10 * xenophobia;
                const int pull = std::max(std::abs(affinity), xenophobia);
                const bool treaty = m.friendshipTurn[slot] >= 0 || s.diplomacyEngineState.highestTreatyRank(
                    diplomacy::ParticipantId::major(e), diplomacy::ParticipantId::minor(m.systemId)) > 0;
                if (att < goal) att = std::min(goal, att + pull);
                else if (att > goal && !treaty) att = std::max(goal, att - pull);
            }
            att = std::clamp(att, 0, 100);
        }
    }
}

// Throws on any inconsistency. Called from validate().
inline void validateMinorRaces(const GameState& s) {
    std::set<int> races;
    for (const auto& m : s.minors) {
        if (!managed(m)) {
            if (m.techLevel != 0 || m.population != 0 || m.progress != 0) detail::fail("Legacy minor carries roster progression");
            continue;
        }
        const auto* r = raceById(m.raceId);
        if (!r) detail::fail("Unknown minor race");
        if (!races.insert(m.raceId).second) detail::fail("Duplicate minor race");
        if (m.name != r->name) detail::fail("Minor race name mismatch");
        if (m.techLevel < 1 || m.techLevel > MaxTechLevel) detail::fail("Invalid minor technology level");
        if (m.population < 1 || m.population > MaxMinorPopulation) detail::fail("Invalid minor population");
        if (m.progress < 0 || m.progress >= techThreshold(m.techLevel) || (m.techLevel == MaxTechLevel && m.progress != 0))
            detail::fail("Invalid minor technology progress");
    }
}

// Optional save section; nothing is written when no minor is managed.
inline void writeMinorRacesSection(std::ostream& out, const GameState& s) {
    const auto count = std::count_if(s.minors.begin(), s.minors.end(), [](const Minor& m) { return managed(m); });
    if (count == 0) return;
    out << "MINOR_RACES " << count << '\n';
    for (const auto& m : s.minors)
        if (managed(m)) out << m.systemId << ' ' << m.raceId << ' ' << m.techLevel << ' ' << m.population << ' ' << m.progress << '\n';
}

// Reads the section body after its tag. Every row must name an existing minor, at most once.
inline void readMinorRacesSection(std::istream& in, GameState& s) {
    int count = -1;
    if (!(in >> count) || count < 1 || count > static_cast<int>(s.minors.size())) detail::fail("Invalid MINOR_RACES count");
    std::set<int> seen;
    for (int i = 0; i < count; ++i) {
        int systemId = 0, raceId = 0, techLevel = 0, population = 0, progress = 0;
        if (!(in >> systemId >> raceId >> techLevel >> population >> progress)) detail::fail("Malformed MINOR_RACES row");
        const auto it = std::find_if(s.minors.begin(), s.minors.end(), [systemId](const Minor& m) { return m.systemId == systemId; });
        if (it == s.minors.end() || !seen.insert(systemId).second || raceId == 0) detail::fail("Invalid MINOR_RACES row");
        it->raceId = raceId; it->techLevel = techLevel; it->population = population; it->progress = progress;
    }
}

} // namespace botf::minors
