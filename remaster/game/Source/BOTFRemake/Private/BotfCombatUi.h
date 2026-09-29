#pragma once
// Combat-only UI helpers for the BOTF Remake Slate interface:
// - STacticalViewport: Full-screen interactive 3D combat viewport: orbit, pan, cursor zoom, click to pick, tactical markers.
// - SShipStatusBar: Compact high-contrast Hull and Shields telemetry status bars.

#include "CoreMinimal.h"
#include "BotfUiTheme.h"
#include "BotfCombatScene.h"
#include "InputCoreTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include <functional>

namespace BotfUi {

class STacticalViewport final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(STacticalViewport) {}
    SLATE_END_ARGS()

    // Controls: left-drag orbits, right-drag pans (and frees the camera), the wheel zooms toward the cursor, a click
    // selects the ship under it (OnPick), a double-click flies to and follows that ship or, on empty space, flies the
    // camera there; middle-click resets. Markers bracket every ship so a speck-sized escort stays findable at any zoom.
    void Construct(const FArguments&, TWeakObjectPtr<ABotfCombatScene> InScene, const FSlateBrush* InBrush, std::function<bool()> InShow,
                   std::function<void(int32)> InOnPick = nullptr, std::function<FString(int32)> InNameOf = nullptr) {
        Scene = InScene;
        Brush = InBrush;
        Show = MoveTemp(InShow);
        OnPick = MoveTemp(InOnPick);
        NameOf = MoveTemp(InNameOf);
        // Ships move every frame: repaint the markers with them.
        RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &STacticalViewport::Repaint));
    }

    virtual FVector2D ComputeDesiredSize(float) const override {
        return FVector2D(1280, 800);
    }

    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                         FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override {
        // The brush's texture belongs to the scene actor: draw only while the scene is alive.
        if (!Scene.IsValid() || !Brush || !Brush->GetResourceObject() || (Show && !Show())) return Layer;
        const FVector2D Size = Geometry.GetLocalSize();
        FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform()),
            Brush, ESlateDrawEffect::IgnoreTextureAlpha, FLinearColor::White);
        TArray<FBotfShipMarker> Markers;
        Scene->GetShipMarkers(Markers);
        for (const FBotfShipMarker& M : Markers) PaintMarker(Elements, Layer + 1, Geometry, M);
        return Layer + 4;
    }

    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override {
        const FKey Button = MouseEvent.GetEffectingButton();
        if (Button == EKeys::LeftMouseButton || Button == EKeys::RightMouseButton) {
            bIsDragging = true;
            bMoved = false;
            DragButton = Button;
            LastMousePos = PressPos = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
            return FReply::Handled().CaptureMouse(AsShared());
        }
        if (Button == EKeys::MiddleMouseButton) {
            if (Scene.IsValid()) Scene->ResetCameraOrbit();
            return FReply::Handled();
        }
        return FReply::Unhandled();
    }

    virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override {
        if (bIsDragging && MouseEvent.GetEffectingButton() == DragButton) {
            // A left press that never became a drag is a click: select the ship under the cursor.
            if (DragButton == EKeys::LeftMouseButton && !bMoved && Scene.IsValid()) {
                const int32 Fleet = Scene->PickShipAt(ToScreen01(Geometry, MouseEvent));
                if (Fleet && OnPick) OnPick(Fleet);
            }
            EndDrag();
            return FReply::Handled().ReleaseMouseCapture();
        }
        return FReply::Unhandled();
    }

    virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override {
        const FVector2D Local = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
        if (!bIsDragging) {
            HoverFleet = Scene.IsValid() ? Scene->PickShipAt(ToScreen01(Geometry, MouseEvent)) : 0;
            return FReply::Unhandled();
        }
        // The button was released where we did not hear it (another window, alt-tab): stop at once.
        if (!MouseEvent.IsMouseButtonDown(DragButton) || !HasMouseCapture()) {
            EndDrag();
            return FReply::Handled().ReleaseMouseCapture();
        }
        if (!bMoved && FVector2D::Distance(Local, PressPos) < 4.0) return FReply::Handled(); // still a click
        bMoved = true;
        const FVector2D Delta = Local - LastMousePos;
        LastMousePos = Local;
        if (Scene.IsValid()) {
            if (DragButton == EKeys::RightMouseButton) {
                const FVector2D Size = Geometry.GetLocalSize();
                Scene->AddCameraPan(FVector2D(Delta.X / FMath::Max(1.0, Size.X), Delta.Y / FMath::Max(1.0, Size.Y)));
            } else {
                Scene->AddCameraOrbit(Delta.X * 0.35f, -Delta.Y * 0.35f);
            }
        }
        return FReply::Handled();
    }

    virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override {
        SLeafWidget::OnMouseLeave(MouseEvent);
        HoverFleet = 0;
    }

    virtual FCursorReply OnCursorQuery(const FGeometry&, const FPointerEvent&) const override {
        if (bIsDragging && bMoved) return FCursorReply::Cursor(DragButton == EKeys::RightMouseButton ? EMouseCursor::GrabHandClosed : EMouseCursor::CardinalCross);
        return FCursorReply::Cursor(HoverFleet ? EMouseCursor::Hand : EMouseCursor::Default);
    }

    // Capture taken away (focus change, alt-tab, a popup): a later move must never resume a stale drag.
    virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override {
        SLeafWidget::OnMouseCaptureLost(CaptureLostEvent);
        EndDrag();
    }

    virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override {
        if (Scene.IsValid()) {
            Scene->AddCameraZoomAt(MouseEvent.GetWheelDelta(), ToScreen01(Geometry, MouseEvent));
            return FReply::Handled();
        }
        return FReply::Unhandled();
    }

    virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override {
        if (!Scene.IsValid() || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
        const FVector2D At = ToScreen01(Geometry, MouseEvent);
        if (const int32 Fleet = Scene->PickShipAt(At)) {
            if (OnPick) OnPick(Fleet);
            Scene->FollowFleet(Fleet);
        } else {
            Scene->FlyToScreenPoint(At);
        }
        return FReply::Handled();
    }

private:
    EActiveTimerReturnType Repaint(double, float) {
        Invalidate(EInvalidateWidgetReason::Paint);
        return EActiveTimerReturnType::Continue;
    }
    void EndDrag() { bIsDragging = false; bMoved = false; DragButton = EKeys::Invalid; }
    static FVector2D ToScreen01(const FGeometry& Geometry, const FPointerEvent& MouseEvent) {
        const FVector2D Local = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
        const FVector2D Size = Geometry.GetLocalSize();
        return FVector2D(Local.X / FMath::Max(1.0, Size.X), Local.Y / FMath::Max(1.0, Size.Y));
    }

    // Corner brackets around the hull's projected outline, in the owner's colour. They fade as the ship grows on screen
    // (it speaks for itself) and a ship smaller than a few pixels also gets a diamond. The active ship, the target and
    // the ship under the cursor get their name and hull and shield bars.
    void PaintMarker(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, const FBotfShipMarker& M) const {
        const FVector2D Size = Geometry.GetLocalSize();
        const FVector2D C(M.Screen.X * Size.X, M.Screen.Y * Size.Y);
        const double R = M.Radius * Size.Y;
        const bool bHover = M.FleetId == HoverFleet;
        const bool bKey = M.bSelected || M.bTarget || bHover;
        const FPalette& Owner = Palette(M.OwnerId);
        const FPalette& Ui = Theme();
        const FLinearColor Base = M.bTarget ? Ui.Alert : M.bSelected ? Ui.Accent : Owner.Primary;
        // A hull filling much of the view (a follow shot) needs no frame: only its name and bars, kept compact.
        const bool bLarge = R > Size.Y * 0.16;
        if (bLarge && !bKey) return;
        const float Alpha = bKey ? 0.95f : FMath::GetMappedRangeValueClamped(FVector2f(14.f, 110.f), FVector2f(0.75f, 0.16f), static_cast<float>(R));
        const FLinearColor Col = WithAlpha(Lift(Base, bKey ? 0.15f : 0.f), Alpha);
        const double H = bLarge ? FMath::Min(R * 0.55, Size.Y * 0.2) : FMath::Max(R * 1.08, 9.0);
        const double L = FMath::Clamp(H * 0.32, 4.0, 22.0);
        const float Thick = bKey ? 2.f : 1.4f;
        for (int Corner = 0; Corner < 4 && !bLarge; ++Corner) {
            const double Sx = (Corner & 1) ? 1.0 : -1.0, Sy = (Corner & 2) ? 1.0 : -1.0;
            const FVector2D P(C.X + Sx * H, C.Y + Sy * H);
            DrawLines(Elements, Layer, Geometry, {FVector2D(P.X - Sx * L, P.Y), P, FVector2D(P.X, P.Y - Sy * L)}, Col, Thick);
        }
        if (R < 5.0) {
            const double D = 3.5;
            DrawLines(Elements, Layer, Geometry, {FVector2D(C.X, C.Y - D), FVector2D(C.X + D, C.Y), FVector2D(C.X, C.Y + D), FVector2D(C.X - D, C.Y), FVector2D(C.X, C.Y - D)}, Col, 1.6f);
        }
        if (!bKey) return;
        // Name above the bracket, bars below it.
        if (NameOf) {
            const FString Name = NameOf(M.FleetId);
            if (!Name.IsEmpty())
                FSlateDrawElement::MakeText(Elements, Layer + 2, Geometry.ToPaintGeometry(FVector2f(320.f, 16.f),
                    FSlateLayoutTransform(FVector2f(static_cast<float>(C.X - H), static_cast<float>(C.Y - H - 18.0)))),
                    Name.ToUpper(), Caption(6), ESlateDrawEffect::None, Col);
        }
        const float W = static_cast<float>(FMath::Clamp(H * 2.0, 46.0, 180.0));
        const FVector2f Origin(static_cast<float>(C.X - W * 0.5), static_cast<float>(C.Y + H + 5.0));
        const FLinearColor Track = WithAlpha(Ui.Void, 0.7f);
        const FLinearColor HullCol = M.Hull > 0.5f ? Ui.Good : M.Hull > 0.25f ? Ui.Warn : Ui.Alert;
        PaintRect(Elements, Layer + 1, Geometry, Origin, FVector2f(W, 3.f), Track);
        PaintRect(Elements, Layer + 2, Geometry, Origin, FVector2f(W * M.Hull, 3.f), HullCol);
        if (M.Shields > 0.f || M.bTarget || M.bSelected) {
            PaintRect(Elements, Layer + 1, Geometry, Origin + FVector2f(0.f, 5.f), FVector2f(W, 3.f), Track);
            PaintRect(Elements, Layer + 2, Geometry, Origin + FVector2f(0.f, 5.f), FVector2f(W * M.Shields, 3.f), Ui.Shield);
        }
    }

    TWeakObjectPtr<ABotfCombatScene> Scene;
    const FSlateBrush* Brush = nullptr;
    std::function<bool()> Show;
    std::function<void(int32)> OnPick;
    std::function<FString(int32)> NameOf;
    bool bIsDragging = false;
    bool bMoved = false;
    FKey DragButton = EKeys::Invalid;
    FVector2D LastMousePos = FVector2D::ZeroVector;
    FVector2D PressPos = FVector2D::ZeroVector;
    int32 HoverFleet = 0;
};
class SShipStatusBar final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(SShipStatusBar)
        : _Hull(100), _MaxHull(100), _Shields(50), _MaxShields(50), _Empire(1) {}
        SLATE_ARGUMENT(int, Hull)
        SLATE_ARGUMENT(int, MaxHull)
        SLATE_ARGUMENT(int, Shields)
        SLATE_ARGUMENT(int, MaxShields)
        SLATE_ARGUMENT(int, Empire)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs) {
        Hull = InArgs._Hull;
        MaxHull = FMath::Max(1, InArgs._MaxHull);
        Shields = InArgs._Shields;
        MaxShields = FMath::Max(0, InArgs._MaxShields);
        Empire = InArgs._Empire;
    }

    virtual FVector2D ComputeDesiredSize(float) const override {
        return FVector2D(120, 12);
    }

    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                         FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override {
        const FVector2f Size = Geometry.GetLocalSize();
        const FPalette& Pal = Palette(Empire);
        const float Gap = 3.f;
        const float BarH = FMath::Max(3.f, (Size.Y - Gap) * 0.5f);
        const float R = BarH * 0.5f;
        const FVector4 Round(R, R, R, R);
        const float HullFrac = FMath::Clamp(static_cast<float>(Hull) / MaxHull, 0.f, 1.f);
        const FLinearColor HullColor = (HullFrac > 0.5f) ? Pal.Good : (HullFrac > 0.25f) ? Pal.Warn : Pal.Alert;
        const FLinearColor Track = WithAlpha(Pal.FieldHi, 0.95f);

        PaintRounded(Elements, Layer, Geometry, FVector2f::ZeroVector, FVector2f(Size.X, BarH), Round, Track);
        if (HullFrac > 0.f)
            PaintGradient(Elements, Layer + 1, Geometry, FVector2f::ZeroVector, FVector2f(FMath::Max(BarH, Size.X * HullFrac), BarH), Round,
                Lift(HullColor, 0.25f), Shade(HullColor, 0.85f));
        if (MaxShields > 0) {
            const float ShieldFrac = FMath::Clamp(static_cast<float>(Shields) / MaxShields, 0.f, 1.f);
            PaintRounded(Elements, Layer, Geometry, FVector2f(0.f, BarH + Gap), FVector2f(Size.X, BarH), Round, Track);
            if (ShieldFrac > 0.f)
                PaintGradient(Elements, Layer + 1, Geometry, FVector2f(0.f, BarH + Gap), FVector2f(FMath::Max(BarH, Size.X * ShieldFrac), BarH), Round,
                    Lift(Pal.Shield, 0.25f), Shade(Pal.Shield, 0.85f));
        }
        return Layer + 2;
    }

private:
    int Hull = 100, MaxHull = 100, Shields = 50, MaxShields = 50, Empire = 1;
};

} // namespace BotfUi
