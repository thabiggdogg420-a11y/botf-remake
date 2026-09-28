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
            FSlateDrawElement::MakeLines(Elements, Layer + L, Geometry.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Color, true, Thick);
        };
        auto Outline = [&](int L, FVector2D A, double Ext, const FLinearColor& Color, float Thick) {
            TArray<FVector2D> Pts = { A, A + FVector2D(Ext, 0), A + FVector2D(Ext, Ext), A + FVector2D(0, Ext), A };
            FSlateDrawElement::MakeLines(Elements, Layer + L, Geometry.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Color, true, Thick);
        };
        auto Text = [&](int L, FVector2D At, const FString& Value, int Pt, bool Bold, const FLinearColor& Color) {
            FSlateDrawElement::MakeText(Elements, Layer + L, Geometry.ToPaintGeometry(FVector2D(220, Pt + 8), FSlateLayoutTransform(At)),
                Value, Font(Pt, Bold), ESlateDrawEffect::None, Color);
        };
        auto OnScreen = [&](FVector2D A) { return A.X > -CS && A.Y > -CS && A.X < Size.X + CS && A.Y < Size.Y + CS; };

        Box(0, FVector2D::ZeroVector, Size, Pal.Void);
        // Fixed decorative starfield with slight parallax; consumes no simulation randomness.
        for (int I = 0; I < 260; ++I) {
            FVector2D P((I * 173 % 997) / 997.0 * Size.X, (I * 317 % 991) / 991.0 * Size.Y);
            P += B.View->Pan * (I % 3 == 0 ? 0.08 : 0.03);
            P.X = std::fmod(std::fmod(P.X, Size.X) + Size.X, Size.X); P.Y = std::fmod(std::fmod(P.Y, Size.Y) + Size.Y, Size.Y);
            const float S = I % 11 == 0 ? 2.f : 1.f;
            Box(0, P, FVector2D(S, S), Shade(Pal.Text, I % 5 == 0 ? 0.30f : 0.12f));
        }
        // Sectors: explored space is lit, sensor coverage brighter, colonised sectors carry their owner's tint.
        for (int Y = 0; Y < State.height; ++Y) for (int X = 0; X < W; ++X) {
            const FVector2D A = Origin(X, Y, Size);
            if (!OnScreen(A) || !botf::explored(State, X, Y)) continue;
            const int Id = Y * W + X;
            FLinearColor Fill = Shade(Pal.Field, 1.f, 0.72f);
            if (botf::visible(State, State.playerEmpire, X, Y)) Fill = Shade(Pal.FieldHi, 0.75f, 0.78f);
            if (HoverCell == Id) Fill = Lift(Fill, 0.07f);
            Box(1, A + FVector2D(0.5, 0.5), FVector2D(CS - 1, CS - 1), Fill);
        }
        for (const auto& Sys : State.systems) {
            const auto* Col = botf::colonyAt(State, Sys.id);
            if (!Col || !botf::explored(State, Sys.x, Sys.y)) continue;
            if (Col->ownerId != State.playerEmpire && !botf::visible(State, State.playerEmpire, Sys.x, Sys.y)) continue;
            Box(1, Origin(Sys.x, Sys.y, Size) + FVector2D(0.5, 0.5), FVector2D(CS - 1, CS - 1), Shade(FactionColor(Col->ownerId), 0.6f, 0.22f));
        }
        if (B.View->bGrid || CS >= 34) {
            const FLinearColor Grid = Shade(Pal.Secondary, 0.22f, B.View->bGrid ? 1.f : 0.45f);
            const FVector2D O = Offset(Size);
            for (int X = 0; X <= W; ++X) Line(2, O + FVector2D(X * CS, 0), O + FVector2D(X * CS, State.height * CS), Grid, 1.f);
            for (int Y = 0; Y <= State.height; ++Y) Line(2, O + FVector2D(0, Y * CS), O + FVector2D(W * CS, Y * CS), Grid, 1.f);
            if (B.View->bGrid && CS >= 18) {
                for (int X = 0; X < W; ++X) Text(2, O + FVector2D(X * CS + 3, -15), FString::FromInt(X), 7, false, Pal.Muted);
                for (int Y = 0; Y < State.height; ++Y) Text(2, O + FVector2D(-16, Y * CS + 2), FString::FromInt(Y), 7, false, Pal.Muted);
            }
        }
        const bool Military = B.View->Mode == EMapMode::Military;
        const botf::Fleet* Ship = OwnShip(Draft);
        RefreshPaintCache(Draft, Ship);
        // Supply range of the selected unit: a task force is bounded by its shortest-ranged member.
        if (Military && Ship && B.View->bRange) {
            const FLinearColor Edge = Shade(Pal.Good, 0.85f, 0.9f);
            for (int Y = 0; Y < State.height; ++Y) for (int X = 0; X < W; ++X) {
                if (!CachedSupplied(X, Y)) continue;
                const FVector2D A = Origin(X, Y, Size);
                if (!CachedSupplied(X - 1, Y)) Line(3, A, A + FVector2D(0, CS), Edge, 2.f);
                if (!CachedSupplied(X + 1, Y)) Line(3, A + FVector2D(CS, 0), A + FVector2D(CS, CS), Edge, 2.f);
                if (!CachedSupplied(X, Y - 1)) Line(3, A, A + FVector2D(CS, 0), Edge, 2.f);
                if (!CachedSupplied(X, Y + 1)) Line(3, A + FVector2D(0, CS), A + FVector2D(CS, CS), Edge, 2.f);
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
        Outline(5, Origin(*B.Cell % W, *B.Cell / W, Size), CS, Pal.Accent, 2.f);
        if (HoverCell >= 0 && HoverCell != *B.Cell)
            Outline(5, Origin(HoverCell % W, HoverCell / W, Size), CS, B.View->bPlotting ? Pal.Good : Shade(Pal.Text, 0.6f, 0.6f), 1.f);

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
            Box(6, P - FVector2D(Star, Star), FVector2D(Star, Star) * 2, Shade(Color, 1.f, 0.16f), Round);   // glow
            if (*B.Selected == S.id) {
                TArray<FVector2D> Ring;
                for (int I = 0; I <= 32; ++I) { const double A = I * 6.283185307 / 32; Ring.Add(P + FVector2D(std::cos(A), std::sin(A)) * (Star + 5)); }
                FSlateDrawElement::MakeLines(Elements, Layer + 6, Geometry.ToPaintGeometry(), Ring, ESlateDrawEffect::None, Pal.Accent, true, 2.f);
            }
            Box(7, P - FVector2D(Star, Star) * 0.5, FVector2D(Star, Star), Color, Round);
            if (B.View->bNames && CS >= 22) {
                const FVector2D At = P + FVector2D(-CS * 0.45, Star * 0.5 + 2);
                Text(8, At, MapStr(S.name), CS >= 48 ? 9 : 8, Own, Seen ? Color : Pal.Text);
                if (!Military && Own && CS >= 40) {
                    const auto E = botf::economy(State, *Colony);
                    Text(8, At + FVector2D(0, 12), FString::Printf(TEXT("pop %d  ind %d"), Colony->population, E.industry), 7, false, Pal.Muted);
                }
            }
        }
        if (Military) {
            // One marker per owner per sector with a unit count; own units top-left, foreign bottom-left.
            std::set<int> Sectors;
            const double Mark = FMath::Clamp(CS * 0.2, 6.0, 12.0);
            for (const auto& F : Draft.fleets) {
                const bool Mine = F.ownerId == State.playerEmpire;
                if (!Mine && !botf::visible(State, State.playerEmpire, F.x, F.y)) continue;
                if (!Sectors.insert(F.y * W + F.x).second) continue;
                int Own = 0, Foreign = 0, ForeignOwner = 0;
                for (const auto& O : Draft.fleets) if (O.x == F.x && O.y == F.y) {
                    if (O.ownerId == State.playerEmpire) ++Own;
                    else if (botf::visible(State, State.playerEmpire, O.x, O.y)) { ++Foreign; ForeignOwner = O.ownerId; }
                }
                const FVector2D A = Origin(F.x, F.y, Size) + FVector2D(3, 3);
                if (!OnScreen(A)) continue;
                if (Own) {
                    const bool Sel = Ship && Ship->x == F.x && Ship->y == F.y;
                    if (Sel) Outline(9, A - FVector2D(2, 2), Mark + 4, Pal.Accent, 1.5f);
                    Box(9, A, FVector2D(Mark, Mark), Sel ? Pal.Accent : FactionColor(State.playerEmpire));
                    if (Own > 1) Text(9, A + FVector2D(Mark + 2, -2), FString::FromInt(Own), 8, true, Pal.Text);
                }
                if (Foreign) {
                    const FVector2D Z = Origin(F.x, F.y, Size) + FVector2D(3, CS - Mark - 3);
                    Box(9, Z, FVector2D(Mark, Mark), FactionColor(ForeignOwner));
                    if (Foreign > 1) Text(9, Z + FVector2D(Mark + 2, -2), FString::FromInt(Foreign), 8, true, FactionColor(ForeignOwner));
                }
            }
        }
        // Cursor hint: what a right click would do here, travel time, sector identity.
        if (HoverCell >= 0 && B.Hint) {
            const FString Hint = B.Hint(HoverCell);
            if (!Hint.IsEmpty()) {
                const FVector2D At = HoverLocal + FVector2D(16, 14);
                const FVector2D Ext(FMath::Min(360.0, 7.0 * Hint.Len() + 16), 22);
                const FVector2D Place(FMath::Min(At.X, Size.X - Ext.X - 4), FMath::Min(At.Y, Size.Y - Ext.Y - 4));
                Box(10, Place, Ext, Shade(Pal.Void, 1.f, 0.88f), Styles().Get(State.playerEmpire, EShape::Row));
                Text(11, Place + FVector2D(8, 3), Hint, 9, false, B.View->bPlotting ? Pal.Good : Pal.Text);
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
