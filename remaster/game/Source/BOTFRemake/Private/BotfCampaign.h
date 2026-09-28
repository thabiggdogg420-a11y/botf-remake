#pragma once
// Campaign ownership for the Slate client (first step of splitting SBotfInterface; see remaster/docs/UNREAL-CLIENT.md).
//
// FBotfCampaign owns everything that is the campaign rather than its presentation:
//   - the committed GameState and the open strategic turn transaction (botf::session::TurnSession),
//   - the draft preview cache (never throws; a failed preview falls back to the committed state),
//   - order submission, replacement and dry runs against core validation,
//   - save / load / snapshot listing with verification,
//   - the single exception boundary (Run) that turns core exceptions into a message,
//   - change notifications for views.
// It is plain C++ (not a UObject) so it can be owned by the widget today and moved into a subsystem later.

#include "CoreMinimal.h"
#include "Simulation.h"
#include "CampaignSession.h"
#include "TacticalCombat.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

enum class EBotfCampaignChange : uint8
{
    Draft,          // orders drafted, cancelled or replaced
    Replaced,       // a different campaign: new game or load (ids from the old one are meaningless)
    TurnCommitted,  // a strategic turn was committed
    Session,        // the turn transaction opened, paused on a battle or changed
    Saved           // a verified snapshot was published
};

// One listed snapshot. A snapshot is an ordinary campaign or a BOTF_TURN_SESSION envelope saved mid-turn.
struct FBotfSnapshotInfo
{
    FString Path;
    FDateTime Modified;
    int Turn = 0, Empire = 1, Pending = 0, Colonies = 0;
    bool bBattle = false;              // saved mid-turn with a tactical transaction open
    int BattleRound = 0, BattlesDone = 0;
    std::uint64_t Seed = 0;
    botf::Ruleset Ruleset = botf::Ruleset::Prototype;
};

struct FBotfSnapshot
{
    botf::GameState State;                                   // for an envelope: the committed baseline
    std::optional<botf::session::TurnSession> Session;
};

class FBotfCampaign
{
public:
    using FListener = std::function<void(EBotfCampaignChange)>;

    // Largest snapshot file read: a turn-session envelope (botf::session::deserialize accepts up to 32 MiB).
    // Plain campaigns are bounded by botf::deserialize itself, so the client does not repeat that limit.
    static constexpr int64 MaxSnapshotBytes = 32 * 1024 * 1024;

    // ---- Owned campaign (public while SBotfInterface is being split; views should prefer the methods) ----
    botf::GameState State;
    botf::session::TurnSession Session;          // while active, State is the committed pre-turn campaign
    std::uint64_t StateVersion = 1;              // bumped on every change to State or the draft
    std::uint64_t CampaignVersion = 1;           // bumped only when State is replaced wholesale or a turn commits
    bool bHasActiveCampaign = false;
    bool bHasSavedBaseline = false;
    std::uint64_t SavedFingerprint = 0;
    FString SavedSessionText;                    // envelope at the last verified save/load ("" when no session)

    // Snapshot listing
    FString FolderOverride;                      // automation tests write to an isolated folder
    TArray<FString> Saves;
    TArray<FBotfSnapshotInfo> SaveInfos;
    FString SavePath;

    // ---- Change notifications ----
    int Subscribe(FListener Listener);
    void Unsubscribe(int Handle);
    void Notify(EBotfCampaignChange Change);

    // ---- The single exception boundary: runs Action; a thrown exception becomes OutError and false ----
    static bool Run(const std::function<void()>& Action, FString& OutError);

    // ---- Draft ----
    // Committed campaign plus this turn's orders. Cached by version and campaign identity. Never throws: if core
    // rejects the draft the committed state is returned and PreviewError() says why.
    const botf::GameState& Preview() const;
    // Changes whenever the cached preview is rebuilt; views key their own caches on it.
    std::uint64_t PreviewGeneration() const;
    const FString& PreviewError() const { return PreviewErrorText; }

    std::uint64_t NextId() const { return State.lastAcceptedCommand + 1; }
    void Submit(const botf::Command& Command);
    void CancelLast();
    // Atomically cancels matching drafted orders and optionally submits a replacement, through the real
    // cancel/submit contract on a copy; the live draft changes only if every step is accepted.
    void ReplaceOrders(const std::function<bool(const botf::Command&)>& Match, std::optional<botf::Command> Replacement);
    // Core's exact reason an order would be refused ("" when it would be accepted). No mutation.
    FString DryRun(const botf::Command& Command) const;
    void ReplaceState(botf::GameState&& Next);

    // ---- Turn transaction ----
    FString SessionText() const;                 // exact envelope of the open turn; "" when none
    void BeginTurn();
    // Resumes the open turn. Returns the report when the turn committed; nothing while a battle awaits the player.
    std::optional<botf::TurnReport> ResumeTurn();
    // Encounters fought in the last committed turn of this session (their per-round orders feed the replay).
    const std::vector<botf::tactical::Encounter>& LastTurnEncounters() const { return TurnEncounters; }

    // ---- Dirty tracking ----
    bool HasUnsavedChanges() const;

    // ---- Snapshots ----
    FString SnapshotFolder() const;
    FBotfSnapshot ReadSnapshot(const FString& Path) const;
    void RefreshSaves();
    // Writes, verifies and publishes a snapshot. FinalOrder (the unapplied intelligence allocation) is added to a
    // copy of the draft and committed to State only once the file is published. Returns the published path.
    FString SaveSnapshot(const std::optional<botf::Command>& FinalOrder);
    void LoadSnapshot(const FString& Path);

private:
    struct FPreviewKey
    {
        std::uint64_t Version = 0, Seed = 0, Rng = 0, LastCommand = 0;
        int Turn = -1, Player = 0, Pending = -1, Colonies = -1, Fleets = -1;
        std::int64_t Credits = 0;
        bool operator==(const FPreviewKey& O) const
        {
            return Version == O.Version && Seed == O.Seed && Rng == O.Rng && LastCommand == O.LastCommand && Turn == O.Turn
                && Player == O.Player && Pending == O.Pending && Colonies == O.Colonies && Fleets == O.Fleets && Credits == O.Credits;
        }
    };
    FPreviewKey MakePreviewKey() const;

    mutable FPreviewKey CachedKey;
    mutable botf::GameState CachedPreview;
    mutable std::uint64_t Generation = 0;
    mutable FString PreviewErrorText;

    std::vector<botf::tactical::Encounter> TurnEncounters;
    std::vector<std::pair<int, FListener>> Listeners;
    int NextListener = 1;
};
