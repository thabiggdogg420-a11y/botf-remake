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

// Core headers by relative path so the bridge also compiles from remaster/CMakeLists.txt unchanged.
#include "../../game/Source/BotfSim/Simulation.h"
#include "../../game/Source/BotfSim/CampaignSession.h"
#include "../../game/Source/BotfSim/ReferenceCatalogue.h"
#include "../../game/Source/BotfSim/ShipRoster.h"
#include "../../game/Source/BotfSim/MinorRaces.h"
#include "../../game/Source/BotfSim/EmpireRules.h"
#include "../../game/Source/BotfSim/Content.h"
#include "Json.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
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
constexpr std::array<const char*, 49> CommandTypeNames{
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
    "SetColonyGovernor", "SetQueuePin", "ClearGovernorOverride",
    "BuildOutpost", "UpgradeStation", "ResolveEventChoice",
    "DefineBuildTemplate", "DeleteBuildTemplate", "ApplyBuildTemplate", "SetDefaultBuildTemplate",
    "StartMegaproject", "CancelMegaproject", "RefuseCallToArms",
    "SetEmergencyProgram", "ResolveAftermath"};
constexpr std::array<const char*, 2> StationTypeNames{"Outpost", "Starbase"};
constexpr std::array<const char*, 6> FeatureKeys{"Nebula", "Wormhole", "BlackHole", "NeutronStar", "IonStorm", "SubspaceRift"};
constexpr std::array<const char*, 7> EventKeys{"Plague", "SupernovaWarning", "SubspaceRift", "TradeBoom", "DerelictVessel",
    "LaborUnrest", "ScientificBreakthrough"};
constexpr std::array<const char*, 4> VictoryKeys{"Conquest", "Domination", "Scientific", "Diplomatic"};
constexpr std::array<const char*, 10> ChronicleKeys{"construction", "research", "exploration", "colonization", "combat",
    "diplomacy", "intel", "events", "stations", "victory"};
constexpr std::array<const char*, 13> BuildingNames{
    "Farm", "Factory", "Laboratory", "Shipyard", "Scout", "ColonyShip", "PowerPlant", "FusionPlant",
    "ResearchInstitute", "Destroyer", "Transport", "DefenseBattery", "DilithiumRefinery"};
constexpr std::array<const char*, 4> StanceNames{"Balanced", "Aggressive", "Defensive", "Withdraw"};
constexpr std::array<const char*, 5> GovernorNames{"Manual", "Balanced", "Growth", "Research", "Manufacturing"};
constexpr std::array<const char*, 12> ManeuverKeys{"Engage", "Charge", "Circle", "Hold", "Retreat", "Assault",
    "Harry", "Flyby", "Strafe", "Evade", "Ram", "Hail"};
constexpr std::array<const char*, 6> SubsystemKeys{"none", "weapons", "engines", "shields", "sensors", "warpCore"};
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
    // Combat sandbox (tactical rules 4): a standalone WEGO encounter the battle scene can drive before campaigns
    // switch rules 4 on (that needs GameplayRules::wegoCombat, outside this bridge). Never touches the campaign.
    std::optional<tac::Encounter> sandbox;
    botf::GameState sandboxState;
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
// Catalogue designation of a system the player has not explored: deterministic from the id, carries no real name.
std::string uncharted_designation(int id) {
    char buf[24];
    std::snprintf(buf, sizeof buf, "FGC-%03d", id);
    return buf;
}
std::string empireLabel(int id) {
    try { return botf::empireName(id); } catch (...) { return "Empire " + std::to_string(id); }
}

// Mirrors the per-ship movement speed the core applies in resolveFleets (reference catalogue speed,
// clamped 1..3; prototype hulls move one sector). Task forces use botf::taskForceSpeed directly.
int shipSpeed(const botf::Fleet& f) {
    if (const auto* ship = botf::roster::findShip(f.referenceShipId))
        return std::clamp(static_cast<int>(ship->speed), 1, 3);
    return 1;
}
int unitSpeed(const botf::GameState& s, const botf::Fleet& f) {
    if (f.taskForceId > 0 && botf::taskForceAt(s, f.taskForceId)) return botf::taskForceSpeed(s, f.taskForceId);
    return shipSpeed(f);
}
int maintenanceOf(const botf::Fleet& f) {
    if (const auto* ship = botf::roster::findShip(f.referenceShipId))
        return ship->maintenance;
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
    // Schema-13 gameplay layer
    c.stationId = j.getInt("stationId");
    c.eventId = j.getInt("eventId");
    c.eventOption = j.getInt("eventOption");
    c.templateId = j.getInt("templateId");
    c.templateName = j.getString("templateName");
    if (j["templateItems"].isArray())
        for (const auto& item : j["templateItems"].items) {
            botf::Construction project(static_cast<botf::Building>(enumIndex(item["product"], BuildingNames, "building", 0)), 0,
                item.getInt("referenceId", -1));
            c.templateItems.push_back(project);
        }
    return c;
}

std::string constructionLabel(const botf::GameState& s, const botf::Construction& project) {
    if (botf::roster::validShipId(project.referenceId)) {
        const auto& ship = *botf::roster::findShip(project.referenceId);
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
    case botf::CommandType::SetEmergencyProgram:
        return systemLabel(s, c.systemId) + (c.eventOption == 0 ? ": lift the emergency program" : ": " + std::string(botf::empire_rules::programName(c.eventOption)));
    case botf::CommandType::ResolveAftermath:
        return systemLabel(s, c.systemId) + ": " + botf::empire_rules::aftermathName(c.eventOption);
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
    case botf::CommandType::RefuseCallToArms:
        return "Refuse " + empireLabel(c.targetEmpire) + "'s call to arms against " + empireLabel(c.callAggressor);
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
    case botf::CommandType::BuildOutpost: return fleet(c.fleetId) + ": assemble an outpost";
    case botf::CommandType::UpgradeStation: {
        const auto* st = botf::stationById(s, c.stationId);
        return (st ? botf::stationName(*st) : "Station " + std::to_string(c.stationId)) + ": upgrade to starbase, funded by " + systemLabel(s, c.systemId);
    }
    case botf::CommandType::ResolveEventChoice: {
        for (const auto& e : s.galacticEvents)
            if (e.id == c.eventId) {
                const auto options = botf::eventChoices(e.kind);
                const std::string label = c.eventOption >= 0 && static_cast<std::size_t>(c.eventOption) < options.size()
                    ? options[static_cast<std::size_t>(c.eventOption)].label : "option " + std::to_string(c.eventOption + 1);
                return std::string(botf::galacticEventName(e.kind)) + ": " + label;
            }
        return "Event " + std::to_string(c.eventId) + ": option " + std::to_string(c.eventOption + 1);
    }
    case botf::CommandType::DefineBuildTemplate:
        return (c.templateId ? "Update build template \"" : "Create build template \"") + c.templateName + "\" (" +
            std::to_string(c.templateItems.size()) + " items)";
    case botf::CommandType::DeleteBuildTemplate: {
        const auto* t = botf::buildTemplateById(s, c.templateId);
        return "Delete build template " + (t ? "\"" + t->name + "\"" : "#" + std::to_string(c.templateId));
    }
    case botf::CommandType::ApplyBuildTemplate: {
        const auto* t = botf::buildTemplateById(s, c.templateId);
        return systemLabel(s, c.systemId) + ": apply template " + (t ? "\"" + t->name + "\"" : "#" + std::to_string(c.templateId));
    }
    case botf::CommandType::SetDefaultBuildTemplate: {
        if (!c.templateId) return "Clear the default build template";
        const auto* t = botf::buildTemplateById(s, c.templateId);
        return "Default build template: " + (t ? "\"" + t->name + "\"" : "#" + std::to_string(c.templateId));
    }
    case botf::CommandType::StartMegaproject: return systemLabel(s, c.systemId) + ": begin the " + botf::megaprojectName(s);
    case botf::CommandType::CancelMegaproject: return std::string("Abandon the ") + botf::megaprojectName(s);
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
    if (c.stationId) w.field("stationId", c.stationId);
    if (c.type == botf::CommandType::ResolveEventChoice) { w.field("eventId", c.eventId); w.field("eventOption", c.eventOption); }
    if (c.templateId) w.field("templateId", c.templateId);
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
    w.field("refineryPowered", e.refineryPowered);
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
    if (const auto* ship = botf::roster::findShip(project.referenceId))
        return ship->industryCost;
    return botf::definition(project.product).industryCost;
}

// Schema-15 empire rules at one colony: the dilithium refinery and the empire's ship slots.
void writeEmpireRulesColony(Writer& w, const botf::GameState& s, const botf::Colony& c) {
    namespace er = botf::empire_rules;
    w.key("empireRules").beginObject();
    w.field("enabled", er::enabled(s));
    if (er::enabled(s)) {
        const auto d = er::dilithiumStatus(s, c.ownerId);
        w.key("dilithium").beginObject()
            .field("deposit", er::hasDeposit(s, c.systemId)).field("refinery", c.refineries > 0)
            .field("refineryPowered", botf::economy(s, c).refineryPowered)
            .field("empireRefineries", d.refineries).field("empirePowered", d.powered)
            .field("slots", d.slots).field("slotsUsed", d.used)
            .field("klingonBonusPercent", er::shipIndustryBonusPercent(s, c))
            .field("blocker", er::refineryBlocker(s, c)).endObject();
        const auto mine = er::raceProgram(c.ownerId);
        const int id = static_cast<int>(mine);
        w.key("program").beginObject()
            .field("id", id).field("name", er::programName(id)).field("effect", er::programEffect(id))
            .field("startCost", er::programStartCost(c, mine))
            .field("upkeep", std::max(1, c.population / 10) * (mine == er::Program::Tribunal ? 2 : 1) * (er::sustained(id) ? 1 : 0))
            .field("activeId", c.program).field("activeName", er::programName(c.program))
            .field("turnsLeft", c.programTurns).field("blocker", er::programBlocker(s, c, mine)).endObject();
    }
    w.endObject();
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
        .field("shipyards", c.shipyards).field("powerPlants", c.powerPlants).field("fusionPlants", c.fusionPlants)
        .field("defenseBatteries", c.defenseBatteries).field("refineries", c.refineries).endObject();
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
    writeEmpireRulesColony(w, s, c);
    w.key("governor").beginObject()
        .field("workforce", enumName(static_cast<int>(c.workforcePriority), GovernorNames))
        .field("queue", enumName(static_cast<int>(c.queuePriority), GovernorNames))
        .field("override", c.workforceManualOverride)
        .field("workforceExplanation", c.workforceExplanation)
        .field("queueExplanation", c.queueExplanation)
        .field("warning", c.governorWarning).endObject();
    w.endObject();
}

// Expanded ship roster (docs/SHIP-ROSTER.md): enum name tables mirror ShipRosterTypes.h.
namespace rosterNames {
constexpr std::array<const char*, 28> Faction{"Federation", "Klingon", "Romulan", "Cardassian", "Ferengi", "Dominion", "Borg",
    "Breen", "Tholian", "Gorn", "Orion", "Hirogen", "Kazon", "Vidiian", "Suliban", "Xindi", "Species8472", "Krenim", "Malon",
    "SonA", "Maquis", "Reman", "Devore", "Andorian", "Vulcan", "Bajoran", "Nausicaan", "Talarian"};
constexpr std::array<const char*, 19> HullRole{"Scout", "Science", "Escort", "Frigate", "Destroyer", "Raider", "LightCruiser",
    "Cruiser", "HeavyCruiser", "StrikeCruiser", "Carrier", "Battlecruiser", "Explorer", "Battleship", "Dreadnought", "Colony",
    "Transport", "Station", "Special"};
constexpr std::array<const char*, 8> Era{"Enterprise", "Original", "Movie", "NextGeneration", "DominionWar", "Nemesis", "Kelvin", "Future"};
constexpr std::array<const char*, 3> Source{"Canon", "ModDerived", "Kelvin"};
constexpr std::array<const char*, 21> Weapon{"None", "PhaseCannon", "PhaserBank", "PhaserArray", "PulsePhaser", "Disruptor",
    "DisruptorCannon", "PlasmaCannon", "PolaronBeam", "CuttingBeam", "EnergyDampener", "TholianWeb", "BioEnergy", "TemporalWeapon",
    "SpatialTorpedo", "PhotonicTorpedo", "PhotonTorpedo", "QuantumTorpedo", "PlasmaTorpedo", "TricobaltDevice", "Torpedo"};
constexpr std::array<const char*, 5> Arc{"Forward", "ForwardWide", "Broadside", "Aft", "All"};
} // namespace rosterNames

void writeRosterWeapons(Writer& w, const botf::roster::RosterClass& c) {
    w.key("weapons").beginArray();
    for (int i = 0; i < c.weaponCount && i < static_cast<int>(c.weapons.size()); ++i) {
        const auto& m = c.weapons[static_cast<std::size_t>(i)];
        if (m.type == botf::roster::WeaponType::None) continue;
        const std::string type = enumName(static_cast<int>(m.type), rosterNames::Weapon);
        w.beginObject().field("type", type).field("key", "weapon." + type).field("count", m.count)
            .field("arc", enumName(static_cast<int>(m.arc), rosterNames::Arc))
            .field("torpedo", botf::roster::torpedoWeapon(m.type)).endObject();
    }
    w.endArray();
}

// Roster detail for an expanded class id; writes "roster": null for legacy ids.
void writeRosterDetail(Writer& w, int id) {
    const auto* c = botf::roster::findRosterClass(id);
    w.key("roster");
    if (!c) { w.null(); return; }
    w.beginObject();
    w.field("assetKey", c->assetKey);
    w.field("faction", enumName(static_cast<int>(c->faction), rosterNames::Faction));
    w.field("raceId", c->raceId); w.field("minorRaceId", c->minorRaceId);
    w.field("hullRole", enumName(static_cast<int>(c->hullRole), rosterNames::HullRole));
    w.field("era", enumName(static_cast<int>(c->era), rosterNames::Era));
    w.field("tier", c->tier);
    w.field("sourceKind", enumName(static_cast<int>(c->sourceKind), rosterNames::Source));
    w.field("source", c->source);
    w.field("buildable", c->buildable);
    w.field("buildTurns", c->buildTurns);
    w.field("torpedoSharePercent", c->torpedoSharePercent);
    w.field("sensors", c->sensors); w.field("cloak", c->cloak);
    writeRosterWeapons(w, *c);
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
    if (botf::roster::validShipId(f.referenceShipId)) {
        const auto& r = *botf::roster::findShip(f.referenceShipId);
        w.field("designClass", r.shipClass); w.field("designName", r.name);
    }
    writeRosterDetail(w, f.referenceShipId);
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
    w.field("buildingOutpost", f.buildingOutpost);
    w.field("outpostProgress", f.outpostProgress);
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

bool listHas(const std::vector<int>& list, int id) { return std::find(list.begin(), list.end(), id) != list.end(); }

void writeRelation(Writer& w, const botf::GameState& s, int other) {
    const int me = s.playerEmpire;
    const auto* r = botf::majorRelation(s, me, other);
    w.beginObject();
    w.field("empire", other);
    w.field("name", empireLabel(other));
    w.field("contact", r && r->contact);
    bool eliminated = false;
    try { eliminated = botf::empireEliminated(s, other); } catch (...) {}
    w.field("eliminated", eliminated);
    if (r && r->contact) {
        w.field("atWar", r->atWar); w.field("tradeTreaty", r->tradeTreaty); w.field("trust", r->trust);
        w.field("warStartTurn", r->warStartTurn);
        w.field("personality", dip::personalityName(dip::personalityForEmpire(other)));
        try {
            w.field("allied", listHas(botf::alliedEmpires(s, me), other));
            w.field("sensorSharing", listHas(botf::sensorSharingEmpires(s, me), other));
            w.field("treatyForbidsAttack", botf::treatyForbidsAttack(s, me, other));
        } catch (...) {}
        // Relation memory and galaxy-wide standing (modern diplomacy, see DIPLOMACY.md §3.3, §5).
        const auto& ds = s.diplomacyEngineState;
        w.key("memory").beginObject();
        w.field("theirGrievances", ds.grievances(other, me));
        w.field("ourGrievances", ds.grievances(me, other));
        w.field("theirBetrayals", ds.betrayals(other));
        if (const auto* m = ds.findMemory(me, other)) {
            w.field("lastWarTurn", m->lastWarTurn); w.field("lastPeaceTurn", m->lastPeaceTurn);
            w.field("ourRefusedProposals", m->refusalsFrom(me));
            w.field("theirRefusedProposals", m->refusalsFrom(other));
            w.field("lastProposalFromThem", m->lastProposalFrom(other));
        }
        w.endObject();
    }
    w.endObject();
}

bool fleetVisibleToPlayer(const botf::GameState& s, const botf::Fleet& f) {
    return f.ownerId == s.playerEmpire || botf::visible(s, s.playerEmpire, f.x, f.y);
}

// ------------------------------------------------------------------------------------------------
// Schema-13 gameplay layer: stations, anomalies, galactic events, templates, victory
// ------------------------------------------------------------------------------------------------
const char* featureEffect(botf::GalaxyFeatureType t) {
    switch (t) {
    case botf::GalaxyFeatureType::Nebula:
        return "Sensor-blind: only ships inside see into it, and they see only their own sector. Shields fail inside, so battles here are hull-only.";
    case botf::GalaxyFeatureType::Wormhole:
        return "Instant link: once explored, routes may step straight to the paired endpoint. Supply flows through to the exit.";
    case botf::GalaxyFeatureType::BlackHole: return "Impassable. Routes steer around it.";
    case botf::GalaxyFeatureType::NeutronStar:
        return "Hazard: ships here lose 10% of their hull (never below 1) and half their shields each turn. Routes avoid it when they can.";
    case botf::GalaxyFeatureType::IonStorm:
        return "Drifting hazard: moves one sector every 2 turns. Ships inside lose their shields and 5% hull each turn.";
    case botf::GalaxyFeatureType::SubspaceRift: return "Impassable until it collapses.";
    }
    return "";
}
const char* featureSurvey(botf::GalaxyFeatureType t) {
    switch (t) {
    case botf::GalaxyFeatureType::Nebula: return "Energy research";
    case botf::GalaxyFeatureType::Wormhole: return "Propulsion research, intelligence and the exit charted";
    case botf::GalaxyFeatureType::BlackHole: return "Propulsion research";
    case botf::GalaxyFeatureType::NeutronStar: return "Construction research";
    case botf::GalaxyFeatureType::IonStorm: return "Energy research";
    case botf::GalaxyFeatureType::SubspaceRift: return "Computers research";
    }
    return "";
}

void writeStationStats(Writer& w, const botf::StationStats& st) {
    w.field("maxHull", st.maxHull); w.field("maxShields", st.maxShields);
    w.field("beam", st.beam); w.field("torpedo", st.torpedo);
    w.field("supplyRadius", st.supplyRadius); w.field("sensorRadius", st.sensorRadius);
    w.field("upkeep", st.upkeep); w.field("repairPercent", st.repairPercent);
}

void writeStation(Writer& w, const botf::GameState& s, const botf::Station& st, int me) {
    w.beginObject();
    w.field("id", st.id); w.field("owner", st.ownerId); w.field("ownerName", empireLabel(st.ownerId));
    w.field("mine", st.ownerId == me);
    w.field("name", botf::stationName(st));
    w.field("type", enumName(static_cast<int>(st.type), StationTypeNames));
    w.field("typeName", botf::stationTypeName(st.type));
    w.field("x", st.x); w.field("y", st.y);
    w.field("hull", st.hull); w.field("shields", st.shields);
    w.field("builtTurn", st.builtTurn);
    writeStationStats(w, botf::stationStats(s, st));
    if (st.ownerId == me) {
        if (const auto* u = botf::stationUpgradeFor(s, st.id)) {
            w.key("upgrade").beginObject().field("fundingSystemId", u->fundingSystemId).field("funding", systemLabel(s, u->fundingSystemId))
                .field("work", u->work).field("cost", botf::stationUpgradeCost(s, me).industry).endObject();
        }
    }
    w.endObject();
}

void writeFeature(Writer& w, const botf::GameState& s, const botf::GalaxyFeature& f, int me) {
    w.beginObject();
    w.field("id", f.id);
    w.field("type", enumName(static_cast<int>(f.type), FeatureKeys));
    w.field("name", botf::galaxyFeatureName(f.type));
    w.field("x", f.x); w.field("y", f.y);
    if (f.pairId) {
        w.field("pairId", f.pairId);
        // The exit is known once it is explored (a wormhole survey charts it).
        for (const auto& other : s.galaxyFeatures)
            if (other.id == f.pairId && botf::explored(s, other.x, other.y))
                w.key("exit").beginObject().field("x", other.x).field("y", other.y).endObject();
    }
    if (f.clusterId) w.field("clusterId", f.clusterId);
    if (f.type == botf::GalaxyFeatureType::IonStorm) {
        w.field("driftX", f.driftX); w.field("driftY", f.driftY);
        w.key("cells").beginArray();
        for (const auto& [cx, cy] : botf::ionStormCells(s, f)) w.value(cy * s.width + cx);
        w.endArray();
    }
    if (f.expiresTurn) w.field("expiresTurn", f.expiresTurn);
    w.field("surveyed", me >= 0 && me < 31 && (f.surveyedMask & (1 << me)) != 0);
    w.field("effect", featureEffect(f.type));
    w.field("surveyReward", featureSurvey(f.type));
    w.field("passable", f.type != botf::GalaxyFeatureType::BlackHole && f.type != botf::GalaxyFeatureType::SubspaceRift);
    w.endObject();
}

void writeGalacticEvent(Writer& w, const botf::GameState& s, const botf::GalacticEvent& e) {
    w.beginObject();
    w.field("id", e.id);
    w.field("kind", enumName(static_cast<int>(e.kind), EventKeys));
    w.field("name", botf::galacticEventName(e.kind));
    w.field("empire", e.empireId);
    if (e.systemId) { w.field("systemId", e.systemId); w.field("system", systemLabel(s, e.systemId)); }
    if (e.x >= 0) { w.field("x", e.x); w.field("y", e.y); }
    w.field("startTurn", e.startTurn); w.field("endTurn", e.endTurn);
    w.field("deadline", e.choiceDeadline);
    w.field("choice", e.choice); w.field("choiceApplied", e.choiceApplied);
    w.field("magnitude", e.magnitude);
    w.field("description", botf::describeEvent(s, e));
    int drafted = -1;
    std::vector<std::uint64_t> draftedIds;
    for (const auto& c : s.pending)
        if (c.type == botf::CommandType::ResolveEventChoice && c.eventId == e.id) { drafted = c.eventOption; draftedIds.push_back(c.id); }
    w.field("drafted", drafted);
    w.key("draftIds").beginArray(); for (auto id : draftedIds) w.value(id); w.endArray();
    const bool open = e.choiceDeadline > 0 && e.choice < 0 && e.empireId == s.playerEmpire;
    w.field("awaitingChoice", open);
    w.field("defaultChoice", e.choiceDeadline > 0 ? botf::eventDefaultChoice(e.kind) : -1);
    w.key("choices").beginArray();
    if (e.choiceDeadline > 0)
        for (const auto& o : botf::eventChoices(e.kind)) {
            w.beginObject().field("index", o.index).field("label", o.label).field("effect", o.effect).field("isDefault", o.isDefault);
            if (open) {
                // Dry-run through the core (replacing any drafted decision) so the UI shows the core's reason.
                auto copy = s;
                auto ids = draftedIds;
                std::sort(ids.rbegin(), ids.rend());
                std::string blocker;
                try {
                    for (auto id : ids) botf::cancel(copy, id);
                    botf::submit(copy, botf::makeResolveEventChoiceCommand(copy.lastAcceptedCommand + 1, e.id, o.index));
                } catch (const std::exception& ex) { blocker = ex.what(); }
                w.field("blocker", blocker);
            }
            w.endObject();
        }
    w.endArray();
    w.endObject();
}

void writeTemplate(Writer& w, const botf::GameState& s, const botf::BuildTemplate& t) {
    w.beginObject();
    w.field("id", t.id); w.field("name", t.name); w.field("isDefault", t.autoApply);
    w.key("items").beginArray();
    for (const auto& item : t.items)
        w.beginObject().field("product", enumName(static_cast<int>(item.product), BuildingNames)).field("referenceId", item.referenceId)
            .field("name", constructionLabel(s, item)).field("isShip", botf::isShip(item.product)).endObject();
    w.endArray();
    w.endObject();
}

void writeVictory(Writer& w, const botf::GameState& s, int me) {
    w.beginObject();
    w.key("conditions").beginArray();
    for (const auto& p : botf::victoryProgress(s, me))
        w.beginObject().field("condition", enumName(static_cast<int>(p.condition), VictoryKeys))
            .field("name", botf::victoryConditionName(p.condition)).field("enabled", p.enabled).field("achieved", p.achieved)
            .field("percent", p.percent).field("explanation", p.explanation).endObject();
    w.endArray();
    w.field("alternative", s.rules.alternativeVictories);
    w.field("threshold", botf::scientificVictoryThreshold(s));
    w.field("megaprojectName", botf::megaprojectName(s));
    w.field("megaprojectCost", botf::megaprojectIndustryCost(s));
    w.field("megaprojectCredits", botf::megaprojectCreditCost(s));
    w.field("megaprojectWorkPerTurn", botf::megaprojectWorkPerTurn(s, me));
    if (const auto* m = botf::empireMegaproject(s, me))
        w.key("megaproject").beginObject().field("systemId", m->systemId).field("system", systemLabel(s, m->systemId))
            .field("work", m->work).field("startTurn", m->startTurn).endObject();
    // Rival megaprojects are announced to every empire as galactic news.
    w.key("rivalProjects").beginArray();
    for (const auto& m : s.megaprojects)
        if (m.ownerId != me)
            w.beginObject().field("owner", m.ownerId).field("ownerName", empireLabel(m.ownerId)).field("work", m.work)
                .field("startTurn", m.startTurn).endObject();
    w.endArray();
    w.endObject();
}

// Minor-race roster profile (MinorRaces.h): identity, temperament, join bonus and description.
void writeMinorProfile(Writer& w, const botf::minors::MinorRaceRecord& r) {
    w.field("raceId", r.id);
    w.field("raceName", r.name);
    w.field("homeworld", r.homeworld);
    w.field("temperament", botf::minors::temperamentName(r.temperament));
    w.field("bonus", botf::minors::describeJoinBonus(r));
    w.field("bonusKind", botf::minors::joinBonusName(r.bonus));
    w.field("bonusValue", r.bonusValue);
    w.field("description", r.description);
    if (r.signatureShipId >= 0 && static_cast<std::size_t>(r.signatureShipId) < botf::referenceData::ShipCatalogue.size()) {
        const auto& ship = botf::referenceData::ShipCatalogue[static_cast<std::size_t>(r.signatureShipId)];
        w.field("signatureShip", std::string(ship.shipClass) + " " + ship.name);
    }
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
    w.key("rules").beginObject()
        .field("preset", s.rules.preset == botf::RulesetPreset::Remastered ? "remastered" : "classic")
        .field("presetName", botf::rulesetPresetName(s.rules.preset))
        .field("stations", s.rules.stations).field("galaxyFeatures", s.rules.galaxyFeatures)
        .field("curatedEvents", s.rules.curatedEvents).field("alternativeVictories", s.rules.alternativeVictories)
        .field("empireRules", s.rules.empireRules).endObject();
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
    for (const auto& st : d.stations) if (st.ownerId == me) ships += botf::stationStats(d, st).upkeep;   // military upkeep
    int researchOutput = 0, industryTotal = 0, foodTotal = 0, population = 0;
    for (const auto& c : d.colonies) if (c.ownerId == me) {
        const auto e = botf::economy(d, c);
        researchOutput += e.research; industryTotal += e.industry; foodTotal += e.food; population += c.population;
    }
    w.key("empire").beginObject();
    w.field("id", me); w.field("name", empireLabel(me));
    {
        const auto dil = botf::empire_rules::dilithiumStatus(d, me);
        w.key("dilithium").beginObject().field("enforced", dil.enforced).field("refineries", dil.refineries)
            .field("powered", dil.powered).field("slots", dil.slots).field("used", dil.used).endObject();
    }
    w.field("credits", s.credits); w.field("draftCredits", d.credits);
    w.field("research", s.research); w.field("researchReserve", s.researchReserve);
    w.field("intelligence", s.intelligence);
    w.field("researchOutput", researchOutput); w.field("industry", industryTotal); w.field("food", foodTotal); w.field("population", population);
    {
        const auto bonus = botf::minors::memberBonuses(d, me);
        w.key("memberBonuses").beginObject().field("members", bonus.members).field("research", bonus.researchPct)
            .field("industry", bonus.industryPct).field("food", bonus.foodPct).field("credits", bonus.creditsPct)
            .field("morale", bonus.morale).field("groundDefense", bonus.groundDefensePct);
        w.key("minors").beginArray();
        for (const auto& m : d.minors)
            if (m.memberOf == me) {
                w.beginObject().field("systemId", m.systemId).field("name", m.name);
                if (const auto* race = botf::minors::managed(m) ? botf::minors::raceById(m.raceId) : nullptr)
                    w.field("bonus", botf::minors::describeJoinBonus(*race));
                else w.field("bonus", "+10% research");
                w.endObject();
            }
        w.endArray();
        w.endObject();
    }
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

    // --- schema 15: conquered worlds waiting for the player's aftermath decision
    w.key("aftermaths").beginArray();
    for (const auto& a : d.aftermaths) {
        const auto* minor = botf::minorAt(d, a.systemId);
        const auto* colony = botf::colonyAt(d, a.systemId);
        w.beginObject().field("systemId", a.systemId).field("name", systemLabel(d, a.systemId))
            .field("conqueror", a.conqueror).field("formerOwner", a.formerOwner)
            .field("formerName", a.formerOwner >= 1 && a.formerOwner <= 5 ? empireLabel(a.formerOwner) : std::string(minor ? minor->name : "natives"))
            .field("minor", minor ? minor->name : std::string()).field("turn", a.turn)
            .field("turnsLeft", std::max(0, a.turn + botf::empire_rules::AftermathLapseTurns - d.turn))
            .field("populationBefore", a.populationBefore).field("population", colony ? colony->population : 0);
        w.key("options").beginArray();
        for (int o = 0; o < 3; ++o)
            w.beginObject().field("id", o).field("name", botf::empire_rules::aftermathName(o))
                .field("effect", botf::empire_rules::aftermathEffect(o))
                .field("blocker", botf::empire_rules::aftermathBlocker(d, a, static_cast<botf::empire_rules::Aftermath>(o))).endObject();
        w.endArray();
        w.endObject();
    }
    w.endArray();

    // --- fog grid: 0 unknown, 1 explored, 2 in sensor range
    std::string fog(static_cast<std::size_t>(s.width * s.height), '0');
    for (int y = 0; y < s.height; ++y) for (int x = 0; x < s.width; ++x) {
        char v = '0';
        if (botf::explored(s, x, y)) v = '1';
        if (botf::visible(s, me, x, y)) v = '2';
        fog[static_cast<std::size_t>(y * s.width + x)] = v;
    }
    w.field("fog", fog);

    // --- systems: every star in the galaxy is on the chart from turn 1 (Simon, 10 Nov 2026: the galaxy screen shows the whole
    // disc). A system the player has not explored is "uncharted": id, position and a catalogue designation only, so nothing
    // about it (name, owner, planets, minor race, structures, capacity) reaches the client. The star's type and colour are
    // the client's own deterministic hash of (x, y, seed); the core holds none. Explored systems are listed in full as before.
    w.key("systems").beginArray();
    for (const auto& sys : s.systems) {
        if (!knownSystem(s, sys)) {
            w.beginObject();
            w.field("id", sys.id); w.field("x", sys.x); w.field("y", sys.y);
            w.field("name", uncharted_designation(sys.id));
            w.field("explored", false);
            w.endObject();
            continue;
        }
        const bool seen = botf::visible(s, me, sys.x, sys.y);
        const auto* col = botf::colonyAt(s, sys.id);
        const auto* minor = botf::minorAt(s, sys.id);
        w.beginObject();
        w.field("id", sys.id); w.field("x", sys.x); w.field("y", sys.y); w.field("name", sys.name);
        w.field("explored", true);
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
                .field("friendship", minor->friendshipTurn[static_cast<std::size_t>(me)] >= 0);
            if (const auto* race = botf::minors::raceFor(*minor)) {
                w.field("temperament", botf::minors::temperamentName(race->temperament));
                w.field("bonus", botf::minors::describeJoinBonus(*race));
            }
            if (botf::minors::managed(*minor)) { w.field("techLevel", minor->techLevel); w.field("population", minor->population); }
            w.endObject();
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

    // --- schema-13 gameplay layer
    w.key("stations").beginArray();
    if (s.rules.stations) for (const auto& st : botf::visibleStations(s, me)) writeStation(w, d, st, me);
    w.endArray();
    w.field("outpostTurns", s.rules.stations ? botf::outpostBuildTurns(s, me) : 0);
    w.key("stationUpgradeCost").beginObject().field("credits", botf::stationUpgradeCost(s, me).credits)
        .field("industry", botf::stationUpgradeCost(s, me).industry).endObject();
    w.key("features").beginArray();
    if (s.rules.galaxyFeatures) for (const auto& f : botf::knownFeatures(s, me)) writeFeature(w, s, f, me);
    w.endArray();
    w.key("events").beginArray();
    for (const auto& e : botf::empireEvents(s, me)) writeGalacticEvent(w, s, e);
    w.endArray();
    w.key("templates").beginArray();
    for (const auto& t : botf::empireBuildTemplates(d, me)) writeTemplate(w, d, t);
    w.endArray();
    w.key("victory"); writeVictory(w, d, me);

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
        if (c.shipyards > 0 && !e.shipyardPowered)
            w.beginObject().field("kind", "shipyard").field("severity", "warn").field("systemId", c.systemId)
                .field("text", systemLabel(s, c.systemId) + ": shipyard " + (c.shipyardEnabled ? "unpowered" : "switched off") +
                    " — no shipbuilding or supply range").endObject();
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
                .field("text", (p.proposer.isMinor ? systemLabel(s, p.proposer.id) : empireLabel(p.proposer.id)) +
                    (p.treatyType == dip::TreatyType::None
                        ? (p.creditDemand > 0 ? " demands " + std::to_string(p.creditDemand) + " credits in tribute"
                                              : " offers a gift of " + std::to_string(p.creditOffer) + " credits")
                        : std::string(" proposes a ") + dip::treatyTypeName(p.treatyType)) +
                    " — answer by turn " + std::to_string(p.dueTurn)).endObject();
        for (const auto& call : botf::openCallsToArms(s, me)) {
            const auto* rel = botf::majorRelation(s, me, call.aggressor);
            if (rel && rel->atWar) continue;
            w.beginObject().field("kind", "callToArms").field("severity", "alert").field("empire", call.defender)
                .field("text", empireLabel(call.defender) + " calls us to arms against " + empireLabel(call.aggressor) +
                    " — declare war by turn " + std::to_string(call.dueTurn)).endObject();
        }
    } catch (...) {}
    for (const auto& e : botf::empireEvents(s, me)) {
        if (e.choiceDeadline <= 0 || e.choice >= 0 || e.empireId != me) continue;
        bool drafted = false;
        for (const auto& c : s.pending) drafted |= c.type == botf::CommandType::ResolveEventChoice && c.eventId == e.id;
        if (drafted) continue;
        w.beginObject().field("kind", "event").field("severity", "warn").field("eventId", e.id)
            .field("text", std::string(botf::galacticEventName(e.kind)) + ": decision needed by turn " + std::to_string(e.choiceDeadline)).endObject();
    }
    for (const auto& a : d.aftermaths)
        w.beginObject().field("kind", "aftermath").field("severity", "warn")
            .field("text", "Decide the fate of the conquered world " + systemLabel(d, a.systemId)).endObject();
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
    writeRosterDetail(w, r.id);
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
        if (const auto* ship = botf::roster::findShip(ch.referenceId))
            writeShipRecord(w, *ship);
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
    const std::array<std::pair<botf::Building, int>, 8> kinds{{
        {botf::Building::Farm, c->farms}, {botf::Building::Factory, c->factories}, {botf::Building::Laboratory, c->laboratories},
        {botf::Building::PowerPlant, c->powerPlants}, {botf::Building::FusionPlant, c->fusionPlants},
        {botf::Building::Shipyard, c->shipyards}, {botf::Building::DefenseBattery, c->defenseBatteries},
        {botf::Building::DilithiumRefinery, c->refineries}}};
    for (const auto& [kind, count] : kinds) {
        if (kind == botf::Building::DilithiumRefinery && !botf::empire_rules::enabled(s)) continue;
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

    // Schema-13: megaproject, build templates and starbase funding at this colony.
    const std::string lock = B.session.active ? "Finish the battle in progress first" : "";
    {
        const auto* m = botf::empireMegaproject(d, s.playerEmpire);
        w.key("megaproject").beginObject();
        w.field("enabled", s.rules.alternativeVictories);
        w.field("name", botf::megaprojectName(s));
        w.field("here", m && m->systemId == systemId);
        if (m) { w.field("systemId", m->systemId); w.field("system", systemLabel(s, m->systemId)); w.field("work", m->work); }
        w.field("cost", botf::megaprojectIndustryCost(s));
        w.field("credits", botf::megaprojectCreditCost(s));
        w.field("workPerTurn", botf::megaprojectWorkPerTurn(d, s.playerEmpire));
        w.field("startBlocker", !lock.empty() ? lock : dryRunError(s, botf::makeStartMegaprojectCommand(next, systemId)));
        w.field("cancelBlocker", !lock.empty() ? lock : dryRunError(s, botf::makeCancelMegaprojectCommand(next)));
        w.endObject();
    }
    w.key("templates").beginArray();
    for (const auto& t : botf::empireBuildTemplates(d, s.playerEmpire))
        w.beginObject().field("id", t.id).field("name", t.name)
            .field("blocker", !lock.empty() ? lock : dryRunError(s, botf::makeApplyBuildTemplateCommand(next, t.id, systemId))).endObject();
    w.endArray();
    w.field("queueExplanation", c->queueExplanation);
    for (const auto& u : d.stationUpgrades)
        if (u.fundingSystemId == systemId)
            if (const auto* st = botf::stationById(d, u.stationId))
                w.key("fundsStation").beginObject().field("stationId", st->id).field("name", botf::stationName(*st))
                    .field("x", st->x).field("y", st->y).field("work", u.work)
                    .field("cost", botf::stationUpgradeCost(d, s.playerEmpire).industry).endObject();
    w.endObject();
}

// Station inspector: own outposts list every colony that could fund a starbase upgrade, each
// dry-run through the core so a refused option carries the core's reason.
void writeStationDetail(Writer& w, int stationId) {
    requireGame();
    const auto& s = B.state;
    const auto& d = draft();
    const int me = s.playerEmpire;
    const botf::Station* st = nullptr;
    for (const auto& v : botf::visibleStations(s, me)) if (v.id == stationId) st = botf::stationById(s, v.id);
    if (!st) fail("That station is not in sensor range");
    w.beginObject();
    w.key("station"); writeStation(w, d, *st, me);
    const bool mine = st->ownerId == me;
    // Starbase statistics for comparison (what an upgrade buys).
    w.key("starbase").beginObject(); writeStationStats(w, botf::stationStats(s, st->ownerId, botf::StationType::Starbase)); w.endObject();
    const auto cost = botf::stationUpgradeCost(s, me);
    w.field("upgradeCredits", cost.credits); w.field("upgradeIndustry", cost.industry);
    w.key("funding").beginArray();
    if (mine && st->type == botf::StationType::Outpost) {
        const std::uint64_t next = s.lastAcceptedCommand + 1;
        for (const auto& c : d.colonies) {
            if (c.ownerId != me) continue;
            const auto* sys = systemById(d, c.systemId);
            if (!sys) continue;
            const auto e = botf::economy(d, c);
            w.beginObject().field("systemId", c.systemId).field("name", sys->name)
                .field("distance", std::max(std::abs(sys->x - st->x), std::abs(sys->y - st->y)))
                .field("industry", e.industry)
                .field("blocker", B.session.active ? std::string("Finish the battle in progress first")
                    : dryRunError(s, botf::makeUpgradeStationCommand(next, st->id, c.systemId))).endObject();
        }
    }
    w.endArray();
    bool drafted = false;
    for (const auto& c : s.pending) drafted |= c.type == botf::CommandType::UpgradeStation && c.stationId == st->id;
    w.field("upgradeDrafted", drafted);
    // Ships in the sector (own ships are repaired by a starbase; hostile ships start a battle).
    int own = 0, hostile = 0;
    for (const auto& f : s.fleets) if (f.x == st->x && f.y == st->y) {
        if (f.ownerId == me) ++own;
        else if (fleetVisibleToPlayer(s, f)) ++hostile;
    }
    w.field("ownShipsHere", own); w.field("contactsHere", hostile);
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

// Trade routes with their value, and every route the player could still establish. The value is the
// per-turn credit income the turn resolver pays for a route (5 + min(both populations) / 10), shown
// even while a route is blocked so the client can say what it would be worth.
void writeTradeRoutes(Writer& w) {
    requireGame();
    const auto& s = B.state;
    const auto& d = draft();
    const int me = d.playerEmpire;
    const auto routeValue = [&](const botf::Colony& a, const botf::Colony& b) { return 5 + std::min(a.population, b.population) / 10; };
    const auto blockerFor = [&](int partner) -> std::string {
        const auto* rel = botf::majorRelation(d, me, partner);
        if (!rel) return "No contact with that empire";
        if (rel->atWar) return "War blocks trade";
        if (d.advancedRulesEnabled && !rel->tradeTreaty) return "An accepted trade treaty is required";
        return "";
    };
    const auto writePair = [&](const botf::Colony& a, const botf::Colony& b, const std::string& blocker) {
        w.field("from", a.systemId).field("to", b.systemId);
        w.field("fromName", systemLabel(s, a.systemId)).field("toName", systemLabel(s, b.systemId));
        w.field("partner", b.ownerId).field("partnerName", empireLabel(b.ownerId));
        w.field("value", routeValue(a, b)).field("blocker", blocker);
    };
    w.beginObject();
    w.field("advancedRules", d.advancedRulesEnabled);
    w.key("routes").beginArray();
    for (const auto& route : d.tradeRoutes) {
        if (route.ownerId != me) continue;
        const auto* src = botf::colonyAt(d, route.fromSystemId);
        const auto* dst = botf::colonyAt(d, route.toSystemId);
        w.beginObject();
        bool active = false;
        if (!src || !dst) {
            w.field("from", route.fromSystemId).field("to", route.toSystemId).field("fromName", systemLabel(s, route.fromSystemId))
                .field("toName", systemLabel(s, route.toSystemId)).field("partner", 0).field("partnerName", "").field("value", 0)
                .field("blocker", "One end is no longer a colony");
        } else if (src->ownerId != me || dst->ownerId == me) {
            writePair(*src, *dst, "A colony changed hands");
        } else {
            const std::string blocker = blockerFor(dst->ownerId);
            writePair(*src, *dst, blocker);
            active = blocker.empty();
        }
        w.field("active", active);
        w.endObject();
    }
    w.endArray();
    w.key("candidates").beginArray();
    for (const auto& src : d.colonies) {
        if (src.ownerId != me) continue;
        if (std::any_of(d.tradeRoutes.begin(), d.tradeRoutes.end(), [&](const botf::TradeRoute& r) { return r.ownerId == me && r.fromSystemId == src.systemId; })) continue;
        for (const auto& dst : d.colonies) {
            if (dst.ownerId == me || dst.ownerId == 0) continue;
            const auto* sys = systemById(s, dst.systemId);
            if (!sys || !knownSystem(s, *sys) || !botf::visible(s, me, sys->x, sys->y)) continue;
            const auto* rel = botf::majorRelation(d, me, dst.ownerId);
            if (!rel || !rel->contact) continue;
            w.beginObject();
            writePair(src, dst, blockerFor(dst.ownerId));
            w.endObject();
        }
    }
    w.endArray();
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
        if (const auto* race = botf::minors::raceFor(m)) { w.key("race").beginObject(); writeMinorProfile(w, *race); w.endObject(); }
        w.field("managed", botf::minors::managed(m));
        if (botf::minors::managed(m)) {
            w.field("techLevel", m.techLevel); w.field("population", m.population);
            w.field("progress", m.progress); w.field("progressNeeded", botf::minors::techThreshold(m.techLevel));
        }
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
            w.field("kind", p.treatyType == dip::TreatyType::None ? (p.creditDemand > 0 ? "tribute" : "gift")
                : p.treatyType == dip::TreatyType::War ? "war" : "treaty");
            w.field("reason", p.rejectionReason);
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
            w.beginObject().field("turn", it->turn).field("type", dip::eventTypeName(it->type)).field("text", it->description)
                .field("typeKey", static_cast<int>(it->type)).field("value", it->numericValue);
            w.key("a"); writeParticipant(w, s, it->partyA);
            w.key("b"); writeParticipant(w, s, it->partyB);
            w.endObject();
        }
    } catch (...) {}
    w.endArray();
    // Galactic news: newsworthy engine events between powers we have met (betrayals, wars, calls
    // to arms, eliminations, secessions) — the memory the AI empires act on.
    w.key("news").beginArray();
    try {
        using T = dip::DiplomaticEventType;
        const auto knows = [&](dip::ParticipantId p) {
            if (p == self) return true;
            if (p.isMinor) { const auto* m = botf::minorAt(s, p.id); return m && m->contact[static_cast<std::size_t>(me)]; }
            const auto* rel = botf::majorRelation(s, me, p.id);
            return rel && rel->contact;
        };
        const auto events = botf::diplomaticEvents(s);
        int count = 0;
        for (auto it = events.rbegin(); it != events.rend() && count < 40; ++it) {
            if (it->partyA == self || it->partyB == self) continue;
            const auto t = it->type;
            if (!(t == T::WarDeclared || t == T::PeaceEstablished || t == T::TreatyBetrayed || t == T::AllianceHonored ||
                  t == T::AllianceRefused || t == T::AllianceCalledToArms || t == T::ParticipantEliminated || t == T::MinorSeceded ||
                  t == T::MinorMembershipJoined)) continue;
            if (!knows(it->partyA) && !knows(it->partyB)) continue;
            ++count;
            w.beginObject().field("turn", it->turn).field("type", dip::eventTypeName(t)).field("typeKey", static_cast<int>(t))
                .field("text", it->description).field("value", it->numericValue);
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

    const auto& ds = s.diplomacyEngineState;
    // Our own galaxy-wide reputation: wars declared while bound by a treaty.
    w.key("standing").beginObject().field("betrayals", ds.betrayals(me))
        .field("lastBetrayalTurn", ds.findStanding(me) ? ds.findStanding(me)->lastBetrayalTurn : -1).endObject();

    // Calls to arms addressed to us (the player honours one by declaring war on the aggressor).
    w.key("callsToArms").beginArray();
    try {
        for (const auto& call : botf::openCallsToArms(s, me)) {
            const auto* rel = botf::majorRelation(s, me, call.aggressor);
            const bool drafted = std::any_of(s.pending.begin(), s.pending.end(), [&](const botf::Command& c) {
                return c.type == botf::CommandType::DeclareWar && c.targetEmpire == call.aggressor; });
            botf::Command war; war.type = botf::CommandType::DeclareWar; war.targetEmpire = call.aggressor;
            w.beginObject();
            w.field("defender", call.defender); w.field("defenderName", empireLabel(call.defender));
            w.field("aggressor", call.aggressor); w.field("aggressorName", empireLabel(call.aggressor));
            w.field("issuedTurn", call.issuedTurn); w.field("dueTurn", call.dueTurn);
            w.field("honored", rel && rel->atWar);
            w.field("honorDrafted", drafted);
            w.field("boundByTreaty", botf::treatyForbidsAttack(s, me, call.aggressor));
            w.field("honorBlocker", B.session.active ? std::string("Finish the battle in progress first") : drafted ? std::string() : dryRunError(s, war));
            w.endObject();
        }
    } catch (...) {}
    w.endArray();

    // Treaty history: ended treaties (expired, broken by war, superseded, dissolved) involving us.
    w.key("treatyHistory").beginArray();
    {
        int count = 0;
        for (auto it = ds.treatyHistory.rbegin(); it != ds.treatyHistory.rend() && count < 60; ++it) {
            if (!it->involves(self)) continue;
            ++count;
            w.beginObject();
            w.field("id", it->id);
            w.key("counterparty"); writeParticipant(w, s, it->getCounterparty(self));
            w.field("type", enumName(static_cast<int>(it->type), TreatyKeys));
            w.field("typeName", dip::treatyTypeName(it->type));
            w.field("startTurn", it->startTurn);
            w.field("status", dip::treatyStatusName(it->status));
            w.field("statusKey", static_cast<int>(it->status));
            w.endObject();
        }
    }
    w.endArray();

    // Diplomatic victory predicate (DIPLOMACY.md §8).
    try {
        const auto v = botf::diplomaticVictoryStatus(s, me);
        w.key("victory").beginObject().field("bloc", v.blocPopulation).field("galaxy", v.galaxyPopulation)
            .field("achieved", v.achieved);
        w.key("allies").beginArray(); for (int id : v.allies) w.value(id); w.endArray();
        w.key("memberMinors").beginArray(); for (int id : v.memberMinors) w.value(systemLabel(s, id)); w.endArray();
        w.endObject();
    } catch (...) {}
    w.field("turn", s.turn);
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
    // Our counter-intelligence strength in milli-points (at the rules' default 100% morale).
    if (committedIntel)
        w.field("defenseStrengthMilli", intel::calculateDefenseStrength(committedIntel->getRollingSecuritySum(),
            committedIntel->internalSecurityStockpile, 100, intel::IntelligenceRules{}));
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
            // Integer milli-point attack strength against this rival (20-turn average x concentration).
            if (const auto* crs = committedIntel ? committedIntel->getRivalState(r.id) : nullptr)
                w.field("attackStrengthMilli", intel::concentratedAttackStrength(*crs, intel::IntelligenceRules{}));
        }
        w.field("eliminated", botf::empireEliminated(s, r.id));
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
// Mirrors the order-gathering step of Encounter::resolveRound: pending order, else standing
// order (a destroyed target is cleared; an impossible standing Retreat falls back to the AI), else
// the tactical AI's decision. Presentation only: resolution always happens in the core.
struct PlannedOrder { tac::Order order; const char* source = "ai"; };
PlannedOrder plannedOrder(const tac::Encounter& e, const tac::Ship& ship) {
    for (const auto& o : e.pendingOrders) if (o.fleetId == ship.fleetId) return {o, "order"};
    if (const auto it = e.standingOrders.find(ship.fleetId); it != e.standingOrders.end()) {
        auto stand = it->second;
        if (const auto* t = e.getShip(stand.targetFleetId); t && !tac::Encounter::shipActive(*t)) stand.targetFleetId = 0;
        if (!(e.remasterRules() && stand.maneuver == tac::TacticalManeuver::Retreat && !ship.canRetreat)) return {stand, "standing"};
    }
    return {e.aiOrderFor(ship), "ai"};
}

void writeCoord(Writer& w, const tac::TacticalCoord& c) { w.beginObject().field("x", c.x).field("y", c.y).endObject(); }

// Rules 4 per-ship block: facings, subsystems, crew and deck layer (COMBAT-RULES.md "Tactical rules 4").
void writeWegoShip(Writer& w, const tac::Ship& ship) {
    w.key("wego").beginObject();
    w.key("facings").beginArray(); for (int v : ship.shieldFacing) w.value(v); w.endArray();
    w.key("maxFacings").beginArray(); for (int v : ship.maxShieldFacing) w.value(v); w.endArray();
    w.key("subsystems").beginObject();
    for (int i = 1; i <= tac::SubsystemCount; ++i) w.field(SubsystemKeys[static_cast<std::size_t>(i)], ship.subsystems[static_cast<std::size_t>(i - 1)]);
    w.endObject();
    w.field("crew", tac::crewRatingName(ship.crew)).field("crewLevel", ship.crew)
        .field("elevation", ship.elevation).field("coreBreached", ship.coreBreached)
        .field("canWarp", ship.canRetreat && ship.system(tac::Subsystem::WarpCore) > 0 && ship.system(tac::Subsystem::Engines) > 0);
    w.endObject();
}

void writeEncounter(Writer& w, const tac::Encounter& e, int me, const botf::GameState& gs = B.state) {
    w.beginObject();
    w.field("arenaWidth", e.arenaWidth); w.field("arenaHeight", e.arenaHeight);
    w.field("round", e.currentRound); w.field("maxRounds", e.maxRounds);
    w.field("finished", e.isFinished); w.field("winner", e.winnerOwnerId);
    w.field("sectorX", e.sectorX); w.field("sectorY", e.sectorY); w.field("turn", e.strategicTurn);
    w.field("controller", e.controllingOwnerId);
    const auto* sys = systemAtCell(gs, e.sectorX, e.sectorY);
    w.field("location", sys && knownSystem(gs, *sys) ? sys->name : "Sector " + std::to_string(e.sectorX) + "," + std::to_string(e.sectorY));
    // The battle backdrop planet: the system's largest non-asteroid world (clients fall back to a gas giant).
    if (sys && knownSystem(gs, *sys)) {
        const botf::Planet* backdrop = nullptr;
        for (const auto& p : sys->planets)
            if (p.planetClass != botf::PlanetClass::Asteroids && (!backdrop || p.maxCapacity > backdrop->maxCapacity)) backdrop = &p;
        if (backdrop) w.field("planetClass", enumName(static_cast<int>(backdrop->planetClass), PlanetClassKeys));
    }
    w.field("rules", e.rulesVersion);
    w.field("ticksPerRound", botf::CombatTicksPerRound);
    int foe = 0;
    for (const auto& ship : e.ships) if (ship.ownerId != me) { foe = ship.ownerId; break; }
    w.field("foe", foe);
    // Hail truce preview (tactical rules 2): the AI side accepts when not formally at war, or when
    // its strength is small enough relative to ours (Encounter::aiAcceptsHail).
    w.key("hail").beginObject()
        .field("atWar", e.relationAtWar).field("trust", e.relationTrust)
        .field("ourStrength", static_cast<std::int64_t>(e.sideStrength(me)))
        .field("theirStrength", static_cast<std::int64_t>(foe ? e.sideStrength(foe) : 0))
        .field("accepts", foe && e.remasterRules() && e.aiAcceptsHail(foe, me)).endObject();
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
            w.field("disengaging", false);
            w.endObject();
            continue;
        }
        w.field("hull", ship.hull); w.field("maxHull", ship.maxHull); w.field("initialHull", ship.initialHull);
        w.field("shields", ship.shields); w.field("maxShields", ship.maxShields); w.field("initialShields", ship.initialShields);
        w.field("beam", ship.beamAttack); w.field("torpedo", ship.torpedoAttack); w.field("speed", ship.speed);
        w.key("position"); writeCoord(w, ship.position);
        if (const auto* rc = botf::roster::findRosterClass(ship.referenceShipId)) w.field("assetKey", rc->assetKey);
        if (!ship.mounts.empty()) {
            // Rules 3: the encounter's own mounts (legacy hulls included); Fire events index into this list.
            w.key("weapons").beginArray();
            for (const auto& m : ship.mounts) {
                const std::string type = botf::roster::weaponTypeName(m.type);
                const auto family = botf::roster::weaponFamily(m.type);
                // Arc geometry and range rings for plan-phase overlays (degrees from the bow, grid cells).
                w.beginObject().field("type", type).field("key", "weapon." + type).field("count", m.count)
                    .field("arc", enumName(static_cast<int>(m.arc), rosterNames::Arc))
                    .field("torpedo", botf::roster::torpedoWeapon(m.type))
                    .field("family", botf::roster::weaponFamilyName(family))
                    .field("arcCentre", tac::arcCentre(m.arc)).field("arcHalfWidth", tac::arcHalfWidth(m.arc))
                    .field("maxRange", tac::weaponMaxRange(family)).field("attack", m.attack).endObject();
            }
            w.endArray();
        } else if (const auto* rc = botf::roster::findRosterClass(ship.referenceShipId)) {
            writeRosterWeapons(w, *rc);
        }
        w.field("alive", ship.isAlive); w.field("active", ship.activeInCombat); w.field("retreated", ship.retreated);
        w.field("canRetreat", ship.canRetreat); w.field("canCloak", ship.canCloak); w.field("cloaked", ship.isCloaked);
        w.field("groupId", ship.groupId);
        if (const auto* g = e.getGroupForShip(ship.fleetId)) w.field("group", tac::tacticalGroupClassName(g->groupClass));
        // Outposts and starbases fight as immobile combatants (fleet id StationCombatantBase + station id).
        w.field("immobile", ship.immobile);
        if (botf::isStationCombatant(ship.fleetId)) {
            w.field("station", true);
            if (const auto* st = botf::stationById(gs, botf::stationIdForCombatant(ship.fleetId))) {
                w.field("stationType", enumName(static_cast<int>(st->type), StationTypeNames));
                w.field("stationName", botf::stationName(*st));
            }
        }
        w.field("disengaging", ship.disengaging());
        if (e.flightRules()) w.field("facing", ship.facing).field("elevation", ship.elevation);
        if (e.wegoRules()) writeWegoShip(w, ship);
        if (mine) {
            w.field("maneuver", enumName(static_cast<int>(order.maneuver), ManeuverKeys));
            w.field("target", order.targetFleetId);
            w.field("ordered", hasPending);
            if (e.wegoRules()) w.field("subsystem", enumName(static_cast<int>(order.subsystem), SubsystemKeys));
        }
        // What the ship will execute next round: the player's order, a standing order (strategic
        // stance or an earlier tactical order) or the tactical AI's own decision (Encounter::aiOrderFor).
        if (!e.isFinished && tac::Encounter::shipActive(ship)) {
            const auto next = plannedOrder(e, ship);
            w.key("next").beginObject()
                .field("maneuver", enumName(static_cast<int>(next.order.maneuver), ManeuverKeys))
                .field("target", next.order.targetFleetId)
                .field("subsystem", enumName(static_cast<int>(next.order.subsystem), SubsystemKeys))
                .field("source", next.source).endObject();
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
            w.field("ai", st.aiControlled);
            // Rules 2: the first round of Retreat keeps the ship on the field, disengaging under fire.
            w.field("disengaging", e.remasterRules() && st.maneuverExecuted == tac::TacticalManeuver::Retreat &&
                !st.retreatedThisRound && !st.destroyedThisRound && st.hullBefore > 0);
            w.field("yaw", st.facingYaw);
            if (e.wegoRules()) w.field("subsystem", enumName(static_cast<int>(st.subsystemTarget), SubsystemKeys));
            w.endObject();
        }
        w.endArray();
        // Rules 4: facings, subsystems and crew after the round (persistent damage).
        if (e.wegoRules()) {
            w.key("wego").beginArray();
            for (const auto& ws : r.wegoStates) {
                w.beginObject().field("id", ws.fleetId);
                w.key("facings").beginArray(); for (int v : ws.shieldFacing) w.value(v); w.endArray();
                w.key("subsystems").beginObject();
                for (int i = 1; i <= tac::SubsystemCount; ++i) w.field(SubsystemKeys[static_cast<std::size_t>(i)], ws.subsystems[static_cast<std::size_t>(i - 1)]);
                w.endObject();
                w.field("crew", tac::crewRatingName(ws.crew)).field("crewLevel", ws.crew).field("coreBreached", ws.coreBreached);
                w.endObject();
            }
            w.endArray();
        }
        w.key("salvos").beginArray();
        for (const auto& sv : r.salvos) {
            const auto* shooter = r.getStatus(sv.attackerFleetId);
            w.beginObject().field("from", sv.attackerFleetId).field("to", sv.defenderFleetId).field("beam", sv.beamDamage)
                .field("torpedo", sv.torpedoDamage).field("absorbed", sv.absorbedByShields).field("ram", sv.ram)
                .field("ambush", !sv.ram && shooter && shooter->decloakedThisRound).endObject();
        }
        w.endArray();
        w.key("lost").beginArray(); for (int id : r.lostFleetIds) w.value(id); w.endArray();
        w.key("retreatedIds").beginArray(); for (int id : r.retreatedFleetIds) w.value(id); w.endArray();
        // Tactical rules 3 presentation stream (COMBAT-RULES.md "Event stream for client authors"); empty otherwise.
        static constexpr std::array<const char*, 10> EventKinds{"ManeuverStart", "ManeuverEnd", "Cloak", "Decloak", "Fire",
            "Ram", "Destroyed", "Retreated", "Disengage", "Truce"};
        // Rules 4 hits are matched to their Fire/Ram event by (shooter, target, mount, tick), in order.
        std::vector<char> hitUsed(r.hits.size(), 0);
        auto hitFor = [&](const botf::CombatEvent& ev) -> const tac::WegoHit* {
            const int mount = ev.kind == botf::CombatEventKind::Ram ? -1 : ev.mountIndex;
            for (std::size_t i = 0; i < r.hits.size(); ++i) {
                const auto& h = r.hits[i];
                if (hitUsed[i] || h.shooter != ev.fleetId || h.target != ev.targetFleetId || h.mount != mount || h.tick != ev.tick) continue;
                hitUsed[i] = 1;
                return &h;
            }
            return nullptr;
        };
        w.key("events").beginArray();
        for (const auto& ev : r.events) {
            w.beginObject().field("tick", ev.tick).field("kind", enumName(static_cast<int>(ev.kind), EventKinds))
                .field("fleetId", ev.fleetId).field("targetFleetId", ev.targetFleetId);
            if (ev.kind == botf::CombatEventKind::Fire) {
                const auto type = static_cast<botf::roster::WeaponType>(ev.weaponType);
                w.field("mountIndex", ev.mountIndex).field("weapon", botf::roster::weaponTypeName(type))
                    .field("key", botf::roster::weaponTypeKey(type))
                    .field("family", botf::roster::weaponFamilyName(static_cast<botf::roster::WeaponFamily>(ev.weaponFamily)))
                    .field("projectiles", ev.projectiles).field("hit", ev.hit).field("intercepted", ev.intercepted);
            }
            if (ev.kind == botf::CombatEventKind::Fire || ev.kind == botf::CombatEventKind::Ram)
                w.field("impactTick", ev.impactTick).field("shieldDamage", ev.shieldDamage).field("hullDamage", ev.hullDamage)
                    .field("destroyed", ev.destroyed);
            if (e.wegoRules() && (ev.kind == botf::CombatEventKind::Fire || ev.kind == botf::CombatEventKind::Ram)) {
                if (const auto* h = hitFor(ev)) {
                    w.field("hitChance", h->hitChance).field("shieldFacing", tac::shieldFacingName(h->facing))
                        .field("subsystem", enumName(h->subsystem, SubsystemKeys)).field("aimed", h->aimed)
                        .field("subsystemDamage", h->subsystemDamage).field("subsystemDestroyed", h->subsystemDestroyed)
                        .field("breachDamage", h->breachDamage)
                        // Read-only presentation field (M1 battleplay): the hit's place in the core's resolution order
                        // (RoundResult::hits is filled as shots resolve), so playback lands impacts in the core's order.
                        .field("resolveOrder", static_cast<int>(h - r.hits.data()));
                }
            }
            w.field("x", ev.x).field("y", ev.y).field("facing", ev.facing);
            if (ev.kind == botf::CombatEventKind::ManeuverStart || ev.kind == botf::CombatEventKind::ManeuverEnd)
                w.field("maneuver", enumName(ev.maneuver, ManeuverKeys)).field("value", ev.value);
            w.endObject();
        }
        w.endArray();
        w.endObject();
    }
    w.endArray();
    w.endObject();
}

// Plan-phase forecast (rules 3 and 4): the ship's previewed path and end pose, and per mount the arc, range band,
// hit chance and expected damage against the target (Encounter::previewMove / forecastFire).
void writeForecast(Writer& w, const tac::Encounter& e, const tac::Order& order) {
    const auto move = e.previewMove(order);
    const auto mounts = e.forecastFire(order);
    w.beginObject();
    w.field("fleetId", order.fleetId).field("target", mounts.empty() ? order.targetFleetId : mounts.front().target)
        .field("maneuver", enumName(static_cast<int>(order.maneuver), ManeuverKeys))
        .field("subsystem", enumName(static_cast<int>(order.subsystem), SubsystemKeys));
    w.field("exact", move.exact).field("slid", move.slid);
    w.key("path").beginArray();
    for (const auto& [c, f] : move.path) w.beginObject().field("x", c.x).field("y", c.y).field("facing", f).endObject();
    w.endArray();
    w.key("end").beginObject().field("x", move.end.x).field("y", move.end.y).field("facing", move.endFacing).endObject();
    int expected = 0, firing = 0;
    w.key("mounts").beginArray();
    for (const auto& m : mounts) {
        const std::string type = botf::roster::weaponTypeName(m.type);
        w.beginObject().field("mount", m.mount).field("weapon", type).field("key", "weapon." + type)
            .field("family", botf::roster::weaponFamilyName(m.family))
            .field("arc", enumName(static_cast<int>(m.arc), rosterNames::Arc))
            .field("distance", m.distance).field("band", tac::rangeBandName(m.band))
            .field("inArc", m.inArc).field("inRange", m.inRange).field("hitChance", m.hitChance)
            .field("damage", m.damage).field("expectedDamage", m.expectedDamage)
            .field("facing", tac::shieldFacingName(m.facing)).field("facingShields", m.facingShields).endObject();
        expected += m.expectedDamage;
        if (m.hitChance > 0) ++firing;
    }
    w.endArray();
    w.field("expectedDamage", expected).field("mountsFiring", firing);
    w.endObject();
}

tac::Order readTacticalOrder(const Value& a, const tac::Encounter& e) {
    tac::Order o;
    o.fleetId = a.getInt("fleetId");
    o.maneuver = static_cast<tac::TacticalManeuver>(enumIndex(a["maneuver"], ManeuverKeys, "maneuver", 0));
    o.targetFleetId = o.maneuver == tac::TacticalManeuver::Retreat ? 0 : a.getInt("targetFleetId");
    if (o.targetFleetId) {
        const auto* ts = e.getShip(o.targetFleetId);
        if (!ts || !tac::Encounter::shipActive(*ts)) o.targetFleetId = 0;
    }
    const int sub = enumIndex(a["subsystem"], SubsystemKeys, "subsystem", 0);
    if (sub < 0 || sub > tac::MaxSubsystemValue) fail("Unknown subsystem");
    o.subsystem = static_cast<tac::Subsystem>(sub);
    return o;
}

// Rounds resolve through the plain session API. session::resume re-derives AI decisions during
// replay verification (ShipRoundStatus::aiControlled), so implicit orders need no pinning: an
// auto-resolved battle here is exactly session::autoResolve (see the bridge regression test).
void requireEncounter() {
    if (!B.session.active || !B.session.pending) fail("No tactical encounter awaits orders");
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
    std::string category, severity, text, kind;
    int systemId = 0, fleetId = 0, x = -1, y = -1, battle = -1, dueTurn = -1, stationId = 0, eventId = 0;
    std::uint64_t proposalId = 0;
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
            BriefEvent e; e.category = old ? "combat" : "colonization"; e.severity = "good"; e.systemId = c.systemId;
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
    // The turn chronicle (TurnReport::chronicle, one categorised entry with structured references per
    // player-visible report line). Lines already covered by the structured items above are skipped;
    // everything else keeps the core's category and references, with presentation tags added.
    for (const auto& entry : r.chronicle) {
        const std::string& raw = entry.text;
        if (contains(raw, "advanced to level") || startsWith(raw, "Discovered ") || startsWith(raw, "Colony founded at ") ||
            startsWith(raw, "Battle at sector ")) continue;
        if (!r.invasions.empty() && (startsWith(raw, "Invasion of ") || contains(raw, " captured by "))) continue;
        BriefEvent e; e.severity = "info";
        const int cat = static_cast<int>(entry.category);
        e.category = enumName(cat, ChronicleKeys);
        e.text = humanise(after, raw, &e.systemId);
        if (entry.systemId) e.systemId = entry.systemId;
        if (entry.fleetId) e.fleetId = entry.fleetId;
        if (entry.x >= 0 && entry.y >= 0 && entry.x < after.width && entry.y < after.height && botf::explored(after, entry.x, entry.y)) {
            e.x = entry.x; e.y = entry.y;
        }
        e.stationId = entry.stationId;
        e.eventId = entry.eventId;
        switch (entry.category) {
        case botf::ChronicleCategory::Construction:
            if (contains(raw, "paused")) e.severity = "warn";
            else if (contains(raw, "launched at")) e.severity = "good";
            break;
        case botf::ChronicleCategory::Combat:
            if (contains(raw, "captured by") || contains(raw, "failed") || contains(raw, "blocked")) e.severity = "alert";
            break;
        case botf::ChronicleCategory::Exploration:
        case botf::ChronicleCategory::Colonization:
            if (contains(raw, "paused")) e.severity = "warn";
            else if (contains(raw, "completed") || contains(raw, "survey")) e.severity = "good";
            break;
        case botf::ChronicleCategory::Stations:
            if (contains(raw, "destroyed") || contains(raw, "captured") || contains(raw, "boarded") || contains(raw, "lost") ||
                contains(raw, "abandoned") || contains(raw, "dropped")) e.severity = "alert";
            else if (contains(raw, "operational") || contains(raw, "commissioned") || contains(raw, "completed") || contains(raw, "assembled"))
                e.severity = "good";
            break;
        case botf::ChronicleCategory::Events:
            if (entry.eventId) e.kind = "galacticEvent";
            if (contains(raw, "no decision")) e.severity = "warn";
            if (const auto at = raw.find("Decide by turn "); at != std::string::npos) {
                e.kind = "eventChoice"; e.severity = "warn";
                try { e.dueTurn = std::stoi(raw.substr(at + 15)); } catch (...) {}
            } else if (contains(raw, "shortage") || contains(raw, "unrest") || contains(raw, "warning") || contains(raw, "plague") ||
                       contains(raw, "Plague") || contains(raw, "detonat") || contains(raw, "lost")) e.severity = "warn";
            else if (contains(raw, "windfall") || contains(raw, "celebration") || contains(raw, "boom") || contains(raw, "breakthrough") ||
                     contains(raw, "salvage") || contains(raw, "Salvage")) e.severity = "good";
            break;
        case botf::ChronicleCategory::Victory:
            e.severity = contains(raw, "defeat") || contains(raw, "lost the") ? "alert" : entry.empireId && entry.empireId != me ? "warn" : "good";
            break;
        case botf::ChronicleCategory::Research: e.severity = "good"; break;
        default: break;
        }
        if (entry.category == botf::ChronicleCategory::Diplomacy) {
            // Modern diplomacy: the core's own player-facing sentences (Simulation.cpp reportDiplomaticEvent,
            // submitAiProposals), tagged so the UI can link them to the inbox and treaty history.
            const bool news = startsWith(raw, "Galactic news");
            if (news) e.kind = "news";
            if (contains(raw, "calls on us to honor")) { e.kind = "callToArms"; e.severity = "alert"; }
            else if (contains(raw, "respond by turn")) {
                e.kind = contains(raw, "demands") ? "tribute" : contains(raw, "gift") ? "gift" : "proposal";
                e.severity = e.kind == "tribute" ? "warn" : "info";
                if (const auto at = raw.find("(proposal "); at != std::string::npos) {
                    try { e.proposalId = std::stoull(raw.substr(at + 10)); } catch (...) {}
                }
                if (const auto at = raw.find("respond by turn "); at != std::string::npos) {
                    try { e.dueTurn = std::stoi(raw.substr(at + 16)); } catch (...) {}
                }
            } else if (contains(raw, "broke its treaty")) { e.kind = "betrayal"; e.severity = news ? "warn" : "alert"; }
            else if (contains(raw, "refused") && contains(raw, "call to arms")) { e.kind = "callRefused"; e.severity = "warn"; }
            else if (contains(raw, "joined the war") || contains(raw, "honored its alliance")) { e.kind = "callHonored"; e.severity = news ? "info" : "good"; }
            else if (contains(raw, "eliminated")) { e.kind = "eliminated"; e.severity = "info"; }
            else if (contains(raw, "seceded")) { e.kind = "secession"; e.severity = contains(raw, "our empire") ? "alert" : "info"; }
            else if (contains(raw, "declared war")) { e.kind = "war"; e.severity = news ? "info" : "alert"; }
            else if (contains(raw, "made peace")) { e.kind = "peace"; e.severity = news ? "info" : "good"; }
            else if (contains(raw, "Dissolved") || contains(raw, "dissolved")) { e.kind = "dissolved"; e.severity = "warn"; }
            else if (contains(raw, "Rejected") || contains(raw, "Ignored")) e.severity = "warn";
            else if (contains(raw, "Accepted") || contains(raw, "Established")) e.severity = "good";
            else if (contains(raw, "at war")) e.severity = "alert";
        } else if (contains(raw, "seceded") || contains(raw, "Galactic news") || startsWith(raw, "Diplomacy")) {
            // Defensive: diplomatic sentences the classifier filed elsewhere stay with diplomacy.
            e.category = "diplomacy";
            if (startsWith(raw, "Galactic news")) e.kind = "news";
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
    // Diplomatic engine events reach the player through the core's own report sentences above
    // (reportDiplomaticEvent covers every event involving the player), so they are not repeated.
    // Calls to arms still open after this turn get an explicit reminder with their deadline.
    try {
        for (const auto& call : botf::openCallsToArms(after, me)) {
            const auto* rel = botf::majorRelation(after, me, call.aggressor);
            if ((rel && rel->atWar) || call.issuedTurn >= after.turn) continue;   // honoured, or announced this turn
            BriefEvent e; e.category = "diplomacy"; e.severity = "warn"; e.kind = "callToArms"; e.dueTurn = call.dueTurn;
            e.text = empireLabel(call.defender) + " still awaits our answer to its call to arms against " + empireLabel(call.aggressor) +
                " (declare war by turn " + std::to_string(call.dueTurn) + ").";
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
        if (!e.kind.empty()) w.field("kind", e.kind);
        if (e.proposalId) w.field("proposalId", e.proposalId);
        if (e.dueTurn >= 0) w.field("dueTurn", e.dueTurn);
        if (e.stationId) w.field("stationId", e.stationId);
        if (e.eventId) w.field("eventId", e.eventId);
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
    // New games go through botf::newCampaign: the ruleset preset (Remastered by default,
    // docs/GAMEPLAY-REMASTER.md 1) and the minor-race roster (docs/MINOR-RACES.md) are placed there.
    botf::CampaignSetup setup;
    setup.seed = seed; setup.empires = empires; setup.width = width; setup.height = height; setup.systems = systems;
    setup.playerEmpire = faction; setup.era = static_cast<botf::StartingEra>(era); setup.reference = ruleset != "prototype";
    setup.preset = botf::RulesetPreset::Remastered;
    if (a.has("preset") && !botf::parseRulesetPreset(a.getString("preset"), setup.preset)) fail("Unknown ruleset preset");
    auto s = botf::newCampaign(setup);
    // "wego": false keeps tactical rules 3 in a Remastered game (the WASM-recorded determinism fixture predates rules 4).
    if (a.has("wego") && !a.getBool("wego")) s.rules.wegoCombat = false;
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
    // Past the race's armed classes the list repeats, so large drills (up to 24 a side) stay mixed.
    for (int i = 0; i < count && !picks.empty(); ++i) {
        const auto& r = *picks[static_cast<std::size_t>(i) % picks.size()];
        botf::Fleet f;
        f.id = s.nextFleetId++; f.ownerId = owner; f.x = f.targetX = x; f.y = f.targetY = y;
        f.shipClass = botf::Building::Destroyer; f.referenceShipId = r.id;
        f.hull = botf::fleetMaxHull(f); f.shields = botf::fleetMaxShields(f);
        s.fleets.push_back(f);
    }
}

void markExplored(std::vector<int>& explored, int cell) { explored.push_back(cell); }
void normaliseExplored(std::vector<int>& explored) {
    std::sort(explored.begin(), explored.end());
    explored.erase(std::unique(explored.begin(), explored.end()), explored.end());
}

botf::Station makeStation(const botf::GameState& s, int owner, int x, int y, botf::StationType type) {
    botf::Station st;
    st.id = s.nextStationId; st.ownerId = owner; st.x = x; st.y = y; st.type = type; st.builtTurn = s.turn;
    const auto stats = botf::stationStats(s, owner, type);
    st.hull = stats.maxHull; st.shields = stats.maxShields;
    return st;
}

void newBattleDrill(bool withStation, int shipsPerSide = 3, bool stationForFoe = false) {
    shipsPerSide = std::clamp(shipsPerSide, 1, 24);
    auto s = botf::generateReferenceCampaign(1999);
    // The station variant plays under the Remastered preset, where outposts and starbases fight.
    if (withStation) botf::applyRulesetPreset(s, botf::RulesetPreset::Remastered);
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
    addDrillShips(armed, s.playerEmpire, sol.x, sol.y, shipsPerSide);
    addDrillShips(armed, foe, sol.x, sol.y, shipsPerSide);
    try { botf::validate(armed); s = std::move(armed); } catch (...) { botf::validate(s); }
    if (withStation) {
        // Sol's starbase joins the defence as an immobile combatant (fleet id StationCombatantBase + id).
        auto fortified = s;
        const auto& home = fortified.systems.front();
        // stationForFoe: the rival's starbase instead (a station fight against the player), validated like the rest.
        fortified.stations.push_back(makeStation(fortified, stationForFoe ? foe : s.playerEmpire, home.x, home.y, botf::StationType::Starbase));
        fortified.nextStationId++;
        try { botf::validate(fortified); s = std::move(fortified); }
        catch (...) {
            if (!stationForFoe) throw;
            auto own = s;
            own.stations.push_back(makeStation(own, s.playerEmpire, home.x, home.y, botf::StationType::Starbase));
            own.nextStationId++;
            botf::validate(own);
            s = std::move(own);
        }
    }
    botf::Command war; war.id = s.lastAcceptedCommand + 1; war.type = botf::CommandType::DeclareWar; war.targetEmpire = foe;
    botf::submit(s, war);
    startGame(std::move(s), withStation ? "drill-station" : "drill");
}

// Combat sandbox: the drill's Sol squadrons (Federation against the Klingon patrol, up to 24 a side) as a
// standalone tactical rules 4 encounter. The player commands the Federation side.
void newCombatSandbox(int shipsPerSide, std::uint64_t seed) {
    shipsPerSide = std::clamp(shipsPerSide, 1, 24);
    auto s = botf::generateReferenceCampaign(1999);
    botf::applyRulesetPreset(s, botf::RulesetPreset::Remastered);
    s.eventsEnabled = false; s.aiAggressionEnabled = false;
    const auto& sol = s.systems.front();
    auto rival = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const botf::Fleet& f) { return f.ownerId != s.playerEmpire; });
    if (rival == s.fleets.end()) fail("Sandbox needs a rival ship");
    const int foe = rival->ownerId;
    s.fleets.erase(std::remove_if(s.fleets.begin(), s.fleets.end(), [&](const botf::Fleet& f) {
        return f.x == sol.x && f.y == sol.y; }), s.fleets.end());
    addDrillShips(s, s.playerEmpire, sol.x, sol.y, shipsPerSide);
    addDrillShips(s, foe, sol.x, sol.y, shipsPerSide);
    for (auto& r : s.majorRelations)
        if ((r.first == s.playerEmpire && r.second == foe) || (r.first == foe && r.second == s.playerEmpire)) { r.atWar = true; r.contact = true; }
    std::vector<int> ids;
    for (const auto& f : s.fleets) if (f.x == sol.x && f.y == sol.y && (f.ownerId == s.playerEmpire || f.ownerId == foe)) ids.push_back(f.id);
    auto enc = tac::constructEncounter(s, ids, seed ? seed : 8112026);
    enc.controllingOwnerId = s.playerEmpire;
    tac::enableWegoRules(enc);
    B.sandboxState = std::move(s);
    B.sandbox = std::move(enc);
}

// Frontier drill (also the e2e fixture for the schema-13 layer): a Remastered campaign in the
// Developed era where long-range sensors have charted the region around the capital and every
// anomaly. A troop transport waits at the capital, ready to assemble an outpost; an outpost already
// stands nearby and can be upgraded to a starbase; and two galactic events await a decision.
void newFrontierDrill() {
    botf::CampaignSetup setup;
    setup.seed = 7707; setup.era = botf::StartingEra::Developed; setup.preset = botf::RulesetPreset::Remastered;
    auto s = botf::newCampaign(setup);
    s.aiAggressionEnabled = false;
    s.eventsEnabled = false;   // only the drill's own events
    const int me = s.playerEmpire;
    const auto home = std::find_if(s.colonies.begin(), s.colonies.end(), [&](const botf::Colony& c) { return c.ownerId == me; });
    if (home == s.colonies.end()) fail("Drill needs a home colony");
    const auto* capital = systemById(s, home->systemId);
    // Long-range survey: the capital's neighbourhood and every anomaly are charted.
    for (int y = 0; y < s.height; ++y) for (int x = 0; x < s.width; ++x)
        if (std::max(std::abs(x - capital->x), std::abs(y - capital->y)) <= 4) markExplored(s.explored, y * s.width + x);
    for (const auto& f : s.galaxyFeatures) {
        markExplored(s.explored, f.y * s.width + f.x);
        if (f.type == botf::GalaxyFeatureType::IonStorm)
            for (const auto& [cx, cy] : botf::ionStormCells(s, f)) markExplored(s.explored, cy * s.width + cx);
    }
    normaliseExplored(s.explored);
    s.credits = std::max<decltype(s.credits)>(s.credits, 2600);

    // A troop transport at the capital.
    const int race = catalogueRaceOf(me);
    const botf::referenceData::ShipRecord* transport = nullptr;
    for (const auto& ship : botf::referenceData::ShipCatalogue)
        if (ship.raceId == race && ship.role == 9) {   // role 9: troop transport
            const int t = *std::max_element(ship.technologyRequirements.begin(), ship.technologyRequirements.end());
            if (!transport || t < *std::max_element(transport->technologyRequirements.begin(), transport->technologyRequirements.end())) transport = &ship;
        }
    {
        botf::Fleet f;
        f.id = s.nextFleetId++; f.ownerId = me; f.x = f.targetX = capital->x; f.y = f.targetY = capital->y;
        f.shipClass = botf::Building::Transport; f.referenceShipId = transport ? transport->id : -1;
        f.hull = botf::fleetMaxHull(f); f.shields = botf::fleetMaxShields(f);
        s.fleets.push_back(f);
    }
    // An outpost two sectors out, in charted open space.
    for (int radius = 2; radius <= 3 && s.stations.empty(); ++radius)
        for (int dy = -radius; dy <= radius && s.stations.empty(); ++dy)
            for (int dx = -radius; dx <= radius && s.stations.empty(); ++dx) {
                const int x = capital->x + dx, y = capital->y + dy;
                if (std::max(std::abs(dx), std::abs(dy)) != radius || x < 0 || y < 0 || x >= s.width || y >= s.height) continue;
                if (systemAtCell(s, x, y) || !botf::featuresAt(s, x, y).empty() || !botf::explored(s, x, y)) continue;
                s.stations.push_back(makeStation(s, me, x, y, botf::StationType::Outpost));
                s.nextStationId++;
            }
    // Two decisions from the curated deck.
    const auto addEvent = [&](botf::GalacticEventKind kind, int x, int y) {
        botf::GalacticEvent e;
        e.id = s.nextEventId++; e.kind = kind; e.empireId = me; e.x = x; e.y = y;
        e.startTurn = s.turn; e.choiceDeadline = s.turn + 2; e.endTurn = s.turn + 3;
        s.galacticEvents.push_back(e);
    };
    const auto scout = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const botf::Fleet& f) { return f.ownerId == me && f.shipClass == botf::Building::Scout; });
    addEvent(botf::GalacticEventKind::DerelictVessel, scout != s.fleets.end() ? scout->x : capital->x, scout != s.fleets.end() ? scout->y : capital->y);
    addEvent(botf::GalacticEventKind::ScientificBreakthrough, -1, -1);
    botf::validate(s);
    queueStarterProjects(s);
    startGame(std::move(s), "frontier");
}

// A diplomacy training scenario (also the e2e fixture for the incoming-proposal inbox and calls to
// arms). Every step goes through the core's diplomacy engine (DiplomacyEngine::establishTreaty,
// declareWar, submitProposal) with a context built from the campaign, then the state is validated:
//   * the Cardassian Union has lost its colonies, so it is eliminated and its treaty with us dissolves;
//   * we hold a Full Alliance with the Romulan Star Empire;
//   * the Klingon Empire betrays its non-aggression pact with the Romulans and declares war on them,
//     which calls us to arms;
//   * the Ferengi propose a trade pact and the Klingons demand tribute, both awaiting our answer.
dip::SimpleDiplomacyContext campaignContext(const botf::GameState& s) {
    dip::SimpleDiplomacyContext ctx;
    ctx.setHuman(dip::ParticipantId::major(s.playerEmpire));
    std::vector<int> majors{s.playerEmpire};
    for (const auto& r : s.rivals) majors.push_back(r.id);
    std::sort(majors.begin(), majors.end());
    for (int id : majors) {
        const auto p = dip::ParticipantId::major(id);
        if (botf::empireEliminated(s, id)) ctx.setEliminated(p, true); else ctx.majors.push_back(id);
        std::int64_t power = 0;
        for (const auto& f : s.fleets) if (f.ownerId == id) power += botf::fleetAttack(f);
        ctx.setPower(p, power, 100);
        ctx.setBalance(p, botf::empire(s, id).credits);
    }
    for (const auto& r : s.majorRelations) {
        const auto a = dip::ParticipantId::major(r.first), b = dip::ParticipantId::major(r.second);
        ctx.setContact(a, b, r.contact); ctx.setWar(a, b, r.atWar); ctx.setTrust(a, b, r.trust);
    }
    for (const auto& m : s.minors) ctx.setMinorOwner(m.systemId, m.memberOf);
    return ctx;
}

botf::MajorRelation& mutableRelation(botf::GameState& s, int a, int b) {
    for (auto& r : s.majorRelations)
        if ((r.first == a && r.second == b) || (r.first == b && r.second == a)) return r;
    fail("No relation between those empires");
}

void applyEngineDelta(botf::GameState& s, const dip::TurnResolutionDelta& delta) {
    for (const auto& op : delta.operations) {
        if (op.partyA.isMinor || op.partyB.isMinor || op.partyA == op.partyB) continue;
        if (op.opType == dip::DiplomaticOpType::SetWarState) {
            auto& rel = mutableRelation(s, op.partyA.id, op.partyB.id);
            rel.atWar = op.setAtWar;
            rel.warStartTurn = s.turn;
            if (op.setAtWar) rel.tradeTreaty = false;
        } else if (op.opType == dip::DiplomaticOpType::AdjustTrust) {
            auto& rel = mutableRelation(s, op.partyA.id, op.partyB.id);
            rel.trust = std::clamp(rel.trust + op.attitudeDelta, 0, 100);
        }
    }
}

void newDiplomacyDrill() {
    auto s = botf::generateReferenceCampaign(2024, 5);
    s.eventsEnabled = false;
    s.aiAggressionEnabled = false;
    const int me = s.playerEmpire;
    const int klingon = 2, romulan = 3, cardassian = 4, ferengi = 5;
    for (auto& r : s.majorRelations) { r.contact = true; r.trust = 55; }
    auto P = [](int id) { return dip::ParticipantId::major(id); };
    auto& ds = s.diplomacyEngineState;
    botf::syncDiplomacyState(s);
    {
        dip::TurnResolutionDelta delta;
        ds.currentTurn = s.turn;
        dip::DiplomacyEngine::establishTreaty(ds, P(me), P(cardassian), dip::TreatyType::Friendship, dip::TreatyDuration::Turns50, delta);
    }
    // The Cardassian Union falls: no colonies (its fleets and task forces are gone too).
    std::erase_if(s.colonies, [&](const botf::Colony& c) { return c.ownerId == cardassian; });
    std::erase_if(s.fleets, [&](const botf::Fleet& f) { return f.ownerId == cardassian; });
    std::erase_if(s.taskForces, [&](const botf::TaskForce& t) { return t.ownerId == cardassian; });
    std::erase_if(s.tradeRoutes, [&](const botf::TradeRoute& t) { return t.ownerId == cardassian; });
    for (auto& m : s.minors) if (m.memberOf == cardassian) m.memberOf = 0;
    botf::validate(s);
    botf::advance(s);   // the engine dissolves the eliminated empire's treaties
    ds.currentTurn = s.turn;

    mutableRelation(s, me, romulan).trust = 80;
    mutableRelation(s, me, ferengi).trust = 70;
    mutableRelation(s, klingon, romulan).trust = 45;
    {
        dip::TurnResolutionDelta delta;
        dip::DiplomacyEngine::establishTreaty(ds, P(me), P(romulan), dip::TreatyType::Alliance, dip::TreatyDuration::Indefinite, delta);
        dip::DiplomacyEngine::establishTreaty(ds, P(klingon), P(romulan), dip::TreatyType::NonAggression, dip::TreatyDuration::Turns25, delta);
    }
    {
        const auto ctx = campaignContext(s);
        dip::TurnResolutionDelta delta;
        dip::DiplomacyEngine::declareWar(ds, P(klingon), P(romulan), delta, &ctx);
        applyEngineDelta(s, delta);
    }
    {
        const auto ctx = campaignContext(s);
        const auto pendingFrom = [&](int proposer) {
            for (const auto& p : botf::incomingProposals(s, me)) if (!p.proposer.isMinor && p.proposer.id == proposer) return true;
            return false;
        };
        std::string why;
        // The Ferengi planner may already have proposed a trade pact on its own during the first turn.
        if (!pendingFrom(ferengi) && !dip::DiplomacyEngine::submitProposal(ds, ctx, P(ferengi), P(me), dip::TreatyType::Trade,
                dip::TreatyDuration::Turns25, dip::DiplomaticTone::Cordial, 0, 0, nullptr, &why, 2)) fail("Drill proposal rejected: " + why);
        if (!pendingFrom(klingon) && !dip::DiplomacyEngine::submitProposal(ds, ctx, P(klingon), P(me), dip::TreatyType::None,
                dip::TreatyDuration::Indefinite, dip::DiplomaticTone::Demanding, 0, 150, nullptr, &why, 2)) fail("Drill tribute rejected: " + why);
    }
    botf::validate(s);
    queueStarterProjects(s);
    startGame(std::move(s), "diplomacy");
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
    for (const auto& s : botf::roster::ExpandedShipRecords) {   // Expanded roster (ids >= 1000), with "roster" detail.
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
    w.key("minorRaces").beginArray();
    for (const auto& r : botf::minors::Roster) {
        w.beginObject();
        writeMinorProfile(w, r);
        w.field("techLevel", r.techLevel); w.field("population", r.population);
        w.key("affinity").beginArray(); for (int i = 1; i <= 5; ++i) w.value(r.affinity[static_cast<std::size_t>(i)]); w.endArray();
        w.endObject();
    }
    w.endArray();
    w.endObject();
}

// ------------------------------------------------------------------------------------------------
// Dispatch
// ------------------------------------------------------------------------------------------------
// Content pack identity (docs/DATA-FORMAT.md section 6). The checksum is a 16-digit hex string (JSON numbers lose 64-bit precision).
void writeContentInfo(Writer& w, const botf::content::PackInfo& info) {
    w.beginObject().field("id", info.id).field("name", info.name).field("format", info.format).field("revision", info.revision)
        .field("checksum", botf::content::checksumHex(info.checksum)).field("bytes", static_cast<std::int64_t>(info.bytes))
        .field("base", info.base).endObject();
}

std::string dispatch(const std::string& method, const Value& a) {
    Writer w;
    if (method == "meta") writeMeta(w);
    // loadContent {"bundle": "<pack bytes>"}: install a content pack the host read through res:// (the core never opens
    // files). Atomic: a bad pack leaves the active one in place. Call before newCampaign or loading a save.
    else if (method == "loadContent") writeContentInfo(w, botf::content::install(a.getString("bundle")));
    else if (method == "loadBaseContent") writeContentInfo(w, botf::content::installBase());
    else if (method == "contentInfo") writeContentInfo(w, botf::content::active());
    else if (method == "encyclopedia") writeEncyclopedia(w);
    else if (method == "newCampaign") { newCampaign(a); writeView(w); }
    else if (method == "newBattleDrill") { newBattleDrill(a.getBool("station"), a.getInt("ships", 3), a.getBool("foeStation")); writeView(w); }
    else if (method == "newFrontierDrill") { newFrontierDrill(); writeView(w); }
    else if (method == "station") writeStationDetail(w, a.getInt("stationId"));
    else if (method == "newDiplomacyDrill") { newDiplomacyDrill(); writeView(w); }
    else if (method == "headlessFingerprint") {
        // Fingerprint of the drafted turn advanced headlessly by botf::advance (auto-resolving every
        // battle): an auto-resolved session turn must commit exactly this state.
        requireGame(); requireNoSession();
        auto copy = B.state;
        botf::advance(copy);
        w.value(botf::fingerprint(copy));
    }
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
    else if (method == "tradeRoutes") writeTradeRoutes(w);
    else if (method == "diplomacy") writeDiplomacy(w);
    else if (method == "intel") writeIntel(w);
    else if (method == "canTerraform") {
        requireGame();
        w.value(botf::canTerraformPlanet(draft(), a.getInt("fleetId"), a.getInt("systemId"), a.getInt("planetIndex")));
    }
    else if (method == "endTurn") endTurn(w);
    else if (method == "briefing") w.raw(B.lastBriefing.empty() ? "null" : B.lastBriefing);
    else if (method == "combatSandbox") {
        newCombatSandbox(a.getInt("ships", 3), static_cast<std::uint64_t>(a.getInt("seed", 0)));
        writeEncounter(w, *B.sandbox, B.sandboxState.playerEmpire, B.sandboxState);
    } else if (a.getBool("sandbox") && (method == "tactical" || method == "tacticalOrder" || method == "tacticalRound" ||
                                        method == "tacticalAuto" || method == "tacticalForecast")) {
        // The same tactical calls against the combat sandbox.
        if (!B.sandbox) fail("No combat sandbox");
        auto& e = *B.sandbox;
        const int me = B.sandboxState.playerEmpire;
        if (method == "tacticalOrder") {
            if (a.getBool("all")) {
                int n = 0;
                auto trial = e;
                for (const auto& ship : e.ships) {
                    if (ship.ownerId != me || !tac::Encounter::shipActive(ship)) continue;
                    auto o = readTacticalOrder(a, e);
                    o.fleetId = ship.fleetId;
                    if (o.maneuver == tac::TacticalManeuver::Retreat && !e.mayRetreat(ship)) continue;
                    trial.submitOrder(o);
                    ++n;
                }
                e = std::move(trial);
                w.value(n);
            } else {
                e.submitOrder(readTacticalOrder(a, e));
                w.value(1);
            }
        } else if (method == "tacticalForecast") {
            writeForecast(w, e, readTacticalOrder(a, e));
        } else {
            if (method == "tacticalRound") { if (e.isFinished) fail("The sandbox battle is over"); e.resolveRound(); }
            if (method == "tacticalAuto") e.runUntilFinished();
            writeEncounter(w, e, me, B.sandboxState);
        }
    } else if (method == "tacticalForecast") {
        requireGame();
        if (!B.session.active || !B.session.pending) fail("No tactical encounter awaits orders");
        writeForecast(w, *B.session.pending, readTacticalOrder(a, *B.session.pending));
    } else if (method == "tactical") {
        requireGame();
        if (B.session.active && B.session.pending) writeEncounter(w, *B.session.pending, B.state.playerEmpire);
        else w.null();
    } else if (method == "tacticalOrder") {
        requireGame();
        if (!B.session.active || !B.session.pending) fail("No tactical encounter awaits orders");
        const auto maneuver = static_cast<tac::TacticalManeuver>(enumIndex(a["maneuver"], ManeuverKeys, "maneuver", 0));
        const int target = a.getInt("targetFleetId");
        std::vector<int> ids;
        if (a.has("groupId")) {
            // Like-class group order: Encounter::submitGroupOrder validates every member atomically.
            auto trial = B.session;
            const int groupId = a.getInt("groupId");
            const auto* g = trial.pending->getGroup(groupId);
            if (!g) fail("No such tactical group");
            int count = 0;
            for (int fid : g->fleetIds)
                if (const auto* s = trial.pending->getShip(fid); s && tac::Encounter::shipActive(*s) &&
                    (maneuver != tac::TacticalManeuver::Retreat || s->canRetreat)) ++count;
            if (!count) fail(maneuver == tac::TacticalManeuver::Retreat ? "No ship in this group has a safe withdrawal sector" : "No active ships in this group");
            trial.pending->submitGroupOrder(groupId, maneuver, maneuver == tac::TacticalManeuver::Retreat ? 0 : target);
            B.session = std::move(trial);
            touch();
            w.value(count);
            return w.out;
        }
        if (a.getBool("all")) {
            for (const auto& ship : B.session.pending->ships)
                if (ship.ownerId == B.state.playerEmpire && ship.isAlive && ship.activeInCombat && !ship.retreated &&
                    (maneuver != tac::TacticalManeuver::Retreat || ship.canRetreat)) ids.push_back(ship.fleetId);
        } else ids.push_back(a.getInt("fleetId"));
        if (ids.empty()) fail(maneuver == tac::TacticalManeuver::Retreat ? "No ship has a safe withdrawal sector" : "No active ships to command");
        // Rules 4 subsystem aim ("subsystem": weapons, engines, shields, sensors, warpCore); none by default.
        const int aim = enumIndex(a["subsystem"], SubsystemKeys, "subsystem", 0);
        if (aim < 0 || aim > tac::MaxSubsystemValue) fail("Unknown subsystem");
        auto trial = B.session;   // all-or-nothing
        for (int id : ids) {
            const auto* ship = trial.pending->getShip(id);
            int t = maneuver == tac::TacticalManeuver::Retreat ? 0 : target;
            if (t) { const auto* ts = trial.pending->getShip(t); if (!ts || !ts->isAlive || ts->retreated || !ts->activeInCombat) t = 0; }
            (void)ship;
            botf::session::order(trial, {id, t, maneuver, static_cast<tac::Subsystem>(aim)});
        }
        B.session = std::move(trial);
        touch();
        w.value(static_cast<int>(ids.size()));
    } else if (method == "tacticalRound") {
        requireGame();
        requireEncounter();
        botf::session::resolveRound(B.session);
        touch();
        writeEncounter(w, *B.session.pending, B.state.playerEmpire);
    } else if (method == "tacticalAuto") {
        requireGame();
        requireEncounter();
        botf::session::autoResolve(B.session);
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
            if (const auto* ship = botf::roster::findShip(p.referenceShipId))
                name = std::string(ship->shipClass) + " " + ship->name;
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
