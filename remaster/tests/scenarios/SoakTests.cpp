// Long AI-only soak: many seeds x eras x player empires, each advanced for 150+
// turns. Every turn must advance without throwing and leave a state that
// validates, previews and round-trips through a save. A sample of campaigns also
// runs the campaign-session path in lockstep with headless advance() to prove
// both resolve battles identically.
//
// BOTF_SOAK_SCALE=N (environment) multiplies the seed count and adds turns for
// longer local soaks; the default keeps CI time modest.
#include "CampaignSession.h"

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
};

std::string describe(const SoakConfig& c) {
    std::ostringstream out;
    out << (c.reference ? "reference" : "prototype") << " seed " << c.seed << " era " << botf::eraName(c.era)
        << " empires " << c.empires << " player " << c.player;
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

void soak(const SoakConfig& config) {
    auto state = config.reference
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
            (void)botf::preview(state);
            botf::advance(state);
            botf::validate(state);
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
    int passed = 0, failed = 0;
    for (const auto& config : configs) {
        try {
            soak(config);
            ++passed;
            std::cout << "PASS soak " << describe(config) << std::endl;
        } catch (const std::exception& error) {
            ++failed;
            std::cout << "FAIL soak " << error.what() << std::endl;
        }
    }
    std::cout << (failed ? "FAILED: " : "ALL PASSED: ") << passed << " passed, " << failed << " failed soak campaigns" << std::endl;
    return failed ? 1 : 0;
}
