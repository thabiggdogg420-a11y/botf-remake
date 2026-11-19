// Scenario scenarios (lane MOD-8, schema 25; docs/design/SCENARIOS.md): the scenario format and its validation, the three shipped
// scenarios (Dominion War, Klingon Civil War, Mirror Universe) played for 50 turns, the secession rule, flagships, the vendetta victory,
// and the save: the SCENARIO section, migration from 14 to 24, and the Classic and Remastered games unchanged.
#include "EconomyRules.h"
#include "EmpireSlots.h"
#include "Scenarios.h"
#include "Simulation.h"
#include "Threats.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace botf;
int failed = 0;
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void rejects(const std::function<void()>& fn, const std::string& message, const std::string& fragment = "") {
    try { fn(); } catch (const std::exception& e) {
        check(fragment.empty() || std::string(e.what()).find(fragment) != std::string::npos,
            message + ": message '" + e.what() + "' lacks '" + fragment + "'");
        return;
    }
    throw std::runtime_error(message);
}
void run(const char* name, const std::function<void()>& fn, int& passed) {
    try { fn(); ++passed; std::cout << "PASS " << name << std::endl; }
    catch (const std::exception& e) { ++failed; std::cout << "FAIL " << name << ": " << e.what() << std::endl; }
}
bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
std::string withChecksum(std::string body) {
    if (const auto pos = body.rfind("CHECKSUM "); pos != std::string::npos) body.erase(pos);
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char byte : body) { hash ^= byte; hash *= 1099511628211ULL; }
    return body + "CHECKSUM " + std::to_string(hash) + "\n";
}
std::string headerOf(int version) { return "BOTF_SIM " + std::to_string(version) + " remaster-" + std::to_string(version) + "\n"; }
std::string withHeader(std::string text, int version) { text.replace(0, text.find('\n') + 1, headerOf(version)); return withChecksum(text); }

std::vector<int> empireIds(const GameState& s) {
    std::vector<int> ids{s.playerEmpire};
    for (const auto& r : s.rivals) ids.push_back(r.id);
    std::sort(ids.begin(), ids.end());
    return ids;
}
bool seatedId(const GameState& s, int id) { const auto ids = empireIds(s); return std::find(ids.begin(), ids.end(), id) != ids.end(); }
int coloniesOf(const GameState& s, int owner) {
    return static_cast<int>(std::count_if(s.colonies.begin(), s.colonies.end(), [owner](const Colony& c) { return c.ownerId == owner; }));
}
int fleetsOf(const GameState& s, int owner) {
    return static_cast<int>(std::count_if(s.fleets.begin(), s.fleets.end(), [owner](const Fleet& f) { return f.ownerId == owner; }));
}
const Fleet* fleetById(const GameState& s, int id) { for (const auto& f : s.fleets) if (f.id == id) return &f; return nullptr; }
const System* systemNamed(const GameState& s, const std::string& name) { for (const auto& sys : s.systems) if (sys.name == name) return &sys; return nullptr; }
bool atWar(const GameState& s, int a, int b) { const auto* r = majorRelation(s, a, b); return r && r->atWar; }
int empireOf(const std::string& key) { return slots::raceForKey(key)->empireId; }

GameState scenarioGame(const std::string& id, int player = 0, std::uint64_t seed = 0) {
    CampaignSetup setup;
    setup.scenarioId = id;
    if (player) { setup.playerEmpire = player; setup.playerSet = true; }
    if (seed) setup.seed = seed;
    return newCampaign(setup);
}
// The player's colonies run on their governors, so the empire plays itself while the rivals run the AI.
void automatePlayer(GameState& s) {
    std::vector<int> ids;
    for (const auto& c : s.colonies) if (c.ownerId == s.playerEmpire && c.queuePriority == GovernorPriority::Manual) ids.push_back(c.systemId);
    for (int id : ids) submit(s, makeSetColonyGovernorCommand(s.lastAcceptedCommand + 1, id, GovernorPriority::Balanced, GovernorPriority::Balanced));
}
int playTurns(GameState& s, int turns) {
    int battles = 0;
    for (int t = 0; t < turns && s.outcome == CampaignOutcome::Ongoing; ++t) {
        if (t % 5 == 0) automatePlayer(s);
        const auto report = advance(s);
        battles += static_cast<int>(report.battles.size());
        if (s.turn % 10 == 0) {
            const auto text = serialize(s);
            const auto back = deserialize(text);
            check(fingerprint(back) == fingerprint(s) && serialize(back) == text, "save round trip at turn " + std::to_string(s.turn));
        }
    }
    return battles;
}
std::string eventsOf(const TurnReport& r) { std::string out; for (const auto& e : r.events) out += e + "\n"; return out; }

const char* const FixedJson = R"({"format":1,"id":"test-fixed","name":"Test fixed","blurb":"A tiny fixed chart.","order":900,"preset":"remastered",
"map":{"width":8,"height":6,"fixed":[{"name":"Ares","x":1,"y":1},{"name":"Brea","x":2,"y":4},{"name":"Cyrene","x":4,"y":2},{"name":"Delos","x":6,"y":4},{"name":"Elba","x":3,"y":3},{"name":"Fenn","x":7,"y":1}],"homes":["Delos"]},
"lineup":["federation","klingon"],"player":"federation","era":1,"year":2250,"briefing":["A tiny test."],
"setup":[{"do":"war","a":"federation","b":"klingon"},{"do":"colony","a":"federation","at":"near:1:capital:federation","population":20}],
"triggers":[{"id":"credits","text":"A grant.","when":{"turn":2},"do":[{"do":"credits","a":"player","amount":250}]}],
"victory":[{"id":"fall","name":"Klingons fall","kind":"eliminate","empires":["klingon"],"text":"They fell."}]}
)";
std::string replaced(std::string text, const std::string& from, const std::string& to) {
    const auto at = text.find(from);
    if (at == std::string::npos) throw std::runtime_error("test text lacks " + from);
    text.replace(at, from.size(), to);
    return text;
}
} // namespace

int main() {
    int passed = 0;

    run("registry: the three shipped scenarios load in order and name real presets", [] {
        const auto list = scenarios::all();
        check(list.size() >= 3, "three shipped scenarios");
        std::vector<std::string> ids;
        for (const auto* s : list) ids.push_back(s->id);
        const auto at = [&](const std::string& id) { return std::find(ids.begin(), ids.end(), id) - ids.begin(); };
        check(at("dominion-war") < at("klingon-civil-war") && at("klingon-civil-war") < at("mirror-universe"), "ordered by their order field");
        for (const auto* s : list) {
            check(presets::find(s->preset) != nullptr, s->id + " names a registered preset");
            check(s->lineup.size() >= 2 && !s->briefing.empty() && !s->victories.empty(), s->id + " has a lineup, a briefing and victory conditions");
            const auto effective = scenarios::effectivePreset(*s);
            check(effective.id == s->preset, "the effective preset keeps the preset id");
        }
        check(scenarios::find("nope") == nullptr, "an unknown scenario is not found");
    }, passed);

    run("format: malformed scenarios are refused with the offending path", [] {
        const std::string good = FixedJson;
        scenarios::parseScenario(good);   // the fixture itself is fine
        rejects([&] { scenarios::parseScenario(replaced(good, "\"do\":\"war\"", "\"do\":\"explode\"")); }, "unknown action", "unknown action");
        rejects([&] { scenarios::parseScenario(replaced(good, "\"near:1:capital:federation\"", "\"near:capital:federation\"")); }, "near without a count", "near");
        rejects([&] { scenarios::parseScenario(replaced(good, "\"when\":{\"turn\":2}", "\"when\":{}")); }, "a trigger without a condition", "at least one condition");
        rejects([&] { scenarios::parseScenario(replaced(good, "\"player\":\"federation\"", "\"player\":\"romulan\"")); }, "player outside the lineup", "not in the lineup");
        rejects([&] { scenarios::parseScenario(replaced(good, "\"homes\":[\"Delos\"]", "\"homes\":[\"Nowhere\"]")); }, "home star not on the map", "not on the map");
        rejects([&] { scenarios::parseScenario(replaced(good, "\"homes\":[\"Delos\"]", "\"homes\":[]")); }, "one home per seat", "home star");
        rejects([&] { scenarios::parseScenario(replaced(good, "{\"name\":\"Brea\",\"x\":2,\"y\":4}", "{\"name\":\"Brea\",\"x\":1,\"y\":1}")); }, "two stars in one sector", "two stars");
        rejects([&] { scenarios::parseScenario(replaced(good, "\"era\":1", "\"era\":9")); }, "era out of range", "outside");
        rejects([&] { scenarios::parseScenario(replaced(good, "\"kind\":\"eliminate\"", "\"kind\":\"conquer\"")); }, "unknown victory kind", "victory kind");
        rejects([&] { scenarios::parseScenario(replaced(good, "\"format\":1", "\"format\":2")); }, "unsupported format", "format");
        rejects([&] { scenarios::parseScenario(replaced(good, "\"blurb\":", "\"stray\":1,\"blurb\":")); }, "unknown key", "unknown key");
        rejects([&] { scenarios::registerScenario(replaced(good, "\"preset\":\"remastered\"", "\"preset\":\"no-such-preset\"")); }, "unregistered preset", "not registered");
        rejects([&] { scenarios::registerScenario(replaced(good, "\"id\":\"test-fixed\"", "\"id\":\"dominion-war\"")); }, "duplicate id", "already registered");
        rejects([&] { scenarios::parseScenario("{"); }, "broken JSON");
    }, passed);

    run("scripted start: an unresolvable reference in a scenario's setup is refused when the game starts", [] {
        const std::string bad = replaced(FixedJson, "\"id\":\"test-fixed\"", "\"id\":\"test-bad-ref\"");
        scenarios::registerScenario(replaced(bad, "\"near:1:capital:federation\"", "\"capital:ferengi\""));
        rejects([] { scenarioGame("test-bad-ref"); }, "an empire that is not seated", "not seated");
        scenarios::registerScenario(replaced(replaced(bad, "\"id\":\"test-bad-ref\"", "\"id\":\"test-bad-ship\""), "\"do\":\"colony\",\"a\":\"federation\",\"at\":\"near:1:capital:federation\",\"population\":20",
            "\"do\":\"fleet\",\"a\":\"federation\",\"at\":\"capital:federation\",\"ships\":[\"Klingon Bird\"]"));
        rejects([] { scenarioGame("test-bad-ship"); }, "a ship class nobody can build", "no ship class");
    }, passed);

    run("fixed map: the listed stars, the named homes, setup actions and a trigger", [] {
        scenarios::registerScenario(FixedJson);
        auto s = scenarioGame("test-fixed");
        check(s.systems.size() == 6 && s.width == 8 && s.height == 6, "exactly the listed stars");
        {   // Homes and minor races take their own names, so the chart is checked by position.
            std::set<std::pair<int, int>> got, want{{1, 1}, {2, 4}, {4, 2}, {6, 4}, {3, 3}, {7, 1}};
            for (const auto& sys : s.systems) got.insert({sys.x, sys.y});
            check(got == want, "stars sit where listed");
        }
        const auto* fed = colonyAt(s, 1);
        check(fed && fed->ownerId == 1, "system 1 is the first lineup empire's capital");
        const auto* klingonHome = colonyAt(s, systemNamed(s, "Qo'noS") ? systemNamed(s, "Qo'noS")->id : 0);
        check(klingonHome && klingonHome->ownerId == 2, "the second empire's capital took the named home star");
        check(atWar(s, 1, 2), "the scripted war");
        check(coloniesOf(s, 1) == 2, "the scripted colony joined the capital");
        check(s.scenarioId == "test-fixed" && scenarios::yearOf(s) == 2250, "scenario id and year");
        const auto before = s.credits;
        advance(s); advance(s);
        check(s.credits >= before + 250, "the turn-2 trigger paid its grant");
        check(s.scenarioFired == std::vector<int>({0}), "the trigger is recorded as fired");
        const auto text = serialize(s);
        check(contains(text, "\nSCENARIO 1 test-fixed 1 0\n"), "the SCENARIO section names the scenario and the fired trigger");
        check(serialize(deserialize(text)) == text, "round trip");
        auto again = scenarioGame("test-fixed");
        check(fingerprint(again) == fingerprint(scenarioGame("test-fixed")), "starting twice gives the same world");
    }, passed);

    run("scenario preset and player: the lineup is seated, the default player, a chosen player, a refused one", [] {
        auto dw = scenarioGame("dominion-war");
        check(empireIds(dw) == std::vector<int>({1, 2, 3, 4, 6}), "Dominion War seats Federation, Klingon, Romulan, Cardassian, Dominion");
        check(dw.playerEmpire == 1 && dw.presetId == "dominion-war" && dw.startingEra == StartingEra::Expanded, "default player, preset and era");
        check(dw.systems.size() == 80 && dw.width == 28 && dw.height == 20, "the scenario's galaxy");
        auto klingon = scenarioGame("dominion-war", 2);
        check(klingon.playerEmpire == 2, "a lineup empire can be chosen");
        rejects([] { scenarioGame("dominion-war", 5); }, "Ferengi are not in the Dominion War", "not in the scenario's lineup");
        auto kcw = scenarioGame("klingon-civil-war");
        check(kcw.playerEmpire == 2, "the Klingon Civil War defaults to the Klingon player");
        auto kcwFed = scenarioGame("klingon-civil-war", 1);
        check(kcwFed.playerEmpire == 1, "and the Federation can be chosen too");
        rejects([] { CampaignSetup s; s.scenarioId = "no-such-scenario"; newCampaign(s); }, "unknown scenario", "Unknown scenario");
    }, passed);

    run("Dominion War: the scripted start (wars, alliances, Bajor and DS9, the wormhole, colonies, fleets, flagships)", [] {
        const auto s = scenarioGame("dominion-war");
        for (const auto& pair : std::vector<std::pair<int, int>>{{6, 1}, {6, 2}, {4, 1}, {4, 2}}) check(atWar(s, pair.first, pair.second), "war " + std::to_string(pair.first) + "-" + std::to_string(pair.second));
        check(!atWar(s, 1, 2) && !atWar(s, 6, 4), "the two blocs are not at war inside");
        const auto fedAllies = alliedEmpires(s, 1), domAllies = alliedEmpires(s, 6);
        check(std::find(fedAllies.begin(), fedAllies.end(), 2) != fedAllies.end(), "Federation and Klingons are allied");
        check(std::find(domAllies.begin(), domAllies.end(), 4) != domAllies.end(), "Dominion and Cardassians are allied");
        check(!atWar(s, 3, 1) && !atWar(s, 3, 6) && alliedEmpires(s, 3).empty(), "the Romulans start neutral");
        const Minor* bajor = nullptr;
        for (const auto& m : s.minors) if (m.name == "Bajorans") bajor = &m;
        check(bajor != nullptr, "Bajor is in this galaxy");
        const auto* bajorSys = [&]() -> const System* { for (const auto& sys : s.systems) if (sys.id == bajor->systemId) return &sys; return nullptr; }();
        const auto* cardassia = systemNamed(s, slots::capital(4));
        check(cardassia && bajorSys && std::max(std::abs(cardassia->x - bajorSys->x), std::abs(cardassia->y - bajorSys->y)) <= 4, "Bajor was moved next to Cardassia");
        check(bajorSys->name == "Bajor", "the system carries the minor's name");
        bool ds9 = false;
        for (const auto& a : s.threatActors) if (a.kind == static_cast<int>(ThreatKind::DeepSpaceNine) && a.x == bajorSys->x && a.y == bajorSys->y) ds9 = true;
        check(ds9, "Deep Space Nine sits in the Bajoran sector");
        int mouths = 0;
        for (const auto& f : s.galaxyFeatures) if (f.type == GalaxyFeatureType::Wormhole) ++mouths;
        check(mouths >= 2, "the Bajoran wormhole exists");
        for (int id : {1, 2, 4, 6}) check(coloniesOf(s, id) >= 3, "each combatant starts with colonies beyond its capital");
        check(scenarios::flagshipFleet(s, 1) && scenarios::flagshipFleet(s, 2) && scenarios::flagshipFleet(s, 6), "three scenario-placed flagships");
        check(!scenarios::flagshipFleet(s, 4) && !scenarios::flagshipFleet(s, 3), "and none for the others");
        check(fleetName(*scenarios::flagshipFleet(s, 1)).rfind("U.S.S. Enterprise", 0) == 0, "the Federation flagship is the Enterprise");
        const auto obj = scenarios::objectives(s);
        check(obj.size() == 2 && !obj[0].achieved && obj[0].id == "break-the-dominion", "two scenario objectives, neither met");
        for (const auto& ship : s.fleets) check(ship.hull <= fleetMaxHull(ship) && ship.hull > 0, "every placed ship is whole");
    }, passed);

    run("Dominion War: the Romulan pact fires on turn 15 and the Cardassian revolt fires when the Union is nearly gone", [] {
        auto s = scenarioGame("dominion-war");
        std::string seen;
        for (int t = 0; t < 16 && s.outcome == CampaignOutcome::Ongoing; ++t) seen += eventsOf(advance(s));
        check(contains(seen, "Romulan Star Empire has signed the pact"), "the pact is announced");
        check(atWar(s, 3, 6), "the Romulans are at war with the Dominion");
        const auto fedAllies = alliedEmpires(s, 1);
        check(std::find(fedAllies.begin(), fedAllies.end(), 3) != fedAllies.end(), "and allied with the Federation");
        check(std::binary_search(s.scenarioFired.begin(), s.scenarioFired.end(), 0), "the trigger is recorded");
        // The revolt: take the Cardassian worlds down to two, advance one turn.
        auto r = scenarioGame("dominion-war");
        int kept = 0;
        for (auto& c : r.colonies) if (c.ownerId == 4 && ++kept > 2) c.ownerId = 1;
        advance(r);
        check(atWar(r, 4, 6) && !atWar(r, 4, 1), "the Cardassians turn on the Dominion and make peace with the Federation");
        check(std::binary_search(r.scenarioFired.begin(), r.scenarioFired.end(), 1), "revolt recorded");
    }, passed);

    run("Dominion War: victory by eliminating the Dominion", [] {
        auto s = scenarioGame("dominion-war");
        std::erase_if(s.colonies, [](const Colony& c) { return c.ownerId == 6; });
        std::erase_if(s.fleets, [](const Fleet& f) { return f.ownerId == 6; });
        s.flagships.erase(std::remove_if(s.flagships.begin(), s.flagships.end(), [](const Flagship& f) { return f.empireId == 6; }), s.flagships.end());
        const auto report = advance(s);
        check(s.outcome == CampaignOutcome::Victory && s.winnerEmpire == 1, "the player wins");
        check(contains(eventsOf(report), "The Dominion broken"), "the scenario's own victory is named");
        check(scenarios::objectives(s)[0].achieved && scenarios::objectives(s)[0].percent == 100, "the objective is shown achieved");
    }, passed);

    run("Mirror Universe: a fixed chart, the Terran Empire and the Alliance renamed and recoloured", [] {
        const auto s = scenarioGame("mirror-universe");
        check(s.systems.size() == 26 && s.width == 16 && s.height == 12, "the fixed chart of 26 stars");
        check(empireIds(s) == std::vector<int>({1, 2, 4}), "Terran Empire, Alliance, Cardassian Alliance");
        check(std::string(slots::name(1)) == "Terran Empire" && std::string(slots::name(2)) == "Klingon-Cardassian Alliance" && std::string(slots::name(4)) == "Cardassian Alliance", "empires are renamed by the overlay");
        check(std::string(slots::capital(1)) == "Earth" && systemNamed(s, "Earth") && colonyAt(s, systemNamed(s, "Earth")->id)->ownerId == 1, "the Terran capital is Earth");
        check(content::palette().present && content::palette().empires.size() == 3, "the pack carries a palette");
        check(atWar(s, 1, 2) && atWar(s, 1, 4) && !atWar(s, 2, 4), "Terran at war with both, the Alliance holds together");
        const auto allies = alliedEmpires(s, 2);
        check(std::find(allies.begin(), allies.end(), 4) != allies.end(), "Klingons and Cardassians are allied");
        check(scenarios::flagshipFleet(s, 1) != nullptr && fleetName(*scenarios::flagshipFleet(s, 1)).rfind("I.S.S. Enterprise", 0) == 0, "the I.S.S. Enterprise");
        check(s.presetId == "mirror-universe", "the mirror preset");
        // Starting a Classic game afterwards restores the plain names.
        CampaignSetup classic; classic.presetId = "classic"; classic.preset = RulesetPreset::Classic;
        const auto c = newCampaign(classic);
        check(std::string(slots::name(1)) == "Federation" && !content::palette().present && c.scenarioId.empty(), "the mirror overlay does not leak into the next game");
    }, passed);

    run("Klingon Civil War: the secession on turn 4 splits the Empire into two factions", [] {
        auto s = scenarioGame("klingon-civil-war");
        check(!seatedId(s, 8) && coloniesOf(s, 2) >= 6, "before the split: eight slots free, the Empire has its provinces");
        const int colonies = coloniesOf(s, 2), fleets = fleetsOf(s, 2);
        const auto credits = s.credits;
        std::string seen;
        for (int t = 0; t < 4; ++t) seen += eventsOf(advance(s));
        check(seatedId(s, 8), "the House of Duras is seated");
        check(contains(seen, "House of Duras has raised its banner") && contains(seen, "has split from"), "both announcements");
        check(std::string(slots::name(8)) == "House of Duras" && std::string(slots::capital(8)) == "Ty'Gokor", "the house is named by the overlay");
        check(coloniesOf(s, 8) >= 1 && coloniesOf(s, 2) >= 1 && coloniesOf(s, 8) + coloniesOf(s, 2) >= colonies - 1, "the worlds are shared, none lost");
        check(coloniesOf(s, 8) <= (colonies * 45) / 100 + 1, "the house took about its 40 percent");
        check(fleetsOf(s, 8) >= 1 && fleetsOf(s, 2) >= 1, "ships went to both sides");
        check(scenarios::flagshipFleet(s, 2) != nullptr && scenarios::flagshipFleet(s, 2)->ownerId == 2, "the loyalists kept the flagship");
        check(atWar(s, 2, 8), "the two houses are at war");
        check(s.secessions.size() == 1 && s.secessions[0].parent == 2 && s.secessions[0].house == 8 && s.secessions[0].turn == 4, "the secession is recorded");
        const auto* capital = systemNamed(s, "Ty'Gokor");
        check(capital && colonyAt(s, capital->id) && colonyAt(s, capital->id)->ownerId == 8 && colonyAt(s, capital->id)->shipyards == 1, "the house's first world is its capital, with a shipyard");
        check(empire(s, 8).credits > 0 && s.credits <= credits + 4 * 1000, "the treasury was shared");
        check(fleetsOf(s, 8) + fleetsOf(s, 2) >= fleets, "no ship vanished");
        const auto text = serialize(s);
        check(contains(text, "SECESSIONS 1\n2 8 4\n") && contains(text, "EMPIRE_SLOTS"), "saved with the secession and the eight-slot layout");
        check(serialize(deserialize(text)) == text && fingerprint(deserialize(text)) == fingerprint(s), "round trip across the split");
        for (int t = 0; t < 4; ++t) advance(s);
        advance(s);
        check(seatedId(s, 8), "the game goes on");
        const auto allies = alliedEmpires(s, 3);
        check(std::find(allies.begin(), allies.end(), 8) != allies.end(), "the Romulans allied with the house on turn 8");
        // F4: the house plays its own AI personality (personality only: no rule differs).
        check(diplomacy::personalityForEmpire(8) == diplomacy::MajorPersonality::Duras, "the house plays the Duras personality");
        check(diplomacy::personalityForEmpire(2) == diplomacy::MajorPersonality::Klingon, "the loyalists stay Klingon");
        check(diplomacy::profileFor(diplomacy::MajorPersonality::Duras).treacherous, "the Duras breaks its word when strong");
    }, passed);

    run("Klingon Civil War: a Federation player sees the same war from outside", [] {
        auto s = scenarioGame("klingon-civil-war", 1);
        for (int t = 0; t < 5; ++t) advance(s);
        check(seatedId(s, 8) && s.playerEmpire == 1, "the house rose while the player is the Federation");
        check(majorRelation(s, 1, 8) != nullptr, "the Federation has a relation slot with the house");
    }, passed);

    run("secession rule: preconditions, conservation and the small-empire refusal", [] {
        CampaignSetup setup; setup.seed = 11; setup.presetId = "klingon-civil-war"; setup.preset = RulesetPreset::Remastered;
        setup.lineup = {"federation", "klingon", "romulan", "cardassian"};
        auto s = newCampaign(setup);
        // The Klingons have one colony: too small to split, nothing changes.
        const auto before = serialize(s);
        check(!scenarios::secede(s, nullptr, 2, 8, 50), "an empire with a single world does not split");
        check(serialize(s) == before, "and the state is untouched");
        rejects([&] { scenarios::secede(s, nullptr, 2, 3, 50); }, "the house slot must be free", "free empire slot");
        rejects([&] { scenarios::secede(s, nullptr, 2, 8, 5); }, "share out of range", "share");
        rejects([&] { scenarios::secede(s, nullptr, 7, 8, 50); }, "parent not seated", "not seated");
        // Give the Klingons provinces by hand, then split.
        int given = 0;
        for (const auto& sys : s.systems) {
            if (given == 5) break;
            if (colonyAt(s, sys.id) || minorAt(s, sys.id)) continue;
            scenarios::Action act; act.kind = scenarios::ActionKind::Colony; act.a = "klingon"; act.at = "system:" + sys.name; act.population = 30;
            scenarios::runAction(s, act, nullptr); ++given;
        }
        validate(s);
        const int total = coloniesOf(s, 2);
        const int ships = fleetsOf(s, 2);
        check(scenarios::secede(s, nullptr, 2, 8, 50), "a six-world empire splits");
        check(coloniesOf(s, 2) + coloniesOf(s, 8) == total && coloniesOf(s, 8) >= 2 && coloniesOf(s, 2) >= 1, "every world is somebody's");
        check(fleetsOf(s, 2) + fleetsOf(s, 8) == ships, "every ship is somebody's");
        check(seatedId(s, 8) && s.secessions.size() == 1, "the house is seated");
        rejects([&] { scenarios::secede(s, nullptr, 2, 8, 50); }, "the house cannot split twice", "free empire slot");
        check(deserialize(serialize(s)).secessions.size() == 1, "the record survives a save");
        playTurns(s, 20);
    }, passed);

    run("flagships: commissioning, bonuses, naming, morale, loss and replacement", [] {
        CampaignSetup setup; setup.seed = 5; setup.presetId = "mirror-universe"; setup.preset = RulesetPreset::Remastered;
        setup.lineup = {"federation", "klingon", "cardassian"};
        auto s = newCampaign(setup);
        check(scenarios::flagshipsOn() && !scenarios::flagshipFleet(s, 1), "the pack has flagships and none exists yet");
        // Find an armed ship of the player's.
        int target = 0;
        for (const auto& f : s.fleets) if (f.ownerId == 1 && f.hull > 0 && fleetAttack(f) > 0 && !f.colonyShip && f.shipClass != Building::Transport) target = f.id;
        if (!target) { // the era fleet may be scouts only: put a warship on the board
            const auto& sys = s.systems.front();
            Fleet f; f.id = s.nextFleetId++; f.ownerId = 1; f.x = f.targetX = sys.x; f.y = f.targetY = sys.y; f.shipClass = Building::Destroyer;
            f.referenceShipId = 1003; f.hull = fleetMaxHull(f); f.shields = fleetMaxShields(f);
            s.fleets.push_back(f); target = f.id;
            validate(s);
        }
        const Fleet* ship = fleetById(s, target);
        const int hull = fleetMaxHull(*ship), attack = fleetAttack(*ship), shields = fleetMaxShields(*ship);
        s.credits = 3000;
        check(scenarios::commissionBlocker(s, 1, target).empty(), "an armed ship of the empire can be commissioned: " + scenarios::commissionBlocker(s, 1, target));
        check(scenarios::commissionBlocker(s, 1, 9999).find("one of our ships") != std::string::npos, "an unknown ship is refused");
        check(scenarios::commissionBlocker(s, 2, target).find("one of our ships") != std::string::npos, "another empire's ship is refused");
        auto poor = s; poor.credits = 10;
        check(scenarios::commissionBlocker(poor, 1, target).find("credits") != std::string::npos, "too few credits are refused");
        const auto before = s.credits;
        submit(s, scenarios::makeCommissionFlagshipCommand(s.lastAcceptedCommand + 1, target));
        const auto draft = preview(s);
        check(draft.credits == before - scenarios::flagshipDef(1)->cost, "the draft shows the refit's cost");
        check(creditForecast(s, 1).orders == -static_cast<std::int64_t>(scenarios::flagshipDef(1)->cost), "the forecast carries it as an order");
        advance(s);
        const Fleet* flag = fleetById(s, target);
        check(flag && flag->flagship == 1, "the ship carries the flagship mark");
        check(fleetMaxHull(*flag) > hull && fleetAttack(*flag) >= attack && fleetMaxShields(*flag) >= shields, "hull, attack and shields grew");
        check(fleetMaxHull(*flag) == hull * (100 + scenarios::flagshipDef(1)->hullPct) / 100, "by the pack's percentage");
        check(fleetName(*flag).rfind("I.S.S. Enterprise", 0) == 0, "named from the pack");
        check(scenarios::flagshipMorale(s, 1) == scenarios::flagshipDef(1)->morale && scenarios::flagshipMorale(s, 2) == (scenarios::flagshipFleet(s, 2) ? scenarios::flagshipDef(2)->morale : 0), "morale for the owner only");
        check(!scenarios::commissionBlocker(s, 1, target).empty(), "a second commission is refused");
        const auto text = serialize(s);
        check(contains(text, "FLAGSHIPS ") && contains(text, "\n1 " + std::to_string(target) + " 1 0\n"), "the flagship is saved");
        check(serialize(deserialize(text)) == text && deserialize(text).fleets.size() == s.fleets.size() && fleetById(deserialize(text), target)->flagship == 1, "and restored with its mark");
        // Lose it.
        const int lostTurn = s.turn;   // The turn's upkeep (step) runs after combat has removed the ship; a mid-turn state is never validated or saved.
        std::erase_if(s.fleets, [&](const Fleet& f) { return f.id == target; });
        s.taskForces.clear();
        TurnReport report;
        scenarios::step(s, report);
        validate(s);
        check(contains(eventsOf(report), "has been lost"), "the loss is reported");
        check(scenarios::flagshipOf(s, 1)->fleetId == 0 && scenarios::flagshipOf(s, 1)->lostTurn == lostTurn, "recorded as lost");
        int other = 0;
        for (const auto& f : s.fleets) if (f.ownerId == 1 && f.hull > 0 && fleetAttack(f) > 0 && !f.colonyShip && f.shipClass != Building::Transport) other = f.id;
        if (!other) {
            const auto& sys = s.systems.front();
            Fleet f; f.id = s.nextFleetId++; f.ownerId = 1; f.x = f.targetX = sys.x; f.y = f.targetY = sys.y; f.shipClass = Building::Destroyer;
            f.referenceShipId = 1003; f.hull = fleetMaxHull(f); f.shields = fleetMaxShields(f);
            s.fleets.push_back(f); other = f.id;
        }
        s.credits = 5000;
        check(scenarios::commissionBlocker(s, 1, other).find("can be commissioned on turn") != std::string::npos, "a replacement waits out the cooldown");
        s.turn = lostTurn + scenarios::flagshipDef(1)->cooldown; s.diplomacyEngineState.currentTurn = s.turn;
        check(scenarios::commissionBlocker(s, 1, other).empty(), "and may be commissioned after it");
        scenarios::commission(s, 1, other);
        check(scenarios::flagshipOf(s, 1)->serial == 2 && fleetName(*fleetById(s, other)).rfind("U.S.S. Defiant", 0) == std::string::npos, "the second commission takes the next name");
        check(fleetName(*fleetById(s, other)).rfind("I.S.S. Defiant", 0) == 0, "the Mirror's second flagship is the I.S.S. Defiant");
    }, passed);

    run("flagships: a game without the flagships pack has none, and the AI commissions its own when the pack has them", [] {
        CampaignSetup plain; plain.seed = 3; plain.presetId = "remastered";
        auto s = newCampaign(plain);
        check(!scenarios::flagshipsOn() && scenarios::commissionBlocker(s, 1, 1) == "This game has no flagships", "no pack, no flagships");
        auto m = scenarioGame("mirror-universe");
        check(scenarios::flagshipFleet(m, 2) == nullptr, "the Alliance starts without one");
        m.rivals.front().credits = 5000;
        for (int t = 0; t < 3; ++t) advance(m);
        check(scenarios::flagshipFleet(m, 2) != nullptr || scenarios::flagshipFleet(m, 4) != nullptr, "an AI empire commissioned its flagship");
    }, passed);

    run("vendetta victory: both sworn rivals gone wins under the remastered rules, not under Classic", [] {
        CampaignSetup setup; setup.seed = 21;
        auto s = newCampaign(setup);
        const auto progress = victoryProgress(s, 1);
        check(progress.size() == 5 && progress.back().condition == VictoryCondition::Vendetta && progress.back().enabled, "the Vendetta row exists and is live in a five-empire Remastered game");
        check(std::string(victoryConditionName(VictoryCondition::Vendetta)) == "Vendetta", "its name");
        check(scenarios::vendettaRivals(1) == std::vector<int>({3, 4}), "the Federation's rivals are the Romulans and the Cardassians");
        // Eliminate them now; the vendetta is claimed on turn 20, not before.
        std::erase_if(s.colonies, [](const Colony& c) { return c.ownerId == 3 || c.ownerId == 4; });
        std::erase_if(s.fleets, [](const Fleet& f) { return f.ownerId == 3 || f.ownerId == 4; });
        s.taskForces.clear();
        validate(s);
        for (int i = 0; i < 19; ++i) { advance(s); check(s.outcome == CampaignOutcome::Ongoing, "no vendetta before turn 20 (turn " + std::to_string(s.turn) + ")"); }
        const auto report = advance(s);
        check(s.outcome == CampaignOutcome::Victory && s.winnerEmpire == 1, "the Federation wins the vendetta: " + eventsOf(report));
        check(contains(eventsOf(report), "Vendetta victory"), "named in the turn report");
        check(victoryProgress(s, 1).back().achieved && victoryProgress(s, 1).back().percent == 100, "the progress row shows it achieved");
        // A rival half done: 50 percent.
        auto half = newCampaign(setup);
        std::erase_if(half.colonies, [](const Colony& c) { return c.ownerId == 3; });
        std::erase_if(half.fleets, [](const Fleet& f) { return f.ownerId == 3; });
        half.taskForces.clear();
        check(victoryProgress(half, 1).back().percent == 50, "one rival down is half a vendetta");
        // Classic: disabled.
        CampaignSetup classic = setup; classic.preset = RulesetPreset::Classic;
        const auto c = newCampaign(classic);
        const auto row = victoryProgress(c, 1).back();
        check(row.condition == VictoryCondition::Vendetta && !row.enabled && row.percent == 0, "Classic shows the row disabled");
        // Two empires: the rivals are not both seated, so the vendetta is not live.
        CampaignSetup two; two.seed = 4; two.empires = 2;
        check(!victoryProgress(newCampaign(two), 1).back().enabled, "a two-empire game has no vendetta");
    }, passed);

    for (const std::string id : {"dominion-war", "klingon-civil-war", "mirror-universe"}) {
        const std::string name = "soak: " + id + ", 50 turns with the AI, determinism and save/reload";
        run(name.c_str(), [id] {
            auto a = scenarioGame(id);
            auto b = scenarioGame(id);
            check(fingerprint(a) == fingerprint(b), "the same start gives the same world");
            const int battlesA = playTurns(a, 50);
            const int battlesB = playTurns(b, 50);
            check(battlesA == battlesB && fingerprint(a) == fingerprint(b), "two runs agree on 50 turns");
            check(a.turn == 50 || a.outcome != CampaignOutcome::Ongoing, "50 turns were played");
            auto c = scenarioGame(id);
            playTurns(c, 25);
            auto resumed = deserialize(serialize(c));
            playTurns(resumed, 25);
            check(fingerprint(resumed) == fingerprint(a), "reload at turn 25 continues identically");
            std::ostringstream summary;
            for (int e : empireIds(a)) summary << ' ' << slots::key(e) << ":c" << coloniesOf(a, e) << "/f" << fleetsOf(a, e);
            std::cout << "    turn " << a.turn << ", battles " << battlesA << ", fired " << a.scenarioFired.size() << ", empires" << summary.str() << std::endl;
        }, passed);
    }

    run("save: a free game has no SCENARIO section and differs from schema 24 only by the header; the section is refused under old headers", [] {
        CampaignSetup setup; setup.seed = 1999; setup.preset = RulesetPreset::Classic; setup.presetId = "classic";
        auto s = newCampaign(setup);
        economy_rules::setFlagBits(s.rules, 0);   // the legacy-header texts pin the pre-schema-23 layout
        for (int i = 0; i < 5; ++i) advance(s);
        const auto text = serialize(s);
        check(!contains(text, "SCENARIO") && !contains(text, "FLAGSHIPS") && text.starts_with(headerOf(SaveVersion)), "free game: no section, current header");
        for (int version : {16, 17, 18, 20, 21, 22, 23, 24}) {
            const auto old = withHeader(text, version);
            check(serialize(deserialize(old)) == text, "schema " + std::to_string(version) + " text loads and re-saves as the current schema");
        }
        auto m = scenarioGame("mirror-universe");
        const auto mt = serialize(m);
        check(contains(mt, "\nSCENARIO 1 mirror-universe 0\n"), "a scenario game writes the section");
        for (int version : {23, 24}) rejects([&] { deserialize(withHeader(mt, version)); }, "the section under an older header", "older save");
        rejects([&] { deserialize(replaced(mt, "SCENARIO 1 mirror-universe 0", "SCENARIO 1 nowhere 0")); }, "a forged scenario id", "");
        rejects([&] { deserialize(withChecksum(replaced(mt, "SCENARIO 1 mirror-universe 0", "SCENARIO 1 mirror-universe 1 7"))); }, "a forged fired list", "fired trigger");
        auto k = scenarioGame("klingon-civil-war");
        for (int t = 0; t < 4; ++t) advance(k);
        const auto kt = serialize(k);
        rejects([&] { deserialize(withChecksum(replaced(kt, "SECESSIONS 1\n2 8 4", "SECESSIONS 1\n2 2 4"))); }, "a forged secession", "secession");
        rejects([&] { deserialize(withChecksum(replaced(kt, "FLAGSHIPS 1\n", "FLAGSHIPS 1\n1 99999 1 0\n"))); }, "a flagship row for a ship that is not there", "");
    }, passed);

    std::cout << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
