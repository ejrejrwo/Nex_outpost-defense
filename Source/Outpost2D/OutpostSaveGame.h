#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "OutpostSaveGame.generated.h"

// Separate records prevent practice difficulty from replacing a challenge score.
UCLASS()
class OUTPOST2D_API UOutpostSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 HardBest = 0;
    UPROPERTY(SaveGame) int32 PracticeBest = 0;
    UPROPERTY(SaveGame) bool bSound = true;
};
