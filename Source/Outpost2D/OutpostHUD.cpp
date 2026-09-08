#include "OutpostHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
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

void AOutpostHUD::DrawTexture(UTexture2D* Texture, const FVector2D& Position, const FVector2D& Size,
    const FVector2D& UV0, const FVector2D& UV1, const FLinearColor& Tint)
{
    if (!Texture || !Texture->GetResource())
    {
        return;
    }
    FCanvasTileItem Tile(Position, Texture->GetResource(), Size, UV0, UV1, Tint);
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
    const float H = 82.f * UiScale;
    DrawPanel(FVector2D::ZeroVector, FVector2D(ScreenWidth, H), FLinearColor(Navy.R, Navy.G, Navy.B, .96f));
    DrawPanel(FVector2D(0.f, H - 2.f * UiScale), FVector2D(ScreenWidth, 2.f * UiScale), Cyan * FLinearColor(1.f, 1.f, 1.f, .58f));

    const float Margin = 18.f * UiScale;
    DrawLabel(TEXT("OUTPOST"), FVector2D(Margin, 12.f * UiScale), Ink, .67f, false, false, EOutpostFont::Large);
    DrawLabel(TEXT("180"), FVector2D(Margin + 128.f * UiScale, 12.f * UiScale), Cyan,
        .67f, false, false, EOutpostFont::Large);
    DrawLabel(TEXT("LAST SIGNAL"), FVector2D(Margin, 49.f * UiScale), Muted, .42f);

    const float StatusX = 226.f * UiScale;
    DrawPanel(FVector2D(StatusX, 10.f * UiScale), FVector2D(270.f, 58.f) * UiScale,
        FLinearColor(Panel.R, Panel.G, Panel.B, .82f));
    DrawPanel(FVector2D(StatusX, 10.f * UiScale), FVector2D(3.f, 58.f) * UiScale, Amber);

    FLinearColor PhaseColor = Cyan;
    if (Sim.Phase == Outpost::EPhase::Combat) PhaseColor = Amber;
    if ((Sim.Phase == Outpost::EPhase::Prep && Sim.PrepRemaining <= 10.f) || Sim.Phase == Outpost::EPhase::Lost) PhaseColor = Red;
    DrawLabel(PhaseText(Sim), FVector2D(StatusX + 15.f * UiScale, 16.f * UiScale), PhaseColor, .59f);
    DrawLabel(FString::Printf(TEXT("WAVE %d / 3    KILLS %02d / %d"), Sim.Wave, Sim.Kills, Outpost::TotalEnemies),
        FVector2D(StatusX + 15.f * UiScale, 45.f * UiScale), Muted, .41f);

    const float StatsX = 510.f * UiScale;
    DrawPanel(FVector2D(StatsX, 10.f * UiScale), FVector2D(112.f, 58.f) * UiScale,
        FLinearColor(Panel.R, Panel.G, Panel.B, .82f));
    DrawLabel(TEXT("ORE"), FVector2D(StatsX + 13.f * UiScale, 16.f * UiScale), Muted, .38f);
    DrawLabel(FString::Printf(TEXT("%03d"), Sim.Ore), FVector2D(StatsX + 13.f * UiScale, 39.f * UiScale), Amber, .59f);
    DrawPanel(FVector2D(632.f, 10.f) * UiScale, FVector2D(244.f, 58.f) * UiScale,
        FLinearColor(Panel.R, Panel.G, Panel.B, .82f));
    DrawLabel(TEXT("SUIT HP"), FVector2D(645.f, 16.f) * UiScale, Muted, .38f);
    DrawLabel(FString::Printf(TEXT("%03d / %03d"), FMath::CeilToInt(Sim.PlayerHP), FMath::CeilToInt(Sim.MaxHP)),
        FVector2D(720.f, 15.f) * UiScale, Ink, .47f);
    DrawBar(FVector2D(645.f, 44.f) * UiScale, FVector2D(218.f, 7.f) * UiScale,
        Sim.PlayerHP / FMath::Max(1.f, Sim.MaxHP), Sim.PlayerHP < Sim.MaxHP * .3f ? Red : Cyan, PanelBright);
    DrawLabel(FString::Printf(TEXT("화력 %s"), *RankText(Sim.Rank)), FVector2D(886.f, 18.f) * UiScale, Cyan, .43f);
    DrawLabel(FString::Printf(TEXT("보호구 %s"), Sim.ArmorRank > 0 ? TEXT("I") : TEXT("—")),
        FVector2D(886.f, 45.f) * UiScale, Sim.ArmorRank > 0 ? Cyan : Muted, .41f);

    const FString ModeText = Sim.Mode == Outpost::EMode::Hard ? TEXT("HARD") : TEXT("PRACTICE");
    DrawLabel(ModeText, FVector2D(ScreenWidth - 205.f * UiScale, 18.f * UiScale),
        Sim.Mode == Outpost::EMode::Hard ? Red : Green, .43f);
    const FVector2D SoundPos(ScreenWidth - 124.f * UiScale, 42.f * UiScale);
    DrawButton(GameMode.bSound ? TEXT("SFX ON") : TEXT("SFX OFF"), TEXT("SOUND"), SoundPos,
        FVector2D(104.f, 28.f) * UiScale, false, GameMode.bSound, 20, .45f);
}

void AOutpostHUD::DrawFooter(AOutpostGameMode& GameMode)
{
    const Outpost::FSimulation& Sim = GameMode.Sim;
    const float H = 88.f * UiScale;
    const float Y = ScreenHeight - H;
    DrawPanel(FVector2D(0.f, Y), FVector2D(ScreenWidth, H), Navy);
    DrawPanel(FVector2D(0.f, Y), FVector2D(ScreenWidth, 2.f * UiScale), Cyan * FLinearColor(1.f, 1.f, 1.f, .58f));

    const float ToolW = 134.f * UiScale;
    const float Gap = 6.f * UiScale;
    const float ToolY = Y + 13.f * UiScale;
    const bool bCanChangeTool = Sim.CanWork() && Sim.JobRemaining <= 0.f && !Sim.bCarrying && Sim.DashRemaining <= 0.f;
    const float ToolsX = 18.f * UiScale;
    DrawButton(TEXT("1  블래스터"), TEXT("TOOL_1"), FVector2D(ToolsX, ToolY),
        FVector2D(ToolW, 40.f * UiScale), false, Sim.Tool == 1, 2, .53f, bCanChangeTool);
    DrawButton(TEXT("2  곡괭이"), TEXT("TOOL_2"), FVector2D(ToolsX + ToolW + Gap, ToolY),
        FVector2D(ToolW, 40.f * UiScale), false, Sim.Tool == 2, 2, .53f, bCanChangeTool);
    DrawButton(TEXT("3  방벽"), TEXT("TOOL_3"), FVector2D(ToolsX + (ToolW + Gap) * 2.f, ToolY),
        FVector2D(ToolW, 40.f * UiScale), false, Sim.Tool == 3, 2, .53f, bCanChangeTool);
    if (GameMode.ArtSprites && GameMode.ArtSprites->GetResource())
    {
        for (int32 ToolIndex = 0; ToolIndex < 3; ++ToolIndex)
        {
            const FVector2D IconPos(ToolsX + (ToolW + Gap) * ToolIndex + 8.f * UiScale, ToolY + 8.f * UiScale);
            const float V0 = static_cast<float>(ToolIndex) / 8.f;
            DrawTexture(GameMode.ArtSprites.Get(), IconPos, FVector2D(24.f) * UiScale,
                FVector2D(0.f, V0), FVector2D(1.f / 8.f, V0 + 1.f / 8.f));
        }
    }

    const bool bDashReady = Sim.Phase == Outpost::EPhase::Combat && Sim.DashCooldown <= 0.f
        && Sim.JobRemaining <= 0.f && !Sim.bCarrying && Sim.DashRemaining <= 0.f && !Sim.bPaused;
    FString DashText;
    if (Sim.bCarrying) DashText = TEXT("운반 중");
    else if (Sim.JobRemaining > 0.f) DashText = TEXT("작업 중");
    else if (Sim.DashRemaining > 0.f) DashText = TEXT("DASH");
    else if (Sim.Phase != Outpost::EPhase::Combat) DashText = TEXT("전투 전용");
    else if (Sim.DashCooldown <= 0.f) DashText = TEXT("SPACE  READY");
    else DashText = FString::Printf(TEXT("SPACE  %.1fs"), Sim.DashCooldown);
    const float DashX = ToolsX + (ToolW + Gap) * 3.f;
    DrawButton(DashText, TEXT("DASH"), FVector2D(DashX, ToolY), FVector2D(118.f, 40.f) * UiScale,
        false, bDashReady, 2, .43f, bDashReady);

    DrawLabel(TEXT("LMB 사용  ·  RMB 이동  ·  E 상호작용/취소  ·  F 보호구  ·  R 회전"),
        FVector2D(18.f * UiScale, Y + 64.f * UiScale), Muted, .39f);

    const float HintX = 600.f * UiScale;
    DrawLine(FVector2D(HintX - 20.f * UiScale, Y + 13.f * UiScale),
        FVector2D(HintX - 20.f * UiScale, Y + 73.f * UiScale), Hairline);
    DrawLabel(TEXT("TACTICAL FEED"), FVector2D(HintX, Y + 13.f * UiScale), Cyan, .40f);
    const FString Hint = GameMode.ToastRemaining > 0.f && !GameMode.Toast.IsEmpty() ? GameMode.Toast : Sim.Hint();
    const FString HintText = Hint.IsEmpty() ? TEXT("신호 기지를 방어할 준비를 하십시오.") : Hint;
    const float DefaultHintScale = .49f;
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
    DrawLabel(HintText, FVector2D(HintX, Y + 40.f * UiScale), Ink, HintScale);

    if (Sim.JobRemaining > 0.f)
    {
        const float Progress = 1.f - Sim.JobRemaining / Outpost::ForgeSeconds;
        const FLinearColor JobColor = Sim.JobKind == Outpost::EUpgradeKind::Armor ? Cyan : Amber;
        DrawBar(FVector2D(HintX, Y + 68.f * UiScale), FVector2D(330.f * UiScale, 4.f * UiScale),
            Progress, JobColor, PanelBright);
    }
    else if (Sim.bMining)
    {
        DrawBar(FVector2D(HintX, Y + 68.f * UiScale), FVector2D(330.f * UiScale, 4.f * UiScale),
            Sim.MineProgress / Outpost::MineSeconds, Cyan, PanelBright);
    }

    DrawButton(Sim.bPaused ? TEXT("계속") : TEXT("II  일시정지"), TEXT("PAUSE"),
        FVector2D(ScreenWidth - 124.f * UiScale, ToolY), FVector2D(106.f, 40.f) * UiScale,
        false, Sim.bPaused, 20, .45f);
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
    if (Sim.HurtRemaining > 0.f)
    {
        DrawPanel(Player + FVector2D(-25.f, -62.f) * UiScale, FVector2D(50.f, 17.f) * UiScale,
            Srgb(38, 4, 9, 220));
        DrawLabel(TEXT("피격"), Player + FVector2D(0.f, -53.5f) * UiScale,
            Red, .43f, true, true, EOutpostFont::Small);
    }

    const int32 NearbyMine = Sim.NearestMine();
    for (int32 MineIndex = 0; MineIndex < Sim.Mines.Num(); ++MineIndex)
    {
        const Outpost::FMine& Mine = Sim.Mines[MineIndex];
        if (Mine.Remaining <= 0)
        {
            continue;
        }
        if (MineIndex != NearbyMine && Mine.Rect.Id != Sim.ActiveMine)
        {
            continue;
        }
        const FVector2D P = GameMode.ScreenPoint(Mine.Rect.Center());
        DrawPanel(P + FVector2D(-34.f, -38.f) * UiScale, FVector2D(68.f, 16.f) * UiScale,
            FLinearColor(Navy.R, Navy.G, Navy.B, .72f));
        DrawLabel(FString::Printf(TEXT("광석 %03d"), Mine.Remaining), P + FVector2D(0.f, -30.f) * UiScale,
            Amber, .43f, true, true, EOutpostFont::Small);
    }

    const FVector2D Forge = GameMode.ScreenPoint(Sim.Forge.Center());
    const bool bForgeJob = Sim.JobRemaining > 0.f;
    const bool bForgeActive = bForgeJob || (Sim.CanWork() && Sim.NearForge() && Sim.DashRemaining <= 0.f && !Sim.bCarrying);
    const FVector2D ForgePanelSize = bForgeActive ? FVector2D(200.f, 20.f) : FVector2D(58.f, 15.f);
    DrawPanel(Forge + FVector2D(-ForgePanelSize.X * .5f, -54.f) * UiScale, ForgePanelSize * UiScale,
        FLinearColor(Navy.R, Navy.G, Navy.B, bForgeActive ? .84f : .55f));
    FString ForgeText;
    FLinearColor ForgeColor = Amber;
    if (bForgeJob)
    {
        ForgeColor = Sim.JobKind == Outpost::EUpgradeKind::Armor ? Cyan : Amber;
        ForgeText = FString::Printf(TEXT("%s 제작 %.1fs  ·  E 취소"),
            Sim.JobKind == Outpost::EUpgradeKind::Armor ? TEXT("보호구") : TEXT("화력"), Sim.JobRemaining);
    }
    else if (bForgeActive)
    {
        const FString FireCost = Sim.Rank >= Outpost::MaxRank ? TEXT("MAX") : FString::FromInt(Sim.UpgradeCost());
        const FString ArmorCost = Sim.ArmorRank > 0 ? TEXT("MAX") : TEXT("10");
        ForgeText = FString::Printf(TEXT("E 화력 %s  ·  F 보호구 %s  ·  제작 5초"), *FireCost, *ArmorCost);
    }
    else
    {
        ForgeText = TEXT("FORGE");
        ForgeColor = Muted;
    }
    DrawLabel(ForgeText, Forge + FVector2D(0.f, -44.f) * UiScale, ForgeColor,
        bForgeActive ? .46f : .36f, true, true, EOutpostFont::Small);
    if (!bForgeJob && Sim.CanWork() && Sim.NearForge() && Sim.DashRemaining <= 0.f && !Sim.bCarrying)
    {
        DrawLine(Forge + FVector2D(0.f, -54.f) * UiScale, Forge + FVector2D(0.f, -34.f) * UiScale, Hairline);
        const bool bFireAvailable = Sim.Rank < Outpost::MaxRank && Sim.Ore >= Sim.UpgradeCost();
        const bool bArmorAvailable = Sim.ArmorRank <= 0 && Sim.Ore >= 10;
        if (bFireAvailable)
        {
            AddHitBox(Forge + FVector2D(-100.f, -54.f) * UiScale,
                FVector2D(99.f, 20.f) * UiScale, TEXT("FORGE"), true, 4);
        }
        if (bArmorAvailable)
        {
            AddHitBox(Forge + FVector2D(1.f, -54.f) * UiScale,
                FVector2D(99.f, 20.f) * UiScale, TEXT("ARMOR"), true, 4);
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

    if (bMenu)
    {
        // The key art is composed with its action on the right. A dense, opaque rail on
        // the left keeps the Korean copy readable while leaving most of the illustration intact.
        const float MenuW = 1180.f * UiScale;
        const float MenuH = 700.f * UiScale;
        const FVector2D Menu((ScreenWidth - MenuW) * .5f, (ScreenHeight - MenuH) * .5f);
        DrawPanel(Menu - FVector2D(2.f * UiScale), FVector2D(MenuW, MenuH) + FVector2D(4.f * UiScale),
            FLinearColor(Cyan.R, Cyan.G, Cyan.B, .30f));
        DrawPanel(Menu, FVector2D(MenuW, MenuH), NavySoft);
        if (GameMode.ArtKeyArt && GameMode.ArtKeyArt->GetResource())
        {
            DrawTexture(GameMode.ArtKeyArt.Get(), Menu, FVector2D(MenuW, MenuH),
                FVector2D(0.f, .055f), FVector2D(1.f, .945f), FLinearColor(.84f, .90f, .94f, 1.f));
        }
        else
        {
            DrawPanel(Menu + FVector2D(520.f, 0.f) * UiScale, FVector2D(660.f, 700.f) * UiScale,
                Srgb(7, 25, 35));
            for (int32 I = 0; I < 8; ++I)
            {
                DrawLine(Menu + FVector2D(560.f + I * 88.f, 0.f) * UiScale,
                    Menu + FVector2D(560.f + I * 88.f, 700.f) * UiScale,
                    FLinearColor(Cyan.R, Cyan.G, Cyan.B, .055f));
            }
        }

        const float RailW = 520.f;
        DrawPanel(Menu, FVector2D(RailW, 700.f) * UiScale, FLinearColor(Navy.R, Navy.G, Navy.B, .975f));
        for (int32 I = 0; I < 10; ++I)
        {
            const float Alpha = .88f * (1.f - static_cast<float>(I) / 10.f);
            DrawPanel(Menu + FVector2D(RailW + I * 13.f, 0.f) * UiScale, FVector2D(13.f, 700.f) * UiScale,
                FLinearColor(Navy.R, Navy.G, Navy.B, Alpha));
        }
        DrawPanel(Menu + FVector2D(520.f, 570.f) * UiScale, FVector2D(660.f, 130.f) * UiScale,
            FLinearColor(Navy.R, Navy.G, Navy.B, .67f));
        DrawPanel(Menu, FVector2D(4.f, 700.f) * UiScale, Cyan);

        const FVector2D Copy = Menu + FVector2D(34.f, 0.f) * UiScale;
        DrawLabel(TEXT("SECTOR 01  /  LAST TRANSMISSION"), Copy + FVector2D(0.f, 31.f) * UiScale,
            Cyan, .43f);
        DrawLabel(TEXT("OUTPOST"), Copy + FVector2D(0.f, 68.f) * UiScale,
            Ink, 1.04f, false, false, EOutpostFont::Large);
        DrawLabel(TEXT("180"), Copy + FVector2D(199.f, 68.f) * UiScale,
            Cyan, 1.04f, false, false, EOutpostFont::Large);
        DrawLabel(TEXT("최후의 신호"), Copy + FVector2D(2.f, 122.f) * UiScale,
            Amber, .61f);
        DrawLine(Copy + FVector2D(0.f, 162.f) * UiScale, Copy + FVector2D(450.f, 162.f) * UiScale,
            Hairline, 1.f);

        DrawLabel(TEXT("30초 동안 준비하고 세 번의 공세를 버티십시오."),
            Copy + FVector2D(0.f, 185.f) * UiScale, Ink, .52f);
        DrawLabel(TEXT("광석 채굴과 장비 강화, 이동식 방벽이 생존 수단입니다."),
            Copy + FVector2D(0.f, 216.f) * UiScale, Muted, .43f);
        DrawLabel(TEXT("3 WAVE 이후 최종 보스가 진입합니다."),
            Copy + FVector2D(0.f, 244.f) * UiScale, Red, .43f);

        DrawLabel(TEXT("BEST RECORD"), Copy + FVector2D(0.f, 286.f) * UiScale, Cyan, .39f);
        DrawPanel(Copy + FVector2D(0.f, 312.f) * UiScale, FVector2D(216.f, 62.f) * UiScale,
            FLinearColor(Panel.R, Panel.G, Panel.B, .86f));
        DrawPanel(Copy + FVector2D(230.f, 312.f) * UiScale, FVector2D(216.f, 62.f) * UiScale,
            FLinearColor(Panel.R, Panel.G, Panel.B, .86f));
        DrawLabel(TEXT("HARD"), Copy + FVector2D(14.f, 321.f) * UiScale, Red, .37f);
        DrawLabel(FString::Printf(TEXT("%06d"), GameMode.BestScore(Outpost::EMode::Hard)),
            Copy + FVector2D(14.f, 344.f) * UiScale, Ink, .53f);
        DrawLabel(TEXT("PRACTICE"), Copy + FVector2D(244.f, 321.f) * UiScale, Green, .37f);
        DrawLabel(FString::Printf(TEXT("%06d"), GameMode.BestScore(Outpost::EMode::Practice)),
            Copy + FVector2D(244.f, 344.f) * UiScale, Ink, .53f);

        DrawLabel(TEXT("조작"), Copy + FVector2D(0.f, 402.f) * UiScale, Cyan, .40f);
        DrawLabel(TEXT("WASD / RMB 이동    ·    1 / 2 / 3 도구"),
            Copy + FVector2D(0.f, 430.f) * UiScale, Ink, .43f);
        DrawLabel(TEXT("LMB 사용    ·    E 상호작용 / 취소    ·    F 보호구"),
            Copy + FVector2D(0.f, 458.f) * UiScale, Ink, .41f);
        DrawLabel(TEXT("SPACE 회피    ·    R 방벽 회전    ·    ESC 일시정지"),
            Copy + FVector2D(0.f, 486.f) * UiScale, Muted, .40f);

        DrawLabel(TEXT("모드 선택"), Copy + FVector2D(0.f, 531.f) * UiScale, Cyan, .40f);
        DrawButton(TEXT("도전 시작  /  HARD"), TEXT("START"),
            Copy + FVector2D(0.f, 557.f) * UiScale, FVector2D(216.f, 51.f) * UiScale,
            true, false, 50, .52f);
        DrawButton(TEXT("연습 시작  /  PRACTICE"), TEXT("PRACTICE"),
            Copy + FVector2D(230.f, 557.f) * UiScale, FVector2D(216.f, 51.f) * UiScale,
            false, false, 50, .47f);
        DrawButton(GameMode.bSound ? TEXT("SFX  ON") : TEXT("SFX  OFF"), TEXT("SOUND"),
            Copy + FVector2D(0.f, 627.f) * UiScale, FVector2D(116.f, 38.f) * UiScale,
            false, GameMode.bSound, 50, .43f);
        DrawLabel(TEXT("준비 중 ENTER를 누르면 즉시 전투가 시작됩니다."),
            Copy + FVector2D(136.f, 638.f) * UiScale, Muted, .36f);

        DrawLabel(TEXT("30 SEC PREP"), Menu + FVector2D(550.f, 610.f) * UiScale, Cyan, .41f);
        DrawLabel(TEXT("03 WAVES"), Menu + FVector2D(748.f, 610.f) * UiScale, Amber, .41f);
        DrawLabel(TEXT("FINAL BOSS"), Menu + FVector2D(916.f, 610.f) * UiScale, Red, .41f);
        DrawLabel(TEXT("광석 · 강화 · 이동식 방벽"), Menu + FVector2D(550.f, 649.f) * UiScale, Ink, .47f);
        return;
    }

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
