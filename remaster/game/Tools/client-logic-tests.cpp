// Engine-independent unit tests for the Unreal client's asset registry, audio cue and ship database logic.
// Built and run by Tools/check-unreal-tu.sh (plain g++/clang++ against the real core; no Unreal needed).
#include "BotfAssetCatalog.h"
#include "BotfAudioCues.h"
#include "BotfAudioEvents.h"
#include "BotfCombatTimeline.h"
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
    // True relative scale: a 642 m Galaxy is 642/170 Defiants long; craft under 60 m show at 60 m; giants stop at 1000 units.
    EXPECT(std::fabs(assets::displayLengthForLength(350) - 120.0) < 1e-9);
    EXPECT(std::fabs(assets::displayLengthForLength(642) / assets::displayLengthForLength(170) - 642.0 / 170.0) < 1e-9);
    EXPECT(assets::displayLengthForLength(15) == assets::displayLengthForLength(60));
    EXPECT(std::fabs(assets::displayLengthForLength(100000) - 1000.0) < 1e-9);
    EXPECT(std::fabs(assets::scaleForCanonLength(350, 60) - 2.0) < 1e-9);
    EXPECT(std::fabs(assets::uniformScaleFor(350, 999, 60) - 2.0) < 1e-9);  // length wins over the bounding sphere
    EXPECT(std::fabs(assets::uniformScaleFor(0, 40) - 2.0) < 1e-9);          // unknown length keeps radius 80
    EXPECT(assets::normaliseObjectPath("/Game/X/Mesh") == "/Game/X/Mesh.Mesh");
    EXPECT(assets::normaliseObjectPath("/Game/X/Mesh.Mesh") == "/Game/X/Mesh.Mesh");
    EXPECT(assets::builtInShipPlaceholder("kli") == "/Game/Presentation/Klingon/StaticMeshes/Klingon.Klingon");
    EXPECT(assets::builtInShipPlaceholder("borg").empty());
    EXPECT(assets::generatedShipPath("ship.fed.explorer") == "/Game/Generated/Ships/fed/SM_explorer.SM_explorer");
    EXPECT(assets::generatedShipPath("ship.kli.k-vort.b") == "/Game/Generated/Ships/kli/SM_k_vort_b.SM_k_vort_b");
    EXPECT(assets::generatedShipPath("study.fed").empty() && assets::generatedShipPath("ship.fed").empty());
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
    for (int c = 0; c <= static_cast<int>(GameCue::CombatBark); ++c)
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

// ---------------------------------------------------------------------------------------------------------------
// Rules 3 battle playback schedule (BotfCombatTimeline.h)
// ---------------------------------------------------------------------------------------------------------------

static CombatEvent ev(int tick, CombatEventKind kind, int fleet) {
    CombatEvent e;
    e.tick = tick; e.kind = kind; e.fleetId = fleet;
    return e;
}
static CombatEvent pose(int tick, CombatEventKind kind, int fleet, int x, int y, int facing, int value = 0) {
    CombatEvent e = ev(tick, kind, fleet);
    e.x = x; e.y = y; e.facing = facing; e.value = value;
    return e;
}
static CombatEvent fire(int tick, int shooter, int target, int mount, roster::WeaponType type, int impactTick, int projectiles,
                        bool hit, int shield, int hull, bool destroyed = false, bool intercepted = false) {
    CombatEvent e = ev(tick, CombatEventKind::Fire, shooter);
    e.targetFleetId = target; e.mountIndex = mount; e.weaponType = static_cast<int>(type);
    e.weaponFamily = static_cast<int>(roster::weaponFamily(type)); e.impactTick = impactTick; e.projectiles = projectiles;
    e.hit = hit; e.shieldDamage = shield; e.hullDamage = hull; e.destroyed = destroyed; e.intercepted = intercepted;
    return e;
}

static void testCombatTimeline() {
    using namespace combatfx;
    using W = roster::WeaponType;
    using K = CombatEventKind;
    // Fleet 1: player (Federation) strafes; 2: Klingon, dies at tick 8; 3: Romulan, starts cloaked and decloaks;
    // 4: Klingon, retreats; 6: Klingon, rams.
    std::vector<ShipInfo> ships = {
        {1, 1, 100, 100, false, 0, 0, 0}, {2, 2, 50, 50, false, 6, 0, 180}, {3, 3, 40, 40, true, 6, 3, 180},
        {4, 2, 20, 20, false, 7, -2, 180}, {6, 2, 10, 10, false, 3, 1, 180}};
    std::vector<CombatEvent> events;
    events.push_back(ev(0, K::Cloak, 3));
    events.push_back(pose(0, K::Disengage, 4, 7, -2, 180));
    events.push_back(pose(0, K::Retreated, 4, 7, -2, 180));
    events.push_back(pose(0, K::ManeuverStart, 1, 0, 0, 0));
    events.push_back(pose(0, K::ManeuverStart, 2, 6, 0, 180));
    events.push_back(pose(4, K::ManeuverEnd, 1, 2, 1, 45, 1));
    events.push_back(pose(4, K::ManeuverEnd, 2, 4, 0, 135));
    events.push_back(pose(4, K::Decloak, 3, 6, 3, 180));
    events.push_back(fire(5, 1, 2, 0, W::PhaserArray, 5, 2, true, 30, 0));          // 8: beam, 2 emitters
    events.push_back(fire(5, 2, 1, 0, W::Disruptor, 5, 1, true, 30, 0));            // 9
    { auto r = ev(5, K::Ram, 6); r.targetFleetId = 1; r.impactTick = 5; r.hullDamage = 10; events.push_back(r); } // 10
    events.push_back(fire(6, 1, 2, 1, W::PhotonTorpedo, 8, 2, true, 20, 40, true)); // 11: kills 2 at tick 8
    events.push_back(fire(6, 3, 1, 0, W::PulsePhaser, 6, 1, true, 30, 0));          // 12: player shields 70 -> 40
    events.push_back(fire(7, 3, 1, 0, W::PulsePhaser, 7, 1, false, 0, 0));          // 13: miss
    events.push_back(fire(8, 2, 1, 1, W::PlasmaTorpedo, 10, 1, false, 0, 0, false, true)); // 14: intercepted
    events.push_back(ev(8, K::Destroyed, 2));                                        // 15
    { auto t = ev(11, K::Truce, 0); events.push_back(t); }                           // 16

    TimelineConfig cfg;
    cfg.roundSeconds = 6.0;
    cfg.playerEmpire = 1;
    const auto tl = buildRoundTimeline(events, ships, cfg);
    const double tk = tl.tickSeconds;
    EXPECT(std::fabs(tk - 0.5) < 1e-12);
    EXPECT(tl.consumed.size() == events.size() && tl.allConsumed());

    // Ordering: jobs by start, sounds and barks by time.
    for (std::size_t i = 1; i < tl.jobs.size(); ++i) EXPECT(tl.jobs[i - 1].start <= tl.jobs[i].start);
    for (std::size_t i = 1; i < tl.sounds.size(); ++i) EXPECT(tl.sounds[i - 1].time <= tl.sounds[i].time);
    for (const auto& j : tl.jobs) EXPECT(j.duration > 0.0 && j.start >= 0.0 && j.end() <= tl.endSeconds + 1e-9);
    EXPECT(tl.endSeconds >= tl.roundSeconds);

    // Determinism: same input, identical schedule.
    const auto again = buildRoundTimeline(events, ships, cfg);
    EXPECT(again.jobs == tl.jobs && again.sounds == tl.sounds && again.barks == tl.barks);
    for (double t = 0.0; t <= 6.0; t += 0.37) EXPECT(again.track(1)->poseAt(t) == tl.track(1)->poseAt(t));

    // Flight: exact poses at the ends, no snapping in between, banking into the slide and back to level.
    const auto* t1 = tl.track(1);
    EXPECT(t1 && t1->segments.size() == 1 && t1->segments[0].slideCells == 1);
    // The plane part of a pose (height and pitch are the 3D flight layer, checked below).
    const auto plane = [](PlanePose p) { p.altitude = 0.0; p.pitch = 0.0; return p; };
    EXPECT(plane(t1->poseAt(0.0)) == (PlanePose{0, 0, 0, 0}));
    EXPECT(plane(t1->poseAt(4 * tk)) == (PlanePose{2, 1, 45, 0}));
    EXPECT(plane(t1->poseAt(6.0)) == (PlanePose{2, 1, 45, 0}));
    EXPECT(std::fabs(t1->poseAt(2 * tk).bank) > 5.0 && std::fabs(t1->poseAt(2 * tk).bank) <= cfg.maxBankDegrees);
    for (int fleet : {1, 2}) {
        const auto* tr = tl.track(fleet);
        PlanePose prev = tr->poseAt(0.0);
        double maxStep = 0.0, maxTurn = 0.0, maxBankStep = 0.0;
        for (double t = 0.001; t <= 6.0; t += 0.001) {
            const auto p = tr->poseAt(t);
            maxStep = std::max(maxStep, std::hypot(p.x - prev.x, p.y - prev.y));
            maxTurn = std::max(maxTurn, std::fabs(shortestTurn(prev.facing, p.facing)));
            maxBankStep = std::max(maxBankStep, std::fabs(p.bank - prev.bank));
            prev = p;
        }
        EXPECT(maxStep < 0.02 && maxTurn < 0.5 && maxBankStep < 0.5);
    }
    const auto* t2 = tl.track(2);
    EXPECT(t2->poseAt(4 * tk).facing == 135);
    EXPECT(t2->poseAt(2 * tk).bank > 0.0); // 180 -> 135 is a clockwise (right) turn: roll right

    // Beams: two emitters from tick 5, one tick long, faction colour, fire sound faction variant first.
    std::vector<const VisualJob*> beams;
    for (const auto& j : tl.jobs) if (j.kind == JobKind::Beam && j.eventIndex == 8) beams.push_back(&j);
    EXPECT(beams.size() == 2 && beams[0]->start == 5 * tk && beams[1]->start > beams[0]->start);
    EXPECT(std::fabs(beams[0]->end() - 6 * tk) < 1e-9 && beams[0]->color == weaponColor(static_cast<int>(W::PhaserArray), 1));
    EXPECT(beams[0]->vfxKey == "vfx.PhaserArray" && vfxKeysFor(*beams[0]).back() == "vfx.generic.beam");
    bool beamSound = false;
    for (const auto& s : tl.sounds)
        if (s.eventIndex == 8 && s.cue == audio::GameCue::BeamFire)
            beamSound = s.time == 5 * tk && s.keys.front() == "weapon.PhaserArray.fed" && s.keys.back() == "weapon.generic.beam";
    EXPECT(beamSound);

    // Torpedoes: two, arcing opposite ways, arriving at the impact tick; shield and hull impacts there; explosion.
    std::vector<const VisualJob*> torps;
    for (const auto& j : tl.jobs) if (j.kind == JobKind::Torpedo && j.eventIndex == 11) torps.push_back(&j);
    EXPECT(torps.size() == 2 && torps[0]->start == 6 * tk && torps[0]->arcCells * torps[1]->arcCells < 0);
    EXPECT(std::fabs(torps[0]->end() - 8 * tk) < 1e-9 && torps[1]->end() > torps[0]->end());
    int shieldHits = 0, hullHits = 0;
    double impactAt = -1;
    for (const auto& j : tl.jobs) if (j.eventIndex == 11) {
        if (j.kind == JobKind::ShieldImpact) { ++shieldHits; impactAt = j.start; EXPECT(j.sourceFleetId == 1 && j.targetFleetId == 2); }
        if (j.kind == JobKind::HullImpact) { ++hullHits; EXPECT(j.destroysTarget && j.hullDamage == 40); }
    }
    EXPECT(shieldHits == 1 && hullHits == 1 && std::fabs(impactAt - torps[1]->end()) < 1e-9);
    const VisualJob* boom = nullptr;
    for (const auto& j : tl.jobs) if (j.kind == JobKind::Explosion) boom = &j;
    EXPECT(boom && boom->fleetId == 2 && std::fabs(boom->start - impactAt) < 1e-9);
    EXPECT(t2->hideAt > boom->start && t2->visibilityAt(boom->start) == 1.0 && t2->visibilityAt(boom->end()) == 0.0);
    // Torpedo path: starts at the shooter, arcs, ends at the target; a miss overshoots.
    const auto p0 = torpedoPoint(0, 0, 4, 0, 0.35, 0, 0.0), p5 = torpedoPoint(0, 0, 4, 0, 0.35, 0, 0.5), p1 = torpedoPoint(0, 0, 4, 0, 0.35, 0, 1.0);
    EXPECT(p0.x == 0 && std::fabs(p1.x - 4) < 1e-12 && std::fabs(p1.y) < 1e-12 && std::fabs(p5.y - 0.35) < 1e-12 && p5.lift > 0);
    EXPECT(std::fabs(torpedoPoint(0, 0, 4, 0, 0, 2, 1.0).x - 6) < 1e-12);

    // Intercepted volley: stops short, point-defence burst, no impact on the target.
    int intercepts = 0, interceptedImpacts = 0;
    for (const auto& j : tl.jobs) if (j.eventIndex == 14) {
        if (j.kind == JobKind::Torpedo) EXPECT(j.outcome == ShotOutcome::Intercepted && j.endFraction < 1.0);
        if (j.kind == JobKind::PointDefence) ++intercepts;
        if (j.kind == JobKind::ShieldImpact || j.kind == JobKind::HullImpact) ++interceptedImpacts;
    }
    EXPECT(intercepts == 1 && interceptedImpacts == 0);
    // Missed pulse flies past; no impact.
    for (const auto& j : tl.jobs) if (j.eventIndex == 13) EXPECT(j.kind == JobKind::PulseBolt && j.outcome == ShotOutcome::Miss && j.overshootCells > 0);

    // No overlap: one mount's beams/bolts never overlap each other across events.
    for (std::size_t a = 0; a < tl.jobs.size(); ++a)
        for (std::size_t b = a + 1; b < tl.jobs.size(); ++b) {
            const auto& x = tl.jobs[a];
            const auto& y = tl.jobs[b];
            if (x.eventIndex == y.eventIndex || x.fleetId != y.fleetId || x.mountIndex != y.mountIndex) continue;
            if ((x.kind != JobKind::Beam && x.kind != JobKind::PulseBolt) || x.kind != y.kind) continue;
            EXPECT(x.end() <= y.start + 1e-9);
        }

    // Ram: impact on fleet 1 at tick 5.
    bool ramHull = false;
    for (const auto& j : tl.jobs) ramHull = ramHull || (j.eventIndex == 10 && j.kind == JobKind::HullImpact && j.targetFleetId == 1 && j.start == 5 * tk);
    EXPECT(ramHull);

    // Cloak: fleet 3 invisible until it decloaks at tick 4, fully visible a fade later.
    const auto* t3 = tl.track(3);
    EXPECT(t3->visibilityAt(0.0) == 0.0 && t3->visibilityAt(4 * tk - 0.01) == 0.0);
    EXPECT(t3->visibilityAt(4 * tk + 0.5 * cfg.cloakTicks * tk) > 0.2 && t3->visibilityAt(6.0) == 1.0);
    // Retreat: warps out at tick 0 and is gone after the warp.
    const auto* t4 = tl.track(4);
    EXPECT(t4->visibilityAt(0.0) == 1.0 && t4->visibilityAt(cfg.warpTicks * tk + 0.01) == 0.0);
    bool warpSound = false, cloakSound = false, boomSound = false;
    for (const auto& s : tl.sounds) {
        warpSound = warpSound || (s.cue == audio::GameCue::Retreat && s.keys.front() == "sfx.warp.out");
        cloakSound = cloakSound || (s.cue == audio::GameCue::Decloak && s.keys.front() == "sfx.decloak.rom");
        boomSound = boomSound || (s.cue == audio::GameCue::ShipDestroyed && s.keys.front() == "sfx.explosion.large");
    }
    EXPECT(warpSound && cloakSound && boomSound);

    // Barks: torpedo away at launch, target destroyed after the kill, shields crossing 75 then 50 -> one "shields.50".
    std::vector<std::string> barkDetails;
    for (const auto& b : tl.barks) barkDetails.push_back(b.detail);
    EXPECT(barkDetails.size() == 3);
    EXPECT(std::count(barkDetails.begin(), barkDetails.end(), "torpedo") == 1);
    EXPECT(std::count(barkDetails.begin(), barkDetails.end(), "kill") == 1);
    EXPECT(std::count(barkDetails.begin(), barkDetails.end(), "shields.50") == 1);
    const auto bark = audio::cueFor(audio::GameCue::CombatBark, "fed", {}, "shields.50");
    EXPECT(bark.voiceKeys.size() == 2 && bark.voiceKeys[0] == "vo.combat.shields.50.fed" && bark.voiceKeys[1] == "vo.combat.shields.fed");
    EXPECT(bark.sfxKeys.empty() && !bark.beepIfNoVoice && bark.priority == 6);
    EXPECT(audio::cueFor(audio::GameCue::CombatBark, "kli", {}, "kill").voiceKeys.size() == 1);
    // Without a player empire nobody barks.
    TimelineConfig quiet = cfg;
    quiet.playerEmpire = 0;
    EXPECT(buildRoundTimeline(events, ships, quiet).barks.empty());

    // Round duration is configurable: everything scales.
    TimelineConfig slow = cfg;
    slow.roundSeconds = 12.0;
    const auto tl2 = buildRoundTimeline(events, ships, slow);
    EXPECT(tl2.jobs.size() == tl.jobs.size() && std::fabs(tl2.jobs.back().start - 2 * tl.jobs.back().start) < 1e-9);

    // Empty round (rules 1/2 or nothing happened): static tracks, no jobs.
    const auto none = buildRoundTimeline({}, ships, cfg);
    EXPECT(none.jobs.empty() && none.sounds.empty() && none.allConsumed() && plane(none.track(2)->poseAt(3.0)) == (PlanePose{6, 0, 180, 0}));

    // 3D flight: heights are deterministic, sides sit in layers, small craft spread and climb more than capital
    // ships, the nose pitches with a climb and levels at the end, and heights change smoothly.
    {
        EXPECT(homeHeight(7, 110, true) == homeHeight(7, 110, true));
        double capMax = 0.0, smallMax = 0.0, firstSum = 0.0, otherSum = 0.0;
        for (int id = 1; id <= 400; ++id) {
            capMax = std::max(capMax, std::fabs(homeHeight(id, 650, true) - TimelineConfig{}.sideHeightOffset));
            smallMax = std::max(smallMax, std::fabs(homeHeight(id, 110, true) - TimelineConfig{}.sideHeightOffset));
            firstSum += homeHeight(id, 650, true);
            otherSum += homeHeight(id, 650, false);
        }
        EXPECT(capMax <= TimelineConfig{}.capitalHeightSpread + 1e-9 && smallMax > 1.0 && firstSum > otherSum);
        auto sized = ships;
        for (auto& s : sized) s.lengthMeters = 110.0; // both escorts
        const auto t3 = buildRoundTimeline(events, sized, cfg);
        const auto* s1 = t3.track(1);
        EXPECT(s1 && s1->small && s1->segments.size() == 1);
        const auto& seg = s1->segments[0];
        EXPECT(s1->poseAt(0.0).altitude == seg.from.altitude && s1->poseAt(6.0).altitude == seg.to.altitude);
        EXPECT(s1->poseAt(6.0).pitch == 0.0 && std::fabs(seg.pitchPeak) <= cfg.maxPitchDegrees);
        if (std::fabs(seg.to.altitude - seg.from.altitude) > 0.05)
            EXPECT((seg.pitchPeak > 0.0) == (seg.to.altitude > seg.from.altitude)); // climbing noses up
        double maxClimbStep = 0.0;
        for (double t = 0.001; t <= 6.0; t += 0.001) maxClimbStep = std::max(maxClimbStep, std::fabs(s1->poseAt(t).altitude - s1->poseAt(t - 0.001).altitude));
        EXPECT(maxClimbStep < 0.02);
        // Escorts roll harder than the same maneuver flown by a capital ship.
        auto big = ships;
        for (auto& s : big) s.lengthMeters = 650.0;
        const auto t4 = buildRoundTimeline(events, big, cfg);
        EXPECT(std::fabs(t3.track(1)->segments[0].bankPeak) > std::fabs(t4.track(1)->segments[0].bankPeak));
        EXPECT(!t4.track(1)->small && t4.track(1)->wingLeader == 0);
    }

    // Wings: escorts of one side starting close together fly finger-four on the lowest id; a far-off escort, another
    // side's escort and capital ships fly alone. Wingmen hold station in the leader's frame and peel off when it dies.
    {
        std::vector<ShipInfo> fleet;
        const auto add = [&](int id, int owner, double x, double y, double len) {
            ShipInfo s; s.fleetId = id; s.ownerId = owner; s.x = x; s.y = y; s.facing = 0; s.lengthMeters = len; s.maxShields = 10;
            fleet.push_back(s);
        };
        add(10, 1, 2, 5, 110); add(11, 1, 2, 6, 120); add(12, 1, 3, 4, 90); add(13, 1, 1, 5, 150); add(14, 1, 2, 7, 100);
        add(15, 1, 12, 2, 110); add(16, 2, 3, 5, 110); add(17, 1, 2, 4, 650);
        std::vector<CombatEvent> wingEvents;
        { auto d = ev(6, K::Destroyed, 10); d.x = 2; d.y = 5; wingEvents.push_back(d); }
        const auto w = buildRoundTimeline(wingEvents, fleet, cfg);
        EXPECT(w.track(10)->wingLeader == 0 && w.track(11)->wingLeader == 10 && w.track(12)->wingLeader == 10 && w.track(13)->wingLeader == 10);
        EXPECT(w.track(14)->wingLeader == 0);  // the wing is full (4)
        EXPECT(w.track(15)->wingLeader == 0 && w.track(16)->wingLeader == 0 && w.track(17)->wingLeader == 0);
        std::size_t i11 = 0;
        for (std::size_t i = 0; i < w.ships.size(); ++i) if (w.ships[i].fleetId == 11) i11 = i;
        const PlanePose lead = w.track(10)->poseAt(0.5), mate = w.poseOf(i11, 0.5);
        // Facing 0 (+x): station is aft (-x) and to port (+y) of the leader, and above it.
        EXPECT(std::fabs(mate.x - (lead.x - cfg.wingSpacingCells)) < 1e-9 && std::fabs(mate.y - (lead.y + cfg.wingSpacingCells)) < 1e-9);
        EXPECT(mate.altitude > lead.altitude && mate.facing == lead.facing);
        // Leader destroyed at tick 6 (plus its explosion): after it is gone the wingman is back on its own track.
        EXPECT(w.track(10)->hideAt > 0.0);
        EXPECT(plane(w.poseOf(i11, w.track(10)->hideAt + 3.0 * w.tickSeconds)) == plane(w.track(11)->poseAt(w.track(10)->hideAt + 3.0 * w.tickSeconds)));
    }

    // Spacing: true-scale hulls that overlap are pushed apart (mostly in height), clear hulls and inactive ones stay
    // put, coincident hulls split deterministically, and the result is stable across calls.
    {
        const auto gapOf = [](const std::vector<SpacingBody>& b, const std::vector<SpacingOffset>& o, std::size_t i, std::size_t j) {
            const double dx = (b[j].x + o[j].x) - (b[i].x + o[i].x), dy = (b[j].y + o[j].y) - (b[i].y + o[i].y);
            const double da = (b[j].alt + o[j].alt) - (b[i].alt + o[i].alt);
            return std::sqrt(dx * dx + dy * dy + da * da) - (b[i].radius + b[j].radius);
        };
        std::vector<SpacingBody> clear = {{0, 0, 0, 0.5}, {3, 0, 0, 0.5}};
        const auto none = spacingOffsets(clear);
        EXPECT(none[0].x == 0.0 && none[0].alt == 0.0 && none[1].x == 0.0 && none[1].alt == 0.0);
        std::vector<SpacingBody> level = {{0, 0, 0, 1.2}, {1, 0, 0, 1.2}};
        const auto lv = spacingOffsets(level);
        EXPECT(gapOf(level, lv, 0, 1) > -1e-6);
        EXPECT(std::fabs(lv[0].alt) > 0.2 && std::fabs(lv[1].alt) > 0.2 && lv[0].alt * lv[1].alt < 0.0); // they stack
        std::vector<SpacingBody> same = {{2, 2, 0, 0.8}, {2, 2, 0, 0.8}};
        const auto sm = spacingOffsets(same);
        EXPECT(gapOf(same, sm, 0, 1) > -1e-6 && sm[0].x == 0.0 && sm[0].y == 0.0);
        const auto again = spacingOffsets(same);
        EXPECT(again[0].alt == sm[0].alt && again[1].alt == sm[1].alt);
        // A cruiser beside an escort: the escort moves further.
        std::vector<SpacingBody> mixed = {{0, 0, 0, 1.4}, {0.5, 0.3, 0.1, 0.3}};
        const auto mx = spacingOffsets(mixed);
        const auto mag = [](const SpacingOffset& o) { return std::sqrt(o.x * o.x + o.y * o.y + o.alt * o.alt); };
        EXPECT(gapOf(mixed, mx, 0, 1) > -1e-6 && mag(mx[1]) > 2.0 * mag(mx[0]));
        // Eight capitals stacked in two cells settle apart; an inactive hull is ignored.
        std::vector<SpacingBody> crowd;
        for (int i = 0; i < 8; ++i) crowd.push_back({static_cast<double>(i % 2), 0.2 * (i / 2), 0.1 * i, 1.0});
        crowd.push_back({0, 0, 0, 1.0, false});
        const auto cr = spacingOffsets(crowd, 40);
        double worst = 0.0;
        for (std::size_t i = 0; i < 8; ++i) for (std::size_t j = i + 1; j < 8; ++j) worst = std::min(worst, gapOf(crowd, cr, i, j));
        EXPECT(worst > -0.15);
        EXPECT(cr[8].x == 0.0 && cr[8].alt == 0.0);
    }

    // Sockets: WPN_Beam_NN for beams, WPN_Torp / *_Torp_* for torpedoes, deterministic by name; none -> -1.
    const std::vector<std::string> sockets = {"FX_Engine_01", "WPN_Torp_F_02", "WPN_Beam_D_02", "WPN_Beam_D_01", "WPN_Plasma_Torp_01", "WPN_Pulse_01"};
    EXPECT(pickWeaponSocket(sockets, 0, 0, 0) == 3 && pickWeaponSocket(sockets, 0, 1, 0) == 2 && pickWeaponSocket(sockets, 0, 2, 0) == 3);
    EXPECT(pickWeaponSocket(sockets, 1, 0, 0) == 5);
    EXPECT(pickWeaponSocket(sockets, 2, 0, 0) == 4 && pickWeaponSocket(sockets, 2, 1, 0) == 1);
    EXPECT(pickWeaponSocket({"FX_Engine_01"}, 0, 0, 0) == -1 && pickWeaponSocket({"WPN_Torp_01"}, 0, 0, 0) == -1);
    EXPECT(pickWeaponSocket({"WPN_Disruptor_01"}, 1, 0, 0) == 0); // pulse falls back to any beam-type socket
    EXPECT(fallbackHardpoint(2, 0, 0).x > 0.8 && fallbackHardpoint(0, 0, 0).y == -fallbackHardpoint(0, 1, 0).y);

    // Cinematic director: follows the torpedo, cuts to the explosion, and holds shots for the minimum length.
    CinematicDirector director;
    EXPECT(director.update(tl, 6 * tk + 0.05));
    EXPECT(director.shot.subjectFleetId != 0);
    const auto held = director.shot;
    director.update(tl, 6 * tk + 0.1);
    EXPECT(director.shot == held);
    director.update(tl, boom->start + 0.01);
    EXPECT(director.shot.subjectFleetId == 2);
    std::vector<std::string> keys;
    for (int w = 1; w <= static_cast<int>(W::Torpedo); ++w) keys.push_back(vfxWeaponKey(w, 0));
    EXPECT(keys.front() == "vfx.PhaseCannon" && keys.back() == "vfx.Torpedo" && vfxWeaponKey(0, 2) == "vfx.generic.torpedo");
    EXPECT(weaponSoundKeys(0, 2, 1).size() == 1 && weaponSoundKeys(0, 2, 1)[0] == "weapon.generic.torpedo");
}

int main() {
    testJson();
    testKeys();
    testCatalog();
    testManifestCoverage();
    testCues();
    testTurnCues();
    testShipDatabase();
    testCombatTimeline();
    std::printf("client logic: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
