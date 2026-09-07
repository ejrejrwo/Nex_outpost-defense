#pragma once

#include "CoreMinimal.h"

class AOutpostGameMode;
class UCanvas;

/** Crisp, texture-free arena presentation drawn in the same projection as the simulation. */
class OUTPOST2D_API FOutpostArenaRenderer
{
public:
    static void Draw(UCanvas* Canvas, const AOutpostGameMode& GameMode, float UiScale);
};
