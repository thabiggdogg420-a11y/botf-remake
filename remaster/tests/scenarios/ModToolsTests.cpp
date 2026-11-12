// Mod tools (lane MOD-10, docs/design/MOD-TOOLS.md): user packs through the registry the pack editor drives. packPart (the merged
// JSON an editor shows), validatePack (the core's own overlay and preset validators, nothing registered), installUserPack (create,
// replace), removeUserPack, the refusal of a shipped id, and the round trip: a user pack starts a game, plays 20 turns, saves,
// reloads, and a save refuses to load when its pack is missing or changed. Classic and Remastered must stay identical.
#include "Content.h"
#include "Presets.h"
#include "ReferenceCatalogue.h"
#include "ShipRoster.h"
#include "Simulation.h"

#include <functional>
#include <iostream>
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
std::string refusal(const std::function<void()>& fn, const std::string& what) {
    try { fn(); } catch (const std::exception& e) { return e.what(); }
    throw std::runtime_error(what + " accepted");
}
void expectRefusal(const std::function<void()>& fn, const std::string& fragment, const std::string& what) {
    const auto message = refusal(fn, what);
    check(message.find(fragment) != std::string::npos, what + ": message '" + message + "' lacks '" + fragment + "'");
}
bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

std::string overlayText(const std::string& id, int shipCost, int weaponPct, const std::string& extra = "") {
    return "{\"format\":1,\"kind\":\"overlay\",\"id\":\"" + id + "\",\"name\":\"User " + id + "\",\"revision\":1,\"requires\":[\"botf-base\"],\n"
           "\"balance\":{\"weaponOutputPercent\":" + std::to_string(weaponPct) + "},\n"
           "\"roster\":{\"patch\":[{\"id\":1000,\"industryCost\":" + std::to_string(shipCost) + ",\"hull\":140}]}" + extra + "}\n";
}
std::string presetText(const std::string& id, const std::string& overlays = "", const std::string& flags = "\"expandedRoster\":true") {
    return "{\"format\":1,\"id\":\"" + id + "\",\"name\":\"User " + id + "\",\"blurb\":\"A pack made in the editor.\",\"order\":900,\"overlays\":[" +
           (overlays.empty() ? "\"" + id + "\"" : overlays + ",\"" + id + "\"") + "],\"rules\":{\"base\":\"remastered\",\"flags\":{" + flags +
           "}},\"lineup\":[\"Federation\",\"Klingon\",\"Romulan\",\"Cardassian\",\"Ferengi\"],\"options\":{\"empires\":3,\"era\":1,\"size\":\"small\"},\"needs\":[]}\n";
}
std::string replaceFirst(std::string text, const std::string& from, const std::string& to) {
    const auto at = text.find(from);
    check(at != std::string::npos, "test anchor missing: " + from);
    return text.replace(at, from.size(), to);
}

GameState game(const std::string& presetId, std::uint64_t seed = 31, int empires = 3) {
    CampaignSetup setup;
    setup.seed = seed; setup.empires = empires; setup.width = 16; setup.height = 12; setup.systems = 28;
    setup.presetId = presetId;
    setup.threats = ThreatSetup{};
    return newCampaign(setup);
}
std::uint64_t regHash() {   // a stable view of the registry: every preset id, then the user packs' checksums
    std::string t;
    for (const auto* p : presets::all()) t += p->id + ",";
    for (const auto& u : presets::userPacks()) t += u.id + ":" + content::checksumHex(u.checksum) + ",";
    return content::fnv1a64(t);
}
} // namespace

int main() {
    int passed = 0;

    run("packPart returns the merged JSON an editor shows", [] {
        content::installBase();
        const auto none = presets::packPart({}, "balance");
        check(none == "null", "the base pack has no balance block: " + none);
        const auto ult = presets::packPart({"ultimate"}, "balance");
        check(contains(ult, "\"researchCostPercent\":85"), "ultimate's balance shows: " + ult);
        const auto roster = presets::packPart({}, "roster");
        check(roster.size() > 1000 && roster.front() == '[' && roster.back() == ']' && contains(roster, "\"className\":\"NX\""), "the roster section");
        const auto eras = presets::packPart({"ages"}, "eras");
        check(contains(eras, "\"techCap\":14") && contains(eras, "\"years\":[2240,2280,2320,2350,2380]"), "ages' eras block: " + eras.substr(0, 120));
        const auto range = presets::packPart({}, "roster");
        check(contains(range, "\"range\":2,"), "integers stay integers");
        expectRefusal([] { presets::packPart({"no-such-overlay"}, "roster"); }, "not registered", "unknown overlay");
        check(presets::packPart({"ages"}, "events") != "null", "events section");
    }, passed);

    run("validatePack uses the core's own validators and registers nothing", [] {
        content::installBase();
        const auto h = regHash();
        const auto ok = presets::validateUserPack(overlayText("ut-a", 1500, 90), presetText("ut-a"));
        check(ok.id == "ut-a" && ok.stackId == "botf-base+ut-a" && ok.checksum.size() == 16, "ok summary " + ok.stackId);
        check(regHash() == h && presets::userPacks().empty(), "validate registered nothing");
        auto bad = [&](const std::string& ov, const std::string& pre, const std::string& fragment, const std::string& what) {
            expectRefusal([&] { presets::validateUserPack(ov, pre); }, fragment, what);
            check(regHash() == h, what + ": registry changed");
        };
        bad(replaceFirst(overlayText("ut-a", 1500, 90), "\"hull\":140", "\"hull\":-5"), presetText("ut-a"), "outside", "a hull below its range");
        bad(replaceFirst(overlayText("ut-a", 1500, 90), "\"hull\":140", "\"armour\":5"), presetText("ut-a"), "unknown field \"armour\"", "an unknown field");
        bad(replaceFirst(overlayText("ut-a", 1500, 90), "\"id\":1000", "\"id\":1999"), presetText("ut-a"), "no such record", "a missing class");
        bad(overlayText("ut-a", 1500, 5), presetText("ut-a"), "outside", "balance out of range");
        bad(overlayText("ut-a", 1500, 90), presetText("ut-b"), "must list the pack's overlay", "a preset that does not list the pack");
        bad(overlayText("ut-a", 1500, 90), replaceFirst(presetText("ut-a", "\"ultimate\""), "[\"ultimate\",\"ut-a\"]", "[\"ut-a\",\"ultimate\"]"), "must list", "the pack's overlay not last in the stack");
        bad(overlayText("ut-a", 1500, 90), presetText("ut-a", "\"missing-pack\""), "not registered", "an unregistered overlay under it");
        bad(overlayText("ut-a", 1500, 90), presetText("ut-a", "", "\"expandedRoster\":true,\"eras\":true"), "eras block", "an eras flag with no eras block");
        bad(overlayText("ut-a", 1500, 90), presetText("ut-a", "", "\"stations\":false"), "cannot be off", "a layer switched off under Remastered");
        bad(overlayText("ultimate", 1500, 90), presetText("ultimate"), "shipped pack", "a shipped overlay id");
        bad(overlayText("ut-a", 1500, 90), replaceFirst(presetText("remastered", "", ""), "\"overlays\":[\"remastered\"]", "\"overlays\":[\"ut-a\"]"), "shipped preset", "a shipped preset id");
        bad(overlayText("botf-base", 1500, 90), presetText("botf-base"), "overlay.id", "the reserved id");
        bad("{not json", presetText("ut-a"), "JSON", "text that is not JSON");
        // With the pack ahead of an eras overlay: a later overlay's field wins, and Ages under it passes the eras flag.
        const auto under = presets::validateUserPack(overlayText("ut-a", 1500, 90), presetText("ut-a", "\"ages\"", "\"expandedRoster\":true,\"eras\":true"));
        check(under.stackId == "botf-base+ages+ut-a", "a user pack on top of a shipped one: " + under.stackId);
    }, passed);

    run("installUserPack creates, replaces and removes a user pack; shipped packs are never touched", [] {
        content::installBase();
        const auto shipped = presets::all().size();
        check(presets::installUserPack(overlayText("ut-b", 1500, 90), presetText("ut-b")) == "ut-b", "created");
        check(presets::all().size() == shipped + 1 && presets::find("ut-b") != nullptr && presets::isUserPreset("ut-b"), "listed");
        check(!presets::isUserPreset("ultimate") && !presets::isUserPreset("classic"), "shipped presets are not user presets");
        const auto first = presets::userPacks();
        check(first.size() == 1 && first[0].id == "ut-b" && first[0].hasPreset, "userPacks lists it");
        check(presets::installUserPack(overlayText("ut-b", 1700, 90), presetText("ut-b")) == "ut-b", "replaced by its author");
        check(presets::all().size() == shipped + 1, "replace does not add a second preset");
        const auto second = presets::userPacks();
        check(second.size() == 1 && second[0].checksum != first[0].checksum, "the checksum follows the bytes");
        check(contains(presets::packPart({"ut-b"}, "roster"), "\"industryCost\":1700"), "packPart sees the new bytes");
        check(!presets::removeUserPack("ultimate") && !presets::removeUserPack("classic"), "a shipped pack cannot be removed");
        check(presets::find("ultimate") != nullptr && presets::overlayBytes("ultimate") != nullptr, "ultimate still there");
        check(presets::removeUserPack("ut-b") && presets::find("ut-b") == nullptr && presets::overlayBytes("ut-b") == nullptr, "removed");
        check(!presets::removeUserPack("ut-b") && presets::all().size() == shipped, "removing twice is a no-op");
    }, passed);

    run("a user pack round trip: new game, 20 turns, save, reload, tamper, missing", [] {
        content::installBase();
        const auto classicFp = fingerprint(game("classic"));
        const auto remasteredFp = fingerprint(game("remastered"));
        const std::string ov = overlayText("ut-c", 1500, 90);
        presets::installUserPack(ov, presetText("ut-c"));
        check(fingerprint(game("classic")) == classicFp && fingerprint(game("remastered")) == remasteredFp, "Classic and Remastered are identical with a user pack registered");
        auto s = game("ut-c");
        check(s.presetId == "ut-c" && content::active().id == "botf-base+ut-c", "the user preset's stack is active");
        check(content::balance().weaponOutputPercent == 90, "the balance value is in force");
        bool cost = false;
        for (const auto& c : roster::ExpandedRoster) if (c.id == 1000) cost = c.industryCost == 1500 && c.hull == 140;
        check(cost, "the patched class is in force");
        for (int t = 0; t < 20; ++t) advance(s);
        const auto text = serialize(s);
        check(contains(text, "PACKS ut-c 1 ut-c " + content::checksumHex(content::fnv1a64(ov))), "the save records the pack and the checksum of its bytes");
        const auto fp = fingerprint(s);
        content::installBase();
        auto loaded = deserialize(text);
        check(fingerprint(loaded) == fp && serialize(loaded) == text, "reload gives the saved state");
        auto a = s, b = loaded;
        for (int t = 0; t < 4; ++t) { advance(a); advance(b); }
        check(fingerprint(a) == fingerprint(b), "the reloaded game plays on in lockstep");
        // Tampered: the author saves the pack again with other bytes.
        presets::installUserPack(overlayText("ut-c", 1501, 90), presetText("ut-c"));
        content::installBase();
        expectRefusal([&] { deserialize(text); }, "overlay ut-c changed since the save", "a changed pack");
        check(content::active().base, "a refused load leaves the active pack alone");
        // Edited back to the same bytes: loads again.
        presets::installUserPack(ov, presetText("ut-c"));
        check(fingerprint(deserialize(text)) == fp, "restoring the exact bytes restores the save");
        // Missing: the pack is gone.
        presets::removeUserPack("ut-c");
        content::installBase();
        expectRefusal([&] { deserialize(text); }, "needs overlay ut-c, which is not installed", "a missing pack");
        content::installBase();
        check(fingerprint(game("classic")) == classicFp && fingerprint(game("remastered")) == remasteredFp, "Classic and Remastered fingerprints unchanged at the end");
    }, passed);

    std::cout << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
