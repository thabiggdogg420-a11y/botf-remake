#pragma once
// UI-only battle presentation widgets.
//   SBattleImage: the 3D combat capture, fitted to its box. Drawn without texture alpha: a scene capture's alpha
//                 channel is not coverage, and honouring it can leave the panel showing only the background.
//   SBattlePlot:  a 2D replay of a recorded battle from the combat report alone (both sides, hull and shields at the
//                 chosen round, hits, losses and withdrawals). It never depends on the 3D scene, so a replay is
//                 always readable even when the capture is unavailable.

#include "BotfUiTheme.h"
#include "BotfGameplayViewModel.h"
#include "Simulation.h"
#include <functional>
#include <vector>

namespace BotfUi {

class SBattleImage final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(SBattleImage) {} SLATE_END_ARGS()
    void Construct(const FArguments&, const FSlateBrush* InBrush, std::function<bool()> InShow) { Brush = InBrush; Show = MoveTemp(InShow); }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(320, 180); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                         FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override {
        if (!Brush || !Brush->GetResourceObject() || (Show && !Show())) return Layer;
        const FVector2D Size = Geometry.GetLocalSize();
        const FVector2D Image = Brush->ImageSize.X > 0 && Brush->ImageSize.Y > 0 ? FVector2D(Brush->ImageSize) : FVector2D(16, 9);
        const double Scale = FMath::Min(Size.X / Image.X, Size.Y / Image.Y);
        const FVector2D Fit = Image * Scale;
        FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(Fit, FSlateLayoutTransform((Size - Fit) * 0.5)),
            Brush, ESlateDrawEffect::IgnoreTextureAlpha, FLinearColor::White);
        return Layer;
    }
private:
    const FSlateBrush* Brush = nullptr;
    std::function<bool()> Show;
};

// State of one recorded ship at a replay round.
struct FBattleShipState {
    int FleetId = 0, OwnerId = 0, Side = 0;
    FString Name;
    int Hull = 0, MaxHull = 1, Shields = 0, MaxShields = 0;
    bool bLost = false, bRetreated = false, bHit = false;
};

// Replays the report up to Round (clamped; -1 = the end). Reports saved before per-ship records rebuild the fleet
// lists from the side ids and the recorded damage, as the 3D scene does.
inline std::vector<FBattleShipState> BattleShipStates(const botf::CombatReport& Report, int Round) {
    std::vector<FBattleShipState> Out;
    auto Add = [&](int FleetId, int OwnerId, botf::Building Class, int ReferenceId, int Hull, int Shields) {
        FBattleShipState S;
        S.FleetId = FleetId; S.OwnerId = OwnerId; S.Side = OwnerId == Report.firstOwner ? 0 : 1;
        // A schema-13 station is recorded as a Destroyer-class participant: name it by its combatant id instead.
        const std::string Label = botf::view::participantLabel(nullptr, FleetId, OwnerId, Class, ReferenceId);
        S.Name = botf::isStationCombatant(FleetId) ? FString(UTF8_TO_TCHAR(Label.c_str()))
            : FString::Printf(TEXT("#%d %s"), FleetId, UTF8_TO_TCHAR(Label.c_str()));
        S.Hull = S.MaxHull = FMath::Max(1, Hull); S.Shields = S.MaxShields = FMath::Max(0, Shields);
        Out.push_back(S);
    };
    if (!Report.participants.empty()) {
        for (const auto& P : Report.participants) Add(P.fleetId, P.ownerId, P.shipClass, P.referenceShipId, P.initialHull, P.initialShields);
    } else {
        auto Legacy = [&](int FleetId, int OwnerId) {
            int Hull = 12;
            for (const auto& D : Report.damage) if (D.fleetId == FleetId) { Hull = D.hullBefore; break; }
            Add(FleetId, OwnerId, botf::Building::Scout, -1, Hull, 0);
        };
        for (const int Id : Report.firstFleetIds) Legacy(Id, Report.firstOwner);
        for (const int Id : Report.secondFleetIds) Legacy(Id, Report.secondOwner);
    }
    auto Find = [&Out](int Id) -> FBattleShipState* {
        for (auto& S : Out) if (S.FleetId == Id) return &S;
        return nullptr;
    };
    if (Report.rounds.empty()) {   // only the final result was recorded
        for (const auto& D : Report.damage) if (auto* S = Find(D.fleetId)) { S->Hull = D.hullAfter; S->Shields = D.shieldAfter; S->bHit = true; }
        for (const int Id : Report.lostFleetIds) if (auto* S = Find(Id)) { S->bLost = true; S->Hull = 0; }
        for (const int Id : Report.retreatedFleetIds) if (auto* S = Find(Id)) S->bRetreated = true;
        return Out;
    }
    const int Last = Round < 0 ? static_cast<int>(Report.rounds.size()) - 1 : FMath::Min(Round, static_cast<int>(Report.rounds.size()) - 1);
    for (int R = 0; R <= Last; ++R) {
        for (auto& S : Out) S.bHit = false;
        for (const auto& D : Report.rounds[R].damage)
            if (auto* S = Find(D.fleetId)) { S->Hull = D.hullAfter; S->Shields = D.shieldAfter; S->bHit = D.hullAfter < D.hullBefore || D.shieldAfter < D.shieldBefore; }
        for (const int Id : Report.rounds[R].lostFleetIds) if (auto* S = Find(Id)) { S->bLost = true; S->Hull = 0; }
        for (const int Id : Report.rounds[R].retreatedFleetIds) if (auto* S = Find(Id)) S->bRetreated = true;
    }
    return Out;
}

class SBattlePlot final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(SBattlePlot) {} SLATE_END_ARGS()
    void Construct(const FArguments&, std::function<const botf::CombatReport*()> InReport, std::function<int()> InRound, const int* InPlayer) {
        Report = MoveTemp(InReport); Round = MoveTemp(InRound); Player = InPlayer;
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(320, 160); }
    // Paint must never let an exception unwind through Slate (ship names come from the catalogue, which can throw).
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                         FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override {
        try { return PaintPlot(Geometry, Elements, Layer); }
        catch (...) { return Layer; }
    }
    int32 PaintPlot(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer) const {
        const botf::CombatReport* B = Report ? Report() : nullptr;
        if (!B) return Layer;
        const FPalette& Pal = Palette(Player ? *Player : 1);
        const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
        const FVector2D Size = Geometry.GetLocalSize();
        auto Box = [&](int L, FVector2D At, FVector2D Extent, const FLinearColor& Color) {
            FSlateDrawElement::MakeBox(Elements, Layer + L, Geometry.ToPaintGeometry(Extent, FSlateLayoutTransform(At)), White, ESlateDrawEffect::None, Color);
        };
        auto Text = [&](int L, FVector2D At, double Width, const FString& Value, int Pt, bool Bold, const FLinearColor& Color) {
            FSlateDrawElement::MakeText(Elements, Layer + L, Geometry.ToPaintGeometry(FVector2D(Width, Pt + 8), FSlateLayoutTransform(At)),
                Value, Font(Pt, Bold), ESlateDrawEffect::None, Color);
        };
        const auto Ships = BattleShipStates(*B, Round ? Round() : -1);
        const double Column = (Size.X - 36) * 0.5, Top = 24;
        int Counts[2] = {0, 0};
        for (const auto& S : Ships) ++Counts[S.Side];
        const double RowH = FMath::Clamp((Size.Y - Top - 4) / FMath::Max(1, FMath::Max(Counts[0], Counts[1])), 16.0, 34.0);
        for (int Side = 0; Side < 2; ++Side) {
            const int Owner = Side == 0 ? B->firstOwner : B->secondOwner;
            const double X = Side == 0 ? 8 : 28 + Column;
            Text(1, FVector2D(X, 2), Column, FString(UTF8_TO_TCHAR(botf::empireName(Owner))).ToUpper()
                + (B->winnerOwner == Owner ? TEXT("  /  HOLDS THE FIELD") : TEXT("")), 10, true, FactionColor(Owner));
        }
        Text(1, FVector2D(Size.X * 0.5 - 8, 2), 24, TEXT("VS"), 9, true, Pal.Muted);
        int Row[2] = {0, 0};
        for (const auto& S : Ships) {
            const double X = S.Side == 0 ? 8 : 28 + Column;
            const double Y = Top + Row[S.Side]++ * RowH;
            if (Y + RowH > Size.Y + 1) continue;
            const bool Out = S.bLost || S.bRetreated;
            const FLinearColor Faction = FactionColor(S.OwnerId);
            Box(1, FVector2D(X, Y + 2), FVector2D(5, RowH - 4), Out ? Shade(Faction, 0.35f) : Faction);
            if (S.bHit && !Out) Box(1, FVector2D(X + 7, Y + 2), FVector2D(3, RowH - 4), Pal.Alert);
            const double TextW = Column * 0.5 - 14;
            Text(2, FVector2D(X + 14, Y + (RowH - 16) * 0.5), TextW, S.Name, RowH >= 24 ? 9 : 8, false, Out ? Pal.Muted : Pal.Text);
            const double BarX = X + 14 + TextW + 4, BarW = FMath::Max(10.0, Column - (BarX - X) - 4);
            if (S.bLost || S.bRetreated) {
                Text(2, FVector2D(BarX, Y + (RowH - 16) * 0.5), BarW, S.bLost ? TEXT("DESTROYED") : TEXT("WITHDRAWN"), 9, true, S.bLost ? Pal.Alert : Pal.Muted);
                if (S.bLost) Box(3, FVector2D(X + 14, Y + RowH * 0.5), FVector2D(TextW, 1.5), Shade(Pal.Alert, 1.f, 0.8f));
                continue;
            }
            const double HullH = FMath::Max(4.0, RowH * 0.32), ShieldH = FMath::Max(2.0, RowH * 0.16);
            const double HullY = Y + RowH * 0.5 - HullH, ShieldY = Y + RowH * 0.5 + 2;
            const float HullF = FMath::Clamp(static_cast<float>(S.Hull) / S.MaxHull, 0.f, 1.f);
            Box(1, FVector2D(BarX, HullY), FVector2D(BarW, HullH), Shade(Pal.Secondary, 0.22f));
            Box(2, FVector2D(BarX, HullY), FVector2D(BarW * HullF, HullH), HullF > 0.5f ? Pal.Good : HullF > 0.25f ? Pal.Tertiary : Pal.Alert);
            if (S.MaxShields > 0) {
                const float ShieldF = FMath::Clamp(static_cast<float>(S.Shields) / S.MaxShields, 0.f, 1.f);
                Box(1, FVector2D(BarX, ShieldY), FVector2D(BarW, ShieldH), Shade(Pal.Secondary, 0.22f));
                Box(2, FVector2D(BarX, ShieldY), FVector2D(BarW * ShieldF, ShieldH), Pal.Secondary);
            }
        }
        return Layer + 3;
    }
private:
    std::function<const botf::CombatReport*()> Report;
    std::function<int()> Round;
    const int* Player = nullptr;
};

} // namespace BotfUi
