#include "Simulation.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
std::uint64_t number(const std::string& text) {
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        throw std::runtime_error("Expected a nonnegative integer: " + text);
    return value;
}
botf::StartingEra parseEra(const std::string& text) {
    std::string lower;
    for (char c : text) lower.push_back(static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c));
    for (const auto era : botf::availableStartingEras()) {
        std::string name = botf::eraName(era);
        for (auto& c : name) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        if (lower == name || lower == std::to_string(static_cast<int>(era))) return era;
    }
    throw std::runtime_error("Unknown era: " + text + " (early, developed, expanded, advanced, supreme)");
}
std::string readSave(const std::filesystem::path& path) {
    if (std::filesystem::file_size(path) > 1024 * 1024)
        throw std::runtime_error("Save exceeds prototype size limit");
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open save");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void saveNew(const std::filesystem::path& path, const botf::GameState& state) {
    // Never overwrite an existing user save. A failed write is left recoverable.
    auto temporary = path;
    temporary += ".pending-write";
    if (std::filesystem::exists(path) || std::filesystem::exists(temporary))
        throw std::runtime_error("Save or pending-write already exists; choose a new filename");
    const auto text = botf::serialize(state);
    std::ofstream file(temporary, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot create save; parent directory must exist");
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    file.flush();
    if (!file) throw std::runtime_error("Save write failed; previous saves unchanged");
    file.close();
    if (!file) throw std::runtime_error("Save close failed");
    if (botf::serialize(botf::deserialize(readSave(temporary))) != text)
        throw std::runtime_error("Save verification failed");
    std::filesystem::rename(temporary, path);
}
}

int main(int argc, char** argv) {
    try {
        std::uint64_t seed = 1999;
        int turns = 10;
        int empires = 0;
        int playerEmpire = 1;
        bool seedSet = false;
        bool reference = false;
        bool eraSet = false;
        botf::StartingEra era = botf::StartingEra::Early;
        std::string loadPath, savePath, battleFixturePath;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help") {
                std::cout << "BotfSim [--seed N | --load FILE] [--empires 2..5] [--player 1..5] [--reference]\n"
                    << "        [--era early|developed|expanded|advanced|supreme] [--turns N] [--save NEW_FILE]\n"
                    << "--era selects the starting era for a new --empires campaign (default early; also 1..5).\n"
                    << "New campaigns use save schema " << botf::SaveVersion << " (" << botf::ContentVersion
                    << "): every battle resolves through the tactical engine. Older saves load and continue.\n"
                    << "BotfSim --battle-fixture NEW_FILE writes a campaign after one structured battle.\n"
                    << "Developer simulation runner; the Unreal game uses this same core. Balance is provisional.\n"
                    << "New games queue Factory, Farm and Laboratory. Saves retain pending orders.\n";
                return 0;
            }
            if (option == "--reference") { reference = true; continue; }
            if (i + 1 >= argc) throw std::runtime_error("Missing option value");
            const std::string value = argv[++i];
            if (option == "--seed") { seed = number(value); seedSet = true; }
            else if (option == "--turns") {
                const auto n = number(value);
                if (n > 10000) throw std::runtime_error("At most 10000 turns per invocation");
                turns = static_cast<int>(n);
            } else if (option == "--empires") {
                const auto n = number(value);
                if (n < 2 || n > 5) throw std::runtime_error("Campaign requires two to five empires");
                empires = static_cast<int>(n);
            } else if (option == "--player") {
                const auto n = number(value);
                if (n < 1 || n > 5) throw std::runtime_error("Player empire must be one through five");
                playerEmpire = static_cast<int>(n);
            } else if (option == "--era") { era = parseEra(value); eraSet = true; }
            else if (option == "--load") loadPath = value;
            else if (option == "--save") savePath = value;
            else if (option == "--battle-fixture") battleFixturePath = value;
            else throw std::runtime_error("Unknown option: " + option);
        }
        if (!battleFixturePath.empty()) {
            if (seedSet || empires || !loadPath.empty() || !savePath.empty() || reference || playerEmpire != 1 || eraSet)
                throw std::runtime_error("Battle fixture option cannot be combined with a campaign or save option");
            auto fixture = botf::generateReferenceCampaign(1999);
            fixture.eventsEnabled = false; fixture.aiAggressionEnabled = false;
            const auto& sol = fixture.systems.front();
            auto& rivalScout = fixture.fleets[2];
            rivalScout.x = rivalScout.targetX = sol.x; rivalScout.y = rivalScout.targetY = sol.y;
            auto& surveyed = fixture.rivals[0].explored;
            surveyed.push_back(sol.y * fixture.width + sol.x);
            std::sort(surveyed.begin(), surveyed.end()); surveyed.erase(std::unique(surveyed.begin(), surveyed.end()), surveyed.end());
            fixture.majorRelations[0].contact = true;
            botf::Command war; war.id = 1; war.type = botf::CommandType::DeclareWar; war.targetEmpire = 2;
            botf::submit(fixture, war);
            const auto battle = botf::advance(fixture);
            if (battle.battles.empty()) throw std::runtime_error("Battle fixture failed to record an encounter");
            saveNew(battleFixturePath, fixture);
            std::cout << "Battle fixture saved: " << battleFixturePath << " | battles " << battle.battles.size()
                << " | rounds " << battle.battles.front().rounds.size()
                << " | hash " << battle.stateHash << '\n';
            return 0;
        }
        if (seedSet && !loadPath.empty()) throw std::runtime_error("Choose --seed or --load, not both");
        if ((empires || reference || playerEmpire != 1 || eraSet) && !loadPath.empty())
            throw std::runtime_error("Cannot change campaign setup while loading a save");
        if (playerEmpire != 1 && !empires) throw std::runtime_error("Choose --empires with --player");
        if (eraSet && !empires) throw std::runtime_error("Choose --empires with --era");
        if (empires && playerEmpire > empires) throw std::runtime_error("Player empire exceeds campaign empire count");
        auto state = loadPath.empty() ? (empires ? (reference ? botf::generateReferenceCampaign(seed, empires, 16, 12, 28, playerEmpire, era)
            : botf::generateCampaign(seed, empires, 16, 12, 28, playerEmpire, era)) : botf::generate(seed)) : botf::deserialize(readSave(loadPath));
        if (loadPath.empty()) {
            std::uint64_t id = 0;
            const auto home = std::find_if(state.colonies.begin(), state.colonies.end(), [&](const botf::Colony& colony) {
                return colony.ownerId == state.playerEmpire;
            });
            if (home == state.colonies.end()) throw std::runtime_error("Player has no starting colony");
            const int homeSystemId = home->systemId; // submit replaces state; iterators cannot span submissions.
            for (auto building : {botf::Building::Factory, botf::Building::Farm, botf::Building::Laboratory}) {
                botf::Command command;
                command.id = ++id; command.type = botf::CommandType::EnqueueBuilding;
                command.systemId = homeSystemId; command.building = building;
                botf::submit(state, command);
            }
        }
        std::cout << "BOTF standalone simulation | " << botf::ContentVersion << '\n'
            << "Seed " << state.seed << ", " << state.width << 'x' << state.height
            << ", " << state.systems.size() << " systems, turn " << state.turn << '\n';
        for (const auto& system : state.systems)
            std::cout << "  " << system.id << ' ' << system.name << " (" << system.x << ',' << system.y << ")\n";
        for (int i = 0; i < turns && state.outcome == botf::CampaignOutcome::Ongoing; ++i)
            std::cout << botf::describe(botf::advance(state));
        std::cout << "FINAL_HASH " << botf::fingerprint(state) << '\n';
        if (!savePath.empty()) {
            saveNew(savePath, state);
            std::cout << "Saved and read back: " << savePath << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
