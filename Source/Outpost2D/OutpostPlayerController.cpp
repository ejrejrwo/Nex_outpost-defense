#include "OutpostPlayerController.h"
#include "OutpostGameMode.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"

AOutpostPlayerController::AOutpostPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    DefaultMouseCursor = EMouseCursor::Crosshairs;
}
void AOutpostPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AOutpostPlayerController::One);
    InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AOutpostPlayerController::Two);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AOutpostPlayerController::Three);
    InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AOutpostPlayerController::Interact);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AOutpostPlayerController::Rotate);
    InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AOutpostPlayerController::Armor);
    InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AOutpostPlayerController::Dash);
    InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AOutpostPlayerController::Enter);
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AOutpostPlayerController::Pause);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AOutpostPlayerController::Primary);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AOutpostPlayerController::Secondary);
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
}
#define OUTPOST_ACTION(Action) if (AOutpostGameMode* G = Cast<AOutpostGameMode>(GetWorld()->GetAuthGameMode())) { G->Action; }
void AOutpostPlayerController::One() { OUTPOST_ACTION(SelectTool(1)); }
void AOutpostPlayerController::Two() { OUTPOST_ACTION(SelectTool(2)); }
void AOutpostPlayerController::Three() { OUTPOST_ACTION(SelectTool(3)); }
void AOutpostPlayerController::Interact() { OUTPOST_ACTION(HandleInteract()); }
void AOutpostPlayerController::Rotate() { OUTPOST_ACTION(HandleRotate()); }
void AOutpostPlayerController::Armor() { OUTPOST_ACTION(HandleArmor()); }
void AOutpostPlayerController::Dash() { OUTPOST_ACTION(HandleDash()); }
void AOutpostPlayerController::Enter() { OUTPOST_ACTION(HandleEnter()); }
void AOutpostPlayerController::Pause() { OUTPOST_ACTION(TogglePause()); }
void AOutpostPlayerController::Primary() { OUTPOST_ACTION(HandlePrimaryClick()); }
void AOutpostPlayerController::Secondary() { OUTPOST_ACTION(HandleSecondaryClick()); }
#undef OUTPOST_ACTION
