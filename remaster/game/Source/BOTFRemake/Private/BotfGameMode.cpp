#include "BotfGameMode.h"
#include "BotfCombatScene.h"
#include "BotfSettingsPanel.h"
#include "BotfUiTheme.h"
#include "BotfGalaxyMap.h"
#include "BotfUiControls.h"
#include "BotfTacticalRadar.h"
#include "BotfCombatUi.h"
#include "BotfBattleReplay.h"
#include "BotfCampaign.h"
#include "BotfViewModel.h"
#include "BotfGameplayViewModel.h"
#include "BotfShipDatabase.h"
#include "BotfAudioSubsystem.h"
#include "Simulation.h"
#include "CampaignSession.h"
#include "ReferenceCatalogue.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <stdexcept>
#include <set>
#include <tuple>
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace {
using BotfUi::FPalette;
using BotfUi::EShape;
using BotfUi::ERole;
FText Txt(const FString& Value) { return FText::FromString(Value); }
FString Str(const std::string& Value) { return UTF8_TO_TCHAR(Value.c_str()); }
FSlateFontInfo Font(int Size, bool Bold = false) { return BotfUi::Font(Size, Bold); }
const TCHAR* StanceName(botf::TacticalStance Stance) {
    switch (Stance) {
    case botf::TacticalStance::Aggressive: return TEXT("AGGRESSIVE");
    case botf::TacticalStance::Defensive: return TEXT("DEFENSIVE");
    case botf::TacticalStance::Withdraw: return TEXT("WITHDRAW");
    default: return TEXT("BALANCED");
    }
}

// Command deck: the star chart is always present and fills the screen. The original's separate screens
// (Solar System, Technology, Diplomacy, Intelligence, Empire, Combat; Tutorial Guide p3-32) open as sheets
// over the chart. PanelGalaxy means no sheet is open.
enum : int { PanelGalaxy = 0, PanelSystem = 1, PanelResearch = 2, PanelDiplomacy = 3, PanelIntel = 4, PanelEmpire = 5, PanelCombat = 6 };

#if WITH_DEV_AUTOMATION_TESTS
class FBotfUiShellAutomationTest;
#endif
class SBotfInterface final : public SCompoundWidget {
public:
    virtual bool SupportsKeyboardFocus() const override { return true; }
    // Keyboard model.
    //   Command deck (Shell == Game): global shortcuts run on the tunnelling pass, before the focused control sees the
    //   key, so a button that kept focus after a mouse click cannot swallow Enter, Space or Ctrl+Enter. Keys with no
    //   global meaning still reach the focused control, so Enter/Space press a focused button.
    //   Menus, dialogs and settings: ordinary Slate focus navigation; only the bubbling pass (OnKeyDown) is used.
    //   Text entry: while a text field has focus no hotkey fires; Escape still unwinds once the field ignores it.
    virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override {
        if (Shell != EUiView::Game || TextInputHasFocus()) return FReply::Unhandled();
        return HandleShortcut(Geometry, Event, true);
    }
    virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override {
        if (Event.GetKey() != EKeys::Escape && TextInputHasFocus()) return SCompoundWidget::OnKeyDown(Geometry, Event);
        return HandleShortcut(Geometry, Event, false);
    }
    // True while one of the interface's text fields has keyboard focus (typing must not trigger hotkeys).
    bool TextInputHasFocus() const {
        if (!FSlateApplication::IsInitialized()) return false;
        if (ReferenceSearchBox.IsValid() && (ReferenceSearchBox->HasKeyboardFocus() || ReferenceSearchBox->HasFocusedDescendants())) return true;
        if (!HasFocusedDescendants()) return false;   // focus is outside this interface
        const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
        if (!Focused.IsValid()) return false;
        const FName Type = Focused->GetType();
        return Type == FName(TEXT("SEditableText")) || Type == FName(TEXT("SMultiLineEditableText"));
    }
    // bPreview: called on the tunnelling pass, where an unrecognised key must stay unhandled (no default navigation)
    // so the focused control still receives it.
    FReply HandleShortcut(const FGeometry& Geometry, const FKeyEvent& Event, bool bPreview) {
        const FKey Key = Event.GetKey();
        if (Key == EKeys::Escape) {
            // Escape unwinds one layer at a time: the end-of-game overlay, transient mode, popups, the open sheet, the unit
            // selection, then the menu.
            if (Shell == EUiView::Game && OutcomeOverlayVisible()) bOutcomeDismissed = true;
            else if (Shell == EUiView::Game && (MapView.bPlotting || bMenuOpen || bSummaryOpen || bEndTurnCheck || bEventsOpen)) {
                MapView.bPlotting = false; bMenuOpen = false; bSummaryOpen = false; bEndTurnCheck = false; bEventsOpen = false;
            }
            else if (Shell == EUiView::Game && ActivePanel != PanelGalaxy && !(ActivePanel == PanelCombat && Session.pending)) GoPanel(PanelGalaxy);
            else if (Shell == EUiView::Game && (SelectedFleet || SelectedTaskForce)) { SelectedFleet = 0; SelectedTaskForce = 0; RefreshAll(); }
            else if (Shell == EUiView::Game) ShowView(EUiView::Menu);
            else if (Shell == EUiView::Menu) ShowView(EUiView::Game);
            else if (Shell == EUiView::Confirm) CancelConfirmation();
            else if (Shell == EUiView::Setup || Shell == EUiView::Load) ShowView(SubPageReturn);
            else if (Shell == EUiView::Settings) ShowView(SubPageReturn);
            return FReply::Handled();
        }
        if (Shell != EUiView::Game) return SCompoundWidget::OnKeyDown(Geometry, Event);
        // The end-of-game overlay is modal: shortcuts are held back, a focused overlay button still gets its key.
        if (OutcomeOverlayVisible()) return bPreview ? FReply::Unhandled() : FReply::Handled();
        auto Guarded = [this](std::function<void()> Action) {
            RunGuarded(Action);
            return FReply::Handled();
        };
        // The end-turn warning is modal: plain Enter confirms it, Esc (above) reviews, every other key is held back
        // so shortcuts cannot act behind it. Ctrl+Enter, the End Turn shortcut, deliberately does not confirm.
        if (bEndTurnCheck) {
            if (Key == EKeys::Enter && !Event.IsControlDown()) return Guarded([this] { ConfirmEndTurn(); });
            return FReply::Handled();
        }
        if (Key == EKeys::F1) return Guarded([this] { GoPanel(PanelGalaxy); });
        if (Key == EKeys::F2) return Guarded([this] { GoPanel(PanelSystem); });
        if (Key == EKeys::F3) return Guarded([this] { GoPanel(PanelResearch); });
        if (Key == EKeys::F4) return Guarded([this] { GoPanel(PanelDiplomacy); });
        if (Key == EKeys::F5) return Guarded([this] { GoPanel(PanelIntel); });
        if (Key == EKeys::F6) return Guarded([this] { GoPanel(PanelEmpire); });
        if (Key == EKeys::F7) return Guarded([this] { GoPanel(PanelCombat); });
        if (Key == EKeys::BackSpace && Shell == EUiView::Game && !(ActivePanel == PanelCombat && Session.pending)) {
            if (ActivePanel != PanelGalaxy || PreviousPanel != PanelGalaxy) {
                return Guarded([this] { GoBack(); });
            }
        }
        if (Key == EKeys::Enter && Event.IsControlDown())
            return Guarded([this] { if (State.outcome == botf::CampaignOutcome::Ongoing) RequestEndTurn(); });
        if (Key == EKeys::S && !Event.IsControlDown()) { bSummaryOpen = !bSummaryOpen; bMenuOpen = false; return FReply::Handled(); }
        if (Key == EKeys::S && Event.IsControlDown()) return Guarded([this] { if (bHasActiveCampaign) Save(); });
        if (Key == EKeys::Tilde) return Guarded([this] { if (bMenuOpen) bMenuOpen = false; else OpenSectorMenu(SelectedCell, FVector2D::ZeroVector); });
        if (ActivePanel == PanelCombat && Session.pending) {
            using botf::tactical::TacticalManeuver;
            if (bEncounterChoice) {
                if (Key == EKeys::F) return Guarded([this] { bEncounterChoice = false; UpdateScene(); });
                if (Key == EKeys::A) return Guarded([this] { TacAuto(); });
                if (Key == EKeys::H) return Guarded([this] { bEncounterChoice = false; bTacOrderAll = true; TacOrder(TacticalManeuver::Hail); TacNextRound(); });
            } else {
                if (Key == EKeys::One) return Guarded([this] { TacOrder(TacticalManeuver::Engage); });
                if (Key == EKeys::Two) return Guarded([this] { TacOrder(TacticalManeuver::Charge); });
                if (Key == EKeys::Three) return Guarded([this] { TacOrder(TacticalManeuver::Circle); });
                if (Key == EKeys::Four) return Guarded([this] { TacOrder(TacticalManeuver::Hold); });
                if (Key == EKeys::Five) return Guarded([this] { TacOrder(TacticalManeuver::Retreat); });
                if (Key == EKeys::Six) return Guarded([this] { TacOrder(TacticalManeuver::Assault); });
                if (Key == EKeys::Seven) return Guarded([this] { TacOrder(TacticalManeuver::Harry); });
                if (Key == EKeys::Eight) return Guarded([this] { TacOrder(TacticalManeuver::Flyby); });
                if (Key == EKeys::Nine) return Guarded([this] { TacOrder(TacticalManeuver::Strafe); });
                if (Key == EKeys::Zero) return Guarded([this] { TacOrder(TacticalManeuver::Evade); });
                if (Key == EKeys::R) return Guarded([this] { TacOrder(TacticalManeuver::Ram); });
                if (Key == EKeys::H) return Guarded([this] { TacOrder(TacticalManeuver::Hail); });
                if (Key == EKeys::Tab) return Guarded([this] { NextTacShip(); });
                if (Key == EKeys::T) return Guarded([this] { NextTacTarget(); });
                if (Key == EKeys::C && Scene.IsValid()) { Scene->CycleCameraMode(); return FReply::Handled(); }
                if (Key == EKeys::Home && Scene.IsValid()) { Scene->ResetCameraOrbit(); return FReply::Handled(); }
                if (Key == EKeys::Left && Scene.IsValid()) { Scene->AddCameraOrbit(-15.f, 0.f); return FReply::Handled(); }
                if (Key == EKeys::Right && Scene.IsValid()) { Scene->AddCameraOrbit(15.f, 0.f); return FReply::Handled(); }
                if (Key == EKeys::Up && Scene.IsValid()) { Scene->AddCameraOrbit(0.f, 10.f); return FReply::Handled(); }
                if (Key == EKeys::Down && Scene.IsValid()) { Scene->AddCameraOrbit(0.f, -10.f); return FReply::Handled(); }
                if (Key == EKeys::SpaceBar) return Guarded([this] { if (Session.pending->isFinished) TacContinue(); else TacNextRound(); });
            }
        }
        if (Key == EKeys::Tab) return Guarded([this] { NextShip(); if (Chart.IsValid()) Chart->CentreOn(SelectedCell); });
        if (ActivePanel == PanelGalaxy) {
            if (Key == EKeys::M) { MapView.Mode = MapView.Mode == BotfUi::EMapMode::Military ? BotfUi::EMapMode::Economic : BotfUi::EMapMode::Military; return FReply::Handled(); }
            if (Key == EKeys::N) { MapView.bNames = !MapView.bNames; return FReply::Handled(); }
            if (Key == EKeys::R) { MapView.bRange = !MapView.bRange; return FReply::Handled(); }
            if (Key == EKeys::G) { MapView.bGrid = !MapView.bGrid; return FReply::Handled(); }
            if (Key == EKeys::P) return Guarded([this] { if (Fleet()) MapView.bPlotting = true; });
            if (Key == EKeys::C) { if (Chart.IsValid()) Chart->CentreOn(SelectedCell); return FReply::Handled(); }
            if (Key == EKeys::Home) { if (Chart.IsValid()) Chart->ResetView(); return FReply::Handled(); }
            if (Key == EKeys::Equals || Key == EKeys::Add) { if (Chart.IsValid()) Chart->ZoomBy(1.2f, ChartMiddle()); return FReply::Handled(); }
            if (Key == EKeys::Hyphen || Key == EKeys::Subtract) { if (Chart.IsValid()) Chart->ZoomBy(1.f / 1.2f, ChartMiddle()); return FReply::Handled(); }
            if (Key == EKeys::Enter) return Guarded([this] { ActivateCell(SelectedCell); });
            if (Key == EKeys::Left) return Guarded([this] { MoveSelection(-1, 0); });
            if (Key == EKeys::Right) return Guarded([this] { MoveSelection(1, 0); });
            if (Key == EKeys::Up) return Guarded([this] { MoveSelection(0, -1); });
            if (Key == EKeys::Down) return Guarded([this] { MoveSelection(0, 1); });
        }
        if (ActivePanel == PanelSystem) {
            if (Key == EKeys::Q) return Guarded([this] { CycleColony(-1); });
            if (Key == EKeys::E) return Guarded([this] { CycleColony(1); });
        }
        return bPreview ? FReply::Unhandled() : SCompoundWidget::OnKeyDown(Geometry, Event);
    }
    SLATE_BEGIN_ARGS(SBotfInterface) {}
#if WITH_DEV_AUTOMATION_TESTS
        SLATE_ARGUMENT(FString, TestSnapshotFolder)
#endif
    SLATE_END_ARGS()
    void Construct(const FArguments& Args, ABotfCombatScene* InScene) {
#if WITH_DEV_AUTOMATION_TESTS
        TestSnapshotFolder = Args._TestSnapshotFolder;
#else
        (void)Args;
#endif
#if WITH_DEV_AUTOMATION_TESTS
        Campaign.FolderOverride = TestSnapshotFolder;
#endif
        Scene = InScene;
        BattleBrush.DrawAs = ESlateBrushDrawType::Image;
        BattleBrush.ImageSize = FVector2D(1920, 1080);
        BindBattleTexture();
        // The widget consumes campaign changes: a replaced campaign drops every selection made in the old one, and
        // any change repaints the chart (its supply and route caches are keyed on the draft preview).
        Campaign.Subscribe([this](EBotfCampaignChange Change) {
            if (Change == EBotfCampaignChange::Replaced) { OrderCheck = FColonyOrderCheck(); ResetCampaignSelection(); }
            if (Chart.IsValid()) Chart->Invalidate(EInvalidateWidgetReason::Paint);
        });
        State = botf::generateReferenceCampaign(1999);
        SelectedCell = State.systems.front().y * State.width + State.systems.front().x;
        RefreshSaves();
        Status = SavePath.IsEmpty() ? TEXT("Welcome, commander.")
            : TEXT("A saved campaign is available: ") + FPaths::GetBaseFilename(SavePath) + TEXT(".");
        // Command deck. Layer order: chart, HUD (top bar, docks, bottom bar), sheet, popups.
        const TSharedRef<SWidget> GameShell = SNew(SOverlay)
            + SOverlay::Slot() [ SAssignNew(Chart, BotfUi::SStarChart, ChartBinding()) ]
            + SOverlay::Slot().VAlign(VAlign_Top) [ BuildTopBar() ]
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill).Padding(10, 74, 0, 64) [ BuildLeftDock() ]
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Fill).Padding(0, 74, 10, 64) [ BuildInbox() ]
            + SOverlay::Slot().VAlign(VAlign_Bottom) [ BuildBottomBar() ]
            + SOverlay::Slot() [ BuildSheetLayer() ]
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
                .Padding(TAttribute<FMargin>::CreateLambda([this] { return FMargin(MenuAt.X, MenuAt.Y, 0, 0); })) [ BuildSectorMenu() ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(60) [ BuildSummaryWindow() ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(60) [ BuildEventCard() ]
            + SOverlay::Slot() [ BuildEndTurnCheck() ]
            + SOverlay::Slot() [ BuildOutcomeOverlay() ];
        ChildSlot [ SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return static_cast<int>(Shell); })
            + SWidgetSwitcher::Slot() [ BuildTitleScreen() ]
            + SWidgetSwitcher::Slot() [ GameShell ]
            + SWidgetSwitcher::Slot() [ BuildSetupScreen() ]
            + SWidgetSwitcher::Slot() [ BuildLoadScreen() ]
            + SWidgetSwitcher::Slot() [ BuildSettingsScreen() ]
            + SWidgetSwitcher::Slot() [ BuildMenuScreen() ]
            + SWidgetSwitcher::Slot() [ BuildConfirmScreen() ] ];
        RefreshAll();
        UE_LOG(LogTemp, Display, TEXT("BOTF_INTERFACE_READY seed=1999 campaign=2 systems=%d"), static_cast<int>(State.systems.size()));
    }
private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FBotfUiShellAutomationTest;
    FString TestSnapshotFolder;
#endif
    enum class EUiView : int { Title, Game, Setup, Load, Settings, Menu, Confirm };
    enum class EPendingAction : int { None, StartCampaign, LoadCampaign, Quit };
    // The campaign (state, turn transaction, draft preview, saves) lives in FBotfCampaign. The references below keep
    // the existing widget code and the automation test working unchanged while the widget is split into views;
    // Campaign must stay declared before them (member initialisation order).
    FBotfCampaign Campaign;
    botf::GameState& State = Campaign.State;
    botf::session::TurnSession& Session = Campaign.Session;   // while active, State is the committed pre-turn campaign
    std::uint64_t& StateVersion = Campaign.StateVersion;
    std::uint64_t& CampaignVersion = Campaign.CampaignVersion;
    bool& bHasActiveCampaign = Campaign.bHasActiveCampaign;
    bool& bHasSavedBaseline = Campaign.bHasSavedBaseline;
    std::uint64_t& SavedFingerprint = Campaign.SavedFingerprint;
    FString& SavedSessionText = Campaign.SavedSessionText;
    TArray<FString>& Saves = Campaign.Saves;
    TArray<FBotfSnapshotInfo>& SaveInfos = Campaign.SaveInfos;
    FString& SavePath = Campaign.SavePath;
    bool bEncounterChoice = false;          // Original encounter dialog: Fight / Auto
    int TacShip = 0, TacTarget = 0;         // Selected own ship and enemy target in the live encounter
    bool bTacOrderAll = false;              // Apply the next manoeuvre to every active own ship
    FString TacLog;
    int SelectedTaskForce = 0;              // Non-zero when a task force (not a single ship) is selected
    std::set<int> FormSelection;            // Colocated ships marked for a new task force
    std::set<int> ExpandedTaskForces;
    // The actor may be destroyed before the widget (end of play): never hold it, or its render target, strongly.
    TWeakObjectPtr<ABotfCombatScene> Scene;
    FSlateBrush BattleBrush;                // resource re-bound through BindBattleTexture; drawn only while Scene is valid
    TSharedPtr<SBotfSettingsPanel> SettingsPanel;
    EUiView Shell = EUiView::Title, SubPageReturn = EUiView::Title, ConfirmReturn = EUiView::Title;
    EPendingAction PendingAction = EPendingAction::None;
    FString PendingLoadPath;
    int ActivePanel = PanelGalaxy, PreviousPanel = PanelGalaxy;
    int Selected = 1;
    int SelectedCell = 0, SelectedFleet = 1, SelectedEnemyFleet = 0, SelectedEmpire = 2, SelectedMinor = 0;
    int SetupEmpires = 2, SetupSize = 0, SetupSeed = 0, SetupPlayer = 1, SetupRuleset = 1;
    int SetupEra = static_cast<int>(botf::StartingEra::Early);
    int SetupPreset = static_cast<int>(botf::RulesetPreset::Remastered);   // new campaigns default to Remastered
    // Schema-13 gameplay layer presentation state (all ids are campaign-local; ResetCampaignSelection drops them).
    bool bEventsOpen = false;               // galactic event card
    bool bOutcomeDismissed = false;         // the end-of-game overlay was closed to review the galaxy
    int UpgradeFunder = 0;                  // colony chosen to fund a starbase upgrade (0 = nearest that can)
    int SelectedTemplate = 0;               // build template picked on the Solar System sheet
    FString TemplateNameText;               // name typed for a new or updated build template
    TSharedPtr<SEditableTextBox> TemplateNameBox;
    std::vector<botf::view::BriefingEntry> LastBriefing;   // last resolved turn's chronicle, for the inbox
    int ResearchMode = 0, DiplomacyMode = 0, IntelMode = 0, EmpireMode = 0, SummaryMode = 0;
    int EmpireColonyFilter = 0, EmpireFleetFilter = 0;
    int SelectedColonyInEmpire = -1, SelectedFleetInEmpire = 0;
    int TradeSource = 0, TradeDestination = 0, IntelTargetSystem = 0, BattleIndex = 0, BattleRoundIndex = 0;
    botf::diplomacy::TreatyType OfferTreaty = botf::diplomacy::TreatyType::NonAggression;
    botf::diplomacy::TreatyDuration OfferDuration = botf::diplomacy::TreatyDuration::Turns25;
    botf::diplomacy::DiplomaticTone OfferTone = botf::diplomacy::DiplomaticTone::Cordial;
    std::int64_t OfferCredits = 0, DemandCredits = 0;
    int DraftSecurity = 100;
    std::vector<std::pair<int, std::pair<int, int>>> DraftIntelBudget;
    bool bIntelDraft = false;
    int ReferenceCategory = 0, ReferencePage = 0, ReferenceSelected = 0, SelectedResearchField = 0;
    int SavePage = 0;
    bool SetupEvents = true;
    bool bMenuOpen = false, bSummaryOpen = false, bAllocationDirty = false, bEndTurnCheck = false;
    int MenuCell = -1;                      // sector the context menu was opened on
    FVector2D MenuAt = FVector2D::ZeroVector;
    int CatalogueMode = 0;                  // System sheet: 0 structures, 1 starships
    int InboxCount = 0;
    std::array<int, botf::ResearchFields> DraftAllocation{};
    std::array<bool, botf::ResearchFields> AllocationLocked{};
    BotfUi::FMapView MapView;
    FString ReferenceSearch;
    TSharedPtr<SEditableTextBox> ReferenceSearchBox;
    FString Status, LastReport;
    TArray<FString> LastEvents;             // Last resolved turn's notable lines, for the inbox
    std::set<std::tuple<int, int, int, int>> RefusedCalls;   // calls to arms the player chose to let lapse (ally, defender, aggressor, issued)
    // Every wholesale replacement of State goes through here (the campaign notifies Replaced, see Construct).
    void ReplaceState(botf::GameState&& Next) { Campaign.ReplaceState(std::move(Next)); }
    // Ids are only meaningful inside one campaign: a loaded save can reuse the same task force, ship or system ids
    // for different objects. Everything the player had picked in the outgoing campaign is dropped here.
    void ResetCampaignSelection() {
        SelectedFleet = SelectedTaskForce = SelectedEnemyFleet = SelectedMinor = 0;
        FormSelection.clear(); ExpandedTaskForces.clear();
        TradeSource = TradeDestination = IntelTargetSystem = 0;
        bIntelDraft = false; DraftIntelBudget.clear(); DraftSecurity = 100;
        BattleIndex = BattleRoundIndex = 0; ShownBattle = FShownBattle();
        MenuCell = -1; bMenuOpen = bEndTurnCheck = false; MapView.bPlotting = false;
        bTurnForecast = bLastForecast = false; LastFleetUpkeep = LastTradeIncome = 0;
        RefusedCalls.clear();
        bEventsOpen = bOutcomeDismissed = false; UpgradeFunder = SelectedTemplate = 0; LastBriefing.clear();
    }
    TSharedPtr<SVerticalBox> SectorRows, BuildRows, ShipRows, QueueRows, RaceRows, IntelCards, IntelReportRows,
        SystemTableRows, FleetTableRows, ReferenceRows, SaveRows, MenuRows, InboxRows, SheetPlanetRows, ProposalRows;
    TSharedPtr<SVerticalBox> EventRows, TemplateRows;
    TSharedPtr<SVerticalBox> PlanetRows;
    TSharedPtr<SVerticalBox> TacRows;
    TSharedPtr<SVerticalBox> TacFriendlyRows;
    TSharedPtr<SVerticalBox> TacHostileRows;
    TSharedPtr<BotfUi::SStarChart> Chart;

    // ---- Theme and LCARS building blocks (presentation only) ----
    int ThemeEmpire() const { return Shell == EUiView::Setup || !bHasActiveCampaign ? SetupPlayer : State.playerEmpire; }
    const FPalette& P() const { return BotfUi::Palette(ThemeEmpire()); }
    TAttribute<FSlateColor> C(FLinearColor FPalette::* Field) const {
        return TAttribute<FSlateColor>::CreateLambda([this, Field] { return FSlateColor(P().*Field); });
    }
    // Attribute and paint code runs inside Slate, which is not exception-safe: a core exception must never unwind
    // through it. Every text attribute goes through SafeText, every control predicate through SafeBool.
    static FString SafeText(const std::function<FString()>& Value) {
        FString Out, Error;
        if (!FBotfCampaign::Run([&Out, &Value] { Out = Value(); }, Error)) return TEXT("[unavailable: ") + Error + TEXT("]");
        return Out;
    }
    static bool SafeBool(const std::function<bool()>& Value, bool Fallback) {
        bool Out = Fallback;
        FString Error;
        if (!FBotfCampaign::Run([&Out, &Value] { Out = Value(); }, Error)) return Fallback;
        return Out;
    }
    static TAttribute<FText> Dyn(std::function<FString()> Value) {
        return TAttribute<FText>::CreateLambda([Value] { return Txt(SafeText(Value)); });
    }
    // The single place UI actions meet the campaign's exception boundary: a rejected order is reported in Status.
    bool RunGuarded(const std::function<void()>& Action) {
        FString Error;
        if (FBotfCampaign::Run([this, &Action] { Action(); RefreshAll(); }, Error)) return true;
        Status = TEXT("Order rejected: ") + Error;
        return false;
    }
    TSharedRef<SWidget> Para(const TAttribute<FText>& Value, int Size, FLinearColor FPalette::* Field, bool Bold = false) const {
        return SNew(STextBlock).Text(Value).Font(Font(Size, Bold)).ColorAndOpacity(C(Field)).AutoWrapText(true);
    }
    TSharedRef<SWidget> Heading(const TAttribute<FText>& Value, int Size, FLinearColor FPalette::* Field) const {
        return SNew(STextBlock).Text(Value).Font(BotfUi::Condensed(Size)).ColorAndOpacity(C(Field)).AutoWrapText(true);
    }
    // Single-line text for slots that size to their content (AutoWidth): wrapping there would use the text's own
    // previous width as its wrap width and can stick narrow after a resize.
    TSharedRef<SWidget> Label(const TAttribute<FText>& Value, int Size, FLinearColor FPalette::* Field, bool bCondensed = false) const {
        return SNew(STextBlock).Text(Value).Font(bCondensed ? BotfUi::Condensed(Size) : Font(Size)).ColorAndOpacity(C(Field));
    }
    TSharedRef<SWidget> Fill(EShape Shape, FLinearColor FPalette::* Field,
                             const TSharedRef<SWidget>& Content = SNullWidget::NullWidget, FMargin Pad = FMargin(0)) const {
        return SNew(SBorder).BorderImage_Lambda([this, Shape] { return BotfUi::Styles().Get(ThemeEmpire(), Shape); })
            .BorderBackgroundColor(C(Field)).Padding(Pad) [ Content ];
    }
    TSharedRef<SWidget> Bar(EShape Shape, FLinearColor FPalette::* Field, float Width, float Height) const {
        return SNew(SBox).WidthOverride(Width > 0 ? FOptionalSize(Width) : FOptionalSize())
            .HeightOverride(Height > 0 ? FOptionalSize(Height) : FOptionalSize()) [ Fill(Shape, Field) ];
    }
    static TSharedRef<SWidget> Swatch(int Empire, float Height) {
        return SNew(SBox).HeightOverride(Height > 0 ? FOptionalSize(Height) : FOptionalSize())
            [ SNew(SBorder).BorderImage(BotfUi::Styles().Get(Empire, EShape::Block)).BorderBackgroundColor(BotfUi::FactionColor(Empire)) ];
    }
    TSharedRef<SWidget> Screen(const TSharedRef<SWidget>& Content, FMargin Pad = FMargin(18)) const {
        return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(C(&FPalette::Void)).Padding(Pad)
            [ Content ];
    }
    TSharedRef<SWidget> Sidebar() const {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(1) [ Fill(EShape::Block, &FPalette::Primary) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0) [ Bar(EShape::Block, &FPalette::Secondary, 0, 46) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0) [ Bar(EShape::Block, &FPalette::Tertiary, 0, 20) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0) [ Bar(EShape::Block, &FPalette::Primary, 0, 12) ];
    }
    // LCARS console frame. Trailing sits in the header rail (sheet tabs and the close control).
    TSharedRef<SWidget> Console(const TAttribute<FText>& Title, const TSharedRef<SWidget>& Body, float Side = 14,
                                const TSharedRef<SWidget>& Trailing = SNullWidget::NullWidget) const {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth() [ Bar(EShape::ElbowTop, &FPalette::Primary, Side + 36, 28) ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0, 10, 0)
                [ SNew(STextBlock).Text(Title).Font(BotfUi::Condensed(17)).ColorAndOpacity(C(&FPalette::Accent)) ]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center) [ Bar(EShape::Block, &FPalette::Secondary, 0, 10) ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 4, 0) [ Trailing ]
                + SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0) [ Bar(EShape::Block, &FPalette::Tertiary, 22, 28) ]
                + SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0) [ Bar(EShape::CapRight, &FPalette::Primary, 42, 28) ] ]
            + SVerticalBox::Slot().FillHeight(1).Padding(0, 4, 0, 0)
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth() [ SNew(SBox).WidthOverride(Side) [ Sidebar() ] ]
                + SHorizontalBox::Slot().FillWidth(1).Padding(12, 6, 2, 2) [ Body ] ];
    }
    TSharedRef<SWidget> Section(const TAttribute<FText>& Title, const TSharedRef<SWidget>& Body) const {
        return Fill(EShape::Block, &FPalette::Field, SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ Bar(EShape::CapLeft, &FPalette::Secondary, 18, 14) ]
                // The title may be long (a ship and its target); it takes the row and ends in an ellipsis rather than
                // running past a narrow column. The rule after it keeps a short fixed stub.
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(8, 0)
                [ SNew(STextBlock).Text(Title).Font(BotfUi::Condensed(11)).ColorAndOpacity(C(&FPalette::Tertiary))
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis).ToolTipText(Title) ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ Bar(EShape::Block, &FPalette::Secondary, 36, 3) ] ]
            + SVerticalBox::Slot().AutoHeight() [ Body ],
            FMargin(10, 8, 10, 10));
    }
    // Honest label where the original showed information this build cannot yet provide.
    TSharedRef<SWidget> Unavailable(const FString& What) const {
        return Fill(EShape::Row, &FPalette::Field, SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0) [ Bar(EShape::Block, &FPalette::Muted, 4, 0) ]
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center) [ Para(Txt(What + TEXT(": not yet available")), 9, &FPalette::Muted) ],
            FMargin(8, 5));
    }
    TSharedRef<SWidget> Readout(const FString& Caption, std::function<FString()> Value, const TAttribute<FSlateColor>& Color,
                                const TAttribute<FText>& Tip = TAttribute<FText>()) const {
        TSharedRef<SWidget> Chip = Fill(EShape::Block, &FPalette::Field, SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(STextBlock).Text(Txt(Caption)).Font(BotfUi::Condensed(8)).ColorAndOpacity(C(&FPalette::Muted)) ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(STextBlock).Text(Dyn(Value)).Font(BotfUi::Condensed(13)).ColorAndOpacity(Color) ],
            FMargin(8, 3, 8, 4));
        if (Tip.IsSet()) Chip->SetToolTipText(Tip);
        return Chip;
    }
    // Every control funnels through here: consistent LCARS states, and order errors surface in Status.
    TSharedRef<SWidget> DynControl(TAttribute<FText> Label, std::function<void()> Action, std::function<bool()> Enabled = nullptr,
                                   std::function<bool()> IsSelected = nullptr, ERole Role = ERole::Primary, FMargin Pad = FMargin(8, 7),
                                   bool bRow = false, int FontSize = 10, const FString& Tip = FString(), int FixedEmpire = 0) {
        TFunction<bool()> En; if (Enabled) En = [Enabled] { return SafeBool(Enabled, false); };
        TFunction<bool()> Sel; if (IsSelected) Sel = [IsSelected] { return SafeBool(IsSelected, false); };
        return BotfUi::MakeButton(Label, [this, Action] { RunGuarded(Action); },
            En, Sel, [this, FixedEmpire] { return FixedEmpire ? FixedEmpire : ThemeEmpire(); },
            Role, Pad, bRow, FontSize, Txt(Tip));
    }
    TSharedRef<SWidget> Control(const FString& Label, std::function<void()> Action, std::function<bool()> Enabled,
                                std::function<bool()> IsSelected, ERole Role, FMargin Pad, bool bRow, int FontSize,
                                const FString& Tip, int FixedEmpire = 0) {
        return DynControl(Txt(Label), Action, Enabled, IsSelected, Role, Pad, bRow, FontSize, Tip, FixedEmpire);
    }
    TSharedRef<SWidget> Button(const FString& Label, std::function<void()> Action,
                              std::function<bool()> Enabled = [] { return true; }, float HorizontalPadding = 12,
                              const FString& Tip = FString(), ERole Role = ERole::Primary) {
        return Control(Label, Action, Enabled, nullptr, Role, FMargin(HorizontalPadding + 4, 7), false, 10, Tip);
    }
    TSharedRef<SWidget> Pill(const FString& Label, std::function<void()> Action, std::function<bool()> Enabled = nullptr,
                             ERole Role = ERole::Primary, const FString& Tip = FString()) {
        return Control(Label, Action, Enabled, nullptr, Role, FMargin(8, 5), false, 9, Tip);
    }
    TSharedRef<SWidget> Option(const FString& Label, std::function<void()> Action, std::function<bool()> IsSelected,
                              ERole Role = ERole::Secondary, int FixedEmpire = 0, const FString& Tip = FString()) {
        return Control(Label, Action, nullptr, IsSelected, Role, FMargin(10, 6), false, 10, Tip, FixedEmpire);
    }
    TSharedRef<SWidget> Option(const FString& Label, std::function<void()> Action, std::function<bool()> IsSelected, const FString& Tip) {
        return Control(Label, Action, nullptr, IsSelected, ERole::Secondary, FMargin(10, 6), false, 10, Tip, 0);
    }
    TSharedRef<SWidget> ModeButton(const FString& Label, int* Mode, int Value, const FString& Tip = FString()) {
        return Control(Label, [Mode, Value] { *Mode = Value; }, nullptr, [Mode, Value] { return *Mode == Value; },
            ERole::Secondary, FMargin(12, 7), false, 10, Tip);
    }
    TSharedRef<SWidget> Row(const FString& Label, std::function<void()> Action, std::function<bool()> IsSelected,
                            std::function<bool()> Enabled = nullptr, const FString& Tip = FString()) {
        return Control(Label, Action, Enabled, IsSelected, ERole::Primary, FMargin(4, 5, 8, 5), true, 10, Tip);
    }
    TSharedRef<SWidget> Scroll(const TSharedRef<SWidget>& Body) {
        return SNew(SScrollBox) + SScrollBox::Slot() [ Body ];
    }
    TSharedRef<SWidget> Stack(std::initializer_list<TSharedRef<SWidget>> Items, float Gap = 8) {
        auto Box = SNew(SVerticalBox);
        for (const auto& Item : Items) Box->AddSlot().AutoHeight().Padding(0, 0, 0, Gap) [ Item ];
        return Box;
    }
    TSharedRef<SWidget> Line(std::initializer_list<TSharedRef<SWidget>> Items, float Gap = 5) {
        auto Box = SNew(SHorizontalBox);
        for (const auto& Item : Items) Box->AddSlot().AutoWidth().Padding(0, 0, Gap, 0) [ Item ];
        return Box;
    }
    TSharedRef<SWidget> Grid(std::initializer_list<TSharedRef<SWidget>> Items, int Columns) {
        auto Panel = SNew(SUniformGridPanel).SlotPadding(FMargin(0, 0, 4, 4));
        int Index = 0;
        for (const auto& Item : Items) { Panel->AddSlot(Index % Columns, Index / Columns) [ Item ]; ++Index; }
        return Panel;
    }
    // Committed campaign plus this turn's orders; never throws (see FBotfCampaign::Preview).
    const botf::GameState& Preview() const { return Campaign.Preview(); }
    // Exact envelope of the open turn transaction; empty when none. Only serialized while a turn is open.
    FString SessionText() const { return Campaign.SessionText(); }
    // Every strategic order goes through here: no new orders while a turn transaction is open.
    void Submit(const botf::Command& Cmd) { Campaign.Submit(Cmd); }
    void CancelLast() { Campaign.CancelLast(); }
    std::uint64_t NextId() const { return Campaign.NextId(); }

    // ---- Shell flow (unchanged contracts: confirmation, pending load path, settings rollback) ----
    void ShowView(EUiView View) {
        if (Shell == EUiView::Settings && View != EUiView::Settings && SettingsPanel.IsValid())
            SettingsPanel->CancelPendingChanges();
        Shell = View;
        if (View == EUiView::Settings && SettingsPanel.IsValid() && GEngine && GEngine->GameViewport)
            SettingsPanel->RefreshFromRuntime();
        UpdateScene();
    }
    void OpenSubPage(EUiView View) {
        SubPageReturn = Shell;
        if (View == EUiView::Load) RefreshSaves();
        ShowView(View);
    }
    bool HasUnsavedChanges() const {
        // Includes an open turn transaction: tactical orders, rounds and completed encounters.
        return bHasActiveCampaign && (IntelDraftDirty() || Campaign.HasUnsavedChanges());
    }
    bool CombatPresentation() const {
        return Shell == EUiView::Game && ActivePanel == PanelCombat && (Session.pending.has_value() || CurrentBattle() != nullptr);
    }
    // Points the battle brush at the scene's render target, or at nothing once the scene is gone.
    void BindBattleTexture() {
        BattleBrush.SetResourceObject(nullptr);
        if (Scene.IsValid()) BattleBrush.SetResourceObject(Scene->GetBattleTexture());
    }
    void UpdateScene() {
        if (!Scene.IsValid()) { BindBattleTexture(); return; }
        if (Session.pending && ActivePanel == PanelCombat) {
            Scene->ConfigureEncounter(*Session.pending);
            BindBattleTexture();
            ShownBattle = FShownBattle();   // the scene now holds the live encounter, not a record
        }
        // Battle records: whatever path led here (a finished encounter, a loaded save, the inbox, F7), the scene is
        // rebuilt whenever it does not hold the selected record and round.
        else if (CombatPresentation() && !(ShownBattle == WantedBattle())) { ShowBattle(); return; }
        Scene->SetPresentationActive(CombatPresentation());
    }
    void ContinueCampaign() {
        if (bHasActiveCampaign) { ShowView(EUiView::Game); return; }
        if (!SavePath.IsEmpty()) RequestAction(EPendingAction::LoadCampaign);
    }
    TSharedRef<SWidget> MenuButton(const FString& Label, std::function<void()> Action,
                                   std::function<bool()> Enabled = nullptr, ERole Role = ERole::Primary) {
        return Control(Label, Action, Enabled, nullptr, Role, FMargin(18, 11), false, 12, FString());
    }
    TSharedRef<SWidget> BuildTitleScreen() {
        auto Factions = SNew(SHorizontalBox);
        for (int Id = 1; Id <= 5; ++Id) Factions->AddSlot().FillWidth(1).Padding(0, 0, Id < 5 ? 6 : 0, 0)
            [ SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight() [ Swatch(Id, 6) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                [ SNew(STextBlock).Text(Txt(Str(botf::empireName(Id)).ToUpper())).Font(BotfUi::Condensed(9)).ColorAndOpacity(C(&FPalette::Muted)).AutoWrapText(true) ] ];
        return Screen(Console(Txt(TEXT("MAIN MENU")), SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(8, 0, 28, 0)
            [ SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight() [ Heading(Txt(TEXT("BIRTH OF THE")), 26, &FPalette::Tertiary) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6) [ Heading(Txt(TEXT("FEDERATION")), 54, &FPalette::Accent) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18) [ Para(Txt(TEXT("Lead one of five great powers. Explore, expand, and shape the fate of the quadrant.")), 13, &FPalette::Muted) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18) [ Bar(EShape::Block, &FPalette::Secondary, 0, 4) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18) [ Factions ]
                + SVerticalBox::Slot().AutoHeight()
                [ Para(Dyn([this] {
                    return bHasActiveCampaign
                        ? FString(TEXT("Campaign in progress: ")) + Str(botf::empireName(State.playerEmpire))
                            + FString::Printf(TEXT(", turn %d  /  %s"), State.turn,
                                HasUnsavedChanges() ? TEXT("unsaved progress") : TEXT("saved"))
                        : FString::Printf(TEXT("%d saved campaigns available"), Saves.Num());
                }), 12, &FPalette::Primary, true) ] ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [ SNew(SBox).WidthOverride(300)
                [ SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                    [ MenuButton(TEXT("CONTINUE"), [this] { ContinueCampaign(); }, [this] { return bHasActiveCampaign || !SavePath.IsEmpty(); }, ERole::Accent) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                    [ MenuButton(TEXT("NEW CAMPAIGN"), [this] { OpenSubPage(EUiView::Setup); }) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                    [ MenuButton(TEXT("LOAD CAMPAIGN"), [this] { OpenSubPage(EUiView::Load); }) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                    [ MenuButton(TEXT("SETTINGS"), [this] { OpenSubPage(EUiView::Settings); }, nullptr, ERole::Secondary) ]
                    + SVerticalBox::Slot().AutoHeight()
                    [ MenuButton(TEXT("QUIT"), [this] { RequestAction(EPendingAction::Quit); }, nullptr, ERole::Alert) ] ] ],
            40));
    }
    TSharedRef<SWidget> BackRow(const FString& Caption) {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 14, 0)
            [ Button(TEXT("< BACK"), [this] { ShowView(SubPageReturn); }, [] { return true; }, 12, FString(), ERole::Tertiary) ]
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center) [ Para(Txt(Caption), 11, &FPalette::Muted) ];
    }
    TSharedRef<SWidget> BuildSetupScreen() {
        return Screen(Console(Txt(TEXT("NEW CAMPAIGN")), SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
            [ BackRow(TEXT("Choose your empire and galaxy. Escape returns without changing your current campaign.")) ]
            + SVerticalBox::Slot().FillHeight(1)
            [ SNew(SScrollBox) + SScrollBox::Slot() [ BuildCampaignControls() ] ], 24));
    }
    TSharedRef<SWidget> BuildLoadScreen() {
        return Screen(Console(Txt(TEXT("LOAD CAMPAIGN")), SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
            [ BackRow(TEXT("Select a saved campaign, review it, then load. You will be asked before unsaved progress is replaced.")) ]
            + SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Fill)
            [ SNew(SBox).MaxDesiredWidth(1000) [ BuildSnapshotBrowser() ] ], 24));
    }
    TSharedRef<SWidget> BuildSettingsScreen() {
        return Screen(SAssignNew(SettingsPanel, SBotfSettingsPanel)
            .ThemeEmpire_Lambda([this] { return ThemeEmpire(); })
            .OnClose(FSimpleDelegate::CreateLambda([this] { ShowView(SubPageReturn); })), FMargin(16));
    }
    TSharedRef<SWidget> CenteredConsole(const FString& Title, const TSharedRef<SWidget>& Body) {
        return Screen(SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Center).VAlign(VAlign_Center)
            [ SNew(SBox).MinDesiredWidth(480).MaxDesiredWidth(640)
                [ Console(Txt(Title), Body, 24) ] ], FMargin(26));
    }
    TSharedRef<SWidget> BuildMenuScreen() {
        return CenteredConsole(TEXT("CAMPAIGN MENU"), SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ MenuButton(TEXT("RESUME  /  ESC"), [this] { ShowView(EUiView::Game); }, nullptr, ERole::Accent) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ MenuButton(TEXT("SAVE CAMPAIGN"), [this] { Save(); }) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ MenuButton(TEXT("LOAD CAMPAIGN"), [this] { OpenSubPage(EUiView::Load); }) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ MenuButton(TEXT("NEW CAMPAIGN"), [this] { OpenSubPage(EUiView::Setup); }) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ MenuButton(TEXT("SETTINGS"), [this] { OpenSubPage(EUiView::Settings); }, nullptr, ERole::Secondary) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ MenuButton(TEXT("RETURN TO TITLE"), [this] { ShowView(EUiView::Title); }, nullptr, ERole::Tertiary) ]
            + SVerticalBox::Slot().AutoHeight()
            [ MenuButton(TEXT("QUIT"), [this] { RequestAction(EPendingAction::Quit); }, nullptr, ERole::Alert) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)
            [ Para(Dyn([this] { return Status; }), 11, &FPalette::Primary) ]);
    }
    TSharedRef<SWidget> BuildConfirmScreen() {
        return CenteredConsole(TEXT("CONFIRM ACTION"), SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
            [ Para(Dyn([this] { return ConfirmationText(); }), 13, &FPalette::Text) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ MenuButton(TEXT("SAVE AND PROCEED"), [this] { ConfirmAction(true); }, [this] { return bHasActiveCampaign; }) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ MenuButton(TEXT("PROCEED WITHOUT SAVING"), [this] { ConfirmAction(false); }, nullptr, ERole::Alert) ]
            + SVerticalBox::Slot().AutoHeight()
            [ MenuButton(TEXT("CANCEL"), [this] { CancelConfirmation(); }, nullptr, ERole::Tertiary) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)
            [ Para(Dyn([this] { return Status; }), 11, &FPalette::Primary) ]);
    }
    FString ConfirmationText() const {
        const bool Dirty = HasUnsavedChanges();
        const FString DraftNote = IntelDraftDirty() ? TEXT(" Save first to apply the intelligence allocation, or proceed without saving to discard it.") : TEXT("");
        if (PendingAction == EPendingAction::StartCampaign)
            return Dirty
                ? FString(TEXT("Starting a new campaign will replace the unsaved campaign in progress. Save first, start without saving, or cancel to keep playing.")) + DraftNote
                : TEXT("Start this new campaign? It will replace the campaign in progress. You can save first or cancel.");
        if (PendingAction == EPendingAction::LoadCampaign)
            return Dirty
                ? FString(TEXT("Loading the selected save will replace the unsaved campaign in progress. Save first, load without saving, or cancel.")) + DraftNote
                : TEXT("Load the selected save and replace the campaign in progress?");
        return Dirty
            ? FString(TEXT("Quit the game? Your campaign has unsaved progress. Save before quitting, quit without saving, or cancel.")) + DraftNote
            : TEXT("Quit the game? Your campaign is saved, or no campaign is in progress.");
    }
    void RequestAction(EPendingAction Action) {
        if (Action == EPendingAction::None) return;
        if (Action == EPendingAction::LoadCampaign) {
            if (SavePath.IsEmpty()) throw std::runtime_error("Choose a compatible snapshot before loading");
            PendingLoadPath = SavePath;
        }
        if (Action == EPendingAction::Quit || HasUnsavedChanges()) {
            PendingAction = Action; ConfirmReturn = Shell;
            ShowView(EUiView::Confirm);
            return;
        }
        ExecuteAction(Action);
    }
    void CancelConfirmation() {
        PendingAction = EPendingAction::None;
        PendingLoadPath.Empty();
        ShowView(ConfirmReturn);
    }
    void ConfirmAction(bool SaveFirst) {
        if (PendingAction == EPendingAction::None) return;
        if (SaveFirst && bHasActiveCampaign) {
            Save();
            if (HasUnsavedChanges()) throw std::runtime_error("Verified save did not preserve the current campaign");
        }
        const auto Action = PendingAction;
        ExecuteAction(Action);
        PendingAction = EPendingAction::None;
    }
    void ExecuteAction(EPendingAction Action) {
        if (Action == EPendingAction::StartCampaign) {
            StartCampaign(); ShowView(EUiView::Game);
        } else if (Action == EPendingAction::LoadCampaign) {
            if (!PendingLoadPath.IsEmpty()) SavePath = PendingLoadPath;
            if (SavePath.IsEmpty()) throw std::runtime_error("Choose a compatible snapshot before loading");
            Load(); ShowView(EUiView::Game);
            PendingLoadPath.Empty();
        } else if (Action == EPendingAction::Quit) RequestQuit();
    }
    // Quits through the engine's normal path: a packaged game exits, a Play-In-Editor session ends without killing
    // the editor. RequestExit is only a last resort outside the editor when no world is available.
    void RequestQuit() {
        UWorld* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
        if (World) {
            UKismetSystemLibrary::QuitGame(World, World->GetFirstPlayerController(), EQuitPreference::Quit, false);
            return;
        }
        if (!GIsEditor) FPlatformMisc::RequestExit(false);
    }

    // ---- Empire-wide figures ----
    // Credits the treasury will gain or lose when this turn resolves, from the draft with this turn's orders: colony
    // income less structure upkeep, trade route revenue, less ship maintenance. Mirrors the charges core applies at
    // turn end (Simulation.cpp advance); battles, random events and rival actions during resolution can still differ.
    // One-off order costs (purchases, gifts) are already deducted in the draft treasury and reported as Spend.
    // The arithmetic lives in BotfViewModel.h (botf::view::forecastIncome); the rules it mirrors are listed there.
    struct FIncomeForecast {
        int Colonies = 0, Trade = 0, Treaties = 0, Ships = 0;   // Trade includes treaty dividends, as core reports it
        std::int64_t Spend = 0;
        int Net() const { return Colonies + Trade - Ships; }
    };
    FIncomeForecast TurnForecast, LastForecast;   // taken when End Turn opens the transaction; the last resolved turn's
    bool bTurnForecast = false, bLastForecast = false;
    int LastFleetUpkeep = 0, LastTradeIncome = 0;
    // Cached per draft preview: the top bar asks for it several times a frame.
    mutable std::uint64_t ForecastGeneration = 0;
    mutable FIncomeForecast ForecastCache;
    FIncomeForecast IncomeForecast() const {
        const std::uint64_t Generation = Campaign.PreviewGeneration();
        if (Generation != ForecastGeneration) {
            FIncomeForecast Out;
            FString Error;
            FBotfCampaign::Run([this, &Out] {
                const botf::view::IncomeForecast F = botf::view::forecastIncome(State, Preview(), State.playerEmpire);
                Out.Colonies = F.colonies; Out.Trade = F.tradeReported(); Out.Treaties = F.treaties; Out.Ships = F.ships; Out.Spend = F.spend;
            }, Error);
            ForecastCache = Out;
            ForecastGeneration = Generation;
        }
        return ForecastCache;
    }
    FString IncomeTip() const {
        const auto F = IncomeForecast();
        FString Tip = FString::Printf(TEXT("Expected change at turn end: %+d\nColonies (income less structure upkeep) %+d\nTrade routes and treaty dividends %+d (treaties %+d)\nShip maintenance -%d"),
            F.Net(), F.Colonies, F.Trade, F.Treaties, F.Ships);
        if (F.Spend) Tip += FString::Printf(TEXT("\nOrders this turn already commit %lld credits (purchases, gifts)"), static_cast<long long>(F.Spend));
        return Tip + TEXT("\nBattles, events and rival actions during the turn can change the result.");
    }
    FString SystemName(int SystemId) const {
        for (const auto& S : State.systems) if (S.id == SystemId) return Str(S.name);
        return FString::Printf(TEXT("System %d"), SystemId);
    }
    TArray<FString> Alerts() const {
        TArray<FString> Out;
        if (State.outcome != botf::CampaignOutcome::Ongoing) Out.Add(OutcomeText());
        const auto& Draft = Preview();
        for (const auto& Col : Draft.colonies) {
            if (Col.ownerId != State.playerEmpire) continue;
            const auto E = botf::economy(Draft, Col);
            if (E.foodDeficit > 0) Out.Add(FString::Printf(TEXT("%s: food shortage of %d"), *SystemName(Col.systemId), E.foodDeficit));
            if (Col.queue.empty()) Out.Add(SystemName(Col.systemId) + TEXT(": nothing under construction"));
        }
        if (const auto Income = IncomeForecast(); Income.Net() < 0)
            Out.Add(FString::Printf(TEXT("Treasury falls by %d per turn (ship maintenance %d)"), -Income.Net(), Income.Ships));
        for (const auto& Offer : State.pendingMajorOffers)
            if (Offer.target == State.playerEmpire)
                Out.Add(FString::Printf(TEXT("%s proposes %s (answer due turn %d)"),
                    *Str(botf::empireName(Offer.proposer)),
                    Offer.type == botf::CommandType::OfferPeace ? TEXT("peace") : TEXT("a trade treaty"), Offer.dueTurn));
        for (const auto& Proposal : botf::incomingProposals(State, State.playerEmpire))
            if (!ProposalResponsePending(Proposal.id))
                Out.Add(Str(botf::view::proposalSummary(State, Proposal, State.playerEmpire)) + TEXT(" (") + Str(botf::view::deadlineText(State, Proposal.dueTurn)) + TEXT(")"));
        for (const auto& Call : OpenCallsToArms())
            Out.Add(Str(botf::view::callToArmsText(State, Call)));
        for (const auto& Warning : botf::view::unresolvedEventWarnings(State)) Out.Add(Str(Warning));
        return Out;
    }
    // Calls to arms still awaiting the player: not yet answered by a drafted declaration and not let lapse.
    std::vector<botf::diplomacy::DefenseCall> OpenCallsToArms() const {
        std::vector<botf::diplomacy::DefenseCall> Out;
        for (const auto& Call : botf::openCallsToArms(State, State.playerEmpire)) {
            if (RefusedCalls.count(std::make_tuple(Call.ally, Call.defender, Call.aggressor, Call.issuedTurn))) continue;
            if (WarDrafted(Call.aggressor)) continue;
            Out.push_back(Call);
        }
        return Out;
    }
    bool WarDrafted(int Empire) const {
        return std::any_of(State.pending.begin(), State.pending.end(), [Empire](const botf::Command& C) {
            return C.type == botf::CommandType::DeclareWar && C.targetEmpire == Empire;
        });
    }
    FString AlertsText() const {
        const auto List = Alerts();
        return List.IsEmpty() ? FString(TEXT("No alerts.")) : FString::Join(List, TEXT("\n"));
    }
    int OwnShipCount() const {
        return static_cast<int>(std::count_if(State.fleets.begin(), State.fleets.end(), [this](const botf::Fleet& F) { return F.ownerId == State.playerEmpire; }));
    }
    int OwnColonyCount() const {
        return static_cast<int>(std::count_if(State.colonies.begin(), State.colonies.end(), [this](const botf::Colony& Col) { return Col.ownerId == State.playerEmpire; }));
    }
    int AtRiskColonyCount() const {
        int Count = 0;
        const auto& Draft = Preview();
        for (const auto& Col : Draft.colonies) {
            if (Col.ownerId != State.playerEmpire) continue;
            const auto E = botf::economy(Draft, Col);
            if (E.foodDeficit > 0 || !Col.governorWarning.empty()) ++Count;
        }
        return Count;
    }
    int IdleColonyCount() const {
        int Count = 0;
        const auto& Draft = Preview();
        for (const auto& Col : Draft.colonies) {
            if (Col.ownerId == State.playerEmpire && Col.queue.empty()) ++Count;
        }
        return Count;
    }
    int IdleFleetCount() const {
        int Count = 0;
        const auto& Draft = Preview();
        for (const auto& F : Draft.fleets) {
            if (F.ownerId == State.playerEmpire && F.x == F.targetX && F.y == F.targetY && !FleetHasDraft(F.id)) ++Count;
        }
        return Count;
    }
    int EnRouteFleetCount() const {
        int Count = 0;
        const auto& Draft = Preview();
        for (const auto& F : Draft.fleets) {
            if (F.ownerId == State.playerEmpire && (F.x != F.targetX || F.y != F.targetY || FleetHasDraft(F.id))) ++Count;
        }
        return Count;
    }
    FString SystemStrategicStatus() const {
        if (!Selected) return TEXT("[UNEXPLORED DEEP SPACE]");
        if (Owned()) return TEXT("[FEDERATION SOVEREIGN SYSTEM]");
        const auto& Systems = Preview().systems;
        if (std::none_of(Systems.begin(), Systems.end(), [this](const botf::System& S) { return S.id == Selected; })) return TEXT("[DEEP SPACE]");
        const auto* Col = botf::colonyAt(Preview(), Selected);
        if (Col) {
            return FString::Printf(TEXT("[%s OCCUPIED COLONY]"), *Str(botf::empireName(Col->ownerId)).ToUpper());
        }
        for (const auto& M : Preview().minors) {
            if (M.systemId == Selected) return FString::Printf(TEXT("[%s INDEPENDENT SYSTEM]"), *Str(M.name).ToUpper());
        }
        return TEXT("[UNCLAIMED SOLAR SYSTEM]");
    }
    FString ColonyQuickLook() const {
        const auto* Col = DraftColony();
        if (!Col || Col->ownerId != State.playerEmpire) return FString();
        const auto E = botf::economy(Preview(), *Col);
        FString Out = FString::Printf(TEXT("POP: %d / %d  |  MORALE: %d%%\n"),
            Col->population, botf::systemCapacity(Preview(), Selected), Col->morale);
        if (E.foodDeficit > 0) {
            Out += FString::Printf(TEXT("FOOD: %d / %d (DEFICIT -%d) [CRITICAL FOOD RISK!]\n"),
                E.food, Col->population, E.foodDeficit);
        } else {
            Out += FString::Printf(TEXT("FOOD: %d / %d (+%d) [FOOD SECURE]\n"),
                E.food, Col->population, E.food - Col->population);
        }
        Out += FString::Printf(TEXT("GOV: Work [%s%s] Queue [%s]\n"),
            *Str(botf::governorPriorityName(Col->workforcePriority)).ToUpper(),
            Col->workforceManualOverride && Col->workforcePriority != botf::GovernorPriority::Manual ? TEXT("*PAUSED") : TEXT(""),
            *Str(botf::governorPriorityName(Col->queuePriority)).ToUpper());
        if (!Col->queue.empty()) {
            Out += FString::Printf(TEXT("BUILDING: %s"), *ConstructionName(Preview(), Selected, Col->queue.front()));
        } else {
            Out += TEXT("BUILDING: [IDLE QUEUE]");
        }
        return Out;
    }

    // ---- Navigation ----
    void GoPanel(int Panel, bool RecordHistory = true) {
        if (RecordHistory && Panel != ActivePanel) {
            PreviousPanel = ActivePanel;
        }
        ActivePanel = Panel; bMenuOpen = false; bEndTurnCheck = false; MapView.bPlotting = false;
        if (Panel == PanelSystem && !Owned()) CycleColony(0);
        if (Panel == PanelCombat && !Session.pending) ShowBattle();
        UpdateScene();
    }
    void GoBack() {
        if (ActivePanel == PanelCombat && Session.pending) return;
        const int Target = (PreviousPanel != ActivePanel) ? PreviousPanel : PanelGalaxy;
        PreviousPanel = PanelGalaxy;
        GoPanel(Target, false);
    }
    void CycleColony(int Direction) {
        std::vector<int> Ids;
        for (const auto& Col : State.colonies) if (Col.ownerId == State.playerEmpire) Ids.push_back(Col.systemId);
        if (Ids.empty()) return;
        auto It = std::find(Ids.begin(), Ids.end(), Selected);
        int Index = It == Ids.end() ? 0 : static_cast<int>(It - Ids.begin()) + Direction;
        Index = (Index % static_cast<int>(Ids.size()) + static_cast<int>(Ids.size())) % static_cast<int>(Ids.size());
        SelectSystem(Ids[Index]);
    }
    // Tab cycles units: each task force once (by its lead ship) and each ungrouped ship.
    void NextShip() {
        const auto& Draft = Preview();
        std::vector<std::pair<int, int>> Units;   // lead fleet id, task force id
        for (const auto& F : Draft.fleets) {
            if (F.ownerId != State.playerEmpire) continue;
            const auto* Group = botf::fleetTaskForce(Draft, F.id);
            if (!Group) Units.push_back({F.id, 0});
            else if (!Group->fleetIds.empty() && Group->fleetIds.front() == F.id) Units.push_back({F.id, Group->id});
        }
        if (Units.empty()) { SelectedFleet = 0; SelectedTaskForce = 0; return; }
        auto It = std::find_if(Units.begin(), Units.end(), [this](const auto& U) { return U.first > SelectedFleet; });
        const auto Pick = It != Units.end() ? *It : Units.front();
        SelectedFleet = Pick.first; SelectedTaskForce = Pick.second;
        if (const auto* Ship = Fleet()) SelectCell(Ship->y * State.width + Ship->x);
    }
    const botf::TaskForce* CurrentTaskForce() const {
        return SelectedTaskForce ? botf::taskForceAt(Preview(), SelectedTaskForce) : nullptr;
    }
    // Ships of the player in the selected sector (draft positions are unchanged until turn end).
    std::vector<const botf::Fleet*> OwnShipsHere() const {
        std::vector<const botf::Fleet*> Out;
        for (const auto& F : Preview().fleets)
            if (F.ownerId == State.playerEmpire && F.x == SelectedCell % State.width && F.y == SelectedCell / State.width) Out.push_back(&F);
        return Out;
    }
    // Every mark must still be an owned, ungrouped ship in the selected sector in the current draft.
    bool ValidFormMarks() const {
        if (FormSelection.size() < 2 || Session.active) return false;
        const auto& Draft = Preview();
        for (const int Id : FormSelection) {
            const auto It = std::find_if(Draft.fleets.begin(), Draft.fleets.end(), [Id](const botf::Fleet& F) { return F.id == Id; });
            if (It == Draft.fleets.end() || It->ownerId != State.playerEmpire || botf::fleetTaskForce(Draft, Id)
                || It->x != SelectedCell % State.width || It->y != SelectedCell / State.width) return false;
        }
        return true;
    }
    void PruneFormMarks() {
        const auto& Draft = Preview();
        for (auto It = FormSelection.begin(); It != FormSelection.end();) {
            const int Id = *It;
            const auto F = std::find_if(Draft.fleets.begin(), Draft.fleets.end(), [Id](const botf::Fleet& X) { return X.id == Id; });
            const bool Keep = F != Draft.fleets.end() && F->ownerId == State.playerEmpire && !botf::fleetTaskForce(Draft, Id)
                && F->x == SelectedCell % State.width && F->y == SelectedCell / State.width;
            It = Keep ? std::next(It) : FormSelection.erase(It);
        }
    }
    void FormTaskForce() {
        std::vector<int> Ids(FormSelection.begin(), FormSelection.end());
        if (!ValidFormMarks()) throw std::runtime_error("Mark at least two of your ungrouped ships in this sector to form a task force");
        Submit(botf::makeCreateTaskForceCommand(NextId(), Ids));
        FormSelection.clear();
        const auto* Group = botf::fleetTaskForce(Preview(), Ids.front());
        SelectedFleet = Ids.front(); SelectedTaskForce = Group ? Group->id : 0;
        Status = TEXT("Task force formed. It moves and fights as one group.");
    }
    void DisbandTaskForce() {
        const auto* Group = CurrentTaskForce();
        if (!Group) throw std::runtime_error("Select a task force first");
        Submit(botf::makeDisbandTaskForceCommand(NextId(), Group->id));
        SelectedTaskForce = 0;
        Status = TEXT("Task force split. Its ships now take individual orders.");
    }
    void TaskForceOrder(int X, int Y, botf::TacticalStance Stance, int TargetFleet = 0) {
        const auto* Group = CurrentTaskForce();
        if (!Group) throw std::runtime_error("Select a task force first");
        const int GroupId = Group->id;
        // A new group order replaces the group's earlier drafted order rather than stacking a conflicting one.
        ReplaceOrders([GroupId](const botf::Command& O) { return O.type == botf::CommandType::SetTaskForceOrder && O.taskForceId == GroupId; },
            botf::makeTaskForceOrderCommand(NextId(), GroupId, X, Y, Stance, TargetFleet));
    }
    // Atomically cancels matching drafted orders and optionally submits a replacement, through the real
    // cancel/submit contract on a copy; the live draft changes only if every step is accepted.
    void ReplaceOrders(const std::function<bool(const botf::Command&)>& Match, std::optional<botf::Command> Replacement) {
        Campaign.ReplaceOrders(Match, std::move(Replacement));
    }
    const botf::Fleet* DraftFleet() const {
        for (const auto& F : Preview().fleets) if (F.id == SelectedFleet && F.ownerId == State.playerEmpire) return &F;
        return nullptr;
    }
    bool FleetHasDraft(int FleetId) const {
        return std::any_of(State.pending.begin(), State.pending.end(), [FleetId](const botf::Command& O) { return O.fleetId == FleetId; });
    }
    // Orders drafted this turn for the selected unit (ship, or the selected task force and its create order).
    std::function<bool(const botf::Command&)> SelectedUnitOrders() const {
        const int FleetId = SelectedFleet, GroupId = SelectedTaskForce;
        return [FleetId, GroupId](const botf::Command& O) {
            if (GroupId) return O.taskForceId == GroupId && O.type == botf::CommandType::SetTaskForceOrder;
            return O.fleetId == FleetId && O.type != botf::CommandType::CreateTaskForce;
        };
    }
    bool SelectedUnitHasDraft() const {
        const auto Match = SelectedUnitOrders();
        return (SelectedFleet || SelectedTaskForce) && std::any_of(State.pending.begin(), State.pending.end(), Match);
    }
    void HoldSelected() {
        const auto* Draft = DraftFleet();
        if (!Draft) throw std::runtime_error("Select one of your ships first");
        const int Here = Draft->y * State.width + Draft->x;
        if (CurrentTaskForce()) { PlotCourse(Here); return; }
        const int FleetId = SelectedFleet;
        const auto IsMove = [FleetId](const botf::Command& O) { return O.fleetId == FleetId && O.type == botf::CommandType::MoveFleet; };
        const auto* Base = Fleet();
        if (Base && Base->targetX == Base->x && Base->targetY == Base->y) ReplaceOrders(IsMove, std::nullopt);   // drafted move only
        else {
            botf::Command Stop; Stop.type = botf::CommandType::MoveFleet; Stop.fleetId = FleetId; Stop.targetX = Draft->x; Stop.targetY = Draft->y;
            ReplaceOrders(IsMove, Stop);
        }
        Status = TEXT("Ship will hold at its current sector.");
    }
    void MoveSelection(int DX, int DY) {
        const int X = FMath::Clamp(SelectedCell % State.width + DX, 0, State.width - 1);
        const int Y = FMath::Clamp(SelectedCell / State.width + DY, 0, State.height - 1);
        SelectCell(Y * State.width + X);
    }
    void PlotCourse(int Cell) {
        MapView.bPlotting = false;
        if (!Fleet()) throw std::runtime_error("Select one of your ships first");
        if (const auto* Group = CurrentTaskForce()) {
            TaskForceOrder(Cell % State.width, Cell / State.width, Group->stance);
            Status = FString::Printf(TEXT("Course set for %s to sector %d,%d."), *Str(Group->name), Cell % State.width, Cell / State.width);
            return;
        }
        botf::Command Cmd; Cmd.id = State.lastAcceptedCommand + 1; Cmd.type = botf::CommandType::MoveFleet;
        Cmd.fleetId = SelectedFleet; Cmd.targetX = Cell % State.width; Cmd.targetY = Cell / State.width;
        const int FleetId = SelectedFleet;
        ReplaceOrders([FleetId](const botf::Command& O) { return O.fleetId == FleetId && O.type == botf::CommandType::MoveFleet; }, Cmd);
        Status = FString::Printf(TEXT("Course set for %s to sector %d,%d. Ships move when you end the turn."),
            *Str(botf::fleetName(*Fleet())), Cmd.targetX, Cmd.targetY);
    }

    // ================= COMMAND DECK HUD =================
    // Translucent panel over the chart.
    TSharedRef<SWidget> Glass(const TSharedRef<SWidget>& Content, FMargin Pad = FMargin(10, 8)) const {
        return SNew(SBorder).BorderImage_Lambda([this] { return BotfUi::Styles().Get(ThemeEmpire(), EShape::Row); })
            .BorderBackgroundColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Void, 1.f, 0.9f)); }).Padding(Pad) [ Content ];
    }
    TSharedRef<SWidget> SheetButton(const FString& Label, int Panel, const FString& Key) {
        return Control(Label, [this, Panel] { GoPanel(ActivePanel == Panel ? PanelGalaxy : Panel); }, nullptr, [this, Panel] { return ActivePanel == Panel; },
            ERole::Secondary, FMargin(10, 7), false, 9, Label + TEXT("  (") + Key + TEXT(")"));
    }
    TSharedRef<SWidget> Chip(const FString& Caption, std::function<FString()> Value, const TAttribute<FSlateColor>& Color, const FString& Tip,
                             std::function<FString()> LiveTip = nullptr) const {
        TSharedRef<SWidget> Box = SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(Txt(Caption)).Font(BotfUi::Condensed(7)).ColorAndOpacity(C(&FPalette::Muted)) ]
            + SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(Dyn(Value)).Font(BotfUi::Condensed(13)).ColorAndOpacity(Color) ];
        if (LiveTip) Box->SetToolTipText(Dyn(LiveTip)); else Box->SetToolTipText(Txt(Tip));
        return Box;
    }
    TSharedRef<SWidget> BuildTopBar() {
        auto Chips = SNew(SHorizontalBox);
        const auto AddChip = [&Chips](const TSharedRef<SWidget>& W) { Chips->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 18, 0) [ W ]; };
        AddChip(Chip(TEXT("TREASURY"), [this] { return FString::Printf(TEXT("%lld"), static_cast<long long>(State.credits)); }, C(&FPalette::Primary),
            TEXT("Credits in the treasury")));
        // Net forecast, as the original's income readout: negative (red, with a minus sign) when ship support
        // outweighs revenue. The breakdown is in the tooltip.
        AddChip(Chip(TEXT("PER TURN"), [this] { return FString::Printf(TEXT("%+d"), IncomeForecast().Net()); },
            TAttribute<FSlateColor>::CreateLambda([this] { return FSlateColor(IncomeForecast().Net() < 0 ? P().Alert : P().Good); }),
            FString(), [this] { return IncomeTip(); }));
        AddChip(Chip(TEXT("RESEARCH"), [this] { return FString::Printf(TEXT("%lld"), static_cast<long long>(State.research)); }, C(&FPalette::Secondary),
            TEXT("Research points accumulated")));
        AddChip(Chip(TEXT("SYSTEMS"), [this] { return FString::FromInt(OwnColonyCount()); }, C(&FPalette::Text), TEXT("Systems you control")));
        AddChip(Chip(TEXT("SHIPS"), [this] { return FString::FromInt(OwnShipCount()); }, C(&FPalette::Text), TEXT("Ships in service")));
        AddChip(Chip(TEXT("ORDERS"), [this] { return FString::FromInt(static_cast<int>(State.pending.size())); }, C(&FPalette::Tertiary),
            TEXT("Orders drafted this turn. They are carried out when you end the turn and can be cancelled until then.")));
        return SNew(SBox).HeightOverride(64)
            [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Void, 1.f, 0.92f)); }).Padding(FMargin(0, 0, 10, 0))
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth()
                    [ SNew(SBox).WidthOverride(210)
                        [ Fill(EShape::ElbowBottom, &FPalette::Primary, SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                            [ SNew(STextBlock).Text(Dyn([this] { return FString::Printf(TEXT("TURN %d"), State.turn); })).Font(BotfUi::Condensed(20)).ColorAndOpacity(C(&FPalette::Ink)) ]
                            + SVerticalBox::Slot().AutoHeight()
                            [ SNew(STextBlock).Text(Dyn([this] { return Str(botf::empireName(State.playerEmpire)).ToUpper(); })).Font(BotfUi::Condensed(9)).ColorAndOpacity(C(&FPalette::Ink)) ],
                            FMargin(16, 6, 10, 6)) ] ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16, 0, 0, 0) [ Chips ]
                    + SHorizontalBox::Slot().FillWidth(1) [ SNullWidget::NullWidget ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ Line({ SheetButton(TEXT("SYSTEM"), PanelSystem, TEXT("F2")), SheetButton(TEXT("RESEARCH"), PanelResearch, TEXT("F3")),
                             SheetButton(TEXT("DIPLOMACY"), PanelDiplomacy, TEXT("F4")), SheetButton(TEXT("INTELLIGENCE"), PanelIntel, TEXT("F5")),
                             SheetButton(TEXT("EMPIRE"), PanelEmpire, TEXT("F6")), SheetButton(TEXT("BATTLES"), PanelCombat, TEXT("F7")) }, 4) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12, 0, 0, 0)
                    [ Line({ DynControl(Dyn([this] { return FString::Printf(TEXT("EVENTS %d"), static_cast<int>(botf::view::eventCards(State).size())); }),
                                [this] { bEventsOpen = !bEventsOpen; bSummaryOpen = bMenuOpen = false; }, nullptr, [this] { return bEventsOpen; },
                                ERole::Tertiary, FMargin(10, 7), false, 9, TEXT("Galactic events and the decisions they need")),
                             Control(TEXT("SUMMARY"), [this] { bSummaryOpen = !bSummaryOpen; bMenuOpen = false; bEventsOpen = false; }, nullptr, [this] { return bSummaryOpen; },
                                ERole::Tertiary, FMargin(10, 7), false, 9, TEXT("Events, relationships and systems (S)")),
                             Control(TEXT("MENU"), [this] { ShowView(EUiView::Menu); }, nullptr, nullptr, ERole::Tertiary, FMargin(10, 7), false, 9,
                                TEXT("Save, load, settings, quit (Esc)")) }, 4) ] ] ];
    }
    // Left dock: whatever is selected. Sector and system first, then units, then the selected unit's card.
    TSharedRef<SWidget> BuildLeftDock() {
        return SNew(SBox).WidthOverride(330)
            .Visibility_Lambda([this] { return ActivePanel == PanelGalaxy ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
            [ SNew(SScrollBox) + SScrollBox::Slot()
                [ Stack({
                    Glass(SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight() [ Heading(Dyn([this] { return SystemTitle(); }), 16, &FPalette::Accent) ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                        [ SNew(STextBlock).Text(Dyn([this] { return SectorLabel(SelectedCell); })).Font(BotfUi::Condensed(8)).ColorAndOpacity(C(&FPalette::Tertiary)).AutoWrapText(true) ]
                        + SVerticalBox::Slot().AutoHeight() [ Para(Dyn([this] { return SectorBrief(); }), 9, &FPalette::Text) ]
                        // Charted anomalies in the sector and, for a contacted minor race, its profile.
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                        [ SNew(SBox).Visibility_Lambda([this] { return SafeText([this] { return SectorExtras(); }).IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
                            [ Para(Dyn([this] { return SectorExtras(); }), 9, &FPalette::Muted) ] ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0) [ SAssignNew(PlanetRows, SVerticalBox) ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
                        [ Line({
                            Pill(TEXT("OPEN SYSTEM"), [this] { GoPanel(PanelSystem); }, [this] { return Owned(); }, ERole::Primary, TEXT("Solar System sheet (double-click the system)")),
                            Pill(TEXT("DIPLOMACY"), [this] { OpenDiplomacyFor(Selected); }, [this] { return DiplomacyTargetAt(Selected) != 0; }, ERole::Secondary,
                                TEXT("Open the dossier of the race that holds this system")),
                            Pill(TEXT("CENTRE"), [this] { if (Chart.IsValid()) Chart->CentreOn(SelectedCell); }, nullptr, ERole::Tertiary, TEXT("Centre the chart here (C)")) }, 4) ]),
                    // Economic map: trade routes are assigned here, from the chart (Tutorial p13), as well as in Diplomacy.
                    SNew(SBox).Visibility_Lambda([this] { return MapView.Mode == BotfUi::EMapMode::Economic ? EVisibility::Visible : EVisibility::Collapsed; })
                        [ Glass(BuildTradeRoutePanel()) ],
                    SNew(SBox).Visibility_Lambda([this] { return SafeBool([this] { return SelectedStation() != nullptr; }, false) ? EVisibility::Visible : EVisibility::Collapsed; })
                        [ Glass(BuildStationInspector()) ],
                    Glass(SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4) [ Heading(Txt(TEXT("UNITS IN SECTOR")), 10, &FPalette::Tertiary) ]
                        + SVerticalBox::Slot().AutoHeight() [ SAssignNew(SectorRows, SVerticalBox) ]),
                    SNew(SBox).Visibility_Lambda([this] { return Fleet() ? EVisibility::Visible : EVisibility::Collapsed; })
                        [ Glass(BuildUnitCard()) ] }, 6) ] ];
    }
    // Turn inbox: everything that needs a decision, each entry jumps to where it is dealt with.
    TSharedRef<SWidget> BuildInbox() {
        return SNew(SBox).WidthOverride(300)
            .Visibility_Lambda([this] { return ActivePanel == PanelGalaxy ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
            [ SNew(SVerticalBox).Visibility(EVisibility::SelfHitTestInvisible)
                + SVerticalBox::Slot().AutoHeight()
                [ Glass(SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                    [ Heading(Dyn([this] { return FString::Printf(TEXT("TURN INBOX  /  %d"), InboxCount); }), 11, &FPalette::Accent) ]
                    + SVerticalBox::Slot().AutoHeight() [ SNew(SBox).MaxDesiredHeight(460) [ Scroll(SAssignNew(InboxRows, SVerticalBox)) ] ]) ]
                + SVerticalBox::Slot().FillHeight(1) [ SNullWidget::NullWidget ] ];
    }
    TSharedRef<SWidget> MapToggle(const FString& Label, bool BotfUi::FMapView::* Flag, const FString& Key, const FString& Tip) {
        return Control(Label, [this, Flag] { MapView.*Flag = !(MapView.*Flag); }, nullptr, [this, Flag] { return MapView.*Flag; },
            ERole::Tertiary, FMargin(8, 6), false, 8, Key.IsEmpty() ? Tip : Tip + TEXT(" (") + Key + TEXT(")"));
    }
    FString MapHelp() const {
        if (MapView.bPlotting) return TEXT("PLOT COURSE: left-click a destination. Right-click or Esc cancels.");
        if (MapView.Mode == BotfUi::EMapMode::Economic)
            return TEXT("Trade: select your system, Set as Source, then a rival colony, Route to Here  |  Right-click: route options  |  M: military map");
        if (Fleet()) return TEXT("Right-click: order unit there  |  Shift+right-click: all options  |  Click: select  |  Double-click: open  |  Tab: next unit");
        return TEXT("Click: select  |  Double-click: open system or dossier  |  Right-click: options  |  Drag: pan  |  Wheel: zoom");
    }
    TSharedRef<SWidget> BuildBottomBar() {
        return SNew(SBox).HeightOverride(56)
            [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Void, 1.f, 0.92f)); }).Padding(FMargin(10, 6))
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ Line({
                        Option(TEXT("MILITARY"), [this] { MapView.Mode = BotfUi::EMapMode::Military; }, [this] { return MapView.Mode == BotfUi::EMapMode::Military; },
                            ERole::Secondary, 0, TEXT("Ships, ranges and courses (M)")),
                        Option(TEXT("ECONOMIC"), [this] { MapView.Mode = BotfUi::EMapMode::Economic; }, [this] { return MapView.Mode == BotfUi::EMapMode::Economic; },
                            ERole::Secondary, 0, TEXT("Trade routes and colony output; ships hidden (M)")) }, 4) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0, 0, 0)
                    [ Line({ MapToggle(TEXT("NAMES"), &BotfUi::FMapView::bNames, TEXT("N"), TEXT("System names")),
                             MapToggle(TEXT("RANGE"), &BotfUi::FMapView::bRange, TEXT("R"), TEXT("Supply range of the selected ship")),
                             MapToggle(TEXT("COURSES"), &BotfUi::FMapView::bCourses, FString(),TEXT("Courses of all your units")),
                             MapToggle(TEXT("GRID"), &BotfUi::FMapView::bGrid, TEXT("G"), TEXT("Sector grid and coordinates")),
                             MapToggle(TEXT("ANOMALIES"), &BotfUi::FMapView::bAnomalies, FString(), TEXT("Charted nebulae, black holes, neutron stars, ion storms and wormholes; hover a sector for details")),
                             MapToggle(TEXT("STATIONS"), &BotfUi::FMapView::bStations, FString(), TEXT("Supply (bright) and sensor (faint) reach of your outposts and starbases")) }, 3) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0, 0, 0)
                    [ Line({ Pill(TEXT("-"), [this] { if (Chart.IsValid()) Chart->ZoomBy(1.f / 1.25f, ChartMiddle()); }, nullptr, ERole::Tertiary, TEXT("Zoom out (-, wheel)")),
                             Pill(TEXT("+"), [this] { if (Chart.IsValid()) Chart->ZoomBy(1.25f, ChartMiddle()); }, nullptr, ERole::Tertiary, TEXT("Zoom in (+, wheel)")),
                             Pill(TEXT("FIT"), [this] { if (Chart.IsValid()) Chart->ResetView(); }, nullptr, ERole::Tertiary, TEXT("Whole galaxy (Home)")) }, 3) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0, 0, 0)
                    [ SNew(SBox).Visibility_Lambda([this] { return (ActivePanel == PanelGalaxy && PreviousPanel != PanelGalaxy) ? EVisibility::Visible : EVisibility::Collapsed; })
                        [ Pill(TEXT("BACK"), [this] { GoBack(); }, nullptr, ERole::Secondary, TEXT("Return to previous screen (Backspace)")) ] ]
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(14, 0)
                    [ SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [ SNew(STextBlock).Text(Dyn([this] { return Status; })).Font(BotfUi::Font(9)).ColorAndOpacity_Lambda([this] {
                            return FSlateColor(Status.StartsWith(TEXT("Order rejected")) ? P().Alert : P().Text); }) ]
                        + SVerticalBox::Slot().AutoHeight()
                        [ SNew(STextBlock).Text(Dyn([this] { return MapHelp(); })).Font(BotfUi::Font(8)).ColorAndOpacity(C(&FPalette::Muted)) ] ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ SNew(SBox).WidthOverride(190)
                        [ Control(TEXT("END TURN"), [this] { RequestEndTurn(); }, [this] { return State.outcome == botf::CampaignOutcome::Ongoing; },
                            nullptr, ERole::Accent, FMargin(14, 10), false, 15, TEXT("Carry out all orders (Ctrl+Enter)")) ] ] ] ];
    }
    FVector2D ChartMiddle() const { return Chart.IsValid() ? Chart->ViewSize() * 0.5 : FVector2D::ZeroVector; }

    // ---- Sheets: the original's separate screens, opened over the chart ----
    TSharedRef<SWidget> SheetHeader(std::initializer_list<TSharedRef<SWidget>> Tabs) {
        auto Box = SNew(SHorizontalBox);
        for (const auto& Tab : Tabs) Box->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0) [ Tab ];
        Box->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
            [ Control(TEXT("BACK"), [this] { GoBack(); },
                [this] { return !(ActivePanel == PanelCombat && Session.pending) && (ActivePanel != PanelGalaxy || PreviousPanel != PanelGalaxy); },
                nullptr, ERole::Secondary, FMargin(10, 6), false, 9, TEXT("Return to previous screen (Backspace)")) ];
        Box->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
            [ Control(TEXT("CLOSE"), [this] { GoPanel(PanelGalaxy); }, [this] { return !(ActivePanel == PanelCombat && Session.pending); }, nullptr,
                ERole::Alert, FMargin(10, 6), false, 9, TEXT("Back to the chart (Esc, F1)")) ];
        return Box;
    }
    TSharedRef<SWidget> BuildSheetLayer() {
        return SNew(SBox).Visibility_Lambda([this] { return ActivePanel != PanelGalaxy ? EVisibility::Visible : EVisibility::Collapsed; })
            [ SNew(SOverlay)
                // Dimmed chart behind the sheet; clicking it returns to the chart.
                + SOverlay::Slot()
                [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f))
                    .OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent&) {
                        if (!(ActivePanel == PanelCombat && Session.pending)) GoPanel(PanelGalaxy);
                        return FReply::Handled();
                    }) ]
                + SOverlay::Slot().Padding(TAttribute<FMargin>::CreateLambda([this] { return (ActivePanel == PanelCombat && Session.pending) ? FMargin(0) : FMargin(28, 70, 28, 62); }))
                [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor_Lambda([this] { return FSlateColor(P().Void); })
                    .Padding(TAttribute<FMargin>::CreateLambda([this] { return (ActivePanel == PanelCombat && Session.pending) ? FMargin(0) : FMargin(16, 12); }))
                    .OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
                    [ SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return FMath::Clamp(ActivePanel - 1, 0, 5); })
                        + SWidgetSwitcher::Slot() [ BuildSystemScreen() ]
                        + SWidgetSwitcher::Slot() [ BuildResearchScreen() ]
                        + SWidgetSwitcher::Slot() [ BuildDiplomacyScreen() ]
                        + SWidgetSwitcher::Slot() [ BuildIntelligenceScreen() ]
                        + SWidgetSwitcher::Slot() [ BuildEmpireScreen() ]
                        + SWidgetSwitcher::Slot() [ BuildCombatScreen() ] ] ] ];
    }

    // ---- Sector context menu (right click / Shift+right click / ~) ----
    TSharedRef<SWidget> BuildSectorMenu() {
        return SNew(SBox).WidthOverride(270).Visibility_Lambda([this] { return bMenuOpen && Shell == EUiView::Game ? EVisibility::Visible : EVisibility::Collapsed; })
            [ SNew(SBorder).BorderImage_Lambda([this] { return BotfUi::Styles().Get(ThemeEmpire(), EShape::Row); })
                .BorderBackgroundColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Secondary, 0.6f)); }).Padding(2)
                [ SNew(SBorder).BorderImage_Lambda([this] { return BotfUi::Styles().Get(ThemeEmpire(), EShape::Row); })
                    .BorderBackgroundColor_Lambda([this] { return FSlateColor(P().Void); }).Padding(FMargin(8, 6))
                    [ SAssignNew(MenuRows, SVerticalBox) ] ] ];
    }
    struct FMenuItem { FString Label, Tip; std::function<void()> Action; bool bEnabled = true; ERole Role = ERole::Primary; };
    std::vector<FMenuItem> SectorActions(int CellId) {
        std::vector<FMenuItem> Items;
        const int X = CellId % State.width, Y = CellId / State.width;
        const int SystemId = SystemAtCell(CellId);
        const auto* Unit = Fleet();
        if (Unit) {
            const FString Name = UnitName();
            const bool Here = Unit->x == X && Unit->y == Y;
            if (!Here) {
                const auto Path = UnitRoute(X, Y);
                Items.push_back({ Path.empty() ? Name + TEXT(": out of range") : FString::Printf(TEXT("Set course here  /  %d turn%s"), static_cast<int>(Path.size()) - 1,
                        Path.size() == 2 ? TEXT("") : TEXT("s")),
                    TEXT("Ships move when you end the turn"), [this, CellId] { PlotCourse(CellId); }, !Path.empty() && !Session.active });
            }
            if (const auto* Enemy = HostileFleetAt(CellId))
                Items.push_back({ FString::Printf(TEXT("Attack %s #%d"), *Str(botf::fleetName(*Enemy)), Enemy->id), TEXT("The battle is fought when you end the turn"),
                    [this, CellId] { AttackAt(CellId); }, CanAttackAt(CellId), ERole::Alert });
            if (Here) {
                if (CanSettle()) Items.push_back({ TEXT("Colonize this system"), TEXT("The colony ship is used up"), [this] { OrderColonize(); }, true, ERole::Accent });
                if (CanInvadeHere()) Items.push_back({ TEXT("Invade this system"), TEXT("Ground assault at turn end"), [this] { OrderInvade(); }, true, ERole::Alert });
                if (Unit->shipClass == botf::Building::Transport && State.rules.stations && !CurrentTaskForce())
                    Items.push_back({ TEXT("Build outpost here"), OutpostText(), [this] { OrderBuildOutpost(); }, CanBuildOutpost(), ERole::Accent });
                if (DraftFleetOr(*Unit)->targetX != Unit->x || DraftFleetOr(*Unit)->targetY != Unit->y)
                    Items.push_back({ TEXT("Hold position"), TEXT("Cancel the course"), [this] { HoldSelected(); }, !Session.active, ERole::Secondary });
            }
            Items.push_back({ TEXT("Deselect ") + Name, FString(), [this] { SelectedFleet = 0; SelectedTaskForce = 0; }, true, ERole::Tertiary });
        }
        // Own units in this sector: select directly.
        int Listed = 0;
        const auto& Draft = Preview();
        std::set<int> Groups;
        for (const auto& F : Draft.fleets) {
            if (F.ownerId != State.playerEmpire || F.x != X || F.y != Y || Listed >= 6) continue;
            const auto* Group = botf::fleetTaskForce(Draft, F.id);
            if (Group && !Groups.insert(Group->id).second) continue;
            if ((Group && SelectedTaskForce == Group->id) || (!Group && !SelectedTaskForce && SelectedFleet == F.id)) continue;
            ++Listed;
            const int GroupId = Group ? Group->id : 0, Lead = Group && !Group->fleetIds.empty() ? Group->fleetIds.front() : F.id;
            Items.push_back({ TEXT("Select ") + (Group ? Str(Group->name) + FString::Printf(TEXT(" (%d)"), static_cast<int>(Group->fleetIds.size())) : Str(botf::fleetName(F)) + FString::Printf(TEXT(" #%d"), F.id)),
                FString(), [this, CellId, GroupId, Lead] { SelectCell(CellId); SelectedFleet = Lead; SelectedTaskForce = GroupId; }, true, ERole::Secondary });
        }
        if (SystemId) {
            const auto* Col = botf::colonyAt(State, SystemId);
            if (Col && Col->ownerId == State.playerEmpire)
                Items.push_back({ TEXT("Open system"), TEXT("Solar System sheet"), [this, SystemId] { SelectSystem(SystemId); GoPanel(PanelSystem); }, true });
            if (MapView.Mode == BotfUi::EMapMode::Economic) {
                if (Col && Col->ownerId == State.playerEmpire && TradeSource != SystemId)
                    Items.push_back({ TEXT("Set as trade route source"), FString(), [this, SystemId] { TradeSource = SystemId; }, true, ERole::Secondary });
                if (RouteTargetOwner(SystemId))
                    Items.push_back({ TradeSource ? TEXT("Trade route from ") + SystemName(TradeSource) + TEXT(" to here") : FString(TEXT("Trade route to here")),
                        RouteBlocker(SystemId), [this, SystemId] { EstablishRouteTo(SystemId); }, RouteBlocker(SystemId).IsEmpty() });
            }
            if (DiplomacyTargetAt(SystemId))
                Items.push_back({ TEXT("Diplomacy: ") + DiplomacyTargetName(SystemId), FString(), [this, SystemId] { OpenDiplomacyFor(SystemId); }, true, ERole::Secondary });
            if (Col && Col->ownerId != State.playerEmpire && botf::visible(State, State.playerEmpire, X, Y))
                Items.push_back({ TEXT("Intelligence on ") + Str(botf::empireName(Col->ownerId)), FString(),
                    [this, Owner = Col->ownerId, SystemId] { SelectedEmpire = Owner; IntelTargetSystem = SystemId; GoPanel(PanelIntel); }, true, ERole::Secondary });
        }
        if (const auto* St = botf::view::visibleStationAt(State, X, Y))
            Items.push_back({ TEXT("Inspect ") + Str(botf::stationName(*St)), TEXT("Station details and, for your outposts, the starbase upgrade"),
                [this, CellId] { SelectCell(CellId); }, true, ERole::Secondary });
        Items.push_back({ TEXT("Centre view here"), FString(), [this, CellId] { if (Chart.IsValid()) Chart->CentreOn(CellId); }, true, ERole::Tertiary });
        return Items;
    }
    void OpenSectorMenu(int CellId, FVector2D Local) {
        if (CellId < 0) return;
        const auto Items = SectorActions(CellId);
        MenuCell = CellId; bMenuOpen = true; bSummaryOpen = false;
        const FVector2D Size = Chart.IsValid() ? Chart->ViewSize() : FVector2D(1280, 720);
        if (Local.IsZero()) Local = Size * 0.5;
        const double Height = 40.0 + 30.0 * static_cast<double>(Items.size() + 1);
        MenuAt = FVector2D(FMath::Clamp(Local.X + 6, 4.0, FMath::Max(4.0, Size.X - 280)), FMath::Clamp(Local.Y + 6, 70.0, FMath::Max(70.0, Size.Y - Height - 60)));
        if (!MenuRows.IsValid()) return;
        MenuRows->ClearChildren();
        MenuRows->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
            [ SNew(STextBlock).Text(Txt(SectorLabel(CellId))).Font(BotfUi::Condensed(9)).ColorAndOpacity(C(&FPalette::Tertiary)).AutoWrapText(true) ];
        for (const auto& Item : Items)
            MenuRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
                [ Control(Item.Label, [this, Action = Item.Action] { bMenuOpen = false; Action(); }, [Enabled = Item.bEnabled] { return Enabled; },
                    nullptr, Item.Role, FMargin(10, 6), false, 9, Item.Tip) ];
        MenuRows->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
            [ Line({ Pill(TEXT("RESEARCH"), [this] { bMenuOpen = false; GoPanel(PanelResearch); }, nullptr, ERole::Tertiary),
                     Pill(TEXT("EMPIRE"), [this] { bMenuOpen = false; GoPanel(PanelEmpire); }, nullptr, ERole::Tertiary),
                     Pill(TEXT("CLOSE"), [this] { bMenuOpen = false; }, nullptr, ERole::Alert) }, 3) ];
    }

    // ---- Chart interaction ----
    int SystemAtCell(int CellId) const {
        for (const auto& S : State.systems)
            if (S.y * State.width + S.x == CellId && botf::explored(State, S.x, S.y)) return S.id;
        return 0;
    }
    const botf::Fleet* DraftFleetOr(const botf::Fleet& Base) const { const auto* D = DraftFleet(); return D ? D : &Base; }
    // Route of the selected unit in the draft. A task force goes only where every member can (core validates group
    // orders member by member), so its range is its shortest-ranged member's, not the lead ship's.
    std::vector<int> UnitRoute(int X, int Y) const {
        const auto& Draft = Preview();
        const int Group = CurrentTaskForce() ? SelectedTaskForce : 0;
        if (Group || botf::view::findFleet(Draft, SelectedFleet)) return botf::view::unitRoute(Draft, SelectedFleet, Group, X, Y);
        return botf::view::unitRoute(State, SelectedFleet, 0, X, Y);
    }
    // Attack reach: a single ship is judged on the committed campaign as before; a task force on its whole membership.
    bool UnitCanReach(const botf::Fleet& Ship, int X, int Y) const {
        if (CurrentTaskForce()) return !UnitRoute(X, Y).empty();
        return !botf::route(State, Ship, X, Y).empty();
    }
    FString UnitName() const {
        if (const auto* Group = CurrentTaskForce()) return Str(Group->name);
        if (const auto* F = Fleet()) return Str(botf::fleetName(*F)) + FString::Printf(TEXT(" #%d"), F->id);
        return FString();
    }
    const botf::Fleet* HostileFleetAt(int CellId) const {
        const int X = CellId % State.width, Y = CellId / State.width;
        if (!botf::visible(State, State.playerEmpire, X, Y)) return nullptr;
        for (const auto& F : State.fleets) if (F.ownerId != State.playerEmpire && F.x == X && F.y == Y) return &F;
        return nullptr;
    }
    bool CanAttackAt(int CellId) const {
        const auto* F = Fleet(); const auto* Enemy = HostileFleetAt(CellId);
        if (!F || !Enemy || Session.active) return false;
        const auto* R = botf::majorRelation(State, State.playerEmpire, Enemy->ownerId);
        return R && R->atWar && UnitCanReach(*F, Enemy->x, Enemy->y);
    }
    void AttackAt(int CellId) {
        const auto* Enemy = HostileFleetAt(CellId);
        if (!Enemy || !CanAttackAt(CellId)) throw std::runtime_error("Attacking needs war with the owner and a target within range");
        if (const auto* Group = CurrentTaskForce()) TaskForceOrder(Enemy->x, Enemy->y, Group->stance, Enemy->id);
        else {
            botf::Command Cmd; Cmd.id = NextId(); Cmd.type = botf::CommandType::AttackFleet; Cmd.fleetId = SelectedFleet; Cmd.targetFleetId = Enemy->id;
            Submit(Cmd);
        }
        Status = FString::Printf(TEXT("%s ordered to attack %s #%d. The battle is fought when you end the turn."), *UnitName(), *Str(botf::fleetName(*Enemy)), Enemy->id);
    }
    bool CanInvadeHere() const {
        const auto* F = Fleet(); if (!F) return false;
        const int SystemId = SystemAtCell(F->y * State.width + F->x);
        const auto* Col = SystemId ? botf::colonyAt(State, SystemId) : nullptr;
        if (!Col || Col->ownerId == State.playerEmpire) return false;
        const auto* R = botf::majorRelation(State, State.playerEmpire, Col->ownerId);
        return F->shipClass == botf::Building::Transport && R && R->atWar;
    }
    void OrderColonize() {
        botf::Command Cmd; Cmd.id = NextId(); Cmd.type = botf::CommandType::Colonize; Cmd.fleetId = SelectedFleet;
        Submit(Cmd); Status = TEXT("Colony ship ordered to settle this system at turn end.");
    }
    void OrderInvade() {
        const auto* F = Fleet(); if (!F) throw std::runtime_error("Select a troop transport first");
        botf::Command Cmd; Cmd.id = NextId(); Cmd.type = botf::CommandType::Invade;
        Cmd.fleetId = SelectedFleet; Cmd.systemId = SystemAtCell(F->y * State.width + F->x);
        Submit(Cmd); Status = TEXT("Ground invasion ordered for turn end.");
    }
    // ---- Outposts (schema 13): a troop transport assembles one in its sector ----
    // Orders that claim a fleet for the turn in core (one per fleet); Build Outpost replaces any of them.
    static bool ClaimsFleet(const botf::Command& O) {
        using botf::CommandType;
        return O.type == CommandType::MoveFleet || O.type == CommandType::Colonize || O.type == CommandType::AttackFleet
            || O.type == CommandType::RetreatFleet || O.type == CommandType::Invade || O.type == CommandType::TerraformPlanet
            || O.type == CommandType::BuildOutpost;
    }
    bool OutpostDrafted(int FleetId) const {
        return std::any_of(State.pending.begin(), State.pending.end(), [FleetId](const botf::Command& O) {
            return O.type == botf::CommandType::BuildOutpost && O.fleetId == FleetId;
        });
    }
    // A single transport (not a task force selection) that core would let assemble an outpost now.
    bool CanBuildOutpost() const {
        const auto* F = Fleet();
        if (!F || Session.active || CurrentTaskForce() || OutpostDrafted(F->id)) return false;
        return botf::outpostBlocker(State, F->id).empty();
    }
    void OrderBuildOutpost() {
        const auto* F = Fleet();
        if (!F || CurrentTaskForce()) throw std::runtime_error("Select a single troop transport first");
        const int FleetId = F->id;
        ReplaceOrders([FleetId](const botf::Command& O) { return O.fleetId == FleetId && ClaimsFleet(O); },
            botf::makeBuildOutpostCommand(NextId(), FleetId));
        Status = FString::Printf(TEXT("Outpost assembly ordered: it takes %d turns and uses up the transport. Any new order to it abandons the work."),
            botf::outpostBuildTurns(State, State.playerEmpire));
    }
    FString OutpostText() const {
        const auto* F = Fleet();
        if (!F || !State.rules.stations || F->shipClass != botf::Building::Transport) return FString();
        if (OutpostDrafted(F->id))
            return FString::Printf(TEXT("Outpost ordered: assembly starts when the turn ends and takes %d turns."), botf::outpostBuildTurns(State, State.playerEmpire));
        return Str(botf::view::outpostOrderText(botf::view::outpostOrder(State, *F)));
    }
    // Race whose dossier a system opens: an empire id (1..5), a minor's system id as negative, or 0.
    int DiplomacyTargetAt(int SystemId) const {
        const auto S = std::find_if(State.systems.begin(), State.systems.end(), [SystemId](const botf::System& X) { return X.id == SystemId; });
        if (!SystemId || S == State.systems.end()) return 0;
        if (const auto M = botf::minorAt(State, SystemId); M && M->contact[State.playerEmpire] && !M->memberOf) return -SystemId;
        const auto* Col = botf::colonyAt(State, SystemId);
        if (Col && Col->ownerId != State.playerEmpire && botf::visible(State, State.playerEmpire, S->x, S->y)) {
            const auto* R = botf::majorRelation(State, State.playerEmpire, Col->ownerId);
            if (R && R->contact) return Col->ownerId;
        }
        return 0;
    }
    FString DiplomacyTargetName(int SystemId) const {
        const int Target = DiplomacyTargetAt(SystemId);
        if (Target < 0) return Str(botf::minorAt(State, -Target)->name);
        return Target ? Str(botf::empireName(Target)) : FString();
    }
    void OpenDiplomacyFor(int SystemId) {
        const int Target = DiplomacyTargetAt(SystemId);
        if (!Target) throw std::runtime_error("No contacted race holds this system");
        if (Target < 0) { SelectedMinor = -Target; } else { SelectedEmpire = Target; SelectedMinor = 0; }
        GoPanel(PanelDiplomacy);
    }
    // Left click: select the sector; clicking the selected sector again cycles the units in it.
    void ClickCell(int CellId) {
        bMenuOpen = false;
        if (CellId == SelectedCell && CycleUnitHere()) return;
        SelectCell(CellId);
        SelectedFleet = 0; SelectedTaskForce = 0;
        CycleUnitHere();
    }
    // Selects the next own unit (task force or ungrouped ship) in the selected sector; false if there is none.
    bool CycleUnitHere() {
        const auto& Draft = Preview();
        std::vector<std::pair<int, int>> Units;
        for (const auto* F : OwnShipsHere()) {
            const auto* Group = botf::fleetTaskForce(Draft, F->id);
            if (!Group) Units.push_back({F->id, 0});
            else if (!Group->fleetIds.empty() && Group->fleetIds.front() == F->id) Units.push_back({F->id, Group->id});
        }
        if (Units.empty()) return false;
        auto It = std::find_if(Units.begin(), Units.end(), [this](const auto& U) { return U.first == SelectedFleet && U.second == SelectedTaskForce; });
        const auto Pick = It == Units.end() || std::next(It) == Units.end() ? Units.front() : *std::next(It);
        SelectedFleet = Pick.first; SelectedTaskForce = Pick.second;
        return true;
    }
    // Double click / Enter: open whatever the sector holds.
    void ActivateCell(int CellId) {
        bMenuOpen = false;
        if (CellId != SelectedCell) SelectCell(CellId);
        const int SystemId = SystemAtCell(CellId);
        if (SystemId && Owned()) { GoPanel(PanelSystem); return; }
        if (SystemId && DiplomacyTargetAt(SystemId)) { OpenDiplomacyFor(SystemId); return; }
        if (Chart.IsValid()) {
            Chart->CentreOn(CellId);
            if (MapView.Zoom < 1.8f) Chart->ZoomBy(1.8f / MapView.Zoom, ChartMiddle());
        }
    }
    // Right click: a unit is selected -> the obvious order; otherwise (or with Shift) the sector's options.
    void CommandCell(int CellId, FVector2D Local, bool bForceMenu) {
        const auto* Unit = Fleet();
        if (bForceMenu || !Unit || MapView.Mode == BotfUi::EMapMode::Economic) {
            if (!Unit) SelectCell(CellId);
            OpenSectorMenu(CellId, Local);
            return;
        }
        bMenuOpen = false;
        if (Unit->x == CellId % State.width && Unit->y == CellId / State.width) { OpenSectorMenu(CellId, Local); return; }
        if (CanAttackAt(CellId)) { AttackAt(CellId); return; }
        const auto Path = UnitRoute(CellId % State.width, CellId / State.width);
        if (Path.empty()) throw std::runtime_error("That sector is outside the unit's supply range");
        PlotCourse(CellId);
    }
    // Hover hint: what a right click would do (first line), then what the sector holds (anomalies, station, race).
    FString CellHint(int CellId) const { return CellOrderHint(CellId) + CellDetailLines(CellId); }
    FString CellOrderHint(int CellId) const {
        const auto* Unit = Fleet();
        const FString Where = SystemAtCell(CellId) ? SystemName(SystemAtCell(CellId)) : FString::Printf(TEXT("%d,%d"), CellId % State.width, CellId / State.width);
        if (!Unit || MapView.Mode == BotfUi::EMapMode::Economic) return botf::explored(State, CellId % State.width, CellId / State.width) ? Where : FString(TEXT("Unexplored"));
        if (Unit->x == CellId % State.width && Unit->y == CellId / State.width) return Where + TEXT("  /  right-click for orders");
        const auto Path = UnitRoute(CellId % State.width, CellId / State.width);
        if (Path.empty()) return Where + TEXT("  /  out of range");
        const int Turns = static_cast<int>(Path.size()) - 1;
        if (CanAttackAt(CellId)) return FString::Printf(TEXT("ATTACK %s  /  %d turn%s"), *Where, Turns, Turns == 1 ? TEXT("") : TEXT("s"));
        return FString::Printf(TEXT("%s  /  %d turn%s"), *Where, Turns, Turns == 1 ? TEXT("") : TEXT("s"));
    }
    BotfUi::SStarChart::FBinding ChartBinding() {
        BotfUi::SStarChart::FBinding B;
        B.State = &State; B.Draft = [this]() -> const botf::GameState& { return Preview(); };
        B.Selected = &Selected; B.Cell = &SelectedCell; B.Fleet = &SelectedFleet; B.TaskForce = &SelectedTaskForce; B.View = &MapView;
        B.Version = [this] { return Campaign.PreviewGeneration(); };
        const auto Guard = [this](std::function<void()> Action) { RunGuarded(Action); };
        B.Select = [this, Guard](int Cell) { Guard([this, Cell] { ClickCell(Cell); }); };
        B.Activate = [this, Guard](int Cell) { Guard([this, Cell] { ActivateCell(Cell); }); };
        B.Plot = [this, Guard](int Cell) { Guard([this, Cell] { PlotCourse(Cell); }); };
        B.Command = [this, Guard](int Cell, FVector2D Local, bool bMenu) { Guard([this, Cell, Local, bMenu] { CommandCell(Cell, Local, bMenu); }); };
        B.Hint = [this](int Cell) { return SafeText([this, Cell] { return CellHint(Cell); }); };
        return B;
    }

    // ---- Turn inbox ----
    void InboxRow(const FString& Label, std::function<void()> Action, FLinearColor FPalette::* Tone, const FString& Tip = FString()) {
        ++InboxCount;
        InboxRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0) [ SNew(SBox).WidthOverride(4) [ Fill(EShape::Block, Tone) ] ]
                + SHorizontalBox::Slot().FillWidth(1)
                [ Row(Label, Action ? Action : std::function<void()>([] {}), nullptr, [HasAction = static_cast<bool>(Action)] { return HasAction; }, Tip) ] ];
    }
    void RefreshInbox() {
        if (!InboxRows.IsValid()) return;
        InboxRows->ClearChildren(); InboxCount = 0;
        if (State.outcome != botf::CampaignOutcome::Ongoing) InboxRow(OutcomeText(), [this] { bSummaryOpen = true; }, &FPalette::Accent);
        if (Session.pending) InboxRow(TEXT("Battle awaits your command"), [this] { GoPanel(PanelCombat); }, &FPalette::Alert);
        for (const auto& Offer : State.pendingMajorOffers)
            if (Offer.target == State.playerEmpire)
                InboxRow(FString::Printf(TEXT("%s propose %s  (due turn %d)"), *Str(botf::empireName(Offer.proposer)),
                    Offer.type == botf::CommandType::OfferPeace ? TEXT("peace") : TEXT("a trade treaty"), Offer.dueTurn),
                    [this, Who = Offer.proposer] { SelectedEmpire = Who; SelectedMinor = 0; DiplomacyMode = 0; GoPanel(PanelDiplomacy); }, &FPalette::Accent);
        // AI-initiated proposals (modern diplomacy) with their response window.
        for (const auto& Proposal : botf::incomingProposals(State, State.playerEmpire)) {
            if (ProposalResponsePending(Proposal.id)) continue;
            const int Who = Proposal.proposer.isMinor ? 0 : Proposal.proposer.id;
            InboxRow(Str(botf::view::proposalSummary(State, Proposal, State.playerEmpire)) + TEXT("  (") + Str(botf::view::deadlineText(State, Proposal.dueTurn)) + TEXT(")"),
                [this, Who] { if (Who) { SelectedEmpire = Who; SelectedMinor = 0; } DiplomacyMode = 0; GoPanel(PanelDiplomacy); }, &FPalette::Accent,
                TEXT("Accept or reject in Diplomacy, Events. Unanswered proposals are ignored when the window closes."));
        }
        for (const auto& Call : OpenCallsToArms())
            InboxRow(FString::Printf(TEXT("Call to arms: %s asks you to fight %s (by turn %d)"), *Str(botf::empireName(Call.defender)),
                    *Str(botf::empireName(Call.aggressor)), Call.dueTurn),
                [this, Who = Call.defender] { SelectedEmpire = Who; SelectedMinor = 0; DiplomacyMode = 0; GoPanel(PanelDiplomacy); }, &FPalette::Alert,
                TEXT("Join by declaring war, or let the call lapse"));
        // Galactic events awaiting the player's decision open the event card.
        for (const auto& Card : botf::view::eventCards(State)) {
            if (!Card.needsDecision) continue;
            InboxRow(FString::Printf(TEXT("Event: %s%s  /  %s"), *Str(Card.title), Card.where.empty() ? TEXT("") : *(TEXT(" at ") + Str(Card.where)),
                    Card.chosenThisTurn ? *(TEXT("ordered: ") + Str(botf::view::optionLabel(Card, Card.chosen))) : *Str(botf::view::eventDeadlineText(Card))),
                [this] { bEventsOpen = true; bSummaryOpen = bMenuOpen = false; }, Card.chosenThisTurn ? &FPalette::Tertiary : &FPalette::Accent,
                TEXT("Choose how your empire responds; the default applies once the deadline passes"));
        }
        const auto& Draft = Preview();
        for (const auto& Col : Draft.colonies) {
            if (Col.ownerId != State.playerEmpire) continue;
            const auto E = botf::economy(Draft, Col);
            if (E.foodDeficit > 0)
                InboxRow(FString::Printf(TEXT("%s: food short by %d"), *SystemName(Col.systemId), E.foodDeficit),
                    [this, Id = Col.systemId] { SelectSystem(Id); GoPanel(PanelSystem); }, &FPalette::Alert, TEXT("People starve if food falls short"));
            if (!Col.governorWarning.empty())
                InboxRow(FString::Printf(TEXT("%s: %s"), *SystemName(Col.systemId), *Str(Col.governorWarning)),
                    [this, Id = Col.systemId] { SelectSystem(Id); GoPanel(PanelSystem); }, &FPalette::Alert, TEXT("Governor alert: food capacity cannot support population"));
            if (Col.workforceManualOverride && Col.workforcePriority != botf::GovernorPriority::Manual)
                InboxRow(FString::Printf(TEXT("%s: governor paused by manual workforce assignment"), *SystemName(Col.systemId)),
                    [this, Id = Col.systemId] { SelectSystem(Id); GoPanel(PanelSystem); }, &FPalette::Tertiary, TEXT("Click to inspect and resume governor"));
            if (Col.queue.empty() && Col.queuePriority == botf::GovernorPriority::Manual)
                InboxRow(SystemName(Col.systemId) + TEXT(": nothing being built"),
                    [this, Id = Col.systemId] { SelectSystem(Id); GoPanel(PanelSystem); }, &FPalette::Tertiary);
        }
        if (bAllocationDirty) InboxRow(TEXT("Research allocation changed but not applied"), [this] { GoPanel(PanelResearch); }, &FPalette::Tertiary);
        if (const auto Income = IncomeForecast(); Income.Net() < 0)
            InboxRow(FString::Printf(TEXT("Income %+d per turn (ship maintenance %d)"), Income.Net(), Income.Ships),
                [this] { EmpireMode = 0; GoPanel(PanelEmpire); }, &FPalette::Alert, TEXT("Review your systems' credits and your fleet"));
        int Idle = 0;
        for (const auto& F : Draft.fleets)
            if (F.ownerId == State.playerEmpire && F.x == F.targetX && F.y == F.targetY && !FleetHasDraft(F.id) && !botf::fleetTaskForce(Draft, F.id)) ++Idle;
        if (Idle) InboxRow(FString::Printf(TEXT("%d ship%s without a course"), Idle, Idle == 1 ? TEXT("") : TEXT("s")),
            [this] { NextShip(); if (Chart.IsValid()) Chart->CentreOn(SelectedCell); }, &FPalette::Secondary, TEXT("Cycle through your units (Tab)"));
        for (const auto& Battle : State.latestBattles)
            if (Battle.turn == State.turn && (Battle.firstOwner == State.playerEmpire || Battle.secondOwner == State.playerEmpire))
                InboxRow(FString::Printf(TEXT("Battle at %d,%d: %s"), Battle.x, Battle.y,
                    Battle.winnerOwner == State.playerEmpire ? TEXT("victory") : Battle.winnerOwner ? TEXT("defeat") : TEXT("no victor")),
                    [this, Index = static_cast<int>(&Battle - State.latestBattles.data())] { BattleIndex = Index; BattleRoundIndex = 0; GoPanel(PanelCombat); ShowBattle(); },
                    &FPalette::Alert);
        // Turn briefing: completed projects, then the last turn's chronicle by category; entries with a place focus the chart.
        int Lines = 0;
        for (const auto& Line_ : LastEvents) if (Lines++ < 8) InboxRow(Line_, nullptr, &FPalette::Muted);
        int Briefed = 0;
        for (const auto& Entry : LastBriefing) {
            if (Briefed++ >= 24) {
                InboxRow(FString::Printf(TEXT("%d more briefing entries in the Summary"), static_cast<int>(LastBriefing.size()) - 24),
                    [this] { bSummaryOpen = true; SummaryMode = 0; bEventsOpen = bMenuOpen = false; }, &FPalette::Muted);
                break;
            }
            InboxRow(Str(Entry.categoryName).ToUpper() + TEXT("  /  ") + Str(Entry.text), BriefingAction(Entry), BriefingTone(Entry.category),
                Entry.focusCell >= 0 ? TEXT("Show on the chart") : FString());
        }
        if (!InboxCount) InboxRows->AddSlot().AutoHeight() [ Para(Txt(TEXT("Nothing needs your attention.")), 9, &FPalette::Muted) ];
    }
    using FPaletteColor = FLinearColor FPalette::*;
    static FPaletteColor BriefingTone(botf::ChronicleCategory Category) {
        switch (Category) {
        case botf::ChronicleCategory::Combat: return &FPalette::Alert;
        case botf::ChronicleCategory::Victory: return &FPalette::Accent;
        case botf::ChronicleCategory::Events: return &FPalette::Tertiary;
        case botf::ChronicleCategory::Diplomacy:
        case botf::ChronicleCategory::Intelligence: return &FPalette::Secondary;
        case botf::ChronicleCategory::Stations: return &FPalette::Primary;
        default: return &FPalette::Muted;
        }
    }
    // Clicking a briefing entry centres the chart on its place (and opens the event card for a live event).
    std::function<void()> BriefingAction(const botf::view::BriefingEntry& Entry) {
        const int Cell = Entry.focusCell, EventId = Entry.eventId;
        if (Cell < 0 && !EventId) return nullptr;
        return [this, Cell, EventId] {
            if (EventId) {
                const auto Cards = botf::view::eventCards(State);
                if (std::any_of(Cards.begin(), Cards.end(), [EventId](const botf::view::EventCard& C) { return C.id == EventId; })) {
                    bEventsOpen = true; bSummaryOpen = bMenuOpen = false;
                }
            }
            if (Cell >= 0) FocusCell(Cell);
        };
    }
    void FocusCell(int Cell) {
        if (Cell < 0 || Cell >= State.width * State.height) return;
        if (ActivePanel != PanelGalaxy) GoPanel(PanelGalaxy);
        SelectCell(Cell);
        if (Chart.IsValid()) Chart->CentreOn(Cell);
    }

    // ---- End-turn check: catch idle colonies, starvation and unanswered proposals before resolving ----
    TArray<FString> EndTurnWarnings() const {
        TArray<FString> Out;
        const auto& Draft = Preview();
        int Idle = 0;
        for (const auto& Col : Draft.colonies) {
            if (Col.ownerId != State.playerEmpire) continue;
            const auto E = botf::economy(Draft, Col);
            if (E.foodDeficit > 0) Out.Add(FString::Printf(TEXT("%s will be short of food by %d."), *SystemName(Col.systemId), E.foodDeficit));
            else if (!Col.governorWarning.empty()) Out.Add(FString::Printf(TEXT("%s: %s"), *SystemName(Col.systemId), *Str(Col.governorWarning)));
            if (Col.queue.empty() && Col.queuePriority == botf::GovernorPriority::Manual) ++Idle;
        }
        if (Idle) Out.Add(FString::Printf(TEXT("%d system%s with nothing to build."), Idle, Idle == 1 ? TEXT("") : TEXT("s")));
        if (bAllocationDirty) Out.Add(TEXT("Your research allocation change has not been applied."));
        if (IntelDraftDirty()) Out.Add(TEXT("Your unapplied intelligence allocation will be discarded if you end the turn anyway. Apply it first to keep it."));
        for (const auto& Proposal : botf::incomingProposals(State, State.playerEmpire))
            if (!ProposalResponsePending(Proposal.id) && botf::view::turnsLeft(State, Proposal.dueTurn) == 0)
                Out.Add(Str(botf::view::proposalSummary(State, Proposal, State.playerEmpire)) + TEXT(": unanswered, it will be ignored when this turn ends."));
        for (const auto& Call : OpenCallsToArms())
            if (botf::view::turnsLeft(State, Call.dueTurn) == 0)
                Out.Add(FString::Printf(TEXT("%s's call to arms against %s falls due: refusing costs 20 trust."),
                    *Str(botf::empireName(Call.defender)), *Str(botf::empireName(Call.aggressor))));
        // Galactic events awaiting a decision (the default applies once the deadline passes).
        for (const auto& Warning : botf::view::unresolvedEventWarnings(State)) Out.Add(Str(Warning));
        const auto Income = IncomeForecast();
        if (Income.Net() < 0) {
            const std::int64_t After = Draft.credits + Income.Net();
            Out.Add(After < 0
                ? FString::Printf(TEXT("The treasury will run dry: %+d this turn (ship maintenance %d) leaves %lld credits."), Income.Net(), Income.Ships, static_cast<long long>(After))
                : FString::Printf(TEXT("Income is negative: %+d this turn, including ship maintenance of %d."), Income.Net(), Income.Ships));
        }
        return Out;
    }
    // A request never confirms itself: while the warning is open, a repeated END TURN click or Ctrl+Enter leaves it
    // open. Only END TURN ANYWAY (or Enter inside the dialog) resolves the turn; REVIEW or Esc closes it.
    void RequestEndTurn() {
        bMenuOpen = false;
        if (bEndTurnCheck) return;
        if (Session.active || EndTurnWarnings().IsEmpty()) { EndTurn(); return; }
        bEndTurnCheck = true;
    }
    void ConfirmEndTurn() {
        if (!bEndTurnCheck) return;
        bEndTurnCheck = false;
        EndTurn();
    }
    TSharedRef<SWidget> BuildEndTurnCheck() {
        // Modal: a full-screen scrim takes every click, so nothing behind the dialog (END TURN included) can act.
        return SNew(SOverlay).Visibility_Lambda([this] { return bEndTurnCheck && Shell == EUiView::Game ? EVisibility::Visible : EVisibility::Collapsed; })
            + SOverlay::Slot()
            [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.5f))
                .OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); }) ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [ SNew(SBox).WidthOverride(520)
                [ Fill(EShape::Row, &FPalette::Void, Stack({
                    Heading(Txt(TEXT("BEFORE YOU END THE TURN")), 16, &FPalette::Accent),
                    Para(Dyn([this] { return FString::Join(EndTurnWarnings(), TEXT("\n")); }), 11, &FPalette::Text),
                    Line({ Control(TEXT("END TURN ANYWAY"), [this] { ConfirmEndTurn(); }, nullptr, nullptr, ERole::Accent, FMargin(16, 10), false, 11, TEXT("Enter")),
                           Control(TEXT("REVIEW"), [this] { bEndTurnCheck = false; }, nullptr, nullptr, ERole::Tertiary, FMargin(16, 10), false, 11, TEXT("Esc")) }, 6) }, 10),
                    FMargin(18, 14)) ] ];
    }

    // ---- Summary window (original: Events / Relationships / Systems, Tutorial p12) ----
    FString RelationshipsText() const {
        FString Text;
        for (int Id = 1; Id <= 5; ++Id) {
            if (Id == State.playerEmpire) continue;
            const auto* R = botf::majorRelation(State, State.playerEmpire, Id);
            if (!R || !R->contact) continue;
            if (botf::empireEliminated(State, Id)) { Text += Str(botf::empireName(Id)) + TEXT(": eliminated\n"); continue; }
            Text += FString::Printf(TEXT("%s: %s, trust %d%s\n"), *Str(botf::empireName(Id)), R->atWar ? TEXT("at war") : TEXT("at peace"),
                R->trust, R->tradeTreaty ? TEXT(", trade treaty") : TEXT(""));
        }
        for (const auto& M : State.minors) {
            if (!M.contact[State.playerEmpire]) continue;
            Text += FString::Printf(TEXT("%s: %s, relations %d%s\n"), *Str(M.name),
                M.memberOf ? *(FString(TEXT("member of ")) + Str(botf::empireName(M.memberOf))) : TEXT("independent"),
                M.attitude[State.playerEmpire], M.friendshipTurn[State.playerEmpire] >= 0 ? TEXT(", friendship") : TEXT(""));
        }
        return Text.IsEmpty() ? FString(TEXT("No contacts yet. Explore to meet other races.")) : Text;
    }
    FString SystemsText() const {
        FString Text;
        for (const auto& Col : State.colonies) {
            if (Col.ownerId != State.playerEmpire) continue;
            const auto E = botf::economy(State, Col);
            Text += FString::Printf(TEXT("%s: population %d, morale %d, food %d/%d, industry %d, research %d, credits %+d\n"),
                *SystemName(Col.systemId), Col.population, Col.morale, E.food, Col.population, E.industry, E.research, E.income - E.upkeep);
        }
        return Text.IsEmpty() ? FString(TEXT("You control no systems.")) : Text;
    }
    TSharedRef<SWidget> BuildSummaryWindow() {
        return SNew(SBox).Visibility_Lambda([this] { return bSummaryOpen && Shell == EUiView::Game ? EVisibility::Visible : EVisibility::Collapsed; })
            .MinDesiredWidth(560).MaxDesiredWidth(860).MaxDesiredHeight(620)
            [ Fill(EShape::Row, &FPalette::Void, SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth() [ Bar(EShape::ElbowTop, &FPalette::Primary, 40, 24) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0) [ Label(Txt(TEXT("SUMMARY")), 15, &FPalette::Accent, true) ]
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center) [ Bar(EShape::Block, &FPalette::Secondary, 0, 6) ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)
                    [ Pill(TEXT("CLOSE"), [this] { bSummaryOpen = false; }, nullptr, ERole::Alert, TEXT("Close (Esc)")) ] ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                [ Line({ ModeButton(TEXT("EVENTS"), &SummaryMode, 0), ModeButton(TEXT("RELATIONSHIPS"), &SummaryMode, 1), ModeButton(TEXT("SYSTEMS"), &SummaryMode, 2) }) ]
                + SVerticalBox::Slot().FillHeight(1)
                [ Scroll(Stack({
                    Fill(EShape::Row, &FPalette::FieldHi, Heading(Dyn([this] { return OutcomeText(); }), 15, &FPalette::Accent), FMargin(12, 6)),
                    Para(Dyn([this] {
                        if (SummaryMode == 1) return RelationshipsText();
                        if (SummaryMode == 2) return SystemsText();
                        return (LastReport.IsEmpty() ? FString(TEXT("No turn has been resolved yet this session.")) : LastReport)
                            + TEXT("\n\nALERTS\n") + AlertsText();
                    }), 11, &FPalette::Text) })) ],
                FMargin(16, 12)) ];
    }

    // ---- Galactic event card (schema 13 curated events): decisions with options and a deadline ----
    TSharedRef<SWidget> BuildEventCard() {
        return SNew(SBox).Visibility_Lambda([this] { return bEventsOpen && Shell == EUiView::Game ? EVisibility::Visible : EVisibility::Collapsed; })
            .MinDesiredWidth(560).MaxDesiredWidth(760).MaxDesiredHeight(620)
            [ Fill(EShape::Row, &FPalette::Void, SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth() [ Bar(EShape::ElbowTop, &FPalette::Primary, 40, 24) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0) [ Label(Txt(TEXT("GALACTIC EVENTS")), 15, &FPalette::Accent, true) ]
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center) [ Bar(EShape::Block, &FPalette::Secondary, 0, 6) ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)
                    [ Pill(TEXT("CLOSE"), [this] { bEventsOpen = false; }, nullptr, ERole::Alert, TEXT("Close (Esc)")) ] ]
                + SVerticalBox::Slot().FillHeight(1) [ Scroll(SAssignNew(EventRows, SVerticalBox)) ],
                FMargin(16, 12)) ];
    }
    // A decision is a drafted order: choosing again replaces it; it takes effect when the turn ends.
    void ChooseEventOption(int EventId, int Option) {
        ReplaceOrders([EventId](const botf::Command& O) { return O.type == botf::CommandType::ResolveEventChoice && O.eventId == EventId; },
            botf::makeResolveEventChoiceCommand(NextId(), EventId, Option));
        for (const auto& Card : botf::view::eventCards(State))
            if (Card.id == EventId) Status = TEXT("Decision ordered: ") + Str(botf::view::optionLabel(Card, Option)) + TEXT(". It takes effect when the turn ends.");
    }
    void RefreshEvents() {
        if (!EventRows.IsValid()) return;
        EventRows->ClearChildren();
        const auto Cards = botf::view::eventCards(State);
        for (const auto& Card : Cards) {
            const int EventId = Card.id, Focus = Card.focusCell;
            auto Options = SNew(SVerticalBox);
            for (const auto& Option : Card.options) {
                const int Index = Option.index;
                const bool Chosen = Card.chosen == Index;
                Options->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
                    [ Control(Str(Option.label).ToUpper() + (Option.isDefault ? TEXT("  (DEFAULT)") : TEXT("")) + TEXT("\n") + Str(Option.effect),
                        [this, EventId, Index] { ChooseEventOption(EventId, Index); },
                        [this, Open = Card.needsDecision] { return Open && !Session.active; }, [Chosen] { return Chosen; },
                        ERole::Primary, FMargin(4, 5, 8, 5), true, 10,
                        Card.needsDecision ? FString(TEXT("Order this response; you can change it until the turn ends")) : FString(TEXT("Already decided"))) ];
            }
            const FString Heading_ = Str(Card.title).ToUpper() + (Card.where.empty() ? FString() : TEXT("  /  ") + Str(Card.where).ToUpper());
            FString Detail = Str(Card.description);
            if (Card.endTurn > State.turn) Detail += FString::Printf(TEXT("\nIts effects last until turn %d."), Card.endTurn);
            const std::string Status_ = botf::view::eventStatusText(Card);
            EventRows->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
            [ Section(Txt(Heading_), SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5) [ Para(Txt(Detail), 10, &FPalette::Text) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
                [ Para(Txt(Str(Status_)), 10, Card.needsDecision && !Card.chosenThisTurn ? &FPalette::Alert : &FPalette::Muted) ]
                + SVerticalBox::Slot().AutoHeight() [ Options ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                [ SNew(SBox).Visibility(Focus >= 0 ? EVisibility::Visible : EVisibility::Collapsed)
                    [ Pill(TEXT("SHOW ON CHART"), [this, Focus] { bEventsOpen = false; FocusCell(Focus); }, nullptr, ERole::Tertiary) ] ]) ];
        }
        if (Cards.empty())
            EventRows->AddSlot().AutoHeight()
            [ Para(Txt(State.rules.curatedEvents
                ? TEXT("No galactic events affect your empire. The event deck draws from turn 6, then every 7 turns.")
                : TEXT("This campaign uses the original random events; curated galactic events are part of the Remastered ruleset.")), 10, &FPalette::Muted) ];
    }

    // ---- End-of-game overlay ----
    bool OutcomeOverlayVisible() const { return bHasActiveCampaign && State.outcome != botf::CampaignOutcome::Ongoing && !bOutcomeDismissed; }
    TSharedRef<SWidget> BuildOutcomeOverlay() {
        return SNew(SOverlay).Visibility_Lambda([this] { return OutcomeOverlayVisible() && Shell == EUiView::Game ? EVisibility::Visible : EVisibility::Collapsed; })
            + SOverlay::Slot()
            [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.72f))
                .OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); }) ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [ SNew(SBox).WidthOverride(640)
                [ Fill(EShape::Row, &FPalette::Void, Stack({
                    SNew(STextBlock).Text(Dyn([this] { return Str(botf::view::outcomeView(State).title); })).Font(BotfUi::Condensed(44))
                        .ColorAndOpacity_Lambda([this] { return FSlateColor(State.outcome == botf::CampaignOutcome::Victory ? P().Accent : P().Alert); }),
                    Para(Dyn([this] { return Str(botf::view::outcomeView(State).detail); }), 13, &FPalette::Text),
                    Para(Dyn([this] { return VictorySummaryText(); }), 10, &FPalette::Muted),
                    Line({ Control(TEXT("REVIEW THE GALAXY"), [this] { bOutcomeDismissed = true; }, nullptr, nullptr, ERole::Accent, FMargin(16, 10), false, 11,
                               TEXT("Close this and look around; the campaign cannot continue (Esc)")),
                           Control(TEXT("NEW CAMPAIGN"), [this] { OpenSubPage(EUiView::Setup); }, nullptr, nullptr, ERole::Primary, FMargin(16, 10), false, 11, FString()),
                           Control(TEXT("MAIN MENU"), [this] { ShowView(EUiView::Title); }, nullptr, nullptr, ERole::Tertiary, FMargin(16, 10), false, 11, FString()) }, 6) }, 12),
                    FMargin(22, 18)) ] ];
    }
    // One line per victory condition with the player's progress.
    FString VictorySummaryText() const {
        FString Text;
        for (const auto& Row_ : botf::view::victoryRows(State, State.playerEmpire)) {
            if (!Text.IsEmpty()) Text += TEXT("\n");
            Text += FString::Printf(TEXT("%s: %s"), *Str(Row_.name), Row_.enabled ? *FString::Printf(TEXT("%d%%%s"), Row_.percent, Row_.achieved ? TEXT(", achieved") : TEXT("")) : TEXT("off"));
        }
        return Text;
    }

    // ======================= GALAXY =======================
    FString SectorLabel(int Cell) const {
        if (Cell < 0) return FString();
        const int X = Cell % State.width, Y = Cell / State.width;
        FString Text = FString::Printf(TEXT("SECTOR %d,%d"), X, Y);
        if (!botf::explored(State, X, Y)) return Text + TEXT("  /  UNEXPLORED");
        const bool Seen = botf::visible(State, State.playerEmpire, X, Y);
        for (const auto& S : State.systems) {
            if (S.x != X || S.y != Y) continue;
            Text += TEXT("  /  ") + Str(S.name).ToUpper();
            const auto Col = botf::colonyAt(State, S.id);
            if (Col && (Col->ownerId == State.playerEmpire || Seen)) Text += TEXT("  /  ") + Str(botf::empireName(Col->ownerId)).ToUpper();
            else if (const auto M = botf::minorAt(State, S.id); M && M->contact[State.playerEmpire]) Text += TEXT("  /  ") + Str(M->name).ToUpper();
            if (!Seen) Text += TEXT("  /  NOT IN SENSOR RANGE");
            return Text;
        }
        return Text + (Seen ? TEXT("  /  OPEN SPACE") : TEXT("  /  OPEN SPACE, NOT IN SENSOR RANGE"));
    }
    FString SectorBrief() const {
        if (const auto* Col = OwnedColony()) {
            const auto E = botf::economy(State, *Col);
            return FString::Printf(TEXT("Population %d  |  Morale %d  |  Food %d/%d  |  Industry %d  |  Research %d  |  Credits %+d"),
                Col->population, Col->morale, E.food, Col->population, E.industry, E.research, E.income - E.upkeep);
        }
        const auto System = std::find_if(State.systems.begin(), State.systems.end(), [this](const botf::System& S) { return S.id == Selected; });
        if (System == State.systems.end()) return botf::explored(State, SelectedCell % State.width, SelectedCell / State.width)
            ? TEXT("No star system in this sector.") : TEXT("Unexplored. Send a ship to scan it.");
        if (!botf::visible(State, State.playerEmpire, System->x, System->y)) return TEXT("Charted earlier; current ownership unknown outside sensor range.");
        if (const auto M = botf::minorAt(State, Selected); M && M->memberOf == 0)
            return M->contact[State.playerEmpire] ? Str(M->name) + TEXT(", an independent civilization. Open Diplomacy to court them.")
                                                  : FString(TEXT("An inhabited world whose people you have not yet met."));
        if (const auto Col = botf::colonyAt(State, Selected)) return ForeignTargetText(*Col);
        return TEXT("Unclaimed system. A colony ship can settle it.");
    }
    // Invasion planning readout for a foreign colony in sensor range (Tutorial p28): population against capacity,
    // orbital defences as a bound (how many are powered is not known), visible warships and war status.
    FString ForeignTargetText(const botf::Colony& Col) const {
        const auto S = std::find_if(State.systems.begin(), State.systems.end(), [&Col](const botf::System& X) { return X.id == Col.systemId; });
        FString Text = Str(botf::empireName(Col.ownerId)) + TEXT(" colony.");
        if (S == State.systems.end() || !botf::visible(State, State.playerEmpire, S->x, S->y)) return Text;
        int Ships = 0;
        for (const auto& F : State.fleets) if (F.ownerId == Col.ownerId && F.x == S->x && F.y == S->y) ++Ships;
        const auto* R = botf::majorRelation(State, State.playerEmpire, Col.ownerId);
        Text += FString::Printf(TEXT("\nPopulation %d of %d  |  orbital batteries: up to %d, powered count unknown  |  warships here %d"),
            Col.population, botf::systemCapacity(State, Col.systemId), Col.defenseBatteries, Ships);
        Text += R && R->atWar ? TEXT("\nAt war: troop transports can invade.") : TEXT("\nNot at war: invasion needs a declaration of war.");
        return Text;
    }
    // Schema 13: anomalies charted in the selected sector and the profile of a contacted minor race there.
    FString SectorExtras() const {
        const int X = SelectedCell % State.width, Y = SelectedCell / State.width;
        FString Text = Str(botf::view::featureTooltip(State, X, Y));
        if (const auto* M = Selected ? botf::minorAt(State, Selected) : nullptr; M && M->contact[State.playerEmpire]) {
            if (!Text.IsEmpty()) Text += TEXT("\n\n");
            Text += Str(botf::view::minorProfileText(botf::view::minorProfile(State, *M)));
        }
        return Text;
    }
    // Short sector lines for the chart's hover hint: anomalies, a visible station and a contacted minor race.
    FString CellDetailLines(int CellId) const {
        const int X = CellId % State.width, Y = CellId / State.width;
        FString Text;
        if (!botf::explored(State, X, Y)) return Text;
        for (const auto& Feature : botf::featuresAt(State, X, Y)) {
            Text += TEXT("\n") + Str(botf::galaxyFeatureName(Feature.type)) + TEXT(": ");
            switch (Feature.type) {
            case botf::GalaxyFeatureType::Nebula: Text += TEXT("hides ships; shields fail in battle"); break;
            case botf::GalaxyFeatureType::Wormhole: {
                FString Exit = TEXT("exit uncharted");
                for (const auto& Other : State.galaxyFeatures)
                    if (Other.id == Feature.pairId && botf::explored(State, Other.x, Other.y)) Exit = FString::Printf(TEXT("leads to %d,%d"), Other.x, Other.y);
                Text += Exit;
                break;
            }
            case botf::GalaxyFeatureType::BlackHole: Text += TEXT("impassable"); break;
            case botf::GalaxyFeatureType::NeutronStar: Text += TEXT("10% hull damage per turn"); break;
            case botf::GalaxyFeatureType::IonStorm: Text += TEXT("drifting; drains shields and 5% hull per turn"); break;
            case botf::GalaxyFeatureType::SubspaceRift: Text += FString::Printf(TEXT("impassable until turn %d"), Feature.expiresTurn); break;
            }
        }
        if (const auto* St = botf::view::visibleStationAt(State, X, Y)) {
            const auto Stats = botf::stationStats(State, *St);
            Text += FString::Printf(TEXT("\n%s (%s): hull %d/%d, supply %d, sensors %d"), *Str(botf::stationName(*St)), *Str(botf::empireName(St->ownerId)),
                St->hull, Stats.maxHull, Stats.supplyRadius, Stats.sensorRadius);
        }
        if (const int SystemId = SystemAtCell(CellId))
            if (const auto* M = botf::minorAt(State, SystemId); M && M->contact[State.playerEmpire])
                Text += TEXT("\n") + Str(botf::view::minorShortText(botf::view::minorProfile(State, *M)));
        return Text;
    }
    // ---- Stations: inspector and starbase upgrade ----
    const botf::Station* SelectedStation() const {
        return botf::view::visibleStationAt(State, SelectedCell % State.width, SelectedCell / State.width);
    }
    // The funding colony shown for an upgrade: the one picked, while it is still an option, else the nearest that can.
    int UpgradeFundingColony() const {
        const auto* St = SelectedStation();
        if (!St) return 0;
        const auto Options = botf::view::upgradeOptions(Preview(), St->id);
        for (const auto& Option : Options) if (Option.systemId == UpgradeFunder) return UpgradeFunder;
        const int Best = botf::view::bestUpgradeFunder(Preview(), St->id);
        return Best ? Best : (Options.empty() ? 0 : Options.front().systemId);
    }
    FString UpgradeBlocker() const {
        const auto* St = SelectedStation();
        if (!St || St->ownerId != State.playerEmpire) return TEXT("Not one of your stations.");
        if (Session.active) return TEXT("Finish the battle in progress first.");
        const int Funder = UpgradeFundingColony();
        if (!Funder) return St->type == botf::StationType::Outpost ? TEXT("No colony of yours can fund the upgrade.") : TEXT("Already a starbase.");
        return Str(botf::stationUpgradeBlocker(Preview(), St->id, Funder));
    }
    void CycleUpgradeFunder() {
        const auto* St = SelectedStation();
        if (!St) return;
        const auto Options = botf::view::upgradeOptions(Preview(), St->id);
        if (Options.empty()) { UpgradeFunder = 0; return; }
        const int Current = UpgradeFundingColony();
        const auto It = std::find_if(Options.begin(), Options.end(), [Current](const botf::view::UpgradeOption& O) { return O.systemId == Current; });
        UpgradeFunder = It == Options.end() || std::next(It) == Options.end() ? Options.front().systemId : std::next(It)->systemId;
    }
    void OrderStationUpgrade() {
        const auto* St = SelectedStation();
        if (!St || St->ownerId != State.playerEmpire) throw std::runtime_error("Select one of your outposts first");
        const int Funder = UpgradeFundingColony();
        const int StationId = St->id;
        Submit(botf::makeUpgradeStationCommand(NextId(), StationId, Funder));
        Status = FString::Printf(TEXT("Starbase upgrade ordered, funded by %s. The credits are paid now; its industry goes to the upgrade from next turn."),
            *SystemName(Funder));
    }
    TSharedRef<SWidget> BuildStationInspector() {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
            [ Heading(Dyn([this] { const auto* St = SelectedStation(); return St ? Str(botf::stationName(*St)).ToUpper() : FString(); }), 12, &FPalette::Accent) ]
            + SVerticalBox::Slot().AutoHeight()
            [ Para(Dyn([this] { const auto* St = SelectedStation(); return St ? Str(botf::view::stationInspectorText(Preview(), St->id)) : FString(); }), 9, &FPalette::Text) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
            [ SNew(SBox).Visibility_Lambda([this] {
                    const auto* St = SelectedStation();
                    return St && St->ownerId == State.playerEmpire && St->type == botf::StationType::Outpost ? EVisibility::Visible : EVisibility::Collapsed; })
                [ SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                    [ Para(Dyn([this] {
                        FString Text = Str(botf::view::upgradeCostText(Preview()));
                        if (const int Funder = UpgradeFundingColony()) Text += TEXT("\nFunding colony: ") + SystemName(Funder);
                        const FString Blocker = UpgradeBlocker();
                        return Blocker.IsEmpty() ? Text : Text + TEXT("\n") + Blocker;
                    }), 9, &FPalette::Muted) ]
                    + SVerticalBox::Slot().AutoHeight()
                    [ Grid({
                        Pill(TEXT("NEXT FUNDING COLONY"), [this] { CycleUpgradeFunder(); }, [this] { return UpgradeFundingColony() != 0; }, ERole::Secondary,
                            TEXT("Choose which of your colonies within 6 sectors pays the industry")),
                        Pill(TEXT("UPGRADE TO STARBASE"), [this] { OrderStationUpgrade(); }, [this] { return UpgradeBlocker().IsEmpty(); }, ERole::Accent,
                            TEXT("Starbase: supply radius 5, sensor radius 3, repairs friendly ships in its sector")) }, 2) ] ] ];
    }
    TSharedRef<SWidget> OrderButton(const FString& Label, std::function<void()> Action, std::function<bool()> Enabled, const FString& Tip, ERole Role = ERole::Primary) {
        return Control(Label, Action, Enabled, nullptr, Role, FMargin(6, 6), false, 9, Tip);
    }
    // Selected unit card in the left dock: readout and every order the unit can take.
    TSharedRef<SWidget> BuildUnitCard() {
        return Stack({
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1) [ Heading(Dyn([this] { return UnitName().ToUpper(); }), 13, &FPalette::Accent) ]
                + SHorizontalBox::Slot().AutoWidth() [ Pill(TEXT("FIND"), [this] {
                    if (const auto* F = Fleet()) { SelectCell(F->y * State.width + F->x); if (Chart.IsValid()) Chart->CentreOn(SelectedCell); }
                  }, nullptr, ERole::Tertiary, TEXT("Centre the chart on this unit")) ],
            Para(Dyn([this] { return ShipReadout(); }), 9, &FPalette::Text),
            Section(Txt(TEXT("ORDERS")), SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [ Grid({
                    OrderButton(TEXT("SET COURSE"), [this] { MapView.bPlotting = true; }, [this] { return Fleet() != nullptr; }, TEXT("Then click a destination, or simply right-click one (P)")),
                    OrderButton(TEXT("HOLD"), [this] { HoldSelected(); }, [this] {
                        const auto* F = DraftFleet(); return F && (F->targetX != F->x || F->targetY != F->y);
                    }, TEXT("Stop at the current sector, replacing any drafted course")),
                    OrderButton(TEXT("COLONIZE"), [this] { OrderColonize(); }, [this] { return CanSettle(); }, TEXT("Colony ship in an unclaimed system")),
                    OrderButton(TEXT("ATTACK"), [this] {
                        botf::Command Cmd; Cmd.id = State.lastAcceptedCommand + 1; Cmd.type = botf::CommandType::AttackFleet;
                        Cmd.fleetId = SelectedFleet; Cmd.targetFleetId = TargetFleet()->id;
                        if (const auto* Group = CurrentTaskForce()) TaskForceOrder(TargetFleet()->x, TargetFleet()->y, Group->stance, TargetFleet()->id);
                        else Submit(Cmd);
                        Status = TEXT("Attack ordered. The battle is fought when you end the turn.");
                    }, [this] {
                        const auto* F = Fleet(); const auto* Enemy = TargetFleet();
                        if (!F || !Enemy) return false;
                        const auto* R = botf::majorRelation(State, State.playerEmpire, Enemy->ownerId);
                        return R && R->atWar && UnitCanReach(*F, Enemy->x, Enemy->y);
                    }, TEXT("Select an enemy ship in this sector (or right-click it on the chart); requires war"), ERole::Alert),
                    OrderButton(TEXT("INVADE"), [this] { OrderInvade(); }, [this] { return CanInvadeHere(); }, TEXT("Troop transport over an enemy colony, at war"), ERole::Alert),
                    OrderButton(TEXT("BUILD OUTPOST"), [this] { OrderBuildOutpost(); }, [this] { return CanBuildOutpost(); },
                        TEXT("Troop transport: assemble an outpost in this sector. It extends supply and sensor range; the transport is used up."), ERole::Accent),
                    OrderButton(TEXT("RETREAT"), [this] {
                        botf::Command Cmd; Cmd.id = State.lastAcceptedCommand + 1; Cmd.type = botf::CommandType::RetreatFleet; Cmd.fleetId = SelectedFleet;
                        Submit(Cmd); Status = TEXT("Ship ordered to disengage.");
                    }, [this] { return Fleet() != nullptr; }, TEXT("Disengage from combat"), ERole::Tertiary),
                    OrderButton(TEXT("NEXT UNIT"), [this] { NextShip(); }, [this] { return OwnShipCount() > 0; }, TEXT("Cycle your task forces and ships (Tab)"), ERole::Secondary),
                    OrderButton(TEXT("CANCEL UNIT ORDERS"), [this] {
                        ReplaceOrders(SelectedUnitOrders(), std::nullopt);
                        Status = TEXT("This turn's orders for the selected unit were cancelled.");
                    }, [this] { return !Session.active && SelectedUnitHasDraft(); }, TEXT("Withdraw every order drafted this turn for the selected ship or task force"), ERole::Tertiary),
                    OrderButton(TEXT("CANCEL LAST ORDER"), [this] { CancelLast(); Status = TEXT("Last order cancelled."); },
                        [this] { return !Session.active && !State.pending.empty(); }, TEXT("Undo the most recent order of any kind"), ERole::Tertiary) }, 2) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                [ Para(Dyn([this] { return PendingUnitText(); }), 9, &FPalette::Muted) ]
                // Troop transports in a station ruleset: outpost assembly time, core's blocker or assembly progress.
                + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                [ SNew(SBox).Visibility_Lambda([this] { return SafeText([this] { return OutpostText(); }).IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
                    [ Para(Dyn([this] { return OutpostText(); }), 9, &FPalette::Tertiary) ] ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 2) [ Heading(Txt(TEXT("TASK FORCE")), 9, &FPalette::Muted) ]
                + SVerticalBox::Slot().AutoHeight()
                [ Grid({
                    OrderButton(TEXT("FORM FROM MARKED"), [this] { FormTaskForce(); }, [this] { return ValidFormMarks(); },
                        TEXT("Group the ships marked + in this sector")),
                    OrderButton(TEXT("SPLIT"), [this] { DisbandTaskForce(); }, [this] { return CurrentTaskForce() != nullptr; },
                        TEXT("Disband the selected task force into individual ships"), ERole::Tertiary) }, 2) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 2) [ Heading(Txt(TEXT("COMBAT STANCE")), 9, &FPalette::Muted) ]
                + SVerticalBox::Slot().AutoHeight()
                [ Grid({ StanceButton(botf::TacticalStance::Balanced), StanceButton(botf::TacticalStance::Aggressive),
                         StanceButton(botf::TacticalStance::Defensive), StanceButton(botf::TacticalStance::Withdraw) }, 2) ]) });
    }
    TSharedRef<SWidget> StanceButton(botf::TacticalStance Stance) {
        return Control(StanceName(Stance), [this, Stance] {
                if (const auto* Group = CurrentTaskForce()) {
                    TaskForceOrder(Group->targetX, Group->targetY, Stance, Group->attackTarget);
                    Status = FString(TEXT("Task force stance ordered: ")) + StanceName(Stance) + TEXT(".");
                } else SetStance(Stance);
            },
            [this] { return State.advancedRulesEnabled && Fleet() != nullptr; },
            [this, Stance] {
                if (const auto* Group = CurrentTaskForce()) return Group->stance == Stance;
                return Fleet() && Fleet()->stance == Stance;
            }, ERole::Secondary, FMargin(6, 5), false, 9,
            TEXT("How the selected ship or task force fights when battle is joined"));
    }
    FString PendingUnitText() const {
        const auto Match = SelectedUnitOrders();
        const int Unit = (SelectedFleet || SelectedTaskForce) ? static_cast<int>(std::count_if(State.pending.begin(), State.pending.end(), Match)) : 0;
        return FString::Printf(TEXT("%d order%s drafted for this unit  |  %d this turn in total"), Unit, Unit == 1 ? TEXT("") : TEXT("s"),
            static_cast<int>(State.pending.size()));
    }
    FString ShipReadout() const {
        if (const auto* Group = CurrentTaskForce()) {
            const auto& Draft = Preview();
            FString Text = FString::Printf(TEXT("%s  (%d ships)\nHull %d  |  Shields %d  |  Attack %d  |  Speed %d\nStance %s  |  Destination %d,%d"),
                *Str(Group->name).ToUpper(), static_cast<int>(Group->fleetIds.size()),
                botf::taskForceHull(Draft, Group->id), botf::taskForceShields(Draft, Group->id), botf::taskForceAttack(Draft, Group->id),
                botf::taskForceSpeed(Draft, Group->id), StanceName(Group->stance), Group->targetX, Group->targetY);
            if (const auto* Limit = botf::view::rangeLimitingFleet(Draft, Group->id))
                Text += FString::Printf(TEXT("\nRange %d, set by its shortest-ranged ship: %s #%d"),
                    botf::view::supplyRange(Draft, *Limit), *Str(botf::fleetName(*Limit)), Limit->id);
            for (const auto& Member : botf::taskForceFleets(Draft, Group->id))
                Text += FString::Printf(TEXT("\n  %s #%d  hull %d/%d"), *Str(botf::fleetName(Member)), Member.id, Member.hull, botf::fleetMaxHull(Member));
            return Text;
        }
        const auto F = DraftFleet();   // drafted destination/stance this turn
        if (!F) return TEXT("No ship selected. Select a sector with your ships, or press Tab.");
        const auto Path = botf::route(Preview(), *F, F->targetX, F->targetY);
        FString Text = FString::Printf(TEXT("%s  #%d\nSector %d,%d  |  Hull %d/%d  |  Shields %d/%d\nAttack %d  |  Range %d  |  Stance %s"),
            *Str(botf::fleetName(*F)), F->id, F->x, F->y, F->hull, botf::fleetMaxHull(*F), F->shields, botf::fleetMaxShields(*F),
            botf::fleetAttack(*F), botf::view::supplyRange(Preview(), *F), StanceName(F->stance));
        if (F->targetX != F->x || F->targetY != F->y)
            Text += Path.empty() ? FString::Printf(TEXT("\nDestination %d,%d: out of supply range, holding"), F->targetX, F->targetY)
                : FString::Printf(TEXT("\nDestination %d,%d: %d turns"), F->targetX, F->targetY, static_cast<int>(Path.size()) - 1);
        for (const auto& Order : State.pending) if (Order.fleetId == F->id) {
            if (Order.type == botf::CommandType::MoveFleet) Text += FString::Printf(TEXT("\nOrdered: course to %d,%d"), Order.targetX, Order.targetY);
            else if (Order.type == botf::CommandType::Colonize) Text += TEXT("\nOrdered: colonize");
            else if (Order.type == botf::CommandType::AttackFleet) Text += FString::Printf(TEXT("\nOrdered: attack ship #%d"), Order.targetFleetId);
            else if (Order.type == botf::CommandType::RetreatFleet) Text += TEXT("\nOrdered: retreat");
            else if (Order.type == botf::CommandType::Invade) Text += TEXT("\nOrdered: invade");
            else if (Order.type == botf::CommandType::SetTacticalStance) Text += FString::Printf(TEXT("\nOrdered: stance %s"), StanceName(Order.stance));
            else if (Order.type == botf::CommandType::BuildOutpost) Text += TEXT("\nOrdered: build outpost");
        }
        if (const auto* Base = Fleet(); Base && Base->buildingOutpost)
            Text += FString::Printf(TEXT("\nAssembling an outpost: %d of %d turns"), Base->outpostProgress, botf::outpostBuildTurns(State, Base->ownerId));
        return Text;
    }
    void RefreshSector() {
        if (!SectorRows.IsValid()) return;
        SectorRows->ClearChildren();
        int Count = 0;
        PruneFormMarks();
        const auto& Draft = Preview();
        // Own task forces first, as one row each; members appear only when expanded.
        std::set<int> Shown;
        for (const auto* Lead : OwnShipsHere()) {
            const auto* Group = botf::fleetTaskForce(Draft, Lead->id);
            if (!Group || !Shown.insert(Group->id).second) continue;
            ++Count;
            const int GroupId = Group->id, LeadId = Group->fleetIds.empty() ? Lead->id : Group->fleetIds.front();
            const FString Label = FString::Printf(TEXT("%s  /  %d SHIPS\nHull %d  |  Shields %d  |  %s"), *Str(Group->name).ToUpper(),
                static_cast<int>(Group->fleetIds.size()), botf::taskForceHull(Draft, GroupId), botf::taskForceShields(Draft, GroupId), StanceName(Group->stance));
            const bool Expanded = ExpandedTaskForces.count(GroupId) > 0;
            SectorRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0) [ SNew(SBox).WidthOverride(5) [ Swatch(State.playerEmpire, 0) ] ]
                    + SHorizontalBox::Slot().FillWidth(1)
                    [ Row(Label, [this, GroupId, LeadId] { SelectedFleet = LeadId; SelectedTaskForce = GroupId; },
                        [this, GroupId] { return SelectedTaskForce == GroupId; }) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3, 0, 0, 0)
                    [ Pill(Expanded ? TEXT("-") : TEXT("+"), [this, GroupId] {
                        if (!ExpandedTaskForces.erase(GroupId)) ExpandedTaskForces.insert(GroupId);
                        RefreshSector();
                    }, nullptr, ERole::Tertiary, TEXT("Show or hide the ships in this task force")) ] ];
            if (!Expanded) continue;
            for (const auto& Member : botf::taskForceFleets(Draft, GroupId))
                SectorRows->AddSlot().AutoHeight().Padding(14, 0, 0, 3)
                    [ Row(FString::Printf(TEXT("%s #%d  hull %d/%d"), *Str(botf::fleetName(Member)), Member.id, Member.hull, botf::fleetMaxHull(Member)),
                        [this, Id = Member.id] { SelectedFleet = Id; SelectedTaskForce = 0; },
                        [this, Id = Member.id] { return SelectedTaskForce == 0 && SelectedFleet == Id; }, nullptr,
                        TEXT("Select this ship on its own (colonize, invade, terraform)")) ];
        }
        for (const auto& F : Draft.fleets) {
            if (F.x != SelectedCell % State.width || F.y != SelectedCell / State.width) continue;
            const bool Mine = F.ownerId == State.playerEmpire;
            if (!Mine && !botf::visible(State, State.playerEmpire, F.x, F.y)) continue;
            if (Mine && botf::fleetTaskForce(Draft, F.id)) continue;
            ++Count;
            const FString Label = FString::Printf(TEXT("%s%s #%d\nHull %d/%d  |  Shields %d/%d"),
                Mine ? TEXT("") : *(Str(botf::empireName(F.ownerId)).ToUpper() + TEXT("  ")),
                *Str(botf::fleetName(F)), F.id, F.hull, botf::fleetMaxHull(F), F.shields, botf::fleetMaxShields(F));
            SectorRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0) [ SNew(SBox).WidthOverride(5) [ Swatch(F.ownerId, 0) ] ]
                    + SHorizontalBox::Slot().FillWidth(1)
                    [ Mine ? Row(Label, [this, Id = F.id] { SelectedFleet = Id; SelectedTaskForce = 0; }, [this, Id = F.id] { return SelectedTaskForce == 0 && SelectedFleet == Id; })
                           : Row(Label, [this, Id = F.id] { SelectedEnemyFleet = Id; }, [this, Id = F.id] { return SelectedEnemyFleet == Id; }) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3, 0, 0, 0)
                    [ Mine ? Control(TEXT("+"), [this, Id = F.id] { if (!FormSelection.erase(Id)) FormSelection.insert(Id); }, nullptr,
                                [this, Id = F.id] { return FormSelection.count(Id) > 0; }, ERole::Secondary, FMargin(6, 4), false, 9,
                                TEXT("Mark for a new task force"))
                           : SNullWidget::NullWidget ] ];
        }
        if (!Count) SectorRows->AddSlot().AutoHeight()
            [ Para(Txt(FString::Printf(TEXT("No ships in this sector. You have %d ships; Tab cycles them."), OwnShipCount())), 9, &FPalette::Muted) ];
        RefreshPlanets();
    }
    // Planets strip under the chart (original Main Galactic screen, Tutorial p5-7); terraforming by colony ship.
    // Planet survey (original planet strip, Tutorial p5-7): one row per planet; clicking terraforms with the selected colony ship.
    void RefreshPlanets() {
        for (TSharedPtr<SVerticalBox>* Target : { &PlanetRows, &SheetPlanetRows }) {
            if (!Target->IsValid()) continue;
            SVerticalBox& Box = **Target;
            Box.ClearChildren();
            const auto& Planets = botf::systemPlanets(State, Selected);
            if (!Selected || Planets.empty()) {
                if (Selected) Box.AddSlot().AutoHeight() [ Para(Txt(TEXT("No planet survey for this system.")), 9, &FPalette::Muted) ];
                continue;
            }
            for (const auto& Planet : Planets) {
                const bool Habitable = Planet.terraformState == botf::TerraformingState::Terraformed;
                const FString Label = FString::Printf(TEXT("%s  /  %s  /  %s  /  pop %d of %d"), *Str(Planet.name).ToUpper(),
                    *Str(botf::planetClassName(Planet.planetClass)), *Str(botf::terraformingStateName(Planet.terraformState)).ToLower(),
                    Planet.currentCapacity(), Planet.maxCapacity);
                const FString Tip = Planet.terraformState == botf::TerraformingState::InProgress
                    ? FString::Printf(TEXT("Terraforming %d of %d"), Planet.terraformWork, Planet.terraformNeeded)
                    : TEXT("Select a colony ship in this system, then click to terraform");
                Box.AddSlot().AutoHeight().Padding(0, 0, 0, 2)
                    [ SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
                        [ SNew(SBox).WidthOverride(4) [ Fill(EShape::Block, Habitable ? &FPalette::Good
                            : Planet.terraformState == botf::TerraformingState::Uninhabitable ? &FPalette::Muted : &FPalette::Tertiary) ] ]
                        + SHorizontalBox::Slot().FillWidth(1)
                        [ Control(Label, [this, Index = Planet.index] {
                                botf::Command Cmd; Cmd.id = NextId(); Cmd.type = botf::CommandType::TerraformPlanet;
                                Cmd.fleetId = SelectedFleet; Cmd.systemId = Selected; Cmd.planetIndex = Index;
                                Submit(Cmd); Status = TEXT("Colony ship ordered to terraform this planet.");
                            }, [this, Index = Planet.index] { return Fleet() && botf::canTerraformPlanet(State, SelectedFleet, Selected, Index); },
                            nullptr, ERole::Secondary, FMargin(4, 3), true, 8, Tip) ] ];
            }
        }
    }

    // ======================= SOLAR SYSTEM =======================
    const botf::Colony* OwnedColony() const {
        const auto Col = botf::colonyAt(State, Selected);
        return Col && Col->ownerId == State.playerEmpire ? Col : nullptr;
    }
    const botf::Colony* DraftColony() const {
        const auto Col = botf::colonyAt(Preview(), Selected);
        return Col && Col->ownerId == State.playerEmpire ? Col : nullptr;
    }
    // Solar System sheet: everything about one colony on one page. Left: planets, workforce, energy.
    // Middle: what can be built (structures, starships, installed structures). Right: the build queue.
    FString ColonyFigure(int Which) const {
        const auto* Col = DraftColony();
        if (!Col) return TEXT("-");
        const auto E = botf::economy(Preview(), *Col);
        switch (Which) {
        case 0: return FString::FromInt(Col->population);
        case 1: return FString::FromInt(botf::systemCapacity(Preview(), Selected));
        case 2: return FString::FromInt(Col->morale);
        case 3: return FString::Printf(TEXT("%+d"), E.food - Col->population);
        case 4: return FString::FromInt(E.industry);
        case 5: return FString::Printf(TEXT("%d/%d"), E.energyUsed, Col->energy);
        case 6: return FString::FromInt(E.research);
        default: return FString::Printf(TEXT("%+d"), E.income - E.upkeep);
        }
    }
    TSharedRef<SWidget> BuildSystemScreen() {
        auto Figures = SNew(SHorizontalBox);
        const TCHAR* Captions[] = { TEXT("POPULATION"), TEXT("MAX POP"), TEXT("MORALE"), TEXT("FOOD"), TEXT("INDUSTRY"), TEXT("ENERGY"), TEXT("RESEARCH"), TEXT("CREDITS") };
        for (int I = 0; I < 8; ++I)
            Figures->AddSlot().AutoWidth().Padding(0, 0, 16, 0)
                [ Chip(Captions[I], [this, I] { return ColonyFigure(I); },
                    TAttribute<FSlateColor>::CreateLambda([this, I] { return FSlateColor(ColonyFigure(I).StartsWith(TEXT("-")) && I != 1 && I != 5 ? P().Alert : P().Text); }),
                    I == 1 ? TEXT("Current capacity; terraforming raises it") : TEXT("Projected with this turn's orders")) ];
        Figures->AddSlot().AutoWidth() [ Chip(TEXT("GROWTH"), [this] { return GrowthText(); }, C(&FPalette::Good), TEXT("Population change expected next turn")) ];
        return Console(Dyn([this] { return TEXT("SOLAR SYSTEM  /  ") + SystemTitle(); }), SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
                [ Pill(TEXT("<"), [this] { CycleColony(-1); }, [this] { return OwnColonyCount() > 1; }, ERole::Secondary, TEXT("Previous system (Q)")) ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)
                [ Pill(TEXT(">"), [this] { CycleColony(1); }, [this] { return OwnColonyCount() > 1; }, ERole::Secondary, TEXT("Next system (E)")) ]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center) [ Figures ] ]
            + SVerticalBox::Slot().FillHeight(1)
            [ SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return OwnedColony() ? 0 : 1; })
                + SWidgetSwitcher::Slot()
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 10, 0)
                    [ Scroll(Stack({
                        Section(Txt(TEXT("PLANETS")), SAssignNew(SheetPlanetRows, SVerticalBox)),
                        Section(Txt(TEXT("WORKFORCE")), BuildProductionView()),
                        Section(Txt(TEXT("ENERGY")), BuildEnergyView()),
                        BuildMegaprojectSection() })) ]
                    + SHorizontalBox::Slot().FillWidth(1.1f).Padding(0, 0, 10, 0)
                    [ SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                        [ Line({ ModeButton(TEXT("STRUCTURES"), &CatalogueMode, 0, TEXT("Structures you can build here")),
                                 ModeButton(TEXT("STARSHIPS"), &CatalogueMode, 1, TEXT("Starships this system's shipyard can build")),
                                 ModeButton(TEXT("INSTALLED"), &CatalogueMode, 2, TEXT("Structures present; scrap them here")) }, 4) ]
                        + SVerticalBox::Slot().FillHeight(1)
                        [ SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return CatalogueMode; })
                            + SWidgetSwitcher::Slot() [ BuildChoiceView(TEXT("BUILD LIST"), BuildRows) ]
                            + SWidgetSwitcher::Slot() [ BuildChoiceView(TEXT("SHIP LIST"), ShipRows) ]
                            + SWidgetSwitcher::Slot() [ Scroll(BuildStructuresView()) ] ] ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [ SNew(SBox).WidthOverride(300) [ Scroll(Stack({ BuildQueueColumn(), BuildTemplateSection() })) ] ] ]
                + SWidgetSwitcher::Slot()
                [ Para(Txt(TEXT("This is not one of your systems. Use < > to page through your colonies, or double-click one on the chart.")), 12, &FPalette::Muted) ] ],
            14, SheetHeader({}));
    }
    FString GrowthText() const {
        const auto* Col = DraftColony();
        if (!Col) return TEXT("-");
        if (!State.populationEnabled) return TEXT("fixed");
        const auto E = botf::economy(Preview(), *Col);
        const auto G = botf::populationForecast(Col->population, E.food, botf::systemCapacity(Preview(), Selected),
            10 * (Col->farms + Col->factories + Col->laboratories), botf::PrototypeGrowthBasisPoints);
        return FString::Printf(TEXT("%+d to %+d"), G.minimum, G.maximum);
    }
    // One production row per worked structure type: count, output, lit-block slider and arrows (Tutorial p8-9).
    TSharedRef<SWidget> ProductionRow(int Field) {
        const FString Structure = Field == 0 ? TEXT("FARMS") : Field == 1 ? TEXT("FACTORIES") : TEXT("LABORATORIES");
        auto Count = [this, Field] {
            const auto* Col = DraftColony(); if (!Col) return 0;
            return Field == 0 ? Col->farms : Field == 1 ? Col->factories : Col->laboratories;
        };
        auto Workers = [this, Field] {
            const auto* Col = DraftColony(); if (!Col) return 0;
            return Field == 0 ? Col->foodWorkers : Field == 1 ? Col->industryWorkers : Col->researchWorkers;
        };
        return Fill(EShape::Row, &FPalette::FieldHi, SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)
                [ Heading(Dyn([Structure, Count, Workers] { return FString::Printf(TEXT("%d %s  /  %d WORKERS"), Count(), *Structure, Workers()); }), 11, &FPalette::Tertiary) ]
                + SHorizontalBox::Slot().AutoWidth()
                [ SNew(STextBlock).Text(Dyn([this, Field] {
                        const auto* Col = DraftColony(); if (!Col) return FString();
                        const auto E = botf::economy(Preview(), *Col);
                        if (Field == 0) return FString::Printf(TEXT("OUTPUT %d FOOD  (NEED %d)"), E.food, Col->population);
                        return Field == 1 ? FString::Printf(TEXT("OUTPUT %d INDUSTRY"), E.industry) : FString::Printf(TEXT("OUTPUT %d RESEARCH"), E.research);
                    })).Font(BotfUi::Condensed(11))
                    .ColorAndOpacity_Lambda([this, Field] {
                        const auto* Col = DraftColony();
                        const bool Short = Field == 0 && Col && botf::economy(Preview(), *Col).foodDeficit > 0;
                        return FSlateColor(Short ? P().Alert : P().Text);
                    }) ] ]
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
                [ Pill(TEXT("<"), [this, Field] { AdjustWorkers(Field, -10); }, [this, Field] { return CanAdjustWorkers(Field, -10); }, ERole::Tertiary, TEXT("Move 10 workers to the labor pool")) ]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                // Click or drag across the blocks to staff exactly that many structures.
                [ SNew(BotfUi::SPickBar, [this, Field](int Blocks) {
                        RunGuarded([this, Field, Blocks] { SetWorkerBlocks(Field, Blocks); });
                    }).Height(14.f).Limit_Lambda(Count).Value_Lambda([Workers] { return (Workers() + 9) / 10; })
                    .FillColor(C(&FPalette::Primary)).EmptyColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Secondary, 0.25f)); }) ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)
                [ Pill(TEXT(">"), [this, Field] { AdjustWorkers(Field, 10); }, [this, Field] { return CanAdjustWorkers(Field, 10); }, ERole::Primary, TEXT("Staff another structure from the labor pool")) ] ],
            FMargin(10, 8));
    }
    TSharedRef<SWidget> BuildProductionView() {
        return Stack({
            Fill(EShape::Row, &FPalette::FieldHi, SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1)
                    [ Heading(Dyn([this] {
                        const auto* Col = DraftColony(); if (!Col) return FString();
                        return FString::Printf(TEXT("WORKFORCE GOVERNOR: %s"), *Str(botf::governorPriorityName(Col->workforcePriority)).ToUpper());
                    }), 11, &FPalette::Accent) ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [ SNew(STextBlock).Text(Dyn([this] {
                        const auto* Col = DraftColony(); if (!Col) return FString();
                        return Col->workforceManualOverride ? FString(TEXT("MANUAL OVERRIDE")) : FString();
                    })).Font(BotfUi::Condensed(10)).ColorAndOpacity(C(&FPalette::Alert)) ] ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                [ Line({
                    Option(TEXT("MANUAL"), [this] { SetWorkforceGovernor(botf::GovernorPriority::Manual); },
                        [this] { const auto* Col = DraftColony(); return Col && Col->workforcePriority == botf::GovernorPriority::Manual; }, TEXT("Full manual control")),
                    Option(TEXT("BALANCED"), [this] { SetWorkforceGovernor(botf::GovernorPriority::Balanced); },
                        [this] { const auto* Col = DraftColony(); return Col && Col->workforcePriority == botf::GovernorPriority::Balanced; }, TEXT("Auto-feed population; balance industry and research")),
                    Option(TEXT("GROWTH"), [this] { SetWorkforceGovernor(botf::GovernorPriority::Growth); },
                        [this] { const auto* Col = DraftColony(); return Col && Col->workforcePriority == botf::GovernorPriority::Growth; }, TEXT("Auto-feed population; maximize farms for population growth")),
                    Option(TEXT("RESEARCH"), [this] { SetWorkforceGovernor(botf::GovernorPriority::Research); },
                        [this] { const auto* Col = DraftColony(); return Col && Col->workforcePriority == botf::GovernorPriority::Research; }, TEXT("Auto-feed population; maximize laboratory workforce")),
                    Option(TEXT("MANUFACTURING"), [this] { SetWorkforceGovernor(botf::GovernorPriority::Manufacturing); },
                        [this] { const auto* Col = DraftColony(); return Col && Col->workforcePriority == botf::GovernorPriority::Manufacturing; }, TEXT("Auto-feed population; maximize factory workforce"))
                }) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
                [ SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] {
                    const auto* Col = DraftColony();
                    return (Col && Col->workforceManualOverride && Col->workforcePriority != botf::GovernorPriority::Manual) ? 1 : 0;
                })
                    + SWidgetSwitcher::Slot() [ SNullWidget::NullWidget ]
                    + SWidgetSwitcher::Slot()
                    [ SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                        [ Para(Txt(TEXT("Manual worker order active. Governor automated assignment is paused.")), 9, &FPalette::Alert) ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [ Pill(TEXT("RESUME GOVERNOR"), [this] { ClearGovernorOverride(); }, nullptr, ERole::Primary, TEXT("Clear manual override and let governor manage workforce")) ] ] ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
                [ SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] {
                    const auto* Col = DraftColony();
                    return (Col && !Col->governorWarning.empty()) ? 1 : 0;
                })
                    + SWidgetSwitcher::Slot() [ SNullWidget::NullWidget ]
                    + SWidgetSwitcher::Slot()
                    [ Para(Dyn([this] {
                        const auto* Col = DraftColony(); return Col ? Str(Col->governorWarning) : FString();
                    }), 9, &FPalette::Alert) ] ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
                [ Para(Dyn([this] {
                    const auto* Col = DraftColony();
                    if (!Col || Col->workforcePriority == botf::GovernorPriority::Manual) return FString();
                    return Str(Col->workforceExplanation);
                }), 9, &FPalette::Muted) ],
                FMargin(10, 8)),
            ProductionRow(0), ProductionRow(1), ProductionRow(2),
            Fill(EShape::Row, &FPalette::Field, SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                [ Heading(Dyn([this] {
                    const auto* Col = DraftColony(); if (!Col) return FString();
                    return FString::Printf(TEXT("LABOR POOL  /  %d UNASSIGNED"), Col->population - Col->foodWorkers - Col->industryWorkers - Col->researchWorkers);
                }), 11, &FPalette::Tertiary) ]
                + SVerticalBox::Slot().AutoHeight()
                [ SNew(BotfUi::SBlockMeter).Total_Lambda([this] {
                        const auto* Col = DraftColony(); return Col ? (Col->population - Col->foodWorkers - Col->industryWorkers - Col->researchWorkers + 9) / 10 : 0;
                    }).Lit_Lambda([this] {
                        const auto* Col = DraftColony(); return Col ? (Col->population - Col->foodWorkers - Col->industryWorkers - Col->researchWorkers + 9) / 10 : 0;
                    }).LitColor(C(&FPalette::Secondary)).DarkColor(C(&FPalette::Field)) ],
                FMargin(10, 8)),
            Line({ Pill(TEXT("BALANCED STAFFING"), [this] { Assign(false); }, [this] { return Owned(); }, ERole::Secondary, TEXT("Feed the population, split the rest between industry and research")),
                   Pill(TEXT("INDUSTRY STAFFING"), [this] { Assign(true); }, [this] { return Owned(); }, ERole::Secondary, TEXT("Feed the population, all remaining workers to industry")) }),
            Para(Txt(TEXT("Each block is one structure staffed by 10 workers. Red output means a shortage: people starve if food falls short.")), 9, &FPalette::Muted) });
    }
    TSharedRef<SWidget> BuildEnergyView() {
        return Stack({
            Fill(EShape::Row, &FPalette::FieldHi, SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                [ Heading(Dyn([this] {
                    const auto* Col = OwnedColony(); if (!Col) return FString();
                    const auto E = botf::economy(State, *Col);
                    return FString::Printf(TEXT("ENERGY IN USE %d OF %d"), E.energyUsed, Col->energy);
                }), 12, &FPalette::Tertiary) ]
                + SVerticalBox::Slot().AutoHeight()
                [ SNew(BotfUi::SSegmentBar).Segments(24).Height(12)
                    .Fraction_Lambda([this] { const auto* Col = OwnedColony(); return Col && Col->energy > 0 ? static_cast<float>(botf::economy(State, *Col).energyUsed) / Col->energy : 0.f; })
                    .FillColor(C(&FPalette::Secondary)).EmptyColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Secondary, 0.22f)); }) ],
                FMargin(10, 8)),
            Section(Txt(TEXT("POWERED STRUCTURES")), SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                    [ Para(Dyn([this] {
                        const auto* Col = OwnedColony(); if (!Col) return FString();
                        if (!Col->shipyards) return FString(TEXT("Shipyard: none built"));
                        return FString(TEXT("Shipyard: ")) + (botf::economy(State, *Col).shipyardPowered ? TEXT("POWERED — extends ship range and builds starships") : TEXT("UNPOWERED — no shipbuilding, no range support"));
                    }), 11, &FPalette::Text) ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [ Pill(TEXT("SWITCH ON / OFF"), [this] {
                        botf::Command Cmd; Cmd.id = State.lastAcceptedCommand + 1; Cmd.type = botf::CommandType::ToggleShipyard; Cmd.systemId = Selected;
                        Submit(Cmd); Status = TEXT("Shipyard power change ordered. Switching off pauses shipbuilding and reduces ship range.");
                    }, [this] { const auto* Col = OwnedColony(); return Col && Col->shipyards == 1; }, ERole::Secondary, TEXT("Takes effect at turn end; cancel with Cancel Last Order")) ] ]
                + SVerticalBox::Slot().AutoHeight()
                [ Para(Dyn([this] {
                    const auto* Col = OwnedColony(); if (!Col) return FString();
                    const auto E = botf::economy(State, *Col);
                    return FString::Printf(TEXT("Power plants %d (upkeep %d each)\nFusion plants %d (upkeep %d each)\nIdle for lack of power: %d factories, %d laboratories"),
                        Col->powerPlants, botf::definition(botf::Building::PowerPlant).creditUpkeep,
                        Col->fusionPlants, botf::definition(botf::Building::FusionPlant).creditUpkeep, E.inactiveFactories, E.inactiveLabs);
                }), 10, &FPalette::Text) ]),
            Unavailable(TEXT("Switching other individual structures on and off")) });
    }
    TSharedRef<SWidget> BuildChoiceView(const FString& Title, TSharedPtr<SVerticalBox>& Rows) {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1) [ Heading(Txt(Title), 13, &FPalette::Tertiary) ]
                + SHorizontalBox::Slot().AutoWidth() [ Label(Dyn([this] {
                    const auto* Col = DraftColony();
                    return Col ? FString::Printf(TEXT("Industry %d per turn"), botf::economy(Preview(), *Col).industry) : FString();
                }), 10, &FPalette::Muted) ] ]
            + SVerticalBox::Slot().FillHeight(1) [ Scroll(SAssignNew(Rows, SVerticalBox)) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
            [ Para(Txt(TEXT("Click an entry to add it to the build queue. Dimmed entries show why in their tooltip.")), 9, &FPalette::Muted) ];
    }
    void RefreshChoices() {
        if (!BuildRows.IsValid() || !ShipRows.IsValid()) return;
        BuildRows->ClearChildren(); ShipRows->ClearChildren();
        const auto& Draft = Preview();
        const auto* Col = botf::colonyAt(Draft, Selected);
        // Both the committed campaign and its draft must agree this is the player's colony before asking core.
        if (!OwnedColony() || !Col || Col->ownerId != State.playerEmpire) return;
        const int Output = Col ? botf::economy(Draft, *Col).industry : 0;
        int Buildings = 0, Ships = 0;
        for (const auto& Choice : botf::constructionChoices(Draft, Selected)) {
            FString Name = Str(Choice.name);
            Name.ReplaceInline(TEXT(" (remake infrastructure)"), TEXT(""));
            const FString Turns = Output > 0 ? FString::Printf(TEXT("%d turns"), std::max(1, (Choice.industryCost + Output - 1) / Output)) : FString(TEXT("no industry"));
            const FString Label = FString::Printf(TEXT("%s\nCost %d  |  %s  |  upkeep %d%s"), *Name.ToUpper(), Choice.industryCost, *Turns, Choice.creditUpkeep,
                Choice.blocker.empty() ? TEXT("") : TEXT("  |  UNAVAILABLE"));
            const FString Tip = Choice.blocker.empty() ? TEXT("Add to the build queue") : TEXT("Unavailable: ") + Str(Choice.blocker);
            auto Widget = Row(Label, [this, Choice] {
                botf::Command Cmd; Cmd.id = State.lastAcceptedCommand + 1;
                Cmd.type = botf::CommandType::EnqueueBuilding; Cmd.systemId = Selected;
                Cmd.building = Choice.product; Cmd.referenceId = Choice.referenceId;
                Submit(Cmd);
                Status = TEXT("Added ") + Str(Choice.name) + TEXT(" to the build queue.");
            }, nullptr, [this, Choice] { return OwnedColony() && Choice.blocker.empty(); }, Tip);
            if (botf::isShip(Choice.product)) { ShipRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3) [ Widget ]; ++Ships; }
            else { BuildRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3) [ Widget ]; ++Buildings; }
        }
        if (!Buildings) BuildRows->AddSlot().AutoHeight() [ Para(Txt(TEXT("Nothing can be built here right now.")), 10, &FPalette::Muted) ];
        if (!Ships) ShipRows->AddSlot().AutoHeight() [ Para(Txt(TEXT("No starships available. Build and power a shipyard first.")), 10, &FPalette::Muted) ];
    }
    // Buy / scrap eligibility comes from core validation itself: each order is dry-run against a copy of the
    // draft, so the blocker shown is core's exact reason. Cached per state version and system.
    // Fusion plants (schema 12) are their own structure: built from the Build List like any other, scrapped here.
    static constexpr std::size_t ScrapKindCount = 7;
    static constexpr std::array<botf::Building, ScrapKindCount> ScrapKinds{ botf::Building::Farm, botf::Building::Factory, botf::Building::Laboratory,
        botf::Building::PowerPlant, botf::Building::FusionPlant, botf::Building::Shipyard, botf::Building::DefenseBattery };
    struct FColonyOrderCheck { std::uint64_t Version = 0; int System = -1; FString Buy; std::array<FString, ScrapKindCount> Scrap; };
    mutable FColonyOrderCheck OrderCheck;
    FString DryRun(const botf::Command& Cmd) const { return Campaign.DryRun(Cmd); }
    const FColonyOrderCheck& ColonyChecks() const {
        if (OrderCheck.Version != StateVersion || OrderCheck.System != Selected) {
            OrderCheck.Version = StateVersion; OrderCheck.System = Selected;
            if (!Owned()) {
                OrderCheck.Buy = TEXT("Select one of your systems");
                OrderCheck.Scrap.fill(OrderCheck.Buy);
            } else {
                OrderCheck.Buy = DryRun(botf::makeBuyConstructionCommand(NextId(), Selected));
                for (std::size_t I = 0; I < ScrapKinds.size(); ++I)
                    OrderCheck.Scrap[I] = DryRun(botf::makeScrapStructureCommand(NextId(), Selected, ScrapKinds[I]));
            }
        }
        return OrderCheck;
    }
    int StructureCount(const botf::Colony& Col, botf::Building Kind) const {
        switch (Kind) {
        case botf::Building::Farm: return Col.farms;
        case botf::Building::Factory: return Col.factories;
        case botf::Building::Laboratory: return Col.laboratories;
        case botf::Building::PowerPlant: return Col.powerPlants;
        case botf::Building::FusionPlant: return Col.fusionPlants;
        case botf::Building::Shipyard: return Col.shipyards;
        case botf::Building::DefenseBattery: return Col.defenseBatteries;
        default: return 0;
        }
    }
    void BuyFirst() {
        const int Cost = botf::rushCost(Preview(), Selected);
        Submit(botf::makeBuyConstructionCommand(NextId(), Selected));
        Status = FString::Printf(TEXT("Purchase ordered for %d credits. The item completes when the turn ends; Cancel Last Order undoes it."), Cost);
    }
    void ScrapStructure(botf::Building Kind) {
        Submit(botf::makeScrapStructureCommand(NextId(), Selected, Kind));
        Status = FString::Printf(TEXT("Scrapping ordered: one %s. No credits are returned."), *Str(botf::definition(Kind).name).ToLower());
    }
    TSharedRef<SWidget> ScrapRow(std::size_t Index) {
        const botf::Building Kind = ScrapKinds[Index];
        return Fill(EShape::Row, &FPalette::FieldHi, SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
            [ SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [ Heading(Dyn([this, Kind] {
                    const auto* Col = DraftColony();
                    return FString::Printf(TEXT("%s  /  %d"), *Str(botf::definition(Kind).name).ToUpper(), Col ? StructureCount(*Col, Kind) : 0);
                }), 11, &FPalette::Tertiary) ]
                + SVerticalBox::Slot().AutoHeight()
                [ Para(Dyn([this, Index] { const auto& B = ColonyChecks().Scrap[Index]; return B.IsEmpty() ? FString(TEXT("Can be scrapped")) : B; }), 9, &FPalette::Muted) ] ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [ Pill(TEXT("SCRAP ONE"), [this, Kind] { ScrapStructure(Kind); }, [this, Index] { return ColonyChecks().Scrap[Index].IsEmpty(); },
                ERole::Alert, TEXT("Remove one structure at turn end. No credits are returned.")) ],
            FMargin(10, 6));
    }
    TSharedRef<SWidget> BuildStructuresView() {
        return Stack({
            Section(Txt(TEXT("CURRENT STRUCTURES")), Para(Dyn([this] {
                const auto* Col = OwnedColony(); if (!Col) return FString();
                return FString::Printf(TEXT("Farms %d\nFactories %d\nLaboratories %d\nPower plants %d\nFusion plants %d\nShipyards %d%s\nDefense batteries %d"),
                    Col->farms, Col->factories, Col->laboratories, Col->powerPlants, Col->fusionPlants, Col->shipyards,
                    Col->shipyards ? (Col->shipyardEnabled ? TEXT("  (switched on)") : TEXT("  (switched off)")) : TEXT(""), Col->defenseBatteries);
            }), 12, &FPalette::Text)),
            Section(Txt(TEXT("SCRAP STRUCTURES")), Stack({ ScrapRow(0), ScrapRow(1), ScrapRow(2), ScrapRow(3), ScrapRow(4), ScrapRow(5), ScrapRow(6),
                Para(Txt(TEXT("Scrapping returns no credits. Move workers out of a structure before scrapping it.")), 9, &FPalette::Muted) }, 4)),
            Unavailable(TEXT("Structure upgrades")) });
    }
    TSharedRef<SWidget> BuildQueueColumn() {
        return Section(Txt(TEXT("BUILD QUEUE")), SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ Fill(EShape::Row, &FPalette::FieldHi, SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                [ Heading(Dyn([this] {
                    const auto* Col = DraftColony(); if (!Col) return FString();
                    return FString::Printf(TEXT("QUEUE GOVERNOR: %s"), *Str(botf::governorPriorityName(Col->queuePriority)).ToUpper());
                }), 11, &FPalette::Accent) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                [ Line({
                    Option(TEXT("MANUAL"), [this] { SetQueueGovernor(botf::GovernorPriority::Manual); },
                        [this] { const auto* Col = DraftColony(); return Col && Col->queuePriority == botf::GovernorPriority::Manual; }, TEXT("Only build player-queued items")),
                    Option(TEXT("BALANCED"), [this] { SetQueueGovernor(botf::GovernorPriority::Balanced); },
                        [this] { const auto* Col = DraftColony(); return Col && Col->queuePriority == botf::GovernorPriority::Balanced; }, TEXT("Auto-queue infrastructure balancing factories and labs")),
                    Option(TEXT("GROWTH"), [this] { SetQueueGovernor(botf::GovernorPriority::Growth); },
                        [this] { const auto* Col = DraftColony(); return Col && Col->queuePriority == botf::GovernorPriority::Growth; }, TEXT("Auto-queue farms, factories, and colony ships")),
                    Option(TEXT("RESEARCH"), [this] { SetQueueGovernor(botf::GovernorPriority::Research); },
                        [this] { const auto* Col = DraftColony(); return Col && Col->queuePriority == botf::GovernorPriority::Research; }, TEXT("Auto-queue research institutes and laboratories")),
                    Option(TEXT("MANUFACTURING"), [this] { SetQueueGovernor(botf::GovernorPriority::Manufacturing); },
                        [this] { const auto* Col = DraftColony(); return Col && Col->queuePriority == botf::GovernorPriority::Manufacturing; }, TEXT("Auto-queue factories, power plants, and ships"))
                }) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
                [ Para(Dyn([this] {
                    const auto* Col = DraftColony();
                    if (!Col || Col->queuePriority == botf::GovernorPriority::Manual) return FString();
                    return Str(Col->queueExplanation);
                }), 9, &FPalette::Muted) ],
                FMargin(8, 6)) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ Para(Dyn([this] {
                const auto* Col = DraftColony(); if (!Col) return FString();
                if (Col->queue.empty()) return FString(TEXT("Idle: choose something from the Build or Ship list."));
                const auto Blocker = botf::constructionBlocker(Preview(), *Col);
                if (!Blocker.empty()) return TEXT("PAUSED: ") + Str(Blocker);
                const int Output = botf::economy(Preview(), *Col).industry;
                const int Remaining = std::max(0, ConstructionCost(Preview(), Selected, Col->queue.front()) - Col->queue.front().work - Col->overflow);
                return Output > 0 ? FString::Printf(TEXT("First item completes in %d turns"), std::max(1, (Remaining + Output - 1) / Output))
                    : FString(TEXT("Paused: no industry output"));
            }), 10, &FPalette::Text) ]
            + SVerticalBox::Slot().AutoHeight() [ SAssignNew(QueueRows, SVerticalBox) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 6)
            [ Fill(EShape::Row, &FPalette::FieldHi, SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                [ Para(Dyn([this] {
                    const auto& Blocker = ColonyChecks().Buy;
                    const int Cost = Owned() ? botf::rushCost(Preview(), Selected) : 0;
                    FString Text = Cost > 0 ? FString::Printf(TEXT("Buy first item: %d credits (treasury %lld)"), Cost, static_cast<long long>(Preview().credits))
                                            : FString(TEXT("Buy first item"));
                    return Blocker.IsEmpty() ? Text : Text + TEXT("\n") + Blocker;
                }), 9, &FPalette::Text) ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)
                [ Pill(TEXT("BUY"), [this] { BuyFirst(); }, [this] { return ColonyChecks().Buy.IsEmpty(); }, ERole::Accent,
                    TEXT("Pay credits to finish the first item in the queue when the turn ends")) ],
                FMargin(8, 5)) ]
            + SVerticalBox::Slot().AutoHeight()
            [ Line({
                Pill(TEXT("CANCEL FIRST"), [this] { CancelFirst(); }, [this] { return CanCancelFirst(); }, ERole::Alert,
                    TEXT("Cancel and discard the first project in queue (cannot be pinned)")),
                Pill(TEXT("PRIORITIZE LAST"), [this] { PrioritizeLast(); }, [this] { return CanPrioritizeLast(); }, ERole::Secondary,
                    TEXT("Move the last item directly to the top of queue (neither can be pinned)")),
                Pill(TEXT("CANCEL LAST ORDER"), [this] { CancelLast(); Status = TEXT("Last order cancelled."); },
                    [this] { return !State.pending.empty(); }, ERole::Tertiary, TEXT("Undo the most recent order of any kind"))
              }, 4) ]);
    }
    bool CanRemoveQueue(int Index) const {
        const auto* Col = DraftColony();
        if (!Col || static_cast<std::size_t>(Index) >= Col->queue.size()) return false;
        return !Col->queue[Index].pinned;
    }
    bool CanMoveQueue(int Index, int Destination) const {
        const auto* Col = DraftColony();
        if (!Col) return false;
        if (Index < 0 || static_cast<std::size_t>(Index) >= Col->queue.size()) return false;
        if (Destination < 0 || static_cast<std::size_t>(Destination) >= Col->queue.size()) return false;
        const int MinIdx = std::min(Index, Destination);
        const int MaxIdx = std::max(Index, Destination);
        for (int i = MinIdx; i <= MaxIdx; ++i) {
            if (Col->queue[i].pinned) return false;
        }
        return true;
    }
    bool CanCancelFirst() const {
        const auto* Col = DraftColony();
        return Col && !Col->queue.empty() && !Col->queue.front().pinned;
    }
    void CancelFirst() {
        if (!CanCancelFirst()) {
            Status = TEXT("Cannot cancel: first project is pinned (unpin first).");
            return;
        }
        Submit(botf::makeCancelFirstConstructionCommand(State.lastAcceptedCommand + 1, Selected));
        Status = TEXT("First construction project cancelled.");
        RefreshAll();
    }
    bool CanPrioritizeLast() const {
        const auto* Col = DraftColony();
        if (!Col || Col->queue.size() <= 1) return false;
        for (const auto& P : Col->queue) {
            if (P.pinned) return false;
        }
        return true;
    }
    void PrioritizeLast() {
        if (!CanPrioritizeLast()) {
            Status = TEXT("Cannot prioritize: queue item or target position is pinned (unpin first).");
            return;
        }
        Submit(botf::makePrioritizeLastConstructionCommand(State.lastAcceptedCommand + 1, Selected));
        Status = TEXT("Last construction project prioritized.");
        RefreshAll();
    }
    void RefreshQueue() {
        if (!QueueRows.IsValid()) return;
        QueueRows->ClearChildren();
        if (!Owned()) return;
        const auto Draft = botf::preview(State);
        const auto& C = *botf::colonyAt(Draft, Selected);
        for (int Index = 0; Index < static_cast<int>(C.queue.size()); ++Index) {
            const auto& Project = C.queue[Index];
            const int Cost = ConstructionCost(Draft, Selected, Project);
            const bool bIsPinned = Project.pinned;
            QueueRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
                [ Fill(EShape::Row, Index == 0 ? &FPalette::FieldHi : &FPalette::Field, SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [ Para(Txt(FString::Printf(TEXT("%d. %s  %d/%d%s"), Index + 1, *ConstructionName(Draft, Selected, Project), Project.work, Cost, bIsPinned ? TEXT("  [PINNED]") : TEXT(""))),
                        9, Index == 0 ? &FPalette::Accent : (bIsPinned ? &FPalette::Secondary : &FPalette::Text)) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
                    [ SNew(BotfUi::SSegmentBar).Segments(16).Height(5)
                        .Fraction(Cost > 0 ? static_cast<float>(Project.work) / Cost : 0.f)
                        .FillColor(this->C(&FPalette::Primary)).EmptyColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Secondary, 0.22f)); }) ]
                    + SVerticalBox::Slot().AutoHeight()
                    [ SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 3, 0)
                        [ Pill(bIsPinned ? TEXT("UNPIN") : TEXT("PIN"), [this, Index, bIsPinned] { ToggleQueuePin(Index, !bIsPinned); },
                            nullptr, bIsPinned ? ERole::Accent : ERole::Tertiary,
                            bIsPinned ? TEXT("Unpin item to allow moving, reordering, or removing") : TEXT("Pin to protect from governor mutations and accidental changes")) ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 3, 0)
                        [ Pill(TEXT("UP"), [this, Index] { EditQueue(Index, Index - 1, false); },
                            [this, Index] { return CanMoveQueue(Index, Index - 1); }, ERole::Secondary,
                            bIsPinned ? TEXT("Cannot move pinned item (unpin first)") : TEXT("Move up in construction queue")) ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 3, 0)
                        [ Pill(TEXT("DOWN"), [this, Index] { EditQueue(Index, Index + 1, false); },
                            [this, Index] { return CanMoveQueue(Index, Index + 1); }, ERole::Secondary,
                            bIsPinned ? TEXT("Cannot move pinned item (unpin first)") : TEXT("Move down in construction queue")) ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [ Pill(TEXT("REMOVE"), [this, Index] { EditQueue(Index, 0, true); },
                            [this, Index] { return CanRemoveQueue(Index); }, ERole::Alert,
                            bIsPinned ? TEXT("Cannot remove pinned item (unpin first)") : TEXT("Invested work on this item is lost")) ] ],
                    FMargin(8, 5)) ];
        }
    }

    // ---- Megaproject (scientific victory): start or cancel at the selected colony ----
    botf::view::MegaprojectView MegaprojectHere() const { return botf::view::megaprojectView(Preview(), State.playerEmpire, Selected); }
    bool MegaprojectStartDrafted() const {
        return std::any_of(State.pending.begin(), State.pending.end(), [](const botf::Command& O) { return O.type == botf::CommandType::StartMegaproject; });
    }
    void StartMegaproject() {
        if (!Owned()) throw std::runtime_error("Select one of your systems");
        Submit(botf::makeStartMegaprojectCommand(NextId(), Selected));
        Status = TEXT("The ") + Str(botf::megaprojectName(State)) + TEXT(" is ordered here. Its credits are paid now; work begins when the turn ends.");
    }
    // A project started this turn is withdrawn (its credits return with the order); one under way is abandoned.
    void CancelMegaproject() {
        if (MegaprojectStartDrafted()) {
            ReplaceOrders([](const botf::Command& O) { return O.type == botf::CommandType::StartMegaproject; }, std::nullopt);
            Status = TEXT("Megaproject order withdrawn.");
            return;
        }
        Submit(botf::makeCancelMegaprojectCommand(NextId()));
        Status = TEXT("The megaproject will be abandoned when the turn ends. Its invested industry is lost.");
    }
    TSharedRef<SWidget> BuildMegaprojectSection() {
        return SNew(SBox).Visibility_Lambda([this] { return State.rules.alternativeVictories ? EVisibility::Visible : EVisibility::Collapsed; })
            [ Section(Dyn([this] { return TEXT("MEGAPROJECT  /  ") + Str(botf::megaprojectName(State)).ToUpper(); }), SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                [ Para(Dyn([this] { return Str(botf::view::megaprojectText(Preview(), MegaprojectHere())); }), 9, &FPalette::Text) ]
                + SVerticalBox::Slot().AutoHeight()
                [ Line({
                    Pill(TEXT("START HERE"), [this] { StartMegaproject(); }, [this] { return !Session.active && Owned() && MegaprojectHere().startBlocker.empty(); },
                        ERole::Accent, TEXT("Begin the megaproject at this colony; completing it wins a scientific victory")),
                    Pill(TEXT("CANCEL PROJECT"), [this] { CancelMegaproject(); }, [this] { return !Session.active && MegaprojectHere().underWay; },
                        ERole::Alert, TEXT("Abandon the megaproject (no refund once work has begun)")) }, 4) ]) ];
    }

    // ---- Build-queue templates: define from the queue, apply, set the default for new colonies, delete ----
    std::vector<botf::BuildTemplate> OwnTemplates() const { return botf::empireBuildTemplates(Preview(), State.playerEmpire); }
    FString TemplateFallbackName() const {
        return Owned() ? SystemName(Selected) + TEXT(" queue") : FString::Printf(TEXT("Template %d"), static_cast<int>(OwnTemplates().size()) + 1);
    }
    std::string TemplateNameFor(const FString& Fallback) const {
        return botf::view::templateName(std::string(TCHAR_TO_UTF8(*TemplateNameText)), std::string(TCHAR_TO_UTF8(*Fallback)));
    }
    std::vector<botf::Construction> QueueAsTemplate() const {
        const auto* Col = DraftColony();
        if (!Col || Col->queue.empty()) throw std::runtime_error("The build queue is empty: queue the projects to save first");
        return botf::view::templateItemsFromQueue(*Col);
    }
    void SaveQueueAsTemplate() {
        const auto Items = QueueAsTemplate();
        Submit(botf::makeDefineBuildTemplateCommand(NextId(), 0, TemplateNameFor(TemplateFallbackName()), Items));
        const auto Templates = OwnTemplates();
        SelectedTemplate = Templates.empty() ? 0 : Templates.back().id;
        Status = TEXT("Build template saved. Apply it to any colony, or make it the default for new colonies.");
    }
    void UpdateTemplateFromQueue(int TemplateId) {
        const auto* Existing = botf::buildTemplateById(Preview(), TemplateId);
        if (!Existing) throw std::runtime_error("Build template not found");
        const auto Items = QueueAsTemplate();
        Submit(botf::makeDefineBuildTemplateCommand(NextId(), TemplateId, TemplateNameFor(Str(Existing->name)), Items));
        Status = TEXT("Build template replaced with this colony's queue.");
    }
    void ApplyTemplate(int TemplateId) {
        if (!Owned()) throw std::runtime_error("Select one of your systems");
        Submit(botf::makeApplyBuildTemplateCommand(NextId(), TemplateId, Selected));
        const auto* Col = DraftColony();
        Status = Col && !Col->queueExplanation.empty() ? Str(Col->queueExplanation) : FString(TEXT("Build template applied to the queue."));
    }
    void SetDefaultTemplate(int TemplateId) {
        Submit(botf::makeSetDefaultBuildTemplateCommand(NextId(), TemplateId));
        Status = TemplateId ? TEXT("Default template set: colonies you found, win or capture with an empty queue adopt it.")
                            : TEXT("No default template: new colonies start with an empty queue.");
    }
    void DeleteTemplate(int TemplateId) {
        Submit(botf::makeDeleteBuildTemplateCommand(NextId(), TemplateId));
        if (SelectedTemplate == TemplateId) SelectedTemplate = 0;
        Status = TEXT("Build template deleted.");
    }
    TSharedRef<SWidget> BuildTemplateSection() {
        return Section(Txt(TEXT("BUILD TEMPLATES")), SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ SAssignNew(TemplateNameBox, SEditableTextBox).HintText(Txt(TEXT("Template name (optional)")))
                .OnTextChanged_Lambda([this](const FText& Text) { TemplateNameText = Text.ToString(); }) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ Pill(TEXT("SAVE QUEUE AS TEMPLATE"), [this] { SaveQueueAsTemplate(); },
                [this] { const auto* Col = DraftColony(); return !Session.active && Col && !Col->queue.empty(); }, ERole::Primary,
                TEXT("Save this colony's build queue (up to 32 projects) as a named template")) ]
            + SVerticalBox::Slot().AutoHeight() [ SAssignNew(TemplateRows, SVerticalBox) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
            [ Para(Txt(TEXT("Applying appends the projects in order; any that are illegal at the time are skipped with the reason shown in the queue governor note.")), 8, &FPalette::Muted) ]);
    }
    void RefreshTemplates() {
        if (!TemplateRows.IsValid()) return;
        TemplateRows->ClearChildren();
        const auto Templates = OwnTemplates();
        for (const auto& Tmpl : Templates) {
            const int Id = Tmpl.id;
            const bool bDefault = Tmpl.autoApply;
            TemplateRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
                [ Fill(EShape::Row, SelectedTemplate == Id ? &FPalette::FieldHi : &FPalette::Field, SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 3) [ Para(Txt(Str(botf::view::templateSummary(Tmpl))), 9, bDefault ? &FPalette::Accent : &FPalette::Text) ]
                    + SVerticalBox::Slot().AutoHeight()
                    [ Grid({
                        Pill(TEXT("APPLY"), [this, Id] { ApplyTemplate(Id); }, [this] { return !Session.active && Owned(); }, ERole::Primary,
                            TEXT("Append these projects to this colony's queue")),
                        Pill(bDefault ? TEXT("UNSET DEFAULT") : TEXT("DEFAULT"), [this, Id, bDefault] { SetDefaultTemplate(bDefault ? 0 : Id); },
                            [this] { return !Session.active; }, ERole::Secondary, TEXT("Colonies newly acquired with an empty queue adopt the default template")),
                        Pill(TEXT("UPDATE"), [this, Id] { UpdateTemplateFromQueue(Id); },
                            [this] { const auto* Col = DraftColony(); return !Session.active && Col && !Col->queue.empty(); }, ERole::Tertiary,
                            TEXT("Replace the template with this colony's queue (and the typed name, if any)")),
                        Pill(TEXT("DELETE"), [this, Id] { DeleteTemplate(Id); }, [this] { return !Session.active; }, ERole::Alert, TEXT("Delete this template")) }, 2) ],
                    FMargin(8, 5)) ];
        }
        if (Templates.empty())
            TemplateRows->AddSlot().AutoHeight() [ Para(Txt(TEXT("No templates yet. Queue projects here, then save the queue as a template.")), 9, &FPalette::Muted) ];
    }

    // ======================= RESEARCH =======================
    void SyncAllocation() {
        if (bAllocationDirty) return;
        for (int Field = 0; Field < botf::ResearchFields; ++Field) DraftAllocation[Field] = Preview().technologies[Field].allocation;
    }
    void AdjustAllocation(int Field, int Delta) {
        if (AllocationLocked[Field]) return;
        int LockedTotal = 0;
        std::vector<int> Others;
        for (int I = 0; I < botf::ResearchFields; ++I) {
            if (I == Field) continue;
            if (AllocationLocked[I]) LockedTotal += DraftAllocation[I]; else Others.push_back(I);
        }
        if (Others.empty()) return;
        const int Old = DraftAllocation[Field];
        const int New = FMath::Clamp(Old + Delta, 0, 100 - LockedTotal);
        int Need = Old - New;   // amount the other unlocked fields must absorb
        DraftAllocation[Field] = New;
        for (int Guard = 0; Need != 0 && Guard < 400; ++Guard) {
            bool Moved = false;
            for (int I : Others) {
                if (Need > 0 && DraftAllocation[I] < 100) { ++DraftAllocation[I]; --Need; Moved = true; }
                else if (Need < 0 && DraftAllocation[I] > 0) { --DraftAllocation[I]; ++Need; Moved = true; }
                if (Need == 0) break;
            }
            if (!Moved) break;
        }
        DraftAllocation[Field] += Need;
        bAllocationDirty = true;
    }
    void ApplyAllocation() {
        botf::Command Cmd; Cmd.id = State.lastAcceptedCommand + 1; Cmd.type = botf::CommandType::AllocateResearch;
        for (int Field = 0; Field < botf::ResearchFields; ++Field) Cmd.researchAllocation[Field] = DraftAllocation[Field];
        Submit(Cmd);
        bAllocationDirty = false;
        Status = TEXT("Research allocation set. It applies from the next turn.");
    }
    TSharedRef<SWidget> ResearchTile(int Field) {
        return SNew(SBorder).BorderImage_Lambda([this] { return BotfUi::Styles().Get(ThemeEmpire(), EShape::Row); })
            .BorderBackgroundColor_Lambda([this, Field] { return FSlateColor(SelectedResearchField == Field ? P().FieldHi : P().Field); })
            .Padding(FMargin(10, 8))
            [ SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1) [ Heading(Txt(Str(botf::researchName(Field)).ToUpper()), 13, &FPalette::Tertiary) ]
                    + SHorizontalBox::Slot().AutoWidth() [ Label(Dyn([this, Field] { return FString::Printf(TEXT("LEVEL %d"), State.technologies[Field].level); }), 12, &FPalette::Accent, true) ] ]
                + SVerticalBox::Slot().AutoHeight()
                [ Para(Dyn([this, Field] {
                    const auto& T = State.technologies[Field];
                    return T.level == botf::MaxTechLevel ? FString(TEXT("All levels complete"))
                        : TEXT("Researching: ") + Str(botf::technologyName(State, Field, T.level + 1))
                            + FString::Printf(TEXT("\n%lld / %d points"), static_cast<long long>(T.progress), botf::researchCost(State, Field, T.level + 1));
                }), 9, &FPalette::Text) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
                [ SNew(BotfUi::SSegmentBar).Segments(20).Height(7)
                    .Fraction_Lambda([this, Field] {
                        const auto& T = State.technologies[Field];
                        if (T.level == botf::MaxTechLevel) return 1.f;
                        const int Cost = botf::researchCost(State, Field, T.level + 1);
                        return Cost > 0 ? static_cast<float>(T.progress) / Cost : 0.f;
                    })
                    .FillColor(C(&FPalette::Accent)).EmptyColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Secondary, 0.22f)); }) ]
                + SVerticalBox::Slot().AutoHeight()
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ Pill(TEXT("<"), [this, Field] { AdjustAllocation(Field, -5); },
                        [this, Field] { return !AllocationLocked[Field] && DraftAllocation[Field] > 0; },
                        ERole::Tertiary, TEXT("Allocate 5% less (disabled when field is locked or at 0%)")) ]
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(4, 0)
                    // Click or drag to set the share directly (5% steps); arrows fine-tune; locked fields cannot move.
                    // The inbox is rebuilt as the allocation becomes dirty, so the reminder appears at once.
                    [ SNew(BotfUi::SPickBar, [this, Field](int Step) {
                        const bool WasDirty = bAllocationDirty;
                        AdjustAllocation(Field, Step * 5 - DraftAllocation[Field]);
                        if (WasDirty != bAllocationDirty) RefreshInbox();
                      })
                        .Steps(20).Height(12.f).Value_Lambda([this, Field] { return (DraftAllocation[Field] + 2) / 5; })
                        .Locked_Lambda([this, Field] { return AllocationLocked[Field]; })
                        .FillColor(C(&FPalette::Secondary)).EmptyColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Secondary, 0.18f)); }) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ Pill(TEXT(">"), [this, Field] { AdjustAllocation(Field, 5); },
                        [this, Field] { return !AllocationLocked[Field] && DraftAllocation[Field] < 100; },
                        ERole::Primary, TEXT("Allocate 5% more (disabled when field is locked or at 100%)")) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 4, 0)
                    [ SNew(SBox).WidthOverride(38) [ Heading(Dyn([this, Field] { return FString::Printf(TEXT("%d%%"), DraftAllocation[Field]); }), 12, &FPalette::Text) ] ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ Control(TEXT("LOCK"), [this, Field] { AllocationLocked[Field] = !AllocationLocked[Field]; }, nullptr,
                        [this, Field] { return AllocationLocked[Field]; }, ERole::Secondary, FMargin(6, 5), false, 8, TEXT("Locked fields keep their share when others change")) ] ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                [ Line({ Pill(TEXT("FOCUS"), [this, Field] { Allocate(Field); }, nullptr, ERole::Primary, TEXT("Order 50% here and 10% to every other field")),
                         Option(TEXT("DETAILS"), [this, Field] { SelectedResearchField = Field; }, [this, Field] { return SelectedResearchField == Field; }) }) ] ];
    }
    TSharedRef<SWidget> BuildResearchScreen() {
        return Console(Dyn([this] { return ResearchMode == 0 ? FString(TEXT("TECHNOLOGY  /  RESEARCH")) : FString(TEXT("TECHNOLOGY  /  OBJECT DATABASE")); }),
            SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return ResearchMode; })
            + SWidgetSwitcher::Slot()
            [ Scroll(Stack({
                Grid({ ResearchTile(0), ResearchTile(1), ResearchTile(2), ResearchTile(3), ResearchTile(4), ResearchTile(5) }, 3),
                SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)
                    [ Control(TEXT("APPLY ALLOCATION"), [this] { ApplyAllocation(); }, [this] { return bAllocationDirty; }, nullptr, ERole::Accent, FMargin(14, 7), false, 10,
                        TEXT("Apply draft research allocation to empire orders (enabled when changes have been made)")) ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)
                    [ Pill(TEXT("DISCARD CHANGES"), [this] { bAllocationDirty = false; SyncAllocation(); }, [this] { return bAllocationDirty; }, ERole::Tertiary,
                        TEXT("Revert draft allocation back to active orders (enabled when changes have been made)")) ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
                    [ Pill(TEXT("EVEN SPLIT"), [this] { Allocate(-1); }, nullptr, ERole::Secondary, TEXT("Order an even split across all six fields")) ]
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                    [ Para(Dyn([this] { return bAllocationDirty ? FString(TEXT("Allocation changed; apply to order it.")) : FString(TEXT("Allocation matches your orders.")); }), 9, &FPalette::Muted) ],
                Section(Dyn([this] { return Str(botf::researchName(SelectedResearchField)).ToUpper() + TEXT("  /  FIELD DETAIL"); }),
                    Para(Dyn([this] { return ResearchFieldDetail(); }), 10, &FPalette::Text)) })) ]
            + SWidgetSwitcher::Slot() [ BuildDatabaseView() ],
            14, SheetHeader({ ModeButton(TEXT("FIELDS"), &ResearchMode, 0, TEXT("Allocation and progress")),
                              ModeButton(TEXT("OBJECT DATABASE"), &ResearchMode, 1, TEXT("Technologies, ships and structures")) }));
    }
    FString ResearchFieldDetail() const {
        const int Field = SelectedResearchField;
        const auto& T = State.technologies[Field];
        FString DomainEffect;
        FString CurrentBenefit;
        switch (Field) {
        case 0: // Biotech
            DomainEffect = TEXT("Food production (+5% per level ladder bonus)");
            CurrentBenefit = FString::Printf(TEXT("+%d%% food yield"), T.level * 5);
            break;
        case 1: // Computers
            DomainEffect = TEXT("Research output (+5% per level ladder bonus) and intelligence calculations");
            CurrentBenefit = FString::Printf(TEXT("+%d%% research output, Level %d intel rating"), T.level * 5, T.level);
            break;
        case 2: // Construction
            DomainEffect = TEXT("Colony industry output (+5% per level ladder bonus)");
            CurrentBenefit = FString::Printf(TEXT("+%d%% industry yield"), T.level * 5);
            break;
        case 3: // Energy
            DomainEffect = TEXT("Colony power generation (+5% per level ladder bonus)");
            CurrentBenefit = FString::Printf(TEXT("+%d%% energy generation"), T.level * 5);
            break;
        case 4: // Propulsion
            DomainEffect = TEXT("Fleet operational range (+1 sector route range per 2 tech levels)");
            CurrentBenefit = FString::Printf(TEXT("+%d sector(s) operational range"), T.level / 2);
            break;
        case 5: // Weapons
            DomainEffect = TEXT("Tactical fleet combat attack (+1 per level) and ground invasion strength (+2 per level)");
            CurrentBenefit = FString::Printf(TEXT("+%d fleet combat attack, +%d ground invasion strength"), T.level, T.level * 2);
            break;
        default:
            DomainEffect = TEXT("Domain technological progress");
            CurrentBenefit = FString::Printf(TEXT("Level %d"), T.level);
            break;
        }
        FString Detail = FString::Printf(TEXT("FIELD: %s (Level %d / %d)\nDOMAIN EFFECT: %s\nCURRENT BENEFIT: %s\n"),
            *Str(botf::researchName(Field)).ToUpper(), T.level, botf::MaxTechLevel, *DomainEffect, *CurrentBenefit);
        if (T.level < botf::MaxTechLevel) {
            const int NextLevel = T.level + 1;
            Detail += FString::Printf(TEXT("\nNEXT ADVANCE: Level %d - %s\nProgress: %lld / %d points\n"),
                NextLevel, *Str(botf::technologyName(State, Field, NextLevel)),
                static_cast<long long>(T.progress), botf::researchCost(State, Field, NextLevel));
            if (State.ruleset == botf::Ruleset::Reference) {
                const FString Desc = ReferenceTechnologyDescription(Field, NextLevel);
                if (!Desc.IsEmpty()) Detail += Desc + TEXT("\n");
            }
            Detail += FString::Printf(TEXT("\nVERIFIED UNLOCKS AT %s LEVEL %d:\n"), *Str(botf::researchName(Field)).ToUpper(), NextLevel);
            int UnlockCount = 0;
            const int PlayerRace = State.playerEmpire - 1;
            for (const auto& B : botf::referenceData::BuildingCatalogue) {
                if (B.technologyField == Field && B.technologyLevel == NextLevel) {
                    if (PlayerRace >= 0 && (B.excludedEmpireMask & (1 << PlayerRace))) continue;
                    Detail += FString::Printf(TEXT("  * Structure: %s (Industry %d, Energy %d)\n"), *Str(B.name), B.industryCost, B.energyCost);
                    ++UnlockCount;
                }
            }
            for (const auto& S : botf::referenceData::ShipCatalogue) {
                if (PlayerRace >= 0 && S.raceId == PlayerRace && S.technologyRequirements[Field] == NextLevel) {
                    FString Prereqs;
                    for (int F = 0; F < botf::ResearchFields; ++F) {
                        if (F == Field) continue;
                        if (S.technologyRequirements[F] > 0) {
                            if (!Prereqs.IsEmpty()) Prereqs += TEXT(", ");
                            Prereqs += FString::Printf(TEXT("%s %d"), *Str(botf::researchName(F)), S.technologyRequirements[F]);
                        }
                    }
                    if (Prereqs.IsEmpty()) {
                        Detail += FString::Printf(TEXT("  * Ship: %s (%s, Industry %d)\n"), *Str(S.name), *Str(S.shipClass), S.industryCost);
                    } else {
                        Detail += FString::Printf(TEXT("  * Ship: %s (%s, Industry %d, additional prerequisites: %s)\n"),
                            *Str(S.name), *Str(S.shipClass), S.industryCost, *Prereqs);
                    }
                    ++UnlockCount;
                }
            }
            if (UnlockCount == 0) {
                if (Field == 4) {
                    Detail += TEXT("  * Theoretical foundation advancement; increases fleet operational range (+1 sector per 2 levels)\n");
                } else if (Field == 5) {
                    Detail += TEXT("  * Theoretical foundation advancement; increases combat attack power (+1 fleet attack, +2 ground invasion)\n");
                } else {
                    Detail += TEXT("  * Theoretical foundation advancement; increases domain efficiency (+5%)\n");
                }
            }
        } else {
            Detail += TEXT("\nAll research levels complete for this domain. Maximum tech level reached.\n");
        }
        return Detail;
    }
    FString ReferenceTechnologyDescription(int Field, int Level) const {
        for (const auto& Entry : botf::referenceData::TechnologyCatalogue)
            if (Entry.field == Field && Entry.level == Level) return Str(Entry.description);
        return FString();
    }
    void Allocate(int Focus) {
        botf::Command C; C.id = State.lastAcceptedCommand + 1; C.type = botf::CommandType::AllocateResearch;
        for (int Field = 0; Field < botf::ResearchFields; ++Field)
            C.researchAllocation[Field] = Focus < 0 ? (Field < 4 ? 17 : 16) : (Field == Focus ? 50 : 10);
        Submit(C); bAllocationDirty = false;
        Status = TEXT("Research allocation set. Cancel Last Order undoes it before the turn ends.");
    }
    TSharedRef<SWidget> BuildDatabaseView() {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0) [ Option(TEXT("TECHNOLOGIES"), [this] { SetReferenceCategory(0); }, [this] { return ReferenceCategory == 0; }) ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0) [ Option(TEXT("SHIPS"), [this] { SetReferenceCategory(1); }, [this] { return ReferenceCategory == 1; }) ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0) [ Option(TEXT("STRUCTURES"), [this] { SetReferenceCategory(2); }, [this] { return ReferenceCategory == 2; }) ]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                [ SAssignNew(ReferenceSearchBox, SEditableTextBox).HintText(Txt(TEXT("Search")))
                    .OnTextChanged_Lambda([this](const FText& Text) {
                        ReferenceSearch = Text.ToString(); ReferencePage = 0;
                        const auto Matches = ReferenceMatches(); ReferenceSelected = Matches.empty() ? -1 : Matches.front(); RefreshReference();
                    }) ] ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0) [ Pill(TEXT("PREVIOUS"), [this] { ShiftReferencePage(-1); }, [this] { return ReferencePage > 0; }, ERole::Tertiary) ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0) [ Pill(TEXT("NEXT"), [this] { ShiftReferencePage(1); }, [this] { return (ReferencePage + 1) * 12 < static_cast<int>(ReferenceMatches().size()); }, ERole::Tertiary) ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [ Label(Dyn([this] { return FString::Printf(TEXT("Page %d of %d  |  %d entries"), ReferencePage + 1,
                    std::max(1, (static_cast<int>(ReferenceMatches().size()) + 11) / 12), static_cast<int>(ReferenceMatches().size())); }), 10, &FPalette::Text) ] ]
            + SVerticalBox::Slot().FillHeight(1)
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
                [ SNew(SBox).WidthOverride(290) [ Scroll(SAssignNew(ReferenceRows, SVerticalBox)) ] ]
                + SHorizontalBox::Slot().FillWidth(1)
                [ Scroll(Section(Txt(TEXT("ENTRY")), Para(Dyn([this] { return ReferenceDetails(); }), 11, &FPalette::Text))) ] ];
    }
    int ReferenceCount() const {
        return ReferenceCategory == 0 ? static_cast<int>(botf::referenceData::TechnologyCatalogue.size())
            : ReferenceCategory == 1 ? static_cast<int>(botf::view::shipDatabaseIds().size())
            : static_cast<int>(botf::referenceData::BuildingCatalogue.size());
    }
    // Cached by category and search text (the catalogues are static): page labels ask for it every frame.
    mutable int MatchesCategory = -1;
    mutable FString MatchesSearch;
    mutable std::vector<int> MatchesCache;
    const std::vector<int>& ReferenceMatches() const {
        if (MatchesCategory != ReferenceCategory || !MatchesSearch.Equals(ReferenceSearch, ESearchCase::CaseSensitive)) {
            MatchesCache.clear();
            for (int Index = 0; Index < ReferenceCount(); ++Index)
                if (ReferenceSearch.IsEmpty() || ReferenceItemLabel(Index).Contains(ReferenceSearch, ESearchCase::IgnoreCase)) MatchesCache.push_back(Index);
            MatchesCategory = ReferenceCategory;
            MatchesSearch = ReferenceSearch;
        }
        return MatchesCache;
    }
    void SetReferenceCategory(int Category) {
        ReferenceCategory = Category; ReferencePage = 0;
        const auto Matches = ReferenceMatches(); ReferenceSelected = Matches.empty() ? -1 : Matches.front(); RefreshReference();
    }
    void ShiftReferencePage(int Direction) {
        const auto Matches = ReferenceMatches();
        ReferencePage = std::clamp(ReferencePage + Direction, 0, std::max(0, (static_cast<int>(Matches.size()) - 1) / 12));
        ReferenceSelected = Matches.empty() ? -1 : Matches[ReferencePage * 12]; RefreshReference();
    }
    FString SourceRaceName(int Id) const {
        if (Id >= 0 && Id < static_cast<int>(botf::referenceData::RaceCatalogue.size()))
            return Str(botf::referenceData::RaceCatalogue[Id].name);
        return FString::Printf(TEXT("Race %d"), Id);
    }
    FString SourceFieldName(int Field) const {
        if (Field >= 0 && Field < botf::ResearchFields) return Str(botf::researchName(Field));
        if (Field == 6) return TEXT("Sociology");
        return FString::Printf(TEXT("Field %d"), Field);
    }
    FString ReferenceItemLabel(int Index) const {
        if (ReferenceCategory == 0) {
            const auto& T = botf::referenceData::TechnologyCatalogue[Index];
            return FString::Printf(TEXT("%s  /  L%d"), *Str(T.name), T.level);
        }
        if (ReferenceCategory == 1) {
            // Legacy catalogue then expanded roster (core findShip); see BotfShipDatabase.h.
            return Str(botf::view::shipDatabaseLabel(botf::view::shipDatabaseIds()[static_cast<std::size_t>(Index)]));
        }
        const auto& B = botf::referenceData::BuildingCatalogue[Index];
        return Str(B.name);
    }
    void RefreshReference() {
        if (!ReferenceRows.IsValid()) return;
        ReferenceRows->ClearChildren();
        const auto Matches = ReferenceMatches();
        for (int Position = ReferencePage * 12; Position < std::min(static_cast<int>(Matches.size()), (ReferencePage + 1) * 12); ++Position) {
            const int Index = Matches[Position];
            ReferenceRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
                [ Row(ReferenceItemLabel(Index), [this, Index] { ReferenceSelected = Index; }, [this, Index] { return ReferenceSelected == Index; }) ];
        }
        if (Matches.empty()) ReferenceRows->AddSlot().AutoHeight()
            [ Para(Txt(TEXT("No matching entries.")), 11, &FPalette::Muted) ];
    }
    FString ReferenceDetails() const {
        if (ReferenceSelected < 0 || ReferenceSelected >= ReferenceCount()) return TEXT("Select an entry.");
        if (ReferenceCategory == 0) {
            const auto& T = botf::referenceData::TechnologyCatalogue[ReferenceSelected];
            return FString::Printf(TEXT("%s\n%s, level %d  |  cost %lld\n\n%s"),
                *Str(T.name), *SourceFieldName(T.field), T.level, static_cast<long long>(T.cost), *Str(T.description));
        }
        if (ReferenceCategory == 1) {
            std::vector<std::string> FieldNames;
            for (int Field = 0; Field < botf::ResearchFields; ++Field) FieldNames.emplace_back(botf::researchName(Field));
            return Str(botf::view::shipDatabaseDetails(botf::view::shipDatabaseIds()[static_cast<std::size_t>(ReferenceSelected)], FieldNames));
        }
        const auto& B = botf::referenceData::BuildingCatalogue[ReferenceSelected];
        return FString::Printf(TEXT("%s\n\nIndustry %d  |  energy %d  |  population %d\nRequires %s level %d\n\n%s\n\nOnly structures listed in a system's Build List can currently be built."),
            *Str(B.name), B.industryCost, B.energyCost, B.populationCost,
            *SourceFieldName(B.technologyField), B.technologyLevel, *Str(B.description));
    }

    // ======================= DIPLOMACY =======================
    bool DipMinor() const { return SelectedMinor != 0 && botf::minorAt(State, SelectedMinor); }
    // Diplomacy sheet: known races on the left; the selected race's dossier, proposals and treaties on the right.
    TSharedRef<SWidget> BuildDiplomacyScreen() {
        return Console(Txt(TEXT("DIPLOMACY")), SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
            [ SNew(SBox).WidthOverride(290)
                [ Section(Txt(TEXT("KNOWN RACES")), SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight() [ SNew(SBox).MaxDesiredHeight(560) [ Scroll(SAssignNew(RaceRows, SVerticalBox)) ] ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                    [ Pill(TEXT("CANCEL LAST ORDER"), [this] { CancelLast(); Status = TEXT("Last order cancelled."); },
                        [this] { return !Session.active && !State.pending.empty(); }, ERole::Tertiary, TEXT("Undo the most recent order of any kind")) ]) ] ]
            + SHorizontalBox::Slot().FillWidth(1) [ BuildDossier() ],
            14, SheetHeader({ ModeButton(TEXT("EVENTS"), &DiplomacyMode, 0, TEXT("Proposals in progress")),
                              ModeButton(TEXT("ACTIVE"), &DiplomacyMode, 1, TEXT("Current treaties and trade routes")),
                              ModeButton(TEXT("PROPOSE"), &DiplomacyMode, 2, TEXT("Make an offer to the selected race")),
                              ModeButton(TEXT("RACE INFO"), &DiplomacyMode, 3, TEXT("What you know about the selected race")) }));
    }
    TSharedRef<SWidget> BuildDossier() {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [ Fill(EShape::Row, &FPalette::Field, SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
                [ SNew(SBox).WidthOverride(64).HeightOverride(64)
                    [ SNew(SBorder).BorderImage_Lambda([this] { return BotfUi::Styles().Get(DipMinor() ? ThemeEmpire() : SelectedEmpire, EShape::Pill); })
                        .BorderBackgroundColor_Lambda([this] { return FSlateColor(DipMinor() ? FLinearColor(0.8f, 0.8f, 0.9f) : BotfUi::FactionColor(SelectedEmpire)); }) ] ]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                [ SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight() [ Heading(Dyn([this] { return DipName().ToUpper(); }), 18, &FPalette::Accent) ]
                    + SVerticalBox::Slot().AutoHeight() [ Para(Dyn([this] { return DipStatusLine(); }), 10, &FPalette::Text) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                    [ SNew(BotfUi::SSegmentBar).Segments(25).Height(8)
                        .Fraction_Lambda([this] { return DipAttitude() / 100.f; })
                        .FillColor(C(&FPalette::Good)).EmptyColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Secondary, 0.22f)); }) ] ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12, 0, 0, 0) [ SNew(SBox).WidthOverride(260) [ Unavailable(TEXT("Race portraits and message tone")) ] ],
                FMargin(12, 8)) ]
            + SVerticalBox::Slot().FillHeight(1)
            [ SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return DiplomacyMode; })
                + SWidgetSwitcher::Slot() [ Scroll(SAssignNew(ProposalRows, SVerticalBox)) ]
                + SWidgetSwitcher::Slot() [ Scroll(Para(Dyn([this] { return TreatiesText(); }), 11, &FPalette::Text)) ]
                + SWidgetSwitcher::Slot() [ Scroll(BuildProposeView()) ]
                + SWidgetSwitcher::Slot() [ Scroll(Para(Dyn([this] { return RaceInfoText(); }), 11, &FPalette::Text)) ] ];
    }
    FString DipName() const {
        if (DipMinor()) return Str(botf::minorAt(State, SelectedMinor)->name);
        return Str(botf::empireName(SelectedEmpire));
    }
    FString ParticipantName(botf::diplomacy::ParticipantId Id) const {
        if (!Id.isMinor) return Str(botf::empireName(Id.id));
        const auto* M = botf::minorAt(State, Id.id);
        return M ? Str(M->name) : SystemName(Id.id);
    }
    void RefreshProposals() {
        if (!ProposalRows.IsValid()) return;
        ProposalRows->ClearChildren();
        int Count = 0;
        // Calls to arms: an ally was attacked and expects the player to join the war before the call falls due.
        // Joining is a declaration of war on the aggressor; refusing lets the call lapse (core applies the penalty then).
        for (const auto& Call : OpenCallsToArms()) {
            ++Count;
            const auto Key = std::make_tuple(Call.ally, Call.defender, Call.aggressor, Call.issuedTurn);
            const int Aggressor = Call.aggressor;
            ProposalRows->AddSlot().AutoHeight().Padding(0, 0, 0, 7)
            [ Section(Txt(TEXT("CALL TO ARMS")), SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5) [ Para(Txt(Str(botf::view::callToArmsText(State, Call))), 10, &FPalette::Text) ]
                + SVerticalBox::Slot().AutoHeight() [ Grid({
                    Pill(TEXT("JOIN THE WAR"), [this, Aggressor] {
                        Draft(botf::CommandType::DeclareWar, Aggressor);
                        Status = FString::Printf(TEXT("War on %s ordered: the alliance is honoured when the turn ends."), *Str(botf::empireName(Aggressor)));
                    }, [this, Aggressor] {
                        const auto* R = botf::majorRelation(State, State.playerEmpire, Aggressor);
                        return !Session.active && R && R->contact && !R->atWar && !WarDrafted(Aggressor) && !botf::empireEliminated(State, Aggressor);
                    }, ERole::Alert, TEXT("Declare war on the aggressor (+10 trust with your ally)")),
                    Pill(TEXT("REFUSE"), [this, Key] {
                        RefusedCalls.insert(Key);
                        Status = TEXT("You will not answer the call. It lapses when due: -20 trust and 2 grievances with your ally.");
                    }, nullptr, ERole::Tertiary, TEXT("Let the call lapse")) }, 2) ]) ];
        }
        for (const auto& P : botf::pendingProposals(State, State.playerEmpire)) {
            ++Count;
            const bool Incoming = P.target == botf::diplomacy::ParticipantId::major(State.playerEmpire);
            const FString Description = Str(botf::view::proposalSummary(State, P, State.playerEmpire)) + TEXT("\n")
                + (Incoming ? Str(botf::view::deadlineText(State, P.dueTurn)) + TEXT(". Unanswered proposals are ignored when the window closes.")
                            : FString::Printf(TEXT("Their answer is due by turn %d."), P.dueTurn));
            ProposalRows->AddSlot().AutoHeight().Padding(0, 0, 0, 7)
            [ Section(Txt(Incoming ? TEXT("INCOMING PROPOSAL") : TEXT("OUTGOING PROPOSAL")), SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5) [ Para(Txt(Description), 10, &FPalette::Text) ]
                + SVerticalBox::Slot().AutoHeight() [ Incoming ? Grid({
                    Pill(TEXT("ACCEPT"), [this, Id = P.id] { Submit(botf::makeRespondProposalCommand(NextId(), Id, true)); Status = TEXT("Response ordered for turn end."); },
                        [this, Id = P.id] { return !Session.active && !ProposalResponsePending(Id); }, ERole::Primary),
                    Pill(TEXT("REJECT"), [this, Id = P.id] { Submit(botf::makeRespondProposalCommand(NextId(), Id, false)); Status = TEXT("Rejection ordered for turn end."); },
                        [this, Id = P.id] { return !Session.active && !ProposalResponsePending(Id); }, ERole::Tertiary) }, 2)
                    : Para(Txt(TEXT("Awaiting their answer.")), 9, &FPalette::Muted) ] ) ];
        }
        if (!Count) ProposalRows->AddSlot().AutoHeight() [ Para(Txt(TEXT("No modern proposals in progress.")), 10, &FPalette::Muted) ];
        ProposalRows->AddSlot().AutoHeight().Padding(0, 12, 0, 0) [ Para(Dyn([this] { return DiplomacyEventsText(); }), 10, &FPalette::Muted) ];
    }
    bool ProposalResponsePending(std::uint64_t Id) const {
        return std::any_of(State.pending.begin(), State.pending.end(), [Id](const botf::Command& C) {
            return C.type == botf::CommandType::RespondDiplomaticProposal && C.proposalId == Id;
        });
    }
    int DipAttitude() const {
        if (DipMinor()) return botf::minorAt(State, SelectedMinor)->attitude[State.playerEmpire];
        const auto* R = botf::majorRelation(State, State.playerEmpire, SelectedEmpire);
        return R ? R->trust : 0;
    }
    FString DipStatusLine() const {
        if (DipMinor()) {
            const auto* M = botf::minorAt(State, SelectedMinor);
            return FString::Printf(TEXT("Minor race of %s  |  relations %d / 100  |  %s  |  %s\n%s"), *Str(botf::view::minorProfile(State, *M).homeworld),
                M->attitude[State.playerEmpire],
                M->memberOf ? *(FString(TEXT("member of ")) + Str(botf::empireName(M->memberOf))) : TEXT("independent"),
                M->friendshipTurn[State.playerEmpire] >= 0 ? TEXT("friendship treaty") : TEXT("no treaty"), *MinorRowDetail(*M));
        }
        const auto* R = botf::majorRelation(State, State.playerEmpire, SelectedEmpire);
        if (!R || !R->contact) return TEXT("No contact. Explore until your sensors meet them.");
        if (botf::empireEliminated(State, SelectedEmpire)) return TEXT("Major power  |  ELIMINATED: no colonies remain; no treaties, war or espionage are possible.");
        return FString::Printf(TEXT("Major power  |  %s  |  trust %d / 100  |  trade treaty %s%s"),
            R->atWar ? TEXT("AT WAR") : TEXT("at peace"), R->trust, R->tradeTreaty ? TEXT("active") : TEXT("none"),
            R->atWar ? *FString::Printf(TEXT("  |  war since turn %d"), R->warStartTurn) : TEXT(""));
    }
    FString DiplomacyEventsText() const {
        FString Text;
        int History = 0;
        for (auto It = botf::campaignDiplomacyState(State).eventHistory.rbegin();
             It != botf::campaignDiplomacyState(State).eventHistory.rend() && History < 30; ++It) {
            if (It->partyA != botf::diplomacy::ParticipantId::major(State.playerEmpire)
                && It->partyB != botf::diplomacy::ParticipantId::major(State.playerEmpire)) continue;
            if (It->type == botf::diplomacy::DiplomaticEventType::ProposalSubmitted) continue;
            if (!History++) Text += TEXT("RECENT DIPLOMATIC OUTCOMES\n");
            Text += FString::Printf(TEXT("Turn %d  |  %s  |  %s\n"), It->turn,
                UTF8_TO_TCHAR(botf::diplomacy::eventTypeName(It->type)), UTF8_TO_TCHAR(It->description.c_str()));
        }
        if (History) Text += TEXT("\n");
        for (const auto& Offer : State.pendingMajorOffers)
            if (Offer.proposer == State.playerEmpire || Offer.target == State.playerEmpire)
                Text += FString::Printf(TEXT("%s %s proposal %s %s — answer due turn %d\n"),
                    Offer.proposer == State.playerEmpire ? TEXT("Your") : *Str(botf::empireName(Offer.proposer)),
                    Offer.type == botf::CommandType::OfferPeace ? TEXT("peace") : TEXT("trade treaty"),
                    Offer.proposer == State.playerEmpire ? TEXT("to") : TEXT("for"),
                    Offer.proposer == State.playerEmpire ? *Str(botf::empireName(Offer.target)) : TEXT("you"), Offer.dueTurn);
        for (const auto& O : State.diplomacy) if (O.empireId == State.playerEmpire)
            Text += FString::Printf(TEXT("Your %s to %s arrives turn %d\n"), O.type == botf::CommandType::SendGift ? TEXT("gift") : TEXT("proposal"),
                *SystemName(O.systemId), O.dueTurn);
        for (const auto& Order : State.pending)
            if (Order.type == botf::CommandType::SendGift || Order.type == botf::CommandType::OfferFriendship || Order.type == botf::CommandType::OfferMembership
                || Order.type == botf::CommandType::DeclareWar || Order.type == botf::CommandType::OfferPeace || Order.type == botf::CommandType::ProposeTradeTreaty)
                Text += TEXT("Order waiting for turn end: ") + DiplomaticOrderName(Order) + TEXT("\n");
        return Text.IsEmpty() ? FString(TEXT("No proposals in progress.")) : Text;
    }
    FString DiplomaticOrderName(const botf::Command& Order) const {
        switch (Order.type) {
        case botf::CommandType::SendGift: return TEXT("gift to ") + SystemName(Order.systemId);
        case botf::CommandType::OfferFriendship: return TEXT("friendship with ") + SystemName(Order.systemId);
        case botf::CommandType::OfferMembership: return TEXT("membership for ") + SystemName(Order.systemId);
        case botf::CommandType::DeclareWar: return TEXT("declare war on ") + Str(botf::empireName(Order.targetEmpire));
        case botf::CommandType::OfferPeace: return TEXT("peace with ") + Str(botf::empireName(Order.targetEmpire));
        default: return TEXT("trade treaty with ") + Str(botf::empireName(Order.targetEmpire));
        }
    }
    FString TreatiesText() const {
        FString Text = TEXT("ACTIVE TREATIES");
        int Count = 0;
        for (const auto& T : botf::activeTreaties(State, State.playerEmpire)) {
            ++Count;
            Text += FString::Printf(TEXT("\n%s with %s  |  %s"), UTF8_TO_TCHAR(botf::diplomacy::treatyTypeName(T.type)),
                *ParticipantName(T.getCounterparty(botf::diplomacy::ParticipantId::major(State.playerEmpire))),
                T.turnsRemaining < 0 ? TEXT("indefinite") : *FString::Printf(TEXT("%d turns remaining"), T.turnsRemaining));
        }
        if (!Count) Text += TEXT("\nNone");
        const auto Ended = botf::view::treatyHistoryLines(State, State.playerEmpire, 8);
        if (!Ended.empty()) {
            Text += TEXT("\n\nENDED TREATIES");
            for (const auto& Line_ : Ended) Text += TEXT("\n") + Str(Line_);
        }
        const std::string Victory = botf::view::diplomaticVictoryText(State, State.playerEmpire);
        if (!Victory.empty()) Text += TEXT("\n\n") + Str(Victory);
        Text += TEXT("\n\n") + RelationshipsText() + TEXT("\n") + TradeRoutesText();
        return Text;
    }
    FString OfferTermsText() const {
        return FString::Printf(TEXT("%s  |  %s  |  %s tone\nOffer %lld credits  |  demand %lld credits"),
            UTF8_TO_TCHAR(botf::diplomacy::treatyTypeName(OfferTreaty)), UTF8_TO_TCHAR(botf::diplomacy::treatyDurationName(OfferDuration)),
            UTF8_TO_TCHAR(botf::diplomacy::toneName(OfferTone)), static_cast<long long>(OfferCredits), static_cast<long long>(DemandCredits));
    }
    bool CanSendModernOffer() const {
        const int Target = DipMinor() ? SelectedMinor : SelectedEmpire;
        if (Session.active || Target == 0 || OfferCredits > State.credits) return false;
        if (std::any_of(State.pending.begin(), State.pending.end(), [this, Target](const botf::Command& C) {
            return C.type == botf::CommandType::SubmitDiplomaticProposal && C.proposalTargetIsMinor == !!DipMinor()
                && (DipMinor() ? C.targetSystemId : C.targetEmpire) == Target;
        })) return false;
        return botf::canProposeTreaty(State, State.playerEmpire, Target, !!DipMinor(), OfferTreaty);
    }
    TSharedRef<SWidget> BuildModernOffer() {
        return Section(Txt(TEXT("NEGOTIATE TREATY")), Stack({
            Para(Dyn([this] { return OfferTermsText(); }), 10, &FPalette::Text),
            Grid({
                Pill(TEXT("TREATY TYPE"), [this] {
                    static const botf::diplomacy::TreatyType Types[] = { botf::diplomacy::TreatyType::NonAggression,
                        botf::diplomacy::TreatyType::Friendship, botf::diplomacy::TreatyType::Affiliation,
                        botf::diplomacy::TreatyType::Alliance, botf::diplomacy::TreatyType::Membership,
                        botf::diplomacy::TreatyType::Peace, botf::diplomacy::TreatyType::Trade };
                    auto It = std::find(std::begin(Types), std::end(Types), OfferTreaty);
                    OfferTreaty = It == std::end(Types) || std::next(It) == std::end(Types) ? Types[0] : *std::next(It);
                }, nullptr, ERole::Secondary),
                Pill(TEXT("DURATION"), [this] { using D = botf::diplomacy::TreatyDuration;
                    OfferDuration = OfferDuration == D::Turns25 ? D::Turns50 : OfferDuration == D::Turns50 ? D::Turns100
                        : OfferDuration == D::Turns100 ? D::Indefinite : D::Turns25; }, nullptr, ERole::Secondary),
                Pill(TEXT("TONE"), [this] { using T = botf::diplomacy::DiplomaticTone;
                    OfferTone = static_cast<T>((static_cast<int>(OfferTone) + 1) % 4); }, nullptr, ERole::Secondary) }, 3),
            Grid({
                Pill(TEXT("OFFER +50"), [this] { OfferCredits = std::min<std::int64_t>(State.credits, OfferCredits + 50); },
                    [this] { return OfferCredits < State.credits; }, ERole::Secondary),
                Pill(TEXT("OFFER -50"), [this] { OfferCredits = std::max<std::int64_t>(0, OfferCredits - 50); },
                    [this] { return OfferCredits > 0; }, ERole::Tertiary),
                Pill(TEXT("DEMAND +50"), [this] { DemandCredits += 50; }, [this] { return DemandCredits < 1000000; }, ERole::Secondary),
                Pill(TEXT("DEMAND -50"), [this] { DemandCredits = std::max<std::int64_t>(0, DemandCredits - 50); },
                    [this] { return DemandCredits > 0; }, ERole::Tertiary) }, 4),
            Para(Dyn([this] { std::string Why; const int Target = DipMinor() ? SelectedMinor : SelectedEmpire;
                if (botf::canProposeTreaty(State, State.playerEmpire, Target, !!DipMinor(), OfferTreaty, &Why)) return FString(TEXT("Terms are ready to send."));
                return Str(Why); }), 9, &FPalette::Muted),
            Pill(TEXT("SEND PROPOSAL"), [this] {
                Submit(botf::makeProposeTreatyCommand(NextId(), DipMinor() ? SelectedMinor : SelectedEmpire, !!DipMinor(),
                    OfferTreaty, OfferDuration, OfferTone, OfferCredits, DemandCredits));
                Status = TEXT("Treaty proposal ordered for turn end.");
            }, [this] { return CanSendModernOffer(); }, ERole::Primary)
        }));
    }
    FString RaceInfoText() const {
        if (DipMinor()) {
            const auto* M = botf::minorAt(State, SelectedMinor);
            // Roster profile (MinorRaces.h): homeworld, temperament, attitude, technology, population, join bonus, description.
            const auto Profile = botf::view::minorProfile(State, *M);
            FString Text = Str(botf::view::minorProfileText(Profile)) + TEXT("\n");
            if (M->memberOf == State.playerEmpire) Text += TEXT("\nA member of your empire: its join bonus applies to every colony you hold.\n");
            Text += TEXT("\nFriendship needs relations of 50. Membership needs relations of 80 and a friendship signed in an earlier turn. Each gift adds 20 when delivered. Rival offers favour the empire with better relations.");
            if (Profile.managed)
                Text += TEXT(" Every five turns an independent race grows, advances its technology and drifts toward its natural view of each empire; its temperament shapes the loyalty of a member.");
            return Text;
        }
        const auto* R = botf::majorRelation(State, State.playerEmpire, SelectedEmpire);
        if (!R || !R->contact) return TEXT("No contact.");
        FString Text = DipStatusLine();
        const auto Memory = botf::view::relationMemoryLines(State, State.playerEmpire, SelectedEmpire);
        if (!Memory.empty()) {
            Text += TEXT("\n\nRELATIONS MEMORY");
            for (const auto& Line_ : Memory) Text += TEXT("\n") + Str(Line_);
        }
        Text += TEXT("\n\n") + MajorOfferText();
        for (auto It = State.intelReports.rbegin(); It != State.intelReports.rend(); ++It)
            if (It->viewer == State.playerEmpire && It->target == SelectedEmpire && It->success) {
                Text += FString::Printf(TEXT("\n\nLatest intelligence (turn %d): %d known colonies, about %lld credits."), It->turn, It->knownColonies, static_cast<long long>(It->estimatedCredits));
                break;
            }
        return Text;
    }
    TSharedRef<SWidget> BuildProposeView() {
        return SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return DipMinor() ? 1 : 0; })
            + SWidgetSwitcher::Slot()
            [ Stack({
                Section(Txt(TEXT("TREATIES")), Grid({
                    Pill(TEXT("DECLARE WAR"), [this] {
                        const bool Betrayal = botf::treatyForbidsAttack(State, State.playerEmpire, SelectedEmpire);
                        Draft(botf::CommandType::DeclareWar, SelectedEmpire);
                        if (Betrayal) Status = TEXT("War ordered. A treaty binds you: this is a betrayal every power will remember.");
                    }, [this] {
                        const auto* R = botf::majorRelation(State, State.playerEmpire, SelectedEmpire);
                        return R && R->contact && !R->atWar && !botf::empireEliminated(State, SelectedEmpire);
                    }, ERole::Alert, TEXT("Required before attacking their ships or systems. Breaking a treaty is a betrayal.")),
                    Pill(TEXT("PROPOSE PEACE"), [this] { Draft(botf::CommandType::OfferPeace, SelectedEmpire); }, [this] {
                        const auto* R = botf::majorRelation(State, State.playerEmpire, SelectedEmpire);
                        return R && R->contact && R->atWar && !HasPendingMajorOffer(botf::CommandType::OfferPeace);
                    }, ERole::Primary),
                    Pill(TEXT("PROPOSE TRADE TREATY"), [this] { Draft(botf::CommandType::ProposeTradeTreaty, SelectedEmpire); }, [this] {
                        const auto* R = botf::majorRelation(State, State.playerEmpire, SelectedEmpire);
                        return State.advancedRulesEnabled && R && R->contact && !R->atWar && !R->tradeTreaty
                            && !HasPendingMajorOffer(botf::CommandType::ProposeTradeTreaty);
                    }, ERole::Primary, TEXT("They answer after a delay")) }, 3)),
                BuildModernOffer(),
                Section(Txt(TEXT("TRADE ROUTE")), SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                    [ Para(Dyn([this] { return FString::Printf(TEXT("From %s  to  %s"),
                        TradeSource ? *SystemName(TradeSource) : TEXT("(choose your system)"),
                        TradeDestination ? *SystemName(TradeDestination) : TEXT("(choose their system)")); }), 11, &FPalette::Text) ]
                    + SVerticalBox::Slot().AutoHeight()
                    [ Grid({
                        Pill(TEXT("NEXT SOURCE"), [this] { CycleTradeSource(); }, nullptr, ERole::Secondary, TEXT("Cycle your systems")),
                        Pill(TEXT("NEXT DESTINATION"), [this] { CycleTradeDestination(); }, nullptr, ERole::Secondary, TEXT("Cycle their visible systems")),
                        Pill(TEXT("ESTABLISH ROUTE"), [this] { Draft(botf::CommandType::EstablishTradeRoute, SelectedEmpire, TradeSource, TradeDestination); }, [this] {
                            const auto* R = botf::majorRelation(State, State.playerEmpire, SelectedEmpire);
                            return TradeSource && TradeDestination && R && R->contact && !R->atWar && (!State.advancedRulesEnabled || R->tradeTreaty);
                        }, ERole::Primary, TEXT("Needs peace and, in classic games, a trade treaty")),
                        Pill(TEXT("CANCEL SOURCE ROUTE"), [this] { Draft(botf::CommandType::CancelTradeRoute, 0, TradeSource); }, [this] { return TradeSource != 0; }, ERole::Tertiary) }, 2) ]),
                Para(Dyn([this] { return MajorOfferText(); }), 9, &FPalette::Muted) }) ]
            + SWidgetSwitcher::Slot()
            [ Stack({
                BuildModernOffer(),
                Section(Txt(TEXT("OFFERS")), Grid({
                    MinorAction(botf::CommandType::OfferFriendship, TEXT("PROPOSE FRIENDSHIP")),
                    MinorAction(botf::CommandType::SendGift, TEXT("SEND GIFT (50 CREDITS)")),
                    MinorAction(botf::CommandType::OfferMembership, TEXT("OFFER MEMBERSHIP")) }, 3)),
                Para(Txt(TEXT("Only one offer to a race can be in progress at a time. Gifts are paid when the turn ends and arrive two turns later.")), 9, &FPalette::Muted) }) ];
    }
    bool MinorWaiting(int System) const {
        return std::any_of(State.diplomacy.begin(), State.diplomacy.end(), [&](const botf::DiplomaticOffer& O) {
            return O.systemId == System && O.empireId == State.playerEmpire;
        }) || std::any_of(State.pending.begin(), State.pending.end(), [System](const botf::Command& Order) {
            return Order.systemId == System && (Order.type == botf::CommandType::SendGift || Order.type == botf::CommandType::OfferFriendship || Order.type == botf::CommandType::OfferMembership);
        });
    }
    TSharedRef<SWidget> MinorAction(botf::CommandType Type, const FString& Label) {
        return Pill(Label, [this, Type] {
            botf::Command Cmd; Cmd.id = State.lastAcceptedCommand + 1; Cmd.systemId = SelectedMinor; Cmd.type = Type;
            Submit(Cmd);
            Status = Type == botf::CommandType::SendGift ? TEXT("Gift ordered: 50 credits are paid at turn end and it arrives two turns later.")
                : TEXT("Proposal ordered. It is delivered at turn end; their attitude decides the answer.");
        }, [this, Type] { return CanMinorAction(Type); },
            Type == botf::CommandType::SendGift ? ERole::Tertiary : ERole::Primary);
    }
    bool CanMinorAction(botf::CommandType Type) const {
        const auto* M = botf::minorAt(State, SelectedMinor);
        return !Session.active && M && !M->memberOf && M->contact[State.playerEmpire] && !MinorWaiting(SelectedMinor)
            && (Type != botf::CommandType::SendGift || Preview().credits >= 50);
    }
    void RefreshRaces() {
        if (!RaceRows.IsValid()) return;
        RaceRows->ClearChildren();
        int Count = 0;
        for (int Id = 1; Id <= 5; ++Id) {
            if (Id == State.playerEmpire) continue;
            const auto* R = botf::majorRelation(State, State.playerEmpire, Id);
            if (!R || !R->contact) continue;
            ++Count;
            RaceRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0) [ SNew(SBox).WidthOverride(5) [ Swatch(Id, 0) ] ]
                    + SHorizontalBox::Slot().FillWidth(1)
                    [ Row(RaceRowLabel(Id, *R),
                        [this, Id] { SelectedEmpire = Id; SelectedMinor = 0; }, [this, Id] { return !DipMinor() && SelectedEmpire == Id; }) ] ];
        }
        for (const auto& M : State.minors) {
            if (!M.contact[State.playerEmpire]) continue;
            ++Count;
            RaceRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
                [ Row(FString::Printf(TEXT("%s\n%s  |  relations %d%s\n%s"), *Str(M.name).ToUpper(),
                    M.memberOf ? *(FString(TEXT("member of ")) + Str(botf::empireName(M.memberOf))) : TEXT("independent"),
                    M.attitude[State.playerEmpire], M.friendshipTurn[State.playerEmpire] >= 0 ? TEXT("  |  friendship") : TEXT(""),
                    *MinorRowDetail(M)),
                    [this, Sys = M.systemId] { SelectedMinor = Sys; }, [this, Sys = M.systemId] { return SelectedMinor == Sys; }) ];
        }
        if (!Count) RaceRows->AddSlot().AutoHeight() [ Para(Txt(TEXT("No races contacted yet. Explore to meet them.")), 10, &FPalette::Muted) ];
    }
    // "Scientific  |  +15% research  |  tech 8, pop 80" for the known-races list.
    FString MinorRowDetail(const botf::Minor& M) const {
        const auto Profile = botf::view::minorProfile(State, M);
        FString Text = Profile.temperament.empty() ? FString() : Str(Profile.temperament) + TEXT("  |  ");
        Text += Str(Profile.joinBonus);
        if (Profile.managed) Text += FString::Printf(TEXT("  |  tech %d, pop %d"), Profile.techLevel, Profile.population);
        return Text;
    }
    FString RaceRowLabel(int Id, const botf::MajorRelation& R) const {
        if (botf::empireEliminated(State, Id)) return Str(botf::empireName(Id)).ToUpper() + TEXT("\nELIMINATED");
        const auto Allies = botf::alliedEmpires(State, State.playerEmpire);
        const auto Sensors = botf::sensorSharingEmpires(State, State.playerEmpire);
        const bool Ally = std::find(Allies.begin(), Allies.end(), Id) != Allies.end();
        const bool Shared = std::find(Sensors.begin(), Sensors.end(), Id) != Sensors.end();
        return FString::Printf(TEXT("%s\n%s  |  trust %d  |  trade treaty %s%s"), *Str(botf::empireName(Id)).ToUpper(),
            R.atWar ? TEXT("AT WAR") : TEXT("peace"), R.trust, R.tradeTreaty ? TEXT("yes") : TEXT("no"),
            Ally ? TEXT("  |  ALLY") : Shared ? TEXT("  |  shared sensors") : TEXT(""));
    }
    bool HasPendingMajorOffer(botf::CommandType Type) const {
        for (const auto& Offer : State.pendingMajorOffers)
            if (Offer.type == Type && ((Offer.proposer == State.playerEmpire && Offer.target == SelectedEmpire)
                || (Offer.proposer == SelectedEmpire && Offer.target == State.playerEmpire))) return true;
        for (const auto& DraftOrder : State.pending)
            if (DraftOrder.targetEmpire == SelectedEmpire && DraftOrder.type == Type) return true;
        return false;
    }
    FString MajorOfferText() const {
        if (!State.advancedRulesEnabled) return TEXT("In this galaxy, treaties take effect immediately and trade routes create their own treaty.");
        return TEXT("Proposals are answered after a delay. A trade treaty needs trust of 40; peace needs trust of 20 and at least two turns of war, or enough fleet strength. Trade routes need a trade treaty.");
    }
    FString TradeRoutesText() const {
        FString Result = TEXT("YOUR TRADE ROUTES");
        int Count = 0;
        for (const auto& T : State.tradeRoutes) {
            if (T.ownerId != State.playerEmpire) continue;
            ++Count;
            Result += TEXT("\n") + SystemName(T.fromSystemId) + TEXT(" to ") + SystemName(T.toSystemId);
        }
        if (!Count) Result += TEXT("\nNone");
        return Result;
    }
    // Owner of a foreign colony the player can see and has met (a possible route destination), else 0.
    int RouteTargetOwner(int SystemId) const {
        const auto* Col = SystemId ? botf::colonyAt(State, SystemId) : nullptr;
        if (!Col || Col->ownerId == State.playerEmpire || DiplomacyTargetAt(SystemId) != Col->ownerId) return 0;
        return Col->ownerId;
    }
    FString RouteBlocker(int Destination) const {
        if (Session.active) return TEXT("Finish the battle in progress first.");
        if (!TradeSource) return TEXT("Select one of your systems and choose SET AS SOURCE.");
        const int Owner = RouteTargetOwner(Destination);
        if (!Owner) return TEXT("Select a rival colony in sensor range as the destination.");
        const auto* R = botf::majorRelation(State, State.playerEmpire, Owner);
        if (!R || R->atWar) return TEXT("No routes while at war.");
        if (State.advancedRulesEnabled && !R->tradeTreaty) return TEXT("Needs a trade treaty first (Diplomacy, Propose).");
        return FString();
    }
    void EstablishRouteTo(int Destination) {
        const FString Blocker = RouteBlocker(Destination);
        if (!Blocker.IsEmpty()) throw std::runtime_error(TCHAR_TO_UTF8(*Blocker));
        Draft(botf::CommandType::EstablishTradeRoute, RouteTargetOwner(Destination), TradeSource, Destination);
        Status = FString::Printf(TEXT("Trade route ordered: %s to %s. It opens when you end the turn."), *SystemName(TradeSource), *SystemName(Destination));
    }
    TSharedRef<SWidget> BuildTradeRoutePanel() {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4) [ Heading(Txt(TEXT("TRADE ROUTES")), 10, &FPalette::Tertiary) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
            [ Para(Dyn([this] {
                FString Text = TEXT("Source: ") + (TradeSource ? SystemName(TradeSource) : FString(TEXT("none")));
                if (RouteTargetOwner(Selected)) Text += TEXT("\nDestination: ") + SystemName(Selected);
                const FString Blocker = RouteBlocker(Selected);
                if (!Blocker.IsEmpty()) Text += TEXT("\n") + Blocker;
                return Text;
            }), 9, &FPalette::Text) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
            [ Grid({
                Pill(TEXT("SET AS SOURCE"), [this] { TradeSource = Selected; }, [this] { return Owned(); }, ERole::Secondary,
                    TEXT("Use the selected system of yours as the route's origin")),
                Pill(TEXT("ROUTE TO HERE"), [this] { EstablishRouteTo(Selected); }, [this] { return RouteBlocker(Selected).IsEmpty(); }, ERole::Primary,
                    TEXT("Open a route from the source to the selected rival colony at turn end")),
                Pill(TEXT("CANCEL SOURCE'S ROUTE"), [this] { Draft(botf::CommandType::CancelTradeRoute, 0, TradeSource); },
                    [this] { return TradeSource != 0 && !Session.active && std::any_of(State.tradeRoutes.begin(), State.tradeRoutes.end(),
                        [this](const botf::TradeRoute& T) { return T.ownerId == State.playerEmpire && T.fromSystemId == TradeSource; }); },
                    ERole::Tertiary, TEXT("Close the route leaving the source system at turn end")) }, 2) ]
            + SVerticalBox::Slot().AutoHeight()
            [ Para(Dyn([this] {
                FString Text = TradeRoutesText();
                if (bLastForecast || LastTradeIncome) Text += FString::Printf(TEXT("\nLast turn's trade revenue: %d"), LastTradeIncome);
                if (const auto Income = IncomeForecast(); Income.Trade) Text += FString::Printf(TEXT("\nExpected this turn: %d"), Income.Trade);
                return Text;
            }), 9, &FPalette::Muted) ];
    }
    void CycleTradeSource() {
        std::vector<int> Ids;
        for (const auto& C : State.colonies) if (C.ownerId == State.playerEmpire) Ids.push_back(C.systemId);
        if (Ids.empty()) { TradeSource = 0; return; }
        const auto It = std::find(Ids.begin(), Ids.end(), TradeSource);
        TradeSource = It == Ids.end() || std::next(It) == Ids.end() ? Ids.front() : *std::next(It);
    }
    void CycleTradeDestination() {
        std::vector<int> Ids;
        for (const auto& C : State.colonies) {
            if (C.ownerId != SelectedEmpire) continue;
            const auto S = std::find_if(State.systems.begin(), State.systems.end(), [&](const botf::System& X) { return X.id == C.systemId; });
            if (S != State.systems.end() && botf::visible(State, State.playerEmpire, S->x, S->y)) Ids.push_back(C.systemId);
        }
        if (Ids.empty()) { TradeDestination = 0; return; }
        const auto It = std::find(Ids.begin(), Ids.end(), TradeDestination);
        TradeDestination = It == Ids.end() || std::next(It) == Ids.end() ? Ids.front() : *std::next(It);
    }

    // ======================= INTELLIGENCE =======================
    void BeginIntelDraft() {
        if (bIntelDraft) return;
        DraftIntelBudget.clear(); DraftSecurity = 100;
        if (const auto* I = botf::empireIntelState(Preview(), State.playerEmpire)) {
            DraftSecurity = I->budget.internalSecurity.percentage;
            for (const auto& A : I->budget.rivalAllocations)
                DraftIntelBudget.push_back({ A.rivalId, { A.espionage.percentage, A.sabotage.percentage } });
        }
        for (const auto& R : State.rivals) if (R.id != State.playerEmpire &&
            std::none_of(DraftIntelBudget.begin(), DraftIntelBudget.end(), [&R](const auto& A) { return A.first == R.id; }))
            DraftIntelBudget.push_back({ R.id, { 0, 0 } });
        bIntelDraft = true;
    }
    bool IntelDraftDirty() const {
        if (!bIntelDraft) return false;
        const auto* I = botf::empireIntelState(Preview(), State.playerEmpire);
        if (DraftSecurity != (I ? I->budget.internalSecurity.percentage : 100)) return true;
        for (const auto& A : DraftIntelBudget) {
            int Espionage = 0, Sabotage = 0;
            if (I) for (const auto& Existing : I->budget.rivalAllocations) if (Existing.rivalId == A.first) {
                Espionage = Existing.espionage.percentage; Sabotage = Existing.sabotage.percentage; break;
            }
            if (A.second.first != Espionage || A.second.second != Sabotage) return true;
        }
        return false;
    }
    int IntelAllocation(int Id, bool Sabotage) const {
        if (bIntelDraft) for (const auto& A : DraftIntelBudget) if (A.first == Id) return Sabotage ? A.second.second : A.second.first;
        if (const auto* I = botf::empireIntelState(Preview(), State.playerEmpire))
            for (const auto& A : I->budget.rivalAllocations) if (A.rivalId == Id)
                return Sabotage ? A.sabotage.percentage : A.espionage.percentage;
        return 0;
    }
    int IntelSecurity() const {
        if (bIntelDraft) return DraftSecurity;
        if (const auto* I = botf::empireIntelState(Preview(), State.playerEmpire)) return I->budget.internalSecurity.percentage;
        return 100;
    }
    void ChangeIntelAllocation(int Id, bool Sabotage, int Delta) {
        BeginIntelDraft();
        for (auto& A : DraftIntelBudget) if (A.first == Id) {
            int& Target = Sabotage ? A.second.second : A.second.first;
            const int Applied = Delta > 0 ? std::min(Delta, DraftSecurity) : -std::min(-Delta, Target);
            Target += Applied; DraftSecurity -= Applied;
            return;
        }
    }
    void ChangeSecurity(int Delta) {
        BeginIntelDraft();
        if (Delta < 0 && !DraftIntelBudget.empty()) { ChangeIntelAllocation(DraftIntelBudget.front().first, false, -Delta); return; }
        if (Delta > 0) for (auto& A : DraftIntelBudget) {
            for (int* P : { &A.second.second, &A.second.first }) {
                const int Take = std::min(Delta, *P); *P -= Take; DraftSecurity += Take; Delta -= Take;
                if (!Delta) return;
            }
        }
    }
    FString IntelBudgetText() const {
        return FString::Printf(TEXT("Empire output %lld / turn  |  Internal security %d%%  |  security reserve %lld%s"),
            static_cast<long long>(botf::empireIntelOutput(State, State.playerEmpire)), IntelSecurity(),
            static_cast<long long>(botf::empireSecurityStockpile(State, State.playerEmpire)),
            IntelDraftDirty() ? TEXT("  |  UNAPPLIED: save applies; end turn discards") : TEXT(""));
    }
    TSharedRef<SWidget> BuildIntelligenceScreen() {
        return Console(Txt(TEXT("INTELLIGENCE")), SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 16, 0)
                [ Chip(TEXT("INTELLIGENCE RESERVE"), [this] { return FString::Printf(TEXT("%lld"), static_cast<long long>(State.intelligence)); },
                    C(&FPalette::Accent), TEXT("Points available to operations")) ]
                + SHorizontalBox::Slot().FillWidth(1) [ Section(Txt(TEXT("INTELLIGENCE BUDGET")), SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight() [ Para(Dyn([this] { return IntelBudgetText(); }), 9, &FPalette::Text) ]
                    + SVerticalBox::Slot().AutoHeight() [ Grid({
                        Pill(TEXT("SECURITY +5"), [this] { ChangeSecurity(5); }, [this] { return !Session.active; }, ERole::Secondary),
                        Pill(TEXT("SECURITY -5"), [this] { ChangeSecurity(-5); }, [this] { return !Session.active; }, ERole::Secondary),
                        Pill(TEXT("APPLY ALLOCATION"), [this] {
                            Submit(botf::makeSetIntelBudgetCommand(NextId(), DraftSecurity, DraftIntelBudget));
                            bIntelDraft = false; Status = TEXT("Intelligence allocation ordered for turn end.");
                        }, [this] { return IntelDraftDirty() && !Session.active; }, ERole::Primary),
                        Pill(TEXT("DISCARD EDITS"), [this] { bIntelDraft = false; DraftIntelBudget.clear(); Status = TEXT("Unapplied intelligence edits discarded."); },
                            [this] { return IntelDraftDirty(); }, ERole::Tertiary)
                    }, 2) ] ) ] ]
            + SVerticalBox::Slot().FillHeight(1)
            [ SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return IntelMode; })
                + SWidgetSwitcher::Slot() [ Scroll(SAssignNew(IntelCards, SVerticalBox)) ]
                + SWidgetSwitcher::Slot() [ Scroll(SAssignNew(IntelReportRows, SVerticalBox)) ] ],
            14, SheetHeader({ ModeButton(TEXT("OPERATIONS"), &IntelMode, 0, TEXT("Espionage and sabotage against each rival")),
                              ModeButton(TEXT("REPORTS"), &IntelMode, 1, TEXT("Results of completed operations")) }));
    }
    TSharedRef<SWidget> IntelCard(int Id) {
        return Fill(EShape::Row, &FPalette::Field, SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0) [ SNew(SBox).WidthOverride(8).HeightOverride(34) [ Swatch(Id, 0) ] ]
                + SHorizontalBox::Slot().FillWidth(1)
                [ SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight() [ Heading(Txt(Str(botf::empireName(Id)).ToUpper()), 14, &FPalette::Accent) ]
                    + SVerticalBox::Slot().AutoHeight() [ Para(Dyn([this, Id] {
                        const auto* R = botf::majorRelation(State, State.playerEmpire, Id);
                        return R && R->atWar ? FString(TEXT("At war: sabotage permitted")) : FString(TEXT("At peace: espionage only"));
                    }), 9, &FPalette::Muted) ] ] ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
            [ Para(Dyn([this, Id] {
                for (auto It = State.intelReports.rbegin(); It != State.intelReports.rend(); ++It)
                    if (It->viewer == State.playerEmpire && It->target == Id)
                        return FString::Printf(TEXT("Last operation, turn %d: %s. %d known colonies, about %lld credits."), It->turn,
                            It->success ? TEXT("success") : TEXT("failed"), It->knownColonies, static_cast<long long>(It->estimatedCredits));
                return FString(TEXT("No operations yet."));
            }), 10, &FPalette::Text) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
            [ Para(Dyn([this, Id] { return TEXT("Sabotage target: ") + (SelectedEmpire == Id && IntelTargetSystem ? SystemName(IntelTargetSystem) : FString(TEXT("none chosen"))); }), 10, &FPalette::Tertiary) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
            [ Para(Dyn([this, Id] {
                FString Text = FString::Printf(TEXT("Espionage %d%%  |  Sabotage %d%%"), IntelAllocation(Id, false), IntelAllocation(Id, true));
                if (const auto* I = botf::empireIntelState(Preview(), State.playerEmpire)) if (const auto* R = I->getRivalState(Id))
                    Text += FString::Printf(TEXT("  |  Focus %s  |  stockpiles %lld / %lld"),
                        UTF8_TO_TCHAR(botf::intelligence::departmentName(R->focusDepartment)),
                        static_cast<long long>(R->espionageStockpile), static_cast<long long>(R->sabotageStockpile));
                return Text;
            }), 9, &FPalette::Text) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
            [ Grid({
                Pill(TEXT("ESP +5"), [this, Id] { ChangeIntelAllocation(Id, false, 5); }, [this] { return !Session.active; }, ERole::Secondary),
                Pill(TEXT("ESP -5"), [this, Id] { ChangeIntelAllocation(Id, false, -5); }, [this] { return !Session.active; }, ERole::Tertiary),
                Pill(TEXT("SAB +5"), [this, Id] { ChangeIntelAllocation(Id, true, 5); }, [this] { return !Session.active; }, ERole::Secondary),
                Pill(TEXT("SAB -5"), [this, Id] { ChangeIntelAllocation(Id, true, -5); }, [this] { return !Session.active; }, ERole::Tertiary),
                Pill(TEXT("CYCLE FOCUS"), [this, Id] {
                    const auto* I = botf::empireIntelState(Preview(), State.playerEmpire);
                    const auto* R = I ? I->getRivalState(Id) : nullptr;
                    const int Next = R ? (static_cast<int>(R->focusDepartment) + 1) % 4 : 1;
                    Submit(botf::makeSetIntelDepartmentFocusCommand(NextId(), Id, static_cast<botf::intelligence::IntelDepartment>(Next)));
                    Status = TEXT("Department focus ordered for turn end.");
                }, [this] { return !Session.active; }, ERole::Primary) }, 3) ]
            + SVerticalBox::Slot().AutoHeight()
            [ Grid({
                Pill(TEXT("ESPIONAGE"), [this, Id] { SelectedEmpire = Id; Draft(botf::CommandType::GatherIntelligence, Id); }, nullptr, ERole::Primary, TEXT("Gather intelligence at turn end")),
                Pill(TEXT("CHOOSE TARGET"), [this, Id] { if (SelectedEmpire != Id) { SelectedEmpire = Id; IntelTargetSystem = 0; } CycleIntelTarget(); }, nullptr, ERole::Secondary, TEXT("Cycle their colonies within sensor range")),
                Pill(TEXT("SABOTAGE"), [this, Id] { Draft(botf::CommandType::Sabotage, Id, 0, IntelTargetSystem); }, [this, Id] {
                    const auto* R = botf::majorRelation(State, State.playerEmpire, Id);
                    return SelectedEmpire == Id && IntelTargetSystem != 0 && R && R->atWar;
                }, ERole::Alert, TEXT("Requires war and a chosen target colony")) }, 3) ],
            FMargin(12, 10));
    }
    void RefreshIntel() {
        if (!IntelCards.IsValid() || !IntelReportRows.IsValid()) return;
        IntelCards->ClearChildren(); IntelReportRows->ClearChildren();
        std::vector<int> Rivals;
        for (int Id = 1; Id <= 5; ++Id) {
            if (Id == State.playerEmpire) continue;
            const auto* R = botf::majorRelation(State, State.playerEmpire, Id);
            if (R && R->contact) Rivals.push_back(Id);
        }
        for (std::size_t I = 0; I < Rivals.size(); I += 2) {
            auto Pair = SNew(SHorizontalBox);
            Pair->AddSlot().FillWidth(1).Padding(0, 0, 8, 0) [ IntelCard(Rivals[I]) ];
            if (I + 1 < Rivals.size()) Pair->AddSlot().FillWidth(1) [ IntelCard(Rivals[I + 1]) ];
            else Pair->AddSlot().FillWidth(1) [ SNullWidget::NullWidget ];
            IntelCards->AddSlot().AutoHeight().Padding(0, 0, 0, 8) [ Pair ];
        }
        if (Rivals.empty()) IntelCards->AddSlot().AutoHeight() [ Para(Txt(TEXT("No rival empires contacted yet.")), 11, &FPalette::Muted) ];
        int Reports = 0;
        for (auto It = State.intelReports.rbegin(); It != State.intelReports.rend() && Reports < 30; ++It) {
            if (It->viewer != State.playerEmpire) continue;
            ++Reports;
            IntelReportRows->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
                [ Fill(EShape::Row, &FPalette::FieldHi, SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [ Heading(Txt(FString::Printf(TEXT("TURN %d  /  %s  /  %s"), It->turn, *Str(botf::empireName(It->target)).ToUpper(),
                        It->success ? TEXT("SUCCESS") : TEXT("FAILED"))), 11, It->success ? &FPalette::Good : &FPalette::Alert) ]
                    + SVerticalBox::Slot().AutoHeight()
                    [ Para(Txt(FString::Printf(TEXT("%d known colonies  |  about %lld credits%s"), It->knownColonies,
                        static_cast<long long>(It->estimatedCredits), It->systemId ? *(TEXT("  |  ") + SystemName(It->systemId)) : TEXT(""))), 10, &FPalette::Text) ],
                    FMargin(12, 6)) ];
        }
        for (auto It = State.intelOperations.rbegin(); It != State.intelOperations.rend() && Reports < 30; ++It) {
            if (It->attackerEmpireId != State.playerEmpire && !(It->defenderEmpireId == State.playerEmpire && It->detectedByDefender)) continue;
            ++Reports;
            const FString Text = FString::Printf(TEXT("Turn %d  |  %s  |  %s against %s  |  %s\n%s"), It->turn,
                It->attackerEmpireId == State.playerEmpire ? TEXT("YOUR OPERATION") : TEXT("INTRUSION DETECTED"),
                UTF8_TO_TCHAR(botf::intelligence::intelAreaName(It->area)), *Str(botf::empireName(It->defenderEmpireId)),
                UTF8_TO_TCHAR(botf::intelligence::outcomeName(It->outcome)), UTF8_TO_TCHAR(It->details.c_str()));
            IntelReportRows->AddSlot().AutoHeight().Padding(0, 0, 0, 4) [ Para(Txt(Text), 10, &FPalette::Text) ];
        }
        if (!Reports) IntelReportRows->AddSlot().AutoHeight() [ Para(Txt(TEXT("No operations completed yet.")), 11, &FPalette::Muted) ];
    }
    void CycleIntelTarget() {
        std::vector<int> Ids;
        for (const auto& C : State.colonies) {
            if (C.ownerId != SelectedEmpire) continue;
            const auto S = std::find_if(State.systems.begin(), State.systems.end(), [&](const botf::System& X) { return X.id == C.systemId; });
            if (S != State.systems.end() && botf::visible(State, State.playerEmpire, S->x, S->y)) Ids.push_back(C.systemId);
        }
        if (Ids.empty()) { IntelTargetSystem = 0; Status = TEXT("None of their colonies are within sensor range."); return; }
        const auto It = std::find(Ids.begin(), Ids.end(), IntelTargetSystem);
        IntelTargetSystem = It == Ids.end() || std::next(It) == Ids.end() ? Ids.front() : *std::next(It);
    }

    // ======================= EMPIRE =======================
    TSharedRef<SWidget> BuildEmpireScreen() {
        return Console(Txt(TEXT("EMPIRE")), SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return EmpireMode; })
            + SWidgetSwitcher::Slot() [ Scroll(SAssignNew(SystemTableRows, SVerticalBox)) ]
            + SWidgetSwitcher::Slot() [ Scroll(SAssignNew(FleetTableRows, SVerticalBox)) ]
            + SWidgetSwitcher::Slot()
            [ Scroll(Stack({
                Section(Txt(TEXT("CAMPAIGN RECORD")), Para(Dyn([this] {
                    return FString::Printf(TEXT("%s  |  turn %d  |  %s galaxy, seed %llu  |  %s ruleset\n%d systems  |  %d ships  |  %s"),
                        *Str(botf::empireName(State.playerEmpire)), State.turn,
                        State.ruleset == botf::Ruleset::Reference ? TEXT("classic") : TEXT("prototype"),
                        static_cast<unsigned long long>(State.seed), *Str(botf::view::rulesetLabel(State)), OwnColonyCount(), OwnShipCount(),
                        HasUnsavedChanges() ? TEXT("unsaved progress") : TEXT("saved"));
                }), 13, &FPalette::Text)),
                // Member races (MinorRaces.h): the join bonuses every colony of the empire currently draws.
                Section(Txt(TEXT("MEMBER RACES")), Para(Dyn([this] {
                    const auto Lines = botf::view::memberBonusLines(State, State.playerEmpire);
                    if (Lines.empty()) return FString(TEXT("No minor race has joined your empire. Court them in Diplomacy: each member adds its join bonus to every colony."));
                    FString Text;
                    for (const auto& Entry : Lines) Text += (Text.IsEmpty() ? FString() : FString(TEXT("\n"))) + Str(Entry);
                    return Text;
                }), 11, &FPalette::Text)),
                Fill(EShape::Row, &FPalette::FieldHi, Heading(Dyn([this] { return OutcomeText(); }), 15, &FPalette::Accent), FMargin(12, 6)),
                Line({ Button(TEXT("SAVE CAMPAIGN"), [this] { Save(); }),
                       Button(TEXT("LOAD CAMPAIGN"), [this] { OpenSubPage(EUiView::Load); }),
                       Button(TEXT("NEW CAMPAIGN"), [this] { OpenSubPage(EUiView::Setup); }),
                       Button(TEXT("SETTINGS"), [this] { OpenSubPage(EUiView::Settings); }, [] { return true; }, 12, FString(), ERole::Secondary) }) })) ]
            + SWidgetSwitcher::Slot() [ Scroll(BuildVictoryView()) ],
            14, SheetHeader({ ModeButton(TEXT("SYSTEMS"), &EmpireMode, 0, TEXT("All your systems; click to open")),
                              ModeButton(TEXT("FLEETS"), &EmpireMode, 1, TEXT("All your task forces and ships; click to find on the chart")),
                              ModeButton(TEXT("CAMPAIGN"), &EmpireMode, 2, TEXT("Campaign record, member races, save and load")),
                              ModeButton(TEXT("VICTORY"), &EmpireMode, 3, TEXT("Progress toward every victory condition")) }));
    }
    // Victory progress (victoryProgress): one bar per condition, the megaproject and the rivals racing for it.
    TSharedRef<SWidget> VictoryRowWidget(int Index) {
        const auto RowAt = [this, Index]() -> botf::view::VictoryRow {
            const auto Rows = botf::view::victoryRows(State, State.playerEmpire);
            return Index < static_cast<int>(Rows.size()) ? Rows[static_cast<std::size_t>(Index)] : botf::view::VictoryRow();
        };
        return Fill(EShape::Row, &FPalette::FieldHi, SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)
                [ Heading(Dyn([RowAt] { const auto R = RowAt(); return Str(R.name).ToUpper() + (R.achieved ? TEXT("  /  ACHIEVED") : TEXT("")); }), 12, &FPalette::Tertiary) ]
                + SHorizontalBox::Slot().AutoWidth()
                [ Label(Dyn([RowAt] { const auto R = RowAt(); return R.enabled ? FString::Printf(TEXT("%d%%"), R.percent) : FString(TEXT("OFF")); }), 12, &FPalette::Accent, true) ] ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
            [ SNew(BotfUi::SSegmentBar).Segments(25).Height(9)
                .Fraction_Lambda([RowAt] {
                    float Out = 0.f;
                    FString Error;
                    FBotfCampaign::Run([&Out, &RowAt] { const auto R = RowAt(); Out = R.enabled ? R.percent / 100.f : 0.f; }, Error);
                    return Out;
                })
                .FillColor(C(&FPalette::Good)).EmptyColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Secondary, 0.22f)); }) ]
            + SVerticalBox::Slot().AutoHeight() [ Para(Dyn([RowAt] { return Str(RowAt().explanation); }), 9, &FPalette::Text) ],
            FMargin(10, 8));
    }
    TSharedRef<SWidget> BuildVictoryView() {
        return Stack({
            Section(Txt(TEXT("VICTORY CONDITIONS")), Stack({ VictoryRowWidget(0), VictoryRowWidget(1), VictoryRowWidget(2), VictoryRowWidget(3) }, 6)),
            Section(Dyn([this] { return TEXT("MEGAPROJECT  /  ") + Str(botf::megaprojectName(State)).ToUpper(); }), Para(Dyn([this] {
                const auto* Project = botf::empireMegaproject(Preview(), State.playerEmpire);
                FString Text = Str(botf::view::megaprojectText(Preview(), botf::view::megaprojectView(Preview(), State.playerEmpire, Project ? Project->systemId : 0)));
                if (!Project && State.rules.alternativeVictories) Text += TEXT("\nStart it from the Solar System sheet of the colony that should build it.");
                for (const auto& Rival : botf::view::rivalMegaprojectLines(State)) Text += TEXT("\n") + Str(Rival);
                return Text;
            }), 10, &FPalette::Text)) });
    }
    void RefreshEmpireTables() {
        if (SystemTableRows.IsValid()) {
            SystemTableRows->ClearChildren();
            for (const auto& Col : State.colonies) {
                if (Col.ownerId != State.playerEmpire) continue;
                const auto E = botf::economy(State, Col);
                const FString Building = Col.queue.empty() ? FString(TEXT("idle")) : ConstructionName(State, Col.systemId, Col.queue.front());
                FString GovStatus = FString::Printf(TEXT("Gov: Work [%s%s]  Queue [%s]"),
                    Col.workforcePriority == botf::GovernorPriority::Manual ? TEXT("Manual") :
                    Col.workforcePriority == botf::GovernorPriority::Balanced ? TEXT("Balanced") :
                    Col.workforcePriority == botf::GovernorPriority::Growth ? TEXT("Growth") :
                    Col.workforcePriority == botf::GovernorPriority::Research ? TEXT("Research") : TEXT("Manufacturing"),
                    Col.workforceManualOverride && Col.workforcePriority != botf::GovernorPriority::Manual ? TEXT("*Paused") : TEXT(""),
                    Col.queuePriority == botf::GovernorPriority::Manual ? TEXT("Manual") :
                    Col.queuePriority == botf::GovernorPriority::Balanced ? TEXT("Balanced") :
                    Col.queuePriority == botf::GovernorPriority::Growth ? TEXT("Growth") :
                    Col.queuePriority == botf::GovernorPriority::Research ? TEXT("Research") : TEXT("Manufacturing"));
                if (!Col.governorWarning.empty()) {
                    GovStatus += FString::Printf(TEXT("  |  ALERT: %s"), *Str(Col.governorWarning));
                }
                SystemTableRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
                    [ Row(FString::Printf(TEXT("%s\nPop %d  |  morale %d  |  food %+d  |  industry %d  |  research %d  |  credits %+d  |  building: %s\n%s"),
                        *SystemName(Col.systemId).ToUpper(), Col.population, Col.morale, E.food - Col.population, E.industry, E.research, E.income - E.upkeep, *Building, *GovStatus),
                        [this, Id = Col.systemId] { SelectSystem(Id); GoPanel(PanelSystem); }, [this, Id = Col.systemId] { return Selected == Id; },
                        nullptr, TEXT("Open in the Solar System screen")) ];
            }
        }
        if (FleetTableRows.IsValid()) {
            FleetTableRows->ClearChildren();
            const auto& Draft = Preview();
            for (const auto& Group : botf::empireTaskForces(Draft, State.playerEmpire)) {
                if (Group.fleetIds.empty()) continue;
                const int GroupId = Group.id, LeadId = Group.fleetIds.front();
                FString Members;
                for (const auto& Member : botf::taskForceFleets(Draft, GroupId)) Members += FString::Printf(TEXT("  %s #%d"), *Str(botf::fleetName(Member)), Member.id);
                FleetTableRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
                    [ Row(FString::Printf(TEXT("%s  /  %d SHIPS  /  %s\nDestination %d,%d  |  hull %d  |  shields %d  |  attack %d\n%s"),
                        *Str(Group.name).ToUpper(), static_cast<int>(Group.fleetIds.size()), StanceName(Group.stance), Group.targetX, Group.targetY,
                        botf::taskForceHull(Draft, GroupId), botf::taskForceShields(Draft, GroupId), botf::taskForceAttack(Draft, GroupId), *Members),
                        [this, GroupId, LeadId] { SelectedFleet = LeadId; SelectedTaskForce = GroupId; if (const auto* Ship = Fleet()) SelectCell(Ship->y * State.width + Ship->x); GoPanel(PanelGalaxy); if (Chart.IsValid()) Chart->CentreOn(SelectedCell); },
                        [this, GroupId] { return SelectedTaskForce == GroupId; }, nullptr, TEXT("Show on the galactic chart")) ];
            }
            for (const auto& F : Draft.fleets) {
                if (F.ownerId != State.playerEmpire || botf::fleetTaskForce(Draft, F.id)) continue;
                FleetTableRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
                    [ Row(FString::Printf(TEXT("#%d  %s\nSector %d,%d  |  destination %d,%d  |  hull %d/%d  |  shields %d/%d  |  %s"),
                        F.id, *Str(botf::fleetName(F)), F.x, F.y, F.targetX, F.targetY, F.hull, botf::fleetMaxHull(F),
                        F.shields, botf::fleetMaxShields(F), StanceName(F.stance)),
                        [this, Id = F.id] { SelectedFleet = Id; SelectedTaskForce = 0; if (const auto* Ship = Fleet()) SelectCell(Ship->y * State.width + Ship->x); GoPanel(PanelGalaxy); if (Chart.IsValid()) Chart->CentreOn(SelectedCell); },
                        [this, Id = F.id] { return SelectedTaskForce == 0 && SelectedFleet == Id; }, nullptr, TEXT("Show on the galactic chart")) ];
            }
            if (!OwnShipCount()) FleetTableRows->AddSlot().AutoHeight() [ Para(Txt(TEXT("You have no ships. Build them at a powered shipyard.")), 11, &FPalette::Muted) ];
        }
    }

    // ======================= COMBAT =======================
    TSharedRef<SWidget> BuildCombatContext() {
        return SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return Session.pending ? 1 : 0; })
            + SWidgetSwitcher::Slot() [ BuildReplayContext() ]
            + SWidgetSwitcher::Slot()
            [ Stack({
                Section(Txt(TEXT("ENCOUNTER")), Para(Dyn([this] { return EncounterSummary(); }), 9, &FPalette::Text)),
                Section(Txt(TEXT("THIS TURN")), Para(Dyn([this] {
                    return FString::Printf(TEXT("%d earlier battle%s resolved this turn.\nThe turn completes once every battle is over; your campaign is unchanged until then."),
                        static_cast<int>(Session.completed.size()), Session.completed.size() == 1 ? TEXT("") : TEXT("s"));
                }), 9, &FPalette::Muted)),
                Para(Txt(TEXT("Click ships on the radar or in the list. 1-5: Engage, Charge, Circle, Hold, Retreat. Space: next round. You may save mid-battle.\nCloaked ships cannot fire; any attack order decloaks them for a +25% ambush volley. Retreat takes a full round of disengaging under fire before the ship escapes. A ram hurts both ships. Ships you give no order are commanded by the tactical AI.")), 8, &FPalette::Muted) }) ];
    }
    FString EncounterSummary() const {
        if (!Session.pending) return FString();
        const auto& E = *Session.pending;
        int Own = 0, Foe = 0, OwnUp = 0, FoeUp = 0, Enemy = 0;
        for (const auto& S : E.ships) {
            const bool Mine = S.ownerId == State.playerEmpire;
            (Mine ? Own : Foe)++;
            if (TacActive(&S)) (Mine ? OwnUp : FoeUp)++;
            if (!Mine) Enemy = S.ownerId;
        }
        FString Text = FString::Printf(TEXT("Sector %d,%d  |  round %d of at most %d\nYour ships: %d of %d fighting\n%s: %d of %d fighting%s"),
            E.sectorX, E.sectorY, E.currentRound, E.maxRounds, OwnUp, Own, *Str(botf::empireName(Enemy)), FoeUp, Foe,
            E.isFinished ? TEXT("\nBATTLE OVER") : TEXT(""));
        const std::string Hail = botf::view::hailOutlook(E, State.playerEmpire);
        if (!Hail.empty()) Text += TEXT("\n") + Str(Hail);
        if (!E.remasterRules()) Text += TEXT("\nThis battle began under the legacy tactical rules of an older save.");
        return Text;
    }
    TSharedRef<SWidget> BuildReplayContext() {
        return Stack({
            Section(Txt(TEXT("ENGAGEMENTS")), SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                [ Grid({ Pill(TEXT("PREVIOUS"), [this] { SelectBattle(-1); }, [this] { return State.latestBattles.size() > 1; }, ERole::Secondary),
                         Pill(TEXT("NEXT"), [this] { SelectBattle(1); }, [this] { return State.latestBattles.size() > 1; }, ERole::Secondary) }, 2) ]
                + SVerticalBox::Slot().AutoHeight() [ Para(Dyn([this] { return BattleText(); }), 9, &FPalette::Text) ]),
            Section(Txt(TEXT("REPLAY")), Grid({
                Pill(TEXT("FIRST"), [this] { SelectBattleRoundAbsolute(0); }, [this] { const auto* B = CurrentBattle(); return B && !B->rounds.empty() && BattleRoundIndex > 0; }, ERole::Tertiary),
                Pill(TEXT("BACK"), [this] { SelectBattleRound(-1); }, [this] { const auto* B = CurrentBattle(); return B && !B->rounds.empty() && BattleRoundIndex > 0; }, ERole::Tertiary),
                Pill(TEXT("FORWARD"), [this] { SelectBattleRound(1); }, [this] { const auto* B = CurrentBattle(); return B && BattleRoundIndex + 1 < static_cast<int>(B->rounds.size()); }, ERole::Tertiary),
                Pill(TEXT("LAST"), [this] { const auto* B = CurrentBattle(); if (B) SelectBattleRoundAbsolute(static_cast<int>(B->rounds.size()) - 1); },
                    [this] { const auto* B = CurrentBattle(); return B && BattleRoundIndex + 1 < static_cast<int>(B->rounds.size()); }, ERole::Tertiary) }, 2)) });
    }
    TSharedRef<SWidget> BattleView() {
        return SNew(SOverlay)
            + SOverlay::Slot() [ Fill(EShape::Block, &FPalette::Field) ]
            + SOverlay::Slot() [ SNew(BotfUi::SBattleImage, &BattleBrush, [this] { return Scene.IsValid() && Session.pending; }) ]
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(6))
            [ DynControl(Dyn([this] { return Scene.IsValid() && Scene->GetCameraMode() == 1 ? FString(TEXT("CHASE CAM")) : FString(TEXT("OVERVIEW CAM")); }),
                [this] { if (Scene.IsValid()) Scene->CycleCameraMode(); }, nullptr, nullptr, ERole::Tertiary, FMargin(6, 4), false, 8,
                TEXT("Toggle 3D Combat Camera (C)")) ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [ SNew(SBox).Visibility_Lambda([this] { return Scene.IsValid() ? EVisibility::Collapsed : EVisibility::Visible; })
                [ Para(Txt(TEXT("3D view unavailable; the radar shows the full battle.")), 10, &FPalette::Muted) ] ];
    }
    TSharedRef<SWidget> ManeuverButton(const FString& Label, botf::tactical::TacticalManeuver Maneuver, const FString& Tip, ERole Role = ERole::Primary) {
        return Control(Label, [this, Maneuver] { TacOrder(Maneuver); },
            [this, Maneuver] { return CanTacOrder(Maneuver); },
            [this, Maneuver] {
                if (!Session.pending || bTacOrderAll) return false;
                for (const auto& O : Session.pending->pendingOrders) if (O.fleetId == TacShip) return O.maneuver == Maneuver;
                return false;
            }, Role, FMargin(6, 6), false, 9, Tip);
    }
    // Original encounter dialog (Tutorial p26): choose to command the battle or let it resolve.
    TSharedRef<SWidget> BuildEncounterChoice() {
        return SNew(SOverlay)
            + SOverlay::Slot() [ BattleView() ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [ SNew(SBox).WidthOverride(580)
                [ Fill(EShape::Row, &FPalette::Field, Stack({
                    Heading(Txt(TEXT("ENEMY CONTACT")), 22, &FPalette::Alert),
                    Para(Dyn([this] { return EncounterSummary(); }), 12, &FPalette::Text),
                    Para(Dyn([this] {
                        FString Text;
                        if (!Session.pending) return Text;
                        for (const auto& S : Session.pending->ships)
                            Text += FString::Printf(TEXT("%s  %s #%d  hull %d  shields %d\n"), *Str(botf::empireName(S.ownerId)), *Str(S.name), S.fleetId, S.hull, S.shields);
                        return Text;
                    }), 9, &FPalette::Muted),
                    Line({
                        Control(TEXT("FIGHT"), [this] { bEncounterChoice = false; TacLog = TEXT("You have tactical command. Give orders, then advance the round."); UpdateScene(); },
                            nullptr, nullptr, ERole::Alert, FMargin(22, 12), false, 13, TEXT("Command your ships round by round (F)")),
                        Control(TEXT("AUTO"), [this] { TacAuto(); }, nullptr, nullptr, ERole::Secondary, FMargin(22, 12), false, 13,
                            TEXT("Your ships fight under automatic command (A)")),
                        Control(TEXT("HAIL"), [this] {
                            bEncounterChoice = false;
                            bTacOrderAll = true;
                            TacOrder(botf::tactical::TacticalManeuver::Hail);
                            TacNextRound();
                        }, nullptr, nullptr, ERole::Tertiary, FMargin(22, 12), false, 13, TEXT("Attempt diplomatic truce with enemy force (H)")) }, 8),
                    Para(Txt(TEXT("Command in 3D (F), let ships fight automatically (A), or attempt a diplomatic truce (H).")), 9, &FPalette::Muted) }, 10), FMargin(20, 16)) ] ];
    }
    std::function<void(int)> TacPickCallback() { return [this](int Id) { TacPick(Id); RefreshAll(); }; }

    TSharedRef<SWidget> BuildTacticalView() {
        return SNew(SOverlay)
            // 0: Fullscreen 3D Combat Viewport (with mouse orbit drag & wheel zoom)
            + SOverlay::Slot()
            [
                SNew(BotfUi::STacticalViewport, Scene, &BattleBrush, [this] { return Scene.IsValid() && Session.pending; })
            ]
            // 1: Top HUD Bar (Title, Round, Faction Matchup, Camera Toolbar)
            + SOverlay::Slot().VAlign(VAlign_Top).HAlign(HAlign_Fill)
            [
                SNew(SBox).HeightOverride(44)
                [
                    SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Void, 0.82f, 0.90f)); })
                    .Padding(FMargin(12, 5))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(STextBlock).Text(Dyn([this] {
                                if (!Session.pending) return FString();
                                return FString::Printf(TEXT("TACTICAL ENGAGEMENT  /  SECTOR %d,%d"), Session.pending->sectorX, Session.pending->sectorY);
                            })).Font(BotfUi::Font(11, true)).ColorAndOpacity(C(&FPalette::Accent))
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16, 0, 0, 0)
                        [
                            SNew(STextBlock).Text(Dyn([this] {
                                if (!Session.pending) return FString();
                                return FString::Printf(TEXT("ROUND %d OF %d  |  %s"),
                                    Session.pending->currentRound, Session.pending->maxRounds,
                                    Session.pending->isFinished ? TEXT("BATTLE OVER") : TEXT("TACTICAL PHASE"));
                            })).Font(BotfUi::Font(9, true)).ColorAndOpacity_Lambda([this] {
                                return FSlateColor(Session.pending && Session.pending->isFinished ? P().Good : P().Text);
                            })
                        ]
                        + SHorizontalBox::Slot().FillWidth(1) [ SNullWidget::NullWidget ]
                        // Camera Mode Presets & Toolbar
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().Padding(2, 0)
                            [
                                Option(TEXT("OVERVIEW (C)"), [this] { if (Scene.IsValid()) Scene->SetCameraMode(0); },
                                    [this] { return Scene.IsValid() && Scene->GetCameraMode() == 0; }, ERole::Tertiary, 0, TEXT("Tactical Overview perspective (C)"))
                            ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(2, 0)
                            [
                                Option(TEXT("CHASE"), [this] { if (Scene.IsValid()) Scene->SetCameraMode(1); },
                                    [this] { return Scene.IsValid() && Scene->GetCameraMode() == 1; }, ERole::Tertiary, 0, TEXT("Dynamic Salvo/Dogfight Chase camera"))
                            ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(2, 0)
                            [
                                Option(TEXT("FOCUS SHIP"), [this] { if (Scene.IsValid()) { Scene->SetCameraMode(2); Scene->SetFocusedShips(TacShip, TacTarget); } },
                                    [this] { return Scene.IsValid() && Scene->GetCameraMode() == 2; }, ERole::Tertiary, 0, TEXT("Track currently selected friendly ship"))
                            ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(2, 0)
                            [
                                Option(TEXT("FOCUS TARGET"), [this] { if (Scene.IsValid()) { Scene->SetCameraMode(3); Scene->SetFocusedShips(TacShip, TacTarget); } },
                                    [this] { return Scene.IsValid() && Scene->GetCameraMode() == 3; }, ERole::Tertiary, 0, TEXT("Track designated hostile target"))
                            ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(2, 0)
                            [
                                Option(TEXT("TOP-DOWN"), [this] { if (Scene.IsValid()) Scene->SetCameraMode(4); },
                                    [this] { return Scene.IsValid() && Scene->GetCameraMode() == 4; }, ERole::Tertiary, 0, TEXT("Overhead 90-degree tactical arena grid"))
                            ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
                            [
                                Pill(TEXT("RESET CAM"), [this] { if (Scene.IsValid()) Scene->ResetCameraOrbit(); }, nullptr, ERole::Secondary, TEXT("Reset camera orbit and zoom (Home, Double-Click)"))
                            ]
                        ]
                    ]
                ]
            ]
            // 2: Left Overlay (Fleet Status / Ship Selection)
            + SOverlay::Slot().VAlign(VAlign_Fill).HAlign(HAlign_Left).Padding(FMargin(10, 50, 0, 110))
            [
                SNew(SBox).WidthOverride(270)
                [
                    SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Void, 0.78f, 0.88f)); })
                    .Padding(FMargin(8, 6))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                        [
                            Heading(Txt(TEXT("FRIENDLY FLEET")), 10, &FPalette::Accent)
                        ]
                        + SVerticalBox::Slot().FillHeight(1.1f).Padding(0, 0, 0, 6)
                        [
                            Scroll(SAssignNew(TacFriendlyRows, SVerticalBox))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 4)
                        [
                            Heading(Txt(TEXT("HOSTILE CONTACTS")), 10, &FPalette::Alert)
                        ]
                        + SVerticalBox::Slot().FillHeight(0.9f)
                        [
                            Scroll(SAssignNew(TacHostileRows, SVerticalBox))
                        ]
                    ]
                ]
            ]
            // 3: Right Overlay (Tactical Radar + Target Card + Orders Grid)
            + SOverlay::Slot().VAlign(VAlign_Fill).HAlign(HAlign_Right).Padding(FMargin(0, 50, 10, 110))
            [
                SNew(SBox).WidthOverride(290)
                [
                    SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Void, 0.78f, 0.88f)); })
                    .Padding(FMargin(8, 6))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                        [
                            Heading(Txt(TEXT("TACTICAL RADAR")), 10, &FPalette::Secondary)
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SBox).HeightOverride(155)
                            [
                                SNew(SBorder).BorderImage_Lambda([this] { return BotfUi::Styles().Get(ThemeEmpire(), EShape::Block); })
                                .BorderBackgroundColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Secondary, 0.45f)); }).Padding(2)
                                [
                                    SNew(BotfUi::STacticalRadar, &Session.pending, &State.playerEmpire, &TacShip, &TacTarget, TacPickCallback())
                                ]
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 4)
                        [
                            Para(Txt(TEXT("Click blip to target/select  |  Drag 3D to orbit")), 8, &FPalette::Muted)
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                        [
                            Section(Dyn([this] {
                                if (bTacOrderAll) return FString(TEXT("ORDERS  /  ALL SHIPS"));
                                return TEXT("ORDERS  /  ") + TacShipName(TacShip).ToUpper() + (TacTarget ? TEXT("  >  #") + FString::FromInt(TacTarget) : FString());
                            }), SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                            [
                                Grid({
                                    ManeuverButton(TEXT("ENGAGE"), botf::tactical::TacticalManeuver::Engage, TEXT("Close to firing range and fire (1)")),
                                    ManeuverButton(TEXT("CHARGE"), botf::tactical::TacticalManeuver::Charge, TEXT("Point-blank: +20% beams, more damage taken (2)")),
                                    ManeuverButton(TEXT("CIRCLE"), botf::tactical::TacticalManeuver::Circle, TEXT("Stand off and evade: -25% damage taken, weaker beams (3)")),
                                    ManeuverButton(TEXT("ASSAULT"), botf::tactical::TacticalManeuver::Assault, TEXT("Forward charge with +20% beam burst (6)")),
                                    ManeuverButton(TEXT("STRAFE"), botf::tactical::TacticalManeuver::Strafe, TEXT("High-speed passing attack: +25% beam damage, +1 speed (9)")),
                                    ManeuverButton(TEXT("FLYBY"), botf::tactical::TacticalManeuver::Flyby, TEXT("Fast medium-range pass: -15% incoming damage, +1 speed (8)")),
                                    ManeuverButton(TEXT("HARRY"), botf::tactical::TacticalManeuver::Harry, TEXT("Close harassment: -20% incoming damage (7)")),
                                    ManeuverButton(TEXT("EVADE"), botf::tactical::TacticalManeuver::Evade, TEXT("Defensive posture: -35% damage taken (0)")),
                                    ManeuverButton(TEXT("HOLD"), botf::tactical::TacticalManeuver::Hold, TEXT("Hold position and brace: -20% damage taken, shields do not recharge (4)"), ERole::Secondary),
                                    ManeuverButton(TEXT("RAM"), botf::tactical::TacticalManeuver::Ram, TEXT("Collide when within one cell: the target takes your hull as damage and you take its hull plus shields; usually both die (R)"), ERole::Alert),
                                    ManeuverButton(TEXT("HAIL"), botf::tactical::TacticalManeuver::Hail, TEXT("Offer a truce: it holds when every ship of yours hails and the enemy accepts (H)"), ERole::Secondary),
                                    ManeuverButton(TEXT("RETREAT"), botf::tactical::TacticalManeuver::Retreat, TEXT("Disengage: one round under fire with weapons offline (-25% damage taken), then escape if still retreating (5)"), ERole::Tertiary),
                                    Control(TEXT("ALL SHIPS"), [this] { bTacOrderAll = !bTacOrderAll; }, nullptr, [this] { return bTacOrderAll; },
                                        ERole::Secondary, FMargin(6, 6), false, 9, TEXT("Give the next order to every active ship")),
                                    DynControl(Dyn([this] { return Scene.IsValid() ? Scene->GetCameraModeName() : FString(TEXT("CAM")); }),
                                        [this] { if (Scene.IsValid()) Scene->CycleCameraMode(); }, nullptr, nullptr,
                                        ERole::Tertiary, FMargin(6, 6), false, 9, TEXT("Cycle 3D Combat Camera (C)")) }, 2)
                            ])
                        ]
                    ]
                ]
            ]
            // 4: Bottom HUD Bar (Telemetry Log on left, Command Execution Controls on right)
            + SOverlay::Slot().VAlign(VAlign_Bottom).HAlign(HAlign_Fill).Padding(FMargin(10, 0, 10, 10))
            [
                SNew(SBox).HeightOverride(100)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 10, 0)
                    [
                        SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .BorderBackgroundColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Void, 0.82f, 0.90f)); })
                        .Padding(FMargin(10, 6))
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().AutoWidth() [ Heading(Txt(TEXT("COMBAT TELEMETRY")), 9, &FPalette::Tertiary) ]
                                + SHorizontalBox::Slot().FillWidth(1) [ SNullWidget::NullWidget ]
                                + SHorizontalBox::Slot().AutoWidth()
                                [
                                    SNew(STextBlock).Visibility_Lambda([this] { return Status.StartsWith(TEXT("Order rejected")) ? EVisibility::Visible : EVisibility::Collapsed; })
                                    .Text(Dyn([this] { return Status; })).Font(BotfUi::Font(9, true)).ColorAndOpacity(C(&FPalette::Alert))
                                ]
                            ]
                            + SVerticalBox::Slot().FillHeight(1)
                            [
                                Scroll(Para(Dyn([this] { return TacLog; }), 9, &FPalette::Text))
                            ]
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SBox).WidthOverride(290)
                        [
                            SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                            .BorderBackgroundColor_Lambda([this] { return FSlateColor(BotfUi::Shade(P().Void, 0.85f, 0.92f)); })
                            .Padding(FMargin(10, 8))
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().FillHeight(1)
                                [
                                    SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return (Session.pending && Session.pending->isFinished) ? 1 : 0; })
                                    + SWidgetSwitcher::Slot()
                                    [
                                        SNew(SVerticalBox)
                                        + SVerticalBox::Slot().FillHeight(1)
                                        [
                                            Control(TEXT("NEXT ROUND"), [this] { TacNextRound(); }, [this] { return Session.pending && !Session.pending->isFinished; }, nullptr,
                                                ERole::Accent, FMargin(10, 10), false, 12, TEXT("Resolve one tactical round (Space). Ships without orders act on their own."))
                                        ]
                                        + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                                        [
                                            Control(TEXT("AUTO FINISH"), [this] { TacAuto(); }, [this] { return Session.pending && !Session.pending->isFinished; }, nullptr,
                                                ERole::Secondary, FMargin(6, 6), false, 10, TEXT("Hand the rest of the battle to automatic command (A)"))
                                        ]
                                    ]
                                    + SWidgetSwitcher::Slot()
                                    [
                                        Control(TEXT("CONTINUE TURN"), [this] { TacContinue(); }, [this] { return Session.pending && Session.pending->isFinished; }, nullptr,
                                            ERole::Accent, FMargin(10, 14), false, 13, TEXT("Apply the result and continue the turn (Space)"))
                                    ]
                                ]
                            ]
                        ]
                    ]
                ]
            ];
    }
    void RefreshTactical() {
        if (TacRows.IsValid()) TacRows->ClearChildren();
        if (TacFriendlyRows.IsValid()) TacFriendlyRows->ClearChildren();
        if (TacHostileRows.IsValid()) TacHostileRows->ClearChildren();
        if (!Session.pending) return;
        const auto& E = *Session.pending;

        for (const auto& S : E.ships) {
            const bool Mine = (S.ownerId == State.playerEmpire);
            const auto Ordered = E.getOrder(S.fleetId);
            const bool HasOrder = std::any_of(E.pendingOrders.begin(), E.pendingOrders.end(), [&](const botf::tactical::Order& O) { return O.fleetId == S.fleetId; });
            const auto* Grp = E.getGroupForShip(S.fleetId);
            const FString GrpTag = Grp ? FString::Printf(TEXT("[%s] "), *Str(botf::tactical::tacticalGroupClassName(Grp->groupClass))) : FString();
            const FString CloakTag = S.isCloaked ? TEXT(" [CLOAKED]") : FString();
            FString State_ = !S.isAlive ? FString(TEXT("DESTROYED")) : S.retreated ? FString(TEXT("WITHDRAWN"))
                : Mine ? (HasOrder ? Str(botf::tactical::maneuverName(Ordered.maneuver)).ToUpper() + (Ordered.targetFleetId ? FString::Printf(TEXT(" > #%d"), Ordered.targetFleetId) : FString())
                                   : E.standingOrders.count(S.fleetId) ? TEXT("STANDING: ") + Str(botf::tactical::maneuverName(Ordered.maneuver)).ToUpper()
                                   : FString(TEXT("NO ORDER: THE TACTICAL AI DECIDES")))
                : FString::Printf(TEXT("at %d,%d"), S.position.x, S.position.y);
            // Rules 2 state: disengaging, no retreat route, unarmed; and what the ship actually did last round.
            if (S.isAlive && !S.retreated) {
                std::vector<std::string> Tags = botf::view::shipTags(E, S);
                Tags.erase(std::remove_if(Tags.begin(), Tags.end(), [](const std::string& Tag) { return Tag.rfind("CLOAKED", 0) == 0; }), Tags.end());
                if (!Tags.empty()) State_ += TEXT("\n") + Str(botf::view::joinTags(Tags));
                const std::string Last = botf::view::executedOrderText(E, S.fleetId);
                if (!Last.empty()) State_ += TEXT("\nLast round: ") + Str(Last);
            }

            const bool bSelected = Mine ? (TacShip == S.fleetId) : (TacTarget == S.fleetId);
            const FLinearColor FrameColor = bSelected ? (Mine ? P().Accent : P().Alert) : BotfUi::Shade(P().Secondary, 0.25f);

            auto CardWidget = SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FSlateColor(bSelected ? BotfUi::Shade(FrameColor, 0.4f, 0.85f) : BotfUi::Shade(P().Void, 0.65f, 0.75f)))
                .Padding(FMargin(6, 4))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
                        [
                            SNew(SBox).WidthOverride(6) [ Swatch(S.ownerId, 0) ]
                        ]
                        + SHorizontalBox::Slot().FillWidth(1)
                        [
                            SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s%s #%d%s"), *GrpTag, *Str(S.name), S.fleetId, *CloakTag)))
                            .Font(BotfUi::Font(9, true))
                            .ColorAndOpacity(FSlateColor(bSelected ? FrameColor : P().Text))
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(STextBlock).Text(FText::FromString(bSelected ? (Mine ? TEXT("[ACTIVE]") : TEXT("[TARGET]")) : FString()))
                            .Font(BotfUi::Font(8, true))
                            .ColorAndOpacity(FSlateColor(FrameColor))
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("H:%d/%d S:%d/%d"), S.hull, S.maxHull, S.shields, S.maxShields)))
                            .Font(BotfUi::Font(8))
                            .ColorAndOpacity(FSlateColor(P().Muted))
                        ]
                        + SHorizontalBox::Slot().FillWidth(1).Padding(6, 1, 0, 1)
                        [
                            SNew(BotfUi::SShipStatusBar)
                            .Hull(S.hull).MaxHull(S.maxHull)
                            .Shields(S.shields).MaxShields(S.maxShields)
                            .Empire(S.ownerId)
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
                    [
                        SNew(STextBlock).Text(FText::FromString(State_))
                        .Font(BotfUi::Font(8))
                        .ColorAndOpacity(FSlateColor(!S.isAlive ? P().Alert : S.retreated ? P().Muted : P().Secondary))
                    ]
                ];

            auto RowButton = SNew(SButton)
                .ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
                .ContentPadding(FMargin(0, 0, 0, 3))
                .OnClicked_Lambda([this, Id = S.fleetId]() {
                    TacPick(Id);
                    RefreshAll();
                    return FReply::Handled();
                })
                .ToolTipText(FText::FromString(Mine ? TEXT("Select to give orders (or click radar blip)") : TEXT("Designate as target (or click radar blip)")))
                [
                    CardWidget
                ];

            if (Mine && TacFriendlyRows.IsValid())
            {
                TacFriendlyRows->AddSlot().AutoHeight() [ RowButton ];
            }
            else if (!Mine && TacHostileRows.IsValid())
            {
                TacHostileRows->AddSlot().AutoHeight() [ RowButton ];
            }
        }
    }
    TSharedRef<SWidget> BuildCombatScreen() {
        return SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] {
            if (!Session.pending) return 0;
            if (bEncounterChoice) return 1;
            return 2;
        })
        + SWidgetSwitcher::Slot()
        [
            Console(Txt(TEXT("TACTICAL COMBAT  /  BATTLE RECORDS")),
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0) [ SNew(SBox).WidthOverride(250) [ Scroll(BuildCombatContext()) ] ]
                + SHorizontalBox::Slot().FillWidth(1) [ BuildReplayView() ],
                14, SheetHeader({}))
        ]
        + SWidgetSwitcher::Slot()
        [
            BuildEncounterChoice()
        ]
        + SWidgetSwitcher::Slot()
        [
            BuildTacticalView()
        ];
    }
    TSharedRef<SWidget> BuildReplayView() {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 10, 0)
            [ SNew(SVerticalBox)
                + SVerticalBox::Slot().FillHeight(1.3f)
                [ SNew(SOverlay)
                    + SOverlay::Slot() [ Fill(EShape::Block, &FPalette::Field) ]
                    + SOverlay::Slot() [ SNew(BotfUi::SBattleImage, &BattleBrush, [this] { return CurrentBattle() && Scene.IsValid() && ShownBattle.bValid; }) ]
                    + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [ SNew(SBox).Visibility_Lambda([this] { return CurrentBattle() && Scene.IsValid() ? EVisibility::Collapsed : EVisibility::Visible; })
                        [ Para(Dyn([this] { return CurrentBattle() ? FString(TEXT("3D view unavailable; the replay below shows the battle."))
                            : FString(TEXT("No battles fought yet. Declare war, select an enemy ship in range and order an attack.")); }), 12, &FPalette::Muted) ] ] ]
                // Report-driven replay: always drawn from the battle record, independent of the 3D scene.
                + SVerticalBox::Slot().FillHeight(1).Padding(0, 6, 0, 0)
                [ SNew(SBox).Visibility_Lambda([this] { return CurrentBattle() ? EVisibility::Visible : EVisibility::Collapsed; })
                    [ Fill(EShape::Block, &FPalette::Field, SNew(BotfUi::SBattlePlot,
                        [this]() -> const botf::CombatReport* { return CurrentBattle(); },
                        [this] { const auto* B = CurrentBattle(); return B && !B->rounds.empty() ? BattleRoundIndex : -1; },
                        &State.playerEmpire), FMargin(4)) ] ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
                [ Para(Txt(TEXT("Ship models: Federation by Icewolf132 (Ulysses design: Taldren); other factions use provisional silhouettes.")), 8, &FPalette::Muted) ] ]
            + SHorizontalBox::Slot().AutoWidth()
            [ SNew(SBox).WidthOverride(320)
                [ Scroll(Stack({
                    Section(Dyn([this] { const auto* B = CurrentBattle(); return B && !B->rounds.empty()
                        ? FString::Printf(TEXT("ROUND %d OF %d"), std::clamp(BattleRoundIndex, 0, static_cast<int>(B->rounds.size()) - 1) + 1, static_cast<int>(B->rounds.size()))
                        : FString(TEXT("ROUND")); }), Para(Dyn([this] { return BattleRoundText(); }), 10, &FPalette::Text)),
                    Section(Txt(TEXT("SHIPS ENGAGED")), Para(Dyn([this] { return BattleParticipantsText(); }), 9, &FPalette::Text)) })) ] ];
    }
    const botf::CombatReport* CurrentBattle() const {
        if (State.latestBattles.empty()) return nullptr;
        return &State.latestBattles[std::min(BattleIndex, static_cast<int>(State.latestBattles.size()) - 1)];
    }
    void SelectBattle(int Direction) {
        if (State.latestBattles.empty()) return;
        BattleIndex = (BattleIndex + Direction + static_cast<int>(State.latestBattles.size())) % static_cast<int>(State.latestBattles.size());
        BattleRoundIndex = std::max(0, static_cast<int>(CurrentBattle()->rounds.size()) - 1);
        ShowBattle();
    }
    void SelectBattleRoundAbsolute(int Index) {
        const auto* B = CurrentBattle();
        if (!B || B->rounds.empty()) return;
        BattleRoundIndex = std::clamp(Index, 0, static_cast<int>(B->rounds.size()) - 1);
        ShowBattle();
    }
    void SelectBattleRound(int Direction) {
        SelectBattleRoundAbsolute(BattleRoundIndex + Direction);
    }
    FString BattleParticipantsText() const {
        const auto* B = CurrentBattle();
        if (!B) return TEXT("-");
        if (B->participants.empty()) return TEXT("This older save did not record the ships involved.");
        FString Text;
        // Stations are recorded as Destroyer-class participants; they are named by their combatant id, never by class.
        for (const auto& Participant : B->participants) {
            const FString Name = Str(botf::view::participantLabel(&State, Participant.fleetId, Participant.ownerId, Participant.shipClass, Participant.referenceShipId));
            Text += botf::view::isStation(Participant.fleetId)
                ? FString::Printf(TEXT("%s %s (station)  |  hull %d  shields %d\n"), *Str(botf::empireName(Participant.ownerId)), *Name,
                    Participant.initialHull, Participant.initialShields)
                : FString::Printf(TEXT("%s #%d %s  |  hull %d  shields %d\n"), *Str(botf::empireName(Participant.ownerId)), Participant.fleetId, *Name,
                    Participant.initialHull, Participant.initialShields);
        }
        return Text;
    }
    FString BattleRoundText() const {
        const auto* B = CurrentBattle();
        if (!B) return TEXT("-");
        if (B->rounds.empty()) return TEXT("This older save recorded only the final result.");
        const auto& Round = B->rounds[std::clamp(BattleRoundIndex, 0, static_cast<int>(B->rounds.size()) - 1)];
        FString Text;
        if (Round.damage.empty()) Text += TEXT("No damage this round.\n");
        const auto Unit = [](int Id) {
            return botf::view::isStation(Id) ? FString::Printf(TEXT("Station %d"), botf::stationIdForCombatant(Id)) : FString::Printf(TEXT("Ship #%d"), Id);
        };
        for (const auto& D : Round.damage)
            Text += FString::Printf(TEXT("%s: shields %d to %d, hull %d to %d\n"), *Unit(D.fleetId), D.shieldBefore, D.shieldAfter, D.hullBefore, D.hullAfter);
        for (const int Id : Round.lostFleetIds) Text += Unit(Id) + TEXT(" destroyed\n");
        for (const int Id : Round.retreatedFleetIds) Text += Unit(Id) + TEXT(" retreated\n");
        // Battles fought in this session also keep their encounter: orders, AI decisions, ambushes, disengages, rams.
        // A committed record alone only holds damage (core's CombatRound has no per-ship orders).
        const auto& Encounters = Campaign.LastTurnEncounters();
        const int Match = botf::view::matchEncounter(Encounters, *B);
        if (Match >= 0) {
            const auto& Encounter = Encounters[static_cast<std::size_t>(Match)];
            const int Index = std::clamp(BattleRoundIndex, 0, static_cast<int>(Encounter.rounds.size()) - 1);
            Text += TEXT("\nORDERS AND EVENTS");
            for (const auto& Line_ : botf::view::roundDetail(Encounter, Encounter.rounds[static_cast<std::size_t>(Index)], {}))
                Text += TEXT("\n") + Str(Line_);
        } else {
            Text += TEXT("\nOrders, ambushes and rams are shown for the battles of the last turn played in this session.");
        }
        return Text;
    }
    // Which recorded battle and round the 3D scene currently holds; invalid while it shows a live encounter.
    struct FShownBattle {
        bool bValid = false;
        int Turn = -1, X = -1, Y = -1, Index = -1, Round = -1;
        std::uint64_t Version = 0;
        bool operator==(const FShownBattle& O) const {
            return bValid == O.bValid && Turn == O.Turn && X == O.X && Y == O.Y && Index == O.Index && Round == O.Round && Version == O.Version;
        }
    };
    FShownBattle ShownBattle;   // CampaignVersion (FBotfCampaign) is bumped when State is replaced wholesale or a turn commits
    FShownBattle WantedBattle() const {
        const auto* B = CurrentBattle();
        if (!B) return FShownBattle();
        return { true, B->turn, B->x, B->y, std::min(BattleIndex, static_cast<int>(State.latestBattles.size()) - 1),
            B->rounds.empty() ? -1 : std::clamp(BattleRoundIndex, 0, static_cast<int>(B->rounds.size()) - 1), CampaignVersion };
    }
    // Configures the 3D scene for the selected record. Presentation is paused around the rebuild so the actor takes
    // its immediate capture path (it captures synchronously only while not capturing every frame), then resumed;
    // the brush is re-bound so Slate drops any handle cached for an earlier texture state.
    void ShowBattle() {
        const auto* B = CurrentBattle();
        if (!Scene.IsValid() || !B) return;
        Scene->SetPresentationActive(false);
        Scene->ConfigureBattleReport(*B, B->rounds.empty() ? -1 : BattleRoundIndex);
        BattleBrush.SetResourceObject(nullptr);
        BattleBrush.SetResourceObject(Scene->GetBattleTexture());
        ShownBattle = WantedBattle();
        Scene->SetPresentationActive(CombatPresentation());
    }
    FString BattleText() const {
        const auto* B = CurrentBattle();
        if (!B) return TEXT("No battles yet.");
        return FString::Printf(TEXT("Turn %d, sector %d,%d\n%s (%d ships) vs %s (%d ships)\nVictor: %s  |  destroyed %d  |  retreated %d"),
            B->turn, B->x, B->y, *Str(botf::empireName(B->firstOwner)), static_cast<int>(B->firstFleetIds.size()),
            *Str(botf::empireName(B->secondOwner)), static_cast<int>(B->secondFleetIds.size()),
            B->winnerOwner ? *Str(botf::empireName(B->winnerOwner)) : TEXT("none"),
            static_cast<int>(B->lostFleetIds.size()), static_cast<int>(B->retreatedFleetIds.size()));
    }
    FString OutcomeText() const {
        if (State.outcome == botf::CampaignOutcome::Victory) return TEXT("VICTORY — your empire controls the quadrant.");
        if (State.outcome == botf::CampaignOutcome::Defeat) return TEXT("DEFEAT — your empire has fallen.");
        return FString::Printf(TEXT("TURN %d  /  %d ORDERS WAITING"), State.turn, static_cast<int>(State.pending.size()));
    }

    // ---- Turn processing ----
    // End Turn opens a strategic transaction; encounters involving the player pause it for Fight / Auto.
    void EndTurn() {
        MapView.bPlotting = false;
        if (Session.active && Session.pending) { GoPanel(PanelCombat); Status = TEXT("A battle awaits your decision before the turn can finish."); return; }
        if (Session.active) { ContinueTurn(); return; }
        bIntelDraft = false; DraftIntelBudget.clear(); // End Turn Anyway explicitly discards an unapplied allocation.
        TurnForecast = IncomeForecast(); bTurnForecast = true;
        Campaign.BeginTurn();
        // A failure before any encounter leaves the committed campaign untouched; close the empty transaction.
        try { ContinueTurn(); }
        catch (...) { if (!Session.pending) { Session = botf::session::TurnSession(); bTurnForecast = false; } throw; }
    }
    void ContinueTurn() {
        // The campaign commits the turn (and closes the transaction) or pauses on the next player battle.
        auto Result = Campaign.ResumeTurn();
        if (!Result) {
            if (!Session.pending) throw std::runtime_error("Turn paused without an encounter");
            OpenEncounter(true);
            return;
        }
        bEncounterChoice = false; TacShip = TacTarget = 0; TacLog.Empty();
        FinishTurn(*Result);
    }
    void OpenEncounter(bool bFresh) {
        const auto& E = *Session.pending;
        bEncounterChoice = bFresh || E.currentRound == 0;
        // Keep the player's ship/target selection when it is still valid in this encounter (e.g. after load).
        const auto* KeptShip = TacShipPtr(TacShip); const auto* KeptTarget = TacShipPtr(TacTarget);
        if (bFresh || !TacActive(KeptShip) || KeptShip->ownerId != State.playerEmpire) TacShip = 0;
        if (bFresh || !TacActive(KeptTarget) || KeptTarget->ownerId == State.playerEmpire) TacTarget = 0;
        if (bFresh) bTacOrderAll = false;
        for (const auto& S : E.ships)
            if (TacActive(&S)) {
                if (!TacShip && S.ownerId == State.playerEmpire) TacShip = S.fleetId;
                if (!TacTarget && S.ownerId != State.playerEmpire) TacTarget = S.fleetId;
            }
        TacLog = FString::Printf(TEXT("Contact in sector %d,%d. %d ships engaged."), E.sectorX, E.sectorY, static_cast<int>(E.ships.size()));
        bSummaryOpen = bMenuOpen = bEndTurnCheck = false;
        ActivePanel = PanelCombat;
        UpdateScene();
        Status = TEXT("Enemy contact. Choose Fight to command your ships or Auto to let them fight.");
    }
    const botf::tactical::Ship* TacShipPtr(int Id) const { return Session.pending ? Session.pending->getShip(Id) : nullptr; }
    bool TacActive(const botf::tactical::Ship* S) const { return S && S->isAlive && S->activeInCombat && !S->retreated; }
    void NextTacShip() {
        if (!Session.pending) return;
        const auto& E = *Session.pending;
        TArray<int> FriendlyIds;
        for (const auto& S : E.ships) if (S.ownerId == State.playerEmpire && TacActive(&S)) FriendlyIds.Add(S.fleetId);
        if (FriendlyIds.IsEmpty()) return;
        int CurrentIndex = FriendlyIds.IndexOfByKey(TacShip);
        int NextIndex = (CurrentIndex + 1) % FriendlyIds.Num();
        TacShip = FriendlyIds[NextIndex];
        if (Scene.IsValid()) Scene->SetFocusedShips(TacShip, TacTarget);
        RefreshAll();
    }
    void NextTacTarget() {
        if (!Session.pending) return;
        const auto& E = *Session.pending;
        TArray<int> EnemyIds;
        for (const auto& S : E.ships) if (S.ownerId != State.playerEmpire && TacActive(&S)) EnemyIds.Add(S.fleetId);
        if (EnemyIds.IsEmpty()) return;
        int CurrentIndex = EnemyIds.IndexOfByKey(TacTarget);
        int NextIndex = (CurrentIndex + 1) % EnemyIds.Num();
        TacTarget = EnemyIds[NextIndex];
        if (Scene.IsValid()) Scene->SetFocusedShips(TacShip, TacTarget);
        RefreshAll();
    }
    void TacPick(int FleetId) {
        const auto* S = TacShipPtr(FleetId);
        if (!S) return;
        if (S->ownerId == State.playerEmpire) TacShip = FleetId; else TacTarget = FleetId;
        if (Scene.IsValid()) Scene->SetFocusedShips(TacShip, TacTarget);
    }
    // Whether the manoeuvre can be ordered now for the selection (or, with ALL SHIPS, for at least one ship):
    // the encounter's own validation, so Retreat is unavailable without a safe withdrawal sector.
    bool CanTacOrder(botf::tactical::TacticalManeuver Maneuver) const {
        if (!Session.pending || Session.pending->isFinished) return false;
        for (const auto& S : Session.pending->ships) {
            if (S.ownerId != State.playerEmpire || !TacActive(&S)) continue;
            if (!bTacOrderAll && S.fleetId != TacShip) continue;
            try { Session.pending->checkOrder(botf::tactical::Order{S.fleetId, 0, Maneuver}); return true; }
            catch (const std::exception&) {}
        }
        return false;
    }
    // All or nothing, like the core's group orders: every order is validated before any is given.
    void TacOrder(botf::tactical::TacticalManeuver Maneuver) {
        if (!Session.pending) return;
        const int Target = TacActive(TacShipPtr(TacTarget)) ? TacTarget : 0;
        std::vector<botf::tactical::Order> Orders;
        for (const auto& S : Session.pending->ships) {
            if (S.ownerId != State.playerEmpire || !TacActive(&S)) continue;
            if (!bTacOrderAll && S.fleetId != TacShip) continue;
            if (Maneuver == botf::tactical::TacticalManeuver::Retreat && !S.canRetreat) continue;
            Orders.push_back(botf::tactical::Order{S.fleetId, Maneuver == botf::tactical::TacticalManeuver::Retreat ? 0 : Target, Maneuver});
        }
        if (Orders.empty()) throw std::runtime_error(Maneuver == botf::tactical::TacticalManeuver::Retreat
            ? "No safe withdrawal sector: the selected ship cannot retreat" : "Select one of your active ships");
        for (const auto& Order : Orders) Session.pending->checkOrder(Order);
        for (const auto& Order : Orders) botf::session::order(Session, Order);
        Campaign.Notify(EBotfCampaignChange::Session);
        const int Given = static_cast<int>(Orders.size());
        Status = TEXT("Tactical order accepted.");
        TacLog = FString::Printf(TEXT("%s ordered for %d ship%s%s."), *Str(botf::tactical::maneuverName(Maneuver)), Given, Given == 1 ? TEXT("") : TEXT("s"),
            Target && Maneuver != botf::tactical::TacticalManeuver::Retreat ? *(TEXT(", target #") + FString::FromInt(Target)) : TEXT(""));
        if (Scene.IsValid()) Scene->SetFocusedShips(TacShip, TacTarget);
    }
    void TacNextRound() {
        botf::session::resolveRound(Session);
        const auto& E = *Session.pending;
        TacLog = TacRoundText(E.rounds.back());
        Status = E.isFinished ? TEXT("Battle over. Continue the turn when ready.") : FString::Printf(TEXT("Round %d resolved."), E.currentRound);
        if (!TacActive(TacShipPtr(TacShip))) for (const auto& S : E.ships) if (S.ownerId == State.playerEmpire && TacActive(&S)) { TacShip = S.fleetId; break; }
        if (!TacActive(TacShipPtr(TacTarget))) for (const auto& S : E.ships) if (S.ownerId != State.playerEmpire && TacActive(&S)) { TacTarget = S.fleetId; break; }
        if (Scene.IsValid()) Scene->SetFocusedShips(TacShip, TacTarget);
        UpdateScene();
    }
    void TacAuto() {
        botf::session::autoResolve(Session);
        TacLog = TEXT("Ships fought under automatic command.\n") + TacRoundText(Session.pending->rounds.back());
        bEncounterChoice = false;
        UpdateScene();
    }
    // After an encounter finishes the turn resumes; further encounters or the final report follow.
    void TacContinue() {
        if (!Session.pending || !Session.pending->isFinished) throw std::runtime_error("The battle is not over yet");
        ContinueTurn();
    }
    FString TacShipName(int Id) const {
        const auto* S = TacShipPtr(Id);
        return S ? FString::Printf(TEXT("%s #%d"), *Str(S->name), Id) : FString::Printf(TEXT("#%d"), Id);
    }
    // Round log: orders (AI decisions marked), ambush volleys, disengages and escapes, both halves of a ram, truce.
    FString TacRoundText(const botf::tactical::RoundResult& Round) const {
        if (!Session.pending) return FString();
        FString Text = FString::Printf(TEXT("ROUND %d"), Round.roundIndex);
        const auto Name = [this](int Id) { return std::string(TCHAR_TO_UTF8(*TacShipName(Id))); };
        for (const auto& Line_ : botf::view::roundDetail(*Session.pending, Round, Name)) Text += TEXT("\n") + Str(Line_);
        return Text;
    }
    void FinishTurn(const botf::TurnReport& R) {
        LastReport = FString::Printf(TEXT("TURN %d\nCredits %+lld  |  research +%lld  |  trade +%d  |  ship maintenance %d"), State.turn,
            static_cast<long long>(R.creditsAfter - R.creditsBefore), static_cast<long long>(R.researchGained), R.tradeIncome, R.fleetUpkeep);
        LastFleetUpkeep = R.fleetUpkeep; LastTradeIncome = R.tradeIncome;
        // A turn resumed from a mid-battle save has no forecast from this session.
        if (bTurnForecast) {
            LastReport += FString::Printf(TEXT("\nForecast before the turn: %+d, plus %lld committed by orders."),
                TurnForecast.Net(), static_cast<long long>(TurnForecast.Spend));
            UE_LOG(LogTemp, Display, TEXT("BOTF_INCOME forecast=%d spend=%lld ships=%d/%d trade=%d/%d actual=%lld"), TurnForecast.Net(),
                static_cast<long long>(TurnForecast.Spend), TurnForecast.Ships, R.fleetUpkeep, TurnForecast.Trade, R.tradeIncome,
                static_cast<long long>(R.creditsAfter - R.creditsBefore));
        }
        LastForecast = TurnForecast; bLastForecast = bTurnForecast; bTurnForecast = false;
        LastEvents.Empty();
        for (const auto& Col : R.colonies) {
            LastReport += FString::Printf(TEXT("\n%s: %d food, %d industry, structure upkeep %d."),
                *SystemName(Col.systemId), Col.economy.food, Col.economy.industry, Col.economy.upkeep);
            if (!Col.completed.empty()) {
                LastReport += TEXT(" Completed: ") + Str(Col.completed) + TEXT(".");
                LastEvents.Add(SystemName(Col.systemId) + TEXT(": completed ") + Str(Col.completed));
            }
        }
        // The turn briefing (TurnReport::chronicle: one categorised entry per event line, with its place) feeds the
        // inbox; a report without a chronicle falls back to the plain event lines.
        LastBriefing = botf::view::briefing(State, R);
        for (const auto& Event : R.events) {
            if (R.chronicle.empty()) LastEvents.Add(Str(Event));
            LastReport += TEXT("\n") + Str(Event);
            UE_LOG(LogTemp, Display, TEXT("BOTF_EVENT %s"), *Str(Event));
        }
        if (!LastBriefing.empty()) LastReport += TEXT("\nBriefing: ") + Str(botf::view::briefingSummary(LastBriefing)) + TEXT(".");
        bool PlayerBattle = false;
        for (const auto& Battle : R.battles) {
            LastReport += FString::Printf(TEXT("\nBattle at %d,%d: %s against %s; victor %s; %d ships lost."),
                Battle.x, Battle.y, *Str(botf::empireName(Battle.firstOwner)), *Str(botf::empireName(Battle.secondOwner)),
                Battle.winnerOwner ? *Str(botf::empireName(Battle.winnerOwner)) : TEXT("none"), static_cast<int>(Battle.lostFleetIds.size()));
            PlayerBattle = PlayerBattle || Battle.firstOwner == State.playerEmpire || Battle.secondOwner == State.playerEmpire;
        }
        for (const auto& Invasion : R.invasions) LastReport += FString::Printf(
            TEXT("\nInvasion of %s: %s against %s, strength %d to %d, casualties %d, %s."),
            *SystemName(Invasion.systemId), *Str(botf::empireName(Invasion.attackerOwner)), *Str(botf::empireName(Invasion.defenderOwner)),
            Invasion.attackStrength, Invasion.defenseStrength, Invasion.casualties,
            Invasion.conquered ? TEXT("colony taken") : TEXT("defenders held"));
        bAllocationDirty = false;
        if (!CurrentTaskForce()) SelectedTaskForce = 0;
        FormSelection.clear();
        SelectCell(SelectedCell); RefreshAll();
        Status = FString::Printf(TEXT("Turn %d begins. %d orders carried out."), State.turn, static_cast<int>(R.appliedCommands.size()));
        if (!R.events.empty() || !R.battles.empty() || !R.invasions.empty() || State.outcome != botf::CampaignOutcome::Ongoing) {
            bSummaryOpen = true; SummaryMode = 0;
        }
        // A new decision (or one falling due) opens the event card over the summary; the end of the game shows its overlay.
        for (const auto& Card : botf::view::eventCards(State))
            if (Card.needsDecision && !Card.chosenThisTurn) { bEventsOpen = true; bSummaryOpen = false; break; }
        if (State.outcome != botf::CampaignOutcome::Ongoing) { bOutcomeDismissed = false; bEventsOpen = false; }
        if (!R.battles.empty()) {
            BattleIndex = std::max(0, static_cast<int>(State.latestBattles.size()) - 1);
            BattleRoundIndex = std::max(0, static_cast<int>(CurrentBattle()->rounds.size()) - 1);
            if (PlayerBattle) ActivePanel = PanelCombat;
        }
        if (ActivePanel == PanelCombat) ShowBattle();
        UpdateScene();
        UE_LOG(LogTemp, Display, TEXT("BOTF_TURN %d HASH %llu"), State.turn, static_cast<unsigned long long>(R.stateHash));
    }
    void RefreshAll() {
        ++StateVersion;
        SyncAllocation();
        RefreshSector(); RefreshChoices(); RefreshQueue(); RefreshRaces(); RefreshProposals(); RefreshIntel(); RefreshEmpireTables(); RefreshReference(); RefreshSaveRows(); RefreshTactical();
        RefreshEvents(); RefreshTemplates();
        RefreshInbox();
        if (bMenuOpen && MenuCell >= 0 && MenuCell < State.width * State.height) OpenSectorMenu(MenuCell, MenuAt - FVector2D(6, 6));
    }

    bool Owned() const { const auto C = botf::colonyAt(State, Selected); return C && C->ownerId == State.playerEmpire; }
    void SelectCell(int CellId) {
        if (SelectedCell != CellId) FormSelection.clear();
        SelectedCell = CellId; Selected = 0;
        for (const auto& S : State.systems)
            if (S.y * State.width + S.x == CellId && botf::explored(State, S.x, S.y)) Selected = S.id;
        SelectedEnemyFleet = 0;
        if (const auto M = botf::minorAt(State, Selected); M && M->contact[State.playerEmpire]) SelectedMinor = Selected;
        RefreshSector(); RefreshChoices(); RefreshQueue(); RefreshTemplates();
    }
    void SelectSystem(int SystemId) {
        for (const auto& S : State.systems) if (S.id == SystemId) {
            SelectCell(S.y * State.width + S.x); return;
        }
    }
    const botf::Fleet* Fleet() const {
        for (const auto& F : State.fleets) if (F.id == SelectedFleet && F.ownerId == State.playerEmpire) return &F;
        return nullptr;
    }
    const botf::Fleet* TargetFleet() const {
        for (const auto& F : State.fleets)
            if (F.ownerId != State.playerEmpire && F.x == SelectedCell % State.width && F.y == SelectedCell / State.width
                && botf::visible(State, State.playerEmpire, F.x, F.y) && (!SelectedEnemyFleet || F.id == SelectedEnemyFleet)) return &F;
        return nullptr;
    }
    void Draft(botf::CommandType Type, int Empire = 0, int System = 0, int Target = 0) {
        botf::Command C; C.id = State.lastAcceptedCommand + 1; C.type = Type;
        C.targetEmpire = Empire; C.systemId = System; C.targetSystemId = Target;
        Submit(C);
        Status = TEXT("Order given. It takes effect when you end the turn.");
    }
    void SetStance(botf::TacticalStance Stance) {
        botf::Command C; C.id = State.lastAcceptedCommand + 1;
        C.type = botf::CommandType::SetTacticalStance; C.fleetId = SelectedFleet; C.stance = Stance;
        Submit(C);
        Status = FString(TEXT("Combat stance ordered: ")) + StanceName(Stance) + TEXT(".");
    }
    bool CanSettle() const {
        const auto F = Fleet();
        if (!F || !F->colonyShip || F->x != F->targetX || F->y != F->targetY) return false;
        for (const auto& S : State.systems)
            if (S.x == F->x && S.y == F->y && !botf::colonyAt(State, S.id) && !botf::minorAt(State, S.id)) return true;
        return false;
    }
    int ConstructionCost(const botf::GameState& Game, int SystemId, const botf::Construction& Project) const {
        for (const auto& Choice : botf::constructionChoices(Game, SystemId))
            if (Choice.product == Project.product && Choice.referenceId == Project.referenceId) return Choice.industryCost;
        return botf::definition(Project.product).industryCost;
    }
    FString ConstructionName(const botf::GameState& Game, int SystemId, const botf::Construction& Project) const {
        FString Name;
        for (const auto& Choice : botf::constructionChoices(Game, SystemId))
            if (Choice.product == Project.product && Choice.referenceId == Project.referenceId) { Name = Str(Choice.name); break; }
        if (Name.IsEmpty()) Name = Str(botf::definition(Project.product).name);
        Name.ReplaceInline(TEXT(" (remake infrastructure)"), TEXT(""));
        return Name;
    }
    FString SystemTitle() const {
        for (const auto& S : State.systems) if (S.id == Selected) return Str(S.name).ToUpper();
        return botf::explored(State, SelectedCell % State.width, SelectedCell / State.width) ? TEXT("OPEN SPACE") : TEXT("UNEXPLORED SECTOR");
    }
    void EditQueue(int Index, int Destination, bool Remove) {
        if (Remove) {
            if (!CanRemoveQueue(Index)) {
                Status = TEXT("Cannot remove: item is pinned to protect it from removal. Unpin first.");
                return;
            }
        } else {
            if (!CanMoveQueue(Index, Destination)) {
                Status = TEXT("Cannot move: pinned queue items cannot be moved or swapped past. Unpin first.");
                return;
            }
        }
        botf::Command C; C.id = State.lastAcceptedCommand + 1; C.systemId = Selected;
        C.type = Remove ? botf::CommandType::RemoveConstruction : botf::CommandType::MoveConstruction;
        C.queueIndex = Index; C.queueDestination = Destination;
        Submit(C);
        Status = Remove ? TEXT("Item removed from the queue. Cancel Last Order restores it.")
                        : TEXT("Queue reordered. Work already invested stays with each item.");
    }
    botf::Command WorkerOrder(int Field, int Change) const {
        if (!Owned()) throw std::runtime_error("No owned colony selected");
        const auto Draft = botf::preview(State); const auto& Colony = *botf::colonyAt(Draft, Selected);
        botf::Command C; C.id = State.lastAcceptedCommand + 1; C.systemId = Selected;
        C.foodWorkers = Colony.foodWorkers; C.industryWorkers = Colony.industryWorkers; C.researchWorkers = Colony.researchWorkers;
        int* Counts[] = {&C.foodWorkers, &C.industryWorkers, &C.researchWorkers};
        const int Capacities[] = {Colony.farms * 10, Colony.factories * 10, Colony.laboratories * 10};
        const int Free = Colony.population - C.foodWorkers - C.industryWorkers - C.researchWorkers;
        const int Delta = Change < 0 ? -std::min(-Change, *Counts[Field]) : std::min({Change, Free, Capacities[Field] - *Counts[Field]});
        if (Delta == 0) throw std::runtime_error("No workers or facility capacity available");
        *Counts[Field] += Delta; return C;
    }
    // Staff exactly Blocks structures of one kind (10 workers each), drawing on the labor pool. Replaces this
    // system's earlier staffing order this turn instead of stacking one order per click.
    void SetWorkerBlocks(int Field, int Blocks) {
        const auto* Col = DraftColony();
        if (!Col) throw std::runtime_error("Select one of your systems");
        int Counts[] = { Col->foodWorkers, Col->industryWorkers, Col->researchWorkers };
        const int Capacities[] = { Col->farms * 10, Col->factories * 10, Col->laboratories * 10 };
        const int Free = Col->population - Counts[0] - Counts[1] - Counts[2];
        const int Target = std::clamp(Blocks * 10, 0, Capacities[Field]);
        const int Delta = Target > Counts[Field] ? std::min(Target - Counts[Field], Free) : Target - Counts[Field];
        if (Delta == 0) return;
        Counts[Field] += Delta;
        botf::Command Cmd; Cmd.type = botf::CommandType::AssignWorkers; Cmd.systemId = Selected;
        Cmd.foodWorkers = Counts[0]; Cmd.industryWorkers = Counts[1]; Cmd.researchWorkers = Counts[2];
        const int SystemId = Selected;
        ReplaceOrders([SystemId](const botf::Command& O) { return O.type == botf::CommandType::AssignWorkers && O.systemId == SystemId; }, Cmd);
        Status = FString::Printf(TEXT("Staffing set: %d workers here. Check food before ending the turn."), Counts[Field]);
    }
    bool CanAdjustWorkers(int Field, int Change) const {
        try { WorkerOrder(Field, Change); return true; } catch (const std::exception&) { return false; }
    }
    void AdjustWorkers(int Field, int Change) {
        Submit(WorkerOrder(Field, Change));
        Status = TEXT("Workers reassigned. Check food before ending the turn.");
    }
    void Assign(bool Industry) {
        botf::Command C; C.id = State.lastAcceptedCommand + 1; C.systemId = Selected;
        const auto& Colony = *botf::colonyAt(State, Selected);
        C.foodWorkers = std::min((Colony.population + 1) / 2, Colony.farms * 10);
        const int Remaining = Colony.population - C.foodWorkers;
        C.researchWorkers = Industry ? 0 : std::min(Remaining / 2, Colony.laboratories * 10);
        C.industryWorkers = std::min(Remaining - C.researchWorkers, Colony.factories * 10);
        Submit(C);
        Status = TEXT("Staffing plan ordered.");
    }
    void SetWorkforceGovernor(botf::GovernorPriority Priority) {
        if (!Owned()) return;
        const auto* Col = DraftColony();
        if (!Col) return;
        botf::Command Cmd = botf::makeSetColonyGovernorCommand(State.lastAcceptedCommand + 1, Selected, Priority, Col->queuePriority);
        Submit(Cmd);
        Status = FString::Printf(TEXT("Workforce governor set to %s."), *Str(botf::governorPriorityName(Priority)));
        RefreshAll();
    }
    void SetQueueGovernor(botf::GovernorPriority Priority) {
        if (!Owned()) return;
        const auto* Col = DraftColony();
        if (!Col) return;
        botf::Command Cmd = botf::makeSetColonyGovernorCommand(State.lastAcceptedCommand + 1, Selected, Col->workforcePriority, Priority);
        Submit(Cmd);
        Status = FString::Printf(TEXT("Queue governor set to %s."), *Str(botf::governorPriorityName(Priority)));
        RefreshAll();
    }
    void ClearGovernorOverride() {
        if (!Owned()) return;
        botf::Command Cmd = botf::makeClearGovernorOverrideCommand(State.lastAcceptedCommand + 1, Selected);
        Submit(Cmd);
        Status = TEXT("Manual override cleared; governor resumed.");
        RefreshAll();
    }
    void ToggleQueuePin(int Index, bool bPin) {
        if (!Owned()) return;
        botf::Command Cmd = botf::makeSetQueuePinCommand(State.lastAcceptedCommand + 1, Selected, Index, bPin);
        Submit(Cmd);
        Status = bPin ? TEXT("Item pinned: governors will not alter or remove it.") : TEXT("Item unpinned.");
        RefreshAll();
    }
    void ToggleQueuePin(int Index) {
        if (!Owned()) return;
        const auto* Col = DraftColony();
        if (!Col || static_cast<std::size_t>(Index) >= Col->queue.size()) return;
        ToggleQueuePin(Index, !Col->queue[Index].pinned);
    }

    // ---- New campaign setup ----
    // Core generates empires 1..N (N = 2..5), so a campaign of N powers can only include players with id <= N.
    // Choosing an empire raises the count to its minimum; the note names exactly which powers will be in play.
    FString SetupPowersNote() const {
        FString Powers;
        for (int Id = 1; Id <= SetupEmpires; ++Id) Powers += (Id > 1 ? (Id == SetupEmpires ? TEXT(" and ") : TEXT(", ")) : TEXT("")) + Str(botf::empireName(Id));
        FString Note = TEXT("In play: ") + Powers + TEXT(".");
        if (SetupPlayer > 2)
            Note += FString::Printf(TEXT(" The %s need at least %d powers in play; smaller galaxies are unavailable."), *Str(botf::empireName(SetupPlayer)), SetupPlayer);
        return Note;
    }
    botf::RulesetPreset SetupPresetValue() const {
        return SetupPreset == static_cast<int>(botf::RulesetPreset::Classic) ? botf::RulesetPreset::Classic : botf::RulesetPreset::Remastered;
    }
    TSharedRef<SWidget> PowersOption(int Count) {
        return Control(FString::Printf(TEXT("%d POWERS"), Count), [this, Count] { SetupEmpires = Count; },
            [this, Count] { return SetupPlayer <= Count; }, [this, Count] { return SetupEmpires == Count; },
            ERole::Secondary, FMargin(10, 6), false, 10,
            FString::Printf(TEXT("Galaxy with the first %d major powers"), Count));
    }
    FString SetupEraNote() const {
        const bool bRef = (SetupRuleset == 1);
        switch (static_cast<botf::StartingEra>(SetupEra)) {
        case botf::StartingEra::Early:
            return bRef
                ? TEXT("Early frontier baseline: Level 1 technology across all fields, 1,000 credits treasury, 40 population, 1 scout and 1 colony ship.")
                : TEXT("Early frontier baseline: Level 0 technology across all fields, 100 credits treasury, 40 population, 1 scout and 1 colony ship.");
        case botf::StartingEra::Developed:
            return bRef
                ? TEXT("Developed regional power: Level 3 technology across all fields, 2,500 credits treasury, 55 population, powered grid, 1 scout, 1 destroyer, and 1 colony ship.")
                : TEXT("Developed regional power: Level 3 technology across all fields, 500 credits treasury, 55 population, powered grid, 1 scout, 1 destroyer, and 1 colony ship.");
        case botf::StartingEra::Expanded:
            return bRef
                ? TEXT("Expanded power: Level 5 technology across all fields, 4,500 credits treasury, 75 population, 1 scout, 1 destroyer, 1 cruiser, 1 colony ship, 1 troop transport, and Class H terraforming.")
                : TEXT("Expanded power: Level 5 technology across all fields, 1,200 credits treasury, 75 population, 1 scout, 2 destroyers, 1 colony ship, 1 troop transport, and Class H terraforming.");
        case botf::StartingEra::Advanced:
            return bRef
                ? TEXT("Advanced power: Level 7 technology across all fields, 7,000 credits treasury, 95 population, 2 scouts, 2 destroyers, 1 cruiser, 1 capital warship, 1 colony ship, 1 troop transport, and Class H/K terraforming.")
                : TEXT("Advanced power: Level 7 technology across all fields, 2,200 credits treasury, 95 population, 2 scouts, 4 destroyers, 1 colony ship, 1 troop transport, and Class H/K terraforming.");
        case botf::StartingEra::Supreme:
            return bRef
                ? TEXT("Supreme late-game presence: Level 10 technology across all fields, 10,000 credits treasury, 120 population, 2 scouts, 2 destroyers, 1 cruiser, 1 dreadnought, 2 colony ships, 1 troop transport, and Class H/K/Y terraforming.")
                : TEXT("Supreme late-game presence: Level 10 technology across all fields, 3,500 credits treasury, 120 population, 2 scouts, 4 destroyers, 2 colony ships, 1 troop transport, and Class H/K/Y terraforming.");
        default:
            return TEXT("Select a starting era to determine initial technology, economy, and fleet strength.");
        }
    }
    TSharedRef<SWidget> EraOption(botf::StartingEra Era) {
        const int EraVal = static_cast<int>(Era);
        const FString Name = FString(botf::startingEraName(Era)).ToUpper();
        return Control(Name, [this, EraVal] { SetupEra = EraVal; },
            nullptr, [this, EraVal] { return SetupEra == EraVal; },
            ERole::Secondary, FMargin(10, 6), false, 10,
            FString::Printf(TEXT("Start campaign in the %s era"), *Name));
    }
    TSharedRef<SWidget> BuildCampaignControls() {
        auto Factions = SNew(SHorizontalBox);
        for (int Id = 1; Id <= 5; ++Id) Factions->AddSlot().FillWidth(1).Padding(0, 0, Id < 5 ? 6 : 0, 0)
            [ SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4) [ Swatch(Id, 5) ]
                + SVerticalBox::Slot().AutoHeight()
                [ Option(Str(botf::empireName(Id)).ToUpper(), [this, Id] { SetupPlayer = Id; SetupEmpires = std::max(SetupEmpires, Id); },
                    [this, Id] { return SetupPlayer == Id; }, ERole::Primary, Id) ] ];
        auto OptionRow = [](std::initializer_list<TSharedRef<SWidget>> Items) {
            auto Box = SNew(SHorizontalBox);
            for (const auto& Item : Items) Box->AddSlot().AutoWidth().Padding(0, 0, 5, 0) [ Item ];
            return Box;
        };
        return Stack({
            Section(Dyn([this] { return TEXT("EMPIRE  /  ") + Str(botf::empireName(SetupPlayer)).ToUpper(); }), Factions),
            Section(Dyn([this] { return TEXT("GAMEPLAY  /  ") + Str(botf::rulesetPresetName(SetupPresetValue())).ToUpper(); }), SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                [ OptionRow({ Option(TEXT("REMASTERED"), [this] { SetupPreset = static_cast<int>(botf::RulesetPreset::Remastered); },
                                  [this] { return SetupPresetValue() == botf::RulesetPreset::Remastered; }, TEXT("Every remaster gameplay layer (recommended)")),
                              Option(TEXT("CLASSIC"), [this] { SetupPreset = static_cast<int>(botf::RulesetPreset::Classic); },
                                  [this] { return SetupPresetValue() == botf::RulesetPreset::Classic; }, TEXT("Original-like rules")) }) ]
                + SVerticalBox::Slot().AutoHeight()
                [ Para(Dyn([this] { return Str(botf::view::presetSummary(SetupPresetValue()))
                    + TEXT(" Both place minor races from the 30-race roster, scaled to the galaxy size."); }), 10, &FPalette::Muted) ]),
            Section(Txt(TEXT("CATALOGUE")), SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                [ OptionRow({ Option(TEXT("CLASSIC CATALOGUE"), [this] { SetupRuleset = 1; }, [this] { return SetupRuleset == 1; }),
                              Option(TEXT("PROTOTYPE GALAXY"), [this] { SetupRuleset = 0; }, [this] { return SetupRuleset == 0; }) }) ]
                + SVerticalBox::Slot().AutoHeight()
                [ Para(Dyn([this] { return SetupRuleset
                    ? FString(TEXT("Original technology names and costs, faction starships and structures, delayed diplomacy and combat stances."))
                    : FString(TEXT("A smaller, simplified early ruleset.")); }), 10, &FPalette::Muted) ]),
            Section(Dyn([this] { return FString::Printf(TEXT("STARTING ERA  /  %s"), *FString(botf::startingEraName(static_cast<botf::StartingEra>(SetupEra))).ToUpper()); }),
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                [ OptionRow({
                    EraOption(botf::StartingEra::Early),
                    EraOption(botf::StartingEra::Developed),
                    EraOption(botf::StartingEra::Expanded),
                    EraOption(botf::StartingEra::Advanced),
                    EraOption(botf::StartingEra::Supreme)
                }) ]
                + SVerticalBox::Slot().AutoHeight()
                [ Para(Dyn([this] { return SetupEraNote(); }), 10, &FPalette::Muted) ]),
            Section(Txt(TEXT("GALAXY")), SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                [ Para(Dyn([this] {
                    return FString::Printf(TEXT("%d major powers  |  %s  |  seed %s  |  random events %s"),
                        SetupEmpires, SetupSize ? TEXT("large galaxy, 20x15, 42 systems") : TEXT("standard galaxy, 16x12, 28 systems"),
                        SetupSeed == 0 ? TEXT("1999") : SetupSeed == 1 ? TEXT("24680") : TEXT("101010"), SetupEvents ? TEXT("on") : TEXT("off"));
                }), 12, &FPalette::Text) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                [ OptionRow({ PowersOption(2), PowersOption(3), PowersOption(4), PowersOption(5) }) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                [ Para(Dyn([this] { return SetupPowersNote(); }), 9, &FPalette::Muted) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                [ OptionRow({ Option(TEXT("STANDARD"), [this] { SetupSize = 0; }, [this] { return SetupSize == 0; }),
                              Option(TEXT("LARGE"), [this] { SetupSize = 1; }, [this] { return SetupSize == 1; }),
                              Option(TEXT("RANDOM EVENTS"), [this] { SetupEvents = !SetupEvents; }, [this] { return SetupEvents; }, ERole::Tertiary) }) ]
                + SVerticalBox::Slot().AutoHeight()
                [ OptionRow({ Option(TEXT("SEED 1999"), [this] { SetupSeed = 0; }, [this] { return SetupSeed == 0; }),
                              Option(TEXT("SEED 24680"), [this] { SetupSeed = 1; }, [this] { return SetupSeed == 1; }),
                              Option(TEXT("SEED 101010"), [this] { SetupSeed = 2; }, [this] { return SetupSeed == 2; }) }) ]),
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [ Control(TEXT("BEGIN CAMPAIGN"), [this] { RequestAction(EPendingAction::StartCampaign); }, nullptr, nullptr,
                    ERole::Accent, FMargin(28, 12), false, 13, FString()) ] });
    }
    void StartCampaign() {
        const std::uint64_t Seed = SetupSeed == 0 ? 1999 : SetupSeed == 1 ? 24680 : 101010;
        const botf::StartingEra ChosenEra = botf::startingEraValid(static_cast<botf::StartingEra>(SetupEra))
            ? static_cast<botf::StartingEra>(SetupEra)
            : botf::StartingEra::Early;
        // New campaigns go through botf::newCampaign (the only generator that places the 30-race minor roster and
        // applies the ruleset preset); the legacy generators remain the fixtures' baseline.
        botf::view::SetupChoice Choice;
        Choice.seed = Seed; Choice.empires = SetupEmpires; Choice.large = SetupSize != 0; Choice.player = SetupPlayer; Choice.era = ChosenEra;
        Choice.reference = SetupRuleset != 0;
        Choice.preset = SetupPresetValue();
        Choice.events = SetupEvents;
        ReplaceState(botf::view::startCampaign(Choice));
        bHasActiveCampaign = true; bHasSavedBaseline = false;
        Session = botf::session::TurnSession(); SavedSessionText.Empty();
        bEncounterChoice = false; TacShip = TacTarget = 0; TacLog.Empty();
        Selected = 0; SelectedFleet = 0; SelectedMinor = 0; SelectedEmpire = SetupPlayer == 1 ? 2 : 1;
        const auto First = std::find_if(State.fleets.begin(), State.fleets.end(), [this](const botf::Fleet& F) { return F.ownerId == State.playerEmpire; });
        if (First != State.fleets.end()) SelectedFleet = First->id;
        SelectedCell = State.systems.front().y * State.width + State.systems.front().x;
        if (First != State.fleets.end()) SelectedCell = First->y * State.width + First->x;
        SelectCell(SelectedCell); TradeSource = TradeDestination = IntelTargetSystem = 0; CycleTradeSource();
        LastReport.Empty(); LastEvents.Empty(); BattleIndex = BattleRoundIndex = 0; bAllocationDirty = false; AllocationLocked.fill(false);
        bMenuOpen = bSummaryOpen = bEndTurnCheck = false; MapView = BotfUi::FMapView(); RefreshAll(); ActivePanel = PanelGalaxy;
        if (Chart.IsValid()) Chart->CentreOn(SelectedCell);
        if (Scene.IsValid()) Scene->SetPresentationActive(false);
        Status = FString::Printf(TEXT("The %s awaits your orders. Turn 1. %s ruleset, %d minor races in the galaxy."),
            *Str(botf::empireName(State.playerEmpire)), *Str(botf::view::rulesetLabel(State)), static_cast<int>(State.minors.size()));
    }

    // ---- Save / load ----
    TSharedRef<SWidget> BuildSnapshotBrowser() {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
            [ SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
                [ Label(Txt(TEXT("SAVED CAMPAIGNS")), 14, &FPalette::Tertiary, true) ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
                [ Button(TEXT("REFRESH"), [this] { RefreshSaves(); }, [] { return true; }, 6, FString(), ERole::Tertiary) ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
                [ Button(TEXT("PREVIOUS"), [this] { SavePage = std::max(0, SavePage - 1); },
                    [this] { return SavePage > 0; }, 6, FString(), ERole::Secondary) ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                [ Button(TEXT("NEXT"), [this] { ++SavePage; },
                    [this] { return (SavePage + 1) * 8 < SaveInfos.Num(); }, 6, FString(), ERole::Secondary) ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [ Label(Dyn([this] { return FString::Printf(TEXT("PAGE %d / %d"), SavePage + 1,
                    std::max(1, (SaveInfos.Num() + 7) / 8)); }), 10, &FPalette::Muted) ] ]
            + SVerticalBox::Slot().FillHeight(1)
            [ SNew(SScrollBox) + SScrollBox::Slot() [ SAssignNew(SaveRows, SVerticalBox) ] ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 8)
            [ Section(Txt(TEXT("SELECTED SAVE")), Para(Dyn([this] { return SnapshotPreview(); }), 10, &FPalette::Text)) ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
            [ Control(TEXT("LOAD SELECTED SAVE"), [this] { RequestAction(EPendingAction::LoadCampaign); }, [this] { return !SavePath.IsEmpty(); },
                nullptr, ERole::Accent, FMargin(22, 10), false, 12, FString()) ];
    }
    // A snapshot is either an ordinary campaign or a BOTF_TURN_SESSION envelope saved mid-turn, both complete in one
    // file. Reading, listing, verification and publishing live in FBotfCampaign.
    FBotfSnapshot ReadSnapshot(const FString& Path) const { return Campaign.ReadSnapshot(Path); }
    botf::GameState ReadSave(const FString& Path) const { return ReadSnapshot(Path).State; }
    FString SnapshotFolder() const { return Campaign.SnapshotFolder(); }
    void RefreshSaves() {
        Campaign.RefreshSaves();
        SavePage = std::max(0, Saves.IndexOfByKey(SavePath)) / 8;
    }
    void RefreshSaveRows() {
        if (!SaveRows.IsValid()) return;
        SaveRows->ClearChildren();
        if (SaveInfos.IsEmpty()) {
            SaveRows->AddSlot().AutoHeight().Padding(0, 6)
                [ Para(Txt(TEXT("No saved campaigns yet. Save from the campaign menu at any time.")), 11, &FPalette::Muted) ];
            return;
        }
        SavePage = std::clamp(SavePage, 0, (SaveInfos.Num() - 1) / 8);
        for (int Index = SavePage * 8; Index < std::min(SaveInfos.Num(), (SavePage + 1) * 8); ++Index) {
            const auto& Info = SaveInfos[Index];
            const FString Label = FString::Printf(TEXT("TURN %d  /  %s  /  %s%s\n%s  /  %d orders waiting"), Info.Turn,
                *Str(botf::empireName(Info.Empire)), Info.Ruleset == botf::Ruleset::Reference ? TEXT("CLASSIC") : TEXT("PROTOTYPE"),
                Info.bBattle ? *FString::Printf(TEXT("  /  BATTLE, ROUND %d"), Info.BattleRound) : TEXT(""),
                *Info.Modified.ToString(), Info.Pending);
            SaveRows->AddSlot().AutoHeight().Padding(0, 0, 0, 3)
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0) [ SNew(SBox).WidthOverride(6) [ Swatch(Info.Empire, 0) ] ]
                    + SHorizontalBox::Slot().FillWidth(1)
                    [ Row(Label, [this, Index] {
                        SavePath = SaveInfos[Index].Path;
                        Status = TEXT("Save selected. Review it, then choose Load Selected Save.");
                    }, [this, Index] { return SaveInfos.IsValidIndex(Index) && SaveInfos[Index].Path == SavePath; }) ] ];
        }
    }
    FString SnapshotPreview() const {
        const int Index = Saves.IndexOfByKey(SavePath);
        if (!SaveInfos.IsValidIndex(Index)) return TEXT("No save selected. Damaged or incompatible saves are not listed.");
        const auto& Info = SaveInfos[Index];
        return FString::Printf(TEXT("Turn %d  |  %s  |  %s\n%d systems  |  %d orders waiting  |  seed %llu\nSaved %s%s"),
            Info.Turn, *Str(botf::empireName(Info.Empire)),
            Info.Ruleset == botf::Ruleset::Reference ? TEXT("classic") : TEXT("prototype"),
            Info.Colonies, Info.Pending, static_cast<unsigned long long>(Info.Seed), *Info.Modified.ToString(),
            Info.bBattle ? *FString::Printf(TEXT("\nSaved during turn resolution: battle round %d, %d earlier battle(s) this turn."), Info.BattleRound, Info.BattlesDone) : TEXT(""));
    }
    void Save() {
        // An unapplied intelligence allocation is saved as an order; the draft only changes once the file is published.
        std::optional<botf::Command> Intel;
        if (IntelDraftDirty()) Intel = botf::makeSetIntelBudgetCommand(NextId(), DraftSecurity, DraftIntelBudget);
        Campaign.SaveSnapshot(Intel);
        bIntelDraft = false; DraftIntelBudget.clear();
        SavePage = std::max(0, Saves.IndexOfByKey(SavePath)) / 8;
        Status = Session.active ? FString::Printf(TEXT("Campaign saved mid-battle during turn %d."), State.turn)
            : FString::Printf(TEXT("Campaign saved at turn %d, including orders not yet carried out."), State.turn);
        UE_LOG(LogTemp, Display, TEXT("BOTF_SAVED %s HASH %llu"), *SavePath, static_cast<unsigned long long>(botf::fingerprint(State)));
    }
    void Load() {
        Campaign.LoadSnapshot(SavePath);   // replaces the campaign (selection reset via the Replaced notification)
        bEncounterChoice = false; TacLog.Empty();   // tactical selection is re-validated by OpenEncounter
        SetupRuleset = State.ruleset == botf::Ruleset::Reference ? 1 : 0;
        SetupPlayer = State.playerEmpire;
        SetupEmpires = 1 + static_cast<int>(State.rivals.size());
        SetupEvents = State.eventsEnabled;
        SetupEra = static_cast<int>(State.startingEra);
        // A schema-13 Classic game keeps Classic for the next setup; older saves (every layer off) leave the default.
        SetupPreset = static_cast<int>(State.rules.stations ? State.rules.preset : botf::RulesetPreset::Remastered);
        Selected = 0; SelectedMinor = 0;
        const auto FirstOwned = std::find_if(State.fleets.begin(), State.fleets.end(), [this](const botf::Fleet& F) { return F.ownerId == State.playerEmpire; });
        SelectedFleet = FirstOwned == State.fleets.end() ? 0 : FirstOwned->id;
        SelectedCell = FirstOwned == State.fleets.end() ? State.systems.front().y * State.width + State.systems.front().x
            : FirstOwned->y * State.width + FirstOwned->x;
        SelectCell(SelectedCell); SelectedEmpire = State.playerEmpire == 1 ? 2 : 1;
        TradeSource = TradeDestination = IntelTargetSystem = 0; CycleTradeSource();
        LastReport.Empty(); LastEvents.Empty(); BattleIndex = std::max(0, static_cast<int>(State.latestBattles.size()) - 1);
        BattleRoundIndex = CurrentBattle() ? std::max(0, static_cast<int>(CurrentBattle()->rounds.size()) - 1) : 0;
        bAllocationDirty = false; AllocationLocked.fill(false); bMenuOpen = bSummaryOpen = bEndTurnCheck = false; MapView.bPlotting = false;
        ActivePanel = PanelGalaxy;
        if (Session.pending) {
            OpenEncounter(false);
            TacLog = FString::Printf(TEXT("Battle resumed at round %d."), Session.pending->currentRound);
            Status = FString::Printf(TEXT("Campaign loaded mid-battle: turn %d, round %d."), State.turn, Session.pending->currentRound);
        } else {
            if (Scene.IsValid()) Scene->SetPresentationActive(false);
            Status = FString::Printf(TEXT("Campaign loaded: turn %d, %d orders waiting."), State.turn, static_cast<int>(State.pending.size()));
        }
        UE_LOG(LogTemp, Display, TEXT("BOTF_LOADED turn=%d HASH %llu"), State.turn, static_cast<unsigned long long>(botf::fingerprint(State)));
    }
};

#if WITH_DEV_AUTOMATION_TESTS
// Test-only campaign fixture: a generated classic campaign with two extra player destroyers co-located at the
// home system, a spare unstaffed farm, ample credits and, optionally, a legal war with an enemy destroyer in the
// same (supplied) sector. Validated by core before use; never reachable from production code.
struct FBotfTestFixture {
    botf::GameState State;
    int HomeCell = 0, EnemyOwner = 2;
    std::vector<int> OwnIds;
};
FBotfTestFixture MakeBotfTestFixture(bool bWar) {
    FBotfTestFixture Fx;
    auto& S = Fx.State;
    S = botf::generateReferenceCampaign(1999, 2, 16, 12, 28, 1);
    S.eventsEnabled = false; S.aiAggressionEnabled = false; S.credits = 100000;
    const auto Home = std::find_if(S.colonies.begin(), S.colonies.end(), [&S](const botf::Colony& C) { return C.ownerId == S.playerEmpire; });
    if (Home == S.colonies.end()) throw std::runtime_error("Fixture: player has no colony");
    Home->farms += 1;
    const auto System = std::find_if(S.systems.begin(), S.systems.end(), [&](const botf::System& X) { return X.id == Home->systemId; });
    Fx.HomeCell = System->y * S.width + System->x;
    Fx.EnemyOwner = S.playerEmpire == 1 ? 2 : 1;
    const auto AddShip = [&S, System](int Owner) {
        botf::Fleet F; F.id = S.nextFleetId++; F.ownerId = Owner; F.shipClass = botf::Building::Destroyer;
        F.x = F.targetX = System->x; F.y = F.targetY = System->y;
        F.hull = botf::fleetMaxHull(F); F.shields = botf::fleetMaxShields(F);
        S.fleets.push_back(F);
        return F.id;
    };
    Fx.OwnIds = {AddShip(S.playerEmpire), AddShip(S.playerEmpire)};
    if (bWar) {
        const int First = std::min(S.playerEmpire, Fx.EnemyOwner), Second = std::max(S.playerEmpire, Fx.EnemyOwner);
        auto Relation = std::find_if(S.majorRelations.begin(), S.majorRelations.end(), [&](const botf::MajorRelation& R) { return R.first == First && R.second == Second; });
        if (Relation == S.majorRelations.end()) throw std::runtime_error("Fixture: no relation between the two empires");
        Relation->contact = true; Relation->atWar = true; Relation->tradeTreaty = false; Relation->warStartTurn = S.turn;
        AddShip(Fx.EnemyOwner);
        // A ship that is really in a sector has seen it: core requires every fleet's sector to be explored by its owner.
        // Record that for the enemy destroyer placed over the player's home, as its arrival would have.
        const auto Rival = std::find_if(S.rivals.begin(), S.rivals.end(), [&Fx](const botf::EmpireState& E) { return E.id == Fx.EnemyOwner; });
        if (Rival == S.rivals.end()) throw std::runtime_error("Fixture: enemy empire state missing");
        const auto At = std::lower_bound(Rival->explored.begin(), Rival->explored.end(), Fx.HomeCell);
        if (At == Rival->explored.end() || *At != Fx.HomeCell) Rival->explored.insert(At, Fx.HomeCell);
    }
    botf::validate(S);
    return Fx;
}

// Test-only: opens a Scientific Breakthrough decision for the player now (deadline two turns out), as the curated
// event deck would. Needs a Remastered campaign. Mirrored by the plain-compiler scratch test of BotfGameplayViewModel.h.
int InjectBotfTestEvent(botf::GameState& S) {
    botf::GalacticEvent Event;
    Event.id = S.nextEventId++;
    Event.kind = botf::GalacticEventKind::ScientificBreakthrough;
    Event.empireId = S.playerEmpire;
    Event.startTurn = S.turn;
    Event.choiceDeadline = S.turn + 2;
    Event.endTurn = S.turn + 3;
    S.galacticEvents.push_back(Event);
    botf::validate(S);
    return Event.id;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBotfUiShellAutomationTest, "BOTF.UI.Shell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBotfUiShellAutomationTest::RunTest(const FString& Parameters) {
    const FString TestFolder = FPaths::ProjectDir() / TEXT("Build/UiAutomationSnapshots") / FGuid::NewGuid().ToString();
    try {
        const TSharedRef<SBotfInterface> Ui = SNew(SBotfInterface, static_cast<ABotfCombatScene*>(nullptr))
            .TestSnapshotFolder(TestFolder);
        TestTrue(TEXT("Launch opens the title screen without an active campaign"),
            Ui->Shell == SBotfInterface::EUiView::Title && !Ui->bHasActiveCampaign && Ui->Saves.IsEmpty());

        Ui->OpenSubPage(SBotfInterface::EUiView::Setup);
        Ui->SetupPlayer = 3;
        Ui->SetupEmpires = 5;
        Ui->RequestAction(SBotfInterface::EPendingAction::StartCampaign);
        TestTrue(TEXT("New Campaign starts the selected empire and opens the game"),
            Ui->Shell == SBotfInterface::EUiView::Game && Ui->bHasActiveCampaign && Ui->State.playerEmpire == 3);

        const auto FirstCampaign = botf::fingerprint(Ui->State);
        Ui->OpenSubPage(SBotfInterface::EUiView::Setup);
        Ui->RequestAction(SBotfInterface::EPendingAction::StartCampaign);
        TestTrue(TEXT("Replacing an unsaved campaign requires confirmation"),
            Ui->Shell == SBotfInterface::EUiView::Confirm &&
            Ui->PendingAction == SBotfInterface::EPendingAction::StartCampaign &&
            botf::fingerprint(Ui->State) == FirstCampaign);
        Ui->CancelConfirmation();
        TestTrue(TEXT("Cancel returns to setup without replacing the campaign"),
            Ui->Shell == SBotfInterface::EUiView::Setup &&
            Ui->PendingAction == SBotfInterface::EPendingAction::None &&
            botf::fingerprint(Ui->State) == FirstCampaign);

        Ui->ShowView(SBotfInterface::EUiView::Game);
        Ui->Save();
        const FString BaselinePath = Ui->SavePath;
        const auto Baseline = botf::fingerprint(Ui->State);
        TestTrue(TEXT("Save creates a verified snapshot in the isolated automation folder"),
            IFileManager::Get().FileExists(*BaselinePath) &&
            BaselinePath.StartsWith(Ui->TestSnapshotFolder) && !Ui->HasUnsavedChanges());

        Ui->Allocate(0);
        TestTrue(TEXT("Drafting an order marks the campaign unsaved"), Ui->HasUnsavedChanges());
        Ui->OpenSubPage(SBotfInterface::EUiView::Load);
        Ui->RequestAction(SBotfInterface::EPendingAction::LoadCampaign);
        TestTrue(TEXT("Loading over unsaved orders captures the selected snapshot and asks first"),
            Ui->Shell == SBotfInterface::EUiView::Confirm &&
            Ui->PendingLoadPath == BaselinePath && Ui->HasUnsavedChanges());
        Ui->ConfirmAction(false);
        TestTrue(TEXT("Confirmed load restores the exact saved campaign"),
            Ui->Shell == SBotfInterface::EUiView::Game &&
            botf::fingerprint(Ui->State) == Baseline && !Ui->HasUnsavedChanges());

        Ui->Allocate(1);
        const auto BeforeReplacement = botf::fingerprint(Ui->State);
        Ui->OpenSubPage(SBotfInterface::EUiView::Setup);
        Ui->SetupSeed = 1;
        Ui->RequestAction(SBotfInterface::EPendingAction::StartCampaign);
        Ui->ConfirmAction(true);
        TestTrue(TEXT("Save and proceed preserves the outgoing draft before starting the new campaign"),
            Ui->Shell == SBotfInterface::EUiView::Game && Ui->State.seed == 24680 &&
            botf::fingerprint(Ui->ReadSave(Ui->SavePath)) == BeforeReplacement);

        const auto SettingsCampaign = botf::fingerprint(Ui->State);
        Ui->OpenSubPage(SBotfInterface::EUiView::Settings);
        TestTrue(TEXT("Settings opens the real persistent settings widget"),
            Ui->Shell == SBotfInterface::EUiView::Settings && Ui->SettingsPanel.IsValid());
        Ui->ShowView(Ui->SubPageReturn);
        TestTrue(TEXT("Leaving settings returns to the campaign without changing it"),
            Ui->Shell == SBotfInterface::EUiView::Game && botf::fingerprint(Ui->State) == SettingsCampaign);

        Ui->ShowView(SBotfInterface::EUiView::Menu);
        Ui->RequestAction(SBotfInterface::EPendingAction::Quit);
        TestTrue(TEXT("Quit requires confirmation and has not requested process exit"),
            Ui->Shell == SBotfInterface::EUiView::Confirm &&
            Ui->PendingAction == SBotfInterface::EPendingAction::Quit);
        Ui->CancelConfirmation();
        TestTrue(TEXT("Cancelling Quit returns to the menu with the campaign intact"),
            Ui->Shell == SBotfInterface::EUiView::Menu &&
            botf::fingerprint(Ui->State) == SettingsCampaign);

        // Redesigned screen workflows, driven through the same functions the controls call.
        Ui->ShowView(SBotfInterface::EUiView::Game);
        Ui->GoPanel(PanelSystem);
        TestTrue(TEXT("Opening the Solar System screen selects one of the player's systems"),
            Ui->ActivePanel == PanelSystem && Ui->Owned());
        const auto BeforeResearch = Ui->State.pending.size();
        Ui->AdjustAllocation(3, 10);
        int Total = 0; for (int Share : Ui->DraftAllocation) Total += Share;
        TestTrue(TEXT("Research sliders keep the allocation at 100 percent"), Total == 100 && Ui->bAllocationDirty);
        Ui->ApplyAllocation();
        TestTrue(TEXT("Applying the allocation submits one real research order"),
            Ui->State.pending.size() == BeforeResearch + 1 && !Ui->bAllocationDirty);
        Ui->NextShip();
        if (Ui->Fleet()) {
            const auto* Ship = Ui->Fleet();
            const int Home = Ship->y * Ui->State.width + Ship->x;
            Ui->PlotCourse(Home);
            TestTrue(TEXT("Plotting a course submits a movement order and leaves plotting mode"),
                !Ui->MapView.bPlotting && Ui->State.pending.back().type == botf::CommandType::MoveFleet);
        }

        // Mid-turn transaction: envelope persisted in the same file, dirty tracking and exact restore.
        Ui->ShowView(SBotfInterface::EUiView::Game);
        Ui->Save();
        botf::session::begin(Ui->Session, Ui->State);
        TestTrue(TEXT("An open turn transaction counts as unsaved progress"), Ui->HasUnsavedChanges());
        bool Blocked = false;
        try { Ui->Allocate(2); } catch (const std::exception&) { Blocked = true; }
        TestTrue(TEXT("New strategic orders are refused while a turn is being resolved"), Blocked);
        const auto SessionBaseline = botf::fingerprint(Ui->State);
        Ui->Save();
        FString SessionFile;
        FFileHelper::LoadFileToString(SessionFile, *Ui->SavePath);
        TestTrue(TEXT("Mid-turn save writes the complete turn-session envelope into the snapshot"),
            SessionFile.StartsWith(TEXT("BOTF_TURN_SESSION")) && !Ui->HasUnsavedChanges() &&
            botf::fingerprint(Ui->ReadSave(Ui->SavePath)) == SessionBaseline);
        Ui->RefreshSaves();
        const int SessionIndex = Ui->Saves.IndexOfByKey(Ui->SavePath);
        TestTrue(TEXT("Snapshot browser lists the mid-turn save with battle metadata"),
            Ui->SaveInfos.IsValidIndex(SessionIndex) && Ui->SaveInfos[SessionIndex].bBattle);
        Ui->Session = botf::session::TurnSession();
        TestTrue(TEXT("Dropping the transaction differs from the saved envelope"), Ui->HasUnsavedChanges());
        Ui->OpenSubPage(SBotfInterface::EUiView::Load);
        Ui->RequestAction(SBotfInterface::EPendingAction::LoadCampaign);
        Ui->ConfirmAction(false);
        TestTrue(TEXT("Loading a mid-turn save restores the session, not just its baseline"),
            Ui->Session.active && botf::fingerprint(Ui->State) == SessionBaseline && !Ui->HasUnsavedChanges());
        Ui->OpenSubPage(SBotfInterface::EUiView::Setup);
        Ui->RequestAction(SBotfInterface::EPendingAction::StartCampaign);
        TestTrue(TEXT("A new campaign over a saved open turn starts directly and clears the session"),
            Ui->Shell == SBotfInterface::EUiView::Game && !Ui->Session.active);

        // ---- Deterministic fixtures (test-only; installed directly into this isolated shell) ----
        const auto Install = [&Ui](const FBotfTestFixture& Fx) {
            Ui->ShowView(SBotfInterface::EUiView::Game);
            Ui->State = Fx.State; Ui->Session = botf::session::TurnSession();
            Ui->bHasActiveCampaign = true; Ui->bHasSavedBaseline = false; Ui->SavedSessionText.Empty();
            Ui->bIntelDraft = false; Ui->DraftIntelBudget.clear();
            Ui->SelectedTaskForce = 0; Ui->FormSelection.clear(); Ui->bEncounterChoice = false; Ui->ActivePanel = PanelGalaxy;
            ++Ui->StateVersion;
            Ui->SelectCell(Fx.HomeCell); Ui->RefreshAll();
        };

        // The intelligence controls must preserve a valid whole-empire budget and submit
        // the same allocation shown in their readout, including after a second edit.
        {
            const auto Fx = MakeBotfTestFixture(false);
            Install(Fx);
            const int Rival = Fx.EnemyOwner;
            Ui->BeginIntelDraft();
            const int BeforeSecurity = Ui->IntelSecurity();
            const int BeforeEspionage = Ui->IntelAllocation(Rival, false);
            const int BeforeSabotage = Ui->IntelAllocation(Rival, true);
            Ui->ChangeIntelAllocation(Rival, false, 10);
            TestTrue(TEXT("Intelligence allocation moves points from security to the chosen rival"),
                Ui->IntelSecurity() == BeforeSecurity - 10 && Ui->IntelAllocation(Rival, false) == BeforeEspionage + 10);
            Ui->Submit(botf::makeSetIntelBudgetCommand(Ui->NextId(), Ui->DraftSecurity, Ui->DraftIntelBudget));
            Ui->bIntelDraft = false;
            const auto* Applied = botf::empireIntelState(Ui->Preview(), Ui->State.playerEmpire);
            TestTrue(TEXT("Intel budget control submits an effective campaign order"), Applied &&
                Applied->budget.isValid(Ui->State.playerEmpire) && Applied->budget.internalSecurity.percentage == BeforeSecurity - 10);
            Ui->ChangeIntelAllocation(Rival, true, 5);
            TestTrue(TEXT("Further edits start from the pending allocation"),
                Ui->IntelSecurity() == BeforeSecurity - 15 && Ui->IntelAllocation(Rival, false) == BeforeEspionage + 10
                && Ui->IntelAllocation(Rival, true) == BeforeSabotage + 5);
        }

        // Social affordances and draft guards use the same predicates as their controls.
        {
            const auto Fx = MakeBotfTestFixture(false);
            Install(Fx);
            if (!Ui->State.minors.empty()) {
                auto& Minor = Ui->State.minors.front();
                Minor.memberOf = 0; Minor.contact[Ui->State.playerEmpire] = true;
                botf::syncDiplomacyState(Ui->State);
                Ui->SelectedMinor = Minor.systemId;
                Ui->State.credits = 49; ++Ui->StateVersion;
                TestFalse(TEXT("Gift control is disabled below the 50-credit price"), Ui->CanMinorAction(botf::CommandType::SendGift));
                Ui->State.credits = 50; ++Ui->StateVersion;
                TestTrue(TEXT("Gift control becomes available at the exact price"), Ui->CanMinorAction(botf::CommandType::SendGift));
                Ui->Session.active = true;
                TestFalse(TEXT("Gift control is disabled during a turn transaction"), Ui->CanMinorAction(botf::CommandType::SendGift));
                Ui->Session = botf::session::TurnSession();
            }

            botf::diplomacy::DiplomaticEvent Event;
            Event.turn = Ui->State.turn - 1;
            Event.type = botf::diplomacy::DiplomaticEventType::ProposalAccepted;
            Event.partyA = botf::diplomacy::ParticipantId::major(Ui->State.playerEmpire);
            Event.partyB = botf::diplomacy::ParticipantId::major(Fx.EnemyOwner);
            Event.description = "Visible treaty accepted";
            Ui->State.diplomacyEngineState.eventHistory.push_back(Event);
            Event.partyA = botf::diplomacy::ParticipantId::major(Fx.EnemyOwner);
            Event.partyB = botf::diplomacy::ParticipantId::major(3);
            Event.description = "Foreign private treaty";
            Ui->State.diplomacyEngineState.eventHistory.push_back(Event);
            const FString HistoryText = Ui->DiplomacyEventsText();
            TestTrue(TEXT("Diplomacy Events shows resolved player history"), HistoryText.Contains(TEXT("Visible treaty accepted")));
            TestFalse(TEXT("Diplomacy Events excludes foreign-only history"), HistoryText.Contains(TEXT("Foreign private treaty")));

            // Calls to arms: listed until the player answers; REFUSE lets the call lapse and hides it.
            botf::diplomacy::DefenseCall Call;
            Call.ally = Ui->State.playerEmpire; Call.defender = Fx.EnemyOwner; Call.aggressor = 3;
            Call.issuedTurn = Ui->State.turn; Call.dueTurn = Ui->State.turn + 3;
            Ui->State.diplomacyEngineState.defenseCalls.push_back(Call);
            ++Ui->StateVersion;
            TestTrue(TEXT("An ally's call to arms reaches the player's alerts"),
                Ui->OpenCallsToArms().size() == 1 && FString::Join(Ui->Alerts(), TEXT(" ")).Contains(TEXT("calls on you")));
            Ui->RefusedCalls.insert(std::make_tuple(Call.ally, Call.defender, Call.aggressor, Call.issuedTurn));
            TestTrue(TEXT("Refusing a call to arms hides it"), Ui->OpenCallsToArms().empty());

            Install(Fx);
            Ui->Save();
            const FString IntelBaselinePath = Ui->SavePath;
            Ui->BeginIntelDraft();
            Ui->ChangeIntelAllocation(Fx.EnemyOwner, false, 5);
            TestTrue(TEXT("An unapplied intelligence edit marks the campaign dirty"), Ui->IntelDraftDirty() && Ui->HasUnsavedChanges());
            TestTrue(TEXT("End Turn warns that an unapplied intelligence edit will be discarded"),
                FString::Join(Ui->EndTurnWarnings(), TEXT(" ")).Contains(TEXT("intelligence allocation")));
            Ui->RequestAction(SBotfInterface::EPendingAction::LoadCampaign);
            TestTrue(TEXT("Loading over an intelligence edit asks before replacement"), Ui->Shell == SBotfInterface::EUiView::Confirm);
            Ui->CancelConfirmation();
            Ui->Save();
            TestTrue(TEXT("Saving applies the intelligence edit as a persistent order"), !Ui->IntelDraftDirty() && !Ui->HasUnsavedChanges()
                && !Ui->State.pending.empty() && Ui->State.pending.back().type == botf::CommandType::SetIntelBudget
                && botf::fingerprint(Ui->ReadSave(Ui->SavePath)) == botf::fingerprint(Ui->State));
            Ui->ChangeIntelAllocation(Fx.EnemyOwner, true, 5);
            Ui->SavePath = IntelBaselinePath;
            Ui->RequestAction(SBotfInterface::EPendingAction::LoadCampaign);
            Ui->ConfirmAction(false);
            TestTrue(TEXT("Confirmed replacement clears an unapplied intelligence edit"), !Ui->bIntelDraft && !Ui->HasUnsavedChanges());
        }

        // Buy, scrap and task forces on a peaceful fixture, committed through one real turn.
        {
            const auto Fx = MakeBotfTestFixture(false);
            Install(Fx);
            TestTrue(TEXT("Fixture selects the player's home system"), Ui->Owned());
            if (const auto* Col = Ui->DraftColony(); Col && Col->queue.empty())
                for (const auto& Choice : botf::constructionChoices(Ui->Preview(), Ui->Selected))
                    if (Choice.blocker.empty() && !botf::isShip(Choice.product)) {
                        botf::Command Cmd; Cmd.id = Ui->NextId(); Cmd.type = botf::CommandType::EnqueueBuilding;
                        Cmd.systemId = Ui->Selected; Cmd.building = Choice.product; Cmd.referenceId = Choice.referenceId;
                        Ui->Submit(Cmd); break;
                    }
            TestTrue(TEXT("Buy is offered with a cost and no blocker when credits suffice"),
                Ui->ColonyChecks().Buy.IsEmpty() && botf::rushCost(Ui->Preview(), Ui->Selected) > 0);
            Ui->BuyFirst();
            TestTrue(TEXT("Buy submits a real BuyConstruction order"), Ui->State.pending.back().type == botf::CommandType::BuyConstruction);
            TestTrue(TEXT("An unstaffed spare farm can be scrapped"), Ui->ColonyChecks().Scrap[0].IsEmpty());
            Ui->ScrapStructure(botf::Building::Farm);
            TestTrue(TEXT("Scrap submits a real ScrapStructure order"), Ui->State.pending.back().type == botf::CommandType::ScrapStructure);
            TestTrue(TEXT("Fusion plants are their own structure with core's own scrap check"),
                SBotfInterface::ScrapKinds[4] == botf::Building::FusionPlant && (Ui->DraftColony()->fusionPlants > 0 || !Ui->ColonyChecks().Scrap[4].IsEmpty()));

            {   // Drafted moves are replaced, not stacked, and HOLD withdraws a drafted course.
                Ui->SelectedFleet = Fx.OwnIds[0]; Ui->SelectedTaskForce = 0;
                const int East = Fx.HomeCell % Fx.State.width + 1 < Fx.State.width ? Fx.HomeCell + 1 : Fx.HomeCell - 1;
                const auto Moves = [&Ui, &Fx] { return std::count_if(Ui->State.pending.begin(), Ui->State.pending.end(), [&Fx](const botf::Command& O) {
                    return O.fleetId == Fx.OwnIds[0] && O.type == botf::CommandType::MoveFleet; }); };
                Ui->PlotCourse(East);
                Ui->PlotCourse(East);
                TestTrue(TEXT("A second course for the same ship replaces the first"), Moves() == 1);
                const auto* Drafted = Ui->DraftFleet();
                TestTrue(TEXT("The drafted course is visible from the preview"), Drafted && (Drafted->targetX != Drafted->x || Drafted->targetY != Drafted->y));
                Ui->HoldSelected();
                const auto* Held = Ui->DraftFleet();
                TestTrue(TEXT("HOLD removes the drafted course"), Moves() == 0 && Held && Held->targetX == Held->x && Held->targetY == Held->y);
            }
            Ui->FormSelection = {Fx.OwnIds[0], Fx.OwnIds[1]};
            Ui->FormTaskForce();
            TestTrue(TEXT("Forming clears the marks so the same ships cannot be grouped twice"),
                Ui->FormSelection.empty() && !Ui->ValidFormMarks());
            const auto* Group = Ui->CurrentTaskForce();
            TestTrue(TEXT("Forming a task force groups both co-located ships and selects the group"),
                Group && botf::fleetTaskForce(Ui->Preview(), Fx.OwnIds[0]) == Group && botf::fleetTaskForce(Ui->Preview(), Fx.OwnIds[1]) == Group);
            const int Adjacent = Fx.HomeCell % Fx.State.width + 1 < Fx.State.width ? Fx.HomeCell + 1 : Fx.HomeCell - 1;
            Ui->PlotCourse(Adjacent);
            TestTrue(TEXT("A course for a task force is a group order"), Ui->State.pending.back().type == botf::CommandType::SetTaskForceOrder);
            Ui->DisbandTaskForce();
            TestTrue(TEXT("Splitting returns both ships to individual command"),
                Ui->SelectedTaskForce == 0 && !botf::fleetTaskForce(Ui->Preview(), Fx.OwnIds[0]) && !botf::fleetTaskForce(Ui->Preview(), Fx.OwnIds[1]));
            Ui->FormSelection = {Fx.OwnIds[0], Fx.OwnIds[1]};
            Ui->FormTaskForce();
            const int TurnBefore = Ui->State.turn;
            const auto Forecast = Ui->IncomeForecast();
            TestTrue(TEXT("The per-turn forecast includes ship maintenance for every own ship"),
                Forecast.Ships > 0 && Forecast.Net() == Forecast.Colonies + Forecast.Trade - Forecast.Ships);
            Ui->EndTurn();
            const auto* Kept = botf::fleetTaskForce(Ui->State, Fx.OwnIds[0]);
            TestTrue(TEXT("A peaceful End Turn commits exactly one turn and keeps the task force"),
                !Ui->Session.active && Ui->State.turn == TurnBefore + 1 && Kept && Kept == botf::fleetTaskForce(Ui->State, Fx.OwnIds[1]));
            TestTrue(TEXT("Forecast ship maintenance and trade match what core charged at turn end"),
                Ui->bLastForecast && Ui->LastForecast.Ships == Ui->LastFleetUpkeep && Ui->LastForecast.Trade == Ui->LastTradeIncome);
        }

        // Live encounter: real End Turn pauses, Fight, orders, rounds, mid-battle save/load, resume, single commit.
        {
            const auto Fx = MakeBotfTestFixture(true);
            Install(Fx);
            const auto PreFingerprint = botf::fingerprint(Ui->State);
            const int PreTurn = Ui->State.turn;
            Ui->EndTurn();
            TestTrue(TEXT("End Turn with enemies in the same sector pauses for the encounter dialog"),
                Ui->Session.active && Ui->Session.pending.has_value() && Ui->bEncounterChoice && Ui->ActivePanel == PanelCombat);
            TestTrue(TEXT("The committed campaign is unchanged while the battle is pending"),
                botf::fingerprint(Ui->State) == PreFingerprint && Ui->State.turn == PreTurn);
            if (!Ui->Session.pending) throw std::runtime_error("Live encounter fixture did not produce a pending encounter");
            {
                Ui->bEncounterChoice = false;   // FIGHT
                const auto* Own = Ui->TacShipPtr(Ui->TacShip);
                const auto* Foe = Ui->TacShipPtr(Ui->TacTarget);
                TestTrue(TEXT("Fight selects one of the player's ships and an enemy target"),
                    Own && Own->ownerId == Ui->State.playerEmpire && Foe && Foe->ownerId == Fx.EnemyOwner);
                Ui->NextTacShip();
                const auto* NextOwn = Ui->TacShipPtr(Ui->TacShip);
                TestTrue(TEXT("NextTacShip cycles to an owned ship"), NextOwn && NextOwn->ownerId == Ui->State.playerEmpire);
                Ui->NextTacTarget();
                const auto* NextFoe = Ui->TacShipPtr(Ui->TacTarget);
                TestTrue(TEXT("NextTacTarget cycles to an enemy ship"), NextFoe && NextFoe->ownerId != Ui->State.playerEmpire);
                Ui->TacOrder(botf::tactical::TacticalManeuver::Charge);
                const auto Given = Ui->Session.pending->getOrder(Ui->TacShip);
                TestTrue(TEXT("A manoeuvre order reaches the encounter with its target"),
                    Given.maneuver == botf::tactical::TacticalManeuver::Charge && Given.targetFleetId == Ui->TacTarget);
                Ui->TacNextRound();
                TestTrue(TEXT("Next Round resolves exactly one tactical round"),
                    Ui->Session.pending->currentRound == 1 && Ui->Session.pending->rounds.size() == 1);

                {   // A failed tactical order is reported and leaves the encounter untouched.
                    const FString Before = Ui->SessionText();
                    const int Mine = Ui->TacShip;
                    Ui->TacShip = Ui->TacTarget;   // an enemy ship cannot take orders
                    bool Rejected = false;
                    try { Ui->TacOrder(botf::tactical::TacticalManeuver::Engage); } catch (const std::exception&) { Rejected = true; }
                    Ui->TacShip = Mine;
                    TestTrue(TEXT("An invalid tactical order is rejected without changing the battle"), Rejected && Ui->SessionText() == Before);
                }
                const int KeptShip = Ui->TacShip, KeptTarget = Ui->TacTarget;
                Ui->ShowView(SBotfInterface::EUiView::Menu);
                Ui->ShowView(SBotfInterface::EUiView::Game);
                TestTrue(TEXT("The live encounter and its selection survive a trip to the menu"),
                    Ui->Session.pending.has_value() && Ui->TacShip == KeptShip && Ui->TacTarget == KeptTarget && Ui->ActivePanel == PanelCombat);
                Ui->Save();
                const auto Snapshot = Ui->ReadSnapshot(Ui->SavePath);
                const FString Envelope = Ui->SessionText();
                TestTrue(TEXT("Mid-battle save stores the live encounter at round 1"),
                    Snapshot.Session && Snapshot.Session->pending && Snapshot.Session->pending->currentRound == 1 && !Ui->HasUnsavedChanges());
                Ui->TacOrder(botf::tactical::TacticalManeuver::Hold);
                TestTrue(TEXT("A new tactical order after saving counts as unsaved progress"), Ui->HasUnsavedChanges());
                Ui->OpenSubPage(SBotfInterface::EUiView::Load);
                Ui->RequestAction(SBotfInterface::EPendingAction::LoadCampaign);
                TestTrue(TEXT("Loading over unsaved tactical orders asks first"), Ui->Shell == SBotfInterface::EUiView::Confirm);
                Ui->ConfirmAction(false);
                TestTrue(TEXT("Loading restores the exact encounter and the untouched baseline"),
                    Ui->SessionText() == Envelope && botf::fingerprint(Ui->State) == PreFingerprint &&
                    Ui->ActivePanel == PanelCombat && !Ui->bEncounterChoice && Ui->Session.pending->currentRound == 1);
                TestTrue(TEXT("Loading keeps the still-valid ship and target selection"),
                    (!Ui->TacActive(Ui->TacShipPtr(KeptShip)) || Ui->TacShip == KeptShip) && (!Ui->TacActive(Ui->TacShipPtr(KeptTarget)) || Ui->TacTarget == KeptTarget));

                for (int Guard = 0; Guard < 60 && Ui->Session.pending && !Ui->Session.pending->isFinished; ++Guard) {
                    try { Ui->TacOrder(botf::tactical::TacticalManeuver::Engage); } catch (const std::exception&) {}
                    Ui->TacNextRound();
                }
                TestTrue(TEXT("The manual battle reaches a result"), Ui->Session.pending && Ui->Session.pending->isFinished);
                Ui->TacContinue();
                for (int Guard = 0; Guard < 8 && Ui->Session.pending; ++Guard) { Ui->TacAuto(); Ui->TacContinue(); }
                TestTrue(TEXT("Resuming after the battle commits exactly one strategic turn"),
                    !Ui->Session.active && Ui->State.turn == PreTurn + 1);
                TestTrue(TEXT("The committed turn records the fought battle"),
                    !Ui->State.latestBattles.empty() &&
                    Ui->State.latestBattles.back().x == Fx.HomeCell % Fx.State.width && Ui->State.latestBattles.back().y == Fx.HomeCell / Fx.State.width);

                // Battle records after the commit: the replay is drawn from the record, both sides, round by round.
                const auto ReplayReadable = [this, &Ui](const TCHAR* When) {
                    const auto* B = Ui->CurrentBattle();
                    const auto First = B ? BotfUi::BattleShipStates(*B, 0) : std::vector<BotfUi::FBattleShipState>();
                    const auto Final = B ? BotfUi::BattleShipStates(*B, -1) : std::vector<BotfUi::FBattleShipState>();
                    const bool BothSides = std::any_of(First.begin(), First.end(), [](const auto& S) { return S.Side == 0; })
                        && std::any_of(First.begin(), First.end(), [](const auto& S) { return S.Side == 1; });
                    const bool Changed = std::any_of(Final.begin(), Final.end(), [](const auto& S) { return S.bLost || S.bRetreated || S.Hull < S.MaxHull || S.Shields < S.MaxShields; });
                    TestTrue(FString(TEXT("Battle replay has both fleets and the fight's damage: ")) + When,
                        Ui->ActivePanel == PanelCombat && B && BothSides && Changed && Ui->WantedBattle().bValid);
                };
                Ui->GoPanel(PanelCombat);
                ReplayReadable(TEXT("after the committed battle"));
                Ui->SelectBattleRoundAbsolute(0);
                TestTrue(TEXT("FIRST selects the first recorded round"), Ui->BattleRoundIndex == 0 && Ui->WantedBattle().Round <= 0);
                Ui->GoPanel(PanelGalaxy);
                Ui->Save();
                Ui->OpenSubPage(SBotfInterface::EUiView::Load);
                Ui->RequestAction(SBotfInterface::EPendingAction::LoadCampaign);
                if (Ui->Shell == SBotfInterface::EUiView::Confirm) Ui->ConfirmAction(false);
                Ui->GoPanel(PanelCombat);
                ReplayReadable(TEXT("after loading the saved campaign"));
                Ui->GoPanel(PanelGalaxy);
            }
        }
        // Command deck interaction, driven through the same entry points the chart and HUD call.
        {
            const auto Fx = MakeBotfTestFixture(false);
            Install(Fx);
            Ui->SelectedFleet = 0; Ui->SelectedTaskForce = 0;
            Ui->ClickCell(Fx.HomeCell);
            TestTrue(TEXT("Clicking a sector with own ships selects that sector and one of its units"),
                Ui->SelectedCell == Fx.HomeCell && Ui->Fleet() != nullptr);
            const int First = Ui->SelectedFleet;
            Ui->ClickCell(Fx.HomeCell);
            TestTrue(TEXT("Clicking the selected sector again cycles to another unit"), Ui->SelectedFleet != First && Ui->Fleet() != nullptr);

            Ui->OpenSectorMenu(Fx.HomeCell, FVector2D(100, 100));
            const auto Actions = Ui->SectorActions(Fx.HomeCell);
            TestTrue(TEXT("The sector context menu opens with actions for the home system"),
                Ui->bMenuOpen && Ui->MenuCell == Fx.HomeCell && Ui->MenuRows.IsValid() && Ui->MenuRows->NumSlots() > 2 &&
                std::any_of(Actions.begin(), Actions.end(), [](const auto& A) { return A.Label == TEXT("Open system"); }));
            Ui->bMenuOpen = false;

            // Right click with a unit selected on another reachable sector drafts exactly one course for it.
            const int UnitId = Ui->SelectedFleet;
            const auto* Unit = Ui->Fleet();
            int Destination = -1;
            for (int Cell = 0; Cell < Fx.State.width * Fx.State.height && Destination < 0; ++Cell)
                if (Cell != Fx.HomeCell && botf::route(Ui->Preview(), *Unit, Cell % Fx.State.width, Cell / Fx.State.width).size() == 2) Destination = Cell;
            TestTrue(TEXT("Fixture has a sector one jump from home"), Destination >= 0);
            if (Destination >= 0) {
                const auto Moves = [&Ui, UnitId] { return std::count_if(Ui->State.pending.begin(), Ui->State.pending.end(), [UnitId](const botf::Command& O) {
                    return O.fleetId == UnitId && O.type == botf::CommandType::MoveFleet; }); };
                Ui->CommandCell(Destination, FVector2D(50, 50), false);
                Ui->CommandCell(Destination, FVector2D(50, 50), false);
                TestTrue(TEXT("Right-clicking a destination orders the selected unit there, replacing rather than stacking"),
                    Moves() == 1 && !Ui->bMenuOpen && Ui->CellHint(Destination).Contains(TEXT("1 turn")));
                Ui->CommandCell(Destination, FVector2D(50, 50), true);
                TestTrue(TEXT("Shift+right-click opens the options menu without giving an order"), Ui->bMenuOpen && Moves() == 1);
                Ui->bMenuOpen = false;
            }

            Ui->ActivateCell(Fx.HomeCell);
            TestTrue(TEXT("Double-clicking an own colony opens its System sheet"), Ui->ActivePanel == PanelSystem && Ui->Owned());
            Ui->GoPanel(PanelGalaxy);

            const int SystemId = Ui->Selected;
            const auto Staffing = [&Ui, SystemId] { return std::count_if(Ui->State.pending.begin(), Ui->State.pending.end(), [SystemId](const botf::Command& O) {
                return O.type == botf::CommandType::AssignWorkers && O.systemId == SystemId; }); };
            const int FarmBlocks = Ui->DraftColony()->foodWorkers / 10;
            TestTrue(TEXT("Fixture home system has staffed farms"), FarmBlocks >= 1);
            if (FarmBlocks >= 1) {
                Ui->SetWorkerBlocks(0, FarmBlocks - 1);   // frees ten workers into the labor pool
                Ui->SetWorkerBlocks(0, FarmBlocks);
                Ui->SetWorkerBlocks(0, FarmBlocks - 1);
                TestTrue(TEXT("Dragging a workforce bar leaves one staffing order with the final value"),
                    Staffing() == 1 && Ui->DraftColony()->foodWorkers == (FarmBlocks - 1) * 10);
            }

            const int TurnBefore = Ui->State.turn;
            Ui->RefreshAll();
            TestTrue(TEXT("The turn inbox lists items"), Ui->InboxCount > 0);
            Ui->AdjustAllocation(0, 5);   // an unapplied research change: a real end-turn warning
            TestTrue(TEXT("An unapplied research change raises an end-turn warning"), !Ui->EndTurnWarnings().IsEmpty());
            if (!Ui->EndTurnWarnings().IsEmpty()) {
                Ui->RequestEndTurn();
                TestTrue(TEXT("End Turn with open issues asks first and does not resolve the turn"), Ui->bEndTurnCheck && Ui->State.turn == TurnBefore);
                Ui->RequestEndTurn();   // a second END TURN click / Ctrl+Enter behind the warning (Astra's repro)
                TestTrue(TEXT("Repeating End Turn while the warning is open neither resolves the turn nor closes the warning"),
                    Ui->bEndTurnCheck && Ui->State.turn == TurnBefore && !Ui->Session.active);
                const bool SummaryBefore = Ui->bSummaryOpen;
                Ui->OnKeyDown(FGeometry(), FKeyEvent(EKeys::S, FModifierKeysState(), 0, false, 0, 0));
                TestTrue(TEXT("Shortcuts are held back while the warning is open"), Ui->bSummaryOpen == SummaryBefore && Ui->bEndTurnCheck);
                Ui->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
                TestTrue(TEXT("Esc reviews: the warning closes and the turn is not resolved"), !Ui->bEndTurnCheck && Ui->State.turn == TurnBefore);
                Ui->RequestEndTurn();
                Ui->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Enter, FModifierKeysState(), 0, false, 0, 0));
                TestTrue(TEXT("Enter inside the warning is the explicit END TURN ANYWAY and resolves exactly one turn"),
                    !Ui->bEndTurnCheck && !Ui->Session.active && Ui->State.turn == TurnBefore + 1);
            }
        }

        // Review P1 (27 Sep): loading must drop every selection made in the outgoing campaign. The loaded save holds a
        // task force with the same id as the one selected, so a stale id would silently resolve to the loaded group.
        {
            const auto Fx = MakeBotfTestFixture(false);
            Install(Fx);
            Ui->FormSelection = {Fx.OwnIds[0], Fx.OwnIds[1]};
            Ui->FormTaskForce();
            const int GroupId = Ui->SelectedTaskForce;
            TestTrue(TEXT("Fixture selects a drafted task force"), GroupId != 0 && Ui->CurrentTaskForce() != nullptr);
            Ui->Save();
            Ui->ExpandedTaskForces.insert(GroupId);
            Ui->FormSelection = {Fx.OwnIds[0]};
            Ui->SelectedEnemyFleet = Fx.OwnIds[1];
            Ui->Allocate(3);   // unsaved change, so the load goes through the confirmation
            Ui->OpenSubPage(SBotfInterface::EUiView::Load);
            Ui->RequestAction(SBotfInterface::EPendingAction::LoadCampaign);
            Ui->ConfirmAction(false);
            TestTrue(TEXT("The loaded save still contains a task force with the previously selected id"),
                botf::taskForceAt(Ui->Preview(), GroupId) != nullptr);
            TestTrue(TEXT("Loading clears task force, formation marks, expanded groups and enemy selection"),
                Ui->SelectedTaskForce == 0 && Ui->FormSelection.empty() && Ui->ExpandedTaskForces.empty() && Ui->SelectedEnemyFleet == 0
                && !Ui->bEndTurnCheck && !Ui->bMenuOpen && !Ui->bLastForecast);
        }

        // Review P2: an economic-map trade route line must not reveal a system the player has not charted.
        {
            auto S = MakeBotfTestFixture(false).State;
            const auto Home = std::find_if(S.colonies.begin(), S.colonies.end(), [&S](const botf::Colony& C) { return C.ownerId == S.playerEmpire; });
            const auto Hidden = std::find_if(S.systems.begin(), S.systems.end(), [&S](const botf::System& X) { return !botf::explored(S, X.x, X.y); });
            const auto Charted = std::find_if(S.systems.begin(), S.systems.end(), [&S, &Home](const botf::System& X) { return X.id != Home->systemId && botf::explored(S, X.x, X.y); });
            TestTrue(TEXT("Fixture has an uncharted system"), Hidden != S.systems.end());
            if (Hidden != S.systems.end()) {
                const int Rival = S.playerEmpire == 1 ? 2 : 1;
                TestFalse(TEXT("A rival route into uncharted space is not drawn"), BotfUi::TradeRouteKnown(S, botf::TradeRoute{Rival, Home->systemId, Hidden->id}));
                TestFalse(TEXT("Even an own route to an uncharted system is not drawn"), BotfUi::TradeRouteKnown(S, botf::TradeRoute{S.playerEmpire, Home->systemId, Hidden->id}));
            }
            if (Charted != S.systems.end())
                TestTrue(TEXT("An own route between charted systems is drawn"), BotfUi::TradeRouteKnown(S, botf::TradeRoute{S.playerEmpire, Home->systemId, Charted->id}));
        }

        // Review P2: every empire is playable, and the power count offered matches what core generates.
        {
            Ui->OpenSubPage(SBotfInterface::EUiView::Setup);
            Ui->SetupPlayer = 4; Ui->SetupEmpires = 4; Ui->SetupSeed = 0;
            Ui->RequestAction(SBotfInterface::EPendingAction::StartCampaign);
            if (Ui->Shell == SBotfInterface::EUiView::Confirm) Ui->ConfirmAction(false);
            TestTrue(TEXT("The Cardassians start in a four-power galaxy"),
                Ui->Shell == SBotfInterface::EUiView::Game && Ui->State.playerEmpire == 4 && Ui->State.rivals.size() == 3);
            TestTrue(TEXT("Setup copy names the four powers in play"), Ui->SetupPowersNote().Contains(TEXT("Cardassian")) && !Ui->SetupPowersNote().Contains(TEXT("Ferengi")));
        }

        // Selectable starting eras: UI setup options, accurate conditions description, generation, and persistence.
        {
            Ui->OpenSubPage(SBotfInterface::EUiView::Setup);
            Ui->SetupRuleset = 1; // Classic Catalogue
            Ui->SetupEra = static_cast<int>(botf::StartingEra::Advanced);
            TestTrue(TEXT("Setup era note describes Advanced era starting conditions"),
                Ui->SetupEraNote().Contains(TEXT("Level 7 technology")) && Ui->SetupEraNote().Contains(TEXT("7,000 credits")) && Ui->SetupEraNote().Contains(TEXT("95 population")));

            Ui->SetupRuleset = 0; // Prototype Galaxy
            TestTrue(TEXT("Setup era note reflects Prototype credit and tech baseline"),
                Ui->SetupEraNote().Contains(TEXT("2,200 credits")) &&
                Ui->SetupEraNote().Contains(TEXT("4 destroyers")) &&
                !Ui->SetupEraNote().Contains(TEXT("cruiser")) &&
                !Ui->SetupEraNote().Contains(TEXT("capital warship")));
            Ui->SetupEra = static_cast<int>(botf::StartingEra::Expanded);
            TestTrue(TEXT("Expanded Prototype note names its two generic destroyers"),
                Ui->SetupEraNote().Contains(TEXT("2 destroyers")) && !Ui->SetupEraNote().Contains(TEXT("cruiser")));
            Ui->SetupEra = static_cast<int>(botf::StartingEra::Supreme);
            TestTrue(TEXT("Supreme Prototype note names generic destroyers, not a dreadnought"),
                Ui->SetupEraNote().Contains(TEXT("4 destroyers")) && !Ui->SetupEraNote().Contains(TEXT("dreadnought")));

            // Start an Advanced era campaign in Classic Catalogue
            Ui->SetupRuleset = 1;
            Ui->SetupPlayer = 1; // Federation
            Ui->SetupEmpires = 2;
            Ui->SetupSeed = 0;
            Ui->SetupEra = static_cast<int>(botf::StartingEra::Advanced);
            Ui->RequestAction(SBotfInterface::EPendingAction::StartCampaign);
            if (Ui->Shell == SBotfInterface::EUiView::Confirm) Ui->ConfirmAction(false);

            TestTrue(TEXT("Advanced campaign starts with StartingEra::Advanced"),
                Ui->State.startingEra == botf::StartingEra::Advanced);
            TestTrue(TEXT("Advanced campaign grants Level 7 base tech"),
                Ui->State.technologies[0].level == 7);
            TestTrue(TEXT("Advanced campaign starting credits match Reference ladder (7000)"),
                Ui->State.credits == 7000);

            // Verify save/load retention
            Ui->Save();
            const auto AdvancedFingerprint = botf::fingerprint(Ui->State);
            Ui->OpenSubPage(SBotfInterface::EUiView::Load);
            Ui->RequestAction(SBotfInterface::EPendingAction::LoadCampaign);
            if (Ui->Shell == SBotfInterface::EUiView::Confirm) Ui->ConfirmAction(false);

            TestTrue(TEXT("Loaded campaign retains StartingEra::Advanced across save/load"),
                Ui->State.startingEra == botf::StartingEra::Advanced && Ui->SetupEra == static_cast<int>(botf::StartingEra::Advanced));
            TestTrue(TEXT("Loaded campaign fingerprint matches pre-save state"),
                botf::fingerprint(Ui->State) == AdvancedFingerprint);

            // Test Supreme era in Prototype rules
            Ui->OpenSubPage(SBotfInterface::EUiView::Setup);
            Ui->SetupRuleset = 0;
            Ui->SetupEra = static_cast<int>(botf::StartingEra::Supreme);
            Ui->RequestAction(SBotfInterface::EPendingAction::StartCampaign);
            if (Ui->Shell == SBotfInterface::EUiView::Confirm) Ui->ConfirmAction(false);

            TestTrue(TEXT("Supreme Prototype campaign starts with StartingEra::Supreme"),
                Ui->State.startingEra == botf::StartingEra::Supreme);
            TestTrue(TEXT("Supreme Prototype campaign grants Level 10 tech"),
                Ui->State.technologies[0].level == 10);
            TestTrue(TEXT("Supreme Prototype credits match Prototype ladder (3500)"),
                Ui->State.credits == 3500);
        }

        // Lifecycle regression (BOTF.UI.Shell 2026-09-27): replacing the campaign must never serve the previous draft.
        {
            Ui->ShowView(SBotfInterface::EUiView::Game);
            const auto Before = Ui->Preview().seed;
            Ui->State = botf::generateReferenceCampaign(Before == 24680 ? 1999 : 24680, 2, 16, 12, 28, 2);   // deliberately no version bump
            const auto Home = std::find_if(Ui->State.colonies.begin(), Ui->State.colonies.end(), [&Ui](const botf::Colony& C) { return C.ownerId == Ui->State.playerEmpire; });
            const auto Sys = std::find_if(Ui->State.systems.begin(), Ui->State.systems.end(), [&Home](const botf::System& S) { return S.id == Home->systemId; });
            Ui->SelectCell(Sys->y * Ui->State.width + Sys->x);
            TestTrue(TEXT("A replaced campaign never reuses the previous campaign's draft preview"),
                Ui->Preview().seed == Ui->State.seed && Ui->Preview().playerEmpire == Ui->State.playerEmpire && Ui->Owned());
        }

        // Colony automation and queue pinning (Simon play feedback: automatic food assignment, independent governors, manual override, pin persistence)
        {
            const auto Fx = MakeBotfTestFixture(false);
            Install(Fx);
            TestTrue(TEXT("Fixture selects the player's home system"), Ui->Owned());
            const auto* ColInitial = Ui->DraftColony();
            TestTrue(TEXT("Colony exists"), ColInitial != nullptr);
            if (ColInitial) {
                // 1. Initial baseline: governors are Manual, no manual override flag
                TestTrue(TEXT("Initial workforce priority is Manual"), ColInitial->workforcePriority == botf::GovernorPriority::Manual);
                TestTrue(TEXT("Initial queue priority is Manual"), ColInitial->queuePriority == botf::GovernorPriority::Manual);
                TestFalse(TEXT("Initial workforceManualOverride is false"), ColInitial->workforceManualOverride);

                // 2. Set Workforce Governor to Growth
                Ui->SetWorkforceGovernor(botf::GovernorPriority::Growth);
                const auto* ColAfterWf = Ui->DraftColony();
                TestTrue(TEXT("Draft colony reflects Growth workforce priority"),
                    ColAfterWf && ColAfterWf->workforcePriority == botf::GovernorPriority::Growth);
                TestFalse(TEXT("Workforce override is false after setting governor"),
                    ColAfterWf && ColAfterWf->workforceManualOverride);
                TestFalse(TEXT("Workforce governor explanation is generated"),
                    ColAfterWf && ColAfterWf->workforceExplanation.empty());

                // 3. Manual override detection when player adjusts workers
                if (Ui->CanAdjustWorkers(0, -10)) {
                    Ui->AdjustWorkers(0, -10);
                } else if (Ui->CanAdjustWorkers(1, 10)) {
                    Ui->AdjustWorkers(1, 10);
                } else if (Ui->CanAdjustWorkers(2, 10)) {
                    Ui->AdjustWorkers(2, 10);
                }
                const auto* ColAfterManual = Ui->DraftColony();
                TestTrue(TEXT("Workforce manual override is flagged after player worker adjustment"),
                    ColAfterManual && ColAfterManual->workforceManualOverride);

                // 4. Resume governor via ClearGovernorOverride
                Ui->ClearGovernorOverride();
                const auto* ColAfterResume = Ui->DraftColony();
                TestFalse(TEXT("Workforce manual override is cleared on resume"),
                    ColAfterResume && ColAfterResume->workforceManualOverride);

                // 5. Set Queue Governor to Manufacturing
                Ui->SetQueueGovernor(botf::GovernorPriority::Manufacturing);
                const auto* ColAfterQueue = Ui->DraftColony();
                TestTrue(TEXT("Draft colony reflects Manufacturing queue priority"),
                    ColAfterQueue && ColAfterQueue->queuePriority == botf::GovernorPriority::Manufacturing);
                TestFalse(TEXT("Queue governor explanation is generated"),
                    ColAfterQueue && ColAfterQueue->queueExplanation.empty());

                // 6. Test Queue Pinning
                if (ColAfterQueue && ColAfterQueue->queue.empty()) {
                    botf::Command Enq; Enq.id = Ui->State.lastAcceptedCommand + 1;
                    Enq.type = botf::CommandType::EnqueueBuilding; Enq.systemId = Ui->Selected;
                    Enq.building = botf::Building::Factory;
                    Ui->Submit(Enq);
                }
                const auto* ColWithQueue = Ui->DraftColony();
                TestTrue(TEXT("Colony has at least one queued item to pin"), ColWithQueue && !ColWithQueue->queue.empty());
                if (ColWithQueue && !ColWithQueue->queue.empty()) {
                    TestFalse(TEXT("Queue item starts unpinned"), ColWithQueue->queue[0].pinned);
                    Ui->ToggleQueuePin(0);
                    const auto* ColPinned = Ui->DraftColony();
                    TestTrue(TEXT("Queue item is now pinned"), ColPinned && ColPinned->queue[0].pinned);

                    // 7. Save and Load Roundtrip Persistence
                    Ui->Save();
                    Ui->OpenSubPage(SBotfInterface::EUiView::Load);
                    Ui->RequestAction(SBotfInterface::EPendingAction::LoadCampaign);
                    if (Ui->Shell == SBotfInterface::EUiView::Confirm) Ui->ConfirmAction(false);

                    const auto* LoadedCol = Ui->DraftColony();
                    TestTrue(TEXT("Loaded colony retains Growth workforce priority"),
                        LoadedCol && LoadedCol->workforcePriority == botf::GovernorPriority::Growth);
                    TestTrue(TEXT("Loaded colony retains Manufacturing queue priority"),
                        LoadedCol && LoadedCol->queuePriority == botf::GovernorPriority::Manufacturing);
                    TestTrue(TEXT("Loaded colony retains pinned queue item"),
                        LoadedCol && !LoadedCol->queue.empty() && LoadedCol->queue[0].pinned);

                    // 8. Queue Pin Mutation Protection in UI
                    TestFalse(TEXT("Cannot remove pinned queue item 0"), Ui->CanRemoveQueue(0));
                    TestFalse(TEXT("Cannot cancel first construction when pinned"), Ui->CanCancelFirst());

                    // Enqueue a second project
                    botf::Command Enq2; Enq2.id = Ui->State.lastAcceptedCommand + 1;
                    Enq2.type = botf::CommandType::EnqueueBuilding; Enq2.systemId = Ui->Selected;
                    Enq2.building = botf::Building::Laboratory;
                    Ui->Submit(Enq2);

                    TestFalse(TEXT("Cannot prioritize last item when destination/queue is pinned"), Ui->CanPrioritizeLast());
                    TestFalse(TEXT("Cannot move item past/onto pinned item"), Ui->CanMoveQueue(1, 0));

                    // Attempt removal and cancellation - UI guards must protect pinned item and set Status message
                    const int QueueCountBefore = static_cast<int>(Ui->DraftColony()->queue.size());
                    Ui->EditQueue(0, 0, true);
                    TestEqual(TEXT("Pinned item was not removed by EditQueue"), static_cast<int>(Ui->DraftColony()->queue.size()), QueueCountBefore);
                    TestTrue(TEXT("Status explains item is pinned against removal"), Ui->Status.Contains(TEXT("pinned")));

                    Ui->CancelFirst();
                    TestEqual(TEXT("Pinned item was not removed by CancelFirst"), static_cast<int>(Ui->DraftColony()->queue.size()), QueueCountBefore);
                    TestTrue(TEXT("Status explains item is pinned against cancel"), Ui->Status.Contains(TEXT("pinned")));

                    // Unpin and verify affordances enable
                    Ui->ToggleQueuePin(0);
                    TestTrue(TEXT("CanRemoveQueue enabled after unpinning"), Ui->CanRemoveQueue(0));
                    TestTrue(TEXT("CanCancelFirst enabled after unpinning"), Ui->CanCancelFirst());

                    // 9. Connected Strategic UI Navigation & Real Slate Input Routes
                    Ui->GoPanel(PanelGalaxy);
                    TestEqual(TEXT("On Galaxy hub"), Ui->ActivePanel, (int)PanelGalaxy);

                    // Drive Slate keyboard route: F6 to Empire
                    Ui->OnKeyDown(FGeometry(), FKeyEvent(EKeys::F6, FModifierKeysState(), 0, false, 0, 0));
                    TestEqual(TEXT("F6 Slate key navigates to Empire screen"), Ui->ActivePanel, (int)PanelEmpire);

                    // Drive Slate keyboard route: F2 to System
                    Ui->OnKeyDown(FGeometry(), FKeyEvent(EKeys::F2, FModifierKeysState(), 0, false, 0, 0));
                    TestEqual(TEXT("F2 Slate key navigates to System detail"), Ui->ActivePanel, (int)PanelSystem);
                    TestEqual(TEXT("Previous panel recorded as PanelEmpire"), Ui->PreviousPanel, (int)PanelEmpire);

                    // Drive Slate keyboard route: Backspace to return to Empire
                    Ui->OnKeyDown(FGeometry(), FKeyEvent(EKeys::BackSpace, FModifierKeysState(), 0, false, 0, 0));
                    TestEqual(TEXT("Backspace Slate key returns directly to PanelEmpire"), Ui->ActivePanel, (int)PanelEmpire);

                    // Drive Slate keyboard route: second Backspace to return to Galaxy
                    Ui->OnKeyDown(FGeometry(), FKeyEvent(EKeys::BackSpace, FModifierKeysState(), 0, false, 0, 0));
                    TestEqual(TEXT("Second Backspace returns to PanelGalaxy"), Ui->ActivePanel, (int)PanelGalaxy);

                    // Shortcuts run on the tunnelling pass in the command deck (a focused button cannot swallow them)
                    // and never outside it, where ordinary focus navigation applies.
                    Ui->OnPreviewKeyDown(FGeometry(), FKeyEvent(EKeys::F6, FModifierKeysState(), 0, false, 0, 0));
                    TestEqual(TEXT("F6 on the preview pass opens the Empire screen"), Ui->ActivePanel, (int)PanelEmpire);
                    Ui->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
                    TestEqual(TEXT("Escape returns to the chart"), Ui->ActivePanel, (int)PanelGalaxy);
                    Ui->ShowView(SBotfInterface::EUiView::Menu);
                    TestFalse(TEXT("Menus leave keys to focus navigation on the preview pass"),
                        Ui->OnPreviewKeyDown(FGeometry(), FKeyEvent(EKeys::F2, FModifierKeysState(), 0, false, 0, 0)).IsEventHandled());
                    Ui->ShowView(SBotfInterface::EUiView::Game);

                    // Drive Slate keyboard route: F3 to Research then Escape back to Galaxy
                    Ui->OnKeyDown(FGeometry(), FKeyEvent(EKeys::F3, FModifierKeysState(), 0, false, 0, 0));
                    TestEqual(TEXT("F3 Slate key navigates to Research screen"), Ui->ActivePanel, (int)PanelResearch);
                    Ui->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
                    TestEqual(TEXT("Escape Slate key unwinds to PanelGalaxy"), Ui->ActivePanel, (int)PanelGalaxy);

                    // 10. Research Screen Detail & Simulation Formula Verification
                    Ui->GoPanel(PanelResearch);
                    Ui->SelectedResearchField = 0; // Biotech
                    const FString BioDetail = Ui->ResearchFieldDetail();
                    TestTrue(TEXT("Biotech detail contains BIOTECH header"), BioDetail.Contains(TEXT("BIOTECH")));
                    TestTrue(TEXT("Biotech detail contains food yield formula bonus"), BioDetail.Contains(TEXT("food yield")));

                    Ui->SelectedResearchField = 4; // Propulsion
                    const FString PropDetail = Ui->ResearchFieldDetail();
                    TestTrue(TEXT("Propulsion detail contains PROPULSION header"), PropDetail.Contains(TEXT("PROPULSION")));
                    TestTrue(TEXT("Propulsion detail contains operational range formula (+1 sector per 2 levels)"),
                        PropDetail.Contains(TEXT("operational range")) && PropDetail.Contains(TEXT("sector(s)")));
                    TestFalse(TEXT("Propulsion does NOT show generic +5% bonus"), PropDetail.Contains(TEXT("+5% per level ladder bonus")));

                    Ui->SelectedResearchField = 5; // Weapons
                    const FString WeapDetail = Ui->ResearchFieldDetail();
                    TestTrue(TEXT("Weapons detail contains WEAPONS header"), WeapDetail.Contains(TEXT("WEAPONS")));
                    TestTrue(TEXT("Weapons detail contains combat attack and invasion strength formula"),
                        WeapDetail.Contains(TEXT("combat attack")) && WeapDetail.Contains(TEXT("invasion strength")));
                    TestFalse(TEXT("Weapons does NOT show generic +5% bonus"), WeapDetail.Contains(TEXT("+5% per level ladder bonus")));
                }
            }
        }

        // Schema-13 gameplay layer and the minor-race roster: a new Remastered campaign through the setup screen, a
        // Build Outpost order, a galactic event decision, one committed turn and a save/load round trip.
        {
            Ui->ShowView(SBotfInterface::EUiView::Game);
            Ui->OpenSubPage(SBotfInterface::EUiView::Setup);
            TestTrue(TEXT("New campaigns default to the Remastered ruleset"), Ui->SetupPresetValue() == botf::RulesetPreset::Remastered);
            Ui->SetupRuleset = 1; Ui->SetupPlayer = 1; Ui->SetupEmpires = 2; Ui->SetupSeed = 0; Ui->SetupSize = 0; Ui->SetupEvents = true;
            Ui->SetupEra = static_cast<int>(botf::StartingEra::Expanded);   // Expanded empires start with a troop transport
            Ui->RequestAction(SBotfInterface::EPendingAction::StartCampaign);
            if (Ui->Shell == SBotfInterface::EUiView::Confirm) Ui->ConfirmAction(false);
            const auto& Rules = Ui->State.rules;
            TestTrue(TEXT("A new Remastered campaign has every gameplay layer on and charted anomalies in the galaxy"),
                Ui->Shell == SBotfInterface::EUiView::Game && Rules.preset == botf::RulesetPreset::Remastered && Rules.stations
                && Rules.galaxyFeatures && Rules.curatedEvents && Rules.alternativeVictories && !Ui->State.galaxyFeatures.empty());
            const auto& Minors = Ui->State.minors;
            TestTrue(TEXT("New campaigns place the minor-race roster, scaled to the galaxy"),
                static_cast<int>(Minors.size()) >= botf::minors::rosterTarget(static_cast<int>(Ui->State.systems.size()))
                && std::all_of(Minors.begin(), Minors.end(), [](const botf::Minor& M) { return botf::minors::managed(M); }));
            if (!Minors.empty()) {
                const FString Profile = Str(botf::view::minorProfileText(botf::view::minorProfile(Ui->State, Minors.front())));
                TestTrue(TEXT("A minor race profile shows its temperament, technology, population and join bonus"),
                    Profile.Contains(TEXT("Join bonus")) && Profile.Contains(TEXT("Technology level")) && Profile.Contains(TEXT("population")));
            }
            TestTrue(TEXT("Victory progress lists four conditions, scientific and diplomatic enabled"),
                botf::view::victoryRows(Ui->State, Ui->State.playerEmpire).size() == 4 && Ui->VictorySummaryText().Contains(TEXT("Scientific"))
                && !Ui->OutcomeOverlayVisible());

            // Build Outpost: the transport in the home system (an own colony system is a legal site).
            const auto Transport = std::find_if(Ui->State.fleets.begin(), Ui->State.fleets.end(), [&Ui](const botf::Fleet& F) {
                return F.ownerId == Ui->State.playerEmpire && F.shipClass == botf::Building::Transport; });
            TestTrue(TEXT("The Expanded era campaign has a troop transport"), Transport != Ui->State.fleets.end());
            const int TransportId = Transport != Ui->State.fleets.end() ? Transport->id : 0;
            if (TransportId) {
                Ui->SelectCell(Transport->y * Ui->State.width + Transport->x);
                Ui->SelectedFleet = TransportId; Ui->SelectedTaskForce = 0;
                TestTrue(TEXT("Build Outpost is offered for a transport on a legal site"), Ui->CanBuildOutpost() && Ui->OutpostText().Contains(TEXT("assembles in")));
                Ui->OrderBuildOutpost();
                const auto& Order = Ui->State.pending.back();
                TestTrue(TEXT("Build Outpost submits a real BuildOutpost order"), Order.type == botf::CommandType::BuildOutpost && Order.fleetId == TransportId);
                const auto* Drafted = botf::view::findFleet(Ui->Preview(), TransportId);
                TestTrue(TEXT("The draft shows the transport assembling and the order is not offered twice"),
                    Drafted && Drafted->buildingOutpost && !Ui->CanBuildOutpost() && Ui->OutpostText().Contains(TEXT("Outpost ordered")));
            }

            // A galactic event decision: warned at end of turn, ordered from the event card, replaced by a second choice.
            const int EventId = InjectBotfTestEvent(Ui->State);
            ++Ui->StateVersion; Ui->RefreshAll();
            TestTrue(TEXT("An undecided galactic event raises an end-turn warning and an inbox entry"),
                FString::Join(Ui->EndTurnWarnings(), TEXT(" ")).Contains(TEXT("Scientific breakthrough")) && Ui->InboxCount > 0
                && Ui->EventRows.IsValid() && Ui->EventRows->NumSlots() == 1);
            const auto Choices = [&Ui, EventId] { return std::count_if(Ui->State.pending.begin(), Ui->State.pending.end(), [EventId](const botf::Command& O) {
                return O.type == botf::CommandType::ResolveEventChoice && O.eventId == EventId; }); };
            Ui->ChooseEventOption(EventId, 1);
            TestTrue(TEXT("Choosing an event option submits ResolveEventChoice"),
                Choices() == 1 && Ui->State.pending.back().type == botf::CommandType::ResolveEventChoice && Ui->State.pending.back().eventOption == 1);
            Ui->ChooseEventOption(EventId, 2);
            TestTrue(TEXT("Choosing again replaces the drafted decision"), Choices() == 1 && Ui->State.pending.back().eventOption == 2);
            TestFalse(TEXT("A decided event no longer warns at end of turn"), FString::Join(Ui->EndTurnWarnings(), TEXT(" ")).Contains(TEXT("Scientific breakthrough")));

            const int TurnBefore = Ui->State.turn;
            Ui->EndTurn();
            for (int Guard = 0; Guard < 8 && Ui->Session.pending; ++Guard) { Ui->TacAuto(); Ui->TacContinue(); }
            const auto Cards = botf::view::eventCards(Ui->State);
            TestTrue(TEXT("End Turn applies the decision and the event closes"), !Ui->Session.active && Ui->State.turn == TurnBefore + 1
                && std::none_of(Cards.begin(), Cards.end(), [EventId](const botf::view::EventCard& C) { return C.id == EventId; }));
            if (TransportId) {
                const auto* Assembling = botf::view::findFleet(Ui->State, TransportId);
                TestTrue(TEXT("The committed transport is assembling its outpost"), Assembling && Assembling->buildingOutpost);
            }
            TestTrue(TEXT("The turn briefing comes from the chronicle, with the event outcome under Events"),
                std::any_of(Ui->LastBriefing.begin(), Ui->LastBriefing.end(), [](const botf::view::BriefingEntry& E) {
                    return E.category == botf::ChronicleCategory::Events && E.text.find("breakthrough") != std::string::npos; }));

            const auto RemasteredFingerprint = botf::fingerprint(Ui->State);
            Ui->Save();
            Ui->OpenSubPage(SBotfInterface::EUiView::Load);
            Ui->RequestAction(SBotfInterface::EPendingAction::LoadCampaign);
            if (Ui->Shell == SBotfInterface::EUiView::Confirm) Ui->ConfirmAction(false);
            TestTrue(TEXT("A Remastered campaign with its minors and outpost work survives save and load"),
                botf::fingerprint(Ui->State) == RemasteredFingerprint && Ui->State.rules.preset == botf::RulesetPreset::Remastered
                && Ui->SetupPresetValue() == botf::RulesetPreset::Remastered && Ui->LastBriefing.empty());

            // Classic stays available from setup and keeps outposts, without the Remastered-only layers.
            Ui->OpenSubPage(SBotfInterface::EUiView::Setup);
            Ui->SetupPreset = static_cast<int>(botf::RulesetPreset::Classic);
            Ui->RequestAction(SBotfInterface::EPendingAction::StartCampaign);
            if (Ui->Shell == SBotfInterface::EUiView::Confirm) Ui->ConfirmAction(false);
            TestTrue(TEXT("A Classic campaign keeps stations and the roster but no anomalies or curated events"),
                Ui->State.rules.preset == botf::RulesetPreset::Classic && Ui->State.rules.stations && Ui->State.galaxyFeatures.empty()
                && !Ui->State.rules.curatedEvents && !Ui->State.minors.empty());
        }
    } catch (const std::exception& Error) {
        AddError(FString(TEXT("Shell test threw: ")) + Str(Error.what()));
    }
    // Snapshots written by this run live only in its own GUID folder; remove it (and the parent once empty).
    IFileManager::Get().DeleteDirectory(*TestFolder, false, true);
    IFileManager::Get().DeleteDirectory(*(FPaths::ProjectDir() / TEXT("Build/UiAutomationSnapshots")), false, false);
    return !HasAnyErrors();
}
#endif
}

ABotfGameMode::ABotfGameMode() { DefaultPawnClass = nullptr; }
void ABotfGameMode::BeginPlay() {
    Super::BeginPlay();
    if (GEngine && GEngine->GameViewport) {
        CombatScene = GetWorld()->SpawnActor<ABotfCombatScene>();
        if (CombatScene) CombatScene->SetPresentationActive(false);
        Interface = SNew(SBotfInterface, CombatScene.Get());
        GEngine->GameViewport->AddViewportWidgetContent(Interface.ToSharedRef(), 10);
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) {
            PC->bShowMouseCursor = true;
            FInputModeUIOnly Input;
            Input.SetWidgetToFocus(Interface); Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(Input);
        }
    }
}
void ABotfGameMode::EndPlay(const EEndPlayReason::Type Reason) {
    if (GEngine && GEngine->GameViewport && Interface.IsValid())
        GEngine->GameViewport->RemoveViewportWidgetContent(Interface.ToSharedRef());
    Interface.Reset();
    if (CombatScene) { CombatScene->Destroy(); CombatScene = nullptr; }
    Super::EndPlay(Reason);
}


