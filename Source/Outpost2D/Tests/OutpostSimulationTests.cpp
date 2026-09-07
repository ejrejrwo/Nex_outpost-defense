#if WITH_DEV_AUTOMATION_TESTS

#include "../OutpostSimulation.h"
#include "../OutpostSaveGame.h"
#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"

namespace Outpost
{
namespace
{
constexpr float TestFrame = 0.05f;
const FIntPoint WestDoor(1, 8);
const FIntPoint EastDoor(26, 8);
const FRect TestDetourWall = { 99, 12, 8, 1, 4 };

void TickFrames(FSimulation& Simulation, int32 Frames, float Dt, const FInput& Input = FInput())
{
    for (int32 Frame = 0; Frame < Frames; ++Frame)
    {
        Simulation.Tick(Dt, Input);
    }
}

bool TickUntilMoveFinished(FSimulation& Simulation, float Dt, int32 MaxFrames = 1200)
{
    for (int32 Frame = 0; Frame < MaxFrames && Simulation.MoveOrder.Num() > 0; ++Frame)
    {
        Simulation.Tick(Dt, FInput());
    }
    return Simulation.MoveOrder.Num() == 0;
}

FSimulation MakeNavigationSimulation(const FIntPoint TargetCell)
{
    FSimulation Simulation;
    Simulation.Reset(EMode::Hard);
    Simulation.Phase = EPhase::Combat;
    Simulation.Wave = 1;
    Simulation.Player = FSimulation::Center(TargetCell);
    Simulation.PlayerHP = 100000.f;
    Simulation.Walls.Add(TestDetourWall);
    Simulation.Rebuild();
    Simulation.SpawnEnemy();
    Simulation.Enemies[0].HP = 100000.f;
    Simulation.Enemies[0].MaxHP = 100000.f;
    // Isolate navigation from the combat windup contract in path fixtures.
    Simulation.Enemies[0].AttackReach = 0.f;
    Simulation.Enemies[0].AttackRadius = 0.f;
    return Simulation;
}

bool IsTargetCell(const FVector2D Position, const FIntPoint Target)
{
    return FSimulation::Cell(Position) == Target;
}

int32 CellKey(const FIntPoint Cell)
{
    return Cell.X * 100 + Cell.Y;
}

FVector2D SafeCombatDirection(const FSimulation& Simulation, FVector2D Desired)
{
    if (Desired.IsNearlyZero()) return FVector2D::ZeroVector;
    const float BaseAngle = FMath::Atan2(Desired.Y, Desired.X);
    const float Offsets[] = { 0.f, .35f, -.35f, .7f, -.7f, 1.05f, -1.05f, PI };
    for (const float Offset : Offsets)
    {
        const float Angle = BaseAngle + Offset;
        const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
        // One-cell lookahead prevents the steering fixture from hugging a
        // corner before it can select a viable detour.
        const FVector2D Probe = Simulation.Player + Direction * 36.f;
        if (Simulation.ClearTravel(Simulation.Player, Probe, 11.f)) return Direction;
    }
    return FVector2D::ZeroVector;
}

FVector2D SwarmRetreatDirection(const FSimulation& Simulation, int32 Frame)
{
    if (Simulation.Enemies.IsEmpty())
    {
        const FIntPoint Ring[] = { FIntPoint(6, 3), FIntPoint(21, 3), FIntPoint(21, 11), FIntPoint(6, 11) };
        const FVector2D Target = FSimulation::Center(Ring[(Frame / 240) % UE_ARRAY_COUNT(Ring)]);
        return SafeCombatDirection(Simulation, Target - Simulation.Player);
    }

    const FVector2D ArenaCenter = FSimulation::Center(FIntPoint(14, 8));
    const FVector2D FromCenter = Simulation.Player - ArenaCenter;
    const float Radius = FMath::Max(FromCenter.Size(), 1.f);
    FVector2D Repel = FVector2D::ZeroVector;
    FVector2D NearestAway = FVector2D::ZeroVector;
    float NearestDistance = TNumericLimits<float>::Max();
    for (const FEnemy& Enemy : Simulation.Enemies)
    {
        const FVector2D Away = Simulation.Player - Enemy.Pos;
        const float DistanceToEnemy = FMath::Max(Away.Size(), 1.f);
        if (DistanceToEnemy < NearestDistance)
        {
            NearestDistance = DistanceToEnemy;
            NearestAway = Away / DistanceToEnemy;
        }
        if (DistanceToEnemy < 330.f)
        {
            const float Weight = FMath::Square((330.f - DistanceToEnemy) / 330.f);
            Repel += Away / DistanceToEnemy * Weight;
        }
    }

    const FVector2D Tangent(-FromCenter.Y / Radius, FromCenter.X / Radius);
    const float Radial = Radius < 205.f ? 1.f : Radius > 250.f ? -1.f : 0.f;
    FVector2D Desired = Tangent * .85f + FromCenter / Radius * Radial * .75f + Repel * 2.4f;
    if (NearestDistance < 150.f) Desired += NearestAway * 2.8f;
    else if (NearestDistance > 245.f) Desired -= NearestAway * .35f;
    return SafeCombatDirection(Simulation, Desired);
}

bool PrepareRankTwo(FSimulation& Simulation, EMode Mode)
{
    Simulation.Reset(Mode);
    if (!Simulation.SetTool(2) || !Simulation.MoveTo(FSimulation::Center(FIntPoint(5, 4)))) return false;
    if (!TickUntilMoveFinished(Simulation, TestFrame)) return false;
    Simulation.Interact();
    TickFrames(Simulation, FMath::CeilToInt(24.f * MineSeconds / TestFrame) + 2, TestFrame);
    if (Simulation.Stats.OreMined < 24) return false;
    if (!Simulation.MoveTo(Simulation.Forge.Center()) || !TickUntilMoveFinished(Simulation, TestFrame)) return false;
    if (!Simulation.StartUpgrade(EUpgradeKind::Weapon)) return false;
    TickFrames(Simulation, 102, TestFrame);
    if (Simulation.Rank != 1) return false;
    if (!Simulation.StartUpgrade(EUpgradeKind::Weapon)) return false;
    TickFrames(Simulation, 102, TestFrame);
    return Simulation.Rank == 2 && Simulation.Phase == EPhase::Prep;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOutpostPrepMiningForgeTest,
    "Outpost.Simulation.PrepMiningAndTwoUpgrades",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOutpostPrepMiningForgeTest::RunTest(const FString& Parameters)
{
    FSimulation Simulation;
    Simulation.Reset(EMode::Hard);
    TestEqual(TEXT("Preparation is 30 seconds"), Simulation.PrepRemaining, 30.f);
    TestTrue(TEXT("Selects the pickaxe"), Simulation.SetTool(2));
    TestTrue(TEXT("MoveTo reaches the mine area"), Simulation.MoveTo(FSimulation::Center(FIntPoint(5, 4))));
    TestTrue(TEXT("Mine movement finishes before prep ends"), TickUntilMoveFinished(Simulation, TestFrame));
    TestTrue(TEXT("Mine is nearby after real movement"), Simulation.NearestMine() >= 0);

    Simulation.Interact();
    TestTrue(TEXT("E starts automatic mining"), Simulation.bAutoMine);
    TickFrames(Simulation, FMath::CeilToInt(24.f * MineSeconds / TestFrame) + 2, TestFrame);
    TestTrue(TEXT("24 ore mined in preparation"), Simulation.Ore >= 24);
    TestEqual(TEXT("First upgrade cost is 10"), Simulation.UpgradeCost(), 10);

    TestTrue(TEXT("MoveTo reaches forge area"), Simulation.MoveTo(Simulation.Forge.Center()));
    TestTrue(TEXT("Forge movement finishes"), TickUntilMoveFinished(Simulation, TestFrame));
    TestTrue(TEXT("First weapon forge job starts"), Simulation.StartUpgrade(EUpgradeKind::Weapon));
    TickFrames(Simulation, 102, TestFrame);
    TestEqual(TEXT("First upgrade completes"), Simulation.Rank, 1);
    TestEqual(TEXT("Second upgrade cost is 14"), Simulation.UpgradeCost(), 14);
    TestTrue(TEXT("Armor forge job starts"), Simulation.StartUpgrade(EUpgradeKind::Armor));
    TickFrames(Simulation, 102, TestFrame);
    TestEqual(TEXT("Armor upgrade completes"), Simulation.ArmorRank, 1);
    TestEqual(TEXT("Armor raises max HP"), Simulation.MaxHP, 140.f);
    TestTrue(TEXT("Two upgrades fit in 30 seconds"), Simulation.Phase == EPhase::Prep);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOutpostWallPlacementTest,
    "Outpost.Simulation.UpgradeAndWallPlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOutpostWallPlacementTest::RunTest(const FString& Parameters)
{
    FSimulation Simulation;
    Simulation.Reset();
    Simulation.Player = FSimulation::Center(FIntPoint(14, 4));
    Simulation.Ore = 10;
    TestTrue(TEXT("One weapon upgrade starts"), Simulation.StartUpgrade(EUpgradeKind::Weapon));
    TickFrames(Simulation, 102, TestFrame);
    TestEqual(TEXT("One upgrade reaches rank 1"), Simulation.Rank, 1);

    Simulation.Player = FSimulation::Center(FIntPoint(8, 8));
    TestTrue(TEXT("Selects bare hands"), Simulation.SetTool(3));
    TestTrue(TEXT("Picks up a nearby wall"), Simulation.PickupWall());
    Simulation.RotateWall();
    Simulation.Player = FSimulation::Center(FIntPoint(12, 3));
    TestTrue(TEXT("Rotated wall can be placed"), Simulation.PlaceWall(FSimulation::Center(FIntPoint(10, 1))));
    TestTrue(TEXT("Wall is no longer carried"), !Simulation.bCarrying);
    TestEqual(TEXT("Wall movement recorded"), Simulation.Stats.WallMoves, 1);
    TestEqual(TEXT("Four walls remain after relocation"), Simulation.Walls.Num(), 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOutpostPlacementValidationTest,
    "Outpost.Simulation.PlacementRejectsOverlapAndSealedRoute",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOutpostPlacementValidationTest::RunTest(const FString& Parameters)
{
    FSimulation Overlap;
    Overlap.Reset(EMode::Hard);
    Overlap.Phase = EPhase::Combat;
    Overlap.Wave = 1;
    Overlap.Player = FSimulation::Center(FIntPoint(8, 8));
    Overlap.SetTool(3);
    TestTrue(TEXT("Wall available for overlap fixture"), Overlap.PickupWall());
    Overlap.SpawnEnemy();
    Overlap.Enemies[0].Pos = FSimulation::Center(FIntPoint(7, 8));
    Overlap.Enemies[0].HP = 100.f;
    const FPlacement EnemyOverlap = Overlap.Placement(Overlap.Enemies[0].Pos);
    TestTrue(TEXT("Wall overlapping a live enemy is rejected"), !EnemyOverlap.bValid);
    TestTrue(TEXT("Enemy overlap reports the specific guard"), EnemyOverlap.Reason.Contains(TEXT("적과 겹")));
    TestTrue(TEXT("Rejected overlap keeps the wall carried"), Overlap.bCarrying);

    FSimulation Sealed;
    Sealed.Reset(EMode::Hard);
    Sealed.Phase = EPhase::Combat;
    Sealed.Walls.Reset();
    Sealed.Walls.Add(FRect{ 100, 4, 6, 4, 1 });
    Sealed.Walls.Add(FRect{ 101, 4, 7, 1, 3 });
    Sealed.Walls.Add(FRect{ 102, 4, 10, 4, 1 });
    Sealed.Carry = { 103, 0, 0, 1, 3 };
    Sealed.CarryOrigin = Sealed.Carry;
    Sealed.bCarrying = true;
    Sealed.Player = FSimulation::Center(FIntPoint(8, 8));
    FEnemy PocketEnemy;
    PocketEnemy.Pos = FSimulation::Center(FIntPoint(5, 8));
    PocketEnemy.HP = PocketEnemy.MaxHP = 100.f;
    PocketEnemy.AttackReach = 0.f;
    Sealed.Enemies.Add(PocketEnemy);
    Sealed.Rebuild();
    const FPlacement SealedPlacement = Sealed.Placement(FSimulation::Center(FIntPoint(7, 8)));
    TestTrue(TEXT("Sealing the live enemy pocket is rejected"), !SealedPlacement.bValid);
    TestTrue(TEXT("Sealed pocket reports a route guard"), SealedPlacement.Reason.Contains(TEXT("통로")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOutpostNavigationFixedTargetTest,
    "Outpost.Simulation.Navigation.FixedTargetAcrossFrameRates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOutpostNavigationFixedTargetTest::RunTest(const FString& Parameters)
{
    FSimulation MapCheck = MakeNavigationSimulation(FIntPoint(14, 10));
    bool bDetourFound = false;
    for (const FIntPoint Door : { WestDoor, EastDoor })
    {
        const TArray<FIntPoint> Route = MapCheck.FindPath(MapCheck.Grid, Door, FIntPoint(14, 10));
        TestTrue(TEXT("Door has a reachable route"), Route.Num() > 0);
        for (const FIntPoint Point : Route)
        {
            const bool bInDetourWall = Point.X >= TestDetourWall.X && Point.X < TestDetourWall.X + TestDetourWall.W &&
                Point.Y >= TestDetourWall.Y && Point.Y < TestDetourWall.Y + TestDetourWall.H;
            TestTrue(TEXT("BFS route does not enter the blocking wall"), !bInDetourWall);
            bDetourFound |= Point.X == TestDetourWall.X &&
                (Point.Y < TestDetourWall.Y || Point.Y >= TestDetourWall.Y + TestDetourWall.H);
        }
    }
    TestTrue(TEXT("At least one door route visibly detours around the wall"), bDetourFound);

    const float TimeSteps[] = { 1.f / 20.f, 1.f / 60.f, 1.f / 144.f };
    float ArrivalTimes[UE_ARRAY_COUNT(TimeSteps)] = {};
    for (int32 StepIndex = 0; StepIndex < UE_ARRAY_COUNT(TimeSteps); ++StepIndex)
    {
        const float Dt = TimeSteps[StepIndex];
        FSimulation Simulation = MakeNavigationSimulation(FIntPoint(14, 10));
        const FIntPoint Target(14, 10);
        float Arrival = -1.f;
        for (int32 Frame = 0; Frame < FMath::CeilToInt(30.f / Dt); ++Frame)
        {
            const FVector2D Before = Simulation.Enemies[0].Pos;
            Simulation.UpdateEnemies(Dt);
            TestTrue(TEXT("Enemy swept segment clears solid geometry"),
                Simulation.ClearTravel(Before, Simulation.Enemies[0].Pos, 12.f));
            TestTrue(TEXT("Enemy never enters solid geometry"), !Simulation.SolidAt(Simulation.Enemies[0].Pos, 12.f));
            if (Arrival < 0.f && IsTargetCell(Simulation.Enemies[0].Pos, Target) &&
                FVector2D::Distance(Simulation.Enemies[0].Pos, Simulation.Player) < 24.f)
            {
                Arrival = (Frame + 1) * Dt;
                break;
            }
        }
        TestTrue(TEXT("Enemy reaches fixed target"), Arrival >= 0.f);
        ArrivalTimes[StepIndex] = Arrival;
    }
    const float Minimum = FMath::Min3(ArrivalTimes[0], ArrivalTimes[1], ArrivalTimes[2]);
    const float Maximum = FMath::Max3(ArrivalTimes[0], ArrivalTimes[1], ArrivalTimes[2]);
    TestTrue(TEXT("Arrival time is consistent across frame rates"), Minimum >= 0.f && Maximum - Minimum < .25f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOutpostNavigationMovingGoalTest,
    "Outpost.Simulation.Navigation.MovingGoalNoBacktracking",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOutpostNavigationMovingGoalTest::RunTest(const FString& Parameters)
{
    const float TimeSteps[] = { 1.f / 20.f, 1.f / 60.f, 1.f / 144.f };
    const FIntPoint Start(14, 10), Finish(20, 10);
    for (const float Dt : TimeSteps)
    {
        FSimulation Simulation = MakeNavigationSimulation(Start);
        FVector2D Previous = Simulation.Enemies[0].Pos;
        int32 BackwardSteps = 0, TargetCellChanges = 0, Reentries = 0;
        float BackwardDistance = 0.f, PreviousTargetKey = CellKey(FSimulation::Cell(Simulation.Player));
        int32 PreviousEnemyKey = INDEX_NONE;
        TSet<int32> Visited;
        const int32 Frames = FMath::CeilToInt(35.f / Dt);
        for (int32 Frame = 0; Frame < Frames; ++Frame)
        {
            const float Elapsed = Frame * Dt;
            const float Ratio = FMath::Min(1.f, Elapsed / 12.f);
            Simulation.Player = FSimulation::Center(FIntPoint(
                FMath::RoundToInt(Start.X + (Finish.X - Start.X) * Ratio), Start.Y));
            // Keep the target's sub-cell movement continuous while crossing cells.
            Simulation.Player.X = (Start.X + .5f + (Finish.X - Start.X) * Ratio) * Tile;
            const int32 TargetKey = CellKey(FSimulation::Cell(Simulation.Player));
            if (TargetKey != PreviousTargetKey) ++TargetCellChanges;
            PreviousTargetKey = TargetKey;

            const FVector2D Before = Simulation.Enemies[0].Pos;
            Simulation.UpdateEnemies(Dt);
            const FVector2D Current = Simulation.Enemies[0].Pos;
            TestTrue(TEXT("Moving-goal swept segment clears solid geometry"), Simulation.ClearTravel(Before, Current, 12.f));
            TestTrue(TEXT("Moving-goal enemy never enters a wall"), !Simulation.SolidAt(Current, 12.f));
            if (Current.X < Previous.X - .1f)
            {
                ++BackwardSteps;
                BackwardDistance += Previous.X - Current.X;
            }
            const int32 EnemyKey = CellKey(FSimulation::Cell(Current));
            if (EnemyKey != PreviousEnemyKey)
            {
                if (Visited.Contains(EnemyKey)) ++Reentries;
                Visited.Add(EnemyKey);
                PreviousEnemyKey = EnemyKey;
            }
            Previous = Current;
        }
        TestTrue(TEXT("Goal crosses cell boundaries"), TargetCellChanges >= 5);
        TestTrue(TEXT("Enemy reaches moving goal's final cell"), FSimulation::Cell(Simulation.Enemies[0].Pos) == Finish);
        TestTrue(TEXT("Enemy reaches moving goal"), FVector2D::Distance(Simulation.Enemies[0].Pos, Simulation.Player) < 24.f);
        TestTrue(TEXT("Enemy avoids repeated reverse travel"), BackwardSteps <= 2 && BackwardDistance < 8.f);
        TestTrue(TEXT("Enemy avoids re-entering passed nodes"), Reentries <= 2);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOutpostNearbyProjectileTest,
    "Outpost.Simulation.Projectile.NearbyEnemyOffset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOutpostNearbyProjectileTest::RunTest(const FString& Parameters)
{
    FSimulation Simulation;
    Simulation.Reset();
    Simulation.BeginCombat();
    Simulation.SpawnEnemy();
    FEnemy& Enemy = Simulation.Enemies[0];
    Enemy.Pos = Simulation.Player + FVector2D(40.f, 0.f);
    Enemy.Speed = 0.f;
    const float InitialHP = Enemy.HP;
    Simulation.Aim = Enemy.Pos;
    TestTrue(TEXT("Nearby shot fires"), Simulation.Fire());
    TestTrue(TEXT("Projectile begins offset from player"), Simulation.Bullets.Num() == 1 &&
        FVector2D::Distance(Simulation.Bullets[0].Pos, Simulation.Player) > 10.f);
    FInput Input;
    Input.Aim = Enemy.Pos;
    Input.bUse = true;
    Simulation.Tick(TestFrame, Input);
    TestTrue(TEXT("Offset projectile hits nearby enemy"), Simulation.Stats.Hits > 0 && Enemy.HP < InitialHP);
    TestEqual(TEXT("Nearby projectile does not damage player"), Simulation.PlayerHP, 100.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOutpostPauseFreezeTest,
    "Outpost.Simulation.Pause.FreezesState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOutpostPauseFreezeTest::RunTest(const FString& Parameters)
{
    FSimulation Simulation;
    Simulation.Reset();
    Simulation.BeginCombat();
    Simulation.SpawnEnemy();
    const float TimeBefore = Simulation.Time;
    const FVector2D PlayerBefore = Simulation.Player;
    const FVector2D EnemyBefore = Simulation.Enemies[0].Pos;
    Simulation.bPaused = true;
    FInput Input;
    Input.Move = FVector2D(1.f, 1.f);
    Input.bUse = true;
    Simulation.Tick(1.f, Input);
    TestEqual(TEXT("Paused time is unchanged"), Simulation.Time, TimeBefore);
    TestTrue(TEXT("Paused player is unchanged"), FVector2D::Distance(Simulation.Player, PlayerBefore) < .01f);
    TestTrue(TEXT("Paused enemy is unchanged"), FVector2D::Distance(Simulation.Enemies[0].Pos, EnemyBefore) < .01f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOutpostCombatWorkContractTest,
    "Outpost.Simulation.Combat.MiningForgeAndRestWork",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOutpostCombatWorkContractTest::RunTest(const FString& Parameters)
{
    FSimulation Mining;
    Mining.Reset(EMode::Hard);
    TestTrue(TEXT("Combat begins for work fixture"), Mining.BeginCombat());
    TestTrue(TEXT("Combat is a work phase"), Mining.CanWork());
    Mining.bPaused = true;
    TestTrue(TEXT("Paused combat cannot perform work"), !Mining.CanWork());
    Mining.bPaused = false;
    Mining.Player = FSimulation::Center(FIntPoint(6, 4));
    TestTrue(TEXT("Pickaxe is available during combat"), Mining.SetTool(2));
    Mining.Interact();
    TestTrue(TEXT("Combat E starts automatic mining"), Mining.bAutoMine);

    Mining.SpawnEnemy();
    FEnemy& Attacker = Mining.Enemies[0];
    Attacker.Pos = Mining.Player + FVector2D(35.f, 0.f);
    Attacker.AttackCooldown = 0.f;
    Mining.SpawnEnemy();
    FEnemy& Mover = Mining.Enemies[1];
    Mover.Pos = Mining.Player + FVector2D(200.f, 72.f);
    Mover.AttackReach = 0.f;
    const float InitialHP = Mining.PlayerHP;
    TickFrames(Mining, 10, TestFrame);
    TestEqual(TEXT("One combat mine cycle yields one ore"), Mining.Ore, 1);
    TestEqual(TEXT("Combat ore counter records the cycle"), Mining.Stats.CombatOreMined, 1);
    TestTrue(TEXT("Enemies keep moving while mining"), Mover.Traveled > 0.f);
    TestTrue(TEXT("Combat mining remains vulnerable to attack windup"), Mining.PlayerHP < InitialHP);

    FSimulation Job;
    Job.Reset(EMode::Hard);
    TestTrue(TEXT("Combat begins for forge fixture"), Job.BeginCombat());
    // Stand just below the forge rather than inside its solid footprint.
    Job.Player = FSimulation::Center(FIntPoint(14, 4));
    Job.Ore = 20;
    Job.SpawnEnemy();
    FEnemy& JobEnemy = Job.Enemies[0];
    JobEnemy.Pos = Job.Player + FVector2D(35.f, 0.f);
    JobEnemy.Speed = 0.f;
    JobEnemy.AttackCooldown = 0.f;
    Job.SpawnClock = TNumericLimits<float>::Max();
    TestTrue(TEXT("Weapon upgrade starts during combat"), Job.StartUpgrade(EUpgradeKind::Weapon));
    const FVector2D PositionBeforeJob = Job.Player;
    TickFrames(Job, 40, TestFrame, FInput{ FVector2D(1, 0), Job.Player, false, false });
    TestTrue(TEXT("Forge time advances while enemy attacks"), Job.JobRemaining > 0.f && Job.JobRemaining < ForgeSeconds);
    TestTrue(TEXT("Movement remains locked during forge job"), FVector2D::Distance(Job.Player, PositionBeforeJob) < .01f);
    TestTrue(TEXT("Combat forge job receives live damage"), Job.Stats.DamageTaken > 0.f);
    TestEqual(TEXT("Forge cost is charged once"), Job.Ore, 10);
    Job.Interact();
    TestEqual(TEXT("E cancellation refunds the exact forge cost"), Job.Ore, 20);
    TestEqual(TEXT("E cancellation clears the job"), Job.JobRemaining, 0.f);
    Job.Interact();
    TestEqual(TEXT("A second E starts a new job instead of double refunding"), Job.Ore, 10);
    Job.Interact();
    TestEqual(TEXT("Cancelling the second job restores the same balance"), Job.Ore, 20);

    Job.Phase = EPhase::Rest;
    Job.RestRemaining = 10.f;
    Job.Enemies.Reset();
    TestTrue(TEXT("Armor upgrade starts during rest"), Job.StartUpgrade(EUpgradeKind::Armor));
    TickFrames(Job, 102, TestFrame);
    TestEqual(TEXT("Rest forge job completes"), Job.ArmorRank, 1);
    TestEqual(TEXT("Armor job raises max HP"), Job.MaxHP, 140.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOutpostCombatPlacementRepathTest,
    "Outpost.Simulation.Combat.WallGuardsAndRepath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOutpostCombatPlacementRepathTest::RunTest(const FString& Parameters)
{
    FSimulation Overlap;
    Overlap.Reset(EMode::Hard);
    Overlap.BeginCombat();
    Overlap.Player = FSimulation::Center(FIntPoint(8, 8));
    TestTrue(TEXT("Hands are available during combat"), Overlap.SetTool(3));
    TestTrue(TEXT("Nearby wall can be picked up during combat"), Overlap.PickupWall());
    Overlap.SpawnEnemy();
    Overlap.Enemies[0].Pos = FSimulation::Center(FIntPoint(7, 8));
    const FPlacement EnemyOverlap = Overlap.Placement(Overlap.Enemies[0].Pos);
    TestTrue(TEXT("Combat placement rejects a live enemy overlap"), !EnemyOverlap.bValid);
    TestTrue(TEXT("Enemy overlap reason is explicit"), EnemyOverlap.Reason.Contains(TEXT("적과 겹")));

    FSimulation Sealed;
    Sealed.Reset(EMode::Hard);
    Sealed.BeginCombat();
    Sealed.Walls.Reset();
    Sealed.Walls.Add(FRect{ 100, 4, 6, 4, 1 });
    Sealed.Walls.Add(FRect{ 101, 4, 7, 1, 3 });
    Sealed.Walls.Add(FRect{ 102, 4, 10, 4, 1 });
    Sealed.Carry = { 103, 0, 0, 1, 3 };
    Sealed.CarryOrigin = Sealed.Carry;
    Sealed.bCarrying = true;
    Sealed.Player = FSimulation::Center(FIntPoint(8, 8));
    FEnemy PocketEnemy;
    PocketEnemy.Pos = FSimulation::Center(FIntPoint(5, 8));
    PocketEnemy.HP = PocketEnemy.MaxHP = 100.f;
    Sealed.Enemies.Add(PocketEnemy);
    Sealed.Rebuild();
    const FPlacement SealedPlacement = Sealed.Placement(FSimulation::Center(FIntPoint(7, 8)));
    TestTrue(TEXT("Combat placement rejects stranding a live enemy"), !SealedPlacement.bValid);
    TestTrue(TEXT("Stranded enemy reports a route guard"), SealedPlacement.Reason.Contains(TEXT("통로")));

    FSimulation Repath;
    Repath.Reset(EMode::Hard);
    Repath.BeginCombat();
    Repath.Player = FSimulation::Center(FIntPoint(8, 8));
    Repath.SetTool(3);
    TestTrue(TEXT("Repath fixture picks up a wall"), Repath.PickupWall());
    Repath.RotateWall();
    Repath.SpawnEnemy();
    FEnemy& Enemy = Repath.Enemies[0];
    Enemy.HP = Enemy.MaxHP = 100000.f;
    Enemy.AttackReach = 0.f;
    Repath.SpawnClock = TNumericLimits<float>::Max();
    Repath.Tick(TestFrame, FInput());
    Repath.Player = FSimulation::Center(FIntPoint(12, 3));
    const int32 VersionBefore = Repath.GridVersion;
    TestTrue(TEXT("Combat wall placement succeeds with an existing enemy route"),
        Repath.PlaceWall(FSimulation::Center(FIntPoint(10, 1))));
    TestTrue(TEXT("Combat wall movement updates the navigation version"), Repath.GridVersion > VersionBefore);
    const FVector2D Start = Enemy.Pos;
    for (int32 Frame = 0; Frame < 1200; ++Frame)
    {
        Repath.Tick(TestFrame, FInput());
        TestTrue(TEXT("Repath enemy remains outside solid geometry"), !Repath.SolidAt(Enemy.Pos, Enemy.Radius));
    }
    TestTrue(TEXT("Repath enemy makes forward progress after placement"), FVector2D::Distance(Start, Enemy.Pos) > 20.f);
    const FIntPoint EndCell = FSimulation::Cell(Enemy.Pos);
    const FIntPoint TargetCell = FSimulation::Cell(Repath.Player);
    const bool bReached = EndCell == TargetCell;
    TestTrue(FString::Printf(TEXT("Repath enemy reaches moved player: end=(%d,%d) target=(%d,%d) distance=%.1f path=%d grid=%d targetKey=%d"),
        EndCell.X, EndCell.Y, TargetCell.X, TargetCell.Y, FVector2D::Distance(Enemy.Pos, Repath.Player),
        Enemy.Path.Num(), Enemy.Version, Enemy.Target), bReached);
    TestEqual(TEXT("Combat wall counter records placement"), Repath.Stats.CombatWallMoves, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOutpostDashAndAttackContractTest,
    "Outpost.Simulation.Combat.DashCollisionAndWindup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOutpostDashAndAttackContractTest::RunTest(const FString& Parameters)
{
    FSimulation Carry;
    Carry.Reset(EMode::Hard);
    Carry.BeginCombat();
    Carry.Player = FSimulation::Center(FIntPoint(8, 8));
    Carry.SetTool(3);
    TestTrue(TEXT("Dash fixture picks up a wall"), Carry.PickupWall());
    TestTrue(TEXT("Carrying a wall prevents dashing"), !Carry.StartDash(FVector2D(1, 0)));

    FSimulation Dash;
    Dash.Reset(EMode::Hard);
    Dash.BeginCombat();
    Dash.Player = FSimulation::Center(FIntPoint(8, 8));
    TestTrue(TEXT("Combat dash starts"), Dash.StartDash(FVector2D(1, 0)));
    TestEqual(TEXT("Dash is counted"), Dash.Stats.Dashes, 1);
    const float XBefore = Dash.Player.X;
    TickFrames(Dash, 4, TestFrame);
    TestTrue(TEXT("Dash collides with a solid wall instead of tunnelling"), Dash.Player.X <= XBefore + 10.f);
    TestTrue(TEXT("Dash ends after its short duration"), Dash.DashRemaining <= 0.f);
    TestTrue(TEXT("Dash recovery blocks an immediate second dash"), !Dash.StartDash(FVector2D(0, 1)));

    FSimulation Attack;
    Attack.Reset(EMode::Hard);
    Attack.BeginCombat();
    Attack.SpawnEnemy();
    FEnemy& Enemy = Attack.Enemies[0];
    Enemy.Pos = Attack.Player + FVector2D(35.f, 0.f);
    Enemy.Speed = 0.f;
    Enemy.AttackReach = 100.f;
    Enemy.AttackRadius = 100.f;
    Enemy.AttackPoint = Attack.Player;
    Enemy.AttackRemaining = .01f;
    const float HPBefore = Attack.PlayerHP;
    TestTrue(TEXT("Dash can be started before a locked strike lands"), Attack.StartDash(FVector2D(0, 1)));
    Attack.Tick(TestFrame, FInput());
    TestEqual(TEXT("Dash grants invulnerability during attack resolution"), Attack.PlayerHP, HPBefore);

    FSimulation Telegraph;
    Telegraph.Reset(EMode::Hard);
    Telegraph.BeginCombat();
    Telegraph.SpawnEnemy();
    FEnemy& TelegraphEnemy = Telegraph.Enemies[0];
    TelegraphEnemy.Pos = Telegraph.Player + FVector2D(35.f, 0.f);
    TelegraphEnemy.Speed = 0.f;
    TelegraphEnemy.AttackPoint = Telegraph.Player;
    TelegraphEnemy.AttackRemaining = .01f;
    Telegraph.Tick(TestFrame, FInput());
    TestTrue(TEXT("A locked attack damages a player who stays in its radius"), Telegraph.Stats.DamageTaken > 0.f);

    FSimulation LOS;
    LOS.Reset(EMode::Hard);
    LOS.BeginCombat();
    LOS.Player = FSimulation::Center(FIntPoint(10, 8));
    LOS.SpawnEnemy();
    FEnemy& LOSEnemy = LOS.Enemies[0];
    LOSEnemy.Pos = FSimulation::Center(FIntPoint(8, 8));
    LOSEnemy.Speed = 0.f;
    LOSEnemy.AttackReach = 100.f;
    LOSEnemy.AttackRadius = 100.f;
    LOSEnemy.AttackPoint = LOS.Player;
    LOSEnemy.AttackRemaining = .01f;
    LOS.Tick(TestFrame, FInput());
    TestEqual(TEXT("A wall blocks the locked attack line of sight"), LOS.Stats.DamageTaken, 0.f);
    LOS.Walls.RemoveAll([](const FRect& Wall) { return Wall.Id == 1; });
    LOS.Rebuild();
    LOSEnemy.AttackRemaining = .01f;
    LOS.Tick(TestFrame, FInput());
    TestTrue(TEXT("Removing the wall restores attack line of sight"), LOS.Stats.DamageTaken > 0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOutpostPracticeBossScoreTest,
    "Outpost.Simulation.Modes.BossRosterAndScore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOutpostPracticeBossScoreTest::RunTest(const FString& Parameters)
{
    FSimulation Hard;
    Hard.Reset(EMode::Hard);
    Hard.BeginCombat();
    Hard.Wave = 3;
    Hard.Spawned = 12;
    Hard.SpawnEnemy();
    TestTrue(TEXT("The thirtieth enemy is the final boss"), Hard.Enemies[0].Kind == EEnemyKind::Boss);
    TestTrue(TEXT("Boss has the large hard-mode health pool"), Hard.Enemies[0].MaxHP > 600.f);

    FSimulation Practice;
    Practice.Reset(EMode::Practice);
    Practice.BeginCombat();
    Practice.Wave = 3;
    Practice.Spawned = 12;
    Practice.SpawnEnemy();
    TestTrue(TEXT("Practice keeps the same boss roster"), Practice.Enemies[0].Kind == EEnemyKind::Boss);
    TestTrue(TEXT("Practice boss health is scaled below hard"), Practice.Enemies[0].MaxHP < Hard.Enemies[0].MaxHP);
    TestTrue(TEXT("Practice boss speed is scaled below hard"), Practice.Enemies[0].Speed < Hard.Enemies[0].Speed);
    TestTrue(TEXT("Practice boss damage is scaled below hard"), Practice.Enemies[0].Damage < Hard.Enemies[0].Damage);

    Hard.Phase = EPhase::Lost;
    Hard.Kills = TotalEnemies;
    TestEqual(TEXT("Lost runs do not receive the clear bonus"), Hard.Score(), TotalEnemies * 100);
    Hard.Phase = EPhase::Won;
    Hard.PlayerHP = 100.f;
    Hard.CombatTime = 60.f;
    TestTrue(TEXT("Won runs receive the clear bonus after all kills"), Hard.Score() > TotalEnemies * 100);

    FSimulation Terminal;
    Terminal.Reset(EMode::Hard);
    Terminal.BeginCombat();
    Terminal.Player = FSimulation::Center(FIntPoint(14, 4));
    Terminal.Ore = 10;
    TestTrue(TEXT("Terminal fixture starts a forge job"), Terminal.StartUpgrade(EUpgradeKind::Weapon));
    Terminal.Wave = 3;
    Terminal.Spawned = 13;
    Terminal.Enemies.Reset();
    Terminal.Tick(.05f, FInput());
    TestTrue(TEXT("Clearing the final wave reaches won"), Terminal.Phase == EPhase::Won);
    TestEqual(TEXT("Terminal win refunds unfinished forge cost"), Terminal.Ore, 10);
    TestEqual(TEXT("Terminal win clears unfinished job"), Terminal.JobRemaining, 0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOutpostThirtyEnemyAssaultTest,
    "Outpost.Simulation.Combat.ThirtyEnemyAssault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOutpostThirtyEnemyAssaultTest::RunTest(const FString& Parameters)
{
    FSimulation Simulation;
    TestTrue(TEXT("Real preparation mines ore and completes two weapon jobs"), PrepareRankTwo(Simulation, EMode::Hard));
    TestEqual(TEXT("Full run starts combat at normal HP"), Simulation.PlayerHP, 100.f);
    TestTrue(TEXT("Preparation used the real mine path"), Simulation.Stats.OreMined >= 24 && Simulation.PlayerTraveled > 0.f);
    TestTrue(TEXT("Combat starts"), Simulation.BeginCombat());
    int32 Frames = 0;
    while ((Simulation.Phase == EPhase::Combat || Simulation.Phase == EPhase::Rest) && Frames++ < 60000)
    {
        FInput Input;
        const FEnemy* NearestEnemy = nullptr;
        float NearestDistance = TNumericLimits<float>::Max();
        for (const FEnemy& Enemy : Simulation.Enemies)
        {
            const float DistanceToEnemy = FVector2D::DistSquared(Simulation.Player, Enemy.Pos);
            if (DistanceToEnemy < NearestDistance)
            {
                NearestDistance = DistanceToEnemy;
                NearestEnemy = &Enemy;
            }
        }
        if (NearestEnemy)
        {
            Input.Aim = NearestEnemy->Pos;
            Input.bUse = true;
        }
        Input.Move = SwarmRetreatDirection(Simulation, Frames);
        // Pulse the public dash input when an enemy is close or has already
        // telegraphed.  This is deliberately an evasive controller for the
        // assay, not a hard-coded answer to the simulation's pathfinding.
        const bool bDanger = NearestEnemy &&
            (FVector2D::Distance(Simulation.Player, NearestEnemy->Pos) < 190.f || NearestEnemy->AttackRemaining > 0.f);
        Input.bDash = bDanger && (Frames % 10 == 0);
        Simulation.Tick(TestFrame, Input);
    }
    TestTrue(TEXT("Thirty-enemy assault reaches a terminal state"), Frames < 60000);
    TestEqual(TEXT("All thirty enemies are defeated"), Simulation.Kills, 30);
    TestTrue(TEXT("Simulation reaches win state"), Simulation.Phase == EPhase::Won);
    TestTrue(TEXT("Player survives with real starting health"), Simulation.PlayerHP > 0.f && Simulation.PlayerHP <= Simulation.MaxHP);
    TestTrue(TEXT("Real bullets were fired and hit"), Simulation.Stats.Shots > 0 && Simulation.Stats.Hits >= 30);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOutpostPersistenceTest,
    "Outpost.Simulation.Persistence.SeparateModeRecords",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOutpostPersistenceTest::RunTest(const FString& Parameters)
{
    const FString Slot = FString::Printf(TEXT("OutpostAutomation_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    USaveGame* SaveObject = UGameplayStatics::CreateSaveGameObject(UOutpostSaveGame::StaticClass());
    TestTrue(TEXT("Creates a save object"), SaveObject != nullptr);

    bool bSaved = false;
    if (UOutpostSaveGame* Save = Cast<UOutpostSaveGame>(SaveObject))
    {
        Save->HardBest = 4321;
        Save->PracticeBest = 9876;
        Save->bSound = false;
        bSaved = UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
    }
    TestTrue(TEXT("Saves mode records to the isolated automation slot"), bSaved);

    UOutpostSaveGame* Loaded = Cast<UOutpostSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    TestTrue(TEXT("Loads the isolated automation slot"), Loaded != nullptr);
    if (Loaded)
    {
        TestEqual(TEXT("Hard best survives reload"), Loaded->HardBest, 4321);
        TestEqual(TEXT("Practice best survives reload"), Loaded->PracticeBest, 9876);
        TestTrue(TEXT("Sound preference survives reload"), !Loaded->bSound);
    }

    // Delete exactly the generated slot; the player's OutpostNative2 slot is
    // never read or written by this regression test.
    TestTrue(TEXT("Cleans up only the isolated automation slot"), UGameplayStatics::DeleteGameInSlot(Slot, 0));
    return true;
}

}

#endif
