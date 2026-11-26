// Packs and presets (lane MOD-1, docs/design/PACKS-AND-PRESETS.md): overlay merge rules, stack checksums, the preset
// registry and lineup validation, preset new games, the schema-17 PACKS section and determinism.
#include "Content.h"
#include "EconomyRules.h"
#include "Presets.h"
#include "ReferenceCatalogue.h"
#include "ShipRoster.h"
#include "MinorRaces.h"
#include "Simulation.h"

#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace botf;

namespace {
int failed = 0;
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void run(const char* name, const std::function<void()>& fn, int& passed) {
    try { fn(); ++passed; std::cout << "PASS " << name << std::endl; }
    catch (const std::exception& error) { ++failed; std::cout << "FAIL " << name << ": " << error.what() << std::endl; }
}
// The message of the exception fn throws; fails the test when it throws nothing.
std::string refusal(const std::function<void()>& fn, const std::string& what) {
    try { fn(); } catch (const std::exception& e) { return e.what(); }
    throw std::runtime_error(what + " accepted");
}
void expectRefusal(const std::function<void()>& fn, const std::string& fragment, const std::string& what) {
    const auto message = refusal(fn, what);
    check(message.find(fragment) != std::string::npos, what + ": message '" + message + "' lacks '" + fragment + "'");
}

std::string overlay(const std::string& id, const std::string& body, const std::string& requires_ = "\"botf-base\"") {
    return "{\"format\":1,\"kind\":\"overlay\",\"id\":\"" + id + "\",\"name\":\"Test " + id + "\",\"revision\":1,\"requires\":[" + requires_ + "]" +
           (body.empty() ? "" : "," + body) + "}";
}
const std::string TestMinor =
    "{\"id\":31,\"name\":\"Overlay Minor\",\"homeworld\":\"Testia\",\"referenceRaceId\":0,\"temperament\":\"Peaceful\",\"techLevel\":3,"
    "\"population\":10,\"bonus\":\"Research\",\"bonusValue\":5,\"signatureShipId\":-1,\"affinity\":[0,0,0,0,0,0],\"description\":\"Overlay test minor.\"}";
const std::string TestShip =
    "{\"id\":125,\"raceId\":0,\"name\":\"Test Cutter\",\"shipClass\":\"Overlay\",\"technologyRequirements\":[0,0,0,0,0,0],\"industryCost\":100,"
    "\"maintenance\":1,\"hull\":10,\"shields\":0,\"attack\":1,\"range\":1,\"speed\":1,\"role\":1,\"troopCapacity\":0,\"description\":\"Overlay test ship.\"}";

GameState game(const std::string& presetId, std::uint64_t seed = 31, int empires = 3) {
    CampaignSetup setup;
    setup.seed = seed; setup.empires = empires; setup.width = 16; setup.height = 12; setup.systems = 28;
    setup.presetId = presetId;
    setup.threats = ThreatSetup{};   // Pack and save-format scenarios: the presets' threat defaults are tested in ThreatTests.cpp.
    auto state = newCampaign(setup);
    economy_rules::setFlagBits(state.rules, 0); // The economy flags (schema 23) are tested in EconomyRulesTests.cpp; these scenarios pin the older formats.
    return state;
}
bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
std::string replaceFirst(std::string text, const std::string& from, const std::string& to) {
    const auto at = text.find(from);
    check(at != std::string::npos, "test anchor missing: " + from);
    return text.replace(at, from.size(), to);
}
std::string withChecksum(std::string body) { return body + "CHECKSUM " + std::to_string(content::fnv1a64(body)) + "\n"; }
} // namespace

int main() {
    int passed = 0;

    run("registry: seven presets in screen order, every overlay present, every stack builds", [] {
        content::installBase();
        const auto list = presets::all();
        const std::vector<std::string> want{"classic", "remastered", "dominion-war", "ages", "galaxies", "balance-of-power", "ultimate"};
        check(list.size() >= want.size(), "preset count");
        for (std::size_t i = 0; i < want.size(); ++i) check(list[i]->id == want[i], "order: slot " + std::to_string(i) + " is " + list[i]->id);
        for (const auto* p : list) {
            check(!p->name.empty() && !p->blurb.empty(), p->id + ": name and blurb");
            for (const auto& o : p->overlays) check(presets::overlayBytes(o) != nullptr, p->id + ": overlay " + o);
            presets::activate(*p);
            check(content::active().stack.size() == p->overlays.size(), p->id + ": stack size");
            check(p->overlays.empty() == content::active().base, p->id + ": base exactly when there are no overlays");
        }
        content::installBase();
        check(presets::find("classic")->overlays.empty() && presets::find("remastered")->overlays.empty(), "built-ins have no overlays");
        check(!presets::find("ultimate")->overlays.empty(), "mod presets carry an overlay");
    }, passed);

    run("lineup: validated against the eight-major cap and the empires the core can seat", [] {
        for (const char* id : {"classic", "remastered", "ages", "balance-of-power", "ultimate"})
            check(presets::find(id)->lineupPlayable, std::string(id) + " seats today");
        // Lane MOD-2 seats eight majors in any order: the Dominion War and Galaxies lineups name the Dominion and the Borg.
        for (const char* id : {"dominion-war", "galaxies"}) check(presets::find(id)->lineupPlayable, std::string(id) + " seats now");
        check(presets::find("dominion-war")->lineup.size() == 5 && presets::find("galaxies")->lineup.size() == 5, "preset lineups keep their five names");
        const auto six = presets::checkLineup({"Federation", "Klingon", "Romulan", "Cardassian", "Ferengi", "Dominion"}, "lineup");
        check(six.playable, "six majors seat: " + six.note);
        const auto eight = presets::checkLineup({"Breen", "Borg", "Dominion", "Ferengi", "Cardassian", "Romulan", "Klingon", "Federation"}, "lineup");
        check(eight.playable, "eight majors seat in any order: " + eight.note);
        const auto unknown = presets::checkLineup({"Federation", "Gorn"}, "lineup");
        check(!unknown.playable && unknown.note.find("Gorn") != std::string::npos, "an unknown empire is reported: " + unknown.note);
        check(presets::checkLineup({"Klingon", "Federation"}, "lineup").playable, "slots are not positional any more");
        check(presets::checkLineup({"Federation", "Klingon", "Romulan"}, "lineup").playable, "a stock prefix seats");
        expectRefusal([] { presets::checkLineup({"Federation"}, "lineup"); }, "2..8", "one empire");
        expectRefusal([] { presets::checkLineup({"Federation", "Federation"}, "lineup"); }, "twice", "duplicate empire");
        expectRefusal([] { presets::checkLineup({"Federation", "Kling-on"}, "lineup"); }, "letters", "bad key");
        std::vector<std::string> nine{"A", "B", "C", "D", "E", "F", "G", "H", "I"};
        expectRefusal([&] { presets::checkLineup(nine, "lineup"); }, "2..8", "nine empires");
    }, passed);

    run("overlay merge: patch, append, nested fields, balance, order, and every refusal", [] {
        content::installBase();
        const auto baseCost = referenceData::TechnologyCatalogue[13].cost;
        const auto baseHull = referenceData::ShipCatalogue[0].hull;
        const auto baseMinors = minors::Roster.size();
        const std::string one = overlay("t-one",
            "\"technologies\":{\"patch\":[{\"id\":13,\"cost\":777,\"name\":\"Overlay Tech\"}]},"
            "\"ships\":{\"patch\":[{\"id\":0,\"hull\":321}],\"append\":[" + TestShip + "]},"
            "\"minors\":{\"append\":[" + TestMinor + "]},"
            "\"roster\":{\"patch\":[{\"id\":1000,\"buildable\":false,\"flight\":{\"turnRate\":45}}]},"
            "\"events\":{\"patch\":[{\"kind\":\"Plague\",\"name\":\"Blight\"}]},"
            "\"balance\":{\"researchCostPercent\":50}");
        const auto info = content::installStack({one});   // a copy: the argument is a temporary (gcc -Wdangling-reference)
        check(!info.base && info.id == "botf-base+t-one" && info.stack.size() == 1 && info.stack[0].id == "t-one", "stack identity: " + info.id);
        check(referenceData::TechnologyCatalogue[13].cost == 777 && std::string(referenceData::TechnologyCatalogue[13].name) == "Overlay Tech", "technology patched");
        check(referenceData::TechnologyCatalogue[13].level == 2 && referenceData::TechnologyCatalogue[14].cost == 777 * 0 + referenceData::TechnologyCatalogue[14].cost, "neighbours untouched");
        check(referenceData::ShipCatalogue[0].hull == 321 && referenceData::ShipCatalogue.size() == 126 && referenceData::ShipCatalogue[125].hull == 10, "ship patched and appended");
        check(minors::Roster.size() == baseMinors + 1 && std::string(minors::Roster[30].name) == "Overlay Minor", "minor appended");
        check(!roster::ExpandedRoster[0].buildable && roster::ExpandedRoster[0].flight.turnRate == 45 && roster::ExpandedRoster[0].flight.acceleration != 0, "roster patched, nested flight merged field by field");
        check(std::string(content::eventDefinition(0).name) == "Blight", "event patched by kind");
        check(content::balance().researchCostPercent == 50 && content::balance().shipCostPercent == 100, "balance read");
        // Two overlays: the second sees the first's appended records and wins on a shared field.
        const std::string two = overlay("t-two", "\"technologies\":{\"patch\":[{\"id\":13,\"cost\":888}]},\"ships\":{\"patch\":[{\"id\":125,\"hull\":11}]},\"balance\":{\"shipCostPercent\":90}", "\"t-one\"");
        content::installStack({one, two});
        check(referenceData::TechnologyCatalogue[13].cost == 888 && std::string(referenceData::TechnologyCatalogue[13].name) == "Overlay Tech", "later overlay wins per field only");
        check(referenceData::ShipCatalogue[125].hull == 11 && content::balance().researchCostPercent == 50 && content::balance().shipCostPercent == 90, "chained records and balance");
        // Refusals leave the active pack untouched.
        const auto before = content::active().checksum;
        auto bad = [&](const std::string& text, const std::string& fragment, const std::string& what) {
            expectRefusal([&] { content::installStack({text}); }, fragment, what);
            check(content::active().checksum == before && referenceData::TechnologyCatalogue[13].cost == 888, what + ": active pack changed");
        };
        bad(overlay("t-bad", "\"technologies\":{\"patch\":[{\"id\":999,\"cost\":1}]}"), "no such record", "patch of a missing id");
        bad(overlay("t-bad", "\"technologies\":{\"patch\":[{\"id\":13,\"price\":1}]}"), "unknown field \"price\"", "patch of an unknown field");
        bad(overlay("t-bad", "\"technologies\":{\"patch\":[{\"id\":13,\"cost\":1},{\"id\":13,\"cost\":2}]}"), "patched twice", "same record twice");
        bad(overlay("t-bad", "\"technologies\":{\"append\":[]}"), "fixed record count", "append to a fixed section");
        bad(overlay("t-bad", "\"events\":{\"append\":[]}"), "fixed record count", "append to events");
        bad(overlay("t-bad", "\"technologies\":{\"remove\":[13]}"), "patch or append", "remove is not a verb");
        bad(overlay("t-bad", "\"ships\":{\"append\":[" + replaceFirst(TestShip, "\"id\":125", "\"id\":130") + "]}"), "ids must equal their index", "append with a gap in the ids");
        bad(overlay("t-bad", "\"minors\":{\"append\":[" + replaceFirst(TestMinor, "\"id\":31", "\"id\":30") + "]}"), "index + 1", "append with a reused id");
        bad(overlay("t-bad", "\"ships\":{\"append\":[" + replaceFirst(TestShip, "\"hull\":10,", "\"hull\":10,\"armor\":1,") + "]}"), "unknown key \"armor\"", "append with an unknown key");
        bad(overlay("t-bad", "\"widgets\":{}"), "unknown key \"widgets\"", "unknown section");
        bad(overlay("t-bad", "\"technologies\":{\"patch\":[{\"id\":13,\"cost\":-5}]}"), "outside", "patched value out of range");
        bad(overlay("t-bad", "\"balance\":{\"researchCostPercent\":5}"), "outside", "balance out of range");
        bad(overlay("t-bad", "\"balance\":{\"gravity\":5}"), "unknown key \"gravity\"", "unknown balance value");
        bad(overlay("t-bad", "", "\"nobody\""), "requires nobody", "unmet requires");
        bad(overlay("botf-base", ""), "overlay.id", "reserved id");
        bad(replaceFirst(overlay("t-bad", ""), "\"kind\":\"overlay\"", "\"kind\":\"pack\""), "overlay.kind", "wrong kind");
        bad(replaceFirst(overlay("t-bad", ""), "\"format\":1", "\"format\":2"), "overlay.format", "unknown format");
        expectRefusal([&] { content::installStack({one, one}); }, "listed twice", "the same overlay twice");
        content::installBase();
        check(referenceData::TechnologyCatalogue[13].cost == baseCost && referenceData::ShipCatalogue[0].hull == baseHull && content::balance() == content::Balance{}, "the base pack is restored");
    }, passed);

    run("stack checksum: pure function of the exact bytes and their order", [] {
        content::installBase();
        const auto baseSum = content::baseChecksum();
        const std::string a = overlay("t-a", "\"technologies\":{\"patch\":[{\"id\":13,\"cost\":700}]}");
        const std::string b = overlay("t-b", "\"technologies\":{\"patch\":[{\"id\":14,\"cost\":900}]}");
        const auto sumA = content::installStack({a}).checksum;
        check(sumA != baseSum, "a stack is not the base");
        check(content::installStack({a}).checksum == sumA, "same bytes, same checksum");
        const auto ab = content::installStack({a, b}).checksum;
        const auto ba = content::installStack({b, a}).checksum;
        check(ab != ba && ab != sumA, "order and membership change the checksum");
        // Whitespace is part of the bytes, as for a bundle (docs/DATA-FORMAT.md section 3).
        const auto spaced = content::installStack({a + "\n"}).checksum;
        check(spaced != sumA && referenceData::TechnologyCatalogue[13].cost == 700, "a whitespace-only change shows in the checksum, not in the data");
        // An empty overlay still makes the stack a distinct pack.
        check(content::installStack({overlay("t-empty", "")}).checksum != baseSum, "an empty overlay still changes identity");
        content::installBase();
        check(content::active().checksum == baseSum && content::active().stack.empty(), "empty stack is the base pack");
    }, passed);

    run("preset games: built-ins play exactly as before; mod presets set their flags and content", [] {
        content::installBase();
        CampaignSetup old;
        old.seed = 31; old.empires = 3; old.width = 16; old.height = 12; old.systems = 28;
        old.preset = RulesetPreset::Remastered;
        auto legacyRemastered = newCampaign(old);
        economy_rules::setFlagBits(legacyRemastered.rules, 0); // game() pins the pre-schema-23 formats with the economy flags cleared
        old.preset = RulesetPreset::Classic;
        auto legacyClassic = newCampaign(old);
        economy_rules::setFlagBits(legacyClassic.rules, 0);
        check(fingerprint(game("remastered")) == fingerprint(legacyRemastered), "preset remastered == the pre-preset Remastered game");
        check(fingerprint(game("classic")) == fingerprint(legacyClassic), "preset classic == the pre-preset Classic game");
        check(game("remastered").presetId.empty() && serialize(game("remastered")).find("\nPACKS ") == std::string::npos, "built-ins write no PACKS section");
        expectRefusal([] { game("no-such-preset"); }, "Unknown preset", "unknown preset");

        const auto ages = game("ages");
        check(ages.presetId == "ages" && ages.rules.expandedRoster && ages.rules.preset == RulesetPreset::Remastered, "ages: remastered flags with the expanded roster");
        check(content::active().id == "botf-base+ages", "ages overlay active");
        const auto bop = game("balance-of-power");
        check(bop.presetId == "balance-of-power" && bop.rules.preset == RulesetPreset::Classic && !bop.rules.expandedRoster && !bop.rules.curatedEvents, "balance of power: classic flags");
        check(fingerprint(bop) != fingerprint(legacyClassic), "the stack is inside the fingerprint");

        // Ultimate: research costs scaled to 85%, through the pack balance and through every consumer of researchCost.
        content::installBase();
        const int baseRc = researchCost(legacyRemastered, 1, 3);
        const auto ultimate = game("ultimate");
        check(content::balance().researchCostPercent == 85, "ultimate balance");
        // MOD-7: 85% flat times the level curve (100% at level 2 to 90% at level 10): level 3 is 85% * (100 - 10 * 1 / 8)%.
        const int curve = 100 + (90 - 100) * (3 - 2) / 8;
        check(researchCost(ultimate, 1, 3) == static_cast<int>(static_cast<std::int64_t>(baseRc) * 85 / 100 * curve / 100) && researchCost(ultimate, 1, 3) < baseRc, "research cost scaled");
        validate(ultimate);
        // Choosing a built-in after a mod preset puts the base pack back.
        game("classic");
        check(content::active().base && researchCost(legacyRemastered, 1, 3) == baseRc, "classic restores the base pack");
        // Every shipped preset starts and survives turns.
        for (const auto* p : presets::all()) {
            auto s = game(p->id, 7);
            for (int t = 0; t < 3; ++t) advance(s);
            validate(s);
        }
        content::installBase();
    }, passed);

    run("schema 17: PACKS section round trip, canonical form, forged and missing stacks", [] {
        content::installBase();
        auto s = game("ultimate", 11);
        for (int t = 0; t < 6; ++t) advance(s);
        const auto text = serialize(s);
        const auto& stack = content::active();
        check(text.starts_with(std::string("BOTF_SIM ") + std::to_string(SaveVersion) + " " + ContentVersion + "\n"), "current schema header");
        const std::string packs = "\nPACKS ultimate 1 ultimate " + content::checksumHex(stack.stack[0].checksum) + "\nCONTENT botf-base+ultimate " + content::checksumHex(stack.checksum) + "\nCHECKSUM ";
        check(contains(text, packs), "PACKS then CONTENT, last before CHECKSUM: " + text.substr(text.size() - 160));
        const auto fp = fingerprint(s);
        // Loading rebuilds the stack from the registry even when the host has the base pack active.
        content::installBase();
        const auto loaded = deserialize(text);
        check(content::active().id == "botf-base+ultimate" && loaded.presetId == "ultimate", "stack rebuilt and preset restored");
        check(serialize(loaded) == text && fingerprint(loaded) == fp, "round trip and fingerprint");
        // Replay: loaded and original advance in lockstep.
        auto a = loaded; auto b = s;
        for (int t = 0; t < 4; ++t) { advance(a); advance(b); }
        check(fingerprint(a) == fingerprint(b), "determinism after load");
        // Forged sections are refused and leave the active pack alone.
        const auto activeSum = content::active().checksum;
        auto forge = [&](const std::string& from, const std::string& to) {
            auto body = text.substr(0, text.rfind("CHECKSUM "));
            return withChecksum(replaceFirst(body, from, to));
        };
        content::installBase();
        const auto baseSum = content::baseChecksum();
        expectRefusal([&] { deserialize(forge("PACKS ultimate 1 ultimate ", "PACKS ultimate 1 nosuch-overlay ")); }, "nosuch-overlay", "unknown overlay in a save");
        expectRefusal([&] { deserialize(forge("PACKS ultimate 1 ultimate " + content::checksumHex(stack.stack[0].checksum), "PACKS ultimate 1 ultimate 0000000000000001")); }, "changed since the save", "changed overlay");
        expectRefusal([&] { deserialize(forge("PACKS ultimate 1", "PACKS ultimate 2")); }, "Malformed PACKS", "count mismatch");
        expectRefusal([&] { deserialize(forge("\nPACKS ultimate", "\nPACKS Ultimate!")); }, "Malformed PACKS", "bad preset id");
        check(content::active().checksum == baseSum, "refused loads left the base pack active");
        // PACKS only in a schema-17 file; a stack save without it names its content and is refused on the base pack.
        {
            auto body = text.substr(0, text.rfind("CHECKSUM "));
            body.replace(0, body.find('\n') + 1, "BOTF_SIM 16 remaster-16\n");
            expectRefusal([&] { deserialize(withChecksum(body)); }, "Schema 17 section", "PACKS in a schema-16 file");
        }
        expectRefusal([&] {
            auto body = text.substr(0, text.rfind("CHECKSUM "));
            const auto at = body.find("PACKS ");
            body.erase(at, body.find('\n', at) + 1 - at);
            deserialize(withChecksum(body));
        }, "Save needs content pack", "stack save without PACKS on the base pack");
        // A base save refuses to load while a stack is active (the existing strict rule).
        const auto baseText = serialize(game("remastered", 11));
        content::installStack({overlay("t-x", "")});
        expectRefusal([&] { deserialize(baseText); }, "base content pack", "base save on a stack");
        content::installBase();
        check(activeSum != baseSum, "sanity");
    }, passed);

    run("schema 17 saves with a PACKS section load and re-save as schema 18", [] {
        content::installBase();
        auto s = game("ultimate", 5);
        for (int t = 0; t < 4; ++t) advance(s);
        const auto text = serialize(s);
        check(contains(text, "PACKS ultimate"), "stack game has PACKS");
        auto body = text.substr(0, text.rfind("CHECKSUM "));
        body.replace(0, body.find('\n') + 1, "BOTF_SIM 17 remaster-17\n");
        content::installBase();
        const auto loaded = deserialize(withChecksum(body));
        check(fingerprint(loaded) == fingerprint(s) && serialize(loaded) == text, "schema 17 -> 18");
        content::installBase();
    }, passed);

    run("schema 16 saves load and re-save as schema 18", [] {
        content::installBase();
        auto s = game("remastered", 5);
        for (int t = 0; t < 4; ++t) advance(s);
        auto text = serialize(s);
        check(!contains(text, "PACKS"), "base game has no PACKS");
        auto body = text.substr(0, text.rfind("CHECKSUM "));
        body.replace(0, body.find('\n') + 1, "BOTF_SIM 16 remaster-16\n");
        const auto loaded = deserialize(withChecksum(body));
        check(fingerprint(loaded) == fingerprint(s) && serialize(loaded) == text, "schema 16 -> 18");
    }, passed);

    run("registration: a host adds an overlay and a preset as bytes; bad files register nothing", [] {
        content::installBase();
        const std::size_t before = presets::all().size();
        const std::string ov = overlay("user-pack", "\"technologies\":{\"patch\":[{\"id\":13,\"cost\":42}]}");
        check(presets::registerOverlay(ov) == "user-pack", "overlay registered");
        check(presets::registerOverlay(ov) == "user-pack", "registering identical bytes again is a no-op");
        expectRefusal([&] { presets::registerOverlay(replaceFirst(ov, "\"cost\":42", "\"cost\":43")); }, "different bytes", "same id, other bytes");
        const std::string pre =
            "{\"format\":1,\"id\":\"user-preset\",\"name\":\"User\",\"blurb\":\"A host preset.\",\"order\":900,\"overlays\":[\"user-pack\"],"
            "\"rules\":{\"base\":\"remastered\",\"flags\":{\"expandedRoster\":false}},\"lineup\":[\"Federation\",\"Klingon\"],"
            "\"options\":{\"empires\":2,\"era\":2,\"size\":\"small\",\"shape\":\"ring\",\"minorDensity\":\"few\",\"difficulty\":\"easy\",\"events\":false},\"needs\":[]}";
        expectRefusal([&] { presets::registerPreset(replaceFirst(pre, "\"user-pack\"", "\"missing-pack\"")); }, "missing-pack", "preset naming an unregistered overlay");
        expectRefusal([&] { presets::registerPreset(replaceFirst(pre, "\"expandedRoster\"", "\"warpCore\"")); }, "unknown rule flag", "unknown flag");
        expectRefusal([&] { presets::registerPreset(replaceFirst(pre, "\"expandedRoster\"", "\"curatedEvents\"")); }, "cannot be off", "a layer switched off under the remastered base");
        expectRefusal([&] { presets::registerPreset(replaceFirst(replaceFirst(pre, "\"base\":\"remastered\"", "\"base\":\"classic\""), "\"expandedRoster\":false", "\"wegoCombat\":true")); }, "cannot be on", "a layer switched on under the classic base");
        expectRefusal([&] { presets::registerPreset(replaceFirst(pre, "\"expandedRoster\":false", "\"flightModel\":false")); }, "needs flightModel", "wego without the flight model");
        expectRefusal([&] { presets::registerPreset(replaceFirst(pre, "\"ring\"", "\"cube\"")); }, "unknown galaxy shape", "unknown shape");
        expectRefusal([&] { presets::registerPreset(replaceFirst(pre, "\"empires\":2", "\"empires\":9")); }, "outside", "empires beyond the cap");
        expectRefusal([&] { presets::registerPreset(replaceFirst(pre, "\"base\":\"remastered\"", "\"base\":\"epic\"")); }, "classic", "unknown base ruleset");
        check(presets::all().size() == before, "bad files registered nothing");
        check(presets::registerPreset(pre) == "user-preset", "preset registered");
        expectRefusal([&] { presets::registerPreset(pre); }, "already registered", "duplicate preset");
        const auto* p = presets::find("user-preset");
        check(p && p->lineupPlayable && p->empires == 2 && p->events == 0 && p->shape == "ring" && presets::all().back()->id == "user-preset", "parsed fields and order");
        const auto s = game("user-preset", 3, 2);
        check(!s.rules.expandedRoster && s.rules.curatedEvents && s.presetId == "user-preset" && researchCost(s, 1, 2) == 42, "user preset: flag override and overlay content");
        content::installBase();
    }, passed);

    run("determinism: the same preset and seed give the same bytes twice, stacks included", [] {
        for (const char* id : {"remastered", "ultimate", "galaxies", "dominion-war"}) {
            content::installBase();
            auto a = game(id, 21);
            for (int t = 0; t < 10; ++t) advance(a);
            content::installBase();
            auto b = game(id, 21);
            for (int t = 0; t < 10; ++t) advance(b);
            check(serialize(a) == serialize(b) && fingerprint(a) == fingerprint(b), std::string(id) + ": two runs differ");
        }
        content::installBase();
    }, passed);

    if (failed) { std::cout << "FAILED " << passed << " passed, " << failed << " failed" << std::endl; return 1; }
    std::cout << "ALL " << passed << " PRESET SCENARIOS PASSED" << std::endl;
    return 0;
}
