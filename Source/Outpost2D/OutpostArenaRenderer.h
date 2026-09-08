#pragma once

#include "CoreMinimal.h"

class AOutpostGameMode;
class UCanvas;

/** Blender sprite atlas and generated materials, projected onto simulation ground positions. */
class OUTPOST2D_API FOutpostArenaRenderer
{
public:
    static void Draw(UCanvas* Canvas, const AOutpostGameMode& GameMode, float UiScale);
};
