#include "OutpostArenaRenderer.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "GlobalRenderResources.h"
#include "OutpostGameMode.h"
#include "OutpostSimulation.h"

#include <initializer_list>

namespace
{
FLinearColor Srgb(uint8 R, uint8 G, uint8 B, uint8 A = 255)
{
    return FLinearColor::FromSRGBColor(FColor(R, G, B, A));
}

const FLinearColor Void = Srgb(3, 8, 15);
const FLinearColor FloorA = Srgb(9, 24, 35);
const FLinearColor Grid = Srgb(26, 57, 70, 60);
const FLinearColor Frame = Srgb(39, 86, 99);
const FLinearColor Cyan = Srgb(30, 232, 207);
const FLinearColor Amber = Srgb(255, 166, 46);
const FLinearColor Red = Srgb(255, 60, 76);
const FLinearColor Purple = Srgb(166, 103, 221);
const FLinearColor Ink = Srgb(210, 232, 232);

class FArenaDraw
{
public:
    FArenaDraw(UCanvas* InCanvas, const AOutpostGameMode& InGameMode, float InUiScale)
        : Canvas(InCanvas), GameMode(InGameMode), Sim(InGameMode.Sim), UiScale(InUiScale)
    {
        ProjectedOrigin = GameMode.ScreenPoint(FVector2D::ZeroVector);
        ProjectedExtent = GameMode.ScreenPoint(FVector2D(Outpost::Width, Outpost::Height)) - ProjectedOrigin;
        WhiteTexture = GWhiteTexture;
    }

    void Draw()
    {
        DrawFloor();
        DrawSignalField();
        DrawDoors();
        for (const Outpost::FMine& Mine : Sim.Mines) DrawMine(Mine);
        DrawForge();
        for (const Outpost::FRect& Wall : Sim.Walls) DrawWall(Wall, 1.f, false, true);
        if (Sim.bCarrying)
        {
            const Outpost::FPlacement Placement = Sim.Placement(GameMode.MouseWorld());
            if (Placement.Rect.W > 0 && Placement.Rect.H > 0)
            {
                DrawWall(Placement.Rect, .64f, true, Placement.bValid);
            }
        }
        for (const Outpost::FEnemy& Enemy : Sim.Enemies) DrawTelegraph(Enemy);
        for (const Outpost::FEnemy& Enemy : Sim.Enemies) DrawEnemy(Enemy);
        for (const Outpost::FBullet& Bullet : Sim.Bullets) DrawBullet(Bullet);
        DrawPlayer();
    }

private:
    UCanvas* Canvas;
    const AOutpostGameMode& GameMode;
    const Outpost::FSimulation& Sim;
    float UiScale;
    FVector2D ProjectedOrigin = FVector2D::ZeroVector;
    FVector2D ProjectedExtent = FVector2D(1.f, 1.f);
    const FTexture* WhiteTexture = nullptr;

    FVector2D P(const FVector2D& World) const
    {
        return ProjectedOrigin + FVector2D(World.X * ProjectedExtent.X / Outpost::Width,
            World.Y * ProjectedExtent.Y / Outpost::Height);
    }

    void Tile(const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color) const
    {
        FCanvasTileItem Item(Position, Size, Color);
        Item.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(Item);
    }

    void Line(const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness = 1.f) const
    {
        const FVector2D Delta = B - A;
        const float Length = Delta.Size();
        if (Length <= KINDA_SMALL_NUMBER) return;
        const float HalfWidth = FMath::Max(.3f, Thickness * UiScale * .5f);
        const FVector2D Normal(-Delta.Y / Length * HalfWidth, Delta.X / Length * HalfWidth);
        Triangle(A - Normal, B - Normal, B + Normal, Color);
        Triangle(A - Normal, B + Normal, A + Normal, Color);
    }

    void Triangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color) const
    {
        check(WhiteTexture);
        FCanvasTriangleItem Item(A, B, C, WhiteTexture);
        Item.SetColor(Color);
        Item.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(Item);
    }

    void Polygon(const TArray<FVector2D>& Points, const FLinearColor& Fill, const FLinearColor& Stroke = FLinearColor::Transparent) const
    {
        if (Points.Num() < 3) return;
        for (int32 I = 1; I + 1 < Points.Num(); ++I) Triangle(Points[0], Points[I], Points[I + 1], Fill);
        if (Stroke.A > 0.f)
        {
            for (int32 I = 0; I < Points.Num(); ++I) Line(Points[I], Points[(I + 1) % Points.Num()], Stroke, 1.f);
        }
    }

    void WorldPolygon(FVector2D Center, float Angle, std::initializer_list<FVector2D> Local,
        const FLinearColor& Fill, const FLinearColor& Stroke = FLinearColor::Transparent, float Scale = 1.f) const
    {
        const float C = FMath::Cos(Angle), S = FMath::Sin(Angle);
        TArray<FVector2D> Points;
        Points.Reserve(static_cast<int32>(Local.size()));
        for (const FVector2D& V : Local)
        {
            const FVector2D R((V.X * C - V.Y * S) * Scale, (V.X * S + V.Y * C) * Scale);
            Points.Add(P(Center + R));
        }
        Polygon(Points, Fill, Stroke);
    }

    void WorldLine(FVector2D Center, float Angle, FVector2D A, FVector2D B,
        const FLinearColor& Color, float Thickness = 1.f, float Scale = 1.f) const
    {
        const float C = FMath::Cos(Angle), S = FMath::Sin(Angle);
        const auto Transform = [=](FVector2D V)
        {
            return Center + FVector2D((V.X * C - V.Y * S) * Scale, (V.X * S + V.Y * C) * Scale);
        };
        Line(P(Transform(A)), P(Transform(B)), Color, Thickness);
    }

    void Circle(FVector2D WorldCenter, float WorldRadius, const FLinearColor& Color,
        float Thickness = 1.f, int32 Segments = 40, float Start = 0.f, float End = 2.f * PI) const
    {
        FVector2D Previous = P(WorldCenter + FVector2D(FMath::Cos(Start), FMath::Sin(Start)) * WorldRadius);
        for (int32 I = 1; I <= Segments; ++I)
        {
            const float T = static_cast<float>(I) / Segments;
            const float Angle = FMath::Lerp(Start, End, T);
            const FVector2D Next = P(WorldCenter + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * WorldRadius);
            Line(Previous, Next, Color, Thickness);
            Previous = Next;
        }
    }

    void WorldRect(const Outpost::FRect& Rect, const FLinearColor& Color, float InsetWorld = 0.f) const
    {
        const FVector2D A = P(FVector2D(Rect.X * Outpost::Tile + InsetWorld, Rect.Y * Outpost::Tile + InsetWorld));
        const FVector2D B = P(FVector2D((Rect.X + Rect.W) * Outpost::Tile - InsetWorld,
            (Rect.Y + Rect.H) * Outpost::Tile - InsetWorld));
        const FVector2D Min(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
        Tile(Min, FVector2D(FMath::Abs(B.X - A.X), FMath::Abs(B.Y - A.Y)), Color);
    }

    void DrawFloor()
    {
        const FVector2D A = P(FVector2D::ZeroVector);
        const FVector2D B = P(FVector2D(Outpost::Width, Outpost::Height));
        const FVector2D Min(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
        const FVector2D Size(FMath::Abs(B.X - A.X), FMath::Abs(B.Y - A.Y));
        Tile(Min - FVector2D(14.f * UiScale), Size + FVector2D(28.f * UiScale), Void);
        Tile(Min, Size, FloorA);

        // Keep the field quiet enough that units and telegraphs read at a glance.
        for (int32 Y = 1; Y < Outpost::Rows; ++Y)
        {
            const FVector2D A0 = P(FVector2D(Outpost::Tile, Y * Outpost::Tile));
            const FVector2D B0 = P(FVector2D(Outpost::Width - Outpost::Tile, Y * Outpost::Tile));
            Line(A0, B0, Grid, Y % 4 == 0 ? 1.f : .55f);
        }
        for (int32 X = 1; X < Outpost::Cols; ++X)
        {
            const FVector2D A0 = P(FVector2D(X * Outpost::Tile, Outpost::Tile));
            const FVector2D B0 = P(FVector2D(X * Outpost::Tile, Outpost::Height - Outpost::Tile));
            Line(A0, B0, Grid, X % 4 == 0 ? 1.f : .55f);
        }

        for (int32 Y = 3; Y < Outpost::Rows - 2; Y += 4)
        {
            for (int32 X = 4; X < Outpost::Cols - 2; X += 6)
            {
                const FVector2D C = P(FVector2D(X * Outpost::Tile + 9.f, Y * Outpost::Tile + 9.f));
                Tile(C - FVector2D(1.f * UiScale), FVector2D(2.f * UiScale),
                    FLinearColor(Frame.R, Frame.G, Frame.B, .42f));
            }
        }

        const FVector2D InnerA = P(FVector2D(Outpost::Tile, Outpost::Tile));
        const FVector2D InnerB = P(FVector2D(Outpost::Width - Outpost::Tile, Outpost::Height - Outpost::Tile));
        const FVector2D TL(FMath::Min(InnerA.X, InnerB.X), FMath::Min(InnerA.Y, InnerB.Y));
        const FVector2D BR(FMath::Max(InnerA.X, InnerB.X), FMath::Max(InnerA.Y, InnerB.Y));
        Line(TL, FVector2D(BR.X, TL.Y), Frame, 2.f);
        Line(FVector2D(BR.X, TL.Y), BR, Frame, 2.f);
        Line(BR, FVector2D(TL.X, BR.Y), Frame, 2.f);
        Line(FVector2D(TL.X, BR.Y), TL, Frame, 2.f);

        for (int32 X = 3; X < Outpost::Cols - 2; X += 4)
        {
            Line(P(FVector2D(X * Outpost::Tile, 28.f)), P(FVector2D(X * Outpost::Tile + 35.f, 28.f)), FLinearColor(Cyan.R, Cyan.G, Cyan.B, .42f), 3.f);
            Line(P(FVector2D(X * Outpost::Tile, Outpost::Height - 28.f)),
                P(FVector2D(X * Outpost::Tile + 35.f, Outpost::Height - 28.f)), FLinearColor(Cyan.R, Cyan.G, Cyan.B, .42f), 3.f);
        }
    }

    void DrawSignalField()
    {
        const FVector2D Center(Outpost::Width * .5f, 390.f);
        Circle(Center, 72.f, FLinearColor(Cyan.R, Cyan.G, Cyan.B, .12f), 1.f, 48);
        Circle(Center, 96.f, FLinearColor(Cyan.R, Cyan.G, Cyan.B, .22f), 1.4f, 48);
        for (int32 I = 0; I < 12; ++I)
        {
            const float A0 = I * 2.f * PI / 12.f + Sim.Time * .05f;
            const float A1 = A0 + .09f;
            Circle(Center, 108.f, FLinearColor(Cyan.R, Cyan.G, Cyan.B, .24f), 1.2f, 3, A0, A1);
        }
        Circle(Center, 31.f, FLinearColor(Amber.R, Amber.G, Amber.B, .12f), 1.f, 32);
    }

    void DrawDoors()
    {
        const FVector2D Doors[] = { Outpost::FSimulation::Center(FIntPoint(1, 8)), Outpost::FSimulation::Center(FIntPoint(26, 8)) };
        for (int32 I = 0; I < 2; ++I)
        {
            const FVector2D C = Doors[I];
            const FVector2D A = P(C + FVector2D(-8.f, -45.f));
            const FVector2D B = P(C + FVector2D(8.f, 45.f));
            const FVector2D TL(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
            Tile(TL, FVector2D(FMath::Abs(B.X - A.X), FMath::Abs(B.Y - A.Y)), Srgb(48, 12, 24));
            for (int32 K = -1; K <= 1; ++K)
            {
                Line(P(C + FVector2D(-6.f, K * 21.f + 6.f)), P(C + FVector2D(6.f, K * 21.f - 6.f)), Red, 2.f);
            }
            const float Direction = I == 0 ? 1.f : -1.f;
            WorldPolygon(C + FVector2D(Direction * 18.f, 0.f), I == 0 ? 0.f : PI,
                { FVector2D(-5, -7), FVector2D(7, 0), FVector2D(-5, 7) }, FLinearColor(Amber.R, Amber.G, Amber.B, .85f));
        }
    }

    void DrawMine(const Outpost::FMine& Mine)
    {
        const FVector2D C = Mine.Rect.Center();
        const bool bLive = Mine.Remaining > 0;
        const FLinearColor Dark = bLive ? Srgb(17, 108, 108) : Srgb(46, 64, 69);
        const FLinearColor Light = bLive ? Srgb(74, 222, 194) : Srgb(74, 92, 94);
        WorldPolygon(C + FVector2D(0, 8), 0.f, { FVector2D(-24, 0), FVector2D(0, -8), FVector2D(24, 0), FVector2D(0, 10) }, FLinearColor(0.f, 0.f, 0.f, .42f));
        WorldPolygon(C, 0.f, { FVector2D(-21, 8), FVector2D(-17, -10), FVector2D(-8, -22), FVector2D(-2, -6), FVector2D(-4, 12) }, Dark, Light);
        WorldPolygon(C, 0.f, { FVector2D(-5, 10), FVector2D(-7, -15), FVector2D(3, -31), FVector2D(13, -13), FVector2D(10, 8) }, Light, Srgb(153, 255, 230, 230));
        WorldPolygon(C, 0.f, { FVector2D(7, 11), FVector2D(12, -9), FVector2D(23, -17), FVector2D(24, 2), FVector2D(17, 12) }, Dark, Light);
        if (Sim.bMining && Sim.ActiveMine == Mine.Rect.Id)
        {
            Circle(C, 31.f + FMath::Sin(Sim.Time * 16.f) * 3.f, FLinearColor(Cyan.R, Cyan.G, Cyan.B, .65f), 2.f, 28);
        }
    }

    void DrawForge()
    {
        const Outpost::FRect& F = Sim.Forge;
        WorldRect(F, FLinearColor(0.f, 0.f, 0.f, .48f), -5.f);
        WorldRect(F, Srgb(20, 36, 50), 1.f);
        WorldRect(F, Srgb(43, 66, 78), 6.f);
        const FVector2D C = F.Center();
        const float Pulse = .78f + .22f * FMath::Sin(Sim.Time * 8.f);
        const FLinearColor Flame = Srgb(255, 101, 24);
        WorldPolygon(C + FVector2D(-10, -8), 0.f,
            { FVector2D(-19, -14), FVector2D(14, -14), FVector2D(18, 8), FVector2D(0, 19), FVector2D(-19, 8) },
            FLinearColor(Flame.R * Pulse, Flame.G * Pulse, Flame.B * Pulse, 1.f), Srgb(255, 190, 86));
        WorldPolygon(C + FVector2D(12, 7), 0.f,
            { FVector2D(-20, -3), FVector2D(20, -3), FVector2D(12, 4), FVector2D(5, 4), FVector2D(8, 14), FVector2D(-8, 14), FVector2D(-5, 4), FVector2D(-20, 4) },
            Srgb(145, 172, 176), Ink);
        for (int32 I = 0; I < Outpost::MaxRank; ++I)
        {
            Tile(P(C + FVector2D(-24.f + I * 14.f, 25.f)) - FVector2D(4.f, 1.5f) * UiScale,
                FVector2D(8.f, 3.f) * UiScale, I < Sim.Rank ? Amber : Srgb(55, 74, 82));
        }
        Tile(P(C + FVector2D(16.f, 25.f)) - FVector2D(4.f, 1.5f) * UiScale,
            FVector2D(8.f, 3.f) * UiScale, Sim.ArmorRank > 0 ? Cyan : Srgb(55, 74, 82));
    }

    void DrawWall(const Outpost::FRect& Wall, float Alpha, bool bGhost, bool bValid)
    {
        FLinearColor Base = bGhost ? (bValid ? Srgb(24, 132, 115) : Srgb(158, 30, 56))
            : Srgb(43, 59, 68);
        Base.A = Alpha;
        WorldRect(Wall, FLinearColor(0.f, 0.f, 0.f, .55f * Alpha), -5.f);
        FLinearColor WallEdge = Srgb(111, 94, 57); WallEdge.A = Alpha;
        WorldRect(Wall, bGhost ? Base : WallEdge, 2.f);
        WorldRect(Wall, Base, 6.f);

        const FVector2D C = Wall.Center();
        const float Long = (Wall.W > Wall.H ? Wall.W : Wall.H) * Outpost::Tile;
        const bool bHorizontal = Wall.W > Wall.H;
        for (float D = -Long * .5f + 14.f; D < Long * .5f - 6.f; D += 19.f)
        {
            const FVector2D A = C + (bHorizontal ? FVector2D(D, -Wall.H * Outpost::Tile * .5f + 7.f) : FVector2D(-Wall.W * Outpost::Tile * .5f + 7.f, D));
            const FVector2D B = A + (bHorizontal ? FVector2D(8.f, 6.f) : FVector2D(6.f, 8.f));
            Line(P(A), P(B), bGhost ? (bValid ? Cyan : Red) : Amber, 2.2f);
        }
    }

    void DrawTelegraph(const Outpost::FEnemy& Enemy)
    {
        if (Enemy.AttackRemaining <= 0.f || Enemy.AttackDuration <= 0.f) return;
        const FLinearColor Accent = Enemy.Kind == Outpost::EEnemyKind::Boss ? Red
            : Enemy.Kind == Outpost::EEnemyKind::Brute ? Purple
            : Enemy.Kind == Outpost::EEnemyKind::Runner ? Amber : Red;
        const float Progress = 1.f - FMath::Clamp(Enemy.AttackRemaining / Enemy.AttackDuration, 0.f, 1.f);
        for (int32 I = 0; I < 12; ++I)
        {
            const float A0 = I * PI / 6.f;
            Circle(Enemy.AttackPoint, Enemy.AttackRadius, FLinearColor(Accent.R, Accent.G, Accent.B, .72f), 1.5f, 2, A0, A0 + .28f);
        }
        Circle(Enemy.AttackPoint, Enemy.AttackRadius + 5.f, Accent, 3.f, 42, -PI * .5f, -PI * .5f + Progress * 2.f * PI);
        Circle(Enemy.AttackPoint, 4.f + Progress * 4.f, FLinearColor(Accent.R, Accent.G, Accent.B, .88f), 2.f, 18);
        Line(P(Enemy.Pos), P(Enemy.AttackPoint), FLinearColor(Accent.R, Accent.G, Accent.B, .20f), 1.f);
    }

    void DrawEnemy(const Outpost::FEnemy& Enemy)
    {
        const float Angle = FMath::Atan2(Enemy.Facing.Y, Enemy.Facing.X);
        float Scale = FMath::Max(.82f, Enemy.Radius / 12.f);
        FLinearColor Body = Srgb(151, 44, 75), Edge = Srgb(242, 99, 112), Leg = Srgb(91, 47, 67);
        if (Enemy.Kind == Outpost::EEnemyKind::Runner) { Body = Srgb(214, 124, 27); Edge = Srgb(255, 199, 65); Leg = Srgb(100, 68, 32); Scale *= .94f; }
        else if (Enemy.Kind == Outpost::EEnemyKind::Brute) { Body = Srgb(91, 58, 132); Edge = Purple; Leg = Srgb(57, 44, 76); Scale *= 1.10f; }
        else if (Enemy.Kind == Outpost::EEnemyKind::Boss) { Body = Srgb(75, 18, 57); Edge = Red; Leg = Srgb(49, 15, 41); Scale *= 1.15f; }
        if (Enemy.Flash > 0.f) Body = Srgb(255, 240, 196);

        WorldPolygon(Enemy.Pos + FVector2D(0.f, 8.f), 0.f,
            { FVector2D(-18,-7), FVector2D(0,-11), FVector2D(18,-7), FVector2D(21,0), FVector2D(0,8), FVector2D(-21,0) },
            Srgb(0, 0, 0, 120), FLinearColor::Transparent, Scale);

        const float GaitSpeed = Enemy.Kind == Outpost::EEnemyKind::Runner ? 15.f : Enemy.Kind == Outpost::EEnemyKind::Brute ? 7.f : 10.f;
        for (int32 I = 0; I < 3; ++I)
        {
            for (const float Side : { -1.f, 1.f })
            {
                const float X = -10.f + I * 8.f;
                const float Gait = Enemy.AttackRemaining > 0.f ? 0.f : FMath::Sin(Sim.Time * GaitSpeed + I * 2.f) * 3.f;
                WorldLine(Enemy.Pos, Angle, FVector2D(X, Side * 7.f), FVector2D(X - 5.f + Gait, Side * 19.f), Leg, 3.f, Scale);
            }
        }

        WorldPolygon(Enemy.Pos, Angle,
            { FVector2D(-15,-5), FVector2D(-9,-14), FVector2D(5,-13), FVector2D(16,-6), FVector2D(16,6), FVector2D(5,13), FVector2D(-9,14), FVector2D(-15,5) },
            Body, Edge, Scale);
        if (Enemy.Kind == Outpost::EEnemyKind::Runner)
        {
            WorldPolygon(Enemy.Pos, Angle, { FVector2D(-12,-4), FVector2D(-23,0), FVector2D(-12,4) }, Edge, Srgb(255, 226, 140), Scale);
        }
        if (Enemy.Kind == Outpost::EEnemyKind::Brute || Enemy.Kind == Outpost::EEnemyKind::Boss)
        {
            WorldPolygon(Enemy.Pos, Angle, { FVector2D(-11,-12), FVector2D(-3,-12), FVector2D(-3,12), FVector2D(-11,12) }, Srgb(35, 30, 54), Edge, Scale);
            WorldPolygon(Enemy.Pos, Angle, { FVector2D(4,-11), FVector2D(12,-9), FVector2D(12,9), FVector2D(4,11) }, Srgb(35, 30, 54), Edge, Scale);
        }
        if (Enemy.Kind == Outpost::EEnemyKind::Boss)
        {
            Circle(Enemy.Pos, 23.f * Scale, FLinearColor(Red.R, Red.G, Red.B, .55f), 2.f, 28);
            WorldPolygon(Enemy.Pos, Angle, { FVector2D(1,-7), FVector2D(11,0), FVector2D(1,7), FVector2D(-6,0) }, Red, Amber, Scale);
        }
        WorldPolygon(Enemy.Pos, Angle, { FVector2D(7,-8), FVector2D(12,-8), FVector2D(12,-3), FVector2D(7,-3) }, Srgb(255, 230, 82), FLinearColor::Transparent, Scale);
        WorldPolygon(Enemy.Pos, Angle, { FVector2D(7,3), FVector2D(12,3), FVector2D(12,8), FVector2D(7,8) }, Srgb(255, 230, 82), FLinearColor::Transparent, Scale);

        const FVector2D Center = P(Enemy.Pos);
        const float BarWidth = (Enemy.Kind == Outpost::EEnemyKind::Boss ? 76.f : Enemy.Kind == Outpost::EEnemyKind::Brute ? 44.f : 34.f) * UiScale;
        const float Y = Center.Y - (Enemy.Kind == Outpost::EEnemyKind::Boss ? 42.f : 29.f) * UiScale;
        Tile(FVector2D(Center.X - BarWidth * .5f, Y), FVector2D(BarWidth, 4.f * UiScale), Srgb(7, 5, 12, 210));
        Tile(FVector2D(Center.X - BarWidth * .5f, Y), FVector2D(BarWidth * FMath::Clamp(Enemy.HP / FMath::Max(1.f, Enemy.MaxHP), 0.f, 1.f), 4.f * UiScale),
            Enemy.Kind == Outpost::EEnemyKind::Boss ? Red : Enemy.Kind == Outpost::EEnemyKind::Brute ? Purple : Enemy.Kind == Outpost::EEnemyKind::Runner ? Amber : Red);
    }

    void DrawBullet(const Outpost::FBullet& Bullet)
    {
        const FVector2D Tail = Bullet.Pos - Bullet.Velocity.GetSafeNormal() * 18.f;
        Line(P(Tail), P(Bullet.Pos), Srgb(153, 255, 220), 3.f);
        Tile(P(Bullet.Pos) - FVector2D(2.f * UiScale), FVector2D(4.f * UiScale), Srgb(255, 237, 138));
    }

    void DrawPlayer()
    {
        FVector2D Facing = Sim.Aim - Sim.Player;
        if (!Facing.Normalize()) Facing = FVector2D(1.f, 0.f);
        const float Angle = FMath::Atan2(Facing.Y, Facing.X);
        if (Sim.DashRemaining > 0.f)
        {
            for (int32 I = 3; I >= 1; --I)
            {
                const FVector2D After = Sim.Player - Sim.DashVector * (I * 13.f);
                Circle(After, 14.f, FLinearColor(Cyan.R, Cyan.G, Cyan.B, .10f + I * .06f), 2.f, 20);
            }
            for (int32 I = -1; I <= 1; ++I)
            {
                const FVector2D Side(-Sim.DashVector.Y, Sim.DashVector.X);
                Line(P(Sim.Player - Sim.DashVector * 42.f + Side * I * 7.f), P(Sim.Player - Sim.DashVector * 9.f + Side * I * 7.f), FLinearColor(Cyan.R,Cyan.G,Cyan.B,.65f), 2.f);
            }
        }

        WorldPolygon(Sim.Player + FVector2D(0, 11), 0.f,
            { FVector2D(-18,0), FVector2D(0,-7), FVector2D(18,0), FVector2D(0,8) }, FLinearColor(0.f,0.f,0.f,.48f));
        Circle(Sim.Player, 21.f, Sim.HurtRemaining > 0.f ? Red : (Sim.ArmorRank > 0 ? Cyan : FLinearColor(Cyan.R,Cyan.G,Cyan.B,.55f)), Sim.ArmorRank > 0 ? 3.f : 1.f, 30);
        const float Gait = FMath::Sin(Sim.PlayerTraveled * .38f) * 4.f;
        WorldLine(Sim.Player, Angle, FVector2D(-7,-8), FVector2D(-12 + Gait,-15), Srgb(67, 91, 98), 6.f);
        WorldLine(Sim.Player, Angle, FVector2D(-7,8), FVector2D(-12 - Gait,15), Srgb(67, 91, 98), 6.f);
        WorldPolygon(Sim.Player, Angle,
            { FVector2D(-13,-10), FVector2D(4,-12), FVector2D(12,-5), FVector2D(12,5), FVector2D(4,12), FVector2D(-13,10) },
            Srgb(108, 168, 164), Srgb(190, 232, 222));
        WorldPolygon(Sim.Player, Angle,
            { FVector2D(-7,-7), FVector2D(5,-8), FVector2D(9,0), FVector2D(5,8), FVector2D(-7,7) },
            Srgb(154, 210, 198));
        if (Sim.ArmorRank > 0)
        {
            WorldLine(Sim.Player, Angle, FVector2D(-11,-13), FVector2D(5,-15), Cyan, 3.f);
            WorldLine(Sim.Player, Angle, FVector2D(-11,13), FVector2D(5,15), Cyan, 3.f);
        }

        if (Sim.Tool == 1)
        {
            const float Recoil = Sim.Cooldown > 0.f ? FMath::Clamp(Sim.Cooldown / .24f, 0.f, 1.f) * 7.f : 0.f;
            WorldPolygon(Sim.Player, Angle, { FVector2D(5-Recoil,7), FVector2D(32-Recoil,7), FVector2D(32-Recoil,14), FVector2D(5-Recoil,14) }, Srgb(87, 118, 122), Ink);
            WorldLine(Sim.Player, Angle, FVector2D(15-Recoil,10), FVector2D(31-Recoil,10), Cyan, 2.5f);
            if (Sim.Cooldown > .16f) WorldPolygon(Sim.Player, Angle, { FVector2D(34-Recoil,5), FVector2D(47-Recoil,10), FVector2D(34-Recoil,15), FVector2D(39-Recoil,10) }, Srgb(255, 232, 133));
        }
        else if (Sim.Tool == 2)
        {
            const float Swing = Sim.bMining ? FMath::Sin(Sim.Time * 18.f) * .45f : .18f;
            WorldLine(Sim.Player, Angle + Swing, FVector2D(4,11), FVector2D(30,2), Srgb(166, 119, 65), 4.f);
            WorldPolygon(Sim.Player, Angle + Swing, { FVector2D(21,-8), FVector2D(30,-3), FVector2D(34,8), FVector2D(25,3), FVector2D(18,0) }, Cyan, Ink);
        }
        else
        {
            WorldPolygon(Sim.Player, Angle, { FVector2D(8,-14), FVector2D(18,-14), FVector2D(18,-6), FVector2D(8,-6) }, Srgb(207, 174, 118), Amber);
            WorldPolygon(Sim.Player, Angle, { FVector2D(8,6), FVector2D(18,6), FVector2D(18,14), FVector2D(8,14) }, Srgb(207, 174, 118), Amber);
        }
    }
};
}

void FOutpostArenaRenderer::Draw(UCanvas* Canvas, const AOutpostGameMode& GameMode, float UiScale)
{
    if (Canvas) FArenaDraw(Canvas, GameMode, UiScale).Draw();
}
