#pragma once
// Internal interface between Simulation.cpp and the schema-13 gameplay layer
// (Rules.cpp, Stations.cpp, GalaxyFeatures.cpp, GalacticEvents.cpp,
// BuildTemplates.cpp, Victory.cpp, Chronicle.cpp). Not a client API: clients use
// Simulation.h. Everything here lives in named namespaces so the Unreal bridge,
// which compiles every core .cpp into one translation unit, never sees clashes
// with Simulation.cpp's anonymous-namespace helpers.
#include "Simulation.h"
#include "ReferenceCatalogue.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iosfwd>
#include <set>
#include <string>
#include <vector>

namespace botf::internal {
// Mirrors of Simulation.cpp limits (static_asserted there).
inline constexpr int QueueLimit = 64;
inline constexpr int BuildingLimit = 1000;
inline constexpr std::int64_t BalanceLimit = 1000000000000LL;

// Implemented in Simulation.cpp as thin wrappers over its private helpers.
std::uint64_t nextRandom(std::uint64_t& state);
std::uint64_t bounded(std::uint64_t& state, std::uint64_t bound);
EmpireState& mutableEmpire(GameState& state, int id);
bool known(const GameState& state, int owner, int x, int y);
void reveal(GameState& state, int x, int y, int owner);
bool queueShip(GameState& state, Colony& colony, Building role);
void detachFromTaskForce(GameState& state, Fleet& fleet);
void reconcileTaskForces(GameState& state);
int catalogueRace(int owner);
int donorRace(int owner);
bool shipUsable(const referenceData::ShipRecord& ship, int owner);
bool playableShip(const referenceData::ShipRecord& ship);
Building roleProduct(int role);
const referenceData::ShipRecord& sourceShip(int id);
int constructionCost(const Construction& project, int owner = 0);
std::string constructionTechBlocker(const GameState& state, const Construction& project, int owner);
void validateColony(const Colony& colony);
// supplied() without the gameplay layer: powered shipyards within ship range.
bool colonySupplied(const GameState& state, const Fleet& fleet, int x, int y);
bool colonySuppliedWithRange(const GameState& state, int owner, int range, int x, int y);
// Full supply test for a ship of `owner` with base range `range` (supplied() uses it):
// own and allied powered shipyards, own and allied stations, known wormhole exits.
bool supplyReaches(const GameState& state, int owner, int range, int x, int y);
std::vector<int> activeEmpires(const GameState& state); // Player and rivals, sorted.
// Schema 25 (Scenarios.cpp): relations, treaties and seating for scenario scripts and secession.
MajorRelation& relation(GameState& state, int a, int b);
void war(GameState& state, int a, int b, bool engine);   // engine: through the diplomacy engine (calls to arms); false: the relation only
void peace(GameState& state, int a, int b);
void alliance(GameState& state, int a, int b);           // A Full Alliance treaty, indefinite; any war between them ends
void seatEmpire(GameState& state, const EmpireState& empire); // A new rival joins mid-game: relations to every empire, intel and diplomacy state
void syncDiplomacy(GameState& state);
void setMapOverride(const void* override); // scenarios::MapOverride* for the next generate/generateCampaign calls (nullptr clears)
} // namespace botf::internal

namespace botf::gameplay {
// No module uses namespace-scope using-directives: the unity build would leak
// them into every later file. Shared helpers live here instead.
void require(bool condition, const char* message);
void require(bool condition, const std::string& message);
inline int chebyshev(int ax, int ay, int bx, int by) { return std::max(std::abs(ax - bx), std::abs(ay - by)); }
inline std::string sectorText(int x, int y) { return "(" + std::to_string(x) + "," + std::to_string(y) + ")"; }
inline const System* systemById(const GameState& s, int id) {
    for (const auto& sys : s.systems) if (sys.id == id) return &sys;
    return nullptr;
}
inline const System* systemAtCell(const GameState& s, int x, int y) {
    for (const auto& sys : s.systems) if (sys.x == x && sys.y == y) return &sys;
    return nullptr;
}
// Schema 14: hostile only when combat is allowed (at war and no treaty forbids it).
inline bool empiresAtWar(const GameState& s, int a, int b) { return combatAllowed(s, a, b); }
inline int stationSupplyRadiusFor(StationType type) { return type == StationType::Outpost ? 3 : 5; }
inline int stationSensorRadiusFor(StationType type) { return type == StationType::Outpost ? 2 : 3; }
inline bool empireAtAnyWar(const GameState& s, int owner) {
    for (const auto& r : s.majorRelations) if (r.atWar && (r.first == owner || r.second == owner)) return true;
    return false;
}

// ---- Rules / persistence (Rules.cpp) ----
bool isGameplayCommand(CommandType type);
void checkCommand(const Command& command);         // Field shape of gameplay commands.
void checkUnrelatedFields(const Command& command); // Gameplay fields stay empty on other commands.
void applyCommand(GameState& state, const Command& command, std::set<int>& orderedFleets);
void writeCommandTail(std::ostream& out, const Command& command);
void readCommandTail(std::istream& in, Command& command);
void validate(const GameState& state);
std::string serializeSection(const GameState& state);
void parseSection(std::istream& in, GameState& state); // Called after the GAMEPLAY_LAYER tag.
void stripSection(std::string& payload);               // Removes the section from canonical text.
bool layerActive(const GameState& state);               // Stations or features exist.

// ---- Stations (Stations.cpp) ----
void validateStations(const GameState& state);
void applyStationCommand(GameState& state, const Command& command, std::set<int>& orderedFleets);
void chargeStationUpkeep(GameState& next, TurnReport& report);
int divertIndustry(GameState& next, const Colony& colony, int industry, TurnReport& report);
void advanceOutpostConstruction(GameState& moving, TurnReport& report);
void cancelInterruptedOutposts(GameState& next, TurnReport& report);
void stationsAfterCombat(GameState& moving, TurnReport& report, const std::set<int>& stationsInBattle);
void resolveStationCaptures(GameState& moving, TurnReport& report);
void completeStationUpgrades(GameState& next, TurnReport& report);
void planRivalStations(GameState& next);
bool stationSupplies(const GameState& state, int owner, int x, int y);
bool stationDefends(const GameState& state, int ownerId, int x, int y);
const Station* combatStation(const GameState& state, int x, int y, int first, int second);
tactical::Encounter constructStationEncounter(GameState& state, const std::vector<int>& fleetIds,
                                              std::uint64_t seed, const Station& station);
void applyStationEncounter(GameState& state, const tactical::Encounter& encounter);

// ---- Galaxy features (GalaxyFeatures.cpp) ----
void generateGalaxyFeatures(GameState& state);
void validateFeatures(const GameState& state);
bool extendedSupply(const GameState& state, int owner, int x, int y,
                    const std::function<bool(int, int)>& colonySupplied);
bool extendedVisible(const GameState& state, int owner, int x, int y);
std::vector<int> findPath(const GameState& state, int owner, int startCell, int goalCell,
                          const std::function<bool(int, int)>& supplied);
struct EmpireDelta { int empireId = 0; std::int64_t credits = 0; int field = -1; std::int64_t research = 0; std::int64_t intelligence = 0; };
struct TurnEffects { std::vector<EmpireDelta> deltas; };
void surveyAnomalies(GameState& moving, TurnReport& report, TurnEffects& effects);
void applySectorHazards(GameState& moving, TurnReport& report);
void driftIonStorms(GameState& next);
void expireFeatures(GameState& next, TurnReport& report);
void planAnomalySurveys(GameState& next);
int addFeature(GameState& state, GalaxyFeatureType type, int x, int y, int expiresTurn);
// Grants research to one field, keeping the research ledger reconciled.
void grantResearch(GameState& state, int empireId, int field, std::int64_t amount, TurnReport* report);
std::int64_t researchGrantFor(const GameState& state, int empireId, int field, int percent, std::int64_t floor);

// ---- Galactic events (GalacticEvents.cpp) ----
void initializeEventDeck(GameState& state);
void validateEvents(const GameState& state);
void applyEventChoiceCommand(GameState& state, const Command& command);
void resolveGalacticEvents(GameState& next, TurnReport& report);
struct EventCredit { std::int64_t income = 0, cost = 0; };
EventCredit eventCredits(const GameState& state, const GalacticEvent& event, int turn, std::int64_t balance, int colonies);

// ---- Build templates (BuildTemplates.cpp) ----
void validateTemplates(const GameState& state);
void applyTemplateCommand(GameState& state, const Command& command);
void checkTemplateItems(const Command& command);
void autoApplyTemplates(const GameState& before, GameState& next, TurnReport& report);

// ---- Victory (Victory.cpp) ----
void validateMegaprojects(const GameState& state);
void applyMegaprojectCommand(GameState& state, const Command& command);
void planRivalMegaprojects(GameState& next, TurnReport& report);
void resolveMegaprojectLosses(GameState& next, TurnReport& report);
void evaluateVictory(GameState& next, TurnReport& report);

// ---- Chronicle (Chronicle.cpp) ----
// Appends a structured entry and its text line to TurnReport::events.
void note(TurnReport& report, ChronicleEntry entry);
void buildChronicle(const GameState& next, TurnReport& report);

// ---- Turn orchestration (Rules.cpp) ----
void planTurn(GameState& next, TurnReport& report);
void beforeCombat(GameState& moving, TurnReport& report, TurnEffects& effects);
void afterCombat(GameState& moving, TurnReport& report);
void carryOver(GameState& next, GameState& moving, const TurnEffects& effects, TurnReport& report);
void finishTurn(const GameState& before, GameState& next, TurnReport& report);
} // namespace botf::gameplay
