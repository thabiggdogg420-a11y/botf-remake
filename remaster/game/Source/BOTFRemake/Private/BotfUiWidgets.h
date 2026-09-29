#pragma once
// Design-system widgets for the BOTF Remake interface: vector icons, empire emblems, meters, badges, stat tiles and the
// starfield backdrop. All original artwork: icons are drawn from lines and discs, emblems are the beta's geometric marks.

#include "BotfUiTheme.h"

namespace BotfUi {

// ---------------------------------------------------------------------------------------------------------------------
// Icons
// ---------------------------------------------------------------------------------------------------------------------
enum class EIcon : uint8 {
    Coin, Income, Research, System, Ship, Orders, Clock, Alert, Shield, Combat, ChevronLeft, ChevronRight, ChevronUp, ChevronDown,
    Eye, Diplomacy, Empire, Menu, Close, Plus, Minus, Check, Bolt, Target, Camera, Inbox, Flag, Star, Pop, Industry, Food, Hull,
    Pin, Dot
};

struct FIconPen {
    FSlateWindowElementList& Elements; int32 Layer; const FGeometry& Geometry; FVector2f Origin; float Size; FLinearColor Colour; float Width;
    FVector2f At(float X, float Y) const { return Origin + FVector2f(X, Y) * Size; }
    void Line(std::initializer_list<FVector2f> Points, bool bClosed = false) const {
        TArray<FVector2f> Local;
        for (const FVector2f& P : Points) Local.Add(At(P.X, P.Y));
        if (bClosed && Local.Num() > 1) { const FVector2f First = Local[0]; Local.Add(First); }
        FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Local), ESlateDrawEffect::None, Colour, true, Width);
    }
    void Ring(float Cx, float Cy, float Rx, float Ry, float TiltDeg = 0.f, int32 Steps = 24, float FromDeg = 0.f, float ToDeg = 360.f) const {
        TArray<FVector2f> Local;
        const float Tilt = FMath::DegreesToRadians(TiltDeg), Cs = FMath::Cos(Tilt), Sn = FMath::Sin(Tilt);
        for (int32 I = 0; I <= Steps; ++I) {
            const float A = FMath::DegreesToRadians(FMath::Lerp(FromDeg, ToDeg, static_cast<float>(I) / Steps));
            const float X = FMath::Cos(A) * Rx, Y = FMath::Sin(A) * Ry;
            Local.Add(At(Cx + X * Cs - Y * Sn, Cy + X * Sn + Y * Cs));
        }
        FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Local), ESlateDrawEffect::None, Colour, true, Width);
    }
    void Disc(float Cx, float Cy, float R) const { PaintCircle(Elements, Layer, Geometry, At(Cx, Cy), R * Size, Colour); }
    void Fill(std::initializer_list<FVector2f> Points, FVector2f Centre) const {
        TArray<FVector2f> Local;
        for (const FVector2f& P : Points) Local.Add(At(P.X, P.Y));
        PaintFan(Elements, Layer, Geometry, At(Centre.X, Centre.Y), Local, Colour);
    }
};

inline void DrawIcon(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, EIcon Icon, FVector2f Origin, float Size,
    const FLinearColor& Colour, float Width = 1.6f)
{
    const FIconPen Pen{ Elements, Layer, Geometry, Origin, Size, Colour, Width };
    using V = FVector2f;
    switch (Icon) {
    case EIcon::Coin:
        Pen.Ring(.5f, .5f, .42f, .42f); Pen.Ring(.5f, .5f, .22f, .22f, 0.f, 16); break;
    case EIcon::Income:
        Pen.Line({ V(.08f, .76f), V(.36f, .46f), V(.55f, .62f), V(.9f, .22f) }); Pen.Line({ V(.68f, .2f), V(.9f, .22f), V(.88f, .44f) }); break;
    case EIcon::Research:
        Pen.Line({ V(.38f, .08f), V(.38f, .40f), V(.14f, .84f), V(.86f, .84f), V(.62f, .40f), V(.62f, .08f) });
        Pen.Line({ V(.3f, .08f), V(.7f, .08f) }); Pen.Line({ V(.28f, .62f), V(.72f, .62f) }); break;
    case EIcon::System:
        Pen.Disc(.5f, .5f, .26f); Pen.Ring(.5f, .5f, .48f, .15f, -22.f, 28); break;
    case EIcon::Ship:
        Pen.Fill({ V(.5f, .04f), V(.92f, .92f), V(.5f, .70f), V(.08f, .92f) }, V(.5f, .62f)); break;
    case EIcon::Orders:
        for (int32 I = 0; I < 3; ++I) { const float Y = .22f + I * .28f; Pen.Disc(.14f, Y, .06f); Pen.Line({ V(.3f, Y), V(.92f, Y) }); } break;
    case EIcon::Clock:
        Pen.Ring(.5f, .5f, .44f, .44f, 0.f, 28); Pen.Line({ V(.5f, .24f), V(.5f, .52f), V(.7f, .62f) }); break;
    case EIcon::Alert:
        Pen.Line({ V(.5f, .08f), V(.94f, .88f), V(.06f, .88f) }, true); Pen.Line({ V(.5f, .38f), V(.5f, .62f) }); Pen.Disc(.5f, .76f, .045f); break;
    case EIcon::Shield:
        Pen.Line({ V(.5f, .06f), V(.88f, .2f), V(.84f, .56f), V(.5f, .94f), V(.16f, .56f), V(.12f, .2f) }, true); break;
    case EIcon::Combat:
        Pen.Line({ V(.14f, .14f), V(.86f, .86f) }); Pen.Line({ V(.86f, .14f), V(.14f, .86f) });
        Pen.Line({ V(.14f, .32f), V(.32f, .14f) }); Pen.Line({ V(.86f, .32f), V(.68f, .14f) }); break;
    case EIcon::ChevronLeft: Pen.Line({ V(.64f, .14f), V(.3f, .5f), V(.64f, .86f) }); break;
    case EIcon::ChevronRight: Pen.Line({ V(.36f, .14f), V(.7f, .5f), V(.36f, .86f) }); break;
    case EIcon::ChevronUp: Pen.Line({ V(.14f, .64f), V(.5f, .3f), V(.86f, .64f) }); break;
    case EIcon::ChevronDown: Pen.Line({ V(.14f, .36f), V(.5f, .7f), V(.86f, .36f) }); break;
    case EIcon::Eye:
        Pen.Line({ V(.04f, .5f), V(.28f, .24f), V(.5f, .18f), V(.72f, .24f), V(.96f, .5f), V(.72f, .76f), V(.5f, .82f), V(.28f, .76f) }, true);
        Pen.Ring(.5f, .5f, .14f, .14f, 0.f, 14); break;
    case EIcon::Diplomacy:
        Pen.Ring(.36f, .5f, .3f, .3f, 0.f, 22); Pen.Ring(.64f, .5f, .3f, .3f, 0.f, 22); break;
    case EIcon::Empire:
        Pen.Line({ V(.5f, .04f), V(.9f, .27f), V(.9f, .73f), V(.5f, .96f), V(.1f, .73f), V(.1f, .27f) }, true); Pen.Disc(.5f, .5f, .13f); break;
    case EIcon::Menu:
        for (int32 I = 0; I < 3; ++I) { const float Y = .25f + I * .25f; Pen.Line({ V(.14f, Y), V(.86f, Y) }); } break;
    case EIcon::Close: Pen.Line({ V(.2f, .2f), V(.8f, .8f) }); Pen.Line({ V(.8f, .2f), V(.2f, .8f) }); break;
    case EIcon::Plus: Pen.Line({ V(.5f, .18f), V(.5f, .82f) }); Pen.Line({ V(.18f, .5f), V(.82f, .5f) }); break;
    case EIcon::Minus: Pen.Line({ V(.18f, .5f), V(.82f, .5f) }); break;
    case EIcon::Check: Pen.Line({ V(.16f, .54f), V(.4f, .78f), V(.86f, .24f) }); break;
    case EIcon::Bolt:
        Pen.Line({ V(.6f, .04f), V(.2f, .56f), V(.48f, .56f), V(.4f, .96f), V(.8f, .4f), V(.52f, .4f) }, true); break;
    case EIcon::Target:
        Pen.Ring(.5f, .5f, .4f, .4f, 0.f, 26); Pen.Disc(.5f, .5f, .07f);
        Pen.Line({ V(.5f, .0f), V(.5f, .22f) }); Pen.Line({ V(.5f, .78f), V(.5f, 1.f) }); Pen.Line({ V(.0f, .5f), V(.22f, .5f) }); Pen.Line({ V(.78f, .5f), V(1.f, .5f) }); break;
    case EIcon::Camera:
        Pen.Line({ V(.06f, .3f), V(.3f, .3f), V(.38f, .16f), V(.62f, .16f), V(.7f, .3f), V(.94f, .3f), V(.94f, .84f), V(.06f, .84f) }, true);
        Pen.Ring(.5f, .56f, .2f, .2f, 0.f, 16); break;
    case EIcon::Inbox:
        Pen.Line({ V(.06f, .56f), V(.24f, .16f), V(.76f, .16f), V(.94f, .56f), V(.94f, .84f), V(.06f, .84f) }, true);
        Pen.Line({ V(.06f, .56f), V(.34f, .56f), V(.4f, .68f), V(.6f, .68f), V(.66f, .56f), V(.94f, .56f) }); break;
    case EIcon::Flag:
        Pen.Line({ V(.22f, .96f), V(.22f, .06f) }); Pen.Line({ V(.22f, .1f), V(.86f, .1f), V(.7f, .32f), V(.86f, .54f), V(.22f, .54f) }); break;
    case EIcon::Star: {
        TArray<FVector2f> P;
        for (int32 I = 0; I < 10; ++I) { const float A = -PI / 2.f + I * PI / 5.f, R = (I % 2 == 0) ? .48f : .2f; P.Add(V(.5f + FMath::Cos(A) * R, .52f + FMath::Sin(A) * R)); }
        for (FVector2f& Q : P) Q = Pen.At(Q.X, Q.Y);
        { const FVector2f First = P[0]; P.Add(First); }
        FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), MoveTemp(P), ESlateDrawEffect::None, Colour, true, Width);
        break; }
    case EIcon::Pop:
        Pen.Ring(.5f, .3f, .2f, .2f, 0.f, 16); Pen.Ring(.5f, 1.02f, .38f, .5f, 0.f, 16, 180.f, 360.f); break;
    case EIcon::Industry:
        Pen.Ring(.5f, .5f, .3f, .3f, 0.f, 16); Pen.Disc(.5f, .5f, .1f);
        for (int32 I = 0; I < 8; ++I) { const float A = I * PI / 4.f; Pen.Line({ V(.5f + FMath::Cos(A) * .3f, .5f + FMath::Sin(A) * .3f), V(.5f + FMath::Cos(A) * .46f, .5f + FMath::Sin(A) * .46f) }); }
        break;
    case EIcon::Food:
        Pen.Line({ V(.5f, .96f), V(.5f, .18f) });
        for (int32 I = 0; I < 3; ++I) { const float Y = .3f + I * .2f; Pen.Line({ V(.5f, Y + .1f), V(.24f, Y - .04f) }); Pen.Line({ V(.5f, Y + .1f), V(.76f, Y - .04f) }); }
        break;
    case EIcon::Hull:
        Pen.Line({ V(.5f, .96f), V(.08f, .44f), V(.08f, .16f), V(.5f, .3f), V(.92f, .16f), V(.92f, .44f) }, true); break;
    case EIcon::Pin:
        Pen.Ring(.5f, .36f, .26f, .26f, 0.f, 18); Pen.Line({ V(.3f, .55f), V(.5f, .96f), V(.7f, .55f) }); break;
    case EIcon::Dot: Pen.Disc(.5f, .5f, .28f); break;
    }
}

class SBotfIcon final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(SBotfIcon) : _Icon(EIcon::Dot), _Size(16.f), _Width(1.7f) {}
        SLATE_ATTRIBUTE(EIcon, Icon)
        SLATE_ATTRIBUTE(FSlateColor, Colour)
        SLATE_ARGUMENT(float, Size)
        SLATE_ARGUMENT(float, Width)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Icon = Args._Icon; Colour = Args._Colour; Size = Args._Size; Width = Args._Width; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements, int32 Layer,
                         const FWidgetStyle& Style, bool) const override {
        const FVector2f Local = Geometry.GetLocalSize();
        const float Side = FMath::Min(Local.X, Local.Y);
        const FLinearColor C = Colour.Get(FLinearColor(Theme().Text2)).GetColor(Style);
        DrawIcon(Elements, Layer, Geometry, Icon.Get(EIcon::Dot), FVector2f((Local.X - Side) * 0.5f, (Local.Y - Side) * 0.5f), Side, C, Width);
        return Layer;
    }
private:
    TAttribute<EIcon> Icon;
    TAttribute<FSlateColor> Colour;
    float Size = 16.f, Width = 1.7f;
};

inline TSharedRef<SWidget> MakeIcon(EIcon Icon, float Size, TFunction<FLinearColor()> Colour) {
    return SNew(SBotfIcon).Icon(Icon).Size(Size)
        .Colour(TAttribute<FSlateColor>::CreateLambda([Colour] { return FSlateColor(Colour()); }));
}

// ---------------------------------------------------------------------------------------------------------------------
// Empire emblem: non-colour owner cue (distinct silhouette per empire).
// ---------------------------------------------------------------------------------------------------------------------
inline void DrawEmblem(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry, int Empire, FVector2f Origin, float Size,
    const FLinearColor& Colour)
{
    using V = FVector2f;
    const FIconPen Pen{ Elements, Layer, Geometry, Origin, Size, Colour, 1.5f };
    const auto P = [](float X, float Y) { return V(X / 100.f, Y / 100.f); };
    switch (Empire) {
    case 1: Pen.Fill({ P(50, 8), P(78, 86), P(50, 68), P(22, 86) }, P(50, 62)); break;
    case 2: Pen.Fill({ P(50, 6), P(60, 42), P(94, 58), P(58, 60), P(50, 94), P(42, 60), P(6, 58), P(40, 42) }, P(50, 50)); break;
    case 3: Pen.Fill({ P(50, 20), P(62, 44), P(96, 30), P(72, 62), P(56, 60), P(50, 84), P(44, 60), P(28, 62), P(4, 30), P(38, 44) }, P(50, 52)); break;
    case 4: Pen.Fill({ P(10, 16), P(90, 16), P(78, 38), P(70, 38), P(50, 90), P(30, 38), P(22, 38) }, P(50, 30)); break;
    default:
        PaintCircle(Elements, Layer, Geometry, Origin + V(.5f, .5f) * Size, Size * 0.34f, FLinearColor::Transparent, Colour, Size * 0.16f); break;
    }
}
class SBotfEmblem final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(SBotfEmblem) : _Empire(1), _Size(24.f) {}
        SLATE_ATTRIBUTE(int, Empire)
        SLATE_ARGUMENT(float, Size)
        SLATE_ATTRIBUTE(FSlateColor, Colour)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Empire = Args._Empire; Size = Args._Size; Colour = Args._Colour; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements, int32 Layer,
                         const FWidgetStyle& Style, bool) const override {
        const FVector2f Local = Geometry.GetLocalSize();
        const float Side = FMath::Min(Local.X, Local.Y);
        const int E = Empire.Get(1);
        const FLinearColor C = Colour.IsSet() ? Colour.Get().GetColor(Style) : FactionColor(E);
        DrawEmblem(Elements, Layer, Geometry, E, FVector2f((Local.X - Side) * 0.5f, (Local.Y - Side) * 0.5f), Side, C);
        return Layer;
    }
private:
    TAttribute<int> Empire;
    TAttribute<FSlateColor> Colour;
    float Size = 24.f;
};

// ---------------------------------------------------------------------------------------------------------------------
// Meter: rounded track, gradient fill, optional ghost (preview) fill, optional segment ticks.
// ---------------------------------------------------------------------------------------------------------------------
class SBotfMeter final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(SBotfMeter) : _Value(0.f), _Ghost(-1.f), _Height(7.f), _Segments(0) {}
        SLATE_ATTRIBUTE(float, Value)
        SLATE_ATTRIBUTE(float, Ghost)
        SLATE_ATTRIBUTE(FSlateColor, Fill)
        SLATE_ARGUMENT(float, Height)
        SLATE_ARGUMENT(int32, Segments)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Value = Args._Value; Ghost = Args._Ghost; Fill = Args._Fill; Height = Args._Height; Segments = Args._Segments; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(40.0, Height); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements, int32 Layer,
                         const FWidgetStyle& Style, bool) const override {
        const FVector2f Size = Geometry.GetLocalSize();
        const FPalette& P = Theme();
        const float R = Size.Y * 0.5f;
        const FVector4 Round(R, R, R, R);
        const FLinearColor Colour = Fill.Get(FSlateColor(P.Primary)).GetColor(Style);
        PaintRounded(Elements, Layer, Geometry, FVector2f::ZeroVector, Size, Round, WithAlpha(P.FieldHi, 0.9f));
        const float G = Ghost.Get(-1.f);
        if (G >= 0.f) PaintRounded(Elements, Layer + 1, Geometry, FVector2f::ZeroVector, FVector2f(FMath::Max(Size.Y, Size.X * FMath::Clamp(G, 0.f, 1.f)), Size.Y),
            Round, WithAlpha(Colour, 0.32f));
        const float V = FMath::Clamp(Value.Get(0.f), 0.f, 1.f);
        if (V > 0.001f) {
            const FVector2f FillSize(FMath::Max(Size.Y, Size.X * V), Size.Y);
            PaintGradient(Elements, Layer + 2, Geometry, FVector2f::ZeroVector, FillSize, Round, Lift(Colour, 0.28f), Shade(Colour, 0.86f));
        }
        if (Segments > 1)
            for (int32 I = 1; I < Segments; ++I)
                PaintRect(Elements, Layer + 3, Geometry, FVector2f(Size.X * I / Segments - 0.5f, 0.f), FVector2f(1.f, Size.Y), WithAlpha(P.Void, 0.85f));
        return Layer + 3;
    }
private:
    TAttribute<float> Value, Ghost;
    TAttribute<FSlateColor> Fill;
    float Height = 7.f;
    int32 Segments = 0;
};

// ---------------------------------------------------------------------------------------------------------------------
// Starfield backdrop for the menu screens: navy gradient, empire-tinted nebulae, a lit horizon and a scatter of stars.
// ---------------------------------------------------------------------------------------------------------------------
class SBotfBackdrop final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(SBotfBackdrop) : _Horizon(true) {}
        SLATE_ARGUMENT(bool, Horizon)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) {
        bHorizon = Args._Horizon;
        uint32 Seed = 0x9e3779b9u;
        const auto Next = [&Seed] { Seed = Seed * 1664525u + 1013904223u; return static_cast<float>(Seed >> 8) / 16777216.f; };
        for (int32 I = 0; I < 260; ++I) Stars.Add({ Next(), Next(), 0.5f + Next() * 1.4f, 0.15f + Next() * 0.65f, Next() });
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(64.0, 64.0); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements, int32 Layer,
                         const FWidgetStyle&, bool) const override {
        const FVector2f Size = Geometry.GetLocalSize();
        const FPalette& P = Theme();
        PaintGradient(Elements, Layer, Geometry, FVector2f::ZeroVector, Size, FVector4(0, 0, 0, 0), Mix(P.Void, Hex(0x070b18), 0.5f), P.Void);
        const float Scale = FMath::Max(Size.X, Size.Y);
        PaintGlow(Elements, Layer + 1, Geometry, FVector2f(Size.X * 0.22f, Size.Y * 0.28f), Scale * 0.42f, WithAlpha(P.Primary, 0.10f));
        PaintGlow(Elements, Layer + 1, Geometry, FVector2f(Size.X * 0.82f, Size.Y * 0.72f), Scale * 0.46f, WithAlpha(P.Secondary, 0.07f));
        PaintGlow(Elements, Layer + 1, Geometry, FVector2f(Size.X * 0.55f, Size.Y * 0.05f), Scale * 0.3f, WithAlpha(P.Tertiary, 0.05f));
        for (const FStar& S : Stars) {
            const FLinearColor C = Mix(Hex(0xdfe9ff), P.Accent, S.Tint * 0.35f);
            PaintRect(Elements, Layer + 2, Geometry, FVector2f(S.X * Size.X, S.Y * Size.Y), FVector2f(S.Size, S.Size), WithAlpha(C, S.Alpha));
        }
        if (bHorizon) {
            // A distant world's limb rising along the bottom edge.
            const FVector2f Centre(Size.X * 0.5f, Size.Y + Scale * 1.05f);
            const float Radius = Scale * 1.22f;
            const FLinearColor Rim = WithAlpha(P.Primary, 0.34f);
            PaintCircle(Elements, Layer + 3, Geometry, Centre, Radius, WithAlpha(Mix(P.Void, P.Field, 0.6f), 1.f), Rim, 2.f);
            PaintCircle(Elements, Layer + 3, Geometry, Centre, Radius + 6.f, FLinearColor::Transparent, WithAlpha(P.Primary, 0.10f), 8.f);
            PaintGlow(Elements, Layer + 3, Geometry, FVector2f(Size.X * 0.5f, Size.Y * 0.98f), Scale * 0.34f, WithAlpha(P.Primary, 0.16f));
        }
        return Layer + 3;
    }
private:
    struct FStar { float X, Y, Size, Alpha, Tint; };
    TArray<FStar> Stars;
    bool bHorizon = true;
};

// Fading header rule: accent at the left, transparent at the right.
class SBotfRule final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(SBotfRule) {}
    SLATE_END_ARGS()
    void Construct(const FArguments&) {}
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(20.0, 4.0); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements, int32 Layer,
                         const FWidgetStyle&, bool) const override {
        const FVector2f Size = Geometry.GetLocalSize();
        const FLinearColor C = Theme().Primary;
        PaintGradientH(Elements, Layer, Geometry, FVector2f(0.f, Size.Y * 0.5f - 1.f), FVector2f(Size.X, 2.f), WithAlpha(C, 0.7f), WithAlpha(C, 0.f));
        return Layer;
    }
};

// Two-sided comparison bar: the left share in one colour, the right in another, a tick at the balance point.
class SBotfVersus final : public SLeafWidget {
public:
    SLATE_BEGIN_ARGS(SBotfVersus) : _Fraction(0.5f), _Height(10.f) {}
        SLATE_ATTRIBUTE(float, Fraction)
        SLATE_ATTRIBUTE(FSlateColor, Left)
        SLATE_ATTRIBUTE(FSlateColor, Right)
        SLATE_ARGUMENT(float, Height)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) { Fraction = Args._Fraction; Left = Args._Left; Right = Args._Right; Height = Args._Height; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(60.0, Height); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements, int32 Layer,
                         const FWidgetStyle& Style, bool) const override {
        const FVector2f Size = Geometry.GetLocalSize();
        const float R = Size.Y * 0.5f;
        const float F = FMath::Clamp(Fraction.Get(0.5f), 0.02f, 0.98f);
        const FLinearColor L = Left.Get(FSlateColor(Theme().Primary)).GetColor(Style), Rt = Right.Get(FSlateColor(Theme().Alert)).GetColor(Style);
        PaintRounded(Elements, Layer, Geometry, FVector2f::ZeroVector, Size, FVector4(R, R, R, R), Shade(Rt, 0.9f));
        PaintGradient(Elements, Layer + 1, Geometry, FVector2f::ZeroVector, FVector2f(Size.X * F, Size.Y), FVector4(R, 0, 0, R), Lift(L, 0.22f), Shade(L, 0.9f));
        PaintRect(Elements, Layer + 2, Geometry, FVector2f(Size.X * F - 1.f, -2.f), FVector2f(2.f, Size.Y + 4.f), WithAlpha(FLinearColor::White, 0.9f));
        return Layer + 2;
    }
private:
    TAttribute<float> Fraction;
    TAttribute<FSlateColor> Left, Right;
    float Height = 10.f;
};

} // namespace BotfUi
