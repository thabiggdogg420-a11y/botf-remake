#include "BotfAudioSubsystem.h"
#include "BotfAssetRegistry.h"
#include "BotfAudioEvents.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformTime.h"
#include "Sound/SoundBase.h"

namespace
{
const TCHAR* AudioSection = TEXT("BotfAudio");
const TCHAR* CategoryConfigName(botf::audio::Category Category)
{
    switch (Category)
    {
    case botf::audio::Category::Sfx: return TEXT("SfxVolume");
    case botf::audio::Category::Voice: return TEXT("VoiceVolume");
    case botf::audio::Category::Ui: return TEXT("UiVolume");
    case botf::audio::Category::Music: return TEXT("MusicVolume");
    default: return TEXT("UnknownVolume");
    }
}
}

void UBotfAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency(UBotfAssetRegistry::StaticClass());
    Super::Initialize(Collection);
    LoadSettings();
}

void UBotfAudioSubsystem::Deinitialize()
{
    StopMusic();
    if (VoiceComponent) VoiceComponent->Stop();
    for (const auto& Active : ActiveSfx) if (Active.Component.IsValid()) Active.Component->Stop();
    ActiveSfx.Reset();
    Super::Deinitialize();
}

TStatId UBotfAudioSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UBotfAudioSubsystem, STATGROUP_Tickables);
}

UBotfAudioSubsystem* UBotfAudioSubsystem::Get()
{
    UGameInstance* Instance = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetGameInstance() : nullptr;
    return Instance ? Instance->GetSubsystem<UBotfAudioSubsystem>() : nullptr;
}

double UBotfAudioSubsystem::Now() const { return FPlatformTime::Seconds(); }

std::string UBotfAudioSubsystem::Faction(int32 EmpireId) const { return botf::assets::ownerFactionPrefix(EmpireId); }

void UBotfAudioSubsystem::SetPlayerEmpire(int32 EmpireId) { PlayerEmpire = EmpireId; }

UAudioComponent* UBotfAudioSubsystem::Spawn(USoundBase* Sound, float Volume)
{
    UGameInstance* Instance = GetGameInstance();
    UWorld* World = Instance ? Instance->GetWorld() : nullptr;
    if (!Sound || !World) return nullptr;
    UAudioComponent* Component = UGameplayStatics::CreateSound2D(World, Sound, Volume, 1.f, 0.f, nullptr, true, true);
    if (Component) Component->Play();
    return Component;
}

void UBotfAudioSubsystem::PlaySfxKeys(const std::vector<std::string>& Keys, botf::audio::Category Category, int32 Priority)
{
    const UBotfAssetRegistry* Registry = UBotfAssetRegistry::Get();
    if (!Registry || Keys.empty()) return;
    // Cooldowns are per logical key (the first one), so a variant falling back to a generic sound is still limited.
    const auto Decision = Scheduler.requestSfx(Keys.front(), Category, Priority, Now());
    if (!Decision.play) return;
    if (Decision.cullHandle)
    {
        for (int32 Index = ActiveSfx.Num() - 1; Index >= 0; --Index)
            if (ActiveSfx[Index].Handle == Decision.cullHandle)
            {
                if (ActiveSfx[Index].Component.IsValid()) ActiveSfx[Index].Component->Stop();
                ActiveSfx.RemoveAt(Index);
            }
    }
    USoundBase* Sound = Registry->LoadSound(Keys);
    UAudioComponent* Component = Spawn(Sound, botf::audio::categoryGain(Scheduler.settings, Category, Scheduler.voiceActive()));
    if (!Component) { Scheduler.sfxFinished(Decision.handle); return; }
    ActiveSfx.Add({Component, Decision.handle, Category});
}

bool UBotfAudioSubsystem::PlayFirstVoice(const std::vector<std::string>& Keys, int32 Priority)
{
    const UBotfAssetRegistry* Registry = UBotfAssetRegistry::Get();
    if (!Registry) return false;
    for (const auto& Key : Keys)
        if (!Registry->GetCatalog().resolve(Key).placeholder)
        {
            Scheduler.requestVoice(Key, Priority, Now()); // May be dropped as a repeat; it still counts as voiced.
            return true;
        }
    return false;
}

void UBotfAudioSubsystem::PlayCue(botf::audio::GameCue Cue, int32 OtherEmpire, const FString& Detail)
{
    const auto Request = botf::audio::cueFor(Cue, Faction(PlayerEmpire), Faction(OtherEmpire), std::string(TCHAR_TO_UTF8(*Detail)));
    if (!Request.sfxKeys.empty()) PlaySfxKeys(Request.sfxKeys, Request.sfxCategory, Request.priority);
    const bool bVoice = !Request.voiceKeys.empty() && PlayFirstVoice(Request.voiceKeys, Request.priority);
    if (!bVoice && Request.beepIfNoVoice && Request.sfxKeys.empty())
        PlaySfxKeys({"sfx.ui.beep"}, botf::audio::Category::Ui, Request.priority);
}

void UBotfAudioSubsystem::OnTurnFinished(const botf::GameState& State, const botf::TurnReport& Report)
{
    SetPlayerEmpire(State.playerEmpire);
    for (const auto& Event : botf::audio::turnCues(State, Report))
    {
        int32 Other = 0;
        for (int32 Empire = 1; Empire <= 5; ++Empire)
            if (Faction(Empire) == Event.other) Other = Empire;
        PlayCue(Event.cue, Other, FString(UTF8_TO_TCHAR(Event.detail.c_str())));
    }
    PlayMusic(TEXT("galaxy"));
}

void UBotfAudioSubsystem::OnEncounterOpened()
{
    PlayCue(botf::audio::GameCue::EncounterOpens);
    PlayMusic(TEXT("combat"));
}

void UBotfAudioSubsystem::OnRoundResolved(const botf::tactical::Encounter& Encounter, const botf::tactical::RoundResult& Round)
{
    // Tactical rules 3: ABotfCombatScene plays each shot, impact and bark at its moment in the round
    // (BotfCombatTimeline.h), so the one-shot summary below would double every sound.
    if (!Round.events.empty()) return;
    for (const auto& Sound : botf::audio::roundCues(Encounter, Round))
    {
        const auto Request = botf::audio::cueFor(Sound.cue, Faction(PlayerEmpire));
        PlaySfxKeys(Sound.keys, botf::audio::Category::Sfx, Request.priority);
    }
}

void UBotfAudioSubsystem::OnOrderAccepted(const FString& ManeuverDetail)
{
    PlayCue(botf::audio::GameCue::OrderAccepted, 0, ManeuverDetail);
}

void UBotfAudioSubsystem::OnDiplomacySent(int32 OtherEmpire, bool bWar)
{
    PlayCue(bWar ? botf::audio::GameCue::WarDeclared : botf::audio::GameCue::DiplomaticProposal, OtherEmpire);
}

void UBotfAudioSubsystem::PlayMusic(const FString& Context)
{
    if (Context == MusicContext && MusicComponent && MusicComponent->IsPlaying()) return;
    const UBotfAssetRegistry* Registry = UBotfAssetRegistry::Get();
    if (!Registry) return;
    USoundBase* Sound = Registry->LoadSound({std::string("mus.botf.") + TCHAR_TO_UTF8(*Context), "mus.botf.all"});
    if (!Sound) return; // Silent when no music is installed.
    StopMusic();
    MusicContext = Context;
    MusicComponent = Spawn(Sound, botf::audio::categoryGain(Scheduler.settings, botf::audio::Category::Music, Scheduler.voiceActive()));
}

void UBotfAudioSubsystem::StopMusic()
{
    if (MusicComponent) MusicComponent->FadeOut(1.0f, 0.f);
    MusicComponent = nullptr;
    MusicContext.Reset();
}

void UBotfAudioSubsystem::Tick(float /*DeltaTime*/)
{
    const double Time = Now();
    for (int32 Index = ActiveSfx.Num() - 1; Index >= 0; --Index)
        if (!ActiveSfx[Index].Component.IsValid() || !ActiveSfx[Index].Component->IsPlaying())
        {
            Scheduler.sfxFinished(ActiveSfx[Index].Handle);
            ActiveSfx.RemoveAt(Index);
        }
    const bool bWasActive = Scheduler.voiceActive();
    botf::audio::VoiceLine Line;
    if (Scheduler.nextVoice(Time, VoiceComponent && VoiceComponent->IsPlaying(), Line))
    {
        const UBotfAssetRegistry* Registry = UBotfAssetRegistry::Get();
        const int32 Variants = Registry ? Registry->CountVariants(Line.key) : 0;
        const int32 Variant = static_cast<int32>(Scheduler.pickVariant(Line.key, static_cast<std::size_t>(FMath::Max(1, Variants))));
        USoundBase* Sound = Registry ? Registry->LoadSound({Line.key}, Variant) : nullptr;
        if (VoiceComponent) VoiceComponent->Stop();
        VoiceComponent = Spawn(Sound, botf::audio::categoryGain(Scheduler.settings, botf::audio::Category::Voice, true));
    }
    if (bWasActive != Scheduler.voiceActive()) ApplyGains();
}

void UBotfAudioSubsystem::ApplyGains()
{
    const bool bVoice = Scheduler.voiceActive();
    for (const auto& Active : ActiveSfx)
        if (Active.Component.IsValid())
            Active.Component->AdjustVolume(0.25f, botf::audio::categoryGain(Scheduler.settings, Active.Category, bVoice));
    if (MusicComponent)
        MusicComponent->AdjustVolume(0.4f, botf::audio::categoryGain(Scheduler.settings, botf::audio::Category::Music, bVoice));
    if (VoiceComponent)
        VoiceComponent->AdjustVolume(0.1f, botf::audio::categoryGain(Scheduler.settings, botf::audio::Category::Voice, bVoice));
}

float UBotfAudioSubsystem::GetCategoryVolume(botf::audio::Category Category) const
{
    const int32 Index = static_cast<int32>(Category);
    return Index >= 0 && Index < static_cast<int32>(botf::audio::Category::Count) ? Scheduler.settings.category[Index] : 0.f;
}

void UBotfAudioSubsystem::SetMasterVolume(float Volume)
{
    Scheduler.settings.master = botf::audio::clampVolume(Volume);
    ApplyGains();
}

void UBotfAudioSubsystem::SetCategoryVolume(botf::audio::Category Category, float Volume)
{
    const int32 Index = static_cast<int32>(Category);
    if (Index < 0 || Index >= static_cast<int32>(botf::audio::Category::Count)) return;
    Scheduler.settings.category[Index] = botf::audio::clampVolume(Volume);
    ApplyGains();
}

void UBotfAudioSubsystem::LoadSettings()
{
    if (!GConfig) return;
    float Value = 0.f;
    if (GConfig->GetFloat(AudioSection, TEXT("MasterVolume"), Value, GGameUserSettingsIni)) Scheduler.settings.master = botf::audio::clampVolume(Value);
    for (int32 Index = 0; Index < static_cast<int32>(botf::audio::Category::Count); ++Index)
        if (GConfig->GetFloat(AudioSection, CategoryConfigName(static_cast<botf::audio::Category>(Index)), Value, GGameUserSettingsIni))
            Scheduler.settings.category[Index] = botf::audio::clampVolume(Value);
}

void UBotfAudioSubsystem::SaveSettings() const
{
    if (!GConfig) return;
    GConfig->SetFloat(AudioSection, TEXT("MasterVolume"), Scheduler.settings.master, GGameUserSettingsIni);
    for (int32 Index = 0; Index < static_cast<int32>(botf::audio::Category::Count); ++Index)
        GConfig->SetFloat(AudioSection, CategoryConfigName(static_cast<botf::audio::Category>(Index)), Scheduler.settings.category[Index], GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}
