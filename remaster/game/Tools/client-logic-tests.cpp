// Engine-independent unit tests for the Unreal client's asset registry, audio cue and ship database logic.
// Built and run by Tools/check-unreal-tu.sh (plain g++/clang++ against the real core; no Unreal needed).
#include "BotfAssetCatalog.h"
#include "BotfAudioCues.h"
#include "BotfAudioEvents.h"
#include "BotfJson.h"
#include "BotfShipDatabase.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static int failures = 0, checks = 0;
#define EXPECT(cond) do { ++checks; if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

using namespace botf;

static void testJson() {
    json::Value v;
    std::string err;
    EXPECT(json::parse("\xEF\xBB\xBF{\"a\": [1, 2.5, -3e2], \"b\": {\"c\": \"x\\u00e9\\n\\\"\"}, \"d\": true, \"e\": null}", v, &err));
    EXPECT(v.isObject() && v.find("a") && v.find("a")->items.size() == 3);
    EXPECT(std::fabs(v.find("a")->items[2].number + 300.0) < 1e-9);
    EXPECT(v.find("b")->stringOr("c") == "x\xC3\xA9\n\"");
    EXPECT(v.find("d")->boolean && v.find("e")->isNull());
    EXPECT(v.stringsAt("missing").empty());
    EXPECT(json::parse("\"\\ud83d\\ude80\"", v) && v.text == "\xF0\x9F\x9A\x80");
    EXPECT(!json::parse("{\"a\": }", v, &err) && !err.empty());
    EXPECT(!json::parse("[1, 2", v));
    EXPECT(!json::parse("{} x", v));
    std::string deep(100, '[');
    EXPECT(!json::parse(deep, v));
}

static void testKeys() {
    EXPECT(assets::slug("K'T'Inga") == "ktinga");
    EXPECT(assets::slug("Galaxy-X") == "galaxy_x");
    EXPECT(assets::slug("Outpost ") == "outpost");
    EXPECT(assets::slug("LI'wI'") == "liwi");
    EXPECT(assets::slug("") == "unknown");
    EXPECT(assets::shipAssetKey(24) == "ship.fed.galaxy");
    EXPECT(assets::shipAssetKey(69) == "ship.kli.vorcha");
    EXPECT(assets::shipAssetKey(89) == "ship.rom.dderidex");
    EXPECT(assets::shipAssetKey(12) == "ship.car.starbase");
    EXPECT(assets::shipAssetKey(115) == "ship.borg.cube");
    EXPECT(assets::shipAssetKey(106).empty()); // Bajoran fighter: no legacy art key
    EXPECT(assets::shipAssetKey(1000) == "ship.fed.nx");
    EXPECT(assets::shipAssetKey(-1).empty() && assets::shipAssetKey(99999).empty());
    EXPECT(assets::keyFactionPrefix("ship.kli.d7") == "kli");
    EXPECT(assets::keyFactionPrefix("weapon.Disruptor.rom") == "rom");
    EXPECT(assets::keyFactionPrefix("weapon.Disruptor").empty());
    EXPECT(assets::keyFactionPrefix("study.car") == "car");
    const auto w = assets::weaponAssetKeys(roster::WeaponType::Disruptor, "rom");
    EXPECT(w.size() == 3 && w[0] == "weapon.Disruptor.rom" && w[1] == "weapon.Disruptor" && w[2] == "weapon.generic.beam");
    EXPECT(assets::weaponAssetKeys(roster::WeaponType::QuantumTorpedo, "").back() == "weapon.generic.torpedo");
    EXPECT(assets::weaponAssetKeys(roster::WeaponType::None, "fed").empty());
    const auto classic = assets::shipWeapons(24, 3);
    EXPECT(classic.beam == roster::WeaponType::Disruptor && classic.torpedo == roster::WeaponType::PlasmaTorpedo);
    const auto nx = assets::shipWeapons(1000, 1);
    EXPECT(nx.beam == roster::WeaponType::PhaseCannon && nx.torpedo == roster::WeaponType::PhotonicTorpedo);
    EXPECT(assets::displayRadiusForLength(0) == 80.0);
    EXPECT(std::fabs(assets::displayRadiusForLength(350) - 80.0) < 1e-9);
    EXPECT(assets::displayRadiusForLength(5) == 40.0 && assets::displayRadiusForLength(100000) == 170.0);
    EXPECT(assets::displayRadiusForLength(642) > assets::displayRadiusForLength(170));
    EXPECT(std::fabs(assets::uniformScaleFor(0, 40) - 2.0) < 1e-9);
    EXPECT(assets::normaliseObjectPath("/Game/X/Mesh") == "/Game/X/Mesh.Mesh");
    EXPECT(assets::normaliseObjectPath("/Game/X/Mesh.Mesh") == "/Game/X/Mesh.Mesh");
    EXPECT(assets::builtInShipPlaceholder("kli") == "/Game/Presentation/Klingon/StaticMeshes/Klingon.Klingon");
    EXPECT(assets::builtInShipPlaceholder("borg").empty());
}

static const char* ResolvedSample = R"({
 "generated": "2026-09-28T12:00:00",
 "ueContentRoot": "/Game/Local",
 "assets": {
  "study.fed": {"kind": "model", "status": "in-repo", "ueAsset": "/Game/Presentation/Federation/StaticMeshes/Federation", "fallback": null},
  "ship.fed.galaxy": {"kind": "model", "status": "resolved", "faction": "Federation", "botfClasses": ["Galaxy", "Command Cruiser"],
    "targetLengthMeters": 642, "ueAssets": ["/Game/Local/Ships/fed/galaxy/ship_fed_galaxy.ship_fed_galaxy"], "fallback": "study.fed"},
  "ship.fed.galaxy_x": {"kind": "model", "status": "missing", "targetLengthMeters": 700, "fallback": "ship.fed.galaxy"},
  "ship.fed.nx": {"kind": "model", "status": "missing", "fallback": "ship.fed.excelsior"},
  "ship.fed.excelsior": {"kind": "model", "status": "convert-failed", "ueAssets": ["/Game/Local/Ships/fed/excelsior/x.x"], "fallback": "study.fed"},
  "ship.kli.vorcha": {"kind": "model", "status": "missing", "fallback": "ship.kli.neghvar"},
  "ship.kli.neghvar": {"kind": "model", "status": "missing", "fallback": "ship.kli.vorcha"},
  "ship.rom.x": {"kind": "model", "status": "resolved", "ueAssets": [], "fallback": null},
  "weapon.Disruptor": {"kind": "sfx", "status": "resolved", "ueAssets": ["/Game/Local/Audio/Weapons/Disruptor/a.a"], "fallback": "weapon.generic.beam"},
  "weapon.generic.beam": {"kind": "sfx", "status": "missing", "fallback": null},
  "vo.advisor.fed": {"kind": "voice", "status": "resolved", "ueAssets": ["/Game/Local/Audio/Voice/advisor_fed/l1.l1", "/Game/Local/Audio/Voice/advisor_fed/l2.l2"], "fallback": null},
  "sfx.ui.beep": {"kind": "sfx", "status": "resolved", "ueAssets": "/Game/Local/Audio/UI/beep/beep", "fallback": null}
 }
})";

static void testCatalog() {
    assets::Catalog c;
    std::string err;
    EXPECT(c.loadFromJson(ResolvedSample, &err));
    EXPECT(c.size() == 12);
    auto r = c.resolve("ship.fed.galaxy");
    EXPECT(!r.placeholder && r.resolvedKey == "ship.fed.galaxy" && r.primaryPath() == "/Game/Local/Ships/fed/galaxy/ship_fed_galaxy.ship_fed_galaxy");
    EXPECT(r.targetLengthMeters == 642);
    r = c.resolve("ship.fed.galaxy_x");
    EXPECT(r.resolvedKey == "ship.fed.galaxy" && r.targetLengthMeters == 700 && r.chain.size() == 2);
    r = c.resolve("ship.fed.nx"); // excelsior failed to convert -> in-repo study mesh
    EXPECT(r.resolvedKey == "study.fed" && r.primaryPath() == "/Game/Presentation/Federation/StaticMeshes/Federation.Federation");
    r = c.resolve("ship.kli.vorcha"); // cycle stops, placeholder
    EXPECT(r.placeholder && r.chain.size() == 2 && r.factionPrefix == "kli");
    EXPECT(c.resolve("ship.rom.x").placeholder); // resolved but no object path is not usable
    r = c.resolve("weapon.Disruptor.gorn"); // unknown variant -> parent key
    EXPECT(!r.placeholder && r.resolvedKey == "weapon.Disruptor");
    EXPECT(c.resolve("weapon.Nothing").placeholder);
    EXPECT(c.resolve("").placeholder);
    EXPECT(c.resolve("sfx.ui.beep").primaryPath() == "/Game/Local/Audio/UI/beep/beep.beep");
    EXPECT(c.resolve("vo.advisor.fed").objectPaths.size() == 2);
    r = c.resolveFirst({"weapon.PhaserArray.fer", "weapon.Disruptor"});
    EXPECT(r.resolvedKey == "weapon.Disruptor");
    // Ships by id, down to built-in placeholders.
    r = c.resolveShip(24, 1);
    EXPECT(r.resolvedKey == "ship.fed.galaxy");
    r = c.resolveShip(69, 2); // Vor'cha: nothing resolves -> Klingon built-in mesh
    EXPECT(r.placeholder && r.primaryPath() == assets::builtInShipPlaceholder("kli"));
    r = c.resolveShip(106, 4); // no key -> owner's study key -> built-in
    EXPECT(r.placeholder && r.primaryPath() == assets::builtInShipPlaceholder("car"));
    r = c.resolveShip(1060, 0); // roster minor without art and unknown owner
    // Id 1060 may or may not exist; either way a non-major key gives no built-in mesh.
    EXPECT(r.placeholder);
    // An empty catalogue (no LocalAssets on this PC) still gives the built-in faction meshes.
    assets::Catalog empty;
    EXPECT(empty.resolveShip(24, 1).primaryPath() == assets::builtInShipPlaceholder("fed"));
    EXPECT(!empty.loadFromJson("{\"nope\": 1}", &err) && !err.empty());
    EXPECT(!empty.loadFromJson("not json", &err));
}

static void testManifestCoverage() {
    // Every roster key must be known to the (source) manifest; parse it with the same reader as the client.
    std::ifstream in(BOTF_MANIFEST_PATH);
    std::stringstream buffer;
    buffer << in.rdbuf();
    json::Value manifest;
    EXPECT(json::parse(buffer.str(), manifest));
    const json::Value* entries = manifest.find("assets");
    EXPECT(entries && entries->isObject());
    if (!entries) return;
    int missing = 0;
    for (const auto& rc : roster::ExpandedRoster)
        if (!entries->find(rc.assetKey)) { ++missing; std::printf("  manifest lacks %s\n", rc.assetKey); }
    for (const auto& s : referenceData::ShipCatalogue) {
        const std::string key = assets::shipAssetKey(s.id);
        if (!key.empty() && !entries->find(key)) { ++missing; std::printf("  manifest lacks %s\n", key.c_str()); }
    }
    EXPECT(missing == 0);
}

static void testCues() {
    using namespace audio;
    auto r = cueFor(GameCue::WarDeclared, "fed", "kli");
    EXPECT(r.priority == 9 && r.sfxKeys.front() == "sfx.alert.red" && r.voiceKeys.front() == "vo.diplomacy.kli");
    r = cueFor(GameCue::OutpostBuilt, "rom");
    EXPECT(r.voiceKeys.size() == 2 && r.voiceKeys[0] == "vo.station.rom.built" && r.voiceKeys[1] == "vo.advisor.rom");
    r = cueFor(GameCue::OrderAccepted, "car", {}, "ram");
    EXPECT(r.voiceKeys[0] == "vo.combat.ram.car" && r.voiceKeys[1] == "vo.combat.ack.car" && r.priority == 6);
    r = cueFor(GameCue::GalacticEvent, "fer", {}, "12");
    EXPECT(r.voiceKeys[0] == "vo.event.12" && r.voiceKeys[1] == "vo.advisor.fer");
    // Voice never crosses factions: every voice key names the player's or the speaker's faction.
    for (int c = 0; c <= static_cast<int>(GameCue::UiClick); ++c)
        for (const auto& k : cueFor(static_cast<GameCue>(c), "fed", "kli").voiceKeys)
            EXPECT(k.find(".fed") != std::string::npos || k.find(".kli") != std::string::npos || k.rfind("vo.event.", 0) == 0);
    EXPECT(cueFor(GameCue::Victory, "fed").priority == 10);
    EXPECT(cueFor(GameCue::Cloak, "fed", "kli").sfxKeys[0] == "sfx.cloak.kli");
    EXPECT(cueFor(GameCue::BeamFire, "fed", {}, "weapon.generic.beam").sfxKeys.size() == 1);
    EXPECT(cueFor(GameCue::FirstContact, "fed", "rom").beepIfNoVoice);

    MixSettings s;
    EXPECT(categoryGain(s, Category::Sfx, false) == 1.0f);
    EXPECT(std::fabs(categoryGain(s, Category::Sfx, true) - 0.45f) < 1e-6f);
    EXPECT(categoryGain(s, Category::Voice, true) == 1.0f);
    s.master = 0.5f; s.category[static_cast<int>(Category::Music)] = 2.0f;
    EXPECT(std::fabs(categoryGain(s, Category::Music, false) - 0.5f) < 1e-6f);
    EXPECT(categoryGain(s, Category::Count, false) == 0.0f);

    CueScheduler q;
    q.settings.maxSfxVoices = 2;
    auto d1 = q.requestSfx("sfx.explosion.small", Category::Sfx, 2, 0.0);
    EXPECT(d1.play && d1.cullHandle == 0);
    EXPECT(!q.requestSfx("sfx.explosion.small", Category::Sfx, 2, 0.05).play); // cooldown
    auto d2 = q.requestSfx("sfx.shield.hit", Category::Sfx, 5, 0.06);
    EXPECT(d2.play);
    auto d3 = q.requestSfx("sfx.explosion.large", Category::Sfx, 5, 0.07); // full: culls the priority-2 sound
    EXPECT(d3.play && d3.cullHandle == d1.handle && q.activeSfx() == 2);
    EXPECT(!q.requestSfx("weapon.Disruptor", Category::Sfx, 1, 0.08).play); // lower than everything playing
    q.sfxFinished(d2.handle);
    EXPECT(q.activeSfx() == 1);
    EXPECT(q.requestSfx("weapon.Disruptor", Category::Sfx, 1, 0.09).play);
    q.pruneFinished(100.0);
    EXPECT(q.activeSfx() == 0);
    EXPECT(q.requestSfx("sfx.alert.red", Category::Sfx, 8, 10.0).play && !q.requestSfx("sfx.alert.red", Category::Sfx, 8, 12.0).play);

    CueScheduler v;
    VoiceLine line;
    EXPECT(v.requestVoice("vo.advisor.fed", 4, 0.0));
    EXPECT(v.requestVoice("vo.diplomacy.kli", 9, 0.1));
    EXPECT(!v.requestVoice("vo.advisor.fed", 4, 0.2)); // already queued
    EXPECT(v.nextVoice(0.2, false, line) && line.key == "vo.diplomacy.kli"); // priority first
    EXPECT(v.voiceActive());
    EXPECT(!v.nextVoice(0.3, true, line)); // channel busy
    EXPECT(!v.nextVoice(1.0, false, line)); // gap after the line ended
    EXPECT(v.nextVoice(1.7, false, line) && line.key == "vo.advisor.fed");
    EXPECT(!v.requestVoice("vo.advisor.fed", 4, 2.0)); // voice cooldown
    EXPECT(v.nextVoice(3.0, false, line) == false);
    v.settings.maxVoiceQueue = 2;
    EXPECT(v.requestVoice("a", 3, 10.0) && v.requestVoice("b", 2, 10.0));
    EXPECT(!v.requestVoice("c", 1, 10.0)); // queue full of higher priority
    EXPECT(v.requestVoice("d", 7, 10.0) && v.queuedVoices() == 2); // evicts b
    EXPECT(!v.nextVoice(30.0, false, line) && v.queuedVoices() == 0); // stale lines dropped
    EXPECT(v.pickVariant("k", 3) == 0 && v.pickVariant("k", 3) == 1 && v.pickVariant("k", 3) == 2 && v.pickVariant("k", 3) == 0);
    EXPECT(v.pickVariant("x", 1) == 0);
}

static void testTurnCues() {
    GameState state;
    state.playerEmpire = 1;
    TurnReport report;
    ChronicleEntry research; research.category = ChronicleCategory::Research; research.empireId = 1; research.text = "Warp theory";
    ChronicleEntry war; war.category = ChronicleCategory::Diplomacy; war.empireId = 2; war.otherEmpireId = 1; war.text = "The Klingon Empire declared war";
    ChronicleEntry other; other.category = ChronicleCategory::Construction; other.empireId = 3; other.text = "Romulan shipyard";
    report.chronicle = {research, war, other, research};
    const auto cues = audio::turnCues(state, report);
    EXPECT(cues.size() == 3);
    EXPECT(cues[0].cue == audio::GameCue::WarDeclared && cues[0].other == "kli");
    EXPECT(cues[1].cue == audio::GameCue::ResearchComplete && cues[2].cue == audio::GameCue::TurnStart);
}

static void testShipDatabase() {
    const auto& ids = view::shipDatabaseIds();
    EXPECT(ids.size() == referenceData::ShipCatalogue.size() + roster::ExpandedRoster.size());
    EXPECT(ids.front() == 0 && ids.back() == roster::ExpandedRoster.back().id);
    for (int id : ids) EXPECT(roster::findShip(id) != nullptr);
    EXPECT(view::shipDatabaseLabel(24).find("Federation") != std::string::npos);
    const std::string label = view::shipDatabaseLabel(1000);
    EXPECT(label.find("NX") != std::string::npos && label.find("roster") != std::string::npos);
    const std::string detail = view::shipDatabaseDetails(1000, {"Biotech", "Computers", "Construction", "Energy", "Propulsion", "Weapons"});
    EXPECT(detail.find("Weapons: 4x PhaseCannon") != std::string::npos && detail.find("ship.fed.nx") != std::string::npos);
    EXPECT(detail.find("Biotech 1") != std::string::npos);
    EXPECT(view::shipDatabaseDetails(24, {}).find("Asset key") == std::string::npos);
    EXPECT(view::shipDatabaseDetails(-5, {}) == "Select an entry.");
}

int main() {
    testJson();
    testKeys();
    testCatalog();
    testManifestCoverage();
    testCues();
    testTurnCues();
    testShipDatabase();
    std::printf("client logic: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
