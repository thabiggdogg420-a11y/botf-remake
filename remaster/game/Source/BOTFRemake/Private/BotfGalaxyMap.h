#pragma once
// UI-only: the full-screen star chart at the heart of the command deck.
// Input model (modernised from the original Main Galactic screen, Tutorial Guide p3-7, p13-14):
//   left click      select a sector; clicking the same sector again cycles the units in it
//   double click    activate: open the colony, the race's dossier, or centre on a unit
//   right click     order the selected unit (course / attack / colonize); with nothing selected, or with
//                   Shift held, open the sector's context menu
//   left/middle drag pan; mouse wheel zooms about the cursor

#include "BotfUiTheme.h"
#include "BotfViewModel.h"
#include "BotfGameplayViewModel.h"
#include "Simulation.h"
#include "InputCoreTypes.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <vector>

namespace BotfUi {

enum class EMapMode : uint8 { Military, Economic };

// Shared view state owned by the interface. The chart mutates only Zoom and Pan.
struct FMapView {
    EMapMode Mode = EMapMode::Military;
    bool bNames = true, bRange = true, bGrid = false, bCourses = true;
    bool bAnomalies = true;   // Schema 13: charted nebulae, black holes, neutron stars, ion storms, wormholes, rifts
    bool bStations = true;    // Schema 13: supply (bright) and sensor (faint) rings of own stations and the selected one
    bool bPlotting = false;   // Course plotting: the next left click sets the selected unit's destination.
    float Zoom = 1.f;
    FVector2D Pan = FVector2D::ZeroVector;
};

inline FString MapStr(const std::string& Value) { return UTF8_TO_TCHAR(Value.c_str()); }

// A trade route is shown only when the player could know both ends: both charted, and for a rival's route both in
// sensor range now. A line must never point at a system the chart does not show.
inline bool TradeRouteKnown(const botf::GameState& State, const botf::TradeRoute& Route) {
    const bool Own = Route.ownerId == State.playerEmpire;
    for (const int Id : { Route.fromSystemId, Route.toSystemId }) {
        const auto S = std::find_if(State.systems.begin(), State.systems.end(), [Id](const botf::System& X) { return X.id == Id; });
        if (S == State.systems.end() || !botf::explored(State, S->x, S->y)) return false;
        if (!Own && !botf::visible(State, State.playerEmpire, S->x, S->y)) return false;
    }
    return true;
}

class SStarChart final : public SLeafWidget {
public:
    // Slate's required-argument forwarding is limited, so the chart takes one bundled binding.
    struct FBinding {
        const botf::GameState* State = nullptr;                   // committed campaign
        std::function<const botf::GameState&()> Draft;             // committed campaign plus this turn's orders
        const int* Selected = nullptr;                             // selected system id (0 = none)
        const int* Cell = nullptr;                                 // selected sector
        const int* Fleet = nullptr;                                // selected own ship (0 = none)
        const int* TaskForce = nullptr;                            // selected task force (0 = none); its range is its shortest-ranged member's
        std::function<std::uint64_t()> Version;                    // changes whenever the draft changes; keys the paint caches
        FMapView* View = nullptr;
        std::function<void(int)> Select, Activate, Plot;
        std::function<void(int, FVector2D, bool)> Command;         // cell, local position, force context menu
        std::function<FString(int)> Hint;                          // cursor hint for a hovered sector
    };
    SLATE_BEGIN_ARGS(SStarChart) {} SLATE_END_ARGS()
    void Construct(const FArguments&, const FBinding& In) {
        B = In;
        SetClipping(EWidgetClipping::ClipToBounds);
    }
    static constexpr double Margin = 24;
    int HoveredCell() const { return HoverCell; }
    FVector2D ViewSize() const { return LastSize; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(640, 420); }
    virtual bool SupportsKeyboardFocus() const override { return false; }

    // ---- Geometry: square sectors, fitted to the widget, then zoomed and panned ----
    double CellSize(FVector2D Size) const {
        const auto& S = *B.State;
        const double Fit = FMath::Max(4.0, FMath::Min((Size.X - 2 * Margin) / S.width, (Size.Y - 2 * Margin) / S.height));
        return Fit * B.View->Zoom;
    }
    FVector2D Offset(FVector2D Size) const {
        const double C = CellSize(Size);
        return (Size - FVector2D(C * B.State->width, C * B.State->height)) * 0.5 + B.View->Pan;
    }
    FVector2D Origin(int X, int Y, FVector2D Size) const { const double C = CellSize(Size); return Offset(Size) + FVector2D(X * C, Y * C); }
    FVector2D Centre(int X, int Y, FVector2D Size) const { const double C = CellSize(Size); return Origin(X, Y, Size) + FVector2D(C, C) * 0.5; }
    int CellAtLocal(FVector2D Local, FVector2D Size) const {
        const double C = CellSize(Size);
        const FVector2D P = (Local - Offset(Size)) / C;
        if (P.X < 0 || P.Y < 0 || P.X >= B.State->width || P.Y >= B.State->height) return -1;
        return static_cast<int>(P.Y) * B.State->width + static_cast<int>(P.X);
    }
    // Centres the view on a sector without changing zoom.
    void CentreOn(int CellId) {
        if (!B.State || CellId < 0 || LastSize.X <= 0) return;
        const FVector2D Want = LastSize * 0.5;
        const FVector2D Now = Centre(CellId % B.State->width, CellId / B.State->width, LastSize);
        B.View->Pan += Want - Now;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    void ZoomBy(float Factor, FVector2D Anchor) {
        if (LastSize.X <= 0) return;
        const double Before = CellSize(LastSize);
        const FVector2D World = (Anchor - Offset(LastSize)) / Before;
        B.View->Zoom = FMath::Clamp(B.View->Zoom * Factor, 0.6f, 4.5f);
        const double After = CellSize(LastSize);
        // Keep the world point under the anchor fixed.
        const FVector2D Base = (LastSize - FVector2D(After * B.State->width, After * B.State->height)) * 0.5;
        B.View->Pan = Anchor - World * After - Base;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    void ResetView() { B.View->Zoom = 1.f; B.View->Pan = FVector2D::ZeroVector; Invalidate(EInvalidateWidgetReason::Paint); }

    const botf::Fleet* OwnShip(const botf::GameState& S) const {
        for (const auto& F : S.fleets) if (F.id == *B.Fleet && F.ownerId == S.playerEmpire) return &F;
        return nullptr;
    }
    int SelectedGroup() const { return B.TaskForce ? *B.TaskForce : 0; }

    // Paint caches. Supply coverage and routes are pure functions of the draft, so they are rebuilt only when the
    // draft version, the selected unit or the hovered sector changes, never per frame.
    struct FPaintCache {
        std::uint64_t Version = 0;
        int Fleet = -1, Group = -1;
        std::vector<unsigned char> Supply;                 // range of the selected unit (limiting member for a group)
        std::map<int, std::vector<int>> Courses;           // fleet id -> route to its drafted destination
        int HoverCell = -2;
        std::vector<int> HoverPath;
        // Schema 13 overlays (fog-filtered by the view model): charted anomalies of the committed campaign, stations
        // the player owns or sees, and own transports assembling an outpost in the draft.
        std::vector<botf::view::FeatureMark> Features;
        std::vector<botf::view::StationMark> Stations;
        std::vector<std::pair<int, int>> Assembling;
    };
    mutable FPaintCache Cache;
    void RefreshPaintCache(const botf::GameState& Draft, const botf::Fleet* Ship) const {
        const std::uint64_t Version = B.Version ? B.Version() : 0;
        const int FleetId = Ship ? Ship->id : 0, Group = Ship ? SelectedGroup() : 0;
        if (Version != 0 && Cache.Version == Version && Cache.Fleet == FleetId && Cache.Group == Group) return;
        Cache = FPaintCache();
        Cache.Version = Version; Cache.Fleet = FleetId; Cache.Group = Group;
        if (const botf::Fleet* Limit = Ship ? botf::view::movementFleet(Draft, FleetId, Group) : nullptr)
            Cache.Supply = botf::view::supplyGrid(Draft, *Limit);
        for (const auto& F : Draft.fleets)
            if (F.ownerId == Draft.playerEmpire && (F.targetX != F.x || F.targetY != F.y))
                Cache.Courses[F.id] = botf::route(Draft, F, F.targetX, F.targetY);
        Cache.Features = botf::view::chartFeatures(*B.State);
        Cache.Stations = botf::view::chartStations(*B.State);
        for (const auto& F : Draft.fleets)
            if (F.ownerId == Draft.playerEmpire && F.buildingOutpost) Cache.Assembling.push_back({F.x, F.y});
    }
    bool CachedSupplied(int X, int Y) const {
        if (!B.State || X < 0 || Y < 0 || X >= B.State->width || Y >= B.State->height) return false;
        const std::size_t Index = static_cast<std::size_t>(Y * B.State->width + X);
        return Index < Cache.Supply.size() && Cache.Supply[Index] != 0;
    }
    const std::vector<int>& CachedHoverPath(const botf::GameState& Draft, const botf::Fleet& Ship, int Cell) const {
        if (Cache.HoverCell != Cell) {
            Cache.HoverCell = Cell;
            Cache.HoverPath = botf::view::unitRoute(Draft, Ship.id, SelectedGroup(), Cell % Draft.width, Cell / Draft.width);
        }
        return Cache.HoverPath;
    }

    // Paint must never let an exception unwind through Slate.
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                         FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override {
        try { return PaintChart(Geometry, Elements, Layer); }
        catch (...) { Cache = FPaintCache(); return Layer; }
    }
    int32 PaintChart(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer) const {
        const auto& State = *B.State;
        const auto& Draft = B.Draft ? B.Draft() : State;
        const FPalette& Pal = Palette(State.playerEmpire);
        const FVector2D Size = Geometry.GetLocalSize();
        LastSize = Size;
        const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
        const FSlateBrush* Round = Styles().Get(1, EShape::Pill);
        const double CS = CellSize(Size);
        const int W = State.width;
        auto Box = [&](int L, FVector2D At, FVector2D Extent, const FLinearColor& Color, const FSlateBrush* Brush = nullptr) {
            FSlateDrawElement::MakeBox(Elements, Layer + L, Geometry.ToPaintGeometry(Extent, FSlateLayoutTransform(At)),
                Brush ? Brush : White, ESlateDrawEffect::None, Color);
        };
        auto Line = [&](int L, FVector2D A, FVector2D Z, const FLinearColor& Color, float Thick) {
            TArray<FVector2D> Pts; Pts.Add(A); Pts.Add(Z);
            BotfUi::DrawLines(Elements, Layer + L, Geometry, Pts, Color, Thick);
        };
        auto Outline = [&](int L, FVector2D A, double Ext, const FLinearColor& Color, float Thick) {
            TArray<FVector2D> Pts = { A, A + FVector2D(Ext, 0), A + FVector2D(Ext, Ext), A + FVector2D(0, Ext), A };
            BotfUi::DrawLines(Elements, Layer + L, Geometry, Pts, Color, Thick);
        };
        auto Text = [&](int L, FVector2D At, const FString& Value, int Pt, bool Bold, const FLinearColor& Color) {
            FSlateDrawElement::MakeText(Elements, Layer + L, Geometry.ToPaintGeometry(FVector2D(220, Pt + 8), FSlateLayoutTransform(At)),
                Value, Font(Pt, Bold), ESlateDrawEffect::None, Color);
        };
        auto OnScreen = [&](FVector2D A) { return A.X > -CS && A.Y > -CS && A.X < Size.X + CS && A.Y < Size.Y + CS; };

        const auto F2 = [](FVector2D V) { return FVector2f(static_cast<float>(V.X), static_cast<float>(V.Y)); };
        // Deep-space backdrop: a vertical wash, two soft nebula clouds that drift a little with the pan, then the starfield.
        BotfUi::PaintGradient(Elements, Layer, Geometry, FVector2f(0.f, 0.f), F2(Size), FVector4(0, 0, 0, 0),
            BotfUi::Mix(Pal.Void, Pal.FieldHi, 0.30f), Shade(Pal.Void, 0.8f));
        BotfUi::PaintGlow(Elements, Layer, Geometry, F2(Size * FVector2D(0.28, 0.30) + B.View->Pan * 0.05), static_cast<float>(Size.X * 0.42),
            BotfUi::WithAlpha(Pal.Primary, 0.06f));
        BotfUi::PaintGlow(Elements, Layer, Geometry, F2(Size * FVector2D(0.74, 0.70) + B.View->Pan * 0.05), static_cast<float>(Size.X * 0.36),
            BotfUi::WithAlpha(Pal.Secondary, 0.035f));
        // Fixed decorative starfield with slight parallax; consumes no simulation randomness.
        for (int I = 0; I < 320; ++I) {
            FVector2D P((I * 173 % 997) / 997.0 * Size.X, (I * 317 % 991) / 991.0 * Size.Y);
            P += B.View->Pan * (I % 3 == 0 ? 0.08 : 0.03);
            P.X = std::fmod(std::fmod(P.X, Size.X) + Size.X, Size.X); P.Y = std::fmod(std::fmod(P.Y, Size.Y) + Size.Y, Size.Y);
            const float S = I % 13 == 0 ? 2.5f : (I % 4 == 0 ? 1.6f : 1.f);
            const FLinearColor Tint = I % 7 == 0 ? Pal.Secondary : (I % 5 == 0 ? Pal.Primary : Pal.Text);
            Box(0, P, FVector2D(S, S), Shade(Tint, 1.f, I % 13 == 0 ? 0.55f : (I % 5 == 0 ? 0.30f : 0.14f)), S > 1.5f ? Round : nullptr);
        }
        // The galaxy itself: a lit plate with a hairline edge; unexplored space stays dark, explored space glows faintly.
        {
            const FVector2D O = Offset(Size);
            BotfUi::PaintRounded(Elements, Layer + 1, Geometry, F2(O - FVector2D(6, 6)), F2(FVector2D(CS * W + 12, CS * State.height + 12)),
                FVector4(14, 14, 14, 14), BotfUi::WithAlpha(Pal.Void, 0.55f), BotfUi::WithAlpha(Pal.LineHi, 0.75f), 1.f);
        }
        // Sectors: explored space is lit, sensor coverage brighter, colonised sectors carry their owner's tint.
        for (int Y = 0; Y < State.height; ++Y) for (int X = 0; X < W; ++X) {
            const FVector2D A = Origin(X, Y, Size);
            if (!OnScreen(A) || !botf::explored(State, X, Y)) continue;
            const int Id = Y * W + X;
            FLinearColor Fill = BotfUi::WithAlpha(Pal.Field, 0.85f);
            if (botf::visible(State, State.playerEmpire, X, Y)) Fill = BotfUi::WithAlpha(BotfUi::Mix(Pal.FieldHi, Pal.Primary, 0.10f), 0.92f);
            if (HoverCell == Id) Fill = Lift(Fill, 0.08f);
            Box(1, A, FVector2D(CS, CS), Fill);
        }
        for (const auto& Sys : State.systems) {
            const auto* Col = botf::colonyAt(State, Sys.id);
            if (!Col || !botf::explored(State, Sys.x, Sys.y)) continue;
            if (Col->ownerId != State.playerEmpire && !botf::visible(State, State.playerEmpire, Sys.x, Sys.y)) continue;
            const FLinearColor Owner = FactionColor(Col->ownerId);
            const FVector2D A = Origin(Sys.x, Sys.y, Size);
            BotfUi::PaintGlow(Elements, Layer + 1, Geometry, F2(A + FVector2D(CS, CS) * 0.5), static_cast<float>(CS * 1.5), BotfUi::WithAlpha(Owner, 0.20f));
            Box(1, A, FVector2D(CS, CS), Shade(Owner, 0.6f, 0.16f));
        }
        if (B.View->bGrid || CS >= 34) {
            const FLinearColor Grid = Shade(Pal.Secondary, 0.28f, B.View->bGrid ? 0.55f : 0.16f);
            const FVector2D O = Offset(Size);
            for (int X = 0; X <= W; ++X) Line(2, O + FVector2D(X * CS, 0), O + FVector2D(X * CS, State.height * CS), Grid, 1.f);
            for (int Y = 0; Y <= State.height; ++Y) Line(2, O + FVector2D(0, Y * CS), O + FVector2D(W * CS, Y * CS), Grid, 1.f);
            if (B.View->bGrid && CS >= 18) {
                for (int X = 0; X < W; ++X) Text(2, O + FVector2D(X * CS + 3, -17), FString::FromInt(X), 7, false, Pal.Muted);
                for (int Y = 0; Y < State.height; ++Y) Text(2, O + FVector2D(-20, Y * CS + 2), FString::FromInt(Y), 7, false, Pal.Muted);
            }
        }
        const bool Military = B.View->Mode == EMapMode::Military;
        const botf::Fleet* Ship = OwnShip(Draft);
        RefreshPaintCache(Draft, Ship);
        auto Circle = [&](int L, FVector2D C, double Radius, const FLinearColor& Color, float Thick) {
            TArray<FVector2D> Pts;
            for (int I = 0; I <= 24; ++I) { const double A = I * 6.283185307 / 24; Pts.Add(C + FVector2D(std::cos(A), std::sin(A)) * Radius); }
            BotfUi::DrawLines(Elements, Layer + L, Geometry, Pts, Color, Thick);
        };
        // Charted anomalies (explored sectors only): nebulae and ion storms tint their sectors, point anomalies get a glyph.
        if (B.View->bAnomalies) {
            for (const auto& Mark : Cache.Features) {
                const FVector2D A = Origin(Mark.x, Mark.y, Size), P = Centre(Mark.x, Mark.y, Size);
                if (!OnScreen(A)) continue;
                switch (Mark.type) {
                case botf::GalaxyFeatureType::Nebula:
                    Box(2, A + FVector2D(0.5, 0.5), FVector2D(CS - 1, CS - 1), FLinearColor(0.55f, 0.24f, 0.78f, 0.30f));
                    break;
                case botf::GalaxyFeatureType::IonStorm:
                    Box(2, A + FVector2D(0.5, 0.5), FVector2D(CS - 1, CS - 1), FLinearColor(0.95f, 0.72f, 0.18f, 0.16f));
                    for (int I = 1; I <= 3; ++I)
                        Line(2, A + FVector2D(CS * I / 4.0, 1), A + FVector2D(CS * I / 4.0 - CS * 0.18, CS - 1), FLinearColor(1.f, 0.82f, 0.30f, 0.45f), 1.f);
                    break;
                case botf::GalaxyFeatureType::BlackHole:
                    Box(2, P - FVector2D(CS, CS) * 0.26, FVector2D(CS, CS) * 0.52, FLinearColor(0.f, 0.f, 0.f, 1.f), Round);
                    Circle(3, P, CS * 0.30, FLinearColor(1.f, 0.55f, 0.15f, 0.85f), 1.5f);
                    break;
                case botf::GalaxyFeatureType::NeutronStar:
                    Box(3, P - FVector2D(CS, CS) * 0.09, FVector2D(CS, CS) * 0.18, FLinearColor(0.80f, 0.92f, 1.f, 1.f), Round);
                    Line(3, P - FVector2D(CS * 0.28, 0), P + FVector2D(CS * 0.28, 0), FLinearColor(0.70f, 0.88f, 1.f, 0.7f), 1.f);
                    Line(3, P - FVector2D(0, CS * 0.28), P + FVector2D(0, CS * 0.28), FLinearColor(0.70f, 0.88f, 1.f, 0.7f), 1.f);
                    break;
                case botf::GalaxyFeatureType::Wormhole:
                    Circle(3, P, CS * 0.30, FLinearColor(0.40f, 0.90f, 1.f, 0.95f), 2.f);
                    Circle(3, P, CS * 0.16, FLinearColor(0.40f, 0.90f, 1.f, 0.55f), 1.f);
                    // The link to a charted exit (drawn from both ends when both are charted; harmless overdraw).
                    if (Mark.exitX >= 0) Line(4, P, Centre(Mark.exitX, Mark.exitY, Size), FLinearColor(0.40f, 0.90f, 1.f, 0.35f), 1.5f);
                    break;
                case botf::GalaxyFeatureType::SubspaceRift:
                    Line(3, A + FVector2D(CS * 0.2, CS * 0.15), P + FVector2D(-CS * 0.08, 0), FLinearColor(1.f, 0.30f, 0.35f, 0.95f), 2.f);
                    Line(3, P + FVector2D(-CS * 0.08, 0), P + FVector2D(CS * 0.08, CS * 0.05), FLinearColor(1.f, 0.30f, 0.35f, 0.95f), 2.f);
                    Line(3, P + FVector2D(CS * 0.08, CS * 0.05), A + FVector2D(CS * 0.8, CS * 0.85), FLinearColor(1.f, 0.30f, 0.35f, 0.95f), 2.f);
                    break;
                }
            }
        }
        // Station rings: supply (bright) and sensor (faint) reach as Chebyshev squares, for own stations and for the
        // station in the selected sector.
        if (B.View->bStations) {
            const auto Ring = [&](int X, int Y, int Radius, const FLinearColor& Color, float Thick) {
                const int X0 = FMath::Max(0, X - Radius), Y0 = FMath::Max(0, Y - Radius);
                const int X1 = FMath::Min(W - 1, X + Radius) + 1, Y1 = FMath::Min(State.height - 1, Y + Radius) + 1;
                const FVector2D TL = Origin(X0, Y0, Size), BR = Origin(X1, Y1, Size);
                TArray<FVector2D> Pts = { TL, FVector2D(BR.X, TL.Y), BR, FVector2D(TL.X, BR.Y), TL };
                BotfUi::DrawLines(Elements, Layer + 3, Geometry, Pts, Color, Thick);
            };
            for (const auto& St : Cache.Stations) {
                if (!St.own && St.y * W + St.x != *B.Cell) continue;
                const FLinearColor Owner = FactionColor(St.ownerId);
                Ring(St.x, St.y, St.supplyRadius, Shade(Owner, 1.f, 0.55f), 1.5f);
                Ring(St.x, St.y, St.sensorRadius, Shade(Owner, 0.8f, 0.28f), 1.f);
            }
        }
        // Supply range of the selected unit: a task force is bounded by its shortest-ranged member.
        if (Military && Ship && B.View->bRange) {
            const FLinearColor Edge = Shade(Pal.Good, 0.85f, 0.75f);
            for (int Y = 0; Y < State.height; ++Y) for (int X = 0; X < W; ++X) {
                if (!CachedSupplied(X, Y)) continue;
                const FVector2D A = Origin(X, Y, Size);
                Box(2, A, FVector2D(CS, CS), BotfUi::WithAlpha(Pal.Good, 0.07f));
                if (!CachedSupplied(X - 1, Y)) Line(3, A, A + FVector2D(0, CS), Edge, 1.5f);
                if (!CachedSupplied(X + 1, Y)) Line(3, A + FVector2D(CS, 0), A + FVector2D(CS, CS), Edge, 1.5f);
                if (!CachedSupplied(X, Y - 1)) Line(3, A, A + FVector2D(CS, 0), Edge, 1.5f);
                if (!CachedSupplied(X, Y + 1)) Line(3, A + FVector2D(0, CS), A + FVector2D(CS, CS), Edge, 1.5f);
            }
        }
        auto DrawRoute = [&](const std::vector<int>& Path, const FLinearColor& Color, float Thick, bool Arrow) {
            for (std::size_t I = 1; I < Path.size(); ++I) {
                const FVector2D A = Centre(Path[I - 1] % W, Path[I - 1] / W, Size), Z = Centre(Path[I] % W, Path[I] / W, Size);
                Line(4, A, Z, Color, Thick);
                if (Arrow) {   // step chevron, as the original's arrow trail
                    const FVector2D Dir = (Z - A).GetSafeNormal(), Side(-Dir.Y, Dir.X);
                    const FVector2D Tip = A + (Z - A) * 0.6;
                    Line(4, Tip, Tip - Dir * 6 + Side * 4, Color, Thick);
                    Line(4, Tip, Tip - Dir * 6 - Side * 4, Color, Thick);
                }
            }
        };
        const auto CourseOf = [this](int FleetId) -> const std::vector<int>* {
            const auto It = Cache.Courses.find(FleetId);
            return It == Cache.Courses.end() ? nullptr : &It->second;
        };
        if (Military) {
            // Every own unit's drafted or standing course, faintly; the selected unit's brightly.
            if (B.View->bCourses)
                for (const auto& F : Draft.fleets)
                    if (F.ownerId == State.playerEmpire && (F.targetX != F.x || F.targetY != F.y) && (!Ship || F.id != Ship->id))
                        if (const auto* Course = CourseOf(F.id)) DrawRoute(*Course, Shade(Pal.Primary, 0.8f, 0.55f), 1.5f, false);
            if (Ship) {
                if (Ship->targetX != Ship->x || Ship->targetY != Ship->y)
                    if (const auto* Course = CourseOf(Ship->id)) DrawRoute(*Course, Pal.Accent, 2.5f, true);
                if (HoverCell >= 0 && HoverCell != Ship->y * W + Ship->x) {
                    const auto& Path = CachedHoverPath(Draft, *Ship, HoverCell);
                    DrawRoute(Path, B.View->bPlotting ? Pal.Good : Shade(Pal.Good, 0.8f, 0.7f), 2.f, true);
                    if (Path.empty())
                        Line(4, Centre(Ship->x, Ship->y, Size), Centre(HoverCell % W, HoverCell / W, Size), Shade(Pal.Alert, 1.f, 0.7f), 1.5f);
                }
            }
        } else {   // Economic mode: trade routes; ships hidden (Tutorial p13).
            for (const auto& T : State.tradeRoutes) {
                const botf::System* A = nullptr; const botf::System* Z = nullptr;
                for (const auto& S : State.systems) { if (S.id == T.fromSystemId) A = &S; if (S.id == T.toSystemId) Z = &S; }
                if (!A || !Z || !TradeRouteKnown(State, T)) continue;
                Line(4, Centre(A->x, A->y, Size), Centre(Z->x, Z->y, Size), T.ownerId == State.playerEmpire ? Pal.Good : Shade(FactionColor(T.ownerId), 0.8f), 2.f);
            }
        }
        // Selected sector: soft wash and four corner brackets rather than a hard box. Hover: a hairline.
        {
            const FVector2D A = Origin(*B.Cell % W, *B.Cell / W, Size);
            Box(5, A, FVector2D(CS, CS), BotfUi::WithAlpha(Pal.Accent, 0.09f));
            const double Arm = FMath::Clamp(CS * 0.28, 7.0, 22.0);
            const FLinearColor Br = Pal.Accent;
            const auto Corner = [&](FVector2D At, double Sx, double Sy) {
                TArray<FVector2D> Pts = { At + FVector2D(Sx * Arm, 0), At, At + FVector2D(0, Sy * Arm) };
                BotfUi::DrawLines(Elements, Layer + 5, Geometry, Pts, Br, 2.5f);
            };
            Corner(A, 1, 1); Corner(A + FVector2D(CS, 0), -1, 1); Corner(A + FVector2D(0, CS), 1, -1); Corner(A + FVector2D(CS, CS), -1, -1);
        }
        if (HoverCell >= 0 && HoverCell != *B.Cell)
            Outline(5, Origin(HoverCell % W, HoverCell / W, Size), CS, B.View->bPlotting ? Pal.Good : Shade(Pal.Text, 0.6f, 0.35f), 1.f);

        const float Star = FMath::Clamp(static_cast<float>(CS) * 0.2f, 4.f, 14.f);
        for (const auto& S : State.systems) {
            if (!botf::explored(State, S.x, S.y)) continue;
            const FVector2D P = Centre(S.x, S.y, Size);
            if (!OnScreen(P)) continue;
            const auto Colony = botf::colonyAt(State, S.id);
            const bool Own = Colony && Colony->ownerId == State.playerEmpire;
            const bool Seen = Colony && (Own || botf::visible(State, State.playerEmpire, S.x, S.y));
            const auto Minor = botf::minorAt(State, S.id);
            const FLinearColor Color = Seen ? FactionColor(Colony->ownerId) : Minor && Minor->contact[State.playerEmpire]
                ? FLinearColor(0.85f, 0.85f, 0.95f) : FLinearColor(0.95f, 0.88f, 0.72f);
            // Corona, halo and a hot core; a four-point flare on the larger stars.
            BotfUi::PaintGlow(Elements, Layer + 6, Geometry, F2(P), Star * 3.4f, BotfUi::WithAlpha(Color, 0.30f));
            BotfUi::PaintGlow(Elements, Layer + 6, Geometry, F2(P), Star * 1.7f, BotfUi::WithAlpha(Color, 0.55f));
            if (*B.Selected == S.id) {
                TArray<FVector2D> Ring;
                for (int I = 0; I <= 40; ++I) { const double A = I * 6.283185307 / 40; Ring.Add(P + FVector2D(std::cos(A), std::sin(A)) * (Star + 6)); }
                BotfUi::DrawLines(Elements, Layer + 6, Geometry, Ring, Pal.Accent, 2.f);
            }
            BotfUi::PaintDisc(Elements, Layer + 7, Geometry, F2(P), Star * 0.62f, Lift(Color, 0.15f), 24);
            BotfUi::PaintDisc(Elements, Layer + 7, Geometry, F2(P), Star * 0.30f, FLinearColor(1.f, 1.f, 1.f, 0.95f), 16);
            if (Star >= 8.f) {
                Line(7, P - FVector2D(Star * 1.5, 0), P + FVector2D(Star * 1.5, 0), BotfUi::WithAlpha(Lift(Color, 0.4f), 0.35f), 1.f);
                Line(7, P - FVector2D(0, Star * 1.5), P + FVector2D(0, Star * 1.5), BotfUi::WithAlpha(Lift(Color, 0.4f), 0.35f), 1.f);
            }
            // A contacted independent minor race (the roster's homeworlds) wears a thin ring; hover for its profile.
            if (!Seen && Minor && !Minor->memberOf && Minor->contact[State.playerEmpire])
                Circle(7, P, Star + 3, FLinearColor(0.80f, 0.76f, 1.f, 0.85f), 1.f);
            if (B.View->bNames && CS >= 22) {
                const FVector2D At = P + FVector2D(-CS * 0.45, Star * 0.5 + 3);
                const FString Name = MapStr(S.name);
                const int Pt = CS >= 48 ? 9 : 8;
                Text(8, At + FVector2D(1, 1), Name, Pt, Own, FLinearColor(0.f, 0.f, 0.f, 0.85f));
                Text(8, At, Name, Pt, Own, Seen ? Lift(Color, 0.25f) : Pal.Text2);
                if (!Military && Own && CS >= 40) {
                    const auto E = botf::economy(State, *Colony);
                    Text(8, At + FVector2D(0, 13), FString::Printf(TEXT("pop %d  ind %d"), Colony->population, E.industry), 7, false, Pal.Muted);
                }
            }
        }
        // Stations sit in the top-right corner of their sector: an outpost is a small diamond, a starbase a larger one
        // with a second frame; an upgrade in progress is framed in the accent colour. Transports assembling an
        // outpost show an empty frame there.
        {
            const double Mark = FMath::Clamp(CS * 0.2, 6.0, 12.0);
            const auto Diamond = [&](FVector2D C, double R, const FLinearColor& Fill, const FLinearColor* Rim) {
                TArray<FVector2f> Pts = { F2(C + FVector2D(0, -R)), F2(C + FVector2D(R, 0)), F2(C + FVector2D(0, R)), F2(C + FVector2D(-R, 0)) };
                BotfUi::PaintFan(Elements, Layer + 8, Geometry, F2(C), Pts, Fill, Rim);
            };
            for (const auto& St : Cache.Stations) {
                const bool Starbase = St.type == botf::StationType::Starbase;
                const double R = (Starbase ? Mark * 0.85 : Mark * 0.6);
                const FVector2D C = Origin(St.x, St.y, Size) + FVector2D(CS - R - 5, R + 5);
                if (!OnScreen(C)) continue;
                const FLinearColor Owner = FactionColor(St.ownerId);
                const FLinearColor Dark = Shade(Owner, 0.45f);
                Diamond(C, R, Lift(Owner, 0.1f), &Dark);
                TArray<FVector2D> Frame = { C + FVector2D(0, -R - 3), C + FVector2D(R + 3, 0), C + FVector2D(0, R + 3), C + FVector2D(-R - 3, 0), C + FVector2D(0, -R - 3) };
                BotfUi::DrawLines(Elements, Layer + 8, Geometry, Frame, St.upgrading ? Pal.Accent : BotfUi::WithAlpha(Owner, 0.7f), 1.f);
            }
            for (const auto& Site : Cache.Assembling) {
                const FVector2D C = Origin(Site.first, Site.second, Size) + FVector2D(CS - Mark * 0.6 - 5, Mark * 0.6 + 5);
                if (!OnScreen(C)) continue;
                const double R = Mark * 0.6 + 3;
                TArray<FVector2D> Frame = { C + FVector2D(0, -R), C + FVector2D(R, 0), C + FVector2D(0, R), C + FVector2D(-R, 0), C + FVector2D(0, -R) };
                BotfUi::DrawLines(Elements, Layer + 8, Geometry, Frame, Shade(FactionColor(State.playerEmpire), 1.f, 0.8f), 1.f);
            }
        }
        if (Military) {
            // One marker per owner per sector with a unit count; own units top-left, foreign bottom-left.
            // A ship is a small chevron: own ships point up, foreign ships point down.
            std::set<int> Sectors;
            const double Mark = FMath::Clamp(CS * 0.2, 6.0, 12.0);
            const auto Chevron = [&](FVector2D C, double R, bool Up, const FLinearColor& Fill, const FLinearColor& Rim) {
                const float D = Up ? 1.f : -1.f;
                TArray<FVector2f> Pts = { F2(C + FVector2D(0, -R * D)), F2(C + FVector2D(R * 0.85, R * 0.95 * D)), F2(C + FVector2D(0, R * 0.42 * D)), F2(C + FVector2D(-R * 0.85, R * 0.95 * D)) };
                BotfUi::PaintFan(Elements, Layer + 9, Geometry, F2(C), Pts, Fill, &Rim);
            };
            const auto Count = [&](FVector2D At, int N, const FLinearColor& Tone) {
                const FString Label = FString::FromInt(N);
                const double W2 = 8.0 + 6.0 * Label.Len();
                BotfUi::PaintRounded(Elements, Layer + 9, Geometry, F2(At), FVector2f(static_cast<float>(W2), 15.f), FVector4(7, 7, 7, 7),
                    BotfUi::WithAlpha(Pal.Void, 0.9f), BotfUi::WithAlpha(Tone, 0.8f), 1.f);
                Text(10, At + FVector2D(4, 0), Label, 8, true, Pal.Text);
            };
            for (const auto& F : Draft.fleets) {
                const bool Mine = F.ownerId == State.playerEmpire;
                if (!Mine && !botf::visible(State, State.playerEmpire, F.x, F.y)) continue;
                if (!Sectors.insert(F.y * W + F.x).second) continue;
                int Own = 0, Foreign = 0, ForeignOwner = 0;
                for (const auto& O : Draft.fleets) if (O.x == F.x && O.y == F.y) {
                    if (O.ownerId == State.playerEmpire) ++Own;
                    else if (botf::visible(State, State.playerEmpire, O.x, O.y)) { ++Foreign; ForeignOwner = O.ownerId; }
                }
                const FVector2D A = Origin(F.x, F.y, Size);
                if (!OnScreen(A)) continue;
                const double R = Mark * 0.95;
                if (Own) {
                    const bool Sel = Ship && Ship->x == F.x && Ship->y == F.y;
                    const FVector2D C = A + FVector2D(R + 4, R + 4);
                    const FLinearColor Body = Sel ? Pal.Accent : FactionColor(State.playerEmpire);
                    if (Sel) BotfUi::PaintGlow(Elements, Layer + 9, Geometry, F2(C), static_cast<float>(R * 2.6), BotfUi::WithAlpha(Pal.Accent, 0.5f));
                    Chevron(C, R, true, Lift(Body, Sel ? 0.1f : 0.05f), Shade(Body, 0.85f));
                    if (Own > 1) Count(A + FVector2D(R * 2 + 6, 3), Own, Body);
                }
                if (Foreign) {
                    const FVector2D C = A + FVector2D(R + 4, CS - R - 4);
                    const FLinearColor Body = FactionColor(ForeignOwner);
                    Chevron(C, R, false, Lift(Body, 0.05f), Shade(Body, 0.8f));
                    if (Foreign > 1) Count(A + FVector2D(R * 2 + 6, CS - 18), Foreign, Body);
                }
            }
        }
        // Cursor hint: what a right click would do here, travel time, sector identity.
        if (HoverCell >= 0 && B.Hint) {
            const FString Hint = B.Hint(HoverCell);
            if (!Hint.IsEmpty()) {
                // The first line is the order hint; further lines describe the sector (anomalies, station, race).
                TArray<FString> Lines;
                Hint.ParseIntoArray(Lines, TEXT("\n"), true);
                int Longest = 0;
                for (const FString& Entry : Lines) Longest = FMath::Max(Longest, Entry.Len());
                const FVector2D At = HoverLocal + FVector2D(16, 14);
                const FVector2D Ext(FMath::Min(560.0, 7.0 * Longest + 22), 8.0 + 15.0 * Lines.Num());
                const FVector2D Place(FMath::Max(4.0, FMath::Min(At.X, Size.X - Ext.X - 4)), FMath::Max(4.0, FMath::Min(At.Y, Size.Y - Ext.Y - 4)));
                BotfUi::PaintRounded(Elements, Layer + 10, Geometry, F2(Place), F2(Ext), FVector4(10, 10, 10, 10),
                    BotfUi::WithAlpha(Pal.Void, 0.94f), BotfUi::WithAlpha(Pal.LineHi, 1.f), 1.f);
                for (int I = 0; I < Lines.Num(); ++I)
                    FSlateDrawElement::MakeText(Elements, Layer + 11,
                        Geometry.ToPaintGeometry(FVector2D(Ext.X - 12, 17), FSlateLayoutTransform(Place + FVector2D(8, 3 + 15.0 * I))),
                        Lines[I], Font(I == 0 ? 9 : 8, false), ESlateDrawEffect::None,
                        I == 0 ? (B.View->bPlotting ? Pal.Good : Pal.Text) : Pal.Muted);
            }
        }
        return Layer + 11;
    }

    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override {
        const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
        const int Id = CellAtLocal(Local, Geometry.GetLocalSize());
        if (Event.GetEffectingButton() == EKeys::RightMouseButton) {
            if (B.View->bPlotting) { B.View->bPlotting = false; return FReply::Handled(); }   // right click cancels plotting
            if (Id >= 0 && B.Command) B.Command(Id, Local, Event.IsShiftDown());
            return FReply::Handled();
        }
        if (Event.GetEffectingButton() == EKeys::LeftMouseButton || Event.GetEffectingButton() == EKeys::MiddleMouseButton) {
            bPressed = true; bDragged = false; PressLocal = LastLocal = Local;
            bMiddle = Event.GetEffectingButton() == EKeys::MiddleMouseButton;
            return FReply::Handled().CaptureMouse(SharedThis(this));
        }
        return FReply::Unhandled();
    }
    virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override {
        if (!bPressed) return FReply::Unhandled();
        bPressed = false;
        FReply Reply = FReply::Handled().ReleaseMouseCapture();
        if (!bDragged && !bMiddle) {
            const int Id = CellAtLocal(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()), Geometry.GetLocalSize());
            if (Id >= 0) { if (B.View->bPlotting || Event.IsShiftDown()) B.Plot(Id); else B.Select(Id); }
        }
        return Reply;
    }
    virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override {
        if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
        const int Id = CellAtLocal(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()), Geometry.GetLocalSize());
        bPressed = false;
        if (Id >= 0 && B.Activate) B.Activate(Id);
        return FReply::Handled();
    }
    virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override {
        const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
        HoverLocal = Local;
        // The button was released where we did not hear it (another window, alt-tab): end the press, never keep panning.
        if (bPressed && !Event.IsMouseButtonDown(bMiddle ? EKeys::MiddleMouseButton : EKeys::LeftMouseButton)) {
            EndPress();
            return FReply::Handled().ReleaseMouseCapture();
        }
        if (bPressed && HasMouseCapture()) {
            if (!bDragged && FVector2D::Distance(Local, PressLocal) > 5) bDragged = true;
            if (bDragged) { B.View->Pan += Local - LastLocal; LastLocal = Local; }
        }
        const int NewHover = CellAtLocal(Local, Geometry.GetLocalSize());
        HoverCell = NewHover;
        Invalidate(EInvalidateWidgetReason::Paint);
        return bPressed ? FReply::Handled() : FReply::Unhandled();
    }
    virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override {
        LastSize = Geometry.GetLocalSize();
        ZoomBy(Event.GetWheelDelta() > 0 ? 1.15f : 1.f / 1.15f, Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
        return FReply::Handled();
    }
    virtual void OnMouseLeave(const FPointerEvent& Event) override {
        SLeafWidget::OnMouseLeave(Event);
        HoverCell = -1;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    // Capture taken away (focus change, alt-tab, a popup): drop the press so a later move cannot resume a stale drag
    // and a later button-up cannot turn into a click.
    virtual void OnMouseCaptureLost(const FCaptureLostEvent& Event) override {
        SLeafWidget::OnMouseCaptureLost(Event);
        EndPress();
    }
    void EndPress() { bPressed = false; bDragged = false; bMiddle = false; Invalidate(EInvalidateWidgetReason::Paint); }
    bool IsPressed() const { return bPressed; }
    virtual FCursorReply OnCursorQuery(const FGeometry&, const FPointerEvent&) const override {
        if (bPressed && bDragged) return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
        return FCursorReply::Cursor(B.View->bPlotting ? EMouseCursor::Crosshairs : EMouseCursor::Default);
    }
private:
    FBinding B;
    int HoverCell = -1;
    FVector2D HoverLocal = FVector2D::ZeroVector, PressLocal = FVector2D::ZeroVector, LastLocal = FVector2D::ZeroVector;
    bool bPressed = false, bDragged = false, bMiddle = false;
    mutable FVector2D LastSize = FVector2D::ZeroVector;
};

} // namespace BotfUi
