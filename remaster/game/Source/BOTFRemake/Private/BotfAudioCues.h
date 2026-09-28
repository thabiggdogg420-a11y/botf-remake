#pragma once
// Engine-independent audio cue selection and mixing rules (docs/AUDIO-PLAN.md section 3 and 4).
//
// UBotfAudioSubsystem turns game events into GameCue values, asks cueFor() which sfx/voice keys to try, and lets a
// CueScheduler decide what actually plays: priority, per-key cooldowns, one voice line at a time with a 0.6 s gap
// and a priority queue, at most 24 sfx with the lowest priority culled, and voice ducking of sfx and music.
// Time is passed in explicitly (seconds), so everything here is deterministic and unit-tested outside Unreal
// (Tools/client-logic-tests.cpp). Audio never feeds back into the simulation.

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace botf::audio {

enum class Category : int { Sfx = 0, Voice = 1, Ui = 2, Music = 3, Count = 4 };

inline const char* categoryName(Category c) {
    switch (c) {
    case Category::Sfx: return "SFX";
    case Category::Voice: return "Voice";
    case Category::Ui: return "UI";
    case Category::Music: return "Music";
    case Category::Count: break;
    }
    return "Unknown";
}

enum class GameCue : int {
    TurnStart, ResearchComplete, ConstructionComplete, ColonyFounded, OutpostBuilt, SystemUnderAttack, FirstContact,
    DiplomaticProposal, WarDeclared, TreatySigned, GalacticEvent, Victory, Defeat, EncounterOpens, OrderAccepted,
    BeamFire, TorpedoLaunch, ShieldHit, HullHit, ShipDestroyed, Cloak, Decloak, Retreat, FleetArrives, UiClick,
    CombatBark // Tactical voice bark; detail = "shields.<N>", "torpedo" or "kill" (BotfCombatTimeline.h BarkJob)
};

// One logical request. Keys are tried in order; the first that resolves plays (see Catalog::resolveFirst).
struct CueRequest {
    std::vector<std::string> sfxKeys;
    Category sfxCategory = Category::Sfx;
    std::vector<std::string> voiceKeys; // Never crosses factions (AUDIO-PLAN: missing voice -> beep, not another race).
    int priority = 1;
    bool beepIfNoVoice = false;         // Play sfx.ui.beep when no voice key resolves and no sfx was requested.
};

// fac: the listening player's faction prefix (fed, kli, rom, car, fer). other: the other party (diplomacy speaker,
// cloaking ship's faction). detail: event/card id, station event, weapon key or victory type depending on the cue.
inline CueRequest cueFor(GameCue cue, const std::string& fac, const std::string& other = {}, const std::string& detail = {}) {
    CueRequest r;
    auto advisor = [&](std::vector<std::string> specific) {
        if (!fac.empty()) specific.push_back("vo.advisor." + fac);
        return specific;
    };
    const std::string speaker = other.empty() ? fac : other;
    switch (cue) {
    case GameCue::TurnStart: r.sfxKeys = {"sfx.ui.beep"}; r.sfxCategory = Category::Ui; r.priority = 1; break;
    case GameCue::UiClick: r.sfxKeys = {"sfx.ui.beep"}; r.sfxCategory = Category::Ui; r.priority = 1; break;
    case GameCue::ResearchComplete: r.sfxKeys = {"sfx.ui.beep"}; r.sfxCategory = Category::Ui; r.voiceKeys = advisor({}); r.priority = 4; break;
    case GameCue::ConstructionComplete: r.sfxKeys = {"sfx.ui.beep"}; r.sfxCategory = Category::Ui; r.voiceKeys = advisor({}); r.priority = 3; break;
    case GameCue::ColonyFounded: r.sfxKeys = {"sfx.ui.beep"}; r.sfxCategory = Category::Ui; r.voiceKeys = advisor({}); r.priority = 4; break;
    case GameCue::OutpostBuilt:
        r.sfxKeys = {"sfx.ui.beep"}; r.sfxCategory = Category::Ui;
        r.voiceKeys = advisor(fac.empty() ? std::vector<std::string>{} : std::vector<std::string>{"vo.station." + fac + "." + (detail.empty() ? "built" : detail)});
        r.priority = 4; break;
    case GameCue::SystemUnderAttack: r.sfxKeys = {"sfx.alert.red"}; r.voiceKeys = advisor({}); r.priority = 8; break;
    case GameCue::FirstContact: if (!speaker.empty()) r.voiceKeys = {"vo.diplomacy." + speaker}; r.priority = 7; r.beepIfNoVoice = true; break;
    case GameCue::DiplomaticProposal: if (!speaker.empty()) r.voiceKeys = {"vo.diplomacy." + speaker}; r.priority = 6; r.beepIfNoVoice = true; break;
    case GameCue::WarDeclared: r.sfxKeys = {"sfx.alert.red"}; if (!speaker.empty()) r.voiceKeys = {"vo.diplomacy." + speaker}; r.priority = 9; break;
    case GameCue::TreatySigned:
        r.sfxKeys = {"sfx.ui.beep"}; r.sfxCategory = Category::Ui;
        if (!fac.empty() && !detail.empty()) r.voiceKeys = {"vo.treaty." + detail + "." + fac};
        if (!speaker.empty()) r.voiceKeys.push_back("vo.diplomacy." + speaker);
        r.priority = 5; break;
    case GameCue::GalacticEvent:
        r.sfxKeys = {"sfx.ui.beep"}; r.sfxCategory = Category::Ui;
        r.voiceKeys = advisor(detail.empty() ? std::vector<std::string>{} : std::vector<std::string>{"vo.event." + detail});
        r.priority = 5; break;
    case GameCue::Victory:
    case GameCue::Defeat:
        if (!fac.empty()) r.voiceKeys = {"vo.victory." + (detail.empty() ? std::string(cue == GameCue::Victory ? "win" : "defeat") : detail) + "." + fac};
        r.voiceKeys = advisor(r.voiceKeys);
        r.priority = 10; break;
    case GameCue::EncounterOpens: r.sfxKeys = {"sfx.alert.red"}; r.priority = 8; break;
    case GameCue::OrderAccepted:
        r.sfxKeys = {"sfx.ui.beep"}; r.sfxCategory = Category::Ui;
        if (!fac.empty()) {
            if (!detail.empty()) r.voiceKeys.push_back("vo.combat." + detail + "." + fac);
            r.voiceKeys.push_back("vo.combat.ack." + fac);
        }
        r.priority = 6; break;
    case GameCue::BeamFire: r.sfxKeys = {detail.empty() ? std::string("weapon.generic.beam") : detail, "weapon.generic.beam"}; r.priority = 3; break;
    case GameCue::TorpedoLaunch: r.sfxKeys = {detail.empty() ? std::string("weapon.generic.torpedo") : detail, "weapon.generic.torpedo"}; r.priority = 3; break;
    case GameCue::ShieldHit: r.sfxKeys = {"sfx.shield.hit"}; r.priority = 2; break;
    case GameCue::HullHit: r.sfxKeys = {"sfx.explosion.small"}; r.priority = 2; break;
    case GameCue::ShipDestroyed: r.sfxKeys = {"sfx.explosion.large"}; r.priority = 5; break;
    case GameCue::Cloak: r.sfxKeys = {"sfx.cloak." + (speaker.empty() ? std::string("rom") : speaker), "sfx.cloak.rom"}; r.priority = 4; break;
    case GameCue::Decloak: r.sfxKeys = {"sfx.decloak." + (speaker.empty() ? std::string("rom") : speaker), "sfx.decloak.rom"}; r.priority = 4; break;
    case GameCue::Retreat: r.sfxKeys = {"sfx.warp.out"}; r.priority = 3; break;
    case GameCue::FleetArrives: r.sfxKeys = {"sfx.warp.in"}; r.priority = 2; break;
    case GameCue::CombatBark:
        // vo.combat.shields.50.fed, then vo.combat.shields.fed; never another faction's voice, never a beep.
        if (!fac.empty() && !detail.empty()) {
            r.voiceKeys.push_back("vo.combat." + detail + "." + fac);
            const auto dot = detail.find('.');
            if (dot != std::string::npos) r.voiceKeys.push_back("vo.combat." + detail.substr(0, dot) + "." + fac);
        }
        r.priority = detail == "shields.25" ? 7 : detail.rfind("shields", 0) == 0 ? 6 : detail == "kill" ? 5 : 3;
        break;
    }
    // A duplicate key in the list (e.g. detail == generic) is harmless but wasteful.
    r.sfxKeys.erase(std::unique(r.sfxKeys.begin(), r.sfxKeys.end()), r.sfxKeys.end());
    return r;
}

// ---------------------------------------------------------------------------------------------------------------
// Settings and scheduling
// ---------------------------------------------------------------------------------------------------------------

struct MixSettings {
    float master = 1.0f;
    float category[static_cast<int>(Category::Count)] = {1.0f, 1.0f, 0.8f, 0.6f};
    float duckFactor = 0.45f;     // Sfx and music gain while a voice line plays.
    float voiceGapSeconds = 0.6f; // Silence between two voice lines.
    int maxSfxVoices = 24;
    int maxVoiceQueue = 4;
    float voiceQueueTimeout = 6.0f; // A queued line older than this is stale and dropped.
};

inline float clampVolume(float v) { return std::clamp(v, 0.0f, 1.0f); }

// Gain for a category, including master volume and voice ducking.
inline float categoryGain(const MixSettings& s, Category c, bool voiceActive) {
    const int i = static_cast<int>(c);
    if (i < 0 || i >= static_cast<int>(Category::Count)) return 0.0f;
    float gain = clampVolume(s.master) * clampVolume(s.category[i]);
    if (voiceActive && (c == Category::Sfx || c == Category::Music)) gain *= s.duckFactor;
    return gain;
}

// Minimum seconds between two plays of the same key. Weapon fire is rapid; UI blips and alerts are not.
inline float cooldownFor(const std::string& key, Category category) {
    if (category == Category::Voice) return 4.0f;
    if (key.rfind("weapon.", 0) == 0) return 0.08f;
    if (key.rfind("sfx.alert", 0) == 0) return 3.0f;
    if (category == Category::Ui) return 0.12f;
    return 0.15f;
}

struct VoiceLine {
    std::string key;
    int priority = 0;
    double requestedAt = 0.0;
    long long sequence = 0;
};

struct SfxInstance {
    long long handle = 0;
    int priority = 0;
    double startedAt = 0.0;
};

struct SfxDecision {
    bool play = false;
    long long handle = 0;      // New instance handle when play is true.
    long long cullHandle = 0;  // An instance the caller must stop first (0 = none).
};

class CueScheduler {
public:
    MixSettings settings;

    // Sfx: returns whether to play, honouring the per-key cooldown and the voice limit (lowest priority, then oldest,
    // is culled; a new sound never culls one of higher priority).
    SfxDecision requestSfx(const std::string& key, Category category, int priority, double now) {
        SfxDecision d;
        if (!offCooldown(key, category, now)) return d;
        pruneFinished(now);
        if (static_cast<int>(sfx.size()) >= std::max(1, settings.maxSfxVoices)) {
            auto victim = std::min_element(sfx.begin(), sfx.end(), [](const SfxInstance& a, const SfxInstance& b) {
                return a.priority != b.priority ? a.priority < b.priority : a.startedAt < b.startedAt;
            });
            if (victim == sfx.end() || victim->priority > priority) return d;
            d.cullHandle = victim->handle;
            sfx.erase(victim);
        }
        lastPlayed.push_back({key, now});
        d.play = true;
        d.handle = ++nextHandle;
        sfx.push_back({d.handle, priority, now});
        return d;
    }
    // The caller reports when an sfx instance ends (or is stopped); unknown handles are ignored.
    void sfxFinished(long long handle) {
        sfx.erase(std::remove_if(sfx.begin(), sfx.end(), [handle](const SfxInstance& s) { return s.handle == handle; }), sfx.end());
    }
    // Instances are also dropped after a generous maximum length, in case the engine never reports the end.
    void pruneFinished(double now, double maxSeconds = 12.0) {
        sfx.erase(std::remove_if(sfx.begin(), sfx.end(), [&](const SfxInstance& s) { return now - s.startedAt > maxSeconds; }), sfx.end());
    }
    int activeSfx() const { return static_cast<int>(sfx.size()); }

    // Voice: queued by priority (higher first, then request order). Repeats of the same key inside its cooldown, or
    // of a key already queued, are dropped. When the queue is full the lowest-priority entry gives way.
    bool requestVoice(const std::string& key, int priority, double now) {
        if (key.empty()) return false;
        if (!offCooldown(key, Category::Voice, now, false)) return false;
        for (const auto& line : queue) if (line.key == key) return false;
        if (static_cast<int>(queue.size()) >= std::max(1, settings.maxVoiceQueue)) {
            auto lowest = std::min_element(queue.begin(), queue.end(), [](const VoiceLine& a, const VoiceLine& b) {
                return a.priority != b.priority ? a.priority < b.priority : a.sequence > b.sequence;
            });
            if (lowest->priority >= priority) return false;
            queue.erase(lowest);
        }
        queue.push_back({key, priority, now, ++nextSequence});
        return true;
    }

    // Call every frame with whether the voice channel is still playing. Returns the line to start now, if any.
    bool nextVoice(double now, bool channelBusy, VoiceLine& out) {
        if (channelBusy) { voiceEndedAt = -1.0; voicePlaying = true; return false; }
        if (voicePlaying) { voicePlaying = false; voiceEndedAt = now; }
        queue.erase(std::remove_if(queue.begin(), queue.end(), [&](const VoiceLine& l) {
            return now - l.requestedAt > settings.voiceQueueTimeout; }), queue.end());
        if (queue.empty()) return false;
        if (voiceEndedAt >= 0.0 && now - voiceEndedAt < settings.voiceGapSeconds) return false;
        auto best = std::max_element(queue.begin(), queue.end(), [](const VoiceLine& a, const VoiceLine& b) {
            return a.priority != b.priority ? a.priority < b.priority : a.sequence > b.sequence;
        });
        out = *best;
        queue.erase(best);
        lastPlayed.push_back({out.key, now});
        voicePlaying = true;
        return true;
    }
    bool voiceActive() const { return voicePlaying; }
    std::size_t queuedVoices() const { return queue.size(); }

    // Round-robin variant choice for keys with several files (voice lines), stable per key.
    std::size_t pickVariant(const std::string& key, std::size_t variants) {
        if (variants <= 1) return 0;
        for (auto& v : variantCursor) if (v.first == key) return (v.second++) % variants;
        variantCursor.push_back({key, 1});
        return 0;
    }

private:
    struct Played { std::string key; double at; };
    std::vector<Played> lastPlayed;
    std::vector<SfxInstance> sfx;
    std::vector<VoiceLine> queue;
    std::vector<std::pair<std::string, std::size_t>> variantCursor;
    long long nextHandle = 0;
    long long nextSequence = 0;
    double voiceEndedAt = -1.0;
    bool voicePlaying = false;

    bool offCooldown(const std::string& key, Category category, double now, bool record = false) {
        const float cooldown = cooldownFor(key, category);
        lastPlayed.erase(std::remove_if(lastPlayed.begin(), lastPlayed.end(), [&](const Played& p) { return now - p.at > 10.0; }), lastPlayed.end());
        for (const auto& p : lastPlayed)
            if (p.key == key && now - p.at < cooldown) return false;
        if (record) lastPlayed.push_back({key, now});
        return true;
    }
};

} // namespace botf::audio
