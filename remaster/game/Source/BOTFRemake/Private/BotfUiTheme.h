#pragma once
// UI-only styling for the BOTF Remake Slate interface: design tokens, type, surfaces, buttons and tooltips.
//
// Design language (carried over from the browser beta in remaster/web): deep navy panels with a hairline border, one
// accent hue per empire, rounded pills, condensed uppercase labels over a clean text face, soft glows on interaction.
// Everything here is presentation. Widgets read colours from Palette(empire) at paint time so a theme change is live.

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include <functional>

namespace BotfUi {

// ---------------------------------------------------------------------------------------------------------------------
// Colour tokens
// ---------------------------------------------------------------------------------------------------------------------
inline FLinearColor Hex(uint32 Rgb, float Alpha = 1.f) {
    FLinearColor Out = FLinearColor::FromSRGBColor(FColor((Rgb >> 16) & 0xFF, (Rgb >> 8) & 0xFF, Rgb & 0xFF));
    Out.A = Alpha;
    return Out;
}
inline FLinearColor WithAlpha(const FLinearColor& C, float Alpha) { FLinearColor Out = C; Out.A = Alpha; return Out; }
inline FLinearColor Shade(const FLinearColor& C, float Scale, float Alpha = 1.f) {
    return FLinearColor(C.R * Scale, C.G * Scale, C.B * Scale, Alpha);
}
inline FLinearColor Lift(const FLinearColor& C, float T) {
    FLinearColor Out = FMath::Lerp(C, FLinearColor::White, T); Out.A = C.A; return Out;
}
inline FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, float T) {
    FLinearColor Out = FMath::Lerp(A, B, T); Out.A = FMath::Lerp(A.A, B.A, T); return Out;
}

struct FPalette {
    // Surfaces: Void (screen), Field (panel), FieldHi (raised / selected row).
    // Accents: Primary (the empire's main accent), Secondary, Tertiary, Accent (Primary lifted for headings and focus).
    FLinearColor Void, Field, FieldHi, Primary, Secondary, Tertiary, Accent, Text, Muted, Ink, Alert, Good;
    FLinearColor Text2, Line, LineHi, Warn, Info, Shield, Glow;
};

// Empire ids follow botf::empireName: 1 Federation, 2 Klingon, 3 Romulan, 4 Cardassian, 5 Ferengi.
inline FPalette MakePalette(uint32 Primary, uint32 Secondary, uint32 Tertiary, uint32 Ink) {
    FPalette P;
    const FLinearColor Prim = Hex(Primary);
    // The navy chrome is shared by every empire, tilted slightly toward the empire hue so the themes stay distinct.
    const auto Tilt = [&Prim](uint32 Navy) { return Mix(Hex(Navy), Prim, 0.035f); };
    P.Void = Tilt(0x04060c); P.Field = Tilt(0x0a1022); P.FieldHi = Tilt(0x131d3c);
    P.Primary = Prim; P.Secondary = Hex(Secondary); P.Tertiary = Hex(Tertiary);
    P.Accent = Lift(Prim, 0.22f);
    P.Text = Hex(0xe7edf9); P.Text2 = Hex(0xb2bfd8); P.Muted = Hex(0x7c8aa8);
    P.Ink = Hex(Ink);
    P.Alert = Hex(0xff5c50); P.Good = Hex(0x5fd98f); P.Warn = Hex(0xf5c451); P.Info = Hex(0x7fb3ff); P.Shield = Hex(0x6fd3ff);
    P.Line = Hex(0x8caae6, 0.14f); P.LineHi = Hex(0x8caae6, 0.30f);
    P.Glow = WithAlpha(Prim, 0.38f);
    return P;
}
inline const FPalette& Palette(int Empire) {
    static const FPalette Palettes[5] = {
        MakePalette(0x6fa8ff, 0xf2b45a, 0xa8b8ff, 0x050912),   // Federation
        MakePalette(0xe0484a, 0xc98a4b, 0x9e3a36, 0x120505),   // Klingon
        MakePalette(0x3fc98a, 0x93dcbf, 0x2b8c62, 0x03110a),   // Romulan
        MakePalette(0xd9a44a, 0xe9d6a6, 0x9c7437, 0x120c03),   // Cardassian
        MakePalette(0xf08a2c, 0xc8743d, 0xf5c16c, 0x130902),   // Ferengi
    };
    return Palettes[FMath::Clamp(Empire, 1, 5) - 1];
}

// The empire whose theme is showing. The interface publishes it (SetThemeEmpire) whenever it resolves its palette, so
// free-standing widgets (meters, icons, emblems) can colour themselves without every caller threading it through.
inline int& ThemeEmpireRef() { static int Value = 1; return Value; }
inline void SetThemeEmpire(int Empire) { ThemeEmpireRef() = FMath::Clamp(Empire, 1, 5); }
inline const FPalette& Theme() { return Palette(ThemeEmpireRef()); }

// Map ownership colours: distinct per empire regardless of the active theme (matches the beta's owner colours).
inline FLinearColor FactionColor(int Empire) {
    switch (Empire) {
    case 1: return Hex(0x5b9dff);
    case 2: return Hex(0xe2463f);
    case 3: return Hex(0x35c37f);
    case 4: return Hex(0xd6a13f);
    case 5: return Hex(0xf28c28);
    default: return Hex(0x9aa6b8);
    }
}

enum class EShape : uint8 { Pill, Row, CapLeft, CapRight, ElbowTop, ElbowBottom, Block, Count };
// Segment: an option inside a capsule switch; transparent until it is the selected one, then solid.
enum class ERole : uint8 { Primary, Secondary, Tertiary, Alert, Accent, Segment };

inline FLinearColor RoleColor(const FPalette& P, ERole Role) {
    switch (Role) {
    case ERole::Secondary: return P.Secondary;
    case ERole::Tertiary: return P.Tertiary;
    case ERole::Alert: return P.Alert;
    case ERole::Accent: return P.Accent;
    default: return P.Primary;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Shape: corner radii X=TL Y=TR Z=BR W=BL. Every empire is rounded; the hard-edged powers are tighter.
// ---------------------------------------------------------------------------------------------------------------------
inline FVector4 Radii(int Empire, EShape Shape) {
    static const float Base[static_cast<int>(EShape::Count)][4] = {
        {16,16,16,16}, {9,9,9,9}, {10,0,0,10}, {0,10,10,0}, {18,0,0,0}, {0,0,0,18}, {5,5,5,5} };
    static const float Scale[5] = { 1.0f, 0.42f, 0.8f, 0.55f, 1.0f };   // Federation, Klingon, Romulan, Cardassian, Ferengi
    const int E = FMath::Clamp(Empire, 1, 5) - 1;
    const float* R = Base[static_cast<int>(Shape)];
    FVector4 Out(R[0] * Scale[E], R[1] * Scale[E], R[2] * Scale[E], R[3] * Scale[E]);
    if (E == 2 && (Shape == EShape::Pill || Shape == EShape::Row)) Out = FVector4(R[0], R[1] * 0.3f, R[2], R[3] * 0.3f);   // Romulan sweep
    return Out;
}

struct FStyles {
    FButtonStyle Bare;
    FSlateBrush Brushes[5][static_cast<int>(EShape::Count)];
    FSlateBrush Panels[5];   // filled white with a hairline outline; tint the fill with BorderBackgroundColor
    FStyles() {
        const FSlateNoResource None;
        Bare.SetNormal(None).SetHovered(None).SetPressed(None).SetDisabled(None)
            .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
        for (int E = 1; E <= 5; ++E) {
            for (int S = 0; S < static_cast<int>(EShape::Count); ++S)
                Brushes[E - 1][S] = FSlateRoundedBoxBrush(FLinearColor::White, Radii(E, static_cast<EShape>(S)));
            Panels[E - 1] = FSlateRoundedBoxBrush(FLinearColor::White, Radii(E, EShape::Row), Hex(0x8caae6, 0.16f), 1.f);
        }
    }
    const FSlateBrush* Get(int Empire, EShape Shape) const {
        return &Brushes[FMath::Clamp(Empire, 1, 5) - 1][static_cast<int>(Shape)];
    }
    const FSlateBrush* Panel(int Empire) const { return &Panels[FMath::Clamp(Empire, 1, 5) - 1]; }
};
inline const FStyles& Styles() { static const FStyles Instance; return Instance; }

// ---------------------------------------------------------------------------------------------------------------------
// Type. Roboto ships with the engine: Regular for reading, Bold for emphasis, BoldCondensed (uppercase, tracked) for
// labels and headings. Sizes are authored for the original 1080p layout; this maps them up so nothing falls under
// 10 pt (about 13 px at 1080p), and the engine's DPI curve scales the rest with the viewport.
// ---------------------------------------------------------------------------------------------------------------------
inline int Effective(int Size) { return FMath::Max(10, Size + 2); }
inline FSlateFontInfo Font(int Size, bool Bold = false) {
    return FCoreStyle::GetDefaultFontStyle(Bold ? "Bold" : "Regular", Effective(Size));
}
inline FSlateFontInfo Condensed(int Size) {
    // Falls back to the default typeface if the condensed face is unavailable.
    FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle("BoldCondensed", Effective(Size));
    Info.LetterSpacing = 45;
    return Info;
}
// Small tracked caption (section labels, stat captions).
inline FSlateFontInfo Caption(int Size = 8) {
    FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle("BoldCondensed", Effective(Size));
    Info.LetterSpacing = 110;
    return Info;
}
// Large display figures (turn counter, stat values).
inline FSlateFontInfo Display(int Size) {
    FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle("BoldCondensed", Effective(Size));
    Info.LetterSpacing = 20;
    return Info;
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

// ---------------------------------------------------------------------------------------------------------------------
// Draw primitives
// ---------------------------------------------------------------------------------------------------------------------
inline const FSlateBrush* WhiteBrush() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }

// Rounded rectangle at a local offset and size, filled and/or outlined. Scratch brushes are safe: MakeBox copies the
// radii and outline into the element rather than keeping the brush.
inline void PaintRounded(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, FVector2f Offset, FVector2f Size,
    const FVector4& Radius, const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
{
    if (Size.X <= 0.f || Size.Y <= 0.f) return;
    if (Fill.A <= 0.001f && (Outline.A <= 0.001f || OutlineWidth <= 0.f)) return;
    // The fill colour is the element tint (MakeBox does not read the brush's own tint); the outline colour rides in the brush.
    const FSlateRoundedBoxBrush Brush(FLinearColor::White, Radius, Outline, OutlineWidth);
    FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Offset)), &Brush,
        ESlateDrawEffect::None, Fill);
}
inline void PaintRect(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, FVector2f Offset, FVector2f Size,
    const FLinearColor& Color)
{
    if (Size.X <= 0.f || Size.Y <= 0.f || Color.A <= 0.001f) return;
    FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Offset)), WhiteBrush(),
        ESlateDrawEffect::None, Color);
}
inline void PaintCircle(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, FVector2f Centre, float Radius,
    const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
{
    PaintRounded(Elements, Layer, Geometry, Centre - FVector2f(Radius, Radius), FVector2f(Radius * 2.f, Radius * 2.f),
        FVector4(Radius, Radius, Radius, Radius), Fill, Outline, OutlineWidth);
}
// Vertical two-stop gradient (Top at y=0, Bottom at y=height) with rounded corners.
inline void PaintGradient(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, FVector2f Offset, FVector2f Size,
    const FVector4& Radius, const FLinearColor& Top, const FLinearColor& Bottom)
{
    if (Size.X <= 0.f || Size.Y <= 0.f) return;
    TArray<FSlateGradientStop> Stops;
    Stops.Add(FSlateGradientStop(FVector2f(0.f, 0.f), Top));
    Stops.Add(FSlateGradientStop(Size, Bottom));
    FSlateDrawElement::MakeGradient(Elements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Offset)), MoveTemp(Stops),
        Orient_Vertical, ESlateDrawEffect::None, FVector4f(static_cast<float>(Radius.X), static_cast<float>(Radius.Y),
            static_cast<float>(Radius.Z), static_cast<float>(Radius.W)));
}
// Horizontal two-stop gradient.
inline void PaintGradientH(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, FVector2f Offset, FVector2f Size,
    const FLinearColor& Left, const FLinearColor& Right)
{
    if (Size.X <= 0.f || Size.Y <= 0.f) return;
    TArray<FSlateGradientStop> Stops;
    Stops.Add(FSlateGradientStop(FVector2f(0.f, 0.f), Left));
    Stops.Add(FSlateGradientStop(Size, Right));
    FSlateDrawElement::MakeGradient(Elements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Offset)), MoveTemp(Stops),
        Orient_Horizontal, ESlateDrawEffect::None, FVector4f(0.f));
}

// Triangle fan from Centre through Points (local space): fills any polygon that is star-shaped around Centre.
inline void PaintFan(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, FVector2f Centre,
    const TArray<FVector2f>& Points, const FLinearColor& Fill, const FLinearColor* RimColor = nullptr)
{
    if (Points.Num() < 3 || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer()) return;
    const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WhiteBrush());
    const FSlateRenderTransform& Transform = Geometry.GetAccumulatedRenderTransform();
    const FColor CentreColor = Fill.ToFColorSRGB();
    const FColor Rim = (RimColor ? *RimColor : Fill).ToFColorSRGB();
    TArray<FSlateVertex> Verts;
    TArray<SlateIndex> Indexes;
    Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Centre, FVector2f(0.5f, 0.5f), CentreColor));
    for (const FVector2f& Point : Points)
        Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Point, FVector2f(0.5f, 0.5f), Rim));
    const int32 Count = Points.Num();
    for (int32 I = 0; I < Count; ++I) {
        Indexes.Add(0); Indexes.Add(static_cast<SlateIndex>(1 + I)); Indexes.Add(static_cast<SlateIndex>(1 + (I + 1) % Count));
    }
    FSlateDrawElement::MakeCustomVerts(Elements, Layer, Handle, Verts, Indexes, nullptr, 0, 0);
}
// Soft radial glow: Colour at the centre fading to transparent at Radius.
inline void PaintGlow(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, FVector2f Centre, float Radius,
    const FLinearColor& Colour)
{
    TArray<FVector2f> Ring;
    const int32 Steps = 28;
    for (int32 I = 0; I < Steps; ++I) {
        const float A = 2.f * PI * I / Steps;
        Ring.Add(Centre + FVector2f(FMath::Cos(A), FMath::Sin(A)) * Radius);
    }
    const FLinearColor Rim = WithAlpha(Colour, 0.f);
    PaintFan(Elements, Layer, Geometry, Centre, Ring, Colour, &Rim);
}
// Filled circle with a smooth disc (fan), for star discs where a rounded box is overkill.
inline void PaintDisc(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, FVector2f Centre, float Radius,
    const FLinearColor& Colour, int32 Steps = 20)
{
    TArray<FVector2f> Ring;
    for (int32 I = 0; I < Steps; ++I) {
        const float A = 2.f * PI * I / Steps;
        Ring.Add(Centre + FVector2f(FMath::Cos(A), FMath::Sin(A)) * Radius);
    }
    PaintFan(Elements, Layer, Geometry, Centre, Ring, Colour);
}

// ---------------------------------------------------------------------------------------------------------------------
// SBotfSurface: the one paint-your-own-background widget. Panels, buttons, chips and rows are all surfaces; the look is
// a function evaluated at paint time so hover, selection and theme changes need no rebuild.
// ---------------------------------------------------------------------------------------------------------------------
struct FSurfaceLook {
    FLinearColor Fill = FLinearColor::Transparent;
    FLinearColor Top = FLinearColor::Transparent;        // gradient start; alpha 0 = flat fill
    FLinearColor Outline = FLinearColor::Transparent;
    float OutlineWidth = 1.f;
    FLinearColor Glow = FLinearColor::Transparent;       // soft halo behind the surface; alpha 0 = none
    float GlowSize = 0.f;
    FLinearColor Stripe = FLinearColor::Transparent;     // left marker bar; alpha 0 = none
    float StripeWidth = 3.f;
    FLinearColor Sheen = FLinearColor::Transparent;      // 1px highlight along the top edge
    FVector4 Radius = FVector4(9, 9, 9, 9);
};

class SBotfSurface final : public SCompoundWidget {
public:
    SLATE_BEGIN_ARGS(SBotfSurface) : _Padding(FMargin(0)), _HAlign(HAlign_Fill), _VAlign(VAlign_Fill) {}
        SLATE_ARGUMENT(std::function<FSurfaceLook()>, Look)
        SLATE_ARGUMENT(FMargin, Padding)
        SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
        SLATE_ARGUMENT(EVerticalAlignment, VAlign)
        SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) {
        LookFn = Args._Look;
        ChildSlot.HAlign(Args._HAlign).VAlign(Args._VAlign).Padding(Args._Padding) [ Args._Content.Widget ];
    }
    virtual int32 OnPaint(const FPaintArgs& PaintArgs, const FGeometry& Geometry, const FSlateRect& ClipRect,
                          FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override {
        const FSurfaceLook L = LookFn ? LookFn() : FSurfaceLook();
        const FVector2f Size = Geometry.GetLocalSize();
        if (L.Glow.A > 0.f && L.GlowSize > 0.f) {
            // Three fading rings read as a soft halo without a blur pass.
            for (int32 I = 3; I >= 1; --I) {
                const float Grow = L.GlowSize * I / 3.f;
                PaintRounded(Elements, Layer, Geometry, FVector2f(-Grow, -Grow), Size + FVector2f(Grow, Grow) * 2.f,
                    FVector4(L.Radius.X + Grow, L.Radius.Y + Grow, L.Radius.Z + Grow, L.Radius.W + Grow),
                    WithAlpha(L.Glow, L.Glow.A * 0.16f / I));
            }
        }
        if (L.Top.A > 0.001f) PaintGradient(Elements, Layer + 1, Geometry, FVector2f::ZeroVector, Size, L.Radius, L.Top, L.Fill);
        else PaintRounded(Elements, Layer + 1, Geometry, FVector2f::ZeroVector, Size, L.Radius, L.Fill);
        if (L.Sheen.A > 0.001f)
            PaintRect(Elements, Layer + 2, Geometry, FVector2f(FMath::Max(2.f, static_cast<float>(L.Radius.X) * 0.5f), 0.f),
                FVector2f(FMath::Max(0.f, Size.X - static_cast<float>(L.Radius.X + L.Radius.Y) * 0.5f - 4.f), 1.f), L.Sheen);
        if (L.Outline.A > 0.001f && L.OutlineWidth > 0.f)
            PaintRounded(Elements, Layer + 2, Geometry, FVector2f::ZeroVector, Size, L.Radius, FLinearColor::Transparent, L.Outline, L.OutlineWidth);
        if (L.Stripe.A > 0.001f && L.StripeWidth > 0.f)
            PaintRounded(Elements, Layer + 3, Geometry, FVector2f(3.f, 5.f), FVector2f(L.StripeWidth, FMath::Max(2.f, Size.Y - 10.f)),
                FVector4(L.StripeWidth * 0.5f, L.StripeWidth * 0.5f, L.StripeWidth * 0.5f, L.StripeWidth * 0.5f), L.Stripe);
        return SCompoundWidget::OnPaint(PaintArgs, Geometry, ClipRect, Elements, Layer + 4, Style, bParentEnabled);
    }
private:
    std::function<FSurfaceLook()> LookFn;
};

// Standard panel look: navy gradient, hairline border, soft top highlight.
inline FSurfaceLook PanelLook(int Empire, float Emphasis = 0.f, const FLinearColor* Edge = nullptr) {
    const FPalette& P = Palette(Empire);
    FSurfaceLook L;
    L.Top = WithAlpha(Mix(P.FieldHi, P.Field, 0.25f), 0.94f);
    L.Fill = WithAlpha(P.Field, 0.94f);
    L.Outline = Edge ? *Edge : Mix(P.Line, P.Primary, FMath::Clamp(Emphasis, 0.f, 1.f) * 0.5f);
    L.Sheen = WithAlpha(FLinearColor::White, 0.05f);
    L.Radius = Radii(Empire, EShape::Row) + FVector4(4, 4, 4, 4);
    return L;
}
// A panel around Content, themed by the interface's current empire.
inline TSharedRef<SWidget> MakePanel(const TSharedRef<SWidget>& Content, FMargin Padding = FMargin(12, 10),
    std::function<FSurfaceLook()> Look = nullptr)
{
    return SNew(SBotfSurface).Padding(Padding)
        .Look(Look ? Look : std::function<FSurfaceLook()>([] { return PanelLook(ThemeEmpireRef()); }))
        [ Content ];
}

// ---------------------------------------------------------------------------------------------------------------------
// Tooltips: a navy card with a hairline border in place of the engine's grey box.
// ---------------------------------------------------------------------------------------------------------------------
inline TSharedRef<SToolTip> MakeTip(const TAttribute<FText>& Body, const FText& Title = FText::GetEmpty()) {
    static const FSlateNoResource NoBrush;
    return SNew(SToolTip).BorderImage(&NoBrush)
        .TextMargin(FMargin(0))
        [ SNew(SBorder).BorderImage(Styles().Panel(ThemeEmpireRef())).BorderBackgroundColor_Lambda([] { return FSlateColor(WithAlpha(Theme().FieldHi, 0.98f)); })
            .Padding(FMargin(12, 9))
            [ SNew(SBox).MaxDesiredWidth(380)
                [ SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [ SNew(STextBlock).Visibility(Title.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible).Text(Title)
                        .Font(Condensed(10)).ColorAndOpacity_Lambda([] { return FSlateColor(Theme().Accent); }) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, Title.IsEmpty() ? 0 : 3, 0, 0)
                    [ SNew(STextBlock).Text(Body).Font(Font(9)).AutoWrapText(true)
                        .ColorAndOpacity_Lambda([] { return FSlateColor(Theme().Text2); }) ] ] ] ];
}
inline void SetTip(const TSharedRef<SWidget>& Widget, const TAttribute<FText>& Body, const FText& Title = FText::GetEmpty()) {
    Widget->SetToolTip(MakeTip(Body, Title));
}

// ---------------------------------------------------------------------------------------------------------------------
// Buttons. Solid = the one action that matters (Accent role, or a selected primary). Soft = an action (tinted, bordered).
// Ghost = an option or navigation (outline only; selection fills it). Row = a dense list entry with a selection stripe.
// ---------------------------------------------------------------------------------------------------------------------
inline TSharedRef<SWidget> MakeKeyChip(const FString& Key, TFunction<FLinearColor()> Colour) {
    return SNew(SBotfSurface).Padding(FMargin(5, 1, 5, 1))
        .Look([Colour] {
            FSurfaceLook L; const FLinearColor C = Colour();
            L.Fill = WithAlpha(C, 0.12f); L.Outline = WithAlpha(C, 0.55f); L.Radius = FVector4(4, 4, 4, 4);
            return L; })
        [ SNew(STextBlock).Text(FText::FromString(Key)).Font(Caption(7)).ColorAndOpacity_Lambda([Colour] { return FSlateColor(Colour()); }) ];
}

inline TSharedRef<SWidget> MakeButton(const TAttribute<FText>& Label, TFunction<void()> OnClick,
    TFunction<bool()> Enabled, TFunction<bool()> Selected, TFunction<int()> Empire,
    ERole Role, FMargin Padding, bool bRow, int32 FontSize, const FText& Tip, const FString& Key = FString())
{
    const TSharedRef<TWeakPtr<SButton>> Cell = MakeShared<TWeakPtr<SButton>>();
    auto IsOn = [Enabled] { return !Enabled || Enabled(); };
    auto IsSel = [Selected] { return Selected && Selected(); };
    struct FState { bool Hover, Pressed, Focus; };
    auto Probe = [Cell]() -> FState {
        const TSharedPtr<SButton> B = Cell->Pin();
        return B.IsValid() ? FState{ B->IsHovered(), B->IsPressed(), B->HasKeyboardFocus() } : FState{ false, false, false };
    };
    // Text colour is shared by the label and the key chip.
    auto TextColour = [Probe, IsOn, IsSel, Empire, Role, bRow]() -> FLinearColor {
        const FPalette& P = Palette(Empire());
        const FState S = Probe();
        if (!IsOn()) return WithAlpha(P.Muted, 0.8f);
        if (bRow) return IsSel() ? P.Text : (S.Hover ? P.Text : P.Text2);
        if (Role == ERole::Accent || (IsSel() && (Role == ERole::Primary || Role == ERole::Segment))) return P.Ink;
        if (Role == ERole::Alert) return S.Hover ? Lift(P.Alert, 0.55f) : Lift(P.Alert, 0.35f);
        if (Role == ERole::Primary) return S.Hover ? Lift(P.Primary, 0.7f) : Lift(P.Primary, 0.4f);
        return (IsSel() || S.Hover) ? P.Text : P.Text2;
    };
    auto Look = [Probe, IsOn, IsSel, Empire, Role, bRow]() -> FSurfaceLook {
        const int E = Empire();
        const FPalette& P = Palette(E);
        const FState S = Probe();
        const bool On = IsOn(), Sel = IsSel();
        FSurfaceLook L;
        L.Radius = Radii(E, bRow ? EShape::Row : EShape::Pill);
        const FLinearColor Hue = RoleColor(P, Role == ERole::Secondary || Role == ERole::Tertiary ? ERole::Primary : Role);
        if (bRow) {
            L.Fill = Sel ? WithAlpha(P.FieldHi, 0.95f) : WithAlpha(P.Field, S.Hover ? 0.85f : 0.55f);
            if (S.Hover && !Sel) L.Fill = WithAlpha(P.FieldHi, 0.7f);
            L.Outline = Sel ? WithAlpha(P.Primary, 0.55f) : (S.Hover ? P.LineHi : P.Line);
            L.Stripe = Sel ? P.Primary : (S.Hover ? WithAlpha(P.Primary, 0.5f) : FLinearColor::Transparent);
            if (S.Pressed) L.Fill = WithAlpha(P.Field, 1.f);
            if (S.Focus) { L.Outline = P.Accent; L.OutlineWidth = 2.f; }
            if (!On) { L.Fill.A *= 0.6f; L.Outline.A *= 0.6f; }
            return L;
        }
        const bool Solid = Role == ERole::Accent || (Sel && (Role == ERole::Primary || Role == ERole::Segment));
        if (Solid) {
            const FLinearColor Base = Role == ERole::Alert ? P.Alert : (Role == ERole::Accent ? P.Primary : P.Primary);
            L.Fill = S.Pressed ? Shade(Base, 0.72f) : Base;
            L.Top = S.Pressed ? L.Fill : Lift(Base, S.Hover ? 0.34f : 0.16f);
            L.Outline = WithAlpha(Lift(Base, 0.4f), 0.7f);
            L.Sheen = WithAlpha(FLinearColor::White, 0.4f);
            if (S.Hover && On) { L.Glow = P.Glow; L.GlowSize = 12.f; }
        } else if (Role == ERole::Segment) {
            L.Fill = WithAlpha(FLinearColor::White, S.Pressed ? 0.12f : (S.Hover ? 0.07f : 0.f));
            L.Outline = S.Hover ? WithAlpha(P.Primary, 0.6f) : FLinearColor::Transparent;
        } else if (Role == ERole::Primary || Role == ERole::Alert) {
            L.Fill = WithAlpha(Hue, S.Pressed ? 0.34f : (S.Hover ? 0.28f : 0.15f));
            L.Outline = WithAlpha(Hue, S.Hover ? 0.9f : 0.55f);
            if (S.Hover && On) { L.Glow = WithAlpha(Hue, 0.4f); L.GlowSize = 10.f; }
        } else {
            const FLinearColor Edge = Sel ? P.Primary : (S.Hover ? P.Primary : P.LineHi);
            L.Fill = Sel ? WithAlpha(P.Primary, 0.20f) : WithAlpha(FLinearColor::White, S.Pressed ? 0.09f : (S.Hover ? 0.06f : 0.02f));
            L.Outline = Sel ? WithAlpha(Edge, 0.95f) : (S.Hover ? WithAlpha(Edge, 0.7f) : Edge);
            if (Sel) { L.Glow = P.Glow; L.GlowSize = 6.f; }
        }
        if (S.Focus) { L.Outline = P.Accent; L.OutlineWidth = 2.f; L.Glow = P.Glow; L.GlowSize = 9.f; }
        if (!On) {
            L.Fill.A *= 0.45f; L.Top.A *= 0.45f; L.Outline.A *= 0.45f; L.Glow.A = 0.f; L.Sheen.A = 0.f;
            if (Solid) L.Fill = WithAlpha(P.Muted, 0.22f), L.Top = L.Fill;
        }
        return L;
    };
    TSharedRef<SWidget> Text = bRow
        ? StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(Label).Font(Font(FontSize, false)).AutoWrapText(true)
            .ColorAndOpacity_Lambda([TextColour] { return FSlateColor(TextColour()); }))
        // Pills size to their own label, so auto-wrap would feed the pill's previous width back in as the wrap width:
        // after a resolution or DPI change a stale narrow frame wraps "SAVE CAMPAIGN" and never widens again.
        // Pill labels stay on the lines they are written with; if a parent squeezes one, it ends in an ellipsis.
        : StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(Label).Font(Condensed(FontSize + 1))
            .OverflowPolicy(ETextOverflowPolicy::Ellipsis).Justification(ETextJustify::Center)
            .ColorAndOpacity_Lambda([TextColour] { return FSlateColor(TextColour()); }));
    const TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox);
    Content->AddSlot().FillWidth(1).VAlign(VAlign_Center) [ Text ];
    if (!Key.IsEmpty())
        Content->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0) [ MakeKeyChip(Key, [TextColour] { return TextColour(); }) ];
    const TSharedRef<SButton> Button = SNew(SButton)
        .ButtonStyle(&Styles().Bare)
        .ContentPadding(FMargin(0))
        // Not IsEnabled: the engine's disabled effect dims text a second time and makes labels unreadable. The look and
        // label colour already show the state, the click is ignored, and the tooltip explains why.
        .OnClicked_Lambda([OnClick, IsOn] { if (OnClick && IsOn()) OnClick(); return FReply::Handled(); })
        [ SNew(SBotfSurface).Look(Look)
            .Padding(bRow ? FMargin(Padding.Left + 9, Padding.Top, Padding.Right, Padding.Bottom) : Padding)
            .HAlign(HAlign_Fill).VAlign(VAlign_Center)
            [ bRow ? StaticCastSharedRef<SWidget>(Content) : StaticCastSharedRef<SWidget>(SNew(SBox).HAlign(HAlign_Center) [ Content ]) ] ];
    *Cell = Button;
    if (!Tip.IsEmpty()) SetTip(Button, Tip);
    return Button;
}

// A clickable card: any content over a surface whose look depends on hover, press, focus and enabled state.
inline TSharedRef<SWidget> MakeCardButton(const TSharedRef<SWidget>& Content,
    std::function<FSurfaceLook(bool Hover, bool Pressed, bool Focus, bool On)> Look,
    TFunction<void()> OnClick, TFunction<bool()> Enabled, const FText& Tip, FMargin Padding = FMargin(12, 10))
{
    const TSharedRef<TWeakPtr<SButton>> Cell = MakeShared<TWeakPtr<SButton>>();
    auto IsOn = [Enabled] { return !Enabled || Enabled(); };
    const TSharedRef<SButton> Button = SNew(SButton)
        .ButtonStyle(&Styles().Bare)
        .ContentPadding(FMargin(0))
        .OnClicked_Lambda([OnClick, IsOn] { if (OnClick && IsOn()) OnClick(); return FReply::Handled(); })
        [ SNew(SBotfSurface).Padding(Padding)
            .Look([Cell, Look, IsOn] {
                const TSharedPtr<SButton> B = Cell->Pin();
                return Look(B.IsValid() && B->IsHovered(), B.IsValid() && B->IsPressed(), B.IsValid() && B->HasKeyboardFocus(), IsOn());
            })
            [ Content ] ];
    *Cell = Button;
    if (!Tip.IsEmpty()) SetTip(Button, Tip);
    return Button;
}

// ---------------------------------------------------------------------------------------------------------------------
// Segmented readout bar and block meter (kept API, restyled: rounded segments on a dim track).
// ---------------------------------------------------------------------------------------------------------------------
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
        const FVector2f Size = Geometry.GetLocalSize();
        const float Gap = 2.f, W = FMath::Max(1.f, (Size.X - Gap * (Segments - 1)) / Segments);
        const int Lit = FMath::RoundToInt(FMath::Clamp(Fraction.Get(0.f), 0.f, 1.f) * Segments);
        const FLinearColor On = FillColor.Get(FLinearColor::White).GetColor(Style);
        const FLinearColor Off = WithAlpha(EmptyColor.Get(FLinearColor(0.1f, 0.1f, 0.1f)).GetColor(Style), 0.8f);
        const float R = FMath::Min(2.f, Size.Y * 0.5f);
        for (int I = 0; I < Segments; ++I)
            PaintRounded(Elements, Layer, Geometry, FVector2f(I * (W + Gap), 0.f), FVector2f(W, Size.Y), FVector4(R, R, R, R), I < Lit ? On : Off);
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
        const FVector2f Size = Geometry.GetLocalSize();
        const int32 Count = FMath::Max(0, Total.Get(0));
        const float Gap = 3.f;
        const int32 Fit = FMath::Max(1, static_cast<int32>((Size.X + Gap) / (BlockWidth + Gap)));
        const float W = Count > Fit ? FMath::Max(2.f, (Size.X - Gap * (Count - 1)) / Count) : BlockWidth;
        const int32 On = FMath::Clamp(Lit.Get(0), 0, Count);
        const FLinearColor A = LitColor.Get(FLinearColor::White).GetColor(Style);
        const FLinearColor B = WithAlpha(DarkColor.Get(FLinearColor(0.1f, 0.1f, 0.1f)).GetColor(Style), 0.8f);
        for (int32 I = 0; I < Count; ++I)
            PaintRounded(Elements, Layer, Geometry, FVector2f(I * (W + Gap), 0.f), FVector2f(W, Size.Y), FVector4(3, 3, 3, 3), I < On ? A : B);
        return Layer;
    }
private:
    TAttribute<int32> Total, Lit;
    TAttribute<FSlateColor> LitColor, DarkColor;
    float BlockWidth = 10.f, Height = 14.f;
};

} // namespace BotfUi
