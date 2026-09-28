// BOTF Remastered — WebAssembly bridge.
//
// A thin JSON facade over the deterministic simulation core (Simulation.h / CampaignSession.h).
// The browser client never sees GameState directly: every call returns a player-visible view that
// respects fog of war, mirroring what the Unreal reference client (BotfGameMode.cpp) exposes.
//
// Rules of this file:
//  * Never re-implement game rules. Values come from core functions (economy, preview, route,
//    supplied, constructionChoices, rushCost, populationForecast, canProposeTreaty, ...).
//    Order legality comes from dry-running botf::submit on a copy, so disabled reasons are the
//    core's own messages.
//  * Derive views from public structs so schema growth in the core flows through untouched.
//  * The single entry point is botf_call(method, argsJson) -> {"ok":bool,"result"|"error"}.

#include "Simulation.h"
#include "CampaignSession.h"
#include "ReferenceCatalogue.h"
#include "Json.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define BOTF_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define BOTF_EXPORT
#endif

namespace {
using botfweb::json::Value;
using botfweb::json::Writer;
namespace tac = botf::tactical;
namespace dip = botf::diplomacy;
namespace intel = botf::intelligence;

[[noreturn]] void fail(const std::string& message) { throw std::runtime_error(message); }

// ------------------------------------------------------------------------------------------------
// Enum name tables. Numbers are always accepted as well, so enum growth in the core keeps working.
// ------------------------------------------------------------------------------------------------
constexpr std::array<const char*, 37> CommandTypeNames{
    "AssignWorkers", "EnqueueBuilding", "MoveFleet", "Colonize", "ToggleShipyard",
    "CancelFirstConstruction", "PrioritizeLastConstruction", "MoveConstruction",
    "RemoveConstruction", "AllocateResearch", "SendGift", "OfferFriendship",
    "OfferMembership", "AttackFleet", "RetreatFleet", "Invade", "DeclareWar",
    "OfferPeace", "EstablishTradeRoute", "CancelTradeRoute", "GatherIntelligence",
    "Sabotage", "SetTacticalStance", "ProposeTradeTreaty",
    "TerraformPlanet", "CreateTaskForce", "DisbandTaskForce", "SetTaskForceOrder",
    "BuyConstruction", "ScrapStructure",
    "SubmitDiplomaticProposal", "RespondDiplomaticProposal",
    "SetIntelBudget", "SetIntelFocus",
    "SetColonyGovernor", "SetQueuePin", "ClearGovernorOverride"};
constexpr std::array<const char*, 12> BuildingNames{
    "Farm", "Factory", "Laboratory", "Shipyard", "Scout", "ColonyShip", "PowerPlant", "FusionPlant",
    "ResearchInstitute", "Destroyer", "Transport", "DefenseBattery"};
constexpr std::array<const char*, 4> StanceNames{"Balanced", "Aggressive", "Defensive", "Withdraw"};
constexpr std::array<const char*, 5> GovernorNames{"Manual", "Balanced", "Growth", "Research", "Manufacturing"};
constexpr std::array<const char*, 12> ManeuverKeys{"Engage", "Charge", "Circle", "Hold", "Retreat", "Assault",
    "Harry", "Flyby", "Strafe", "Evade", "Ram", "Hail"};
constexpr std::array<const char*, 9> TreatyKeys{"None", "NonAggression", "Friendship", "Affiliation", "Alliance",
    "Membership", "Peace", "Trade", "War"};
constexpr std::array<const char*, 4> ToneKeys{"Cordial", "Neutral", "Demanding", "Hostile"};
constexpr std::array<const char*, 4> DepartmentKeys{"General", "Economic", "Science", "Military"};
constexpr std::array<const char*, 9> PlanetClassKeys{"M", "O", "P", "L", "H", "K", "Y", "GasGiant", "Asteroids"};

template <std::size_t N>
int enumIndex(const Value& v, const std::array<const char*, N>& names, const char* what, int fallback) {
    if (v.isNull()) return fallback;
    if (v.isNumber()) return static_cast<int>(v.asInt());
    if (v.isString()) {
        for (std::size_t i = 0; i < N; ++i) if (v.text == names[i]) return static_cast<int>(i);
        try { std::size_t used = 0; const int n = std::stoi(v.text, &used); if (used == v.text.size()) return n; } catch (...) {}
    }
    fail(std::string("Unknown ") + what + ": " + v.asString());
}
template <std::size_t N>
const char* enumName(int index, const std::array<const char*, N>& names) {
    return index >= 0 && static_cast<std::size_t>(index) < N ? names[static_cast<std::size_t>(index)] : "Unknown";
}

// Empire id (1..5) -> reference catalogue race id. The same identity mapping the core uses for
// reference ship ownership (Simulation.cpp catalogueRace); kept here only for catalogue lookups.
int catalogueRaceOf(int empire) {
    static constexpr std::array<int, 6> map{-1, 1, 3, 4, 0, 2};
    return empire >= 1 && empire <= 5 ? map[static_cast<std::size_t>(empire)] : -1;
}

// ------------------------------------------------------------------------------------------------
// Bridge state
// ------------------------------------------------------------------------------------------------
struct Bridge {
    bool active = false;
    botf::GameState state;
    botf::session::TurnSession session;
    std::uint64_t version = 1;          // bumps on every mutation (preview cache key)
    std::uint64_t previewVersion = 0;
    botf::GameState previewCache;
    std::vector<tac::Encounter> lastEncounters;   // player encounters fought in the last committed turn
    std::string lastBriefing;                      // JSON of the last turn briefing
    std::string scenario = "campaign";
};
Bridge B;

void touch() { ++B.version; }
void requireGame() { if (!B.active) fail("No campaign is loaded"); }
void requireNoSession() {
    if (B.session.active) fail("Finish the battle in progress before giving new orders");
}

const botf::GameState& draft() {
    requireGame();
    if (B.previewVersion != B.version) {
        B.previewCache = botf::preview(B.state);
        B.previewVersion = B.version;
    }
    return B.previewCache;
}

const botf::System* systemById(const botf::GameState& s, int id) {
    for (const auto& sys : s.systems) if (sys.id == id) return &sys;
    return nullptr;
}
const botf::System* systemAtCell(const botf::GameState& s, int x, int y) {
    for (const auto& sys : s.systems) if (sys.x == x && sys.y == y) return &sys;
    return nullptr;
}
const botf::Fleet* fleetById(const botf::GameState& s, int id) {
    for (const auto& f : s.fleets) if (f.id == id) return &f;
    return nullptr;
}
bool knownSystem(const botf::GameState& s, const botf::System& sys) { return botf::explored(s, sys.x, sys.y); }
std::string systemLabel(const botf::GameState& s, int id) {
    const auto* sys = systemById(s, id);
    if (sys && knownSystem(s, *sys)) return sys->name;
    return "System " + std::to_string(id);
}
std::string empireLabel(int id) {
    try { return botf::empireName(id); } catch (...) { return "Empire " + std::to_string(id); }
}

// Mirrors the per-ship movement speed the core applies in resolveFleets (reference catalogue speed,
// clamped 1..3; prototype hulls move one sector). Task forces use botf::taskForceSpeed directly.
int shipSpeed(const botf::Fleet& f) {
    if (f.referenceShipId >= 0 && static_cast<std::size_t>(f.referenceShipId) < botf::referenceData::ShipCatalogue.size())
        return std::clamp(static_cast<int>(botf::referenceData::ShipCatalogue[static_cast<std::size_t>(f.referenceShipId)].speed), 1, 3);
    return 1;
}
int unitSpeed(const botf::GameState& s, const botf::Fleet& f) {
    if (f.taskForceId > 0 && botf::taskForceAt(s, f.taskForceId)) return botf::taskForceSpeed(s, f.taskForceId);
    return shipSpeed(f);
}
int maintenanceOf(const botf::Fleet& f) {
    if (f.referenceShipId >= 0 && static_cast<std::size_t>(f.referenceShipId) < botf::referenceData::ShipCatalogue.size())
        return botf::referenceData::ShipCatalogue[static_cast<std::size_t>(f.referenceShipId)].maintenance;
    return f.colonyShip ? 2 : f.shipClass == botf::Building::Destroyer ? 4 : f.shipClass == botf::Building::Transport ? 3 : 1;
}

// ------------------------------------------------------------------------------------------------
// Commands: JSON <-> botf::Command
// ------------------------------------------------------------------------------------------------
std::vector<int> intList(const Value& v) {
    std::vector<int> out;
    if (v.isArray()) for (const auto& item : v.items) out.push_back(static_cast<int>(item.asInt()));
    return out;
}

botf::Command commandFromJson(const Value& j, std::uint64_t id) {
    if (!j.isObject()) fail("Command must be an object");
    botf::Command c;
    c.id = id;
    c.type = static_cast<botf::CommandType>(enumIndex(j["type"], CommandTypeNames, "command type", -1));
    if (static_cast<int>(c.type) < 0) fail("Command type is required");
    c.systemId = j.getInt("systemId");
    c.foodWorkers = j.getInt("foodWorkers");
    c.industryWorkers = j.getInt("industryWorkers");
    c.researchWorkers = j.getInt("researchWorkers");
    c.building = static_cast<botf::Building>(enumIndex(j["building"], BuildingNames, "building", 0));
    c.fleetId = j.getInt("fleetId");
    c.targetX = j.getInt("targetX");
    c.targetY = j.getInt("targetY");
    c.queueIndex = j.getInt("queueIndex");
    c.queueDestination = j.getInt("queueDestination");
    if (j.has("researchAllocation")) {
        const auto list = intList(j["researchAllocation"]);
        if (list.size() != c.researchAllocation.size()) fail("researchAllocation needs six fields");
        std::copy(list.begin(), list.end(), c.researchAllocation.begin());
    }
    c.targetEmpire = j.getInt("targetEmpire");
    c.targetFleetId = j.getInt("targetFleetId");
    c.targetSystemId = j.getInt("targetSystemId");
    c.referenceId = j.getInt("referenceId", -1);
    c.stance = static_cast<botf::TacticalStance>(enumIndex(j["stance"], StanceNames, "stance", 0));
    c.taskForceId = j.getInt("taskForceId");
    c.planetIndex = j.getInt("planetIndex");
    c.memberFleetIds = intList(j["memberFleetIds"]);
    c.taskForceName = j.getString("taskForceName");
    c.proposalTreatyType = static_cast<dip::TreatyType>(enumIndex(j["proposalTreatyType"], TreatyKeys, "treaty type", 1));
    c.proposalDuration = static_cast<dip::TreatyDuration>(j.getInt("proposalDuration", 25));
    c.proposalTone = static_cast<dip::DiplomaticTone>(enumIndex(j["proposalTone"], ToneKeys, "tone", 0));
    c.proposalCreditOffer = j["proposalCreditOffer"].asInt(0);
    c.proposalCreditDemand = j["proposalCreditDemand"].asInt(0);
    c.proposalTargetIsMinor = j.getBool("proposalTargetIsMinor");
    c.proposalId = j["proposalId"].asUInt(0);
    c.proposalAccept = j.getBool("proposalAccept");
    c.proposalReason = j.getString("proposalReason");
    c.intelInternalSecurity = j.getInt("intelInternalSecurity");
    c.intelRivalIds = intList(j["intelRivalIds"]);
    c.intelEspionagePcts = intList(j["intelEspionagePcts"]);
    c.intelSabotagePcts = intList(j["intelSabotagePcts"]);
    c.intelDepartmentFocus = static_cast<intel::IntelDepartment>(enumIndex(j["intelDepartmentFocus"], DepartmentKeys, "department", 0));
    c.governorWorkforce = static_cast<botf::GovernorPriority>(enumIndex(j["governorWorkforce"], GovernorNames, "governor priority", 0));
    c.governorQueue = static_cast<botf::GovernorPriority>(enumIndex(j["governorQueue"], GovernorNames, "governor priority", 0));
    c.queuePinned = j.getBool("queuePinned");
    return c;
}

std::string constructionLabel(const botf::GameState& s, const botf::Construction& project) {
    if (project.referenceId >= 0 && static_cast<std::size_t>(project.referenceId) < botf::referenceData::ShipCatalogue.size()) {
        const auto& ship = botf::referenceData::ShipCatalogue[static_cast<std::size_t>(project.referenceId)];
        return std::string(ship.shipClass) + " " + ship.name;
    }
    (void)s;
    return botf::definition(project.product).name;
}

std::string describeCommand(const botf::GameState& s, const botf::Command& c) {
    const auto fleet = [&](int id) {
        const auto* f = fleetById(s, id);
        return f ? botf::fleetName(*f) + " #" + std::to_string(id) : "ship #" + std::to_string(id);
    };
    const auto sector = [](int x, int y) { return "sector " + std::to_string(x) + "," + std::to_string(y); };
    const auto place = [&](int x, int y) {
        const auto* sys = systemAtCell(s, x, y);
        return sys && knownSystem(s, *sys) ? sys->name : sector(x, y);
    };
    switch (c.type) {
    case botf::CommandType::AssignWorkers:
        return systemLabel(s, c.systemId) + ": staff " + std::to_string(c.foodWorkers) + " food / " + std::to_string(c.industryWorkers) +
            " industry / " + std::to_string(c.researchWorkers) + " research";
    case botf::CommandType::EnqueueBuilding:
        return systemLabel(s, c.systemId) + ": build " + constructionLabel(s, botf::Construction(c.building, 0, c.referenceId));
    case botf::CommandType::MoveFleet: return fleet(c.fleetId) + ": set course for " + place(c.targetX, c.targetY);
    case botf::CommandType::Colonize: return fleet(c.fleetId) + ": colonize system";
    case botf::CommandType::ToggleShipyard: return systemLabel(s, c.systemId) + ": switch shipyard power";
    case botf::CommandType::CancelFirstConstruction: return systemLabel(s, c.systemId) + ": cancel current construction";
    case botf::CommandType::PrioritizeLastConstruction: return systemLabel(s, c.systemId) + ": prioritise last queued item";
    case botf::CommandType::MoveConstruction:
        return systemLabel(s, c.systemId) + ": move queue item " + std::to_string(c.queueIndex + 1) + " to " + std::to_string(c.queueDestination + 1);
    case botf::CommandType::RemoveConstruction: return systemLabel(s, c.systemId) + ": remove queue item " + std::to_string(c.queueIndex + 1);
    case botf::CommandType::AllocateResearch: {
        std::string text = "Research allocation";
        for (int i = 0; i < botf::ResearchFields; ++i)
            text += std::string(i ? " / " : ": ") + botf::researchName(i) + " " + std::to_string(c.researchAllocation[static_cast<std::size_t>(i)]) + "%";
        return text;
    }
    case botf::CommandType::SendGift: return "Gift to " + systemLabel(s, c.systemId);
    case botf::CommandType::OfferFriendship: return "Offer friendship to " + systemLabel(s, c.systemId);
    case botf::CommandType::OfferMembership: return "Offer membership to " + systemLabel(s, c.systemId);
    case botf::CommandType::AttackFleet: return fleet(c.fleetId) + ": attack " + fleet(c.targetFleetId);
    case botf::CommandType::RetreatFleet: return fleet(c.fleetId) + ": retreat";
    case botf::CommandType::Invade: return fleet(c.fleetId) + ": invade " + systemLabel(s, c.systemId);
    case botf::CommandType::DeclareWar: return "Declare war on " + empireLabel(c.targetEmpire);
    case botf::CommandType::OfferPeace: return "Propose peace to " + empireLabel(c.targetEmpire);
    case botf::CommandType::EstablishTradeRoute:
        return "Trade route " + systemLabel(s, c.systemId) + " to " + systemLabel(s, c.targetSystemId);
    case botf::CommandType::CancelTradeRoute: return "Cancel trade route from " + systemLabel(s, c.systemId);
    case botf::CommandType::GatherIntelligence: return "Gather intelligence on " + empireLabel(c.targetEmpire);
    case botf::CommandType::Sabotage: return "Sabotage " + systemLabel(s, c.targetSystemId);
    case botf::CommandType::SetTacticalStance:
        return fleet(c.fleetId) + ": stance " + enumName(static_cast<int>(c.stance), StanceNames);
    case botf::CommandType::ProposeTradeTreaty: return "Propose trade treaty to " + empireLabel(c.targetEmpire);
    case botf::CommandType::TerraformPlanet: {
        const auto* p = botf::planetAt(s, c.systemId, c.planetIndex);
        return fleet(c.fleetId) + ": terraform " + (p ? p->name : "planet " + std::to_string(c.planetIndex + 1));
    }
    case botf::CommandType::CreateTaskForce:
        return "Form task force" + (c.taskForceName.empty() ? std::string() : " " + c.taskForceName) + " (" + std::to_string(c.memberFleetIds.size()) + " ships)";
    case botf::CommandType::DisbandTaskForce: return "Disband task force " + std::to_string(c.taskForceId);
    case botf::CommandType::SetTaskForceOrder: {
        const auto* tf = botf::taskForceAt(s, c.taskForceId);
        return (tf ? tf->name : "Task force " + std::to_string(c.taskForceId)) + ": " +
            (c.targetFleetId ? "attack " + fleet(c.targetFleetId) : "move to " + place(c.targetX, c.targetY));
    }
    case botf::CommandType::BuyConstruction: return systemLabel(s, c.systemId) + ": buy current construction";
    case botf::CommandType::ScrapStructure:
        return systemLabel(s, c.systemId) + ": scrap " + botf::definition(c.building).name;
    case botf::CommandType::SubmitDiplomaticProposal: {
        const std::string target = c.proposalTargetIsMinor ? systemLabel(s, c.targetSystemId ? c.targetSystemId : c.systemId) : empireLabel(c.targetEmpire);
        return std::string("Propose ") + dip::treatyTypeName(c.proposalTreatyType) + " to " + target;
    }
    case botf::CommandType::RespondDiplomaticProposal:
        return std::string(c.proposalAccept ? "Accept" : "Reject") + " proposal #" + std::to_string(c.proposalId);
    case botf::CommandType::SetIntelBudget: return "Intelligence budget: security " + std::to_string(c.intelInternalSecurity) + "%";
    case botf::CommandType::SetIntelFocus:
        return std::string("Intelligence focus on ") + empireLabel(c.targetEmpire) + ": " + intel::departmentName(c.intelDepartmentFocus);
    case botf::CommandType::SetColonyGovernor:
        return systemLabel(s, c.systemId) + ": governor " + botf::governorPriorityName(c.governorWorkforce) + " / " + botf::governorPriorityName(c.governorQueue);
    case botf::CommandType::SetQueuePin:
        return systemLabel(s, c.systemId) + ": " + (c.queuePinned ? "pin" : "unpin") + " queue item " + std::to_string(c.queueIndex + 1);
    case botf::CommandType::ClearGovernorOverride: return systemLabel(s, c.systemId) + ": resume governor";
    }
    return enumName(static_cast<int>(c.type), CommandTypeNames);
}

void writeCommand(Writer& w, const botf::GameState& s, const botf::Command& c) {
    w.beginObject();
    w.field("id", c.id);
    w.field("type", enumName(static_cast<int>(c.type), CommandTypeNames));
    w.field("summary", describeCommand(s, c));
    if (c.systemId) w.field("systemId", c.systemId);
    if (c.fleetId) w.field("fleetId", c.fleetId);
    if (c.taskForceId) w.field("taskForceId", c.taskForceId);
    if (c.targetFleetId) w.field("targetFleetId", c.targetFleetId);
    if (c.targetSystemId) w.field("targetSystemId", c.targetSystemId);
    if (c.targetEmpire) w.field("targetEmpire", c.targetEmpire);
    if (c.type == botf::CommandType::MoveFleet || c.type == botf::CommandType::SetTaskForceOrder) {
        w.field("targetX", c.targetX); w.field("targetY", c.targetY);
    }
    if (c.type == botf::CommandType::AssignWorkers) {
        w.field("foodWorkers", c.foodWorkers); w.field("industryWorkers", c.industryWorkers); w.field("researchWorkers", c.researchWorkers);
    }
    if (c.type == botf::CommandType::EnqueueBuilding || c.type == botf::CommandType::ScrapStructure) {
        w.field("building", enumName(static_cast<int>(c.building), BuildingNames));
        w.field("referenceId", c.referenceId);
    }
    if (c.type == botf::CommandType::AllocateResearch) {
        w.key("researchAllocation").beginArray();
        for (int v : c.researchAllocation) w.value(v);
        w.endArray();
    }
    if (c.type == botf::CommandType::RespondDiplomaticProposal) { w.field("proposalId", c.proposalId); w.field("proposalAccept", c.proposalAccept); }
    if (c.type == botf::CommandType::SubmitDiplomaticProposal) {
        w.field("proposalTargetIsMinor", c.proposalTargetIsMinor);
        w.field("proposalTreatyType", enumName(static_cast<int>(c.proposalTreatyType), TreatyKeys));
    }
    if (!c.memberFleetIds.empty()) {
        w.key("memberFleetIds").beginArray();
        for (int id : c.memberFleetIds) w.value(id);
        w.endArray();
    }
    if (c.type == botf::CommandType::SetTacticalStance || c.type == botf::CommandType::SetTaskForceOrder)
        w.field("stance", enumName(static_cast<int>(c.stance), StanceNames));
    w.endObject();
}

// Applies cancellations then the command to a copy; returns the copy. Throws the core's reason.
botf::GameState applyOrder(const botf::GameState& base, const std::vector<std::uint64_t>& cancelIds,
                           const std::optional<Value>& commandJson, std::uint64_t* acceptedId = nullptr) {
    auto copy = base;
    std::vector<std::uint64_t> ids = cancelIds;
    std::sort(ids.rbegin(), ids.rend());
    for (auto id : ids) botf::cancel(copy, id);
    if (commandJson) {
        const auto c = commandFromJson(*commandJson, copy.lastAcceptedCommand + 1);
        botf::submit(copy, c);
        if (acceptedId) *acceptedId = c.id;
    }
    return copy;
}
std::vector<std::uint64_t> idList(const Value& v) {
    std::vector<std::uint64_t> out;
    if (v.isArray()) for (const auto& item : v.items) out.push_back(item.asUInt());
    return out;
}

// ------------------------------------------------------------------------------------------------
// View helpers
// ------------------------------------------------------------------------------------------------
void writeEconomy(Writer& w, const botf::Economy& e) {
    w.beginObject();
    w.field("food", e.food); w.field("foodDeficit", e.foodDeficit); w.field("industry", e.industry);
    w.field("research", e.research); w.field("energyUsed", e.energyUsed); w.field("energyAvailable", e.energyAvailable);
    w.field("inactiveFactories", e.inactiveFactories); w.field("inactiveLabs", e.inactiveLabs);
    w.field("income", e.income); w.field("upkeep", e.upkeep); w.field("net", e.income - e.upkeep);
    w.field("shipyardPowered", e.shipyardPowered);
    w.field("poweredBatteries", e.poweredBatteries); w.field("inactiveBatteries", e.inactiveBatteries);
    w.endObject();
}

void writePlanet(Writer& w, const botf::Planet& p) {
    w.beginObject();
    w.field("index", p.index); w.field("name", p.name);
    w.field("class", enumName(static_cast<int>(p.planetClass), PlanetClassKeys));
    w.field("className", botf::planetClassName(p.planetClass));
    w.field("terraformState", botf::terraformingStateName(p.terraformState));
    w.field("terraformWork", p.terraformWork); w.field("terraformNeeded", p.terraformNeeded);
    w.field("baseCapacity", p.baseCapacity); w.field("maxCapacity", p.maxCapacity); w.field("capacity", p.currentCapacity());
    w.endObject();
}

botf::PopulationForecast growthOf(const botf::GameState& s, const botf::Colony& c, const botf::Economy& e) {
    return botf::populationForecast(c.population, e.food, botf::systemCapacity(s, c.systemId),
        10 * (c.farms + c.factories + c.laboratories), botf::PrototypeGrowthBasisPoints);
}

void writeGrowth(Writer& w, const botf::GameState& s, const botf::PopulationForecast& g) {
    w.beginObject();
    w.field("enabled", s.populationEnabled);
    w.field("min", g.minimum); w.field("max", g.maximum);
    w.field("upChance", g.upwardChances / 200.0);   // percent (core denominator 20000)
    w.endObject();
}

int constructionCostOf(const botf::GameState& s, int systemId, const botf::Construction& project) {
    try {
        for (const auto& choice : botf::constructionChoices(s, systemId))
            if (choice.product == project.product && choice.referenceId == project.referenceId) return choice.industryCost;
    } catch (...) {}
    if (project.referenceId >= 0 && static_cast<std::size_t>(project.referenceId) < botf::referenceData::ShipCatalogue.size())
        return botf::referenceData::ShipCatalogue[static_cast<std::size_t>(project.referenceId)].industryCost;
    return botf::definition(project.product).industryCost;
}

void writeColonyBrief(Writer& w, const botf::GameState& s, const botf::Colony& c) {
    const auto e = botf::economy(s, c);
    w.beginObject();
    w.field("systemId", c.systemId);
    w.field("name", systemLabel(s, c.systemId));
    w.field("population", c.population);
    w.field("capacity", botf::systemCapacity(s, c.systemId));
    w.field("morale", c.morale);
    w.key("workers").beginObject().field("food", c.foodWorkers).field("industry", c.industryWorkers).field("research", c.researchWorkers)
        .field("unassigned", c.population - c.foodWorkers - c.industryWorkers - c.researchWorkers).endObject();
    w.key("structures").beginObject().field("farms", c.farms).field("factories", c.factories).field("laboratories", c.laboratories)
        .field("shipyards", c.shipyards).field("powerPlants", c.powerPlants).field("defenseBatteries", c.defenseBatteries).endObject();
    w.field("energy", c.energy);
    w.field("shipyardEnabled", c.shipyardEnabled);
    w.key("economy"); writeEconomy(w, e);
    w.key("growth"); writeGrowth(w, s, growthOf(s, c, e));
    w.field("queueLength", static_cast<int>(c.queue.size()));
    if (!c.queue.empty()) {
        const auto& front = c.queue.front();
        const int cost = constructionCostOf(s, c.systemId, front);
        w.key("building").beginObject().field("name", constructionLabel(s, front)).field("work", front.work).field("cost", cost)
            .field("eta", e.industry > 0 ? std::max(1, (std::max(0, cost - front.work) + e.industry - 1) / e.industry) : -1).endObject();
    }
    w.field("queueBlocker", botf::constructionBlocker(s, c));
    w.key("governor").beginObject()
        .field("workforce", enumName(static_cast<int>(c.workforcePriority), GovernorNames))
        .field("queue", enumName(static_cast<int>(c.queuePriority), GovernorNames))
        .field("override", c.workforceManualOverride)
        .field("workforceExplanation", c.workforceExplanation)
        .field("queueExplanation", c.queueExplanation)
        .field("warning", c.governorWarning).endObject();
    w.endObject();
}

void writeOwnFleet(Writer& w, const botf::GameState& d, const botf::GameState& committed, const botf::Fleet& f) {
    w.beginObject();
    w.field("id", f.id);
    w.field("owner", f.ownerId);
    w.field("name", botf::fleetName(f));
    w.field("shipClass", enumName(static_cast<int>(f.shipClass), BuildingNames));
    w.field("className", botf::shipName(f.shipClass));
    w.field("referenceShipId", f.referenceShipId);
    if (f.referenceShipId >= 0 && static_cast<std::size_t>(f.referenceShipId) < botf::referenceData::ShipCatalogue.size()) {
        const auto& r = botf::referenceData::ShipCatalogue[static_cast<std::size_t>(f.referenceShipId)];
        w.field("designClass", r.shipClass); w.field("designName", r.name);
    }
    w.field("x", f.x); w.field("y", f.y); w.field("targetX", f.targetX); w.field("targetY", f.targetY);
    const auto* base = fleetById(committed, f.id);
    if (base) { w.field("committedTargetX", base->targetX); w.field("committedTargetY", base->targetY); }
    w.field("hull", f.hull); w.field("maxHull", botf::fleetMaxHull(f));
    w.field("shields", f.shields); w.field("maxShields", botf::fleetMaxShields(f));
    w.field("attack", botf::fleetAttack(f));
    w.field("range", botf::shipRange(f));
    w.field("speed", unitSpeed(d, f));
    w.field("maintenance", maintenanceOf(f));
    w.field("colonyShip", f.colonyShip);
    w.field("colonize", base ? base->colonize : f.colonize);
    w.field("taskForceId", f.taskForceId);
    w.field("stance", enumName(static_cast<int>(f.stance), StanceNames));
    w.field("attackTarget", f.attackTarget);
    w.field("retreat", f.retreat);
    w.field("invadeSystemId", f.invadeSystemId);
    w.field("terraforming", f.terraforming);
    w.field("terraformSystemId", f.terraformSystemId);
    w.field("terraformPlanetIndex", f.terraformPlanetIndex);
    // Remaining course along the core's supplied route (draft positions/targets).
    const auto path = (f.x == f.targetX && f.y == f.targetY) ? std::vector<int>{} : botf::route(d, f, f.targetX, f.targetY);
    w.key("route").beginArray();
    for (int cell : path) w.value(cell);
    w.endArray();
    const int steps = path.empty() ? 0 : static_cast<int>(path.size()) - 1;
    const int speed = unitSpeed(d, f);
    w.field("eta", steps ? (steps + speed - 1) / speed : 0);
    w.field("routeBlocked", (f.x != f.targetX || f.y != f.targetY) && path.empty());
    bool drafted = false;
    for (const auto& c : committed.pending)
        if ((c.fleetId == f.id && c.type != botf::CommandType::CreateTaskForce) ||
            (f.taskForceId && c.taskForceId == f.taskForceId && c.type == botf::CommandType::SetTaskForceOrder)) drafted = true;
    w.field("hasOrders", drafted);
    w.endObject();
}

void writeRelation(Writer& w, const botf::GameState& s, int other) {
    const auto* r = botf::majorRelation(s, s.playerEmpire, other);
    w.beginObject();
    w.field("empire", other);
    w.field("name", empireLabel(other));
    w.field("contact", r && r->contact);
    if (r && r->contact) {
        w.field("atWar", r->atWar); w.field("tradeTreaty", r->tradeTreaty); w.field("trust", r->trust);
        w.field("warStartTurn", r->warStartTurn);
    }
    w.endObject();
}

bool fleetVisibleToPlayer(const botf::GameState& s, const botf::Fleet& f) {
    return f.ownerId == s.playerEmpire || botf::visible(s, s.playerEmpire, f.x, f.y);
}

// ------------------------------------------------------------------------------------------------
// Full player view
// ------------------------------------------------------------------------------------------------
void writeView(Writer& w) {
    requireGame();
    const auto& s = B.state;
    const auto& d = draft();
    const int me = s.playerEmpire;
    w.beginObject();

    // --- meta
    w.key("meta").beginObject();
    w.field("turn", s.turn); w.field("width", s.width); w.field("height", s.height);
    w.field("seed", s.seed);
    w.field("player", me); w.field("playerName", empireLabel(me));
    w.field("ruleset", s.ruleset == botf::Ruleset::Reference ? "reference" : "prototype");
    w.field("era", static_cast<int>(s.startingEra)); w.field("eraName", botf::startingEraName(s.startingEra));
    w.field("contentVersion", botf::ContentVersion); w.field("saveVersion", botf::SaveVersion);
    w.field("outcome", s.outcome == botf::CampaignOutcome::Victory ? "victory" : s.outcome == botf::CampaignOutcome::Defeat ? "defeat" : "ongoing");
    w.field("winner", s.winnerEmpire);
    w.field("advancedRules", s.advancedRulesEnabled);
    w.field("campaignExpansion", s.campaignExpansionEnabled);
    w.field("modernEngines", s.modernEnginesActive);
    w.field("populationEnabled", s.populationEnabled);
    w.field("scenario", B.scenario);
    w.field("version", B.version);
    w.field("empireCount", static_cast<int>(s.rivals.size()) + 1);
    w.endObject();

    // --- empire summary (player only)
    int colonyNet = 0, trade = 0, ships = 0;
    for (const auto& c : d.colonies) if (c.ownerId == me) { const auto e = botf::economy(d, c); colonyNet += e.income - e.upkeep; }
    for (const auto& route : d.tradeRoutes) {
        if (route.ownerId != me) continue;
        const auto* src = botf::colonyAt(d, route.fromSystemId);
        const auto* dst = botf::colonyAt(d, route.toSystemId);
        if (!src || !dst || src->ownerId != me || dst->ownerId == me) continue;
        const auto* rel = botf::majorRelation(d, me, dst->ownerId);
        if (!rel || rel->atWar || !rel->tradeTreaty) continue;
        trade += 5 + std::min(src->population, dst->population) / 10;   // as resolved by advanceWithCombat
    }
    for (const auto& f : d.fleets) if (f.ownerId == me) ships += maintenanceOf(f);
    int researchOutput = 0, industryTotal = 0, foodTotal = 0, population = 0;
    for (const auto& c : d.colonies) if (c.ownerId == me) {
        const auto e = botf::economy(d, c);
        researchOutput += e.research; industryTotal += e.industry; foodTotal += e.food; population += c.population;
    }
    w.key("empire").beginObject();
    w.field("id", me); w.field("name", empireLabel(me));
    w.field("credits", s.credits); w.field("draftCredits", d.credits);
    w.field("research", s.research); w.field("researchReserve", s.researchReserve);
    w.field("intelligence", s.intelligence);
    w.field("researchOutput", researchOutput); w.field("industry", industryTotal); w.field("food", foodTotal); w.field("population", population);
    w.key("income").beginObject().field("colonies", colonyNet).field("trade", trade).field("ships", ships)
        .field("spend", s.credits - d.credits).field("net", colonyNet + trade - ships).endObject();
    w.key("techs").beginArray();
    for (int i = 0; i < botf::ResearchFields; ++i) {
        const auto& t = s.technologies[static_cast<std::size_t>(i)];
        const auto& dt = d.technologies[static_cast<std::size_t>(i)];
        w.beginObject();
        w.field("field", i); w.field("name", botf::researchName(i));
        w.field("level", t.level); w.field("progress", t.progress);
        w.field("allocation", dt.allocation); w.field("committedAllocation", t.allocation);
        w.field("current", botf::technologyName(s, i, t.level));
        if (t.level < botf::MaxTechLevel) {
            w.field("next", botf::technologyName(s, i, t.level + 1));
            w.field("cost", botf::researchCost(s, i, t.level + 1));
        } else { w.key("next").null(); w.field("cost", 0); }
        w.endObject();
    }
    w.endArray();
    w.endObject();

    // --- fog grid: 0 unknown, 1 explored, 2 in sensor range
    std::string fog(static_cast<std::size_t>(s.width * s.height), '0');
    for (int y = 0; y < s.height; ++y) for (int x = 0; x < s.width; ++x) {
        char v = '0';
        if (botf::explored(s, x, y)) v = '1';
        if (botf::visible(s, me, x, y)) v = '2';
        fog[static_cast<std::size_t>(y * s.width + x)] = v;
    }
    w.field("fog", fog);

    // --- systems (explored only)
    w.key("systems").beginArray();
    for (const auto& sys : s.systems) {
        if (!knownSystem(s, sys)) continue;
        const bool seen = botf::visible(s, me, sys.x, sys.y);
        const auto* col = botf::colonyAt(s, sys.id);
        const auto* minor = botf::minorAt(s, sys.id);
        w.beginObject();
        w.field("id", sys.id); w.field("x", sys.x); w.field("y", sys.y); w.field("name", sys.name);
        w.field("visible", seen);
        int owner = 0;
        if (col && (col->ownerId == me || seen)) owner = col->ownerId;
        w.field("owner", owner);
        w.field("capacity", botf::systemCapacity(d, sys.id));
        w.field("maxCapacity", botf::systemMaxCapacity(d, sys.id));
        w.key("planets").beginArray();
        for (const auto& p : botf::systemPlanets(d, sys.id)) writePlanet(w, p);
        w.endArray();
        if (minor && minor->contact[static_cast<std::size_t>(me)]) {
            w.key("minor").beginObject().field("name", minor->name).field("memberOf", minor->memberOf)
                .field("attitude", minor->attitude[static_cast<std::size_t>(me)])
                .field("friendship", minor->friendshipTurn[static_cast<std::size_t>(me)] >= 0).endObject();
        } else if (minor && seen) {
            w.key("minor").beginObject().field("name", "Unknown natives").field("memberOf", 0).field("unknown", true).endObject();
        }
        w.endObject();
    }
    w.endArray();

    // --- own colonies (draft values)
    w.key("colonies").beginArray();
    for (const auto& c : d.colonies) if (c.ownerId == me) writeColonyBrief(w, d, c);
    w.endArray();

    // --- fleets: own (draft), foreign (visible only)
    w.key("fleets").beginArray();
    for (const auto& f : d.fleets) if (f.ownerId == me) writeOwnFleet(w, d, s, f);
    w.endArray();
    w.key("contacts").beginArray();
    for (const auto& f : s.fleets) {
        if (f.ownerId == me || !fleetVisibleToPlayer(s, f)) continue;
        w.beginObject();
        w.field("id", f.id); w.field("owner", f.ownerId); w.field("ownerName", empireLabel(f.ownerId));
        w.field("name", botf::fleetName(f));
        w.field("className", botf::shipName(f.shipClass));
        w.field("shipClass", enumName(static_cast<int>(f.shipClass), BuildingNames));
        w.field("x", f.x); w.field("y", f.y);
        w.field("hull", f.hull); w.field("maxHull", botf::fleetMaxHull(f));
        w.field("shields", f.shields); w.field("maxShields", botf::fleetMaxShields(f));
        w.field("attack", botf::fleetAttack(f));
        w.field("taskForce", f.taskForceId > 0);
        const auto* r = botf::majorRelation(s, me, f.ownerId);
        w.field("atWar", r && r->atWar);
        w.endObject();
    }
    w.endArray();

    w.key("taskForces").beginArray();
    for (const auto& tf : botf::empireTaskForces(d, me)) {
        w.beginObject();
        w.field("id", tf.id); w.field("name", tf.name);
        w.key("fleetIds").beginArray(); for (int id : tf.fleetIds) w.value(id); w.endArray();
        w.field("stance", enumName(static_cast<int>(tf.stance), StanceNames));
        w.field("targetX", tf.targetX); w.field("targetY", tf.targetY); w.field("attackTarget", tf.attackTarget);
        w.field("speed", botf::taskForceSpeed(d, tf.id)); w.field("attack", botf::taskForceAttack(d, tf.id));
        w.field("hull", botf::taskForceHull(d, tf.id)); w.field("shields", botf::taskForceShields(d, tf.id));
        w.endObject();
    }
    w.endArray();

    // --- other empires
    w.key("empires").beginArray();
    for (const auto& r : s.rivals) writeRelation(w, s, r.id);
    w.endArray();

    // --- trade routes (own; foreign when both ends are in sensor range)
    w.key("tradeRoutes").beginArray();
    for (const auto& route : d.tradeRoutes) {
        const auto* a = systemById(s, route.fromSystemId);
        const auto* b = systemById(s, route.toSystemId);
        if (!a || !b || !knownSystem(s, *a) || !knownSystem(s, *b)) continue;
        if (route.ownerId != me && !(botf::visible(s, me, a->x, a->y) && botf::visible(s, me, b->x, b->y))) continue;
        w.beginObject().field("owner", route.ownerId).field("from", route.fromSystemId).field("to", route.toSystemId).endObject();
    }
    w.endArray();

    // --- drafted orders
    w.key("pending").beginArray();
    for (const auto& c : s.pending) writeCommand(w, s, c);
    w.endArray();

    // --- recent battles the player fought or saw
    w.key("battles").beginArray();
    for (std::size_t i = 0; i < s.latestBattles.size(); ++i) {
        const auto& b = s.latestBattles[i];
        const bool involved = b.firstOwner == me || b.secondOwner == me;
        if (!involved && !botf::visible(s, me, b.x, b.y)) continue;
        w.beginObject();
        w.field("index", static_cast<int>(i));
        w.field("turn", b.turn); w.field("x", b.x); w.field("y", b.y);
        w.field("first", b.firstOwner); w.field("second", b.secondOwner); w.field("winner", b.winnerOwner);
        w.field("rounds", static_cast<int>(b.rounds.size()));
        w.field("lost", static_cast<int>(b.lostFleetIds.size()));
        w.field("involved", involved);
        w.endObject();
    }
    w.endArray();

    // --- session state
    w.key("session").beginObject();
    w.field("active", B.session.active);
    w.field("battlePending", B.session.active && B.session.pending.has_value());
    w.field("battleFinished", B.session.active && B.session.pending && B.session.pending->isFinished);
    w.field("replays", static_cast<int>(B.lastEncounters.size()));
    w.endObject();

    // --- alerts (derived from core values, like the Unreal inbox)
    w.key("alerts").beginArray();
    for (const auto& c : d.colonies) {
        if (c.ownerId != me) continue;
        const auto e = botf::economy(d, c);
        if (e.foodDeficit > 0)
            w.beginObject().field("kind", "food").field("severity", "alert").field("systemId", c.systemId)
                .field("text", systemLabel(s, c.systemId) + ": food shortage of " + std::to_string(e.foodDeficit)).endObject();
        if (c.queue.empty())
            w.beginObject().field("kind", "idle").field("severity", "warn").field("systemId", c.systemId)
                .field("text", systemLabel(s, c.systemId) + ": nothing under construction").endObject();
        else if (const auto blocker = botf::constructionBlocker(d, c); !blocker.empty())
            w.beginObject().field("kind", "blocked").field("severity", "warn").field("systemId", c.systemId)
                .field("text", systemLabel(s, c.systemId) + ": " + blocker).endObject();
        if (!c.governorWarning.empty())
            w.beginObject().field("kind", "governor").field("severity", "warn").field("systemId", c.systemId)
                .field("text", systemLabel(s, c.systemId) + ": " + c.governorWarning).endObject();
    }
    if (colonyNet + trade - ships < 0)
        w.beginObject().field("kind", "treasury").field("severity", "alert")
            .field("text", "Treasury falls by " + std::to_string(-(colonyNet + trade - ships)) + " per turn").endObject();
    for (const auto& offer : s.pendingMajorOffers)
        if (offer.target == me)
            w.beginObject().field("kind", "offer").field("severity", "info").field("empire", offer.proposer)
                .field("text", empireLabel(offer.proposer) + (offer.type == botf::CommandType::OfferPeace ? " proposes peace" : " proposes a trade treaty") +
                    " (answer due turn " + std::to_string(offer.dueTurn) + ")").endObject();
    try {
        for (const auto& p : botf::incomingProposals(s, me))
            w.beginObject().field("kind", "proposal").field("severity", "info").field("empire", p.proposer.isMinor ? 0 : p.proposer.id)
                .field("text", (p.proposer.isMinor ? systemLabel(s, p.proposer.id) : empireLabel(p.proposer.id)) + " proposes " +
                    dip::treatyTypeName(p.treatyType)).endObject();
    } catch (...) {}
    for (const auto& f : d.fleets)
        if (f.ownerId == me && (f.x != f.targetX || f.y != f.targetY) && botf::route(d, f, f.targetX, f.targetY).empty())
            w.beginObject().field("kind", "route").field("severity", "warn").field("fleetId", f.id)
                .field("text", botf::fleetName(f) + " #" + std::to_string(f.id) + ": course leaves supply range").endObject();
    w.endArray();

    w.endObject();
}

// ------------------------------------------------------------------------------------------------
// Colony detail
// ------------------------------------------------------------------------------------------------
std::string dryRunError(const botf::GameState& base, const botf::Command& c) {
    try { auto copy = base; auto cmd = c; cmd.id = copy.lastAcceptedCommand + 1; botf::submit(copy, cmd); return {}; }
    catch (const std::exception& e) { return e.what(); }
}

void writeShipRecord(Writer& w, const botf::referenceData::ShipRecord& r) {
    w.field("designClass", r.shipClass); w.field("designName", r.name);
    w.field("hull", r.hull); w.field("shields", r.shields); w.field("attack", r.attack);
    w.field("range", r.range); w.field("speed", r.speed); w.field("role", r.role);
    w.field("maintenance", r.maintenance);
    w.field("description", r.description);
}

void writeColonyDetail(Writer& w, int systemId) {
    requireGame();
    const auto& s = B.state;
    const auto& d = draft();
    const auto* c = botf::colonyAt(d, systemId);
    const auto* committed = botf::colonyAt(s, systemId);
    if (!c || c->ownerId != s.playerEmpire || !committed || committed->ownerId != s.playerEmpire) fail("Not one of your colonies");
    const auto e = botf::economy(d, *c);
    w.beginObject();
    w.key("colony"); writeColonyBrief(w, d, *c);
    w.key("committedEconomy"); writeEconomy(w, botf::economy(s, *committed));
    w.key("committedWorkers").beginObject().field("food", committed->foodWorkers).field("industry", committed->industryWorkers)
        .field("research", committed->researchWorkers).endObject();
    w.key("capacityLimits").beginObject().field("food", c->farms * 10).field("industry", c->factories * 10)
        .field("research", c->laboratories * 10).endObject();
    w.field("baseCapacity", botf::systemBaseCapacity(d, systemId));
    w.field("maxCapacity", botf::systemMaxCapacity(d, systemId));
    const auto* sys = systemById(d, systemId);
    w.key("planets").beginArray();
    if (sys) for (const auto& p : sys->planets) writePlanet(w, p);
    w.endArray();

    // Build options straight from the core, with its blockers.
    const auto choices = botf::constructionChoices(d, systemId);
    w.key("choices").beginArray();
    for (const auto& ch : choices) {
        w.beginObject();
        w.field("product", enumName(static_cast<int>(ch.product), BuildingNames));
        w.field("referenceId", ch.referenceId);
        std::string name = ch.name;
        if (const auto pos = name.find(" (remake infrastructure)"); pos != std::string::npos) name.erase(pos);
        w.field("name", name);
        w.field("cost", ch.industryCost); w.field("upkeep", ch.creditUpkeep);
        w.field("blocker", ch.blocker);
        w.field("isShip", botf::isShip(ch.product));
        w.field("turns", e.industry > 0 ? std::max(1, (ch.industryCost + e.industry - 1) / e.industry) : -1);
        if (ch.referenceId >= 0 && static_cast<std::size_t>(ch.referenceId) < botf::referenceData::ShipCatalogue.size())
            writeShipRecord(w, botf::referenceData::ShipCatalogue[static_cast<std::size_t>(ch.referenceId)]);
        else {
            const auto& def = botf::definition(ch.product);
            w.field("energyCost", def.energyCost);
        }
        w.endObject();
    }
    w.endArray();

    // Queue with cumulative estimated completion (industry per turn from core economy()).
    w.key("queue").beginArray();
    int cumulative = 0;
    const std::string blocker = botf::constructionBlocker(d, *c);
    for (std::size_t i = 0; i < c->queue.size(); ++i) {
        const auto& project = c->queue[i];
        const int cost = constructionCostOf(d, systemId, project);
        const int remaining = std::max(0, cost - project.work - (i == 0 ? c->overflow : 0));
        cumulative += remaining;
        w.beginObject();
        w.field("index", static_cast<int>(i));
        w.field("product", enumName(static_cast<int>(project.product), BuildingNames));
        w.field("referenceId", project.referenceId);
        w.field("name", constructionLabel(d, project));
        w.field("isShip", botf::isShip(project.product));
        w.field("work", project.work); w.field("cost", cost); w.field("pinned", project.pinned);
        w.field("eta", e.industry > 0 ? std::max(1, (cumulative + e.industry - 1) / e.industry) : -1);
        w.endObject();
    }
    w.endArray();
    w.field("queueBlocker", blocker);
    w.field("overflow", c->overflow);
    w.field("rushCost", botf::rushCost(d, systemId));
    const std::uint64_t next = s.lastAcceptedCommand + 1;
    w.field("buyBlocker", B.session.active ? std::string("Finish the battle in progress first")
        : dryRunError(s, botf::makeBuyConstructionCommand(next, systemId)));
    {
        botf::Command toggle; toggle.type = botf::CommandType::ToggleShipyard; toggle.systemId = systemId;
        w.field("shipyardToggleBlocker", dryRunError(s, toggle));
    }
    w.key("scrap").beginArray();
    const std::array<std::pair<botf::Building, int>, 6> kinds{{
        {botf::Building::Farm, c->farms}, {botf::Building::Factory, c->factories}, {botf::Building::Laboratory, c->laboratories},
        {botf::Building::PowerPlant, c->powerPlants}, {botf::Building::Shipyard, c->shipyards}, {botf::Building::DefenseBattery, c->defenseBatteries}}};
    for (const auto& [kind, count] : kinds) {
        const auto& def = botf::definition(kind);
        w.beginObject();
        w.field("building", enumName(static_cast<int>(kind), BuildingNames));
        w.field("name", def.name); w.field("count", count);
        w.field("energyCost", def.energyCost); w.field("upkeep", def.creditUpkeep);
        w.field("blocker", B.session.active ? std::string("Finish the battle in progress first")
            : dryRunError(s, botf::makeScrapStructureCommand(next, systemId, kind)));
        w.endObject();
    }
    w.endArray();
    w.endObject();
}

// Live workforce forecast: the staffing order is applied to a copy (replacing this system's earlier
// staffing order) and the core's preview/economy/populationForecast report the outcome.
void writeWorkforcePreview(Writer& w, const Value& args) {
    requireGame();
    const int systemId = args.getInt("systemId");
    auto copy = B.state;
    std::vector<std::uint64_t> cancelIds;
    for (const auto& c : copy.pending)
        if (c.type == botf::CommandType::AssignWorkers && c.systemId == systemId) cancelIds.push_back(c.id);
    std::string error;
    try {
        for (auto it = cancelIds.rbegin(); it != cancelIds.rend(); ++it) botf::cancel(copy, *it);
        botf::Command cmd; cmd.id = copy.lastAcceptedCommand + 1; cmd.type = botf::CommandType::AssignWorkers; cmd.systemId = systemId;
        cmd.foodWorkers = args.getInt("food"); cmd.industryWorkers = args.getInt("industry"); cmd.researchWorkers = args.getInt("research");
        botf::submit(copy, cmd);
    } catch (const std::exception& ex) { error = ex.what(); }
    w.beginObject();
    w.field("error", error);
    if (error.empty()) {
        const auto view = botf::preview(copy);
        const auto* c = botf::colonyAt(view, systemId);
        if (c) {
            const auto e = botf::economy(view, *c);
            w.key("economy"); writeEconomy(w, e);
            w.key("growth"); writeGrowth(w, view, growthOf(view, *c, e));
            w.key("workers").beginObject().field("food", c->foodWorkers).field("industry", c->industryWorkers)
                .field("research", c->researchWorkers).endObject();
        }
        w.key("cancelIds").beginArray(); for (auto id : cancelIds) w.value(id); w.endArray();
    }
    w.endObject();
}

// ------------------------------------------------------------------------------------------------
// Research
// ------------------------------------------------------------------------------------------------
void writeResearch(Writer& w) {
    requireGame();
    const auto& s = B.state;
    const auto& d = draft();
    const int race = catalogueRaceOf(s.playerEmpire);
    w.beginObject();
    w.key("fields").beginArray();
    for (int i = 0; i < botf::ResearchFields; ++i) {
        const auto& t = s.technologies[static_cast<std::size_t>(i)];
        w.beginObject();
        w.field("field", i); w.field("name", botf::researchName(i));
        w.field("level", t.level); w.field("progress", t.progress);
        w.field("allocation", d.technologies[static_cast<std::size_t>(i)].allocation);
        w.field("committedAllocation", t.allocation);
        w.field("current", botf::technologyName(s, i, t.level));
        w.key("levels").beginArray();
        for (int level = 0; level <= botf::MaxTechLevel; ++level) {
            w.beginObject();
            w.field("level", level);
            w.field("name", botf::technologyName(s, i, level));
            w.field("cost", level >= 1 ? botf::researchCost(s, i, level) : 0);
            if (s.ruleset == botf::Ruleset::Reference) {
                const auto& entry = botf::referenceData::TechnologyCatalogue[static_cast<std::size_t>(i * 11 + level)];
                if (entry.field == i && entry.level == level) w.field("description", entry.description);
            }
            // Unlock preview from the reference catalogue: this empire's ships whose requirement in
            // this field is exactly this level, and structures keyed to this field/level.
            w.key("ships").beginArray();
            for (const auto& ship : botf::referenceData::ShipCatalogue) {
                if (ship.raceId != race || ship.technologyRequirements[static_cast<std::size_t>(i)] != level || level == 0) continue;
                bool othersMet = true;
                for (int f = 0; f < botf::ResearchFields; ++f)
                    if (f != i && ship.technologyRequirements[static_cast<std::size_t>(f)] > s.technologies[static_cast<std::size_t>(f)].level) othersMet = false;
                w.beginObject().field("id", ship.id).field("name", std::string(ship.shipClass) + " " + ship.name).field("cost", ship.industryCost)
                    .field("othersMet", othersMet);
                w.key("requires").beginArray(); for (int r : ship.technologyRequirements) w.value(r); w.endArray();
                w.endObject();
            }
            w.endArray();
            w.key("structures").beginArray();
            if (level > 0) for (const auto& b : botf::referenceData::BuildingCatalogue) {
                if (b.technologyField != i || b.technologyLevel != level) continue;
                if (race >= 0 && (b.excludedEmpireMask & (1 << race))) continue;
                w.beginObject().field("id", b.id).field("name", b.name).field("cost", b.industryCost).field("energy", b.energyCost).endObject();
            }
            w.endArray();
            w.endObject();
        }
        w.endArray();
        w.endObject();
    }
    w.endArray();
    int output = 0;
    for (const auto& c : d.colonies) if (c.ownerId == s.playerEmpire) output += botf::economy(d, c).research;
    w.field("output", output);
    w.field("reserve", s.researchReserve);
    // Blockers the core reports for the prototype structure/ship families.
    w.key("blockers").beginArray();
    for (int b = 0; b < static_cast<int>(BuildingNames.size()); ++b) {
        const auto blocker = botf::technologyBlocker(s, static_cast<botf::Building>(b), s.playerEmpire);
        w.beginObject().field("product", BuildingNames[static_cast<std::size_t>(b)]).field("name", botf::definition(static_cast<botf::Building>(b)).name)
            .field("blocker", blocker).endObject();
    }
    w.endArray();
    w.endObject();
}

// ------------------------------------------------------------------------------------------------
// Forecast: advance a copy one turn with the drafted orders; report only the player's own numbers.
// ------------------------------------------------------------------------------------------------
void writeForecast(Writer& w) {
    requireGame();
    const auto& s = B.state;
    w.beginObject();
    if (s.outcome != botf::CampaignOutcome::Ongoing) { w.field("error", "Campaign has ended"); w.endObject(); return; }
    try {
        auto copy = s;
        const auto report = botf::advance(copy);
        w.field("error", "");
        w.field("credits", report.creditsAfter - report.creditsBefore);
        w.field("creditsAfter", report.creditsAfter);
        w.field("research", report.researchGained);
        w.field("fleetUpkeep", report.fleetUpkeep);
        w.field("tradeIncome", report.tradeIncome);
        w.key("fields").beginArray();
        for (int i = 0; i < botf::ResearchFields; ++i) {
            const auto& before = s.technologies[static_cast<std::size_t>(i)];
            const auto& after = copy.technologies[static_cast<std::size_t>(i)];
            std::int64_t gained = after.progress - before.progress;
            for (int level = before.level + 1; level <= after.level; ++level) gained += botf::researchCost(s, i, level);
            w.beginObject().field("field", i).field("gained", gained).field("levelsGained", after.level - before.level).endObject();
        }
        w.endArray();
        w.key("colonies").beginArray();
        for (const auto& cr : report.colonies) {
            const auto* before = botf::colonyAt(s, cr.systemId);
            const auto* after = botf::colonyAt(copy, cr.systemId);
            w.beginObject();
            w.field("systemId", cr.systemId);
            w.field("completed", cr.completed);
            w.field("population", before && after && after->ownerId == before->ownerId ? after->population - before->population : 0);
            w.endObject();
        }
        w.endArray();
    } catch (const std::exception& e) {
        w.field("error", e.what());
    }
    w.endObject();
}

// ------------------------------------------------------------------------------------------------
// Diplomacy & intelligence views
// ------------------------------------------------------------------------------------------------
std::string participantName(const botf::GameState& s, dip::ParticipantId p) {
    if (!p.isMinor) return empireLabel(p.id);
    if (const auto* m = botf::minorAt(s, p.id)) return m->name;
    return systemLabel(s, p.id);
}
void writeParticipant(Writer& w, const botf::GameState& s, dip::ParticipantId p) {
    w.beginObject().field("id", p.id).field("minor", p.isMinor).field("name", participantName(s, p)).endObject();
}

void writeDiplomacy(Writer& w) {
    requireGame();
    const auto& s = B.state;
    const int me = s.playerEmpire;
    const auto self = dip::ParticipantId::major(me);
    w.beginObject();
    w.field("modern", s.campaignExpansionEnabled && s.modernEnginesActive);
    w.field("advancedRules", s.advancedRulesEnabled);
    w.key("empires").beginArray();
    for (const auto& r : s.rivals) writeRelation(w, s, r.id);
    w.endArray();
    w.key("minors").beginArray();
    for (const auto& m : s.minors) {
        if (!m.contact[static_cast<std::size_t>(me)]) continue;
        const bool waiting = std::any_of(s.diplomacy.begin(), s.diplomacy.end(), [&](const botf::DiplomaticOffer& o) {
            return o.systemId == m.systemId && o.empireId == me; });
        w.beginObject();
        w.field("systemId", m.systemId); w.field("name", m.name); w.field("system", systemLabel(s, m.systemId));
        w.field("memberOf", m.memberOf);
        w.field("attitude", m.attitude[static_cast<std::size_t>(me)]);
        w.field("friendship", m.friendshipTurn[static_cast<std::size_t>(me)] >= 0);
        w.field("offerInFlight", waiting);
        w.endObject();
    }
    w.endArray();
    // Treaty eligibility straight from the core's canProposeTreaty.
    const std::array<dip::TreatyType, 7> types{dip::TreatyType::NonAggression, dip::TreatyType::Friendship, dip::TreatyType::Affiliation,
        dip::TreatyType::Alliance, dip::TreatyType::Membership, dip::TreatyType::Peace, dip::TreatyType::Trade};
    w.key("eligibility").beginArray();
    const auto eligibility = [&](int target, bool isMinor) {
        w.beginObject().field("target", target).field("minor", isMinor);
        w.key("treaties").beginArray();
        for (auto t : types) {
            std::string why;
            bool ok = false;
            try { ok = botf::canProposeTreaty(s, me, target, isMinor, t, &why); } catch (const std::exception& e) { why = e.what(); }
            w.beginObject().field("type", enumName(static_cast<int>(t), TreatyKeys)).field("name", dip::treatyTypeName(t))
                .field("ok", ok).field("reason", ok ? std::string() : why).endObject();
        }
        w.endArray();
        w.endObject();
    };
    for (const auto& r : s.rivals) {
        const auto* rel = botf::majorRelation(s, me, r.id);
        if (rel && rel->contact) eligibility(r.id, false);
    }
    for (const auto& m : s.minors) if (m.contact[static_cast<std::size_t>(me)]) eligibility(m.systemId, true);
    w.endArray();

    w.key("proposals").beginArray();
    try {
        for (const auto& p : botf::pendingProposals(s, me)) {
            w.beginObject();
            w.field("id", p.id);
            w.field("incoming", p.target == self);
            w.key("proposer"); writeParticipant(w, s, p.proposer);
            w.key("target"); writeParticipant(w, s, p.target);
            w.field("type", enumName(static_cast<int>(p.treatyType), TreatyKeys));
            w.field("typeName", dip::treatyTypeName(p.treatyType));
            w.field("duration", dip::treatyDurationName(p.duration));
            w.field("tone", dip::toneName(p.tone));
            w.field("creditOffer", p.creditOffer); w.field("creditDemand", p.creditDemand);
            w.field("submittedTurn", p.submittedTurn); w.field("dueTurn", p.dueTurn);
            w.field("status", dip::proposalStatusName(p.status));
            const bool responded = std::any_of(s.pending.begin(), s.pending.end(), [&](const botf::Command& c) {
                return c.type == botf::CommandType::RespondDiplomaticProposal && c.proposalId == p.id; });
            w.field("responseDrafted", responded);
            w.endObject();
        }
    } catch (...) {}
    w.endArray();
    w.key("treaties").beginArray();
    try {
        for (const auto& t : botf::activeTreaties(s, me)) {
            w.beginObject();
            w.field("id", t.id);
            w.key("counterparty"); writeParticipant(w, s, t.getCounterparty(self));
            w.field("type", enumName(static_cast<int>(t.type), TreatyKeys));
            w.field("typeName", dip::treatyTypeName(t.type));
            w.field("duration", dip::treatyDurationName(t.duration));
            w.field("turnsRemaining", t.turnsRemaining); w.field("startTurn", t.startTurn);
            w.endObject();
        }
    } catch (...) {}
    w.endArray();
    w.key("events").beginArray();
    try {
        const auto events = botf::diplomaticEvents(s);
        int count = 0;
        for (auto it = events.rbegin(); it != events.rend() && count < 60; ++it) {
            if (it->partyA != self && it->partyB != self) continue;
            ++count;
            w.beginObject().field("turn", it->turn).field("type", dip::eventTypeName(it->type)).field("text", it->description);
            w.key("a"); writeParticipant(w, s, it->partyA);
            w.key("b"); writeParticipant(w, s, it->partyB);
            w.endObject();
        }
    } catch (...) {}
    w.endArray();
    w.key("majorOffers").beginArray();
    for (const auto& o : s.pendingMajorOffers) {
        if (o.proposer != me && o.target != me) continue;
        w.beginObject().field("id", o.id).field("proposer", o.proposer).field("target", o.target).field("dueTurn", o.dueTurn)
            .field("type", enumName(static_cast<int>(o.type), CommandTypeNames)).endObject();
    }
    w.endArray();
    w.key("minorOffers").beginArray();
    for (const auto& o : s.diplomacy) {
        if (o.empireId != me) continue;
        w.beginObject().field("id", o.id).field("systemId", o.systemId).field("system", systemLabel(s, o.systemId)).field("dueTurn", o.dueTurn)
            .field("type", enumName(static_cast<int>(o.type), CommandTypeNames)).endObject();
    }
    w.endArray();
    // Intelligence dossiers from successful reports.
    w.key("dossiers").beginArray();
    for (auto it = s.intelReports.rbegin(); it != s.intelReports.rend(); ++it) {
        if (it->viewer != me || !it->success) continue;
        w.beginObject().field("turn", it->turn).field("target", it->target).field("knownColonies", it->knownColonies)
            .field("estimatedCredits", it->estimatedCredits).endObject();
    }
    w.endArray();
    w.endObject();
}

void writeIntel(Writer& w) {
    requireGame();
    const auto& s = B.state;
    const auto& d = draft();
    const int me = s.playerEmpire;
    w.beginObject();
    w.field("modern", s.campaignExpansionEnabled && s.modernEnginesActive);
    w.field("reserve", s.intelligence);
    w.field("output", botf::empireIntelOutput(s, me));
    w.field("securityStockpile", botf::empireSecurityStockpile(s, me));
    const auto* draftIntel = botf::empireIntelState(d, me);
    const auto* committedIntel = botf::empireIntelState(s, me);
    const auto writeBudget = [&](const intel::EmpireIntelState* st) {
        w.beginObject();
        w.field("security", st ? st->budget.internalSecurity.percentage : 100);
        w.key("rivals").beginArray();
        if (st) for (const auto& a : st->budget.rivalAllocations)
            w.beginObject().field("id", a.rivalId).field("espionage", a.espionage.percentage).field("sabotage", a.sabotage.percentage).endObject();
        w.endArray();
        w.endObject();
    };
    w.key("budget"); writeBudget(draftIntel);
    w.key("committedBudget"); writeBudget(committedIntel);
    w.key("rivals").beginArray();
    for (const auto& r : s.rivals) {
        const auto* rel = botf::majorRelation(s, me, r.id);
        w.beginObject();
        w.field("id", r.id); w.field("name", empireLabel(r.id));
        w.field("contact", rel && rel->contact); w.field("atWar", rel && rel->atWar);
        const intel::RivalIntelState* rs = draftIntel ? draftIntel->getRivalState(r.id) : nullptr;
        if (rs) {
            w.field("focus", enumName(static_cast<int>(rs->focusDepartment), DepartmentKeys));
            w.field("focusName", intel::departmentName(rs->focusDepartment));
            w.field("espionageStockpile", rs->espionageStockpile); w.field("sabotageStockpile", rs->sabotageStockpile);
            w.key("departmentWeights").beginArray(); for (int v : rs->departmentWeights) w.value(v); w.endArray();
        }
        // Colonies of this rival currently within sensor range (sabotage targets).
        w.key("visibleColonies").beginArray();
        for (const auto& c : s.colonies) {
            if (c.ownerId != r.id) continue;
            const auto* sys = systemById(s, c.systemId);
            if (sys && botf::visible(s, me, sys->x, sys->y)) w.beginObject().field("systemId", c.systemId).field("name", sys->name).endObject();
        }
        w.endArray();
        w.endObject();
    }
    w.endArray();
    w.key("reports").beginArray();
    int count = 0;
    for (auto it = s.intelReports.rbegin(); it != s.intelReports.rend() && count < 40; ++it) {
        if (it->viewer != me) continue;
        ++count;
        w.beginObject().field("turn", it->turn).field("target", it->target).field("targetName", empireLabel(it->target))
            .field("success", it->success).field("knownColonies", it->knownColonies).field("estimatedCredits", it->estimatedCredits)
            .field("systemId", it->systemId).field("system", it->systemId ? systemLabel(s, it->systemId) : std::string()).endObject();
    }
    w.endArray();
    w.key("operations").beginArray();
    count = 0;
    for (auto it = s.intelOperations.rbegin(); it != s.intelOperations.rend() && count < 60; ++it) {
        const bool mine = it->attackerEmpireId == me;
        if (!mine && !(it->defenderEmpireId == me && it->detectedByDefender)) continue;
        ++count;
        w.beginObject();
        w.field("turn", it->turn); w.field("mine", mine);
        w.field("attacker", mine || it->detectedByDefender ? it->attackerEmpireId : 0);
        w.field("attackerName", empireLabel(it->attackerEmpireId));
        w.field("defender", it->defenderEmpireId); w.field("defenderName", empireLabel(it->defenderEmpireId));
        w.field("area", intel::intelAreaName(it->area));
        w.field("department", intel::departmentName(it->department));
        w.field("outcome", intel::outcomeName(it->outcome));
        w.field("success", it->outcome == intel::IntelOutcome::MinorSuccess || it->outcome == intel::IntelOutcome::StandardSuccess ||
            it->outcome == intel::IntelOutcome::MajorSuccess);
        w.field("creditsStolen", it->creditsStolen); w.field("researchStolen", it->researchStolen);
        w.field("sabotageSeverity", it->sabotageSeverity);
        w.field("detected", it->detectedByDefender);
        w.field("details", it->details);
        w.endObject();
    }
    w.endArray();
    w.endObject();
}

// ------------------------------------------------------------------------------------------------
// Tactical
// ------------------------------------------------------------------------------------------------
void writeCoord(Writer& w, const tac::TacticalCoord& c) { w.beginObject().field("x", c.x).field("y", c.y).endObject(); }

void writeEncounter(Writer& w, const tac::Encounter& e, int me) {
    w.beginObject();
    w.field("arenaWidth", e.arenaWidth); w.field("arenaHeight", e.arenaHeight);
    w.field("round", e.currentRound); w.field("maxRounds", e.maxRounds);
    w.field("finished", e.isFinished); w.field("winner", e.winnerOwnerId);
    w.field("sectorX", e.sectorX); w.field("sectorY", e.sectorY); w.field("turn", e.strategicTurn);
    w.field("controller", e.controllingOwnerId);
    const auto* sys = systemAtCell(B.state, e.sectorX, e.sectorY);
    w.field("location", sys && knownSystem(B.state, *sys) ? sys->name : "Sector " + std::to_string(e.sectorX) + "," + std::to_string(e.sectorY));
    w.key("ships").beginArray();
    for (const auto& ship : e.ships) {
        const bool mine = ship.ownerId == me;
        const auto order = e.getOrder(ship.fleetId);
        const bool hasPending = std::any_of(e.pendingOrders.begin(), e.pendingOrders.end(), [&](const tac::Order& o) { return o.fleetId == ship.fleetId; });
        w.beginObject();
        w.field("id", ship.fleetId); w.field("owner", ship.ownerId); w.field("ownerName", empireLabel(ship.ownerId));
        w.field("mine", mine);
        w.field("name", ship.name);
        w.field("shipClass", enumName(static_cast<int>(ship.shipClass), BuildingNames));
        w.field("referenceShipId", ship.referenceShipId);
        const bool hidden = !mine && ship.isCloaked && ship.isAlive && !ship.retreated;
        w.field("hidden", hidden);
        if (hidden) {   // A cloaked enemy is only a sensor ghost: no position or condition readout.
            w.field("hull", -1); w.field("maxHull", ship.maxHull); w.field("initialHull", ship.initialHull);
            w.field("shields", -1); w.field("maxShields", ship.maxShields); w.field("initialShields", ship.initialShields);
            w.field("beam", 0); w.field("torpedo", 0); w.field("speed", ship.speed);
            w.key("position").null();
            w.field("alive", true); w.field("active", true); w.field("retreated", false);
            w.field("canRetreat", false); w.field("canCloak", true); w.field("cloaked", true);
            w.field("groupId", ship.groupId);
            w.endObject();
            continue;
        }
        w.field("hull", ship.hull); w.field("maxHull", ship.maxHull); w.field("initialHull", ship.initialHull);
        w.field("shields", ship.shields); w.field("maxShields", ship.maxShields); w.field("initialShields", ship.initialShields);
        w.field("beam", ship.beamAttack); w.field("torpedo", ship.torpedoAttack); w.field("speed", ship.speed);
        w.key("position"); writeCoord(w, ship.position);
        w.field("alive", ship.isAlive); w.field("active", ship.activeInCombat); w.field("retreated", ship.retreated);
        w.field("canRetreat", ship.canRetreat); w.field("canCloak", ship.canCloak); w.field("cloaked", ship.isCloaked);
        w.field("groupId", ship.groupId);
        if (const auto* g = e.getGroupForShip(ship.fleetId)) w.field("group", tac::tacticalGroupClassName(g->groupClass));
        if (mine) {
            w.field("maneuver", enumName(static_cast<int>(order.maneuver), ManeuverKeys));
            w.field("target", order.targetFleetId);
            w.field("ordered", hasPending);
        }
        w.endObject();
    }
    w.endArray();
    w.key("rounds").beginArray();
    for (const auto& r : e.rounds) {
        w.beginObject();
        w.field("index", r.roundIndex); w.field("ended", r.encounterEnded); w.field("winner", r.winnerOwnerId);
        w.field("truce", r.hailedTruce);
        w.key("ships").beginArray();
        for (const auto& st : r.shipStatuses) {
            w.beginObject();
            w.field("id", st.fleetId); w.field("owner", st.ownerId);
            w.field("hullBefore", st.hullBefore); w.field("hullAfter", st.hullAfter);
            w.field("shieldBefore", st.shieldBefore); w.field("shieldAfter", st.shieldAfter);
            w.field("beam", st.beamDamageDealt); w.field("torpedo", st.torpedoDamageDealt); w.field("taken", st.totalDamageTaken);
            w.key("from"); writeCoord(w, st.positionBefore);
            w.key("via"); writeCoord(w, st.intermediatePosition);
            w.key("to"); writeCoord(w, st.positionAfter);
            w.field("maneuver", enumName(static_cast<int>(st.maneuverExecuted), ManeuverKeys));
            w.field("target", st.targetFleetId);
            w.field("destroyed", st.destroyedThisRound); w.field("retreated", st.retreatedThisRound);
            w.field("decloaked", st.decloakedThisRound); w.field("hailed", st.hailedThisRound);
            w.field("yaw", st.facingYaw);
            w.endObject();
        }
        w.endArray();
        w.key("salvos").beginArray();
        for (const auto& sv : r.salvos)
            w.beginObject().field("from", sv.attackerFleetId).field("to", sv.defenderFleetId).field("beam", sv.beamDamage)
                .field("torpedo", sv.torpedoDamage).field("absorbed", sv.absorbedByShields).endObject();
        w.endArray();
        w.key("lost").beginArray(); for (int id : r.lostFleetIds) w.value(id); w.endArray();
        w.key("retreatedIds").beginArray(); for (int id : r.retreatedFleetIds) w.value(id); w.endArray();
        w.endObject();
    }
    w.endArray();
    w.endObject();
}

// session::resume verifies a finished encounter by replaying it with each player ship's executed
// maneuver/target submitted as an explicit order. Rounds resolved with implicit (automatic) orders
// can diverge from that replay, so before each round the bridge resolves a trial copy and pins the
// engine's own choices as explicit orders; the real round then matches the verification replay.
void resolveCanonicalRound() {
    auto& s = B.session;
    if (!s.active || !s.pending) fail("No tactical encounter awaits orders");
    const int player = B.state.playerEmpire;
    for (int iteration = 0; iteration < 4; ++iteration) {
        auto trial = *s.pending;
        const auto result = trial.resolveRound();
        bool changed = false;
        for (const auto& st : result.shipStatuses) {
            const auto* ship = s.pending->getShip(st.fleetId);
            if (!ship || ship->ownerId != player || !ship->isAlive || ship->retreated || !ship->activeInCombat) continue;
            const auto current = s.pending->getOrder(st.fleetId);
            const bool explicitOrder = std::any_of(s.pending->pendingOrders.begin(), s.pending->pendingOrders.end(),
                [&](const tac::Order& o) { return o.fleetId == st.fleetId; });
            if (explicitOrder && current.maneuver == st.maneuverExecuted && current.targetFleetId == st.targetFleetId) continue;
            try { botf::session::order(s, {st.fleetId, st.targetFleetId, st.maneuverExecuted}); changed = true; }
            catch (const std::exception&) {}
        }
        if (!changed) break;
    }
    botf::session::resolveRound(s);
}

void writeManeuvers(Writer& w) {
    w.beginArray();
    for (int i = 0; i < static_cast<int>(ManeuverKeys.size()); ++i)
        w.beginObject().field("key", ManeuverKeys[static_cast<std::size_t>(i)])
            .field("name", tac::maneuverName(static_cast<tac::TacticalManeuver>(i))).endObject();
    w.endArray();
}

// ------------------------------------------------------------------------------------------------
// Turn briefing: structured, categorised events from the TurnReport and a before/after diff.
// ------------------------------------------------------------------------------------------------
struct BriefEvent {
    std::string category, severity, text;
    int systemId = 0, fleetId = 0, x = -1, y = -1, battle = -1;
};

bool startsWith(const std::string& s, const std::string& p) { return s.rfind(p, 0) == 0; }
bool contains(const std::string& s, const std::string& p) { return s.find(p) != std::string::npos; }

// Replaces "System N" tokens with the known system's name and records the first referenced id.
std::string humanise(const botf::GameState& s, const std::string& text, int* systemId) {
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        if (text.compare(i, 7, "System ") == 0 && i + 7 < text.size() && std::isdigit(static_cast<unsigned char>(text[i + 7]))) {
            std::size_t j = i + 7;
            int id = 0;
            while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) id = id * 10 + (text[j++] - '0');
            if (systemId && !*systemId) *systemId = id;
            out += systemLabel(s, id);
            i = j;
        } else out += text[i++];
    }
    return out;
}

std::string writeBriefing(const botf::GameState& before, const botf::GameState& after, const botf::TurnReport& r) {
    const int me = after.playerEmpire;
    std::vector<BriefEvent> events;
    const auto add = [&](BriefEvent e) { events.push_back(std::move(e)); };

    // Construction completions (structured).
    for (const auto& c : r.colonies) {
        if (c.completed.empty()) continue;
        BriefEvent e; e.category = "construction"; e.severity = "good"; e.systemId = c.systemId;
        e.text = systemLabel(after, c.systemId) + " completed " + c.completed + ".";
        add(e);
    }
    // Research advances (diff).
    for (int i = 0; i < botf::ResearchFields; ++i) {
        const int from = before.technologies[static_cast<std::size_t>(i)].level;
        const int to = after.technologies[static_cast<std::size_t>(i)].level;
        for (int level = from + 1; level <= to; ++level) {
            BriefEvent e; e.category = "research"; e.severity = "good";
            e.text = std::string(botf::researchName(i)) + " reached level " + std::to_string(level) + ": " + botf::technologyName(after, i, level) + ".";
            add(e);
        }
    }
    // Newly surveyed systems (diff).
    for (const auto& sys : after.systems) {
        if (botf::explored(before, sys.x, sys.y) || !botf::explored(after, sys.x, sys.y)) continue;
        BriefEvent e; e.category = "exploration"; e.severity = "info"; e.systemId = sys.id; e.x = sys.x; e.y = sys.y;
        const auto* minor = botf::minorAt(after, sys.id);
        e.text = "Surveyed " + sys.name + (sys.planets.empty() ? "" : " (" + std::to_string(sys.planets.size()) + " planets)") +
            (minor && minor->contact[static_cast<std::size_t>(me)] ? ", home of the " + minor->name : "") + ".";
        add(e);
    }
    // Colonies gained and lost (diff).
    for (const auto& c : after.colonies) {
        const auto* old = botf::colonyAt(before, c.systemId);
        if (c.ownerId == me && (!old || old->ownerId != me)) {
            BriefEvent e; e.category = "exploration"; e.severity = "good"; e.systemId = c.systemId;
            e.text = (old ? "Captured " : "Colony founded at ") + systemLabel(after, c.systemId) + ".";
            add(e);
        } else if (c.ownerId != me && old && old->ownerId == me) {
            BriefEvent e; e.category = "combat"; e.severity = "alert"; e.systemId = c.systemId;
            e.text = systemLabel(after, c.systemId) + " was lost to " + empireLabel(c.ownerId) + ".";
            add(e);
        }
    }
    // Battles and invasions (structured).
    for (std::size_t i = 0; i < r.battles.size(); ++i) {
        const auto& b = r.battles[i];
        const bool involved = b.firstOwner == me || b.secondOwner == me;
        if (!involved && !botf::visible(after, me, b.x, b.y)) continue;
        BriefEvent e; e.category = "combat"; e.x = b.x; e.y = b.y;
        e.battle = static_cast<int>(after.latestBattles.size() - r.battles.size() + i);
        const int foe = b.firstOwner == me ? b.secondOwner : b.firstOwner;
        int ownLost = 0;
        for (int id : b.lostFleetIds) for (const auto& p : b.participants) if (p.fleetId == id && p.ownerId == me) ++ownLost;
        e.severity = !involved ? "info" : b.winnerOwner == me ? "good" : "alert";
        const auto* sys = systemAtCell(after, b.x, b.y);
        const std::string where = sys && knownSystem(after, *sys) ? sys->name : "sector " + std::to_string(b.x) + "," + std::to_string(b.y);
        if (involved)
            e.text = "Battle at " + where + " against " + empireLabel(foe) + ": " +
                (b.winnerOwner == me ? "victory" : b.winnerOwner ? "defeat" : "no victor") + ", " + std::to_string(b.rounds.size()) +
                " rounds, " + std::to_string(ownLost) + " of our ships lost.";
        else
            e.text = "Sensors recorded a battle at " + where + " between " + empireLabel(b.firstOwner) + " and " + empireLabel(b.secondOwner) + ".";
        add(e);
    }
    for (const auto& inv : r.invasions) {
        if (inv.attackerOwner != me && inv.defenderOwner != me) continue;
        BriefEvent e; e.category = "combat"; e.systemId = inv.systemId;
        e.severity = (inv.attackerOwner == me) == inv.conquered ? "good" : "alert";
        e.text = "Invasion of " + systemLabel(after, inv.systemId) + ": " + empireLabel(inv.attackerOwner) + " " + std::to_string(inv.attackStrength) +
            " vs " + std::to_string(inv.defenseStrength) + ", " + (inv.conquered ? "colony taken" : "defenders held") + ".";
        add(e);
    }
    // Remaining core event strings, categorised for presentation.
    for (const auto& raw : r.events) {
        if (contains(raw, "advanced to level") || startsWith(raw, "Discovered ") || startsWith(raw, "Colony founded at ")) continue;
        BriefEvent e; e.severity = "info";
        e.text = humanise(after, raw, &e.systemId);
        if (startsWith(raw, "Fleet ")) {
            try { e.fleetId = std::stoi(raw.substr(6)); } catch (...) {}
        }
        if (contains(raw, "launched at") || contains(raw, "construction paused") || contains(raw, "repaired at")) {
            e.category = "construction";
            if (contains(raw, "paused")) e.severity = "warn";
            if (contains(raw, "launched at")) {
                const auto at = raw.rfind(" at ");
                if (at != std::string::npos) {
                    const std::string name = raw.substr(at + 4, raw.size() - at - 5);
                    for (const auto& sys : after.systems) if (sys.name == name) e.systemId = sys.id;
                }
                e.severity = "good";
            }
        } else if (contains(raw, "First contact") || contains(raw, "at war") || contains(raw, "peace") || contains(raw, "treaty") ||
                   contains(raw, "joined") || contains(raw, "friendship") || contains(raw, "membership") || contains(raw, "proposal") ||
                   contains(raw, "gift") || contains(raw, "Gift") || contains(raw, "accepted") || contains(raw, "declined")) {
            e.category = "diplomacy";
            if (contains(raw, "at war")) e.severity = "alert";
        } else if (contains(raw, "Intelligence") || contains(raw, "Sabotage") || contains(raw, "intelligence") || contains(raw, "espionage")) {
            e.category = "intel";
        } else if (contains(raw, "route paused") || contains(raw, "Terraforming")) {
            e.category = "fleets";
            if (contains(raw, "paused")) e.severity = "warn";
        } else if (contains(raw, "victory") || contains(raw, "defeat")) {
            e.category = "combat"; e.severity = contains(raw, "victory") ? "good" : "alert";
        } else {
            e.category = "economy";
            if (contains(raw, "shortage") || contains(raw, "unrest") || contains(raw, "warning")) e.severity = "warn";
            else if (contains(raw, "windfall") || contains(raw, "celebration")) e.severity = "good";
        }
        add(e);
    }
    // Modern intelligence operations resolved this turn.
    for (const auto& op : after.intelOperations) {
        if (op.turn != after.turn && op.turn != r.turn) continue;
        const bool mine = op.attackerEmpireId == me;
        if (!mine && !(op.defenderEmpireId == me && op.detectedByDefender)) continue;
        if (std::any_of(before.intelOperations.begin(), before.intelOperations.end(), [&](const intel::OperationRecord& o) { return o == op; })) continue;
        BriefEvent e; e.category = "intel";
        e.severity = mine ? (op.outcome == intel::IntelOutcome::Fiasco || op.outcome == intel::IntelOutcome::UndetectedFailure ? "warn" : "good") : "alert";
        e.text = mine ? std::string(intel::intelAreaName(op.area)) + " against " + empireLabel(op.defenderEmpireId) + ": " + intel::outcomeName(op.outcome) +
                (op.details.empty() ? "." : ". " + op.details)
            : "Counter-intelligence detected a " + std::string(intel::intelAreaName(op.area)) + " operation by " + empireLabel(op.attackerEmpireId) + ".";
        add(e);
    }
    // Diplomatic engine events from this turn involving the player.
    try {
        const auto self = dip::ParticipantId::major(me);
        const auto beforeCount = botf::diplomaticEvents(before).size();
        const auto all = botf::diplomaticEvents(after);
        for (std::size_t i = std::min(beforeCount, all.size()); i < all.size(); ++i) {
            const auto& ev = all[i];
            if (ev.partyA != self && ev.partyB != self) continue;
            if (ev.type == dip::DiplomaticEventType::ProposalSubmitted && ev.partyA == self) continue;
            BriefEvent e; e.category = "diplomacy";
            e.severity = ev.type == dip::DiplomaticEventType::WarDeclared || ev.type == dip::DiplomaticEventType::TreatyBrokenByWar ? "alert"
                : ev.type == dip::DiplomaticEventType::ProposalRejected ? "warn" : "info";
            e.text = std::string(dip::eventTypeName(ev.type)) + (ev.description.empty() ? "" : ": " + ev.description);
            add(e);
        }
    } catch (...) {}

    for (auto& e : events) {
        if (e.systemId && e.x < 0) if (const auto* sys = systemById(after, e.systemId); sys && knownSystem(after, *sys)) { e.x = sys->x; e.y = sys->y; }
        if (e.fleetId && e.x < 0) if (const auto* f = fleetById(after, e.fleetId); f && f->ownerId == me) { e.x = f->x; e.y = f->y; }
    }

    Writer w;
    w.beginObject();
    w.field("turn", after.turn);
    w.field("creditsBefore", r.creditsBefore); w.field("creditsAfter", r.creditsAfter);
    w.field("researchGained", r.researchGained); w.field("fleetUpkeep", r.fleetUpkeep); w.field("tradeIncome", r.tradeIncome);
    w.field("ordersApplied", static_cast<int>(r.appliedCommands.size()));
    w.field("stateHash", r.stateHash);
    w.field("outcome", r.outcome == botf::CampaignOutcome::Victory ? "victory" : r.outcome == botf::CampaignOutcome::Defeat ? "defeat" : "ongoing");
    w.key("colonies").beginArray();
    for (const auto& c : r.colonies) {
        w.beginObject().field("systemId", c.systemId).field("name", systemLabel(after, c.systemId)).field("completed", c.completed);
        w.key("economy"); writeEconomy(w, c.economy);
        w.endObject();
    }
    w.endArray();
    w.key("events").beginArray();
    for (const auto& e : events) {
        w.beginObject().field("category", e.category).field("severity", e.severity).field("text", e.text);
        if (e.systemId) w.field("systemId", e.systemId);
        if (e.fleetId) w.field("fleetId", e.fleetId);
        if (e.x >= 0) { w.field("x", e.x); w.field("y", e.y); }
        if (e.battle >= 0) w.field("battle", e.battle);
        w.endObject();
    }
    w.endArray();
    w.field("replays", static_cast<int>(B.lastEncounters.size()));
    w.endObject();
    return w.out;
}

// ------------------------------------------------------------------------------------------------
// Turn flow through the CampaignSession API
// ------------------------------------------------------------------------------------------------
void writeTurnStatus(Writer& w, const char* status) {
    w.beginObject();
    w.field("status", status);
    w.field("turn", B.state.turn);
    if (std::string(status) == "complete") w.key("briefing").raw(B.lastBriefing.empty() ? "null" : B.lastBriefing);
    w.endObject();
}

void continueTurn(Writer& w) {
    const auto before = B.session.baseline;
    auto report = botf::session::resume(B.session, B.state);
    touch();
    if (!report) {
        if (!B.session.pending) fail("Turn paused without an encounter");
        writeTurnStatus(w, "battle");
        return;
    }
    B.lastEncounters = B.session.completed;
    B.session = botf::session::TurnSession();
    B.lastBriefing = writeBriefing(before, B.state, *report);
    writeTurnStatus(w, "complete");
}

void endTurn(Writer& w) {
    requireGame();
    if (B.session.active) {
        if (B.session.pending && !B.session.pending->isFinished) { writeTurnStatus(w, "battle"); return; }
        continueTurn(w);
        return;
    }
    botf::session::begin(B.session, B.state);
    try { continueTurn(w); }
    catch (...) {
        if (!B.session.pending) B.session = botf::session::TurnSession();
        touch();
        throw;
    }
}

// ------------------------------------------------------------------------------------------------
// Campaign setup
// ------------------------------------------------------------------------------------------------
void queueStarterProjects(botf::GameState& s) {
    const auto home = std::find_if(s.colonies.begin(), s.colonies.end(), [&](const botf::Colony& c) { return c.ownerId == s.playerEmpire; });
    if (home == s.colonies.end()) return;
    const int homeId = home->systemId;
    for (auto b : {botf::Building::Factory, botf::Building::Farm, botf::Building::Laboratory}) {
        botf::Command c; c.id = s.lastAcceptedCommand + 1; c.type = botf::CommandType::EnqueueBuilding; c.systemId = homeId; c.building = b;
        try { botf::submit(s, c); } catch (...) {}
    }
}

void startGame(botf::GameState&& s, const std::string& scenario) {
    B = Bridge();
    B.state = std::move(s);
    B.active = true;
    B.scenario = scenario;
    touch();
}

void newCampaign(const Value& a) {
    const int empires = a.getInt("empires", 5);
    const int faction = a.getInt("faction", 1);
    const std::uint64_t seed = a["seed"].asUInt(1999);
    const int era = a.getInt("era", 1);
    const std::string ruleset = a.getString("ruleset", "reference");
    const int width = a.getInt("width", 16), height = a.getInt("height", 12), systems = a.getInt("systems", 28);
    if (!botf::startingEraValid(static_cast<botf::StartingEra>(era))) fail("Unknown starting era");
    auto s = ruleset == "prototype"
        ? botf::generateCampaign(seed, empires, width, height, systems, faction, static_cast<botf::StartingEra>(era))
        : botf::generateReferenceCampaign(seed, empires, width, height, systems, faction, static_cast<botf::StartingEra>(era));
    if (a.has("events")) s.eventsEnabled = a.getBool("events");
    if (a.has("aiAggression")) s.aiAggressionEnabled = a.getBool("aiAggression");
    botf::validate(s);
    if (a.getBool("starterQueue", true)) queueStarterProjects(s);
    startGame(std::move(s), "campaign");
}

// A fixed tactical training scenario modelled on the core's --battle-fixture: a Klingon patrol loiters
// in Sol while the Federation declares war. Both sides receive a small squadron of catalogue warships
// so the encounter is worth commanding. Ending the turn opens a player-commanded encounter.
void addDrillShips(botf::GameState& s, int owner, int x, int y, int count) {
    const int race = catalogueRaceOf(owner);
    std::vector<const botf::referenceData::ShipRecord*> picks;
    for (const auto& ship : botf::referenceData::ShipCatalogue)
        if (ship.raceId == race && ship.role >= 1 && ship.role <= 4 && ship.attack > 0) picks.push_back(&ship);
    std::sort(picks.begin(), picks.end(), [](const auto* a, const auto* b) {
        const int ta = *std::max_element(a->technologyRequirements.begin(), a->technologyRequirements.end());
        const int tb = *std::max_element(b->technologyRequirements.begin(), b->technologyRequirements.end());
        return ta != tb ? ta < tb : a->id < b->id;
    });
    for (int i = 0; i < count && i < static_cast<int>(picks.size()); ++i) {
        const auto& r = *picks[static_cast<std::size_t>(i)];
        botf::Fleet f;
        f.id = s.nextFleetId++; f.ownerId = owner; f.x = f.targetX = x; f.y = f.targetY = y;
        f.shipClass = botf::Building::Destroyer; f.referenceShipId = r.id;
        f.hull = botf::fleetMaxHull(f); f.shields = botf::fleetMaxShields(f);
        s.fleets.push_back(f);
    }
}

void newBattleDrill() {
    auto s = botf::generateReferenceCampaign(1999);
    s.eventsEnabled = false; s.aiAggressionEnabled = false;
    const auto& sol = s.systems.front();
    auto rival = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const botf::Fleet& f) { return f.ownerId != s.playerEmpire; });
    if (rival == s.fleets.end()) fail("Drill needs a rival ship");
    const int foe = rival->ownerId;
    rival->x = rival->targetX = sol.x; rival->y = rival->targetY = sol.y;
    auto& surveyed = s.rivals[0].explored;
    surveyed.push_back(sol.y * s.width + sol.x);
    std::sort(surveyed.begin(), surveyed.end());
    surveyed.erase(std::unique(surveyed.begin(), surveyed.end()), surveyed.end());
    s.majorRelations[0].contact = true;
    // Extra squadrons, validated by the core; fall back to the plain fixture if it rejects them.
    auto armed = s;
    addDrillShips(armed, s.playerEmpire, sol.x, sol.y, 3);
    addDrillShips(armed, foe, sol.x, sol.y, 3);
    try { botf::validate(armed); s = std::move(armed); } catch (...) { botf::validate(s); }
    botf::Command war; war.id = s.lastAcceptedCommand + 1; war.type = botf::CommandType::DeclareWar; war.targetEmpire = foe;
    botf::submit(s, war);
    startGame(std::move(s), "drill");
}

// ------------------------------------------------------------------------------------------------
// Static reference data
// ------------------------------------------------------------------------------------------------
void writeMeta(Writer& w) {
    w.beginObject();
    w.field("contentVersion", botf::ContentVersion);
    w.field("saveVersion", botf::SaveVersion);
    w.key("empires").beginArray();
    static constexpr const char* Homes[] = {"", "Sol", "Qo'noS", "Romulus", "Cardassia", "Ferenginar"};
    for (int id = 1; id <= 5; ++id)
        w.beginObject().field("id", id).field("name", botf::empireName(id)).field("home", Homes[id]).field("race", catalogueRaceOf(id)).endObject();
    w.endArray();
    w.key("eras").beginArray();
    for (auto era : botf::availableStartingEras())
        w.beginObject().field("id", static_cast<int>(era)).field("name", botf::startingEraName(era))
            .field("techLevel", botf::eraBaseTechLevel(era, botf::Ruleset::Reference))
            .field("prototypeTechLevel", botf::eraBaseTechLevel(era, botf::Ruleset::Prototype)).endObject();
    w.endArray();
    w.key("researchFields").beginArray();
    for (int i = 0; i < botf::ResearchFields; ++i) w.value(botf::researchName(i));
    w.endArray();
    w.field("maxTechLevel", botf::MaxTechLevel);
    w.key("maneuvers"); writeManeuvers(w);
    w.key("stances").beginArray(); for (auto n : StanceNames) w.value(n); w.endArray();
    w.key("governors").beginArray();
    for (int i = 0; i < static_cast<int>(GovernorNames.size()); ++i)
        w.beginObject().field("key", GovernorNames[static_cast<std::size_t>(i)])
            .field("name", botf::governorPriorityName(static_cast<botf::GovernorPriority>(i))).endObject();
    w.endArray();
    w.key("treaties").beginArray();
    for (int i = 1; i <= 7; ++i)
        w.beginObject().field("key", TreatyKeys[static_cast<std::size_t>(i)]).field("name", dip::treatyTypeName(static_cast<dip::TreatyType>(i))).endObject();
    w.endArray();
    w.key("durations").beginArray();
    for (int v : {25, 50, 100, -1})
        w.beginObject().field("value", v).field("name", dip::treatyDurationName(static_cast<dip::TreatyDuration>(v))).endObject();
    w.endArray();
    w.key("tones").beginArray();
    for (int i = 0; i < 4; ++i) w.beginObject().field("key", ToneKeys[static_cast<std::size_t>(i)]).field("name", dip::toneName(static_cast<dip::DiplomaticTone>(i))).endObject();
    w.endArray();
    w.key("departments").beginArray();
    for (int i = 0; i < 4; ++i) w.beginObject().field("key", DepartmentKeys[static_cast<std::size_t>(i)]).field("name", intel::departmentName(static_cast<intel::IntelDepartment>(i))).endObject();
    w.endArray();
    w.key("structures").beginArray();
    for (int i = 0; i < static_cast<int>(BuildingNames.size()); ++i) {
        const auto& def = botf::definition(static_cast<botf::Building>(i));
        w.beginObject().field("key", BuildingNames[static_cast<std::size_t>(i)]).field("name", def.name).field("industryCost", def.industryCost)
            .field("energyCost", def.energyCost).field("upkeep", def.creditUpkeep).field("isShip", botf::isShip(static_cast<botf::Building>(i))).endObject();
    }
    w.endArray();
    w.endObject();
}

void writeEncyclopedia(Writer& w) {
    w.beginObject();
    w.key("races").beginArray();
    for (const auto& r : botf::referenceData::RaceCatalogue) w.beginObject().field("id", r.id).field("name", r.name).endObject();
    w.endArray();
    w.key("technologies").beginArray();
    for (const auto& t : botf::referenceData::TechnologyCatalogue)
        w.beginObject().field("id", t.id).field("name", t.name).field("field", t.field).field("level", t.level).field("cost", t.cost)
            .field("description", t.description).endObject();
    w.endArray();
    w.key("ships").beginArray();
    for (const auto& s : botf::referenceData::ShipCatalogue) {
        w.beginObject().field("id", s.id).field("race", s.raceId).field("industryCost", s.industryCost).field("troops", s.troopCapacity);
        writeShipRecord(w, s);
        w.key("requires").beginArray(); for (int r : s.technologyRequirements) w.value(r); w.endArray();
        w.endObject();
    }
    w.endArray();
    w.key("buildings").beginArray();
    for (const auto& b : botf::referenceData::BuildingCatalogue) {
        w.beginObject().field("id", b.id).field("name", b.name).field("field", b.technologyField).field("level", b.technologyLevel)
            .field("industryCost", b.industryCost).field("energyCost", b.energyCost).field("populationCost", b.populationCost)
            .field("moraleBonus", b.moraleBonus).field("excludedEmpireMask", b.excludedEmpireMask).field("bonusType", b.bonusType)
            .field("description", b.description);
        w.key("bonusValues").beginArray(); for (int v : b.bonusValues) w.value(v); w.endArray();
        w.endObject();
    }
    w.endArray();
    w.endObject();
}

// ------------------------------------------------------------------------------------------------
// Dispatch
// ------------------------------------------------------------------------------------------------
std::string dispatch(const std::string& method, const Value& a) {
    Writer w;
    if (method == "meta") writeMeta(w);
    else if (method == "encyclopedia") writeEncyclopedia(w);
    else if (method == "newCampaign") { newCampaign(a); writeView(w); }
    else if (method == "newBattleDrill") { newBattleDrill(); writeView(w); }
    else if (method == "hasGame") w.value(B.active);
    else if (method == "view") writeView(w);
    else if (method == "submit") {
        requireGame(); requireNoSession();
        std::uint64_t id = 0;
        B.state = applyOrder(B.state, idList(a["cancelIds"]), a["command"], &id);
        touch();
        w.beginObject().field("id", id).endObject();
    } else if (method == "dryRun") {
        requireGame();
        std::string error;
        if (B.session.active) error = "Finish the battle in progress before giving new orders";
        else {
            try { applyOrder(B.state, idList(a["cancelIds"]), a.has("command") ? std::optional<Value>(a["command"]) : std::nullopt); }
            catch (const std::exception& e) { error = e.what(); }
        }
        w.beginObject().field("ok", error.empty()).field("error", error).endObject();
    } else if (method == "dryRunMany") {
        requireGame();
        w.beginArray();
        for (const auto& item : a["items"].items) {
            std::string error;
            if (B.session.active) error = "Finish the battle in progress before giving new orders";
            else {
                try { applyOrder(B.state, idList(item["cancelIds"]), item.has("command") ? std::optional<Value>(item["command"]) : std::nullopt); }
                catch (const std::exception& e) { error = e.what(); }
            }
            w.value(error);
        }
        w.endArray();
    } else if (method == "cancel") {
        requireGame(); requireNoSession();
        std::vector<std::uint64_t> ids = a.has("ids") ? idList(a["ids"]) : std::vector<std::uint64_t>{a["id"].asUInt()};
        B.state = applyOrder(B.state, ids, std::nullopt);
        touch();
        w.value(true);
    } else if (method == "route") {
        requireGame();
        const auto& d = draft();
        const auto* f = fleetById(d, a.getInt("fleetId"));
        if (!f || f->ownerId != d.playerEmpire) fail("Select one of your ships");
        const int x = a.getInt("x"), y = a.getInt("y");
        const auto path = botf::route(d, *f, x, y);
        const int speed = unitSpeed(d, *f);
        const int steps = path.empty() ? -1 : static_cast<int>(path.size()) - 1;
        w.beginObject();
        w.key("cells").beginArray(); for (int c : path) w.value(c); w.endArray();
        w.field("steps", steps); w.field("speed", speed);
        w.field("eta", steps > 0 ? (steps + speed - 1) / speed : steps == 0 ? 0 : -1);
        w.field("supplied", botf::supplied(d, *f, x, y));
        w.endObject();
    } else if (method == "supply") {
        requireGame();
        const auto& d = draft();
        const auto* f = fleetById(d, a.getInt("fleetId"));
        if (!f || f->ownerId != d.playerEmpire) fail("Select one of your ships");
        // A task force travels within its shortest-ranged member's supply range.
        const botf::Fleet* limiting = f;
        if (const auto* tf = botf::fleetTaskForce(d, f->id))
            for (int id : tf->fleetIds) if (const auto* m = fleetById(d, id); m && botf::shipRange(*m) < botf::shipRange(*limiting)) limiting = m;
        std::string grid(static_cast<std::size_t>(d.width * d.height), '0');
        for (int y = 0; y < d.height; ++y) for (int x = 0; x < d.width; ++x)
            if (botf::supplied(d, *limiting, x, y)) grid[static_cast<std::size_t>(y * d.width + x)] = '1';
        w.value(grid);
    } else if (method == "colony") writeColonyDetail(w, a.getInt("systemId"));
    else if (method == "workforcePreview") writeWorkforcePreview(w, a);
    else if (method == "research") writeResearch(w);
    else if (method == "forecast") writeForecast(w);
    else if (method == "diplomacy") writeDiplomacy(w);
    else if (method == "intel") writeIntel(w);
    else if (method == "canTerraform") {
        requireGame();
        w.value(botf::canTerraformPlanet(draft(), a.getInt("fleetId"), a.getInt("systemId"), a.getInt("planetIndex")));
    }
    else if (method == "endTurn") endTurn(w);
    else if (method == "briefing") w.raw(B.lastBriefing.empty() ? "null" : B.lastBriefing);
    else if (method == "tactical") {
        requireGame();
        if (B.session.active && B.session.pending) writeEncounter(w, *B.session.pending, B.state.playerEmpire);
        else w.null();
    } else if (method == "tacticalOrder") {
        requireGame();
        if (!B.session.active || !B.session.pending) fail("No tactical encounter awaits orders");
        const auto maneuver = static_cast<tac::TacticalManeuver>(enumIndex(a["maneuver"], ManeuverKeys, "maneuver", 0));
        const int target = a.getInt("targetFleetId");
        std::vector<int> ids;
        if (a.getBool("all")) {
            for (const auto& ship : B.session.pending->ships)
                if (ship.ownerId == B.state.playerEmpire && ship.isAlive && ship.activeInCombat && !ship.retreated &&
                    (maneuver != tac::TacticalManeuver::Retreat || ship.canRetreat)) ids.push_back(ship.fleetId);
        } else ids.push_back(a.getInt("fleetId"));
        if (ids.empty()) fail(maneuver == tac::TacticalManeuver::Retreat ? "No ship has a safe withdrawal sector" : "No active ships to command");
        auto trial = B.session;   // all-or-nothing
        for (int id : ids) {
            const auto* ship = trial.pending->getShip(id);
            int t = maneuver == tac::TacticalManeuver::Retreat ? 0 : target;
            if (t) { const auto* ts = trial.pending->getShip(t); if (!ts || !ts->isAlive || ts->retreated || !ts->activeInCombat) t = 0; }
            (void)ship;
            botf::session::order(trial, {id, t, maneuver});
        }
        B.session = std::move(trial);
        touch();
        w.value(static_cast<int>(ids.size()));
    } else if (method == "tacticalRound") {
        requireGame();
        resolveCanonicalRound();
        touch();
        writeEncounter(w, *B.session.pending, B.state.playerEmpire);
    } else if (method == "tacticalAuto") {
        requireGame();
        if (!B.session.active || !B.session.pending) fail("No tactical encounter awaits resolution");
        // Equivalent to session::autoResolve, one verified round at a time (see resolveCanonicalRound).
        for (int guard = 0; !B.session.pending->isFinished && guard <= B.session.pending->maxRounds + 1; ++guard) resolveCanonicalRound();
        touch();
        writeEncounter(w, *B.session.pending, B.state.playerEmpire);
    } else if (method == "replays") {
        w.beginArray();
        for (std::size_t i = 0; i < B.lastEncounters.size(); ++i) {
            const auto& e = B.lastEncounters[i];
            w.beginObject().field("index", static_cast<int>(i)).field("turn", e.strategicTurn).field("x", e.sectorX).field("y", e.sectorY)
                .field("rounds", static_cast<int>(e.rounds.size())).field("winner", e.winnerOwnerId).endObject();
        }
        w.endArray();
    } else if (method == "replay") {
        const int i = a.getInt("index");
        if (i < 0 || static_cast<std::size_t>(i) >= B.lastEncounters.size()) fail("No such replay");
        // The stored encounter holds the final state; each round status records before/after
        // positions and condition, which the client uses to replay the battle round by round.
        writeEncounter(w, B.lastEncounters[static_cast<std::size_t>(i)], B.state.playerEmpire);
    } else if (method == "battleReport") {
        requireGame();
        const int i = a.getInt("index");
        if (i < 0 || static_cast<std::size_t>(i) >= B.state.latestBattles.size()) fail("No such battle");
        const auto& b = B.state.latestBattles[static_cast<std::size_t>(i)];
        const int me = B.state.playerEmpire;
        if (b.firstOwner != me && b.secondOwner != me && !botf::visible(B.state, me, b.x, b.y)) fail("That battle was not observed");
        w.beginObject();
        w.field("turn", b.turn); w.field("x", b.x); w.field("y", b.y);
        w.field("first", b.firstOwner); w.field("second", b.secondOwner); w.field("winner", b.winnerOwner);
        w.key("participants").beginArray();
        for (const auto& p : b.participants) {
            std::string name = botf::shipName(p.shipClass);
            if (p.referenceShipId >= 0 && static_cast<std::size_t>(p.referenceShipId) < botf::referenceData::ShipCatalogue.size())
                name = std::string(botf::referenceData::ShipCatalogue[static_cast<std::size_t>(p.referenceShipId)].shipClass) + " " +
                    botf::referenceData::ShipCatalogue[static_cast<std::size_t>(p.referenceShipId)].name;
            w.beginObject().field("id", p.fleetId).field("owner", p.ownerId).field("name", name).field("hull", p.initialHull).field("shields", p.initialShields).endObject();
        }
        w.endArray();
        w.key("rounds").beginArray();
        for (const auto& r : b.rounds) {
            w.beginObject().field("index", r.index);
            w.key("damage").beginArray();
            for (const auto& dmg : r.damage)
                w.beginObject().field("id", dmg.fleetId).field("hullBefore", dmg.hullBefore).field("hullAfter", dmg.hullAfter)
                    .field("shieldBefore", dmg.shieldBefore).field("shieldAfter", dmg.shieldAfter).endObject();
            w.endArray();
            w.key("lost").beginArray(); for (int id : r.lostFleetIds) w.value(id); w.endArray();
            w.key("retreated").beginArray(); for (int id : r.retreatedFleetIds) w.value(id); w.endArray();
            w.endObject();
        }
        w.endArray();
        w.key("lost").beginArray(); for (int id : b.lostFleetIds) w.value(id); w.endArray();
        w.endObject();
    } else if (method == "save") {
        requireGame();
        w.beginObject();
        if (B.session.active) { w.field("kind", "session"); w.field("data", botf::session::serialize(B.session)); }
        else { w.field("kind", "game"); w.field("data", botf::serialize(B.state)); }
        w.field("turn", B.state.turn);
        w.field("player", B.state.playerEmpire);
        w.field("fingerprint", botf::fingerprint(B.state));
        w.field("scenario", B.scenario);
        w.endObject();
    } else if (method == "load") {
        const std::string data = a.getString("data");
        if (data.empty()) fail("Save is empty");
        Bridge next;
        if (startsWith(data, "BOTF_TURN_SESSION")) {
            next.session = botf::session::deserialize(data);
            next.state = next.session.baseline;
        } else next.state = botf::deserialize(data);
        botf::validate(next.state);
        next.active = true;
        next.scenario = a.getString("scenario", "campaign");
        next.lastBriefing = a.getString("briefing");
        B = std::move(next);
        touch();
        writeView(w);
    } else if (method == "fingerprint") {
        requireGame();
        w.value(botf::fingerprint(B.state));
    } else if (method == "describeTurn") {
        requireGame();
        w.value(B.lastBriefing);
    } else fail("Unknown method: " + method);
    return w.out;
}

std::string resultBuffer;
} // namespace

extern "C" {
// Returns a pointer to a UTF-8 JSON string owned by the bridge; valid until the next call.
BOTF_EXPORT const char* botf_call(const char* method, const char* args) {
    Writer w;
    try {
        const std::string m = method ? method : "";
        const std::string text = args && *args ? args : "{}";
        const Value parsed = botfweb::json::parse(text);
        const std::string result = dispatch(m, parsed);
        w.beginObject().field("ok", true).key("result").raw(result.empty() ? "null" : result).endObject();
    } catch (const std::exception& e) {
        w = Writer();
        w.beginObject().field("ok", false).field("error", e.what()).endObject();
    } catch (...) {
        w = Writer();
        w.beginObject().field("ok", false).field("error", "Unknown simulation error").endObject();
    }
    resultBuffer = std::move(w.out);
    return resultBuffer.c_str();
}
}
