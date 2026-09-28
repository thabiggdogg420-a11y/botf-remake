#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Diplomacy.h"
#include "Intelligence.h"

namespace botf {
// BOTF Remastered schema and content. Schema 12 adds the unified tactical combat
// model and fusion-plant accounting; schema 13 adds the gameplay layer (ruleset
// presets, stations, galaxy features, curated events, alternative victories and
// build templates, see docs/GAMEPLAY-REMASTER.md); schema 14 adds per-ship orders and
// flags to recorded battle rounds (COMBAT_ORDERS) and the RefuseCallToArms order.
// Schemas 4-13 still load and continue.
inline constexpr const char* ContentVersion = "remaster-14";
inline constexpr int SaveVersion = 14;
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
enum class Building : int { Farm, Factory, Laboratory, Shipyard, Scout, ColonyShip, PowerPlant, FusionPlant, ResearchInstitute, Destroyer, Transport, DefenseBattery };
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
    RefuseCallToArms
};
enum class CampaignOutcome : int { Ongoing, Victory, Defeat };
inline constexpr int ResearchFields = 6, MaxTechLevel = 10;
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
    std::vector<int> memberFleetIds;
    std::string taskForceName;
    diplomacy::TreatyType proposalTreatyType = diplomacy::TreatyType::NonAggression;
    diplomacy::TreatyDuration proposalDuration = diplomacy::TreatyDuration::Turns25;
    diplomacy::DiplomaticTone proposalTone = diplomacy::DiplomaticTone::Cordial;
    std::int64_t proposalCreditOffer = 0;
    std::int64_t proposalCreditDemand = 0;
    bool proposalTargetIsMinor = false;
    std::uint64_t proposalId = 0;
    bool proposalAccept = false;
    std::string proposalReason;
    int intelInternalSecurity = 0;
    std::vector<int> intelRivalIds;
    std::vector<int> intelEspionagePcts;
    std::vector<int> intelSabotagePcts;
    intelligence::IntelDepartment intelDepartmentFocus = intelligence::IntelDepartment::General;
    GovernorPriority governorWorkforce = GovernorPriority::Manual;
    GovernorPriority governorQueue = GovernorPriority::Manual;
    bool queuePinned = false;
    // Schema-13 gameplay layer (zero/empty for every other command type):
    int stationId = 0;                       // UpgradeStation
    int eventId = 0, eventOption = 0;        // ResolveEventChoice
    int templateId = 0;                      // Define/Delete/Apply/SetDefault build template
    std::string templateName;                // DefineBuildTemplate
    std::vector<Construction> templateItems; // DefineBuildTemplate (product + referenceId only)
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
    std::array<bool, 6> contact{};
    std::array<int, 6> attitude{0,60,60,60,60,60};
    std::array<int, 6> friendshipTurn{-1,-1,-1,-1,-1,-1};
    // Minor-race roster (Lane G, MinorRaces.h; optional MINOR_RACES save section).
    // raceId 0 = legacy minor without roster progression; the other three are then 0.
    int raceId = 0;
    int techLevel = 0, population = 0, progress = 0;
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
    int surveyedMask = 0;      // Bit (1 << empireId) once that empire collected the survey reward.
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

enum class VictoryCondition : int { Conquest = 0, Domination = 1, Scientific = 2, Diplomatic = 3 };
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
};
struct Economy {
    int food = 0, foodDeficit = 0, industry = 0, research = 0;
    int energyUsed = 0, inactiveFactories = 0, inactiveLabs = 0;
    int income = 0, upkeep = 0;
    bool shipyardPowered = false;
    int poweredBatteries = 0, inactiveBatteries = 0;
    int energyAvailable = 0;
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
std::vector<StartingEra> availableStartingEras();
StartingEra campaignStartingEra(const GameState& state);

GameState generate(std::uint64_t seed, int width = 12, int height = 8, int count = 12);
GameState generateCampaign(std::uint64_t seed, int empires = 2, int width = 16, int height = 12, int systems = 28, int playerEmpire = 1, StartingEra era = StartingEra::Early);
GameState generateReferenceCampaign(std::uint64_t seed, int empires = 2, int width = 16, int height = 12, int systems = 28, int playerEmpire = 1, StartingEra era = StartingEra::Early);
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
};
GameState newCampaign(const CampaignSetup& setup);
// Lane G: places a deterministic, galaxy-size-scaled subset of the minor-race roster (MinorRaces.h)
// on a turn-0 campaign and adopts roster-named starter minors. newCampaign() calls it for every preset;
// generateCampaign()/generateReferenceCampaign() stay on the legacy baseline.
void placeMinorRaceRoster(GameState& state);
GameplayRules rulesForPreset(RulesetPreset preset);
const char* rulesetPresetName(RulesetPreset preset);
bool parseRulesetPreset(const std::string& text, RulesetPreset& preset); // "classic" / "remastered" (case-insensitive)
// Switches a freshly generated (turn 0) campaign to a preset, generating galaxy
// features and the event deck when the preset enables them.
void applyRulesetPreset(GameState& state, RulesetPreset preset);

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

