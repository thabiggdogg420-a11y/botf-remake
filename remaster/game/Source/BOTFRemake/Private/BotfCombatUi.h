#pragma once
// Combat-only UI helpers for the BOTF Remake Slate interface:
// - STacticalViewport: Full-screen interactive 3D combat viewport with mouse orbit drag, scroll-wheel zoom, and reset.
// - SShipStatusBar: Compact high-contrast Hull and Shields telemetry status bars.

#include "CoreMinimal.h"
#include "BotfUiTheme.h"
#include "BotfCombatScene.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include <functional>

namespace BotfUi {

class STacticalViewport final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(STacticalViewport) {}
    SLATE_END_ARGS()

    void Construct(const FArguments&, ABotfCombatScene* InScene, const FSlateBrush* InBrush, std::function<bool()> InShow) {
        Scene = InScene;
        Brush = InBrush;
        Show = MoveTemp(InShow);
    }

    virtual FVector2D ComputeDesiredSize(float) const override {
        return FVector2D(1280, 800);
    }

    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                         FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override {
        if (!Brush || !Brush->GetResourceObject() || (Show && !Show())) return Layer;
        const FVector2D Size = Geometry.GetLocalSize();
        FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform()),
            Brush, ESlateDrawEffect::IgnoreTextureAlpha, FLinearColor::White);
        return Layer;
    }

    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override {
        if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || MouseEvent.GetEffectingButton() == EKeys::RightMouseButton) {
            bIsDragging = true;
            LastMousePos = MouseEvent.GetScreenSpacePosition();
            return FReply::Handled().CaptureMouse(AsShared());
        }
        if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton) {
            if (Scene) Scene->ResetCameraOrbit();
            return FReply::Handled();
        }
        return FReply::Unhandled();
    }

    virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent& MouseEvent) override {
        if (bIsDragging && (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)) {
            bIsDragging = false;
            return FReply::Handled().ReleaseMouseCapture();
        }
        return FReply::Unhandled();
    }

    virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent& MouseEvent) override {
        if (bIsDragging && Scene) {
            const FVector2D CurrentPos = MouseEvent.GetScreenSpacePosition();
            const FVector2D Delta = CurrentPos - LastMousePos;
            LastMousePos = CurrentPos;
            Scene->AddCameraOrbit(Delta.X * 0.35f, -Delta.Y * 0.35f);
            return FReply::Handled();
        }
        return FReply::Unhandled();
    }

    virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent& MouseEvent) override {
        if (Scene) {
            Scene->AddCameraZoom(-MouseEvent.GetWheelDelta() * 140.f);
            return FReply::Handled();
        }
        return FReply::Unhandled();
    }

    virtual FReply OnMouseButtonDoubleClick(const FGeometry&, const FPointerEvent& MouseEvent) override {
        if (Scene) {
            Scene->ResetCameraOrbit();
            return FReply::Handled();
        }
        return FReply::Unhandled();
    }

private:
    ABotfCombatScene* Scene = nullptr;
    const FSlateBrush* Brush = nullptr;
    std::function<bool()> Show;
    bool bIsDragging = false;
    FVector2D LastMousePos = FVector2D::ZeroVector;
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
        return FVector2D(120, 14);
    }

    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                         FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override {
        const FVector2D Size = Geometry.GetLocalSize();
        const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
        const FPalette& Pal = Palette(Empire);

        const double BarH = FMath::Max(2.0, (Size.Y - 2.0) * 0.5);
        const double BarW = Size.X;
        const float HullFrac = FMath::Clamp(static_cast<float>(Hull) / MaxHull, 0.f, 1.f);
        const FLinearColor HullColor = (HullFrac > 0.5f) ? Pal.Good : (HullFrac > 0.25f) ? Pal.Tertiary : Pal.Alert;

        // Background & Hull Bar
        FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(FVector2D(BarW, BarH), FSlateLayoutTransform(FVector2D(0, 0))), White, ESlateDrawEffect::None, Shade(Pal.Void, 0.45f));
        FSlateDrawElement::MakeBox(Elements, Layer + 1, Geometry.ToPaintGeometry(FVector2D(BarW * HullFrac, BarH), FSlateLayoutTransform(FVector2D(0, 0))), White, ESlateDrawEffect::None, HullColor);

        // Shields Bar
        if (MaxShields > 0) {
            const float ShieldFrac = FMath::Clamp(static_cast<float>(Shields) / MaxShields, 0.f, 1.f);
            FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(FVector2D(BarW, BarH), FSlateLayoutTransform(FVector2D(0, BarH + 2.0))), White, ESlateDrawEffect::None, Shade(Pal.Void, 0.45f));
            FSlateDrawElement::MakeBox(Elements, Layer + 1, Geometry.ToPaintGeometry(FVector2D(BarW * ShieldFrac, BarH), FSlateLayoutTransform(FVector2D(0, BarH + 2.0))), White, ESlateDrawEffect::None, Pal.Secondary);
        }
        return Layer + 2;
    }

private:
    int Hull = 100, MaxHull = 100, Shields = 50, MaxShields = 50, Empire = 1;
};

} // namespace BotfUi
