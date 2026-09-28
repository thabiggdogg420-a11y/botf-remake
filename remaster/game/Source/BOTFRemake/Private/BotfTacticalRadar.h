#pragma once
// UI-only: tactical radar for a live encounter. Draws every ship at its real tactical position
// (the 3D scene may cap displayed ships; this view never does), last-round movement and ordered targets.

#include "BotfUiTheme.h"
#include "TacticalCombat.h"
#include "InputCoreTypes.h"
#include <functional>
#include <optional>

namespace BotfUi {

class STacticalRadar final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(STacticalRadar) {} SLATE_END_ARGS()
    void Construct(const FArguments&, const std::optional<botf::tactical::Encounter>* InEncounter, const int* InPlayer,
                   const int* InSelected, const int* InTarget, std::function<void(int)> InPick) {
        Encounter = InEncounter; Player = InPlayer; SelectedShip = InSelected; TargetShip = InTarget; Pick = MoveTemp(InPick);
    }
    static constexpr double M = 18;
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(480, 360); }
    FVector2D CellSize(const botf::tactical::Encounter& E, FVector2D Size) const {
        return FVector2D((Size.X - 2 * M) / E.arenaWidth, (Size.Y - 2 * M) / E.arenaHeight);
    }
    FVector2D Centre(const botf::tactical::Encounter& E, botf::tactical::TacticalCoord P, FVector2D Size) const {
        const FVector2D C = CellSize(E, Size);
        return FVector2D(M + (P.x + 0.5) * C.X, M + (P.y + 0.5) * C.Y);
    }
    // Ships sharing a cell are fanned out so each one stays clickable.
    FVector2D ShipPoint(const botf::tactical::Encounter& E, const botf::tactical::Ship& S, FVector2D Size) const {
        int Index = 0, Count = 0;
        for (const auto& O : E.ships) if (O.position == S.position) { if (O.fleetId == S.fleetId) Index = Count; ++Count; }
        const FVector2D C = CellSize(E, Size);
        const double Spread = Count > 1 ? FMath::Min(C.X, C.Y) * 0.32 : 0;
        const double Angle = Count > 1 ? 2 * UE_DOUBLE_PI * Index / Count : 0;
        return Centre(E, S.position, Size) + FVector2D(std::cos(Angle), std::sin(Angle)) * Spread;
    }
    // Paint must never let an exception unwind through Slate.
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                         FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override {
        try { return PaintRadar(Geometry, Elements, Layer); }
        catch (...) { return Layer; }
    }
    int32 PaintRadar(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer) const {
        if (!Encounter || !Encounter->has_value()) return Layer;
        const auto& E = **Encounter;
        const FPalette& Pal = Palette(*Player);
        const FVector2D Size = Geometry.GetLocalSize();
        const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
        const FVector2D CS = CellSize(E, Size);
        auto Box = [&](int L, FVector2D At, FVector2D Extent, const FLinearColor& Color) {
            FSlateDrawElement::MakeBox(Elements, Layer + L, Geometry.ToPaintGeometry(Extent, FSlateLayoutTransform(At)), White, ESlateDrawEffect::None, Color);
        };
        auto Line = [&](int L, FVector2D A, FVector2D B, const FLinearColor& Color, float Thick) {
            TArray<FVector2D> Pts; Pts.Add(A); Pts.Add(B);
            FSlateDrawElement::MakeLines(Elements, Layer + L, Geometry.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Color, true, Thick);
        };
        Box(0, FVector2D(0, 0), Size, Shade(Pal.Void, 1.f));
        const FLinearColor GridColor = Shade(Pal.Secondary, 0.22f, 0.6f);
        for (int X = 0; X <= E.arenaWidth; ++X) Line(1, FVector2D(M + X * CS.X, M), FVector2D(M + X * CS.X, Size.Y - M), GridColor, 1);
        for (int Y = 0; Y <= E.arenaHeight; ++Y) Line(1, FVector2D(M, M + Y * CS.Y), FVector2D(Size.X - M, M + Y * CS.Y), GridColor, 1);
        // Movement of the last resolved round.
        if (!E.rounds.empty())
            for (const auto& St : E.rounds.back().shipStatuses)
                if (!(St.positionBefore == St.positionAfter))
                    Line(2, Centre(E, St.positionBefore, Size), Centre(E, St.positionAfter, Size), Shade(FactionColor(St.ownerId), 0.6f, 0.7f), 1.5f);
        // Ordered targets for the player's ships.
        for (const auto& O : E.pendingOrders) {
            const auto* From = E.getShip(O.fleetId); const auto* To = O.targetFleetId ? E.getShip(O.targetFleetId) : nullptr;
            if (From && To) Line(3, ShipPoint(E, *From, Size), ShipPoint(E, *To, Size), O.fleetId == *SelectedShip ? Pal.Accent : Shade(Pal.Accent, 0.6f), O.fleetId == *SelectedShip ? 2.f : 1.f);
        }
        const double R = FMath::Clamp(FMath::Min(CS.X, CS.Y) * 0.22, 4.0, 11.0);
        for (const auto& S : E.ships) {
            const FVector2D P = ShipPoint(E, S, Size);
            const FLinearColor Base = FactionColor(S.ownerId);
            if (!S.isAlive) {
                Line(4, P - FVector2D(R, R), P + FVector2D(R, R), Shade(Base, 0.5f), 2);
                Line(4, P + FVector2D(-R, R), P + FVector2D(R, -R), Shade(Base, 0.5f), 2);
                continue;
            }
            if (S.fleetId == *SelectedShip) Box(4, P - FVector2D(R + 4, R + 4), FVector2D(2 * R + 8, 2 * R + 8), Pal.Accent);
            else if (S.fleetId == *TargetShip) Box(4, P - FVector2D(R + 4, R + 4), FVector2D(2 * R + 8, 2 * R + 8), Pal.Alert);
            // Cloaked ships are drawn translucent; a disengaging ship (rules 2) carries an outline in its own colour.
            const FLinearColor Fill = S.retreated ? Shade(Base, 0.35f, 0.7f) : S.isCloaked ? Shade(Base, 1.f, 0.4f) : Base;
            if (!S.retreated && E.remasterRules() && S.disengaging()) {
                const double O = R + 2;
                TArray<FVector2D> Ring = { P + FVector2D(-O, -O), P + FVector2D(O, -O), P + FVector2D(O, O), P + FVector2D(-O, O), P + FVector2D(-O, -O) };
                FSlateDrawElement::MakeLines(Elements, Layer + 5, Geometry.ToPaintGeometry(), Ring, ESlateDrawEffect::None, Pal.Tertiary, true, 1.5f);
            }
            Box(5, P - FVector2D(R, R), FVector2D(2 * R, 2 * R), Fill);
            // Hull bar under the marker.
            const double Frac = S.maxHull > 0 ? static_cast<double>(S.hull) / S.maxHull : 0;
            Box(5, P + FVector2D(-R, R + 2), FVector2D(2 * R, 3), Shade(Pal.Alert, 0.4f));
            Box(6, P + FVector2D(-R, R + 2), FVector2D(2 * R * Frac, 3), Pal.Good);
        }
        return Layer + 7;
    }
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override {
        if (!Encounter || !Encounter->has_value() || Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
        const auto& E = **Encounter;
        const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
        const FVector2D Size = Geometry.GetLocalSize();
        int Best = 0; double BestD = 1e9;
        for (const auto& S : E.ships) {
            if (!S.isAlive) continue;
            const double D = FVector2D::Distance(ShipPoint(E, S, Size), Local);
            if (D < BestD) { BestD = D; Best = S.fleetId; }
        }
        if (Best && BestD < FMath::Max(16.0, FMath::Min(CellSize(E, Size).X, CellSize(E, Size).Y))) { Pick(Best); Invalidate(EInvalidateWidgetReason::Paint); }
        return FReply::Handled();
    }
private:
    const std::optional<botf::tactical::Encounter>* Encounter = nullptr;
    const int* Player = nullptr;
    const int* SelectedShip = nullptr;
    const int* TargetShip = nullptr;
    std::function<void(int)> Pick;
};

} // namespace BotfUi
