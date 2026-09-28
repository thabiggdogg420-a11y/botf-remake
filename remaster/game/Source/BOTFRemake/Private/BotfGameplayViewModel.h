#pragma once
// Engine-independent view models for the schema-13 gameplay layer (stations, anomalies, galactic events, victory,
// the turn chronicle, build templates) and the minor-race roster. See remaster/docs/GAMEPLAY-REMASTER.md and
// remaster/docs/MINOR-RACES.md for the rules, and remaster/docs/UNREAL-CLIENT.md for how this header is checked.
//
// Like BotfViewModel.h, nothing here includes Unreal headers: the functions take core types and return plain values
// or UTF-8 text, so the header is compiled and unit-tested with an ordinary C++ compiler against the core. Numbers
// come from core queries; the few rules repeated here are marked MIRROR, as in BotfViewModel.h, and listed in
// UNREAL-CLIENT.md until the core offers the query. The Slate widgets only convert the results.
//
// Fog of war: every function that describes the galaxy to the player shows only what the player may know: features
// on explored sectors (the core's knownFeatures), stations the player owns or currently sees (visibleStations), the
// player's own galactic events (empireEvents) and chronicle entries (already player-visible in the core).

#include "Simulation.h"
#include "TacticalCombat.h"
#include "ReferenceCatalogue.h"
#include "MinorRaces.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace botf::view {

// ---------------------------------------------------------------------------------------------------------------
// Shared lookups
// ---------------------------------------------------------------------------------------------------------------

inline const System* systemWithId(const GameState& state, int systemId) {
    for (const auto& system : state.systems)
        if (system.id == systemId) return &system;
    return nullptr;
}

inline std::string systemLabel(const GameState& state, int systemId) {
    const System* system = systemWithId(state, systemId);
    return system ? system->name : "System " + std::to_string(systemId);
}

inline std::string sectorText(int x, int y) { return "(" + std::to_string(x) + "," + std::to_string(y) + ")"; }

inline bool campaignEmpire(const GameState& state, int empireId) {
    if (empireId == state.playerEmpire) return true;
    return std::any_of(state.rivals.begin(), state.rivals.end(), [empireId](const EmpireState& e) { return e.id == empireId; });
}

// Row-major chart cell of a sector, or -1 outside the map.
inline int cellOf(const GameState& state, int x, int y) {
    return x >= 0 && y >= 0 && x < state.width && y < state.height ? y * state.width + x : -1;
}

// ---------------------------------------------------------------------------------------------------------------
// New campaign (ruleset presets and the minor-race roster)
// ---------------------------------------------------------------------------------------------------------------

// What the setup screen chose. New campaigns go through botf::newCampaign, the only generator that places the
// minor-race roster and applies a ruleset preset; the legacy generators stay the fixtures' baseline.
struct SetupChoice {
    std::uint64_t seed = 1999;
    int empires = 2;
    bool large = false;
    int player = 1;
    StartingEra era = StartingEra::Early;
    bool reference = true;
    RulesetPreset preset = RulesetPreset::Remastered;
    bool events = true;
};

inline CampaignSetup campaignSetup(const SetupChoice& choice) {
    CampaignSetup setup;
    setup.seed = choice.seed;
    setup.empires = choice.empires;
    setup.width = choice.large ? 20 : 16;
    setup.height = choice.large ? 15 : 12;
    setup.systems = choice.large ? 42 : 28;
    setup.playerEmpire = choice.player;
    setup.era = choice.era;
    setup.reference = choice.reference;
    setup.preset = choice.preset;
    return setup;
}

// A validated turn-0 campaign. "Random events off" stops new draws of either event system (eventsEnabled).
inline GameState startCampaign(const SetupChoice& choice) {
    GameState state = newCampaign(campaignSetup(choice));
    if (state.eventsEnabled != choice.events) {
        state.eventsEnabled = choice.events;
        validate(state);
    }
    return state;
}

inline std::string presetSummary(RulesetPreset preset) {
    return preset == RulesetPreset::Remastered
        ? "Outposts and starbases, nebulae, black holes, neutron stars, ion storms and wormholes, curated galactic events "
          "with decisions, and scientific and diplomatic victories alongside conquest and domination."
        : "Original-like rules: outposts and starbases, the original random events, and conquest or domination victory only.";
}

// Ruleset of a loaded or running campaign. Saves from before schema 13 load as Classic with every layer off.
inline std::string rulesetLabel(const GameState& state) {
    if (state.rules.preset == RulesetPreset::Remastered) return "Remastered";
    return state.rules.stations ? "Classic" : "Classic (older save: remaster layers off)";
}

// ---------------------------------------------------------------------------------------------------------------
// Galaxy features and anomalies (star chart)
// ---------------------------------------------------------------------------------------------------------------

// One charted feature cell. Ion storms produce one mark per explored cell of their plus-shaped front; a wormhole
// carries its exit only once the player has charted the exit.
struct FeatureMark {
    GalaxyFeatureType type = GalaxyFeatureType::Nebula;
    int featureId = 0;
    int x = 0, y = 0;
    int exitX = -1, exitY = -1;     // wormhole: charted exit endpoint
    int clusterId = 0;              // nebula cloud
    bool surveyed = false;          // the player collected this anomaly's survey reward
    int expiresTurn = 0;            // subspace rift: last turn it blocks the sector
};

inline bool surveyedBy(const GalaxyFeature& feature, int empireId) {
    return empireId >= 0 && empireId < 31 && (feature.surveyedMask & (1 << empireId)) != 0;
}

inline std::vector<FeatureMark> chartFeatures(const GameState& state) {
    std::vector<FeatureMark> marks;
    const int player = state.playerEmpire;
    for (const auto& feature : knownFeatures(state, player)) {
        FeatureMark mark;
        mark.type = feature.type;
        mark.featureId = feature.id;
        mark.clusterId = feature.clusterId;
        mark.surveyed = surveyedBy(feature, player);
        mark.expiresTurn = feature.expiresTurn;
        if (feature.type == GalaxyFeatureType::IonStorm) {
            for (const auto& cell : ionStormCells(state, feature)) {
                if (!explored(state, cell.first, cell.second)) continue;
                mark.x = cell.first; mark.y = cell.second;
                marks.push_back(mark);
            }
            continue;
        }
        mark.x = feature.x; mark.y = feature.y;
        if (feature.type == GalaxyFeatureType::Wormhole && feature.pairId) {
            for (const auto& exit : state.galaxyFeatures)
                if (exit.id == feature.pairId && explored(state, exit.x, exit.y)) { mark.exitX = exit.x; mark.exitY = exit.y; }
        }
        marks.push_back(mark);
    }
    return marks;
}

// What each anomaly does, in the player's terms (GAMEPLAY-REMASTER.md section 3).
inline std::string featureEffect(GalaxyFeatureType type) {
    switch (type) {
    case GalaxyFeatureType::Nebula:
        return "sensor-blind (ships inside are hidden and see only their own sector); shields fail in battle here";
    case GalaxyFeatureType::Wormhole:
        return "once charted, a ship can step straight to the other end; supply reaches through it";
    case GalaxyFeatureType::BlackHole: return "impassable";
    case GalaxyFeatureType::NeutronStar: return "damages ships: 10% of hull and half the shields each turn";
    case GalaxyFeatureType::IonStorm: return "drifting storm: ships inside lose shields and 5% hull each turn";
    case GalaxyFeatureType::SubspaceRift: return "impassable until it closes";
    }
    return "unknown effect";
}

// Hover text for a sector's anomalies; empty for unexplored sectors or sectors without features.
inline std::string featureTooltip(const GameState& state, int x, int y) {
    if (!explored(state, x, y)) return {};
    std::string text;
    for (const auto& feature : featuresAt(state, x, y)) {
        std::string line = galaxyFeatureName(feature.type);
        if (feature.type == GalaxyFeatureType::Wormhole) {
            const GalaxyFeature* exit = nullptr;
            for (const auto& other : state.galaxyFeatures) if (other.id == feature.pairId) exit = &other;
            line += exit && explored(state, exit->x, exit->y) ? " to " + sectorText(exit->x, exit->y) : std::string(", exit uncharted");
        }
        line += ": " + featureEffect(feature.type);
        if (feature.type == GalaxyFeatureType::SubspaceRift && feature.expiresTurn > 0)
            line += " (turn " + std::to_string(feature.expiresTurn) + ")";
        line += surveyedBy(feature, state.playerEmpire) ? ". Surveyed." : ". A scout ending its turn next to it surveys it for research.";
        if (!text.empty()) text += "\n";
        text += line;
    }
    return text;
}

// ---------------------------------------------------------------------------------------------------------------
// Stations (outposts and starbases)
// ---------------------------------------------------------------------------------------------------------------

struct StationMark {
    int id = 0, ownerId = 0, x = 0, y = 0;
    StationType type = StationType::Outpost;
    int supplyRadius = 0, sensorRadius = 0;   // Chebyshev radii: the rings are squares on the sector grid
    bool own = false, upgrading = false;
};

// Stations the player owns or currently has in sensor range.
inline std::vector<StationMark> chartStations(const GameState& state) {
    std::vector<StationMark> marks;
    for (const auto& station : visibleStations(state, state.playerEmpire)) {
        const StationStats stats = stationStats(state, station);
        StationMark mark;
        mark.id = station.id; mark.ownerId = station.ownerId; mark.x = station.x; mark.y = station.y; mark.type = station.type;
        mark.supplyRadius = stats.supplyRadius; mark.sensorRadius = stats.sensorRadius;
        mark.own = station.ownerId == state.playerEmpire;
        mark.upgrading = stationUpgradeFor(state, station.id) != nullptr;
        marks.push_back(mark);
    }
    return marks;
}

// The visible station in a sector, or nullptr.
inline const Station* visibleStationAt(const GameState& state, int x, int y) {
    const Station* station = stationAt(state, x, y);
    if (!station) return nullptr;
    if (station->ownerId != state.playerEmpire && !visible(state, state.playerEmpire, x, y)) return nullptr;
    return station;
}

// Station inspector: identity, condition, armament, reach, upkeep and any starbase upgrade in progress.
inline std::string stationInspectorText(const GameState& state, int stationId) {
    const Station* station = stationById(state, stationId);
    if (!station || (station->ownerId != state.playerEmpire && !visible(state, state.playerEmpire, station->x, station->y)))
        return "No station in sensor range.";
    const StationStats stats = stationStats(state, *station);
    const bool own = station->ownerId == state.playerEmpire;
    std::string text = stationName(*station) + " at " + sectorText(station->x, station->y) + ", " + empireName(station->ownerId);
    text += "\nHull " + std::to_string(station->hull) + "/" + std::to_string(stats.maxHull) +
        "  |  shields " + std::to_string(station->shields) + "/" + std::to_string(stats.maxShields) +
        "  |  beams " + std::to_string(stats.beam) + ", torpedoes " + std::to_string(stats.torpedo);
    text += "\nSupply radius " + std::to_string(stats.supplyRadius) + "  |  sensor radius " + std::to_string(stats.sensorRadius);
    if (own) text += "  |  upkeep " + std::to_string(stats.upkeep) + " credits per turn";
    if (stats.repairPercent > 0)
        text += "\nRepairs friendly ships in its sector by " + std::to_string(stats.repairPercent) +
            "% of hull per turn when they did not fight and no enemy is present.";
    text += "\nIn battle it fights for its owner, cannot move or retreat, and braces for -20% damage.";
    if (own) text += " Commissioned turn " + std::to_string(station->builtTurn) + ".";
    if (const StationUpgrade* upgrade = stationUpgradeFor(state, station->id)) {
        const StationUpgradeCost cost = stationUpgradeCost(state, station->ownerId);
        text += "\nStarbase upgrade: " + std::to_string(upgrade->work) + "/" + std::to_string(cost.industry) + " industry";
        if (own) text += ", funded by " + systemLabel(state, upgrade->fundingSystemId) + " (its build queue is paused)";
        text += ".";
    }
    return text;
}

// A colony that could fund a starbase upgrade of a station, with core's exact blocker ("" when legal).
struct UpgradeOption {
    int systemId = 0;
    std::string name, blocker;
    int distance = 0;
};

// Own colonies ordered by distance to the station (then id). Empty for foreign stations or starbases.
inline std::vector<UpgradeOption> upgradeOptions(const GameState& state, int stationId) {
    std::vector<UpgradeOption> options;
    const Station* station = stationById(state, stationId);
    if (!station || station->ownerId != state.playerEmpire || station->type != StationType::Outpost) return options;
    for (const auto& colony : state.colonies) {
        if (colony.ownerId != station->ownerId) continue;
        const System* system = systemWithId(state, colony.systemId);
        if (!system) continue;
        UpgradeOption option;
        option.systemId = colony.systemId;
        option.name = system->name;
        option.distance = std::max(std::abs(system->x - station->x), std::abs(system->y - station->y));
        option.blocker = stationUpgradeBlocker(state, stationId, colony.systemId);
        options.push_back(std::move(option));
    }
    std::sort(options.begin(), options.end(), [](const UpgradeOption& a, const UpgradeOption& b) {
        return a.distance != b.distance ? a.distance < b.distance : a.systemId < b.systemId;
    });
    return options;
}

// The first colony that can fund the upgrade now (0 when none).
inline int bestUpgradeFunder(const GameState& state, int stationId) {
    for (const auto& option : upgradeOptions(state, stationId))
        if (option.blocker.empty()) return option.systemId;
    return 0;
}

inline std::string upgradeCostText(const GameState& state) {
    const StationUpgradeCost cost = stationUpgradeCost(state, state.playerEmpire);
    return "Upgrade to starbase: " + std::to_string(cost.credits) + " credits now, then " + std::to_string(cost.industry) +
        " industry from one colony within 6 sectors (its build queue pauses until done). Needs Construction 2.";
}

// Build Outpost for one fleet: whether the order applies at all (troop transports in a station ruleset), core's
// blocker, the assembly time and any assembly in progress.
struct OutpostOrder {
    bool applicable = false;
    bool assembling = false;
    int progress = 0, turns = 0;
    std::string blocker;
};

inline OutpostOrder outpostOrder(const GameState& state, const Fleet& fleet) {
    OutpostOrder order;
    order.applicable = state.rules.stations && fleet.shipClass == Building::Transport && fleet.ownerId == state.playerEmpire;
    order.assembling = fleet.buildingOutpost;
    order.progress = fleet.outpostProgress;
    order.turns = outpostBuildTurns(state, fleet.ownerId);
    order.blocker = outpostBlocker(state, fleet.id);
    return order;
}

inline std::string outpostOrderText(const OutpostOrder& order) {
    if (order.assembling)
        return "Assembling an outpost: " + std::to_string(order.progress) + " of " + std::to_string(order.turns) +
            " turns. Any new order to this transport abandons it.";
    if (!order.applicable) return "Only troop transports assemble outposts.";
    if (!order.blocker.empty()) return "Build outpost: " + order.blocker + ".";
    return "Build outpost here: assembles in " + std::to_string(order.turns) +
        " turns; the transport becomes the outpost (supply radius 3, sensor radius 2).";
}

// ---------------------------------------------------------------------------------------------------------------
// Stations in tactical combat and battle records
// ---------------------------------------------------------------------------------------------------------------

// Stations take part in encounters as fleet ids >= StationCombatantBase and are recorded as Destroyer-class
// participants (they must satisfy the isShip bound). Clients must detect them by id, never by class.
inline bool isStation(int fleetId) { return isStationCombatant(fleetId); }

// Display name of a recorded participant: a station by its id, a ship by its class.
inline std::string participantLabel(const GameState* state, int fleetId, int ownerId, Building shipClass, int referenceShipId) {
    if (isStationCombatant(fleetId)) {
        const int id = stationIdForCombatant(fleetId);
        if (state)
            if (const Station* station = stationById(*state, id)) return stationName(*station);
        return "Station " + std::to_string(id);
    }
    Fleet ship;
    ship.id = fleetId; ship.ownerId = ownerId; ship.shipClass = shipClass; ship.referenceShipId = referenceShipId;
    return fleetName(ship);
}

// ---------------------------------------------------------------------------------------------------------------
// Curated galactic events
// ---------------------------------------------------------------------------------------------------------------

struct EventOptionView {
    int index = 0;
    std::string label, effect;
    bool isDefault = false;
};

struct EventCard {
    int id = 0;
    GalacticEventKind kind = GalacticEventKind::Plague;
    std::string title, description, where;
    int focusCell = -1;            // chart cell of the affected system or sector, -1 when none
    bool needsDecision = false;    // a decision is still open in the committed campaign
    int deadline = 0, turnsLeft = 0;
    int chosen = -1;               // drafted this turn, or already decided; -1 undecided
    bool chosenThisTurn = false;   // the choice is a drafted order (cancellable until the turn ends)
    int defaultOption = -1;
    int endTurn = 0;
    std::vector<EventOptionView> options;
};

// A ResolveEventChoice drafted this turn for the event, or -1.
inline int draftedEventChoice(const GameState& committed, int eventId) {
    for (auto it = committed.pending.rbegin(); it != committed.pending.rend(); ++it)
        if (it->type == CommandType::ResolveEventChoice && it->eventId == eventId) return it->eventOption;
    return -1;
}

// The player's active events, from the committed campaign (decisions are drafted as orders).
inline std::vector<EventCard> eventCards(const GameState& committed) {
    std::vector<EventCard> cards;
    for (const auto& event : empireEvents(committed, committed.playerEmpire)) {
        EventCard card;
        card.id = event.id;
        card.kind = event.kind;
        card.title = galacticEventName(event.kind);
        card.description = describeEvent(committed, event);
        if (event.systemId) {
            card.where = systemLabel(committed, event.systemId);
            if (const System* system = systemWithId(committed, event.systemId)) card.focusCell = cellOf(committed, system->x, system->y);
        } else if (event.x >= 0) {
            card.where = "sector " + sectorText(event.x, event.y);
            card.focusCell = cellOf(committed, event.x, event.y);
        }
        card.needsDecision = event.empireId == committed.playerEmpire && event.choiceDeadline > 0 && event.choice < 0;
        card.deadline = event.choiceDeadline;
        card.turnsLeft = std::max(0, event.choiceDeadline - committed.turn);
        card.endTurn = event.endTurn;
        card.defaultOption = eventDefaultChoice(event.kind);
        const int drafted = card.needsDecision ? draftedEventChoice(committed, event.id) : -1;
        card.chosenThisTurn = drafted >= 0;
        card.chosen = drafted >= 0 ? drafted : event.choice;
        for (const auto& option : eventChoices(event.kind))
            card.options.push_back({option.index, option.label, option.effect, option.isDefault});
        cards.push_back(std::move(card));
    }
    return cards;
}

inline std::string optionLabel(const EventCard& card, int index) {
    for (const auto& option : card.options) if (option.index == index) return option.label;
    return "option " + std::to_string(index + 1);
}

// "decide by turn 14 (this turn)" / "... (2 turns left)"
inline std::string eventDeadlineText(const EventCard& card) {
    if (!card.needsDecision) return {};
    return "decide by turn " + std::to_string(card.deadline) + " (" + (card.turnsLeft == 0 ? std::string("this turn") :
        std::to_string(card.turnsLeft) + (card.turnsLeft == 1 ? " turn left" : " turns left")) + ")";
}

inline std::string eventStatusText(const EventCard& card) {
    if (card.needsDecision && card.chosenThisTurn)
        return "Ordered: " + optionLabel(card, card.chosen) + ". It takes effect when the turn ends.";
    if (card.needsDecision)
        return "Undecided: " + eventDeadlineText(card) + ". Otherwise " + optionLabel(card, card.defaultOption) + " applies.";
    if (card.chosen >= 0) return "Decided: " + optionLabel(card, card.chosen) + ".";
    return {};
}

// End-turn warnings: every open decision without a drafted choice, with what happens if it stays open.
inline std::vector<std::string> unresolvedEventWarnings(const GameState& committed) {
    std::vector<std::string> lines;
    for (const auto& card : eventCards(committed)) {
        if (!card.needsDecision || card.chosenThisTurn) continue;
        std::string line = card.title + (card.where.empty() ? std::string() : " at " + card.where) + ": no decision. ";
        line += card.turnsLeft == 0
            ? "The default (" + optionLabel(card, card.defaultOption) + ") applies when this turn ends."
            : std::to_string(card.turnsLeft) + (card.turnsLeft == 1 ? " turn" : " turns") + " left before the default (" +
                optionLabel(card, card.defaultOption) + ") applies.";
        lines.push_back(std::move(line));
    }
    return lines;
}

// ---------------------------------------------------------------------------------------------------------------
// Victory conditions, megaproject and the end of the game
// ---------------------------------------------------------------------------------------------------------------

struct VictoryRow {
    VictoryCondition condition = VictoryCondition::Conquest;
    std::string name, explanation;
    int percent = 0;
    bool enabled = true, achieved = false;
};

inline std::vector<VictoryRow> victoryRows(const GameState& state, int empireId) {
    std::vector<VictoryRow> rows;
    if (!campaignEmpire(state, empireId)) return rows;
    for (const auto& progress : victoryProgress(state, empireId)) {
        VictoryRow row;
        row.condition = progress.condition;
        row.name = victoryConditionName(progress.condition);
        row.explanation = progress.explanation;
        row.percent = std::clamp(progress.percent, 0, 100);
        row.enabled = progress.enabled;
        row.achieved = progress.achieved;
        rows.push_back(std::move(row));
    }
    return rows;
}

struct MegaprojectView {
    bool enabled = false;          // the ruleset has alternative victories
    std::string name;
    int industryCost = 0, workPerTurn = 0, work = 0;
    std::int64_t creditCost = 0;
    int systemId = 0;              // colony building it, 0 when not under way
    bool underWay = false, here = false;
    std::string startBlocker;      // core's reason the project cannot start at the viewed colony ("" when it can)
    int turnsLeft = -1;            // at the colony's current industry, -1 when unknown or stalled
    int threshold = 0;             // research level every field needs
};

inline MegaprojectView megaprojectView(const GameState& state, int empireId, int systemId) {
    MegaprojectView view;
    view.enabled = state.rules.alternativeVictories;
    view.name = megaprojectName(state);
    view.industryCost = megaprojectIndustryCost(state);
    view.creditCost = megaprojectCreditCost(state);
    view.threshold = scientificVictoryThreshold(state);
    if (!campaignEmpire(state, empireId)) return view;
    view.workPerTurn = megaprojectWorkPerTurn(state, empireId);
    if (const Megaproject* project = empireMegaproject(state, empireId)) {
        view.underWay = true;
        view.work = project->work;
        view.systemId = project->systemId;
        view.here = project->systemId == systemId;
        if (const Colony* colony = colonyAt(state, project->systemId)) {
            // MIRROR of divertIndustry's megaproject share (core query wanted: megaprojectPace(state, empire)).
            const int pace = std::min(view.workPerTurn, economy(state, *colony).industry);
            const int remaining = std::max(0, view.industryCost - view.work);
            if (pace > 0) view.turnsLeft = (remaining + pace - 1) / pace;
        }
    }
    view.startBlocker = megaprojectBlocker(state, empireId, systemId);
    return view;
}

inline std::string megaprojectText(const GameState& state, const MegaprojectView& view) {
    if (!view.enabled) return "The Classic ruleset has no scientific victory.";
    std::string text = view.name + ": " + std::to_string(view.industryCost) + " industry and " + std::to_string(view.creditCost) +
        " credits. The building colony gives it up to " + std::to_string(view.workPerTurn) +
        " industry per turn and builds with the rest. Requires turn 50 and every research field at level " +
        std::to_string(view.threshold) + ". Completing it wins the game.";
    if (view.underWay) {
        text += "\nUnder way at " + systemLabel(state, view.systemId) + ": " + std::to_string(view.work) + "/" +
            std::to_string(view.industryCost) + " industry";
        text += view.turnsLeft >= 0 ? ", about " + std::to_string(view.turnsLeft) + " turns to go." : std::string(", stalled: no industry.");
    } else if (!view.startBlocker.empty()) {
        text += "\nCannot start here: " + view.startBlocker + ".";
    } else {
        text += "\nThis colony can start it now.";
    }
    return text;
}

// Rivals racing for the megaproject that the player can know about (their start is announced as galactic news).
inline std::vector<std::string> rivalMegaprojectLines(const GameState& state) {
    std::vector<std::string> lines;
    for (const auto& project : state.megaprojects) {
        if (project.ownerId == state.playerEmpire) continue;
        lines.push_back(std::string(empireName(project.ownerId)) + " is building the " + megaprojectName(state) + ": " +
            std::to_string(project.work) + "/" + std::to_string(megaprojectIndustryCost(state)) + " industry.");
    }
    return lines;
}

struct OutcomeView {
    bool over = false, victory = false;
    int winner = 0;
    std::string title, detail;
};

// The end-of-game overlay: who won and by which condition. MIRROR: the core records only outcome and winnerEmpire,
// so the condition is inferred from victoryProgress and the colony count (core query wanted: outcomeCondition(state)).
inline OutcomeView outcomeView(const GameState& state) {
    OutcomeView view;
    if (state.outcome == CampaignOutcome::Ongoing) return view;
    view.over = true;
    view.victory = state.outcome == CampaignOutcome::Victory;
    view.winner = state.winnerEmpire;
    const auto achieved = [&state](int empireId, VictoryCondition condition) {
        if (!campaignEmpire(state, empireId)) return false;
        for (const auto& progress : victoryProgress(state, empireId))
            if (progress.condition == condition) return progress.achieved;
        return false;
    };
    const int own = static_cast<int>(std::count_if(state.colonies.begin(), state.colonies.end(),
        [&state](const Colony& colony) { return colony.ownerId == state.playerEmpire; }));
    const std::string player = empireName(state.playerEmpire);
    if (view.victory) {
        view.title = "VICTORY";
        if (achieved(state.playerEmpire, VictoryCondition::Scientific))
            view.detail = "Scientific victory: the " + player + " completed the " + megaprojectName(state) + ".";
        else if (achieved(state.playerEmpire, VictoryCondition::Diplomatic))
            view.detail = "Diplomatic victory: the " + player + " alliance bloc holds two-thirds of the galaxy's population.";
        else if (own > 0 && own == static_cast<int>(state.colonies.size()))
            view.detail = "Conquest victory: every colony in the galaxy answers to the " + player + ".";
        else
            view.detail = "Domination victory: the " + player + " control 60% of the galaxy's systems.";
    } else {
        view.title = "DEFEAT";
        const int rival = state.winnerEmpire;
        if (own == 0)
            view.detail = "The " + player + " have fallen: no colony remains" +
                (rival ? ". The " + std::string(empireName(rival)) + " hold the galaxy." : std::string("."));
        else if (rival && achieved(rival, VictoryCondition::Scientific))
            view.detail = "The " + std::string(empireName(rival)) + " completed the " + megaprojectName(state) + ": scientific victory.";
        else if (rival && achieved(rival, VictoryCondition::Diplomatic))
            view.detail = "The " + std::string(empireName(rival)) + " united the galaxy through diplomacy: diplomatic victory.";
        else if (rival)
            view.detail = "The " + std::string(empireName(rival)) + " won the campaign.";
        else
            view.detail = "The campaign is lost.";
    }
    view.detail += " Turn " + std::to_string(state.turn) + ".";
    return view;
}

// ---------------------------------------------------------------------------------------------------------------
// Turn briefing (TurnReport::chronicle)
// ---------------------------------------------------------------------------------------------------------------

struct BriefingEntry {
    ChronicleCategory category = ChronicleCategory::Events;
    std::string categoryName, text;
    int focusCell = -1;   // where clicking the entry centres the chart, -1 when it has no place
    int systemId = 0, fleetId = 0, stationId = 0, eventId = 0;
};

// The chart cell an entry refers to: its sector, else its system, station or fleet (only an own or visible fleet).
inline int chronicleFocusCell(const GameState& state, const ChronicleEntry& entry) {
    if (entry.x >= 0 && entry.y >= 0) return cellOf(state, entry.x, entry.y);
    if (entry.systemId)
        if (const System* system = systemWithId(state, entry.systemId)) return cellOf(state, system->x, system->y);
    if (entry.stationId)
        if (const Station* station = stationById(state, entry.stationId))
            if (station->ownerId == state.playerEmpire || visible(state, state.playerEmpire, station->x, station->y))
                return cellOf(state, station->x, station->y);
    if (entry.fleetId)
        for (const auto& fleet : state.fleets)
            if (fleet.id == entry.fleetId && (fleet.ownerId == state.playerEmpire || visible(state, state.playerEmpire, fleet.x, fleet.y)))
                return cellOf(state, fleet.x, fleet.y);
    return -1;
}

// One entry per chronicle line, in report order, focused against the campaign after the turn.
inline std::vector<BriefingEntry> briefing(const GameState& state, const TurnReport& report) {
    std::vector<BriefingEntry> entries;
    for (const auto& entry : report.chronicle) {
        BriefingEntry out;
        out.category = entry.category;
        out.categoryName = chronicleCategoryName(entry.category);
        out.text = entry.text;
        out.focusCell = chronicleFocusCell(state, entry);
        out.systemId = entry.systemId; out.fleetId = entry.fleetId; out.stationId = entry.stationId; out.eventId = entry.eventId;
        entries.push_back(std::move(out));
    }
    return entries;
}

// "3 Construction, 1 Combat" in category order.
inline std::string briefingSummary(const std::vector<BriefingEntry>& entries) {
    std::string text;
    for (int category = 0; category <= static_cast<int>(ChronicleCategory::Victory); ++category) {
        const auto count = std::count_if(entries.begin(), entries.end(),
            [category](const BriefingEntry& entry) { return static_cast<int>(entry.category) == category; });
        if (!count) continue;
        if (!text.empty()) text += ", ";
        text += std::to_string(count) + " " + chronicleCategoryName(static_cast<ChronicleCategory>(category));
    }
    return text.empty() ? std::string("Nothing to report") : text;
}

// ---------------------------------------------------------------------------------------------------------------
// Build-queue templates
// ---------------------------------------------------------------------------------------------------------------

inline constexpr std::size_t TemplateNameLimit = 48, TemplateItemLimit = 32;

inline std::string templateItemName(const Construction& item) {
    const auto& ships = referenceData::ShipCatalogue;
    if (item.referenceId >= 0 && static_cast<std::size_t>(item.referenceId) < ships.size())
        return ships[static_cast<std::size_t>(item.referenceId)].name;
    return definition(item.product).name;
}

inline std::string templateSummary(const BuildTemplate& buildTemplate, std::size_t listed = 6) {
    std::string text = buildTemplate.name + (buildTemplate.autoApply ? "  [DEFAULT]" : "") + ": " +
        std::to_string(buildTemplate.items.size()) + (buildTemplate.items.size() == 1 ? " project" : " projects");
    for (std::size_t i = 0; i < buildTemplate.items.size() && i < listed; ++i)
        text += (i ? ", " : ": ") + templateItemName(buildTemplate.items[i]);
    if (buildTemplate.items.size() > listed) text += ", ...";
    return text;
}

// A colony's queue as template projects (product and class only), capped at the template size limit.
inline std::vector<Construction> templateItemsFromQueue(const Colony& colony) {
    std::vector<Construction> items;
    for (const auto& project : colony.queue) {
        if (items.size() >= TemplateItemLimit) break;
        items.emplace_back(project.product, 0, project.referenceId);
    }
    return items;
}

// A template name the core accepts: control characters dropped, surrounding spaces trimmed, cut to 48 bytes on a
// UTF-8 character boundary. Empty input falls back to Fallback. MIRROR of the DefineBuildTemplate name check in
// Rules.cpp (1-48 printable bytes; core query wanted: templateNameProblem(name)); core still validates the order.
inline std::string templateName(const std::string& wanted, const std::string& fallback) {
    std::string name;
    for (const char ch : wanted) {
        const auto byte = static_cast<unsigned char>(ch);
        if (byte >= 0x20 && byte != 0x7f) name.push_back(ch);
    }
    const auto first = name.find_first_not_of(' ');
    name = first == std::string::npos ? std::string() : name.substr(first, name.find_last_not_of(' ') - first + 1);
    if (name.empty()) name = fallback;
    if (name.size() > TemplateNameLimit) {
        std::size_t cut = TemplateNameLimit;
        while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xC0) == 0x80) --cut;
        name.resize(cut);
        const auto last = name.find_last_not_of(' ');
        name = last == std::string::npos ? fallback.substr(0, TemplateNameLimit) : name.substr(0, last + 1);
    }
    return name;
}

// ---------------------------------------------------------------------------------------------------------------
// Minor races (MinorRaces.h roster)
// ---------------------------------------------------------------------------------------------------------------

struct MinorProfile {
    bool known = false;            // on the roster (managed, or a legacy minor matched by name)
    bool managed = false;          // roster progression (tech, population) is live
    std::string name, homeworld, temperament, joinBonus, description;
    int attitude = 0, techLevel = 0, population = 0, memberOf = 0;
};

inline MinorProfile minorProfile(const GameState& state, const Minor& minor) {
    MinorProfile profile;
    profile.name = minor.name;
    profile.homeworld = systemLabel(state, minor.systemId);
    profile.memberOf = minor.memberOf;
    const int player = state.playerEmpire;
    profile.attitude = player >= 0 && player < static_cast<int>(minor.attitude.size()) ? minor.attitude[static_cast<std::size_t>(player)] : 0;
    profile.managed = minors::managed(minor);
    profile.techLevel = minor.techLevel;
    profile.population = minor.population;
    if (const minors::MinorRaceRecord* race = minors::raceFor(minor)) {
        profile.known = true;
        profile.homeworld = race->homeworld;
        profile.temperament = minors::temperamentName(race->temperament);
        profile.description = race->description;
        // Legacy minors keep the flat pre-roster dividend whatever their roster entry says. MIRROR of the legacy branch
        // of minors::memberBonuses (core query wanted: minors::joinBonusFor(minor)).
        profile.joinBonus = profile.managed ? minors::describeJoinBonus(*race) : std::string("+10% research (older save rules)");
    } else {
        profile.joinBonus = "+10% research";
    }
    return profile;
}

inline std::string minorProfileText(const MinorProfile& profile) {
    std::string text = profile.name + " of " + profile.homeworld;
    if (!profile.temperament.empty()) text += "  |  " + profile.temperament;
    text += "\nRelations with you " + std::to_string(profile.attitude) + " / 100  |  " +
        (profile.memberOf ? std::string("member of the ") + empireName(profile.memberOf) : std::string("independent"));
    if (profile.managed)
        text += "\nTechnology level " + std::to_string(profile.techLevel) + "  |  population " + std::to_string(profile.population);
    text += "\nJoin bonus: " + profile.joinBonus + " for every colony of the empire it joins.";
    if (!profile.description.empty()) text += "\n\n" + profile.description;
    return text;
}

// One-line chart / list summary: "Vulcans  |  Scientific  |  +15% research".
inline std::string minorShortText(const MinorProfile& profile) {
    std::string text = profile.name;
    if (!profile.temperament.empty()) text += "  |  " + profile.temperament;
    return text + "  |  " + profile.joinBonus;
}

// The empire's active member-race bonuses, one line each; empty when it has no members.
inline std::vector<std::string> memberBonusLines(const GameState& state, int empireId) {
    std::vector<std::string> lines;
    const minors::MemberBonuses bonus = minors::memberBonuses(state, empireId);
    if (!bonus.members) return lines;
    lines.push_back(std::to_string(bonus.members) + (bonus.members == 1 ? " member race" : " member races"));
    const auto add = [&lines](int value, const char* what, bool percent) {
        if (value) lines.push_back("+" + std::to_string(value) + (percent ? "% " : " ") + what);
    };
    add(bonus.researchPct, "research", true);
    add(bonus.industryPct, "industry", true);
    add(bonus.foodPct, "food", true);
    add(bonus.creditsPct, "credit income", true);
    add(bonus.morale, "morale target (at most +20)", false);
    add(bonus.groundDefensePct, "ground defense", true);
    for (const auto& minor : state.minors) {
        if (minor.memberOf != empireId) continue;
        lines.push_back("  " + minor.name + ": " + minorProfile(state, minor).joinBonus);
    }
    return lines;
}

} // namespace botf::view
