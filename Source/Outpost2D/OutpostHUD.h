#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "OutpostHUD.generated.h"

class AOutpostGameMode;
class UFont;
class UTexture2D;
enum class EOutpostFont : uint8 { Small, Medium, Large };

UCLASS()
class OUTPOST2D_API AOutpostHUD : public AHUD
{
    GENERATED_BODY()

public:
    AOutpostHUD();
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;

private:
    float UiScale = 1.f;
    float ScreenWidth = 1280.f;
    float ScreenHeight = 800.f;

    AOutpostGameMode* GetOutpostGameMode() const;
    void DrawPanel(const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color);
    void DrawTexture(UTexture2D* Texture, const FVector2D& Position, const FVector2D& Size,
        const FVector2D& UV0 = FVector2D::ZeroVector, const FVector2D& UV1 = FVector2D(1.f, 1.f),
        const FLinearColor& Tint = FLinearColor::White);
    void DrawLine(const FVector2D& Start, const FVector2D& End, const FLinearColor& Color, float Thickness = 1.f);
    void DrawLabel(const FString& Text, const FVector2D& Position, const FLinearColor& Color,
        float Scale = 1.f, bool bCenterX = false, bool bCenterY = false, EOutpostFont Font = EOutpostFont::Medium);
    void DrawBar(const FVector2D& Position, const FVector2D& Size, float Fraction,
        const FLinearColor& Fill, const FLinearColor& Back);
    void DrawButton(const FString& Label, FName HitBox, const FVector2D& Position,
        const FVector2D& Size, bool bAccent = false, bool bSelected = false, int32 Priority = 1,
        float TextScale = .78f, bool bEnabled = true);
    void DrawHeader(AOutpostGameMode& GameMode);
    void DrawFooter(AOutpostGameMode& GameMode);
    void DrawWorldHud(AOutpostGameMode& GameMode);
    void DrawOverlay(AOutpostGameMode& GameMode);
    void DrawCrosshair(AOutpostGameMode& GameMode);
};
