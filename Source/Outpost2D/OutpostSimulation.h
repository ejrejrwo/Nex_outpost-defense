#pragma once
#include "CoreMinimal.h"

namespace Outpost
{
constexpr int32 Tile = 36, Cols = 28, Rows = 17, Width = Tile * Cols, Height = Tile * Rows;
constexpr float PrepSeconds = 30.f, MineSeconds = .45f, ForgeSeconds = 5.f;
constexpr int32 TotalEnemies = 30, MaxRank = 2;
constexpr float DashSeconds = .16f, DashRecovery = 3.2f, DashSpeed = 620.f;
enum class EMode : uint8 { Hard, Practice };
enum class EEnemyKind : uint8 { Grunt, Runner, Brute, Boss };
enum class EUpgradeKind : uint8 { Weapon, Armor };
enum class EPhase : uint8 { Menu, Prep, Combat, Rest, Won, Lost };
enum class EEvent : uint8 { Message, Shot, Mine, Forge, Upgrade, Lift, Place, Wave, Hit, Kill, Hurt, Win, Lose, Dash, Strike, Boss };
struct FRect
{
    int32 Id = 0, X = 0, Y = 0, W = 1, H = 1;
    FVector2D Center() const { return FVector2D((X + W * .5f) * Tile, (Y + H * .5f) * Tile); }
};
struct FMine { FRect Rect; int32 Remaining = 120; };
struct FEnemy
{
    int32 Id = 0; FVector2D Pos = FVector2D::ZeroVector;
    FVector2D Facing = FVector2D(-1, 0);
    float HP = 70, MaxHP = 70, Speed = 96, Flash = 0, Traveled = 0;
    EEnemyKind Kind = EEnemyKind::Grunt;
    float Radius = 12, AttackDuration = .38f, AttackRemaining = 0, AttackCooldown = 0;
    float AttackReach = 47, AttackRadius = 49, Damage = 20;
    FVector2D AttackPoint = FVector2D::ZeroVector;
    int32 Target = -1, Version = -1; TArray<FVector2D> Path;
};
struct FBullet
{
    FVector2D Pos, Velocity, Origin; float Life = 1.6f, Damage = 18; bool bFirst = true;
};
struct FEvent { EEvent Type; FVector2D Pos = FVector2D::ZeroVector; FString Text; float Radius = 0; };
struct FInput { FVector2D Move = FVector2D::ZeroVector; FVector2D Aim = FVector2D(522, 250); bool bUse = false, bDash = false; };
struct FPlacement { bool bValid = false; FRect Rect; FString Reason; };
struct FStats { int32 OreMined = 0, WallMoves = 0, Shots = 0, Hits = 0, Dashes = 0, CombatOreMined = 0, CombatWallMoves = 0; float MiningTime = 0, ForgeTime = 0, CarryTime = 0, DamageTaken = 0; };

class FSimulation
{
public:
    FSimulation();
    EPhase Phase = EPhase::Menu;
    EMode Mode = EMode::Hard;
    EUpgradeKind JobKind = EUpgradeKind::Weapon;
    bool bPaused = false, bAutoMine = false, bMining = false, bCarrying = false;
    float Time = 0, PrepRemaining = PrepSeconds, CombatTime = 0, RestRemaining = 0, JobRemaining = 0, MineProgress = 0;
    float PlayerHP = 100, HurtRemaining = 0, Cooldown = 0;
    float MaxHP = 100, DashRemaining = 0, DashCooldown = 0, PlayerTraveled = 0;
    FVector2D DashVector = FVector2D::ZeroVector;
    int32 ArmorRank = 0, JobCost = 0;
    FVector2D Player = FVector2D(522, 378), Aim = FVector2D(522, 250);
    int32 Tool = 1, Ore = 0, Rank = 0, Wave = 0, Kills = 0, Spawned = 0, GridVersion = 0, ActiveMine = -1;
    float SpawnClock = 0;
    FRect Forge = { 0, 13, 2, 2, 2 }, Carry, CarryOrigin;
    TArray<FMine> Mines; TArray<FRect> Walls; TArray<FEnemy> Enemies; TArray<FBullet> Bullets;
    TArray<FVector2D> MoveOrder; TArray<uint8> Grid; TArray<FEvent> Events; FStats Stats;

    void Reset(EMode NewMode = EMode::Hard);
    void Tick(float Dt, const FInput& Input);
    bool SetTool(int32 Value);
    bool MoveTo(FVector2D Point);
    void Interact();
    bool BeginCombat();
    bool CanWork() const;
    bool StartUpgrade(EUpgradeKind Kind = EUpgradeKind::Weapon);
    bool StartDash(FVector2D Direction);
    int32 Score() const;
    bool PickupWall();
    void RotateWall();
    FPlacement Placement(FVector2D Point) const;
    bool PlaceWall(FVector2D Point);
    bool Fire();
    int32 UpgradeCost() const;
    int32 NearestMine() const;
    bool NearForge() const;
    FString Hint() const;
    bool SolidAt(FVector2D Point, float Radius) const;
    bool ClearTravel(FVector2D From, FVector2D To, float Radius) const;
    void Move(FVector2D& Pos, float Radius, FVector2D Delta) const;
    void Rebuild();
    TArray<uint8> MakeGrid(const FRect* Extra = nullptr) const;
    TArray<FIntPoint> FindPath(const TArray<uint8>& Map, FIntPoint Start, FIntPoint Goal) const;
    static FIntPoint Cell(FVector2D Point);
    static FVector2D Center(FIntPoint Point);
    static bool CircleRect(FVector2D Point, float Radius, FRect Rect);
    void SpawnEnemy();
    void UpdateEnemies(float Dt);
    void Message(const FString& Text);
private:
    int32 NextEnemyId = 1;
    void Emit(EEvent Type, FVector2D Pos = FVector2D::ZeroVector);
    void BeginWave();
    void ReturnCarry();
    void UpdateBullets(float Dt);
    void Finish(EPhase Result);
};
}
