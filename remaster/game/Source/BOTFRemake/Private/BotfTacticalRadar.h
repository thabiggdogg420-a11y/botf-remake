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
        const FVector2D CS = CellSize(E, Size);
        auto Line = [&](int L, FVector2D A, FVector2D B, const FLinearColor& Color, float Thick) {
            TArray<FVector2D> Pts; Pts.Add(A); Pts.Add(B);
            BotfUi::DrawLines(Elements, Layer + L, Geometry, Pts, Color, Thick);
        };
        const auto F2 = [](FVector2D V) { return FVector2f(static_cast<float>(V.X), static_cast<float>(V.Y)); };
        PaintGradient(Elements, Layer, Geometry, FVector2f::ZeroVector, F2(Size), FVector4(10, 10, 10, 10), Mix(Pal.Void, Pal.FieldHi, 0.35f), Shade(Pal.Void, 0.85f));
        // Arena: a dim plate with a centre line, so the two fleets read as two sides.
        PaintRounded(Elements, Layer + 1, Geometry, F2(FVector2D(M, M)), F2(Size - FVector2D(2 * M, 2 * M)), FVector4(6, 6, 6, 6),
            WithAlpha(FLinearColor::Black, 0.25f), WithAlpha(Pal.LineHi, 0.7f), 1.f);
        const FLinearColor GridColor = Shade(Pal.Secondary, 0.3f, 0.16f);
        for (int X = 1; X < E.arenaWidth; ++X) Line(1, FVector2D(M + X * CS.X, M), FVector2D(M + X * CS.X, Size.Y - M), GridColor, 1);
        for (int Y = 1; Y < E.arenaHeight; ++Y) Line(1, FVector2D(M, M + Y * CS.Y), FVector2D(Size.X - M, M + Y * CS.Y), GridColor, 1);
        Line(1, FVector2D(Size.X * 0.5, M), FVector2D(Size.X * 0.5, Size.Y - M), WithAlpha(Pal.Accent, 0.25f), 1.f);
        // Movement of the last resolved round.
        if (!E.rounds.empty())
            for (const auto& St : E.rounds.back().shipStatuses)
                if (!(St.positionBefore == St.positionAfter))
                    Line(2, Centre(E, St.positionBefore, Size), Centre(E, St.positionAfter, Size), Shade(FactionColor(St.ownerId), 0.7f, 0.55f), 1.5f);
        // Ordered targets for the player's ships.
        for (const auto& O : E.pendingOrders) {
            const auto* From = E.getShip(O.fleetId); const auto* To = O.targetFleetId ? E.getShip(O.targetFleetId) : nullptr;
            if (From && To) Line(3, ShipPoint(E, *From, Size), ShipPoint(E, *To, Size), O.fleetId == *SelectedShip ? Pal.Accent : Shade(Pal.Accent, 0.6f, 0.6f), O.fleetId == *SelectedShip ? 2.f : 1.f);
        }
        const double R = FMath::Clamp(FMath::Min(CS.X, CS.Y) * 0.24, 4.0, 11.0);
        for (const auto& S : E.ships) {
            const FVector2D P = ShipPoint(E, S, Size);
            const FLinearColor Base = FactionColor(S.ownerId);
            if (!S.isAlive) {
                Line(4, P - FVector2D(R * 0.6, R * 0.6), P + FVector2D(R * 0.6, R * 0.6), Shade(Base, 0.5f, 0.7f), 1.5f);
                Line(4, P + FVector2D(-R * 0.6, R * 0.6), P + FVector2D(R * 0.6, -R * 0.6), Shade(Base, 0.5f, 0.7f), 1.5f);
                continue;
            }
            if (S.fleetId == *SelectedShip) PaintGlow(Elements, Layer + 4, Geometry, F2(P), static_cast<float>(R * 3.2), WithAlpha(Pal.Accent, 0.65f));
            else if (S.fleetId == *TargetShip) PaintGlow(Elements, Layer + 4, Geometry, F2(P), static_cast<float>(R * 3.2), WithAlpha(Pal.Alert, 0.65f));
            // Cloaked ships are drawn translucent; a disengaging ship (rules 2) carries an outline in its own colour.
            const FLinearColor Fill = S.retreated ? Shade(Base, 0.35f, 0.7f) : S.isCloaked ? Shade(Base, 1.f, 0.4f) : Lift(Base, 0.08f);
            if (!S.retreated && E.remasterRules() && S.disengaging()) {
                const double O = R + 3;
                TArray<FVector2D> Ring;
                for (int I = 0; I <= 20; ++I) { const double A = I * 6.2831853 / 20; Ring.Add(P + FVector2D(std::cos(A), std::sin(A)) * O); }
                BotfUi::DrawLines(Elements, Layer + 5, Geometry, Ring, Pal.Tertiary, 1.5f);
            }
            // A ship is an arrowhead pointing at the enemy: own fleet to the right, the foe to the left.
            const double D = S.ownerId == *Player ? 1.0 : -1.0;
            TArray<FVector2f> Head = { F2(P + FVector2D(R * D * 1.1, 0)), F2(P + FVector2D(-R * D * 0.8, -R * 0.85)), F2(P + FVector2D(-R * D * 0.35, 0)), F2(P + FVector2D(-R * D * 0.8, R * 0.85)) };
            const FLinearColor Rim = Shade(Base, 0.7f, Fill.A);
            PaintFan(Elements, Layer + 5, Geometry, F2(P), Head, Fill, &Rim);
            // Schema-13 stations are immobile units: a diamond frame in their owner's colour marks them apart from ships.
            if (S.immobile) {
                const double O = R + 5;
                TArray<FVector2D> Frame = { P + FVector2D(0, -O), P + FVector2D(O, 0), P + FVector2D(0, O), P + FVector2D(-O, 0), P + FVector2D(0, -O) };
                BotfUi::DrawLines(Elements, Layer + 5, Geometry, Frame, Base, 2.f);
            }
            // Hull bar under the marker.
            const double Frac = S.maxHull > 0 ? static_cast<double>(S.hull) / S.maxHull : 0;
            PaintRounded(Elements, Layer + 5, Geometry, F2(P + FVector2D(-R, R + 3)), FVector2f(static_cast<float>(2 * R), 3.f), FVector4(1.5f, 1.5f, 1.5f, 1.5f), WithAlpha(FLinearColor::Black, 0.55f));
            PaintRounded(Elements, Layer + 6, Geometry, F2(P + FVector2D(-R, R + 3)), FVector2f(static_cast<float>(FMath::Max(2.0, 2 * R * Frac)), 3.f), FVector4(1.5f, 1.5f, 1.5f, 1.5f),
                Frac > 0.5 ? Pal.Good : (Frac > 0.25 ? Pal.Warn : Pal.Alert));
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
