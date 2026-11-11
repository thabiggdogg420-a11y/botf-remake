#pragma once
// Schema 20: threat actors (docs/design/THREAT-ACTORS.md). The Borg, Species 8472 and the vanilla monsters are map objects
// with their own hull and shields. They are deterministic: every draw comes from GameState::rngState and happens only when a
// threat switch is on, so a game with the switches off consumes no random number and saves exactly as before.
//
// Turn flow (Simulation.cpp advanceWithCombat):
//   threats::step           spawn, retarget, move, despawn            (on the moving copy, before combat)
//   resolveTacticalBattles  threats::battles: actor vs each empire's fleets in its sector, through the tactical engine
//   threats::afterCombat    siege, assimilation, bombardment, regeneration, Borg-empire rules   (on the carried state)
#include "Simulation.h"

#include <functional>
#include <iosfwd>
#include <set>
#include <string>
#include <vector>

namespace botf::threats {

enum class Behaviour : int { Hunt, Roam, Drift, Guard };      // Hunt colonies; wander toward random cells; wander slowly; sit still.
enum class Hostility : int { Peaceful, Cornered, Aggressive };  // Never fights; fights fleets sharing its sector; also works on colonies.
enum class Effect : int { None, Assimilate, Bombard, Strip, Raid };

struct ActorDef {
    const char* key;
    const char* name;
    const char* assetKey;   // Existing roster asset key used for the map marker and the battle model (stand-ins noted in the design doc).
    int shipId;             // Catalogue ship record (ships.json) or roster class (>= 1000): hull, shields, attack.
    int minTurn;            // No spawn before this turn.
    int spawnPer1000;       // Chance per turn at normal frequency (static actors spawn once at turn 1).
    int gap;                // Turns between two spawns of this kind.
    int cap;                // Concurrent actors of this kind (doubled at frequent).
    int speed;              // Sectors per turn (0 static).
    Behaviour behaviour;
    Hostility hostility;
    Effect effect;
    int lifespan;           // Turns before it leaves (0 = never).
    int regenPct;           // Hull percent restored per turn while out of battle.
    int attackPct;          // Tactical attack scale against the record (100 = the record).
    bool borg;              // Borg family: the Borg-only switch, adaptation, assimilation.
};

inline constexpr ActorDef Defs[ThreatKinds] = {
    {"borg_cube",   "Borg cube",          "ship.borg.tactical_cube", 115, 50, 40, 40, 1, 1, Behaviour::Hunt,  Hostility::Aggressive, Effect::Assimilate, 90, 15, 100, true},
    {"borg_sphere", "Borg sphere",        "ship.borg.sphere",        1600, 35, 50, 30, 1, 2, Behaviour::Hunt,  Hostility::Aggressive, Effect::Assimilate, 70, 12, 100, true},
    {"species_8472", "Species 8472 bioship", "ship.s8472.bioship",   1714, 45, 40, 35, 1, 2, Behaviour::Hunt,  Hostility::Aggressive, Effect::Bombard,    50, 20, 100, false},
    {"husnock",     "Husnock warship",    "ship.krenim.warship",       122, 30, 30, 40, 1, 1, Behaviour::Hunt,  Hostility::Aggressive, Effect::Raid,       60,  5, 100, false},
    {"calamarain",  "Calamarain",         "ship.s8472.bioship",      116, 10, 30, 30, 1, 1, Behaviour::Drift, Hostility::Aggressive, Effect::None,       45,  3, 100, false},
    {"chodak",      "Chodak dreadnought", "ship.kli.neghtev",       117, 20, 30, 40, 1, 1, Behaviour::Roam,  Hostility::Aggressive, Effect::Raid,       50,  4, 100, false},
    {"crystalline", "Crystalline Entity", "ship.xindi.weapon",      118, 25, 30, 45, 1, 1, Behaviour::Hunt,  Hostility::Aggressive, Effect::Strip,      55,  2, 100, false},
    {"echo_papa",   "Echo Papa 607 drone", "ship.borg.probe",        119, 1,  0,  0,  1, 0, Behaviour::Guard, Hostility::Aggressive, Effect::None,        0,  0, 100, false},
    {"edo_guardian", "Edo Guardian",      "ship.krenim.timeship",      120, 1,  0,  0,  1, 0, Behaviour::Guard, Hostility::Cornered,   Effect::None,        0,  0, 100, false},
    {"gomtuu",      "Gomtuu",             "ship.xindi.aquatic",         121, 15, 20, 50, 1, 1, Behaviour::Drift, Hostility::Peaceful,   Effect::None,       60,  3, 100, false},
    {"tarellian",   "Tarellian plague ship", "ship.malon.freighter",      124, 5,  25, 40, 1, 1, Behaviour::Drift, Hostility::Cornered,   Effect::None,       40,  3, 100, false},
    {"deep_space_9", "Deep Space Nine",   "ship.car.nor",            1303, 1, 0,  0,  1, 0, Behaviour::Guard, Hostility::Peaceful,   Effect::None,        0,  0, 100, false},
};
inline const ActorDef& def(int kind) { return Defs[static_cast<std::size_t>(kind)]; }
// The kind whose catalogue record or roster class is `shipId` (a recorded battle participant's referenceShipId), or -1.
inline int kindForShip(int shipId) {
    for (int k = 0; k < ThreatKinds; ++k) if (Defs[k].shipId == shipId) return k;
    return -1;
}
inline bool kindValid(int kind) { return kind >= 0 && kind < ThreatKinds; }
inline constexpr int BorgEmpire = 7;                       // Empire slot of the Borg Collective.
inline constexpr int AdaptCap = 40;                        // Borg adaptation ceiling (percent).
inline constexpr int AssimilatedMemory = 60;               // Turns a depopulated, assimilated system is marked on the map.
inline const char* frequencyName(int frequency) {
    static const char* const Names[] = {"none", "rare", "normal", "frequent"};
    return frequency >= 0 && frequency <= 3 ? Names[frequency] : "normal";
}

bool borgSeated(const GameState& state);          // A Borg Collective empire is in this campaign.
bool kindEnabled(const GameState& state, int kind); // Switches, per-actor toggles and replacements allow this kind to spawn.
bool anyEnabled(const GameState& state);
std::string actorName(const ThreatActor& actor);
int actorMaxHull(const ThreatActor& actor);
int actorMaxShields(const ThreatActor& actor);      // Includes Borg adaptation.
int actorAttack(const ThreatActor& actor);          // Includes Borg adaptation and the kind's attack scale.
bool hostile(const ThreatActor& actor);             // Fights fleets that share its sector.
bool canSee(const GameState& state, int viewer, const ThreatActor& actor); // Inside the viewer's sensor coverage.
const ThreatActor* actorById(const GameState& state, int id);
bool isThreatCombatant(int fleetId);
int threatIdForCombatant(int fleetId);
const AssimilatedSystem* assimilatedAt(const GameState& state, int systemId);

// Structural checks and the optional THREATS save section (schema 20).
void validate(const GameState& state);
bool sectionNeeded(const GameState& state);
void writeSection(std::ostream& out, const GameState& state);
void readSection(std::istream& in, GameState& state);

// Turn hooks (see the flow above).
void step(GameState& moving, TurnReport& report);
void battles(GameState& state, TurnReport& report, std::set<int>& withdrawn, std::vector<CombatReport>& out,
             const std::function<void(tactical::Encounter&)>& playerEncounter);
void afterCombat(GameState& next, TurnReport& report);

// Borg rules shared by the threat and a playable Borg empire (docs/design/THREAT-ACTORS.md section 5).
int moraleTarget(const GameState& state, const Colony& colony, int minorMorale); // Borg colonies hold their own morale.
void onColonyCaptured(GameState& state, Colony& colony, TurnReport& report);     // A Borg empire took the colony: assimilate it.
// A Borg empire won this battle: half of the enemy ships it destroyed return as drones (appended to `fresh`).
void assimilateShips(GameState& state, const CombatReport& battle, TurnReport& report, std::vector<Fleet>& fresh);
} // namespace botf::threats
