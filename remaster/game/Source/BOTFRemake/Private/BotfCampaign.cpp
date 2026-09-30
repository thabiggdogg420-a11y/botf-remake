#include "BotfCampaign.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "CoreGlobals.h"

#include <algorithm>
#include <stdexcept>

namespace
{
FString CampaignText(const std::string& Value) { return UTF8_TO_TCHAR(Value.c_str()); }
}

int FBotfCampaign::Subscribe(FListener Listener)
{
    const int Handle = NextListener++;
    Listeners.emplace_back(Handle, std::move(Listener));
    return Handle;
}

void FBotfCampaign::Unsubscribe(int Handle)
{
    Listeners.erase(std::remove_if(Listeners.begin(), Listeners.end(),
        [Handle](const std::pair<int, FListener>& Entry) { return Entry.first == Handle; }), Listeners.end());
}

void FBotfCampaign::Notify(EBotfCampaignChange Change)
{
    // A copy: a listener may subscribe or unsubscribe while being notified.
    const auto Current = Listeners;
    for (const auto& Entry : Current)
        if (Entry.second) Entry.second(Change);
}

bool FBotfCampaign::Run(const std::function<void()>& Action, FString& OutError)
{
    try
    {
        Action();
        return true;
    }
    catch (const std::exception& Error)
    {
        OutError = CampaignText(Error.what());
    }
    catch (...)
    {
        OutError = TEXT("Unexpected error");
    }
    return false;
}

FBotfCampaign::FPreviewKey FBotfCampaign::MakePreviewKey() const
{
    // The key pairs the explicit version with the campaign's identity, so a replaced campaign can never be served
    // the previous campaign's draft, even on a path that forgets to bump StateVersion.
    FPreviewKey Key;
    Key.Version = StateVersion; Key.Seed = State.seed; Key.Rng = State.rngState; Key.LastCommand = State.lastAcceptedCommand;
    Key.Turn = State.turn; Key.Player = State.playerEmpire; Key.Pending = static_cast<int>(State.pending.size());
    Key.Colonies = static_cast<int>(State.colonies.size()); Key.Fleets = static_cast<int>(State.fleets.size());
    Key.Credits = State.credits;
    return Key;
}

const botf::GameState& FBotfCampaign::Preview() const
{
    const FPreviewKey Key = MakePreviewKey();
    if (!(CachedKey == Key))
    {
        try
        {
            CachedPreview = botf::preview(State);
            PreviewErrorText.Empty();
        }
        catch (const std::exception& Error)
        {
            CachedPreview = State;
            PreviewErrorText = CampaignText(Error.what());
        }
        catch (...)
        {
            CachedPreview = State;
            PreviewErrorText = TEXT("Draft preview failed");
        }
        CachedKey = Key;
        ++Generation;
    }
    return CachedPreview;
}

std::uint64_t FBotfCampaign::PreviewGeneration() const
{
    Preview();
    return Generation;
}

void FBotfCampaign::Submit(const botf::Command& Command)
{
    if (Session.active) throw std::runtime_error("Finish the battle in progress before giving new orders");
    botf::submit(State, Command);
    ++StateVersion;
    Notify(EBotfCampaignChange::Draft);
}

void FBotfCampaign::CancelLast()
{
    if (Session.active) throw std::runtime_error("Finish the battle in progress before changing orders");
    if (State.pending.empty()) throw std::runtime_error("No order to cancel");
    botf::cancel(State, State.pending.back().id);
    ++StateVersion;
    Notify(EBotfCampaignChange::Draft);
}

void FBotfCampaign::ReplaceOrders(const std::function<bool(const botf::Command&)>& Match, std::optional<botf::Command> Replacement)
{
    if (Session.active) throw std::runtime_error("Finish the battle in progress before changing orders");
    botf::GameState Copy = State;
    std::vector<std::uint64_t> Ids;
    for (const auto& Order : Copy.pending) if (Match(Order)) Ids.push_back(Order.id);
    for (auto It = Ids.rbegin(); It != Ids.rend(); ++It) botf::cancel(Copy, *It);
    if (Replacement)
    {
        Replacement->id = Copy.lastAcceptedCommand + 1;
        botf::submit(Copy, *Replacement);
    }
    State = std::move(Copy);
    ++StateVersion;
    Notify(EBotfCampaignChange::Draft);
}

FString FBotfCampaign::DryRun(const botf::Command& Command) const
{
    if (Session.active) return TEXT("Finish the battle in progress first");
    try
    {
        botf::GameState Copy = State;
        botf::submit(Copy, Command);
        return FString();
    }
    catch (const std::exception& Error)
    {
        return CampaignText(Error.what());
    }
}

void FBotfCampaign::ReplaceState(botf::GameState&& Next)
{
    State = std::move(Next);
    ++StateVersion;
    ++CampaignVersion;
    TurnEncounters.clear();
    Notify(EBotfCampaignChange::Replaced);
}

FString FBotfCampaign::SessionText() const
{
    return Session.active ? CampaignText(botf::session::serialize(Session)) : FString();
}

void FBotfCampaign::BeginTurn()
{
    botf::session::begin(Session, State);
    Notify(EBotfCampaignChange::Session);
}

std::optional<botf::TurnReport> FBotfCampaign::ResumeTurn()
{
    std::optional<botf::TurnReport> Result = botf::session::resume(Session, State);
    ++StateVersion;   // resume may have committed a new campaign state
    if (Result)
    {
        TurnEncounters = std::move(Session.completed);
        Session = botf::session::TurnSession();
        ++CampaignVersion;   // a committed turn replaces the battle records
        Notify(EBotfCampaignChange::TurnCommitted);
    }
    else
    {
        Notify(EBotfCampaignChange::Session);
    }
    return Result;
}

bool FBotfCampaign::HasUnsavedChanges() const
{
    // Includes an open turn transaction: tactical orders, rounds and completed encounters.
    return bHasActiveCampaign && (!bHasSavedBaseline || botf::fingerprint(State) != SavedFingerprint || SessionText() != SavedSessionText);
}

FString FBotfCampaign::SnapshotFolder() const
{
    if (!FolderOverride.IsEmpty()) return FolderOverride;
    return FPaths::ProjectSavedDir() / TEXT("BotfSnapshots");
}

FBotfSnapshot FBotfCampaign::ReadSnapshot(const FString& Path) const
{
    const int64 Size = IFileManager::Get().FileSize(*Path);
    if (Size < 0 || Size > MaxSnapshotBytes) throw std::runtime_error("Save missing or exceeds size limit");
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Path)) throw std::runtime_error("Could not read saved campaign");
    const std::string Utf8 = TCHAR_TO_UTF8(*Text);
    FBotfSnapshot Out;
    if (Utf8.rfind("BOTF_TURN_SESSION", 0) == 0)
    {
        botf::session::TurnSession Restored = botf::session::deserialize(Utf8);
        if (botf::session::serialize(Restored) != Utf8) throw std::runtime_error("Mid-battle save does not round-trip exactly");
        Out.State = Restored.baseline;
        Out.Session = std::move(Restored);
    }
    else
    {
        Out.State = botf::deserialize(Utf8);   // core enforces its own campaign size limit
    }
    return Out;
}

void FBotfCampaign::RefreshSaves()
{
    const FString Folder = SnapshotFolder();
    TArray<FString> Names;
    IFileManager::Get().FindFiles(Names, *(Folder / TEXT("*.botfsave")), true, false);
    Names.Sort([&Folder](const FString& A, const FString& B)
    {
        const FDateTime AT = IFileManager::Get().GetTimeStamp(*(Folder / A));
        const FDateTime BT = IFileManager::Get().GetTimeStamp(*(Folder / B));
        return AT == BT ? A < B : AT > BT;
    });
    Saves.Empty();
    SaveInfos.Empty();
    for (const FString& Name : Names)
    {
        const FString Path = Folder / Name;
        try
        {
            const FBotfSnapshot Snapshot = ReadSnapshot(Path);
            const botf::GameState& Saved = Snapshot.State;
            FBotfSnapshotInfo Info;
            if (Snapshot.Session)
            {
                Info.bBattle = true;
                Info.BattlesDone = static_cast<int>(Snapshot.Session->completed.size());
                Info.BattleRound = Snapshot.Session->pending ? Snapshot.Session->pending->currentRound : 0;
            }
            Info.Path = Path;
            Info.Modified = IFileManager::Get().GetTimeStamp(*Path);
            Info.Turn = Saved.turn;
            Info.Empire = Saved.playerEmpire;
            Info.Pending = static_cast<int>(Saved.pending.size());
            Info.Colonies = static_cast<int>(std::count_if(Saved.colonies.begin(), Saved.colonies.end(),
                [&Saved](const botf::Colony& C) { return C.ownerId == Saved.playerEmpire; }));
            Info.Seed = Saved.seed;
            Info.Ruleset = Saved.ruleset;
            Saves.Add(Path);
            SaveInfos.Add(MoveTemp(Info));
        }
        catch (const std::exception& Error)
        {
            UE_LOG(LogTemp, Display, TEXT("BOTF_SAVE_SKIPPED %s: %s"), *Name, *CampaignText(Error.what()));
        }
        if (Saves.Num() >= 256) break;
    }
    if (!Saves.Contains(SavePath)) SavePath = Saves.IsEmpty() ? FString() : Saves[0];
}

FString FBotfCampaign::SaveSnapshot(const std::optional<botf::Command>& FinalOrder)
{
    if (!bHasActiveCampaign) throw std::runtime_error("Start or load a campaign before saving");
    // The draft is only changed once the snapshot is published: a failed save leaves every order as it was.
    botf::GameState Next = State;
    if (FinalOrder)
    {
        if (Session.active) throw std::runtime_error("Finish the battle in progress before giving new orders");
        botf::Command Order = *FinalOrder;
        Order.id = Next.lastAcceptedCommand + 1;
        botf::submit(Next, Order);
    }
    const FString Folder = SnapshotFolder();
    if (!IFileManager::Get().MakeDirectory(*Folder, true)) throw std::runtime_error("Cannot create snapshot folder");
    const FString Candidate = Folder / (FString::Printf(TEXT("Turn-%04d-"), Next.turn) + FGuid::NewGuid().ToString() + TEXT(".botfsave"));
    const FString Temporary = Candidate + TEXT(".pending-write");
    // Mid-turn saves persist the whole transaction envelope in the same file.
    const std::string Text = Session.active ? botf::session::serialize(Session) : botf::serialize(Next);
    bool bPublished = false;
    try
    {
        if (!FFileHelper::SaveStringToFile(CampaignText(Text), *Temporary, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
            throw std::runtime_error("Snapshot write failed");
        const FBotfSnapshot Checked = ReadSnapshot(Temporary);
        const bool bMatches = Session.active
            ? (Checked.Session.has_value() && botf::session::serialize(*Checked.Session) == Text)
            : (!Checked.Session.has_value() && botf::fingerprint(Checked.State) == botf::fingerprint(Next));
        if (!bMatches) throw std::runtime_error("Snapshot verification failed");
        if (!IFileManager::Get().Move(*Candidate, *Temporary, false, false, false, true))
            throw std::runtime_error("Could not publish verified save");
        bPublished = true;
    }
    catch (...)
    {
        // Never leave a half-written or unverified file behind.
        if (!bPublished) IFileManager::Get().Delete(*Temporary, false, true, true);
        throw;
    }
    if (FinalOrder)
    {
        State = std::move(Next);
        ++StateVersion;
    }
    SavePath = Candidate;
    RefreshSaves();
    SavedFingerprint = botf::fingerprint(State);
    bHasSavedBaseline = true;
    SavedSessionText = SessionText();
    Notify(EBotfCampaignChange::Saved);
    return Candidate;
}

void FBotfCampaign::LoadSnapshot(const FString& Path)
{
    FBotfSnapshot Snapshot = ReadSnapshot(Path);
    // Campaigns from before the star-name table get it now (a mid-battle save keeps its names until the battle is
    // done: its baseline must stay byte-identical). The next save writes the new names; this is not unsaved progress.
    if (!Snapshot.Session) botf::adoptStarNames(Snapshot.State);
    SavePath = Path;
    Session = Snapshot.Session ? std::move(*Snapshot.Session) : botf::session::TurnSession();
    ReplaceState(std::move(Snapshot.State));
    bHasActiveCampaign = true;
    SavedFingerprint = botf::fingerprint(State);
    bHasSavedBaseline = true;
    SavedSessionText = SessionText();
    ++StateVersion;
}
