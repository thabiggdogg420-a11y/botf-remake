#pragma once
// Plays sfx, voice and music by cue key (docs/AUDIO-PLAN.md). The rules (which keys a game event asks for,
// priority, cooldowns, the one-line voice queue, the sfx voice limit and voice ducking) are the engine-independent
// botf::audio::CueScheduler / cueFor() in BotfAudioCues.h; this class resolves keys through UBotfAssetRegistry
// and drives UAudioComponents. Nothing installed locally means silence, never an error. The simulation never
// depends on audio.

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "BotfAudioCues.h"
#include "BotfAudioSubsystem.generated.h"

class UAudioComponent;
namespace botf { struct GameState; struct TurnReport; }
namespace botf::tactical { struct Encounter; struct RoundResult; }

UCLASS()
class UBotfAudioSubsystem final : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // FTickableGameObject
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override { return !IsTemplate(); }
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
    virtual bool IsTickableWhenPaused() const override { return true; }

    static UBotfAudioSubsystem* Get();

    // Listening player's empire (1..5); voice lines use its faction.
    void SetPlayerEmpire(int32 EmpireId);

    // A game event. Other = the other party's empire id (0 = none); Detail = event/treaty/order detail key part.
    void PlayCue(botf::audio::GameCue Cue, int32 OtherEmpire = 0, const FString& Detail = FString());
    // Plays the first resolvable key directly (used for per-weapon keys from the combat scene).
    void PlaySfxKeys(const std::vector<std::string>& Keys, botf::audio::Category Category, int32 Priority);

    // Hooks for existing client events.
    void OnTurnFinished(const botf::GameState& State, const botf::TurnReport& Report);
    void OnEncounterOpened();
    void OnRoundResolved(const botf::tactical::Encounter& Encounter, const botf::tactical::RoundResult& Round);
    void OnOrderAccepted(const FString& ManeuverDetail = FString());
    void OnDiplomacySent(int32 OtherEmpire, bool bWar);

    // Music by context key part ("galaxy", "combat", "diplomacy"): mus.botf.<context> then mus.botf.all.
    void PlayMusic(const FString& Context);
    void StopMusic();

    // Settings (0..1), stored in GameUserSettings.ini [BotfAudio].
    float GetMasterVolume() const { return Scheduler.settings.master; }
    float GetCategoryVolume(botf::audio::Category Category) const;
    void SetMasterVolume(float Volume);
    void SetCategoryVolume(botf::audio::Category Category, float Volume);
    void SaveSettings() const;

private:
    struct FActiveSfx
    {
        TWeakObjectPtr<UAudioComponent> Component;
        long long Handle = 0;
        botf::audio::Category Category = botf::audio::Category::Sfx;
    };

    void LoadSettings();
    void ApplyGains();
    bool PlayFirstVoice(const std::vector<std::string>& Keys, int32 Priority);
    UAudioComponent* Spawn(class USoundBase* Sound, float Volume);
    std::string Faction(int32 EmpireId) const;
    double Now() const;

    botf::audio::CueScheduler Scheduler;
    TArray<FActiveSfx> ActiveSfx;
    UPROPERTY() TObjectPtr<UAudioComponent> VoiceComponent;
    UPROPERTY() TObjectPtr<UAudioComponent> MusicComponent;
    FString MusicContext;
    int32 PlayerEmpire = 1;
};
