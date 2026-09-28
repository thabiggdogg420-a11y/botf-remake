#include "Simulation.h"
#include "GameplayInternal.h"
#include "MinorRaces.h"
#include "ReferenceCatalogue.h"
#include "ReferencePlanets.h"
#include "TacticalCombat.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace botf {
namespace {
constexpr int MaxSystems = 256, MaxQueue = 64, MaxPending = 1024;
constexpr int MaxPopulation = 10000, MaxBuildings = 1000, MaxTurns = 1000000;
constexpr int MaxTaskForces = 128, MaxPlanetsPerSystem = 10;
constexpr std::int64_t MaxBalance = 1000000000000LL;
void require(bool condition, const char* message);
int catalogueRace(int owner) {
    static constexpr std::array<int, 6> map{-1, 1, 3, 4, 0, 2};
    require(owner >= 1 && owner <= 5, "Invalid major empire");
    return map[owner];
}
const referenceData::ShipRecord& sourceShip(int id) {
    require(id >= 0 && static_cast<std::size_t>(id) < referenceData::ShipCatalogue.size(), "Unknown reference ship");
    const auto& ship = referenceData::ShipCatalogue[static_cast<std::size_t>(id)];
    require(ship.id == id, "Reference ship IDs are not indexed canonically");
    return ship;
}
Building roleProduct(int role) {
    if (role == 0) return Building::Scout;
    if (role == 5) return Building::ColonyShip;
    if (role == 9) return Building::Transport;
    if (role >= 1 && role <= 4) return Building::Destroyer;
    throw std::runtime_error("Reference ship role is not supported in campaign play");
}
bool playableShip(const referenceData::ShipRecord& ship) {
    return ship.raceId >= 0 && ship.raceId <= 4 && (ship.role == 0 || ship.role == 5 || ship.role == 9 ||
        (ship.role >= 1 && ship.role <= 4));
}
int constructionCost(const Construction& project) {
    return project.referenceId < 0 ? definition(project.product).industryCost : sourceShip(project.referenceId).industryCost;
}
std::string constructionTechBlocker(const GameState& state, const Construction& project, int owner);
constexpr std::array<BuildingDefinition, 12> Definitions{{
    {Building::Farm, "Farm", 60, 0, 1},
    {Building::Factory, "Factory", 100, 5, 1},
    {Building::Laboratory, "Laboratory", 80, 5, 1},
    {Building::Shipyard, "Shipyard", 120, 5, 2},
    {Building::Scout, "Scout", 60, 0, 0},
    {Building::ColonyShip, "Colony ship", 100, 0, 0},
    {Building::PowerPlant, "Power plant", 80, 0, 1},
    {Building::FusionPlant, "Fusion plant", 160, 0, 2},
    {Building::ResearchInstitute, "Research institute", 160, 10, 2},
    {Building::Destroyer, "Destroyer", 160, 0, 0},
    {Building::Transport, "Transport", 120, 0, 0},
    {Building::DefenseBattery, "Defense battery", 140, 5, 2}
}};
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool buildingValid(Building value) {
    return static_cast<int>(value) >= 0 && static_cast<int>(value) < static_cast<int>(Definitions.size());
}
std::uint64_t hashText(const std::string& text) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : text) { hash ^= byte; hash *= 1099511628211ULL; }
    return hash; // Corruption/determinism check, not authentication.
}
std::uint64_t nextRandom(std::uint64_t& state) {
    state += 0x9e3779b97f4a7c15ULL;
    auto z = state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
std::uint64_t bounded(std::uint64_t& state, std::uint64_t bound) {
    const auto threshold = (std::uint64_t{0} - bound) % bound;
    for (;;) {
        const auto value = nextRandom(state);
        if (value >= threshold) return value % bound;
    }
}
std::string romanNumeral(int n) {
    static constexpr const char* r[] = {"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};
    return (n >= 1 && n <= 10) ? r[n] : std::to_string(n);
}

referencePlanets::EPlanetType toReferencePlanetType(PlanetClass pc) {
    switch (pc) {
    case PlanetClass::ClassM: return referencePlanets::EPlanetType::Terran;
    case PlanetClass::ClassO: return referencePlanets::EPlanetType::Oceanic;
    case PlanetClass::ClassP: return referencePlanets::EPlanetType::Arctic;
    case PlanetClass::ClassL: return referencePlanets::EPlanetType::Jungle;
    case PlanetClass::ClassH: return referencePlanets::EPlanetType::Desert;
    case PlanetClass::ClassK: return referencePlanets::EPlanetType::Barren;
    case PlanetClass::ClassY: return referencePlanets::EPlanetType::Volcanic;
    case PlanetClass::GasGiant:
    case PlanetClass::Asteroids:
    default: return referencePlanets::EPlanetType::GasGiant;
    }
}

std::vector<Planet> generateSystemPlanets(std::uint64_t seed, int systemId, const std::string& systemName, bool isHome = false) {
    std::uint64_t prng = seed ^ (static_cast<std::uint64_t>(systemId) * 0x9e3779b97f4a7c15ULL);
    std::vector<Planet> planets;
    if (isHome) {
        // PROVISIONAL: Home system layout uses a provisional Sol-style distribution with primary Class M world fully terraformed at capacity 100 for legacy compatibility. Retail/vanilla home compositions are not established by local decoder.
        planets.push_back({systemId, 0, systemName + " I", PlanetClass::ClassK, TerraformingState::Untouched, 0,
            referencePlanets::CalculateTerraformWork(40, referencePlanets::EPlanetType::Barren, referencePlanets::EAtmosphere::None), 10, 40});
        planets.push_back({systemId, 1, systemName + " II", PlanetClass::ClassY, TerraformingState::Untouched, 0,
            referencePlanets::CalculateTerraformWork(30, referencePlanets::EPlanetType::Volcanic, referencePlanets::EAtmosphere::None), 0, 30});
        planets.push_back({systemId, 2, systemName + " III", PlanetClass::ClassM, TerraformingState::Terraformed, 0,
            0, 80, 100});
        planets.push_back({systemId, 3, systemName + " IV", PlanetClass::ClassH, TerraformingState::Untouched, 0,
            referencePlanets::CalculateTerraformWork(50, referencePlanets::EPlanetType::Desert, referencePlanets::EAtmosphere::None), 20, 50});
        planets.push_back({systemId, 4, systemName + " V", PlanetClass::GasGiant, TerraformingState::Uninhabitable, 0,
            0, 0, 0});
        return planets;
    }
    const int count = 2 + static_cast<int>(bounded(prng, 4)); // 2 to 5 planets
    const int primaryIdx = static_cast<int>(bounded(prng, static_cast<std::uint64_t>(count)));
    for (int i = 0; i < count; ++i) {
        Planet p;
        p.systemId = systemId;
        p.index = i;
        p.name = systemName + " " + romanNumeral(i + 1);
        if (i == primaryIdx) {
            const int roll = static_cast<int>(bounded(prng, 4));
            if (roll == 0) {
                p.planetClass = PlanetClass::ClassM;
                p.baseCapacity = 50; p.maxCapacity = 100;
            } else if (roll == 1) {
                p.planetClass = PlanetClass::ClassO;
                p.baseCapacity = 40; p.maxCapacity = 80;
            } else if (roll == 2) {
                p.planetClass = PlanetClass::ClassL;
                p.baseCapacity = 30; p.maxCapacity = 70;
            } else {
                p.planetClass = PlanetClass::ClassP;
                p.baseCapacity = 20; p.maxCapacity = 60;
            }
            p.terraformNeeded = referencePlanets::CalculateTerraformWork(p.maxCapacity, toReferencePlanetType(p.planetClass), referencePlanets::EAtmosphere::None);
            p.terraformState = TerraformingState::Untouched;
        } else {
            const int roll = static_cast<int>(bounded(prng, 9));
            if (roll == 0) {
                p.planetClass = PlanetClass::ClassM;
                p.baseCapacity = 50; p.maxCapacity = 100;
                p.terraformNeeded = referencePlanets::CalculateTerraformWork(p.maxCapacity, toReferencePlanetType(p.planetClass), referencePlanets::EAtmosphere::None);
                p.terraformState = TerraformingState::Untouched;
            } else if (roll == 1) {
                p.planetClass = PlanetClass::ClassO;
                p.baseCapacity = 40; p.maxCapacity = 80;
                p.terraformNeeded = referencePlanets::CalculateTerraformWork(p.maxCapacity, toReferencePlanetType(p.planetClass), referencePlanets::EAtmosphere::None);
                p.terraformState = TerraformingState::Untouched;
            } else if (roll == 2) {
                p.planetClass = PlanetClass::ClassP;
                p.baseCapacity = 20; p.maxCapacity = 60;
                p.terraformNeeded = referencePlanets::CalculateTerraformWork(p.maxCapacity, toReferencePlanetType(p.planetClass), referencePlanets::EAtmosphere::None);
                p.terraformState = TerraformingState::Untouched;
            } else if (roll == 3) {
                p.planetClass = PlanetClass::ClassL;
                p.baseCapacity = 30; p.maxCapacity = 70;
                p.terraformNeeded = referencePlanets::CalculateTerraformWork(p.maxCapacity, toReferencePlanetType(p.planetClass), referencePlanets::EAtmosphere::None);
                p.terraformState = TerraformingState::Untouched;
            } else if (roll == 4) {
                p.planetClass = PlanetClass::ClassH;
                p.baseCapacity = 20; p.maxCapacity = 50;
                p.terraformNeeded = referencePlanets::CalculateTerraformWork(p.maxCapacity, toReferencePlanetType(p.planetClass), referencePlanets::EAtmosphere::None);
                p.terraformState = TerraformingState::Untouched;
            } else if (roll == 5) {
                p.planetClass = PlanetClass::ClassK;
                p.baseCapacity = 10; p.maxCapacity = 40;
                p.terraformNeeded = referencePlanets::CalculateTerraformWork(p.maxCapacity, toReferencePlanetType(p.planetClass), referencePlanets::EAtmosphere::None);
                p.terraformState = TerraformingState::Untouched;
            } else if (roll == 6) {
                p.planetClass = PlanetClass::ClassY;
                p.baseCapacity = 0; p.maxCapacity = 30;
                p.terraformNeeded = referencePlanets::CalculateTerraformWork(p.maxCapacity, toReferencePlanetType(p.planetClass), referencePlanets::EAtmosphere::None);
                p.terraformState = TerraformingState::Untouched;
            } else if (roll == 7) {
                p.planetClass = PlanetClass::GasGiant;
                p.baseCapacity = 0; p.maxCapacity = 0; p.terraformNeeded = 0;
                p.terraformState = TerraformingState::Uninhabitable;
            } else {
                p.planetClass = PlanetClass::Asteroids;
                p.baseCapacity = 0; p.maxCapacity = 0; p.terraformNeeded = 0;
                p.terraformState = TerraformingState::Uninhabitable;
            }
        }
        planets.push_back(p);
    }
    return planets;
}

void validateColony(const Colony& c) {
    require(c.shipyards >= 0 && c.shipyards <= 1, "Only one shipyard per colony");
    require(c.powerPlants >= 0 && c.powerPlants <= MaxBuildings, "Power plant count out of bounds");
    require(c.fusionPlants >= 0 && c.fusionPlants <= MaxBuildings, "Fusion plant count out of bounds");
    require(static_cast<int>(c.workforcePriority) >= 0 && static_cast<int>(c.workforcePriority) <= 4 &&
        static_cast<int>(c.queuePriority) >= 0 && static_cast<int>(c.queuePriority) <= 4, "Invalid governor priority");
    // Energy is accounted per structure (power plant 10, fusion plant 40), so the
    // stored grid can never be smaller than what its plants provide.
    require(static_cast<std::int64_t>(c.energy) >= 10LL * c.powerPlants + 40LL * c.fusionPlants,
        "Energy is below the output of the colony's power structures");
    require(c.defenseBatteries >= 0 && c.defenseBatteries <= 10, "Defense battery count out of bounds");
    require(c.ownerId >= 1 && c.ownerId <= 5, "Invalid colony owner");
    require(c.population > 0 && c.population <= MaxPopulation, "Population out of bounds");
    require(c.farms >= 0 && c.farms <= MaxBuildings && c.factories >= 0 &&
        c.factories <= MaxBuildings && c.laboratories >= 0 && c.laboratories <= MaxBuildings,
        "Building count out of bounds");
    require(c.foodWorkers >= 0 && c.foodWorkers <= c.farms * 10 &&
        c.industryWorkers >= 0 && c.industryWorkers <= c.factories * 10 &&
        c.researchWorkers >= 0 && c.researchWorkers <= c.laboratories * 10,
        "Workforce exceeds facilities");
    require(c.foodWorkers + c.industryWorkers + c.researchWorkers <= c.population,
        "Workforce exceeds population");
    require(c.energy >= 0 && c.energy <= 100000, "Energy out of bounds");
    require(c.morale >= 0 && c.morale <= 100, "Morale out of bounds");
    require(c.overflow >= 0 && c.overflow <= 100000000, "Overflow out of bounds");
    require(c.queue.size() <= MaxQueue, "Construction queue full");
    int yards = c.shipyards, batteries = c.defenseBatteries;
    for (auto b : c.queue) {
        require(buildingValid(b), "Unknown queued building");
        require(b.referenceId >= -1 && b.work >= 0 && b.work <= constructionCost(b), "Project work out of bounds");
        if (b.referenceId >= 0)
            require(playableShip(sourceShip(b.referenceId)) && roleProduct(sourceShip(b.referenceId).role) == b.product,
                "Invalid reference construction role");
        if (b == Building::Shipyard) require(++yards <= 1, "Shipyard already built or queued");
        if (b == Building::DefenseBattery) require(++batteries <= 10, "Defense batteries already built or queued");
        if (isShip(b)) require(yards == 1, "Build or queue a shipyard before ships");
    }
}
bool diplomatic(CommandType type) {
    return type == CommandType::SendGift || type == CommandType::OfferFriendship || type == CommandType::OfferMembership;
}
bool majorCommand(CommandType type) {
    return type == CommandType::DeclareWar || type == CommandType::OfferPeace ||
        type == CommandType::ProposeTradeTreaty ||
        type == CommandType::EstablishTradeRoute || type == CommandType::CancelTradeRoute ||
        type == CommandType::GatherIntelligence || type == CommandType::Sabotage;
}
bool campaignExpansionCommand(CommandType type) {
    return type == CommandType::TerraformPlanet || type == CommandType::CreateTaskForce ||
        type == CommandType::DisbandTaskForce || type == CommandType::SetTaskForceOrder;
}
bool modernDiplomaticCommand(CommandType type) {
    return type == CommandType::SubmitDiplomaticProposal || type == CommandType::RespondDiplomaticProposal;
}
bool modernIntelCommand(CommandType type) {
    return type == CommandType::SetIntelBudget || type == CommandType::SetIntelFocus;
}
bool colonyGovernorCommand(CommandType type) {
    return type == CommandType::SetColonyGovernor || type == CommandType::SetQueuePin || type == CommandType::ClearGovernorOverride;
}

bool hasControlCharacters(const std::string& text) {
    return std::any_of(text.begin(), text.end(), [](char ch) {
        const auto byte = static_cast<unsigned char>(ch);
        return byte < 0x20 || byte == 0x7f;
    });
}

void checkCommand(const Command& c) {
    // Player-supplied text is saved inside the pending-command line; control
    // characters (newlines in particular) would make the save unloadable.
    require(c.proposalReason.size() <= 256 && !hasControlCharacters(c.proposalReason),
        "Proposal reason must be at most 256 printable characters");
    require(!hasControlCharacters(c.taskForceName), "Task force name contains control characters");
    require(static_cast<int>(c.stance) >= 0 && static_cast<int>(c.stance) <= 3 &&
        (c.type == CommandType::SetTacticalStance || c.type == CommandType::SetTaskForceOrder || c.stance == TacticalStance::Balanced),
        "Invalid tactical stance field");
    require(c.referenceId >= -1 && c.referenceId < static_cast<int>(referenceData::ShipCatalogue.size()) &&
        (c.type == CommandType::EnqueueBuilding || c.referenceId == -1), "Invalid reference project field");
    require(c.type == CommandType::AssignWorkers || c.type == CommandType::EnqueueBuilding ||
        c.type == CommandType::MoveFleet || c.type == CommandType::Colonize || c.type == CommandType::ToggleShipyard ||
        c.type == CommandType::CancelFirstConstruction || c.type == CommandType::PrioritizeLastConstruction ||
        c.type == CommandType::MoveConstruction || c.type == CommandType::RemoveConstruction || c.type == CommandType::AllocateResearch || diplomatic(c.type) ||
        c.type == CommandType::AttackFleet || c.type == CommandType::RetreatFleet ||
        c.type == CommandType::Invade || c.type == CommandType::SetTacticalStance || majorCommand(c.type) || campaignExpansionCommand(c.type) ||
        c.type == CommandType::BuyConstruction || c.type == CommandType::ScrapStructure ||
        modernDiplomaticCommand(c.type) || modernIntelCommand(c.type) || colonyGovernorCommand(c.type) ||
        gameplay::isGameplayCommand(c.type),
        "Unknown command type");
    if (gameplay::isGameplayCommand(c.type)) { gameplay::checkCommand(c); return; }
    gameplay::checkUnrelatedFields(c);

    if (campaignExpansionCommand(c.type)) {
        require(c.foodWorkers == 0 && c.industryWorkers == 0 && c.researchWorkers == 0 && c.building == Building::Farm,
            "Campaign command contains unrelated workforce");
        for (int amount : c.researchAllocation) require(amount == 0, "Campaign command contains research fields");
        require(c.targetEmpire == 0 && c.queueIndex == 0 && c.queueDestination == 0, "Campaign command contains unrelated fields");
        if (c.type == CommandType::TerraformPlanet) {
            require(c.fleetId > 0 && c.systemId > 0 && c.planetIndex >= 0 && c.targetFleetId == 0 &&
                c.targetSystemId == 0 && c.taskForceId == 0 && c.memberFleetIds.empty(),
                "Invalid terraforming order fields");
        } else if (c.type == CommandType::CreateTaskForce) {
            require(c.taskForceId == 0 && !c.memberFleetIds.empty() && c.memberFleetIds.size() <= 256 &&
                c.systemId == 0 && c.targetFleetId == 0 && c.targetSystemId == 0,
                "Invalid create task force fields");
            require(c.taskForceName.size() <= 64, "Task force name exceeds 64 characters");
            require(c.taskForceName.find_first_of("\r\n\t") == std::string::npos, "Task force name contains control characters");
            for (int fid : c.memberFleetIds) require(fid > 0, "Invalid member fleet ID in task force");
        } else if (c.type == CommandType::DisbandTaskForce) {
            require(c.taskForceId > 0 && c.fleetId == 0 && c.systemId == 0 && c.targetFleetId == 0 &&
                c.targetSystemId == 0 && c.memberFleetIds.empty(),
                "Invalid disband task force fields");
        } else if (c.type == CommandType::SetTaskForceOrder) {
            require(c.taskForceId > 0 && c.fleetId == 0 && c.systemId == 0 && c.targetSystemId == 0 &&
                c.memberFleetIds.empty(),
                "Invalid set task force order fields");
        }
        return;
    }

    if (c.type == CommandType::BuyConstruction || c.type == CommandType::ScrapStructure) {
        require(c.systemId > 0 && c.fleetId == 0 && c.targetX == 0 && c.targetY == 0 &&
            c.foodWorkers == 0 && c.industryWorkers == 0 && c.researchWorkers == 0 &&
            c.targetEmpire == 0 && c.targetFleetId == 0 && c.targetSystemId == 0 &&
            c.queueIndex == 0 && c.queueDestination == 0 && c.taskForceId == 0 && c.memberFleetIds.empty(),
            "Invalid colony structure order fields");
        for (int amount : c.researchAllocation) require(amount == 0, "Colony order contains research fields");
        if (c.type == CommandType::BuyConstruction) {
            require(c.building == Building::Farm, "Buy construction contains unrelated building");
        } else {
            require(c.building == Building::Farm || c.building == Building::Factory ||
                c.building == Building::Laboratory || c.building == Building::PowerPlant ||
                c.building == Building::FusionPlant ||
                c.building == Building::Shipyard || c.building == Building::DefenseBattery,
                "Unsupported structure for scrapping");
        }
        return;
    }

    if (colonyGovernorCommand(c.type)) {
        require(c.systemId > 0 && c.fleetId == 0 && c.targetX == 0 && c.targetY == 0 &&
            c.foodWorkers == 0 && c.industryWorkers == 0 && c.researchWorkers == 0 &&
            c.targetEmpire == 0 && c.targetFleetId == 0 && c.targetSystemId == 0 &&
            c.taskForceId == 0 && c.memberFleetIds.empty(), "Governor command contains unrelated fields");
        for (int amount : c.researchAllocation) require(amount == 0, "Governor command contains research fields");
        if (c.type == CommandType::SetColonyGovernor) {
            require(static_cast<int>(c.governorWorkforce) >= 0 && static_cast<int>(c.governorWorkforce) <= 4, "Invalid governor workforce priority");
            require(static_cast<int>(c.governorQueue) >= 0 && static_cast<int>(c.governorQueue) <= 4, "Invalid governor queue priority");
        } else if (c.type == CommandType::SetQueuePin) {
            require(c.queueIndex >= 0 && c.queueIndex < MaxQueue, "Invalid queue pin index");
        }
        return;
    }

    if (modernDiplomaticCommand(c.type)) {
        require(c.foodWorkers == 0 && c.industryWorkers == 0 && c.researchWorkers == 0 && c.building == Building::Farm &&
            c.fleetId == 0 && c.targetX == 0 && c.targetY == 0 && c.queueIndex == 0 && c.queueDestination == 0 &&
            c.taskForceId == 0 && c.memberFleetIds.empty(), "Diplomatic command contains unrelated fields");
        for (int amount : c.researchAllocation) require(amount == 0, "Diplomatic command contains research fields");
        if (c.type == CommandType::SubmitDiplomaticProposal) {
            require(diplomacy::isValidTreatyType(static_cast<int>(c.proposalTreatyType)), "Invalid treaty type");
            require(diplomacy::isValidTreatyDuration(static_cast<int>(c.proposalDuration)), "Invalid treaty duration");
            require(diplomacy::isValidDiplomaticTone(static_cast<int>(c.proposalTone)), "Invalid diplomatic tone");
            require(c.proposalCreditOffer >= 0 && c.proposalCreditOffer <= MaxBalance, "Proposal credit offer out of bounds");
            require(c.proposalCreditDemand >= 0 && c.proposalCreditDemand <= MaxBalance, "Proposal credit demand out of bounds");
            if (c.proposalTargetIsMinor) {
                require(c.targetSystemId > 0 || c.systemId > 0, "Minor proposal requires target system ID");
            } else {
                require(c.targetEmpire >= 1 && c.targetEmpire <= 5, "Major proposal requires target empire");
            }
        } else {
            require(c.proposalId > 0, "Proposal response requires valid proposal ID");
        }
        return;
    }

    if (modernIntelCommand(c.type)) {
        require(c.foodWorkers == 0 && c.industryWorkers == 0 && c.researchWorkers == 0 && c.building == Building::Farm &&
            c.fleetId == 0 && c.targetX == 0 && c.targetY == 0 && c.queueIndex == 0 && c.queueDestination == 0 &&
            c.taskForceId == 0 && c.memberFleetIds.empty(), "Intel command contains unrelated fields");
        for (int amount : c.researchAllocation) require(amount == 0, "Intel command contains research fields");
        if (c.type == CommandType::SetIntelBudget) {
            require(c.intelInternalSecurity >= 0 && c.intelInternalSecurity <= 100, "Internal security percentage out of bounds");
            require(c.intelRivalIds.size() == c.intelEspionagePcts.size() && c.intelRivalIds.size() == c.intelSabotagePcts.size(),
                "Mismatched rival allocation arrays");
            for (int rid : c.intelRivalIds) {
                require(rid >= 1 && rid <= 5, "Invalid rival ID for intel budget");
            }
            for (int esp : c.intelEspionagePcts) {
                require(esp >= 0 && esp <= 100, "Espionage percentage out of bounds");
            }
            for (int sab : c.intelSabotagePcts) {
                require(sab >= 0 && sab <= 100, "Sabotage percentage out of bounds");
            }
        } else {
            require(c.targetEmpire >= 1 && c.targetEmpire <= 5, "Invalid target empire for intel focus");
            require(static_cast<int>(c.intelDepartmentFocus) >= 0 && static_cast<int>(c.intelDepartmentFocus) <= 3,
                "Invalid intel department focus");
        }
        return;
    }

    if (majorCommand(c.type) || c.type == CommandType::AttackFleet || c.type == CommandType::RetreatFleet ||
        c.type == CommandType::Invade || c.type == CommandType::SetTacticalStance) {
        require(c.foodWorkers == 0 && c.industryWorkers == 0 && c.researchWorkers == 0 && c.building == Building::Farm &&
            c.targetX == 0 && c.targetY == 0 && c.queueIndex == 0 && c.queueDestination == 0,
            "Strategic order contains unrelated fields");
        for (int amount : c.researchAllocation) require(amount == 0, "Strategic order contains research fields");
        if (c.type == CommandType::AttackFleet) require(c.fleetId > 0 && c.targetFleetId > 0 && c.systemId == 0 &&
            c.targetSystemId == 0 && c.targetEmpire == 0, "Invalid attack fields");
        else if (c.type == CommandType::RetreatFleet) require(c.fleetId > 0 && c.targetFleetId == 0 && c.systemId == 0 &&
            c.targetSystemId == 0 && c.targetEmpire == 0, "Invalid retreat fields");
        else if (c.type == CommandType::Invade) require(c.fleetId > 0 && c.systemId > 0 && c.targetFleetId == 0 &&
            c.targetSystemId == 0 && c.targetEmpire == 0, "Invalid invasion fields");
        else if (c.type == CommandType::SetTacticalStance) require(c.fleetId > 0 && c.systemId == 0 &&
            c.targetFleetId == 0 && c.targetSystemId == 0 && c.targetEmpire == 0,
            "Invalid tactical stance order fields");
        else {
            require(c.fleetId == 0 && c.targetFleetId == 0 && c.targetEmpire >= 0 && c.targetEmpire <= 5,
                "Invalid empire order fields");
            if (c.type == CommandType::EstablishTradeRoute)
                require(c.systemId > 0 && c.targetSystemId > 0, "Trade route requires two colonies");
            else if (c.type == CommandType::CancelTradeRoute)
                require(c.systemId > 0 && c.targetSystemId == 0, "Invalid trade cancellation fields");
            else if (c.type == CommandType::Sabotage)
                require(c.systemId == 0 && c.targetSystemId > 0, "Sabotage requires a target colony");
            else require(c.systemId == 0 && c.targetSystemId == 0, "Invalid empire order system fields");
        }
        return;
    }
    require(c.targetEmpire == 0 && c.targetFleetId == 0 && c.targetSystemId == 0,
        "Unrelated strategic order fields");
    if (c.type == CommandType::AllocateResearch) {
        int total = 0;
        for (int amount : c.researchAllocation) { require(amount >= 0 && amount <= 100, "Invalid research allocation"); total += amount; }
        require(total == 100, "Research allocations must total 100 percent");
        require(c.systemId == 0 && c.foodWorkers == 0 && c.industryWorkers == 0 && c.researchWorkers == 0 &&
            c.building == Building::Farm && c.fleetId == 0 && c.targetX == 0 && c.targetY == 0 &&
            c.queueIndex == 0 && c.queueDestination == 0, "Research order contains unrelated fields");
        return;
    }
    for (int amount : c.researchAllocation) require(amount == 0, "Unrelated research allocation fields");
    if (c.type == CommandType::MoveConstruction || c.type == CommandType::RemoveConstruction) {
        require(c.queueIndex >= 0 && c.queueIndex < MaxQueue && c.queueDestination >= 0 &&
            c.queueDestination < MaxQueue, "Queue position out of bounds");
        require(c.type != CommandType::RemoveConstruction || c.queueDestination == 0, "Invalid removal destination");
    } else require(c.queueIndex == 0 && c.queueDestination == 0, "Unrelated queue position fields");
    if (diplomatic(c.type)) {
        require(c.systemId > 0 && c.foodWorkers == 0 && c.industryWorkers == 0 && c.researchWorkers == 0 &&
            c.building == Building::Farm && c.fleetId == 0 && c.targetX == 0 && c.targetY == 0, "Invalid diplomatic command fields");
        return;
    }
    if (c.type == CommandType::MoveFleet || c.type == CommandType::Colonize) {
        require(c.fleetId > 0 && c.systemId == 0 && c.foodWorkers == 0 && c.industryWorkers == 0 &&
            c.researchWorkers == 0 && c.building == Building::Farm, "Invalid fleet command fields");
        require(c.type != CommandType::Colonize || (c.targetX == 0 && c.targetY == 0),
            "Colonize command contains unrelated destination");
        return;
    }
    require(c.fleetId == 0 && c.targetX == 0 && c.targetY == 0, "Colony command contains fleet fields");
    if (c.type == CommandType::ToggleShipyard || c.type == CommandType::CancelFirstConstruction ||
        c.type == CommandType::PrioritizeLastConstruction || c.type == CommandType::MoveConstruction ||
        c.type == CommandType::RemoveConstruction) {
        require(c.foodWorkers == 0 && c.industryWorkers == 0 && c.researchWorkers == 0 &&
            c.building == Building::Farm, "Invalid colony control command");
        return;
    }
    require(c.foodWorkers >= 0 && c.foodWorkers <= MaxPopulation &&
        c.industryWorkers >= 0 && c.industryWorkers <= MaxPopulation &&
        c.researchWorkers >= 0 && c.researchWorkers <= MaxPopulation, "Invalid command workforce");
    require(buildingValid(c.building), "Unknown command building");
    require(c.type != CommandType::EnqueueBuilding ||
        (c.foodWorkers == 0 && c.industryWorkers == 0 && c.researchWorkers == 0),
        "Enqueue command contains unrelated workforce");
    require(c.type != CommandType::AssignWorkers || c.building == Building::Farm,
        "Workforce command contains unrelated building");
}

EmpireState& mutableEmpire(GameState& s, int id);
bool known(const GameState& s, int owner, int x, int y);
MajorRelation& mutableRelation(GameState& s, int a, int b) {
    if (a > b) std::swap(a, b);
    const auto found = std::find_if(s.majorRelations.begin(), s.majorRelations.end(), [&](const MajorRelation& r) {
        return r.first == a && r.second == b;
    });
    require(found != s.majorRelations.end(), "Empire relation unavailable");
    return *found;
}
void draftDiplomacy(GameState& s, int owner, int systemId, CommandType type) {
    const auto m = minorAt(s, systemId);
    require(m && m->contact[owner] && m->memberOf == 0, "Diplomacy needs contact with an independent minor");
    require(s.diplomacy.size() < MaxPending && s.nextDiplomacyId < 10000000, "Diplomatic offer capacity reached");
    require(std::none_of(s.diplomacy.begin(), s.diplomacy.end(), [&](const DiplomaticOffer& o) {
        return o.empireId == owner && o.systemId == systemId;
    }), "An offer to this minor is already pending");
    auto& e = mutableEmpire(s, owner);
    if (type == CommandType::SendGift) { require(e.credits >= 50, "Gift needs 50 available credits"); e.credits -= 50; }
    s.diplomacy.push_back({s.nextDiplomacyId++, owner, systemId, s.turn + (type == CommandType::SendGift ? 1 : 0), type});
}

void detachFromTaskForce(GameState& state, Fleet& fleet) {
    if (fleet.taskForceId <= 0) return;
    const int tfId = fleet.taskForceId;
    fleet.taskForceId = 0;
    auto tfIt = std::find_if(state.taskForces.begin(), state.taskForces.end(), [tfId](const TaskForce& tf) {
        return tf.id == tfId;
    });
    if (tfIt != state.taskForces.end()) {
        std::erase(tfIt->fleetIds, fleet.id);
        if (tfIt->fleetIds.empty()) {
            state.taskForces.erase(tfIt);
        }
    }
}

void reconcileTaskForces(GameState& s) {
    for (auto& f : s.fleets) {
        if (f.taskForceId > 0) {
            auto tfIt = std::find_if(s.taskForces.begin(), s.taskForces.end(), [&](const TaskForce& tf) {
                return tf.id == f.taskForceId;
            });
            if (tfIt == s.taskForces.end() || f.hull <= 0 || f.retreat || f.colonize) {
                f.taskForceId = 0;
            } else {
                auto it = std::find(tfIt->fleetIds.begin(), tfIt->fleetIds.end(), f.id);
                if (it == tfIt->fleetIds.end()) {
                    f.taskForceId = 0;
                }
            }
        }
    }
    for (auto& tf : s.taskForces) {
        std::erase_if(tf.fleetIds, [&](int fid) {
            auto fIt = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const Fleet& f) { return f.id == fid; });
            if (fIt == s.fleets.end() || fIt->hull <= 0 || fIt->retreat || fIt->colonize || fIt->taskForceId != tf.id) {
                if (fIt != s.fleets.end() && fIt->taskForceId == tf.id) {
                    fIt->taskForceId = 0;
                }
                return true;
            }
            return false;
        });
    }
    for (auto& tf : s.taskForces) {
        if (!tf.fleetIds.empty()) {
            auto leader = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const Fleet& f) { return f.id == tf.fleetIds.front(); });
            if (leader != s.fleets.end()) {
                const int lx = leader->x, ly = leader->y;
                std::erase_if(tf.fleetIds, [&](int fid) {
                    auto fIt = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const Fleet& f) { return f.id == fid; });
                    if (fIt != s.fleets.end() && (fIt->x != lx || fIt->y != ly)) {
                        fIt->taskForceId = 0;
                        return true;
                    }
                    return false;
                });
            }
        }
    }
    std::erase_if(s.taskForces, [](const TaskForce& tf) { return tf.fleetIds.empty(); });
    for (auto& tf : s.taskForces) {
        if (!tf.fleetIds.empty()) {
            auto leader = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const Fleet& f) { return f.id == tf.fleetIds.front(); });
            if (leader != s.fleets.end()) {
                tf.targetX = leader->targetX;
                tf.targetY = leader->targetY;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Lane B: modern diplomacy & intelligence wiring (see remaster/docs/DIPLOMACY.md)
// ---------------------------------------------------------------------------
bool campaignEmpire(const GameState& s, int id) {
    return id == s.playerEmpire || std::any_of(s.rivals.begin(), s.rivals.end(), [id](const EmpireState& e) { return e.id == id; });
}
// An empire with no colonies left is eliminated: it can neither negotiate nor spy.
bool empireEliminatedInternal(const GameState& s, int id) {
    if (!campaignEmpire(s, id)) return false;
    return std::none_of(s.colonies.begin(), s.colonies.end(), [id](const Colony& c) { return c.ownerId == id; });
}
std::vector<int> survivingEmpires(const GameState& s) {
    std::vector<int> ids;
    if (!empireEliminatedInternal(s, s.playerEmpire)) ids.push_back(s.playerEmpire);
    for (const auto& r : s.rivals) if (!empireEliminatedInternal(s, r.id)) ids.push_back(r.id);
    std::sort(ids.begin(), ids.end());
    return ids;
}
bool modernDiplomacy(const GameState& s) { return s.campaignExpansionEnabled && s.modernEnginesActive; }
void enactWar(GameState& s, int aggressor, int defender);
void planAiWarDeclarations(GameState& s, int ownerId);

class CampaignDiplomacyContext : public diplomacy::DiplomacyContext {
public:
    const GameState& state;
    explicit CampaignDiplomacyContext(const GameState& s) : state(s) {}

    bool hasContact(diplomacy::ParticipantId a, diplomacy::ParticipantId b) const override {
        if (a == b) return true;
        if (isEliminated(a) || isEliminated(b)) return false;
        if (!a.isMinor && !b.isMinor) {
            const auto* rel = majorRelation(state, a.id, b.id);
            return rel && rel->contact;
        }
        if (a.isMinor && !b.isMinor) {
            const auto* m = minorAt(state, a.id);
            return m && b.id >= 1 && b.id <= 5 && m->contact[b.id];
        }
        if (!a.isMinor && b.isMinor) {
            const auto* m = minorAt(state, b.id);
            return m && a.id >= 1 && a.id <= 5 && m->contact[a.id];
        }
        return false;
    }

    bool isAtWar(diplomacy::ParticipantId a, diplomacy::ParticipantId b) const override {
        if (a == b) return false;
        if (!a.isMinor && !b.isMinor) {
            const auto* rel = majorRelation(state, a.id, b.id);
            return rel && rel->atWar;
        }
        return false;
    }

    int64_t creditBalance(diplomacy::ParticipantId p) const override {
        if (p.isMinor) return 0;
        try {
            return empire(state, p.id).credits;
        } catch (...) {
            return 0;
        }
    }

    int64_t militaryPower(diplomacy::ParticipantId p) const override {
        if (p.isMinor) return 0;
        int64_t mil = 0;
        for (const auto& f : state.fleets) {
            if (f.ownerId == p.id && f.hull > 0) mil += fleetAttack(f) * 10 + f.hull;
        }
        return mil;
    }

    int64_t economicPower(diplomacy::ParticipantId p) const override {
        if (p.isMinor) return 0;
        int64_t econ = 0;
        for (const auto& c : state.colonies) {
            if (c.ownerId == p.id) econ += c.population + c.factories * 5 + c.farms * 2;
        }
        return econ;
    }

    int trust(diplomacy::ParticipantId a, diplomacy::ParticipantId b) const override {
        if (!a.isMinor && !b.isMinor) {
            const auto* rel = majorRelation(state, a.id, b.id);
            return rel ? rel->trust : 50;
        }
        if (a.isMinor && !b.isMinor) {
            const auto* m = minorAt(state, a.id);
            return (m && b.id >= 1 && b.id <= 5) ? m->attitude[b.id] : 50;
        }
        if (!a.isMinor && b.isMinor) {
            const auto* m = minorAt(state, b.id);
            return (m && a.id >= 1 && a.id <= 5) ? m->attitude[a.id] : 50;
        }
        return 50;
    }

    bool isMinor(int id) const override {
        return minorAt(state, id) != nullptr;
    }

    bool isIndependentMinor(int minorId) const override {
        const auto* m = minorAt(state, minorId);
        return m != nullptr && m->memberOf == 0;
    }

    int minorMembershipOwner(int minorId) const override {
        const auto* m = minorAt(state, minorId);
        return m ? m->memberOf : 0;
    }

    bool isHuman(diplomacy::ParticipantId p) const override {
        return !p.isMinor && p.id == state.playerEmpire;
    }

    bool isEliminated(diplomacy::ParticipantId p) const override {
        return !p.isMinor && empireEliminatedInternal(state, p.id);
    }

    std::vector<int> majorEmpires() const override {
        return survivingEmpires(state);
    }

    int warDurationTurns(diplomacy::ParticipantId a, diplomacy::ParticipantId b) const override {
        if (a.isMinor || b.isMinor) return 0;
        const auto* rel = majorRelation(state, a.id, b.id);
        return (rel && rel->atWar) ? std::max(0, state.turn - rel->warStartTurn) : 0;
    }
};

void syncDiplomacyStateInternal(GameState& s) {
    s.diplomacyEngineState.currentTurn = s.turn;
    for (const auto& m : s.minors) {
        auto* ms = s.diplomacyEngineState.getMinorState(m.systemId);
        if (!ms) {
            diplomacy::MinorDiplomacyState newMs;
            newMs.minorId = m.systemId;
            newMs.systemId = m.systemId;
            newMs.raceName = m.name;
            for (int i = 1; i <= 5; ++i) {
                newMs.contacts[i] = m.contact[i];
                newMs.attitudes[i] = m.attitude[i];
                newMs.friendshipTurns[i] = m.friendshipTurn[i];
            }
            newMs.membershipOwner = m.memberOf;
            newMs.membershipTurn = (m.memberOf != 0) ? s.turn : -1;
            s.diplomacyEngineState.minorStates.push_back(newMs);
        } else {
            for (int i = 1; i <= 5; ++i) {
                ms->contacts[i] = m.contact[i];
                ms->attitudes[i] = m.attitude[i];
                ms->friendshipTurns[i] = m.friendshipTurn[i];
            }
            if (ms->membershipOwner != m.memberOf) {
                ms->membershipOwner = m.memberOf;
                if (m.memberOf != 0 && ms->membershipTurn < 0) {
                    ms->membershipTurn = s.turn;
                } else if (m.memberOf == 0) {
                    ms->membershipTurn = -1;
                }
            }
        }
    }

    std::vector<int> activeEmpires{s.playerEmpire};
    for (const auto& r : s.rivals) activeEmpires.push_back(r.id);
    std::sort(activeEmpires.begin(), activeEmpires.end());

    for (int empId : activeEmpires) {
        if (s.empireIntelStates.find(empId) == s.empireIntelStates.end()) {
            intelligence::EmpireIntelState is;
            is.empireId = empId;
            std::vector<int> rivals;
            for (int other : activeEmpires) if (other != empId) rivals.push_back(other);
            is.budget.setDefault(rivals, empId);
            is.securityTurnCounter = 1;
            is.internalSecurityStockpile = 0;
            s.empireIntelStates[empId] = is;
        }
    }
}

void applyDraft(GameState& state) {
    if (state.diplomacyEngineState.minorStates.empty() && !state.minors.empty()) {
        syncDiplomacyStateInternal(state);
    }
    std::set<int> orderedFleets;
    for (const auto& command : state.pending) {
        if (gameplay::isGameplayCommand(command.type)) {
            gameplay::applyCommand(state, command, orderedFleets);
            continue;
        }
        if (modernDiplomaticCommand(command.type)) {
            // Legacy campaigns do not serialize the modern engines: reject rather than lose the order.
            require(modernDiplomacy(state), "Modern diplomacy requires a modern (reference expansion) campaign");
            if (command.type == CommandType::SubmitDiplomaticProposal) {
                CampaignDiplomacyContext ctx(state);
                diplomacy::ParticipantId proposer = diplomacy::ParticipantId::major(state.playerEmpire);
                diplomacy::ParticipantId target = command.proposalTargetIsMinor
                    ? diplomacy::ParticipantId::minor(command.targetSystemId ? command.targetSystemId : command.systemId)
                    : diplomacy::ParticipantId::major(command.targetEmpire);
                require(proposer != target, "Cannot propose treaty to own empire");
                require(!ctx.isEliminated(target), "That empire has been eliminated");
                require(ctx.hasContact(proposer, target), "Diplomatic proposal requires first contact");
                if (command.proposalCreditOffer > 0) {
                    require(state.credits >= command.proposalCreditOffer, "Insufficient credits for proposal offer");
                }
                std::string err;
                bool ok = diplomacy::DiplomacyEngine::submitProposal(
                    state.diplomacyEngineState,
                    ctx,
                    proposer,
                    target,
                    command.proposalTreatyType,
                    command.proposalDuration,
                    command.proposalTone,
                    command.proposalCreditOffer,
                    command.proposalCreditDemand,
                    nullptr,
                    &err
                );
                require(ok, err.empty() ? "Invalid diplomatic proposal" : err.c_str());
            } else if (command.type == CommandType::RespondDiplomaticProposal) {
                auto propIt = std::find_if(state.diplomacyEngineState.proposals.begin(),
                                           state.diplomacyEngineState.proposals.end(),
                                           [&](const diplomacy::Proposal& p) { return p.id == command.proposalId; });
                require(propIt != state.diplomacyEngineState.proposals.end(), "Proposal not found");
                require(!propIt->resolved, "Proposal is already resolved");
                diplomacy::ParticipantId playerPart = diplomacy::ParticipantId::major(state.playerEmpire);
                require(propIt->target == playerPart, "Not authorized to respond to proposal: target is not player");
                require(propIt->treatyType != diplomacy::TreatyType::War, "A declaration of war cannot be answered");
                bool ok = diplomacy::DiplomacyEngine::respondToOffer(
                    state.diplomacyEngineState,
                    playerPart,
                    command.proposalId,
                    command.proposalAccept,
                    command.proposalReason
                );
                require(ok, "Failed to respond to diplomatic proposal");
            }
            continue;
        }
        if (modernIntelCommand(command.type)) {
            require(modernDiplomacy(state), "Modern intelligence requires a modern (reference expansion) campaign");
            if (command.type == CommandType::SetIntelBudget) {
                for (int rid : command.intelRivalIds) {
                    require(std::any_of(state.rivals.begin(), state.rivals.end(), [rid](const EmpireState& r) { return r.id == rid; }),
                        "Target empire for intel budget is not a campaign rival");
                    require(!empireEliminatedInternal(state, rid), "Cannot budget intelligence against an eliminated empire");
                }
                auto& intelState = state.empireIntelStates[state.playerEmpire];
                intelState.empireId = state.playerEmpire;
                intelligence::BudgetAllocations newBudget;
                newBudget.internalSecurity.percentage = command.intelInternalSecurity;
                newBudget.internalSecurity.isLocked = false;
                for (std::size_t i = 0; i < command.intelRivalIds.size(); ++i) {
                    intelligence::RivalAllocation ra;
                    ra.rivalId = command.intelRivalIds[i];
                    ra.espionage.percentage = command.intelEspionagePcts[i];
                    ra.espionage.isLocked = false;
                    ra.sabotage.percentage = command.intelSabotagePcts[i];
                    ra.sabotage.isLocked = false;
                    newBudget.rivalAllocations.push_back(ra);
                }
                require(newBudget.isValid(state.playerEmpire), "Invalid intel budget allocations");
                intelState.budget = newBudget;
            } else if (command.type == CommandType::SetIntelFocus) {
                require(command.targetEmpire != state.playerEmpire, "Cannot target own empire for intel focus");
                require(std::any_of(state.rivals.begin(), state.rivals.end(), [&](const EmpireState& r) { return r.id == command.targetEmpire; }),
                    "Target empire for intel focus is not a campaign rival");
                require(!empireEliminatedInternal(state, command.targetEmpire), "Cannot focus intelligence on an eliminated empire");
                auto& intelState = state.empireIntelStates[state.playerEmpire];
                intelState.empireId = state.playerEmpire;
                auto& rivalState = intelState.getOrCreateRivalState(command.targetEmpire);
                rivalState.focusDepartment = command.intelDepartmentFocus;
            }
            continue;
        }
        if (diplomatic(command.type)) { draftDiplomacy(state, state.playerEmpire, command.systemId, command.type); continue; }
        if (campaignExpansionCommand(command.type)) {
            if (command.type == CommandType::TerraformPlanet) {
                require(state.campaignExpansionEnabled, "Terraforming requires campaign expansion");
                require(orderedFleets.insert(command.fleetId).second, "Cancel existing fleet draft before replacing it");
                auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& x) { return x.id == command.fleetId; });
                require(f != state.fleets.end() && f->ownerId == state.playerEmpire, "Fleet is not owned");
                require(f->colonyShip, "Only colony ships can terraform");
                require(f->hull > 0 && !f->retreat && f->attackTarget == 0 && !f->colonize, "Colony ship is busy");
                const auto col = colonyAt(state, command.systemId);
                require(!col || col->ownerId == f->ownerId, "Cannot terraform a foreign-owned colony");
                const auto sys = std::find_if(state.systems.begin(), state.systems.end(), [&](const System& s) { return s.id == command.systemId; });
                require(sys != state.systems.end(), "Unknown system for terraforming");
                require(f->x == sys->x && f->y == sys->y, "Colony ship must be at target system to terraform");
                require(known(state, f->ownerId, sys->x, sys->y), "Target system must be surveyed");
                require(command.planetIndex >= 0 && static_cast<std::size_t>(command.planetIndex) < sys->planets.size(),
                    "Planet index out of bounds");
                const auto& planet = sys->planets[static_cast<std::size_t>(command.planetIndex)];
                require(planet.planetClass != PlanetClass::GasGiant && planet.planetClass != PlanetClass::Asteroids,
                    "Cannot terraform uninhabitable celestial body");
                require(planet.terraformState != TerraformingState::Terraformed && planet.terraformState != TerraformingState::Uninhabitable,
                    "Planet is already terraformed");
                detachFromTaskForce(state, *f);
                f->terraforming = true;
                f->terraformSystemId = command.systemId;
                f->terraformPlanetIndex = command.planetIndex;
                f->targetX = f->x; f->targetY = f->y;
            } else if (command.type == CommandType::CreateTaskForce) {
                require(!command.memberFleetIds.empty(), "Task force must have at least one member");
                require(state.taskForces.size() < static_cast<std::size_t>(MaxTaskForces) &&
                    state.nextTaskForceId < 10000000, "Task force capacity reached");
                std::set<int> uniqueIds;
                int commonX = -1, commonY = -1;
                for (int fid : command.memberFleetIds) {
                    require(uniqueIds.insert(fid).second, "Duplicate ship in task force formation");
                    auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
                    require(f != state.fleets.end() && f->ownerId == state.playerEmpire, "Task force member is not owned");
                    require(f->hull > 0 && f->taskForceId == 0 && !f->colonize && !f->terraforming, "Task force member unavailable");
                    if (commonX == -1) { commonX = f->x; commonY = f->y; }
                    else { require(f->x == commonX && f->y == commonY, "All task force ships must be in the same sector"); }
                }
                TaskForce tf;
                tf.id = state.nextTaskForceId++;
                tf.ownerId = state.playerEmpire;
                tf.name = command.taskForceName.empty() ? ("Task Force " + std::to_string(tf.id)) : command.taskForceName;
                tf.fleetIds = command.memberFleetIds;
                tf.targetX = commonX; tf.targetY = commonY;
                tf.stance = TacticalStance::Balanced;
                for (int fid : command.memberFleetIds) {
                    auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
                    f->taskForceId = tf.id;
                }
                state.taskForces.push_back(tf);
            } else if (command.type == CommandType::DisbandTaskForce) {
                auto tfIt = std::find_if(state.taskForces.begin(), state.taskForces.end(), [&](const TaskForce& tf) { return tf.id == command.taskForceId; });
                require(tfIt != state.taskForces.end() && tfIt->ownerId == state.playerEmpire, "Task force unavailable or not owned");
                for (int fid : tfIt->fleetIds) {
                    auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
                    if (f != state.fleets.end()) f->taskForceId = 0;
                }
                state.taskForces.erase(tfIt);
            } else if (command.type == CommandType::SetTaskForceOrder) {
                auto tfIt = std::find_if(state.taskForces.begin(), state.taskForces.end(), [&](const TaskForce& tf) { return tf.id == command.taskForceId; });
                require(tfIt != state.taskForces.end() && tfIt->ownerId == state.playerEmpire, "Task force unavailable or not owned");
                if (command.targetX >= 0 && command.targetY >= 0) {
                    require(command.targetX < state.width && command.targetY < state.height,
                        "Task force destination outside galaxy");
                    for (int fid : tfIt->fleetIds) {
                        auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
                        require(f != state.fleets.end(), "Missing member ship");
                        require(!route(state, *f, command.targetX, command.targetY).empty(), "Task force destination outside supplied range");
                    }
                    tfIt->targetX = command.targetX; tfIt->targetY = command.targetY;
                    for (int fid : tfIt->fleetIds) {
                        auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
                        if (f != state.fleets.end()) { f->targetX = command.targetX; f->targetY = command.targetY; }
                    }
                }
                tfIt->stance = command.stance;
                tfIt->attackTarget = command.targetFleetId;
                for (int fid : tfIt->fleetIds) {
                    auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
                    if (f != state.fleets.end()) {
                        f->stance = command.stance;
                        if (command.targetFleetId > 0 && fleetAttack(*f) > 0) f->attackTarget = command.targetFleetId;
                        f->retreat = false;
                    }
                }
            }
            continue;
        }
        if (majorCommand(command.type)) {
            const int self = state.playerEmpire, other = command.targetEmpire;
            require(other != self && other >= 1 && other <= 5, "Choose another active empire");
            empire(state, other);
            auto& relation = mutableRelation(state, self, other);
            require(relation.contact, "Major diplomacy requires first contact");
            require(command.type == CommandType::CancelTradeRoute || !empireEliminatedInternal(state, other),
                "That empire has been eliminated");
            if (command.type == CommandType::DeclareWar) {
                require(!relation.atWar, "Empires are already at war");
                enactWar(state, self, other);
            } else if (command.type == CommandType::OfferPeace) {
                require(relation.atWar, "Empires are already at peace");
                if (!state.advancedRulesEnabled) { relation.atWar = false; relation.warStartTurn = state.turn; }
                else {
                    require(std::none_of(state.pendingMajorOffers.begin(), state.pendingMajorOffers.end(), [&](const MajorOffer& offer) {
                        return offer.proposer == self && offer.target == other && offer.type == command.type;
                    }), "Peace offer is already pending");
                    state.pendingMajorOffers.push_back({state.nextMajorOfferId++, self, other, state.turn + 1, command.type});
                }
            } else if (command.type == CommandType::ProposeTradeTreaty) {
                require(state.advancedRulesEnabled && !relation.atWar && !relation.tradeTreaty,
                    "Trade treaty needs peace and no existing treaty");
                require(std::none_of(state.pendingMajorOffers.begin(), state.pendingMajorOffers.end(), [&](const MajorOffer& offer) {
                    return offer.proposer == self && offer.target == other && offer.type == command.type;
                }), "Trade proposal is already pending");
                state.pendingMajorOffers.push_back({state.nextMajorOfferId++, self, other, state.turn + 1, command.type});
            } else if (command.type == CommandType::EstablishTradeRoute) {
                require(!relation.atWar, "War blocks trade");
                require(!state.advancedRulesEnabled || relation.tradeTreaty, "An accepted trade treaty is required");
                const auto from = colonyAt(state, command.systemId);
                const auto to = colonyAt(state, command.targetSystemId);
                require(from && from->ownerId == self && to && to->ownerId == other, "Trade needs owned and foreign colonies");
                require(std::none_of(state.tradeRoutes.begin(), state.tradeRoutes.end(), [&](const TradeRoute& route) {
                    return route.ownerId == self && route.fromSystemId == command.systemId;
                }), "Source colony already has a trade route");
                if (!state.advancedRulesEnabled) relation.tradeTreaty = true;
                state.tradeRoutes.push_back({self, command.systemId, command.targetSystemId});
            } else if (command.type == CommandType::CancelTradeRoute) {
                const auto oldSize = state.tradeRoutes.size();
                std::erase_if(state.tradeRoutes, [&](const TradeRoute& route) {
                    return route.ownerId == self && route.fromSystemId == command.systemId;
                });
                require(state.tradeRoutes.size() != oldSize, "No trade route from this colony");
            } else {
                auto& owner = mutableEmpire(state, self);
                const std::int64_t cost = command.type == CommandType::Sabotage ? 50 : 20;
                require(owner.intelligence >= cost, "Insufficient intelligence stockpile");
                owner.intelligence -= cost;
                IntelReport report; report.turn = state.turn; report.viewer = self; report.target = other;
                report.estimatedCredits = empire(state, other).credits;
                for (const auto& c : state.colonies) if (c.ownerId == other) ++report.knownColonies;
                if (command.type == CommandType::Sabotage) {
                    require(relation.atWar, "Sabotage requires war");
                    const auto target = std::find_if(state.colonies.begin(), state.colonies.end(), [&](const Colony& c) {
                        return c.systemId == command.targetSystemId && c.ownerId == other;
                    });
                    require(target != state.colonies.end() && known(state, self,
                        std::find_if(state.systems.begin(), state.systems.end(), [&](const System& x) {return x.id == target->systemId;})->x,
                        std::find_if(state.systems.begin(), state.systems.end(), [&](const System& x) {return x.id == target->systemId;})->y),
                        "Target colony must be surveyed");
                    report.systemId = target->systemId;
                    report.success = true;
                    if (!target->queue.empty()) target->queue.front().work = std::max(0, target->queue.front().work - 25);
                    else target->overflow = std::max(0, target->overflow - 25);
                } else report.success = true;
                state.intelReports.push_back(report);
                if (state.intelReports.size() > 32) state.intelReports.erase(state.intelReports.begin());
            }
            continue;
        }
        if (command.type == CommandType::AttackFleet || command.type == CommandType::RetreatFleet ||
            command.type == CommandType::Invade || command.type == CommandType::SetTacticalStance) {
            if (command.type != CommandType::SetTacticalStance)
                require(orderedFleets.insert(command.fleetId).second, "Cancel existing fleet draft before replacing it");
            auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& x) { return x.id == command.fleetId; });
            require(f != state.fleets.end() && f->ownerId == state.playerEmpire, "Fleet is not owned");
            if (command.type != CommandType::SetTacticalStance) {
                detachFromTaskForce(state, *f);
            }
            if (command.type == CommandType::SetTacticalStance) {
                require(state.advancedRulesEnabled, "Tactical stance requires advanced reference rules");
                f->stance = command.stance;
            } else if (command.type == CommandType::AttackFleet) {
                require(fleetAttack(*f) > 0, "Ship cannot attack");
                const auto target = std::find_if(state.fleets.begin(), state.fleets.end(), [&](const Fleet& x) { return x.id == command.targetFleetId; });
                require(target != state.fleets.end() && target->ownerId != f->ownerId && visible(state, f->ownerId, target->x, target->y),
                    "Enemy fleet is not visible");
                require(mutableRelation(state, f->ownerId, target->ownerId).atWar, "Declare war before attacking");
                require(!route(state, *f, target->x, target->y).empty(), "Target is outside supplied range");
                f->attackTarget = target->id; f->targetX = target->x; f->targetY = target->y; f->retreat = false;
            } else if (command.type == CommandType::RetreatFleet) {
                f->retreat = true; f->attackTarget = 0;
                const System* home = nullptr; int nearest = std::numeric_limits<int>::max();
                bool homePoweredYard = false;
                for (const auto& colony : state.colonies) if (colony.ownerId == f->ownerId) {
                    const auto loc = std::find_if(state.systems.begin(), state.systems.end(), [&](const System& system) {
                        return system.id == colony.systemId;
                    });
                    const int distance = std::max(std::abs(f->x - loc->x), std::abs(f->y - loc->y));
                    const bool poweredYard = state.advancedRulesEnabled && economy(state, colony).shipyardPowered;
                    if (state.advancedRulesEnabled && f->hull < fleetMaxHull(*f)) {
                        if ((poweredYard && !homePoweredYard) || (poweredYard == homePoweredYard && distance < nearest)) {
                            nearest = distance; home = &*loc; homePoweredYard = poweredYard;
                        }
                    } else if (distance < nearest) {
                        nearest = distance; home = &*loc;
                    }
                }
                f->targetX = home ? home->x : f->x;
                f->targetY = home ? home->y : f->y;
            } else {
                require(f->shipClass == Building::Transport && f->x == f->targetX && f->y == f->targetY,
                    "Idle transport must be at invasion target");
                const auto c = colonyAt(state, command.systemId);
                require(c && c->ownerId != f->ownerId, "Target is not an enemy colony");
                const auto system = std::find_if(state.systems.begin(), state.systems.end(), [&](const System& x) { return x.id == c->systemId; });
                require(system->x == f->x && system->y == f->y && mutableRelation(state, f->ownerId, c->ownerId).atWar,
                    "Transport must be at a colony of an empire at war");
                f->invadeSystemId = c->systemId;
            }
            continue;
        }
        if (command.type == CommandType::AllocateResearch) {
            for (int i = 0; i < ResearchFields; ++i) state.technologies[i].allocation = command.researchAllocation[i];
            continue;
        }
        if (command.type == CommandType::MoveFleet || command.type == CommandType::Colonize) {
            require(orderedFleets.insert(command.fleetId).second, "Cancel existing fleet draft before replacing it");
            auto f = std::find_if(state.fleets.begin(), state.fleets.end(),
                [&](const Fleet& x) { return x.id == command.fleetId; });
            require(f != state.fleets.end() && f->ownerId == state.playerEmpire, "Fleet is not owned");
            detachFromTaskForce(state, *f);
            if (command.type == CommandType::MoveFleet) {
                require(command.targetX >= 0 && command.targetX < state.width && command.targetY >= 0 &&
                    command.targetY < state.height, "Destination outside galaxy");
                require(!route(state, *f, command.targetX, command.targetY).empty(),
                    "No supplied route: destination is outside powered shipyard range");
                f->targetX = command.targetX; f->targetY = command.targetY;
                f->terraforming = false;
            } else {
                require(f->colonyShip && f->x == f->targetX && f->y == f->targetY,
                    "Colonization requires an idle colony ship at a system");
                const auto system = std::find_if(state.systems.begin(), state.systems.end(),
                    [&](const System& x) { return x.x == f->x && x.y == f->y; });
                require(system != state.systems.end() && explored(state, f->x, f->y) &&
                    !colonyAt(state, system->id) && !minorAt(state, system->id), "No unclaimed surveyed system here");
                for (const auto& other : state.fleets)
                    require(!other.colonize || other.x != f->x || other.y != f->y,
                        "Another ship is already colonizing this system");
                f->colonize = true;
                f->terraforming = false;
            }
            continue;
        }
        auto it = std::find_if(state.colonies.begin(), state.colonies.end(),
            [&](const Colony& c) { return c.systemId == command.systemId; });
        require(it != state.colonies.end() && it->ownerId == state.playerEmpire, "Command targets a non-owned system");
        if (command.type == CommandType::BuyConstruction) {
            require(!it->queue.empty(), "Construction queue is empty");
            const int cost = rushCost(state, command.systemId);
            require(cost > 0, "No construction work remaining to rush");
            auto& owner = mutableEmpire(state, state.playerEmpire);
            require(owner.credits >= cost, "Insufficient credits to rush construction");
            owner.credits -= cost;
            it->queue.front().work = constructionCost(it->queue.front());
            validateColony(*it);
            continue;
        }
        if (command.type == CommandType::ScrapStructure) {
            if (command.building == Building::Farm) {
                require(it->farms > 0, "No farm to scrap");
                require(it->foodWorkers <= (it->farms - 1) * 10, "Workforce exceeds remaining farms; reassign workers first");
                --it->farms;
            } else if (command.building == Building::Factory) {
                require(it->factories > 0, "No factory to scrap");
                require(it->industryWorkers <= (it->factories - 1) * 10, "Workforce exceeds remaining factories; reassign workers first");
                --it->factories;
            } else if (command.building == Building::Laboratory) {
                require(it->laboratories > 0, "No laboratory to scrap");
                require(it->researchWorkers <= (it->laboratories - 1) * 10, "Workforce exceeds remaining laboratories; reassign workers first");
                --it->laboratories;
            } else if (command.building == Building::PowerPlant) {
                require(it->powerPlants > 0, "No power plant to scrap");
                require(it->energy >= 10, "Energy level below decommission capacity");
                --it->powerPlants;
                it->energy -= 10;
            } else if (command.building == Building::FusionPlant) {
                // Removes exactly the 40 energy a fusion plant added.
                require(it->fusionPlants > 0, "No fusion plant to scrap");
                require(it->energy >= 40, "Energy level below decommission capacity");
                --it->fusionPlants;
                it->energy -= 40;
            } else if (command.building == Building::Shipyard) {
                require(it->shipyards > 0, "No shipyard to scrap");
                it->shipyards = 0;
            } else if (command.building == Building::DefenseBattery) {
                require(it->defenseBatteries > 0, "No defense battery to scrap");
                --it->defenseBatteries;
            } else {
                throw std::runtime_error("Unsupported structure for scrapping");
            }
            validateColony(*it);
            continue;
        }
        if (colonyGovernorCommand(command.type)) {
            if (command.type == CommandType::SetColonyGovernor) {
                it->workforcePriority = command.governorWorkforce;
                it->queuePriority = command.governorQueue;
                if (command.governorWorkforce == GovernorPriority::Manual) {
                    it->workforceManualOverride = false;
                }
                if (it->workforcePriority != GovernorPriority::Manual && !it->workforceManualOverride) {
                    evaluateWorkforceGovernor(state, *it);
                }
                if (it->queuePriority != GovernorPriority::Manual && it->queue.empty()) {
                    evaluateQueueGovernor(state, *it);
                }
            } else if (command.type == CommandType::ClearGovernorOverride) {
                it->workforceManualOverride = false;
                if (it->workforcePriority != GovernorPriority::Manual) {
                    evaluateWorkforceGovernor(state, *it);
                }
            } else if (command.type == CommandType::SetQueuePin) {
                require(static_cast<std::size_t>(command.queueIndex) < it->queue.size(), "Queue index out of bounds for pin");
                it->queue[command.queueIndex].pinned = command.queuePinned;
            }
            validateColony(*it);
            continue;
        }
        if (command.type == CommandType::AssignWorkers) {
            it->foodWorkers = command.foodWorkers;
            it->industryWorkers = command.industryWorkers;
            it->researchWorkers = command.researchWorkers;
            if (it->workforcePriority != GovernorPriority::Manual) {
                it->workforceManualOverride = true;
            }
        } else if (command.type == CommandType::ToggleShipyard) {
            require(it->shipyards == 1, "No shipyard in this colony");
            it->shipyardEnabled = !it->shipyardEnabled;
        } else if (command.type == CommandType::CancelFirstConstruction) {
            require(!it->queue.empty(), "Construction queue is empty");
            require(!it->queue.front().pinned, "Cannot cancel pinned construction project; unpin it first");
            it->queue.erase(it->queue.begin());
        } else if (command.type == CommandType::PrioritizeLastConstruction) {
            require(it->queue.size() > 1, "Queue needs at least two items");
            require(!it->queue.back().pinned, "Cannot prioritize pinned construction project; unpin it first");
            for (const auto& proj : it->queue) {
                require(!proj.pinned, "Cannot reprioritize queue containing pinned projects; unpin them first");
            }
            const auto last = it->queue.back(); it->queue.pop_back();
            it->queue.insert(it->queue.begin(), last);
        } else if (command.type == CommandType::RemoveConstruction || command.type == CommandType::MoveConstruction) {
            require(static_cast<std::size_t>(command.queueIndex) < it->queue.size(), "Project no longer in queue");
            require(!it->queue[command.queueIndex].pinned,
                command.type == CommandType::RemoveConstruction
                    ? "Cannot remove pinned construction project; unpin it first"
                    : "Cannot move pinned construction project; unpin it first");
            const auto project = it->queue[command.queueIndex];
            if (command.type == CommandType::MoveConstruction) {
                require(static_cast<std::size_t>(command.queueDestination) < it->queue.size(), "Queue destination unavailable");
                const auto minIdx = std::min(command.queueIndex, command.queueDestination);
                const auto maxIdx = std::max(command.queueIndex, command.queueDestination);
                for (auto i = static_cast<std::size_t>(minIdx); i <= static_cast<std::size_t>(maxIdx); ++i) {
                    require(!it->queue[i].pinned, "Cannot reorder construction queue past pinned projects; unpin them first");
                }
            }
            it->queue.erase(it->queue.begin() + command.queueIndex);
            if (command.type == CommandType::MoveConstruction)
                it->queue.insert(it->queue.begin() + command.queueDestination, project);
        } else {
            require(it->queue.size() < MaxQueue, "Construction queue full");
            const Construction project(command.building, 0, command.referenceId);
            require(command.referenceId < 0 || (state.ruleset == Ruleset::Reference &&
                playableShip(sourceShip(command.referenceId)) &&
                sourceShip(command.referenceId).raceId == catalogueRace(state.playerEmpire) &&
                roleProduct(sourceShip(command.referenceId).role) == command.building),
                "Reference ship does not belong to this empire or role");
            require(command.referenceId >= 0 || state.ruleset == Ruleset::Prototype || !isShip(command.building),
                "Choose a named reference ship class");
            require(command.building != Building::DefenseBattery || state.advancedRulesEnabled,
                "Defense batteries are only available in advanced reference campaigns");
            const auto blocked = constructionTechBlocker(state, project, state.playerEmpire);
            if (!blocked.empty()) throw std::runtime_error(blocked);
            it->queue.push_back(project);
        }
        validateColony(*it);
    }
}

EmpireState& mutableEmpire(GameState& s, int id) {
    if (id == s.playerEmpire) return s;
    const auto found = std::find_if(s.rivals.begin(), s.rivals.end(), [id](const EmpireState& e) { return e.id == id; });
    require(found != s.rivals.end(), "Empire does not exist"); return *found;
}
bool known(const GameState& s, int owner, int x, int y) {
    const auto& e = empire(s, owner);
    return x >= 0 && y >= 0 && x < s.width && y < s.height &&
        std::binary_search(e.explored.begin(), e.explored.end(), y * s.width + x);
}
void reveal(GameState& s, int x, int y, int owner = 1) {
    auto& knowledge = mutableEmpire(s, owner).explored;
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
        const int px = x + dx, py = y + dy;
        if (px < 0 || py < 0 || px >= s.width || py >= s.height) continue;
        const int cell = py * s.width + px;
        const auto it = std::lower_bound(knowledge.begin(), knowledge.end(), cell);
        if (it == knowledge.end() || *it != cell) knowledge.insert(it, cell);
    }
}
bool queueMilitaryShip(GameState& s, Colony& colony, Building role) {
    if (s.ruleset == Ruleset::Prototype) { colony.queue.push_back(role); return true; }
    const referenceData::ShipRecord* choice = nullptr;
    for (const auto& ship : referenceData::ShipCatalogue) {
        if (!playableShip(ship) || ship.raceId != catalogueRace(colony.ownerId) || roleProduct(ship.role) != role) continue;
        const Construction project(role, 0, ship.id);
        if (!constructionTechBlocker(s, project, colony.ownerId).empty()) continue;
        if (!choice || ship.industryCost < choice->industryCost) choice = &ship;
    }
    if (!choice) return false;
    colony.queue.emplace_back(role, 0, choice->id);
    return true;
}

void planRivals(GameState& s) {
    for (auto& owner : s.rivals) {
        // Personality- and situation-driven war decisions (treaties, cool-downs,
        // relative strength, trust and grievances); see planAiWarDeclarations.
        planAiWarDeclarations(s, owner.id);
        const bool atWar = std::any_of(s.majorRelations.begin(), s.majorRelations.end(), [&](const MajorRelation& r) {
            return r.atWar && (r.first == owner.id || r.second == owner.id);
        });
        const int focus = (s.turn / 20 + owner.id) % ResearchFields;
        for (int field = 0; field < ResearchFields; ++field)
            owner.technologies[field].allocation = atWar ? (field == 4 || field == 5 ? 40 : 5) : (field == focus ? 50 : 10);
        for (const auto& m : s.minors) {
            if (!m.contact[owner.id] || m.memberOf != 0 || std::any_of(s.diplomacy.begin(), s.diplomacy.end(), [&](const DiplomaticOffer& o) {
                return o.empireId == owner.id && o.systemId == m.systemId;
            })) continue;
            if (m.friendshipTurn[owner.id] < 0 && m.attitude[owner.id] >= 50)
                draftDiplomacy(s, owner.id, m.systemId, CommandType::OfferFriendship);
            else if (m.attitude[owner.id] < 80 && owner.credits >= 50)
                draftDiplomacy(s, owner.id, m.systemId, CommandType::SendGift);
            else if (m.attitude[owner.id] >= 80 && m.friendshipTurn[owner.id] >= 0 && m.friendshipTurn[owner.id] < s.turn)
                draftDiplomacy(s, owner.id, m.systemId, CommandType::OfferMembership);
        }
        int colonyShips = 0;
        for (const auto& f : s.fleets) if (f.ownerId == owner.id && f.colonyShip) ++colonyShips;
        for (const auto& c : s.colonies) if (c.ownerId == owner.id)
            for (auto b : c.queue) if (b == Building::ColonyShip) ++colonyShips;
        int destroyers = 0, transports = 0;
        for (const auto& f : s.fleets) if (f.ownerId == owner.id) {
            if (f.shipClass == Building::Destroyer) ++destroyers;
            if (f.shipClass == Building::Transport) ++transports;
        }
        for (const auto& c : s.colonies) if (c.ownerId == owner.id)
            for (auto b : c.queue) {
                if (b == Building::Destroyer) ++destroyers;
                if (b == Building::Transport) ++transports;
            }
        
        int uncolonizedTargets = 0;
        for (const auto& sys : s.systems) {
            if (known(s, owner.id, sys.x, sys.y) && !colonyAt(s, sys.id) && !minorAt(s, sys.id)) {
                ++uncolonizedTargets;
            }
        }
        const int desiredColonyShips = std::min(2, uncolonizedTargets);

        for (auto& c : s.colonies) {
            if (c.ownerId != owner.id) continue;
            c.foodWorkers = std::min(c.farms * 10, (c.population + 1) / 2);
            const int remaining = c.population - c.foodWorkers;
            c.researchWorkers = std::min(c.laboratories * 10, remaining / 2);
            c.industryWorkers = std::min(c.factories * 10, remaining - c.researchWorkers);
            if (!c.queue.empty()) continue;
            if (economy(s, c).foodDeficit > 0 && c.farms < MaxBuildings) c.queue.push_back(Building::Farm);
            else if (c.energy < c.factories * 5 + c.laboratories * 5 + 5 && c.powerPlants < MaxBuildings)
                c.queue.push_back(Building::PowerPlant);
            else if (s.campaignExpansionEnabled && c.factories < 2 && c.factories < MaxBuildings)
                c.queue.push_back(Building::Factory);
            else if (c.shipyards == 0) c.queue.push_back(Building::Shipyard);
            else if (atWar && s.advancedRulesEnabled && c.defenseBatteries < 1) c.queue.push_back(Building::DefenseBattery);
            else if (atWar && destroyers < 3 && queueMilitaryShip(s, c, Building::Destroyer)) ++destroyers;
            else if (atWar && transports < 1 && queueMilitaryShip(s, c, Building::Transport)) ++transports;
            else if (colonyShips < desiredColonyShips && s.fleets.size() < 256 && s.nextFleetId <= 1000000 &&
                queueMilitaryShip(s, c, Building::ColonyShip)) {
                ++colonyShips;
            } else if (c.laboratories == 0) c.queue.push_back(Building::Laboratory);
            else if (atWar && destroyers < 5 && queueMilitaryShip(s, c, Building::Destroyer)) ++destroyers;
        }

        auto evaluateSectorSafety = [&](int tx, int ty, bool isTransport, const Fleet* movingFleet = nullptr) -> bool {
            if (!visible(s, owner.id, tx, ty)) return true;
            int enemyPower = 0;
            int enemyCombatShips = 0;
            for (const auto& other : s.fleets) {
                if (other.hull > 0 && other.x == tx && other.y == ty) {
                    const auto enemyRel = majorRelation(s, owner.id, other.ownerId);
                    if (enemyRel && enemyRel->atWar) {
                        const int atk = fleetAttack(other);
                        if (atk > 0 && other.shipClass != Building::Transport) ++enemyCombatShips;
                        enemyPower += atk + empire(s, other.ownerId).technologies[5].level;
                    }
                }
            }
            if (isTransport) return enemyCombatShips == 0;
            int friendlyPower = (movingFleet ? fleetAttack(*movingFleet) : 0) + empire(s, owner.id).technologies[5].level;
            for (const auto& ally : s.fleets) {
                if ((!movingFleet || ally.id != movingFleet->id) && ally.ownerId == owner.id && ally.hull > 0 && ally.shipClass != Building::Transport) {
                    if ((ally.x == tx && ally.y == ty) || (movingFleet && ally.x == movingFleet->x && ally.y == movingFleet->y)) {
                        friendlyPower += fleetAttack(ally) + empire(s, owner.id).technologies[5].level;
                    }
                }
            }
            return friendlyPower * 2 >= enemyPower;
        };

        // Form strategic task forces for campaign expansion
        if (s.campaignExpansionEnabled && atWar) {
            std::map<std::pair<int, int>, std::vector<int>> availableBySector;
            for (const auto& f : s.fleets) {
                if (f.ownerId == owner.id && f.hull > 0 && f.taskForceId == 0 && !f.colonyShip && !f.retreat) {
                    availableBySector[{f.x, f.y}].push_back(f.id);
                }
            }
            for (const auto& [loc, fids] : availableBySector) {
                if (fids.size() >= 2 && s.taskForces.size() < MaxTaskForces && s.nextTaskForceId < 10000000) {
                    bool hasCombat = false, hasTransport = false;
                    for (int fid : fids) {
                        const auto f = std::find_if(s.fleets.begin(), s.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
                        if (f != s.fleets.end()) {
                            if (f->shipClass == Building::Destroyer || fleetAttack(*f) > 0) hasCombat = true;
                            if (f->shipClass == Building::Transport) hasTransport = true;
                        }
                    }
                    if (hasCombat || hasTransport) {
                        TaskForce tf;
                        tf.id = s.nextTaskForceId++;
                        tf.ownerId = owner.id;
                        tf.name = "Assault Force " + std::to_string(tf.id);
                        tf.fleetIds = fids;
                        tf.targetX = loc.first; tf.targetY = loc.second;
                        tf.stance = TacticalStance::Balanced;
                        for (int fid : fids) {
                            auto f = std::find_if(s.fleets.begin(), s.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
                            f->taskForceId = tf.id;
                        }
                        s.taskForces.push_back(tf);
                    }
                }
            }
        }

        // Plan Task Forces as unified groups
        for (auto& tf : s.taskForces) {
            if (tf.ownerId != owner.id || tf.fleetIds.empty()) continue;
            auto leaderIt = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const Fleet& f) { return f.id == tf.fleetIds.front(); });
            if (leaderIt == s.fleets.end()) continue;

            const Colony* targetColony = nullptr;
            int bestDist = std::numeric_limits<int>::max();
            for (const auto& candidate : s.colonies) {
                const auto rel = majorRelation(s, owner.id, candidate.ownerId);
                if (!rel || !rel->atWar) continue;
                const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == candidate.systemId; });
                if (loc == s.systems.end() || !known(s, owner.id, loc->x, loc->y)) continue;
                if (!evaluateSectorSafety(loc->x, loc->y, false, &*leaderIt)) continue;

                if (loc->x != leaderIt->x || loc->y != leaderIt->y) {
                    int minTfRange = 999;
                    for (int fid : tf.fleetIds) {
                        auto m = std::find_if(s.fleets.begin(), s.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
                        if (m != s.fleets.end()) minTfRange = std::min(minTfRange, shipRange(*m));
                    }
                    auto tfSupplied = [&](int sx, int sy) {
                        if (internal::colonySuppliedWithRange(s, owner.id, minTfRange, sx, sy)) return true;
                        return gameplay::layerActive(s) && gameplay::extendedSupply(s, owner.id, sx, sy, [&](int px, int py) {
                            return internal::colonySuppliedWithRange(s, owner.id, minTfRange, px, py);
                        });
                    };
                    if (!tfSupplied(loc->x, loc->y)) continue;
                }

                bool canReach = true;
                for (int fid : tf.fleetIds) {
                    auto m = std::find_if(s.fleets.begin(), s.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
                    if (m == s.fleets.end()) { canReach = false; break; }
                    if (loc->x != leaderIt->x || loc->y != leaderIt->y) {
                        if (route(s, *m, loc->x, loc->y).empty()) { canReach = false; break; }
                    }
                }
                if (!canReach) continue;

                const int dist = std::max(std::abs(leaderIt->x - loc->x), std::abs(leaderIt->y - loc->y));
                if (dist < bestDist) {
                    bestDist = dist;
                    targetColony = &candidate;
                }
            }
            if (targetColony) {
                const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == targetColony->systemId; });
                tf.targetX = loc->x; tf.targetY = loc->y;
                for (int fid : tf.fleetIds) {
                    auto m = std::find_if(s.fleets.begin(), s.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
                    if (m != s.fleets.end()) {
                        m->targetX = loc->x; m->targetY = loc->y;
                        if (m->shipClass == Building::Transport && m->x == loc->x && m->y == loc->y) {
                            const int defBat = s.advancedRulesEnabled ? economy(s, *targetColony).poweredBatteries : 0;
                            const int defStr = 8 + targetColony->population / 3 + targetColony->morale / 10 + defBat * 12;
                            const int atkStr = 8 + m->hull + 2 * empire(s, owner.id).technologies[5].level;
                            if (atkStr * 10 >= defStr * 7) {
                                m->invadeSystemId = targetColony->systemId;
                            }
                        }
                    }
                }
            }
        }

        std::set<int> reserved;
        for (auto& f : s.fleets) {
            if (f.ownerId != owner.id || f.colonize || f.taskForceId > 0) continue;

            if (s.advancedRulesEnabled) {
                if (f.retreat) {
                    if (f.x != f.targetX || f.y != f.targetY) continue;
                    f.retreat = false;
                }
                bool atSafePoweredYard = false;
                for (const auto& c : s.colonies) {
                    if (c.ownerId != owner.id) continue;
                    const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& value) { return value.id == c.systemId; });
                    if (loc != s.systems.end() && loc->x == f.x && loc->y == f.y && economy(s, c).shipyardPowered) {
                        bool hostilePresent = false;
                        for (const auto& other : s.fleets) {
                            if (other.hull > 0 && other.x == f.x && other.y == f.y) {
                                const auto rel = majorRelation(s, owner.id, other.ownerId);
                                if (rel && rel->atWar) { hostilePresent = true; break; }
                            }
                        }
                        if (!hostilePresent) { atSafePoweredYard = true; break; }
                    }
                }
                const int maxH = fleetMaxHull(f);
                const int meaningfulThreshold = (maxH * 3 + 3) / 4;
                if (atSafePoweredYard && f.hull < meaningfulThreshold) {
                    f.targetX = f.x; f.targetY = f.y;
                    f.attackTarget = 0; f.retreat = false;
                    continue;
                }
                if (f.hull < meaningfulThreshold) {
                    const System* home = nullptr; int shortest = std::numeric_limits<int>::max();
                    bool homePoweredYard = false;
                    for (const auto& c : s.colonies) if (c.ownerId == owner.id) {
                        const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& value) { return value.id == c.systemId; });
                        const int distance = std::max(std::abs(f.x - loc->x), std::abs(f.y - loc->y));
                        if (distance <= 0) continue;
                        const bool poweredYard = economy(s, c).shipyardPowered;
                        if ((poweredYard && !homePoweredYard) || (poweredYard == homePoweredYard && distance < shortest)) {
                            shortest = distance; home = &*loc; homePoweredYard = poweredYard;
                        }
                    }
                    if (home) {
                        f.targetX = home->x; f.targetY = home->y;
                        f.retreat = true; f.attackTarget = 0;
                        continue;
                    }
                    f.targetX = f.x; f.targetY = f.y;
                    f.attackTarget = 0;
                    continue;
                }
                if (atWar && !f.colonyShip) {
                    if (f.shipClass == Building::Transport) {
                        f.attackTarget = 0;
                        const Colony* atHostileColony = nullptr;
                        for (const auto& candidate : s.colonies) {
                            const auto relation = majorRelation(s, owner.id, candidate.ownerId);
                            if (!relation || !relation->atWar) continue;
                            const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == candidate.systemId; });
                            if (loc != s.systems.end() && loc->x == f.x && loc->y == f.y) {
                                atHostileColony = &candidate; break;
                            }
                        }
                        if (atHostileColony) {
                            if (evaluateSectorSafety(f.x, f.y, true, &f)) {
                                const int defBat = s.advancedRulesEnabled ? economy(s, *atHostileColony).poweredBatteries : 0;
                                const int defStr = 8 + atHostileColony->population / 3 + atHostileColony->morale / 10 + defBat * 12;
                                const int atkStr = 8 + f.hull + 2 * empire(s, owner.id).technologies[5].level;
                                if (atkStr * 10 >= defStr * 7) {
                                    f.invadeSystemId = atHostileColony->systemId;
                                    continue;
                                }
                            }
                            f.targetX = f.x; f.targetY = f.y;
                            continue;
                        }
                        const Colony* enemyColony = nullptr; int shortest = std::numeric_limits<int>::max();
                        for (const auto& candidate : s.colonies) {
                            const auto relation = majorRelation(s, owner.id, candidate.ownerId);
                            if (!relation || !relation->atWar) continue;
                            const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == candidate.systemId; });
                            if (loc == s.systems.end() || !known(s, owner.id, loc->x, loc->y)) continue;
                            if (!evaluateSectorSafety(loc->x, loc->y, true, &f)) continue;
                            const auto path = route(s, f, loc->x, loc->y);
                            if (!path.empty() && path.size() > 1 && !evaluateSectorSafety(path[1] % s.width, path[1] / s.width, true, &f)) continue;
                            if (!path.empty() && (static_cast<int>(path.size()) < shortest ||
                                (static_cast<int>(path.size()) == shortest && (!enemyColony || candidate.systemId < enemyColony->systemId)))) {
                                shortest = static_cast<int>(path.size()); enemyColony = &candidate;
                            }
                        }
                        if (enemyColony) {
                            const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == enemyColony->systemId; });
                            f.targetX = loc->x; f.targetY = loc->y;
                            continue;
                        }
                        f.attackTarget = 0; f.targetX = f.x; f.targetY = f.y;
                        continue;
                    } else {
                        const Fleet* enemy = nullptr; int shortest = std::numeric_limits<int>::max();
                        for (const auto& candidate : s.fleets) {
                            const auto relation = majorRelation(s, owner.id, candidate.ownerId);
                            if (!relation || !relation->atWar || !visible(s, owner.id, candidate.x, candidate.y)) continue;
                            if (!evaluateSectorSafety(candidate.x, candidate.y, false, &f)) continue;
                            const auto path = route(s, f, candidate.x, candidate.y);
                            if (!path.empty() && path.size() > 1 && !evaluateSectorSafety(path[1] % s.width, path[1] / s.width, false, &f)) continue;
                            if (!path.empty() && (static_cast<int>(path.size()) < shortest ||
                                (static_cast<int>(path.size()) == shortest && (!enemy || candidate.id < enemy->id)))) {
                                shortest = static_cast<int>(path.size()); enemy = &candidate;
                            }
                        }
                        if (enemy && fleetAttack(f) > 0) {
                            f.attackTarget = enemy->id; f.targetX = enemy->x; f.targetY = enemy->y; continue;
                        }
                        const Colony* enemyColony = nullptr; shortest = std::numeric_limits<int>::max();
                        for (const auto& candidate : s.colonies) {
                            const auto relation = majorRelation(s, owner.id, candidate.ownerId);
                            if (!relation || !relation->atWar) continue;
                            const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == candidate.systemId; });
                            if (loc == s.systems.end() || !known(s, owner.id, loc->x, loc->y)) continue;
                            if (!evaluateSectorSafety(loc->x, loc->y, false, &f)) continue;
                            const auto path = route(s, f, loc->x, loc->y);
                            if (!path.empty() && path.size() > 1 && !evaluateSectorSafety(path[1] % s.width, path[1] / s.width, false, &f)) continue;
                            if (!path.empty() && (static_cast<int>(path.size()) < shortest ||
                                (static_cast<int>(path.size()) == shortest && (!enemyColony || candidate.systemId < enemyColony->systemId)))) {
                                shortest = static_cast<int>(path.size()); enemyColony = &candidate;
                            }
                        }
                        if (enemyColony) {
                            const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == enemyColony->systemId; });
                            f.targetX = loc->x; f.targetY = loc->y;
                            continue;
                        }
                        f.attackTarget = 0; f.targetX = f.x; f.targetY = f.y;
                        continue;
                    }
                }
            } else {
                if (atWar && !f.colonyShip) {
                    const Fleet* enemy = nullptr; int shortest = std::numeric_limits<int>::max();
                    for (const auto& candidate : s.fleets) {
                        const auto relation = majorRelation(s, owner.id, candidate.ownerId);
                        if (!relation || !relation->atWar || !visible(s, owner.id, candidate.x, candidate.y)) continue;
                        const auto path = route(s, f, candidate.x, candidate.y);
                        if (!path.empty() && static_cast<int>(path.size()) < shortest) { shortest = static_cast<int>(path.size()); enemy = &candidate; }
                    }
                    if (enemy && fleetAttack(f) > 0) {
                        f.attackTarget = enemy->id; f.targetX = enemy->x; f.targetY = enemy->y; continue;
                    }
                    const Colony* enemyColony = nullptr; shortest = std::numeric_limits<int>::max();
                    for (const auto& candidate : s.colonies) {
                        const auto relation = majorRelation(s, owner.id, candidate.ownerId);
                        if (!relation || !relation->atWar) continue;
                        const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == candidate.systemId; });
                        if (!known(s, owner.id, loc->x, loc->y)) continue;
                        const auto path = route(s, f, loc->x, loc->y);
                        if (!path.empty() && static_cast<int>(path.size()) < shortest) { shortest = static_cast<int>(path.size()); enemyColony = &candidate; }
                    }
                    if (enemyColony) {
                        const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == enemyColony->systemId; });
                        f.targetX = loc->x; f.targetY = loc->y;
                        if (f.shipClass == Building::Transport && f.x == loc->x && f.y == loc->y) {
                            const int defBat = s.advancedRulesEnabled ? economy(s, *enemyColony).poweredBatteries : 0;
                            const int defStr = 8 + enemyColony->population / 3 + enemyColony->morale / 10 + defBat * 12;
                            const int atkStr = 8 + f.hull + 2 * empire(s, owner.id).technologies[5].level;
                            if (atkStr * 10 >= defStr * 7) {
                                f.invadeSystemId = enemyColony->systemId;
                            }
                        }
                        continue;
                    }
                }
            }
            if (f.colonyShip) {
                const System* target = nullptr; int shortest = std::numeric_limits<int>::max();
                for (const auto& system : s.systems) {
                    if (!known(s, owner.id, system.x, system.y) || colonyAt(s, system.id) || minorAt(s, system.id) || reserved.count(system.id)) continue;
                    if (s.advancedRulesEnabled && !evaluateSectorSafety(system.x, system.y, true, &f)) continue;
                    if (std::any_of(s.fleets.begin(), s.fleets.end(), [&](const Fleet& other) {
                        return other.colonize && other.x == system.x && other.y == system.y;
                    })) continue;
                    const auto path = route(s, f, system.x, system.y);
                    if (!path.empty() && path.size() > 1 && s.advancedRulesEnabled &&
                        !evaluateSectorSafety(path[1] % s.width, path[1] / s.width, true, &f)) continue;
                    if (!path.empty() && static_cast<int>(path.size()) < shortest) { shortest = static_cast<int>(path.size()); target = &system; }
                }
                if (target) {
                    reserved.insert(target->id); f.targetX = target->x; f.targetY = target->y;
                    if (f.x == target->x && f.y == target->y) {
                        f.colonize = true;
                        f.terraforming = false;
                    }
                    continue;
                }
            }
            if ((f.x != f.targetX || f.y != f.targetY) && !route(s, f, f.targetX, f.targetY).empty()) {
                const auto existingPath = route(s, f, f.targetX, f.targetY);
                const bool isCiv = f.shipClass == Building::Transport || f.colonyShip;
                if (!s.advancedRulesEnabled ||
                    (evaluateSectorSafety(f.targetX, f.targetY, isCiv, &f) &&
                     (existingPath.size() <= 1 || evaluateSectorSafety(existingPath[1] % s.width, existingPath[1] / s.width, isCiv, &f)))) {
                    continue;
                }
                f.targetX = f.x; f.targetY = f.y; f.attackTarget = 0;
            }
            struct Candidate { int cell, score; };
            std::vector<Candidate> candidates;
            for (int y = 0; y < s.height; ++y) for (int x = 0; x < s.width; ++x) {
                if (!supplied(s, f, x, y)) continue;
                int unknown = 0;
                for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
                    if (x + dx >= 0 && y + dy >= 0 && x + dx < s.width && y + dy < s.height &&
                        !known(s, owner.id, x + dx, y + dy)) ++unknown;
                if (unknown) candidates.push_back({y * s.width + x, unknown * 100 - std::max(std::abs(f.x - x), std::abs(f.y - y))});
            }
            std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
                return a.score == b.score ? a.cell < b.cell : a.score > b.score;
            });
            f.targetX = f.x; f.targetY = f.y;
            for (const auto& candidate : candidates) {
                const int x = candidate.cell % s.width, y = candidate.cell / s.width;
                const bool isCiv = f.shipClass == Building::Transport || f.colonyShip;
                if (s.advancedRulesEnabled && !evaluateSectorSafety(x, y, isCiv, &f)) continue;
                const auto path = route(s, f, x, y);
                if (!path.empty()) {
                    if (s.advancedRulesEnabled && path.size() > 1 &&
                        !evaluateSectorSafety(path[1] % s.width, path[1] / s.width, isCiv, &f)) continue;
                    f.targetX = x; f.targetY = y; break;
                }
            }
        }
    }
}

void resolveDiplomacy(GameState& s, TurnReport& report) {
    for (auto& m : s.minors) {
        const auto system = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == m.systemId; });
        for (int owner = 1; owner <= 5; ++owner) {
            if (owner != s.playerEmpire && std::none_of(s.rivals.begin(), s.rivals.end(), [owner](const EmpireState& e) { return e.id == owner; })) continue;
            if (!m.contact[owner] && visible(s, owner, system->x, system->y)) {
                m.contact[owner] = true;
                if (owner == s.playerEmpire) report.events.push_back("First contact with " + m.name + ". Diplomatic channels opened.");
            }
        }
        auto notify = [&](const DiplomaticOffer& offer, const std::string& message) {
            if (offer.empireId == s.playerEmpire) report.events.push_back(m.name + ": " + message);
        };
        for (const auto& offer : s.diplomacy) {
            if (offer.systemId != m.systemId || offer.dueTurn > s.turn) continue;
            const int owner = offer.empireId;
            if (m.memberOf != 0) {
                if (offer.type == CommandType::SendGift) mutableEmpire(s, owner).credits += 50;
                notify(offer, "offer withdrawn because the minor has joined an empire. Any undelivered gift is refunded.");
            } else if (offer.type == CommandType::SendGift) {
                m.attitude[owner] = std::min(100, m.attitude[owner] + 20);
                notify(offer, "gift delivered. Relations improved by 20.");
            } else if (offer.type == CommandType::OfferFriendship) {
                if (m.attitude[owner] >= 50 && m.contact[owner]) {
                    if (m.friendshipTurn[owner] < 0) m.friendshipTurn[owner] = s.turn;
                    notify(offer, "friendship accepted.");
                } else notify(offer, "friendship refused: relations must be at least 50.");
            }
        }
        if (m.memberOf != 0) continue;
        std::vector<const DiplomaticOffer*> eligible;
        int best = -1;
        for (const auto& offer : s.diplomacy) {
            if (offer.systemId != m.systemId || offer.dueTurn > s.turn || offer.type != CommandType::OfferMembership) continue;
            const int owner = offer.empireId;
            if (!m.contact[owner] || m.attitude[owner] < 80 || m.friendshipTurn[owner] < 0 || m.friendshipTurn[owner] >= s.turn) {
                notify(offer, "membership refused: relations 80 and friendship from a prior turn are required."); continue;
            }
            if (m.attitude[owner] > best) { best = m.attitude[owner]; eligible.clear(); }
            if (m.attitude[owner] == best) eligible.push_back(&offer);
        }
        if (eligible.empty()) continue;
        const auto winner = eligible[eligible.size() == 1 ? 0 : bounded(s.rngState, eligible.size())];
        m.memberOf = winner->empireId;
        Colony colony; colony.systemId = m.systemId; colony.ownerId = m.memberOf;
        colony.population = 30; colony.foodWorkers = 15; colony.industryWorkers = 10; colony.researchWorkers = 5;
        minors::configureJoinedColony(m, colony); // Lane G
        s.colonies.push_back(colony); reveal(s, system->x, system->y, m.memberOf);
        for (const auto& offer : s.diplomacy)
            if (offer.systemId == m.systemId && offer.dueTurn <= s.turn && offer.type == CommandType::OfferMembership
                && m.contact[offer.empireId] && m.attitude[offer.empireId] >= 80
                && m.friendshipTurn[offer.empireId] >= 0 && m.friendshipTurn[offer.empireId] < s.turn)
                notify(offer, offer.empireId == m.memberOf ? "membership accepted. Colony and +10% empire research benefit activate next turn."
                    : "membership declined in favour of " + std::string(empireName(m.memberOf)) + ".");
        if (m.memberOf != s.playerEmpire && m.contact[s.playerEmpire])
            report.events.push_back(m.name + " joined " + empireName(m.memberOf) + ".");
    }
    std::erase_if(s.diplomacy, [&](const DiplomaticOffer& offer) { return offer.dueTurn <= s.turn; });
    std::sort(s.colonies.begin(), s.colonies.end(), [](const Colony& a, const Colony& b) { return a.systemId < b.systemId; });
}

// ---------------------------------------------------------------------------
// Lane B: shared war/peace/operation application for legacy and modern paths
// ---------------------------------------------------------------------------
void applyWarRelation(GameState& s, int a, int b) {
    auto& relation = mutableRelation(s, a, b);
    if (relation.atWar) return;
    relation.atWar = true; relation.tradeTreaty = false;
    // warStartTurn records the last war/peace transition; AI cool-downs read it.
    relation.warStartTurn = s.turn;
    if (s.advancedRulesEnabled) relation.trust = std::max(0, relation.trust - 25);
    std::erase_if(s.tradeRoutes, [&](const TradeRoute& route) {
        const auto destination = colonyAt(s, route.toSystemId);
        return destination && ((route.ownerId == a && destination->ownerId == b) ||
            (route.ownerId == b && destination->ownerId == a));
    });
    std::erase_if(s.pendingMajorOffers, [&](const MajorOffer& offer) {
        return (offer.proposer == a && offer.target == b) || (offer.proposer == b && offer.target == a);
    });
}

void applyPeaceRelation(GameState& s, int a, int b) {
    auto& relation = mutableRelation(s, a, b);
    if (!relation.atWar) return;
    relation.atWar = false;
    relation.warStartTurn = s.turn;
}

std::string participantName(const GameState& s, diplomacy::ParticipantId p) {
    if (!p.isMinor) return empireName(p.id);
    const auto* m = minorAt(s, p.id);
    return m ? m->name : std::string("an unknown minor race");
}

void creditEmpire(GameState& s, int id, std::int64_t amount) {
    auto& e = mutableEmpire(s, id);
    e.credits = std::clamp<std::int64_t>(e.credits + amount, -MaxBalance, MaxBalance);
}

void applyDiplomacyOps(GameState& next, const diplomacy::TurnResolutionDelta& delta, TurnReport* report, bool skipWarState) {
    for (const auto& op : delta.operations) {
        if (op.opType == diplomacy::DiplomaticOpType::TransferCredits) {
            // The engine ledger guarantees the payer could afford it; the payee is clamped to MaxBalance.
            const std::int64_t amount = std::clamp<std::int64_t>(op.credits, 0, MaxBalance);
            if (!op.partyA.isMinor && campaignEmpire(next, op.partyA.id)) creditEmpire(next, op.partyA.id, -amount);
            if (!op.partyB.isMinor && campaignEmpire(next, op.partyB.id)) creditEmpire(next, op.partyB.id, amount);
        } else if (op.opType == diplomacy::DiplomaticOpType::ApplyTreaty) {
            if (!op.partyA.isMinor && !op.partyB.isMinor) {
                auto& rel = mutableRelation(next, op.partyA.id, op.partyB.id);
                if (op.treatyType == diplomacy::TreatyType::Trade ||
                    op.treatyType == diplomacy::TreatyType::Affiliation ||
                    op.treatyType == diplomacy::TreatyType::Alliance) {
                    rel.tradeTreaty = true;
                    rel.atWar = false;
                } else if (op.treatyType == diplomacy::TreatyType::NonAggression ||
                           op.treatyType == diplomacy::TreatyType::Peace) {
                    rel.atWar = false;
                }
            } else if (op.treatyType == diplomacy::TreatyType::Friendship ||
                       op.treatyType == diplomacy::TreatyType::Affiliation) {
                // Friendship-class treaties with a minor open the road to membership.
                int majorId = op.partyA.isMinor ? op.partyB.id : op.partyA.id;
                int minorId = op.partyA.isMinor ? op.partyA.id : op.partyB.id;
                auto mIt = std::find_if(next.minors.begin(), next.minors.end(), [minorId](const Minor& m) { return m.systemId == minorId; });
                if (mIt != next.minors.end() && majorId >= 1 && majorId <= 5 && mIt->contact[majorId]) {
                    mIt->friendshipTurn[majorId] = next.turn;
                }
            }
        } else if (op.opType == diplomacy::DiplomaticOpType::TerminateTreaty) {
            if (!op.partyA.isMinor && !op.partyB.isMinor) {
                auto& rel = mutableRelation(next, op.partyA.id, op.partyB.id);
                if (op.treatyType == diplomacy::TreatyType::Trade ||
                    op.treatyType == diplomacy::TreatyType::Affiliation ||
                    op.treatyType == diplomacy::TreatyType::Alliance) {
                    bool stillHasTrade = false;
                    for (const auto& t : next.diplomacyEngineState.activeTreaties) {
                        if (t.status == diplomacy::TreatyStatus::Active &&
                            t.involvesBoth(op.partyA, op.partyB) &&
                            (t.type == diplomacy::TreatyType::Trade ||
                             t.type == diplomacy::TreatyType::Affiliation ||
                             t.type == diplomacy::TreatyType::Alliance)) {
                            stillHasTrade = true;
                            break;
                        }
                    }
                    if (!stillHasTrade) {
                        rel.tradeTreaty = false;
                        // Routes need a treaty (advanced rules); drop the orphans between the pair.
                        if (next.advancedRulesEnabled) {
                            std::erase_if(next.tradeRoutes, [&](const TradeRoute& route) {
                                const auto dest = colonyAt(next, route.toSystemId);
                                return dest && ((route.ownerId == op.partyA.id && dest->ownerId == op.partyB.id) ||
                                                (route.ownerId == op.partyB.id && dest->ownerId == op.partyA.id));
                            });
                        }
                    }
                }
            } else if (op.treatyType == diplomacy::TreatyType::Friendship ||
                       op.treatyType == diplomacy::TreatyType::Affiliation) {
                int majorId = op.partyA.isMinor ? op.partyB.id : op.partyA.id;
                int minorId = op.partyA.isMinor ? op.partyA.id : op.partyB.id;
                const diplomacy::ParticipantId minorP = diplomacy::ParticipantId::minor(minorId);
                const diplomacy::ParticipantId majorP = diplomacy::ParticipantId::major(majorId);
                const bool stillFriends = next.diplomacyEngineState.hasActiveTreaty(majorP, minorP, diplomacy::TreatyType::Friendship) ||
                                          next.diplomacyEngineState.hasActiveTreaty(majorP, minorP, diplomacy::TreatyType::Affiliation);
                auto mIt = std::find_if(next.minors.begin(), next.minors.end(), [minorId](const Minor& m) { return m.systemId == minorId; });
                if (mIt != next.minors.end() && majorId >= 1 && majorId <= 5 && !stillFriends) {
                    mIt->friendshipTurn[majorId] = -1;
                }
            }
        } else if (op.opType == diplomacy::DiplomaticOpType::SetWarState) {
            if (skipWarState || op.partyA.isMinor || op.partyB.isMinor) continue;
            if (op.setAtWar) applyWarRelation(next, op.partyA.id, op.partyB.id);
            else applyPeaceRelation(next, op.partyA.id, op.partyB.id);
        } else if (op.opType == diplomacy::DiplomaticOpType::TransferMinorMembership) {
            int minorId = op.partyA.id;
            int newOwner = op.newOwner;
            auto mIt = std::find_if(next.minors.begin(), next.minors.end(), [minorId](const Minor& m) { return m.systemId == minorId; });
            if (mIt != next.minors.end()) {
                mIt->memberOf = newOwner;
                if (!colonyAt(next, minorId)) {
                    Colony colony;
                    colony.systemId = minorId;
                    colony.ownerId = newOwner;
                    colony.population = 30;
                    colony.foodWorkers = 15;
                    colony.industryWorkers = 10;
                    colony.researchWorkers = 5;
                    minors::configureJoinedColony(*mIt, colony); // Lane G
                    next.colonies.push_back(colony);
                    const auto sys = std::find_if(next.systems.begin(), next.systems.end(), [minorId](const System& s) { return s.id == minorId; });
                    if (sys != next.systems.end()) {
                        reveal(next, sys->x, sys->y, newOwner);
                    }
                    std::sort(next.colonies.begin(), next.colonies.end(), [](const Colony& a, const Colony& b) { return a.systemId < b.systemId; });
                }
                if (report && mIt->contact[next.playerEmpire]) {
                    if (newOwner == next.playerEmpire) {
                        report->events.push_back(mIt->name + " accepted membership! Colony established.");
                    } else {
                        report->events.push_back(mIt->name + " joined " + std::string(empireName(newOwner)) + ".");
                    }
                }
            }
        } else if (op.opType == diplomacy::DiplomaticOpType::UpdateMinorAttitude) {
            int minorId = op.partyA.id;
            int majorId = op.partyB.id;
            auto mIt = std::find_if(next.minors.begin(), next.minors.end(), [minorId](const Minor& m) { return m.systemId == minorId; });
            if (mIt != next.minors.end() && majorId >= 1 && majorId <= 5) {
                mIt->attitude[majorId] = std::clamp(mIt->attitude[majorId] + op.attitudeDelta, 0, 100);
            }
        } else if (op.opType == diplomacy::DiplomaticOpType::AdjustTrust) {
            if (op.partyA.isMinor || op.partyB.isMinor || op.partyA == op.partyB) continue;
            if (!campaignEmpire(next, op.partyA.id) || !campaignEmpire(next, op.partyB.id)) continue;
            auto& rel = mutableRelation(next, op.partyA.id, op.partyB.id);
            rel.trust = std::clamp(rel.trust + op.attitudeDelta, 0, 100);
        }
    }
}

// Unified war declaration for the player's DeclareWar order, AI war planning and
// AI allies answering a call to arms. Legacy campaigns only change the relation.
void enactWar(GameState& s, int aggressor, int defender) {
    if (mutableRelation(s, aggressor, defender).atWar) return;
    applyWarRelation(s, aggressor, defender);
    if (!modernDiplomacy(s)) return;
    auto& ds = s.diplomacyEngineState;
    ds.currentTurn = s.turn;
    CampaignDiplomacyContext ctx(s);
    diplomacy::TurnResolutionDelta delta;
    diplomacy::DiplomacyEngine::declareWar(ds, diplomacy::ParticipantId::major(aggressor),
        diplomacy::ParticipantId::major(defender), delta, &ctx);
    applyDiplomacyOps(s, delta, nullptr, /*skipWarState=*/true);
}

std::int64_t blocMilitaryPower(const GameState& s, const CampaignDiplomacyContext& ctx, int empireId, int excluding) {
    const auto self = diplomacy::ParticipantId::major(empireId);
    std::int64_t power = ctx.militaryPower(self);
    if (!modernDiplomacy(s)) return power;
    for (const auto& t : s.diplomacyEngineState.activeTreaties) {
        if (t.status != diplomacy::TreatyStatus::Active || t.type != diplomacy::TreatyType::Alliance || !t.involves(self)) continue;
        const auto ally = t.getCounterparty(self);
        if (ally.isMinor || ally.id == excluding || ctx.isEliminated(ally)) continue;
        power += ctx.militaryPower(ally);
    }
    return power;
}

// Replaces the old "IDs 2/4 declare war on the player every turn after 25" rule.
void planAiWarDeclarations(GameState& s, int ownerId) {
    if (!s.aiAggressionEnabled || empireEliminatedInternal(s, ownerId)) return;
    const bool modern = modernDiplomacy(s);
    const auto& ds = s.diplomacyEngineState;
    int activeWars = 0;
    std::vector<int> candidates;
    for (const auto& r : s.majorRelations) {
        const int other = r.first == ownerId ? r.second : r.second == ownerId ? r.first : 0;
        if (!other || empireEliminatedInternal(s, other)) continue;
        if (r.atWar) ++activeWars;
        else if (r.contact) candidates.push_back(other);
    }
    std::sort(candidates.begin(), candidates.end());
    int best = 0, bestScore = 0;
    for (int other : candidates) {
        const CampaignDiplomacyContext ctx(s);
        const auto* rel = majorRelation(s, ownerId, other);
        if (!rel) continue;
        diplomacy::WarAssessmentInput in;
        in.aggressorId = ownerId;
        in.targetId = other;
        in.personality = diplomacy::personalityForEmpire(ownerId);
        in.currentTurn = s.turn;
        int lastTransition = rel->warStartTurn;
        if (modern) {
            if (const auto* mem = ds.findMemory(ownerId, other)) {
                lastTransition = std::max({lastTransition, mem->lastWarTurn, mem->lastPeaceTurn});
            }
            const auto a = diplomacy::ParticipantId::major(ownerId), b = diplomacy::ParticipantId::major(other);
            in.treatyRank = ds.highestTreatyRank(a, b);
            in.peaceTreaty = ds.hasActiveTreaty(a, b, diplomacy::TreatyType::Peace);
            in.grievances = ds.grievances(ownerId, other);
            in.targetBetrayals = ds.betrayals(other);
        }
        in.turnsSinceTransition = s.turn - lastTransition;
        in.activeWars = activeWars;
        in.aggressorPower = ctx.militaryPower(diplomacy::ParticipantId::major(ownerId));
        in.targetPower = blocMilitaryPower(s, ctx, other, ownerId);
        in.trust = rel->trust;
        in.tradePact = rel->tradeTreaty;
        in.seed = s.seed ^ (0x9E3779B97F4A7C15ULL * static_cast<std::uint64_t>(ownerId));
        const auto verdict = diplomacy::assessWarDeclaration(in);
        if (verdict.declare && (best == 0 || verdict.score > bestScore)) {
            best = other;
            bestScore = verdict.score;
        }
    }
    if (best) enactWar(s, ownerId, best); // At most one new front per empire per turn.
}

void resolveMajorOffers(GameState& s, TurnReport& report) {
    for (const auto& offer : s.pendingMajorOffers) {
        if (offer.dueTurn > s.turn) continue;
        auto& relation = mutableRelation(s, offer.proposer, offer.target);
        const bool eliminated = empireEliminatedInternal(s, offer.proposer) || empireEliminatedInternal(s, offer.target);
        bool accepted = false;
        if (eliminated) {
            accepted = false;
        } else if (offer.type == CommandType::OfferPeace && relation.atWar) {
            int proposerShips = 0, targetShips = 0;
            for (const auto& fleet : s.fleets) {
                if (fleet.ownerId == offer.proposer) proposerShips += fleetAttack(fleet);
                if (fleet.ownerId == offer.target) targetShips += fleetAttack(fleet);
            }
            accepted = relation.trust >= 20 && (s.turn - relation.warStartTurn >= 2 || proposerShips >= targetShips);
            if (accepted) {
                relation.atWar = false;
                relation.warStartTurn = s.turn; // Peace transition: starts the AI war cool-down
                relation.trust = std::min(100, relation.trust + 10);
            }
        } else if (offer.type == CommandType::ProposeTradeTreaty && !relation.atWar && !relation.tradeTreaty) {
            accepted = relation.trust >= 40;
            if (accepted) relation.tradeTreaty = true;
        }
        if (accepted && modernDiplomacy(s)) {
            // Routed through the engine so the treaty cap, supersession and events apply.
            auto& ds = s.diplomacyEngineState;
            ds.currentTurn = s.turn;
            diplomacy::TurnResolutionDelta delta;
            const auto a = diplomacy::ParticipantId::major(offer.proposer);
            const auto b = diplomacy::ParticipantId::major(offer.target);
            if (offer.type == CommandType::OfferPeace) diplomacy::DiplomacyEngine::recordPeace(ds, a, b, delta);
            diplomacy::DiplomacyEngine::establishTreaty(ds, a, b,
                offer.type == CommandType::OfferPeace ? diplomacy::TreatyType::Peace : diplomacy::TreatyType::Trade,
                diplomacy::TreatyDuration::Turns25, delta);
        }
        if (offer.proposer == s.playerEmpire || offer.target == s.playerEmpire)
            report.events.push_back(std::string(empireName(offer.target)) + (accepted ? " accepted " : " declined ") +
                (offer.type == CommandType::OfferPeace ? "peace terms." : "the trade treaty."));
    }
    std::erase_if(s.pendingMajorOffers, [&](const MajorOffer& offer) { return offer.dueTurn <= s.turn; });
}

// AI allies decide whether to honor open calls to arms (the human ally decides by
// declaring war itself before the call falls due).
void answerCallsToArms(GameState& next, TurnReport& report) {
    if (!next.aiAggressionEnabled) return;
    const auto calls = next.diplomacyEngineState.defenseCalls;
    for (const auto& call : calls) {
        if (call.ally == next.playerEmpire || !campaignEmpire(next, call.ally) || !campaignEmpire(next, call.aggressor)) continue;
        if (empireEliminatedInternal(next, call.ally) || empireEliminatedInternal(next, call.aggressor)) continue;
        const auto* rel = majorRelation(next, call.ally, call.aggressor);
        if (!rel || !rel->contact || rel->atWar) continue;
        const auto* defRel = majorRelation(next, call.defender, call.aggressor);
        if (!defRel || !defRel->atWar) continue;
        const CampaignDiplomacyContext ctx(next);
        const auto ally = diplomacy::ParticipantId::major(call.ally);
        const auto aggressor = diplomacy::ParticipantId::major(call.aggressor);
        const auto& ds = next.diplomacyEngineState;
        const bool bound = ds.highestTreatyRank(ally, aggressor) > 0 || ds.hasActiveTreaty(ally, aggressor, diplomacy::TreatyType::Peace);
        if (!diplomacy::aiHonorsDefenseCall(diplomacy::personalityForEmpire(call.ally), ctx.militaryPower(ally),
                ctx.militaryPower(aggressor), bound)) continue;
        enactWar(next, call.ally, call.aggressor);
        const bool playerInvolved = call.defender == next.playerEmpire || call.aggressor == next.playerEmpire;
        const auto* playerRelA = majorRelation(next, next.playerEmpire, call.ally);
        if (playerInvolved || (playerRelA && playerRelA->contact)) {
            report.events.push_back(std::string(empireName(call.ally)) + " honored its alliance with " +
                empireName(call.defender) + " and declared war on " + empireName(call.aggressor) + ".");
        }
    }
}

// Per-turn treaty consequences between surviving majors (modern rules):
// Friendship/Affiliation/Alliance pay a treaty dividend and grow trust, peaceful
// relations without treaties drift toward the personalities' baselines, long wars
// breed weariness, and Affiliation/Alliance partners share their star charts.
void applyTreatyConsequences(GameState& next, TurnReport& report) {
    const auto& ds = next.diplomacyEngineState;
    for (auto& rel : next.majorRelations) {
        if (!rel.contact || empireEliminatedInternal(next, rel.first) || empireEliminatedInternal(next, rel.second)) continue;
        const auto a = diplomacy::ParticipantId::major(rel.first), b = diplomacy::ParticipantId::major(rel.second);
        const int rank = ds.highestTreatyRank(a, b);
        if (rel.atWar) {
            if (next.turn - rel.warStartTurn >= 10 && rel.trust < 30 && next.turn % 2 == 0) ++rel.trust;
            continue;
        }
        int dividend = 0, cap = 0, every = 0;
        if (rank == 4) { dividend = 8; cap = 100; every = 1; }
        else if (rank == 3) { dividend = 5; cap = 85; every = 1; }
        else if (rank == 2) { dividend = 3; cap = 75; every = 2; }
        else if (rank == 1) { cap = 60; every = 4; }
        if (dividend > 0) {
            creditEmpire(next, rel.first, dividend);
            creditEmpire(next, rel.second, dividend);
            if (rel.first == next.playerEmpire || rel.second == next.playerEmpire) report.tradeIncome += dividend;
        }
        if (rank > 0) {
            if (rel.trust < cap && next.turn % every == 0) ++rel.trust;
        } else if (next.turn % 3 == 0) {
            const int baseline = (diplomacy::profileFor(diplomacy::personalityForEmpire(rel.first)).trustBaseline +
                                  diplomacy::profileFor(diplomacy::personalityForEmpire(rel.second)).trustBaseline) / 2;
            if (rel.trust < baseline) ++rel.trust;
            else if (rel.trust > baseline) --rel.trust;
        }
        if (rank >= 3) {
            // Shared sensors/exploration: both empires learn each other's charted sectors.
            auto& first = mutableEmpire(next, rel.first).explored;
            auto& second = mutableEmpire(next, rel.second).explored;
            std::vector<int> merged;
            merged.reserve(first.size() + second.size());
            std::set_union(first.begin(), first.end(), second.begin(), second.end(), std::back_inserter(merged));
            first = merged;
            second = std::move(merged);
        }
    }
}

// Minor races (modern rules). Every MinorReviewInterval turns each minor council
// reviews its relations: attitudes drift with treaties (legacy friendship counts),
// proximity and rival influence; members weigh their empire's care against its
// wars and may secede when loyalty falls below MinorSecessionAttitude.
constexpr int MinorReviewInterval = 5;
constexpr int MinorSecessionAttitude = 30;

int minorFriendshipRank(const GameState& s, const Minor& m, int empireId) {
    int rank = s.diplomacyEngineState.highestTreatyRank(diplomacy::ParticipantId::major(empireId),
        diplomacy::ParticipantId::minor(m.systemId));
    if (empireId >= 1 && empireId <= 5 && m.friendshipTurn[empireId] >= 0) rank = std::max(rank, 2);
    return rank;
}

void resolveMinorDynamics(GameState& next, TurnReport& report) {
    if (next.turn <= 0 || next.turn % MinorReviewInterval != 0) return;
    auto& ds = next.diplomacyEngineState;
    const auto alive = survivingEmpires(next);
    for (auto& m : next.minors) {
        const auto sys = std::find_if(next.systems.begin(), next.systems.end(), [&](const System& x) { return x.id == m.systemId; });
        if (sys == next.systems.end()) continue;
        const auto minorP = diplomacy::ParticipantId::minor(m.systemId);
        int bestRank = 0;
        for (int e : alive) if (e >= 1 && e <= 5 && m.contact[e]) bestRank = std::max(bestRank, minorFriendshipRank(next, m, e));
        for (int e : alive) {
            if (e < 1 || e > 5 || !m.contact[e]) continue;
            int& att = m.attitude[e];
            if (m.memberOf == e) {
                // Member loyalty: expectations erode it, a content colony restores it,
                // and an empire fighting on several fronts neglects its members.
                const auto* colony = colonyAt(next, m.systemId);
                int wars = 0;
                for (const auto& r : next.majorRelations) {
                    if (r.atWar && (r.first == e || r.second == e)) {
                        const int other = r.first == e ? r.second : r.first;
                        if (!empireEliminatedInternal(next, other)) ++wars;
                    }
                }
                att -= 2;
                if (colony && colony->ownerId == e && colony->morale >= 70) att += 2;
                if (wars >= 2) att -= 2 * (wars - 1);
                att = std::clamp(att, 0, 100);
                continue;
            }
            const int rank = minorFriendshipRank(next, m, e);
            if (rank >= 2) att = std::max(att, std::min(att + 2, rank >= 3 ? 90 : 85));
            else if (rank == 1) att = std::max(att, std::min(att + 1, 65));
            else if (att > 50) att = std::max(50, att - 2);
            else if (att < 50) att = std::min(50, att + 2);
            bool near = false;
            for (const auto& c : next.colonies) {
                if (c.ownerId != e || c.systemId == m.systemId) continue;
                const auto loc = std::find_if(next.systems.begin(), next.systems.end(), [&](const System& x) { return x.id == c.systemId; });
                if (loc != next.systems.end() && std::max(std::abs(loc->x - sys->x), std::abs(loc->y - sys->y)) <= 3) { near = true; break; }
            }
            if (near && att < 70) ++att;
            if (rank < 2 && bestRank >= 2 && att > 30) att = std::max(30, att - 2); // Rival influence
            att = std::clamp(att, 0, 100);
        }
        // Secession of unhappy members.
        const int owner = m.memberOf;
        if (owner >= 1 && owner <= 5 && m.attitude[owner] < MinorSecessionAttitude) {
            const auto colony = std::find_if(next.colonies.begin(), next.colonies.end(), [&](const Colony& c) { return c.systemId == m.systemId; });
            if (colony == next.colonies.end() || colony->ownerId != owner) continue;
            if (std::count_if(next.colonies.begin(), next.colonies.end(), [owner](const Colony& c) { return c.ownerId == owner; }) <= 1) continue;
            next.colonies.erase(colony);
            std::erase_if(next.tradeRoutes, [&](const TradeRoute& r) { return r.fromSystemId == m.systemId || r.toSystemId == m.systemId; });
            m.memberOf = 0;
            m.friendshipTurn[owner] = -1;
            m.attitude[owner] = std::min(m.attitude[owner], 20);
            if (auto* ms = ds.getMinorState(m.systemId)) {
                ms->membershipOwner = 0;
                ms->membershipTurn = -1;
            }
            diplomacy::DiplomaticEvent evt;
            evt.turn = next.turn;
            evt.type = diplomacy::DiplomaticEventType::MinorSeceded;
            evt.partyA = minorP;
            evt.partyB = diplomacy::ParticipantId::major(owner);
            evt.description = "Seceded from its empire after prolonged neglect.";
            diplomacy::DiplomacyEngine::recordEvent(ds, evt);
            if (owner == next.playerEmpire) {
                report.events.push_back(m.name + " seceded from our empire. Neglect and war exhausted their loyalty; the colony is independent again.");
            } else if (m.contact[next.playerEmpire]) {
                report.events.push_back(m.name + " seceded from " + std::string(empireName(owner)) + ".");
            }
        }
    }
}

void submitAiProposals(GameState& next, TurnReport& report) {
    auto& ds = next.diplomacyEngineState;
    ds.currentTurn = next.turn;
    const CampaignDiplomacyContext ctx(next);
    std::vector<int> planners;
    for (int id : survivingEmpires(next)) if (id != next.playerEmpire) planners.push_back(id);
    const auto intents = diplomacy::planAiDiplomacy(ds, ctx, planners, next.seed ^ 0xD1B54A32D192ED03ULL);
    for (const auto& intent : intents) {
        const bool toPlayer = ctx.isHuman(intent.target);
        std::uint64_t id = 0;
        if (!diplomacy::DiplomacyEngine::submitProposal(ds, ctx, intent.proposer, intent.target, intent.treatyType,
                intent.duration, intent.tone, intent.creditOffer, intent.creditDemand, &id, nullptr, toPlayer ? 2 : 1)) continue;
        if (!toPlayer) continue;
        const auto* p = ds.findProposal(id);
        const std::string who = empireName(intent.proposer.id);
        std::string text;
        if (intent.treatyType == diplomacy::TreatyType::None && intent.creditDemand > 0) {
            text = who + " demands " + std::to_string(intent.creditDemand) + " credits in tribute";
        } else if (intent.treatyType == diplomacy::TreatyType::None) {
            text = who + " offers a gift of " + std::to_string(intent.creditOffer) + " credits";
        } else {
            text = who + " proposes a " + diplomacy::treatyTypeName(intent.treatyType);
        }
        report.events.push_back("Diplomacy: " + text + " (proposal " + std::to_string(id) + ", respond by turn " +
            std::to_string(p ? p->dueTurn : next.turn + 2) + ").");
    }
}

bool newsworthy(diplomacy::DiplomaticEventType type) {
    using T = diplomacy::DiplomaticEventType;
    return type == T::WarDeclared || type == T::PeaceEstablished || type == T::TreatyBetrayed ||
        type == T::MinorMembershipJoined || type == T::AllianceRefused || type == T::AllianceHonored ||
        type == T::ParticipantEliminated || type == T::AllianceCalledToArms;
}

// Player-facing sentence for an engine event, with empire and minor names.
std::string describeDiplomaticEvent(const GameState& s, const diplomacy::DiplomaticEvent& evt) {
    using T = diplomacy::DiplomaticEventType;
    const std::string a = participantName(s, evt.partyA);
    const std::string b = participantName(s, evt.partyB);
    const int third = static_cast<int>(evt.numericValue);
    const std::string thirdName = third >= 1 && third <= 5 ? std::string(empireName(third)) : std::string("its enemy");
    switch (evt.type) {
        case T::WarDeclared: return a + " declared war on " + b + ".";
        case T::PeaceEstablished: return a + " and " + b + " made peace.";
        case T::TreatyBetrayed: return a + " broke its treaty with " + b + " by declaring war. Every empire takes note.";
        case T::AllianceCalledToArms: return a + " called its ally " + b + " to arms against " + thirdName + ". " + evt.description;
        case T::AllianceHonored: return b + "'s ally " + a + " joined the war against " + thirdName + ".";
        case T::AllianceRefused: return a + " refused " + b + "'s call to arms against " + thirdName + ".";
        case T::ParticipantEliminated: return a + " has been eliminated; its treaties are dissolved.";
        case T::MinorMembershipJoined: return b + " joined " + a + ".";
        default: return "(" + a + (evt.partyA == evt.partyB ? "" : " / " + b) + "): " + evt.description;
    }
}

void reportDiplomaticEvent(const GameState& next, TurnReport& report, const diplomacy::DiplomaticEvent& evt, bool includePlayerWars) {
    using T = diplomacy::DiplomaticEventType;
    const auto player = diplomacy::ParticipantId::major(next.playerEmpire);
    const bool involvesPlayer = evt.partyA == player || evt.partyB == player;
    if (evt.description.empty() || evt.type == T::ProposalSubmitted) return;
    if (involvesPlayer) {
        if (!includePlayerWars && (evt.type == T::WarDeclared)) return; // Already reported by the relation diff
        if (evt.type == T::AllianceCalledToArms && evt.partyB == player) {
            report.events.push_back("Diplomacy: " + participantName(next, evt.partyA) + " calls on us to honor our alliance against " +
                (evt.numericValue >= 1 && evt.numericValue <= 5 ? std::string(empireName(static_cast<int>(evt.numericValue))) : std::string("its enemy")) +
                ". Declare war before the call falls due or lose their trust. " + evt.description);
        } else {
            const std::string text = describeDiplomaticEvent(next, evt);
            report.events.push_back("Diplomacy" + std::string(text.front() == '(' ? " " : ": ") + text);
        }
        return;
    }
    if (!newsworthy(evt.type) && !(evt.type == T::TreatyEstablished && evt.numericValue >= static_cast<int>(diplomacy::TreatyType::Affiliation) &&
                                   evt.numericValue <= static_cast<int>(diplomacy::TreatyType::Alliance))) return;
    auto knows = [&](diplomacy::ParticipantId p) {
        if (p.isMinor) { const auto* m = minorAt(next, p.id); return m && m->contact[next.playerEmpire]; }
        const auto* rel = majorRelation(next, next.playerEmpire, p.id);
        return rel && rel->contact;
    };
    if (!knows(evt.partyA) && !knows(evt.partyB)) return;
    const std::string text = describeDiplomaticEvent(next, evt);
    report.events.push_back("Galactic news" + std::string(text.front() == '(' ? " " : ": ") + text);
}

// Membership follows conquest: a member minor whose colony was captured becomes a
// (resentful) member of the conqueror. Idempotent with an invasion-merge fix in the
// turn pipeline; keeps "Minor ownership mismatch" from ever reaching validate().
void reconcileMinorMembership(GameState& next, TurnReport& report) {
    for (auto& m : next.minors) {
        const auto* colony = colonyAt(next, m.systemId);
        const int holder = colony ? colony->ownerId : 0;
        if (m.memberOf == holder) continue;
        const int previous = m.memberOf;
        m.memberOf = holder;
        if (holder >= 1 && holder <= 5) {
            m.contact[holder] = true;
            m.attitude[holder] = std::min(m.attitude[holder], 40);
        }
        if (previous >= 1 && previous <= 5) m.friendshipTurn[previous] = -1;
        if (holder && m.contact[next.playerEmpire]) {
            report.events.push_back(m.name + " was conquered by " + std::string(empireName(holder)) +
                (previous ? " and torn from " + std::string(empireName(previous)) : std::string()) + ".");
        }
    }
}

void resolveModernDiplomacy(GameState& next, TurnReport& report) {
    reconcileMinorMembership(next, report);
    syncDiplomacyStateInternal(next);
    auto& ds = next.diplomacyEngineState;
    // Events recorded earlier this turn (war declarations from orders and AI planning).
    std::vector<diplomacy::DiplomaticEvent> earlier;
    for (const auto& evt : ds.eventHistory) if (evt.turn == next.turn) earlier.push_back(evt);
    ds.currentTurn = next.turn;

    answerCallsToArms(next, report);
    for (const auto& evt : ds.eventHistory) {
        if (evt.turn == next.turn && std::find(earlier.begin(), earlier.end(), evt) == earlier.end()) earlier.push_back(evt);
    }

    CampaignDiplomacyContext diploCtx(next);
    auto diploDelta = diplomacy::DiplomacyEngine::resolveTurn(ds, diploCtx, next.rngState);
    ds.currentTurn = next.turn;
    applyDiplomacyOps(next, diploDelta, &report, /*skipWarState=*/false);

    applyTreatyConsequences(next, report);
    resolveMinorDynamics(next, report);
    syncDiplomacyStateInternal(next);
    submitAiProposals(next, report);

    for (const auto& evt : earlier) {
        if (evt.type == diplomacy::DiplomaticEventType::ProposalRejected) continue; // Cancellations by war: noise
        reportDiplomaticEvent(next, report, evt, /*includePlayerWars=*/false);
    }
    for (const auto& evt : diploDelta.events) reportDiplomaticEvent(next, report, evt, /*includePlayerWars=*/true);
    ds.currentTurn = next.turn;
}

// Research ledger helpers: `research` must always equal reserve + progress + the
// cost of every level gained (validate's "Research ledger does not reconcile").
std::int64_t takeResearch(EmpireState& e, std::int64_t amount) {
    amount = std::clamp<std::int64_t>(amount, 0, e.research);
    std::int64_t taken = std::min(amount, e.researchReserve);
    e.researchReserve -= taken;
    while (taken < amount) {
        int best = -1;
        for (int i = 0; i < ResearchFields; ++i) {
            if (e.technologies[i].progress > 0 && (best < 0 || e.technologies[i].progress > e.technologies[best].progress)) best = i;
        }
        if (best < 0) break;
        const auto part = std::min(amount - taken, e.technologies[best].progress);
        e.technologies[best].progress -= part;
        taken += part;
    }
    e.research -= taken;
    return taken;
}

// Returns the number of levels gained.
int grantResearch(const GameState& s, EmpireState& e, std::int64_t amount) {
    amount = std::clamp<std::int64_t>(amount, 0, MaxBalance - e.research);
    if (amount <= 0) return 0;
    e.research += amount;
    int field = -1;
    for (int i = 0; i < ResearchFields; ++i) {
        if (e.technologies[i].level < MaxTechLevel && (field < 0 || e.technologies[i].level < e.technologies[field].level)) field = i;
    }
    if (field < 0) {
        e.researchReserve = std::min(MaxBalance, e.researchReserve + amount);
        return 0;
    }
    int gained = 0;
    auto& tech = e.technologies[field];
    tech.progress += amount;
    while (tech.level < MaxTechLevel && tech.progress >= researchCost(s, field, tech.level + 1)) {
        tech.progress -= researchCost(s, field, ++tech.level);
        ++gained;
    }
    if (tech.level == MaxTechLevel) { e.researchReserve += tech.progress; tech.progress = 0; }
    return gained;
}

std::int64_t stealableResearch(const EmpireState& e) {
    std::int64_t pool = e.researchReserve;
    for (const auto& tech : e.technologies) pool += tech.progress;
    return std::clamp<std::int64_t>(pool, 0, MaxBalance);
}

intelligence::TreatyLevel intelTreatyLevel(const GameState& next, int a, int b) {
    const auto* rel = majorRelation(next, a, b);
    if (rel && rel->atWar) return intelligence::TreatyLevel::War;
    intelligence::TreatyLevel level = intelligence::TreatyLevel::None;
    for (const auto& t : next.diplomacyEngineState.activeTreaties) {
        if (t.status != diplomacy::TreatyStatus::Active ||
            !t.involvesBoth(diplomacy::ParticipantId::major(a), diplomacy::ParticipantId::major(b))) continue;
        if (t.type == diplomacy::TreatyType::Alliance) level = std::max(level, intelligence::TreatyLevel::Alliance);
        else if (t.type == diplomacy::TreatyType::Affiliation) level = std::max(level, intelligence::TreatyLevel::Affiliation);
        else if (t.type == diplomacy::TreatyType::Friendship) level = std::max(level, intelligence::TreatyLevel::Friendship);
        else if (t.type == diplomacy::TreatyType::NonAggression) level = std::max(level, intelligence::TreatyLevel::NonAggression);
    }
    if (level == intelligence::TreatyLevel::None && rel && rel->tradeTreaty) level = intelligence::TreatyLevel::Affiliation;
    return level;
}

// AI empires re-plan budget and focus every turn from doctrine and situation.
void planAiIntelligence(GameState& next, const std::vector<int>& activeEmpires) {
    if (!next.aiAggressionEnabled) return; // Passive AI keeps its default posture
    const CampaignDiplomacyContext ctx(next);
    for (int empId : activeEmpires) {
        if (empId == next.playerEmpire) continue;
        std::vector<intelligence::AiIntelRivalView> views;
        for (int rivalId : activeEmpires) {
            if (rivalId == empId) continue;
            const auto* rel = majorRelation(next, empId, rivalId);
            if (!rel || !rel->contact) continue; // Unknown empires are not targeted
            intelligence::AiIntelRivalView v;
            v.rivalId = rivalId;
            v.atWar = rel->atWar;
            v.treaty = intelTreatyLevel(next, empId, rivalId);
            v.trust = rel->trust;
            v.ownPower = ctx.militaryPower(diplomacy::ParticipantId::major(empId));
            v.rivalPower = ctx.militaryPower(diplomacy::ParticipantId::major(rivalId));
            for (const auto& op : next.intelOperations) {
                if (!op.detectedByDefender || op.turn < next.turn - 10) continue;
                if (op.attackerEmpireId == rivalId && op.defenderEmpireId == empId) ++v.incidentsAgainstUs;
                if (op.attackerEmpireId == empId && op.defenderEmpireId == rivalId) ++v.exposedOperations;
            }
            views.push_back(v);
        }
        auto& intelState = next.empireIntelStates[empId];
        intelState.empireId = empId;
        const auto plan = intelligence::planAiIntelStrategy(empId,
            static_cast<intelligence::IntelDoctrine>(static_cast<int>(diplomacy::personalityForEmpire(empId))), views);
        intelligence::applyAiIntelPlan(intelState, plan);
    }
}

void resolveModernIntelligence(GameState& next, TurnReport& report) {
    // Eliminated empires neither run nor suffer intelligence operations.
    const std::vector<int> activeEmpires = survivingEmpires(next);

    std::map<int, intelligence::DefenderSecurityContext> defenderContexts;
    for (int empId : activeEmpires) {
        intelligence::DefenderSecurityContext dctx;
        auto isIt = next.empireIntelStates.find(empId);
        if (isIt != next.empireIntelStates.end()) {
            dctx.rollingSecuritySum = isIt->second.getRollingSecuritySum();
            dctx.securityStockpile = isIt->second.internalSecurityStockpile;
        }
        const auto& empState = empire(next, empId);
        dctx.computerTechLevel = empState.technologies[static_cast<int>(ResearchField::Computers)].level;
        dctx.defenderMoralePercent = 100; // Provisional per reviewer notes
        defenderContexts[empId] = dctx;
    }

    intelligence::IntelligenceRules rules;
    rules.defaultDefenderMoralePercent = 100; // Provisional per reviewer notes

    planAiIntelligence(next, activeEmpires);

    for (int empId : activeEmpires) {
        auto& intelState = next.empireIntelStates[empId];
        intelState.empireId = empId;
        // Budget lines against eliminated rivals return to internal security.
        auto& allocations = intelState.budget.rivalAllocations;
        for (auto it = allocations.begin(); it != allocations.end();) {
            if (std::find(activeEmpires.begin(), activeEmpires.end(), it->rivalId) == activeEmpires.end()) {
                intelState.budget.internalSecurity.percentage += it->espionage.percentage + it->sabotage.percentage;
                if (empId == next.playerEmpire && it->espionage.percentage + it->sabotage.percentage > 0) {
                    report.events.push_back("Intelligence: agents recalled from " + std::string(empireName(it->rivalId)) +
                        "; their budget returns to internal security.");
                }
                it = allocations.erase(it);
            } else {
                ++it;
            }
        }
        if (!intelState.budget.isValid(empId)) {
            std::vector<int> rivals;
            for (int other : activeEmpires) if (other != empId) rivals.push_back(other);
            intelState.budget.setDefault(rivals, empId);
        }

        intelligence::TurnContext tctx;
        tctx.currentTurn = next.turn;
        tctx.totalEmpireBaseIntelOutput = std::clamp<std::int64_t>(botf::empireIntelOutput(next, empId), 0, 100000000);
        tctx.buildingBonusPercent = 0;
        const auto& empState = empire(next, empId);
        tctx.attackerComputerTechLevel = empState.technologies[static_cast<int>(ResearchField::Computers)].level;

        for (int rivalId : activeEmpires) {
            if (rivalId == empId) continue;
            intelligence::RivalContext rctx;
            rctx.rivalId = rivalId;
            rctx.treaty = intelTreatyLevel(next, empId, rivalId);
            const auto& rivEmp = empire(next, rivalId);
            rctx.currentCredits = std::clamp<std::int64_t>(rivEmp.credits, 0, MaxBalance);
            rctx.currentResearchReserve = stealableResearch(rivEmp);
            rctx.computerTechLevel = rivEmp.technologies[static_cast<int>(ResearchField::Computers)].level;
            rctx.difficultyModifier = 0;
            tctx.knownRivals.push_back(rctx);
        }

        auto ops = intelligence::advanceTurn(intelState, tctx, defenderContexts, rules, next.rngState);
        for (auto op : ops) {
            if (op.outcome >= intelligence::IntelOutcome::MinorSuccess) {
                if (op.area == intelligence::IntelArea::Espionage) {
                    if (op.creditsStolen > 0) {
                        auto& def = mutableEmpire(next, op.defenderEmpireId);
                        const std::int64_t stolen = std::clamp<std::int64_t>(std::min(def.credits, op.creditsStolen), 0, MaxBalance);
                        def.credits -= stolen;
                        creditEmpire(next, op.attackerEmpireId, stolen);
                        op.creditsStolen = stolen;
                    }
                    if (op.researchStolen > 0) {
                        // Theft moves points between research ledgers so both still reconcile.
                        const std::int64_t stolen = takeResearch(mutableEmpire(next, op.defenderEmpireId), op.researchStolen);
                        const int levels = grantResearch(next, mutableEmpire(next, op.attackerEmpireId), stolen);
                        op.researchStolen = stolen;
                        if (op.attackerEmpireId == next.playerEmpire && levels > 0) {
                            report.events.push_back("Intelligence: stolen blueprints advanced our research by " + std::to_string(levels) + " level(s).");
                        }
                    }
                    IntelReport rpt;
                    rpt.turn = next.turn;
                    rpt.viewer = op.attackerEmpireId;
                    rpt.target = op.defenderEmpireId;
                    rpt.success = true;
                    rpt.estimatedCredits = empire(next, op.defenderEmpireId).credits;
                    for (const auto& c : next.colonies) {
                        if (c.ownerId == op.defenderEmpireId) ++rpt.knownColonies;
                    }
                    next.intelReports.push_back(rpt);
                    if (next.intelReports.size() > 32) next.intelReports.erase(next.intelReports.begin());
                    if (op.attackerEmpireId == next.playerEmpire) {
                        std::string gain = op.creditsStolen > 0 ? " (" + std::to_string(op.creditsStolen) + " credits stolen)"
                            : op.researchStolen > 0 ? " (" + std::to_string(op.researchStolen) + " research stolen)" : "";
                        report.events.push_back("Intelligence: Espionage against " + std::string(empireName(op.defenderEmpireId)) + " succeeded" + gain + ".");
                    }
                } else if (op.area == intelligence::IntelArea::Sabotage) {
                    if (op.creditsStolen > 0) {
                        auto& def = mutableEmpire(next, op.defenderEmpireId);
                        def.credits = std::max<std::int64_t>(-MaxBalance, def.credits - op.creditsStolen);
                    }
                    for (auto& c : next.colonies) {
                        if (c.ownerId == op.defenderEmpireId && !c.queue.empty()) {
                            c.queue.front().work = std::max(0, c.queue.front().work - 25 * std::max(1, op.sabotageSeverity));
                            break;
                        }
                    }
                    if (op.attackerEmpireId == next.playerEmpire) {
                        report.events.push_back("Intelligence: Sabotage against " + std::string(empireName(op.defenderEmpireId)) + " succeeded.");
                    }
                }
            }
            if (op.detectedByDefender) {
                auto& rel = mutableRelation(next, op.attackerEmpireId, op.defenderEmpireId);
                rel.trust = std::max(0, rel.trust - (op.diplomaticIncidentPenalty > 0 ? op.diplomaticIncidentPenalty : 15));
                next.diplomacyEngineState.addGrievance(op.defenderEmpireId, op.attackerEmpireId, 1);
                if (op.defenderEmpireId == next.playerEmpire) {
                    report.events.push_back("Security Alert: Hostile agent activity detected from " + std::string(empireName(op.attackerEmpireId)) + "!");
                } else if (op.attackerEmpireId == next.playerEmpire) {
                    report.events.push_back("Intelligence: our agents were exposed by " + std::string(empireName(op.defenderEmpireId)) + ".");
                }
            }
            next.intelOperations.push_back(op);
            if (next.intelOperations.size() > 64) {
                next.intelOperations.erase(next.intelOperations.begin());
            }
        }
    }
}

void resolveFleets(GameState& s, TurnReport& report) {
    const auto before = s.explored;
    std::map<int, std::vector<int>> tfRoutes;
    for (const auto& tf : s.taskForces) {
        if (tf.fleetIds.empty()) continue;
        const auto leader = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const Fleet& f) { return f.id == tf.fleetIds.front(); });
        if (leader != s.fleets.end()) {
            int minRange = 999;
            for (int fid : tf.fleetIds) {
                const auto m = std::find_if(s.fleets.begin(), s.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
                if (m != s.fleets.end()) minRange = std::min(minRange, shipRange(*m));
            }
            auto suppliedWithRange = [&](int x, int y) {
                if (internal::colonySuppliedWithRange(s, leader->ownerId, minRange, x, y)) return true;
                // Schema 13: stations and known wormholes extend task-force supply too.
                return gameplay::layerActive(s) && gameplay::extendedSupply(s, leader->ownerId, x, y, [&](int px, int py) {
                    return internal::colonySuppliedWithRange(s, leader->ownerId, minRange, px, py);
                });
            };
            const int start = leader->y * s.width + leader->x, goal = leader->targetY * s.width + leader->targetX;
            if (start == goal) {
                tfRoutes[tf.id] = {start};
            } else if (!s.galaxyFeatures.empty() && suppliedWithRange(leader->targetX, leader->targetY)) {
                auto p = gameplay::findPath(s, leader->ownerId, start, goal, suppliedWithRange);
                if (!p.empty()) tfRoutes[tf.id] = std::move(p);
            } else if (suppliedWithRange(leader->targetX, leader->targetY)) {
                std::vector<int> prev(static_cast<std::size_t>(s.width * s.height), -1);
                std::queue<int> q; q.push(start); prev[start] = start;
                while (!q.empty() && prev[goal] == -1) {
                    int cell = q.front(); q.pop();
                    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                        if (!dx && !dy) continue;
                        int nx = cell % s.width + dx, ny = cell / s.width + dy;
                        if (!suppliedWithRange(nx, ny)) continue;
                        int next = ny * s.width + nx;
                        if (prev[next] != -1) continue;
                        prev[next] = cell; q.push(next);
                    }
                }
                if (prev[goal] != -1) {
                    std::vector<int> p;
                    for (int c = goal; c != start; c = prev[c]) p.push_back(c);
                    p.push_back(start);
                    std::reverse(p.begin(), p.end());
                    tfRoutes[tf.id] = std::move(p);
                }
            }
        }
    }

    for (auto& f : s.fleets) {
        if (f.attackTarget) {
            const auto target = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const Fleet& other) { return other.id == f.attackTarget; });
            const auto relation = target == s.fleets.end() ? nullptr : majorRelation(s, f.ownerId, target->ownerId);
            if (target == s.fleets.end() || !relation || !relation->atWar) f.attackTarget = 0;
            else if (visible(s, f.ownerId, target->x, target->y)) { f.targetX = target->x; f.targetY = target->y; }
        }
        std::vector<int> path;
        if (f.taskForceId > 0) {
            if (tfRoutes.count(f.taskForceId)) {
                path = tfRoutes[f.taskForceId];
            }
        } else {
            path = route(s, f, f.targetX, f.targetY);
        }
        if (path.size() > 1) {
            int speed = 1;
            if (f.taskForceId > 0) speed = taskForceSpeed(s, f.taskForceId);
            else if (f.referenceShipId >= 0) speed = std::clamp(static_cast<int>(sourceShip(f.referenceShipId).speed), 1, 3);
            for (int step = 1; step <= speed && static_cast<std::size_t>(step) < path.size(); ++step) {
                f.x = path[step] % s.width; f.y = path[step] / s.width;
                reveal(s, f.x, f.y, f.ownerId);
            }
        }
        else if (path.empty() && f.ownerId == s.playerEmpire) report.events.push_back("Fleet " + std::to_string(f.id) + ": route paused, no supplied path.");
        reveal(s, f.x, f.y, f.ownerId);

        // Resolve terraforming orders
        if (f.terraforming) {
            auto sysIt = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& sys) { return sys.id == f.terraformSystemId; });
            if (sysIt != s.systems.end() && f.x == sysIt->x && f.y == sysIt->y &&
                f.terraformPlanetIndex >= 0 && static_cast<std::size_t>(f.terraformPlanetIndex) < sysIt->planets.size()) {
                auto& p = sysIt->planets[static_cast<std::size_t>(f.terraformPlanetIndex)];
                // Work per turn calculated based on Colony Ship production baseline per BETA-FIDELITY.md:
                // Colony Ship I: 100 work/turn. Colony Ship II: 300 work/turn.
                // No unverified Biotech throughput multiplier.
                int workInc = 100;
                if (f.referenceShipId >= 0) {
                    const auto& rShip = sourceShip(f.referenceShipId);
                    workInc = (std::string_view(rShip.name).find("II") != std::string_view::npos || rShip.technologyRequirements[0] >= 5) ? 300 : 100;
                }
                p.terraformWork += workInc;
                p.terraformState = TerraformingState::InProgress;
                if (p.terraformWork >= p.terraformNeeded) {
                    p.terraformWork = p.terraformNeeded;
                    p.terraformState = TerraformingState::Terraformed;
                    f.terraforming = false;
                    f.terraformSystemId = 0;
                    f.terraformPlanetIndex = 0;
                    if (f.ownerId == s.playerEmpire) {
                        report.events.push_back("Terraforming of " + p.name + " completed. Habitable capacity expanded to " +
                            std::to_string(systemCapacity(s, sysIt->id)) + ".");
                    }
                }
            } else {
                f.terraforming = false;
                f.terraformSystemId = 0;
                f.terraformPlanetIndex = 0;
            }
        }

        if (!f.colonize) continue;
        const auto system = std::find_if(s.systems.begin(), s.systems.end(),
            [&](const System& x) { return x.x == f.x && x.y == f.y; });
        require(system != s.systems.end() && !colonyAt(s, system->id), "Colonization target unavailable");
        Colony c; c.systemId = system->id; c.ownerId = f.ownerId;
        c.population = 20; c.foodWorkers = 10; c.industryWorkers = 10; c.researchWorkers = 0;
        c.farms = 1; c.factories = 1; c.laboratories = 0; c.energy = 10;
        s.colonies.push_back(c);
        if (f.ownerId == s.playerEmpire) report.events.push_back("Colony founded at " + system->name + ". Production begins next turn.");
    }
    std::erase_if(s.fleets, [](const Fleet& f) { return f.colonize; });
    std::sort(s.colonies.begin(), s.colonies.end(), [](const Colony& a, const Colony& b) { return a.systemId < b.systemId; });
    reconcileTaskForces(s);

    for (const auto& system : s.systems) {
        const int cell = system.y * s.width + system.x;
        if (!std::binary_search(before.begin(), before.end(), cell) && explored(s, system.x, system.y))
            report.events.push_back("Discovered " + system.name + ".");
    }
}

void establishMajorContact(GameState& s, TurnReport& report) {
    for (auto& relation : s.majorRelations) {
        if (relation.contact) continue;
        bool sighted = false;
        for (const auto& fleet : s.fleets) {
            if (fleet.ownerId == relation.first && visible(s, relation.second, fleet.x, fleet.y)) sighted = true;
            if (fleet.ownerId == relation.second && visible(s, relation.first, fleet.x, fleet.y)) sighted = true;
        }
        for (const auto& colony : s.colonies) {
            if (colony.ownerId != relation.first && colony.ownerId != relation.second) continue;
            const int observer = colony.ownerId == relation.first ? relation.second : relation.first;
            const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == colony.systemId; });
            if (visible(s, observer, loc->x, loc->y)) sighted = true;
        }
        if (sighted) {
            relation.contact = true;
            if (relation.first == s.playerEmpire || relation.second == s.playerEmpire)
                report.events.push_back("First contact with " + std::string(empireName(relation.first == s.playerEmpire ? relation.second : relation.first)) + ".");
        }
    }
}

void resolveBattles(GameState& s, TurnReport& report) {
    std::set<int> withdrawnThisTurn;
    for (int y = 0; y < s.height; ++y) for (int x = 0; x < s.width; ++x) {
        for (std::size_t index = 0; index < s.majorRelations.size(); ++index) {
            const auto& relation = s.majorRelations[index];
            if (!relation.atWar) continue;
            const int first = relation.first, second = relation.second;
            std::vector<Fleet*> a, b;
            for (auto& fleet : s.fleets) if (fleet.x == x && fleet.y == y && fleet.hull > 0 && !withdrawnThisTurn.count(fleet.id)) {
                if (fleet.ownerId == first) a.push_back(&fleet);
                if (fleet.ownerId == second) b.push_back(&fleet);
            }
            if (a.empty() || b.empty()) continue;
            CombatReport battle; battle.turn = s.turn; battle.x = x; battle.y = y;
            battle.firstOwner = first; battle.secondOwner = second;
            auto addParticipant = [&](const Fleet* f, std::vector<int>& side) {
                side.push_back(f->id);
                battle.participants.push_back({f->id, f->ownerId, f->shipClass, f->referenceShipId, f->hull, f->shields});
                battle.damage.push_back({f->id, f->hull, f->hull, f->shields, f->shields});
            };
            for (const auto* f : a) addParticipant(f, battle.firstFleetIds);
            for (const auto* f : b) addParticipant(f, battle.secondFleetIds);
            auto active = [&](const Fleet* f) { return f->hull > 0 && f->x == x && f->y == y && !withdrawnThisTurn.count(f->id); };
            auto hasActive = [&](const std::vector<Fleet*>& side) {
                return std::any_of(side.begin(), side.end(), active);
            };
            auto withdraw = [&](Fleet* f, CombatRound& round) {
                auto safe = [&](int px, int py) {
                    if (px < 0 || py < 0 || px >= s.width || py >= s.height ||
                        !sectorPassable(s, px, py) || !supplied(s, *f, px, py)) return false;
                    return std::none_of(s.fleets.begin(), s.fleets.end(), [&](const Fleet& other) {
                        const auto enemy = majorRelation(s, f->ownerId, other.ownerId);
                        return other.hull > 0 && other.x == px && other.y == py && enemy && enemy->atWar;
                    });
                };
                const System* home = nullptr; int best = std::numeric_limits<int>::max();
                bool homePoweredYard = false;
                for (const auto& c : s.colonies) if (c.ownerId == f->ownerId) {
                    const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& value) { return value.id == c.systemId; });
                    const int distance = std::max(std::abs(f->x - loc->x), std::abs(f->y - loc->y));
                    if (distance <= 0) continue;
                    const bool poweredYard = s.advancedRulesEnabled && economy(s, c).shipyardPowered;
                    if (s.advancedRulesEnabled && f->hull < fleetMaxHull(*f)) {
                        if ((poweredYard && !homePoweredYard) || (poweredYard == homePoweredYard && distance < best)) {
                            best = distance; home = &*loc; homePoweredYard = poweredYard;
                        }
                    } else if (distance < best) {
                        best = distance; home = &*loc;
                    }
                }
                int retreatX = f->x, retreatY = f->y;
                if (home) {
                    const int candidateX = f->x + (home->x > f->x) - (home->x < f->x);
                    const int candidateY = f->y + (home->y > f->y) - (home->y < f->y);
                    if (safe(candidateX, candidateY)) { retreatX = candidateX; retreatY = candidateY; }
                }
                if (retreatX == f->x && retreatY == f->y) {
                    for (const auto& [dx,dy] : {std::pair{-1,-1}, std::pair{1,-1}, std::pair{-1,1}, std::pair{1,1},
                        std::pair{-1,0}, std::pair{1,0}, std::pair{0,-1}, std::pair{0,1}}) {
                        if (!safe(f->x + dx, f->y + dy)) continue;
                        retreatX = f->x + dx; retreatY = f->y + dy; break;
                    }
                }
                if (retreatX == f->x && retreatY == f->y) return false;
                f->x = retreatX; f->y = retreatY;
                if (s.advancedRulesEnabled && home && (retreatX != home->x || retreatY != home->y)) {
                    f->targetX = home->x; f->targetY = home->y;
                    f->retreat = true;
                } else {
                    f->targetX = retreatX; f->targetY = retreatY;
                    f->retreat = false;
                }
                f->attackTarget = 0; f->invadeSystemId = 0;
                detachFromTaskForce(s, *f);
                withdrawnThisTurn.insert(f->id);
                battle.retreatedFleetIds.push_back(f->id); round.retreatedFleetIds.push_back(f->id);
                reveal(s, f->x, f->y, f->ownerId);
                return true;
            };
            CombatRound opening; opening.index = 1;
            for (auto* f : a) if (f->retreat || (s.advancedRulesEnabled && f->stance == TacticalStance::Withdraw)) {
                withdraw(f, opening);
                if (!s.advancedRulesEnabled) f->retreat = false;
            }
            for (auto* f : b) if (f->retreat || (s.advancedRulesEnabled && f->stance == TacticalStance::Withdraw)) {
                withdraw(f, opening);
                if (!s.advancedRulesEnabled) f->retreat = false;
            }
            if (!opening.retreatedFleetIds.empty()) battle.rounds.push_back(opening);
            auto attackPower = [&](const std::vector<Fleet*>& attackers) {
                int damage = 0;
                for (const auto* f : attackers) if (active(f) && fleetAttack(*f) > 0)
                    damage += fleetAttack(*f) + empire(s, f->ownerId).technologies[5].level;
                return damage;
            };
            auto damageSide = [&](int damage, std::vector<Fleet*>& defenders) {
                for (auto* f : defenders) if (active(f)) {
                    if (damage <= 0) break;
                    const int absorbed = std::min(damage, f->hull);
                    f->hull -= absorbed; damage -= absorbed;
                }
            };
            constexpr int MaxTacticalRounds = 64;
            for (int roundIndex = 1; roundIndex <= MaxTacticalRounds && hasActive(a) && hasActive(b); ++roundIndex) {
                CombatRound round; round.index = roundIndex + (opening.retreatedFleetIds.empty() ? 0 : 1);
                for (const auto* f : a) if (active(f)) round.damage.push_back({f->id, f->hull, f->hull, f->shields, f->shields});
                for (const auto* f : b) if (active(f)) round.damage.push_back({f->id, f->hull, f->hull, f->shields, f->shields});
                const int firstVolley = attackPower(a), secondVolley = attackPower(b);
                if (s.advancedRulesEnabled) {
                    std::vector<std::pair<Fleet*, int>> intents;
                    auto plan = [&](const std::vector<Fleet*>& attackers, const std::vector<Fleet*>& defenders) {
                        for (const auto* attacker : attackers) if (active(attacker) && fleetAttack(*attacker) > 0) {
                            Fleet* target = nullptr;
                            for (auto* defender : defenders)
                                if (active(defender) && defender->id == attacker->attackTarget) { target = defender; break; }
                            if (!target) for (auto* defender : defenders) if (active(defender) &&
                                (!target || defender->hull + defender->shields < target->hull + target->shields)) target = defender;
                            if (!target) continue;
                            int power = fleetAttack(*attacker) + empire(s, attacker->ownerId).technologies[5].level;
                            if (attacker->stance == TacticalStance::Aggressive) power = (power * 5 + 3) / 4;
                            if (attacker->stance == TacticalStance::Defensive) power = std::max(1, power * 3 / 4);
                            intents.push_back({target, power});
                        }
                    };
                    plan(a, b); plan(b, a);
                    for (const auto& [target, rawPower] : intents) {
                        int damage = target->stance == TacticalStance::Defensive ? std::max(1, rawPower * 3 / 4) : rawPower;
                        const int absorbed = std::min(target->shields, damage);
                        target->shields -= absorbed; damage -= absorbed;
                        target->hull = std::max(0, target->hull - damage);
                    }
                } else { damageSide(firstVolley, b); damageSide(secondVolley, a); }
                for (auto& hit : round.damage) {
                    for (const auto* f : a) if (f->id == hit.fleetId) hit.hullAfter = f->hull;
                    for (const auto* f : b) if (f->id == hit.fleetId) hit.hullAfter = f->hull;
                    for (const auto* f : a) if (f->id == hit.fleetId) hit.shieldAfter = f->shields;
                    for (const auto* f : b) if (f->id == hit.fleetId) hit.shieldAfter = f->shields;
                    if (hit.hullAfter == 0) { battle.lostFleetIds.push_back(hit.fleetId); round.lostFleetIds.push_back(hit.fleetId); }
                }
                if (hasActive(a) && hasActive(b)) {
                    for (auto* f : a) if (active(f) && f->hull * 4 <= fleetMaxHull(*f)) withdraw(f, round);
                    for (auto* f : b) if (active(f) && f->hull * 4 <= fleetMaxHull(*f)) withdraw(f, round);
                }
                if ((roundIndex == MaxTacticalRounds || (firstVolley == 0 && secondVolley == 0)) && hasActive(a) && hasActive(b)) {
                    const int powerA = attackPower(a), powerB = attackPower(b);
                    auto& loser = powerA >= powerB ? b : a;
                    for (auto* f : loser) if (active(f) && !withdraw(f, round)) {
                        f->hull = 0; battle.lostFleetIds.push_back(f->id); round.lostFleetIds.push_back(f->id);
                        for (auto& hit : round.damage) if (hit.fleetId == f->id) hit.hullAfter = 0;
                    }
                }
                std::erase_if(round.damage, [](const FleetDamage& hit) {
                    return hit.hullBefore == hit.hullAfter && hit.shieldBefore == hit.shieldAfter;
                });
                battle.rounds.push_back(std::move(round));
            }
            for (auto& summary : battle.damage) {
                for (const auto* f : a) if (f->id == summary.fleetId) summary.hullAfter = f->hull;
                for (const auto* f : b) if (f->id == summary.fleetId) summary.hullAfter = f->hull;
                for (const auto* f : a) if (f->id == summary.fleetId) summary.shieldAfter = f->shields;
                for (const auto* f : b) if (f->id == summary.fleetId) summary.shieldAfter = f->shields;
            }
            const bool aSurvives = hasActive(a), bSurvives = hasActive(b);
            battle.winnerOwner = aSurvives != bSurvives ? (aSurvives ? first : second) : 0;
            report.battles.push_back(battle);
            if (first == s.playerEmpire || second == s.playerEmpire)
                report.events.push_back("Battle at sector " + std::to_string(x) + "," + std::to_string(y) +
                    ": " + std::to_string(battle.lostFleetIds.size()) + " ships lost, " +
                    std::to_string(battle.retreatedFleetIds.size()) + " withdrew.");
        }
    }
    std::erase_if(s.fleets, [](const Fleet& f) { return f.hull <= 0; });
    for (auto& fleet : s.fleets)
        if (fleet.x == fleet.targetX && fleet.y == fleet.targetY) fleet.retreat = false;

    reconcileTaskForces(s);

    s.latestBattles.insert(s.latestBattles.end(), report.battles.begin(), report.battles.end());
    if (s.latestBattles.size() > 16) s.latestBattles.erase(s.latestBattles.begin(), s.latestBattles.end() - 16);
}

void resolveInvasions(GameState& s, TurnReport& report) {
    for (auto& fleet : s.fleets) {
        if (!fleet.invadeSystemId) continue;
        auto target = std::find_if(s.colonies.begin(), s.colonies.end(), [&](const Colony& c) { return c.systemId == fleet.invadeSystemId; });
        if (target == s.colonies.end() || target->ownerId == fleet.ownerId) { fleet.invadeSystemId = 0; continue; }
        const auto loc = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == target->systemId; });
        if (loc->x != fleet.x || loc->y != fleet.y) { fleet.invadeSystemId = 0; continue; }
        const bool defended = std::any_of(s.fleets.begin(), s.fleets.end(), [&](const Fleet& f) {
            return f.ownerId == target->ownerId && f.x == fleet.x && f.y == fleet.y && fleetAttack(f) > 0;
        }) || gameplay::stationDefends(s, target->ownerId, fleet.x, fleet.y); // Schema 13: stations guard the system.
        if (defended) {
            if (fleet.ownerId == s.playerEmpire) report.events.push_back("Invasion of " + loc->name + " blocked by defending ships.");
            report.invasions.push_back({s.turn, target->systemId, fleet.ownerId, target->ownerId, fleet.id,
                0, 0, 0, false});
            fleet.invadeSystemId = 0; continue;
        }
        const int attacker = 8 + fleet.hull + 2 * empire(s, fleet.ownerId).technologies[5].level;
        const int defenderBatteries = s.advancedRulesEnabled ? economy(s, *target).poweredBatteries : 0;
        const int defender = minors::invasionDefense(s, *target, 8 + target->population / 3 + target->morale / 10 + defenderBatteries * 12);
        const std::uint64_t a2 = static_cast<std::uint64_t>(attacker) * attacker;
        const std::uint64_t d2 = static_cast<std::uint64_t>(defender) * defender;
        const bool won = bounded(s.rngState, a2 + d2) < a2;
        const int oldOwner = target->ownerId;
        const int originalPopulation = target->population;
        if (won) {
            target->ownerId = fleet.ownerId; target->population = std::max(1, target->population * 8 / 10);
            target->morale = 35; target->queue.clear(); target->overflow = 0;
            int excess = std::max(0, target->foodWorkers + target->industryWorkers + target->researchWorkers - target->population);
            auto trim = [&excess](int& workers) { const int amount = std::min(workers, excess); workers -= amount; excess -= amount; };
            trim(target->industryWorkers); trim(target->researchWorkers); trim(target->foodWorkers);
            // The previous owner's automation settings do not transfer with the colony.
            target->workforcePriority = GovernorPriority::Manual;
            target->queuePriority = GovernorPriority::Manual;
            target->workforceManualOverride = false;
            reveal(s, loc->x, loc->y, fleet.ownerId);
            for (auto& minor : s.minors) if (minor.systemId == target->systemId) minor.memberOf = fleet.ownerId;
            if (fleet.ownerId == s.playerEmpire || oldOwner == s.playerEmpire)
                report.events.push_back(loc->name + " captured by " + empireName(fleet.ownerId) + ".");
        } else {
            if (fleet.ownerId == s.playerEmpire) report.events.push_back("Invasion of " + loc->name + " failed.");
        }
        report.invasions.push_back({s.turn, target->systemId, fleet.ownerId, oldOwner, fleet.id,
            attacker, defender, originalPopulation - target->population, won});
        fleet.invadeSystemId = 0;
        fleet.hull = 0; // A transport makes one ground assault; surviving troops garrison a captured colony.
    }
    std::erase_if(s.fleets, [](const Fleet& f) { return f.hull <= 0; });
    reconcileTaskForces(s);
    std::erase_if(s.tradeRoutes, [&](const TradeRoute& route) {
        const auto source = colonyAt(s, route.fromSystemId), destination = colonyAt(s, route.toSystemId);
        return !source || !destination || source->ownerId != route.ownerId || destination->ownerId == route.ownerId;
    });
}

std::string constructionTechBlocker(const GameState& state, const Construction& project, int owner) {
    if (project.referenceId < 0) return technologyBlocker(state, project.product, owner);
    const auto& ship = sourceShip(project.referenceId);
    if (!playableShip(ship) || ship.raceId != catalogueRace(owner)) return "Ship class is unavailable to this empire";
    for (int field = 0; field < ResearchFields; ++field) {
        if (empire(state, owner).technologies[field].level < ship.technologyRequirements[field])
            return std::string("Requires ") + researchName(field) + " level " +
                std::to_string(ship.technologyRequirements[field]);
    }
    return {};
}

std::string payload(const GameState& s) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "BOTF_SIM " << SaveVersion << ' ' << ContentVersion << '\n';
    out << "STATE " << s.seed << ' ' << s.rngState << ' ' << s.lastAcceptedCommand << ' '
        << s.width << ' ' << s.height << ' ' << s.turn << ' ' << s.credits << ' ' << s.research << '\n';
    out << "NEXT_FLEET " << s.nextFleetId << '\n';
    out << "POPULATION " << s.populationEnabled << '\n';
    out << "RESEARCH " << s.researchReserve;
    for (const auto& tech : s.technologies) out << ' ' << tech.level << ' ' << tech.progress << ' ' << tech.allocation;
    out << ' ' << s.intelligence << '\n';
    out << "RIVALS " << s.rivals.size() << '\n';
    for (const auto& e : s.rivals) {
        out << e.id << ' ' << e.credits << ' ' << e.research << ' ' << e.researchReserve;
        for (const auto& tech : e.technologies) out << ' ' << tech.level << ' ' << tech.progress << ' ' << tech.allocation;
        out << ' ' << e.explored.size(); for (int cell : e.explored) out << ' ' << cell; out << ' ' << e.intelligence << '\n';
    }
    out << "MINORS " << s.nextDiplomacyId << ' ' << s.minors.size() << '\n';
    for (const auto& m : s.minors) {
        out << m.systemId << ' ' << std::quoted(m.name) << ' ' << m.memberOf;
        for (int i = 0; i < 6; ++i) out << ' ' << m.contact[i] << ' ' << m.attitude[i] << ' ' << m.friendshipTurn[i];
        out << '\n';
    }
    out << "DIPLOMACY " << s.diplomacy.size() << '\n';
    for (const auto& offer : s.diplomacy)
        out << offer.id << ' ' << offer.empireId << ' ' << offer.systemId << ' ' << offer.dueTurn << ' ' << static_cast<int>(offer.type) << '\n';
    out << "CAMPAIGN " << static_cast<int>(s.outcome) << ' ' << s.winnerEmpire << ' ' << s.eventsEnabled << ' ' << s.aiAggressionEnabled << ' ' << s.lastRandomEventTurn << '\n';
    out << "RULESET " << static_cast<int>(s.ruleset) << '\n';
    out << "ADVANCED " << s.advancedRulesEnabled << '\n';
    out << "EXPANSION " << s.campaignExpansionEnabled << '\n';
    out << "MAJOROFFERS " << s.nextMajorOfferId << ' ' << s.pendingMajorOffers.size() << '\n';
    for (const auto& offer : s.pendingMajorOffers)
        out << offer.id << ' ' << offer.proposer << ' ' << offer.target << ' ' << offer.dueTurn << ' ' << static_cast<int>(offer.type) << '\n';
    out << "RELATIONS " << s.majorRelations.size() << '\n';
    for (const auto& r : s.majorRelations)
        out << r.first << ' ' << r.second << ' ' << r.contact << ' ' << r.atWar << ' ' << r.tradeTreaty
            << ' ' << r.warStartTurn << ' ' << r.trust << '\n';
    out << "TRADE " << s.tradeRoutes.size() << '\n';
    for (const auto& r : s.tradeRoutes) out << r.ownerId << ' ' << r.fromSystemId << ' ' << r.toSystemId << '\n';
    out << "INTELREPORTS " << s.intelReports.size() << '\n';
    for (const auto& r : s.intelReports)
        out << r.turn << ' ' << r.viewer << ' ' << r.target << ' ' << r.systemId << ' ' << r.success << ' '
            << r.knownColonies << ' ' << r.estimatedCredits << '\n';
    out << "BATTLES " << s.latestBattles.size() << '\n';
    for (const auto& b : s.latestBattles) {
        out << b.turn << ' ' << b.x << ' ' << b.y << ' ' << b.firstOwner << ' ' << b.secondOwner << ' ' << b.winnerOwner;
        auto ids = [&](const std::vector<int>& values) { out << ' ' << values.size(); for (int id : values) out << ' ' << id; };
        ids(b.firstFleetIds); ids(b.secondFleetIds); ids(b.lostFleetIds); ids(b.retreatedFleetIds);
        out << ' ' << b.damage.size(); for (const auto& d : b.damage)
            out << ' ' << d.fleetId << ' ' << d.hullBefore << ' ' << d.hullAfter << ' ' << d.shieldBefore << ' ' << d.shieldAfter;
        out << ' ' << b.participants.size();
        for (const auto& p : b.participants)
            out << ' ' << p.fleetId << ' ' << p.ownerId << ' ' << static_cast<int>(p.shipClass) << ' '
                << p.referenceShipId << ' ' << p.initialHull << ' ' << p.initialShields;
        out << ' ' << b.rounds.size();
        for (const auto& round : b.rounds) {
            out << ' ' << round.index << ' ' << round.damage.size();
            for (const auto& d : round.damage)
                out << ' ' << d.fleetId << ' ' << d.hullBefore << ' ' << d.hullAfter << ' ' << d.shieldBefore << ' ' << d.shieldAfter;
            ids(round.lostFleetIds); ids(round.retreatedFleetIds);
        }
        out << '\n';
    }
    out << "SYSTEMS " << s.systems.size() << '\n';
    for (const auto& x : s.systems)
        out << x.id << ' ' << x.x << ' ' << x.y << ' ' << std::quoted(x.name) << '\n';
    
    int totalPlanets = 0;
    for (const auto& x : s.systems) totalPlanets += static_cast<int>(x.planets.size());
    out << "PLANETS " << totalPlanets << '\n';
    for (const auto& x : s.systems) {
        for (const auto& p : x.planets) {
            out << p.systemId << ' ' << p.index << ' ' << std::quoted(p.name) << ' '
                << static_cast<int>(p.planetClass) << ' ' << static_cast<int>(p.terraformState) << ' '
                << p.terraformWork << ' ' << p.terraformNeeded << ' '
                << p.baseCapacity << ' ' << p.maxCapacity << '\n';
        }
    }

    out << "COLONIES " << s.colonies.size() << '\n';
    for (const auto& c : s.colonies) {
        out << c.systemId << ' ' << c.ownerId << ' ' << c.population << ' ' << c.foodWorkers << ' '
            << c.industryWorkers << ' ' << c.researchWorkers << ' ' << c.farms << ' '
            << c.factories << ' ' << c.laboratories << ' ' << c.energy << ' ' << c.overflow
            << ' ' << c.shipyards << ' ' << c.shipyardEnabled << ' ' << c.powerPlants << ' ' << c.queue.size();
        for (auto b : c.queue) out << ' ' << static_cast<int>(b.product) << ' ' << b.work << ' ' << b.referenceId;
        out << ' ' << c.morale << ' ' << c.defenseBatteries << '\n';
    }
    out << "EXPLORATION " << s.playerEmpire << ' ' << s.explored.size();
    for (int cell : s.explored) out << ' ' << cell;
    out << '\n' << "FLEETS " << s.fleets.size() << '\n';
    for (const auto& f : s.fleets)
        out << f.id << ' ' << f.ownerId << ' ' << f.x << ' ' << f.y << ' ' << f.targetX << ' '
            << f.targetY << ' ' << f.colonyShip << ' ' << f.colonize << ' ' << static_cast<int>(f.shipClass)
            << ' ' << f.hull << ' ' << f.attackTarget << ' ' << f.retreat << ' ' << f.invadeSystemId << ' '
            << f.referenceShipId << ' ' << f.shields << ' ' << static_cast<int>(f.stance) << ' '
            << f.taskForceId << ' ' << f.terraforming << ' ' << f.terraformSystemId << ' ' << f.terraformPlanetIndex << '\n';
    out << "PENDING " << s.pending.size() << '\n';
    for (const auto& c : s.pending) {
        out << c.id << ' ' << static_cast<int>(c.type) << ' ' << c.systemId << ' '
            << c.foodWorkers << ' ' << c.industryWorkers << ' ' << c.researchWorkers
            << ' ' << static_cast<int>(c.building) << ' ' << c.fleetId << ' ' << c.targetX << ' ' << c.targetY
            << ' ' << c.queueIndex << ' ' << c.queueDestination;
        for (int amount : c.researchAllocation) out << ' ' << amount;
        out << ' ' << c.targetEmpire << ' ' << c.targetFleetId << ' ' << c.targetSystemId << ' ' << c.referenceId
            << ' ' << static_cast<int>(c.stance) << ' '
            << c.taskForceId << ' ' << c.planetIndex << ' ' << c.memberFleetIds.size();
        for (int mfid : c.memberFleetIds) out << ' ' << mfid;
        out << ' ' << std::quoted(c.taskForceName)
            << ' ' << static_cast<int>(c.proposalTreatyType)
            << ' ' << static_cast<int>(c.proposalDuration)
            << ' ' << static_cast<int>(c.proposalTone)
            << ' ' << c.proposalCreditOffer
            << ' ' << c.proposalCreditDemand
            << ' ' << (c.proposalTargetIsMinor ? 1 : 0)
            << ' ' << c.proposalId
            << ' ' << (c.proposalAccept ? 1 : 0)
            << ' ' << std::quoted(c.proposalReason)
            << ' ' << c.intelInternalSecurity
            << ' ' << static_cast<int>(c.intelDepartmentFocus)
            << ' ' << c.intelRivalIds.size();
        for (std::size_t m = 0; m < c.intelRivalIds.size(); ++m) {
            out << ' ' << c.intelRivalIds[m] << ' ' << c.intelEspionagePcts[m] << ' ' << c.intelSabotagePcts[m];
        }
        if (colonyGovernorCommand(c.type)) {
            out << ' ' << static_cast<int>(c.governorWorkforce)
                << ' ' << static_cast<int>(c.governorQueue)
                << ' ' << (c.queuePinned ? 1 : 0);
        }
        gameplay::writeCommandTail(out, c); // Schema-13 commands only.
        out << '\n';
    }
    out << "TASKFORCES " << s.nextTaskForceId << ' ' << s.taskForces.size() << '\n';
    for (const auto& tf : s.taskForces) {
        out << tf.id << ' ' << tf.ownerId << ' ' << std::quoted(tf.name) << ' '
            << static_cast<int>(tf.stance) << ' ' << tf.targetX << ' ' << tf.targetY << ' '
            << tf.attackTarget << ' ' << tf.fleetIds.size();
        for (int fid : tf.fleetIds) out << ' ' << fid;
        out << '\n';
    }
    if (s.campaignExpansionEnabled && s.modernEnginesActive) {
        out << "DIPLOMACY_ENGINE\n";
        out << diplomacy::serializeDiplomacyState(s.diplomacyEngineState);
        out << "INTEL_ENGINE " << s.empireIntelStates.size() << '\n';
        for (const auto& [empId, intelState] : s.empireIntelStates) {
            out << intelligence::serializeIntelState(intelState);
        }
        out << "INTEL_OPERATIONS " << s.intelOperations.size() << '\n';
        for (const auto& op : s.intelOperations) {
            out << intelligence::serializeOperationRecord(op) << '\n';
        }
    }
    out << "STARTING_ERA " << static_cast<int>(s.startingEra) << '\n';
    // Schema 12 extension: combat model and per-colony fusion plants (only nonzero entries).
    int fusionColonies = 0;
    for (const auto& c : s.colonies) if (c.fusionPlants > 0) ++fusionColonies;
    out << "REMASTER " << s.unifiedCombat << ' ' << fusionColonies;
    for (const auto& c : s.colonies) if (c.fusionPlants > 0) out << ' ' << c.systemId << ' ' << c.fusionPlants;
    out << '\n';
    out << "COLONY_GOVERNORS " << s.colonies.size() << '\n';
    for (const auto& c : s.colonies) {
        int pinnedCount = 0;
        for (std::size_t idx = 0; idx < c.queue.size(); ++idx) {
            if (c.queue[idx].pinned) ++pinnedCount;
        }
        out << c.systemId << ' ' << static_cast<int>(c.workforcePriority) << ' '
            << static_cast<int>(c.queuePriority) << ' ' << (c.workforceManualOverride ? 1 : 0)
            << ' ' << pinnedCount;
        for (std::size_t idx = 0; idx < c.queue.size(); ++idx) {
            if (c.queue[idx].pinned) out << ' ' << idx;
        }
        out << '\n';
    }
    out << gameplay::serializeSection(s); // Schema 13 (GAMEPLAY_LAYER ... END_GAMEPLAY_LAYER).
    minors::writeMinorRacesSection(out, s); // Lane G: optional MINOR_RACES section (roster minors only)
    return out.str();
}

void tag(std::istream& in, const char* expected) {
    std::string actual;
    require(static_cast<bool>(in >> actual) && actual == expected, "Malformed save section");
}
int count(std::istream& in, int maximum) {
    int value = -1;
    require(static_cast<bool>(in >> value) && value >= 0 && value <= maximum,
        "Invalid save collection size");
    return value;
}
} // namespace

void resolveTacticalBattles(GameState& state, TurnReport& report,
                            const std::function<void(tactical::Encounter&)>& playerEncounter) {
    std::set<int> withdrawn; // Ships that left a battle this turn fight no further duels.
    std::vector<CombatReport> battles;
    for (int y = 0; y < state.height; ++y) for (int x = 0; x < state.width; ++x) {
        for (std::size_t index = 0; index < state.majorRelations.size(); ++index) {
            const MajorRelation relation = state.majorRelations[index];
            if (!relation.atWar) continue;
            std::vector<int> ids;
            bool first = false, second = false;
            for (const auto& fleet : state.fleets) {
                if (fleet.hull <= 0 || fleet.x != x || fleet.y != y || withdrawn.count(fleet.id)) continue;
                if (fleet.ownerId != relation.first && fleet.ownerId != relation.second) continue;
                ids.push_back(fleet.id);
                first |= fleet.ownerId == relation.first;
                second |= fleet.ownerId == relation.second;
            }
            // Schema 13: an outpost or starbase fights for its owner as an immobile unit.
            const Station* station = gameplay::combatStation(state, x, y, relation.first, relation.second);
            if (station) {
                first |= station->ownerId == relation.first;
                second |= station->ownerId == relation.second;
            }
            if (!first || !second || ids.empty()) continue;
            const auto seed = state.rngState ^ (static_cast<std::uint64_t>(state.turn) << 32)
                ^ static_cast<std::uint64_t>(y * state.width + x + 1)
                ^ (static_cast<std::uint64_t>(relation.first * 7 + relation.second) << 16);
            auto encounter = station ? gameplay::constructStationEncounter(state, ids, seed, *station)
                                     : tactical::constructEncounter(state, ids, seed);
            const bool playerInvolved = relation.first == state.playerEmpire || relation.second == state.playerEmpire;
            if (playerInvolved && playerEncounter) playerEncounter(encounter);
            else encounter.runUntilFinished();
            require(encounter.isFinished, "Tactical encounter did not finish");
            if (station) gameplay::applyStationEncounter(state, encounter);
            else tactical::applyEncounterResults(state, encounter);
            for (const auto& ship : encounter.ships) if (ship.retreated) withdrawn.insert(ship.fleetId);
            reconcileTaskForces(state); // Destroyed and independently withdrawing ships leave task forces.
            auto battle = tactical::toCombatReport(encounter);
            if (playerInvolved)
                report.events.push_back("Battle at sector " + std::to_string(x) + "," + std::to_string(y) +
                    ": " + std::to_string(battle.lostFleetIds.size()) + " ships lost, " +
                    std::to_string(battle.retreatedFleetIds.size()) + " withdrew" +
                    (encounter.rounds.empty() || !encounter.rounds.back().hailedTruce ? "." : " under a hailed truce."));
            battles.push_back(std::move(battle));
        }
    }
    // Strategic retreats end on arrival, exactly as in the legacy resolver.
    for (auto& fleet : state.fleets)
        if (fleet.x == fleet.targetX && fleet.y == fleet.targetY) fleet.retreat = false;
    reconcileTaskForces(state);
    report.battles.insert(report.battles.end(), battles.begin(), battles.end());
    state.latestBattles.insert(state.latestBattles.end(), battles.begin(), battles.end());
    if (state.latestBattles.size() > 16) state.latestBattles.erase(state.latestBattles.begin(), state.latestBattles.end() - 16);
}

const Planet* planetAt(const GameState& state, int systemId, int planetIndex) {
    const auto it = std::find_if(state.systems.begin(), state.systems.end(), [systemId](const System& s) { return s.id == systemId; });
    if (it == state.systems.end()) return nullptr;
    if (planetIndex < 0 || static_cast<std::size_t>(planetIndex) >= it->planets.size()) return nullptr;
    return &it->planets[static_cast<std::size_t>(planetIndex)];
}
const std::vector<Planet>& systemPlanets(const GameState& state, int systemId) {
    static const std::vector<Planet> empty;
    const auto it = std::find_if(state.systems.begin(), state.systems.end(), [systemId](const System& s) { return s.id == systemId; });
    return it == state.systems.end() ? empty : it->planets;
}
int systemCapacity(const GameState& state, int systemId) {
    if (!state.campaignExpansionEnabled) return PrototypePopulationCapacity;
    const auto it = std::find_if(state.systems.begin(), state.systems.end(), [systemId](const System& s) { return s.id == systemId; });
    if (it == state.systems.end() || it->planets.empty()) return PrototypePopulationCapacity;
    int total = 0;
    for (const auto& p : it->planets) total += p.currentCapacity();
    return std::max(20, total);
}
int systemBaseCapacity(const GameState& state, int systemId) {
    if (!state.campaignExpansionEnabled) return PrototypePopulationCapacity;
    const auto it = std::find_if(state.systems.begin(), state.systems.end(), [systemId](const System& s) { return s.id == systemId; });
    if (it == state.systems.end() || it->planets.empty()) return PrototypePopulationCapacity;
    int total = 0;
    for (const auto& p : it->planets) total += p.baseCapacity;
    return std::max(20, total);
}
int systemMaxCapacity(const GameState& state, int systemId) {
    if (!state.campaignExpansionEnabled) return PrototypePopulationCapacity;
    const auto it = std::find_if(state.systems.begin(), state.systems.end(), [systemId](const System& s) { return s.id == systemId; });
    if (it == state.systems.end() || it->planets.empty()) return PrototypePopulationCapacity;
    int total = 0;
    for (const auto& p : it->planets) total += p.maxCapacity;
    return std::max(20, total);
}
bool canTerraformPlanet(const GameState& state, int fleetId, int systemId, int planetIndex) {
    if (!state.campaignExpansionEnabled) return false;
    const auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [fleetId](const Fleet& x) { return x.id == fleetId; });
    if (f == state.fleets.end() || !f->colonyShip || f->hull <= 0 || f->retreat || f->attackTarget != 0 || f->colonize) return false;
    const auto col = colonyAt(state, systemId);
    if (col && col->ownerId != f->ownerId) return false;
    const auto sys = std::find_if(state.systems.begin(), state.systems.end(), [systemId](const System& s) { return s.id == systemId; });
    if (sys == state.systems.end() || f->x != sys->x || f->y != sys->y) return false;
    if (!known(state, f->ownerId, sys->x, sys->y)) return false;
    if (planetIndex < 0 || static_cast<std::size_t>(planetIndex) >= sys->planets.size()) return false;
    const auto& p = sys->planets[static_cast<std::size_t>(planetIndex)];
    return p.planetClass != PlanetClass::GasGiant && p.planetClass != PlanetClass::Asteroids &&
           p.terraformState != TerraformingState::Terraformed && p.terraformState != TerraformingState::Uninhabitable;
}
const char* planetClassName(PlanetClass pc) {
    switch (pc) {
    case PlanetClass::ClassM: return "Class M (Terran)";
    case PlanetClass::ClassO: return "Class O (Oceanic)";
    case PlanetClass::ClassP: return "Class P (Glacial)";
    case PlanetClass::ClassL: return "Class L (Jungle)";
    case PlanetClass::ClassH: return "Class H (Desert)";
    case PlanetClass::ClassK: return "Class K (Barren)";
    case PlanetClass::ClassY: return "Class Y (Demon)";
    case PlanetClass::GasGiant: return "Gas Giant";
    case PlanetClass::Asteroids: return "Asteroids";
    default: return "Unknown";
    }
}
const char* terraformingStateName(TerraformingState ts) {
    switch (ts) {
    case TerraformingState::Uninhabitable: return "Uninhabitable";
    case TerraformingState::Untouched: return "Untouched";
    case TerraformingState::InProgress: return "In Progress";
    case TerraformingState::Terraformed: return "Terraformed";
    default: return "Unknown";
    }
}

const TaskForce* taskForceAt(const GameState& state, int taskForceId) {
    const auto it = std::find_if(state.taskForces.begin(), state.taskForces.end(), [taskForceId](const TaskForce& tf) { return tf.id == taskForceId; });
    return it == state.taskForces.end() ? nullptr : &*it;
}
std::vector<TaskForce> empireTaskForces(const GameState& state, int ownerId) {
    std::vector<TaskForce> list;
    for (const auto& tf : state.taskForces) if (tf.ownerId == ownerId) list.push_back(tf);
    return list;
}
const TaskForce* fleetTaskForce(const GameState& state, int fleetId) {
    const auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [fleetId](const Fleet& x) { return x.id == fleetId; });
    if (f == state.fleets.end() || f->taskForceId <= 0) return nullptr;
    return taskForceAt(state, f->taskForceId);
}
std::vector<Fleet> taskForceFleets(const GameState& state, int taskForceId) {
    std::vector<Fleet> members;
    const auto tf = taskForceAt(state, taskForceId);
    if (!tf) return members;
    for (int fid : tf->fleetIds) {
        const auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
        if (f != state.fleets.end()) members.push_back(*f);
    }
    return members;
}
int taskForceSpeed(const GameState& state, int taskForceId) {
    const auto tf = taskForceAt(state, taskForceId);
    if (!tf || tf->fleetIds.empty()) return 1;
    int minSpd = 3;
    for (int fid : tf->fleetIds) {
        const auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
        if (f == state.fleets.end()) continue;
        const int spd = f->referenceShipId >= 0 ? std::clamp(static_cast<int>(sourceShip(f->referenceShipId).speed), 1, 3) : 1;
        minSpd = std::min(minSpd, spd);
    }
    return minSpd;
}
int taskForceAttack(const GameState& state, int taskForceId) {
    const auto tf = taskForceAt(state, taskForceId);
    if (!tf) return 0;
    int total = 0;
    for (int fid : tf->fleetIds) {
        const auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
        if (f != state.fleets.end()) total += fleetAttack(*f);
    }
    return total;
}
int taskForceHull(const GameState& state, int taskForceId) {
    const auto tf = taskForceAt(state, taskForceId);
    if (!tf) return 0;
    int total = 0;
    for (int fid : tf->fleetIds) {
        const auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
        if (f != state.fleets.end()) total += f->hull;
    }
    return total;
}
int taskForceShields(const GameState& state, int taskForceId) {
    const auto tf = taskForceAt(state, taskForceId);
    if (!tf) return 0;
    int total = 0;
    for (int fid : tf->fleetIds) {
        const auto f = std::find_if(state.fleets.begin(), state.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
        if (f != state.fleets.end()) total += f->shields;
    }
    return total;
}

Command makeTerraformCommand(std::uint64_t id, int fleetId, int systemId, int planetIndex) {
    Command c;
    c.id = id;
    c.type = CommandType::TerraformPlanet;
    c.fleetId = fleetId;
    c.systemId = systemId;
    c.planetIndex = planetIndex;
    return c;
}
Command makeCreateTaskForceCommand(std::uint64_t id, const std::vector<int>& fleetIds, const std::string& name) {
    Command c;
    c.id = id;
    c.type = CommandType::CreateTaskForce;
    c.fleetId = fleetIds.empty() ? 0 : fleetIds.front();
    c.memberFleetIds = fleetIds;
    c.taskForceName = name;
    return c;
}
Command makeDisbandTaskForceCommand(std::uint64_t id, int taskForceId) {
    Command c;
    c.id = id;
    c.type = CommandType::DisbandTaskForce;
    c.taskForceId = taskForceId;
    return c;
}
Command makeTaskForceOrderCommand(std::uint64_t id, int taskForceId, int targetX, int targetY, TacticalStance stance, int targetFleetId) {
    Command c;
    c.id = id;
    c.type = CommandType::SetTaskForceOrder;
    c.taskForceId = taskForceId;
    c.targetX = targetX;
    c.targetY = targetY;
    c.stance = stance;
    c.targetFleetId = targetFleetId;
    return c;
}

Command makeBuyConstructionCommand(std::uint64_t id, int systemId) {
    Command c;
    c.id = id;
    c.type = CommandType::BuyConstruction;
    c.systemId = systemId;
    return c;
}

Command makeScrapStructureCommand(std::uint64_t id, int systemId, Building structure) {
    Command c;
    c.id = id;
    c.type = CommandType::ScrapStructure;
    c.systemId = systemId;
    c.building = structure;
    return c;
}

int rushCost(const GameState& state, int systemId) {
    const auto colony = colonyAt(state, systemId);
    if (!colony || colony->queue.empty()) return 0;
    const auto& project = colony->queue.front();
    const int total = constructionCost(project);
    const int remaining = std::max(0, total - project.work);
    // PROVISIONAL: 2 credits per remaining industry unit
    return remaining * 2;
}

const Minor* minorAt(const GameState& s, int systemId) {
    const auto it = std::find_if(s.minors.begin(), s.minors.end(), [systemId](const Minor& m) { return m.systemId == systemId; });
    return it == s.minors.end() ? nullptr : &*it;
}
const MajorRelation* majorRelation(const GameState& s, int first, int second) {
    if (first > second) std::swap(first, second);
    const auto found = std::find_if(s.majorRelations.begin(), s.majorRelations.end(), [&](const MajorRelation& r) {
        return r.first == first && r.second == second;
    });
    return found == s.majorRelations.end() ? nullptr : &*found;
}
const char* shipName(Building shipClass) {
    require(isShip(shipClass), "Unknown ship class");
    return definition(shipClass).name;
}
int shipMaxHull(Building shipClass) {
    switch (shipClass) {
    case Building::Scout: return 12;
    case Building::ColonyShip: return 8;
    case Building::Destroyer: return 24;
    case Building::Transport: return 14;
    default: throw std::runtime_error("Unknown ship class");
    }
}
int shipAttack(Building shipClass) {
    switch (shipClass) {
    case Building::Scout: return 3;
    case Building::Destroyer: return 8;
    case Building::Transport: return 2;
    case Building::ColonyShip: return 0;
    default: throw std::runtime_error("Unknown ship class");
    }
}
std::string fleetName(const Fleet& fleet) {
    if (fleet.referenceShipId < 0) return shipName(fleet.shipClass);
    const auto& ship = sourceShip(fleet.referenceShipId);
    return std::string(ship.shipClass) + " — " + ship.name;
}
int fleetMaxHull(const Fleet& fleet) {
    return fleet.referenceShipId < 0 ? shipMaxHull(fleet.shipClass) : sourceShip(fleet.referenceShipId).hull;
}
int fleetAttack(const Fleet& fleet) {
    if (fleet.referenceShipId < 0) return shipAttack(fleet.shipClass);
    const int strength = sourceShip(fleet.referenceShipId).attack;
    return strength <= 0 ? 0 : std::max(1, (strength + 9) / 20);
}
int fleetMaxShields(const Fleet& fleet) {
    return fleet.referenceShipId < 0 ? 0 : std::max(0, sourceShip(fleet.referenceShipId).shields);
}
const EmpireState& empire(const GameState& s, int id) {
    if (id == s.playerEmpire) return s;
    const auto found = std::find_if(s.rivals.begin(), s.rivals.end(), [id](const EmpireState& e) { return e.id == id; });
    require(found != s.rivals.end(), "Empire does not exist"); return *found;
}
const char* empireName(int id) {
    static constexpr const char* Names[] = {"", "Federation", "Klingon Empire", "Romulan Star Empire", "Cardassian Union", "Ferengi Alliance"};
    require(id >= 1 && id <= 5, "Invalid empire ID"); return Names[id];
}
const BuildingDefinition& definition(Building value) {
    require(buildingValid(value), "Unknown building definition");
    return Definitions[static_cast<std::size_t>(value)];
}
const char* startingEraName(StartingEra era) {
    switch (era) {
    case StartingEra::Early: return "Early";
    case StartingEra::Developed: return "Developed";
    case StartingEra::Expanded: return "Expanded";
    case StartingEra::Advanced: return "Advanced";
    case StartingEra::Supreme: return "Supreme";
    default: return "Unknown";
    }
}
bool startingEraValid(StartingEra era) {
    return era == StartingEra::Early || era == StartingEra::Developed ||
           era == StartingEra::Expanded || era == StartingEra::Advanced ||
           era == StartingEra::Supreme;
}
int eraBaseTechLevel(StartingEra era, Ruleset ruleset) {
    switch (era) {
    case StartingEra::Early: return ruleset == Ruleset::Reference ? 1 : 0;
    case StartingEra::Developed: return 3;
    case StartingEra::Expanded: return 5;
    case StartingEra::Advanced: return 7;
    case StartingEra::Supreme: return 10;
    default: return ruleset == Ruleset::Reference ? 1 : 0;
    }
}
std::vector<StartingEra> availableStartingEras() {
    return {StartingEra::Early, StartingEra::Developed, StartingEra::Expanded, StartingEra::Advanced, StartingEra::Supreme};
}
StartingEra campaignStartingEra(const GameState& state) {
    return state.startingEra;
}
const char* researchName(int field) {
    static constexpr std::array<const char*, ResearchFields> Names{"Biotech", "Computers", "Construction", "Energy", "Propulsion", "Weapons"};
    require(field >= 0 && field < ResearchFields, "Invalid research field");
    return Names[field];
}
int researchCost(int nextLevel) {
    require(nextLevel >= 1 && nextLevel <= MaxTechLevel, "Invalid technology level");
    return 100 * nextLevel * (nextLevel + 1);
}
int researchCost(const GameState& state, int field, int nextLevel) {
    require(field >= 0 && field < ResearchFields && nextLevel >= 1 && nextLevel <= MaxTechLevel,
        "Invalid research catalogue position");
    if (state.ruleset == Ruleset::Prototype) return researchCost(nextLevel);
    const auto& entry = referenceData::TechnologyCatalogue[static_cast<std::size_t>(field * 11 + nextLevel)];
    require(entry.field == field && entry.level == nextLevel && entry.cost >= 0 &&
        entry.cost <= std::numeric_limits<int>::max(), "Invalid sourced research cost");
    return static_cast<int>(entry.cost);
}
std::string technologyName(const GameState& state, int field, int level) {
    require(field >= 0 && field < ResearchFields && level >= 0 && level <= MaxTechLevel,
        "Invalid technology position");
    if (state.ruleset == Ruleset::Prototype) return std::string(researchName(field)) + " level " + std::to_string(level);
    const auto& entry = referenceData::TechnologyCatalogue[static_cast<std::size_t>(field * 11 + level)];
    require(entry.field == field && entry.level == level, "Invalid sourced technology name");
    return entry.name;
}
std::string technologyBlocker(const GameState& s, Building b, int owner) {
    const auto& tech = empire(s, owner == 0 ? s.playerEmpire : owner).technologies;
    if (b == Building::FusionPlant && tech[3].level < 1) return "Requires Energy level 1";
    if (b == Building::ResearchInstitute && (tech[1].level < 1 || tech[2].level < 1))
        return "Requires Computers and Construction level 1";
    if (b == Building::Destroyer && (tech[4].level < 1 || tech[5].level < 1))
        return "Requires Propulsion and Weapons level 1";
    if (b == Building::Transport && tech[4].level < 1) return "Requires Propulsion level 1";
    return {};
}
std::vector<ConstructionChoice> constructionChoices(const GameState& state, int systemId) {
    const auto colony = colonyAt(state, systemId);
    require(colony && colony->ownerId == state.playerEmpire, "Construction list needs an owned colony");
    const bool queuedYard = std::any_of(colony->queue.begin(), colony->queue.end(), [](const Construction& project) {
        return project.product == Building::Shipyard;
    });
    const bool yardAvailable = colony->shipyards || queuedYard;
    const bool queueFull = colony->queue.size() >= MaxQueue;
    std::vector<ConstructionChoice> choices;
    for (Building b : {Building::Farm, Building::Factory, Building::Laboratory, Building::PowerPlant,
        Building::FusionPlant, Building::ResearchInstitute, Building::Shipyard, Building::DefenseBattery}) {
        std::string blocker = technologyBlocker(state, b, colony->ownerId);
        if (b == Building::DefenseBattery && !state.advancedRulesEnabled) blocker = "Available in new reference campaigns";
        if (b == Building::DefenseBattery && colony->defenseBatteries >= 10) blocker = "Defense batteries are at capacity";
        if (b == Building::Shipyard && yardAvailable) blocker = "Shipyard already built or queued";
        if (queueFull) blocker = "Construction queue is full";
        const auto& def = definition(b);
        choices.push_back({b, -1, std::string(def.name) + (state.ruleset == Ruleset::Reference ? " (remake infrastructure)" : ""),
            def.industryCost, def.creditUpkeep, blocker});
    }
    if (state.ruleset == Ruleset::Prototype) {
        for (Building b : {Building::Scout, Building::ColonyShip, Building::Destroyer, Building::Transport}) {
            auto blocker = technologyBlocker(state, b, colony->ownerId);
            if (!yardAvailable && blocker.empty()) blocker = "Build or queue a shipyard first";
            if (queueFull) blocker = "Construction queue is full";
            const auto& def = definition(b);
            choices.push_back({b, -1, def.name, def.industryCost, def.creditUpkeep, blocker});
        }
    } else {
        for (const auto& ship : referenceData::ShipCatalogue) {
            if (!playableShip(ship) || ship.raceId != catalogueRace(colony->ownerId)) continue;
            const auto product = roleProduct(ship.role);
            Construction project(product, 0, ship.id);
            auto blocker = constructionTechBlocker(state, project, colony->ownerId);
            if (!yardAvailable && blocker.empty()) blocker = "Build or queue a shipyard first";
            if (queueFull) blocker = "Construction queue is full";
            choices.push_back({product, ship.id, std::string(ship.shipClass) + " — " + ship.name,
                ship.industryCost, ship.maintenance, blocker});
        }
    }
    return choices;
}
bool explored(const GameState& s, int x, int y) {
    return x >= 0 && y >= 0 && x < s.width && y < s.height &&
        std::binary_search(s.explored.begin(), s.explored.end(), y * s.width + x);
}
bool visible(const GameState& s, int owner, int x, int y) {
    if (x < 0 || y < 0 || x >= s.width || y >= s.height) return false;
    // Schema 13: station sensors and nebulae (sensor-blind) change coverage.
    if (gameplay::layerActive(s)) return gameplay::extendedVisible(s, owner, x, y);
    for (const auto& f : s.fleets)
        if (f.ownerId == owner && std::max(std::abs(f.x - x), std::abs(f.y - y)) <= 1) return true;
    for (const auto& c : s.colonies) {
        if (c.ownerId != owner) continue;
        const auto system = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& a) { return a.id == c.systemId; });
        if (system != s.systems.end() && std::max(std::abs(system->x - x), std::abs(system->y - y)) <= 1) return true;
    }
    return false;
}
const Colony* colonyAt(const GameState& s, int id) {
    const auto it = std::find_if(s.colonies.begin(), s.colonies.end(),
        [&](const Colony& c) { return c.systemId == id; });
    return it == s.colonies.end() ? nullptr : &*it;
}
bool isShip(Building b) { return b == Building::Scout || b == Building::ColonyShip || b == Building::Destroyer || b == Building::Transport; }
int shipRange(const Fleet& f) {
    if (f.referenceShipId >= 0) return 4 + std::clamp(static_cast<int>(sourceShip(f.referenceShipId).range), 0, 2);
    return f.colonyShip || f.shipClass == Building::Transport ? 4 : 6;
}
namespace {
bool colonySuppliedWithRangeImpl(const GameState& s, int owner, int range, int x, int y) {
    if (x < 0 || y < 0 || x >= s.width || y >= s.height) return false;
    for (const auto& c : s.colonies) {
        if (c.ownerId != owner || !economy(s, c).shipyardPowered) continue;
        const auto origin = std::find_if(s.systems.begin(), s.systems.end(),
            [&](const System& system) { return system.id == c.systemId; });
        if (origin != s.systems.end() && std::max(std::abs(origin->x - x), std::abs(origin->y - y)) <= range + empire(s, owner).technologies[4].level / 2) return true;
    }
    return false;
}
} // namespace
bool supplied(const GameState& s, const Fleet& f, int x, int y) {
    if (colonySuppliedWithRangeImpl(s, f.ownerId, shipRange(f), x, y)) return true;
    // Schema 13: stations and known wormholes extend supply.
    return gameplay::layerActive(s) && gameplay::extendedSupply(s, f.ownerId, x, y, [&](int px, int py) {
        return colonySuppliedWithRangeImpl(s, f.ownerId, shipRange(f), px, py);
    });
}
std::vector<int> route(const GameState& s, const Fleet& f, int x, int y) {
    if (x < 0 || y < 0 || x >= s.width || y >= s.height) return {};
    const int start = f.y * s.width + f.x, goal = y * s.width + x;
    if (start == goal) return {start};
    if (!supplied(s, f, x, y)) return {};
    // Schema 13: impassable anomalies, hazard avoidance and wormhole links.
    if (!s.galaxyFeatures.empty())
        return gameplay::findPath(s, f.ownerId, start, goal, [&](int px, int py) { return supplied(s, f, px, py); });
    std::vector<int> previous(static_cast<std::size_t>(s.width * s.height), -1);
    std::queue<int> open; open.push(start); previous[start] = start;
    while (!open.empty() && previous[goal] == -1) {
        const int cell = open.front(); open.pop();
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
            if (!dx && !dy) continue;
            const int nx = cell % s.width + dx, ny = cell / s.width + dy;
            if (!supplied(s, f, nx, ny)) continue;
            const int next = ny * s.width + nx;
            if (previous[next] != -1) continue;
            previous[next] = cell; open.push(next);
        }
    }
    if (previous[goal] == -1) return {};
    std::vector<int> path;
    for (int cell = goal; cell != start; cell = previous[cell]) path.push_back(cell);
    path.push_back(start); std::reverse(path.begin(), path.end()); return path;
}
std::string constructionBlocker(const GameState& s, const Colony& c) {
    if (c.queue.empty()) return {};
    if (const auto blocked = constructionTechBlocker(s, c.queue.front(), c.ownerId); !blocked.empty()) return blocked;
    if (isShip(c.queue.front())) {
        if (!economy(s, c).shipyardPowered) return "Shipyard is unpowered";
        if (s.fleets.size() >= 256 || s.nextFleetId > 1000000) return "Prototype fleet limit reached";
        for (const auto& other : s.colonies) {
            if (other.ownerId != c.ownerId) continue;
            if (other.systemId >= c.systemId) break;
            if (!other.queue.empty() && isShip(other.queue.front()) && economy(s, other).shipyardPowered && economy(s, other).industry > 0)
                return "Empire shipbuilding slot used by system " + std::to_string(other.systemId);
        }
    }
    if (economy(s, c).industry == 0) return "No industry workers with powered facilities";
    return {};
}

GameState generate(std::uint64_t seed, int width, int height, int total) {
    require(width >= 2 && width <= 64 && height >= 2 && height <= 64, "Invalid map dimensions");
    require(total >= 1 && total <= MaxSystems && total <= width * height, "Invalid system count");
    GameState s;
    s.seed = seed; s.rngState = seed; s.width = width; s.height = height;
    s.unifiedCombat = true; // New (schema 12) campaigns resolve every battle tactically.
    std::vector<int> cells;
    for (int i = 0; i < width * height; ++i) cells.push_back(i);
    for (int i = 0; i < total; ++i) {
        const auto j = static_cast<std::size_t>(i) + bounded(s.rngState, cells.size() - i);
        std::swap(cells[static_cast<std::size_t>(i)], cells[j]);
        const auto cell = cells[static_cast<std::size_t>(i)];
        const std::string name = i == 0 ? "Sol" : "System-" + std::to_string(i + 1);
        s.systems.push_back({i + 1, cell % width, cell / width, name});
        s.systems.back().planets = generateSystemPlanets(seed, i + 1, name, i == 0);
    }
    Colony home;
    home.systemId = 1;
    home.shipyards = 1;
    s.colonies.push_back(home);
    const auto& origin = s.systems.front();
    s.fleets.push_back({1, 1, origin.x, origin.y, origin.x, origin.y, false, false});
    s.fleets.push_back({2, 1, origin.x, origin.y, origin.x, origin.y, true, false, Building::ColonyShip, 8});
    reveal(s, origin.x, origin.y);
    validate(s);
    return s;
}

void applyCampaignEra(GameState& s, StartingEra era) {
    require(startingEraValid(era), "Invalid starting era");
    s.startingEra = era;
    const bool isRef = (s.ruleset == Ruleset::Reference);
    const int baseTech = eraBaseTechLevel(era, s.ruleset);

    auto configureEmpire = [&](EmpireState& e) {
        if (isRef) {
            switch (era) {
            case StartingEra::Early: e.credits = 1000; break;
            case StartingEra::Developed: e.credits = 2500; break;
            case StartingEra::Expanded: e.credits = 4500; break;
            case StartingEra::Advanced: e.credits = 7000; break;
            case StartingEra::Supreme: e.credits = 10000; break;
            }
        } else {
            switch (era) {
            case StartingEra::Early: e.credits = 100; break;
            case StartingEra::Developed: e.credits = 500; break;
            case StartingEra::Expanded: e.credits = 1200; break;
            case StartingEra::Advanced: e.credits = 2200; break;
            case StartingEra::Supreme: e.credits = 3500; break;
            }
        }
        e.researchReserve = 0;
        e.research = 0;
        for (int field = 0; field < ResearchFields; ++field) {
            auto& track = e.technologies[field];
            track.level = baseTech;
            track.progress = 0;
            track.allocation = (field < 4 ? 17 : 16);
            for (int lvl = 1; lvl <= baseTech; ++lvl) {
                e.research += researchCost(s, field, lvl);
            }
        }
    };
    configureEmpire(s);
    for (auto& rival : s.rivals) configureEmpire(rival);

    for (auto& c : s.colonies) {
        c.shipyards = 1;
        c.shipyardEnabled = true;
        c.morale = 70;
        c.overflow = 0;
        c.queue.clear();
        switch (era) {
        case StartingEra::Early:
            c.population = 40;
            c.farms = 2; c.factories = 2; c.laboratories = 1;
            c.powerPlants = 0; c.energy = 20; c.defenseBatteries = 0;
            c.foodWorkers = 20; c.industryWorkers = 10; c.researchWorkers = 10;
            break;
        case StartingEra::Developed:
            c.population = 55;
            c.farms = 3; c.factories = 3; c.laboratories = 2;
            c.powerPlants = 2; c.energy = 40; c.defenseBatteries = 0;
            c.foodWorkers = 28; c.industryWorkers = 15; c.researchWorkers = 12;
            break;
        case StartingEra::Expanded:
            c.population = 75;
            c.farms = 4; c.factories = 4; c.laboratories = 3;
            c.powerPlants = 4; c.energy = 60;
            c.defenseBatteries = isRef ? 1 : 0;
            c.foodWorkers = 38; c.industryWorkers = 20; c.researchWorkers = 17;
            break;
        case StartingEra::Advanced:
            c.population = 95;
            c.farms = 5; c.factories = 5; c.laboratories = 4;
            c.powerPlants = 6; c.energy = 85;
            c.defenseBatteries = isRef ? 2 : 0;
            c.foodWorkers = 48; c.industryWorkers = 26; c.researchWorkers = 21;
            break;
        case StartingEra::Supreme:
            c.population = 120;
            c.farms = 6; c.factories = 6; c.laboratories = 5;
            c.powerPlants = 8; c.energy = 110;
            c.defenseBatteries = isRef ? 3 : 0;
            c.foodWorkers = 60; c.industryWorkers = 32; c.researchWorkers = 28;
            break;
        }

        auto sysIt = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& sys) { return sys.id == c.systemId; });
        if (sysIt != s.systems.end()) {
            for (auto& p : sysIt->planets) {
                if (p.planetClass == PlanetClass::GasGiant || p.planetClass == PlanetClass::Asteroids) continue;
                bool shouldTerraform = false;
                if (p.index == 2) {
                    shouldTerraform = true;
                } else if (p.index == 3 && (era == StartingEra::Expanded || era == StartingEra::Advanced || era == StartingEra::Supreme)) {
                    shouldTerraform = true;
                } else if (p.index == 0 && (era == StartingEra::Advanced || era == StartingEra::Supreme)) {
                    shouldTerraform = true;
                } else if (p.index == 1 && era == StartingEra::Supreme) {
                    shouldTerraform = true;
                }
                if (shouldTerraform) {
                    p.terraformState = TerraformingState::Terraformed;
                    p.terraformWork = p.terraformNeeded;
                }
            }
        }
    }

    s.fleets.clear();
    s.nextFleetId = 1;
    std::vector<int> allEmpires{s.playerEmpire};
    for (const auto& r : s.rivals) allEmpires.push_back(r.id);
    std::sort(allEmpires.begin(), allEmpires.end());

    for (int empId : allEmpires) {
        const auto colonyIt = std::find_if(s.colonies.begin(), s.colonies.end(), [empId](const Colony& col) { return col.ownerId == empId; });
        require(colonyIt != s.colonies.end(), "Colony missing for empire during fleet generation");
        const auto sysIt = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& sys) { return sys.id == colonyIt->systemId; });
        require(sysIt != s.systems.end(), "System missing for colony during fleet generation");
        const int hx = sysIt->x, hy = sysIt->y;

        std::vector<int> roles;
        switch (era) {
        case StartingEra::Early:
            roles = {0, 5};
            break;
        case StartingEra::Developed:
            roles = {0, 5, 1};
            break;
        case StartingEra::Expanded:
            roles = {0, 5, 1, 2, 9};
            break;
        case StartingEra::Advanced:
            roles = {0, 0, 5, 1, 1, 2, 4, 9};
            break;
        case StartingEra::Supreme:
            roles = {0, 0, 5, 5, 1, 1, 2, 4, 9};
            break;
        }

        const int raceId = isRef ? catalogueRace(empId) : -1;
        for (int r : roles) {
            Fleet f;
            f.id = s.nextFleetId++;
            f.ownerId = empId;
            f.x = hx; f.y = hy; f.targetX = hx; f.targetY = hy;
            f.stance = TacticalStance::Balanced;
            if (isRef) {
                const referenceData::ShipRecord* rec = nullptr;
                for (const auto& ship : referenceData::ShipCatalogue) {
                    if (!playableShip(ship) || ship.raceId != raceId || ship.role != r) continue;
                    bool ok = true;
                    for (int req : ship.technologyRequirements) if (req > baseTech) { ok = false; break; }
                    if (!ok) continue;
                    if (!rec || ship.industryCost > rec->industryCost) rec = &ship;
                }
                if (!rec) {
                    for (int fallbackRole : {2, 1, 0}) {
                        for (const auto& ship : referenceData::ShipCatalogue) {
                            if (!playableShip(ship) || ship.raceId != raceId || ship.role != fallbackRole) continue;
                            bool ok = true;
                            for (int req : ship.technologyRequirements) if (req > baseTech) { ok = false; break; }
                            if (!ok) continue;
                            if (!rec || ship.industryCost > rec->industryCost) rec = &ship;
                        }
                        if (rec) break;
                    }
                }
                require(rec != nullptr, "Missing catalogue ship for era role");
                f.referenceShipId = rec->id;
                f.shipClass = roleProduct(rec->role);
                f.colonyShip = (f.shipClass == Building::ColonyShip);
                f.hull = rec->hull;
                f.shields = fleetMaxShields(f);
            } else {
                f.referenceShipId = -1;
                f.shipClass = roleProduct(r);
                f.colonyShip = (f.shipClass == Building::ColonyShip);
                f.hull = shipMaxHull(f.shipClass);
                f.shields = 0;
            }
            s.fleets.push_back(f);
        }
        reveal(s, hx, hy, empId);
    }
}

GameState generateCampaign(std::uint64_t seed, int empires, int width, int height, int systems, int playerEmpire, StartingEra era) {
    require(startingEraValid(era), "Invalid starting era");
    require(empires >= 2 && empires <= 5 && systems >= empires && playerEmpire >= 1 && playerEmpire <= empires,
        "Campaign needs two to five empires, a playable faction and sufficient systems");
    auto s = generate(seed, width, height, systems);
    s.startingEra = era;
    s.populationEnabled = true;
    static constexpr const char* Homes[] = {"", "Sol", "Qo'noS", "Romulus", "Cardassia", "Ferenginar"};
    for (int id = 2; id <= empires; ++id) {
        System* chosen = nullptr; int best = -1;
        for (auto& system : s.systems) {
            if (colonyAt(s, system.id)) continue;
            int distance = std::numeric_limits<int>::max();
            for (const auto& colony : s.colonies) {
                const auto home = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& value) { return value.id == colony.systemId; });
                distance = std::min(distance, std::max(std::abs(home->x - system.x), std::abs(home->y - system.y)));
            }
            if (distance > best) { best = distance; chosen = &system; }
        }
        require(chosen != nullptr, "No empire home available"); chosen->name = Homes[id];
        chosen->planets = generateSystemPlanets(seed, chosen->id, chosen->name, true);
        EmpireState owner; owner.id = id; s.rivals.push_back(owner);
        Colony home; home.systemId = chosen->id; home.ownerId = id; home.shipyards = 1;
        s.colonies.push_back(home);
        reveal(s, chosen->x, chosen->y, id);
    }
    std::sort(s.colonies.begin(), s.colonies.end(), [](const Colony& a, const Colony& b) { return a.systemId < b.systemId; });
    System* minorSystem = nullptr; int closest = std::numeric_limits<int>::max();
    for (auto& system : s.systems) {
        if (colonyAt(s, system.id)) continue;
        const auto& home = s.systems.front();
        const int distance = std::max(std::abs(system.x - home.x), std::abs(system.y - home.y));
        if (distance >= 2 && distance < closest) { closest = distance; minorSystem = &system; }
    }
    if (minorSystem) {
        minorSystem->name = "Bynaus";
        minorSystem->planets = generateSystemPlanets(seed, minorSystem->id, minorSystem->name, false);
        Minor minor; minor.systemId = minorSystem->id; minor.name = "Bynars"; s.minors.push_back(minor);
    }
    for (int first = 1; first <= empires; ++first)
        for (int second = first + 1; second <= empires; ++second)
            s.majorRelations.push_back({first, second, false, false, false});
    if (playerEmpire != 1) {
        const EmpireState originalPlayer = s;
        const auto selected = std::find_if(s.rivals.begin(), s.rivals.end(), [&](const EmpireState& e) { return e.id == playerEmpire; });
        const EmpireState chosen = *selected;
        s.rivals.erase(selected);
        s.rivals.push_back(originalPlayer);
        std::sort(s.rivals.begin(), s.rivals.end(), [](const EmpireState& a, const EmpireState& b) { return a.id < b.id; });
        static_cast<EmpireState&>(s) = chosen;
        s.playerEmpire = playerEmpire;
    }
    applyCampaignEra(s, era);
    syncDiplomacyStateInternal(s);
    validate(s); return s;
}

GameState generateReferenceCampaign(std::uint64_t seed, int empires, int width, int height, int systems, int playerEmpire, StartingEra era) {
    auto s = generateCampaign(seed, empires, width, height, systems, playerEmpire, era);
    s.ruleset = Ruleset::Reference;
    s.advancedRulesEnabled = true;
    s.campaignExpansionEnabled = true;
    s.modernEnginesActive = true;
    syncDiplomacyStateInternal(s);
    applyCampaignEra(s, era);
    validate(s); return s;
}

// Lane G: seeded minor-race roster for a new campaign, scaled to galaxy size (MinorRaces.h).
void placeMinorRaceRoster(GameState& s) {
    require(s.turn == 0 && s.pending.empty(), "Minor races are placed in a new campaign");
    minors::placeRosterMinors(s, s.seed, s.startingEra,
        [&](System& sys) { sys.planets = generateSystemPlanets(s.seed, sys.id, sys.name, false); });
    syncDiplomacyStateInternal(s);
    validate(s);
}

GameState generateCampaignWithEra(std::uint64_t seed, StartingEra era, int empires, int width, int height, int systems, int playerEmpire) {
    return generateCampaign(seed, empires, width, height, systems, playerEmpire, era);
}

GameState generateReferenceCampaignWithEra(std::uint64_t seed, StartingEra era, int empires, int width, int height, int systems, int playerEmpire) {
    return generateReferenceCampaign(seed, empires, width, height, systems, playerEmpire, era);
}

void validate(const GameState& s) {
    require(s.width >= 2 && s.width <= 64 && s.height >= 2 && s.height <= 64, "Invalid map dimensions");
    require(s.ruleset == Ruleset::Prototype || s.ruleset == Ruleset::Reference, "Invalid campaign ruleset");
    require(!s.advancedRulesEnabled || s.ruleset == Ruleset::Reference, "Advanced rules require a reference campaign");
    require(startingEraValid(s.startingEra), "Invalid campaign starting era");
    auto validateEmpire = [&](const EmpireState& e) {
        require(e.id >= 1 && e.id <= 5, "Invalid empire identity");
        require(e.credits >= -MaxBalance && e.credits <= MaxBalance && e.research >= 0 && e.research <= MaxBalance &&
            e.intelligence >= 0 && e.intelligence <= MaxBalance, "Empire balance out of bounds");
        int allocated = 0;
        std::int64_t accounted = e.researchReserve;
        require(e.researchReserve >= 0 && e.researchReserve <= MaxBalance, "Invalid research reserve");
        for (int field = 0; field < ResearchFields; ++field) {
            const auto& tech = e.technologies[field];
            require(tech.level >= 0 && tech.level <= MaxTechLevel && tech.allocation >= 0 && tech.allocation <= 100, "Invalid technology state");
            require(s.ruleset != Ruleset::Reference || tech.level >= 1, "Reference campaign begins at technology level one");
            require(tech.progress >= 0 && (tech.level == MaxTechLevel ? tech.progress == 0 : tech.progress < researchCost(s, field, tech.level + 1)), "Invalid research progress");
            allocated += tech.allocation; accounted += tech.progress;
            for (int level = 1; level <= tech.level; ++level) accounted += researchCost(s, field, level);
        }
        require(allocated == 100 && accounted == e.research, "Research ledger does not reconcile");
        int priorSector = -1;
        for (int cell : e.explored) { require(cell > priorSector && cell < s.width * s.height, "Invalid empire knowledge"); priorSector = cell; }
    };
    validateEmpire(s);
    int priorEmpire = 0;
    require(s.rivals.size() <= 4, "Too many empires");
    for (const auto& e : s.rivals) { require(e.id > priorEmpire && e.id != s.playerEmpire, "Rival IDs must be sorted and unique"); priorEmpire = e.id; validateEmpire(e); }
    require(s.nextFleetId >= 3 && s.nextFleetId <= 1000001, "Invalid next fleet ID");
    require(s.playerEmpire >= 1 && s.playerEmpire <= 5 && s.id == s.playerEmpire, "Invalid playable empire identity");
    require(s.turn >= 0 && s.turn <= MaxTurns, "Turn out of bounds");
    require(s.outcome == CampaignOutcome::Ongoing || s.outcome == CampaignOutcome::Victory ||
        s.outcome == CampaignOutcome::Defeat, "Invalid campaign outcome");
    require(s.winnerEmpire >= 0 && s.winnerEmpire <= 5 && s.lastRandomEventTurn >= 0 &&
        s.lastRandomEventTurn <= s.turn, "Invalid campaign event state");
    require(s.outcome != CampaignOutcome::Victory || s.winnerEmpire == s.playerEmpire, "Campaign winner mismatch");
    require(s.outcome != CampaignOutcome::Ongoing || s.winnerEmpire == 0, "Active campaign has a winner");
    require(s.credits >= -MaxBalance && s.credits <= MaxBalance &&
        s.research >= 0 && s.research <= MaxBalance, "Balance out of bounds");
    require(!s.systems.empty() && s.systems.size() <= MaxSystems, "Invalid system count");
    std::set<std::pair<int, int>> locations;
    std::set<int> ids;
    int previous = 0;
    for (const auto& x : s.systems) {
        require(x.id > previous, "System IDs must be positive, unique and sorted");
        previous = x.id; ids.insert(x.id);
        require(x.x >= 0 && x.x < s.width && x.y >= 0 && x.y < s.height, "System outside map");
        require(locations.insert({x.x, x.y}).second, "Duplicate system location");
        require(!x.name.empty() && x.name.size() <= 64 &&
            x.name.find_first_of("\r\n\t") == std::string::npos, "Invalid system name");
        require(x.planets.size() <= MaxPlanetsPerSystem, "Too many planets in system");
        int priorPlanetIndex = -1;
        for (std::size_t pi = 0; pi < x.planets.size(); ++pi) {
            const auto& p = x.planets[pi];
            require(p.systemId == x.id && p.index == static_cast<int>(pi) && p.index > priorPlanetIndex,
                "Invalid planet system ID or ordering");
            priorPlanetIndex = p.index;
            require(!p.name.empty() && p.name.size() <= 64, "Invalid planet name");
            require(static_cast<int>(p.planetClass) >= 0 && static_cast<int>(p.planetClass) <= 8, "Invalid planet class");
            require(static_cast<int>(p.terraformState) >= 0 && static_cast<int>(p.terraformState) <= 3, "Invalid terraform state");
            require(p.baseCapacity >= 0 && p.maxCapacity >= p.baseCapacity && p.maxCapacity <= 200, "Planet capacity out of bounds");
            require(p.terraformNeeded >= 0 && p.terraformWork >= 0 && p.terraformWork <= p.terraformNeeded, "Planet terraforming work out of bounds");
            if (p.terraformState == TerraformingState::Terraformed)
                require(p.terraformWork == p.terraformNeeded, "Terraformed planet work mismatch");
        }
    }
    require(!s.colonies.empty() && s.colonies.size() <= s.systems.size(), "Invalid colony count");
    require(s.minors.size() <= s.systems.size() && s.nextDiplomacyId > 0 && s.nextDiplomacyId <= 10000000, "Invalid diplomacy state");
    int priorMinor = 0;
    for (const auto& m : s.minors) {
        require(m.systemId > priorMinor && ids.count(m.systemId) == 1, "Invalid minor system"); priorMinor = m.systemId;
        require(!m.name.empty() && m.name.size() <= 64 && m.name.find_first_of("\r\n\t") == std::string::npos, "Invalid minor name");
        require(m.memberOf >= 0 && m.memberOf <= 5, "Invalid minor membership");
        const auto colony = colonyAt(s, m.systemId);
        require(m.memberOf == 0 ? colony == nullptr : colony && colony->ownerId == m.memberOf, "Minor ownership mismatch");
        if (m.memberOf) empire(s, m.memberOf);
        for (int i = 0; i < 6; ++i) require(m.attitude[i] >= 0 && m.attitude[i] <= 100 && m.friendshipTurn[i] >= -1 &&
            m.friendshipTurn[i] <= s.turn && (m.friendshipTurn[i] < 0 || m.contact[i]), "Invalid minor relations");
    }
    minors::validateMinorRaces(s); // Lane G: roster identity and progression bounds
    require(s.diplomacy.size() <= MaxPending, "Too many diplomatic offers");
    std::uint64_t priorOffer = 0; std::set<std::pair<int,int>> offerTargets;
    for (const auto& offer : s.diplomacy) {
        require(offer.id > priorOffer && offer.id < s.nextDiplomacyId, "Invalid diplomatic identity"); priorOffer = offer.id;
        empire(s, offer.empireId);
        const auto m = minorAt(s, offer.systemId);
        require(m && m->contact[offer.empireId] && diplomatic(offer.type) && offer.dueTurn >= s.turn && offer.dueTurn <= MaxTurns + 2, "Invalid pending diplomacy");
        require(offerTargets.insert({offer.empireId, offer.systemId}).second, "Duplicate pending minor offer");
    }
    require(s.majorRelations.size() == (s.rivals.size() + 1) * s.rivals.size() / 2, "Incomplete major relations");
    std::set<std::pair<int,int>> relationPairs;
    for (const auto& r : s.majorRelations) {
        require(r.first >= 1 && r.first < r.second && r.second <= 5 &&
            relationPairs.insert({r.first, r.second}).second, "Invalid major relation pair");
        empire(s, r.first); empire(s, r.second);
        require(!r.atWar || (r.contact && !r.tradeTreaty), "War relation mismatch");
        require(r.warStartTurn >= 0 && r.warStartTurn <= s.turn && r.trust >= 0 && r.trust <= 100,
            "Major relation negotiation values out of bounds");
    }
    require(s.pendingMajorOffers.size() <= MaxPending && s.nextMajorOfferId > 0 &&
        s.nextMajorOfferId <= 10000000, "Invalid major offer state");
    std::uint64_t previousMajorOffer = 0;
    std::set<std::tuple<int,int,CommandType>> uniqueMajorOffers;
    for (const auto& offer : s.pendingMajorOffers) {
        require(s.advancedRulesEnabled && offer.id > previousMajorOffer && offer.id < s.nextMajorOfferId &&
            offer.proposer != offer.target && offer.dueTurn > s.turn && offer.dueTurn <= MaxTurns + 1 &&
            (offer.type == CommandType::OfferPeace || offer.type == CommandType::ProposeTradeTreaty) &&
            uniqueMajorOffers.insert({offer.proposer, offer.target, offer.type}).second,
            "Invalid pending major offer");
        previousMajorOffer = offer.id;
        empire(s, offer.proposer); empire(s, offer.target);
        const auto relation = majorRelation(s, offer.proposer, offer.target);
        require(relation && relation->contact, "Pending major offer lacks contact");
    }
    require(s.tradeRoutes.size() <= MaxSystems, "Too many trade routes");
    std::set<std::pair<int,int>> routeSources;
    for (const auto& r : s.tradeRoutes) {
        const auto source = colonyAt(s, r.fromSystemId), target = colonyAt(s, r.toSystemId);
        require(source && target && source->ownerId == r.ownerId && target->ownerId != r.ownerId,
            "Invalid trade route endpoints");
        const auto relation = majorRelation(s, r.ownerId, target->ownerId);
        require(relation && relation->contact && !relation->atWar && relation->tradeTreaty &&
            routeSources.insert({r.ownerId, r.fromSystemId}).second, "Invalid trade treaty or duplicate source");
    }
    require(s.intelReports.size() <= 32 && s.latestBattles.size() <= 16, "History collection out of bounds");
    for (const auto& r : s.intelReports) {
        require(r.turn >= 0 && r.turn <= s.turn && r.viewer != r.target && r.knownColonies >= 0 &&
            r.knownColonies <= MaxSystems && r.estimatedCredits >= -MaxBalance &&
            r.estimatedCredits <= MaxBalance, "Invalid intelligence report");
        empire(s, r.viewer); empire(s, r.target);
        if (r.systemId) require(ids.count(r.systemId) == 1, "Unknown intelligence target system");
    }
    for (const auto& b : s.latestBattles) {
        require(b.turn >= 0 && b.turn <= s.turn && b.x >= 0 && b.x < s.width && b.y >= 0 && b.y < s.height &&
            b.firstOwner != b.secondOwner && b.winnerOwner >= 0 && b.winnerOwner <= 5,
            "Invalid battle record");
        empire(s, b.firstOwner); empire(s, b.secondOwner);
        require(b.participants.size() <= 256 && b.rounds.size() <= 65, "Battle timeline out of bounds");
        std::map<int, int> sides;
        // Schema 13: stations fight under StationCombatantBase + station ID.
        const auto combatantId = [&](int id) {
            return (id > 0 && id < s.nextFleetId) ||
                (isStationCombatant(id) && stationIdForCombatant(id) < s.nextStationId);
        };
        for (int id : b.firstFleetIds) require(combatantId(id) &&
            sides.emplace(id, b.firstOwner).second, "Invalid first battle side");
        for (int id : b.secondFleetIds) require(combatantId(id) &&
            sides.emplace(id, b.secondOwner).second, "Invalid second battle side");
        require(!b.firstFleetIds.empty() && !b.secondFleetIds.empty() &&
            (b.winnerOwner == 0 || b.winnerOwner == b.firstOwner || b.winnerOwner == b.secondOwner),
            "Invalid battle result");
        // Legacy-rules tactical Hold recharges shields, so shields may rise up to the
        // participant's maximum; hull never rises within a battle.
        std::map<int, int> shieldCeiling;
        for (const auto& participant : b.participants)
            shieldCeiling[participant.fleetId] = isStationCombatant(participant.fleetId) ? participant.initialShields :
                participant.referenceShipId < 0 ? 0 : std::max(0, sourceShip(participant.referenceShipId).shields);
        const auto ceiling = [&](const FleetDamage& d) {
            const auto found = shieldCeiling.find(d.fleetId);
            return found == shieldCeiling.end() ? d.shieldBefore : std::max(d.shieldBefore, found->second);
        };
        std::map<int, FleetDamage> summaries;
        for (const auto& d : b.damage) require(sides.count(d.fleetId) && d.hullBefore > 0 &&
            d.hullAfter >= 0 && d.hullAfter <= d.hullBefore && d.shieldBefore >= 0 &&
            d.shieldAfter >= 0 && d.shieldAfter <= ceiling(d) &&
            summaries.emplace(d.fleetId, d).second, "Invalid battle damage record");
        require(summaries.size() == sides.size(), "Incomplete battle damage record");
        std::set<int> losses, retreats;
        for (int id : b.lostFleetIds) require(sides.count(id) && summaries.at(id).hullAfter == 0 &&
            losses.insert(id).second, "Invalid battle loss");
        for (int id : b.retreatedFleetIds) require(sides.count(id) && !losses.count(id) &&
            summaries.at(id).hullAfter > 0 && retreats.insert(id).second, "Invalid battle retreat");
        for (const auto& [id, damage] : summaries)
            require((damage.hullAfter == 0) == losses.count(id), "Missing battle loss");
        if (!b.participants.empty() || !b.rounds.empty())
            require(b.participants.size() == sides.size() && !b.rounds.empty(), "Battle participant count mismatch");
        std::set<int> participantIds;
        std::map<int, int> remaining, remainingShields;
        for (const auto& participant : b.participants) {
            const bool stationSide = isStationCombatant(participant.fleetId); // Bounded by station maxima, not ship classes.
            require(sides.count(participant.fleetId) && participant.ownerId == sides.at(participant.fleetId) &&
                isShip(participant.shipClass) && participant.initialHull > 0 &&
                participant.initialHull <= (stationSide ? 100000 : participant.referenceShipId < 0 ? shipMaxHull(participant.shipClass) :
                    sourceShip(participant.referenceShipId).hull) &&
                participant.initialShields >= 0 && participant.initialShields <=
                    (stationSide ? 100000 : participant.referenceShipId < 0 ? 0 : sourceShip(participant.referenceShipId).shields) &&
                summaries.at(participant.fleetId).hullBefore == participant.initialHull &&
                summaries.at(participant.fleetId).shieldBefore == participant.initialShields &&
                participantIds.insert(participant.fleetId).second, "Invalid battle participant");
            remaining.emplace(participant.fleetId, participant.initialHull);
            remainingShields.emplace(participant.fleetId, participant.initialShields);
        }
        int priorRound = 0;
        std::set<int> roundLosses, roundRetreats;
        for (const auto& round : b.rounds) {
            require(round.index == priorRound + 1 && round.index <= 65 && round.damage.size() <= 256 &&
                round.lostFleetIds.size() <= 256 && round.retreatedFleetIds.size() <= 256, "Invalid tactical round");
            priorRound = round.index;
            std::set<int> touched;
            for (const auto& d : round.damage) {
                require(participantIds.count(d.fleetId) && !roundLosses.count(d.fleetId) &&
                    !roundRetreats.count(d.fleetId) && touched.insert(d.fleetId).second &&
                    d.hullBefore == remaining.at(d.fleetId) && d.hullAfter >= 0 && d.hullAfter <= d.hullBefore &&
                    d.shieldBefore == remainingShields.at(d.fleetId) && d.shieldAfter >= 0 &&
                    d.shieldAfter <= ceiling(d) &&
                    (d.hullAfter < d.hullBefore || d.shieldAfter != d.shieldBefore), "Invalid tactical damage");
                remaining[d.fleetId] = d.hullAfter;
                remainingShields[d.fleetId] = d.shieldAfter;
            }
            for (int id : round.lostFleetIds) require(participantIds.count(id) && losses.count(id) &&
                remaining.at(id) == 0 && roundLosses.insert(id).second, "Invalid tactical loss");
            for (int id : round.retreatedFleetIds) require(participantIds.count(id) && retreats.count(id) &&
                remaining.at(id) > 0 && roundRetreats.insert(id).second, "Invalid tactical retreat");
        }
        if (!b.rounds.empty()) {
            require(roundLosses == losses && roundRetreats == retreats,
                "Tactical outcomes differ from battle summary");
            for (const auto& [id, hull] : remaining)
                require(summaries.at(id).hullAfter == hull && summaries.at(id).shieldAfter == remainingShields.at(id),
                    "Tactical damage differs from battle summary");
        }
    }
    previous = 0;
    for (const auto& c : s.colonies) {
        require(c.systemId > previous && ids.count(c.systemId) == 1, "Invalid colony system ID");
        previous = c.systemId; validateColony(c); empire(s, c.ownerId);
        for (const auto& project : c.queue) if (project.referenceId >= 0)
            require(s.ruleset == Ruleset::Reference && sourceShip(project.referenceId).raceId == catalogueRace(c.ownerId),
                "Reference project owner mismatch");
    }
    previous = -1;
    for (int cell : s.explored) {
        require(cell > previous && cell < s.width * s.height, "Invalid explored sector set");
        previous = cell;
    }
    require(s.fleets.size() <= 256, "Too many fleets");
    require(s.taskForces.size() <= MaxTaskForces, "Too many task forces");
    require(s.nextTaskForceId > 0 && s.nextTaskForceId <= 10000000, "Invalid next task force ID");
    int priorTfId = 0;
    std::set<int> allTfFleetIds;
    for (const auto& tf : s.taskForces) {
        require(tf.id > priorTfId && tf.id < s.nextTaskForceId, "Task force IDs must be unique, positive and sorted");
        priorTfId = tf.id;
        require(tf.ownerId >= 1 && tf.ownerId <= 5, "Invalid task force owner");
        require(!tf.name.empty() && tf.name.size() <= 64 && tf.name.find_first_of("\r\n\t") == std::string::npos,
            "Invalid task force name");
        require(!tf.fleetIds.empty() && tf.fleetIds.size() <= 256, "Invalid task force member count");
        require(static_cast<int>(tf.stance) >= 0 && static_cast<int>(tf.stance) <= 3, "Invalid task force stance");
        require(tf.targetX >= 0 && tf.targetX < s.width && tf.targetY >= 0 && tf.targetY < s.height,
            "Task force target outside galaxy");
        int firstX = -1, firstY = -1;
        for (int fid : tf.fleetIds) {
            require(allTfFleetIds.insert(fid).second, "Fleet belongs to multiple task forces");
            const auto f = std::find_if(s.fleets.begin(), s.fleets.end(), [fid](const Fleet& x) { return x.id == fid; });
            require(f != s.fleets.end(), "Task force member fleet does not exist");
            require(f->ownerId == tf.ownerId, "Task force member owner mismatch");
            require(f->taskForceId == tf.id, "Fleet task force ID mismatch");
            require(f->hull > 0, "Task force member destroyed");
            if (firstX == -1) { firstX = f->x; firstY = f->y; }
            else { require(f->x == firstX && f->y == firstY, "Task force members not co-located"); }
        }
    }
    previous = 0;
    for (const auto& f : s.fleets) {
        require(f.id > previous && f.id < s.nextFleetId && f.ownerId >= 1 && f.ownerId <= 5 && !f.colonize, "Invalid fleet identity or transient state");
        previous = f.id;
        require(f.x >= 0 && f.x < s.width && f.y >= 0 && f.y < s.height &&
            f.targetX >= 0 && f.targetX < s.width && f.targetY >= 0 && f.targetY < s.height,
            "Fleet outside galaxy");
        require(known(s, f.ownerId, f.x, f.y), "Owned fleet sector must be explored");
        require(isShip(f.shipClass) && f.colonyShip == (f.shipClass == Building::ColonyShip) &&
            f.hull > 0 && f.hull <= fleetMaxHull(f) && f.attackTarget >= 0 &&
            f.attackTarget < s.nextFleetId && f.invadeSystemId >= 0 && (f.invadeSystemId == 0 ||
            f.shipClass == Building::Transport) && f.shields >= 0 && f.shields <= fleetMaxShields(f) &&
            static_cast<int>(f.stance) >= 0 && static_cast<int>(f.stance) <= 3, "Invalid fleet combat state");
        if (f.taskForceId > 0) require(allTfFleetIds.count(f.id) == 1, "Fleet task force not registered in task forces");
        if (f.terraforming) {
            require(f.colonyShip, "Only colony ships can terraform");
            require(f.terraformSystemId > 0 && f.terraformPlanetIndex >= 0, "Invalid terraforming target");
            const auto sys = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == f.terraformSystemId; });
            require(sys != s.systems.end() && sys->x == f.x && sys->y == f.y, "Terraforming colony ship must be at target system");
            require(static_cast<std::size_t>(f.terraformPlanetIndex) < sys->planets.size(), "Terraforming planet index out of bounds");
            const auto& p = sys->planets[static_cast<std::size_t>(f.terraformPlanetIndex)];
            require(p.planetClass != PlanetClass::GasGiant && p.planetClass != PlanetClass::Asteroids, "Cannot terraform uninhabitable world");
        }
        if (f.referenceShipId >= 0) {
            const auto& ship = sourceShip(f.referenceShipId);
            require(s.ruleset == Ruleset::Reference && playableShip(ship) && ship.raceId == catalogueRace(f.ownerId) &&
                roleProduct(ship.role) == f.shipClass, "Invalid reference ship owner or role");
        }
    }
    for (const auto& c : s.colonies) {
        const auto system = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& x) { return x.id == c.systemId; });
        require(known(s, c.ownerId, system->x, system->y), "Owned colony must be explored");
    }
    require(s.pending.size() <= MaxPending, "Too many pending commands");
    std::uint64_t prior = 0;
    for (const auto& c : s.pending) {
        require(c.id > prior && c.id <= s.lastAcceptedCommand, "Invalid command sequence");
        prior = c.id; checkCommand(c);
    }
    require(!s.modernEnginesActive || s.campaignExpansionEnabled,
        "Modern social engines require campaign expansion to be enabled");
    if (s.campaignExpansionEnabled && s.modernEnginesActive) {
        require(s.diplomacyEngineState.currentTurn == s.turn,
            "Diplomacy engine turn does not match campaign turn");
        std::set<int> validEmpires{s.playerEmpire};
        for (const auto& r : s.rivals) {
            require(validEmpires.insert(r.id).second, "Duplicate empire ID in rivals");
        }
        for (const auto& [empId, intelState] : s.empireIntelStates) {
            require(empId == intelState.empireId, "Intel state key does not match embedded empireId");
            require(validEmpires.count(empId) == 1, "Intel state empireId is not a campaign empire");
            for (const auto& ra : intelState.budget.rivalAllocations) {
                require(ra.rivalId != empId, "Intel budget allocates to self");
                require(validEmpires.count(ra.rivalId) == 1, "Intel budget rivalId is not a campaign empire");
            }
        }
        std::set<int> validMinors;
        for (const auto& m : s.minors) {
            validMinors.insert(m.systemId);
        }

        auto validateParticipant = [&](const diplomacy::ParticipantId& p, const char* context) {
            if (p.isMinor) {
                require(validMinors.count(p.id) == 1, (std::string(context) + ": minor participant does not exist in campaign").c_str());
            } else {
                require(validEmpires.count(p.id) == 1, (std::string(context) + ": major participant does not exist in campaign").c_str());
            }
        };

        for (const auto& m : s.diplomacyEngineState.minorStates) {
            require(validMinors.count(m.minorId) == 1, "Diplomacy minor state minorId does not exist in campaign");
            require(validMinors.count(m.systemId) == 1, "Diplomacy minor state systemId does not exist in campaign");
            require(m.membershipOwner == 0 || validEmpires.count(m.membershipOwner) == 1,
                "Diplomacy minor membershipOwner is not a valid campaign empire");
            for (const auto& [empId, att] : m.attitudes) {
                require(empId >= 1 && empId <= 5, "Diplomacy minor attitude refers to invalid empire slot");
                require(att >= 0 && att <= 100, "Diplomacy minor attitude out of range");
            }
            for (const auto& [empId, con] : m.contacts) {
                require(empId >= 1 && empId <= 5, "Diplomacy minor contact refers to invalid empire slot");
                if (con) {
                    require(validEmpires.count(empId) == 1, "Diplomacy minor has contact with nonexistent empire");
                }
            }
            for (const auto& [empId, ft] : m.friendshipTurns) {
                require(empId >= 1 && empId <= 5, "Diplomacy minor friendshipTurn refers to invalid empire slot");
                if (ft >= 0) {
                    require(validEmpires.count(empId) == 1, "Diplomacy minor has friendship with nonexistent empire");
                }
            }
        }

        for (const auto& p : s.diplomacyEngineState.proposals) {
            validateParticipant(p.proposer, "Diplomacy proposal proposer");
            validateParticipant(p.target, "Diplomacy proposal target");
            require(p.proposer != p.target, "Diplomacy proposal proposer and target are identical");
        }

        for (const auto& t : s.diplomacyEngineState.activeTreaties) {
            validateParticipant(t.partyA, "Diplomacy active treaty partyA");
            validateParticipant(t.partyB, "Diplomacy active treaty partyB");
            require(t.partyA != t.partyB, "Diplomacy active treaty parties are identical");
        }

        for (const auto& t : s.diplomacyEngineState.treatyHistory) {
            validateParticipant(t.partyA, "Diplomacy treaty history partyA");
            validateParticipant(t.partyB, "Diplomacy treaty history partyB");
            require(t.partyA != t.partyB, "Diplomacy treaty history parties are identical");
        }

        for (const auto& e : s.diplomacyEngineState.eventHistory) {
            validateParticipant(e.partyA, "Diplomacy event history partyA");
            validateParticipant(e.partyB, "Diplomacy event history partyB");
        }

        require(s.intelOperations.size() <= 64, "Too many intel operations");
        for (const auto& op : s.intelOperations) {
            require(validEmpires.count(op.attackerEmpireId) == 1, "Intel operation attacker is not a campaign empire");
            require(validEmpires.count(op.defenderEmpireId) == 1, "Intel operation defender is not a campaign empire");
            require(op.attackerEmpireId != op.defenderEmpireId, "Intel operation attacker and defender are identical");
            require(op.turn >= 0 && op.turn <= s.turn, "Intel operation turn out of bounds");
        }
    }
    gameplay::validate(s); // Schema-13 gameplay layer.
    auto draft = s;
    applyDraft(draft);
}

PopulationForecast populationForecast(int population, int food, int capacity, int employment, int growthBasisPoints) {
    require(population > 0 && population <= MaxPopulation && food >= 0 && food <= 1000000
        && capacity > 0 && capacity <= MaxPopulation && employment >= 0 && employment <= 1000000
        && growthBasisPoints >= 0 && growthBasisPoints <= 10000, "Invalid population inputs");
    if (food < population) {
        const auto squared = static_cast<std::int64_t>(population) * (population - food) / 100;
        int low = 0, high = population;
        while (low < high) {
            const int middle = (low + high + 1) / 2;
            if (static_cast<std::int64_t>(middle) * middle <= squared) low = middle; else high = middle - 1;
        }
        return {-low, -low, 0};
    }
    if (population >= capacity) return {};
    if (employment * 2 <= population) return {1, 1, 0};
    const auto numerator = static_cast<std::int64_t>(std::max(100, growthBasisPoints)) * (population + std::min(food, capacity));
    const int whole = std::max(1, static_cast<int>(numerator / 20000));
    const int room = capacity - population;
    const int fraction = numerator < 20000 || whole >= room ? 0 : static_cast<int>(numerator % 20000);
    return {std::min(whole, room), std::min(whole + (fraction > 0 ? 1 : 0), room), fraction};
}

Economy economy(const Colony& c) {
    validateColony(c);
    Economy e;
    e.energyAvailable = c.energy;
    e.food = c.foodWorkers * 2;
    e.foodDeficit = std::max(0, c.population - e.food);
    // UNCERTAIN CHOICE: Colony power distribution strictly follows documented structure priority: Factories -> Labs -> Shipyard -> Defense Batteries. Unpowered batteries lack tactical power for planetary defense.
    const auto factories = std::min(c.factories, c.energy / definition(Building::Factory).energyCost);
    const auto remaining1 = c.energy - factories * definition(Building::Factory).energyCost;
    const auto labs = std::min(c.laboratories, remaining1 / definition(Building::Laboratory).energyCost);
    e.energyUsed = factories * 5 + labs * 5;
    e.shipyardPowered = c.shipyards == 1 && c.shipyardEnabled && c.energy - e.energyUsed >= 5;
    if (e.shipyardPowered) e.energyUsed += 5;
    const int remaining2 = c.energy - e.energyUsed;
    e.poweredBatteries = std::min(c.defenseBatteries, remaining2 / definition(Building::DefenseBattery).energyCost);
    e.energyUsed += e.poweredBatteries * definition(Building::DefenseBattery).energyCost;
    e.inactiveFactories = c.factories - factories;
    e.inactiveLabs = c.laboratories - labs;
    e.inactiveBatteries = c.defenseBatteries - e.poweredBatteries;
    e.industry = std::min(c.industryWorkers, factories * 10) * 2;
    e.research = std::min(c.researchWorkers, labs * 10);
    e.income = c.population / 2;
    e.upkeep = c.farms * definition(Building::Farm).creditUpkeep +
        c.factories * definition(Building::Factory).creditUpkeep +
        c.laboratories * definition(Building::Laboratory).creditUpkeep + c.shipyards * 2 + c.powerPlants +
        c.fusionPlants * definition(Building::FusionPlant).creditUpkeep +
        c.defenseBatteries * definition(Building::DefenseBattery).creditUpkeep;
    return e;
}
Economy economy(const GameState& s, const Colony& c) {
    const auto& owner = empire(s, c.ownerId);
    auto powered = c;
    powered.energy = std::min(100000, c.energy * (100 + 5 * owner.technologies[3].level) / 100);
    auto e = economy(powered);
    const auto minorBonus = minors::memberBonuses(s, c.ownerId); // Lane G: member-minor join bonuses
    e.food = e.food * (100 + 5 * owner.technologies[0].level + minorBonus.foodPct) / 100;
    e.foodDeficit = std::max(0, c.population - e.food);
    e.industry = e.industry * (100 + 5 * owner.technologies[2].level + minorBonus.industryPct) / 100;
    if (c.morale < 50) e.industry = e.industry * 75 / 100;
    e.research = e.research * (100 + 5 * owner.technologies[1].level + minorBonus.researchPct) / 100;
    if (c.morale < 50) e.research = e.research * 75 / 100;
    if (s.ruleset == Ruleset::Reference) e.income += c.population * 2;
    e.income = e.income * (100 + minorBonus.creditsPct) / 100;
    return e;
}

void submit(GameState& state, const Command& command) {
    validate(state);
    require(command.id > state.lastAcceptedCommand, "Command ID already used or out of order");
    auto next = state;
    next.lastAcceptedCommand = command.id;
    next.pending.push_back(command);
    validate(next);
    state = std::move(next);
}
void cancel(GameState& state, std::uint64_t commandId) {
    validate(state);
    auto next = state;
    const auto it = std::find_if(next.pending.begin(), next.pending.end(),
        [&](const Command& c) { return c.id == commandId; });
    require(it != next.pending.end(), "Pending command not found");
    next.pending.erase(it);
    validate(next);
    state = std::move(next);
}

GameState preview(const GameState& state) {
    validate(state);
    auto draft = state;
    applyDraft(draft);
    draft.pending.clear();
    for (auto& f : draft.fleets) f.colonize = false;
    for (auto& c : draft.colonies) {
        if (c.ownerId == draft.playerEmpire) {
            if (c.workforcePriority != GovernorPriority::Manual && !c.workforceManualOverride) {
                evaluateWorkforceGovernor(draft, c);
            }
            if (c.queuePriority != GovernorPriority::Manual && c.queue.empty()) {
                evaluateQueueGovernor(draft, c);
            }
        }
    }
    return draft;
}

TurnReport advanceWithCombat(GameState& state, const std::function<void(GameState&, TurnReport&)>& combatResolver) {
    validate(state);
    require(state.turn < MaxTurns, "Prototype turn limit reached");
    require(state.outcome == CampaignOutcome::Ongoing, "Campaign has ended");
    auto next = state;
    TurnReport report;
    report.turn = ++next.turn;
    report.creditsBefore = next.credits;
    applyDraft(next);
    for (const auto& c : next.pending) report.appliedCommands.push_back(c.id);
    next.pending.clear();
    for (auto& c : next.colonies) {
        if (c.ownerId == next.playerEmpire) {
            if (c.workforcePriority != GovernorPriority::Manual && !c.workforceManualOverride) {
                evaluateWorkforceGovernor(next, c);
            }
            if (c.queuePriority != GovernorPriority::Manual && c.queue.empty()) {
                evaluateQueueGovernor(next, c);
            }
        }
    }
    planRivals(next);
    gameplay::planTurn(next, report); // Schema 13: AI stations, anomaly surveys, megaprojects.
    for (const auto& relation : next.majorRelations) {
        const auto old = majorRelation(state, relation.first, relation.second);
        if (old && old->atWar != relation.atWar &&
            (relation.first == next.playerEmpire || relation.second == next.playerEmpire)) {
            const int other = relation.first == next.playerEmpire ? relation.second : relation.first;
            report.events.push_back(std::string(empireName(other)) +
                (relation.atWar ? " is now at war with us." : " accepted peace."));
        }
    }
    for (std::size_t i = state.intelReports.size(); i < next.intelReports.size(); ++i) {
        const auto& intel = next.intelReports[i];
        if (intel.viewer == next.playerEmpire)
            report.events.push_back(intel.systemId ? "Sabotage disrupted production at system " + std::to_string(intel.systemId) + "."
                : "Intelligence report: " + std::string(empireName(intel.target)) + " has " +
                    std::to_string(intel.knownColonies) + " colonies and roughly " + std::to_string(intel.estimatedCredits) + " credits.");
    }
    std::array<std::int64_t, 6> researchGained{};
    for (const auto& f : next.fleets) {
        const int upkeep = f.referenceShipId >= 0 ? sourceShip(f.referenceShipId).maintenance :
            f.colonyShip ? 2 : f.shipClass == Building::Destroyer ? 4 : f.shipClass == Building::Transport ? 3 : 1;
        mutableEmpire(next, f.ownerId).credits -= upkeep;
        if (f.ownerId == next.playerEmpire) report.fleetUpkeep += upkeep;
    }
    gameplay::chargeStationUpkeep(next, report);
    const auto productionSnapshot = next;
    std::vector<Fleet> launched;
    for (auto& c : next.colonies) {
        ColonyReport cr;
        cr.systemId = c.systemId; cr.economy = economy(productionSnapshot, c);
        // Schema 13: a colony funding a starbase upgrade or megaproject diverts its industry.
        cr.economy.industry -= gameplay::divertIndustry(next, c, cr.economy.industry, report);
        auto& owner = mutableEmpire(next, c.ownerId);
        owner.credits += cr.economy.income - cr.economy.upkeep;
        owner.research += cr.economy.research;
        owner.intelligence = std::min(MaxBalance, owner.intelligence + cr.economy.research / 2);
        researchGained[c.ownerId] += cr.economy.research;
        if (c.ownerId == next.playerEmpire) report.researchGained += cr.economy.research;
        const auto blocker = constructionBlocker(productionSnapshot, c);
        if (c.ownerId == next.playerEmpire && !c.queue.empty() && !blocker.empty())
            report.events.push_back("System " + std::to_string(c.systemId) + ": construction paused. " + blocker + ".");
        if (!c.queue.empty() && blocker.empty()) {
            const auto project = c.queue.front();
            const int available = project.work + c.overflow + cr.economy.industry;
            c.overflow = 0;
            const auto b = project.product;
            if (available >= constructionCost(project)) {
                c.overflow = available - constructionCost(project);
                c.queue.erase(c.queue.begin());
                if (b == Building::Farm) ++c.farms;
                else if (b == Building::Factory) ++c.factories;
                else if (b == Building::Laboratory) ++c.laboratories;
                else if (b == Building::Shipyard) ++c.shipyards;
                else if (b == Building::PowerPlant) { ++c.powerPlants; c.energy += 10; }
                else if (b == Building::FusionPlant) { ++c.fusionPlants; c.energy += 40; }
                else if (b == Building::ResearchInstitute) c.laboratories += 2;
                else if (b == Building::DefenseBattery) ++c.defenseBatteries;
                else {
                    const auto location = std::find_if(next.systems.begin(), next.systems.end(),
                        [&](const System& system) { return system.id == c.systemId; });
                    Fleet launchedShip{next.nextFleetId++, c.ownerId, location->x, location->y,
                        location->x, location->y, b == Building::ColonyShip, false, b,
                        project.referenceId >= 0 ? sourceShip(project.referenceId).hull : shipMaxHull(b)};
                    launchedShip.referenceShipId = project.referenceId;
                    launchedShip.shields = next.advancedRulesEnabled ? fleetMaxShields(launchedShip) : 0;
                    launched.push_back(launchedShip);
                    if (c.ownerId == next.playerEmpire) report.events.push_back(fleetName(launchedShip) + " launched at " + location->name + ".");
                }
                cr.completed = project.referenceId >= 0 ? sourceShip(project.referenceId).name : definition(b).name;
            } else c.queue.front().work = available;
        }
        if (c.ownerId == next.playerEmpire) report.colonies.push_back(cr);
    }
    for (const auto& route : productionSnapshot.tradeRoutes) {
        const auto source = colonyAt(productionSnapshot, route.fromSystemId);
        const auto target = colonyAt(productionSnapshot, route.toSystemId);
        if (!source || !target || source->ownerId != route.ownerId || target->ownerId == route.ownerId) continue;
        const auto relation = majorRelation(productionSnapshot, route.ownerId, target->ownerId);
        if (!relation || relation->atWar || !relation->tradeTreaty) continue;
        const int income = 5 + std::min(source->population, target->population) / 10;
        mutableEmpire(next, route.ownerId).credits += income;
        if (route.ownerId == next.playerEmpire) report.tradeIncome += income;
    }
    auto settleResearch = [&](EmpireState& owner) {
        std::array<std::int64_t, ResearchFields> awards{}, remainders{};
        std::int64_t distributed = 0;
        for (int i = 0; i < ResearchFields; ++i) {
            const auto weighted = researchGained[owner.id] * owner.technologies[i].allocation;
            awards[i] = weighted / 100; remainders[i] = weighted % 100; distributed += awards[i];
        }
        for (auto left = researchGained[owner.id] - distributed; left > 0; --left) {
            int best = next.turn % ResearchFields;
            for (int offset = 1; offset < ResearchFields; ++offset) {
                const int candidate = (next.turn + offset) % ResearchFields;
                if (remainders[candidate] > remainders[best]) best = candidate;
            }
            ++awards[best]; remainders[best] = -1;
        }
        for (int i = 0; i < ResearchFields; ++i) {
            auto& tech = owner.technologies[i]; tech.progress += awards[i];
            while (tech.level < MaxTechLevel && tech.progress >= researchCost(next, i, tech.level + 1)) {
                tech.progress -= researchCost(next, i, ++tech.level);
                if (owner.id == next.playerEmpire) report.events.push_back(std::string(researchName(i)) + " advanced to level " + std::to_string(tech.level) + ". Benefits apply next turn.");
            }
            if (tech.level == MaxTechLevel) { owner.researchReserve += tech.progress; tech.progress = 0; }
        }
    };
    settleResearch(next); for (auto& owner : next.rivals) settleResearch(owner);
    auto moving = productionSnapshot;
    resolveFleets(moving, report);
    establishMajorContact(moving, report);
    if (moving.advancedRulesEnabled) for (auto& fleet : moving.fleets) {
        const int maximum = fleetMaxShields(fleet);
        fleet.shields = std::min(maximum, fleet.shields + std::max(1, maximum / 5));
    }
    gameplay::TurnEffects layerEffects;
    gameplay::beforeCombat(moving, report, layerEffects); // Outposts, anomaly surveys, sector hazards.
    if (combatResolver) {
        combatResolver(moving, report);
    } else if (moving.unifiedCombat) {
        resolveTacticalBattles(moving, report);
    } else {
        resolveBattles(moving, report); // Schema 4-11 saves keep their legacy strategic resolver.
    }
    if (moving.advancedRulesEnabled) {
        std::set<int> battleFleetIds;
        for (const auto& b : report.battles) {
            for (int id : b.firstFleetIds) battleFleetIds.insert(id);
            for (int id : b.secondFleetIds) battleFleetIds.insert(id);
        }
        for (auto& fleet : moving.fleets) {
            if (fleet.hull <= 0 || battleFleetIds.count(fleet.id)) continue;
            const int maxHull = fleetMaxHull(fleet);
            if (fleet.hull >= maxHull) continue;
            bool hostilePresent = false;
            for (const auto& other : moving.fleets) {
                if (other.hull > 0 && other.x == fleet.x && other.y == fleet.y) {
                    const auto rel = majorRelation(moving, fleet.ownerId, other.ownerId);
                    if (rel && rel->atWar) { hostilePresent = true; break; }
                }
            }
            if (hostilePresent) continue;
            for (const auto& c : moving.colonies) {
                if (c.ownerId != fleet.ownerId) continue;
                const auto loc = std::find_if(moving.systems.begin(), moving.systems.end(),
                    [&](const System& s) { return s.id == c.systemId; });
                if (loc != moving.systems.end() && loc->x == fleet.x && loc->y == fleet.y &&
                    economy(moving, c).shipyardPowered) {
                    const int repair = std::max(1, maxHull / 10);
                    const int oldHull = fleet.hull;
                    fleet.hull = std::min(maxHull, fleet.hull + repair);
                    if (fleet.ownerId == moving.playerEmpire && fleet.hull > oldHull) {
                        report.events.push_back(fleetName(fleet) + " repaired at " + loc->name + ".");
                    }
                    break;
                }
            }
        }
    }
    gameplay::afterCombat(moving, report); // Station regeneration and starbase repairs.
    resolveInvasions(moving, report);
    next.fleets = std::move(moving.fleets); next.explored = std::move(moving.explored);
    next.majorRelations = std::move(moving.majorRelations);
    next.tradeRoutes = std::move(moving.tradeRoutes);
    next.latestBattles = std::move(moving.latestBattles);
    next.taskForces = std::move(moving.taskForces);
    next.nextTaskForceId = moving.nextTaskForceId;
    next.systems = std::move(moving.systems);
    next.rngState = moving.rngState;
    for (auto& e : next.rivals) e.explored = std::move(mutableEmpire(moving, e.id).explored);
    // Invasions may change minor membership; carry it over with the colonies.
    next.minors = std::move(moving.minors);
    for (const auto& c : moving.colonies) {
        auto existing = std::find_if(next.colonies.begin(), next.colonies.end(), [&](const Colony& item) { return item.systemId == c.systemId; });
        if (existing == next.colonies.end()) next.colonies.push_back(c);
        else if (existing->ownerId != c.ownerId) {
            // Captured this turn. Production already settled for the previous owner
            // (credits, research, completed structures and progress stay with the
            // colony), then the conquest applies on top of the produced colony.
            existing->ownerId = c.ownerId;
            existing->population = c.population;
            existing->morale = c.morale;
            existing->queue = c.queue;
            existing->overflow = c.overflow;
            existing->foodWorkers = c.foodWorkers;
            existing->industryWorkers = c.industryWorkers;
            existing->researchWorkers = c.researchWorkers;
            existing->workforcePriority = c.workforcePriority;
            existing->queuePriority = c.queuePriority;
            existing->workforceManualOverride = c.workforceManualOverride;
            existing->workforceExplanation.clear();
            existing->queueExplanation.clear();
            existing->governorWarning.clear();
        }
    }
    std::sort(next.colonies.begin(), next.colonies.end(), [](const Colony& a, const Colony& b) { return a.systemId < b.systemId; });
    next.fleets.insert(next.fleets.end(), launched.begin(), launched.end());
    reconcileTaskForces(next);
    gameplay::carryOver(next, moving, layerEffects, report);
    minors::reviewMinorRaces(next, report.events); // Lane G: minor growth, tech and temperament
    resolveDiplomacy(next, report);
    if (next.campaignExpansionEnabled && next.modernEnginesActive) {
        resolveModernDiplomacy(next, report);
        resolveMajorOffers(next, report);
        resolveModernIntelligence(next, report);
    } else {
        resolveMajorOffers(next, report);
    }
    if (next.populationEnabled) for (auto& c : next.colonies) {
        const auto old = colonyAt(productionSnapshot, c.systemId);
        if (!old || old->ownerId != c.ownerId) continue;
        const auto& inputs = *old;
        const auto food = economy(productionSnapshot, inputs).food;
        const int cap = systemCapacity(productionSnapshot, c.systemId);
        const auto change = populationForecast(inputs.population, food, cap,
            10 * (inputs.farms + inputs.factories + inputs.laboratories), PrototypeGrowthBasisPoints);
        const int delta = change.minimum + (change.upwardChances > 0 && bounded(next.rngState, 20000) < static_cast<std::uint64_t>(change.upwardChances) ? 1 : 0);
        c.population += delta;
        if (c.workforcePriority != GovernorPriority::Manual && !c.workforceManualOverride) {
            evaluateWorkforceGovernor(next, c);
            if (c.ownerId == next.playerEmpire && delta != 0) {
                report.events.push_back("System " + std::to_string(c.systemId) + ": population " + (delta > 0 ? "+" : "") + std::to_string(delta)
                    + ". Governor (" + governorPriorityName(c.workforcePriority) + ") assigned citizens.");
            }
            if (c.ownerId == next.playerEmpire && !c.governorWarning.empty()) {
                report.events.push_back("System " + std::to_string(c.systemId) + " governor warning: " + c.governorWarning + ".");
            }
        } else {
            int excess = std::max(0, c.foodWorkers + c.industryWorkers + c.researchWorkers - c.population);
            auto trim = [&excess](int& workers) { const int removed = std::min(workers, excess); workers -= removed; excess -= removed; };
            trim(c.industryWorkers); trim(c.researchWorkers); trim(c.foodWorkers);
            if (c.ownerId == next.playerEmpire && delta != 0)
                report.events.push_back("System " + std::to_string(c.systemId) + ": population " + (delta > 0 ? "+" : "") + std::to_string(delta)
                    + (delta < 0 ? " from food shortage." : ". New citizens are unassigned."));
        }
    }
    for (auto& c : next.colonies) {
        const int moraleTarget = minors::moraleTarget(next, c.ownerId); // Lane G: 70 plus member-minor morale
        if (c.morale < moraleTarget) ++c.morale;
        else if (c.morale > moraleTarget) --c.morale;
    }
    if (next.rules.curatedEvents) {
        gameplay::resolveGalacticEvents(next, report); // Schema-13 event deck (eventsEnabled gates new draws).
    } else if (next.eventsEnabled && !next.rivals.empty() && next.turn - next.lastRandomEventTurn >= 10 && !next.colonies.empty()) {
        next.lastRandomEventTurn = next.turn;
        const auto index = static_cast<std::size_t>(bounded(next.rngState, next.colonies.size()));
        auto& target = next.colonies[index];
        const auto event = bounded(next.rngState, 3);
        if (event == 0) mutableEmpire(next, target.ownerId).credits += 25;
        else if (event == 1) target.morale = std::min(100, target.morale + 10);
        else target.morale = std::max(0, target.morale - 10);
        if (target.ownerId == next.playerEmpire) report.events.push_back("System " + std::to_string(target.systemId) +
            (event == 0 ? ": local trade windfall (+25 credits)." : event == 1 ? ": civic celebration (+10 morale)." :
                ": civil unrest (-10 morale)."));
    }
    if (!next.rivals.empty()) {
        std::set<int> surviving;
        for (const auto& c : next.colonies) surviving.insert(c.ownerId);
        if (!surviving.count(next.playerEmpire)) {
            next.outcome = CampaignOutcome::Defeat;
            next.winnerEmpire = surviving.size() == 1 ? *surviving.begin() : 0;
        }
        else if (surviving.size() == 1 || (next.turn >= 20 &&
            static_cast<int>(std::count_if(next.colonies.begin(), next.colonies.end(), [&](const Colony& c) { return c.ownerId == next.playerEmpire; })) * 5 >=
            static_cast<int>(next.systems.size()) * 3)) {
            next.outcome = CampaignOutcome::Victory; next.winnerEmpire = next.playerEmpire;
        }
        if (next.outcome != CampaignOutcome::Ongoing) report.events.push_back(next.outcome == CampaignOutcome::Victory ?
            std::string(empireName(next.playerEmpire)) + " campaign victory." : std::string(empireName(next.playerEmpire)) + " campaign defeat.");
    }
    if (next.outcome == CampaignOutcome::Ongoing) gameplay::evaluateVictory(next, report);
    gameplay::finishTurn(state, next, report); // Upgrades, storms, templates, chronicle.
    report.outcome = next.outcome;
    validate(next);
    report.creditsAfter = next.credits;
    report.stateHash = fingerprint(next);
    state = std::move(next);
    return report;
}

TurnReport advance(GameState& state) {
    return advanceWithCombat(state, {});
}

std::string serialize(const GameState& state) {
    validate(state);
    const auto text = payload(state);
    return text + "CHECKSUM " + std::to_string(hashText(text)) + "\n";
}

GameState deserialize(const std::string& text) {
    require(text.size() <= 1024 * 1024, "Save too large");
    const bool version4 = text.starts_with("BOTF_SIM 4 prototype-development-4\n");
    const bool version5 = text.starts_with("BOTF_SIM 5 prototype-empires-5\n");
    const bool version6 = text.starts_with("BOTF_SIM 6 prototype-diplomacy-6\n");
    const std::string previousHeader = version4 ? "BOTF_SIM 4 prototype-development-4\n" : version5 ? "BOTF_SIM 5 prototype-empires-5\n" : "BOTF_SIM 6 prototype-diplomacy-6\n";
    if (version4 || version5 || version6) {
        const auto checksumStart = text.rfind("CHECKSUM ");
        require(checksumStart != std::string::npos, "Missing legacy checksum");
        const auto body = text.substr(0, checksumStart);
        require(text.substr(checksumStart) == "CHECKSUM " + std::to_string(hashText(body)) + "\n", "Legacy save checksum mismatch");
        auto migrated = body;
        migrated.replace(0, previousHeader.size(), "BOTF_SIM 7 prototype-population-7\n");
        const auto systemsStart = migrated.find("\nSYSTEMS ");
        require(systemsStart != std::string::npos, "Missing legacy systems");
        if (version4 || version5) migrated.insert(systemsStart + 1, std::string(version4 ? "RIVALS 0\n" : "") + "MINORS 1 0\nDIPLOMACY 0\n");
        const auto researchStart = migrated.find("\nRESEARCH ");
        require(researchStart != std::string::npos, "Missing legacy research");
        migrated.insert(researchStart + 1, "POPULATION 0\n");
        migrated += "CHECKSUM " + std::to_string(hashText(migrated)) + "\n";
        return deserialize(migrated);
    }
    std::istringstream in(text);
    in.imbue(std::locale::classic());
    tag(in, "BOTF_SIM");
    int version = 0; std::string content;
    in >> version >> content;
    const bool legacy7 = version == 7 && content == "prototype-population-7";
    const bool legacy8 = version == 8 && content == "prototype-campaign-8";
    const bool legacy9 = version == 9 && content == "reference-campaign-9";
    const bool legacy10 = version == 10 && content == "reference-campaign-10";
    // Schema 11 differs from 12 only by the header and the REMASTER section.
    const bool legacy11 = version == 11 && content == "campaign-expansion-11";
    // Schema 12 differs from 13 only by the header and the GAMEPLAY_LAYER section.
    const bool legacy12 = version == 12 && content == "remaster-12";
    const bool current = version == SaveVersion && content == ContentVersion;
    require(current || legacy7 || legacy8 || legacy9 || legacy10 || legacy11 || legacy12, "Unsupported save version");
    require(content == ContentVersion || legacy7 || legacy8 || legacy9 || legacy10 || legacy11 || legacy12, "Incompatible prototype content");
    const auto checksumStart = text.rfind("CHECKSUM ");
    require(checksumStart != std::string::npos, "Missing save checksum");
    const auto rawBody = text.substr(0, checksumStart);
    require(text.substr(checksumStart) == "CHECKSUM " + std::to_string(hashText(rawBody)) + "\n",
        (legacy7 || legacy8 || legacy9 || legacy10 || legacy11 || legacy12) ? "Legacy save checksum mismatch" : "Save checksum mismatch");
    GameState s;
    tag(in, "STATE");
    in >> s.seed >> s.rngState >> s.lastAcceptedCommand >> s.width >> s.height >> s.turn >> s.credits >> s.research;
    tag(in, "NEXT_FLEET"); in >> s.nextFleetId;
    tag(in, "POPULATION"); in >> s.populationEnabled;
    tag(in, "RESEARCH"); in >> s.researchReserve;
    for (auto& tech : s.technologies) in >> tech.level >> tech.progress >> tech.allocation;
    if (!legacy7) in >> s.intelligence;
    tag(in, "RIVALS"); auto n = count(in, 4);
    for (int i = 0; i < n; ++i) {
        EmpireState e; in >> e.id >> e.credits >> e.research >> e.researchReserve;
        for (auto& tech : e.technologies) in >> tech.level >> tech.progress >> tech.allocation;
        const int cells = count(in, 4096);
        for (int j = 0; j < cells; ++j) { int cell = -1; in >> cell; e.explored.push_back(cell); }
        if (!legacy7) in >> e.intelligence;
        s.rivals.push_back(e);
    }
    tag(in, "MINORS"); in >> s.nextDiplomacyId; n = count(in, MaxSystems);
    for (int i = 0; i < n; ++i) {
        Minor m; in >> m.systemId >> std::quoted(m.name) >> m.memberOf;
        for (int j = 0; j < 6; ++j) in >> m.contact[j] >> m.attitude[j] >> m.friendshipTurn[j];
        s.minors.push_back(m);
    }
    tag(in, "DIPLOMACY"); n = count(in, MaxPending);
    for (int i = 0; i < n; ++i) {
        DiplomaticOffer offer; int type = -1;
        in >> offer.id >> offer.empireId >> offer.systemId >> offer.dueTurn >> type;
        offer.type = static_cast<CommandType>(type); s.diplomacy.push_back(offer);
    }
    if (!legacy7) {
        tag(in, "CAMPAIGN"); int outcome = -1;
        in >> outcome >> s.winnerEmpire >> s.eventsEnabled >> s.aiAggressionEnabled >> s.lastRandomEventTurn;
        s.outcome = static_cast<CampaignOutcome>(outcome);
        if (!legacy8) { tag(in, "RULESET"); int ruleset = -1; in >> ruleset; s.ruleset = static_cast<Ruleset>(ruleset); }
        if (!legacy8 && !legacy9) {
            tag(in, "ADVANCED"); in >> s.advancedRulesEnabled;
        }
        if (!legacy7 && !legacy8 && !legacy9 && !legacy10) {
            tag(in, "EXPANSION"); in >> s.campaignExpansionEnabled;
        } else {
            s.campaignExpansionEnabled = false;
        }
        if (!legacy8 && !legacy9) {
            tag(in, "MAJOROFFERS"); in >> s.nextMajorOfferId; const int offers = count(in, MaxPending);
            for (int i = 0; i < offers; ++i) {
                MajorOffer offer; int type = -1;
                in >> offer.id >> offer.proposer >> offer.target >> offer.dueTurn >> type;
                offer.type = static_cast<CommandType>(type); s.pendingMajorOffers.push_back(offer);
            }
        }
        tag(in, "RELATIONS"); n = count(in, 10);
        for (int i = 0; i < n; ++i) {
            MajorRelation r; in >> r.first >> r.second >> r.contact >> r.atWar >> r.tradeTreaty;
            if (!legacy8 && !legacy9) in >> r.warStartTurn >> r.trust;
            s.majorRelations.push_back(r);
        }
        tag(in, "TRADE"); n = count(in, MaxSystems);
        for (int i = 0; i < n; ++i) { TradeRoute r; in >> r.ownerId >> r.fromSystemId >> r.toSystemId; s.tradeRoutes.push_back(r); }
        tag(in, "INTELREPORTS"); n = count(in, 32);
        for (int i = 0; i < n; ++i) {
            IntelReport r; in >> r.turn >> r.viewer >> r.target >> r.systemId >> r.success >> r.knownColonies >> r.estimatedCredits;
            s.intelReports.push_back(r);
        }
        tag(in, "BATTLES"); n = count(in, 16);
        for (int i = 0; i < n; ++i) {
            CombatReport b; in >> b.turn >> b.x >> b.y >> b.firstOwner >> b.secondOwner >> b.winnerOwner;
            auto ids = [&](std::vector<int>& values) { const int total = count(in, 256); for (int j = 0; j < total; ++j) { int id = 0; in >> id; values.push_back(id); } };
            ids(b.firstFleetIds); ids(b.secondFleetIds); ids(b.lostFleetIds); ids(b.retreatedFleetIds);
            const int total = count(in, 256);
            for (int j = 0; j < total; ++j) { FleetDamage d; in >> d.fleetId >> d.hullBefore >> d.hullAfter;
                if (!legacy9 && !legacy8) in >> d.shieldBefore >> d.shieldAfter;
                b.damage.push_back(d); }
            if (!legacy8) {
                const int participants = count(in, 256);
                for (int j = 0; j < participants; ++j) {
                    CombatParticipant p; int shipClass = -1;
                    in >> p.fleetId >> p.ownerId >> shipClass >> p.referenceShipId >> p.initialHull;
                    if (!legacy9) in >> p.initialShields;
                    p.shipClass = static_cast<Building>(shipClass); b.participants.push_back(p);
                }
                const int rounds = count(in, 65);
                for (int j = 0; j < rounds; ++j) {
                    CombatRound round; in >> round.index;
                    const int hits = count(in, 256);
                    for (int k = 0; k < hits; ++k) { FleetDamage d; in >> d.fleetId >> d.hullBefore >> d.hullAfter;
                        if (!legacy9) in >> d.shieldBefore >> d.shieldAfter;
                        round.damage.push_back(d); }
                    ids(round.lostFleetIds); ids(round.retreatedFleetIds);
                    b.rounds.push_back(std::move(round));
                }
            }
            s.latestBattles.push_back(b);
        }
    }
    tag(in, "SYSTEMS"); n = count(in, MaxSystems);
    for (int i = 0; i < n; ++i) {
        System x; in >> x.id >> x.x >> x.y >> std::quoted(x.name); s.systems.push_back(x);
    }
    if (!legacy7 && !legacy8 && !legacy9 && !legacy10) {
        tag(in, "PLANETS"); int totalPlanets = count(in, MaxSystems * MaxPlanetsPerSystem);
        for (int i = 0; i < totalPlanets; ++i) {
            Planet p; int pc = 0, ts = 0;
            in >> p.systemId >> p.index >> std::quoted(p.name) >> pc >> ts
               >> p.terraformWork >> p.terraformNeeded >> p.baseCapacity >> p.maxCapacity;
            p.planetClass = static_cast<PlanetClass>(pc);
            p.terraformState = static_cast<TerraformingState>(ts);
            auto sysIt = std::find_if(s.systems.begin(), s.systems.end(), [&](const System& sys) { return sys.id == p.systemId; });
            if (sysIt != s.systems.end()) sysIt->planets.push_back(p);
        }
    }
    tag(in, "COLONIES"); n = count(in, MaxSystems);
    for (int i = 0; i < n; ++i) {
        Colony c;
        in >> c.systemId >> c.ownerId >> c.population >> c.foodWorkers >> c.industryWorkers >> c.researchWorkers
           >> c.farms >> c.factories >> c.laboratories >> c.energy >> c.overflow >> c.shipyards >> c.shipyardEnabled >> c.powerPlants;
        const auto q = count(in, MaxQueue);
        for (int j = 0; j < q; ++j) {
            int b = -1, work = -1, referenceId = -1; in >> b >> work;
            if (!legacy7 && !legacy8) in >> referenceId;
            c.queue.emplace_back(static_cast<Building>(b), work, referenceId);
        }
        if (!legacy7) in >> c.morale;
        if (!legacy7 && !legacy8 && !legacy9) in >> c.defenseBatteries;
        s.colonies.push_back(c);
    }
    if (legacy7 || legacy8 || legacy9 || legacy10) {
        // UNCERTAIN CHOICE: Legacy save migration regenerates canonical deterministic planets per system while maintaining legacy economy rules.
        for (auto& sys : s.systems) {
            bool isHome = sys.id == 1 || (colonyAt(s, sys.id) != nullptr);
            sys.planets = generateSystemPlanets(s.seed, sys.id, sys.name, isHome);
        }
    }
    tag(in, "EXPLORATION"); in >> s.playerEmpire; n = count(in, 4096);
    for (int i = 0; i < n; ++i) { int cell = -1; in >> cell; s.explored.push_back(cell); }
    s.id = s.playerEmpire;
    tag(in, "FLEETS"); n = count(in, 256);
    for (int i = 0; i < n; ++i) {
        Fleet f; in >> f.id >> f.ownerId >> f.x >> f.y >> f.targetX >> f.targetY >> f.colonyShip >> f.colonize;
        if (legacy7) { f.shipClass = f.colonyShip ? Building::ColonyShip : Building::Scout; f.hull = shipMaxHull(f.shipClass); }
        else { int shipClass = -1; in >> shipClass >> f.hull >> f.attackTarget >> f.retreat >> f.invadeSystemId;
            f.shipClass = static_cast<Building>(shipClass); }
        if (!legacy7 && !legacy8) in >> f.referenceShipId;
        if (!legacy7 && !legacy8 && !legacy9) {
            int stance = -1; in >> f.shields >> stance; f.stance = static_cast<TacticalStance>(stance);
        }
        if (!legacy7 && !legacy8 && !legacy9 && !legacy10) {
            in >> f.taskForceId >> f.terraforming >> f.terraformSystemId >> f.terraformPlanetIndex;
        }
        s.fleets.push_back(f);
    }
    tag(in, "PENDING"); n = count(in, MaxPending);
    for (int i = 0; i < n; ++i) {
        Command c; int type = -1, building = -1;
        in >> c.id >> type >> c.systemId >> c.foodWorkers >> c.industryWorkers >> c.researchWorkers >> building
            >> c.fleetId >> c.targetX >> c.targetY >> c.queueIndex >> c.queueDestination;
        for (auto& amount : c.researchAllocation) in >> amount;
        if (!legacy7) in >> c.targetEmpire >> c.targetFleetId >> c.targetSystemId;
        if (!legacy7 && !legacy8) in >> c.referenceId;
        if (!legacy7 && !legacy8 && !legacy9) { int stance = -1; in >> stance; c.stance = static_cast<TacticalStance>(stance); }
        if (!legacy7 && !legacy8 && !legacy9 && !legacy10) {
            in >> c.taskForceId >> c.planetIndex;
            const int memberCount = count(in, 256);
            for (int m = 0; m < memberCount; ++m) { int mfid = 0; in >> mfid; c.memberFleetIds.push_back(mfid); }
            in >> std::quoted(c.taskForceName);
            std::string remainder;
            std::getline(in, remainder);
            std::istringstream rem(remainder);
            int treatyTypeVal = 0, durationVal = 25, toneVal = 0;
            if (rem >> treatyTypeVal >> durationVal >> toneVal >> c.proposalCreditOffer >> c.proposalCreditDemand) {
                c.proposalTreatyType = static_cast<diplomacy::TreatyType>(treatyTypeVal);
                c.proposalDuration = static_cast<diplomacy::TreatyDuration>(durationVal);
                c.proposalTone = static_cast<diplomacy::DiplomaticTone>(toneVal);
                int targetIsMinor = 0, acceptVal = 0;
                rem >> targetIsMinor >> c.proposalId >> acceptVal >> std::quoted(c.proposalReason);
                c.proposalTargetIsMinor = (targetIsMinor != 0);
                c.proposalAccept = (acceptVal != 0);
                int deptFocusVal = 0;
                std::size_t rivalCount = 0;
                if (rem >> c.intelInternalSecurity >> deptFocusVal >> rivalCount) {
                    // A crafted count must not drive an unbounded loop: at most five empires exist.
                    require(rivalCount <= 5, "Invalid intel rival allocation count");
                    c.intelDepartmentFocus = static_cast<intelligence::IntelDepartment>(deptFocusVal);
                    for (std::size_t m = 0; m < rivalCount; ++m) {
                        int rId = 0, espPct = 0, sabPct = 0;
                        require(static_cast<bool>(rem >> rId >> espPct >> sabPct), "Truncated intel rival allocation");
                        c.intelRivalIds.push_back(rId);
                        c.intelEspionagePcts.push_back(espPct);
                        c.intelSabotagePcts.push_back(sabPct);
                    }
                    if (colonyGovernorCommand(static_cast<CommandType>(type))) {
                        int govWf = 0, govQ = 0, qPin = 0;
                        if (rem >> govWf >> govQ >> qPin) {
                            c.governorWorkforce = static_cast<GovernorPriority>(govWf);
                            c.governorQueue = static_cast<GovernorPriority>(govQ);
                            c.queuePinned = (qPin != 0);
                        }
                    }
                    if (gameplay::isGameplayCommand(static_cast<CommandType>(type))) {
                        require(current, "Schema-13 command in an older save");
                        gameplay::readCommandTail(rem, c);
                    }
                }
            }
        }
        c.type = static_cast<CommandType>(type); c.building = static_cast<Building>(building);
        s.pending.push_back(c);
    }
    if (!legacy7 && !legacy8 && !legacy9 && !legacy10) {
        tag(in, "TASKFORCES"); in >> s.nextTaskForceId; const int tfCount = count(in, MaxTaskForces);
        for (int i = 0; i < tfCount; ++i) {
            TaskForce tf; int stance = 0;
            in >> tf.id >> tf.ownerId >> std::quoted(tf.name) >> stance >> tf.targetX >> tf.targetY >> tf.attackTarget;
            tf.stance = static_cast<TacticalStance>(stance);
            const int memberCount = count(in, 256);
            for (int m = 0; m < memberCount; ++m) { int fid = 0; in >> fid; tf.fleetIds.push_back(fid); }
            s.taskForces.push_back(tf);
        }
    }
    if (legacy7) {
        std::vector<int> owners{s.playerEmpire};
        for (const auto& rival : s.rivals) owners.push_back(rival.id);
        std::sort(owners.begin(), owners.end());
        for (std::size_t i = 0; i < owners.size(); ++i)
            for (std::size_t j = i + 1; j < owners.size(); ++j)
                s.majorRelations.push_back({owners[i], owners[j], false, false, false});
        s.eventsEnabled = false; s.aiAggressionEnabled = false;
    }
    std::string nextTag;
    bool hasIntelOperations = false;
    bool hasStartingEra = false;
    bool hasColonyGovernors = false;
    bool hasRemaster = false;

    // Loading is pure: governor settings are restored exactly as saved and the
    // governors are not re-run (they run during advance()/preview()).
    auto parseColonyGovernors = [&](std::istream& input) {
        const int govCount = count(input, MaxSystems);
        require(static_cast<std::size_t>(govCount) == s.colonies.size(), "Colony governor count mismatch");
        for (int i = 0; i < govCount; ++i) {
            int sysId = 0, wf = 0, q = 0, over = 0;
            require(static_cast<bool>(input >> sysId >> wf >> q >> over), "Malformed colony governor record");
            auto& colony = s.colonies[static_cast<std::size_t>(i)];
            require(colony.systemId == sysId, "Colony governor record out of order");
            require(wf >= 0 && wf <= 4 && q >= 0 && q <= 4 && (over == 0 || over == 1), "Invalid colony governor setting");
            const int pinnedCount = count(input, MaxQueue);
            int priorIndex = -1;
            for (int p = 0; p < pinnedCount; ++p) {
                int qIdx = -1;
                require(static_cast<bool>(input >> qIdx) && qIdx > priorIndex &&
                    static_cast<std::size_t>(qIdx) < colony.queue.size(), "Invalid pinned queue index");
                priorIndex = qIdx;
                colony.queue[static_cast<std::size_t>(qIdx)].pinned = true;
            }
            colony.workforcePriority = static_cast<GovernorPriority>(wf);
            colony.queuePriority = static_cast<GovernorPriority>(q);
            colony.workforceManualOverride = (over != 0);
        }
    };
    auto parseStartingEra = [&]() {
        int eraVal = 0;
        require(static_cast<bool>(in >> eraVal), "Missing starting era value");
        require(startingEraValid(static_cast<StartingEra>(eraVal)), "Invalid starting era in save");
        s.startingEra = static_cast<StartingEra>(eraVal);
    };
    auto parseRemaster = [&]() {
        int unified = -1;
        require(static_cast<bool>(in >> unified) && (unified == 0 || unified == 1), "Invalid combat model flag");
        s.unifiedCombat = unified == 1;
        const int fusionColonies = count(in, MaxSystems);
        int priorSystem = 0;
        for (int i = 0; i < fusionColonies; ++i) {
            int sysId = 0, plants = 0;
            require(static_cast<bool>(in >> sysId >> plants) && sysId > priorSystem &&
                plants > 0 && plants <= MaxBuildings, "Invalid fusion plant record");
            priorSystem = sysId;
            const auto colony = std::find_if(s.colonies.begin(), s.colonies.end(), [sysId](const Colony& c) { return c.systemId == sysId; });
            require(colony != s.colonies.end(), "Fusion plant record for unknown colony");
            colony->fusionPlants = plants;
        }
    };
    auto readTag = [&](const char* context) {
        std::string value;
        require(static_cast<bool>(in >> value), context);
        return value;
    };

    // Optional sections appear in this fixed order; older schemas omit some of them.
    nextTag = readTag("Malformed save section");
    if (nextTag == "DIPLOMACY_ENGINE") {
        require(s.campaignExpansionEnabled, "Modern diplomacy engine requires campaign expansion");
        std::string diploLine, diploData;
        while (std::getline(in, diploLine)) {
            if (!diploLine.empty() && diploLine.back() == '\r') diploLine.pop_back();
            if (diploLine.empty()) continue;
            diploData += diploLine + "\n";
            if (diploLine == "END_BOTF_DIPLOMACY") break;
        }
        std::string diploErr;
        require(diplomacy::deserializeDiplomacyState(diploData, s.diplomacyEngineState, &diploErr),
                ("Malformed diplomacy save: " + diploErr).c_str());

        tag(in, "INTEL_ENGINE");
        int intelCount = count(in, 16);
        std::set<int> seenIntelEmpires;
        for (int i = 0; i < intelCount; ++i) {
            std::string intelLine, intelData;
            while (std::getline(in, intelLine)) {
                if (!intelLine.empty() && intelLine.back() == '\r') intelLine.pop_back();
                if (intelLine.empty()) continue;
                intelData += intelLine + "\n";
                if (intelLine == "END_BOTF_INTEL") break;
            }
            intelligence::EmpireIntelState intelState;
            std::string intelErr;
            require(intelligence::deserializeIntelState(intelData, intelState, &intelErr),
                    ("Malformed intel save: " + intelErr).c_str());
            require(seenIntelEmpires.insert(intelState.empireId).second, "Duplicate empire intel state in save");
            s.empireIntelStates[intelState.empireId] = intelState;
        }
        s.modernEnginesActive = true;
        s.intelOperations.clear();
        nextTag = readTag("Malformed save section after INTEL_ENGINE");
        if (nextTag == "INTEL_OPERATIONS") {
            hasIntelOperations = true;
            const int opCount = count(in, 64); // Same bound validate() enforces.
            for (int i = 0; i < opCount; ++i) {
                std::string opLine;
                while (std::getline(in, opLine) && opLine.empty()) {}
                if (!opLine.empty() && opLine.back() == '\r') opLine.pop_back();
                intelligence::OperationRecord op;
                std::string opErr;
                require(intelligence::deserializeOperationRecord(opLine, op, &opErr),
                        ("Malformed intel operation save: " + opErr).c_str());
                s.intelOperations.push_back(op);
            }
            nextTag = readTag("Malformed save section after INTEL_OPERATIONS");
        }
    } else {
        s.modernEnginesActive = false;
        syncDiplomacyStateInternal(s);
    }
    if (nextTag == "STARTING_ERA") {
        hasStartingEra = true;
        parseStartingEra();
        nextTag = readTag("Malformed save section after era/engine");
    } else {
        s.startingEra = StartingEra::Early;
    }
    if (nextTag == "REMASTER") {
        require(current || legacy12, "Schema 12 section in an older save");
        hasRemaster = true;
        parseRemaster();
        nextTag = readTag("Malformed save section after REMASTER");
    }
    if (nextTag == "COLONY_GOVERNORS") {
        hasColonyGovernors = true;
        parseColonyGovernors(in);
        nextTag = readTag("Malformed save section after COLONY_GOVERNORS");
    }
    bool hasGameplayLayer = false;
    if (nextTag == "GAMEPLAY_LAYER") {
        require(current, "Schema 13 section in an older save");
        hasGameplayLayer = true;
        gameplay::parseSection(in, s);
        nextTag = readTag("Malformed save section after GAMEPLAY_LAYER");
    }
    if (nextTag == "MINOR_RACES") { // Lane G: optional, written only when a roster minor exists
        require(current, "Schema 13 section in an older save");
        minors::readMinorRacesSection(in, s);
        nextTag = readTag("Malformed save section after MINOR_RACES");
    }
    require(nextTag == "CHECKSUM", ("Unexpected save section: " + nextTag).c_str());
    std::uint64_t checksum = 0;
    require(static_cast<bool>(in >> checksum), "Missing save checksum");
    in >> std::ws;
    require(in.eof(), "Unexpected trailing save data");
    validate(s);
    if (current) {
        require(hasStartingEra && hasRemaster && hasColonyGovernors && hasGameplayLayer &&
            (!s.modernEnginesActive || hasIntelOperations), "Schema 13 save is missing a required section");
        require(rawBody == payload(s), "Save is not in canonical format");
    } else if (legacy12) {
        // Schema 12 text is the schema-13 payload without the gameplay layer.
        require(hasStartingEra && hasRemaster && hasColonyGovernors &&
            (!s.modernEnginesActive || hasIntelOperations), "Schema 12 save is missing a required section");
        std::string expected = payload(s);
        const std::string header = std::string("BOTF_SIM ") + std::to_string(SaveVersion) + ' ' + ContentVersion + '\n';
        require(expected.starts_with(header), "Malformed canonical payload header");
        expected.replace(0, header.size(), "BOTF_SIM 12 remaster-12\n");
        gameplay::stripSection(expected);
        require(rawBody == expected, "Save is not in canonical format");
    } else if (legacy11) {
        // Rebuild the schema-11 text this state would have produced and require an exact match.
        std::string expectedOld = payload(s);
        gameplay::stripSection(expectedOld);
        const std::string header = std::string("BOTF_SIM ") + std::to_string(SaveVersion) + ' ' + ContentVersion + '\n';
        require(expectedOld.starts_with(header), "Malformed canonical payload header");
        expectedOld.replace(0, header.size(), "BOTF_SIM 11 campaign-expansion-11\n");
        const auto remasterPos = expectedOld.rfind("\nREMASTER ");
        require(remasterPos != std::string::npos, "Malformed canonical payload construction for schema 12 section");
        expectedOld.erase(remasterPos + 1, expectedOld.find('\n', remasterPos + 1) - remasterPos);
        if (!hasColonyGovernors) {
            std::ostringstream govSection;
            govSection.imbue(std::locale::classic());
            govSection << "COLONY_GOVERNORS " << s.colonies.size() << '\n';
            for (const auto& c : s.colonies) {
                govSection << c.systemId << " 0 0 0 0\n";
            }
            const std::string govToken = govSection.str();
            const auto pos = expectedOld.rfind(govToken);
            require(pos != std::string::npos, "Malformed canonical payload construction for colony governors");
            expectedOld.erase(pos, govToken.size());
        }
        if (!hasStartingEra) {
            const std::string eraToken = "STARTING_ERA " + std::to_string(static_cast<int>(s.startingEra)) + "\n";
            const auto pos = expectedOld.rfind(eraToken);
            require(pos != std::string::npos, "Malformed canonical payload construction for starting era");
            expectedOld.erase(pos, eraToken.size());
        }
        if (!hasIntelOperations && s.campaignExpansionEnabled && s.modernEnginesActive) {
            const std::string opsToken = "INTEL_OPERATIONS 0\n";
            const auto pos = expectedOld.rfind(opsToken);
            require(pos != std::string::npos, "Malformed canonical payload construction");
            expectedOld.erase(pos, opsToken.size());
        }
        require(rawBody == expectedOld, "Save is not in canonical format");
    }
    return s;
}

std::uint64_t fingerprint(const GameState& state) { validate(state); return hashText(payload(state)); }

std::string describe(const TurnReport& r) {
    std::ostringstream out;
    out << "Turn " << r.turn << " | credits " << r.creditsBefore << " -> " << r.creditsAfter
        << " | research +" << r.researchGained << " | hash " << r.stateHash << '\n';
    for (const auto& c : r.colonies) {
        const auto& e = c.economy;
        out << "  System " << c.systemId << ": food " << e.food << " (deficit " << e.foodDeficit
            << "), industry " << e.industry << ", power used " << e.energyUsed
            << ", income " << e.income << ", upkeep " << e.upkeep;
        if (!c.completed.empty()) out << ", COMPLETED " << c.completed;
        out << '\n';
    }
    for (const auto& event : r.events) out << "  " << event << '\n';
    return out.str();
}

const diplomacy::DiplomacyState& campaignDiplomacyState(const GameState& state) {
    return state.diplomacyEngineState;
}

std::vector<diplomacy::Proposal> pendingProposals(const GameState& state, int empireId) {
    std::vector<diplomacy::Proposal> res;
    diplomacy::ParticipantId p = diplomacy::ParticipantId::major(empireId);
    for (const auto& prop : state.diplomacyEngineState.proposals) {
        if (!prop.resolved && (prop.proposer == p || prop.target == p)) {
            res.push_back(prop);
        }
    }
    return res;
}

std::vector<diplomacy::Proposal> incomingProposals(const GameState& state, int empireId) {
    std::vector<diplomacy::Proposal> res;
    diplomacy::ParticipantId p = diplomacy::ParticipantId::major(empireId);
    for (const auto& prop : state.diplomacyEngineState.proposals) {
        if (!prop.resolved && prop.target == p) {
            res.push_back(prop);
        }
    }
    return res;
}

std::vector<diplomacy::Proposal> outgoingProposals(const GameState& state, int empireId) {
    std::vector<diplomacy::Proposal> res;
    diplomacy::ParticipantId p = diplomacy::ParticipantId::major(empireId);
    for (const auto& prop : state.diplomacyEngineState.proposals) {
        if (!prop.resolved && prop.proposer == p) {
            res.push_back(prop);
        }
    }
    return res;
}

std::vector<diplomacy::ActiveTreaty> activeTreaties(const GameState& state, int empireId) {
    std::vector<diplomacy::ActiveTreaty> res;
    diplomacy::ParticipantId p = diplomacy::ParticipantId::major(empireId);
    for (const auto& t : state.diplomacyEngineState.activeTreaties) {
        if (t.status == diplomacy::TreatyStatus::Active && (t.partyA == p || t.partyB == p)) {
            res.push_back(t);
        }
    }
    return res;
}

std::vector<diplomacy::DiplomaticEvent> diplomaticEvents(const GameState& state) {
    return state.diplomacyEngineState.eventHistory;
}

const diplomacy::MinorDiplomacyState* minorDiplomacy(const GameState& state, int minorId) {
    return state.diplomacyEngineState.getMinorState(minorId);
}

bool empireEliminated(const GameState& state, int empireId) {
    return empireEliminatedInternal(state, empireId);
}

namespace {
std::vector<int> treatyPartners(const GameState& state, int empireId, int minimumRank) {
    std::vector<int> partners;
    if (!modernDiplomacy(state) || empireEliminatedInternal(state, empireId)) return partners;
    const auto self = diplomacy::ParticipantId::major(empireId);
    for (const auto& t : state.diplomacyEngineState.activeTreaties) {
        if (t.status != diplomacy::TreatyStatus::Active || !t.involves(self) || diplomacy::getTreatyRank(t.type) < minimumRank) continue;
        const auto other = t.getCounterparty(self);
        if (other.isMinor || !campaignEmpire(state, other.id) || empireEliminatedInternal(state, other.id)) continue;
        if (std::find(partners.begin(), partners.end(), other.id) == partners.end()) partners.push_back(other.id);
    }
    std::sort(partners.begin(), partners.end());
    return partners;
}
} // namespace

std::vector<int> alliedEmpires(const GameState& state, int empireId) {
    return treatyPartners(state, empireId, diplomacy::getTreatyRank(diplomacy::TreatyType::Alliance));
}

std::vector<int> sensorSharingEmpires(const GameState& state, int empireId) {
    return treatyPartners(state, empireId, diplomacy::getTreatyRank(diplomacy::TreatyType::Affiliation));
}

bool treatyForbidsAttack(const GameState& state, int empireA, int empireB) {
    if (!modernDiplomacy(state) || empireA == empireB) return false;
    const auto a = diplomacy::ParticipantId::major(empireA), b = diplomacy::ParticipantId::major(empireB);
    const auto& ds = state.diplomacyEngineState;
    return ds.highestTreatyRank(a, b) > 0 || ds.hasActiveTreaty(a, b, diplomacy::TreatyType::Peace);
}

std::vector<diplomacy::DefenseCall> openCallsToArms(const GameState& state, int allyId) {
    std::vector<diplomacy::DefenseCall> calls;
    for (const auto& c : state.diplomacyEngineState.defenseCalls) if (c.ally == allyId) calls.push_back(c);
    return calls;
}

diplomacy::DiplomaticVictoryStatus diplomaticVictoryStatus(const GameState& state, int empireId) {
    std::map<int, std::int64_t> empirePopulation, independentMinors;
    for (int id : survivingEmpires(state)) empirePopulation[id] = 0;
    for (const auto& c : state.colonies) {
        const auto it = empirePopulation.find(c.ownerId);
        if (it != empirePopulation.end()) it->second += c.population;
    }
    for (const auto& m : state.minors) {
        if (m.memberOf == 0) independentMinors[m.systemId] = diplomacy::IndependentMinorNominalPopulation;
    }
    return diplomacy::evaluateDiplomaticVictory(state.diplomacyEngineState, empireId, empirePopulation, independentMinors);
}

bool diplomaticVictoryAchieved(const GameState& state, int empireId) {
    return modernDiplomacy(state) && diplomaticVictoryStatus(state, empireId).achieved;
}

bool canProposeTreaty(const GameState& state, int proposerId, int targetId, bool isTargetMinor,
                      diplomacy::TreatyType type, std::string* outError) {
    if (!modernDiplomacy(state)) {
        if (outError) *outError = "Modern diplomacy requires a modern (reference expansion) campaign";
        return false;
    }
    CampaignDiplomacyContext ctx(state);
    diplomacy::ParticipantId proposer = diplomacy::ParticipantId::major(proposerId);
    diplomacy::ParticipantId target = isTargetMinor ? diplomacy::ParticipantId::minor(targetId)
                                                    : diplomacy::ParticipantId::major(targetId);
    diplomacy::Proposal dummy;
    dummy.id = state.diplomacyEngineState.nextProposalId;
    dummy.proposer = proposer;
    dummy.target = target;
    dummy.treatyType = type;
    dummy.duration = diplomacy::TreatyDuration::Turns25;
    dummy.tone = diplomacy::DiplomaticTone::Cordial;
    dummy.status = diplomacy::ProposalStatus::Pending;
    dummy.submittedTurn = state.turn;
    dummy.dueTurn = state.turn + 1;
    return diplomacy::validateProposalCreation(state.diplomacyEngineState, ctx, dummy, outError);
}

Command makeProposeTreatyCommand(std::uint64_t id, int targetId, bool isTargetMinor,
                                 diplomacy::TreatyType treatyType,
                                 diplomacy::TreatyDuration duration,
                                 diplomacy::DiplomaticTone tone,
                                 std::int64_t creditOffer, std::int64_t creditDemand) {
    Command c;
    c.id = id;
    c.type = CommandType::SubmitDiplomaticProposal;
    c.proposalTargetIsMinor = isTargetMinor;
    if (isTargetMinor) {
        c.targetSystemId = targetId;
    } else {
        c.targetEmpire = targetId;
    }
    c.proposalTreatyType = treatyType;
    c.proposalDuration = duration;
    c.proposalTone = tone;
    c.proposalCreditOffer = creditOffer;
    c.proposalCreditDemand = creditDemand;
    return c;
}

Command makeRespondProposalCommand(std::uint64_t id, std::uint64_t proposalId, bool accept,
                                   const std::string& reason) {
    Command c;
    c.id = id;
    c.type = CommandType::RespondDiplomaticProposal;
    c.proposalId = proposalId;
    c.proposalAccept = accept;
    c.proposalReason = reason;
    return c;
}

const intelligence::EmpireIntelState* empireIntelState(const GameState& state, int empireId) {
    auto it = state.empireIntelStates.find(empireId);
    if (it != state.empireIntelStates.end()) return &it->second;
    return nullptr;
}

std::vector<intelligence::OperationRecord> empireIntelHistory(const GameState& state, int empireId) {
    std::vector<intelligence::OperationRecord> res;
    for (const auto& op : state.intelOperations) {
        if (op.attackerEmpireId == empireId || (op.defenderEmpireId == empireId && op.detectedByDefender)) {
            res.push_back(op);
        }
    }
    return res;
}

std::int64_t empireIntelOutput(const GameState& state, int empireId) {
    std::int64_t total = 0;
    for (const auto& c : state.colonies) {
        if (c.ownerId == empireId) {
            Economy eco = economy(state, c);
            total += eco.research / 2;
        }
    }
    return std::clamp<std::int64_t>(std::max<std::int64_t>(10, total), 0, 100000000);
}

std::int64_t empireSecurityStockpile(const GameState& state, int empireId) {
    auto it = state.empireIntelStates.find(empireId);
    if (it != state.empireIntelStates.end()) {
        return it->second.internalSecurityStockpile;
    }
    return 0;
}

Command makeSetIntelBudgetCommand(std::uint64_t id, int internalSecurityPct,
                                 const std::vector<std::pair<int, std::pair<int, int>>>& rivalAllocations) {
    Command c;
    c.id = id;
    c.type = CommandType::SetIntelBudget;
    c.intelInternalSecurity = internalSecurityPct;
    for (const auto& r : rivalAllocations) {
        c.intelRivalIds.push_back(r.first);
        c.intelEspionagePcts.push_back(r.second.first);
        c.intelSabotagePcts.push_back(r.second.second);
    }
    return c;
}

Command makeSetIntelDepartmentFocusCommand(std::uint64_t id, int rivalId, intelligence::IntelDepartment focus) {
    Command c;
    c.id = id;
    c.type = CommandType::SetIntelFocus;
    c.targetEmpire = rivalId;
    c.intelDepartmentFocus = focus;
    return c;
}

const char* governorPriorityName(GovernorPriority priority) {
    switch (priority) {
    case GovernorPriority::Manual: return "Manual";
    case GovernorPriority::Balanced: return "Balanced";
    case GovernorPriority::Growth: return "Growth";
    case GovernorPriority::Research: return "Research";
    case GovernorPriority::Manufacturing: return "Manufacturing";
    default: return "Unknown";
    }
}

Command makeSetColonyGovernorCommand(std::uint64_t id, int systemId,
                                     GovernorPriority workforce,
                                     GovernorPriority queue) {
    Command c;
    c.id = id;
    c.type = CommandType::SetColonyGovernor;
    c.systemId = systemId;
    c.governorWorkforce = workforce;
    c.governorQueue = queue;
    return c;
}

Command makeSetQueuePinCommand(std::uint64_t id, int systemId, int queueIndex, bool pinned) {
    Command c;
    c.id = id;
    c.type = CommandType::SetQueuePin;
    c.systemId = systemId;
    c.queueIndex = queueIndex;
    c.queuePinned = pinned;
    return c;
}

Command makeClearGovernorOverrideCommand(std::uint64_t id, int systemId) {
    Command c;
    c.id = id;
    c.type = CommandType::ClearGovernorOverride;
    c.systemId = systemId;
    return c;
}

Command makeCancelFirstConstructionCommand(std::uint64_t id, int systemId) {
    Command c;
    c.id = id;
    c.type = CommandType::CancelFirstConstruction;
    c.systemId = systemId;
    return c;
}

Command makePrioritizeLastConstructionCommand(std::uint64_t id, int systemId) {
    Command c;
    c.id = id;
    c.type = CommandType::PrioritizeLastConstruction;
    c.systemId = systemId;
    return c;
}

Command makeRemoveConstructionCommand(std::uint64_t id, int systemId, int queueIndex) {
    Command c;
    c.id = id;
    c.type = CommandType::RemoveConstruction;
    c.systemId = systemId;
    c.queueIndex = queueIndex;
    return c;
}

Command makeMoveConstructionCommand(std::uint64_t id, int systemId, int queueIndex, int destinationIndex) {
    Command c;
    c.id = id;
    c.type = CommandType::MoveConstruction;
    c.systemId = systemId;
    c.queueIndex = queueIndex;
    c.queueDestination = destinationIndex;
    return c;
}

void evaluateWorkforceGovernor(const GameState& state, Colony& c) {
    if (c.workforcePriority == GovernorPriority::Manual || c.workforceManualOverride) {
        return;
    }

    auto calculateColonyEconomy = [&](int foodW, int indW, int resW) {
        Colony temp = c;
        temp.foodWorkers = foodW;
        temp.industryWorkers = indW;
        temp.researchWorkers = resW;
        return economy(state, temp);
    };

    const int maxFarms = c.farms * 10;
    const auto& owner = empire(state, c.ownerId);
    const int effectiveEnergy = std::min(100000, c.energy * (100 + 5 * owner.technologies[3].level) / 100);
    const int poweredFactories = std::min(c.factories, effectiveEnergy / definition(Building::Factory).energyCost);
    const int remEnergy1 = effectiveEnergy - poweredFactories * definition(Building::Factory).energyCost;
    const int poweredLabs = std::min(c.laboratories, remEnergy1 / definition(Building::Laboratory).energyCost);

    const int maxFactories = poweredFactories * 10;
    const int maxLabs = poweredLabs * 10;

    const int maxFarmWorkers = std::min(c.population, maxFarms);
    const int maxPossibleFood = calculateColonyEconomy(maxFarmWorkers, 0, 0).food;

    int neededFoodWorkers = 0;
    while (neededFoodWorkers < c.population && neededFoodWorkers < maxFarms) {
        if (calculateColonyEconomy(neededFoodWorkers, 0, 0).food >= c.population) {
            break;
        }
        ++neededFoodWorkers;
    }

    if (maxPossibleFood < c.population) {
        c.governorWarning = "Maximum farm capacity cannot feed population (" + std::to_string(maxPossibleFood) + "/" + std::to_string(c.population) + " food): build more farms or advance Biotech research";
        neededFoodWorkers = maxFarmWorkers;
    } else {
        c.governorWarning.clear();
    }

    int foodAssigned = neededFoodWorkers;
    int remainingPop = c.population - foodAssigned;
    int indAssigned = 0;
    int resAssigned = 0;

    if (c.workforcePriority == GovernorPriority::Growth) {
        int extraFood = std::min(remainingPop, maxFarms - foodAssigned);
        foodAssigned += extraFood;
        remainingPop -= extraFood;

        int extraInd = std::min(remainingPop, maxFactories);
        indAssigned += extraInd;
        remainingPop -= extraInd;

        int extraRes = std::min(remainingPop, maxLabs);
        resAssigned += extraRes;
        remainingPop -= extraRes;
    } else if (c.workforcePriority == GovernorPriority::Manufacturing) {
        int extraInd = std::min(remainingPop, maxFactories);
        indAssigned += extraInd;
        remainingPop -= extraInd;

        int extraRes = std::min(remainingPop, maxLabs);
        resAssigned += extraRes;
        remainingPop -= extraRes;

        int extraFood = std::min(remainingPop, maxFarms - foodAssigned);
        foodAssigned += extraFood;
        remainingPop -= extraFood;
    } else if (c.workforcePriority == GovernorPriority::Research) {
        int extraRes = std::min(remainingPop, maxLabs);
        resAssigned += extraRes;
        remainingPop -= extraRes;

        int extraInd = std::min(remainingPop, maxFactories);
        indAssigned += extraInd;
        remainingPop -= extraInd;

        int extraFood = std::min(remainingPop, maxFarms - foodAssigned);
        foodAssigned += extraFood;
        remainingPop -= extraFood;
    } else { // Balanced
        while (remainingPop > 0) {
            bool assignedAny = false;
            if (indAssigned < maxFactories && remainingPop > 0) {
                ++indAssigned; --remainingPop; assignedAny = true;
            }
            if (resAssigned < maxLabs && remainingPop > 0) {
                ++resAssigned; --remainingPop; assignedAny = true;
            }
            if (foodAssigned < maxFarms && remainingPop > 0) {
                ++foodAssigned; --remainingPop; assignedAny = true;
            }
            if (!assignedAny) break;
        }
    }

    c.foodWorkers = foodAssigned;
    c.industryWorkers = indAssigned;
    c.researchWorkers = resAssigned;

    if (!c.governorWarning.empty()) {
        c.workforceExplanation = c.governorWarning + "; assigned all " + std::to_string(foodAssigned) + " available workers to farms.";
    } else {
        c.workforceExplanation = "Reserved " + std::to_string(neededFoodWorkers) + " food workers to prevent starvation; allocated "
            + std::to_string(indAssigned) + " industry, " + std::to_string(resAssigned) + " research ("
            + governorPriorityName(c.workforcePriority) + " priority).";
    }
}

void evaluateQueueGovernor(const GameState& state, Colony& c) {
    if (c.queuePriority == GovernorPriority::Manual) return;
    if (c.queue.size() >= MaxQueue) return;
    if (!c.queue.empty()) return;
    if (c.ownerId != state.playerEmpire) return;

    auto choices = constructionChoices(state, c.systemId);
    auto isLegal = [&](Building b, int refId = -2) -> const ConstructionChoice* {
        for (const auto& ch : choices) {
            if (ch.product == b && (refId == -2 || ch.referenceId == refId) && ch.blocker.empty()) {
                return &ch;
            }
        }
        return nullptr;
    };

    const auto eco = economy(state, c);
    const int maxFarmWorkers = std::min(c.population, c.farms * 10);
    // Probe the farm ceiling with everyone else released; keeping the existing
    // industry/research workers would exceed the population and throw.
    Colony testCol = c;
    testCol.foodWorkers = maxFarmWorkers;
    testCol.industryWorkers = 0;
    testCol.researchWorkers = 0;
    const int maxFoodYield = economy(state, testCol).food;
    const int powerDemand = c.factories * 5 + c.laboratories * 5 + (c.shipyards ? 5 : 0) + c.defenseBatteries * 5;
    const auto& owner = empire(state, c.ownerId);
    const int effectiveEnergy = std::min(100000, c.energy * (100 + 5 * owner.technologies[3].level) / 100);

    const ConstructionChoice* chosen = nullptr;
    std::string reasonCategory;

    // 1. Critical bottleneck: Food shortage
    if (maxFoodYield < c.population || c.farms * 10 < c.population) {
        chosen = isLegal(Building::Farm);
        if (chosen) reasonCategory = "critical food deficit";
    }

    // 2. Critical bottleneck: Energy deficit
    if (!chosen && (effectiveEnergy < powerDemand || eco.inactiveFactories > 0 || eco.inactiveLabs > 0 || (!eco.shipyardPowered && c.shipyards > 0))) {
        chosen = isLegal(Building::FusionPlant);
        if (!chosen) chosen = isLegal(Building::PowerPlant);
        if (chosen) reasonCategory = "energy deficit";
    }

    // 3. Priority-based selection
    if (!chosen) {
        if (c.queuePriority == GovernorPriority::Growth) {
            if (c.farms * 10 < systemCapacity(state, c.systemId)) {
                chosen = isLegal(Building::Farm);
            }
            if (!chosen) chosen = isLegal(Building::Factory);
            if (!chosen) chosen = isLegal(Building::ColonyShip);
            if (!chosen) chosen = isLegal(Building::PowerPlant);
            if (!chosen) chosen = isLegal(Building::Laboratory);
        } else if (c.queuePriority == GovernorPriority::Manufacturing) {
            chosen = isLegal(Building::Factory);
            if (!chosen) chosen = isLegal(Building::Shipyard);
            if (!chosen) chosen = isLegal(Building::FusionPlant);
            if (!chosen) chosen = isLegal(Building::PowerPlant);
            if (!chosen) chosen = isLegal(Building::Destroyer);
            if (!chosen) chosen = isLegal(Building::Laboratory);
        } else if (c.queuePriority == GovernorPriority::Research) {
            chosen = isLegal(Building::ResearchInstitute);
            if (!chosen) chosen = isLegal(Building::Laboratory);
            if (!chosen) chosen = isLegal(Building::FusionPlant);
            if (!chosen) chosen = isLegal(Building::PowerPlant);
            if (!chosen) chosen = isLegal(Building::Factory);
        } else { // Balanced
            if (c.factories <= c.laboratories) {
                chosen = isLegal(Building::Factory);
                if (!chosen) chosen = isLegal(Building::Laboratory);
            } else {
                chosen = isLegal(Building::Laboratory);
                if (!chosen) chosen = isLegal(Building::Factory);
            }
            if (!chosen) chosen = isLegal(Building::PowerPlant);
            if (!chosen) chosen = isLegal(Building::Farm);
        }
        if (chosen) reasonCategory = std::string(governorPriorityName(c.queuePriority)) + " priority";
    }

    // 4. Fallback to any legal choice
    if (!chosen) {
        for (const auto& ch : choices) {
            if (ch.blocker.empty()) {
                chosen = &ch;
                reasonCategory = "general infrastructure";
                break;
            }
        }
    }

    if (chosen) {
        std::string cleanName = chosen->name;
        const std::string suffix = " (remake infrastructure)";
        auto pos = cleanName.rfind(suffix);
        if (pos != std::string::npos && pos + suffix.size() == cleanName.size()) {
            cleanName.erase(pos);
        }
        c.queue.emplace_back(chosen->product, 0, chosen->referenceId);
        c.queueExplanation = "Auto-queued " + cleanName + " (" + reasonCategory + ").";
    } else {
        c.queueExplanation = "No legal construction choices available.";
    }
}

void syncDiplomacyState(GameState& state) {
    syncDiplomacyStateInternal(state);
}

// Internal helpers exported to the schema-13 gameplay modules (GameplayInternal.h).
static_assert(internal::QueueLimit == MaxQueue && internal::BuildingLimit == MaxBuildings &&
    internal::BalanceLimit == MaxBalance, "GameplayInternal.h limits drifted from Simulation.cpp");
namespace internal {
std::uint64_t nextRandom(std::uint64_t& state) { return botf::nextRandom(state); }
std::uint64_t bounded(std::uint64_t& state, std::uint64_t bound) { return botf::bounded(state, bound); }
EmpireState& mutableEmpire(GameState& state, int id) { return botf::mutableEmpire(state, id); }
bool known(const GameState& state, int owner, int x, int y) { return botf::known(state, owner, x, y); }
void reveal(GameState& state, int x, int y, int owner) { botf::reveal(state, x, y, owner); }
bool queueShip(GameState& state, Colony& colony, Building role) { return queueMilitaryShip(state, colony, role); }
void detachFromTaskForce(GameState& state, Fleet& fleet) { botf::detachFromTaskForce(state, fleet); }
void reconcileTaskForces(GameState& state) { botf::reconcileTaskForces(state); }
int catalogueRace(int owner) { return botf::catalogueRace(owner); }
bool playableShip(const referenceData::ShipRecord& ship) { return botf::playableShip(ship); }
Building roleProduct(int role) { return botf::roleProduct(role); }
const referenceData::ShipRecord& sourceShip(int id) { return botf::sourceShip(id); }
int constructionCost(const Construction& project) { return botf::constructionCost(project); }
std::string constructionTechBlocker(const GameState& state, const Construction& project, int owner) {
    return botf::constructionTechBlocker(state, project, owner);
}
void validateColony(const Colony& colony) { botf::validateColony(colony); }
bool colonySupplied(const GameState& state, const Fleet& fleet, int x, int y) {
    return colonySuppliedWithRangeImpl(state, fleet.ownerId, shipRange(fleet), x, y);
}
bool colonySuppliedWithRange(const GameState& state, int owner, int range, int x, int y) {
    return colonySuppliedWithRangeImpl(state, owner, range, x, y);
}
std::vector<int> activeEmpires(const GameState& state) {
    std::vector<int> ids{state.playerEmpire};
    for (const auto& rival : state.rivals) ids.push_back(rival.id);
    std::sort(ids.begin(), ids.end());
    return ids;
}
} // namespace internal
} // namespace botf

