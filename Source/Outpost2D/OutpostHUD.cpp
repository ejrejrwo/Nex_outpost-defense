#include "OutpostHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GlobalRenderResources.h"
#include "OutpostArenaRenderer.h"
#include "OutpostGameMode.h"
#include "OutpostSimulation.h"
#include "Rendering/SlateRenderer.h"

namespace
{
constexpr float LargeFontPixels = 33.f;
constexpr float MediumFontPixels = 24.f;
constexpr float SmallFontPixels = 21.f;

FLinearColor Srgb(uint8 R, uint8 G, uint8 B, uint8 A = 255)
{
    return FLinearColor::FromSRGBColor(FColor(R, G, B, A));
}

FLinearColor WithAlpha(FLinearColor Color, float Alpha)
{
    Color.A = Alpha;
    return Color;
}

const FLinearColor Navy = Srgb(5, 13, 25, 250);
const FLinearColor NavySoft = Srgb(9, 24, 38, 248);
const FLinearColor Panel = Srgb(13, 34, 50, 248);
const FLinearColor PanelBright = Srgb(20, 51, 69);
const FLinearColor Ink = Srgb(222, 237, 240);
const FLinearColor Muted = Srgb(99, 132, 145);
const FLinearColor Cyan = Srgb(30, 232, 207);
const FLinearColor Amber = Srgb(255, 166, 46);
const FLinearColor Red = Srgb(255, 60, 76);
const FLinearColor Green = Srgb(85, 232, 141);
const FLinearColor Hairline = Srgb(28, 79, 93, 184);

FSlateFontInfo MakeUiFont(float PixelSize)
{
    // Canvas requires a UFont object; its composite also supplies Korean fallback.
    return FSlateFontInfo(GEngine->GetMediumFont(), FMath::Max(1, FMath::RoundToInt(PixelSize * .75f)));
}

FString RankText(int32 Rank)
{
    return Rank <= 0 ? TEXT("MK I") : Rank == 1 ? TEXT("MK II") : TEXT("MK III");
}

FString PhaseText(const Outpost::FSimulation& Sim)
{
    using Outpost::EPhase;
    switch (Sim.Phase)
    {
    case EPhase::Menu: return TEXT("대기 중");
    case EPhase::Prep: return FString::Printf(TEXT("준비  %02d초"), FMath::CeilToInt(Sim.PrepRemaining));
    case EPhase::Combat: return FString::Printf(TEXT("WAVE %d  /  적 %d"), Sim.Wave, Sim.Enemies.Num());
    case EPhase::Rest: return FString::Printf(TEXT("재정비  %02d초"), FMath::CeilToInt(Sim.RestRemaining));
    case EPhase::Won: return TEXT("신호 확보");
    case EPhase::Lost: return TEXT("신호 소실");
    default: return FString();
    }
}
}

AOutpostHUD::AOutpostHUD()
{
}

AOutpostGameMode* AOutpostHUD::GetOutpostGameMode() const
{
    return GetWorld() ? Cast<AOutpostGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
}

void AOutpostHUD::DrawPanel(const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color)
{
    FCanvasTileItem Tile(Position, Size, Color);
    Tile.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Tile);
}

void AOutpostHUD::DrawLine(const FVector2D& Start, const FVector2D& End, const FLinearColor& Color, float Thickness)
{
    const FVector2D Delta = End - Start;
    const float Length = Delta.Size();
    if (Length <= KINDA_SMALL_NUMBER) return;
    const float HalfWidth = FMath::Max(.3f, Thickness * UiScale * .5f);
    const FVector2D Normal(-Delta.Y / Length * HalfWidth, Delta.X / Length * HalfWidth);
    const auto DrawTriangle = [this, &Color](const FVector2D& A, const FVector2D& B, const FVector2D& C)
    {
        FCanvasTriangleItem Item(A, B, C, GWhiteTexture);
        Item.SetColor(Color);
        Item.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(Item);
    };
    DrawTriangle(Start - Normal, End - Normal, End + Normal);
    DrawTriangle(Start - Normal, End + Normal, Start + Normal);
}

void AOutpostHUD::DrawLabel(const FString& Text, const FVector2D& Position, const FLinearColor& Color,
    float Scale, bool bCenterX, bool bCenterY, EOutpostFont Font)
{
    const float BasePixels = Font == EOutpostFont::Large ? LargeFontPixels
        : Font == EOutpostFont::Small ? SmallFontPixels : MediumFontPixels;
    const FSlateFontInfo FontInfo = MakeUiFont(BasePixels * Scale * UiScale);
    FCanvasTextItem Item(Position, FText::FromString(Text), FontInfo, Color);
    Item.Scale = FVector2D(1.f, 1.f);
    Item.bCentreX = bCenterX;
    Item.bCentreY = bCenterY;
    Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, .72f), FVector2D(1.f, 1.f) * UiScale);
    Canvas->DrawItem(Item);
}

void AOutpostHUD::DrawBar(const FVector2D& Position, const FVector2D& Size, float Fraction,
    const FLinearColor& Fill, const FLinearColor& Back)
{
    DrawPanel(Position, Size, Back);
    const float Clamped = FMath::Clamp(Fraction, 0.f, 1.f);
    if (Clamped > 0.f)
    {
        DrawPanel(Position, FVector2D(Size.X * Clamped, Size.Y), Fill);
    }
}

void AOutpostHUD::DrawButton(const FString& Label, FName HitBox, const FVector2D& Position,
    const FVector2D& Size, bool bAccent, bool bSelected, int32 Priority, float TextScale, bool bEnabled)
{
    const FLinearColor Fill = !bEnabled ? FLinearColor(Panel.R, Panel.G, Panel.B, .62f)
        : bSelected ? FLinearColor(Cyan.R, Cyan.G, Cyan.B, .17f)
        : bAccent ? FLinearColor(Amber.R, Amber.G, Amber.B, .16f) : Panel;
    const FLinearColor Edge = !bEnabled ? FLinearColor(Hairline.R, Hairline.G, Hairline.B, .35f)
        : bSelected ? Cyan : bAccent ? Amber : Hairline;
    DrawPanel(Position, Size, Fill);
    DrawLine(Position, Position + FVector2D(Size.X, 0.f), Edge, bSelected ? 2.f : 1.f);
    DrawLine(Position + FVector2D(0.f, Size.Y), Position + Size, Edge, 1.f);
    DrawLine(Position, Position + FVector2D(0.f, Size.Y), Edge, 1.f);
    DrawLine(Position + FVector2D(Size.X, 0.f), Position + Size, Edge, 1.f);
    DrawLabel(Label, Position + Size * .5f, !bEnabled ? Muted * FLinearColor(1.f, 1.f, 1.f, .62f)
        : bSelected ? Cyan : bAccent ? Amber : Ink,
        TextScale, true, true, EOutpostFont::Medium);
    if (bEnabled)
    {
        AddHitBox(Position, Size, HitBox, true, Priority);
    }
}

void AOutpostHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas)
    {
        return;
    }

    ScreenWidth = Canvas->SizeX;
    ScreenHeight = Canvas->SizeY;
    UiScale = FMath::Clamp(FMath::Min(ScreenWidth / 1280.f, ScreenHeight / 800.f), .35f, 1.75f);

    AOutpostGameMode* GameMode = GetOutpostGameMode();
    if (!GameMode)
    {
        return;
    }

    FOutpostArenaRenderer::Draw(Canvas, *GameMode, UiScale);
    DrawWorldHud(*GameMode);
    DrawHeader(*GameMode);
    DrawFooter(*GameMode);
    DrawCrosshair(*GameMode);
    DrawOverlay(*GameMode);
}

void AOutpostHUD::DrawHeader(AOutpostGameMode& GameMode)
{
    const Outpost::FSimulation& Sim = GameMode.Sim;
    const float H = 96.f * UiScale;
    DrawPanel(FVector2D::ZeroVector, FVector2D(ScreenWidth, H), Navy);
    DrawPanel(FVector2D(0.f, H - 2.f * UiScale), FVector2D(ScreenWidth, 2.f * UiScale), Cyan * FLinearColor(1.f, 1.f, 1.f, .58f));

    const float Margin = 24.f * UiScale;
    DrawLabel(TEXT("OUTPOST"), FVector2D(Margin, 13.f * UiScale), Ink, .86f, false, false, EOutpostFont::Large);
    DrawLabel(TEXT("180"), FVector2D(Margin + 162.f * UiScale, 13.f * UiScale), Cyan,
        .86f, false, false, EOutpostFont::Large);
    DrawLabel(TEXT("LAST SIGNAL  /  30초 준비 → 3 WAVE → BOSS"),
        FVector2D(Margin, 59.f * UiScale), Muted, .50f, false, false, EOutpostFont::Medium);

    const float StatusX = 382.f * UiScale;
    DrawLine(FVector2D(StatusX - 18.f * UiScale, 14.f * UiScale),
        FVector2D(StatusX - 18.f * UiScale, H - 15.f * UiScale), Hairline);

    FLinearColor PhaseColor = Cyan;
    if (Sim.Phase == Outpost::EPhase::Combat) PhaseColor = Amber;
    if ((Sim.Phase == Outpost::EPhase::Prep && Sim.PrepRemaining <= 10.f) || Sim.Phase == Outpost::EPhase::Lost) PhaseColor = Red;
    DrawLabel(PhaseText(Sim), FVector2D(StatusX, 15.f * UiScale), PhaseColor, .69f, false, false, EOutpostFont::Medium);
    DrawLabel(FString::Printf(TEXT("WAVE %d / 3    KILLS %02d / %d"), Sim.Wave, Sim.Kills, Outpost::TotalEnemies),
        FVector2D(StatusX, 55.f * UiScale), Muted, .49f, false, false, EOutpostFont::Medium);

    const float StatsX = 660.f * UiScale;
    DrawLine(FVector2D(StatsX - 18.f * UiScale, 14.f * UiScale),
        FVector2D(StatsX - 18.f * UiScale, H - 15.f * UiScale), Hairline);
    DrawLabel(FString::Printf(TEXT("ORE  %03d"), Sim.Ore), FVector2D(StatsX, 14.f * UiScale), Amber, .64f);
    DrawLabel(FString::Printf(TEXT("HP  %03d / %03d"), FMath::CeilToInt(Sim.PlayerHP), FMath::CeilToInt(Sim.MaxHP)),
        FVector2D(StatsX + 116.f * UiScale, 14.f * UiScale), Ink, .59f);
    DrawBar(FVector2D(StatsX + 116.f * UiScale, 48.f * UiScale), FVector2D(140.f * UiScale, 6.f * UiScale),
        Sim.PlayerHP / FMath::Max(1.f, Sim.MaxHP), Sim.PlayerHP < Sim.MaxHP * .3f ? Red : Cyan, PanelBright);
    DrawLabel(FString::Printf(TEXT("FIRE  %s"), *RankText(Sim.Rank)), FVector2D(StatsX, 57.f * UiScale), Cyan, .47f);
    DrawLabel(FString::Printf(TEXT("ARMOR  %s"), Sim.ArmorRank > 0 ? TEXT("I") : TEXT("—")),
        FVector2D(StatsX + 116.f * UiScale, 57.f * UiScale), Sim.ArmorRank > 0 ? Cyan : Muted, .47f);

    const FString ModeText = Sim.Mode == Outpost::EMode::Hard ? TEXT("HARD") : TEXT("PRACTICE");
    DrawLabel(ModeText, FVector2D(ScreenWidth - 220.f * UiScale, 17.f * UiScale),
        Sim.Mode == Outpost::EMode::Hard ? Red : Green, .48f);
    const FVector2D SoundPos(ScreenWidth - 128.f * UiScale, 55.f * UiScale);
    DrawButton(GameMode.bSound ? TEXT("SFX ON") : TEXT("SFX OFF"), TEXT("SOUND"), SoundPos,
        FVector2D(98.f, 27.f) * UiScale, false, GameMode.bSound, 20, .51f);
}

void AOutpostHUD::DrawFooter(AOutpostGameMode& GameMode)
{
    const Outpost::FSimulation& Sim = GameMode.Sim;
    const float H = 100.f * UiScale;
    const float Y = ScreenHeight - H;
    DrawPanel(FVector2D(0.f, Y), FVector2D(ScreenWidth, H), Navy);
    DrawPanel(FVector2D(0.f, Y), FVector2D(ScreenWidth, 2.f * UiScale), Cyan * FLinearColor(1.f, 1.f, 1.f, .58f));

    const float ToolW = 146.f * UiScale;
    const float Gap = 7.f * UiScale;
    const float ToolY = Y + 17.f * UiScale;
    const bool bCanChangeTool = Sim.CanWork() && Sim.JobRemaining <= 0.f && !Sim.bCarrying && Sim.DashRemaining <= 0.f;
    DrawButton(TEXT("1  블래스터"), TEXT("TOOL_1"), FVector2D(22.f * UiScale, ToolY),
        FVector2D(ToolW, 46.f * UiScale), false, Sim.Tool == 1, 2, .61f, bCanChangeTool);
    DrawButton(TEXT("2  곡괭이"), TEXT("TOOL_2"), FVector2D(22.f * UiScale + ToolW + Gap, ToolY),
        FVector2D(ToolW, 46.f * UiScale), false, Sim.Tool == 2, 2, .61f, bCanChangeTool);
    DrawButton(TEXT("3  방벽 운반"), TEXT("TOOL_3"), FVector2D(22.f * UiScale + (ToolW + Gap) * 2.f, ToolY),
        FVector2D(ToolW, 46.f * UiScale), false, Sim.Tool == 3, 2, .58f, bCanChangeTool);

    const bool bDashReady = Sim.Phase == Outpost::EPhase::Combat && Sim.DashCooldown <= 0.f
        && Sim.JobRemaining <= 0.f && !Sim.bCarrying && Sim.DashRemaining <= 0.f && !Sim.bPaused;
    FString DashText;
    if (Sim.bCarrying) DashText = TEXT("운반 중");
    else if (Sim.JobRemaining > 0.f) DashText = TEXT("작업 중");
    else if (Sim.DashRemaining > 0.f) DashText = TEXT("DASH");
    else if (Sim.Phase != Outpost::EPhase::Combat) DashText = TEXT("전투 전용");
    else if (Sim.DashCooldown <= 0.f) DashText = TEXT("SPACE  READY");
    else DashText = FString::Printf(TEXT("SPACE  %.1fs"), Sim.DashCooldown);
    const float DashX = 22.f * UiScale + (ToolW + Gap) * 3.f;
    DrawButton(DashText, TEXT("DASH"), FVector2D(DashX, ToolY), FVector2D(132.f, 46.f) * UiScale,
        false, bDashReady, 2, .48f, bDashReady);

    DrawLabel(TEXT("LMB 사용  ·  RMB 이동  ·  E 상호작용/취소  ·  F 보호구  ·  R 회전"),
        FVector2D(22.f * UiScale, Y + 73.f * UiScale), Muted, .49f);

    const float HintX = 665.f * UiScale;
    DrawLine(FVector2D(HintX - 24.f * UiScale, Y + 17.f * UiScale),
        FVector2D(HintX - 24.f * UiScale, Y + 80.f * UiScale), Hairline);
    DrawLabel(TEXT("TACTICAL FEED"), FVector2D(HintX, Y + 17.f * UiScale), Cyan, .47f);
    const FString Hint = GameMode.ToastRemaining > 0.f && !GameMode.Toast.IsEmpty() ? GameMode.Toast : Sim.Hint();
    const FString HintText = Hint.IsEmpty() ? TEXT("신호 기지를 방어할 준비를 하십시오.") : Hint;
    const float DefaultHintScale = .58f;
    const float HintMaxWidth = FMath::Max(120.f * UiScale, ScreenWidth - HintX - 180.f * UiScale);
    float HintScale = DefaultHintScale;
    if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
    {
        const FSlateFontInfo HintFont = MakeUiFont(MediumFontPixels * DefaultHintScale * UiScale);
        const FVector2f Measured = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(HintText, HintFont);
        if (Measured.X > HintMaxWidth && Measured.X > 1.f)
        {
            HintScale *= (HintMaxWidth / Measured.X) * .98f;
        }
    }
    DrawLabel(HintText, FVector2D(HintX, Y + 47.f * UiScale), Ink, HintScale);

    if (Sim.JobRemaining > 0.f)
    {
        const float Progress = 1.f - Sim.JobRemaining / Outpost::ForgeSeconds;
        const FLinearColor JobColor = Sim.JobKind == Outpost::EUpgradeKind::Armor ? Cyan : Amber;
        DrawBar(FVector2D(HintX, Y + 76.f * UiScale), FVector2D(330.f * UiScale, 4.f * UiScale),
            Progress, JobColor, PanelBright);
    }
    else if (Sim.bMining)
    {
        DrawBar(FVector2D(HintX, Y + 76.f * UiScale), FVector2D(330.f * UiScale, 4.f * UiScale),
            Sim.MineProgress / Outpost::MineSeconds, Cyan, PanelBright);
    }

    DrawButton(Sim.bPaused ? TEXT("계속") : TEXT("II  일시정지"), TEXT("PAUSE"),
        FVector2D(ScreenWidth - 142.f * UiScale, ToolY), FVector2D(120.f, 46.f) * UiScale,
        false, Sim.bPaused, 20, .52f);
}

void AOutpostHUD::DrawWorldHud(AOutpostGameMode& GameMode)
{
    const Outpost::FSimulation& Sim = GameMode.Sim;

    const auto DrawDottedRoute = [this, &GameMode](const TArray<FVector2D>& Route, const FLinearColor& Color)
    {
        for (int32 I = 1; I < Route.Num(); ++I)
        {
            const FVector2D A = GameMode.ScreenPoint(Route[I - 1]);
            const FVector2D B = GameMode.ScreenPoint(Route[I]);
            const FVector2D Delta = B - A;
            const float Length = Delta.Size();
            if (Length < 1.f)
            {
                continue;
            }
            const FVector2D Direction = Delta / Length;
            const float Step = 9.f * UiScale;
            const float Dash = 4.f * UiScale;
            for (float Distance = 0.f; Distance < Length; Distance += Step)
            {
                DrawLine(A + Direction * Distance,
                    A + Direction * FMath::Min(Distance + Dash, Length), Color, 1.15f);
            }
        }
    };

    if (Sim.CanWork() && (Sim.Tool == 3 || Sim.bCarrying))
    {
        const TArray<uint8> Grid = Sim.MakeGrid();
        const FIntPoint Goal = Outpost::FSimulation::Cell(Sim.Player);
        const FIntPoint Entrances[] = { FIntPoint(1, 8), FIntPoint(26, 8) };
        for (const FIntPoint Entrance : Entrances)
        {
            const TArray<FIntPoint> Cells = Sim.FindPath(Grid, Entrance, Goal);
            TArray<FVector2D> Route;
            Route.Reserve(Cells.Num());
            for (const FIntPoint Cell : Cells)
            {
                Route.Add(Outpost::FSimulation::Center(Cell));
            }
            DrawDottedRoute(Route, FLinearColor(Amber.R, Amber.G, Amber.B, .48f));
        }
    }

    if (!Sim.MoveOrder.IsEmpty())
    {
        TArray<FVector2D> Route;
        Route.Reserve(Sim.MoveOrder.Num() + 1);
        Route.Add(Sim.Player);
        Route.Append(Sim.MoveOrder);
        DrawDottedRoute(Route, FLinearColor(Cyan.R, Cyan.G, Cyan.B, .55f));

        const FVector2D Destination = GameMode.ScreenPoint(Sim.MoveOrder.Last());
        const float Marker = 7.f * UiScale;
        DrawLine(Destination + FVector2D(0.f, -Marker), Destination + FVector2D(Marker, 0.f), Cyan, 1.3f);
        DrawLine(Destination + FVector2D(Marker, 0.f), Destination + FVector2D(0.f, Marker), Cyan, 1.3f);
        DrawLine(Destination + FVector2D(0.f, Marker), Destination + FVector2D(-Marker, 0.f), Cyan, 1.3f);
        DrawLine(Destination + FVector2D(-Marker, 0.f), Destination + FVector2D(0.f, -Marker), Cyan, 1.3f);
    }

    for (const FOutpostParticle& Particle : GameMode.Particles)
    {
        if (Particle.Life <= 0.f || Particle.TotalLife <= 0.f)
        {
            continue;
        }
        const FVector2D P = GameMode.ScreenPoint(Particle.Pos);
        const float Alpha = FMath::Clamp(Particle.Life / Particle.TotalLife, 0.f, 1.f);
        const float Size = FMath::Max(1.f, Particle.Size * UiScale * (.65f + Alpha * .55f));
        FLinearColor Color = Particle.Color;
        Color.A *= Alpha;
        DrawPanel(P - FVector2D(Size * .5f), FVector2D(Size), Color);
    }

    for (const FOutpostRing& Ring : GameMode.Rings)
    {
        if (Ring.Life <= 0.f || Ring.TotalLife <= 0.f) continue;
        FLinearColor Color = Ring.Color;
        Color.A *= FMath::Clamp(Ring.Life / Ring.TotalLife, 0.f, 1.f);
        FVector2D Previous;
        constexpr int32 Segments = 42;
        for (int32 I = 0; I <= Segments; ++I)
        {
            const float Angle = 2.f * PI * static_cast<float>(I) / Segments;
            const FVector2D Current = GameMode.ScreenPoint(Ring.Pos + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Ring.Radius);
            if (I > 0) DrawLine(Previous, Current, Color, 2.2f);
            Previous = Current;
        }
    }

    const Outpost::FEnemy* Boss = Sim.Enemies.FindByPredicate([](const Outpost::FEnemy& Enemy)
    {
        return Enemy.Kind == Outpost::EEnemyKind::Boss;
    });
    if (Boss)
    {
        const FVector2D Position(ScreenWidth * .5f - 230.f * UiScale, 106.f * UiScale);
        const FVector2D Size(460.f, 34.f);
        DrawPanel(Position, Size * UiScale, FLinearColor(Navy.R, Navy.G, Navy.B, .94f));
        DrawPanel(Position, FVector2D(4.f, Size.Y) * UiScale, Red);
        DrawLabel(TEXT("BREAKER HP"), Position + FVector2D(16.f, 7.f) * UiScale,
            Red, .48f, false, false, EOutpostFont::Small);
        DrawBar(Position + FVector2D(132.f, 13.f) * UiScale, FVector2D(245.f, 8.f) * UiScale,
            Boss->HP / FMath::Max(1.f, Boss->MaxHP), Red, PanelBright);
        DrawLabel(FString::Printf(TEXT("%03d"), FMath::CeilToInt(Boss->HP)),
            Position + FVector2D(397.f, 7.f) * UiScale, Ink, .48f, false, false, EOutpostFont::Small);
    }

    const FVector2D Player = GameMode.ScreenPoint(Sim.Player);
    DrawPanel(Player + FVector2D(-48.f, -43.f) * UiScale, FVector2D(96.f, 18.f) * UiScale,
        FLinearColor(Navy.R, Navy.G, Navy.B, .78f));
    DrawLabel(FString::Printf(TEXT("%s  ·  ARM %d"), *RankText(Sim.Rank), Sim.ArmorRank),
        Player + FVector2D(0.f, -34.f) * UiScale, Sim.HurtRemaining > 0.f ? Red : Cyan,
        .57f, true, true, EOutpostFont::Small);

    for (const Outpost::FMine& Mine : Sim.Mines)
    {
        if (Mine.Remaining <= 0)
        {
            continue;
        }
        const FVector2D P = GameMode.ScreenPoint(Mine.Rect.Center());
        DrawPanel(P + FVector2D(-36.f, -43.f) * UiScale, FVector2D(72.f, 18.f) * UiScale,
            FLinearColor(Navy.R, Navy.G, Navy.B, .74f));
        DrawLabel(FString::Printf(TEXT("ORE %03d"), Mine.Remaining), P + FVector2D(0.f, -34.f) * UiScale,
            Amber, .57f, true, true, EOutpostFont::Small);
    }

    const FVector2D Forge = GameMode.ScreenPoint(Sim.Forge.Center());
    const bool bForgeJob = Sim.JobRemaining > 0.f;
    DrawPanel(Forge + FVector2D(-104.f, -58.f) * UiScale, FVector2D(208.f, 21.f) * UiScale,
        FLinearColor(Navy.R, Navy.G, Navy.B, .82f));
    FString ForgeText;
    FLinearColor ForgeColor = Amber;
    if (bForgeJob)
    {
        ForgeColor = Sim.JobKind == Outpost::EUpgradeKind::Armor ? Cyan : Amber;
        ForgeText = FString::Printf(TEXT("%s 제작 %.1fs  ·  E 취소"),
            Sim.JobKind == Outpost::EUpgradeKind::Armor ? TEXT("보호구") : TEXT("화력"), Sim.JobRemaining);
    }
    else
    {
        const FString FireCost = Sim.Rank >= Outpost::MaxRank ? TEXT("MAX") : FString::FromInt(Sim.UpgradeCost());
        const FString ArmorCost = Sim.ArmorRank > 0 ? TEXT("MAX") : TEXT("10");
        ForgeText = FString::Printf(TEXT("FORGE  ·  E 화력 %s  /  F 보호구 %s  ·  5초"), *FireCost, *ArmorCost);
    }
    DrawLabel(ForgeText, Forge + FVector2D(0.f, -47.f) * UiScale, ForgeColor,
        .53f, true, true, EOutpostFont::Small);
    if (!bForgeJob && Sim.CanWork() && Sim.NearForge() && Sim.DashRemaining <= 0.f && !Sim.bCarrying)
    {
        DrawLine(Forge + FVector2D(0.f, -57.f) * UiScale, Forge + FVector2D(0.f, -38.f) * UiScale, Hairline);
        const bool bFireAvailable = Sim.Rank < Outpost::MaxRank && Sim.Ore >= Sim.UpgradeCost();
        const bool bArmorAvailable = Sim.ArmorRank <= 0 && Sim.Ore >= 10;
        if (bFireAvailable)
        {
            AddHitBox(Forge + FVector2D(-104.f, -58.f) * UiScale,
                FVector2D(103.f, 21.f) * UiScale, TEXT("FORGE"), true, 4);
        }
        if (bArmorAvailable)
        {
            AddHitBox(Forge + FVector2D(1.f, -58.f) * UiScale,
                FVector2D(103.f, 21.f) * UiScale, TEXT("ARMOR"), true, 4);
        }
    }

    if (Sim.bCarrying)
    {
        const FVector2D World = GameMode.MouseWorld();
        const Outpost::FPlacement Placement = Sim.Placement(World);
        const FVector2D Ghost = GameMode.ScreenPoint(World);
        const FLinearColor StateColor = Placement.bValid ? Green : Red;
        DrawPanel(Ghost + FVector2D(-88.f, 25.f) * UiScale, FVector2D(176.f, 21.f) * UiScale,
            FLinearColor(Navy.R, Navy.G, Navy.B, .88f));
        DrawLabel(Placement.bValid ? TEXT("배치 가능 · LMB") : Placement.Reason,
            Ghost + FVector2D(0.f, 35.f) * UiScale, StateColor, .56f, true, true, EOutpostFont::Small);
    }
}

void AOutpostHUD::DrawCrosshair(AOutpostGameMode& GameMode)
{
    const Outpost::EPhase Phase = GameMode.Sim.Phase;
    if (Phase == Outpost::EPhase::Menu || Phase == Outpost::EPhase::Won || Phase == Outpost::EPhase::Lost
        || GameMode.Sim.bPaused)
    {
        return;
    }

    float MouseX = 0.f, MouseY = 0.f;
    APlayerController* PC = GetOwningPlayerController();
    if (!PC || !PC->GetMousePosition(MouseX, MouseY))
    {
        return;
    }
    const FVector2D P(MouseX, MouseY);
    const FLinearColor Color = GameMode.Sim.Tool == 1 ? Amber : Cyan;
    const float Inner = 5.f * UiScale;
    const float Outer = 11.f * UiScale;
    DrawLine(P + FVector2D(-Outer, 0.f), P + FVector2D(-Inner, 0.f), Color, 1.4f);
    DrawLine(P + FVector2D(Inner, 0.f), P + FVector2D(Outer, 0.f), Color, 1.4f);
    DrawLine(P + FVector2D(0.f, -Outer), P + FVector2D(0.f, -Inner), Color, 1.4f);
    DrawLine(P + FVector2D(0.f, Inner), P + FVector2D(0.f, Outer), Color, 1.4f);
    DrawPanel(P - FVector2D(UiScale), FVector2D(2.f * UiScale), Color);
}

void AOutpostHUD::DrawOverlay(AOutpostGameMode& GameMode)
{
    const Outpost::FSimulation& Sim = GameMode.Sim;
    using Outpost::EPhase;

    if (Sim.Phase == EPhase::Prep && Sim.PrepRemaining <= 10.f && Sim.PrepRemaining > 0.f)
    {
        const FVector2D P(ScreenWidth * .5f, 146.f * UiScale);
        DrawPanel(P + FVector2D(-118.f, -30.f) * UiScale, FVector2D(236.f, 62.f) * UiScale,
            Srgb(38, 4, 9, 224));
        DrawLabel(FString::Printf(TEXT("%02d"), FMath::CeilToInt(Sim.PrepRemaining)),
            P + FVector2D(-62.f, 0.f) * UiScale, Red, 1.20f, true, true, EOutpostFont::Large);
        DrawLabel(TEXT("전투 임박"), P + FVector2D(37.f, -8.f) * UiScale, Ink, .64f, true, true, EOutpostFont::Medium);
        DrawLabel(Sim.Mode == Outpost::EMode::Hard ? TEXT("HARD") : TEXT("PRACTICE"),
            P + FVector2D(37.f, 14.f) * UiScale, Sim.Mode == Outpost::EMode::Hard ? Red : Green,
            .43f, true, true, EOutpostFont::Medium);
    }

    if (GameMode.WaveBannerRemaining > 0.f && Sim.Phase == EPhase::Combat)
    {
        bool bBoss = false;
        for (const Outpost::FEnemy& Enemy : Sim.Enemies)
        {
            if (Enemy.Kind == Outpost::EEnemyKind::Boss) { bBoss = true; break; }
        }
        const FVector2D P(ScreenWidth * .5f, (bBoss ? 177.f : 142.f) * UiScale);
        const float Fade = FMath::Clamp(GameMode.WaveBannerRemaining, 0.f, 1.f);
        DrawPanel(P + FVector2D(-145.f, -24.f) * UiScale, FVector2D(290.f, 49.f) * UiScale,
            WithAlpha(Srgb(3, 8, 16), .82f * Fade));
        DrawLine(P + FVector2D(-145.f, 24.f) * UiScale, P + FVector2D(145.f, 24.f) * UiScale,
            bBoss ? Red : Amber, 2.f);
        DrawLabel(bBoss ? TEXT("FINAL BOSS  ·  SIGNAL BREAKER")
            : FString::Printf(TEXT("WAVE %d  ·  INBOUND"), Sim.Wave), P,
            bBoss ? Red : Amber, bBoss ? .55f : .67f, true, true, EOutpostFont::Medium);
    }

    const bool bResult = Sim.Phase == EPhase::Won || Sim.Phase == EPhase::Lost;
    const bool bMenu = Sim.Phase == EPhase::Menu;
    const bool bPause = Sim.bPaused && !bMenu && !bResult;
    if (!bMenu && !bPause && !bResult)
    {
        return;
    }

    DrawPanel(FVector2D::ZeroVector, FVector2D(ScreenWidth, ScreenHeight), WithAlpha(Srgb(2, 7, 14), .83f));
    AddHitBox(FVector2D::ZeroVector, FVector2D(ScreenWidth, ScreenHeight), TEXT("OVERLAY_BLOCK"), true, 40);
    const float CardW = (bMenu ? 680.f : bResult ? 650.f : 560.f) * UiScale;
    const float CardH = (bMenu ? 590.f : bResult ? 520.f : 330.f) * UiScale;
    const FVector2D Card((ScreenWidth - CardW) * .5f, (ScreenHeight - CardH) * .5f);
    DrawPanel(Card, FVector2D(CardW, CardH), NavySoft);
    DrawPanel(Card, FVector2D(4.f * UiScale, CardH), bResult && Sim.Phase == EPhase::Lost ? Red : Cyan);
    DrawLine(Card + FVector2D(30.f, 76.f) * UiScale,
        Card + FVector2D(CardW / UiScale - 30.f, 76.f) * UiScale, Hairline);

    FString Kicker, Title, Body;
    if (bMenu)
    {
        Kicker = TEXT("OUTPOST / 180  ·  SECTOR 01");
        Title = TEXT("LAST SIGNAL");
        Body = TEXT("30초 준비 → 3 WAVE → 최종 보스\n광석과 방벽을 활용해 마지막 송신기를 지키십시오.");
    }
    else if (bPause)
    {
        Kicker = TEXT("SYSTEM HOLD");
        Title = TEXT("일시정지");
        Body = TEXT("전술 시계와 모든 유닛이 정지했습니다.\n현재 진행을 이어가거나 같은 모드로 다시 시작할 수 있습니다.");
    }
    else
    {
        Kicker = Sim.Phase == EPhase::Won ? TEXT("TRANSMISSION SECURED") : TEXT("SIGNAL LOST");
        Title = Sim.Phase == EPhase::Won ? TEXT("임무 성공") : TEXT("임무 실패");
        Body = Sim.Phase == EPhase::Won ? TEXT("마지막 신호가 안전하게 전송되었습니다.")
            : TEXT("신호가 끊겼습니다. 아래 전술 기록을 확인하고 즉시 재도전하십시오.");
    }

    const FLinearColor ResultColor = bResult && Sim.Phase == EPhase::Lost ? Red : Cyan;
    DrawLabel(Kicker, Card + FVector2D(32.f, 26.f) * UiScale, ResultColor, .53f);
    DrawLabel(Title, Card + FVector2D(32.f, 91.f) * UiScale, Ink, 1.03f, false, false, EOutpostFont::Large);
    DrawLabel(Body, Card + FVector2D(32.f, 145.f) * UiScale, Muted, .54f, false, false, EOutpostFont::Medium);

    if (bMenu)
    {
        DrawPanel(Card + FVector2D(30.f, 213.f) * UiScale, FVector2D(620.f, 263.f) * UiScale, Panel);
        DrawLabel(TEXT("FIELD MANUAL"), Card + FVector2D(48.f, 228.f) * UiScale, Cyan, .43f);
        DrawLabel(TEXT("이동"), Card + FVector2D(48.f, 261.f) * UiScale, Muted, .45f);
        DrawLabel(TEXT("WASD  /  우클릭 목적지"), Card + FVector2D(137.f, 261.f) * UiScale, Ink, .52f);
        DrawLabel(TEXT("도구"), Card + FVector2D(48.f, 295.f) * UiScale, Muted, .45f);
        DrawLabel(TEXT("1 블래스터  ·  2 곡괭이  ·  3 맨손/방벽"), Card + FVector2D(137.f, 295.f) * UiScale, Ink, .50f);
        DrawLabel(TEXT("행동"), Card + FVector2D(48.f, 329.f) * UiScale, Muted, .45f);
        DrawLabel(TEXT("LMB 사용  ·  E 채굴/화력/취소  ·  F 보호구"), Card + FVector2D(137.f, 329.f) * UiScale, Ink, .48f);
        DrawLabel(TEXT("전투"), Card + FVector2D(48.f, 363.f) * UiScale, Muted, .45f);
        DrawLabel(TEXT("SPACE 회피  ·  R 방벽 회전  ·  ENTER 조기 전투"), Card + FVector2D(137.f, 363.f) * UiScale, Ink, .47f);
        DrawLine(Card + FVector2D(48.f, 399.f) * UiScale, Card + FVector2D(632.f, 399.f) * UiScale, Hairline);
        DrawLabel(TEXT("전투 중에도 채굴·강화·방벽 재배치가 가능합니다."),
            Card + FVector2D(48.f, 416.f) * UiScale, Amber, .48f);
        DrawLabel(TEXT("작업 중에도 적은 공격합니다. E로 강화를 취소하면 광석이 반환됩니다.  ·  ESC 일시정지"),
            Card + FVector2D(48.f, 447.f) * UiScale, Muted, .40f);
    }

    float ButtonY = Card.Y + CardH - 76.f * UiScale;
    if (bResult)
    {
        const float Accuracy = Sim.Stats.Shots > 0 ? 100.f * static_cast<float>(Sim.Stats.Hits) / Sim.Stats.Shots : 0.f;
        const int32 Score = Sim.Score();
        const int32 Best = GameMode.BestScore(Sim.Mode);
        DrawLabel(FString::Printf(TEXT("SCORE  %06d"), Score), Card + FVector2D(32.f, 207.f) * UiScale, Amber, .72f);
        DrawLabel(FString::Printf(TEXT("BEST  %06d  ·  %s"), Best,
            Sim.Mode == Outpost::EMode::Hard ? TEXT("HARD") : TEXT("PRACTICE")),
            Card + FVector2D(350.f, 213.f) * UiScale, Sim.Mode == Outpost::EMode::Hard ? Red : Green, .47f);
        DrawPanel(Card + FVector2D(30.f, 250.f) * UiScale, FVector2D(590.f, 111.f) * UiScale, Panel);
        DrawLabel(TEXT("RUN STATISTICS"), Card + FVector2D(48.f, 264.f) * UiScale, Cyan, .42f);
        DrawLabel(FString::Printf(TEXT("명중률 %.0f%%   ·   피해 %.0f   ·   회피 %d   ·   전투 %.1fs"),
            Accuracy, Sim.Stats.DamageTaken, Sim.Stats.Dashes, Sim.CombatTime),
            Card + FVector2D(48.f, 294.f) * UiScale, Ink, .49f);
        DrawLabel(FString::Printf(TEXT("빌드  화력 %s / 보호구 %d / 방벽 %d   ·   전투 채굴 %d / 이동 %d"),
            *RankText(Sim.Rank), Sim.ArmorRank, Sim.Stats.WallMoves,
            Sim.Stats.CombatOreMined, Sim.Stats.CombatWallMoves),
            Card + FVector2D(48.f, 326.f) * UiScale, Muted, .45f);

        FString Tip;
        if (Sim.Stats.Dashes < 2) Tip = TEXT("TIP  공격 예고 원이 차오르면 SPACE로 옆으로 회피하십시오.");
        else if (Accuracy < 35.f) Tip = TEXT("TIP  가까운 적의 이동 방향을 보고 앞을 예측해 조준하십시오.");
        else if (Sim.Rank < 1) Tip = TEXT("TIP  전투 중 광석을 더 캐고 대장간에서 화력을 강화하십시오.");
        else Tip = TEXT("TIP  러너의 빠른 접근을 먼저 끊고, 보스 공격 원 밖으로 빠지십시오.");
        DrawLabel(Tip, Card + FVector2D(32.f, 385.f) * UiScale,
            Sim.Phase == EPhase::Lost ? Amber : Cyan, .46f);
    }

    if (bPause)
    {
        DrawButton(TEXT("계속"), TEXT("CONTINUE"), FVector2D(Card.X + 30.f * UiScale, ButtonY),
            FVector2D(155.f, 48.f) * UiScale, true, false, 50, .65f);
        DrawButton(TEXT("같은 모드 재시작"), TEXT("RESTART"), FVector2D(Card.X + 202.f * UiScale, ButtonY),
            FVector2D(174.f, 48.f) * UiScale, false, false, 50, .50f);
        DrawButton(TEXT("메뉴"), TEXT("MENU"), FVector2D(Card.X + 393.f * UiScale, ButtonY),
            FVector2D(137.f, 48.f) * UiScale, false, false, 50, .62f);
    }
    else if (bMenu)
    {
        DrawButton(TEXT("도전 시작  ·  HARD"), TEXT("START"), FVector2D(Card.X + 30.f * UiScale, ButtonY),
            FVector2D(302.f, 49.f) * UiScale, true, false, 50, .62f);
        DrawButton(TEXT("연습 시작  ·  PRACTICE"), TEXT("PRACTICE"), FVector2D(Card.X + 348.f * UiScale, ButtonY),
            FVector2D(302.f, 49.f) * UiScale, false, false, 50, .57f);
    }
    else
    {
        DrawButton(TEXT("빠른 재도전"), TEXT("RESTART"), FVector2D(Card.X + 30.f * UiScale, ButtonY),
            FVector2D(190.f, 48.f) * UiScale, true, false, 50, .62f);
        DrawButton(TEXT("모드 전환"), TEXT("SWITCH_MODE"), FVector2D(Card.X + 236.f * UiScale, ButtonY),
            FVector2D(178.f, 48.f) * UiScale, false, false, 50, .60f);
        DrawButton(TEXT("메뉴"), TEXT("MENU"), FVector2D(Card.X + 430.f * UiScale, ButtonY),
            FVector2D(190.f, 48.f) * UiScale, false, false, 50, .62f);
    }
}

void AOutpostHUD::NotifyHitBoxClick(FName BoxName)
{
    Super::NotifyHitBoxClick(BoxName);
    AOutpostGameMode* GameMode = GetOutpostGameMode();
    if (!GameMode)
    {
        return;
    }

    if (BoxName == TEXT("START")) GameMode->StartGame(Outpost::EMode::Hard);
    else if (BoxName == TEXT("PRACTICE")) GameMode->StartGame(Outpost::EMode::Practice);
    else if (BoxName == TEXT("RESTART")) GameMode->StartGame(GameMode->Sim.Mode);
    else if (BoxName == TEXT("SWITCH_MODE")) GameMode->StartGame(
        GameMode->Sim.Mode == Outpost::EMode::Hard ? Outpost::EMode::Practice : Outpost::EMode::Hard);
    else if (BoxName == TEXT("MENU")) GameMode->ReturnToMenu();
    else if (BoxName == TEXT("CONTINUE") || BoxName == TEXT("PAUSE")) GameMode->TogglePause();
    else if (BoxName == TEXT("TOOL_1")) GameMode->SelectTool(1);
    else if (BoxName == TEXT("TOOL_2")) GameMode->SelectTool(2);
    else if (BoxName == TEXT("TOOL_3")) GameMode->SelectTool(3);
    else if (BoxName == TEXT("FORGE")) GameMode->HandleInteract();
    else if (BoxName == TEXT("ARMOR")) GameMode->HandleArmor();
    else if (BoxName == TEXT("DASH")) GameMode->HandleDash();
    else if (BoxName == TEXT("SOUND")) GameMode->ToggleSound();
}
