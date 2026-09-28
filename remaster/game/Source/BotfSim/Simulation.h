#pragma once

#include <cstdint>
#include <array>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Diplomacy.h"
#include "Intelligence.h"

namespace botf {
// Modern BOTF campaign expansion schema and content.
inline constexpr const char* ContentVersion = "campaign-expansion-11";
inline constexpr int SaveVersion = 11;
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
    SetColonyGovernor, SetQueuePin, ClearGovernorOverride
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
    int powerPlants = 0;
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
struct CombatRound {
    int index = 0;
    std::vector<FleetDamage> damage;
    std::vector<int> lostFleetIds, retreatedFleetIds;
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
};
struct DiplomaticOffer {
    std::uint64_t id = 0;
    int empireId = 1, systemId = 0, dueTurn = 0;
    CommandType type = CommandType::OfferFriendship;
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

Command makeProposeTreatyCommand(std::uint64_t id, int targetId, bool isTargetMinor,
                                 diplomacy::TreatyType treatyType,
                                 diplomacy::TreatyDuration duration = diplomacy::TreatyDuration::Turns25,
                                 diplomacy::DiplomaticTone tone = diplomacy::DiplomaticTone::Cordial,
                                 std::int64_t creditOffer = 0, std::int64_t creditDemand = 0);

Command makeRespondProposalCommand(std::uint64_t id, std::uint64_t proposalId, bool accept,
                                   const std::string& reason = "");

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
} // namespace botf

