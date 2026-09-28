#pragma once
// Engine-independent mapping from core game events to audio cues (docs/AUDIO-PLAN.md section 3).
// UBotfAudioSubsystem plays what these return; the functions only read core state, so replays and reloaded turns
// produce the same cues. Tested in Tools/client-logic-tests.cpp.

#include "BotfAssetCatalog.h"
#include "BotfAudioCues.h"
#include "Simulation.h"
#include "TacticalCombat.h"

#include <algorithm>
#include <string>
#include <vector>

namespace botf::audio {

struct CueEvent {
    GameCue cue = GameCue::TurnStart;
    std::string other;  // Faction prefix of the other party, if any.
    std::string detail; // Event/treaty/victory detail for expansion voice keys.
};

inline int cuePriority(const CueEvent& e) { return cueFor(e.cue, "fed", e.other, e.detail).priority; }

inline bool containsWord(const std::string& text, const char* word) {
    std::string lower = text;
    for (auto& c : lower) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return lower.find(word) != std::string::npos;
}

// Cues for a finished turn, highest priority first, at most one per cue kind (the briefing can hold dozens of lines).
inline std::vector<CueEvent> turnCues(const GameState& state, const TurnReport& report) {
    std::vector<CueEvent> out;
    const auto add = [&out](GameCue cue, std::string other = {}, std::string detail = {}) {
        for (const auto& e : out) if (e.cue == cue) return;
        out.push_back({cue, std::move(other), std::move(detail)});
    };
    add(GameCue::TurnStart);
    const int player = state.playerEmpire;
    for (const auto& entry : report.chronicle) {
        const bool mine = entry.empireId == 0 || entry.empireId == player || entry.otherEmpireId == player;
        const int otherId = entry.empireId == player ? entry.otherEmpireId : entry.empireId;
        const std::string other = assets::ownerFactionPrefix(otherId);
        switch (entry.category) {
        case ChronicleCategory::Research: if (mine) add(GameCue::ResearchComplete); break;
        case ChronicleCategory::Construction: if (mine) add(GameCue::ConstructionComplete); break;
        case ChronicleCategory::Colonization: if (mine) add(GameCue::ColonyFounded); break;
        case ChronicleCategory::Stations: if (mine) add(GameCue::OutpostBuilt); break;
        case ChronicleCategory::Diplomacy:
            if (!mine) break;
            if (containsWord(entry.text, "war")) add(GameCue::WarDeclared, other);
            else if (containsWord(entry.text, "treaty") || containsWord(entry.text, "alliance") || containsWord(entry.text, "pact"))
                add(GameCue::TreatySigned, other);
            else if (containsWord(entry.text, "first contact") || containsWord(entry.text, "met "))
                add(GameCue::FirstContact, other);
            else add(GameCue::DiplomaticProposal, other);
            break;
        case ChronicleCategory::Events:
            add(GameCue::GalacticEvent, {}, entry.eventId > 0 ? std::to_string(entry.eventId) : std::string());
            break;
        case ChronicleCategory::Combat:
        case ChronicleCategory::Exploration:
        case ChronicleCategory::Intelligence:
        case ChronicleCategory::Victory:
            break;
        }
    }
    for (const auto& battle : report.battles)
        if (battle.firstOwner == player || battle.secondOwner == player) {
            add(GameCue::SystemUnderAttack, assets::ownerFactionPrefix(battle.firstOwner == player ? battle.secondOwner : battle.firstOwner));
            break;
        }
    if (state.outcome == CampaignOutcome::Victory) add(GameCue::Victory);
    else if (state.outcome != CampaignOutcome::Ongoing) add(GameCue::Defeat);
    std::stable_sort(out.begin(), out.end(), [](const CueEvent& a, const CueEvent& b) { return cuePriority(a) > cuePriority(b); });
    return out;
}

// Cues for one resolved tactical round: weapon fire by the attacker's dominant weapon, hits, losses, cloaks, retreats.
struct RoundSound {
    GameCue cue = GameCue::BeamFire;
    std::vector<std::string> keys; // Keys to try in order (weapon variants first).
};
inline std::vector<RoundSound> roundCues(const tactical::Encounter& encounter, const tactical::RoundResult& round) {
    std::vector<RoundSound> out;
    for (const auto& salvo : round.salvos) {
        const auto* attacker = encounter.getShip(salvo.attackerFleetId);
        const int owner = attacker ? attacker->ownerId : 0;
        const auto weapons = assets::shipWeapons(attacker ? attacker->referenceShipId : -1, owner);
        const std::string fac = assets::ownerFactionPrefix(owner);
        if (salvo.beamDamage > 0) out.push_back({GameCue::BeamFire, assets::weaponAssetKeys(weapons.beam, fac)});
        if (salvo.torpedoDamage > 0) out.push_back({GameCue::TorpedoLaunch, assets::weaponAssetKeys(weapons.torpedo, fac)});
    }
    bool hull = false;
    for (const auto& salvo : round.salvos) hull = hull || salvo.beamDamage + salvo.torpedoDamage > 0;
    if (hull) out.push_back({GameCue::ShieldHit, {"sfx.shield.hit"}});
    for (const auto& ship : encounter.ships) {
        const auto* status = round.getStatus(ship.fleetId);
        if (!status) continue;
        const std::string fac = assets::ownerFactionPrefix(ship.ownerId);
        if (status->destroyedThisRound) out.push_back({GameCue::ShipDestroyed, {"sfx.explosion.large"}});
        if (status->decloakedThisRound) out.push_back({GameCue::Decloak, {"sfx.decloak." + fac, "sfx.decloak.rom"}});
    }
    return out;
}

} // namespace botf::audio
