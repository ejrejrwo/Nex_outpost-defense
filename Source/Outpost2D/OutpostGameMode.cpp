#include "OutpostGameMode.h"
#include "OutpostHUD.h"
#include "OutpostPlayerController.h"
#include "OutpostSaveGame.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "InputKeyEventArgs.h"

using namespace Outpost;
namespace
{
const FLinearColor Cyan(.15f, .9f, .85f), Amber(1.f, .65f, .2f);
FVector Position(FVector2D P, float Z) { return FVector(P.X, P.Y, Z); }
}
AOutpostGameMode::AOutpostGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = nullptr;
    PlayerControllerClass = AOutpostPlayerController::StaticClass();
    HUDClass = AOutpostHUD::StaticClass();
}
void AOutpostGameMode::BeginPlay()
{
    Super::BeginPlay();
    Records = Cast<UOutpostSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("OutpostNative2"), 0));
    if (!Records) Records = Cast<UOutpostSaveGame>(UGameplayStatics::CreateSaveGameObject(UOutpostSaveGame::StaticClass()));
    if (Records) bSound = Records->bSound;
    for (const TCHAR* Name : { TEXT("shot"), TEXT("mine"), TEXT("forge"), TEXT("upgrade"), TEXT("lift"), TEXT("place"), TEXT("hit"), TEXT("hurt"), TEXT("wave"), TEXT("win"), TEXT("lose") })
        Sounds.Add(LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("/Game/Outpost/Audio/S_%s.S_%s"), Name, Name)));
    ArenaCamera = GetWorld()->SpawnActor<ACameraActor>(FVector(Width * .5f, Height * .5f, 1500), FRotator(-90, -90, 0));
    UCameraComponent* Camera = ArenaCamera->GetCameraComponent();
    Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
    Camera->bConstrainAspectRatio = false;
    Camera->bOverrideAspectRatioAxisConstraint = true;
    Camera->AspectRatioAxisConstraint = AspectRatio_MaintainXFOV;
    Camera->bAutoCalculateOrthoPlanes = false;
    Camera->OrthoNearClipPlane = 1; Camera->OrthoFarClipPlane = 5000;
    Camera->OrthoWidth = 1320;
    Camera->PostProcessSettings.bOverride_VignetteIntensity = true;
    Camera->PostProcessSettings.VignetteIntensity = .2f;
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetViewTarget(ArenaCamera);
    bAutomation = FParse::Param(FCommandLine::Get(), TEXT("OutpostAutoTest"));
    bAutoPractice = FParse::Param(FCommandLine::Get(), TEXT("OutpostPractice"));
    bCapture = FParse::Param(FCommandLine::Get(), TEXT("OutpostCapture"));
    bPreview = FParse::Param(FCommandLine::Get(), TEXT("OutpostPreview"));
    bCapture |= bPreview;
    FParse::Value(FCommandLine::Get(), TEXT("OutpostRank="), AutoRank);
}
void AOutpostGameMode::StartGame(EMode Mode)
{
    Sim.Reset(Mode); Particles.Reset(); Rings.Reset(); bDashQueued = false; Accumulator = 0;
    WaveBannerRemaining = 0; ProcessEvents();
}
void AOutpostGameMode::ReturnToMenu()
{
    StartGame(Sim.Mode); Sim.Phase = EPhase::Menu; ToastRemaining = 0;
}
int32 AOutpostGameMode::BestScore(EMode Mode) const
{
    return Records ? (Mode == EMode::Practice ? Records->PracticeBest : Records->HardBest) : 0;
}
void AOutpostGameMode::SaveRecords()
{
    // Automated runs exercise gameplay without polluting the player's records.
    if (Records && !bAutomation) UGameplayStatics::SaveGameToSlot(Records, TEXT("OutpostNative2"), 0);
}
void AOutpostGameMode::ToggleSound()
{
    bSound = !bSound; if (Records) Records->bSound = bSound; SaveRecords();
}
void AOutpostGameMode::TogglePause()
{
    if (Sim.Phase == EPhase::Prep || Sim.Phase == EPhase::Combat || Sim.Phase == EPhase::Rest) Sim.bPaused = !Sim.bPaused;
}
void AOutpostGameMode::SelectTool(int32 Tool) { Sim.SetTool(Tool); }
void AOutpostGameMode::HandlePrimaryClick()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController(); float X, Y;
    if (!PC || !PC->GetMousePosition(X, Y) || !IsGameplayPoint(FVector2D(X, Y)) || Sim.bPaused) return;
    Sim.Aim = MouseWorld();
    if (Sim.Tool == 3 && Sim.CanWork()) { if (Sim.bCarrying) Sim.PlaceWall(Sim.Aim); else Sim.PickupWall(); }
}
void AOutpostGameMode::HandleSecondaryClick()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController(); float X, Y;
    if (PC && PC->GetMousePosition(X, Y) && IsGameplayPoint(FVector2D(X, Y))) Sim.MoveTo(MouseWorld());
}
void AOutpostGameMode::HandleInteract() { Sim.Aim = MouseWorld(); Sim.Interact(); }
void AOutpostGameMode::HandleRotate() { Sim.RotateWall(); }
void AOutpostGameMode::HandleArmor() { Sim.StartUpgrade(EUpgradeKind::Armor); }
void AOutpostGameMode::HandleDash() { if (Sim.CanWork()) bDashQueued = true; }
void AOutpostGameMode::HandleEnter()
{
    if (Sim.Phase == EPhase::Menu || Sim.Phase == EPhase::Won || Sim.Phase == EPhase::Lost) StartGame(Sim.Mode);
    else if (Sim.bPaused) TogglePause(); else Sim.BeginCombat();
}
FVector2D AOutpostGameMode::MouseWorld() const
{
    if (bAutomation) return AutoAim;
    const APlayerController* PC = GetWorld()->GetFirstPlayerController(); float X, Y;
    if (PC && PC->GetMousePosition(X, Y))
    {
        const FVector2D P = WorldPoint(FVector2D(X, Y));
        return FVector2D(FMath::Clamp(P.X, 0., double(Width)), FMath::Clamp(P.Y, 0., double(Height)));
    }
    return Sim.Aim;
}
FVector2D AOutpostGameMode::ScreenPoint(FVector2D Point) const
{
    FVector2D Screen = FVector2D::ZeroVector;
    if (const APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->ProjectWorldLocationToScreen(Position(Point, 20), Screen);
    return Screen;
}
FVector2D AOutpostGameMode::WorldPoint(FVector2D Screen) const
{
    // The arena is an axis-aligned orthographic plane. Invert the same projected
    // basis used by the HUD, including viewport resize and camera shake.
    const FVector2D Origin = ScreenPoint(FVector2D::ZeroVector);
    const FVector2D Extent = ScreenPoint(FVector2D(Width, Height)) - Origin;
    if (FMath::Abs(Extent.X) < .001 || FMath::Abs(Extent.Y) < .001) return Sim.Player;
    return FVector2D((Screen.X - Origin.X) * Width / Extent.X, (Screen.Y - Origin.Y) * Height / Extent.Y);
}
bool AOutpostGameMode::IsGameplayPoint(FVector2D Screen) const
{
    int32 W = 1280, H = 800; if (const APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->GetViewportSize(W, H);
    const float Scale = FMath::Clamp(FMath::Min(W / 1280.f, H / 800.f), .35f, 1.75f);
    return Screen.Y > 100 * Scale && Screen.Y < H - 104 * Scale;
}
FInput AOutpostGameMode::GatherInput()
{
    FInput Input; Input.Aim = MouseWorld();
    Input.bDash = bDashQueued; bDashQueued = false;
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        Input.Move.X = int(PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) - int(PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left));
        Input.Move.Y = int(PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down)) - int(PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up));
        float X, Y; Input.bUse = PC->IsInputKeyDown(EKeys::LeftMouseButton) && PC->GetMousePosition(X, Y) && IsGameplayPoint(FVector2D(X, Y));
    }
    return Input;
}
void AOutpostGameMode::ProcessEvents()
{
    for (const Outpost::FEvent& Event : Sim.Events)
    {
        if (Event.Type == EEvent::Message) { Toast = Event.Text; ToastRemaining = 3.5f; continue; }
        int32 Sound = -1, Count = 0; FLinearColor Color = Cyan;
        switch (Event.Type)
        {
        case EEvent::Shot: Sound = 0; Count = 2; Color = Amber; break;
        case EEvent::Mine: Sound = 1; Count = 7; break;
        case EEvent::Forge: Sound = 2; Count = 12; Color = Amber; break;
        case EEvent::Upgrade: Sound = 3; Count = 24; Color = Amber; break;
        case EEvent::Lift: Sound = 4; break;
        case EEvent::Place: Sound = 5; Count = 10; break;
        case EEvent::Hit: Sound = 6; Count = 5; Color = FLinearColor(1, .15f, .08f); break;
        case EEvent::Kill: Count = 16; Color = FLinearColor(.9f, .2f, .1f); break;
        case EEvent::Hurt: Sound = 7; Count = 14; CameraShake = .18f; Color = FLinearColor(1, .2f, .1f); break;
        case EEvent::Wave: Sound = 8; WaveBannerRemaining = 2.4f; break;
        case EEvent::Win: Sound = 9; break;
        case EEvent::Lose: Sound = 10; break;
        case EEvent::Dash: Sound = 4; Count = 16; Rings.Add({Event.Pos, Cyan, .3f, .3f, 35}); break;
        case EEvent::Strike: Count = 10; Color = FLinearColor(1, .18f, .3f); Rings.Add({Event.Pos, Color, .28f, .28f, Event.Radius}); break;
        case EEvent::Boss: Sound = 8; WaveBannerRemaining = 3; break;
        default: break;
        }
        if ((Event.Type == EEvent::Win || Event.Type == EEvent::Lose) && Records)
        {
            int32& Best = Sim.Mode == EMode::Practice ? Records->PracticeBest : Records->HardBest;
            Best = FMath::Max(Best, Sim.Score()); SaveRecords();
        }
        if (bSound && Sounds.IsValidIndex(Sound) && Sounds[Sound]) UGameplayStatics::PlaySound2D(this, Sounds[Sound], Sound == 0 || Sound == 6 ? .18f : .35f);
        for (int32 I = 0; I < Count; ++I)
        {
            const float A = FMath::FRandRange(0, 2 * PI), Life = FMath::FRandRange(.18f, .48f);
            Particles.Add({ Event.Pos, FVector2D(FMath::Cos(A), FMath::Sin(A)) * FMath::FRandRange(18.f, 90.f), Color, Life, Life, FMath::FRandRange(2.f, 4.f) });
        }
    }
    Sim.Events.Reset();
}
void AOutpostGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    int32 W = 1280, H = 800; if (const APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->GetViewportSize(W, H);
    const float Scale = FMath::Clamp(FMath::Min(W / 1280.f, H / 800.f), .35f, 1.75f);
    const float WorldScale = FMath::Max(.1f, FMath::Min((W - 60 * Scale) / Width, (H - 216 * Scale) / Height));
    if (ArenaCamera)
    {
        ArenaCamera->GetCameraComponent()->SetOrthoWidth(W / WorldScale);
        CameraShake = FMath::Max(0.f, CameraShake - DeltaSeconds);
        ArenaCamera->SetActorLocation(FVector(Width * .5f + FMath::Sin(Sim.Time * 120) * CameraShake * 12, Height * .5f, 1500));
    }
    const float PreviousTime = Sim.Time;
    const float Dt = FMath::Min(DeltaSeconds, .1f); Accumulator += Dt; CaptureClock += Dt;
    if (bAutomation && !bInputChecked && CaptureClock > 2)
    {
        InputStepClock += Dt;
        if (InputStepClock >= .05f) { CheckInput(); InputStepClock = 0; }
    }
    while (Accumulator >= 1.f / 60.f)
    {
        const FInput Input = bAutomation && bInputChecked ? AutomationInput(1.f / 60.f) : GatherInput();
        Sim.Tick(1.f / 60.f, Input); Accumulator -= 1.f / 60.f;
    }
    ProcessEvents();
    if (!Sim.bPaused)
    {
        ToastRemaining = FMath::Max(0.f, ToastRemaining - Dt);
        WaveBannerRemaining = FMath::Max(0.f, WaveBannerRemaining - Dt);
        for (FOutpostRing& Ring : Rings) Ring.Life -= Dt;
        Rings.RemoveAll([](const FOutpostRing& Ring) { return Ring.Life <= 0; });
        for (FOutpostParticle& P : Particles) { P.Life -= Dt; P.Pos += P.Velocity * Dt; P.Velocity *= FMath::Max(0.f, 1.f - Dt * 5); }
        Particles.RemoveAll([](const FOutpostParticle& P) { return P.Life <= 0; });
    }
    if (bCapture && !bAutoFinished)
    {
        if (CaptureClock >= 1.5f && CaptureClock - Dt < 1.5f) CaptureFrame(TEXT("01-menu"));
        if (Sim.Time >= 8 && PreviousTime < 8) CaptureFrame(TEXT("02-preparation"));
        if (Sim.Time >= 19 && PreviousTime < 19) CaptureFrame(TEXT("02-build"));
        if (Sim.Time >= 64 && PreviousTime < 64) CaptureFrame(TEXT("03-combat"));
        if (!bBossCaptured) for (const FEnemy& E : Sim.Enemies)
            if (E.Kind == EEnemyKind::Boss && E.AttackRemaining > .2f) { bBossCaptured = true; CaptureFrame(TEXT("03-boss")); break; }
    }
    if (bAutomation && !bAutoFinished && (Sim.Phase == EPhase::Won || Sim.Phase == EPhase::Lost || Sim.Time > 200)) FinishAutomation();
    if (bAutoFinished && CaptureClock > 3) FPlatformMisc::RequestExit(false);
    if (bPreview && CaptureClock > 4) FPlatformMisc::RequestExit(false);
}
void AOutpostGameMode::CheckInput()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;
    auto Key = [PC](FKey Value, EInputEvent Event) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(Value, Event, Event == IE_Released ? 0.f : 1.f)); };
    auto Check = [this](bool Pass, const TCHAR* Name)
    {
        if (!Pass) ++InputFailures;
        UE_LOG(LogTemp, Display, TEXT("OUTPOST_INPUT %s=%s"), Name, Pass ? TEXT("PASS") : TEXT("FAIL"));
    };
    switch (InputStep++)
    {
    case 0:
        if (AOutpostHUD* HUD = Cast<AOutpostHUD>(PC->GetHUD())) HUD->NotifyHitBoxClick(TEXT("START"));
        Check(Sim.Phase == EPhase::Prep, TEXT("HUD_START"));
        Key(EKeys::One, IE_Pressed); break;
    case 1: Check(Sim.Tool == 1, TEXT("KEY_1")); Key(EKeys::One, IE_Released); Key(EKeys::Two, IE_Pressed); break;
    case 2: Check(Sim.Tool == 2, TEXT("KEY_2")); Key(EKeys::Two, IE_Released); Key(EKeys::Three, IE_Pressed); break;
    case 3: Check(Sim.Tool == 3, TEXT("KEY_3")); Key(EKeys::Three, IE_Released); InputPosition = Sim.Player; Key(EKeys::D, IE_Pressed); break;
    case 4: break;
    case 5: Check(Sim.Player.X > InputPosition.X, TEXT("MOVE_D")); Key(EKeys::D, IE_Released); Key(EKeys::Escape, IE_Pressed); break;
    case 6: Check(Sim.bPaused, TEXT("ESC_PAUSE")); Key(EKeys::Escape, IE_Released); break;
    case 7: Key(EKeys::Escape, IE_Pressed); break;
    case 8: Check(!Sim.bPaused, TEXT("ESC_RESUME")); Key(EKeys::Escape, IE_Released); break;
    case 9:
    {
        const FVector2D Screen = ScreenPoint(FVector2D(504, 306)); FVector Origin, Direction;
        const bool bProjected = PC->DeprojectScreenPositionToWorld(Screen.X, Screen.Y, Origin, Direction);
        const FVector Hit = Origin + Direction * ((20 - Origin.Z) / (FMath::Abs(Direction.Z) > .001 ? Direction.Z : 1));
        UE_LOG(LogTemp, Display, TEXT("OUTPOST_ENGINE_RAY valid=%d screen=%s hit=%s"), bProjected, *Screen.ToString(), *Hit.ToString());
        bool bRoundTrip = true;
        for (FVector2D P : { FVector2D(54, 306), FVector2D(900, 500), FVector2D(220, 180), FVector2D(504, 306) })
            bRoundTrip &= FVector2D::Distance(P, WorldPoint(ScreenPoint(P))) < .5;
        Check(bRoundTrip, TEXT("PROJECTION"));
        Sim.Reset(); Sim.Player = FSimulation::Center(FIntPoint(12, 3)); Sim.Ore = 20;
        Key(EKeys::F, IE_Pressed); break;
    }
    case 10: Check(Sim.JobRemaining > 0 && Sim.JobKind == EUpgradeKind::Armor, TEXT("F_ARMOR")); Key(EKeys::F, IE_Released); Key(EKeys::E, IE_Pressed); break;
    case 11: Check(Sim.JobRemaining == 0 && Sim.Ore == 20, TEXT("E_CANCEL_REFUND")); Key(EKeys::E, IE_Released); Sim.BeginCombat(); Key(EKeys::Two, IE_Pressed); break;
    case 12: Check(Sim.Tool == 2, TEXT("COMBAT_TOOL_2")); Key(EKeys::Two, IE_Released); Key(EKeys::SpaceBar, IE_Pressed); break;
    case 13: Check(Sim.Stats.Dashes == 1, TEXT("SPACE_DASH")); Key(EKeys::SpaceBar, IE_Released); break;
    case 14: break;
    case 15:
        if (Sim.DashRemaining > 0) { --InputStep; break; }
        Key(EKeys::Three, IE_Pressed); break;
    case 16:
        Check(Sim.Tool == 3, TEXT("COMBAT_TOOL_3")); Key(EKeys::Three, IE_Released);
        FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"failed\":%d,\"checks\":13}"), InputFailures), *(FPaths::ProjectSavedDir() / TEXT("input-check.json")));
        bInputChecked = true; StartGame(bAutoPractice ? EMode::Practice : EMode::Hard); break;
    }
}
FInput AOutpostGameMode::AutomationInput(float Dt)
{
    FInput Input; Input.Aim = AutoAim;
    if (Sim.Phase == EPhase::Menu) { if (CaptureClock > 2) StartGame(); return Input; }
    if (Sim.Phase == EPhase::Prep)
    {
        switch (AutoStep)
        {
        case 0: Sim.SetTool(2); Sim.MoveTo(Sim.Mines[0].Rect.Center()); ++AutoStep; break;
        case 1: if (Sim.MoveOrder.IsEmpty()) { Sim.Interact(); ++AutoStep; } break;
        case 2: if (Sim.Ore >= (AutoRank == 2 ? 24 : 10)) { Sim.Interact(); Sim.MoveTo(Sim.Forge.Center()); ++AutoStep; } break;
        case 3: if (Sim.MoveOrder.IsEmpty()) { Sim.StartUpgrade(); ++AutoStep; } break;
        case 4: if (Sim.Rank >= 1) { if (AutoRank == 2) Sim.StartUpgrade(); ++AutoStep; } break;
        case 5: if (Sim.Rank >= AutoRank) { if (AutoRank == 1) { Sim.SetTool(3); Sim.MoveTo(FSimulation::Center(FIntPoint(8, 8))); } else Sim.MoveTo(FSimulation::Center(FIntPoint(14, 10))); ++AutoStep; } break;
        case 6: if (Sim.MoveOrder.IsEmpty()) { if (AutoRank == 1) { Sim.PickupWall(); Sim.RotateWall(); Sim.PlaceWall(FSimulation::Center(FIntPoint(9, 6))); Sim.MoveTo(FSimulation::Center(FIntPoint(14, 10))); } ++AutoStep; } break;
        }
    }
    else if (Sim.Phase == EPhase::Combat)
    {
        float Nearest = TNumericLimits<float>::Max();
        for (const FEnemy& E : Sim.Enemies) if (FVector2D::DistSquared(Sim.Player, E.Pos) < Nearest) { Nearest = FVector2D::DistSquared(Sim.Player, E.Pos); AutoAim = E.Pos; }
        Input.Aim = AutoAim; Input.bUse = true;
        // A deterministic test controller: normal movement, ammunition, HP and dash rules.
        // It is never active without the explicit automation launch flag.
        const FVector2D FromCenter = Sim.Player - FSimulation::Center(FIntPoint(14, 8));
        const FVector2D Radial = FromCenter.GetSafeNormal(), Tangent(-Radial.Y, Radial.X);
        FVector2D Repel = FVector2D::ZeroVector;
        for (const FEnemy& E : Sim.Enemies)
        {
            const FVector2D Away = Sim.Player - E.Pos; const float Distance = Away.Size();
            if (Distance < 330) Repel += Away.GetSafeNormal() * FMath::Square((330 - Distance) / 330.f);
        }
        const float Radius = FromCenter.Size();
        FVector2D Desired = Tangent * .85 + Radial * (Radius < 205 ? .75 : Radius > 250 ? -.75 : 0) + Repel * 2.4;
        const double Angle = FMath::Atan2(Desired.Y, Desired.X);
        for (double Offset : { 0., .35, -.35, .7, -.7, 1.05, -1.05, double(PI) })
        {
            const FVector2D Direction(FMath::Cos(Angle + Offset), FMath::Sin(Angle + Offset));
            if (Sim.ClearTravel(Sim.Player, Sim.Player + Direction * 36, 11))
            {
                Input.Move = Direction; break;
            }
        }
        bool bThreat = Nearest < 80 * 80;
        for (const FEnemy& E : Sim.Enemies) if (E.AttackRemaining > 0 && E.AttackRemaining < .22f && FVector2D::Distance(Sim.Player, E.AttackPoint) < E.AttackRadius + 11) bThreat = true;
        Input.bDash = bThreat && Sim.DashCooldown <= 0 && Sim.DashRemaining <= 0;
    }
    return Input;
}
void AOutpostGameMode::CaptureFrame(const FString& Name)
{
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Screenshots"); IFileManager::Get().MakeDirectory(*Directory, true);
    FScreenshotRequest::RequestScreenshot(Directory / FString::Printf(TEXT("%s-%s-rank%d.png"), *Name, bAutoPractice ? TEXT("practice") : TEXT("hard"), AutoRank), true, false);
}
void AOutpostGameMode::FinishAutomation()
{
    bAutoFinished = true; CaptureClock = 0;
    const FString Report = FString::Printf(TEXT("{\"won\":%s,\"mode\":\"%s\",\"rank\":%d,\"hp\":%.0f,\"kills\":%d,\"time\":%.2f,\"oreMined\":%d,\"wallMoves\":%d,\"shots\":%d,\"hits\":%d,\"dashes\":%d,\"score\":%d,\"inputFailures\":%d}"),
        Sim.Phase == EPhase::Won ? TEXT("true") : TEXT("false"), bAutoPractice ? TEXT("practice") : TEXT("hard"), Sim.Rank, Sim.PlayerHP, Sim.Kills, Sim.Time, Sim.Stats.OreMined, Sim.Stats.WallMoves, Sim.Stats.Shots, Sim.Stats.Hits, Sim.Stats.Dashes, Sim.Score(), InputFailures);
    FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / FString::Printf(TEXT("autoplay-%s-rank%d.json"), bAutoPractice ? TEXT("practice") : TEXT("hard"), AutoRank)));
    UE_LOG(LogTemp, Display, TEXT("OUTPOST_AUTOPLAY %s"), *Report);
    if (bCapture) CaptureFrame(TEXT("04-result"));
}
