#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OutpostSimulation.h"
#include "OutpostGameMode.generated.h"
class ACameraActor;
class USoundWave;
class UOutpostSaveGame;
class UTexture2D;

struct FOutpostParticle
{
    FVector2D Pos, Velocity; FLinearColor Color; float Life = 0, TotalLife = 0, Size = 3;
};
struct FOutpostRing
{
    FVector2D Pos; FLinearColor Color; float Life = 0, TotalLife = 0, Radius = 0;
};

UCLASS()
class OUTPOST2D_API AOutpostGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AOutpostGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    Outpost::FSimulation Sim;
    UPROPERTY() TObjectPtr<UTexture2D> ArtSprites;
    UPROPERTY() TObjectPtr<UTexture2D> ArtFloor;
    UPROPERTY() TObjectPtr<UTexture2D> ArtKeyArt;
    TArray<FOutpostParticle> Particles;
    TArray<FOutpostRing> Rings;
    float WaveBannerRemaining = 0;
    FString Toast; float ToastRemaining = 0;
    bool bSound = true;
    void StartGame(Outpost::EMode Mode = Outpost::EMode::Hard);
    void ReturnToMenu();
    int32 BestScore(Outpost::EMode Mode) const;
    void TogglePause();
    void SelectTool(int32 Tool);
    void HandlePrimaryClick();
    void HandleSecondaryClick();
    void HandleInteract();
    void HandleRotate();
    void HandleArmor();
    void HandleDash();
    void HandleEnter();
    FVector2D MouseWorld() const;
    FVector2D ScreenPoint(FVector2D Point) const;
    FVector2D WorldPoint(FVector2D Screen) const;
    bool IsGameplayPoint(FVector2D Screen) const;
    void ToggleSound();
private:
    UPROPERTY() TObjectPtr<ACameraActor> ArenaCamera;
    UPROPERTY() TArray<TObjectPtr<USoundWave>> Sounds;
    UPROPERTY() TObjectPtr<UOutpostSaveGame> Records;
    bool bDashQueued = false;
    bool bAutoPractice = false;
    bool bBossCaptured = false;
    float Accumulator = 0, CameraShake = 0;
    bool bAutomation = false, bCapture = false, bAutoFinished = false, bPreview = false;
    bool bInputChecked = false; int32 InputStep = 0, InputFailures = 0;
    FVector2D InputPosition = FVector2D::ZeroVector;
    float InputStepClock = 0;
    int32 AutoStep = 0, AutoRank = 1; float AutoClock = 0, CaptureClock = 0;
    FVector2D AutoAim = FVector2D(522, 250);
    void ProcessEvents();
    Outpost::FInput GatherInput();
    Outpost::FInput AutomationInput(float Dt);
    void CaptureFrame(const FString& Name);
    void FinishAutomation();
    void CheckInput();
    void SaveRecords();
};
