#pragma once
// UI-only styling helpers for the BOTF Remake Slate interface.
// Brushes and button styles live in one function-local static so widgets never point at temporaries.

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace BotfUi {

struct FPalette {
    FLinearColor Void, Field, FieldHi, Primary, Secondary, Tertiary, Accent, Text, Muted, Ink, Alert, Good;
};

// Empire ids follow botf::empireName: 1 Federation, 2 Klingon, 3 Romulan, 4 Cardassian, 5 Ferengi.
inline const FPalette& Palette(int Empire) {
    static const FPalette Palettes[5] = {
        { // Federation: warm peach / orange / lavender LCARS on near-black violet
            FLinearColor(0.008f, 0.007f, 0.016f), FLinearColor(0.036f, 0.030f, 0.058f), FLinearColor(0.095f, 0.078f, 0.135f),
            FLinearColor(1.00f, 0.60f, 0.30f), FLinearColor(0.62f, 0.56f, 0.98f), FLinearColor(1.00f, 0.80f, 0.62f),
            FLinearColor(1.00f, 0.76f, 0.36f), FLinearColor(0.98f, 0.92f, 0.84f), FLinearColor(0.66f, 0.60f, 0.74f),
            FLinearColor(0.025f, 0.018f, 0.035f), FLinearColor(0.95f, 0.30f, 0.22f), FLinearColor(0.50f, 0.88f, 0.52f) },
        { // Klingon: blood red / bronze, angular
            FLinearColor(0.010f, 0.002f, 0.002f), FLinearColor(0.058f, 0.014f, 0.012f), FLinearColor(0.140f, 0.032f, 0.026f),
            FLinearColor(0.72f, 0.10f, 0.07f), FLinearColor(0.56f, 0.34f, 0.24f), FLinearColor(0.84f, 0.44f, 0.18f),
            FLinearColor(1.00f, 0.34f, 0.18f), FLinearColor(0.97f, 0.87f, 0.80f), FLinearColor(0.68f, 0.50f, 0.46f),
            FLinearColor(0.99f, 0.93f, 0.87f), FLinearColor(1.00f, 0.55f, 0.20f), FLinearColor(0.60f, 0.82f, 0.45f) },
        { // Romulan: jade green / imperial purple
            FLinearColor(0.004f, 0.011f, 0.009f), FLinearColor(0.018f, 0.046f, 0.037f), FLinearColor(0.045f, 0.105f, 0.085f),
            FLinearColor(0.26f, 0.74f, 0.44f), FLinearColor(0.58f, 0.42f, 0.86f), FLinearColor(0.58f, 0.88f, 0.64f),
            FLinearColor(0.66f, 1.00f, 0.70f), FLinearColor(0.87f, 0.97f, 0.91f), FLinearColor(0.54f, 0.68f, 0.62f),
            FLinearColor(0.010f, 0.030f, 0.020f), FLinearColor(0.95f, 0.36f, 0.30f), FLinearColor(0.66f, 1.00f, 0.70f) },
        { // Cardassian: amber / brown, blocky
            FLinearColor(0.012f, 0.008f, 0.004f), FLinearColor(0.058f, 0.040f, 0.021f), FLinearColor(0.125f, 0.085f, 0.042f),
            FLinearColor(0.88f, 0.60f, 0.20f), FLinearColor(0.68f, 0.47f, 0.28f), FLinearColor(0.96f, 0.80f, 0.46f),
            FLinearColor(1.00f, 0.74f, 0.24f), FLinearColor(0.97f, 0.91f, 0.78f), FLinearColor(0.68f, 0.58f, 0.44f),
            FLinearColor(0.035f, 0.022f, 0.010f), FLinearColor(0.95f, 0.32f, 0.20f), FLinearColor(0.60f, 0.85f, 0.45f) },
        { // Ferengi: gold / teal
            FLinearColor(0.004f, 0.010f, 0.012f), FLinearColor(0.020f, 0.046f, 0.052f), FLinearColor(0.045f, 0.105f, 0.115f),
            FLinearColor(0.96f, 0.76f, 0.24f), FLinearColor(0.22f, 0.72f, 0.70f), FLinearColor(0.99f, 0.88f, 0.54f),
            FLinearColor(1.00f, 0.86f, 0.32f), FLinearColor(0.96f, 0.95f, 0.86f), FLinearColor(0.56f, 0.68f, 0.66f),
            FLinearColor(0.012f, 0.028f, 0.030f), FLinearColor(0.95f, 0.34f, 0.26f), FLinearColor(0.45f, 0.92f, 0.72f) },
    };
    return Palettes[FMath::Clamp(Empire, 1, 5) - 1];
}

// Map ownership colours: distinct per empire regardless of the active theme.
inline FLinearColor FactionColor(int Empire) {
    switch (Empire) {
    case 1: return FLinearColor(0.40f, 0.62f, 1.00f);
    case 2: return FLinearColor(0.95f, 0.22f, 0.16f);
    case 3: return FLinearColor(0.32f, 0.88f, 0.42f);
    case 4: return FLinearColor(0.88f, 0.66f, 0.26f);
    case 5: return FLinearColor(0.22f, 0.82f, 0.78f);
    default: return FLinearColor(0.80f, 0.78f, 0.72f);
    }
}

inline FLinearColor Shade(const FLinearColor& C, float Scale, float Alpha = 1.f) {
    return FLinearColor(C.R * Scale, C.G * Scale, C.B * Scale, Alpha);
}
inline FLinearColor Lift(const FLinearColor& C, float T) {
    FLinearColor Out = FMath::Lerp(C, FLinearColor::White, T); Out.A = C.A; return Out;
}

enum class EShape : uint8 { Pill, Row, CapLeft, CapRight, ElbowTop, ElbowBottom, Block, Count };
enum class ERole : uint8 { Primary, Secondary, Tertiary, Alert, Accent };

inline FLinearColor RoleColor(const FPalette& P, ERole Role) {
    switch (Role) {
    case ERole::Secondary: return P.Secondary;
    case ERole::Tertiary: return P.Tertiary;
    case ERole::Alert: return P.Alert;
    case ERole::Accent: return P.Accent;
    default: return P.Primary;
    }
}

// Corner radii X=TL Y=TR Z=BR W=BL. Federation pills, Klingon blades, Romulan sweeps, Cardassian blocks, Ferengi rounds.
inline FVector4 Radii(int Empire, EShape Shape) {
    static const float Table[5][static_cast<int>(EShape::Count)][4] = {
        { {13,13,13,13}, {10,0,0,10}, {8,0,0,8},   {0,8,8,0},   {14,0,0,0}, {0,0,0,14}, {3,3,3,3} },
        { {0,10,0,10},   {0,7,0,0},   {0,0,0,8},   {8,0,0,0},   {0,0,0,0},  {0,0,0,0},  {0,0,0,0} },
        { {12,3,12,3},   {8,0,8,0},   {8,0,0,3},   {0,3,8,0},   {14,0,0,0}, {0,0,0,10}, {5,1,5,1} },
        { {4,4,4,4},     {3,0,0,3},   {4,0,0,4},   {0,4,4,0},   {8,0,0,0},  {0,0,0,8},  {2,2,2,2} },
        { {11,11,11,11}, {7,7,7,7},   {8,0,0,8},   {0,8,8,0},   {12,0,0,0}, {0,0,0,12}, {6,6,6,6} },
    };
    const float* R = Table[FMath::Clamp(Empire, 1, 5) - 1][static_cast<int>(Shape)];
    return FVector4(R[0], R[1], R[2], R[3]);
}

struct FStyles {
    FButtonStyle Bare;
    FSlateBrush Brushes[5][static_cast<int>(EShape::Count)];
    FStyles() {
        const FSlateNoResource None;
        Bare.SetNormal(None).SetHovered(None).SetPressed(None).SetDisabled(None)
            .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
        for (int E = 1; E <= 5; ++E)
            for (int S = 0; S < static_cast<int>(EShape::Count); ++S)
                Brushes[E - 1][S] = FSlateRoundedBoxBrush(FLinearColor::White, Radii(E, static_cast<EShape>(S)));
    }
    const FSlateBrush* Get(int Empire, EShape Shape) const {
        return &Brushes[FMath::Clamp(Empire, 1, 5) - 1][static_cast<int>(Shape)];
    }
};
inline const FStyles& Styles() { static const FStyles Instance; return Instance; }

inline FSlateFontInfo Font(int Size, bool Bold = false) {
    return FCoreStyle::GetDefaultFontStyle(Bold ? "Bold" : "Regular", Size);
}
inline FSlateFontInfo Condensed(int Size) {
    // Falls back to the default typeface if the condensed face is unavailable.
    return FCoreStyle::GetDefaultFontStyle("BoldCondensed", Size);
}

// Polyline in widget-local space. UE 5.1+ Slate takes line points as FVector2f (the FVector2D overload is a
// deprecation shim that later engine versions may drop), so every widget draws lines through this one helper.
inline void DrawLines(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
    const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness)
{
    TArray<FVector2f> Local;
    Local.Reserve(Points.Num());
    for (const FVector2D& Point : Points) Local.Add(FVector2f(static_cast<float>(Point.X), static_cast<float>(Point.Y)));
    FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Local), ESlateDrawEffect::None,
        Color, true, Thickness);
}

// Themed LCARS control. Pill = coloured button with dark ink; Row = dense list row with a state cap.
inline TSharedRef<SWidget> MakeButton(const TAttribute<FText>& Label, TFunction<void()> OnClick,
    TFunction<bool()> Enabled, TFunction<bool()> Selected, TFunction<int()> Empire,
    ERole Role, FMargin Padding, bool bRow, int32 FontSize, const FText& Tip)
{
    const TSharedRef<TWeakPtr<SButton>> Cell = MakeShared<TWeakPtr<SButton>>();
    auto IsOn = [Enabled] { return !Enabled || Enabled(); };
    auto IsSel = [Selected] { return Selected && Selected(); };
    auto Fill = [Cell, IsOn, IsSel, Empire, Role, bRow]() -> FSlateColor {
        const FPalette& P = Palette(Empire());
        const TSharedPtr<SButton> B = Cell->Pin();
        const bool Hover = B.IsValid() && B->IsHovered(), Pressed = B.IsValid() && B->IsPressed();
        const bool Focused = B.IsValid() && B->HasKeyboardFocus();
        if (bRow) {
            FLinearColor Base = IsSel() ? P.FieldHi : P.Field;
            if (!IsOn()) return Shade(Base, 0.7f);
            if (Pressed) return Shade(Base, 0.8f);
            if (Focused) return Lift(Base, 0.18f);
            return Hover ? Lift(Base, 0.07f) : Base;
        }
        const FLinearColor Base = IsSel() ? P.Accent : RoleColor(P, Role);
        if (!IsOn()) return Shade(Base, 0.28f, 0.9f);
        if (Pressed) return Shade(Base, 0.72f);
        if (Focused) return Lift(Base, 0.35f);
        return Hover ? Lift(Base, 0.24f) : Base;
    };
    auto Ink = [Cell, IsOn, IsSel, Empire, bRow]() -> FSlateColor {
        const FPalette& P = Palette(Empire());
        const TSharedPtr<SButton> B = Cell->Pin();
        const bool Focused = B.IsValid() && B->HasKeyboardFocus();
        if (bRow) return (IsSel() || Focused) ? P.Accent : IsOn() ? P.Text : P.Muted;
        return IsOn() ? P.Ink : Shade(P.Muted, 0.9f);
    };
    auto Cap = [Cell, IsOn, IsSel, Empire]() -> FSlateColor {
        const FPalette& P = Palette(Empire());
        const TSharedPtr<SButton> B = Cell->Pin();
        if (IsSel()) return P.Accent;
        if (!IsOn()) return Shade(P.Secondary, 0.35f);
        if (B.IsValid() && B->HasKeyboardFocus()) return P.Accent;
        return B.IsValid() && B->IsHovered() ? P.Primary : Shade(P.Secondary, 0.75f);
    };
    auto FocusRing = [Cell, Empire]() -> FSlateColor {
        const TSharedPtr<SButton> B = Cell->Pin();
        if (B.IsValid() && B->HasKeyboardFocus()) {
            return FSlateColor(Palette(Empire()).Accent);
        }
        return FSlateColor(FLinearColor::Transparent);
    };
    TSharedRef<SWidget> Content = bRow
        ? StaticCastSharedRef<SWidget>(SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
            [ SNew(SBox).WidthOverride(5)
                [ SNew(SBorder).BorderImage_Lambda([Empire] { return Styles().Get(Empire(), EShape::Block); })
                    .BorderBackgroundColor_Lambda(Cap) ] ]
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
            [ SNew(STextBlock).Text(Label).Font(Font(FontSize, false)).AutoWrapText(true).ColorAndOpacity_Lambda(Ink) ])
        // Pills size to their own label, so auto-wrap would feed the pill's previous width back in as the wrap width:
        // after a resolution or DPI change a stale narrow frame wraps "SAVE CAMPAIGN" and never widens again.
        // Pill labels stay on the lines they are written with; if a parent squeezes one, it ends in an ellipsis.
        : StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(Label).Font(Condensed(FontSize + 1))
            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            .Justification(ETextJustify::Center).ColorAndOpacity_Lambda(Ink));
    const TSharedRef<SButton> Button = SNew(SButton)
        .ButtonStyle(&Styles().Bare)
        .ContentPadding(FMargin(0))
        .IsEnabled_Lambda(IsOn)
        .OnClicked_Lambda([OnClick] { if (OnClick) OnClick(); return FReply::Handled(); })
        [ SNew(SBorder)
            .BorderImage_Lambda([Empire, bRow] { return Styles().Get(Empire(), bRow ? EShape::Row : EShape::Pill); })
            .BorderBackgroundColor_Lambda(FocusRing)
            .Padding(FMargin(1.f))
            [ SNew(SBorder)
                .BorderImage_Lambda([Empire, bRow] { return Styles().Get(Empire(), bRow ? EShape::Row : EShape::Pill); })
                .BorderBackgroundColor_Lambda(Fill)
                .Padding(Padding)
                .VAlign(VAlign_Center)
                .HAlign(bRow ? HAlign_Fill : HAlign_Center)
                [ Content ] ] ];
    *Cell = Button;
    if (!Tip.IsEmpty()) Button->SetToolTipText(Tip);
    return Button;
}

// Segmented LCARS readout bar. Fraction is clamped to 0..1.
class SSegmentBar final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(SSegmentBar) : _Fraction(0.f), _Segments(16), _Height(9.f) {}
        SLATE_ATTRIBUTE(float, Fraction)
        SLATE_ATTRIBUTE(FSlateColor, FillColor)
        SLATE_ATTRIBUTE(FSlateColor, EmptyColor)
        SLATE_ARGUMENT(int32, Segments)
        SLATE_ARGUMENT(float, Height)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) {
        Fraction = Args._Fraction; FillColor = Args._FillColor; EmptyColor = Args._EmptyColor;
        Segments = FMath::Max(1, Args._Segments); Height = Args._Height;
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Segments * 6.0, Height); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                         FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool) const override {
        const FVector2D Size = Geometry.GetLocalSize();
        const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
        const float Gap = 2.f, W = FMath::Max(1.f, (static_cast<float>(Size.X) - Gap * (Segments - 1)) / Segments);
        const int Lit = FMath::RoundToInt(FMath::Clamp(Fraction.Get(0.f), 0.f, 1.f) * Segments);
        const FLinearColor On = FillColor.Get(FLinearColor::White).GetColor(Style);
        const FLinearColor Off = EmptyColor.Get(FLinearColor(0.1f, 0.1f, 0.1f)).GetColor(Style);
        for (int I = 0; I < Segments; ++I)
            FSlateDrawElement::MakeBox(Elements, Layer,
                Geometry.ToPaintGeometry(FVector2D(W, Size.Y), FSlateLayoutTransform(FVector2D(I * (W + Gap), 0))),
                White, ESlateDrawEffect::None, I < Lit ? On : Off);
        return Layer;
    }
private:
    TAttribute<float> Fraction;
    TAttribute<FSlateColor> FillColor, EmptyColor;
    int32 Segments = 16;
    float Height = 9.f;
};

// Original-style structure slider readout: one block per structure, lit blocks are staffed (Tutorial Guide p8).
class SBlockMeter final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(SBlockMeter) : _Total(0), _Lit(0), _BlockWidth(10.f), _Height(14.f) {}
        SLATE_ATTRIBUTE(int32, Total)
        SLATE_ATTRIBUTE(int32, Lit)
        SLATE_ATTRIBUTE(FSlateColor, LitColor)
        SLATE_ATTRIBUTE(FSlateColor, DarkColor)
        SLATE_ARGUMENT(float, BlockWidth)
        SLATE_ARGUMENT(float, Height)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) {
        Total = Args._Total; Lit = Args._Lit; LitColor = Args._LitColor; DarkColor = Args._DarkColor;
        BlockWidth = Args._BlockWidth; Height = Args._Height;
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(120.0, Height); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                         FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool) const override {
        const FVector2D Size = Geometry.GetLocalSize();
        const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
        const int32 Count = FMath::Max(0, Total.Get(0));
        const float Gap = 3.f;
        const int32 Fit = FMath::Max(1, static_cast<int32>((Size.X + Gap) / (BlockWidth + Gap)));
        const float W = Count > Fit ? FMath::Max(2.f, static_cast<float>(Size.X - Gap * (Count - 1)) / Count) : BlockWidth;
        const int32 On = FMath::Clamp(Lit.Get(0), 0, Count);
        const FLinearColor A = LitColor.Get(FLinearColor::White).GetColor(Style);
        const FLinearColor B = DarkColor.Get(FLinearColor(0.1f, 0.1f, 0.1f)).GetColor(Style);
        for (int32 I = 0; I < Count; ++I)
            FSlateDrawElement::MakeBox(Elements, Layer,
                Geometry.ToPaintGeometry(FVector2D(W, Size.Y), FSlateLayoutTransform(FVector2D(I * (W + Gap), 0))),
                White, ESlateDrawEffect::None, I < On ? A : B);
        return Layer;
    }
private:
    TAttribute<int32> Total, Lit;
    TAttribute<FSlateColor> LitColor, DarkColor;
    float BlockWidth = 10.f, Height = 14.f;
};

} // namespace BotfUi
