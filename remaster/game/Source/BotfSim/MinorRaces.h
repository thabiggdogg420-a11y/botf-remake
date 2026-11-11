// BOTF Remastered: minor-race roster and minor-race life rules (Lane G).
//
// Everything here is original remaster content and deterministic integer logic. The roster is data
// only; the functions below are the whole rule set and are called from Simulation.cpp through a few
// one-line hooks (generation, turn review, economy, morale, invasion, membership, save, validate).
//
// Managed minors: a Minor with raceId != 0 carries roster identity plus progression state
// (techLevel, population, progress). New campaigns (newCampaign -> placeMinorRaceRoster) place
// managed minors; generateCampaign*/generateReferenceCampaign* keep the legacy baseline. Minors from
// older saves and hand-built fixtures keep raceId 0 and the legacy rules exactly (flat +10% research
// on membership, 30-population colony, morale 70); raceFor() matches them by name for display only.
//
// Save support: the optional section
//     MINOR_RACES <count>
//     <systemId> <raceId> <techLevel> <population> <progress>     (one line per managed minor)
// is written after GAMEPLAY_LAYER and before CHECKSUM (schema 13, no bump), and only when at least one
// minor is managed.
// Saves without it load exactly as before.
#pragma once

#include "Content.h"
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

// Temperament, JoinBonus and MinorRaceRecord are declared in Content.h. The roster is data since M0:
// data/base/minors.json (docs/DATA-FORMAT.md), loaded from the installed content pack.
inline constexpr int ReviewInterval = 5;       // Same cadence as the modern minor councils (DIPLOMACY.md 6).
inline constexpr int MaxMinorPopulation = 1000;

inline constexpr content::Table<MinorRaceRecord> Roster{&content::tables::minors};
inline int minorRaceCount() { return static_cast<int>(Roster.size()); }

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
    return id >= 1 && id <= minorRaceCount() ? &Roster[static_cast<std::size_t>(id - 1)] : nullptr;
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
    return std::clamp(systemCount / 5, 1, std::max(1, minorRaceCount()));
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
                              const std::function<void(System&)>& renameSystem = {}, int targetOverride = -1) {
    std::set<int> usedRaces;
    for (auto& m : s.minors) {
        if (const auto* r = raceByName(m.name); r && !usedRaces.count(r->id)) { adopt(m, *r, era); usedRaces.insert(r->id); }
    }
    const int target = targetOverride >= 0 ? targetOverride : rosterTarget(static_cast<int>(s.systems.size()));
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
    auto eligible = [&](const System& sys, int homeSpacing, int minorSpacing = 2) {
        if (colonyAt(s, sys.id) || minorAt(s, sys.id)) return false;
        for (const auto& c : s.colonies)
            if (const auto* home = detail::systemById(s, c.systemId); home && detail::chebyshev(*home, sys) < homeSpacing) return false;
        for (const auto& m : s.minors)
            if (const auto* other = detail::systemById(s, m.systemId); other && detail::chebyshev(*other, sys) < minorSpacing) return false;
        return true;
    };
    std::size_t nextRace = 0;
    // A chosen density (targetOverride) may need a crowded galaxy to give way: a last pass lets minors sit next to each other.
    for (int pass = 0; pass < (targetOverride >= 0 ? 3 : 2); ++pass) {
        const int homeSpacing = pass == 0 ? 3 : pass == 1 ? 2 : 1, minorSpacing = pass == 2 ? 1 : 2;
        for (auto* sys : candidates) {
            if (static_cast<int>(s.minors.size()) >= target || nextRace >= races.size()) break;
            if (!eligible(*sys, homeSpacing, minorSpacing)) continue;
            const auto& race = *races[nextRace++];
            sys->name = race.homeworld;
            if (renameSystem) renameSystem(*sys);
            Minor m;
            m.systemId = sys->id;
            m.name = race.name;
            adopt(m, race, era);
            const int xenophobia = race.temperament == Temperament::Xenophobic ? 5 : 0;
            for (int e = 1; e <= MaxMajors; ++e) if (e <= LegacyMajors || detail::campaignEmpire(s, e)) m.attitude[static_cast<std::size_t>(e)] = std::clamp(60 + 5 * race.affinity[static_cast<std::size_t>(e)] - xenophobia, 0, 100);
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
        const auto* r = managed(m) ? raceById(m.raceId) : nullptr;
        if (!r) { b.researchPct += 10; continue; } // Legacy minors keep the flat pre-roster research dividend.
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
        for (int e = 1; e <= MaxMajors; ++e) {
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
