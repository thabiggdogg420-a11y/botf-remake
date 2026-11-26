#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Diplomacy.h"
#include "Intelligence.h"

namespace botf {
// BOTF Remastered schema and content. Schema 12 adds the unified tactical combat
// model and fusion-plant accounting; schema 13 adds the gameplay layer (ruleset
// presets, stations, galaxy features, curated events, alternative victories and
// build templates, see docs/GAMEPLAY-REMASTER.md); schema 14 adds per-ship orders and
// flags to recorded battle rounds (COMBAT_ORDERS) and the RefuseCallToArms order;
// schema 15 adds the optional GALAXY_SETUP section (galaxy shape, minor-race density, difficulty;
// docs/design/GALAXY-GENERATOR.md); schema 16 adds the empire rules (EmpireRules.h, docs/design/EMPIRE-RULES.md):
// dilithium refineries, emergency programs, invasion aftermath, minor conquest and minor guard ships
// (optional EMPIRE_RULES section); schema 17 adds the new-game preset and its overlay packs (Presets.h,
// docs/design/PACKS-AND-PRESETS.md; optional PACKS section); schema 18 generalises the empire slots (docs/design/EMPIRE-SLOTS.md):
// two to eight majors with stable ids 1..8 (optional EMPIRE_SLOTS section, written only when an id above five is in play);
// schema 20 adds the threat actors (Borg, Species 8472, the vanilla monsters; Threats.h, docs/design/THREAT-ACTORS.md; optional
// THREATS section, written only when a threat switch is on or an actor or assimilated system exists);
// schema 21 adds the race structures (Structures.h, docs/design/DOMINION.md; optional STRUCTURES section, written only while a colony holds one);
// schema 23 adds the economy and rules mechanics (EconomyRules.h, docs/design/ECONOMY-MECHANICS.md; optional ECONOMY_RULES section, written
// only when a rule flag is on): structure upgrades, shared morale, Protectorate HQ, intercept and raid orders, crew experience.
// schema 25 adds the scenario layer (Scenarios.h, docs/design/SCENARIOS.md; optional SCENARIO section, written only in a scenario game or
// once a flagship or secession exists): scripted starts, deterministic triggers, major-empire secession, flagships and the vendetta victory.
// schema 24 (the balance lane) adds the Expert difficulty (GALAXY_SETUP difficulty 5, docs/design/BALANCE-AI.md) and star types (optional STAR_TYPES section,
// written only while a system has one);
// schema 22 (the era lane) adds the campaign era (docs/design/ERAS.md; optional ERAS section, written only when the eras rule is on).
// schema 26 (C11) adds the optional SUBJUGATED section: minor worlds taken by invasion, which never secede (written only when one
// exists). Older saves are migrated on load (see load()).
// schema 27 (E3, E4; Simon's delegation 22 Nov 2026) adds worlds in peace deals and a renameable flagship: pending proposal commands and engine
// proposals may carry ceded/demanded world lists (written only when present, as a tail on the command line and a `proposalTerms` block in the
// engine text), and the optional FLAGSHIP_NAMES section holds the player-given names (written only while one exists). A schema-26 save loads
// unchanged, with no worlds in any proposal and the ships' default names.
// Schemas 4-26 still load and continue.
inline constexpr const char* ContentVersion = "remaster-27";
inline constexpr int SaveVersion = 27;
// D1 (22 Nov 2026): debt costs something, as in the original. With a negative treasury nothing can be bought with credits, and
// the morale of every colony of that empire drifts down towards DebtMoraleTarget (one point a turn, the normal drift) for as
// long as the treasury stays negative; below 50 a colony loses a quarter of its industry and research.
inline constexpr const char* DebtBlocker = "The treasury is in debt: nothing can be bought with credits until it is back above zero";
inline constexpr int DebtMoraleTarget = 30;
// Campaign empire ids are race slots: 1 Federation, 2 Klingon, 3 Romulan, 4 Cardassian, 5 Ferengi, 6 Dominion, 7 Borg,
// 8 Breen. A campaign seats two to eight of them (any subset); per-empire arrays are indexed by id, so they hold nine entries.
inline constexpr int MaxMajors = 8;
inline constexpr int MajorSlots = MaxMajors + 1;   // Index 0 is unused (neutral).
inline constexpr int LegacyMajors = 5;             // The five majors of schemas 4-16.
// Largest campaign save deserialize() accepts (and the CLI reads). A 300-turn
// five-empire Remastered soak peaks far below this; see GAMEPLAY-REMASTER.md.
inline constexpr std::size_t MaxSaveBytes = 8u * 1024u * 1024u;
namespace tactical { struct Encounter; }
enum class Ruleset : int { Prototype, Reference };
enum class StartingEra : int { Early = 1, Developed = 2, Expanded = 3, Advanced = 4, Supreme = 5 };
enum class TacticalStance : int { Balanced, Aggressive, Defensive, Withdraw };
enum class GovernorPriority : int { Manual = 0, Balanced = 1, Growth = 2, Research = 3, Manufacturing = 4 };
const char* governorPriorityName(GovernorPriority priority);
// Queue products include ships; existing Building name retained for source compatibility.
enum class Building : int { Farm, Factory, Laboratory, Shipyard, Scout, ColonyShip, PowerPlant, FusionPlant, ResearchInstitute, Destroyer, Transport, DefenseBattery,
    DilithiumRefinery /* schema 15 */,
    Structure /* schema 21: a race structure from the pack; Construction::referenceId = structures::RefBase + record id */ };
enum class CommandType : int {
    AssignWorkers, EnqueueBuilding, MoveFleet, Colonize, ToggleShipyard,
    CancelFirstConstruction, PrioritizeLastConstruction, MoveConstruction,
    RemoveConstruction, AllocateResearch, SendGift, OfferFriendship,
    OfferMembership, AttackFleet, RetreatFleet, Invade, DeclareWar,
    OfferPeace, EstablishTradeRoute, CancelTradeRoute, GatherIntelligence,
    Sabotage, SetTacticalStance, ProposeTradeTreaty,
    // Appended for campaign expansion:
    TerraformPlanet, CreateTaskForce, DisbandTaskForce, SetTaskForceOrder,
    // Appended for core UI features:
    BuyConstruction, ScrapStructure,
    // Appended for modern diplomacy & intelligence:
    SubmitDiplomaticProposal, RespondDiplomaticProposal,
    SetIntelBudget, SetIntelFocus,
    // Appended for colony automation:
    SetColonyGovernor, SetQueuePin, ClearGovernorOverride,
    // Appended for the schema-13 gameplay layer (GAMEPLAY-REMASTER.md):
    BuildOutpost, UpgradeStation, ResolveEventChoice,
    DefineBuildTemplate, DeleteBuildTemplate, ApplyBuildTemplate, SetDefaultBuildTemplate,
    StartMegaproject, CancelMegaproject,
    // Appended for schema 14 (treaty integration, DIPLOMACY.md 4.1):
    RefuseCallToArms,
    // Appended for schema 15 (EMPIRE-RULES.md). Both use systemId and eventOption.
    SetEmergencyProgram, ResolveAftermath,
    // Appended for schema 23 (ECONOMY-MECHANICS.md): fleetId plus eventOption (0 clear, 1 intercept, 2 raid); raid names the target colony in systemId.
    SetFleetOrder,
    // Appended for schema 25 (Scenarios.h): fleetId names the ship the empire commissions as its flagship.
    CommissionFlagship,
    // Appended for schema 27 (E4): fleetId names the flagship, templateName the new name (1-24 printable characters; empty restores the default).
    RenameFlagship
};
enum class CampaignOutcome : int { Ongoing, Victory, Defeat };
// MaxTechLevel is the classic research cap (every game without the eras rule); HighestTechLevel is the hard ceiling the data
// tables and saves allow. techCap(state) is the cap a campaign plays under.
inline constexpr int ResearchFields = 6, MaxTechLevel = 10, HighestTechLevel = 14;
enum class ResearchField : int { Biotech, Computers, Construction, Energy, Propulsion, Weapons };
struct ResearchTrack { int level = 0; std::int64_t progress = 0; int allocation = 0; };
struct Construction {
    Building product = Building::Farm;
    int work = 0;
    int referenceId = -1;
    bool pinned = false;
    Construction() = default;
    Construction(Building value, int invested = 0, int sourceId = -1, bool isPinned = false)
        : product(value), work(invested), referenceId(sourceId), pinned(isPinned) {}
    operator Building() const { return product; }
};

struct BuildingDefinition {
    Building id;
    const char* name;
    int industryCost;
    int energyCost;
    int creditUpkeep;
};
struct ConstructionChoice {
    Building product = Building::Farm;
    int referenceId = -1;
    std::string name;
    int industryCost = 0, creditUpkeep = 0;
    std::string blocker;
};
const BuildingDefinition& definition(Building building);

enum class PlanetClass : int { ClassM, ClassO, ClassP, ClassL, ClassH, ClassK, ClassY, GasGiant, Asteroids };
enum class TerraformingState : int { Uninhabitable, Untouched, InProgress, Terraformed };

struct Planet {
    int systemId = 0;
    int index = 0;
    std::string name;
    PlanetClass planetClass = PlanetClass::ClassM;
    TerraformingState terraformState = TerraformingState::Terraformed;
    int terraformWork = 0;
    int terraformNeeded = 0;
    int baseCapacity = 0;
    int maxCapacity = 0;
    int currentCapacity() const {
        if (terraformState == TerraformingState::Terraformed) return maxCapacity;
        if (terraformState == TerraformingState::Uninhabitable) return 0;
        return baseCapacity;
    }
};

struct System {
    int id = 0;
    int x = 0, y = 0;
    std::string name;
    std::vector<Planet> planets;
    int starType = -1;  // Schema 24: 0 yellow, 1 orange, 2 red, 3 white, 4 blue, 5 green, 6 violet; -1 = not set (the client draws its own).
    System() = default;
    System(int id_, int x_, int y_, std::string name_, std::vector<Planet> planets_ = {})
        : id(id_), x(x_), y(y_), name(std::move(name_)), planets(std::move(planets_)) {}
};

struct Colony {
    int systemId = 0;
    int ownerId = 1;
    int population = 40;
    int foodWorkers = 20, industryWorkers = 10, researchWorkers = 10;
    int farms = 2, factories = 2, laboratories = 1;
    int energy = 20;
    int overflow = 0; // Completed-project surplus; idle industry is never banked.
    std::vector<Construction> queue;
    int shipyards = 0;
    bool shipyardEnabled = true;
    int powerPlants = 0;   // Each contributes 10 energy and 1 credit upkeep.
    int fusionPlants = 0;  // Schema 12: each contributes 40 energy and 2 credit upkeep.
    int morale = 70;
    int defenseBatteries = 0;
    // Schema 15 (EmpireRules.h): dilithium refinery (0 or 1), emergency program (EmergencyProgram, 0 = none)
    // and the turns it has left. Saved in the optional EMPIRE_RULES section.
    int refineries = 0;
    // Schema 21 (Structures.h): built race structures, ascending buildings.json record ids. Saved in the optional STRUCTURES section.
    std::vector<int> structures;
    int program = 0;
    int programTurns = 0;
    // Schema 23 (EconomyRules.h): tier steps applied to farms, factories and laboratories, and whether the world was taken under the
    // Subjugate choice while the Protectorate rule is on. Saved in the optional ECONOMY_RULES section.
    std::array<int, 3> upgradeSteps{};
    bool subjugated = false;
    // Colony automation:
    GovernorPriority workforcePriority = GovernorPriority::Manual;
    GovernorPriority queuePriority = GovernorPriority::Manual;
    bool workforceManualOverride = false;
    std::string workforceExplanation;
    std::string queueExplanation;
    std::string governorWarning;
};
struct Command {
    std::uint64_t id = 0;
    CommandType type = CommandType::AssignWorkers;
    int systemId = 0;
    int foodWorkers = 0, industryWorkers = 0, researchWorkers = 0;
    Building building = Building::Farm;
    int fleetId = 0, targetX = 0, targetY = 0;
    int queueIndex = 0, queueDestination = 0;
    std::array<int, ResearchFields> researchAllocation{};
    int targetEmpire = 0, targetFleetId = 0, targetSystemId = 0;
    int referenceId = -1;
    TacticalStance stance = TacticalStance::Balanced;
    int taskForceId = 0;
    int planetIndex = 0;
    std::vector<int> memberFleetIds{};
    std::string taskForceName{};
    diplomacy::TreatyType proposalTreatyType = diplomacy::TreatyType::NonAggression;
    diplomacy::TreatyDuration proposalDuration = diplomacy::TreatyDuration::Turns25;
    diplomacy::DiplomaticTone proposalTone = diplomacy::DiplomaticTone::Cordial;
    std::vector<int> proposalCedeSystems{};    // Schema 27 (E3): a peace proposal hands these worlds to the target...
    std::vector<int> proposalDemandSystems{};  // ...and asks for these from it.
    std::int64_t proposalCreditOffer = 0;
    std::int64_t proposalCreditDemand = 0;
    bool proposalTargetIsMinor = false;
    std::uint64_t proposalId = 0;
    bool proposalAccept = false;
    std::string proposalReason{};
    int intelInternalSecurity = 0;
    std::vector<int> intelRivalIds{};
    std::vector<int> intelEspionagePcts{};
    std::vector<int> intelSabotagePcts{};
    intelligence::IntelDepartment intelDepartmentFocus = intelligence::IntelDepartment::General;
    GovernorPriority governorWorkforce = GovernorPriority::Manual;
    GovernorPriority governorQueue = GovernorPriority::Manual;
    bool queuePinned = false;
    // Schema-13 gameplay layer (zero/empty for every other command type):
    int stationId = 0;                       // UpgradeStation
    int eventId = 0, eventOption = 0;        // ResolveEventChoice
    int templateId = 0;                      // Define/Delete/Apply/SetDefault build template
    std::string templateName{};                // DefineBuildTemplate
    std::vector<Construction> templateItems{}; // DefineBuildTemplate (product + referenceId only)
    // Schema 14: RefuseCallToArms names the defender in targetEmpire and the aggressor here.
    int callAggressor = 0;
};
struct Fleet {
    int id = 0, ownerId = 1;
    int x = 0, y = 0, targetX = 0, targetY = 0;
    bool colonyShip = false, colonize = false;
    Building shipClass = Building::Scout;
    int hull = 12;
    int attackTarget = 0;
    bool retreat = false;
    int invadeSystemId = 0;
    int referenceShipId = -1;
    int shields = 0;
    TacticalStance stance = TacticalStance::Balanced;
    int taskForceId = 0;
    bool terraforming = false;
    int terraformSystemId = 0;
    int terraformPlanetIndex = 0;
    // Schema 13: a troop transport assembling an outpost in its current sector.
    bool buildingOutpost = false;
    int outpostProgress = 0; // Turns of assembly completed.
    // Schema 23 (EconomyRules.h): standing order (0 none, 1 intercept, 2 raid), the sector an intercepting fleet guards, the colony a
    // raider works on, and the crew's experience points (25 = a fresh Regular crew). Saved in the optional ECONOMY_RULES section.
    int order = 0;
    int postX = 0, postY = 0;
    int raidSystemId = 0;
    int experience = 25;
    // Schema 25 (Scenarios.h): 0 for an ordinary ship, else the 1-based index of its flagship name; the owner's flagship bonuses apply to
    // the ship. Saved in the FLAGSHIPS rows of the optional SCENARIO section, not in the fleet rows.
    int flagship = 0;
    std::string flagshipLabel{};   // (default initializer keeps -Wmissing-field-initializers quiet for positional Fleet{...}) Schema 27 (E4): the player's name for this flagship, a cache of Flagship::name (saved in FLAGSHIP_NAMES, not in the fleet rows).
};

struct TaskForce {
    int id = 0;
    int ownerId = 1;
    std::string name;
    std::vector<int> fleetIds;
    TacticalStance stance = TacticalStance::Balanced;
    int targetX = 0, targetY = 0;
    int attackTarget = 0;
};

struct EmpireState {
    int id = 1;
    std::int64_t credits = 100, research = 0;
    std::vector<int> explored;
    std::array<ResearchTrack, ResearchFields> technologies{{{0,0,17},{0,0,17},{0,0,17},{0,0,17},{0,0,16},{0,0,16}}};
    std::int64_t researchReserve = 0;
    std::int64_t intelligence = 0;
};
struct MajorRelation {
    int first = 1, second = 2;
    bool contact = false, atWar = false, tradeTreaty = false;
    int warStartTurn = 0, trust = 50;
};
struct MajorOffer { std::uint64_t id = 0; int proposer = 0, target = 0, dueTurn = 0; CommandType type = CommandType::OfferPeace; };
struct TradeRoute { int ownerId = 1, fromSystemId = 0, toSystemId = 0; };
struct IntelReport { int turn = 0, viewer = 1, target = 2, systemId = 0; bool success = false; int knownColonies = 0; std::int64_t estimatedCredits = 0; };
struct FleetDamage { int fleetId = 0, hullBefore = 0, hullAfter = 0, shieldBefore = 0, shieldAfter = 0; };
struct CombatParticipant {
    int fleetId = 0, ownerId = 0;
    Building shipClass = Building::Scout;
    int referenceShipId = -1, initialHull = 0, initialShields = 0;
};
// Schema 14: what one ship did in one recorded round of a unified-resolver battle,
// so replays from saves and older turns can show orders, not only damage.
struct CombatShipOrder {
    int fleetId = 0, targetFleetId = 0;
    int maneuver = 0;          // tactical::TacticalManeuver executed (0 Engage ... 11 Hail)
    bool aiControlled = false; // Chosen by the tactical AI rather than a player or standing order
    bool decloaked = false;    // Dropped its cloak to attack (ambush volley)
    bool disengaging = false;  // Retreating this round but not yet clear of the battle
    bool hailed = false;       // Offered a truce
    bool rammed = false;       // Carried out a ram impact
    bool operator==(const CombatShipOrder&) const = default;
};
// Tactical rules 3 presentation event stream (COMBAT-RULES.md, "Flight model and weapon events").
// One round is CombatTicksPerRound ticks long; events are ordered by tick, then by the order the
// resolver produced them. Clients animate maneuvers, beams, pulses and torpedoes from this stream.
inline constexpr int CombatTicksPerRound = 12;
enum class CombatEventKind : int {
    ManeuverStart = 0, // fleetId begins its move: x, y, facing = start pose; maneuver = order executed
    ManeuverEnd = 1,   // fleetId ends its move: x, y, facing = end pose; value = cells slid sideways (strafe)
    Cloak = 2,         // fleetId is cloaked (emitted at tick 0 of round 1 for ships that start cloaked)
    Decloak = 3,       // fleetId drops its cloak to attack (ambush volley this round)
    Fire = 4,          // One weapon discharge: fleetId -> targetFleetId from mount mountIndex (see fields)
    Ram = 5,           // fleetId rams targetFleetId (impact damage in shieldDamage/hullDamage)
    Destroyed = 6,     // fleetId destroyed (tick = impact tick of the killing shot)
    Retreated = 7,     // fleetId jumped clear of the battle
    Disengage = 8,     // fleetId starts disengaging (Retreat, weapons offline)
    Truce = 9,         // The battle ends under a hailed truce (fleetId 0)
};
inline constexpr int MaxCombatEventKind = 9;
struct CombatEvent {
    int tick = 0;              // 0..CombatTicksPerRound-1 within the round
    CombatEventKind kind = CombatEventKind::Fire;
    int fleetId = 0;           // Actor: shooter, maneuvering, cloaking, destroyed or retreating ship
    int targetFleetId = 0;     // Fire/Ram: target (0 otherwise)
    int mountIndex = -1;       // Fire: index into the shooter's weapon mounts (0..3), else -1
    int weaponType = 0;        // Fire: roster::WeaponType value; asset key "weapon." + roster::weaponTypeName
    int weaponFamily = 0;      // Fire: roster::WeaponFamily (0 beam, 1 pulse, 2 torpedo)
    int impactTick = 0;        // Fire: tick the shot lands (== tick for beams and pulses; later for torpedoes)
    int projectiles = 0;       // Fire: visual count (beam emitters, pulse bolts or torpedoes in the volley)
    bool hit = false;          // Fire: the shot connected (false = missed, intercepted or target already gone)
    bool intercepted = false;  // Fire: torpedo volley shot down by point defence
    int shieldDamage = 0;      // Fire/Ram: damage absorbed by the target's shields
    int hullDamage = 0;        // Fire/Ram: damage to the target's hull
    bool destroyed = false;    // Fire/Ram: this impact destroyed the target
    int x = 0, y = 0;          // Maneuver events: pose; Fire: shooter position at launch
    int facing = 0;            // Degrees [0, 359] (multiples of 45 under rules 3)
    int maneuver = 0;          // Maneuver events: tactical::TacticalManeuver executed
    int value = 0;             // ManeuverEnd: lateral cells slid; otherwise 0
    bool operator==(const CombatEvent&) const = default;
};
struct CombatRound {
    int index = 0;
    std::vector<FleetDamage> damage;
    std::vector<int> lostFleetIds, retreatedFleetIds;
    std::vector<CombatShipOrder> orders; // Empty for legacy-resolver and pre-schema-14 battles.
    std::vector<CombatEvent> events;     // Tactical rules 3 only (optional COMBAT_EVENTS save section).
};
struct CombatReport {
    int turn = 0, x = 0, y = 0, firstOwner = 0, secondOwner = 0, winnerOwner = 0;
    std::vector<int> firstFleetIds, secondFleetIds, lostFleetIds, retreatedFleetIds;
    std::vector<FleetDamage> damage;
    std::vector<CombatParticipant> participants;
    std::vector<CombatRound> rounds;
};
struct InvasionReport {
    int turn = 0, systemId = 0, attackerOwner = 0, defenderOwner = 0, fleetId = 0;
    int attackStrength = 0, defenseStrength = 0, casualties = 0;
    bool conquered = false;
};
struct Minor {
    int systemId = 0;
    std::string name;
    int memberOf = 0;
    std::array<bool, MajorSlots> contact{};
    std::array<int, MajorSlots> attitude{0,60,60,60,60,60,60,60,60};
    std::array<int, MajorSlots> friendshipTurn{-1,-1,-1,-1,-1,-1,-1,-1,-1};
    // Minor-race roster (Lane G, MinorRaces.h; optional MINOR_RACES save section).
    // raceId 0 = legacy minor without roster progression; the other three are then 0.
    int raceId = 0;
    int techLevel = 0, population = 0, progress = 0;
    // C11 (22 Nov 2026, schema 26): a minor world taken by invasion is subjugated, not a member: it never secedes. Treaty-joined
    // members keep the loyalty/secession rule. Cleared whenever the minor leaves its empire or joins one by treaty.
    bool subjugated = false;
};
// Schema 15: a captured world waiting for the player's Liberate / Subjugate / Exterminate choice.
// An undecided choice lapses to Subjugate (the pre-15 behaviour) after AftermathLapseTurns.
struct AftermathChoice {
    int systemId = 0, conqueror = 0, formerOwner = 0; // formerOwner 0 = an independent minor
    int turn = 0;                                     // Turn of the capture
    int populationBefore = 0;                         // Population before the capture's casualties
    bool operator==(const AftermathChoice&) const = default;
};
// Schema 15: a minor race's signature ship, a map object at its homeworld (not a fleet).
struct MinorGuard {
    int systemId = 0, shipId = 0, hull = 0, shields = 0;
    bool operator==(const MinorGuard&) const = default;
};
// Schema 25 (Scenarios.h): an empire's flagship (fleetId 0 and lostTurn > 0 once it was lost; serial counts the commissions) and a secession
// (parent empire, the new house, the turn).
struct Flagship {
    int empireId = 0, fleetId = 0, serial = 0, lostTurn = 0;
    std::string name;   // Schema 27 (E4): the player's name for the ship (empty: the pack's default name for the serial). Saved in FLAGSHIP_NAMES.
    bool operator==(const Flagship&) const = default;
};
struct Secession {
    int parent = 0, house = 0, turn = 0;
    bool operator==(const Secession&) const = default;
};
// Schema 20: threat actors (Threats.h). A threat is a map object with its own hull and shields, not a Fleet and not an empire:
// it fights through the tactical engine as a proxy combatant on the side ThreatOwner, under the fleet id ThreatCombatantBase + id.
inline constexpr int ThreatOwner = 9;
inline constexpr int ThreatCombatantBase = 3000000;
inline constexpr int ThreatKinds = 12;
enum class ThreatKind : int {
    BorgCube = 0, BorgSphere = 1, Species8472 = 2, Husnock = 3, Calamarain = 4, Chodak = 5, CrystallineEntity = 6,
    EchoPapa = 7, EdoGuardian = 8, Gomtuu = 9, Tarellian = 10, DeepSpaceNine = 11
};
// New-game switches. All off by default, so a game that never mentions threats plays and saves exactly as before.
struct ThreatSetup {
    bool monsters = false;       // The vanilla monsters (everything except the Borg).
    bool borg = false;           // The Borg threat (a separate switch: the Borg-only off switch).
    int frequency = 2;           // 0 none, 1 rare, 2 normal, 3 frequent.
    int disabledMask = 0;        // Bit (1 << ThreatKind): that actor never spawns.
    bool borgSphere = false;     // A Borg sphere replaces the cube.
    bool species8472 = false;    // Species 8472 replaces the Husnock.
    bool edoStation = false;     // A peaceful Deep Space Nine replaces the Edo Guardian.
    bool sparesAi = false;       // Actors ignore every empire but the player.
    constexpr bool operator==(const ThreatSetup&) const = default;
};
struct ThreatActor {
    int id = 0, kind = 0;
    int x = 0, y = 0, targetX = 0, targetY = 0;
    int hull = 0, shields = 0;
    int spawnTurn = 0;
    int adapt = 0;   // Borg adaptation: percent added to its shields (and attack) after surviving battles.
    int siege = 0;   // Turns spent besieging the colony at its sector.
    int kills = 0;   // Systems assimilated or colonies destroyed (Borg despawn counter).
    bool operator==(const ThreatActor&) const = default;
};
// A system the Borg threat holds (no colony: the population was assimilated) or a colony a Borg empire assimilated (ownerId 7).
struct AssimilatedSystem {
    int systemId = 0, turn = 0, ownerId = 0;
    bool operator==(const AssimilatedSystem&) const = default;
};
struct DiplomaticOffer {
    std::uint64_t id = 0;
    int empireId = 1, systemId = 0, dueTurn = 0;
    CommandType type = CommandType::OfferFriendship;
};
// ---------------------------------------------------------------------------
// Schema-13 gameplay layer (docs/GAMEPLAY-REMASTER.md). All values are integers.
// ---------------------------------------------------------------------------
// Ruleset presets. Classic is original-like (outposts and starbases, the original
// random events, conquest/domination only). Remastered switches every new layer on.
// Saves from schema 12 and older load as Classic with every layer off, so they
// keep playing exactly as before.
enum class RulesetPreset : int { Classic = 0, Remastered = 1 };
struct GameplayRules {
    RulesetPreset preset = RulesetPreset::Classic;
    bool stations = false;             // Outposts and starbases.
    bool galaxyFeatures = false;       // Nebulae, wormholes, black holes, neutron stars, ion storms.
    bool curatedEvents = false;        // Event deck (replaces the original random events).
    bool alternativeVictories = false; // Scientific megaproject and diplomatic victory.
    // Expanded ship roster (docs/SHIP-ROSTER.md): canon classes with ids >= 1000 become buildable.
    // Remastered only; saved as the optional SHIP_ROSTER section (no schema bump), so Classic games,
    // older saves and Remastered saves made before the roster keep it off and play exactly as before.
    bool expandedRoster = false;
    // Tactical flight model and typed weapons (tactical rules 3, COMBAT-RULES.md). Remastered only; saved as
    // the optional TACTICAL_FLIGHT section, so saves without it keep tactical rules 2 exactly.
    bool flightModel = false;
    // Tactical rules 4 (WEGO, facings, subsystems; COMBAT-RULES.md). Needs flightModel. Saved as the optional
    // TACTICAL_WEGO section, so older saves load with it off and keep rules 3.
    bool wegoCombat = false;
    // Schema 15: dilithium ship slots, emergency programs, invasion aftermath, minor conquest and guard ships
    // (EMPIRE-RULES.md). Saved as the optional EMPIRE_RULES section, so older saves keep it off.
    bool empireRules = false;
    // Schema 22: campaign era (ERAS.md): a timeline year, era-gated roster classes, the pack's start tech levels and research
    // cap (10..14). Needs expandedRoster and a pack with an eras block. Saved as the optional ERAS section.
    bool eras = false;
    // Schema 23 (EconomyRules.h, docs/design/ECONOMY-MECHANICS.md). The first three are the original game's rules and start on in both
    // bases (rulesForPreset); older saves load with them off. Each is saved as one bit of the optional ECONOMY_RULES section.
    bool structureUpgrades = false;  // Farms, factories and laboratories can be upgraded in tiers as the matching technology advances.
    bool autoUpgrades = false;       // Colonies with an empty queue queue the cheaper automatic upgrade themselves.
    bool noUpgrades = false;         // Variant: structures never upgrade (blocks both of the above).
    bool sharedMorale = false;       // A shared morale structure lifts every colony of its empire.
    bool protectorateHq = false;     // Subjugated worlds are restless until a Protectorate HQ stands among them.
    bool allDilithium = false;       // Every minor race's homeworld has dilithium.
    bool interceptOrders = false;    // Warships can hold an intercept post.
    bool raidOrders = false;         // Warships can raid an enemy colony's treasury and trade.
    bool raidBonus = false;          // Orion and Maquis hulls and members raid better.
    bool crewExperience = false;     // Fleets carry crew experience, academies and battle feed it, it shows in combat.
    bool exploitFixes = false;       // Terraforming cannot be stacked or finish in one turn; upgrades do not refund.
    bool operator==(const GameplayRules&) const = default;
};

enum class StationType : int { Outpost = 0, Starbase = 1 };
struct Station {
    int id = 0, ownerId = 1;
    int x = 0, y = 0;
    StationType type = StationType::Outpost;
    int hull = 1, shields = 0; // Current values; maxima come from stationStats().
    int builtTurn = 0;
};
// A starbase upgrade funded by one colony's industry (the colony's queue pauses).
struct StationUpgrade { int stationId = 0, fundingSystemId = 0, work = 0; };
struct StationStats {
    int maxHull = 0, maxShields = 0, beam = 0, torpedo = 0;
    int supplyRadius = 0, sensorRadius = 0, upkeep = 0;
    int repairPercent = 0; // Hull repaired per turn for friendly ships in the sector.
};
struct StationUpgradeCost { std::int64_t credits = 0; int industry = 0; };
// Tactical encounters and battle records identify a station as this fleet ID.
inline constexpr int StationCombatantBase = 2000000;

enum class GalaxyFeatureType : int { Nebula = 0, Wormhole = 1, BlackHole = 2, NeutronStar = 3, IonStorm = 4, SubspaceRift = 5 };
struct GalaxyFeature {
    int id = 0;
    GalaxyFeatureType type = GalaxyFeatureType::Nebula;
    int x = 0, y = 0;          // Ion storms: centre of a plus-shaped storm front.
    int pairId = 0;            // Wormhole: the other endpoint's feature ID.
    int clusterId = 0;         // Nebula cells of one cloud share a cluster (one survey reward).
    int driftX = 0, driftY = 0; // Ion storm drift direction (moves every IonStormDriftTurns).
    int expiresTurn = 0;       // Temporary features (subspace rifts) vanish after this turn; 0 = permanent.
    int surveyedMask = 0;      // Bit (1 << empireId, id 1..8) once that empire collected the survey reward.
};
inline constexpr int IonStormDriftTurns = 2;

enum class GalacticEventKind : int {
    Plague = 0, SupernovaWarning = 1, SubspaceRift = 2, TradeBoom = 3,
    DerelictVessel = 4, LaborUnrest = 5, ScientificBreakthrough = 6
};
struct GalacticEvent {
    int id = 0;
    GalacticEventKind kind = GalacticEventKind::Plague;
    int empireId = 0;          // Affected empire (0 = galaxy-wide).
    int systemId = 0;          // Affected system (0 = none).
    int x = -1, y = -1;        // Affected sector (-1 = none).
    int startTurn = 0, endTurn = 0; // endTurn: last turn of effects / detonation turn.
    int choiceDeadline = 0;    // Last turn a choice may be submitted (0 = no choice).
    int choice = -1;           // -1 undecided, otherwise the option index.
    bool choiceApplied = false;
    int magnitude = 0;         // Kind-specific parameter (e.g. plague loss percent).
    int featureId = 0;         // Feature spawned by the event (subspace rift).
};
struct EventChoiceInfo { int index = 0; std::string label, effect; bool isDefault = false; };

struct BuildTemplate {
    int id = 0, ownerId = 1;
    std::string name;
    std::vector<Construction> items; // product + referenceId (work 0, unpinned)
    bool autoApply = false;          // Default template for newly acquired colonies.
};

struct Megaproject { int ownerId = 1, systemId = 0, work = 0, startTurn = 0; };

// Vendetta (schema 25): the vanilla victory, defeat both of your designated racial rivals (Victory.cpp).
enum class VictoryCondition : int { Conquest = 0, Domination = 1, Scientific = 2, Diplomatic = 3, Vendetta = 4 };
struct VictoryProgress {
    int empireId = 0;
    VictoryCondition condition = VictoryCondition::Conquest;
    bool enabled = true;
    bool achieved = false;
    int percent = 0; // 0-100
    std::string explanation;
};

enum class ChronicleCategory : int {
    Construction = 0, Research = 1, Exploration = 2, Colonization = 3, Combat = 4,
    Diplomacy = 5, Intelligence = 6, Events = 7, Stations = 8, Victory = 9
};
struct ChronicleEntry {
    ChronicleCategory category = ChronicleCategory::Events;
    std::string text;
    int x = -1, y = -1;       // Sector (-1 = none)
    int systemId = 0, fleetId = 0, stationId = 0, eventId = 0;
    int empireId = 0, otherEmpireId = 0;
    int eventIndex = -1;      // Index of the matching line in TurnReport::events.
};

// Galaxy setup (docs/design/GALAXY-GENERATOR.md). The first value of each enum is the pre-schema-15 behaviour
// (uniform scatter; minors = systems / 5; Normal difficulty), so a setup that names none of them plays as before.
enum class GalaxyShape : int { Legacy = 0, Irregular = 1, Spiral = 2, Ring = 3, Elliptical = 4 };
enum class MinorDensity : int { Legacy = 0, None = 1, Few = 2, Some = 3, Many = 4 };
// Expert (schema 24) is saved as 5 so Impossible keeps its 4; it plays in tier order between Hard and Impossible (difficultyRank).
enum class Difficulty : int { Simple = 0, Easy = 1, Normal = 2, Hard = 3, Impossible = 4, Expert = 5 };
inline constexpr std::array<Difficulty, 6> DifficultyOrder{Difficulty::Simple, Difficulty::Easy, Difficulty::Normal, Difficulty::Hard, Difficulty::Expert, Difficulty::Impossible};
struct GalaxySetup {
    GalaxyShape shape = GalaxyShape::Legacy;
    MinorDensity density = MinorDensity::Legacy;
    Difficulty difficulty = Difficulty::Normal;
    constexpr bool operator==(const GalaxySetup&) const = default;
};
struct GameState : EmpireState {
    std::uint64_t seed = 0, rngState = 0;
    std::uint64_t lastAcceptedCommand = 0;
    int width = 0, height = 0, turn = 0;
    std::vector<System> systems;
    std::vector<Colony> colonies;
    std::vector<Command> pending;
    int playerEmpire = 1;
    std::vector<Fleet> fleets;
    int nextFleetId = 3;
    std::vector<EmpireState> rivals;
    std::vector<Minor> minors;
    std::vector<DiplomaticOffer> diplomacy;
    std::uint64_t nextDiplomacyId = 1;
    bool populationEnabled = false; // Old saves and isolated fixtures retain their frozen population rules.
    std::vector<MajorRelation> majorRelations;
    std::vector<MajorOffer> pendingMajorOffers;
    std::uint64_t nextMajorOfferId = 1;
    std::vector<TradeRoute> tradeRoutes;
    std::vector<IntelReport> intelReports;
    std::vector<CombatReport> latestBattles;
    CampaignOutcome outcome = CampaignOutcome::Ongoing;
    int winnerEmpire = 0;
    bool eventsEnabled = true;
    bool aiAggressionEnabled = true;
    // Quick Start (26 Nov 2026, onboarding lane): no AI empire declares war before this turn. 0 (every other game) holds nothing back.
    // Only the new-game option "aiWarHoldTurn" sets it. Saved as the optional AI_WAR_HOLD section (written only when non-zero).
    int aiWarHoldTurn = 0;
    // F5 observer mode (soaks only; no game, screen or save sets it, and it is not serialized): the AI also plays the
    // player's seat, the player's elimination does not end the campaign, and the campaign ends when one empire is left or
    // any empire meets a victory condition (the player's own domination test applied to every empire). Rules are unchanged.
    bool observerMode = false;
    // P1 (Simon's yes, 20 Nov 2026): a diplomatic win must hold for DiplomaticVictoryHoldTurns consecutive turns. Each entry is
    // (empire id, consecutive turns the empire has met every diplomatic condition), ascending id, only while the count is >= 1.
    // Saved as the optional DIPLOMATIC_HOLD section (schema 25, written only when non-empty).
    std::vector<std::pair<int, int>> diplomaticHold;
    // Hotseat multiplayer (MULTIPLAYER.md, 20 Nov 2026): every human empire, ascending, the seated one (playerEmpire) included;
    // empty in a one-player game. The AI plans for none of them; switchSeat() hands the chair from one to the next. Saved as the
    // optional HOTSEAT section (schema 25, written only in a hotseat game).
    std::vector<int> hotseatHumans;
    // C4 (Simon, 20 Nov 2026, "keep playing"): the turn a human winner chose to play on after a domination win, 0 while not.
    // From then domination no longer ends the campaign; conquest and the other victories still do. Saved as the optional
    // PLAYING_ON section (written only when set).
    int playingOnFrom = 0;
    // C7 (21 Nov 2026, Simon's delegation): the New Campaign domination setting, on by default. Off, domination never ends the
    // campaign and conquest is the military win. Saved as the optional NO_DOMINATION section (written only when off).
    bool dominationEnabled = true;
    Ruleset ruleset = Ruleset::Prototype;
    int lastRandomEventTurn = 0;
    bool advancedRulesEnabled = false; // Schema 4-9 games retain their prior combat/diplomacy behavior.
    bool campaignExpansionEnabled = false; // Schema 4-10 games retain legacy economy without planets/tasks.
    std::vector<TaskForce> taskForces;
    int nextTaskForceId = 1;
    diplomacy::DiplomacyState diplomacyEngineState;
    std::map<int, intelligence::EmpireIntelState> empireIntelStates;
    std::vector<intelligence::OperationRecord> intelOperations;
    bool modernEnginesActive = false;
    StartingEra startingEra = StartingEra::Early;
    // Schema 12: every battle (player and AI-vs-AI) resolves through the tactical
    // engine, in advance() and in campaign sessions alike. Schema 4-11 saves load
    // with false and keep the legacy strategic resolver in headless advance().
    bool unifiedCombat = false;
    // Schema-13 gameplay layer (all empty/off for schema 4-12 saves):
    GameplayRules rules;
    std::vector<Station> stations;
    int nextStationId = 1;
    std::vector<StationUpgrade> stationUpgrades;
    std::vector<GalaxyFeature> galaxyFeatures;
    int nextFeatureId = 1;
    std::vector<GalacticEvent> galacticEvents;
    int nextEventId = 1;
    int lastCuratedEventTurn = 0;
    std::vector<int> eventDeck;   // GalacticEventKind values in draw order.
    int eventDeckPosition = 0;
    std::vector<BuildTemplate> buildTemplates;
    int nextBuildTemplateId = 1;
    std::vector<Megaproject> megaprojects;
    GalaxySetup galaxy; // Schema 15: how this galaxy was generated and how hard the rivals play.
    // Schema 16 (all empty for older saves and when rules.empireRules is off):
    std::vector<AftermathChoice> aftermaths;
    std::vector<MinorGuard> minorGuards;
    // Schema 17: the preset whose overlay packs this campaign plays on. Empty for a campaign on a plain pack, which
    // includes the Classic and Remastered presets (they have no overlays). Saved in the optional PACKS section.
    std::string presetId;
    // Schema 20 (all empty and default for older saves and for games with the threat switches off; Threats.h):
    ThreatSetup threats;
    std::vector<ThreatActor> threatActors;
    int nextThreatId = 1;
    std::array<int, ThreatKinds> threatLastSpawn{}; // Turn each kind last spawned (0 = never); spacing between spawns.
    std::vector<AssimilatedSystem> assimilated;
    // Schema 25 (Scenarios.h; all empty for older saves and free games). The scenario this campaign plays (empty = none), the triggers that
    // have fired (indexes, ascending), each empire's flagship and the secessions that have happened.
    std::string scenarioId;
    std::vector<int> scenarioFired;
    std::vector<Flagship> flagships;
    std::vector<Secession> secessions;
};
struct Economy {
    int food = 0, foodDeficit = 0, industry = 0, research = 0;
    int energyUsed = 0, inactiveFactories = 0, inactiveLabs = 0;
    int income = 0, upkeep = 0;
    bool shipyardPowered = false;
    int poweredBatteries = 0, inactiveBatteries = 0;
    int energyAvailable = 0;
    bool refineryPowered = false; // Schema 15: the colony's dilithium refinery has its 5 energy.
};
struct ColonyReport {
    int systemId = 0;
    Economy economy;
    std::string completed;
};
struct PopulationForecast { int minimum = 0, maximum = 0, upwardChances = 0; }; // Chance denominator 20000.
inline constexpr int PrototypePopulationCapacity = 100, PrototypeGrowthBasisPoints = 200;
PopulationForecast populationForecast(int population, int food, int capacity, int employment, int growthBasisPoints);
// Treasury breakdown by source. creditForecast() predicts the coming turn; the
// same structure in TurnReport::ledger records what the turn actually did.
struct CreditForecast {
    int empireId = 0;
    std::int64_t treasury = 0;          // Credits before the turn.
    std::int64_t orders = 0;            // Credits pending orders spend when the turn starts (rush buys, upgrades; <= 0).
    // Income by source.
    std::int64_t colonyIncome = 0;      // Colony revenue (population).
    std::int64_t tradeRouteIncome = 0;  // Active trade routes.
    std::int64_t treatyDividends = 0;   // Friendship/affiliation/alliance dividends (modern diplomacy).
    std::int64_t eventIncome = 0;       // Curated events: trade boom, derelict salvage.
    // Upkeep by source.
    std::int64_t buildingUpkeep = 0;    // Colony structures.
    std::int64_t fleetUpkeep = 0;       // Ship maintenance.
    std::int64_t stationUpkeep = 0;     // Outposts and starbases.
    std::int64_t eventCosts = 0;        // Paid event decisions (cure, shielding, meeting demands).
    std::int64_t income = 0, upkeep = 0, net = 0; // income - upkeep
    // Ledger only: changes no forecast can know in advance (gifts and tribute answered
    // this turn, espionage theft, refunded minor gifts). Always 0 in a forecast.
    std::int64_t other = 0;
    std::int64_t nextTreasury = 0;      // treasury + orders + net (+ other in the ledger).
    bool operator==(const CreditForecast&) const = default;
};
struct TurnReport {
    int turn = 0;
    std::int64_t creditsBefore = 0, creditsAfter = 0;
    std::int64_t researchGained = 0;
    std::vector<std::uint64_t> appliedCommands;
    std::vector<ColonyReport> colonies;
    std::uint64_t stateHash = 0;
    std::vector<std::string> events;
    int fleetUpkeep = 0;
    int tradeIncome = 0;
    std::vector<CombatReport> battles;
    std::vector<InvasionReport> invasions;
    CampaignOutcome outcome = CampaignOutcome::Ongoing;
    // Schema 13: structured, categorized "Turn Briefing" covering every line of
    // `events` (same order) plus structured references. Player-visible only.
    std::vector<ChronicleEntry> chronicle;
    // Schema 14: the player's treasury movement this turn, by source (see CreditForecast).
    CreditForecast ledger;
};

const char* startingEraName(StartingEra era);
inline const char* eraName(StartingEra era) { return startingEraName(era); }
bool startingEraValid(StartingEra era);
int eraBaseTechLevel(StartingEra era, Ruleset ruleset = Ruleset::Reference);
// Schema 22 (ERAS.md). All of these are the classic answers when the eras rule is off.
int techCap(const GameState& state);                  // Highest research level: 10, or the pack's cap (up to 14) under eras.
int eraStartTech(const GameState& state);             // Tech level every field starts at for the campaign's start era.
int campaignYear(const GameState& state);             // Timeline year: the start era's year plus turn / turnsPerYear (eras rule only; 0 otherwise).
int eraStartYear(StartingEra era);                    // The year a start era begins (the active pack's table; 2240..2380 by default).
// Why this roster class cannot be built yet or any more under the campaign's timeline; empty when it can (or the rule is off).
std::string eraBlocker(const GameState& state, int shipId);
void applyCampaignEra(GameState& state, StartingEra era);
std::vector<StartingEra> availableStartingEras();
StartingEra campaignStartingEra(const GameState& state);

// What a difficulty changes (percent; 100 = unchanged). The player's own economy is never touched.
struct DifficultyProfile {
    int aiIncomePct, aiIndustryPct, aiResearchPct; // Rival colonies' credit income, industry and research output.
    int aiStartCreditsPct;                         // Rival treasury at turn 0.
    int aiWarConfidencePct;                        // Scales the military power a rival sees in itself when it weighs a war.
    int intelThreshold;                            // Added to the attack-readiness threshold of rival spy ops against the
                                                   // player (more is safer); subtracted from the player's against rivals.
};
const DifficultyProfile& difficultyProfile(Difficulty d);
int difficultyRank(Difficulty d);  // 0..5 in play order (Simple, Easy, Normal, Hard, Expert, Impossible).
// Schema 24 (BALANCE-AI.md section 4): how the AI plays. Pack flags (balance.ai*) switch single behaviours on for a preset; the Hard tier
// adds the standard build order, Expert and Impossible add all four. All false for a default Normal game.
struct AiPolicy { bool alwaysStarbases = false, newestShips = false, standardBuild = false, aggressiveExpansion = false; };
AiPolicy aiPolicy(const GameState& state);
// Pack balance values read by the rules (identity for the base pack).
int minorGiftCost();
int minorGiftAttitude();
int packWeaponOutputPercent();

const char* galaxyShapeName(GalaxyShape s);
const char* minorDensityName(MinorDensity d);
const char* difficultyName(Difficulty d);
bool parseGalaxyShape(const std::string& text, GalaxyShape& out);   // "irregular"/"spiral"/"ring"/"elliptical" (case-insensitive)
bool parseMinorDensity(const std::string& text, MinorDensity& out); // "none"/"few"/"some"/"many"
bool parseDifficulty(const std::string& text, Difficulty& out);     // "simple".."impossible"
// Minor races a density gives for a galaxy of this many systems (vanilla per-size tables, capped at 29).
// Legacy density gives the pre-15 count (systems / 5).
int minorRaceTarget(MinorDensity density, int systemCount);
GameState generate(std::uint64_t seed, int width = 12, int height = 8, int count = 12, GalaxyShape shape = GalaxyShape::Legacy);
// Campaigns generated before the star-name table named their systems System-<id>; this gives each such system (and its
// planets) the name generate() now gives that id, unless another system already has it. Returns the systems renamed.
int adoptStarNames(GameState& s);
// `lineup` (schema 17) lists the empire ids (race slots 1..8) seated in the campaign, in seating order: the first takes the
// origin star, the rest are placed farthest-first. Empty means slots 1..`empires`; a non-empty lineup overrides `empires`.
// `playerEmpire` is an empire id and must be in the lineup.
GameState generateCampaign(std::uint64_t seed, int empires = 2, int width = 16, int height = 12, int systems = 28, int playerEmpire = 1, StartingEra era = StartingEra::Early, GalaxyShape shape = GalaxyShape::Legacy, const std::vector<int>& lineup = {});
GameState generateReferenceCampaign(std::uint64_t seed, int empires = 2, int width = 16, int height = 12, int systems = 28, int playerEmpire = 1, StartingEra era = StartingEra::Early, GalaxyShape shape = GalaxyShape::Legacy, const std::vector<int>& lineup = {});
GameState generateCampaignWithEra(std::uint64_t seed, StartingEra era, int empires = 2, int width = 16, int height = 12, int systems = 28, int playerEmpire = 1);
GameState generateReferenceCampaignWithEra(std::uint64_t seed, StartingEra era, int empires = 2, int width = 16, int height = 12, int systems = 28, int playerEmpire = 1);
const EmpireState& empire(const GameState& state, int id);
const char* empireName(int id);
const Minor* minorAt(const GameState& state, int systemId);
const MajorRelation* majorRelation(const GameState& state, int first, int second);
const char* shipName(Building shipClass);
int shipMaxHull(Building shipClass);
int shipAttack(Building shipClass);
std::string fleetName(const Fleet& fleet);
// E2 (Simon's delegation, 22 Nov 2026): the odds of a ground assault, shown before the order is given. `attack` is the troops of the
// transport and of every other transport of its empire ordered to the same world (they land together, Round C1); `defense` is the world's
// ground defence as the resolution reckons it (population / 5, morale, batteries, the owner's bonuses, shields); `percent` is the chance
// the landing wins (attack^2 against attack^2 + defense^2). `blocker` is empty when the landing can go ahead, else why not (a defending
// ship or starbase must be beaten first). No ground combat is played; the 20 Nov ruling stands.
struct InvasionOdds {
    int attack = 0, defense = 0, percent = 0, transports = 0;
    bool defended = false;
    std::string blocker;
};
InvasionOdds invasionOdds(const GameState& state, int fleetId, int systemId);
// E3 (schema 27): the war score of `self` against `other` (-100 losing to +100 winning) and the worlds `owner` could put into a peace deal
// (colonies that are not its capital, while it keeps another). Both are what the AI reckons with; the Diplomacy screen shows them too.
int warScoreOf(const GameState& state, int self, int other);
std::vector<int> cedableWorlds(const GameState& state, int owner);
Command makeProposePeaceTermsCommand(std::uint64_t id, int targetId, const std::vector<int>& cede, const std::vector<int>& demand,
                                     std::int64_t creditOffer = 0, std::int64_t creditDemand = 0);
int fleetMaxHull(const Fleet& fleet);
int fleetAttack(const Fleet& fleet);
int fleetMaxShields(const Fleet& fleet);
void validate(const GameState& state); // Throws on invalid input; no mutation.
Economy economy(const Colony& colony);
Economy economy(const GameState& state, const Colony& colony);
const char* researchName(int field);
int researchCost(int nextLevel);
int researchCost(const GameState& state, int field, int nextLevel);
std::string technologyName(const GameState& state, int field, int level);
std::string technologyBlocker(const GameState& state, Building product, int owner = 0);
std::vector<ConstructionChoice> constructionChoices(const GameState& state, int systemId);
void submit(GameState& state, const Command& command); // Atomic; validates full draft.
void cancel(GameState& state, std::uint64_t commandId); // IDs never reused.
// Hotseat (MULTIPLAYER.md): `humans` are the human empires (2 or more, each a seated major, the current player among them).
// An empty list makes the game one-player again.
void setHotseat(GameState& state, std::vector<int> humans);
// E6 (22 Nov 2026), LAN: the humans actually playing this turn. A player who left is not in the list and their empire is run by
// the AI from this turn on; one who rejoins is back in it. The chair goes to the lowest listed human. One human leaves the game
// without a hotseat list (a single player). Listed empires that are no longer active are ignored.
void setLanRoster(GameState& state, std::vector<int> humans);
// Hands the chair to `empireId` (one of the hotseat humans): the seated human's queued orders are applied to the state first
// (as at the start of a turn), then that empire becomes playerEmpire. The turn does not advance.
void switchSeat(GameState& state, int empireId);
// The seat swap alone, no validation and no orders applied (advance uses it when a hotseat human falls).
void switchSeatInternal(GameState& state, int empireId);
// C4: true when the campaign has just been won by a human through domination (other powers still stand) and the winner has
// not already chosen to play on. playOn() takes that choice: the campaign is ongoing again and domination no longer ends it.
bool canPlayOn(const GameState& state);
// C8 (21 Nov 2026, Simon's delegation, sized by the round 6 observer soaks): a world's ground defence counts its population
// divided by this (was 3), so wars between equals take worlds faster. Used by the invasion roll and the AI's landing test.
inline constexpr int InvasionPopDefenseDivisor = 5;
// C4/C7: whether a domination can end the campaign now (the setting is on and nobody is playing on).
inline bool dominationLive(const GameState& s) { return s.dominationEnabled && s.playingOnFrom == 0; }
void playOn(GameState& state);
// True when `empireId` is played by a human: the seated player, or any hotseat human (never in observer mode).
bool humanSeat(const GameState& state, int empireId);
GameState preview(const GameState& state); // Draft colony/queue values for inspection, without resolving a turn.
TurnReport advanceWithCombat(GameState& state, const std::function<void(GameState&, TurnReport&)>& combatResolver = {});
TurnReport advance(GameState& state); // Publish only after complete success.
// Resolves every war encounter on the map through the tactical engine. Battles
// are pairwise: for each sector (row-major) and each warring MajorRelation in
// empire-id order, the two empires' surviving, non-withdrawn ships fight one
// encounter, so three or more empires meeting in a sector fight a deterministic
// sequence of duels. Encounters involving the player are passed to
// playerEncounter when set (it must leave the encounter finished); all other
// encounters are auto-resolved. Used by advance() for unified-combat campaigns
// and by session::resume for every campaign.
void resolveTacticalBattles(GameState& state, TurnReport& report,
                            const std::function<void(tactical::Encounter&)>& playerEncounter = {});
std::string serialize(const GameState& state);
GameState deserialize(const std::string& text);
std::uint64_t fingerprint(const GameState& state);
std::string describe(const TurnReport& report);
bool explored(const GameState& state, int x, int y);
bool visible(const GameState& state, int owner, int x, int y);
const Colony* colonyAt(const GameState& state, int systemId);
bool isShip(Building product);
int shipRange(const Fleet& fleet);
bool supplied(const GameState& state, const Fleet& fleet, int x, int y);
std::vector<int> route(const GameState& state, const Fleet& fleet, int x, int y);
std::string constructionBlocker(const GameState& state, const Colony& colony);
int rushCost(const GameState& state, int systemId);

// Modern Campaign Expansion APIs (Planets, Terraforming, Task Forces)
const Planet* planetAt(const GameState& state, int systemId, int planetIndex);
const std::vector<Planet>& systemPlanets(const GameState& state, int systemId);
int systemCapacity(const GameState& state, int systemId);
int systemBaseCapacity(const GameState& state, int systemId);
int systemMaxCapacity(const GameState& state, int systemId);
bool canTerraformPlanet(const GameState& state, int fleetId, int systemId, int planetIndex);
const char* planetClassName(PlanetClass pc);
const char* terraformingStateName(TerraformingState ts);

const TaskForce* taskForceAt(const GameState& state, int taskForceId);
std::vector<TaskForce> empireTaskForces(const GameState& state, int ownerId);
const TaskForce* fleetTaskForce(const GameState& state, int fleetId);
std::vector<Fleet> taskForceFleets(const GameState& state, int taskForceId);
int taskForceSpeed(const GameState& state, int taskForceId);
int taskForceAttack(const GameState& state, int taskForceId);
int taskForceHull(const GameState& state, int taskForceId);
int taskForceShields(const GameState& state, int taskForceId);

Command makeTerraformCommand(std::uint64_t id, int fleetId, int systemId, int planetIndex);
Command makeCreateTaskForceCommand(std::uint64_t id, const std::vector<int>& fleetIds, const std::string& name = "");
Command makeDisbandTaskForceCommand(std::uint64_t id, int taskForceId);
Command makeTaskForceOrderCommand(std::uint64_t id, int taskForceId, int targetX, int targetY, TacticalStance stance = TacticalStance::Balanced, int targetFleetId = 0);
Command makeBuyConstructionCommand(std::uint64_t id, int systemId);
Command makeScrapStructureCommand(std::uint64_t id, int systemId, Building structure);

// Modern Campaign Diplomacy APIs (UI handoff for Claude)
void syncDiplomacyState(GameState& state);
const diplomacy::DiplomacyState& campaignDiplomacyState(const GameState& state);
std::vector<diplomacy::Proposal> pendingProposals(const GameState& state, int empireId);
std::vector<diplomacy::Proposal> incomingProposals(const GameState& state, int empireId);
std::vector<diplomacy::Proposal> outgoingProposals(const GameState& state, int empireId);
std::vector<diplomacy::ActiveTreaty> activeTreaties(const GameState& state, int empireId);
std::vector<diplomacy::DiplomaticEvent> diplomaticEvents(const GameState& state);
const diplomacy::MinorDiplomacyState* minorDiplomacy(const GameState& state, int minorId);
bool canProposeTreaty(const GameState& state, int proposerId, int targetId, bool isTargetMinor,
                      diplomacy::TreatyType type, std::string* outError = nullptr);

// Modern diplomacy consequences & integration hooks (Lane B; see remaster/docs/DIPLOMACY.md)
bool empireEliminated(const GameState& state, int empireId);                 // Campaign empire with no colonies left
std::vector<int> alliedEmpires(const GameState& state, int empireId);        // Full Alliance: shared supply range + mutual defense
std::vector<int> sensorSharingEmpires(const GameState& state, int empireId); // Affiliation or Alliance: shared sensors/exploration
bool treatyForbidsAttack(const GameState& state, int empireA, int empireB);  // Non-aggression or stronger, or a timed peace, in force
std::vector<diplomacy::DefenseCall> openCallsToArms(const GameState& state, int allyId);
diplomacy::DiplomaticVictoryStatus diplomaticVictoryStatus(const GameState& state, int empireId);
bool diplomaticVictoryAchieved(const GameState& state, int empireId);

Command makeProposeTreatyCommand(std::uint64_t id, int targetId, bool isTargetMinor,
                                 diplomacy::TreatyType treatyType,
                                 diplomacy::TreatyDuration duration = diplomacy::TreatyDuration::Turns25,
                                 diplomacy::DiplomaticTone tone = diplomacy::DiplomaticTone::Cordial,
                                 std::int64_t creditOffer = 0, std::int64_t creditDemand = 0);

Command makeRespondProposalCommand(std::uint64_t id, std::uint64_t proposalId, bool accept,
                                   const std::string& reason = "");

// ---------------------------------------------------------------------------
// Treaty integration and client queries (schema 14; docs/GAMEPLAY-REMASTER.md
// section 13 and DIPLOMACY.md section 4).
// ---------------------------------------------------------------------------
// True when the two empires may fight: formally at war and no non-aggression pact
// or stronger (or timed peace) in force. Battles, invasions, station raids and
// captures, AI targeting and player attack/invade orders all use it.
bool combatAllowed(const GameState& state, int empireA, int empireB);
// Refuses an open call to arms from `defenderId` against `aggressorId` at once:
// -20 trust with the defender and +2 grievances (an AI defender may cancel the alliance).
Command makeRefuseCallToArmsCommand(std::uint64_t id, int defenderId, int aggressorId);
// Treasury forecast for the coming turn, by source. Colony, fleet, station and
// order lines always equal what advance() charges; trade, treaty and event lines
// match unless the turn itself changes a treaty or war (see TurnReport::ledger).
CreditForecast creditForecast(const GameState& state, int empireId);
int fleetUpkeep(const Fleet& fleet);                                  // Credits per turn.
int tradeRouteRevenue(const GameState& state, const TradeRoute& route); // Credits per turn; 0 when idle.
// Sectors per turn the fleet moves (its task force's pace when it belongs to one).
int shipSpeed(const GameState& state, const Fleet& fleet);
// Chebyshev distance from a powered shipyard (own or allied) within which the fleet
// is supplied: shipRange + Propulsion/2. Stations and wormholes extend supply beyond it.
int supplyRange(const GameState& state, const Fleet& fleet);
int taskForceRange(const GameState& state, int taskForceId);          // Shortest member supply range (0 if none).
// The sectors (x, y), start included, a task force would travel to reach (x, y);
// empty exactly when SetTaskForceOrder would reject the destination.
std::vector<std::pair<int, int>> taskForceRoute(const GameState& state, int taskForceId, int x, int y);

// Modern Campaign Intelligence APIs (UI handoff for Claude)
const intelligence::EmpireIntelState* empireIntelState(const GameState& state, int empireId);
std::vector<intelligence::OperationRecord> empireIntelHistory(const GameState& state, int empireId);
std::int64_t empireIntelOutput(const GameState& state, int empireId);
std::int64_t empireSecurityStockpile(const GameState& state, int empireId);

Command makeSetIntelBudgetCommand(std::uint64_t id, int internalSecurityPct,
                                 const std::vector<std::pair<int, std::pair<int, int>>>& rivalAllocations);
Command makeSetIntelDepartmentFocusCommand(std::uint64_t id, int rivalId, intelligence::IntelDepartment focus);

// Modern Colony Automation & Governor APIs
Command makeSetColonyGovernorCommand(std::uint64_t id, int systemId,
                                     GovernorPriority workforce, GovernorPriority queue);
Command makeSetQueuePinCommand(std::uint64_t id, int systemId, int queueIndex, bool pinned);
Command makeClearGovernorOverrideCommand(std::uint64_t id, int systemId);
Command makeCancelFirstConstructionCommand(std::uint64_t id, int systemId);
Command makePrioritizeLastConstructionCommand(std::uint64_t id, int systemId);
Command makeRemoveConstructionCommand(std::uint64_t id, int systemId, int queueIndex);
Command makeMoveConstructionCommand(std::uint64_t id, int systemId, int queueIndex, int destinationIndex);

// Industry floor of the workforce governor (PLAYTEST-FEEDBACK 16 Nov 2026): once a colony is fed, this share of its citizens stays on
// powered factories under the Growth / Research priorities. Capped by powered factories x 10 and by the citizens left after food.
inline constexpr int GrowthIndustryFloorPercent = 25;
inline constexpr int ResearchIndustryFloorPercent = 20;
void evaluateWorkforceGovernor(const GameState& state, Colony& colony);
void evaluateQueueGovernor(const GameState& state, Colony& colony);

// ---------------------------------------------------------------------------
// Schema-13 gameplay layer APIs (docs/GAMEPLAY-REMASTER.md)
// ---------------------------------------------------------------------------
// Ruleset presets and new-game setup. generateCampaign()/generateReferenceCampaign()
// keep producing the pre-13 baseline (Classic with every layer off); new games
// should use newCampaign(), whose preset defaults to Remastered.
struct CampaignSetup {
    std::uint64_t seed = 1999;
    int empires = 5, width = 16, height = 12, systems = 28, playerEmpire = 1;
    StartingEra era = StartingEra::Early;
    bool reference = true;
    RulesetPreset preset = RulesetPreset::Remastered;
    GalaxySetup galaxy;
    // Schema 17: the lineup as race keys ("federation", "klingon", "romulan", "cardassian", "ferengi", "dominion", "borg",
    // "breen"), two to eight, in seating order. Empty keeps `empires` (slots 1..empires). When set it overrides `empires`, and
    // `playerEmpire` is the empire id (race slot) of a seated race.
    std::vector<std::string> lineup;
    // Schema 17: a registered preset (Presets.h). When set, newCampaign installs its overlay stack as the active content,
    // applies its rule flags over `preset`, and records it in the state. Empty keeps the content that is active.
    std::string presetId;
    // Schema 20: the threat switches (monsters, Borg, frequency, per-actor toggles). Unset takes the preset's defaults (none for
    // Classic and Remastered: threats off); the new-game screen fills it from the preset and the player may change any switch.
    std::optional<ThreatSetup> threats;
    // Schema 25: a registered scenario (Scenarios.h). When set it names the preset, lineup, galaxy, era and scripted start; `playerEmpire`
    // (an empire id in the scenario's lineup) and the difficulty are still the player's. Empty plays a free game.
    std::string scenarioId;
    bool playerSet = false;   // The caller chose `playerEmpire` (otherwise a scenario's own default player applies).
    // Schema 24 (docs/design/BALANCE-AI.md section 6). Generation options; their results live in the state, so they are not saved.
    int startSystems = 0;            // Extra colonies each empire starts with (0..8) beside its capital.
    int planetsMin = 0, planetsMax = 0; // Planets per ordinary star, 1..9 each; 0 keeps the generator's own 2..5.
    int starTypes = -1;              // Race home stars take their race's star type: -1 the pack's default, 0 off, 1 on.
};
// Star positions only (no names, planets or empires): the layout generate() would give this shape, so a setup
// screen can preview the exact galaxy. Index 0 is Sol. Deterministic on every platform (integer arithmetic).
std::vector<std::pair<int, int>> galaxyLayout(std::uint64_t seed, int width, int height, int systems, GalaxyShape shape);
GameState newCampaign(const CampaignSetup& setup);
// Applies the generation options of a setup (planets per star, extra start colonies, race star types) to a fresh campaign.
void applyGalaxyOptions(GameState& state, const CampaignSetup& setup);
// Lane G: places a deterministic, galaxy-size-scaled subset of the minor-race roster (MinorRaces.h)
// on a turn-0 campaign and adopts roster-named starter minors. newCampaign() calls it for every preset;
// generateCampaign()/generateReferenceCampaign() stay on the legacy baseline.
void placeMinorRaceRoster(GameState& state);
// Schema 17: adds the home-system ships a race lists as `major.startingFleet` (Dominion, Borg, Breen); needs the expanded roster.
void addStartingFleets(GameState& state);
GameplayRules rulesForPreset(RulesetPreset preset);
const char* rulesetPresetName(RulesetPreset preset);
bool parseRulesetPreset(const std::string& text, RulesetPreset& preset); // "classic" / "remastered" (case-insensitive)
// Switches a freshly generated (turn 0) campaign to a preset, generating galaxy
// features and the event deck when the preset enables them.
void applyRulesetPreset(GameState& state, RulesetPreset preset);
// The same for an explicit flag set (a preset's rules); applyRulesetPreset(state, p) is applyGameplayRules(state, rulesForPreset(p)).
void applyGameplayRules(GameState& state, const GameplayRules& rules);

// Stations (outposts and starbases).
const Station* stationById(const GameState& state, int stationId);
const Station* stationAt(const GameState& state, int x, int y);
StationStats stationStats(const GameState& state, const Station& station);
StationStats stationStats(const GameState& state, int ownerId, StationType type);
const char* stationTypeName(StationType type);
std::string stationName(const Station& station);
std::vector<Station> visibleStations(const GameState& state, int viewerEmpire); // Own stations plus sensor contacts.
int outpostBuildTurns(const GameState& state, int ownerId);
std::string outpostBlocker(const GameState& state, int fleetId); // Empty when BuildOutpost is legal.
StationUpgradeCost stationUpgradeCost(const GameState& state, int ownerId);
std::string stationUpgradeBlocker(const GameState& state, int stationId, int fundingSystemId);
const StationUpgrade* stationUpgradeFor(const GameState& state, int stationId);
bool isStationCombatant(int fleetId);
int stationIdForCombatant(int fleetId);
Command makeBuildOutpostCommand(std::uint64_t id, int fleetId);
Command makeUpgradeStationCommand(std::uint64_t id, int stationId, int fundingSystemId);

// Galaxy features and anomalies.
const char* galaxyFeatureName(GalaxyFeatureType type);
std::vector<GalaxyFeature> featuresAt(const GameState& state, int x, int y); // Includes ion storm fronts.
std::vector<GalaxyFeature> knownFeatures(const GameState& state, int viewerEmpire); // Explored cells only.
std::vector<std::pair<int, int>> ionStormCells(const GameState& state, const GalaxyFeature& storm);
bool sectorPassable(const GameState& state, int x, int y);  // Black holes and subspace rifts block movement.
bool sectorHazardous(const GameState& state, int x, int y); // Neutron stars and ion storms damage ships.
bool sectorInNebula(const GameState& state, int x, int y);

// Curated galactic events.
const char* galacticEventName(GalacticEventKind kind);
std::vector<EventChoiceInfo> eventChoices(GalacticEventKind kind);
int eventDefaultChoice(GalacticEventKind kind);
std::string describeEvent(const GameState& state, const GalacticEvent& event);
std::vector<GalacticEvent> empireEvents(const GameState& state, int empireId); // Active events affecting an empire.
Command makeResolveEventChoiceCommand(std::uint64_t id, int eventId, int option);

// Build-queue templates.
std::vector<BuildTemplate> empireBuildTemplates(const GameState& state, int ownerId);
const BuildTemplate* buildTemplateById(const GameState& state, int templateId);
Command makeDefineBuildTemplateCommand(std::uint64_t id, int templateId, const std::string& name,
                                       const std::vector<Construction>& items);
Command makeDeleteBuildTemplateCommand(std::uint64_t id, int templateId);
Command makeApplyBuildTemplateCommand(std::uint64_t id, int templateId, int systemId);
Command makeSetDefaultBuildTemplateCommand(std::uint64_t id, int templateId); // 0 clears the default.

// Victory conditions and progress.
std::vector<VictoryProgress> victoryProgress(const GameState& state, int empireId);
const char* victoryConditionName(VictoryCondition condition);
int scientificVictoryThreshold(const GameState& state);
const char* megaprojectName(const GameState& state);
const char* megaprojectName(const GameState& state, int empireId);   // E5: the empire's own name for it ("Federation Genesis-class Project")
int megaprojectIndustryCost(const GameState& state);
int megaprojectWorkPerTurn(const GameState& state, int empireId); // Most industry the project absorbs per turn.
std::int64_t megaprojectCreditCost(const GameState& state);
const Megaproject* empireMegaproject(const GameState& state, int empireId);
std::string megaprojectBlocker(const GameState& state, int empireId, int systemId);
Command makeStartMegaprojectCommand(std::uint64_t id, int systemId);
Command makeCancelMegaprojectCommand(std::uint64_t id);

// Turn chronicle.
const char* chronicleCategoryName(ChronicleCategory category);
std::string describeChronicle(const TurnReport& report);
} // namespace botf

