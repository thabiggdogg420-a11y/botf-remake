// Long AI-only soak: many seeds x eras x player empires, each advanced for 150+
// turns. Every turn must advance without throwing and leave a state that
// validates, previews and round-trips through a save. A sample of campaigns also
// runs the campaign-session path in lockstep with headless advance() to prove
// both resolve battles identically.
//
// Schema 13: Remastered-ruleset campaigns (anomalies, curated events, stations,
// alternative victories) run 220+ turns with a scripted player that builds and
// upgrades outposts, answers events, applies build templates and starts the
// megaproject; Classic-preset campaigns exercise stations alone.
//
// BOTF_SOAK_SCALE=N (environment) multiplies the seed count and adds turns for
// longer local soaks; the default keeps CI time modest.
#include "CampaignSession.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct SoakConfig {
    std::uint64_t seed = 0;
    botf::StartingEra era = botf::StartingEra::Early;
    int empires = 5;
    int player = 1;
    bool reference = true;
    int turns = 150;
    bool lockstepSession = false;
    bool gameplayLayer = false; // newCampaign() with `preset` and a scripted player.
    botf::RulesetPreset preset = botf::RulesetPreset::Remastered;
};

std::string describe(const SoakConfig& c) {
    std::ostringstream out;
    out << (c.reference ? "reference" : "prototype") << " seed " << c.seed << " era " << botf::eraName(c.era)
        << " empires " << c.empires << " player " << c.player;
    if (c.gameplayLayer) out << " ruleset " << botf::rulesetPresetName(c.preset);
    return out.str();
}

std::uint64_t mix(std::uint64_t& state) {
    state += 0x9e3779b97f4a7c15ULL;
    auto z = state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

// The player stays AI-free apart from colony governors, which exercises the
// workforce/queue automation (including the governor paths that used to throw).
void assignGovernors(botf::GameState& state, std::uint64_t& rng) {
    // submit() replaces the state, so collect targets before issuing commands.
    std::vector<botf::Colony> owned;
    for (const auto& colony : state.colonies) if (colony.ownerId == state.playerEmpire) owned.push_back(colony);
    for (const auto& colony : owned) {
        const auto workforce = static_cast<botf::GovernorPriority>(1 + mix(rng) % 4);
        const auto queue = static_cast<botf::GovernorPriority>(1 + mix(rng) % 4);
        if (colony.workforcePriority == workforce && colony.queuePriority == queue) continue;
        try {
            botf::submit(state, botf::makeSetColonyGovernorCommand(state.lastAcceptedCommand + 1,
                colony.systemId, workforce, queue));
        } catch (const std::exception& error) {
            throw std::runtime_error(std::string("governor command rejected: ") + error.what());
        }
    }
}

void submitOrThrow(botf::GameState& state, botf::Command command, const char* what) {
    command.id = state.lastAcceptedCommand + 1;
    try {
        botf::submit(state, command);
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string(what) + " rejected although its blocker was clear: " + error.what());
    }
}

// Scripted player for the schema-13 layer. Every order is checked against the
// public blocker queries first, so a rejection is a real inconsistency.
void scriptGameplay(botf::GameState& state, std::uint64_t& rng) {
    const int player = state.playerEmpire;
    std::vector<botf::Colony> owned;
    for (const auto& colony : state.colonies) if (colony.ownerId == player) owned.push_back(colony);
    if (owned.empty()) return;
    // Build templates: define once from legal infrastructure, make it the default, apply now and then.
    if (botf::empireBuildTemplates(state, player).empty()) {
        std::vector<botf::Construction> items;
        for (const auto& choice : botf::constructionChoices(state, owned.front().systemId))
            if (!botf::isShip(choice.product) && choice.product != botf::Building::Shipyard && items.size() < 3)
                items.emplace_back(choice.product, 0, choice.referenceId);
        if (!items.empty()) {
            submitOrThrow(state, botf::makeDefineBuildTemplateCommand(0, 0, "Soak core", items), "DefineBuildTemplate");
            return; // The template exists from the next turn.
        }
    } else {
        const auto templates = botf::empireBuildTemplates(state, player);
        if (!templates.front().autoApply)
            submitOrThrow(state, botf::makeSetDefaultBuildTemplateCommand(0, templates.front().id), "SetDefaultBuildTemplate");
        if (state.turn % 15 == 7) {
            const auto& target = owned[static_cast<std::size_t>(mix(rng) % owned.size())];
            if (target.queue.size() < 4)
                submitOrThrow(state, botf::makeApplyBuildTemplateCommand(0, templates.front().id, target.systemId), "ApplyBuildTemplate");
        }
    }
    // Events: answer pending decisions (sometimes let the default apply).
    for (const auto& event : botf::empireEvents(state, player)) {
        if (event.empireId != player || event.choiceDeadline == 0 || event.choice >= 0 || mix(rng) % 4 == 0) continue;
        const auto options = botf::eventChoices(event.kind);
        submitOrThrow(state, botf::makeResolveEventChoiceCommand(0, event.id,
            static_cast<int>(mix(rng) % options.size())), "ResolveEventChoice");
    }
    // Outposts: idle transports build where they stand, or move to a nearby open sector.
    std::vector<botf::Fleet> transports;
    for (const auto& f : state.fleets)
        if (f.ownerId == player && f.shipClass == botf::Building::Transport && !f.buildingOutpost && f.taskForceId == 0 &&
            f.x == f.targetX && f.y == f.targetY && !f.retreat && !f.invadeSystemId) transports.push_back(f);
    for (const auto& f : transports) {
        if (botf::outpostBlocker(state, f.id).empty()) {
            submitOrThrow(state, botf::makeBuildOutpostCommand(0, f.id), "BuildOutpost");
            continue;
        }
        const int radius = 2 + static_cast<int>(mix(rng) % 2);
        for (int dy = -radius; dy <= radius; ++dy) for (int dx = -radius; dx <= radius; ++dx) {
            const int x = f.x + dx, y = f.y + dy;
            if (std::max(std::abs(dx), std::abs(dy)) != radius || botf::stationAt(state, x, y)) continue;
            if (botf::route(state, f, x, y).empty()) continue;
            botf::Command move; move.type = botf::CommandType::MoveFleet; move.fleetId = f.id; move.targetX = x; move.targetY = y;
            submitOrThrow(state, move, "MoveFleet");
            dx = dy = radius + 1;
        }
    }
    if (transports.empty() && state.turn % 25 == 3) {
        const auto home = owned.front();
        if (home.queue.size() < 3)
            for (const auto& choice : botf::constructionChoices(state, home.systemId))
                if (choice.product == botf::Building::Transport && choice.blocker.empty()) {
                    botf::Command build; build.type = botf::CommandType::EnqueueBuilding; build.systemId = home.systemId;
                    build.building = choice.product; build.referenceId = choice.referenceId;
                    submitOrThrow(state, build, "EnqueueBuilding transport");
                    break;
                }
    }
    // Starbase upgrades and the megaproject when affordable.
    for (const auto& station : state.stations) {
        if (station.ownerId != player || station.type != botf::StationType::Outpost || botf::stationUpgradeFor(state, station.id)) continue;
        for (const auto& colony : owned)
            if (botf::stationUpgradeBlocker(state, station.id, colony.systemId).empty()) {
                submitOrThrow(state, botf::makeUpgradeStationCommand(0, station.id, colony.systemId), "UpgradeStation");
                return; // Credits changed; reconsider next turn.
            }
    }
    if (!botf::empireMegaproject(state, player))
        for (const auto& colony : owned)
            if (botf::megaprojectBlocker(state, player, colony.systemId).empty()) {
                submitOrThrow(state, botf::makeStartMegaprojectCommand(0, colony.systemId), "StartMegaproject");
                break;
            }
}

// Headless advance() and the session path (auto-resolving every player battle)
// must produce the same committed campaign.
void sessionTurn(botf::GameState& state) {
    botf::session::TurnSession session;
    botf::session::begin(session, state);
    for (int guard = 0; guard < 4096; ++guard) {
        if (session.pending && !session.pending->isFinished) botf::session::autoResolve(session);
        if (botf::session::resume(session, state)) return;
    }
    throw std::runtime_error("session turn did not complete");
}

bool governorsEnabled() {
    const char* value = std::getenv("BOTF_SOAK_GOVERNORS"); // "0" isolates pure AI play when bisecting.
    return !value || std::string(value) != "0";
}

struct SoakStats {
    int turns = 0, stationsBuilt = 0, stationBattles = 0, starbases = 0, surveys = 0, events = 0;
    bool alternativeVictory = false;
};

SoakStats soak(const SoakConfig& config) {
    SoakStats stats;
    botf::CampaignSetup setup;
    setup.seed = config.seed; setup.empires = config.empires; setup.playerEmpire = config.player;
    setup.era = config.era; setup.reference = config.reference; setup.preset = config.preset;
    auto state = config.gameplayLayer ? botf::newCampaign(setup) : config.reference
        ? botf::generateReferenceCampaign(config.seed, config.empires, 16, 12, 28, config.player, config.era)
        : botf::generateCampaign(config.seed, config.empires, 16, 12, 28, config.player, config.era);
    auto mirror = state;
    std::uint64_t rng = config.seed ^ 0x51ab5eedULL;
    for (int turn = 0; turn < config.turns && state.outcome == botf::CampaignOutcome::Ongoing; ++turn) {
        const auto where = [&](const char* phase) {
            return describe(config) + " turn " + std::to_string(state.turn) + " (" + phase + ")";
        };
        try {
            if (turn % 20 == 0 && governorsEnabled()) {
                assignGovernors(state, rng);
                if (config.lockstepSession) mirror = state;
            }
            if (config.gameplayLayer) {
                scriptGameplay(state, rng);
                if (config.lockstepSession) mirror = state;
            }
            (void)botf::preview(state);
            const int stationsBefore = state.nextStationId, eventsBefore = state.nextEventId;
            int surveyBits = 0;
            for (const auto& f : state.galaxyFeatures) for (int e = 1; e <= 5; ++e) surveyBits += (f.surveyedMask >> e) & 1;
            const auto report = botf::advance(state);
            botf::validate(state);
            if (report.chronicle.size() != report.events.size()) throw std::runtime_error("chronicle does not cover the events");
            stats.turns = state.turn;
            stats.stationsBuilt += state.nextStationId - stationsBefore;
            stats.events += state.nextEventId - eventsBefore;
            for (const auto& f : state.galaxyFeatures) for (int e = 1; e <= 5; ++e) surveyBits -= (f.surveyedMask >> e) & 1;
            stats.surveys -= surveyBits;
            for (const auto& b : report.battles)
                if (std::any_of(b.participants.begin(), b.participants.end(), [](const botf::CombatParticipant& p) {
                    return botf::isStationCombatant(p.fleetId); })) ++stats.stationBattles;
            if (state.outcome != botf::CampaignOutcome::Ongoing && !state.megaprojects.empty()) stats.alternativeVictory = true;
        } catch (const std::exception& error) {
            throw std::runtime_error(where("advance") + ": " + error.what());
        }
        if (config.lockstepSession) {
            try {
                if (turn % 20 == 0) mirror = botf::deserialize(botf::serialize(mirror));
                sessionTurn(mirror);
            } catch (const std::exception& error) {
                throw std::runtime_error(where("session") + ": " + error.what());
            }
            if (botf::fingerprint(mirror) != botf::fingerprint(state))
                throw std::runtime_error(where("session") + ": session and headless advance diverged");
        }
        if (turn % 10 == 9 || state.outcome != botf::CampaignOutcome::Ongoing) {
            try {
                const auto text = botf::serialize(state);
                auto reloaded = botf::deserialize(text);
                if (botf::serialize(reloaded) != text) throw std::runtime_error("save did not round-trip exactly");
                if (turn % 30 == 29 && state.outcome == botf::CampaignOutcome::Ongoing) {
                    // A reloaded campaign must continue identically.
                    auto live = state;
                    botf::advance(live);
                    botf::advance(reloaded);
                    if (botf::fingerprint(live) != botf::fingerprint(reloaded))
                        throw std::runtime_error("reloaded campaign diverged");
                }
            } catch (const std::exception& error) {
                throw std::runtime_error(where("save") + ": " + error.what());
            }
        }
    }
    for (const auto& st : state.stations) if (st.type == botf::StationType::Starbase) ++stats.starbases;
    return stats;
}
}

int main() {
    int scale = 1;
    if (const char* value = std::getenv("BOTF_SOAK_SCALE")) scale = std::max(1, std::atoi(value));
    std::vector<std::uint64_t> seeds{26713, 1999, 4242, 90210};
    for (int i = 1; i < scale; ++i) for (std::uint64_t base : {26713ULL, 1999ULL, 4242ULL, 90210ULL}) seeds.push_back(base * 31 + static_cast<std::uint64_t>(i));
    std::vector<SoakConfig> configs;
    for (const auto era : botf::availableStartingEras()) {
        for (int player = 1; player <= 5; ++player) {
            for (std::size_t i = 0; i < seeds.size(); ++i) {
                SoakConfig c;
                c.seed = seeds[i] + static_cast<std::uint64_t>(player);
                c.era = era;
                c.player = player;
                c.turns = 150 + 50 * (scale - 1);
                c.lockstepSession = i == 0 && player % 2 == 1;
                configs.push_back(c);
            }
        }
        // Prototype-rule campaigns with fewer empires use the same unified combat.
        for (int empires = 2; empires <= 4; ++empires) {
            SoakConfig c;
            c.seed = 7000 + static_cast<std::uint64_t>(empires);
            c.era = era;
            c.empires = empires;
            c.player = empires;
            c.reference = false;
            c.lockstepSession = empires == 3;
            configs.push_back(c);
        }
    }
    // Schema-13 gameplay layer: Remastered campaigns across every era and player,
    // plus Classic-preset campaigns with stations only.
    const std::vector<std::uint64_t> layerSeeds{31337, 8086};
    for (const auto era : botf::availableStartingEras()) {
        for (int player = 1; player <= 5; ++player) {
            for (std::size_t i = 0; i < layerSeeds.size() * static_cast<std::size_t>(scale); ++i) {
                SoakConfig c;
                c.seed = layerSeeds[i % layerSeeds.size()] + 97 * static_cast<std::uint64_t>(i / layerSeeds.size()) +
                    static_cast<std::uint64_t>(player);
                c.era = era;
                c.player = player;
                c.turns = 220 + 50 * (scale - 1);
                c.gameplayLayer = true;
                c.reference = (i + static_cast<std::size_t>(player)) % 3 != 0;
                c.lockstepSession = i == 0 && player % 2 == 0;
                configs.push_back(c);
            }
        }
        SoakConfig classic;
        classic.seed = 5150 + static_cast<std::uint64_t>(era);
        classic.era = era;
        classic.player = 3;
        classic.turns = 200;
        classic.gameplayLayer = true;
        classic.preset = botf::RulesetPreset::Classic;
        classic.lockstepSession = true;
        configs.push_back(classic);
    }
    int passed = 0, failed = 0;
    int layerTurns = 0, layerCampaigns = 0, alternativeVictories = 0, shortCampaigns = 0;
    SoakStats layerStats;
    for (const auto& config : configs) {
        try {
            const auto stats = soak(config);
            if (config.gameplayLayer) {
                ++layerCampaigns;
                layerTurns += stats.turns;
                layerStats.stationsBuilt += stats.stationsBuilt;
                layerStats.stationBattles += stats.stationBattles;
                layerStats.starbases += stats.starbases;
                layerStats.surveys += stats.surveys;
                layerStats.events += stats.events;
                if (stats.alternativeVictory) ++alternativeVictories;
                if (stats.turns < 200) ++shortCampaigns;
            }
            ++passed;
            std::cout << "PASS soak " << describe(config) << std::endl;
        } catch (const std::exception& error) {
            ++failed;
            std::cout << "FAIL soak " << error.what() << std::endl;
        }
    }
    std::cout << "Gameplay layer: " << layerCampaigns << " campaigns, " << layerTurns << " turns (" << shortCampaigns
        << " ended before turn 200, " << alternativeVictories << " by scientific victory), stations built " << layerStats.stationsBuilt
        << ", station battles " << layerStats.stationBattles << ", starbases standing " << layerStats.starbases
        << ", anomaly surveys " << layerStats.surveys << ", curated events " << layerStats.events << std::endl;
    std::cout << (failed ? "FAILED: " : "ALL PASSED: ") << passed << " passed, " << failed << " failed soak campaigns" << std::endl;
    return failed ? 1 : 0;
}
