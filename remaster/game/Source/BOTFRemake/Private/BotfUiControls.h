#pragma once
// UI-only interactive readouts: bars that can be clicked or dragged to set a value directly,
// as the original sliders allowed ("click near the middle of the slider", Tutorial Guide p28).

#include "BotfUiTheme.h"
#include "InputCoreTypes.h"
#include <functional>

namespace BotfUi {

// Segmented bar; clicking or dragging reports the step under the cursor (0..Steps).
class SPickBar final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(SPickBar) : _Steps(20), _Height(12.f) {}
        SLATE_ATTRIBUTE(int32, Value)          // lit steps
        SLATE_ATTRIBUTE(int32, Limit)          // steps that exist (blocks); defaults to Steps
        SLATE_ATTRIBUTE(FSlateColor, FillColor)
        SLATE_ATTRIBUTE(FSlateColor, EmptyColor)
        SLATE_ATTRIBUTE(bool, Locked)
        SLATE_ARGUMENT(int32, Steps)
        SLATE_ARGUMENT(float, Height)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args, std::function<void(int)> InPick) {
        Value = Args._Value; Limit = Args._Limit; FillColor = Args._FillColor; EmptyColor = Args._EmptyColor;
        Locked = Args._Locked; Steps = FMath::Max(1, Args._Steps); Height = Args._Height; Pick = MoveTemp(InPick);
    }
    int32 Count() const { return Limit.IsSet() ? FMath::Max(0, Limit.Get()) : Steps; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(140.0, Height); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                         FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool) const override {
        const FVector2D Size = Geometry.GetLocalSize();
        const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
        const int32 N = Count();
        if (N <= 0) return Layer;
        const float Gap = N > 40 ? 1.f : 2.f, W = FMath::Max(1.f, (static_cast<float>(Size.X) - Gap * (N - 1)) / N);
        const int32 Lit = FMath::Clamp(Value.Get(0), 0, N);
        FLinearColor On = FillColor.Get(FLinearColor::White).GetColor(Style);
        if (Locked.Get(false)) On = Shade(On, 0.55f);
        const FLinearColor Off = EmptyColor.Get(FLinearColor(0.1f, 0.1f, 0.1f)).GetColor(Style);
        const FLinearColor Hi = Lift(Off, 0.18f);
        for (int32 I = 0; I < N; ++I)
            FSlateDrawElement::MakeBox(Elements, Layer,
                Geometry.ToPaintGeometry(FVector2D(W, Size.Y), FSlateLayoutTransform(FVector2D(I * (W + Gap), 0))),
                White, ESlateDrawEffect::None, I < Lit ? On : (I < Hover ? Hi : Off));
        return Layer;
    }
    int32 StepAt(const FGeometry& Geometry, const FPointerEvent& Event) const {
        const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
        // GetLocalSize() returns FVector2f-based data in UE 5.1+: pin the type so FMath::Max is not float/double mixed.
        const FVector2D LocalSize = Geometry.GetLocalSize();
        const double X = FMath::Clamp(Local.X / FMath::Max(1.0, LocalSize.X), 0.0, 1.0);
        return FMath::Clamp(static_cast<int32>(FMath::RoundToDouble(X * Count())), 0, Count());
    }
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override {
        if (Event.GetEffectingButton() != EKeys::LeftMouseButton || Locked.Get(false) || !Pick) return FReply::Unhandled();
        bDragging = true;
        Pick(StepAt(Geometry, Event));
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }
    virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override {
        Hover = IsHovered() ? StepAt(Geometry, Event) : 0;
        Invalidate(EInvalidateWidgetReason::Paint);
        // Released outside our hearing (another window, alt-tab): stop, never keep setting values on hover.
        if (bDragging && !Event.IsMouseButtonDown(EKeys::LeftMouseButton)) {
            bDragging = false;
            return FReply::Handled().ReleaseMouseCapture();
        }
        if (bDragging && HasMouseCapture()) {
            const int32 Step = StepAt(Geometry, Event);
            if (Step != Value.Get(0)) Pick(Step);
            return FReply::Handled();
        }
        return FReply::Unhandled();
    }
    virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override {
        if (!bDragging) return FReply::Unhandled();
        bDragging = false;
        return FReply::Handled().ReleaseMouseCapture();
    }
    virtual void OnMouseLeave(const FPointerEvent& Event) override { SLeafWidget::OnMouseLeave(Event); Hover = 0; }
    virtual void OnMouseCaptureLost(const FCaptureLostEvent& Event) override { SLeafWidget::OnMouseCaptureLost(Event); bDragging = false; }
    bool IsDragging() const { return bDragging; }
    virtual FCursorReply OnCursorQuery(const FGeometry&, const FPointerEvent&) const override {
        return FCursorReply::Cursor(Locked.Get(false) ? EMouseCursor::SlashedCircle : EMouseCursor::Hand);
    }
private:
    TAttribute<int32> Value, Limit;
    TAttribute<FSlateColor> FillColor, EmptyColor;
    TAttribute<bool> Locked;
    int32 Steps = 20;
    float Height = 12.f;
    int32 Hover = 0;
    bool bDragging = false;
    std::function<void(int)> Pick;
};

} // namespace BotfUi
