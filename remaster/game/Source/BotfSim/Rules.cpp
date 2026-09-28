// Schema-13 gameplay layer: ruleset presets, gameplay command plumbing, the
// GAMEPLAY_LAYER save section and the per-turn orchestration hooks that
// Simulation.cpp calls. See docs/GAMEPLAY-REMASTER.md.
#include "GameplayInternal.h"

#include <algorithm>
#include <iomanip>
#include <istream>
#include <locale>
#include <ostream>
#include <sstream>
#include <stdexcept>

namespace botf::gameplay {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool isGameplayCommand(CommandType type) {
    return type == CommandType::BuildOutpost || type == CommandType::UpgradeStation ||
        type == CommandType::ResolveEventChoice || type == CommandType::DefineBuildTemplate ||
        type == CommandType::DeleteBuildTemplate || type == CommandType::ApplyBuildTemplate ||
        type == CommandType::SetDefaultBuildTemplate || type == CommandType::StartMegaproject ||
        type == CommandType::CancelMegaproject;
}

namespace rules_detail {
bool printable(const std::string& text) {
    return std::none_of(text.begin(), text.end(), [](char ch) {
        const auto byte = static_cast<unsigned char>(ch);
        return byte < 0x20 || byte == 0x7f;
    });
}
} // namespace rules_detail

void checkUnrelatedFields(const Command& c) {
    require(c.stationId == 0 && c.eventId == 0 && c.eventOption == 0 && c.templateId == 0 &&
        c.templateName.empty() && c.templateItems.empty(), "Command contains gameplay-layer fields");
}

void checkCommand(const Command& c) {
    require(c.foodWorkers == 0 && c.industryWorkers == 0 && c.researchWorkers == 0 &&
        c.building == Building::Farm && c.targetX == 0 && c.targetY == 0 && c.queueIndex == 0 &&
        c.queueDestination == 0 && c.targetEmpire == 0 && c.targetFleetId == 0 && c.targetSystemId == 0 &&
        c.taskForceId == 0 && c.planetIndex == 0 && c.memberFleetIds.empty() && c.taskForceName.empty() &&
        c.stance == TacticalStance::Balanced && c.referenceId == -1,
        "Gameplay command contains unrelated fields");
    for (int amount : c.researchAllocation) require(amount == 0, "Gameplay command contains research fields");
    const bool usesFleet = c.type == CommandType::BuildOutpost;
    const bool usesSystem = c.type == CommandType::UpgradeStation || c.type == CommandType::ApplyBuildTemplate ||
        c.type == CommandType::StartMegaproject;
    const bool usesTemplateId = c.type == CommandType::DefineBuildTemplate || c.type == CommandType::DeleteBuildTemplate ||
        c.type == CommandType::ApplyBuildTemplate || c.type == CommandType::SetDefaultBuildTemplate;
    require(usesFleet ? c.fleetId > 0 : c.fleetId == 0, "Invalid gameplay command fleet field");
    require(usesSystem ? c.systemId > 0 : c.systemId == 0, "Invalid gameplay command system field");
    require(c.type == CommandType::UpgradeStation ? c.stationId > 0 : c.stationId == 0, "Invalid station field");
    if (c.type == CommandType::ResolveEventChoice)
        require(c.eventId > 0 && c.eventOption >= 0 && c.eventOption <= 2, "Invalid event choice fields");
    else
        require(c.eventId == 0 && c.eventOption == 0, "Invalid event fields");
    if (usesTemplateId) {
        const bool positive = c.type == CommandType::DeleteBuildTemplate || c.type == CommandType::ApplyBuildTemplate;
        require(c.templateId >= (positive ? 1 : 0) && c.templateId <= 10000000, "Invalid template identity");
    } else {
        require(c.templateId == 0, "Invalid template field");
    }
    if (c.type == CommandType::DefineBuildTemplate) {
        require(!c.templateName.empty() && c.templateName.size() <= 48 && rules_detail::printable(c.templateName),
            "Template name must be 1-48 printable characters");
        checkTemplateItems(c);
    } else {
        require(c.templateName.empty() && c.templateItems.empty(), "Invalid template contents field");
    }
}

void applyCommand(GameState& state, const Command& command, std::set<int>& orderedFleets) {
    switch (command.type) {
    case CommandType::BuildOutpost:
    case CommandType::UpgradeStation:
        applyStationCommand(state, command, orderedFleets); break;
    case CommandType::ResolveEventChoice:
        applyEventChoiceCommand(state, command); break;
    case CommandType::DefineBuildTemplate:
    case CommandType::DeleteBuildTemplate:
    case CommandType::ApplyBuildTemplate:
    case CommandType::SetDefaultBuildTemplate:
        applyTemplateCommand(state, command); break;
    case CommandType::StartMegaproject:
    case CommandType::CancelMegaproject:
        applyMegaprojectCommand(state, command); break;
    default:
        throw std::runtime_error("Not a gameplay-layer command");
    }
}

void writeCommandTail(std::ostream& out, const Command& c) {
    if (!isGameplayCommand(c.type)) return;
    out << ' ' << c.stationId << ' ' << c.eventId << ' ' << c.eventOption << ' ' << c.templateId
        << ' ' << std::quoted(c.templateName) << ' ' << c.templateItems.size();
    for (const auto& item : c.templateItems) out << ' ' << static_cast<int>(item.product) << ' ' << item.referenceId;
}

void readCommandTail(std::istream& in, Command& c) {
    std::size_t items = 0;
    require(static_cast<bool>(in >> c.stationId >> c.eventId >> c.eventOption >> c.templateId
        >> std::quoted(c.templateName) >> items) && items <= 32, "Malformed gameplay command");
    for (std::size_t i = 0; i < items; ++i) {
        int product = -1, ref = -1;
        require(static_cast<bool>(in >> product >> ref), "Truncated gameplay command template");
        c.templateItems.emplace_back(static_cast<Building>(product), 0, ref);
    }
}

bool layerActive(const GameState& state) {
    return !state.stations.empty() || !state.galaxyFeatures.empty();
}

void validate(const GameState& s) {
    const auto& r = s.rules;
    require(r.preset == RulesetPreset::Classic || r.preset == RulesetPreset::Remastered, "Invalid ruleset preset");
    if (r.preset == RulesetPreset::Remastered)
        require(r.stations && r.galaxyFeatures && r.curatedEvents && r.alternativeVictories,
            "Remastered ruleset enables every gameplay layer");
    else
        require(!r.galaxyFeatures && !r.curatedEvents && !r.alternativeVictories && !r.expandedRoster,
            "Classic ruleset has no anomalies, curated events, alternative victories or expanded roster");
    require(!r.stations || s.unifiedCombat, "Stations require the unified tactical combat model");
    validateFeatures(s);
    validateStations(s);
    validateEvents(s);
    validateTemplates(s);
    validateMegaprojects(s);
}

std::string serializeSection(const GameState& s) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    const auto& r = s.rules;
    out << "GAMEPLAY_LAYER " << static_cast<int>(r.preset) << ' ' << r.stations << ' ' << r.galaxyFeatures << ' '
        << r.curatedEvents << ' ' << r.alternativeVictories << '\n';
    out << "STATIONS " << s.nextStationId << ' ' << s.stations.size() << '\n';
    for (const auto& st : s.stations)
        out << st.id << ' ' << st.ownerId << ' ' << st.x << ' ' << st.y << ' ' << static_cast<int>(st.type) << ' '
            << st.hull << ' ' << st.shields << ' ' << st.builtTurn << '\n';
    int builders = 0;
    for (const auto& f : s.fleets) if (f.buildingOutpost) ++builders;
    out << "OUTPOST_BUILDS " << builders << '\n';
    for (const auto& f : s.fleets) if (f.buildingOutpost) out << f.id << ' ' << f.outpostProgress << '\n';
    out << "STATION_UPGRADES " << s.stationUpgrades.size() << '\n';
    for (const auto& u : s.stationUpgrades) out << u.stationId << ' ' << u.fundingSystemId << ' ' << u.work << '\n';
    out << "FEATURES " << s.nextFeatureId << ' ' << s.galaxyFeatures.size() << '\n';
    for (const auto& f : s.galaxyFeatures)
        out << f.id << ' ' << static_cast<int>(f.type) << ' ' << f.x << ' ' << f.y << ' ' << f.pairId << ' '
            << f.clusterId << ' ' << f.driftX << ' ' << f.driftY << ' ' << f.expiresTurn << ' ' << f.surveyedMask << '\n';
    out << "GALACTIC_EVENTS " << s.nextEventId << ' ' << s.lastCuratedEventTurn << ' ' << s.eventDeckPosition << ' '
        << s.eventDeck.size();
    for (int card : s.eventDeck) out << ' ' << card;
    out << ' ' << s.galacticEvents.size() << '\n';
    for (const auto& e : s.galacticEvents)
        out << e.id << ' ' << static_cast<int>(e.kind) << ' ' << e.empireId << ' ' << e.systemId << ' ' << e.x << ' '
            << e.y << ' ' << e.startTurn << ' ' << e.endTurn << ' ' << e.choiceDeadline << ' ' << e.choice << ' '
            << e.choiceApplied << ' ' << e.magnitude << ' ' << e.featureId << '\n';
    out << "TEMPLATES " << s.nextBuildTemplateId << ' ' << s.buildTemplates.size() << '\n';
    for (const auto& t : s.buildTemplates) {
        out << t.id << ' ' << t.ownerId << ' ' << t.autoApply << ' ' << std::quoted(t.name) << ' ' << t.items.size();
        for (const auto& item : t.items) out << ' ' << static_cast<int>(item.product) << ' ' << item.referenceId;
        out << '\n';
    }
    out << "MEGAPROJECTS " << s.megaprojects.size() << '\n';
    for (const auto& m : s.megaprojects) out << m.ownerId << ' ' << m.systemId << ' ' << m.work << ' ' << m.startTurn << '\n';
    out << "END_GAMEPLAY_LAYER\n";
    return out.str();
}

namespace rules_detail {
void tag(std::istream& in, const char* expected) {
    std::string actual;
    require(static_cast<bool>(in >> actual) && actual == expected, std::string("Malformed gameplay section: expected ") + expected);
}
int count(std::istream& in, int maximum) {
    int value = -1;
    require(static_cast<bool>(in >> value) && value >= 0 && value <= maximum, "Invalid gameplay collection size");
    return value;
}
template <typename... T> void read(std::istream& in, T&... values) {
    require(static_cast<bool>((in >> ... >> values)), "Truncated gameplay record");
}
} // namespace rules_detail

void parseSection(std::istream& in, GameState& s) {
    using rules_detail::count;
    using rules_detail::read;
    using rules_detail::tag;
    int preset = -1, stations = -1, features = -1, events = -1, victories = -1;
    read(in, preset, stations, features, events, victories);
    auto flag = [](int value) { require(value == 0 || value == 1, "Invalid gameplay rule flag"); return value == 1; };
    require(preset == 0 || preset == 1, "Invalid ruleset preset");
    s.rules.preset = static_cast<RulesetPreset>(preset);
    s.rules.stations = flag(stations);
    s.rules.galaxyFeatures = flag(features);
    s.rules.curatedEvents = flag(events);
    s.rules.alternativeVictories = flag(victories);

    tag(in, "STATIONS");
    read(in, s.nextStationId);
    int n = count(in, 256);
    for (int i = 0; i < n; ++i) {
        Station st; int type = -1;
        read(in, st.id, st.ownerId, st.x, st.y, type, st.hull, st.shields, st.builtTurn);
        require(type == 0 || type == 1, "Invalid station type");
        st.type = static_cast<StationType>(type);
        s.stations.push_back(st);
    }
    tag(in, "OUTPOST_BUILDS");
    n = count(in, 256);
    int priorFleet = 0;
    for (int i = 0; i < n; ++i) {
        int fleetId = 0, progress = -1;
        read(in, fleetId, progress);
        require(fleetId > priorFleet, "Outpost builds out of order");
        priorFleet = fleetId;
        const auto fleet = std::find_if(s.fleets.begin(), s.fleets.end(), [fleetId](const Fleet& f) { return f.id == fleetId; });
        require(fleet != s.fleets.end(), "Outpost build for unknown fleet");
        fleet->buildingOutpost = true;
        fleet->outpostProgress = progress;
    }
    tag(in, "STATION_UPGRADES");
    n = count(in, 256);
    for (int i = 0; i < n; ++i) {
        StationUpgrade u;
        read(in, u.stationId, u.fundingSystemId, u.work);
        s.stationUpgrades.push_back(u);
    }
    tag(in, "FEATURES");
    read(in, s.nextFeatureId);
    n = count(in, 256);
    for (int i = 0; i < n; ++i) {
        GalaxyFeature f; int type = -1;
        read(in, f.id, type, f.x, f.y, f.pairId, f.clusterId, f.driftX, f.driftY, f.expiresTurn, f.surveyedMask);
        require(type >= 0 && type <= 5, "Invalid galaxy feature type");
        f.type = static_cast<GalaxyFeatureType>(type);
        s.galaxyFeatures.push_back(f);
    }
    tag(in, "GALACTIC_EVENTS");
    read(in, s.nextEventId, s.lastCuratedEventTurn, s.eventDeckPosition);
    n = count(in, 64);
    for (int i = 0; i < n; ++i) { int card = -1; read(in, card); s.eventDeck.push_back(card); }
    n = count(in, 64);
    for (int i = 0; i < n; ++i) {
        GalacticEvent e; int kind = -1, applied = -1;
        read(in, e.id, kind, e.empireId, e.systemId, e.x, e.y, e.startTurn, e.endTurn, e.choiceDeadline, e.choice,
            applied, e.magnitude, e.featureId);
        require(kind >= 0 && kind <= 6 && (applied == 0 || applied == 1), "Invalid galactic event record");
        e.kind = static_cast<GalacticEventKind>(kind);
        e.choiceApplied = applied == 1;
        s.galacticEvents.push_back(e);
    }
    tag(in, "TEMPLATES");
    read(in, s.nextBuildTemplateId);
    n = count(in, 128);
    for (int i = 0; i < n; ++i) {
        BuildTemplate t; int autoApply = -1;
        read(in, t.id, t.ownerId, autoApply);
        require(autoApply == 0 || autoApply == 1, "Invalid template default flag");
        t.autoApply = autoApply == 1;
        require(static_cast<bool>(in >> std::quoted(t.name)), "Malformed template name");
        const int items = count(in, 32);
        for (int k = 0; k < items; ++k) {
            int product = -1, ref = -1;
            read(in, product, ref);
            t.items.emplace_back(static_cast<Building>(product), 0, ref);
        }
        s.buildTemplates.push_back(std::move(t));
    }
    tag(in, "MEGAPROJECTS");
    n = count(in, 5);
    for (int i = 0; i < n; ++i) {
        Megaproject m;
        read(in, m.ownerId, m.systemId, m.work, m.startTurn);
        s.megaprojects.push_back(m);
    }
    tag(in, "END_GAMEPLAY_LAYER");
}

void stripSection(std::string& payload) {
    const auto start = payload.find("\nGAMEPLAY_LAYER ");
    require(start != std::string::npos, "Canonical payload lacks the gameplay layer");
    const std::string endTag = "\nEND_GAMEPLAY_LAYER\n";
    const auto end = payload.find(endTag, start);
    require(end != std::string::npos, "Canonical payload gameplay layer is unterminated");
    payload.erase(start + 1, end + endTag.size() - (start + 1));
}

// ---- Turn orchestration ----
void planTurn(GameState& next, TurnReport& report) {
    if (next.rules.stations) {
        planRivalStations(next);
        cancelInterruptedOutposts(next, report); // Before movement: new orders end assembly.
    }
    if (next.rules.galaxyFeatures) planAnomalySurveys(next);
    if (next.rules.alternativeVictories) planRivalMegaprojects(next, report);
}

void beforeCombat(GameState& moving, TurnReport& report, TurnEffects& effects) {
    if (moving.rules.stations) advanceOutpostConstruction(moving, report);
    if (moving.rules.galaxyFeatures) {
        surveyAnomalies(moving, report, effects);
        applySectorHazards(moving, report);
    }
    if (!moving.stations.empty()) resolveStationCaptures(moving, report); // Boarding on arrival.
}

void afterCombat(GameState& moving, TurnReport& report) {
    if (!moving.rules.stations) return;
    std::set<int> inBattle;
    for (const auto& battle : report.battles)
        for (const auto& p : battle.participants)
            if (isStationCombatant(p.fleetId)) inBattle.insert(stationIdForCombatant(p.fleetId));
    stationsAfterCombat(moving, report, inBattle);
}

void carryOver(GameState& next, GameState& moving, const TurnEffects& effects, TurnReport& report) {
    // Movement/combat worked on a copy of the pre-production state; stations and
    // feature survey marks come from it, production-phase state stays in next.
    next.stations = std::move(moving.stations);
    next.nextStationId = moving.nextStationId;
    next.galaxyFeatures = std::move(moving.galaxyFeatures);
    next.nextFeatureId = moving.nextFeatureId;
    std::erase_if(next.stationUpgrades, [&](const StationUpgrade& u) {
        const auto station = stationById(next, u.stationId);
        const auto colony = colonyAt(next, u.fundingSystemId);
        return !station || station->type != StationType::Outpost || !colony || colony->ownerId != station->ownerId;
    });
    resolveMegaprojectLosses(next, report);
    for (const auto& d : effects.deltas) {
        auto& e = internal::mutableEmpire(next, d.empireId);
        e.credits = std::clamp<std::int64_t>(e.credits + d.credits, -internal::BalanceLimit, internal::BalanceLimit);
        e.intelligence = std::min(internal::BalanceLimit, e.intelligence + d.intelligence);
        if (d.field >= 0 && d.research > 0) grantResearch(next, d.empireId, d.field, d.research, &report);
    }
}

void finishTurn(const GameState& before, GameState& next, TurnReport& report) {
    if (next.rules.stations) completeStationUpgrades(next, report);
    if (next.rules.galaxyFeatures) {
        expireFeatures(next, report);
        driftIonStorms(next);
    }
    autoApplyTemplates(before, next, report);
    buildChronicle(next, report);
}
} // namespace botf::gameplay

namespace botf {
GameplayRules rulesForPreset(RulesetPreset preset) {
    GameplayRules r;
    r.preset = preset;
    r.stations = true; // Outposts and starbases are an original BOTF feature.
    const bool remastered = preset == RulesetPreset::Remastered;
    r.galaxyFeatures = remastered;
    r.curatedEvents = remastered;
    r.alternativeVictories = remastered;
    r.expandedRoster = remastered;
    return r;
}

const char* rulesetPresetName(RulesetPreset preset) {
    switch (preset) {
    case RulesetPreset::Classic: return "Classic";
    case RulesetPreset::Remastered: return "Remastered";
    default: return "Unknown";
    }
}

bool parseRulesetPreset(const std::string& text, RulesetPreset& preset) {
    std::string lower;
    for (char c : text) lower.push_back(static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c));
    if (lower == "classic") { preset = RulesetPreset::Classic; return true; }
    if (lower == "remastered" || lower == "remaster") { preset = RulesetPreset::Remastered; return true; }
    return false;
}

void applyRulesetPreset(GameState& state, RulesetPreset preset) {
    gameplay::require(state.turn == 0 && state.pending.empty(), "Ruleset presets apply to a new campaign");
    gameplay::require(preset == RulesetPreset::Classic || preset == RulesetPreset::Remastered, "Unknown ruleset preset");
    auto next = state;
    next.rules = rulesForPreset(preset);
    next.galaxyFeatures.clear(); next.nextFeatureId = 1;
    next.galacticEvents.clear(); next.nextEventId = 1;
    next.eventDeck.clear(); next.eventDeckPosition = 0; next.lastCuratedEventTurn = 0;
    if (next.rules.galaxyFeatures) gameplay::generateGalaxyFeatures(next);
    if (next.rules.curatedEvents) gameplay::initializeEventDeck(next);
    validate(next);
    state = std::move(next);
}

GameState newCampaign(const CampaignSetup& setup) {
    auto state = setup.reference
        ? generateReferenceCampaign(setup.seed, setup.empires, setup.width, setup.height, setup.systems, setup.playerEmpire, setup.era)
        : generateCampaign(setup.seed, setup.empires, setup.width, setup.height, setup.systems, setup.playerEmpire, setup.era);
    placeMinorRaceRoster(state); // Lane G: minor-race roster (MinorRaces.h)
    applyRulesetPreset(state, setup.preset);
    return state;
}
} // namespace botf
