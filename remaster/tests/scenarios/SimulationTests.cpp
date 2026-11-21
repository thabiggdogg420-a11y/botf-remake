#include "Simulation.h"
#include "ReferencePlanets.h"
#include "ReferenceCatalogue.h"

#include <fstream>
#include <functional>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void rejects(const std::function<void()>& fn) {
    bool rejected = false;
    try { fn(); } catch (const std::exception&) { rejected = true; }
    check(rejected, "Invalid operation was accepted");
}
botf::Command enqueue(std::uint64_t id, botf::Building b) {
    botf::Command c;
    c.id = id; c.type = botf::CommandType::EnqueueBuilding; c.systemId = 1; c.building = b;
    return c;
}
int work(const botf::Colony& c) { return c.overflow + (c.queue.empty() ? 0 : c.queue.front().work); }
std::string asSchemaTen(const botf::GameState& state) {
    std::istringstream input(botf::serialize(state));
    std::ostringstream output; std::string line;
    int planets = 0, fleets = 0, pending = 0, taskforces = 0;
    while (std::getline(input, line)) {
        if (line.starts_with("CHECKSUM ")) break;
        if (taskforces > 0) { --taskforces; continue; }
        if (planets > 0) { --planets; continue; }
        if (line.starts_with("EXPANSION ")) continue;
        if (line.starts_with("REMASTER ")) continue; // Schema 12 only.
        if (line.starts_with("GAMEPLAY_LAYER ")) { // Schema 13 only.
            while (std::getline(input, line) && line != "END_GAMEPLAY_LAYER") {}
            continue;
        }
        if (line.starts_with("PLANETS ")) {
            planets = std::stoi(line.substr(8));
            continue;
        }
        if (line.starts_with("TASKFORCES ")) {
            std::istringstream tfLine(line);
            std::string tag; int nextId = 0;
            tfLine >> tag >> nextId >> taskforces;
            continue;
        }
        if (line.starts_with("DIPLOMACY_ENGINE")) {
            while (std::getline(input, line)) {
                if (line.starts_with("END_BOTF_DIPLOMACY")) break;
            }
            continue;
        }
        if (line.starts_with("INTEL_ENGINE")) {
            while (std::getline(input, line)) {
                if (line.starts_with("CHECKSUM ")) break;
            }
            break;
        }
        if (line.starts_with("BOTF_SIM ")) line = "BOTF_SIM 10 reference-campaign-10";
        else if (line.starts_with("FLEETS ")) fleets = std::stoi(line.substr(7));
        else if (line.starts_with("PENDING ")) pending = std::stoi(line.substr(8));
        else if (fleets > 0) {
            for (int k = 0; k < 4; ++k) line.erase(line.find_last_of(' '));
            --fleets;
        } else if (pending > 0) {
            std::istringstream pStream(line);
            std::ostringstream pOut;
            for (int k = 0; k < 23; ++k) {
                std::string token;
                pStream >> token;
                if (k > 0) pOut << ' ';
                pOut << token;
            }
            line = pOut.str();
            --pending;
        }
        output << line << '\n';
    }
    auto body = output.str(); std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
std::string asSchemaNine(const botf::GameState& state) {
    std::istringstream input(asSchemaTen(state));
    std::ostringstream output; std::string line;
    int offers = 0, relations = 0, colonies = 0, fleets = 0, pending = 0;
    auto trim = [](std::string& value, int fields) {
        while (fields-- > 0) value.erase(value.find_last_of(' '));
    };
    while (std::getline(input, line)) {
        if (line.starts_with("CHECKSUM ")) break;
        if (offers > 0) { --offers; continue; }
        if (line.starts_with("ADVANCED ")) continue;
        if (line.starts_with("MAJOROFFERS ")) {
            std::istringstream values(line); std::string tag; int id = 0;
            values >> tag >> id >> offers; continue;
        }
        if (line.starts_with("BOTF_SIM ")) line = "BOTF_SIM 9 reference-campaign-9";
        else if (line.starts_with("RELATIONS ")) relations = std::stoi(line.substr(10));
        else if (line.starts_with("COLONIES ")) colonies = std::stoi(line.substr(9));
        else if (line.starts_with("FLEETS ")) fleets = std::stoi(line.substr(7));
        else if (line.starts_with("PENDING ")) pending = std::stoi(line.substr(8));
        else if (relations > 0) { trim(line, 2); --relations; }
        else if (colonies > 0) { trim(line, 1); --colonies; }
        else if (fleets > 0) { trim(line, 2); --fleets; }
        else if (pending > 0) { trim(line, 1); --pending; }
        output << line << '\n';
    }
    auto body = output.str(); std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
std::string asSchemaEight(const botf::GameState& state) {
    std::istringstream input(asSchemaTen(state));
    std::ostringstream output;
    std::string line;
    int colonies = 0, fleets = 0, pending = 0, battles = 0, relations = 0, majorOffers = 0;
    while (std::getline(input, line)) {
        if (line.starts_with("CHECKSUM ")) break;
        if (majorOffers > 0) { --majorOffers; continue; }
        if (line.starts_with("RULESET ")) continue;
        if (line.starts_with("ADVANCED ")) continue;
        if (line.starts_with("MAJOROFFERS ")) {
            std::istringstream offerLine(line); std::string ignored; int nextId = 0;
            offerLine >> ignored >> nextId >> majorOffers; continue;
        }
        if (line.starts_with("BOTF_SIM ")) line = "BOTF_SIM 8 prototype-campaign-8";
        else if (line.starts_with("COLONIES ")) colonies = std::stoi(line.substr(9));
        else if (line.starts_with("FLEETS ")) fleets = std::stoi(line.substr(7));
        else if (line.starts_with("PENDING ")) pending = std::stoi(line.substr(8));
        else if (line.starts_with("BATTLES ")) battles = std::stoi(line.substr(8));
        else if (line.starts_with("RELATIONS ")) relations = std::stoi(line.substr(10));
        else if (battles > 0) {
            std::istringstream tokens(line); std::vector<std::string> values;
            for (std::string value; tokens >> value;) values.push_back(value);
            std::size_t used = 6;
            for (int group = 0; group < 4; ++group) {
                check(used < values.size(), "Malformed battle fixture IDs");
                used += 1 + static_cast<std::size_t>(std::stoi(values[used]));
            }
            check(used < values.size(), "Malformed battle fixture damage");
            used += 1 + 3 * static_cast<std::size_t>(std::stoi(values[used]));
            check(used <= values.size(), "Malformed battle fixture size");
            std::ostringstream old;
            for (std::size_t i = 0; i < used; ++i) old << (i ? " " : "") << values[i];
            line = old.str(); --battles;
        }
        else if (colonies > 0) {
            std::istringstream tokens(line); std::vector<std::string> values;
            for (std::string value; tokens >> value;) values.push_back(value);
            check(values.size() >= 17, "Malformed schema-ten colony fixture");
            const int q = std::stoi(values[14]);
            check(values.size() == static_cast<std::size_t>(17 + 3 * q), "Malformed schema-ten project fixture");
            std::ostringstream old;
            for (int i = 0; i < 15; ++i) old << (i ? " " : "") << values[i];
            for (int i = 0; i < q; ++i) old << ' ' << values[15 + 3 * i] << ' ' << values[16 + 3 * i];
            old << ' ' << values[15 + 3 * q]; line = old.str(); --colonies;
        } else if (relations > 0) {
            line.erase(line.find_last_of(' ')); line.erase(line.find_last_of(' ')); --relations;
        } else if (fleets > 0 || pending > 0) {
            line.erase(line.find_last_of(' '));
            line.erase(line.find_last_of(' '));
            if (fleets > 0) line.erase(line.find_last_of(' '));
            if (fleets > 0) --fleets; else --pending;
        }
        output << line << '\n';
    }
    auto body = output.str(); std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
std::string asSchemaSeven(const botf::GameState& state) {
    std::istringstream input(asSchemaEight(state));
    std::ostringstream output;
    std::string line;
    int skip = 0, rivals = 0, colonies = 0, fleets = 0, pending = 0;
    auto trimFields = [](std::string value, int count) {
        while (count-- > 0) value.erase(value.find_last_of(' '));
        return value;
    };
    while (std::getline(input, line)) {
        if (line.starts_with("CHECKSUM ")) break;
        if (skip > 0) { --skip; continue; }
        if (line.starts_with("BOTF_SIM ")) line = "BOTF_SIM 7 prototype-population-7";
        else if (line.starts_with("CAMPAIGN ")) continue;
        else if (line.starts_with("RELATIONS ") || line.starts_with("TRADE ") ||
            line.starts_with("INTELREPORTS ") || line.starts_with("BATTLES ")) {
            skip = std::stoi(line.substr(line.find(' ') + 1)); continue;
        } else if (line.starts_with("RESEARCH ")) line = trimFields(line, 1);
        else if (line.starts_with("RIVALS ")) rivals = std::stoi(line.substr(7));
        else if (line.starts_with("COLONIES ")) colonies = std::stoi(line.substr(9));
        else if (line.starts_with("FLEETS ")) fleets = std::stoi(line.substr(7));
        else if (line.starts_with("PENDING ")) pending = std::stoi(line.substr(8));
        else if (rivals > 0) { line = trimFields(line, 1); --rivals; }
        else if (colonies > 0) { line = trimFields(line, 1); --colonies; }
        else if (fleets > 0) { line = trimFields(line, 5); --fleets; }
        else if (pending > 0) { line = trimFields(line, 3); --pending; }
        output << line << '\n';
    }
    auto body = output.str();
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
int failed = 0;
// Each scenario reports PASS/FAIL independently; the suite fails at the end if any failed.
void run(const char* name, const std::function<void()>& fn, int& passed) {
    try {
        fn(); ++passed; std::cout << "PASS " << name << std::endl;
    } catch (const std::exception& error) {
        ++failed; std::cout << "FAIL " << name << ": " << error.what() << std::endl;
    } catch (...) {
        ++failed; std::cout << "FAIL " << name << ": unknown exception" << std::endl;
    }
}

std::string withChecksum(std::string body) {
    if (const auto pos = body.rfind("CHECKSUM "); pos != std::string::npos) body.erase(pos);
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
// The exact schema-12 text an earlier build wrote for a state without schema-13 content.
std::string asSchemaTwelve(const botf::GameState& state) {
    auto text = botf::serialize(state);
    text.erase(text.rfind("CHECKSUM "));
    text.replace(0, text.find('\n') + 1, "BOTF_SIM 12 remaster-12\n");
    const auto start = text.find("\nGAMEPLAY_LAYER ");
    check(start != std::string::npos, "Schema 13 save lacks its GAMEPLAY_LAYER section");
    const std::string endTag = "\nEND_GAMEPLAY_LAYER\n";
    const auto end = text.find(endTag, start);
    check(end != std::string::npos, "Schema 13 gameplay section is unterminated");
    text.erase(start + 1, end + endTag.size() - (start + 1));
    return withChecksum(text);
}
// The exact schema-11 text an earlier build wrote for a state without schema-12 content.
std::string asSchemaEleven(const botf::GameState& state) {
    auto text = asSchemaTwelve(state);
    text.erase(text.rfind("CHECKSUM "));
    text.replace(0, text.find('\n') + 1, "BOTF_SIM 11 campaign-expansion-11\n");
    const auto pos = text.find("\nREMASTER ");
    check(pos != std::string::npos, "Schema 12 save lacks its REMASTER section");
    text.erase(pos + 1, text.find('\n', pos + 1) - pos);
    return withChecksum(text);
}
void revealFor(botf::GameState& s, int owner, int x, int y) {
    auto& known = owner == s.playerEmpire ? s.explored :
        std::find_if(s.rivals.begin(), s.rivals.end(), [owner](const botf::EmpireState& e) { return e.id == owner; })->explored;
    known.push_back(y * s.width + x);
    std::sort(known.begin(), known.end());
    known.erase(std::unique(known.begin(), known.end()), known.end());
}
botf::Colony& playerHome(botf::GameState& s) {
    const auto it = std::find_if(s.colonies.begin(), s.colonies.end(), [&](const botf::Colony& c) { return c.ownerId == s.playerEmpire; });
    check(it != s.colonies.end(), "Player home colony missing");
    return *it;
}
const botf::System& systemById(const botf::GameState& s, int id) {
    const auto it = std::find_if(s.systems.begin(), s.systems.end(), [id](const botf::System& x) { return x.id == id; });
    check(it != s.systems.end(), "System missing");
    return *it;
}
std::string rejectionMessage(const std::function<void()>& fn) {
    try { fn(); } catch (const std::exception& error) { return error.what(); }
    throw std::runtime_error("Invalid operation was accepted");
}

void remasterScenarios(int& passed) {
    run("regression: queue governor probes farm capacity without exceeding population", [] {
        // Seed 1999 with Balanced/Balanced governors used to throw "Workforce exceeds
        // population" around turn 4 from advance(), preview() and deserialize().
        auto s = botf::generateReferenceCampaign(1999);
        botf::submit(s, botf::makeSetColonyGovernorCommand(1, playerHome(s).systemId,
            botf::GovernorPriority::Balanced, botf::GovernorPriority::Balanced));
        for (int turn = 0; turn < 12; ++turn) {
            (void)botf::preview(s);
            botf::advance(s);
            const auto text = botf::serialize(s);
            check(botf::serialize(botf::deserialize(text)) == text, "Governed campaign did not reload");
        }
        auto direct = botf::generateReferenceCampaign(1999);
        auto colony = playerHome(direct);
        colony.queue.clear();
        colony.queuePriority = botf::GovernorPriority::Balanced;
        colony.foodWorkers = 10; colony.industryWorkers = 20; colony.researchWorkers = 10; // Fully employed.
        botf::evaluateQueueGovernor(direct, colony);
        check(!colony.queue.empty() && colony.industryWorkers == 20, "Queue governor failed or changed workers");
    }, passed);

    run("regression: captured minor-member colony transfers membership and keeps the turn's production", [] {
        for (bool reference : {false, true}) {
            auto base = reference ? botf::generateReferenceCampaign(1999) : botf::generateCampaign(1999);
            base.aiAggressionEnabled = false; base.eventsEnabled = false;
            check(!base.minors.empty(), "Campaign lacks a minor race");
            auto& minor = base.minors.front();
            const int systemId = minor.systemId;
            const auto location = systemById(base, systemId);
            minor.memberOf = 2; minor.contact[1] = true; minor.contact[2] = true;
            botf::Colony member; member.systemId = systemId; member.ownerId = 2;
            member.population = 30; member.foodWorkers = 15; member.industryWorkers = 10; member.researchWorkers = 5;
            member.queue.push_back({botf::Building::Farm, 59}); // Completes during this turn's production.
            base.colonies.push_back(member);
            std::sort(base.colonies.begin(), base.colonies.end(), [](const botf::Colony& a, const botf::Colony& b) { return a.systemId < b.systemId; });
            revealFor(base, 1, location.x, location.y); revealFor(base, 2, location.x, location.y);
            base.fleets.push_back({base.nextFleetId++, 1, location.x, location.y, location.x, location.y,
                false, false, botf::Building::Transport, 14});
            base.majorRelations[0].contact = true; base.majorRelations[0].atWar = true;
            if (reference) botf::syncDiplomacyState(base);
            botf::Command invade; invade.id = 1; invade.type = botf::CommandType::Invade;
            invade.fleetId = base.fleets.back().id; invade.systemId = systemId;
            botf::submit(base, invade);
            bool captured = false;
            for (std::uint64_t rng = 0; rng < 200 && !captured; ++rng) {
                auto s = base; s.rngState = rng;
                const auto creditsBefore = botf::empire(s, 2).credits;
                botf::advance(s);
                botf::validate(s);
                const auto text = botf::serialize(s);
                check(botf::serialize(botf::deserialize(text)) == text, "Captured-colony save did not reload");
                const auto* colony = botf::colonyAt(s, systemId);
                check(colony && colony->farms == 3, "The colony lost this turn's completed farm");
                if (colony->ownerId != 1) continue;
                captured = true;
                check(botf::minorAt(s, systemId)->memberOf == 1, "Minor membership did not follow the conquest");
                check(colony->queue.empty() && colony->morale < 50 &&
                    colony->workforcePriority == botf::GovernorPriority::Manual, "Conquest effects missing");
                check(botf::empire(s, 2).credits != creditsBefore, "Previous owner was not credited for the turn");
                if (reference) check(botf::minorDiplomacy(s, systemId)->membershipOwner == 1,
                    "Modern diplomacy did not follow the new membership");
                botf::advance(s); // The next turn continues without an ownership mismatch.
            }
            check(captured, "No invasion succeeded across deterministic draws");
        }
    }, passed);

    run("old System-N names adopt the star-name table, planets included, without clashes", [] {
        const auto fresh = botf::generate(4242, 16, 12, 30);
        auto old = fresh;
        for (auto& sys : old.systems) {
            if (sys.id == 1) continue;
            const std::string name = "System-" + std::to_string(sys.id);
            for (auto& planet : sys.planets) planet.name = name + planet.name.substr(sys.name.size());
            sys.name = name;
        }
        old.systems[2].name = fresh.systems[3].name;   // a clash: that system keeps its old name
        const int renamed = botf::adoptStarNames(old);
        check(renamed == static_cast<int>(fresh.systems.size()) - 3, "every System-N but the clash renamed");
        check(old.systems[1].name == fresh.systems[1].name && old.systems[1].planets.front().name == fresh.systems[1].planets.front().name,
              "renamed system and planets match a fresh campaign");
        check(old.systems[3].name == "System-" + std::to_string(old.systems[3].id), "a name already in use is not taken twice");
        check(botf::adoptStarNames(old) == 0, "adopting twice changes nothing");
        const auto reloaded = botf::deserialize(botf::serialize(old));
        check(reloaded.systems[1].name == old.systems[1].name, "renamed campaign saves and reloads");
    }, passed);
    run("regression: loading a save is pure and never re-runs colony governors", [] {
        auto s = botf::generateReferenceCampaign(1999);
        auto& home = playerHome(s);
        home.workforcePriority = botf::GovernorPriority::Balanced;
        home.queuePriority = botf::GovernorPriority::Research;
        home.workforceManualOverride = false;
        home.queue.clear();
        home.foodWorkers = 20; home.industryWorkers = 20; home.researchWorkers = 0; // Not what the governor picks.
        botf::validate(s);
        const auto text = botf::serialize(s);
        const auto loaded = botf::deserialize(text);
        check(botf::serialize(loaded) == text, "Load changed campaign state");
        const auto* reloaded = botf::colonyAt(loaded, home.systemId);
        check(reloaded->foodWorkers == 20 && reloaded->industryWorkers == 20 && reloaded->researchWorkers == 0 &&
            reloaded->queue.empty(), "Load re-ran the colony governors");
        auto crafted = text;
        const std::string governorLine = "\n" + std::to_string(home.systemId) + " 1 3 0 0\n";
        check(crafted.find(governorLine) != std::string::npos, "Fixture lacks its governor record");
        crafted.replace(crafted.find(governorLine), governorLine.size(), "\n" + std::to_string(home.systemId) + " 7 3 0 0\n");
        rejects([&] { botf::deserialize(withChecksum(crafted)); });
    }, passed);

    run("regression: fusion plants are one structure whose scrap removes exactly its energy", [] {
        auto s = botf::generate(1999);
        auto& colony = s.colonies.front();
        const int baseUpkeep = botf::economy(colony).upkeep;
        colony.fusionPlants = 1; colony.energy = 60;
        botf::validate(s);
        check(botf::economy(colony).upkeep == baseUpkeep + 2, "Fusion plant upkeep missing");
        // The exploit: a fusion plant used to count as two power plants worth 10 energy each when scrapped.
        rejects([&] { botf::submit(s, botf::makeScrapStructureCommand(1, colony.systemId, botf::Building::PowerPlant)); });
        botf::submit(s, botf::makeScrapStructureCommand(1, colony.systemId, botf::Building::FusionPlant));
        const auto draft = botf::preview(s);
        check(draft.colonies.front().fusionPlants == 0 && draft.colonies.front().energy == 20,
            "Scrapping a fusion plant did not remove its 40 energy");
        auto invalid = s; invalid.pending.clear(); invalid.colonies.front().energy = 30;
        rejects([&] { botf::validate(invalid); });
        const auto text = botf::serialize(s);
        check(text.find("\nREMASTER 1 1 1 1\n") != std::string::npos, "Fusion plant missing from schema-12 save");
        check(botf::serialize(botf::deserialize(text)) == text, "Fusion plant save did not round-trip");
    }, passed);

    run("regression: crafted saves with huge counts or invalid settings are rejected promptly", [] {
        auto s = botf::generateReferenceCampaign(1999);
        botf::submit(s, enqueue(1, botf::Building::Farm));
        const auto text = botf::serialize(s);
        const auto pendingStart = text.find("\nPENDING 1\n");
        check(pendingStart != std::string::npos, "Fixture lacks its pending command");
        const auto lineEnd = text.find('\n', pendingStart + 11);
        auto crafted = text;
        const std::string tail = " 0 0 0";
        check(crafted.compare(lineEnd - tail.size(), tail.size(), tail) == 0, "Unexpected pending command layout");
        crafted.replace(lineEnd - 1, 1, "18446744073709551615");
        rejects([&] { botf::deserialize(withChecksum(crafted)); }); // Used to loop 2^64 times.
        crafted = text;
        crafted.replace(crafted.find("INTEL_OPERATIONS 0\n"), 19, "INTEL_OPERATIONS 100\n");
        rejects([&] { botf::deserialize(withChecksum(crafted)); });
        // CreateTaskForce drafts respect the task force cap.
        auto capped = botf::generateReferenceCampaign(1999);
        const auto home = capped.systems.front();
        for (int i = 0; i < 128; ++i) {
            const int id = capped.nextFleetId++;
            capped.fleets.push_back({id, 1, home.x, home.y, home.x, home.y, false, false, botf::Building::Scout, 12});
            botf::TaskForce group; group.id = capped.nextTaskForceId++; group.ownerId = 1;
            group.name = "Picket " + std::to_string(i); group.fleetIds.push_back(id); group.targetX = home.x; group.targetY = home.y;
            capped.fleets.back().taskForceId = group.id;
            capped.taskForces.push_back(group);
        }
        botf::validate(capped);
        const int spare = capped.fleets.front().id;
        const auto capMessage = rejectionMessage([&] { botf::submit(capped, botf::makeCreateTaskForceCommand(1, {spare}, "One Too Many")); });
        check(capMessage.find("capacity") != std::string::npos, "Task force cap not enforced: " + capMessage);
    }, passed);

    run("regression: player text with control characters is rejected so saves always reload", [] {
        auto s = botf::generateReferenceCampaign(1999);
        const auto message = rejectionMessage([&] {
            botf::submit(s, botf::makeRespondProposalCommand(1, 1, false, "line\nbreak"));
        });
        check(message.find("printable") != std::string::npos, "Newline in a proposal reason was not rejected as text: " + message);
        rejects([&] { botf::submit(s, botf::makeRespondProposalCommand(1, 1, false, std::string(300, 'x'))); });
        rejects([&] { botf::submit(s, botf::makeCreateTaskForceCommand(1, {s.fleets.front().id}, "Alpha\x01")); });
        botf::submit(s, botf::makeCreateTaskForceCommand(1, {s.fleets.front().id}, "Alpha \"Prime\" \\ Group"));
        const auto text = botf::serialize(s);
        check(botf::serialize(botf::deserialize(text)) == text, "Quoted task force name did not reload");
    }, passed);

    run("unified combat: new campaigns resolve battles tactically and schema-11 saves keep the legacy resolver", [] {
        auto s = botf::generateCampaign(1999); s.aiAggressionEnabled = false;
        check(s.unifiedCombat, "New campaigns must use unified tactical combat");
        const auto sol = s.systems.front();
        s.fleets[2].x = s.fleets[2].targetX = sol.x; s.fleets[2].y = s.fleets[2].targetY = sol.y;
        revealFor(s, 2, sol.x, sol.y);
        s.majorRelations[0].contact = true; s.majorRelations[0].atWar = true;
        auto legacy = s; legacy.unifiedCombat = false;
        const auto oldText = asSchemaEleven(legacy);
        auto loaded = botf::deserialize(oldText);
        check(!loaded.unifiedCombat && botf::serialize(loaded) == botf::serialize(legacy), "Schema-11 save did not load exactly");
        auto resumed = botf::deserialize(botf::serialize(s));
        const auto unified = botf::advance(s);
        botf::advance(resumed);
        check(botf::serialize(s) == botf::serialize(resumed), "Unified battle replay diverged after reload");
        check(unified.battles.size() == 1 && unified.battles[0].rounds.size() <= 50, "Unified battle missing or unbounded");
        const auto oldReport = botf::advance(loaded);
        botf::advance(legacy);
        check(botf::serialize(loaded) == botf::serialize(legacy) && oldReport.battles.size() == 1,
            "Schema-11 campaign did not continue on the legacy resolver");
        auto broken = oldText; broken.insert(broken.find("COLONY_GOVERNORS"), "REMASTER 0 0\n");
        const auto brokenMessage = rejectionMessage([&] { botf::deserialize(withChecksum(broken)); });
        check(brokenMessage.find("Schema 12 section") != std::string::npos, "Schema-11 save accepted a schema-12 section");
    }, passed);

    run("unified combat: a strategic retreat disengages under fire instead of escaping for free", [] {
        auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
        const auto sol = s.systems.front();
        auto rival = std::find_if(s.fleets.begin(), s.fleets.end(), [](const botf::Fleet& f) { return f.ownerId == 2; });
        rival->x = rival->targetX = sol.x; rival->y = rival->targetY = sol.y;
        revealFor(s, 2, sol.x, sol.y);
        s.majorRelations[0].contact = true; s.majorRelations[0].atWar = true;
        botf::Command retreat; retreat.id = 1; retreat.type = botf::CommandType::RetreatFleet; retreat.fleetId = s.fleets[0].id;
        botf::submit(s, retreat);
        const int retreating = s.fleets[0].id;
        const auto report = botf::advance(s);
        check(report.battles.size() == 1, "Contested retreat did not trigger a battle");
        const auto& battle = report.battles.front();
        const bool withdrew = std::count(battle.retreatedFleetIds.begin(), battle.retreatedFleetIds.end(), retreating) == 1;
        const bool lost = std::count(battle.lostFleetIds.begin(), battle.lostFleetIds.end(), retreating) == 1;
        check(withdrew || lost, "Retreating fleet neither withdrew nor fell");
        if (withdrew) {
            check(battle.rounds.size() >= 2 && std::find(battle.rounds.front().retreatedFleetIds.begin(),
                battle.rounds.front().retreatedFleetIds.end(), retreating) == battle.rounds.front().retreatedFleetIds.end(),
                "Retreat escaped before spending a round disengaging");
        }
        botf::validate(s);
    }, passed);
}
}

int main() {
    int passed = 0;
    try {
        run("seeded galaxy reproducible, different seed differs, full map terminates", [] {
            check(botf::serialize(botf::generate(1999)) == botf::serialize(botf::generate(1999)), "Seed mismatch");
            auto a = botf::generate(1), b = botf::generate(2);
            bool different = false;
            for (std::size_t i = 0; i < a.systems.size(); ++i)
                different |= a.systems[i].x != b.systems[i].x || a.systems[i].y != b.systems[i].y;
            check(different, "Different seeds have identical coordinates");
            botf::validate(botf::generate(0, 16, 16, 256));
            rejects([] { botf::generate(0, 2, 2, 5); });
        }, passed);
        run("hand-calculated home output and credit ledger", [] {
            auto s = botf::generate(10);
            const auto e = botf::economy(s.colonies[0]);
            check(e.food == 40 && e.foodDeficit == 0 && e.industry == 20 && e.research == 10,
                "Baseline output differs from fixture arithmetic");
            check(e.energyUsed == 20 && e.income == 20 && e.upkeep == 7, "Baseline costs wrong");
            const auto r = botf::advance(s);
            check(s.credits == 110 && s.research == 10 && s.turn == 1, "Settlement wrong");
            check(r.creditsAfter - r.creditsBefore == 10, "Ledger not reconciled");
        }, passed);
        run("power shortage uses deterministic factory-first priority", [] {
            auto s = botf::generate(1); auto& c = s.colonies[0];
            c.energy = 5; c.foodWorkers = 20; c.industryWorkers = 20; c.researchWorkers = 0;
            const auto e = botf::economy(c);
            check(e.industry == 20 && e.research == 0 && e.energyUsed == 5 &&
                e.inactiveFactories == 1 && e.inactiveLabs == 1, "Power allocation wrong");
            c.energy = 0;
            check(botf::economy(c).industry == 0, "Unpowered industry produced output");
        }, passed);
        run("pending commands do not mutate live colony; cancellation and IDs", [] {
            auto s = botf::generate(1);
            botf::submit(s, enqueue(1, botf::Building::Farm));
            check(s.colonies[0].queue.empty() && s.pending.size() == 1, "Draft applied early");
            const auto before = botf::serialize(s);
            rejects([&] { botf::submit(s, enqueue(1, botf::Building::Farm)); });
            check(before == botf::serialize(s), "Rejected duplicate changed state");
            botf::cancel(s, 1); check(s.pending.empty(), "Cancel did not remove draft");
            rejects([&] { botf::submit(s, enqueue(1, botf::Building::Farm)); });
            botf::advance(s); check(s.colonies[0].farms == 2, "Cancelled order completed");
        }, passed);
        run("workforce changes at commit; invalid and unowned orders atomic", [] {
            auto s = botf::generate(1);
            botf::Command c; c.id = 1; c.systemId = 1;
            c.foodWorkers = 20; c.industryWorkers = 20;
            botf::submit(s, c);
            check(s.colonies[0].industryWorkers == 10, "Workers changed before commit");
            auto before = botf::serialize(s);
            c.id = 2; c.researchWorkers = 10;
            rejects([&] { botf::submit(s, c); });
            check(botf::serialize(s) == before, "Bad workforce mutated state");
            auto other = enqueue(2, botf::Building::Farm); other.systemId = 2;
            rejects([&] { botf::submit(s, other); });
            check(botf::serialize(s) == before, "Unowned order mutated state");
            const auto r = botf::advance(s);
            check(r.colonies[0].economy.industry == 40 && r.researchGained == 0, "Assignment not applied");
        }, passed);
        run("one completion per turn; carryover and next-turn upkeep", [] {
            auto s = botf::generate(1);
            s.colonies[0].overflow = 190;
            botf::submit(s, enqueue(1, botf::Building::Factory));
            botf::submit(s, enqueue(2, botf::Building::Farm));
            const auto first = botf::advance(s);
            check(s.colonies[0].factories == 3 && s.colonies[0].farms == 2 &&
                work(s.colonies[0]) == 110 && s.colonies[0].queue.size() == 1,
                "Completion overflow or single-completion rule wrong");
            check(first.colonies[0].economy.upkeep == 7, "New building charged too soon");
            const auto second = botf::advance(s);
            check(s.colonies[0].farms == 3 && work(s.colonies[0]) == 70 &&
                second.colonies[0].economy.upkeep == 8, "Next turn activation wrong");
        }, passed);
        run("exact construction threshold and no free idle banking", [] {
            auto s = botf::generate(1);
            botf::submit(s, enqueue(1, botf::Building::Farm));
            botf::advance(s); botf::advance(s);
            check(s.colonies[0].farms == 2 && work(s.colonies[0]) == 40, "Early construction");
            botf::advance(s);
            check(s.colonies[0].farms == 3 && work(s.colonies[0]) == 0, "Exact cost failed");
            botf::advance(s); check(work(s.colonies[0]) == 0, "Idle output was banked");
        }, passed);
        run("draft queue checked cumulatively", [] {
            auto s = botf::generate(1);
            for (std::uint64_t i = 1; i <= 64; ++i) botf::submit(s, enqueue(i, botf::Building::Farm));
            auto before = botf::serialize(s);
            rejects([&] { botf::submit(s, enqueue(65, botf::Building::Farm)); });
            check(botf::serialize(s) == before, "Queue rejection changed state");
        }, passed);
        run("turn failure rolls back all settlement and pending commands", [] {
            auto s = botf::generate(1); s.credits = 1000000000000LL;
            botf::submit(s, enqueue(1, botf::Building::Farm));
            const auto before = botf::serialize(s);
            rejects([&] { botf::advance(s); });
            check(before == botf::serialize(s), "Failed settlement partially committed");
        }, passed);
        run("save pending orders and resume exact 30-turn continuation", [] {
            auto a = botf::generate(1999);
            botf::submit(a, enqueue(1, botf::Building::Factory));
            botf::submit(a, enqueue(2, botf::Building::Farm));
            auto b = botf::deserialize(botf::serialize(a));
            check(b.pending.size() == 2 && b.rngState == a.rngState, "Pending or random state lost");
            for (int turn = 0; turn < 30; ++turn) {
                const auto ar = botf::advance(a); const auto br = botf::advance(b);
                check(botf::describe(ar) == botf::describe(br), "Continuation reports differ");
                if (turn == 6) b = botf::deserialize(botf::serialize(b));
            }
            check(botf::serialize(a) == botf::serialize(b), "Continuation state differs");
            check(a.colonies[0].factories == 3 && a.colonies[0].farms == 3, "Commands replayed incorrectly");
        }, passed);
        run("save corruption, truncation, content/schema mismatch and oversized input", [] {
            const auto original = botf::serialize(botf::generate(1));
            auto bad = original; bad.replace(bad.find("Sol"), 3, "Sun");
            rejects([&] { botf::deserialize(bad); });
            rejects([&] { botf::deserialize(original.substr(0, original.size() / 2)); });
            bad = original; bad.replace(0, 10, "BOTF_SIM 10");
            rejects([&] { botf::deserialize(bad); });
            bad = original; bad.replace(bad.find(botf::ContentVersion), std::string(botf::ContentVersion).size(), "different-content");
            rejects([&] { botf::deserialize(bad); });
            rejects([&] { botf::deserialize(original + "TRAILING"); });
            rejects([] { botf::deserialize(std::string(1024 * 1024 + 1, 'x')); });
        }, passed);
        run("invalid direct state rejected", [] {
            auto s = botf::generate(1); s.systems[1].x = s.systems[0].x; s.systems[1].y = s.systems[0].y;
            rejects([&] { botf::validate(s); });
            s = botf::generate(1); s.colonies[0].queue.push_back(static_cast<botf::Building>(99));
            rejects([&] { botf::serialize(s); });
        }, passed);
        run("fleet orders are deferred, bounded, cancellable and survive reload", [] {
            auto s = botf::generate(1999); const auto start = s.fleets.front();
            botf::Command c; c.id = 1; c.type = botf::CommandType::MoveFleet; c.fleetId = 1;
            c.targetX = 5; c.targetY = 0;
            botf::submit(s, c);
            check(s.fleets.front().targetX == start.x, "Draft route applied early");
            const auto saved = botf::serialize(s);
            c.id = 2; rejects([&] { botf::submit(s, c); });
            check(saved == botf::serialize(s), "Duplicate fleet draft mutated state");
            auto resumed = botf::deserialize(saved);
            for (int i = 0; i < 15; ++i) { botf::advance(s); botf::advance(resumed); }
            check(botf::serialize(s) == botf::serialize(resumed), "Fleet reload diverged");
            check(s.fleets.front().x == 5 && s.fleets.front().y == 0, "Fleet failed to arrive");
            c.id = 2; c.targetX = s.width;
            rejects([&] { botf::submit(s, c); });
            c.targetX = 6; c.fleetId = 99; rejects([&] { botf::submit(s, c); });
            c.fleetId = 1; botf::submit(s, c); botf::cancel(s, 2); botf::advance(s);
            check(s.fleets.front().x == 5, "Cancelled route executed");
        }, passed);
        run("scout reveals sectors without revealing whole map, knowledge persists", [] {
            auto s = botf::generate(1999);
            check(s.explored.size() <= 9 && !botf::explored(s, -1, 0), "Initial fog invalid");
            const auto known = s.explored;
            botf::Command c; c.id = 1; c.type = botf::CommandType::MoveFleet; c.fleetId = 1;
            c.targetX = 5; c.targetY = 0; botf::submit(s, c);
            const auto before = s.fleets.front(); botf::advance(s);
            check(s.fleets.front().x == before.x - (before.x > 0) &&
                s.fleets.front().y == before.y - (before.y > 0), "Movement exceeds one sector per axis");
            for (int cell : known) check(botf::explored(s, cell % s.width, cell / s.width), "Exploration forgotten");
            check(s.explored.size() < static_cast<std::size_t>(s.width * s.height), "Whole map revealed");
        }, passed);
        run("settle consumes ship, defers economy, supports development and reload", [] {
            auto s = botf::generate(1999); const auto target = s.systems[1];
            botf::Command c; c.id = 1; c.type = botf::CommandType::Colonize; c.fleetId = 1;
            rejects([&] { botf::submit(s, c); }); // Scout cannot settle.
            c.fleetId = 2; rejects([&] { botf::submit(s, c); }); // Home is occupied.
            c.type = botf::CommandType::MoveFleet; c.targetX = target.x; c.targetY = target.y;
            botf::submit(s, c); botf::advance(s);
            c.id = 2; c.type = botf::CommandType::Colonize; c.targetX = c.targetY = 0;
            rejects([&] { botf::submit(s, c); }); // Still in transit.
            for (int i = 0; i < 12; ++i) botf::advance(s);
            botf::submit(s, c); const auto draft = botf::serialize(s);
            auto reload = botf::deserialize(draft); const auto report = botf::advance(s); botf::advance(reload);
            check(botf::serialize(s) == botf::serialize(reload), "Pending colonization reload diverged");
            check(report.colonies.size() == 1 && report.events.size() == 1, "Founding-turn production occurred");
            check(s.fleets.size() == 1 && s.colonies.size() == 2, "Settlement did not consume ship");
            check(botf::colonyAt(s, target.id)->ownerId == s.playerEmpire, "New colony ownership wrong");
            c.id = 3; rejects([&] { botf::submit(s, c); }); // Consumed ship cannot act.
            auto build = enqueue(3, botf::Building::Laboratory); build.systemId = target.id;
            botf::submit(s, build);
            check(botf::advance(s).colonies.size() == 2, "New colony failed to produce following turn");
        }, passed);
        run("invalid knowledge and fleet state rejected; old saves rejected", [] {
            auto s = botf::generate(1999); s.explored.push_back(s.explored.front());
            rejects([&] { botf::serialize(s); });
            s = botf::generate(1999); s.fleets.front().ownerId = 2;
            rejects([&] { botf::serialize(s); });
            s = botf::generate(1999); s.fleets.front().targetX = -1;
            rejects([&] { botf::serialize(s); });
            auto save = botf::serialize(botf::generate(1999)); save.replace(9, 2, "1");
            rejects([&] { botf::deserialize(save); });
        }, passed);
        run("ship launch threshold, next-turn upkeep, identity and save continuation", [] {
            auto s = botf::generate(1999);
            botf::submit(s, enqueue(1, botf::Building::Scout));
            auto resumed = botf::deserialize(botf::serialize(s));
            for (int i = 0; i < 2; ++i) { botf::advance(s); botf::advance(resumed); }
            check(s.fleets.size() == 2 && work(s.colonies[0]) == 40, "Ship launched early");
            const auto launch = botf::advance(s); botf::advance(resumed);
            check(s.fleets.size() == 3 && s.fleets.back().id == 3 && s.nextFleetId == 4, "Ship identity invalid");
            check(work(s.colonies[0]) == 0 && s.credits == 130 && launch.fleetUpkeep == 3, "Launch accounting wrong");
            check(s.fleets.back().x == s.systems.front().x && s.fleets.back().y == s.systems.front().y, "Launch location wrong");
            check(botf::serialize(s) == botf::serialize(resumed), "Shipbuilding reload diverged");
            const auto next = botf::advance(s);
            check(next.fleetUpkeep == 4 && s.credits == 139, "New ship upkeep activation wrong");
            s.fleets.pop_back(); // Simulate removal; identity must never be recycled.
            botf::submit(s, enqueue(2, botf::Building::ColonyShip));
            for (int i = 0; i < 5; ++i) botf::advance(s);
            check(s.fleets.back().id == 4 && s.fleets.back().colonyShip, "Consumed ID recycled or wrong hull launched");
        }, passed);
        run("yard prerequisite, duplicate rejection and deferred yard activation", [] {
            auto s = botf::generate(1999);
            rejects([&] { botf::submit(s, enqueue(1, botf::Building::Shipyard)); });
            s.colonies[0].shipyards = 0;
            rejects([&] { botf::submit(s, enqueue(1, botf::Building::Scout)); });
            botf::submit(s, enqueue(1, botf::Building::Shipyard));
            botf::submit(s, enqueue(2, botf::Building::Scout));
            rejects([&] { botf::cancel(s, 1); }); // Ship draft would lose prerequisite.
            for (int i = 0; i < 6; ++i) botf::advance(s);
            check(s.colonies[0].shipyards == 1 && s.fleets.size() == 2 && work(s.colonies[0]) == 0,
                "Ship completed on yard construction turn");
            botf::advance(s); check(work(s.colonies[0]) == 20, "Powered yard failed to build next turn");
        }, passed);
        run("unpowered shipyard pauses production without banking or discarding progress", [] {
            auto s = botf::generate(1999);
            botf::submit(s, enqueue(1, botf::Building::Scout)); botf::advance(s);
            botf::Command power; power.id = 2; power.type = botf::CommandType::ToggleShipyard; power.systemId = 1;
            botf::submit(s, power); auto report = botf::advance(s);
            check(work(s.colonies[0]) == 20 && !botf::economy(s.colonies[0]).shipyardPowered && !report.events.empty(), "Power loss not handled");
            botf::advance(s); check(work(s.colonies[0]) == 20, "Paused production banked output");
            power.id = 3; botf::submit(s, power); botf::advance(s);
            check(work(s.colonies[0]) == 40, "Re-enabled production failed");
        }, passed);
        run("one shipbuilding slot, deterministic priority, paused yard frees slot", [] {
            auto s = botf::generate(1999);
            botf::Colony other; other.systemId = 2; other.shipyards = 1;
            s.colonies.push_back(other);
            const auto& system = s.systems[1]; s.explored.clear();
            for (int cell = 0; cell < s.width * s.height; ++cell) s.explored.push_back(cell);
            check(botf::explored(s, system.x, system.y), "Fixture exploration invalid");
            botf::submit(s, enqueue(1, botf::Building::Scout));
            auto order = enqueue(2, botf::Building::Scout); order.systemId = 2; botf::submit(s, order);
            botf::advance(s);
            check(work(s.colonies[0]) == 20 && work(s.colonies[1]) == 0, "Concurrent builds ignored slot limit");
            s.colonies[0].shipyardEnabled = false; botf::advance(s);
            check(work(s.colonies[0]) == 20 && work(s.colonies[1]) == 20, "Unpowered yard retained slot");
        }, passed);
        run("range boundary, supply extension, power loss and atomic invalid route", [] {
            auto s = botf::generate(1999); const auto f = s.fleets[1];
            check(botf::supplied(s, f, 7, 3) && !botf::supplied(s, f, 6, 3), "Medium range boundary incorrect");
            check(botf::supplied(s, s.fleets[0], 5, 1), "Scout range not longer");
            botf::Command move; move.id = 1; move.type = botf::CommandType::MoveFleet;
            move.fleetId = 2; move.targetX = 6; move.targetY = 3;
            const auto original = botf::serialize(s); rejects([&] { botf::submit(s, move); });
            check(original == botf::serialize(s), "Invalid route mutated state");
            botf::Colony other; other.systemId = 2; other.shipyards = 1; s.colonies.push_back(other);
            s.explored.clear(); for (int cell = 0; cell < s.width * s.height; ++cell) s.explored.push_back(cell);
            check(botf::supplied(s, f, 3, 3), "New yard failed to extend range");
            s.colonies.back().shipyardEnabled = false;
            check(!botf::supplied(s, f, 3, 3), "Unpowered yard still supplies ships");
            move.targetX = 7; botf::submit(s, move);
            botf::Command off; off.id = 2; off.type = botf::CommandType::ToggleShipyard; off.systemId = 1;
            botf::submit(s, off); const auto report = botf::advance(s);
            check(s.fleets[1].x == f.x && s.fleets[1].y == f.y && !report.events.empty(), "Fleet moved without supply");
            off.id = 3; botf::submit(s, off); botf::advance(s);
            check(s.fleets[1].x != f.x || s.fleets[1].y != f.y, "Restored supply did not resume route");
        }, passed);
        run("routing cannot jump disconnected supply regions", [] {
            auto s = botf::generate(1999, 32, 8, 2);
            s.systems[0].x = 1; s.systems[0].y = 1; s.systems[1].x = 25; s.systems[1].y = 1;
            for (auto& f : s.fleets) { f.x = f.targetX = 1; f.y = f.targetY = 1; }
            botf::Colony other; other.systemId = 2; other.shipyards = 1; s.colonies.push_back(other);
            s.explored.clear(); for (int cell = 0; cell < s.width * s.height; ++cell) s.explored.push_back(cell);
            botf::validate(s);
            check(botf::supplied(s, s.fleets[0], 25, 1) && botf::route(s, s.fleets[0], 25, 1).empty(), "Route crossed unsupplied gap");
        }, passed);
        run("power expansion restores yard without same-turn activation", [] {
            auto s = botf::generate(1999);
            s.colonies[0].energy = 15; // Factories and lab consume all existing power.
            botf::submit(s, enqueue(1, botf::Building::PowerPlant));
            botf::submit(s, enqueue(2, botf::Building::Scout));
            for (int i = 0; i < 3; ++i) botf::advance(s);
            const auto completion = botf::advance(s);
            check(!completion.colonies[0].economy.shipyardPowered && completion.colonies[0].economy.upkeep == 7,
                "Power plant activated/charged on completion turn");
            check(s.colonies[0].energy == 25 && s.colonies[0].powerPlants == 1 && s.fleets.size() == 2,
                "Power plant did not expand capacity");
            const auto next = botf::advance(s);
            check(next.colonies[0].economy.shipyardPowered && next.colonies[0].economy.upkeep == 8 &&
                work(s.colonies[0]) == 20, "Yard did not resume after power expansion");
            check(botf::serialize(s) == botf::serialize(botf::deserialize(botf::serialize(s))), "Power state lost on reload");
        }, passed);
        run("queue recovery from power stall is deferred and cannot transfer progress", [] {
            auto s = botf::generate(1999); s.colonies[0].energy = 15;
            
            botf::submit(s, enqueue(1, botf::Building::Scout));
            botf::submit(s, enqueue(2, botf::Building::PowerPlant)); botf::advance(s);
            s.colonies[0].queue.front().work = 40;
            botf::Command reorder; reorder.id = 3; reorder.systemId = 1;
            reorder.type = botf::CommandType::PrioritizeLastConstruction;
            botf::submit(s, reorder);
            check(s.colonies[0].queue.front() == botf::Building::Scout && work(s.colonies[0]) == 40, "Reorder applied before commit");
            botf::advance(s);
            check(s.colonies[0].queue.front() == botf::Building::PowerPlant && work(s.colonies[0]) == 20, "Ship progress transferred to power plant");
            for (int i = 0; i < 6; ++i) botf::advance(s);
            check(s.fleets.size() == 3 && s.colonies[0].powerPlants == 1, "Stalled queue did not recover");
            botf::submit(s, enqueue(4, botf::Building::ColonyShip)); botf::advance(s);
            reorder.id = 5; reorder.type = botf::CommandType::CancelFirstConstruction; botf::submit(s, reorder);
            botf::advance(s); check(s.colonies[0].queue.empty() && work(s.colonies[0]) == 0, "Queue cancellation retained work");
        }, passed);
        run("moving projects retains individual work across save and arbitrary positions", [] {
            auto s = botf::generate(1999);
            s.colonies[0].queue = {{botf::Building::Scout, 40}, {botf::Building::Farm, 10}, {botf::Building::PowerPlant, 30}};
            botf::Command c; c.id = 1; c.systemId = 1; c.type = botf::CommandType::MoveConstruction;
            c.queueIndex = 2; c.queueDestination = 0; botf::submit(s, c);
            const auto draft = botf::preview(s);
            check(draft.colonies[0].queue[0].work == 30 && draft.colonies[0].queue[1].work == 40,
                "Reorder transferred or discarded project work");
            check(s.colonies[0].queue[0].work == 40, "Preview mutated live queue");
            auto resumed = botf::deserialize(botf::serialize(s));
            botf::advance(s); botf::advance(resumed);
            check(botf::serialize(s) == botf::serialize(resumed), "Pending queue move failed to resume");
            check(s.colonies[0].queue[0].work == 50 && s.colonies[0].queue[1].work == 40, "Wrong project received production");
            c.id = 2; c.type = botf::CommandType::RemoveConstruction; c.queueIndex = 1;
            botf::submit(s, c); const auto removed = botf::preview(s);
            check(removed.colonies[0].queue.size() == 2 && removed.colonies[0].queue[1].work == 10, "Removal affected another project");
            botf::cancel(s, 2); check(botf::preview(s).colonies[0].queue[1].work == 40, "Undo did not restore project work");
        }, passed);
        run("queue move rejects broken prerequisites and invalid positions atomically", [] {
            auto s = botf::generate(1999); s.colonies[0].shipyards = 0;
            s.colonies[0].queue = {botf::Building::Shipyard, botf::Building::Scout};
            const auto before = botf::serialize(s);
            botf::Command c; c.id = 1; c.systemId = 1; c.type = botf::CommandType::MoveConstruction;
            c.queueIndex = 1; c.queueDestination = 0; rejects([&] { botf::submit(s, c); });
            c.queueIndex = 64; rejects([&] { botf::submit(s, c); });
            c.queueIndex = 0; c.type = botf::CommandType::RemoveConstruction; rejects([&] { botf::submit(s, c); });
            check(before == botf::serialize(s), "Rejected queue operation mutated state");
        }, passed);
        run("research allocations conserve rounding and survive pending save reload", [] {
            auto s = botf::generate(1999);
            botf::Command c; c.id = 1; c.type = botf::CommandType::AllocateResearch;
            c.researchAllocation = {10,10,10,50,10,10}; botf::submit(s, c);
            check(s.technologies[3].allocation == 17 && botf::preview(s).technologies[3].allocation == 50, "Research draft timing wrong");
            auto resumed = botf::deserialize(botf::serialize(s));
            for (int turn = 0; turn < 40; ++turn) { botf::advance(s); botf::advance(resumed); }
            check(s.technologies[3].level == 1 && s.technologies[3].progress == 0, "Focused research threshold wrong");
            check(s.research == 400 && botf::serialize(s) == botf::serialize(resumed), "Research ledger or save continuation diverged");
            for (int i = 0; i < 6; ++i) if (i != 3) check(s.technologies[i].progress == 40, "Research points lost");
            c.id = 2; c.researchAllocation[0] = 11;
            const auto before = botf::serialize(s); rejects([&] { botf::submit(s, c); });
            check(before == botf::serialize(s), "Invalid allocation changed state");
        }, passed);
        run("technology gates construction and benefits activate on following turn", [] {
            auto s = botf::generate(1999);
            rejects([&] { botf::submit(s, enqueue(1, botf::Building::FusionPlant)); });
            rejects([&] { botf::submit(s, enqueue(1, botf::Building::ResearchInstitute)); });
            botf::Command c; c.id = 1; c.type = botf::CommandType::AllocateResearch;
            c.researchAllocation = {0,0,100,0,0,0}; botf::submit(s, c);
            for (int turn = 0; turn < 19; ++turn) botf::advance(s);
            const auto discovery = botf::advance(s);
            check(discovery.colonies[0].economy.industry == 20 && s.technologies[2].level == 1, "Technology activated early");
            check(botf::advance(s).colonies[0].economy.industry == 21, "Construction technology has no effect");
            c.id = 2; c.researchAllocation = {0,0,0,100,0,0}; botf::submit(s, c);
            for (int turn = 0; turn < 20; ++turn) botf::advance(s);
            botf::submit(s, enqueue(3, botf::Building::FusionPlant));
            for (int turn = 0; turn < 8; ++turn) botf::advance(s);
            // Schema 12: a fusion plant is its own structure (40 energy), not two 10-energy power plants.
            check(s.colonies[0].energy == 60 && s.colonies[0].fusionPlants == 1 && s.colonies[0].powerPlants == 0,
                "Unlocked fusion plant failed to build");
        }, passed);
        run("research terminal levels reserve surplus and ledger rejects tampering", [] {
            auto s = botf::generate(1999);
            for (auto& tech : s.technologies) {
                tech.level = botf::MaxTechLevel;
                for (int level = 1; level <= tech.level; ++level) s.research += botf::researchCost(level);
            }
            const auto r = botf::advance(s);
            check(s.researchReserve == r.researchGained, "Terminal research vanished");
            const auto save = botf::serialize(s);
            check(save == botf::serialize(botf::deserialize(save)), "Terminal research failed to reload");
            s.researchReserve++; rejects([&] { botf::validate(s); });
        }, passed);
        run("rival empires have separate economies, research, knowledge and orders", [] {
            auto s = botf::generateCampaign(1999);
            check(s.rivals.size() == 1 && s.colonies.size() == 2 && s.fleets.size() == 4, "Campaign setup incomplete");
            const auto playerKnowledge = s.explored;
            const auto enemy = s.fleets.back();
            check(!botf::visible(s, 1, enemy.x, enemy.y), "Distant enemy visible at setup");
            botf::Command c; c.id = 1; c.type = botf::CommandType::MoveFleet; c.fleetId = enemy.id;
            c.targetX = enemy.x; c.targetY = enemy.y; rejects([&] { botf::submit(s, c); });
            const auto r = botf::advance(s);
            check(s.credits == 110 && s.research == 10 && r.colonies.size() == 1, "Rival income or reports leaked into player ledger");
            check(s.rivals[0].credits == 110 && s.rivals[0].research == 10, "Rival economy failed to resolve");
            check(s.explored == playerKnowledge && s.rivals[0].explored.size() > playerKnowledge.size(), "AI exploration leaked or failed");
        }, passed);
        run("five empires expand without grants and resume identical 100-turn campaigns", [] {
            auto s = botf::generateCampaign(1999, 5);
            s.aiAggressionEnabled = false;
            auto resumed = botf::deserialize(botf::serialize(s));
            for (int turn = 0; turn < 100; ++turn) {
                botf::advance(s); botf::advance(resumed);
                if (turn == 29) resumed = botf::deserialize(botf::serialize(resumed));
                check(botf::serialize(s) == botf::serialize(resumed), "AI campaign save diverged");
            }
            check(s.colonies.size() > 5 && s.nextFleetId > 11, "AI never settled or constructed a ship");
            for (const auto& owner : s.rivals) {
                check(owner.research >= 1000 && owner.explored.size() > 9, "Rival did not research and explore");
            }
        }, passed);
        run("previous development saves migrate without injecting rival empires", [] {
            auto s = botf::generate(1999); s.eventsEnabled = false; s.aiAggressionEnabled = false;
            s.unifiedCombat = false; // Pre-schema-12 campaigns load with the legacy combat model.
            botf::submit(s, enqueue(1, botf::Building::Factory)); botf::advance(s);
            s.intelligence = 0;
            botf::submit(s, enqueue(2, botf::Building::PowerPlant));
            auto legacy = asSchemaSeven(s); legacy.erase(legacy.rfind("CHECKSUM "));
            legacy.replace(0, legacy.find('\n') + 1, "BOTF_SIM 4 prototype-development-4\n");
            legacy.erase(legacy.find("POPULATION 0\n"), std::string("POPULATION 0\n").size());
            legacy.erase(legacy.find("RIVALS 0\n"), std::string("RIVALS 0\n").size());
            legacy.erase(legacy.find("MINORS 1 0\nDIPLOMACY 0\n"), std::string("MINORS 1 0\nDIPLOMACY 0\n").size());
            std::uint64_t hash = 14695981039346656037ULL;
            for (unsigned char byte : legacy) { hash ^= byte; hash *= 1099511628211ULL; }
            legacy += "CHECKSUM " + std::to_string(hash) + "\n";
            auto migrated = botf::deserialize(legacy);
            check(botf::serialize(migrated) == botf::serialize(s) && migrated.rivals.empty(), "Migration changed campaign state");
            botf::advance(s); botf::advance(migrated);
            check(botf::serialize(migrated) == botf::serialize(s), "Migrated pending orders diverged");
            legacy.replace(legacy.find("Sol"), 3, "Sun"); rejects([&] { botf::deserialize(legacy); });
        }, passed);
        run("minor contact, gift delay, pending save and membership benefit timing", [] {
            auto s = botf::generate(1999);
            botf::Minor m; m.systemId = 2; m.name = "Bynars"; s.minors.push_back(m);
            botf::Command c; c.id = 1; c.systemId = 2; c.type = botf::CommandType::SendGift;
            rejects([&] { botf::submit(s, c); });
            s.minors[0].contact[1] = true;
            botf::submit(s, c); check(s.credits == 100, "Gift charged before commit");
            botf::advance(s); check(s.credits == 60 && s.minors[0].attitude[1] == 60, "Gift delivery was immediate or ledger wrong");
            check(s.diplomacy.front().dueTurn == 2, "Gift must arrive at draft turn T plus two, not commit turn plus two");
            auto resumed = botf::deserialize(botf::serialize(s));
            botf::advance(s); botf::advance(resumed);
            check(s.minors[0].attitude[1] == 80 && s.diplomacy.empty() && botf::serialize(s) == botf::serialize(resumed), "Delayed gift failed to resume");
            c.id = 2; c.type = botf::CommandType::OfferMembership; botf::submit(s, c); botf::advance(s);
            check(s.minors[0].memberOf == 0, "Membership bypassed friendship");
            c.id = 3; c.type = botf::CommandType::OfferFriendship; botf::submit(s, c); botf::advance(s);
            c.id = 4; c.type = botf::CommandType::OfferMembership; botf::submit(s, c);
            resumed = botf::deserialize(botf::serialize(s));
            const auto joining = botf::advance(s); botf::advance(resumed);
            check(s.minors[0].memberOf == 1 && s.colonies.size() == 2 && joining.colonies.size() == 1 && joining.researchGained == 10,
                "Membership transfer or deferred production failed");
            check(botf::serialize(s) == botf::serialize(resumed), "Pending membership save diverged");
            const auto active = botf::advance(s);
            check(active.colonies.size() == 2 && active.colonies[0].economy.research == 11, "Minor research benefit inactive");
        }, passed);
        run("diplomatic funding and duplicate bids reject without mutation", [] {
            auto s = botf::generate(1999);
            botf::Minor m; m.systemId = 2; m.name = "Bynars"; m.contact[1] = true; s.minors.push_back(m);
            s.credits = 49;
            botf::Command c; c.id = 1; c.systemId = 2; c.type = botf::CommandType::SendGift;
            const auto before = botf::serialize(s); rejects([&] { botf::submit(s, c); });
            check(before == botf::serialize(s), "Unaffordable gift mutated state");
            s.credits = 50; botf::submit(s, c);
            c.id = 2; c.type = botf::CommandType::OfferFriendship; rejects([&] { botf::submit(s, c); });
            botf::cancel(s, 1); check(s.credits == 50 && s.diplomacy.empty(), "Draft cancellation lost funds");
        }, passed);
        run("concurrent memberships choose highest relations; ties reproduce after reload", [] {
            for (bool tied : {false, true}) {
                auto s = botf::generateCampaign(1999); auto& m = s.minors.front();
                m.contact[1] = m.contact[2] = true; m.friendshipTurn[1] = m.friendshipTurn[2] = 0;
                m.attitude[1] = tied ? 80 : 90; m.attitude[2] = 80;
                botf::Command c; c.id = 1; c.systemId = m.systemId; c.type = botf::CommandType::OfferMembership; botf::submit(s, c);
                auto resumed = botf::deserialize(botf::serialize(s));
                botf::advance(s); botf::advance(resumed);
                check(botf::serialize(s) == botf::serialize(resumed), "Concurrent membership RNG diverged");
                check(s.minors[0].memberOf != 0 && (tied || s.minors[0].memberOf == 1), "Membership priority wrong");
                int owners = 0; for (const auto& colony : s.colonies) if (colony.systemId == s.minors[0].systemId) ++owners;
                check(owners == 1, "Concurrent membership created duplicate ownership");
            }
        }, passed);
        run("friendship and membership relation boundaries are explicit", [] {
            for (int attitude : {49,50,79,80}) {
                auto s = botf::generate(1999); botf::Minor m; m.systemId = 2; m.name = "Bynars";
                m.contact[1] = true; m.attitude[1] = attitude; s.minors.push_back(m);
                botf::Command c; c.id = 1; c.systemId = 2; c.type = botf::CommandType::OfferFriendship;
                botf::submit(s, c); botf::advance(s);
                check((s.minors[0].friendshipTurn[1] >= 0) == (attitude >= 50), "Friendship boundary wrong");
                c.id = 2; c.type = botf::CommandType::OfferMembership; botf::submit(s, c); botf::advance(s);
                check((s.minors[0].memberOf == 1) == (attitude >= 80), "Membership boundary wrong");
            }
        }, passed);
        run("undelivered gifts refund if a rival wins membership first", [] {
            auto s = botf::generateCampaign(1999); auto& m = s.minors[0];
            m.contact[1] = m.contact[2] = true; m.attitude[2] = 80; m.friendshipTurn[2] = 0;
            botf::Command c; c.id = 1; c.systemId = m.systemId; c.type = botf::CommandType::SendGift;
            botf::submit(s, c); botf::advance(s);
            check(s.minors[0].memberOf == 2 && s.credits == 60, "Competing transfer fixture failed");
            auto resumed = botf::deserialize(botf::serialize(s));
            for (int turn = 0; turn < 2; ++turn) { botf::advance(s); botf::advance(resumed); }
            check(s.credits == 130 && s.diplomacy.empty() && s.minors[0].attitude[1] == 60, "Invalidated gift was lost or applied");
            check(botf::serialize(s) == botf::serialize(resumed), "Refund diverged after save");
        }, passed);
        run("schema five and six preserve ledgers and saved diplomatic due dates", [] {
            for (const int version : {5, 6}) {
                auto s = botf::generateCampaign(1999); s.populationEnabled = false; s.eventsEnabled = false; s.aiAggressionEnabled = false;
                s.unifiedCombat = false; // Pre-schema-12 campaigns load with the legacy combat model.
                if (version == 5) s.minors.clear();
                else {
                    s.minors[0].contact[1] = true;
                    botf::Command gift; gift.id = 1; gift.type = botf::CommandType::SendGift; gift.systemId = s.minors[0].systemId;
                    botf::submit(s, gift); botf::advance(s);
                    s.diplomacy[0].dueTurn = 3; // Date written by the earlier T+3 build must not be rewritten.
                }
                s.intelligence = 0;
                for (auto& rival : s.rivals) rival.intelligence = 0;
                auto legacy = asSchemaSeven(s); legacy.erase(legacy.rfind("CHECKSUM "));
                legacy.replace(0, legacy.find('\n') + 1, version == 5 ? "BOTF_SIM 5 prototype-empires-5\n" : "BOTF_SIM 6 prototype-diplomacy-6\n");
                legacy.erase(legacy.find("POPULATION 0\n"), std::string("POPULATION 0\n").size());
                if (version == 5) legacy.erase(legacy.find("MINORS 1 0\nDIPLOMACY 0\n"), std::string("MINORS 1 0\nDIPLOMACY 0\n").size());
                std::uint64_t hash = 14695981039346656037ULL;
                for (unsigned char byte : legacy) { hash ^= byte; hash *= 1099511628211ULL; }
                legacy += "CHECKSUM " + std::to_string(hash) + "\n";
                const auto migrated = botf::deserialize(legacy);
                check(botf::serialize(migrated) == botf::serialize(s), "Migration changed old campaign rules or due dates");
            }
        }, passed);
        run("population shortage, growth, employment and capacity boundaries", [] {
            auto change = botf::populationForecast(100, 0, 200, 100, 200);
            check(change.minimum == -10 && change.maximum == -10, "Complete shortage rounding wrong");
            check(botf::populationForecast(9, 0, 100, 10, 200).minimum == 0, "Small colony starvation floor wrong");
            check(botf::populationForecast(100, 99, 200, 100, 200).minimum == -1, "Exact square shortage boundary wrong");
            change = botf::populationForecast(60, 250, 250, 100, 200);
            check(change.minimum == 3 && change.maximum == 4 && change.upwardChances == 2000, "Growth fixed-point formula wrong");
            change = botf::populationForecast(60, 500, 250, 100, 200);
            check(change.minimum == 3 && change.upwardChances == 2000, "Food was not capped for growth");
            check(botf::populationForecast(60, 250, 250, 30, 200).maximum == 1, "Employment cap wrong");
            check(botf::populationForecast(99, 100, 100, 100, 200).maximum == 1, "Growth exceeded capacity");
            check(botf::populationForecast(100, 100, 100, 100, 200).maximum == 0, "Full colony grew");
            rejects([] { botf::populationForecast(40, -1, 100, 100, 200); });
        }, passed);
        run("population resolves atomically, frees workers after losses, and resumes RNG", [] {
            auto s = botf::generate(1999); s.populationEnabled = true;
            auto& c = s.colonies[0]; c.population = 40; c.foodWorkers = 0; c.industryWorkers = 20; c.researchWorkers = 10;
            botf::advance(s); check(s.colonies[0].population == 36 && s.colonies[0].industryWorkers == 20, "Unassigned population losses changed staffing");
            s.colonies[0].population = 30;
            botf::advance(s); check(s.colonies[0].population == 27 && s.colonies[0].industryWorkers == 17 && s.colonies[0].researchWorkers == 10, "Staffing exceeded survivors");
            s.colonies[0].population = 60; s.colonies[0].farms = 6; s.colonies[0].foodWorkers = 60;
            s.colonies[0].industryWorkers = s.colonies[0].researchWorkers = 0;
            auto resumed = botf::deserialize(botf::serialize(s));
            for (int turn = 0; turn < 30; ++turn) { botf::advance(s); botf::advance(resumed); }
            check(s.colonies[0].population == 100 && botf::serialize(s) == botf::serialize(resumed), "Growth capacity or replay failed");
        }, passed);
        run("selectable empires keep their own ledgers, knowledge and save identity", [] {
            for (int faction = 1; faction <= 5; ++faction) {
                auto s = botf::generateCampaign(1999, 5, 16, 12, 28, faction);
                check(s.playerEmpire == faction && s.id == faction && s.rivals.size() == 4, "Faction selection failed");
                const auto copy = botf::deserialize(botf::serialize(s));
                check(copy.playerEmpire == faction && botf::serialize(copy) == botf::serialize(s), "Selected faction save failed");
                for (int turn = 0; turn < 10; ++turn) botf::advance(s);
                check(s.credits > 0 && botf::empire(s, faction).id == faction, "Selected faction economy failed");
            }
        }, passed);
        run("major contact, war, fleet battle, damage and battle history survive reload", [] {
            auto s = botf::generateCampaign(1999);
            s.unifiedCombat = false; // Exercises the legacy strategic resolver kept for schema 4-11 saves.
            const auto home = s.systems.front();
            auto& enemy = s.fleets[2]; enemy.x = enemy.targetX = home.x; enemy.y = enemy.targetY = home.y;
            auto& knowledge = s.rivals[0].explored;
            const int cell = home.y * s.width + home.x;
            knowledge.push_back(cell); std::sort(knowledge.begin(), knowledge.end());
            knowledge.erase(std::unique(knowledge.begin(), knowledge.end()), knowledge.end());
            s.majorRelations[0].contact = true;
            botf::Command war; war.id = 1; war.type = botf::CommandType::DeclareWar; war.targetEmpire = 2;
            botf::submit(s, war);
            auto resumed = botf::deserialize(botf::serialize(s));
            const auto report = botf::advance(s); botf::advance(resumed);
            check(botf::serialize(s) == botf::serialize(resumed), "War battle replay diverged");
            check(report.battles.size() == 1 && s.latestBattles.size() == 1 && report.battles[0].firstOwner == 1,
                "Battle was not recorded structurally");
            check(report.battles[0].rounds.size() >= 2 && report.battles[0].rounds.size() <= 65 &&
                report.battles[0].participants.size() == 3 &&
                s.fleets[0].hull == 3 && s.fleets[2].hull == 3 &&
                report.battles[0].retreatedFleetIds.size() == 2,
                "Bounded simultaneous tactical rounds or persistent hull wrong: rounds=" +
                std::to_string(report.battles[0].rounds.size()) + " participants=" +
                std::to_string(report.battles[0].participants.size()) + " hulls=" +
                std::to_string(s.fleets[0].hull) + "," + std::to_string(s.fleets[2].hull) +
                " retreats=" + std::to_string(report.battles[0].retreatedFleetIds.size()));
            check(s.majorRelations[0].atWar, "War did not persist");
            const auto reloaded = botf::deserialize(botf::serialize(s));
            check(reloaded.latestBattles[0].damage.size() == 3 &&
                reloaded.latestBattles[0].rounds.size() == report.battles[0].rounds.size() &&
                reloaded.fleets[0].hull == 3,
                "Battle history or damage lost on save");
            const auto following = botf::advance(s);
            check(following.battles.empty() && s.latestBattles[0].rounds.size() == report.battles[0].rounds.size(),
                "A resolved encounter spilled into the next strategic turn or lost history");
        }, passed);
        run("tactical loss lists destroyed ships and keeps their source snapshots", [] {
            auto s = botf::generateCampaign(1999); s.aiAggressionEnabled = false;
            s.unifiedCombat = false; // Legacy resolver fixture (schema 4-11 saves).
            const auto sol = s.systems.front();
            s.fleets[0].hull = 3;
            s.fleets[1].hull = 3;
            s.fleets[2].x = s.fleets[2].targetX = sol.x;
            s.fleets[2].y = s.fleets[2].targetY = sol.y;
            auto& known = s.rivals[0].explored;
            known.push_back(sol.y * s.width + sol.x);
            std::sort(known.begin(), known.end()); known.erase(std::unique(known.begin(), known.end()), known.end());
            s.majorRelations[0].contact = true; s.majorRelations[0].atWar = true;
            const auto battle = botf::advance(s).battles.front();
            check(battle.winnerOwner == 2 && battle.lostFleetIds.size() == 2 &&
                battle.participants.size() == 3 && battle.rounds.size() >= 2 &&
                std::none_of(s.fleets.begin(), s.fleets.end(), [](const botf::Fleet& fleet) { return fleet.ownerId == 1; }),
                "Destroyed ships or tactical winner did not reconcile");
            const auto saved = botf::deserialize(botf::serialize(s));
            check(saved.latestBattles[0].participants[0].initialHull == 3 &&
                saved.latestBattles[0].rounds.size() == battle.rounds.size(),
                "Destroyed ship snapshot did not survive save");
        }, passed);
        run("battle history rejects mismatched sides, outcomes and tactical damage", [] {
            auto s = botf::generateCampaign(1999); s.aiAggressionEnabled = false;
            const auto sol = s.systems.front();
            s.fleets[2].x = s.fleets[2].targetX = sol.x;
            s.fleets[2].y = s.fleets[2].targetY = sol.y;
            auto& known = s.rivals[0].explored;
            known.push_back(sol.y * s.width + sol.x);
            std::sort(known.begin(), known.end()); known.erase(std::unique(known.begin(), known.end()), known.end());
            s.majorRelations[0].contact = true; s.majorRelations[0].atWar = true;
            botf::advance(s);
            check(!s.latestBattles.empty() && !s.latestBattles[0].rounds.empty(), "Battle fixture missing timeline");
            const auto baseline = botf::serialize(s);
            check(botf::serialize(botf::deserialize(baseline)) == baseline, "Valid battle history changed on reload");
            auto altered = s;
            altered.latestBattles[0].participants[0].ownerId = 2;
            rejects([&] { botf::serialize(altered); });
            altered = s;
            altered.latestBattles[0].firstFleetIds[0] = altered.latestBattles[0].secondFleetIds[0];
            rejects([&] { botf::serialize(altered); });
            altered = s;
            altered.latestBattles[0].retreatedFleetIds.push_back(altered.latestBattles[0].firstFleetIds[0]);
            rejects([&] { botf::serialize(altered); });
            altered = s;
            altered.latestBattles[0].damage[0].fleetId = s.nextFleetId;
            rejects([&] { botf::serialize(altered); });
            altered = s;
            altered.latestBattles[0].rounds.front().damage.push_back({s.nextFleetId, 1, 0});
            rejects([&] { botf::serialize(altered); });
        }, passed);
        run("strategic retreat replaces an old route and reaches home before fire", [] {
            auto s = botf::generateCampaign(1999);
            const auto home = s.systems.front();
            const int x = home.x + (home.x + 1 < s.width ? 1 : -1), y = home.y;
            s.fleets[0].x = s.fleets[0].targetX = x; s.fleets[0].y = s.fleets[0].targetY = y;
            s.fleets[2].x = s.fleets[2].targetX = x; s.fleets[2].y = s.fleets[2].targetY = y;
            for (auto* knowledge : {&s.explored, &s.rivals[0].explored}) {
                knowledge->push_back(y * s.width + x);
                std::sort(knowledge->begin(), knowledge->end()); knowledge->erase(std::unique(knowledge->begin(), knowledge->end()), knowledge->end());
            }
            s.rivals[0].explored.clear();
            for (int cell = 0; cell < s.width * s.height; ++cell) s.rivals[0].explored.push_back(cell);
            s.majorRelations[0].contact = true; s.majorRelations[0].atWar = true;
            botf::Command retreat; retreat.id = 1; retreat.type = botf::CommandType::RetreatFleet; retreat.fleetId = 1;
            botf::submit(s, retreat);
            const auto draft = botf::preview(s);
            check(draft.fleets.front().targetX == home.x && draft.fleets.front().targetY == home.y,
                "Retreat kept the old fleet route");
            const auto report = botf::advance(s);
            check(report.battles.empty() && s.fleets[0].x == home.x && s.fleets[0].hull == 12 &&
                !s.fleets[0].retreat, "Retreat did not preserve ship or clear on arrival");
        }, passed);
        run("contested-home withdrawal is recorded before tactical fire", [] {
            auto s = botf::generateCampaign(1999); s.aiAggressionEnabled = false;
            s.unifiedCombat = false; // Legacy free withdrawal; unified combat disengages under fire instead.
            const auto sol = s.systems.front();
            s.fleets[2].x = s.fleets[2].targetX = sol.x;
            s.fleets[2].y = s.fleets[2].targetY = sol.y;
            auto& known = s.rivals[0].explored;
            known.push_back(sol.y * s.width + sol.x);
            std::sort(known.begin(), known.end()); known.erase(std::unique(known.begin(), known.end()), known.end());
            s.majorRelations[0].contact = true; s.majorRelations[0].atWar = true;
            botf::Command retreat; retreat.id = 1; retreat.type = botf::CommandType::RetreatFleet; retreat.fleetId = 1;
            botf::submit(s, retreat);
            const auto battle = botf::advance(s).battles.front();
            check(!battle.rounds.empty() && battle.rounds.front().index == 1 &&
                battle.rounds.front().retreatedFleetIds == std::vector<int>{1} && s.fleets.front().hull == 12,
                "Contested retreat or opening timeline failed");
        }, passed);
        run("trade routes settle, then war invalidates them without an income leak", [] {
            auto s = botf::generateCampaign(1999);
            s.majorRelations[0].contact = true;
            const int foreignSystem = s.colonies.back().systemId;
            botf::Command route; route.id = 1; route.type = botf::CommandType::EstablishTradeRoute;
            route.systemId = 1; route.targetSystemId = foreignSystem; route.targetEmpire = 2;
            botf::submit(s, route);
            check(s.tradeRoutes.empty() && botf::preview(s).tradeRoutes.size() == 1, "Trade order applied early");
            const auto traded = botf::advance(s);
            check(traded.tradeIncome == 9 && s.tradeRoutes.size() == 1, "Trade income did not reconcile");
            botf::Command war; war.id = 2; war.type = botf::CommandType::DeclareWar; war.targetEmpire = 2;
            botf::submit(s, war);
            const auto stopped = botf::advance(s);
            check(s.tradeRoutes.empty() && stopped.tradeIncome == 0, "War failed to sever trade");
            botf::Command peace; peace.id = 3; peace.type = botf::CommandType::OfferPeace; peace.targetEmpire = 2;
            botf::submit(s, peace); botf::advance(s);
            check(!s.majorRelations[0].atWar, "Peace transition failed");
        }, passed);
        run("intelligence operations spend stockpile and survive pending save", [] {
            auto s = botf::generateCampaign(1999); s.intelligence = 100;
            s.majorRelations[0].contact = true; s.majorRelations[0].atWar = true;
            const auto target = s.colonies.back().systemId;
            const auto location = s.systems[target - 1];
            s.explored.push_back(location.y * s.width + location.x);
            std::sort(s.explored.begin(), s.explored.end()); s.explored.erase(std::unique(s.explored.begin(), s.explored.end()), s.explored.end());
            auto& foreignColony = s.colonies.back(); foreignColony.queue.push_back({botf::Building::Factory, 30});
            botf::Command intel; intel.id = 1; intel.type = botf::CommandType::GatherIntelligence; intel.targetEmpire = 2;
            botf::submit(s, intel);
            intel.id = 2; intel.type = botf::CommandType::Sabotage; intel.targetSystemId = target;
            botf::submit(s, intel);
            auto draft = botf::preview(s);
            check(draft.intelligence == 30 && draft.intelReports.size() == 2 && draft.colonies.back().queue[0].work == 5,
                "Intelligence draft did not apply expected effects");
            auto resumed = botf::deserialize(botf::serialize(s));
            botf::advance(s); botf::advance(resumed);
            check(botf::serialize(s) == botf::serialize(resumed) && s.intelReports.size() == 2,
                "Intelligence continuation diverged");
        }, passed);
        run("ground assault transfers ownership, consumes transport and ends conquered campaign", [] {
            auto base = botf::generateCampaign(1999); base.aiAggressionEnabled = false;
            const int targetSystem = base.colonies.back().systemId;
            const auto target = base.systems[targetSystem - 1];
            base.fleets.erase(base.fleets.begin() + 2, base.fleets.end()); // Clear orbital defenders for the ground fixture.
            base.fleets.push_back({base.nextFleetId++, 1, target.x, target.y, target.x, target.y,
                false, false, botf::Building::Transport, 14});
            base.explored.push_back(target.y * base.width + target.x);
            std::sort(base.explored.begin(), base.explored.end()); base.explored.erase(std::unique(base.explored.begin(), base.explored.end()), base.explored.end());
            base.majorRelations[0].contact = true; base.majorRelations[0].atWar = true;
            botf::Command invade; invade.id = 1; invade.type = botf::CommandType::Invade;
            invade.fleetId = base.fleets.back().id; invade.systemId = targetSystem;
            botf::submit(base, invade);
            check(botf::preview(base).fleets.back().invadeSystemId == targetSystem, "Assault draft not visible");
            bool captured = false;
            for (std::uint64_t rng = 0; rng < 100 && !captured; ++rng) {
                auto s = base; s.rngState = rng;
                auto resumed = botf::deserialize(botf::serialize(s));
                const auto report = botf::advance(s); botf::advance(resumed);
                check(botf::serialize(s) == botf::serialize(resumed), "Invasion replay diverged");
                check(std::none_of(s.fleets.begin(), s.fleets.end(), [&](const botf::Fleet& f) { return f.id == invade.fleetId; }),
                    "Transport was not consumed");
                if (botf::colonyAt(s, targetSystem)->ownerId == 1) {
                    captured = true;
                    check(s.outcome == botf::CampaignOutcome::Victory && report.outcome == s.outcome &&
                        s.winnerEmpire == 1, "Elimination victory failed");
                    check(botf::colonyAt(s, targetSystem)->morale < 50 && report.colonies.size() == 1,
                        "Conquest morale or deferred ownership production wrong");
                    rejects([&] { botf::advance(s); });
                }
            }
            check(captured, "No assault succeeded across deterministic draws");
        }, passed);
        run("defeat and configurable events resolve deterministically", [] {
            auto defeated = botf::generateCampaign(1999); defeated.aiAggressionEnabled = false;
            defeated.colonies.front().ownerId = 2;
            const auto home = defeated.systems.front();
            auto& rivalKnowledge = defeated.rivals[0].explored;
            rivalKnowledge.push_back(home.y * defeated.width + home.x);
            std::sort(rivalKnowledge.begin(), rivalKnowledge.end());
            rivalKnowledge.erase(std::unique(rivalKnowledge.begin(), rivalKnowledge.end()), rivalKnowledge.end());
            botf::advance(defeated);
            check(defeated.outcome == botf::CampaignOutcome::Defeat && defeated.winnerEmpire == 2,
                "Lost home colony did not end campaign");
            auto active = botf::generateCampaign(1999); active.aiAggressionEnabled = false;
            auto paused = active; paused.eventsEnabled = false;
            auto saved = botf::deserialize(botf::serialize(active));
            for (int turn = 0; turn < 10; ++turn) {
                botf::advance(active); botf::advance(saved); botf::advance(paused);
            }
            check(botf::serialize(active) == botf::serialize(saved) && active.lastRandomEventTurn == 10 &&
                paused.lastRandomEventTurn == 0, "Event schedule or toggle replay failed");
        }, passed);
        run("multi-empire defeat records no arbitrary overall winner", [] {
            auto s = botf::generateCampaign(1999, 3); s.aiAggressionEnabled = false;
            s.colonies.front().ownerId = 2;
            const auto home = s.systems.front();
            auto& known = s.rivals[0].explored;
            known.push_back(home.y * s.width + home.x);
            std::sort(known.begin(), known.end()); known.erase(std::unique(known.begin(), known.end()), known.end());
            auto resumed = botf::deserialize(botf::serialize(s));
            botf::advance(s); botf::advance(resumed);
            check(s.outcome == botf::CampaignOutcome::Defeat && s.winnerEmpire == 0 &&
                botf::serialize(s) == botf::serialize(resumed), "Defeat with surviving rivals failed validation or replay");
        }, passed);
        run("aggressive rival declares war only after contact and threshold", [] {
            // Lane B: war is personality/situation driven (trust, strength, cool-downs), not
            // unconditional. A distrustful Klingon neighbour escalates once the early peace ends.
            // Rivalry update (16 Nov 2026): early peace runs to turn 30 and each personality adds its patience (Klingon: only the pair's seeded 0..8).
            auto s = botf::generateCampaign(1999); s.turn = 24;
            s.majorRelations[0].contact = true;
            s.majorRelations[0].trust = 20;
            for (int i = 0; i < 5; ++i) { botf::advance(s); check(!s.majorRelations[0].atWar, "Klingon rival declared war inside the early peace"); }
            for (int i = 0; i < 12 && !s.majorRelations[0].atWar; ++i) botf::advance(s);
            check(s.majorRelations[0].atWar && s.turn >= 30 && s.turn <= 42, "Klingon rival did not escalate after contact");
            auto cooled = botf::generateCampaign(1999); cooled.turn = 40;
            cooled.majorRelations[0].contact = true; cooled.majorRelations[0].trust = 20;
            cooled.majorRelations[0].warStartTurn = 35; // Peace signed five turns ago
            botf::advance(cooled);
            check(!cooled.majorRelations[0].atWar, "Klingon rival ignored the post-peace cool-down");
            auto peaceful = botf::generateCampaign(1999); peaceful.turn = 24;
            peaceful.aiAggressionEnabled = false; peaceful.majorRelations[0].contact = true;
            botf::advance(peaceful);
            check(!peaceful.majorRelations[0].atWar, "Disabled AI aggression still declared war");
        }, passed);
        run("reference campaigns use sourced starting hulls and six technology ladders", [] {
            for (int faction = 1; faction <= 5; ++faction) {
                auto s = botf::generateReferenceCampaign(1999, 5, 16, 12, 28, faction);
                s.aiAggressionEnabled = false;
                check(s.ruleset == botf::Ruleset::Reference && s.playerEmpire == faction && s.credits == 1000,
                    "Reference faction setup failed");
                for (const auto& fleet : s.fleets) {
                    check(fleet.referenceShipId >= 0 && fleet.hull == botf::fleetMaxHull(fleet), "Starting ship lacks source identity");
                }
                check(botf::technologyName(s, 0, 1) == "Hydroponics" && botf::researchCost(s, 3, 2) == 100,
                    "Sourced research row mismatch");
                auto saved = botf::deserialize(botf::serialize(s));
                for (int turn = 0; turn < 10; ++turn) { botf::advance(s); botf::advance(saved); }
                check(botf::serialize(s) == botf::serialize(saved), "Reference campaign continuation diverged");
            }
        }, passed);
        run("reference production lists sourced ships with blockers and preserves their IDs", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            const auto choices = botf::constructionChoices(s, 1);
            const auto scout = std::find_if(choices.begin(), choices.end(), [](const botf::ConstructionChoice& choice) {
                return choice.product == botf::Building::Scout && choice.referenceId >= 0 && choice.blocker.empty();
            });
            const auto advanced = std::find_if(choices.begin(), choices.end(), [](const botf::ConstructionChoice& choice) {
                return choice.referenceId >= 0 && !choice.blocker.empty();
            });
            check(scout != choices.end() && scout->industryCost > 100 && advanced != choices.end(),
                "Reference ship roster or tech blockers missing");
            botf::Command build; build.id = 1; build.type = botf::CommandType::EnqueueBuilding; build.systemId = 1;
            build.building = scout->product; build.referenceId = scout->referenceId;
            botf::submit(s, build);
            auto draft = botf::preview(s);
            check(draft.colonies.front().queue.front().referenceId == scout->referenceId,
                "Source ship ID lost from construction draft");
            auto resumed = botf::deserialize(botf::serialize(s));
            bool launched = false;
            for (int turn = 0; turn < 60 && !launched; ++turn) {
                botf::advance(s); botf::advance(resumed);
                launched = std::any_of(s.fleets.begin(), s.fleets.end(), [&](const botf::Fleet& f) {
                    return f.ownerId == 1 && f.referenceShipId == scout->referenceId && f.id >= 5;
                });
            }
            check(launched && botf::serialize(s) == botf::serialize(resumed), "Sourced ship did not complete or resume identically");
        }, passed);
        run("schema eight campaigns migrate into prototype rules without reinterpreting projects", [] {
            auto s = botf::generateCampaign(1999); s.aiAggressionEnabled = false;
            s.unifiedCombat = false; // Pre-schema-12 campaigns load with the legacy combat model.
            botf::submit(s, enqueue(1, botf::Building::Factory)); botf::advance(s);
            auto old = asSchemaEight(s);
            const auto restored = botf::deserialize(old);
            check(restored.ruleset == botf::Ruleset::Prototype && botf::serialize(restored) == botf::serialize(s),
                "Schema-eight migration changed campaign state");
        }, passed);
        run("schema nine reference saves keep source IDs but preserve legacy rule behavior", [] {
            auto current = botf::generateReferenceCampaign(1999);
            current.advancedRulesEnabled = false;
            for (auto& fleet : current.fleets) fleet.shields = 0;
            const auto old = asSchemaNine(current);
            const auto migrated = botf::deserialize(old);
            check(migrated.ruleset == botf::Ruleset::Reference && !migrated.advancedRulesEnabled &&
                migrated.fleets[0].referenceShipId == current.fleets[0].referenceShipId &&
                migrated.fleets[0].shields == 0 && migrated.majorRelations[0].trust == 50,
                "Schema-nine migration reinterpreted reference content or advanced rules");
            auto continued = botf::deserialize(botf::serialize(migrated));
            botf::advance(continued);
            check(continued.fleets[0].shields == 0, "Legacy reference ship unexpectedly gained advanced shields");
        }, passed);
        run("advanced campaigns persist sourced shields and tactical stance orders", [] {
            auto s = botf::generateReferenceCampaign(1999);
            check(s.advancedRulesEnabled && s.fleets[0].shields == botf::fleetMaxShields(s.fleets[0]) &&
                s.fleets[0].shields > 0, "Reference shield initialization failed");
            botf::Command stance; stance.id = 1; stance.type = botf::CommandType::SetTacticalStance;
            stance.fleetId = s.fleets[0].id; stance.stance = botf::TacticalStance::Aggressive;
            botf::submit(s, stance);
            check(botf::preview(s).fleets[0].stance == botf::TacticalStance::Aggressive &&
                s.fleets[0].stance == botf::TacticalStance::Balanced, "Stance draft was not deferred");
            auto resumed = botf::deserialize(botf::serialize(s));
            botf::advance(s); botf::advance(resumed);
            check(s.fleets[0].stance == botf::TacticalStance::Aggressive &&
                botf::serialize(s) == botf::serialize(resumed), "Stance continuation diverged");
        }, passed);
        run("advanced battle records shield depletion over bounded tactical rounds", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            const auto sol = s.systems.front();
            auto& enemy = s.fleets[2]; enemy.x = enemy.targetX = sol.x; enemy.y = enemy.targetY = sol.y;
            auto& known = s.rivals[0].explored; known.push_back(sol.y * s.width + sol.x);
            std::sort(known.begin(), known.end()); known.erase(std::unique(known.begin(), known.end()), known.end());
            s.majorRelations[0].contact = true;
            botf::Command war; war.id = 1; war.type = botf::CommandType::DeclareWar; war.targetEmpire = 2;
            botf::submit(s, war);
            const auto report = botf::advance(s);
            check(report.battles.size() == 1 && report.battles[0].rounds.size() >= 2 &&
                report.battles[0].participants[0].initialShields > 0,
                "Advanced battle did not record multiple shielded rounds");
            bool depleted = false;
            for (const auto& round : report.battles[0].rounds)
                for (const auto& hit : round.damage) if (hit.shieldAfter < hit.shieldBefore) depleted = true;
            check(depleted && botf::deserialize(botf::serialize(s)).latestBattles[0].rounds.size() ==
                report.battles[0].rounds.size(), "Shield hits were not persisted");
        }, passed);
        run("major peace and trade proposals resolve after a decision turn", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            s.majorRelations[0].contact = true;
            botf::Command route; route.id = 1; route.type = botf::CommandType::EstablishTradeRoute;
            route.systemId = 1; route.targetSystemId = s.colonies.back().systemId; route.targetEmpire = 2;
            rejects([&] { botf::submit(s, route); });
            botf::Command treaty; treaty.id = 1; treaty.type = botf::CommandType::ProposeTradeTreaty; treaty.targetEmpire = 2;
            botf::submit(s, treaty); botf::advance(s);
            check(!s.majorRelations[0].tradeTreaty && s.pendingMajorOffers.size() == 1,
                "Trade proposal resolved without response delay");
            auto resumed = botf::deserialize(botf::serialize(s));
            botf::advance(s); botf::advance(resumed);
            check(s.majorRelations[0].tradeTreaty && s.pendingMajorOffers.empty() &&
                botf::serialize(s) == botf::serialize(resumed), "Accepted trade offer failed after reload");
            route.id = 2; botf::submit(s, route); botf::advance(s);
            check(s.tradeRoutes.size() == 1, "Accepted treaty did not authorize route");
            botf::Command war; war.id = 3; war.type = botf::CommandType::DeclareWar; war.targetEmpire = 2;
            botf::submit(s, war); botf::advance(s);
            botf::Command peace; peace.id = 4; peace.type = botf::CommandType::OfferPeace; peace.targetEmpire = 2;
            botf::submit(s, peace); botf::advance(s);
            check(s.majorRelations[0].atWar && s.pendingMajorOffers.size() == 1, "Peace bypassed response delay");
            botf::advance(s);
            check(!s.majorRelations[0].atWar && s.pendingMajorOffers.empty(), "Peace offer did not resolve");
        }, passed);
        run("advanced retreat preserves home colony orders and recovers hull at powered shipyard", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            const auto home = s.systems.front();
            auto& fleet = s.fleets[0];
            const int maxH = botf::fleetMaxHull(fleet);
            fleet.hull = maxH / 2;
            const int startX = std::min(s.width - 1, home.x + 2);
            fleet.x = fleet.targetX = startX;
            fleet.y = fleet.targetY = home.y;
            for (auto* k : {&s.explored, &s.rivals[0].explored}) {
                k->push_back(home.y * s.width + startX);
                k->push_back(home.y * s.width + (startX - 1));
                std::sort(k->begin(), k->end()); k->erase(std::unique(k->begin(), k->end()), k->end());
            }
            botf::Command cmd; cmd.id = 1; cmd.type = botf::CommandType::RetreatFleet; cmd.fleetId = fleet.id;
            botf::submit(s, cmd);
            check(botf::preview(s).fleets[0].targetX == home.x && botf::preview(s).fleets[0].retreat,
                "Retreat preview did not target home");
            botf::advance(s);
            if (s.fleets[0].x != home.x || s.fleets[0].y != home.y) {
                check(s.fleets[0].retreat && s.fleets[0].targetX == home.x && s.fleets[0].targetY == home.y,
                    "Retreat order was wiped during transit");
                check(s.fleets[0].hull == maxH / 2, "Ship received free repair in transit");
                botf::advance(s);
            }
            check(s.fleets[0].x == home.x && s.fleets[0].y == home.y && !s.fleets[0].retreat,
                "Retreat did not reach home or clear on arrival");
            const int expectedRepair = std::max(1, maxH / 10);
            check(s.fleets[0].hull == maxH / 2 + expectedRepair, "Ship at powered shipyard did not repair hull");
            const int hullBeforeBattle = s.fleets[0].hull;
            s.fleets[2].x = s.fleets[2].targetX = home.x;
            s.fleets[2].y = s.fleets[2].targetY = home.y;
            s.rivals[0].explored.push_back(home.y * s.width + home.x);
            std::sort(s.rivals[0].explored.begin(), s.rivals[0].explored.end());
            s.rivals[0].explored.erase(std::unique(s.rivals[0].explored.begin(), s.rivals[0].explored.end()), s.rivals[0].explored.end());
            s.majorRelations[0].contact = true; s.majorRelations[0].atWar = true;
            botf::advance(s);
            check(s.fleets[0].hull <= hullBeforeBattle, "Ship under attack received free repair during battle");
        }, passed);
        run("strategic AI avoids peeking at hidden enemy fleets and avoids suicidal attacks", [] {
            auto s = botf::generateReferenceCampaign(1999);
            // The "hidden" corner sits next to the Klingon home, so the scout is visible and a
            // skirmish follows; the first check only holds because the legacy resolver's
            // withdrawal clears attack targets. Keep this AI fixture on the legacy resolver.
            s.unifiedCombat = false;
            s.majorRelations[0].contact = true; s.majorRelations[0].atWar = true;
            const int hiddenX = s.width - 1, hiddenY = s.height - 1;
            s.fleets[0].x = s.fleets[0].targetX = hiddenX;
            s.fleets[0].y = s.fleets[0].targetY = hiddenY;
            s.explored.push_back(hiddenY * s.width + hiddenX);
            std::sort(s.explored.begin(), s.explored.end());
            s.explored.erase(std::unique(s.explored.begin(), s.explored.end()), s.explored.end());
            std::erase_if(s.rivals[0].explored, [&](int cell) {
                return cell == hiddenY * s.width + hiddenX;
            });
            botf::advance(s);
            for (const auto& f : s.fleets) {
                if (f.ownerId == 2) check(f.attackTarget != s.fleets[0].id, "Rival AI targeted hidden enemy fleet without visibility");
            }
            auto rivalScoutIt = std::find_if(s.fleets.begin(), s.fleets.end(), [](const botf::Fleet& f) { return f.ownerId == 2; });
            check(rivalScoutIt != s.fleets.end(), "Rival fleet missing");
            const int scoutId = rivalScoutIt->id;
            const int scoutOriginX = rivalScoutIt->x, scoutOriginY = rivalScoutIt->y;
            const int visibleX = scoutOriginX, visibleY = scoutOriginY + (scoutOriginY + 1 < s.height ? 1 : -1);
            s.fleets[0].x = s.fleets[0].targetX = visibleX;
            s.fleets[0].y = s.fleets[0].targetY = visibleY;
            s.fleets[0].hull = botf::fleetMaxHull(s.fleets[0]);
            s.fleets.push_back({s.nextFleetId++, 1, visibleX, visibleY, visibleX, visibleY,
                false, false, botf::Building::Destroyer, botf::shipMaxHull(botf::Building::Destroyer)});
            s.fleets.push_back({s.nextFleetId++, 1, visibleX, visibleY, visibleX, visibleY,
                false, false, botf::Building::Destroyer, botf::shipMaxHull(botf::Building::Destroyer)});
            s.explored.push_back(visibleY * s.width + visibleX);
            std::sort(s.explored.begin(), s.explored.end());
            s.explored.erase(std::unique(s.explored.begin(), s.explored.end()), s.explored.end());
            s.rivals[0].explored.push_back(visibleY * s.width + visibleX);
            std::sort(s.rivals[0].explored.begin(), s.rivals[0].explored.end());
            s.rivals[0].explored.erase(std::unique(s.rivals[0].explored.begin(), s.rivals[0].explored.end()), s.rivals[0].explored.end());
            const auto report = botf::advance(s);
            const auto rivalAfter = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const botf::Fleet& f) { return f.id == scoutId; });
            check(rivalAfter != s.fleets.end(), "Rival scout must survive rather than dying in suicidal attack");
            check(report.battles.empty(), "Suicidal battle occurred despite superior force");
            check(rivalAfter->x != visibleX || rivalAfter->y != visibleY, "Rival scout moved into suicidal sector");
            check(rivalAfter->targetX != visibleX || rivalAfter->targetY != visibleY, "Rival scout targeted suicidal sector");
        }, passed);
        run("damaged rival stays at safe powered shipyard until meaningful recovery", [] {
            auto s = botf::generateReferenceCampaign(1999, 2); s.aiAggressionEnabled = false;
            auto& scout = s.fleets[2];
            const int id = s.colonies.back().systemId;
            const auto home = s.systems[id - 1];
            scout.x = scout.targetX = home.x; scout.y = scout.targetY = home.y;
            scout.hull = std::max(1, botf::fleetMaxHull(scout) / 4);
            int nx = -1, ny = -1;
            for (int dy = -1; dy <= 1 && nx < 0; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                const int x = home.x + dx, y = home.y + dy;
                if ((!dx && !dy) || x < 0 || y < 0 || x >= s.width || y >= s.height) continue;
                if (std::none_of(s.systems.begin(), s.systems.end(), [&](const botf::System& sys) { return sys.x == x && sys.y == y; })) {
                    nx = x; ny = y; break;
                }
            }
            check(nx >= 0, "No adjacent open cell");
            const int extraId = static_cast<int>(s.systems.size()) + 1;
            s.systems.push_back({extraId, nx, ny, "Review Rival Yard"});
            botf::Colony second; second.systemId = extraId; second.ownerId = 2; second.shipyards = 1;
            s.colonies.push_back(second);
            s.rivals[0].explored.push_back(ny * s.width + nx);
            std::sort(s.rivals[0].explored.begin(), s.rivals[0].explored.end());
            s.rivals[0].explored.erase(std::unique(s.rivals[0].explored.begin(), s.rivals[0].explored.end()), s.rivals[0].explored.end());
            s.majorRelations[0].contact = true; s.majorRelations[0].atWar = true;
            const int before = scout.hull;
            s.aiAggressionEnabled = true;
            botf::advance(s);
            const auto& after = s.fleets[2];
            check(after.x == home.x && after.y == home.y, "Damaged ship left powered yard for another colony or exploration");
            check(after.hull > before, "Damaged ship at powered shipyard did not recover hull");
        }, passed);
        run("hostile colony fallback does not charge suicidal defended coordinates", [] {
            auto s = botf::generateReferenceCampaign(1999, 2); s.aiAggressionEnabled = false;
            const auto rivalHome = s.systems[s.colonies.back().systemId - 1];
            auto& scout = s.fleets[2];
            const int scoutId = scout.id;
            scout.x = scout.targetX = rivalHome.x; scout.y = scout.targetY = rivalHome.y;
            int nx = -1, ny = -1;
            for (int dy = -1; dy <= 1 && nx < 0; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                const int x = rivalHome.x + dx, y = rivalHome.y + dy;
                if ((!dx && !dy) || x < 0 || y < 0 || x >= s.width || y >= s.height) continue;
                if (std::none_of(s.systems.begin(), s.systems.end(), [&](const botf::System& sys) { return sys.x == x && sys.y == y; })) {
                    nx = x; ny = y; break;
                }
            }
            check(nx >= 0, "No adjacent open cell");
            const int extraId = static_cast<int>(s.systems.size()) + 1;
            s.systems.push_back({extraId, nx, ny, "Review Enemy Colony"});
            botf::Colony enemy; enemy.systemId = extraId; enemy.ownerId = 1; enemy.shipyards = 1;
            s.colonies.push_back(enemy);
            s.rivals[0].explored.push_back(ny * s.width + nx);
            std::sort(s.rivals[0].explored.begin(), s.rivals[0].explored.end());
            s.rivals[0].explored.erase(std::unique(s.rivals[0].explored.begin(), s.rivals[0].explored.end()), s.rivals[0].explored.end());
            s.explored.push_back(ny * s.width + nx);
            std::sort(s.explored.begin(), s.explored.end());
            s.explored.erase(std::unique(s.explored.begin(), s.explored.end()), s.explored.end());
            for (int i = 0; i < 3; ++i) {
                botf::Fleet destroyer{s.nextFleetId++, 1, nx, ny, nx, ny,
                    false, false, botf::Building::Destroyer, botf::shipMaxHull(botf::Building::Destroyer)};
                s.fleets.push_back(destroyer);
            }
            s.majorRelations[0].contact = true; s.majorRelations[0].atWar = true;
            s.aiAggressionEnabled = true;
            const auto report = botf::advance(s);
            const auto after = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const botf::Fleet& f) { return f.id == scoutId; });
            check(after != s.fleets.end(), "Rival scout died in colony fallback charge");
            check(report.battles.empty(), report.battles.empty() ? "" : ("Battle occurred at " + std::to_string(report.battles[0].x) + "," + std::to_string(report.battles[0].y) + " first=" + std::to_string(report.battles[0].firstOwner) + " second=" + std::to_string(report.battles[0].secondOwner)));
            check(after->x != nx || after->y != ny, "Rival scout charged into defended enemy colony");
            check(after->targetX != nx || after->targetY != ny, "Rival scout targeted defended enemy colony");
        }, passed);
        run("transports prioritize reachable hostile colonies over dogfighting and invade when clear", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            const auto targetSystem = s.systems.front();
            botf::Fleet transport{s.nextFleetId++, 2, targetSystem.x, targetSystem.y, targetSystem.x, targetSystem.y,
                false, false, botf::Building::Transport, 14};
            s.fleets.push_back(transport);
            s.rivals[0].explored.push_back(targetSystem.y * s.width + targetSystem.x);
            std::sort(s.rivals[0].explored.begin(), s.rivals[0].explored.end());
            s.rivals[0].explored.erase(std::unique(s.rivals[0].explored.begin(), s.rivals[0].explored.end()), s.rivals[0].explored.end());
            s.majorRelations[0].contact = true; s.majorRelations[0].atWar = true;
            std::erase_if(s.fleets, [&](const botf::Fleet& f) {
                return f.ownerId == 1 && f.x == targetSystem.x && f.y == targetSystem.y;
            });
            const int farX = (targetSystem.x + 2) % s.width;
            s.fleets.push_back({s.nextFleetId++, 1, farX, targetSystem.y, farX, targetSystem.y,
                false, false, botf::Building::Scout, 12});
            s.explored.push_back(targetSystem.y * s.width + farX);
            std::sort(s.explored.begin(), s.explored.end());
            s.explored.erase(std::unique(s.explored.begin(), s.explored.end()), s.explored.end());
            s.rivals[0].explored.push_back(targetSystem.y * s.width + farX);
            std::sort(s.rivals[0].explored.begin(), s.rivals[0].explored.end());
            s.rivals[0].explored.erase(std::unique(s.rivals[0].explored.begin(), s.rivals[0].explored.end()), s.rivals[0].explored.end());
            s.aiAggressionEnabled = true;
            const auto report = botf::advance(s);
            check(!report.invasions.empty() && report.invasions[0].attackerOwner == 2,
                "Transport failed to execute invasion when defensive obstacles cleared");
        }, passed);
        run("campaign save reload preserves retreat state, repair progress and tactical orders", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            const auto home = s.systems.front();
            s.fleets[0].hull = 10;
            s.fleets[0].targetX = home.x; s.fleets[0].targetY = home.y;
            s.fleets[0].retreat = true;
            s.fleets[0].stance = botf::TacticalStance::Defensive;
            auto saved = botf::serialize(s);
            auto resumed = botf::deserialize(saved);
            check(botf::serialize(s) == botf::serialize(resumed), "State diverged on serialization");
            check(resumed.fleets[0].retreat && resumed.fleets[0].hull == 10 &&
                resumed.fleets[0].stance == botf::TacticalStance::Defensive, "Restored state lost retreat or stance");
            botf::advance(s); botf::advance(resumed);
            check(botf::serialize(s) == botf::serialize(resumed), "Advanced state diverged after reload");
        }, passed);
        run("deterministic system planets, classes, capacities, and Sol canonical layout", [] {
            auto s1 = botf::generate(42); s1.campaignExpansionEnabled = true;
            auto s2 = botf::generate(42); s2.campaignExpansionEnabled = true;
            check(botf::serialize(s1) == botf::serialize(s2), "Planets must be deterministic for same seed");
            auto diff = botf::generate(999); diff.campaignExpansionEnabled = true;
            bool differ = false;
            for (std::size_t i = 1; i < s1.systems.size() && i < diff.systems.size(); ++i) {
                if (s1.systems[i].planets.size() != diff.systems[i].planets.size()) { differ = true; break; }
                for (std::size_t p = 0; p < s1.systems[i].planets.size(); ++p) {
                    if (s1.systems[i].planets[p].name != diff.systems[i].planets[p].name ||
                        s1.systems[i].planets[p].planetClass != diff.systems[i].planets[p].planetClass) {
                        differ = true; break;
                    }
                }
                if (differ) break;
            }
            check(differ, "Different seeds should produce differing planets in non-home systems");
            const auto& solPlanets = botf::systemPlanets(s1, 1);
            check(solPlanets.size() >= 5, "Sol must have at least 5 planets");
            check(solPlanets[2].name == "Sol III" && solPlanets[2].planetClass == botf::PlanetClass::ClassM, "Sol III must be Class M");
            check(solPlanets[2].terraformState == botf::TerraformingState::Terraformed, "Sol III must be Terraformed");
            check(solPlanets[2].baseCapacity == 80 && solPlanets[2].maxCapacity == 100 && solPlanets[2].currentCapacity() == 100, "Sol III capacity mismatch");
            check(solPlanets[4].planetClass == botf::PlanetClass::GasGiant, "Sol V must be Gas Giant");
            check(!botf::canTerraformPlanet(s1, 2, 1, 4), "Gas giant must not be terraformable");
            check(botf::systemCapacity(s1, 1) >= 100, "Sol system capacity must be at least 100");
            check(botf::systemMaxCapacity(s1, 1) >= botf::systemBaseCapacity(s1, 1), "Max capacity must be >= base capacity");
        }, passed);
        run("colony ship terraforming order, progression, biotech acceleration, and capacity unlock", [] {
            auto s = botf::generate(42); s.campaignExpansionEnabled = true;
            int targetPlanet = -1;
            const auto& planets = botf::systemPlanets(s, 1);
            for (std::size_t i = 0; i < planets.size(); ++i) {
                if (botf::canTerraformPlanet(s, 2, 1, static_cast<int>(i))) {
                    targetPlanet = static_cast<int>(i);
                    break;
                }
            }
            check(targetPlanet >= 0, "Expected a terraformable planet in system 1");
            const int initialCap = botf::systemCapacity(s, 1);
            const auto cmd = botf::makeTerraformCommand(1, 2, 1, targetPlanet);
            botf::submit(s, cmd);
            const auto draft = botf::preview(s);
            check(draft.fleets[1].terraforming && draft.fleets[1].terraformPlanetIndex == targetPlanet,
                "Terraforming draft preview failed");
            botf::advance(s);
            const auto* pAfter1 = botf::planetAt(s, 1, targetPlanet);
            check(pAfter1->terraformState == botf::TerraformingState::InProgress && pAfter1->terraformWork == 100,
                "Colony Ship I must apply 100 terraform work per turn per BETA-FIDELITY");
            const int workBefore = pAfter1->terraformWork;
            botf::advance(s);
            const auto* pAfter2 = botf::planetAt(s, 1, targetPlanet);
            check(pAfter2->terraformWork - workBefore == 100, "Second turn must add 100 work without unverified Biotech boost");
            while (s.fleets[1].terraforming) {
                botf::advance(s);
            }
            const auto* pFinal = botf::planetAt(s, 1, targetPlanet);
            check(pFinal->terraformState == botf::TerraformingState::Terraformed, "Planet should be Terraformed upon completion");
            check(!s.fleets[1].terraforming, "Colony ship terraforming order should clear upon completion");
            check(botf::systemCapacity(s, 1) > initialCap, "System capacity must increase after terraforming completes");
        }, passed);
        run("invalid terraforming and task force orders rejected atomically", [] {
            auto s = botf::generate(42); s.campaignExpansionEnabled = true;
            const auto before = botf::serialize(s);
            rejects([&] { botf::submit(s, botf::makeTerraformCommand(1, 1, 1, 0)); });
            check(botf::serialize(s) == before, "Invalid ship class terraform mutated state");
            rejects([&] { botf::submit(s, botf::makeTerraformCommand(1, 2, 1, 4)); });
            check(botf::serialize(s) == before, "Gas giant terraform mutated state");
            rejects([&] { botf::submit(s, botf::makeTerraformCommand(1, 2, 1, 2)); });
            check(botf::serialize(s) == before, "Terraformed world terraform mutated state");
            rejects([&] { botf::submit(s, botf::makeTerraformCommand(1, 2, 2, 0)); });
            check(botf::serialize(s) == before, "Distant system terraform mutated state");
            rejects([&] { botf::submit(s, botf::makeCreateTaskForceCommand(1, {}, "Empty TF")); });
            check(botf::serialize(s) == before, "Empty task force mutated state");
            const int splitX = s.fleets[0].x + 1 < s.width ? s.fleets[0].x + 1 : s.fleets[0].x - 1;
            s.fleets.push_back({s.nextFleetId++, 1, splitX, s.fleets[0].y, splitX, s.fleets[0].y,
                false, false, botf::Building::Destroyer, 24});
            s.explored.push_back(s.fleets.back().y * s.width + s.fleets.back().x);
            std::sort(s.explored.begin(), s.explored.end());
            s.explored.erase(std::unique(s.explored.begin(), s.explored.end()), s.explored.end());
            const auto beforeWithShip = botf::serialize(s);
            rejects([&] { botf::submit(s, botf::makeCreateTaskForceCommand(1, {1, s.fleets.back().id}, "Split TF")); });
            check(botf::serialize(s) == beforeWithShip, "Split sector task force mutated state");
            rejects([&] { botf::submit(s, botf::makeDisbandTaskForceCommand(1, 999)); });
            check(botf::serialize(s) == beforeWithShip, "Invalid disband mutated state");
        }, passed);
        run("task force grouping, synchronized movement, stance propagation, and queries", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            const auto home = s.systems.front();
            const int d1Id = s.nextFleetId++;
            s.fleets.push_back({d1Id, 1, home.x, home.y, home.x, home.y, false, false, botf::Building::Destroyer, 24});
            const int d2Id = s.nextFleetId++;
            s.fleets.push_back({d2Id, 1, home.x, home.y, home.x, home.y, false, false, botf::Building::Destroyer, 24});
            const int tId = s.nextFleetId++;
            s.fleets.push_back({tId, 1, home.x, home.y, home.x, home.y, false, false, botf::Building::Transport, 14});
            const auto createCmd = botf::makeCreateTaskForceCommand(1, {d1Id, d2Id, tId}, "1st Strike Fleet");
            botf::submit(s, createCmd);
            const auto draft = botf::preview(s);
            check(draft.taskForces.size() == 1, "Task force draft creation failed");
            check(botf::fleetTaskForce(draft, d1Id) != nullptr && botf::fleetTaskForce(draft, d1Id)->id == 1,
                "Fleet task force query failed in draft");
            botf::advance(s);
            check(s.taskForces.size() == 1, "Task force commit failed");
            check(botf::fleetTaskForce(s, d1Id) != nullptr && botf::fleetTaskForce(s, d1Id)->id == 1,
                "Fleet task force query failed after commit");
            check(botf::taskForceFleets(s, 1).size() == 3, "Task force member count wrong");
            check(botf::taskForceSpeed(s, 1) == 1, "Task force speed must match slowest member (1)");
            check(botf::taskForceAttack(s, 1) == 8 + 8 + 2, "Task force attack must sum member attack values");
            check(botf::taskForceHull(s, 1) == 24 + 24 + 14, "Task force hull must sum member hulls");
            check(botf::taskForceShields(s, 1) == 0, "Task force shields must sum member shields (0)");
            const int destX = (home.x + 1) % s.width;
            s.explored.push_back(home.y * s.width + destX);
            std::sort(s.explored.begin(), s.explored.end());
            s.explored.erase(std::unique(s.explored.begin(), s.explored.end()), s.explored.end());
            const auto orderCmd = botf::makeTaskForceOrderCommand(2, 1, destX, home.y, botf::TacticalStance::Aggressive);
            botf::submit(s, orderCmd);
            botf::advance(s);
            for (int fid : {d1Id, d2Id, tId}) {
                const auto f = std::find_if(s.fleets.begin(), s.fleets.end(), [fid](const botf::Fleet& x) { return x.id == fid; });
                check(f != s.fleets.end(), "Member ship missing");
                check(f->x == destX && f->y == home.y, "Member ship did not move with task force");
                check(f->stance == botf::TacticalStance::Aggressive, "Stance did not propagate to member ship");
            }
            const auto disbandCmd = botf::makeDisbandTaskForceCommand(3, 1);
            botf::submit(s, disbandCmd);
            botf::advance(s);
            check(s.taskForces.empty(), "Task force disband failed");
            check(botf::fleetTaskForce(s, d1Id) == nullptr, "Disband did not unbind member fleet");
        }, passed);
        run("campaign expansion save roundtrip and schema ten migration", [] {
            auto s = botf::generateReferenceCampaign(1999);
            const auto home = s.systems.front();
            const int fid1 = s.nextFleetId++;
            s.fleets.push_back({fid1, 1, home.x, home.y, home.x, home.y, false, false, botf::Building::Destroyer, 24});
            botf::submit(s, botf::makeCreateTaskForceCommand(1, {fid1}, "Alpha Group"));
            botf::advance(s);
            check(s.taskForces.size() == 1, "Failed to create task force for save test");
            const auto saved = botf::serialize(s);
            const auto loaded = botf::deserialize(saved);
            check(botf::serialize(loaded) == saved, "Schema 11 exact roundtrip failed");
            check(loaded.taskForces.size() == 1 && loaded.taskForces[0].name == "Alpha Group",
                "Loaded task force data mismatch");
            check(!loaded.systems[0].planets.empty() && loaded.systems[0].planets[2].name == "Sol III",
                "Loaded planet data mismatch");
            const auto schemaTenText = asSchemaTen(s);
            const auto migrated = botf::deserialize(schemaTenText);
            check(!migrated.campaignExpansionEnabled, "Migrated save should have campaignExpansionEnabled false");
            check(migrated.taskForces.empty(), "Migrated save should have empty task forces");
            check(!migrated.systems.empty() && !migrated.systems[0].planets.empty(),
                "Migrated save must retain deterministic system planets");
            check(migrated.credits == s.credits && migrated.research == s.research,
                "Migrated save economy values must match original exactly");
        }, passed);
        run("AI staging, transport protection, and planetary conquest", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = true;
            s.majorRelations[0].contact = true; s.majorRelations[0].atWar = true;
            const auto sol = s.systems.front();
            s.colonies[0].population = 5;
            s.colonies[0].foodWorkers = 2; s.colonies[0].industryWorkers = 2; s.colonies[0].researchWorkers = 1;
            s.colonies[0].morale = 30;
            for (int lvl = 2; lvl <= 5; ++lvl) s.rivals[0].research += botf::researchCost(s, 5, lvl);
            s.rivals[0].technologies[5].level = 5;
            s.rivals[0].explored.push_back(sol.y * s.width + sol.x);
            std::sort(s.rivals[0].explored.begin(), s.rivals[0].explored.end());
            s.rivals[0].explored.erase(std::unique(s.rivals[0].explored.begin(), s.rivals[0].explored.end()), s.rivals[0].explored.end());
            std::erase_if(s.fleets, [&](const botf::Fleet& f) { return f.ownerId == 1 && f.x == sol.x && f.y == sol.y; });
            for (int k = 0; k < 3; ++k) {
                s.fleets.push_back({s.nextFleetId++, 2, sol.x, sol.y, sol.x, sol.y, false, false, botf::Building::Transport, 14});
            }
            const auto report = botf::advance(s);
            check(!report.invasions.empty(), "Assault force failed to initiate planetary invasion");
            const auto col = botf::colonyAt(s, sol.id);
            check(col != nullptr && col->ownerId == 2, "AI assault group failed to conquer target colony");
        }, passed);
        run("energy allocation priorities and defense battery ground combat mechanics", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            auto& c = s.colonies[0];
            c.factories = 1; c.laboratories = 1; c.shipyards = 1; c.shipyardEnabled = true; c.defenseBatteries = 2;
            c.foodWorkers = 20; c.industryWorkers = 10; c.researchWorkers = 10;
            c.energy = 25;
            auto econ = botf::economy(s, c);
            check(econ.poweredBatteries == 2 && econ.inactiveBatteries == 0, "Batteries should be fully powered");
            check(econ.shipyardPowered, "Shipyard should be powered");
            c.energy = 12;
            econ = botf::economy(s, c);
            check(econ.inactiveFactories == 0 && econ.inactiveLabs == 0, "Factories and labs take priority");
            check(!econ.shipyardPowered, "Shipyard unpowered under 12 energy");
            check(econ.poweredBatteries == 0 && econ.inactiveBatteries == 2, "Batteries must be unpowered when energy scarce");
        }, passed);
        run("task force retreat detachment and bidirectional reconciliation", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            s.unifiedCombat = false; // Legacy withdrawal timing; the session suite covers tactical retreats.
            const auto home = s.systems.front();
            const int d1Id = s.nextFleetId++;
            s.fleets.push_back({d1Id, 1, home.x, home.y, home.x, home.y, false, false, botf::Building::Destroyer, 24});
            const int d2Id = s.nextFleetId++;
            s.fleets.push_back({d2Id, 1, home.x, home.y, home.x, home.y, false, false, botf::Building::Destroyer, 24});
            botf::submit(s, botf::makeCreateTaskForceCommand(1, {d1Id, d2Id}, "Strike Group"));
            botf::advance(s);
            check(s.taskForces.size() == 1, "Task force should be formed");

            const int enemyId = s.nextFleetId++;
            s.fleets.push_back({enemyId, 2, home.x, home.y, home.x, home.y, false, false, botf::Building::Destroyer, 24});
            s.rivals[0].explored.push_back(home.y * s.width + home.x);
            std::sort(s.rivals[0].explored.begin(), s.rivals[0].explored.end());
            s.rivals[0].explored.erase(std::unique(s.rivals[0].explored.begin(), s.rivals[0].explored.end()), s.rivals[0].explored.end());
            s.majorRelations[0].contact = true;
            s.majorRelations[0].atWar = true;

            auto d1 = std::find_if(s.fleets.begin(), s.fleets.end(), [d1Id](const botf::Fleet& f) { return f.id == d1Id; });
            d1->stance = botf::TacticalStance::Withdraw;

            botf::advance(s);
            d1 = std::find_if(s.fleets.begin(), s.fleets.end(), [d1Id](const botf::Fleet& f) { return f.id == d1Id; });
            check(d1 != s.fleets.end(), "D1 should have survived via retreat");
            check(d1->taskForceId == 0, "Retreated ship must have taskForceId cleared");
            const auto* tf = botf::taskForceAt(s, 1);
            check(tf != nullptr, "Task force should persist with remaining member");
            check(tf->fleetIds.size() == 1 && tf->fleetIds[0] == d2Id, "Task force should only contain D2");

            auto d2 = std::find_if(s.fleets.begin(), s.fleets.end(), [d2Id](const botf::Fleet& f) { return f.id == d2Id; });
            check(d2 != s.fleets.end(), "D2 must exist");
            const int enemy2Id = s.nextFleetId++;
            s.fleets.push_back({enemy2Id, 2, home.x, home.y, home.x, home.y, false, false, botf::Building::Destroyer, 24});
            d2 = std::find_if(s.fleets.begin(), s.fleets.end(), [d2Id](const botf::Fleet& f) { return f.id == d2Id; });
            d2->stance = botf::TacticalStance::Withdraw;
            botf::advance(s);
            check(botf::taskForceAt(s, 1) == nullptr, "Empty task force must be pruned when all members detach/retreat");
        }, passed);
        run("task force mixed speed, atomic individual order detachment, and consumption reconciliation", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            const auto home = s.systems.front();
            const int dId = s.nextFleetId++;
            s.fleets.push_back({dId, 1, home.x, home.y, home.x, home.y, false, false, botf::Building::Destroyer, 24});
            const int tId = s.nextFleetId++;
            s.fleets.push_back({tId, 1, home.x, home.y, home.x, home.y, false, false, botf::Building::Transport, 14});
            botf::submit(s, botf::makeCreateTaskForceCommand(1, {dId, tId}, "Combined Convoy"));
            botf::advance(s);
            check(botf::taskForceSpeed(s, 1) == 1, "Task force speed must match slowest member (Transport = 1)");

            const int destX = (home.x + 1) % s.width;
            s.explored.push_back(home.y * s.width + destX);
            std::sort(s.explored.begin(), s.explored.end());
            s.explored.erase(std::unique(s.explored.begin(), s.explored.end()), s.explored.end());
            botf::Command moveCmd;
            moveCmd.id = 2;
            moveCmd.type = botf::CommandType::MoveFleet;
            moveCmd.fleetId = dId;
            moveCmd.targetX = destX;
            moveCmd.targetY = home.y;
            botf::submit(s, moveCmd);
            const auto draft = botf::preview(s);
            const auto dFleet = std::find_if(draft.fleets.begin(), draft.fleets.end(), [dId](const botf::Fleet& f) { return f.id == dId; });
            check(dFleet != draft.fleets.end() && dFleet->taskForceId == 0, "Individual order must atomically clear taskForceId");
            const auto* tfDraft = botf::taskForceAt(draft, 1);
            check(tfDraft != nullptr && tfDraft->fleetIds.size() == 1 && tfDraft->fleetIds[0] == tId,
                "Individual order must remove ship from task force fleetIds");

            botf::advance(s);
            const auto dPost = std::find_if(s.fleets.begin(), s.fleets.end(), [dId](const botf::Fleet& f) { return f.id == dId; });
            check(dPost != s.fleets.end() && dPost->x == destX && dPost->taskForceId == 0, "Detached ship moved to destination");

            const int csId = s.nextFleetId++;
            s.fleets.push_back({csId, 1, home.x, home.y, home.x, home.y, true, false, botf::Building::ColonyShip, 8});
            auto uncol = std::find_if(s.systems.begin(), s.systems.end(), [&](const botf::System& sys) {
                return !botf::colonyAt(s, sys.id);
            });
            check(uncol != s.systems.end(), "Need uncolonized system");
            const int uncolId = uncol->id;
            auto* csFleet = &s.fleets.back();
            csFleet->x = uncol->x; csFleet->y = uncol->y;
            csFleet->targetX = uncol->x; csFleet->targetY = uncol->y;
            s.explored.push_back(uncol->y * s.width + uncol->x);
            std::sort(s.explored.begin(), s.explored.end());
            s.explored.erase(std::unique(s.explored.begin(), s.explored.end()), s.explored.end());

            botf::submit(s, botf::makeCreateTaskForceCommand(3, {csId}, "Colony Group"));
            botf::advance(s);
            check(botf::taskForceAt(s, 2) != nullptr, "Colony task force created");

            botf::Command colCmd;
            colCmd.id = 4;
            colCmd.type = botf::CommandType::Colonize;
            colCmd.fleetId = csId;
            botf::submit(s, colCmd);
            botf::advance(s);
            check(botf::colonyAt(s, uncolId) != nullptr, "System colonized");
            check(botf::taskForceAt(s, 2) == nullptr, "Consumed colony ship group must be pruned");
        }, passed);
        run("task force order to sector (0,0) and command validation", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            s.systems[0].x = 1; s.systems[0].y = 0;
            s.colonies[0].shipyards = 1;
            const int fid = s.nextFleetId++;
            s.fleets.push_back({fid, 1, 1, 0, 1, 0, false, false, botf::Building::Destroyer, 24});
            s.explored.push_back(0);
            s.explored.push_back(1);
            std::sort(s.explored.begin(), s.explored.end());
            s.explored.erase(std::unique(s.explored.begin(), s.explored.end()), s.explored.end());
            botf::submit(s, botf::makeCreateTaskForceCommand(1, {fid}, "Origin Movers"));
            botf::advance(s);
            check(s.taskForces.size() == 1, "Task force created");

            botf::submit(s, botf::makeTaskForceOrderCommand(2, 1, 0, 0, botf::TacticalStance::Balanced));
            botf::advance(s);
            const auto f = std::find_if(s.fleets.begin(), s.fleets.end(), [fid](const botf::Fleet& x) { return x.id == fid; });
            check(f != s.fleets.end() && f->x == 0 && f->y == 0, "Task force must successfully move to sector (0,0)");

            std::string longName(65, 'A');
            rejects([&] { botf::submit(s, botf::makeCreateTaskForceCommand(3, {fid}, longName)); });
            rejects([&] { botf::submit(s, botf::makeCreateTaskForceCommand(3, {fid}, "Line\nBreak")); });
            rejects([&] { botf::submit(s, botf::makeCreateTaskForceCommand(3, {fid}, "Carriage\rReturn")); });
            rejects([&] { botf::submit(s, botf::makeCreateTaskForceCommand(3, {fid}, "Tab\tSeparated")); });
        }, passed);
        run("canTerraformPlanet constraints on expansion toggle and foreign colonies", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            const auto home = s.systems.front();
            const int csId = s.fleets[1].id;

            const auto foreignHome = s.systems[1];
            check(botf::colonyAt(s, foreignHome.id)->ownerId != 1, "System 2 must be foreign colony");

            s.fleets[1].x = foreignHome.x; s.fleets[1].y = foreignHome.y;
            s.fleets[1].targetX = foreignHome.x; s.fleets[1].targetY = foreignHome.y;
            s.explored.push_back(foreignHome.y * s.width + foreignHome.x);
            std::sort(s.explored.begin(), s.explored.end());
            s.explored.erase(std::unique(s.explored.begin(), s.explored.end()), s.explored.end());

            check(!botf::canTerraformPlanet(s, csId, foreignHome.id, 0),
                "canTerraformPlanet must reject foreign-owned colony");
            rejects([&] { botf::submit(s, botf::makeTerraformCommand(1, csId, foreignHome.id, 0)); });

            s.fleets[1].x = home.x; s.fleets[1].y = home.y;
            s.fleets[1].targetX = home.x; s.fleets[1].targetY = home.y;

            s.campaignExpansionEnabled = false;
            check(!botf::canTerraformPlanet(s, csId, home.id, 0),
                "canTerraformPlanet must return false when campaignExpansionEnabled is false");
            rejects([&] { botf::submit(s, botf::makeTerraformCommand(1, csId, home.id, 0)); });
        }, passed);
        run("AI transport safety prevents suicidal assault against fortified targets", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = true;
            s.majorRelations[0].contact = true; s.majorRelations[0].atWar = true;
            const auto sol = s.systems.front();
            auto& solColony = s.colonies[0];
            solColony.population = 80;
            solColony.energy = 50;
            solColony.defenseBatteries = 3;
            std::erase_if(s.fleets, [&](const botf::Fleet& f) { return f.ownerId == 1 && f.x == sol.x && f.y == sol.y; });

            const int loneTransportId = s.nextFleetId++;
            s.fleets.push_back({loneTransportId, 2, sol.x, sol.y, sol.x, sol.y, false, false, botf::Building::Transport, 14});
            s.rivals[0].explored.push_back(sol.y * s.width + sol.x);
            std::sort(s.rivals[0].explored.begin(), s.rivals[0].explored.end());
            s.rivals[0].explored.erase(std::unique(s.rivals[0].explored.begin(), s.rivals[0].explored.end()), s.rivals[0].explored.end());

            const auto report = botf::advance(s);
            check(report.invasions.empty(), "Lone transport must not perform suicidal invasion against fortified colony");
            const auto* colAfter = botf::colonyAt(s, sol.id);
            check(colAfter->ownerId == 1, "Fortified colony must remain in player hands");
        }, passed);
        run("advanceWithCombat custom resolver hook and state persistence", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            const auto home = s.systems.front();
            const int enemyId = s.nextFleetId++;
            s.fleets.push_back({enemyId, 2, home.x, home.y, home.x, home.y, false, false, botf::Building::Destroyer, 24});
            s.rivals[0].explored.push_back(home.y * s.width + home.x);
            std::sort(s.rivals[0].explored.begin(), s.rivals[0].explored.end());
            s.rivals[0].explored.erase(std::unique(s.rivals[0].explored.begin(), s.rivals[0].explored.end()), s.rivals[0].explored.end());
            s.majorRelations[0].contact = true;
            s.majorRelations[0].atWar = true;

            bool customHookInvoked = false;
            auto customResolver = [&](botf::GameState& moving, botf::TurnReport& report) {
                customHookInvoked = true;
                auto enemy = std::find_if(moving.fleets.begin(), moving.fleets.end(), [enemyId](const botf::Fleet& f) { return f.id == enemyId; });
                if (enemy != moving.fleets.end()) enemy->hull = 0;
                report.events.push_back("Custom combat hook successfully resolved battle.");
            };

            const auto report = botf::advanceWithCombat(s, customResolver);
            check(customHookInvoked, "advanceWithCombat must invoke supplied combat resolver");
            const auto enemyAfter = std::find_if(s.fleets.begin(), s.fleets.end(), [enemyId](const botf::Fleet& f) { return f.id == enemyId; });
            check(enemyAfter == s.fleets.end(), "Enemy fleet destroyed by custom combat resolver must be removed");
            botf::validate(s);
        }, passed);
        run("BuyConstruction rush cost query, cancellation, and execution", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            botf::submit(s, enqueue(1, botf::Building::Factory));
            botf::advance(s);
            const auto* col = botf::colonyAt(s, 1);
            check(!col->queue.empty(), "Factory should be in queue");
            const int initialCredits = static_cast<int>(s.credits);
            const int cost = botf::rushCost(s, 1);
            check(cost > 0, "Rush cost must be positive for remaining work");

            botf::submit(s, botf::makeBuyConstructionCommand(2, 1));
            const auto previewState = botf::preview(s);
            check(previewState.credits == initialCredits - cost, "Preview must reflect rushed credits deduction");
            check(previewState.colonies[0].queue.front().work >= 40, "Preview must show completed rush work");

            botf::cancel(s, 2);
            check(s.pending.empty(), "Command should be cancelled");
            check(s.credits == initialCredits, "Credits restored on cancellation");

            botf::submit(s, botf::makeBuyConstructionCommand(3, 1));
            const int factoriesBefore = botf::colonyAt(s, 1)->factories;
            const auto report = botf::advance(s);
            check(report.appliedCommands.size() == 1, "BuyConstruction should be applied in report");
            const auto* colAfter = botf::colonyAt(s, 1);
            check(colAfter->factories == factoriesBefore + 1, "Factory should be completed and added to colony");
        }, passed);
        run("ScrapStructure atomic constraints and removal", [] {
            auto s = botf::generateReferenceCampaign(1999); s.aiAggressionEnabled = false;
            auto& col = s.colonies[0];
            col.factories = 2;
            col.industryWorkers = 20;
            col.foodWorkers = 10;
            col.researchWorkers = 0;
            col.population = 30;

            rejects([&] { botf::submit(s, botf::makeScrapStructureCommand(1, 1, botf::Building::Factory)); });

            col.industryWorkers = 10;
            col.foodWorkers = 20;
            botf::submit(s, botf::makeScrapStructureCommand(2, 1, botf::Building::Factory));
            botf::advance(s);
            const auto* colAfter = botf::colonyAt(s, 1);
            check(colAfter->factories == 1, "Factory should be scrapped");

            s.colonies[0].defenseBatteries = 1;
            botf::submit(s, botf::makeScrapStructureCommand(3, 1, botf::Building::DefenseBattery));
            botf::advance(s);
            check(s.colonies[0].defenseBatteries == 0, "Defense battery should be scrapped");
        }, passed);
        run("BETA-FIDELITY synthetic planet terraforming fixture", [] {
            using namespace botf::referencePlanets;
            const int work = CalculateTerraformWork(81, EPlanetType::Desert, EAtmosphere::Methane);
            check(work == 688, "Synthetic formula must yield 688 work");

            const int turnsCS1 = CalculateTerraformTurns(work, 100);
            check(turnsCS1 == 7, "Colony Ship I requires 7 turns for 688 work");

            const int turnsCS2 = CalculateTerraformTurns(work, 300);
            check(turnsCS2 == 3, "Colony Ship II requires 3 turns for 688 work");

            const int workTerran = CalculateTerraformWork(100, EPlanetType::Terran, EAtmosphere::OxygenRich);
            check(workTerran == 0, "Terran planet with Oxygen-rich atmosphere requires 0 terraform work");
        }, passed);
        run("regression: Failure-26713-player4-turn171 advances turn without task force co-location error", [] {
            std::string saveContent;
            for (const auto& path : {"Failure-26713-player4-turn171.sav", "CampaignReview/Failure-26713-player4-turn171.sav",
                                     "game/Build/CampaignReview/Failure-26713-player4-turn171.sav",
                                     "../Build/CampaignReview/Failure-26713-player4-turn171.sav"}) {
                std::ifstream f(path);
                if (f.is_open()) {
                    std::stringstream buffer;
                    buffer << f.rdbuf();
                    saveContent = buffer.str();
                    break;
                }
            }
            if (saveContent.empty()) { std::cout << "SKIP missing fixture Failure-26713-player4-turn171.sav\n"; return; }
            auto s = botf::deserialize(saveContent);
            check(s.turn == 170, "Expected save turn 170");
            check(!s.taskForces.empty(), "Expected task forces in save");

            // Advancing to turn 171 must not throw 'Task force members not co-located'
            const auto report = botf::advance(s);
            check(s.turn == 171, "Expected turn to advance to 171");
            for (const auto& tf : s.taskForces) {
                if (tf.fleetIds.empty()) continue;
                const auto leader = std::find_if(s.fleets.begin(), s.fleets.end(), [&](const botf::Fleet& f) { return f.id == tf.fleetIds.front(); });
                check(leader != s.fleets.end(), "TF leader missing");
                for (int fid : tf.fleetIds) {
                    const auto m = std::find_if(s.fleets.begin(), s.fleets.end(), [fid](const botf::Fleet& f) { return f.id == fid; });
                    check(m != s.fleets.end(), "TF member missing");
                    check(m->x == leader->x && m->y == leader->y, "TF members must remain co-located");
                }
            }
        }, passed);
        run("Modern campaign diplomacy: proposal submission, responder authorization, AI evaluation and events", [] {
            auto s = botf::generateReferenceCampaign(42);
            check(s.playerEmpire == 1, "Player should be empire 1");
            check(!s.rivals.empty() && s.rivals[0].id == 2, "Rival should be empire 2");

            // Without contact, proposals are rejected
            check(!botf::canProposeTreaty(s, 1, 2, false, botf::diplomacy::TreatyType::NonAggression),
                "Cannot propose treaty without first contact");
            check(!botf::canProposeTreaty(s, 1, 1, false, botf::diplomacy::TreatyType::NonAggression),
                "Self-proposals are strictly illegal");

            // Establish contact and high trust
            s.majorRelations[0].contact = true;
            s.majorRelations[0].trust = 80;

            std::string reason;
            check(botf::canProposeTreaty(s, 1, 2, false, botf::diplomacy::TreatyType::NonAggression, &reason),
                "Proposal should be valid with contact and peace");

            // Propose Non-Aggression pact
            botf::submit(s, botf::makeProposeTreatyCommand(1, 2, false, botf::diplomacy::TreatyType::NonAggression));
            check(!s.pending.empty(), "Command queued in pending");

            // Advance turn to submit and resolve proposal with AI recipient
            botf::advance(s);
            const auto treaties1 = botf::activeTreaties(s, 1);
            check(!treaties1.empty() && treaties1.front().type == botf::diplomacy::TreatyType::NonAggression,
                "Non-Aggression pact active for empire 1");
            const auto treaties2 = botf::activeTreaties(s, 2);
            check(!treaties2.empty() && treaties2.front().type == botf::diplomacy::TreatyType::NonAggression,
                "Non-Aggression pact active for empire 2");
            check(!botf::diplomaticEvents(s).empty(), "Diplomatic events recorded");

            // Test responder authorization on an incoming proposal from AI (Empire 2) to player (Empire 1)
            botf::diplomacy::Proposal inc;
            inc.id = s.diplomacyEngineState.nextProposalId++;
            inc.proposer = botf::diplomacy::ParticipantId::major(2);
            inc.target = botf::diplomacy::ParticipantId::major(1);
            inc.treatyType = botf::diplomacy::TreatyType::Trade;
            inc.duration = botf::diplomacy::TreatyDuration::Turns25;
            inc.status = botf::diplomacy::ProposalStatus::Pending;
            inc.submittedTurn = s.turn;
            inc.dueTurn = s.turn + 2;
            s.diplomacyEngineState.proposals.push_back(inc);

            // Proposer (Empire 2) cannot accept own proposal
            check(!botf::diplomacy::DiplomacyEngine::respondToOffer(
                s.diplomacyEngineState,
                botf::diplomacy::ParticipantId::major(2),
                inc.id, true), "Proposer cannot accept own proposal");
            // Third party (Empire 3) cannot respond to proposal
            check(!botf::diplomacy::DiplomacyEngine::respondToOffer(
                s.diplomacyEngineState,
                botf::diplomacy::ParticipantId::major(3),
                inc.id, true), "Third party cannot respond to proposal");

            // Authorized recipient (player Empire 1) accepts
            check(botf::diplomacy::DiplomacyEngine::respondToOffer(
                s.diplomacyEngineState,
                botf::diplomacy::ParticipantId::major(1),
                inc.id, true), "Authorized recipient should accept proposal");
        }, passed);
        run("Modern campaign diplomacy: minor membership offer, arbitration and colony annexation", [] {
            auto s = botf::generateReferenceCampaign(55);
            check(!s.minors.empty(), "Reference campaign should have minor races");
            auto& m = s.minors.front();
            const int minorSystemId = m.systemId;

            // Establish contact, max attitude, and prior-turn friendship
            m.contact[1] = true;
            m.attitude[1] = 95;
            m.friendshipTurn[1] = 1;
            s.turn = 2; // Campaign turn must be later than friendship turn
            botf::syncDiplomacyState(s);

            std::string reason;
            check(botf::canProposeTreaty(s, 1, minorSystemId, true, botf::diplomacy::TreatyType::Membership, &reason),
                "Membership proposal to eligible minor should be valid");

            botf::submit(s, botf::makeProposeTreatyCommand(1, minorSystemId, true, botf::diplomacy::TreatyType::Membership));
            botf::advance(s);

            // Minor evaluates and accepts during turn settlement
            const auto* minorState = botf::minorDiplomacy(s, minorSystemId);
            check(minorState != nullptr, "Minor diplomacy state exists");
            check(minorState->membershipOwner == 1, "Minor joined empire 1");
            check(s.minors.front().memberOf == 1, "Minor memberOf synchronized to empire 1");

            // System converted into a player colony
            const auto col = botf::colonyAt(s, minorSystemId);
            check(col != nullptr && col->ownerId == 1, "Minor system converted to player colony");
            check(col->population == 30, "Annexed colony has starting population");
            check(col->foodWorkers == 15 && col->industryWorkers == 10 && col->researchWorkers == 5,
                "Annexed colony has starting workforce");
        }, passed);
        run("Modern campaign intelligence: budget allocation, department focus, bounded advance and operations", [] {
            auto s = botf::generateReferenceCampaign(77);
            const int64_t out1 = botf::empireIntelOutput(s, 1);
            check(out1 > 0, "Empire has base intel output");

            const auto is1 = botf::empireIntelState(s, 1);
            check(is1 != nullptr && is1->empireId == 1, "Empire has intel state");

            // Invalid budget allocation (does not sum to 100%) rejected
            bool thrown = false;
            try {
                botf::submit(s, botf::makeSetIntelBudgetCommand(1, 50, {{2, {10, 10}}}));
            } catch (...) {
                thrown = true;
            }
            check(thrown, "Intel budget allocation not summing to 100% must throw");

            // Valid budget allocation (60% security, 20% esp, 20% sab against empire 2)
            botf::submit(s, botf::makeSetIntelBudgetCommand(1, 60, {{2, {20, 20}}}));
            botf::submit(s, botf::makeSetIntelDepartmentFocusCommand(2, 2, botf::intelligence::IntelDepartment::Military));

            botf::advance(s);

            const auto isAfter = botf::empireIntelState(s, 1);
            check(isAfter != nullptr, "Empire intel state exists after advance");
            check(isAfter->budget.internalSecurity.percentage == 60, "Internal security percentage updated");
            check(isAfter->internalSecurityStockpile > 0, "Internal security stockpile accumulated");
            check(botf::empireSecurityStockpile(s, 1) > 0, "Security stockpile query matches state");

            const auto rState = isAfter->getRivalState(2);
            check(rState != nullptr, "Rival 2 intel state exists");
            check(rState->focusDepartment == botf::intelligence::IntelDepartment::Military, "Focus department updated");
        }, passed);
        run("Modern campaign saves: diplomacy and intelligence engine roundtrip and continuation", [] {
            auto s = botf::generateReferenceCampaign(88);
            s.majorRelations[0].contact = true;
            s.majorRelations[0].trust = 75;

            // Submit proposal and set intel budget
            botf::submit(s, botf::makeProposeTreatyCommand(1, 2, false, botf::diplomacy::TreatyType::NonAggression));
            botf::submit(s, botf::makeSetIntelBudgetCommand(2, 70, {{2, {15, 15}}}));

            // Pending modern commands roundtrip before advance
            const std::string preSaved = botf::serialize(s);
            auto preLoaded = botf::deserialize(preSaved);
            check(preLoaded.pending.size() == 2, "Pending modern commands roundtrip before advance");
            check(botf::serialize(preLoaded) == preSaved, "Pre-advance save roundtrip matches byte-for-byte");

            botf::advance(s);

            // Add an incoming proposal to player to test active pending proposals in engine
            botf::diplomacy::Proposal inc;
            inc.id = s.diplomacyEngineState.nextProposalId++;
            inc.proposer = botf::diplomacy::ParticipantId::major(2);
            inc.target = botf::diplomacy::ParticipantId::major(1);
            inc.treatyType = botf::diplomacy::TreatyType::Trade;
            inc.duration = botf::diplomacy::TreatyDuration::Turns25;
            inc.status = botf::diplomacy::ProposalStatus::Pending;
            inc.submittedTurn = s.turn;
            inc.dueTurn = s.turn + 2;
            s.diplomacyEngineState.proposals.push_back(inc);

            const std::string savedText = botf::serialize(s);
            check(savedText.find("DIPLOMACY_ENGINE") != std::string::npos, "Save contains DIPLOMACY_ENGINE section");
            check(savedText.find("INTEL_ENGINE") != std::string::npos, "Save contains INTEL_ENGINE section");

            auto loaded = botf::deserialize(savedText);
            check(loaded.turn == s.turn, "Turn matches original");
            check(loaded.campaignExpansionEnabled, "Campaign expansion enabled preserved");
            check(loaded.modernEnginesActive, "Modern engines active preserved");
            check(loaded.empireIntelStates.size() == s.empireIntelStates.size(), "Intel states count matches");
            check(botf::empireSecurityStockpile(loaded, 1) == botf::empireSecurityStockpile(s, 1),
                "Security stockpile matches original");
            check(!botf::incomingProposals(loaded, 1).empty(), "Pending incoming proposals roundtrip in loaded state");
            check(!botf::activeTreaties(loaded, 1).empty(), "Active treaties roundtrip in loaded state");

            // Canonical serialization must match byte-for-byte
            check(botf::serialize(loaded) == savedText, "Canonical save roundtrip matches byte-for-byte");

            // Loaded game advances cleanly
            const auto report = botf::advance(loaded);
            check(loaded.turn == s.turn + 1, "Loaded game advances to next turn");
        }, passed);

        run("Modern consistency: legacy war declaration breaks all canonical modern treaties for player and AI", [] {
            auto s = botf::generateReferenceCampaign(101);
            s.majorRelations[0].contact = true;
            s.majorRelations[0].trust = 80;

            // Seed active Non-Aggression and Trade treaties in modern engine
            botf::diplomacy::ActiveTreaty nat;
            nat.id = s.diplomacyEngineState.nextTreatyId++;
            nat.partyA = botf::diplomacy::ParticipantId::major(1);
            nat.partyB = botf::diplomacy::ParticipantId::major(2);
            nat.type = botf::diplomacy::TreatyType::NonAggression;
            nat.duration = botf::diplomacy::TreatyDuration::Turns25;
            nat.turnsRemaining = 25;
            nat.status = botf::diplomacy::TreatyStatus::Active;
            s.diplomacyEngineState.activeTreaties.push_back(nat);

            botf::diplomacy::ActiveTreaty tt;
            tt.id = s.diplomacyEngineState.nextTreatyId++;
            tt.partyA = botf::diplomacy::ParticipantId::major(1);
            tt.partyB = botf::diplomacy::ParticipantId::major(2);
            tt.type = botf::diplomacy::TreatyType::Trade;
            tt.duration = botf::diplomacy::TreatyDuration::Turns25;
            tt.turnsRemaining = 25;
            tt.status = botf::diplomacy::TreatyStatus::Active;
            s.diplomacyEngineState.activeTreaties.push_back(tt);
            s.majorRelations[0].tradeTreaty = true;

            check(botf::activeTreaties(s, 1).size() == 2, "Two active treaties before war");

            // Player legacy war declaration
            botf::Command war; war.id = 1; war.type = botf::CommandType::DeclareWar; war.targetEmpire = 2;
            botf::submit(s, war);
            botf::advance(s);
            check(s.majorRelations[0].atWar, "Legacy relation shows atWar true");
            check(!s.majorRelations[0].tradeTreaty, "Legacy relation tradeTreaty cleared");

            // Verify modern active treaties between 1 and 2 are broken
            check(botf::activeTreaties(s, 1).empty(), "No active treaties survive war declaration");
            check(s.diplomacyEngineState.treatyHistory.size() == 2, "Treaties moved to history");
            for (const auto& th : s.diplomacyEngineState.treatyHistory) {
                check(th.status == botf::diplomacy::TreatyStatus::BrokenByWar, "Treaty marked BrokenByWar");
            }

            // Test AI war declaration in planRivals
            auto s2 = botf::generateReferenceCampaign(102);
            // Setup relation between empire 2 (AI) and empire 1 (player)
            s2.majorRelations[0].contact = true;
            s2.majorRelations[0].atWar = false;
            s2.aiAggressionEnabled = true;
            s2.turn = 24;
            s2.diplomacyEngineState.currentTurn = 24;

            botf::diplomacy::ActiveTreaty aiTreaty;
            aiTreaty.id = s2.diplomacyEngineState.nextTreatyId++;
            aiTreaty.partyA = botf::diplomacy::ParticipantId::major(2);
            aiTreaty.partyB = botf::diplomacy::ParticipantId::major(1);
            aiTreaty.type = botf::diplomacy::TreatyType::Alliance;
            aiTreaty.duration = botf::diplomacy::TreatyDuration::Indefinite;
            aiTreaty.turnsRemaining = -1;
            aiTreaty.status = botf::diplomacy::TreatyStatus::Active;
            s2.diplomacyEngineState.activeTreaties.push_back(aiTreaty);
            check(!botf::activeTreaties(s2, 2).empty(), "AI has active alliance before aggression");

            // Lane B: an honourable Klingon never breaks a treaty in force, however low the trust.
            s2.majorRelations[0].trust = 10;
            botf::advance(s2);
            check(!s2.majorRelations[0].atWar, "AI broke an active alliance to declare war");
            check(!botf::activeTreaties(s2, 2).empty(), "AI alliance survived turn 25");
            // Without the treaty the same situation escalates, and the engine records the war.
            s2.diplomacyEngineState.activeTreaties.clear();
            s2.majorRelations[0].tradeTreaty = false;
            s2.turn = 45; s2.diplomacyEngineState.currentTurn = 45;
            botf::advance(s2);
            check(s2.majorRelations[0].atWar, "AI declared war once no treaty bound it");
            check(botf::activeTreaties(s2, 2).empty(), "AI war declaration left treaties active");
        }, passed);

        run("Modern consistency: legacy minor diplomacy synchronizes attitude, friendship, and membership to modern engine", [] {
            auto s = botf::generateReferenceCampaign(202);
            check(!s.minors.empty(), "Minors exist in reference campaign");
            const int minorSystemId = s.minors.front().systemId;

            // Initial attitude is 60 in both records
            check(s.minors.front().attitude[1] == 60, "Initial legacy attitude is 60");
            const auto* initMs = s.diplomacyEngineState.getMinorState(minorSystemId);
            check(initMs && initMs->getAttitude(1) == 60, "Initial modern attitude is 60");

            // Player delivers legacy gift (drafted at T=0, due at T=2)
            s.minors.front().contact[1] = true;
            botf::syncDiplomacyState(s);
            botf::Command gift; gift.id = 1; gift.systemId = minorSystemId; gift.type = botf::CommandType::SendGift;
            botf::submit(s, gift);
            botf::advance(s); // Commit draft (due at turn 2)
            botf::advance(s); // Turn 2: gift delivered

            // Legacy attitude increased to 80; verify modern engine synchronized to 80
            check(s.minors.front().attitude[1] == 80, "Legacy attitude increased to 80 after gift");
            const auto* msAfterGift = s.diplomacyEngineState.getMinorState(minorSystemId);
            check(msAfterGift && msAfterGift->getAttitude(1) == 80, "Modern minorState synchronized attitude to 80");

            // Legacy friendship offer
            botf::Command friendship; friendship.id = 2; friendship.systemId = minorSystemId; friendship.type = botf::CommandType::OfferFriendship;
            botf::submit(s, friendship);
            botf::advance(s);

            check(s.minors.front().friendshipTurn[1] >= 0, "Legacy friendship accepted");
            const auto* msAfterFriendship = s.diplomacyEngineState.getMinorState(minorSystemId);
            check(msAfterFriendship && msAfterFriendship->getFriendshipTurn(1) >= 0,
                "Modern minorState synchronized friendshipTurn");
            const int expectedFriendshipTurn = msAfterFriendship->getFriendshipTurn(1);

            // Advance to next turn so friendship is from a prior turn per DIP-02 rule
            botf::advance(s);

            // Modern membership proposal should now be valid because synchronized state satisfies 80 attitude + prior friendship
            std::string reason;
            check(botf::canProposeTreaty(s, 1, minorSystemId, true, botf::diplomacy::TreatyType::Membership, &reason),
                "Synchronized minor state permits modern membership proposal: " + reason);

            // Save and reload preserves synchronized minor state
            const std::string save = botf::serialize(s);
            auto loaded = botf::deserialize(save);
            const auto* loadedMs = loaded.diplomacyEngineState.getMinorState(minorSystemId);
            check(loadedMs && loadedMs->getAttitude(1) == 80, "Loaded modern minorState preserves attitude 80");
            check(loadedMs && loadedMs->getFriendshipTurn(1) == expectedFriendshipTurn,
                "Loaded modern minorState preserves friendshipTurn");
        }, passed);

        run("Modern consistency: legacy major treaty duration retains full 25 turns at acceptance and ticks over exactly 25 boundaries", [] {
            auto s = botf::generateReferenceCampaign(303);
            s.aiAggressionEnabled = false;
            s.eventsEnabled = false;
            s.majorRelations[0].contact = true;
            s.majorRelations[0].trust = 50;
            s.majorRelations[0].atWar = false;
            s.majorRelations[0].tradeTreaty = false;

            // Submit legacy trade treaty proposal
            botf::Command treaty; treaty.id = 1; treaty.type = botf::CommandType::ProposeTradeTreaty; treaty.targetEmpire = 2;
            botf::submit(s, treaty);

            // Turn 0 -> 1: proposal pending (decision turn)
            botf::advance(s);
            check(!s.majorRelations[0].tradeTreaty && s.pendingMajorOffers.size() == 1,
                "Trade proposal pending during decision turn");

            // Turn 1 -> 2: proposal accepted
            botf::advance(s);
            check(s.majorRelations[0].tradeTreaty, "Legacy tradeTreaty accepted");

            const auto treatiesT1 = botf::activeTreaties(s, 1);
            check(!treatiesT1.empty(), "Active treaty exists in modern engine");
            check(treatiesT1.front().type == botf::diplomacy::TreatyType::Trade, "Treaty is Trade");
            check(treatiesT1.front().turnsRemaining == 25,
                "Accepted legacy treaty retains full 25 turns at acceptance turn (not decremented to 24)");

            // Turn 1 -> 2: first tick occurs
            botf::advance(s);
            const auto treatiesT2 = botf::activeTreaties(s, 1);
            check(!treatiesT2.empty() && treatiesT2.front().turnsRemaining == 24,
                "Treaty decremented to 24 on following turn boundary");

            // Advance remaining 23 turns (total 24 turns advanced since acceptance)
            for (int i = 0; i < 23; ++i) {
                botf::advance(s);
            }
            const auto treatiesT25 = botf::activeTreaties(s, 1);
            check(!treatiesT25.empty() && treatiesT25.front().turnsRemaining == 1,
                "Treaty has 1 turn remaining after 24 elapsed turns, got size=" + std::to_string(treatiesT25.size()) +
                (treatiesT25.empty() ? "" : " rem=" + std::to_string(treatiesT25.front().turnsRemaining)));

            // Advance the 25th boundary: treaty should now expire
            botf::advance(s);
            const auto treatiesExpired = botf::activeTreaties(s, 1);
            check(treatiesExpired.empty(), "Treaty expired after exactly 25 boundaries");
            check(!s.diplomacyEngineState.treatyHistory.empty(), "Expired treaty archived in history");
        }, passed);

        run("Modern consistency: persistent intelligence operation history roundtrips through save/load", [] {
            auto s = botf::generateReferenceCampaign(404);

            botf::intelligence::OperationRecord op1;
            op1.attackerEmpireId = 1;
            op1.defenderEmpireId = 2;
            op1.area = botf::intelligence::IntelArea::Espionage;
            op1.department = botf::intelligence::IntelDepartment::Economic;
            op1.outcome = botf::intelligence::IntelOutcome::StandardSuccess;
            op1.opType = botf::intelligence::IntelOpType::StealCredits;
            op1.turn = s.turn;
            op1.creditsStolen = 250;
            op1.details = "Stole 250 credits from financial reserves.";
            s.intelOperations.push_back(op1);

            botf::intelligence::OperationRecord op2;
            op2.attackerEmpireId = 2;
            op2.defenderEmpireId = 1;
            op2.area = botf::intelligence::IntelArea::Sabotage;
            op2.department = botf::intelligence::IntelDepartment::Military;
            op2.outcome = botf::intelligence::IntelOutcome::MajorSuccess;
            op2.opType = botf::intelligence::IntelOpType::SabotageFacility;
            op2.turn = s.turn;
            op2.sabotageSeverity = 3;
            op2.detectedByDefender = true;
            op2.diplomaticIncidentPenalty = 30;
            op2.details = "Sabotaged infrastructure with severity level 3.";
            s.intelOperations.push_back(op2);

            const auto histAttacker = botf::empireIntelHistory(s, 1);
            check(histAttacker.size() == 2, "Empire 1 sees both own offensive op and detected defensive op");

            const std::string savedText = botf::serialize(s);
            check(savedText.find("INTEL_OPERATIONS 2") != std::string::npos,
                "Serialized save contains INTEL_OPERATIONS 2 section");

            auto loaded = botf::deserialize(savedText);
            check(loaded.intelOperations.size() == 2, "Loaded game has 2 intel operations");
            check(loaded.intelOperations[0] == op1, "First operation matches bit-for-bit");
            check(loaded.intelOperations[1] == op2, "Second operation matches bit-for-bit");

            const auto histLoaded = botf::empireIntelHistory(loaded, 1);
            check(histLoaded.size() == 2, "Loaded empireIntelHistory returns both operations");
            check(histLoaded[0].creditsStolen == 250, "Stolen credits preserved");
            check(histLoaded[1].detectedByDefender, "Defender detection preserved");

            // Canonical format test
            check(botf::serialize(loaded) == savedText, "Canonical save matches byte-for-byte after intel ops roundtrip");
        }, passed);

        run("Modern consistency: campaign membership validation rejects malformed social payloads", [] {
            auto s = botf::generateReferenceCampaign(505);

            // Valid base state passes validation
            botf::validate(s);

            // Test 1: invalid major proposer in proposal
            {
                auto bad = s;
                botf::diplomacy::Proposal p;
                p.id = bad.diplomacyEngineState.nextProposalId++;
                p.proposer = botf::diplomacy::ParticipantId::major(99); // Invalid empire
                p.target = botf::diplomacy::ParticipantId::major(1);
                p.treatyType = botf::diplomacy::TreatyType::NonAggression;
                bad.diplomacyEngineState.proposals.push_back(p);
                bool caught = false;
                try { botf::validate(bad); } catch (const std::exception&) { caught = true; }
                check(caught, "validate rejected proposal with nonexistent major empire");
            }

            // Test 2: invalid minor target in proposal
            {
                auto bad = s;
                botf::diplomacy::Proposal p;
                p.id = bad.diplomacyEngineState.nextProposalId++;
                p.proposer = botf::diplomacy::ParticipantId::major(1);
                p.target = botf::diplomacy::ParticipantId::minor(999); // Invalid minor system
                p.treatyType = botf::diplomacy::TreatyType::Membership;
                bad.diplomacyEngineState.proposals.push_back(p);
                bool caught = false;
                try { botf::validate(bad); } catch (const std::exception&) { caught = true; }
                check(caught, "validate rejected proposal with nonexistent minor");
            }

            // Test 3: invalid active treaty participant
            {
                auto bad = s;
                botf::diplomacy::ActiveTreaty at;
                at.id = bad.diplomacyEngineState.nextTreatyId++;
                at.partyA = botf::diplomacy::ParticipantId::major(1);
                at.partyB = botf::diplomacy::ParticipantId::major(99);
                at.type = botf::diplomacy::TreatyType::NonAggression;
                at.status = botf::diplomacy::TreatyStatus::Active;
                bad.diplomacyEngineState.activeTreaties.push_back(at);
                bool caught = false;
                try { botf::validate(bad); } catch (const std::exception&) { caught = true; }
                check(caught, "validate rejected active treaty with nonexistent major empire");
            }

            // Test 4: identical participants in treaty
            {
                auto bad = s;
                botf::diplomacy::ActiveTreaty at;
                at.id = bad.diplomacyEngineState.nextTreatyId++;
                at.partyA = botf::diplomacy::ParticipantId::major(1);
                at.partyB = botf::diplomacy::ParticipantId::major(1);
                at.type = botf::diplomacy::TreatyType::NonAggression;
                at.status = botf::diplomacy::TreatyStatus::Active;
                bad.diplomacyEngineState.activeTreaties.push_back(at);
                bool caught = false;
                try { botf::validate(bad); } catch (const std::exception&) { caught = true; }
                check(caught, "validate rejected active treaty between identical parties");
            }

            // Test 5: invalid minor in minorStates
            {
                auto bad = s;
                botf::diplomacy::MinorDiplomacyState ms;
                ms.minorId = 999;
                ms.systemId = 999;
                ms.raceName = "Phantoms";
                bad.diplomacyEngineState.minorStates.push_back(ms);
                bool caught = false;
                try { botf::validate(bad); } catch (const std::exception&) { caught = true; }
                check(caught, "validate rejected minor state with nonexistent minorId");
            }

            // Test 6: invalid intel operation empire
            {
                auto bad = s;
                botf::intelligence::OperationRecord op;
                op.attackerEmpireId = 99;
                op.defenderEmpireId = 1;
                op.turn = bad.turn;
                bad.intelOperations.push_back(op);
                bool caught = false;
                try { botf::validate(bad); } catch (const std::exception&) { caught = true; }
                check(caught, "validate rejected intel operation with nonexistent attacker");
            }
        }, passed);

        run("regression: backward compatibility and strict checksums on real pre-intel-operations schema-11 saves", [] {
            auto readSnapshot = [](const std::string& filename) -> std::string {
                for (const auto& prefix : {"", "../Saved/BotfSnapshots/", "Saved/BotfSnapshots/", "game/Saved/BotfSnapshots/", "../../Saved/BotfSnapshots/"}) {
                    std::ifstream f(std::string(prefix) + filename, std::ios::binary);
                    if (f) {
                        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                    }
                }
                return {};
            };

            const std::string rawTurn3 = readSnapshot("Turn-0003-313EDF1149FD45C90126C2B8DC3F4AA3.botfsave");
            if (rawTurn3.empty()) { std::cout << "SKIP missing fixture Turn-0003 snapshot\n"; return; }
            check(rawTurn3.find("INTEL_OPERATIONS") == std::string::npos, "Original snapshot lacks INTEL_OPERATIONS section");

            // 1. Clean deserialization of original pre-intel-operations snapshot
            auto s3 = botf::deserialize(rawTurn3);
            check(s3.turn == 3, "Turn-0003 snapshot turn is 3");
            check(s3.campaignExpansionEnabled && s3.modernEnginesActive, "Modern engines active in Turn-0003");
            check(s3.intelOperations.empty(), "Intel operations initialized empty in migrated state");
            botf::validate(s3);

            // 2. In-memory state canonicalizes upon serialization into modern schema-11 format
            const std::string reserialized3 = botf::serialize(s3);
            check(reserialized3.find("INTEL_OPERATIONS 0\n") != std::string::npos,
                "Canonical serialization includes INTEL_OPERATIONS section");
            check(botf::serialize(botf::deserialize(reserialized3)) == reserialized3,
                "Canonical modern save roundtrips byte-for-byte");

            // 3. Campaign turn advancement operates cleanly from migrated baseline
            auto continued3 = s3;
            botf::advance(continued3);
            check(continued3.turn == 4, "Turn-0003 advances to turn 4 cleanly");
            botf::validate(continued3);

            // 4. Adversarial tampering detection: single body byte change triggers checksum mismatch
            {
                std::string tampered = rawTurn3;
                auto statePos = tampered.find("STATE ");
                check(statePos != std::string::npos, "STATE section found");
                tampered[statePos + 6] ^= 1;
                bool caught = false;
                try {
                    botf::deserialize(tampered);
                } catch (const std::exception& e) {
                    caught = std::string(e.what()).find("checksum mismatch") != std::string::npos;
                }
                check(caught, "Tampered byte in old save body must be rejected with checksum mismatch");
            }

            // 5. Adversarial tampering detection: altered checksum value triggers checksum mismatch
            {
                std::string tampered = rawTurn3;
                auto chkPos = tampered.rfind("CHECKSUM ");
                check(chkPos != std::string::npos, "CHECKSUM found");
                tampered[chkPos + 10] = (tampered[chkPos + 10] == '9') ? '1' : '9';
                bool caught = false;
                try {
                    botf::deserialize(tampered);
                } catch (const std::exception& e) {
                    caught = std::string(e.what()).find("checksum mismatch") != std::string::npos;
                }
                check(caught, "Tampered checksum value in old save must be rejected with checksum mismatch");
            }

            // 6. Adversarial tampering detection: non-canonical payload spacing with recomputed checksum is rejected
            {
                auto computeHash = [](std::string_view text) -> std::uint64_t {
                    std::uint64_t hash = 14695981039346656037ULL;
                    for (unsigned char byte : text) { hash ^= byte; hash *= 1099511628211ULL; }
                    return hash;
                };
                auto chkPos = rawTurn3.rfind("CHECKSUM ");
                std::string nonCanonicalBody = rawTurn3.substr(0, chkPos) + " \n";
                std::string nonCanonicalSave = nonCanonicalBody + "CHECKSUM " + std::to_string(computeHash(nonCanonicalBody)) + "\n";
                bool caught = false;
                try {
                    botf::deserialize(nonCanonicalSave);
                } catch (const std::exception& e) {
                    caught = std::string(e.what()).find("canonical format") != std::string::npos;
                }
                check(caught, "Non-canonical old save payload must be rejected");
            }

            // 7. Verify Turn-42 snapshot compatibility
            const std::string rawTurn42 = readSnapshot("Turn-0042-A7DFBC2E40EC23211E7D2BB87766AA3F.botfsave");
            if (rawTurn42.empty()) { std::cout << "SKIP missing fixture Turn-0042 snapshot\n"; return; }
            auto s42 = botf::deserialize(rawTurn42);
            check(s42.turn == 42, "Turn-0042 snapshot turn is 42");
            botf::validate(s42);
            botf::advance(s42);
            check(s42.turn == 43, "Turn-0042 advances to turn 43 cleanly");
            botf::validate(s42);
        }, passed);

        run("selectable starting eras: API queries, validation, and rejected invalid eras", [] {
            const auto eras = botf::availableStartingEras();
            check(eras.size() == 5, "Five starting eras available");
            check(botf::startingEraName(botf::StartingEra::Early) == std::string("Early"), "Early era name");
            check(botf::startingEraName(botf::StartingEra::Developed) == std::string("Developed"), "Developed era name");
            check(botf::startingEraName(botf::StartingEra::Expanded) == std::string("Expanded"), "Expanded era name");
            check(botf::startingEraName(botf::StartingEra::Advanced) == std::string("Advanced"), "Advanced era name");
            check(botf::startingEraName(botf::StartingEra::Supreme) == std::string("Supreme"), "Supreme era name");
            check(botf::eraName(botf::StartingEra::Supreme) == std::string("Supreme"), "eraName alias returns Supreme");

            check(botf::startingEraValid(botf::StartingEra::Early), "Early is valid");
            check(botf::startingEraValid(botf::StartingEra::Developed), "Developed is valid");
            check(botf::startingEraValid(botf::StartingEra::Expanded), "Expanded is valid");
            check(botf::startingEraValid(botf::StartingEra::Advanced), "Advanced is valid");
            check(botf::startingEraValid(botf::StartingEra::Supreme), "Supreme is valid");
            check(!botf::startingEraValid(static_cast<botf::StartingEra>(0)), "Era 0 is invalid");
            check(!botf::startingEraValid(static_cast<botf::StartingEra>(6)), "Era 6 is invalid");
            check(!botf::startingEraValid(static_cast<botf::StartingEra>(-1)), "Era -1 is invalid");

            check(botf::eraBaseTechLevel(botf::StartingEra::Early, botf::Ruleset::Reference) == 1, "Early reference tech is 1");
            check(botf::eraBaseTechLevel(botf::StartingEra::Early, botf::Ruleset::Prototype) == 0, "Early prototype tech is 0");
            check(botf::eraBaseTechLevel(botf::StartingEra::Developed) == 3, "Developed tech is 3");
            check(botf::eraBaseTechLevel(botf::StartingEra::Expanded) == 5, "Expanded tech is 5");
            check(botf::eraBaseTechLevel(botf::StartingEra::Advanced) == 7, "Advanced tech is 7");
            check(botf::eraBaseTechLevel(botf::StartingEra::Supreme) == 10, "Supreme tech is 10");

            bool caughtInvalid = false;
            try {
                botf::generateCampaign(1234, 2, 16, 12, 28, 1, static_cast<botf::StartingEra>(0));
            } catch (const std::exception& e) {
                caughtInvalid = std::string(e.what()).find("Invalid starting era") != std::string::npos;
            }
            check(caughtInvalid, "generateCampaign rejects invalid starting era");

            caughtInvalid = false;
            try {
                botf::generateReferenceCampaign(1234, 2, 16, 12, 28, 1, static_cast<botf::StartingEra>(7));
            } catch (const std::exception& e) {
                caughtInvalid = std::string(e.what()).find("Invalid starting era") != std::string::npos;
            }
            check(caughtInvalid, "generateReferenceCampaign rejects invalid starting era");
        }, passed);

        run("selectable starting eras: progressive technology, economic viability, and planetary unlocks", [] {
            for (auto era : botf::availableStartingEras()) {
                const auto s = botf::generateReferenceCampaign(4242, 3, 16, 12, 28, 1, era);
                check(s.startingEra == era, "Campaign state tracks selected starting era");
                check(botf::campaignStartingEra(s) == era, "campaignStartingEra query matches era");
                botf::validate(s);

                const int expectedTech = botf::eraBaseTechLevel(era, botf::Ruleset::Reference);
                for (int f = 0; f < botf::ResearchFields; ++f) {
                    check(s.technologies[f].level == expectedTech, "Player field tech level matches era");
                    for (const auto& rival : s.rivals) {
                        check(rival.technologies[f].level == expectedTech, "Rival field tech level matches era");
                    }
                }

                std::int64_t expectedCredits = 0;
                int expectedPop = 0;
                int expectedShipsPerEmpire = 0;
                int expectedCapacity = 0;
                switch (era) {
                case botf::StartingEra::Early:
                    expectedCredits = 1000; expectedPop = 40; expectedShipsPerEmpire = 2; expectedCapacity = 130; break;
                case botf::StartingEra::Developed:
                    expectedCredits = 2500; expectedPop = 55; expectedShipsPerEmpire = 3; expectedCapacity = 130; break;
                case botf::StartingEra::Expanded:
                    expectedCredits = 4500; expectedPop = 75; expectedShipsPerEmpire = 5; expectedCapacity = 160; break;
                case botf::StartingEra::Advanced:
                    expectedCredits = 9500; expectedPop = 95; expectedShipsPerEmpire = 8; expectedCapacity = 190; break;
                case botf::StartingEra::Supreme:
                    expectedCredits = 10000; expectedPop = 120; expectedShipsPerEmpire = 9; expectedCapacity = 220; break;
                }

                check(s.credits == expectedCredits, "Starting credits match era");
                check(static_cast<int>(s.fleets.size()) == expectedShipsPerEmpire * 3, "Total fleets scale with era and empire count");

                const auto homeCol = botf::colonyAt(s, 1);
                check(homeCol != nullptr, "Home colony exists");
                check(homeCol->population == expectedPop, "Home colony population matches era");
                check(botf::systemCapacity(s, 1) == expectedCapacity, "Home system capacity unlocked via era terraforming");

                const auto eco = botf::economy(s, *homeCol);
                check(eco.foodDeficit == 0, "No food deficit on turn 1 in era colony");
                check(eco.inactiveFactories == 0, "All factories powered on turn 1 in era colony");
                check(eco.inactiveLabs == 0, "All labs powered on turn 1 in era colony");
                check(eco.shipyardPowered, "Shipyard powered on turn 1 in era colony");
                check(eco.inactiveBatteries == 0, "All defense batteries powered on turn 1 in era colony");
                check(homeCol->foodWorkers + homeCol->industryWorkers + homeCol->researchWorkers == homeCol->population,
                    "Total workforce fully employed in era colony");

                for (const auto& fleet : s.fleets) {
                    check(fleet.referenceShipId >= 0, "Reference ship ID populated");
                    check(fleet.hull == botf::fleetMaxHull(fleet), "Starting hull at max");
                    check(fleet.shields == botf::fleetMaxShields(fleet), "Starting shields at max");
                }

                // Advance turn 1 cleanly
                auto nextState = s;
                const auto report = botf::advance(nextState);
                check(nextState.turn == 1, "Turn advances from turn 0 to turn 1");
                check(report.outcome == botf::CampaignOutcome::Ongoing, "Campaign ongoing after turn 1");
                botf::validate(nextState);
            }
        }, passed);

        run("selectable starting eras: 5-way major empire parity and racial ship catalogues", [] {
            for (int playerFaction = 1; playerFaction <= 5; ++playerFaction) {
                const auto s = botf::generateReferenceCampaignWithEra(7777, botf::StartingEra::Advanced, 5, 20, 16, 35, playerFaction);
                check(s.playerEmpire == playerFaction, "Playable empire identity selected");
                check(s.startingEra == botf::StartingEra::Advanced, "Campaign starting era is Advanced");
                check(s.rivals.size() == 4, "Four rival empires present");
                botf::validate(s);

                for (int empId = 1; empId <= 5; ++empId) {
                    const auto& empState = botf::empire(s, empId);
                    check(empState.credits == 9500, "All 5 empires receive 9500 starting credits in Advanced era");
                    for (int f = 0; f < botf::ResearchFields; ++f) {
                        check(empState.technologies[f].level == 7, "All 5 empires have tech level 7 in Advanced era");
                    }

                    int ownedFleets = 0;
                    bool hasScout = false, hasColony = false, hasDestroyer = false, hasCruiser = false, hasTransport = false;
                    for (const auto& fleet : s.fleets) {
                        if (fleet.ownerId != empId) continue;
                        ++ownedFleets;
                        if (fleet.shipClass == botf::Building::Scout) hasScout = true;
                        if (fleet.shipClass == botf::Building::ColonyShip) hasColony = true;
                        if (fleet.shipClass == botf::Building::Destroyer) {
                            if (fleet.referenceShipId >= 0) {
                                const int role = botf::referenceData::ShipCatalogue[fleet.referenceShipId].role;
                                if (role == 1) hasDestroyer = true;
                                if (role == 2 || role == 3 || role == 4) hasCruiser = true;
                            }
                        }
                        if (fleet.shipClass == botf::Building::Transport) hasTransport = true;
                    }
                    check(ownedFleets == 8, "Each empire has exactly 8 ships in Advanced era");
                    check(hasScout, "Empire fleet has scout");
                    check(hasColony, "Empire fleet has colony ship");
                    check(hasDestroyer, "Empire fleet has destroyer");
                    check(hasCruiser, "Empire fleet has cruiser");
                    check(hasTransport, "Empire fleet has troop transport");
                }
            }
        }, passed);

        run("selectable starting eras: save/load persistence and round-trip continuation", [] {
            for (auto era : botf::availableStartingEras()) {
                const auto original = botf::generateReferenceCampaign(54321, 3, 16, 12, 28, 1, era);
                const std::string savedText = botf::serialize(original);
                check(savedText.find("STARTING_ERA " + std::to_string(static_cast<int>(era)) + "\n") != std::string::npos,
                    "Save contains exact STARTING_ERA tag");

                const auto loaded = botf::deserialize(savedText);
                check(loaded.startingEra == era, "Deserialized campaign preserves exact starting era");
                check(botf::campaignStartingEra(loaded) == era, "campaignStartingEra query preserves era after reload");
                botf::validate(loaded);

                const std::string reserialized = botf::serialize(loaded);
                check(reserialized == savedText, "Canonical save roundtrips byte-for-byte for selected era");

                auto advanced = loaded;
                for (int t = 0; t < 5; ++t) {
                    botf::advance(advanced);
                }
                check(advanced.turn == 5, "Campaign advances 5 turns from loaded state");
                check(advanced.startingEra == era, "Starting era retained after advancing 5 turns");
                botf::validate(advanced);

                const std::string turn5Saved = botf::serialize(advanced);
                const auto turn5Loaded = botf::deserialize(turn5Saved);
                check(turn5Loaded.startingEra == era, "Starting era persists across turn 5 save/load");
                check(turn5Loaded.turn == 5, "Turn 5 preserved");
            }

            // Also verify Prototype campaign with StartingEra::Expanded
            const auto protoExp = botf::generateCampaignWithEra(112233, botf::StartingEra::Expanded, 2, 16, 12, 28, 1);
            check(protoExp.startingEra == botf::StartingEra::Expanded, "Prototype tracks Expanded era");
            check(protoExp.credits == 1200, "Prototype Expanded starting credits are 1200");
            for (int f = 0; f < botf::ResearchFields; ++f) {
                check(protoExp.technologies[f].level == 5, "Prototype Expanded tech level is 5");
            }
            botf::validate(protoExp);
            const std::string protoSaved = botf::serialize(protoExp);
            check(protoSaved.find("STARTING_ERA 3\n") != std::string::npos, "Prototype save includes STARTING_ERA 3");
            const auto protoLoaded = botf::deserialize(protoSaved);
            check(protoLoaded.startingEra == botf::StartingEra::Expanded, "Prototype loaded preserves Expanded era");
            check(protoLoaded.credits == 1200, "Prototype loaded preserves 1200 credits");
            botf::validate(protoLoaded);
        }, passed);

        run("colony governors: independent workforce and queue priorities, evaluation, and explanation", [] {
            auto s = botf::generateReferenceCampaign(701);
            check(!s.colonies.empty(), "Colonies present");
            auto& col = s.colonies.front();
            const int sysId = col.systemId;

            // Initially default to Manual
            check(col.workforcePriority == botf::GovernorPriority::Manual, "Default workforce priority is Manual");
            check(col.queuePriority == botf::GovernorPriority::Manual, "Default queue priority is Manual");
            check(!col.workforceManualOverride, "Default manual override is false");

            // Set governor: Workforce = Manufacturing, Queue = Research
            auto cmd = botf::makeSetColonyGovernorCommand(s.lastAcceptedCommand + 1, sysId,
                botf::GovernorPriority::Manufacturing, botf::GovernorPriority::Research);
            botf::submit(s, cmd);

            // Preview applies governor
            auto prev = botf::preview(s);
            const auto* prevCol = botf::colonyAt(prev, sysId);
            check(prevCol != nullptr, "Colony in preview");
            check(prevCol->workforcePriority == botf::GovernorPriority::Manufacturing, "Preview shows Manufacturing workforce");
            check(prevCol->queuePriority == botf::GovernorPriority::Research, "Preview shows Research queue");
            check(!prevCol->workforceExplanation.empty(), "Workforce explanation is populated");
            // Manufacturing priority allocates to food safety first, then maxes industry
            check(prevCol->foodWorkers * 2 >= prevCol->population, "Governor reserves food to prevent starvation");
            check(prevCol->industryWorkers > 0, "Manufacturing governor allocated workers to industry");

            // Advance turn applies command permanently
            auto report = botf::advance(s);
            const auto* liveCol = botf::colonyAt(s, sysId);
            check(liveCol->workforcePriority == botf::GovernorPriority::Manufacturing, "Live colony has Manufacturing workforce");
            check(liveCol->queuePriority == botf::GovernorPriority::Research, "Live colony has Research queue");
        }, passed);

        run("colony governors: starvation prevention across population growth and shortage warnings", [] {
            auto s = botf::generateReferenceCampaign(702);
            auto& col = s.colonies.front();
            const int sysId = col.systemId;

            // Configure colony with Growth governor and sufficient farm capacity
            col.farms = 6;
            col.workforcePriority = botf::GovernorPriority::Growth;
            col.queuePriority = botf::GovernorPriority::Growth;
            col.workforceManualOverride = false;
            botf::evaluateWorkforceGovernor(s, col);

            const int initialPop = col.population;
            check(col.foodWorkers * 2 >= initialPop, "Initial food workers cover initial population");

            // Advance 10 turns and verify governor automatically adjusts food workers as population grows
            for (int t = 0; t < 10; ++t) {
                auto report = botf::advance(s);
                auto* current = botf::colonyAt(s, sysId);
                check(current != nullptr, "Colony exists");
                // Verify no starvation deficit occurred
                botf::Economy eco = botf::economy(s, *current);
                check(eco.foodDeficit == 0, "No food deficit occurred under Growth governor");
                check(current->foodWorkers * 2 * (100 + 5 * s.technologies[0].level) / 100 >= current->population,
                    "Governor assigned enough food workers to feed grown population");
            }

            // Test food shortage warning when farm capacity is artificially restricted
            auto testShortage = s;
            auto* shortCol = const_cast<botf::Colony*>(botf::colonyAt(testShortage, sysId));
            shortCol->population = 60;
            shortCol->farms = 1; // 1 farm = 10 workers max => max food = 20 (biotech 1 = 21) < 60!
            shortCol->workforcePriority = botf::GovernorPriority::Balanced;
            shortCol->workforceManualOverride = false;
            botf::evaluateWorkforceGovernor(testShortage, *shortCol);

            check(!shortCol->governorWarning.empty(), "Governor sets warning when shortage cannot be prevented");
            check(shortCol->foodWorkers == 10, "Governor assigned max possible food workers during shortage");
            auto shortReport = botf::advance(testShortage);
            bool warningLogged = false;
            for (const auto& ev : shortReport.events) {
                if (ev.find("governor warning") != std::string::npos || ev.find("food shortage") != std::string::npos) {
                    warningLogged = true;
                    break;
                }
            }
            check(warningLogged, "Turn report logs governor food shortage warning");
        }, passed);

        run("colony governors: manual workforce override and resumption lifecycle", [] {
            auto s = botf::generateReferenceCampaign(703);
            auto& col = s.colonies.front();
            const int sysId = col.systemId;

            // Set governor to Balanced
            col.workforcePriority = botf::GovernorPriority::Balanced;
            col.workforceManualOverride = false;
            botf::evaluateWorkforceGovernor(s, col);
            check(!col.workforceManualOverride, "Initially not overridden");

            // Player manually assigns workers
            botf::Command assignCmd;
            assignCmd.id = s.lastAcceptedCommand + 1;
            assignCmd.type = botf::CommandType::AssignWorkers;
            assignCmd.systemId = sysId;
            assignCmd.foodWorkers = 20;
            assignCmd.industryWorkers = 10;
            assignCmd.researchWorkers = 5;
            botf::submit(s, assignCmd);

            botf::advance(s);
            auto* current = const_cast<botf::Colony*>(botf::colonyAt(s, sysId));
            check(current->workforceManualOverride, "AssignWorkers flagged workforceManualOverride = true");
            check(current->foodWorkers == 20, "Manual food workers retained");
            check(current->industryWorkers == 10, "Manual industry workers retained");
            check(current->researchWorkers == 5, "Manual research workers retained");

            // Clear override command resumes governor
            auto clearCmd = botf::makeClearGovernorOverrideCommand(s.lastAcceptedCommand + 1, sysId);
            botf::submit(s, clearCmd);
            botf::advance(s);

            current = const_cast<botf::Colony*>(botf::colonyAt(s, sysId));
            check(!current->workforceManualOverride, "ClearGovernorOverride reset workforceManualOverride to false");
            check(!current->workforceExplanation.empty(), "Governor explanation updated upon resumption");
        }, passed);

        run("colony governors: queue governor project selection, bottleneck awareness, and pinned items preservation", [] {
            auto s = botf::generateReferenceCampaign(704);
            auto& col = s.colonies.front();
            const int sysId = col.systemId;

            // Empty queue and set Queue governor to Manufacturing
            col.queue.clear();
            col.queuePriority = botf::GovernorPriority::Manufacturing;
            botf::evaluateQueueGovernor(s, col);

            check(!col.queue.empty(), "Queue governor auto-queued a project when queue was empty");
            check(!col.queueExplanation.empty(), "Queue governor provided explanation for choice");

            // Pin the project
            auto pinCmd = botf::makeSetQueuePinCommand(s.lastAcceptedCommand + 1, sysId, 0, true);
            botf::submit(s, pinCmd);
            botf::advance(s);

            auto* current = const_cast<botf::Colony*>(botf::colonyAt(s, sysId));
            check(!current->queue.empty() && current->queue.front().pinned, "Project in queue is pinned");

            // Re-evaluating queue governor must NOT overwrite or remove pinned project
            current->queuePriority = botf::GovernorPriority::Research;
            botf::evaluateQueueGovernor(s, *current);
            check(current->queue.front().pinned, "Pinned project remained at front of queue");

            // Unpin project
            auto unpinCmd = botf::makeSetQueuePinCommand(s.lastAcceptedCommand + 1, sysId, 0, false);
            botf::submit(s, unpinCmd);
            botf::advance(s);
            current = const_cast<botf::Colony*>(botf::colonyAt(s, sysId));
            check(!current->queue.empty() && !current->queue.front().pinned, "Project unpinned successfully");
        }, passed);

        run("colony governors: save/load persistence, canonical serialization, and deterministic continuation", [] {
            auto s = botf::generateReferenceCampaign(705);
            auto& col = s.colonies.front();
            const int sysId = col.systemId;

            col.workforcePriority = botf::GovernorPriority::Growth;
            col.queuePriority = botf::GovernorPriority::Manufacturing;
            col.workforceManualOverride = false;
            botf::evaluateWorkforceGovernor(s, col);
            botf::evaluateQueueGovernor(s, col);
            if (!col.queue.empty()) {
                col.queue.front().pinned = true;
            }

            botf::validate(s);

            // Serialize and verify COLONY_GOVERNORS section presence
            const std::string text = botf::serialize(s);
            check(text.find("COLONY_GOVERNORS") != std::string::npos, "Save text contains COLONY_GOVERNORS section");

            // Deserialize and verify all fields preserved
            auto loaded = botf::deserialize(text);
            botf::validate(loaded);
            const auto* loadedCol = botf::colonyAt(loaded, sysId);
            check(loadedCol != nullptr, "Loaded colony exists");
            check(loadedCol->workforcePriority == botf::GovernorPriority::Growth, "Loaded workforcePriority matches");
            check(loadedCol->queuePriority == botf::GovernorPriority::Manufacturing, "Loaded queuePriority matches");
            check(!loadedCol->workforceManualOverride, "Loaded workforceManualOverride matches");
            if (!loadedCol->queue.empty()) {
                check(loadedCol->queue.front().pinned, "Loaded queue pinned status matches");
            }

            // Reserializing matches byte-for-byte
            check(botf::serialize(loaded) == text, "Canonical governor save roundtrips byte-for-byte");

            // Advance both original and loaded, verify bit-for-bit identical state
            auto contOriginal = s;
            auto contLoaded = loaded;
            auto rep1 = botf::advance(contOriginal);
            auto rep2 = botf::advance(contLoaded);
            check(rep1.stateHash == rep2.stateHash, "Advanced turn hash matches bit-for-bit between original and reloaded");
            check(botf::serialize(contOriginal) == botf::serialize(contLoaded), "Serializations match after advancement");
        }, passed);

        run("colony automation: pinned project protection against player queue mutations", [] {
            auto s = botf::generateReferenceCampaign(706);
            auto& col = s.colonies.front();
            const int sysId = col.systemId;

            col.queue.clear();
            col.queue.emplace_back(botf::Building::Farm, 0, -1, true);       // index 0: Pinned
            col.queue.emplace_back(botf::Building::Factory, 0, -1, false);   // index 1: Unpinned
            col.queue.emplace_back(botf::Building::Laboratory, 0, -1, false);// index 2: Unpinned

            // 1. CancelFirstConstruction must fail on pinned first project
            {
                auto sCopy = s;
                auto cmd = botf::makeCancelFirstConstructionCommand(sCopy.lastAcceptedCommand + 1, sysId);
                bool threw = false;
                try {
                    botf::submit(sCopy, cmd);
                } catch (const std::exception& e) {
                    threw = true;
                    check(std::string(e.what()).find("Cannot cancel pinned") != std::string::npos,
                        "CancelFirst error explains pinned protection");
                }
                check(threw, "CancelFirstConstruction rejected on pinned project");
            }

            // 2. RemoveConstruction must fail when targeting pinned index
            {
                auto sCopy = s;
                auto cmd = botf::makeRemoveConstructionCommand(sCopy.lastAcceptedCommand + 1, sysId, 0);
                bool threw = false;
                try {
                    botf::submit(sCopy, cmd);
                } catch (const std::exception& e) {
                    threw = true;
                    check(std::string(e.what()).find("Cannot remove pinned") != std::string::npos,
                        "RemoveConstruction error explains pinned protection");
                }
                check(threw, "RemoveConstruction rejected on pinned index");
            }

            // 3. PrioritizeLastConstruction must fail when queue has pinned project (or last is pinned)
            {
                auto sCopy = s;
                auto cmd = botf::makePrioritizeLastConstructionCommand(sCopy.lastAcceptedCommand + 1, sysId);
                bool threw = false;
                try {
                    botf::submit(sCopy, cmd);
                } catch (const std::exception& e) {
                    threw = true;
                    check(std::string(e.what()).find("pinned") != std::string::npos,
                        "PrioritizeLast error explains pinned protection");
                }
                check(threw, "PrioritizeLastConstruction rejected when pinned item present");
            }

            // 4. MoveConstruction must fail when moving a pinned project
            {
                auto sCopy = s;
                auto cmd = botf::makeMoveConstructionCommand(sCopy.lastAcceptedCommand + 1, sysId, 0, 1);
                bool threw = false;
                try {
                    botf::submit(sCopy, cmd);
                } catch (const std::exception& e) {
                    threw = true;
                    check(std::string(e.what()).find("Cannot move pinned") != std::string::npos,
                        "MoveConstruction error explains pinned item cannot be moved");
                }
                check(threw, "MoveConstruction rejected on moving pinned project");
            }

            // 5. MoveConstruction must fail when moving an unpinned project past/displacing a pinned project
            {
                auto sCopy = s;
                auto cmd = botf::makeMoveConstructionCommand(sCopy.lastAcceptedCommand + 1, sysId, 1, 0);
                bool threw = false;
                try {
                    botf::submit(sCopy, cmd);
                } catch (const std::exception& e) {
                    threw = true;
                    check(std::string(e.what()).find("past pinned") != std::string::npos,
                        "MoveConstruction error explains cannot reorder past pinned projects");
                }
                check(threw, "MoveConstruction rejected when displacing pinned project at index 0");
            }

            // 6. MoveConstruction between unpinned positions outside pinned range SUCCEEDS
            {
                auto sCopy = s;
                // Move item 1 (Factory) to 2 (after Laboratory) -> indices [1, 2] are both unpinned!
                auto cmd = botf::makeMoveConstructionCommand(sCopy.lastAcceptedCommand + 1, sysId, 1, 2);
                botf::submit(sCopy, cmd);
                botf::advance(sCopy);
                const auto* postCol = botf::colonyAt(sCopy, sysId);
                check(postCol->queue[0].product == botf::Building::Farm && postCol->queue[0].pinned, "Pinned Farm remained at index 0");
                check(postCol->queue[1].product == botf::Building::Laboratory, "Laboratory moved to index 1");
                check(postCol->queue[2].product == botf::Building::Factory, "Factory moved to index 2");
            }

            // 7. RemoveConstruction on unpinned index SUCCEEDS
            {
                auto sCopy = s;
                auto cmd = botf::makeRemoveConstructionCommand(sCopy.lastAcceptedCommand + 1, sysId, 2);
                botf::submit(sCopy, cmd);
                botf::advance(sCopy);
                const auto* postCol = botf::colonyAt(sCopy, sysId);
                check(postCol->queue.size() == 2, "Unpinned project successfully removed");
                check(postCol->queue[0].pinned, "Pinned project remained pinned at index 0");
            }

            // 8. Unpinning then CancelFirst SUCCEEDS
            {
                auto sCopy = s;
                auto unpinCmd = botf::makeSetQueuePinCommand(sCopy.lastAcceptedCommand + 1, sysId, 0, false);
                botf::submit(sCopy, unpinCmd);
                auto cancelCmd = botf::makeCancelFirstConstructionCommand(sCopy.lastAcceptedCommand + 1, sysId);
                botf::submit(sCopy, cancelCmd);
                botf::advance(sCopy);
                const auto* postCol = botf::colonyAt(sCopy, sysId);
                check(postCol->queue.size() == 2, "Unpinned project successfully cancelled");
                check(postCol->queue[0].product == botf::Building::Factory, "Factory is now first");
            }
        }, passed);

        run("colony automation: multiple pin positions, command ordering, and queue governor determinism", [] {
            auto s = botf::generateReferenceCampaign(707);
            auto& col = s.colonies.front();
            const int sysId = col.systemId;

            col.queue.clear();
            col.queue.emplace_back(botf::Building::Farm, 0, -1, true);       // 0: Pinned Farm
            col.queue.emplace_back(botf::Building::Factory, 0, -1, false);   // 1: Unpinned Factory
            col.queue.emplace_back(botf::Building::Laboratory, 0, -1, true); // 2: Pinned Lab
            col.queue.emplace_back(botf::Building::PowerPlant, 0, -1, false);// 3: Unpinned Power

            // Moving index 3 (PowerPlant) to index 1 touches pinned index 2 -> REJECTED
            {
                auto sCopy = s;
                auto cmd = botf::makeMoveConstructionCommand(sCopy.lastAcceptedCommand + 1, sysId, 3, 1);
                bool threw = false;
                try { botf::submit(sCopy, cmd); } catch (const std::exception&) { threw = true; }
                check(threw, "Moving across intermediate pinned item 2 rejected");
            }

            // Unpin index 2, then move index 3 to index 1 in same turn -> SUCCEEDS
            {
                auto sCopy = s;
                auto unpinCmd = botf::makeSetQueuePinCommand(sCopy.lastAcceptedCommand + 1, sysId, 2, false);
                botf::submit(sCopy, unpinCmd);
                auto moveCmd = botf::makeMoveConstructionCommand(sCopy.lastAcceptedCommand + 1, sysId, 3, 1);
                botf::submit(sCopy, moveCmd);
                botf::advance(sCopy);
                const auto* postCol = botf::colonyAt(sCopy, sysId);
                check(postCol->queue[0].product == botf::Building::Farm && postCol->queue[0].pinned, "Index 0 pinned Farm preserved");
                check(postCol->queue[1].product == botf::Building::PowerPlant, "Index 1 is now PowerPlant");
                check(postCol->queue[2].product == botf::Building::Factory, "Index 2 is Factory");
                check(postCol->queue[3].product == botf::Building::Laboratory && !postCol->queue[3].pinned, "Index 3 is unpinned Lab");
            }
        }, passed);

        run("colony automation: workforce food reservation formula parity with economy() and diagnostics", [] {
            auto s = botf::generateReferenceCampaign(708);
            auto& col = s.colonies.front();

            // Test across multiple Biotech tech levels (0, 2, 5)
            for (int bioLevel : {0, 2, 5}) {
                s.technologies[0].level = bioLevel;
                col.farms = 5;
                col.population = 45;
                col.workforcePriority = botf::GovernorPriority::Balanced;
                col.workforceManualOverride = false;

                botf::evaluateWorkforceGovernor(s, col);

                botf::Economy eco = botf::economy(s, col);
                check(eco.foodDeficit == 0, "No food deficit under balanced workforce governor at biotech level " + std::to_string(bioLevel));
                check(eco.food >= col.population, "Food yield meets or exceeds population");
                check(col.governorWarning.empty(), "No governor warning when farm capacity is sufficient");
                check(!col.workforceExplanation.empty(), "Workforce explanation is provided");
            }

            // Test starvation warning when maximum possible farm capacity cannot feed population
            col.farms = 1; // 1 farm = max 10 workers => yield at bio 0 = 20 food
            s.technologies[0].level = 0;
            col.population = 50;
            col.workforcePriority = botf::GovernorPriority::Growth;
            col.workforceManualOverride = false;

            botf::evaluateWorkforceGovernor(s, col);
            check(!col.governorWarning.empty(), "Governor sets warning when maximum farm capacity cannot feed population");
            check(col.foodWorkers == 10, "Governor staffs all available 10 farm workers during deficit");
            check(col.workforceExplanation.find(col.governorWarning) != std::string::npos,
                "Workforce explanation incorporates the shortage diagnostic");
        }, passed);
        remasterScenarios(passed);
    } catch (const std::exception& error) {
        std::cerr << "FAIL suite aborted after " << passed << " scenarios: " << error.what() << '\n';
        return 1;
    }
    std::cout << (failed ? "FAILED " : "ALL ") << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}

