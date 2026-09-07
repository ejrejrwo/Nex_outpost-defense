#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OutpostPlayerController.generated.h"
UCLASS()
class OUTPOST2D_API AOutpostPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AOutpostPlayerController();
    virtual void SetupInputComponent() override;
private:
    void One(); void Two(); void Three(); void Interact(); void Rotate();
    void Enter(); void Pause(); void Primary(); void Secondary(); void Armor(); void Dash();
};
