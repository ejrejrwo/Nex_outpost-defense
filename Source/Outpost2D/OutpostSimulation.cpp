#include "OutpostSimulation.h"
#include "Algo/Reverse.h"

namespace Outpost
{
namespace
{
const FIntPoint Doors[] = { FIntPoint(1, 8), FIntPoint(26, 8) };
const int32 Counts[] = { 7, 10, 13 }, Costs[] = { 10, 14 };
const float EnemyHealth[] = { 70, 92, 118 }, EnemySpeed[] = { 102, 116, 130 }, SpawnIntervals[] = { .9f, .75f, .62f };
using K = EEnemyKind;
const K Rosters[3][13] = {
    { K::Grunt, K::Grunt, K::Runner, K::Grunt, K::Runner, K::Grunt, K::Brute },
    { K::Runner, K::Grunt, K::Runner, K::Brute, K::Grunt, K::Runner, K::Grunt, K::Brute, K::Runner, K::Grunt },
    { K::Runner, K::Brute, K::Runner, K::Grunt, K::Runner, K::Brute, K::Runner, K::Grunt, K::Runner, K::Brute, K::Runner, K::Grunt, K::Boss }
};
bool Inside(int32 X, int32 Y) { return X >= 0 && Y >= 0 && X < Cols && Y < Rows; }
int32 Index(int32 X, int32 Y) { return Y * Cols + X; }
float Distance(FVector2D A, FVector2D B) { return static_cast<float>(FVector2D::Distance(A, B)); }
float SegmentHit(FVector2D A, FVector2D B, FVector2D P, float Radius)
{
    const FVector2D D = B - A;
    const double T = D.SizeSquared() > 0 ? FMath::Clamp(FVector2D::DotProduct(P - A, D) / D.SizeSquared(), 0.0, 1.0) : 0;
    return Distance(A + D * T, P) <= Radius ? static_cast<float>(T) : -1.f;
}
}

FIntPoint FSimulation::Cell(FVector2D P) { return FIntPoint(FMath::FloorToInt(P.X / Tile), FMath::FloorToInt(P.Y / Tile)); }
FVector2D FSimulation::Center(FIntPoint P) { return FVector2D((P.X + .5f) * Tile, (P.Y + .5f) * Tile); }
bool FSimulation::CircleRect(FVector2D P, float Radius, FRect R)
{
    const FVector2D Closest(FMath::Clamp(P.X, double(R.X * Tile), double((R.X + R.W) * Tile)),
        FMath::Clamp(P.Y, double(R.Y * Tile), double((R.Y + R.H) * Tile)));
    return FVector2D::DistSquared(P, Closest) < Radius * Radius;
}

FSimulation::FSimulation() { Reset(); Phase = EPhase::Menu; }
void FSimulation::Reset(EMode NewMode)
{
    Mode = NewMode;
    Phase = EPhase::Prep; bPaused = false; Time = 0; PrepRemaining = PrepSeconds; CombatTime = 0;
    Player = Center(FIntPoint(14, 10)); Aim = Player + FVector2D(0, -80); PlayerHP = 100; HurtRemaining = 0;
    Tool = 1; Ore = Rank = Kills = Wave = Spawned = 0; NextEnemyId = 1;
    ArmorRank = JobCost = 0; MaxHP = 100; DashRemaining = DashCooldown = PlayerTraveled = 0;
    DashVector = FVector2D::ZeroVector; JobKind = EUpgradeKind::Weapon;
    Forge = { 0, 13, 2, 2, 2 };
    Mines = { { { 0, 5, 4, 1, 1 }, 120 }, { { 1, 22, 4, 1, 1 }, 120 } };
    Walls = { { 1, 9, 7, 1, 3 }, { 2, 18, 7, 1, 3 }, { 3, 9, 12, 3, 1 }, { 4, 16, 12, 3, 1 } };
    bCarrying = bAutoMine = bMining = false; JobRemaining = MineProgress = RestRemaining = Cooldown = SpawnClock = 0;
    ActiveMine = -1; MoveOrder.Reset(); Enemies.Reset(); Bullets.Reset(); Events.Reset(); Stats = FStats(); GridVersion = 0;
    Rebuild(); Message(TEXT("30초 준비 · 첫 강화는 광석 10개 + 5초 · 2번 곡괭이로 채굴하세요."));
}
void FSimulation::Emit(EEvent Type, FVector2D Pos) { Events.Add({ Type, Pos, FString() }); }
void FSimulation::Message(const FString& Text) { Events.Add({ EEvent::Message, Player, Text }); }

TArray<uint8> FSimulation::MakeGrid(const FRect* Extra) const
{
    TArray<uint8> Map; Map.Init(0, Cols * Rows);
    for (int32 Y = 0; Y < Rows; ++Y) for (int32 X = 0; X < Cols; ++X)
        if (!X || !Y || X == Cols - 1 || Y == Rows - 1) Map[Index(X, Y)] = 1;
    const auto Block = [&Map](FRect R)
    {
        for (int32 Y = R.Y; Y < R.Y + R.H; ++Y) for (int32 X = R.X; X < R.X + R.W; ++X)
            if (Inside(X, Y)) Map[Index(X, Y)] = 1;
    };
    Block(Forge); for (const FMine& M : Mines) Block(M.Rect); for (FRect R : Walls) Block(R); if (Extra) Block(*Extra);
    return Map;
}
void FSimulation::Rebuild() { Grid = MakeGrid(); ++GridVersion; }
TArray<FIntPoint> FSimulation::FindPath(const TArray<uint8>& Map, FIntPoint Start, FIntPoint Goal) const
{
    TArray<FIntPoint> Path;
    if (!Inside(Start.X, Start.Y) || !Inside(Goal.X, Goal.Y) || Map.Num() != Cols * Rows) return Path;
    const int32 First = Index(Start.X, Start.Y), Last = Index(Goal.X, Goal.Y);
    if (Map[First] || Map[Last]) return Path;
    int32 Previous[Cols * Rows], Queue[Cols * Rows];
    for (int32& P : Previous) P = -1;
    int32 Head = 0, Tail = 1; Queue[0] = First; Previous[First] = First;
    const FIntPoint Directions[] = { FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0), FIntPoint(0, -1) };
    while (Head < Tail && Previous[Last] == -1)
    {
        const int32 Current = Queue[Head++]; const FIntPoint C(Current % Cols, Current / Cols);
        for (FIntPoint D : Directions)
        {
            const FIntPoint N = C + D; if (!Inside(N.X, N.Y)) continue;
            const int32 Next = Index(N.X, N.Y);
            if (!Map[Next] && Previous[Next] == -1) { Previous[Next] = Current; Queue[Tail++] = Next; }
        }
    }
    if (Previous[Last] == -1) return Path;
    for (int32 At = Last; ; At = Previous[At]) { Path.Add(FIntPoint(At % Cols, At / Cols)); if (At == First) break; }
    Algo::Reverse(Path);
    return Path;
}
bool FSimulation::SolidAt(FVector2D P, float Radius) const
{
    for (int32 Y = FMath::FloorToInt((P.Y - Radius) / Tile); Y <= FMath::FloorToInt((P.Y + Radius) / Tile); ++Y)
        for (int32 X = FMath::FloorToInt((P.X - Radius) / Tile); X <= FMath::FloorToInt((P.X + Radius) / Tile); ++X)
            if ((!Inside(X, Y) || Grid[Index(X, Y)]) && CircleRect(P, Radius, { 0, X, Y, 1, 1 })) return true;
    return false;
}
void FSimulation::Move(FVector2D& P, float Radius, FVector2D Delta) const
{
    // Substeps also keep a fast dash from tunnelling through a one-tile wall.
    const int32 Steps = FMath::Max(1, FMath::CeilToInt(Delta.Size() / 5));
    Delta /= Steps;
    for (int32 I = 0; I < Steps; ++I)
    {
        if (!SolidAt(P + FVector2D(Delta.X, 0), Radius)) P.X += Delta.X;
        if (!SolidAt(P + FVector2D(0, Delta.Y), Radius)) P.Y += Delta.Y;
    }
}
bool FSimulation::ClearTravel(FVector2D From, FVector2D To, float Radius) const
{
    const FVector2D D = To - From;
    for (int32 Y = FMath::FloorToInt((FMath::Min(From.Y, To.Y) - Radius) / Tile); Y <= FMath::FloorToInt((FMath::Max(From.Y, To.Y) + Radius) / Tile); ++Y)
        for (int32 X = FMath::FloorToInt((FMath::Min(From.X, To.X) - Radius) / Tile); X <= FMath::FloorToInt((FMath::Max(From.X, To.X) + Radius) / Tile); ++X)
        {
            if (Inside(X, Y) && !Grid[Index(X, Y)]) continue;
            double Enter = 0, Leave = 1;
            const auto Slab = [&Enter, &Leave](double Origin, double Delta, double Low, double High)
            {
                if (FMath::Abs(Delta) < 1e-9) { if (Origin <= Low || Origin >= High) Enter = 2; }
                else
                {
                    const double A = (Low - Origin) / Delta, B = (High - Origin) / Delta;
                    Enter = FMath::Max(Enter, FMath::Min(A, B)); Leave = FMath::Min(Leave, FMath::Max(A, B));
                }
            };
            Slab(From.X, D.X, X * Tile - Radius, (X + 1) * Tile + Radius);
            Slab(From.Y, D.Y, Y * Tile - Radius, (Y + 1) * Tile + Radius);
            if (Enter < Leave) return false;
        }
    return true;
}

bool FSimulation::SetTool(int32 Value)
{
    if (!CanWork() || Value < 1 || Value > 3 || JobRemaining > 0 || bCarrying || DashRemaining > 0) return false;
    Tool = Value; MineProgress = 0; ActiveMine = -1; bAutoMine = bMining = false; return true;
}
bool FSimulation::MoveTo(FVector2D Point)
{
    if (!CanWork() || JobRemaining > 0 || DashRemaining > 0) return false;
    FIntPoint Target = Cell(Point); if (!Inside(Target.X, Target.Y)) return false;
    if (Grid[Index(Target.X, Target.Y)])
    {
        TArray<FIntPoint> Options;
        for (int32 Y = Target.Y - 2; Y <= Target.Y + 2; ++Y) for (int32 X = Target.X - 2; X <= Target.X + 2; ++X)
            if (Inside(X, Y) && !Grid[Index(X, Y)]) Options.Add(FIntPoint(X, Y));
        Options.Sort([Point, this](FIntPoint A, FIntPoint B)
        {
            const double DA = FVector2D::DistSquared(Center(A), Point), DB = FVector2D::DistSquared(Center(B), Point);
            return FMath::IsNearlyEqual(DA, DB) ? FVector2D::DistSquared(Center(A), Player) < FVector2D::DistSquared(Center(B), Player) : DA < DB;
        });
        bool bFound = false;
        for (FIntPoint Option : Options) if (FindPath(Grid, Cell(Player), Option).Num()) { Target = Option; bFound = true; break; }
        if (!bFound) return false;
    }
    const TArray<FIntPoint> Route = FindPath(Grid, Cell(Player), Target); if (Route.IsEmpty()) return false;
    MoveOrder.Reset(); for (FIntPoint P : Route) MoveOrder.Add(Center(P)); bAutoMine = false; return true;
}
int32 FSimulation::UpgradeCost() const { return Rank < MaxRank ? Costs[Rank] : 0; }
bool FSimulation::NearForge() const { return Distance(Player, Forge.Center()) < 100; }
int32 FSimulation::NearestMine() const
{
    int32 Best = -1; float BestDistance = 70;
    for (int32 I = 0; I < Mines.Num(); ++I)
    {
        const float D = Distance(Player, Mines[I].Rect.Center());
        if (Mines[I].Remaining > 0 && D < BestDistance) { Best = I; BestDistance = D; }
    }
    return Best;
}
bool FSimulation::CanWork() const
{
    return !bPaused && (Phase == EPhase::Prep || Phase == EPhase::Combat || Phase == EPhase::Rest);
}
bool FSimulation::StartUpgrade(EUpgradeKind Kind)
{
    if (!CanWork() || JobRemaining > 0 || bCarrying || DashRemaining > 0 || !NearForge()) return false;
    if ((Kind == EUpgradeKind::Weapon && Rank >= MaxRank) || (Kind == EUpgradeKind::Armor && ArmorRank >= 1))
    { Message(TEXT("이 장비는 최대 강화입니다.")); return false; }
    if (Phase == EPhase::Prep && PrepRemaining < ForgeSeconds) { Message(TEXT("준비 중 강화에는 5초가 필요합니다. 전투 중에도 강화할 수 있습니다.")); return false; }
    const int32 Cost = Kind == EUpgradeKind::Armor ? 10 : UpgradeCost();
    if (Ore < Cost) { Message(FString::Printf(TEXT("광석 %d개가 필요합니다."), Cost)); return false; }
    Ore -= Cost; JobCost = Cost; JobKind = Kind; JobRemaining = ForgeSeconds;
    bAutoMine = bMining = false; MineProgress = 0; MoveOrder.Reset(); Emit(EEvent::Forge, Forge.Center());
    Message(TEXT("5초 동안 이동·사격 불가 · 위험하면 E로 취소 / 광석 반환")); return true;
}
bool FSimulation::PickupWall()
{
    if (!CanWork() || Tool != 3 || bCarrying || JobRemaining > 0 || DashRemaining > 0) return false;
    int32 Best = -1; float Nearest = 100;
    for (int32 I = 0; I < Walls.Num(); ++I)
    {
        const float D = Distance(Player, Walls[I].Center()); if (D < Nearest) { Best = I; Nearest = D; }
    }
    if (Best < 0) { Message(TEXT("방벽 가까이에서 E 또는 좌클릭으로 들어 올리세요.")); return false; }
    Carry = CarryOrigin = Walls[Best]; Walls.RemoveAt(Best); bCarrying = true; MoveOrder.Reset(); Rebuild(); Emit(EEvent::Lift, Player);
    Message(TEXT("빈 바닥을 가리켜 놓기 · R 회전")); return true;
}
void FSimulation::RotateWall() { if (bCarrying && CanWork()) Swap(Carry.W, Carry.H); }
FPlacement FSimulation::Placement(FVector2D Point) const
{
    FPlacement Result;
    if (!bCarrying) { Result.Reason = TEXT("들고 있는 방벽이 없습니다."); return Result; }
    Result.Rect = { Carry.Id, int32(FMath::FloorToInt(Point.X / Tile)) - Carry.W / 2, int32(FMath::FloorToInt(Point.Y / Tile)) - Carry.H / 2, Carry.W, Carry.H };
    const FRect R = Result.Rect;
    if (Distance(Player, R.Center()) > 120) { Result.Reason = TEXT("조금 더 가까이 이동하세요."); return Result; }
    if (R.X < 2 || R.Y < 1 || R.X + R.W > Cols - 2 || R.Y + R.H > Rows - 1) { Result.Reason = TEXT("출입구와 경계를 비워주세요."); return Result; }
    for (int32 Y = R.Y; Y < R.Y + R.H; ++Y) for (int32 X = R.X; X < R.X + R.W; ++X)
        if (Grid[Index(X, Y)]) { Result.Reason = TEXT("다른 물체와 겹칩니다."); return Result; }
    if (CircleRect(Player, 13, R)) { Result.Reason = TEXT("플레이어가 서 있는 곳입니다."); return Result; }
    for (const FEnemy& E : Enemies) if (E.HP > 0 && CircleRect(E.Pos, E.Radius + 2, R))
    { Result.Reason = TEXT("적과 겹치는 곳에는 놓을 수 없습니다."); return Result; }
    const TArray<uint8> Trial = MakeGrid(&R); const FIntPoint Target = Cell(Player);
    for (FIntPoint Door : Doors) if (FindPath(Trial, Door, Target).IsEmpty()) { Result.Reason = TEXT("적이 돌아올 통로를 남겨주세요."); return Result; }
    for (const FEnemy& E : Enemies) if (E.HP > 0 && FindPath(Trial, Cell(E.Pos), Target).IsEmpty())
    { Result.Reason = TEXT("싸우고 있는 적의 통로를 남겨주세요."); return Result; }
    TArray<FRect> Objects = { Forge }; for (const FMine& M : Mines) Objects.Add(M.Rect);
    for (FRect Object : Objects)
    {
        TArray<FIntPoint> Edges;
        for (int32 X = Object.X; X < Object.X + Object.W; ++X) { Edges.Add(FIntPoint(X, Object.Y - 1)); Edges.Add(FIntPoint(X, Object.Y + Object.H)); }
        for (int32 Y = Object.Y; Y < Object.Y + Object.H; ++Y) { Edges.Add(FIntPoint(Object.X - 1, Y)); Edges.Add(FIntPoint(Object.X + Object.W, Y)); }
        bool bReachable = false;
        for (FIntPoint Edge : Edges) if (FindPath(Trial, Target, Edge).Num()) { bReachable = true; break; }
        if (!bReachable) { Result.Reason = TEXT("광석과 대장간의 길을 남겨주세요."); return Result; }
    }
    Result.bValid = true; return Result;
}
bool FSimulation::PlaceWall(FVector2D Point)
{
    if (!CanWork() || JobRemaining > 0 || DashRemaining > 0) return false;
    const FPlacement P = Placement(Point); if (!P.bValid) { Message(P.Reason); return false; }
    Walls.Add(P.Rect); bCarrying = false; MoveOrder.Reset(); ++Stats.WallMoves; Rebuild(); Emit(EEvent::Place, P.Rect.Center());
    if (Phase == EPhase::Combat) ++Stats.CombatWallMoves;
    Message(TEXT("방벽 배치 완료 · 점선으로 적의 경로를 확인하세요.")); return true;
}
void FSimulation::ReturnCarry()
{
    if (!bCarrying) return;
    Walls.Add(CarryOrigin); bCarrying = false; Rebuild();
    const auto Reachable = [this](FIntPoint P) { for (FIntPoint D : Doors) if (FindPath(Grid, D, P).IsEmpty()) return false; return true; };
    if (SolidAt(Player, 11) || !Reachable(Cell(Player)))
    {
        float Nearest = TNumericLimits<float>::Max(); FVector2D Best = Player;
        for (int32 Y = 1; Y < Rows - 1; ++Y) for (int32 X = 1; X < Cols - 1; ++X)
        {
            const FIntPoint C(X, Y); const float D = Distance(Center(C), Player);
            if (!Grid[Index(X, Y)] && D < Nearest && Reachable(C)) { Nearest = D; Best = Center(C); }
        }
        Player = Best;
    }
}
void FSimulation::Interact()
{
    if (!CanWork() || DashRemaining > 0) return;
    if (JobRemaining > 0)
    {
        Ore += JobCost; JobCost = 0; JobRemaining = 0;
        Message(TEXT("강화 취소 · 광석을 반환했습니다.")); return;
    }
    if (bCarrying) PlaceWall(Aim);
    else if (NearForge()) StartUpgrade();
    else if (Tool == 3) PickupWall();
    else if (Tool == 2 && NearestMine() >= 0) { bAutoMine = !bAutoMine; MoveOrder.Reset(); }
}
bool FSimulation::BeginCombat()
{
    if (Phase != EPhase::Prep || bPaused) return false;
    if (JobRemaining > 0) { Message(TEXT("진행 중인 강화가 끝나면 전투를 시작할 수 있습니다.")); return false; }
    ReturnCarry(); Tool = 1; MineProgress = 0; bAutoMine = bMining = false; MoveOrder.Reset(); Wave = 1; BeginWave(); return true;
}
void FSimulation::BeginWave()
{
    Phase = EPhase::Combat; Spawned = 0; SpawnClock = 1.2f; Emit(EEvent::Wave);
    Message(FString::Printf(TEXT("공세 %d / 3 · 방벽을 사이에 두고 싸우세요."), Wave));
}
void FSimulation::SpawnEnemy()
{
    const int32 W = FMath::Clamp(Wave - 1, 0, 2);
    if (Spawned >= Counts[W]) return;
    FEnemy E; E.Id = NextEnemyId++; E.Pos = Center(Doors[Spawned % 2]); E.Kind = Rosters[W][Spawned];
    float Health = 1, Speed = 1;
    if (E.Kind == K::Runner) { Health = .65f; Speed = 1.34f; E.Radius = 10; E.AttackDuration = .30f; E.AttackReach = 53; E.AttackRadius = 47; E.Damage = 16; }
    else if (E.Kind == K::Brute) { Health = 1.9f; Speed = .78f; E.Radius = 15; E.AttackDuration = .65f; E.AttackReach = 78; E.AttackRadius = 84; E.Damage = 32; }
    else if (E.Kind == K::Boss)
    {
        Health = 5.5f; Speed = .82f; E.Radius = 16; E.AttackDuration = .85f; E.AttackReach = 155; E.AttackRadius = 100; E.Damage = 38;
        Emit(EEvent::Boss, E.Pos); Message(TEXT("BREAKER 출현 · 붉은 원 밖으로 이동 / Space 회피 · 체력 절반부터 폭주"));
    }
    E.HP = E.MaxHP = FMath::RoundToFloat(EnemyHealth[W] * Health * (Mode == EMode::Practice ? .72f : 1.f));
    E.Speed = EnemySpeed[W] * Speed * (Mode == EMode::Practice ? .86f : 1.f);
    E.Damage = FMath::RoundToFloat(E.Damage * (Mode == EMode::Practice ? .6f : 1.f));
    Enemies.Add(E); ++Spawned;
}
bool FSimulation::Fire()
{
    if (Phase != EPhase::Combat || Tool != 1 || Cooldown > 0 || bPaused || JobRemaining > 0 || bCarrying || DashRemaining > 0) return false;
    const FVector2D Direction = (Aim - Player).GetSafeNormal();
    FBullet B; B.Origin = Player; B.Pos = Player + Direction * 20; B.Velocity = Direction * 610; B.Damage = 18 + Rank * 10;
    Bullets.Add(B); Cooldown = .24f; ++Stats.Shots; Emit(EEvent::Shot, Player); return true;
}

bool FSimulation::StartDash(FVector2D Direction)
{
    if (Phase != EPhase::Combat || bPaused || DashCooldown > 0 || JobRemaining > 0 || bCarrying) return false;
    if (Direction.IsNearlyZero()) Direction = Aim - Player;
    if (Direction.IsNearlyZero()) return false;
    DashVector = Direction.GetSafeNormal(); DashRemaining = DashSeconds; DashCooldown = DashRecovery;
    MoveOrder.Reset(); bAutoMine = bMining = false; MineProgress = 0;
    ++Stats.Dashes; Emit(EEvent::Dash, Player); return true;
}
int32 FSimulation::Score() const
{
    const float ClearBonus = Phase == EPhase::Won ? 1500 + PlayerHP * 10 + FMath::Max(0.f, 180 - CombatTime) * 15 + Ore * 10 : 0;
    return FMath::FloorToInt(Kills * 100 + ClearBonus);
}
void FSimulation::Finish(EPhase Result)
{
    Ore += JobCost; JobCost = 0; JobRemaining = 0;
    Phase = Result; bAutoMine = bMining = false; MoveOrder.Reset(); DashRemaining = 0;
    Emit(Result == EPhase::Won ? EEvent::Win : EEvent::Lose, Player);
}
void FSimulation::Tick(float Dt, const FInput& Input)
{
    if (!CanWork()) return;
    Dt = FMath::Clamp(Dt, 0.f, .05f); Time += Dt; Cooldown -= Dt; HurtRemaining = FMath::Max(0.f, HurtRemaining - Dt); Aim = Input.Aim;
    DashCooldown = FMath::Max(0.f, DashCooldown - Dt);
    if (Input.bDash) StartDash(Input.Move);
    const FVector2D Before = Player;
    const bool bMoving = !Input.Move.IsNearlyZero();
    if (DashRemaining > 0)
    {
        Move(Player, 11, DashVector * DashSpeed * FMath::Min(Dt, DashRemaining));
        DashRemaining = FMath::Max(0.f, DashRemaining - Dt);
    }
    else if (bMoving && JobRemaining <= 0)
    {
        MoveOrder.Reset(); bAutoMine = false; Move(Player, 11, Input.Move.GetSafeNormal() * (bCarrying ? 112 : 160) * Dt);
    }
    else if (MoveOrder.Num() && JobRemaining <= 0)
    {
        const FVector2D Target = MoveOrder[0]; const float D = Distance(Player, Target);
        if (D > 1) Move(Player, 11, (Target - Player).GetSafeNormal() * FMath::Min(D, (bCarrying ? 112.f : 160.f) * Dt));
        if (D < 3) MoveOrder.RemoveAt(0);
    }
    PlayerTraveled += Distance(Before, Player);
    bMining = false;
    if (bCarrying) Stats.CarryTime += Dt;
    // Work is independent of the phase clock: a forge job never freezes combat.
    if (JobRemaining > 0)
    {
        Stats.ForgeTime += FMath::Min(Dt, JobRemaining); JobRemaining -= Dt;
        if (JobRemaining <= .0001f)
        {
            JobRemaining = 0; JobCost = 0;
            if (JobKind == EUpgradeKind::Armor) { ++ArmorRank; MaxHP += 40; PlayerHP += 40; Message(TEXT("보호구 완료 · 최대 체력 140")); }
            else { ++Rank; Message(FString::Printf(TEXT("화력 %d단계 완료 · 탄환 피해 %d"), Rank, 18 + Rank * 10)); }
            Emit(EEvent::Upgrade, Forge.Center());
        }
    }
    else if (Tool == 2 && (Input.bUse || bAutoMine) && !bCarrying && DashRemaining <= 0 && !bMoving && MoveOrder.IsEmpty())
    {
        const int32 M = NearestMine(); if (M != ActiveMine) MineProgress = 0; ActiveMine = M;
        if (M >= 0)
        {
            bMining = true; Stats.MiningTime += Dt; MineProgress += Dt;
            while (MineProgress + .00001f >= MineSeconds && Mines[M].Remaining > 0)
            { MineProgress = FMath::Max(0.f, MineProgress - MineSeconds); --Mines[M].Remaining; ++Ore; ++Stats.OreMined;
                if (Phase == EPhase::Combat) ++Stats.CombatOreMined;
                Emit(EEvent::Mine, Mines[M].Rect.Center()); }
        }
        else { MineProgress = 0; bAutoMine = false; }
    }
    else { MineProgress = 0; ActiveMine = -1; }
    if (Phase == EPhase::Prep)
    {
        PrepRemaining = FMath::Max(0.f, PrepRemaining - Dt);
        if (PrepRemaining <= .0001f && JobRemaining <= 0) BeginCombat();
    }
    else if (Phase == EPhase::Rest)
    {
        RestRemaining -= Dt; if (RestRemaining <= 0) { ++Wave; BeginWave(); }
    }
    else
    {
        CombatTime += Dt; if (Input.bUse) Fire(); SpawnClock -= Dt;
        if (SpawnClock <= 0 && Spawned < Counts[Wave - 1] && Enemies.Num() < 12) { SpawnEnemy(); SpawnClock = SpawnIntervals[Wave - 1]; }
        UpdateBullets(Dt); UpdateEnemies(Dt);
        if (PlayerHP <= 0) Finish(EPhase::Lost);
        else if (Spawned == Counts[Wave - 1] && Enemies.IsEmpty())
        {
            if (Wave == 3) Finish(EPhase::Won);
            else { Phase = EPhase::Rest; RestRemaining = 3; Bullets.Reset(); PlayerHP = FMath::Min(MaxHP, PlayerHP + 5); Message(TEXT("공세 격퇴 · 체력 5 회복 · 3초 재정비 중에도 작업 가능")); }
        }
    }
}
void FSimulation::UpdateBullets(float Dt)
{
    for (FBullet& B : Bullets)
    {
        const FVector2D Next = B.Pos + B.Velocity * Dt, From = B.bFirst ? B.Origin : B.Pos; B.bFirst = false;
        int32 Target = -1; float Nearest = TNumericLimits<float>::Max();
        for (int32 I = 0; I < Enemies.Num(); ++I)
        {
            if (Enemies[I].HP <= 0) continue;
            const float T = SegmentHit(From, Next, Enemies[I].Pos, Enemies[I].Radius + 4); if (T >= 0 && T < Nearest) { Nearest = T; Target = I; }
        }
        if (Target >= 0)
        {
            FEnemy& E = Enemies[Target]; E.HP -= B.Damage; E.Flash = .14f; B.Life = 0; ++Stats.Hits; Emit(EEvent::Hit, E.Pos);
            if (E.HP <= 0) { ++Kills; Emit(EEvent::Kill, E.Pos); }
        }
        B.Pos = Next; B.Life -= Dt;
    }
    Bullets.RemoveAll([](const FBullet& B) { return B.Life <= 0 || B.Pos.X < 0 || B.Pos.X > Width || B.Pos.Y < 0 || B.Pos.Y > Height; });
    Enemies.RemoveAll([](const FEnemy& E) { return E.HP <= 0; });
}
void FSimulation::UpdateEnemies(float Dt)
{
    for (FEnemy& E : Enemies)
    {
        if (E.HP <= 0) continue;
        E.Flash = FMath::Max(0.f, E.Flash - Dt);
        E.AttackCooldown = FMath::Max(0.f, E.AttackCooldown - Dt);
        if (E.AttackRemaining > 0)
        {
            E.AttackRemaining -= Dt;
            if (E.AttackRemaining <= .0001f)
            {
                // The target is locked at windup start. A visible warning can be escaped.
                Events.Add({ EEvent::Strike, E.AttackPoint, FString(), E.AttackRadius });
                if (Distance(Player, E.AttackPoint) < E.AttackRadius + 11 && HurtRemaining <= 0 && DashRemaining <= 0 && ClearTravel(E.Pos, Player, 0))
                {
                    const float Damage = FMath::Min(PlayerHP, E.Damage); PlayerHP -= Damage;
                    Stats.DamageTaken += Damage; HurtRemaining = .65f; Emit(EEvent::Hurt, Player);
                }
                E.AttackRemaining = 0; E.AttackCooldown = E.Kind == K::Boss ? .85f : .65f;
            }
            continue;
        }
        if (E.AttackCooldown <= 0 && Distance(E.Pos, Player) < E.AttackReach && ClearTravel(E.Pos, Player, 0))
        {
            if (E.Kind == K::Boss)
            {
                const bool bEnraged = E.HP < E.MaxHP * .5f;
                E.AttackDuration = bEnraged ? .62f : .85f; E.AttackRadius = bEnraged ? 86 : 100;
            }
            E.AttackPoint = Player; E.AttackRemaining = E.AttackDuration; E.Facing = (Player - E.Pos).GetSafeNormal(); continue;
        }
        FVector2D Target = E.Pos; bool bTarget = false;
        if (ClearTravel(E.Pos, Player, E.Radius)) { E.Path.Reset(); Target = Player; bTarget = true; }
        else
        {
            const FIntPoint Goal = Cell(Player); const int32 Key = Index(Goal.X, Goal.Y);
            if (E.Version != GridVersion || E.Target != Key || E.Path.IsEmpty())
            {
                const TArray<FIntPoint> Route = FindPath(Grid, Cell(E.Pos), Goal);
                E.Path.Reset(); for (FIntPoint C : Route) E.Path.Add(Center(C)); E.Version = GridVersion; E.Target = Key;
            }
            // Route updates must not send a moving enemy back to a previous cell centre.
            for (int32 I = E.Path.Num() - 1; I > 0; --I) if (ClearTravel(E.Pos, E.Path[I], E.Radius)) { E.Path.RemoveAt(0, I); break; }
            while (E.Path.Num() && Distance(E.Pos, E.Path[0]) < 1) E.Path.RemoveAt(0);
            if (E.Path.Num()) { Target = E.Path[0]; bTarget = true; }
        }
        if (bTarget)
        {
            const float D = Distance(E.Pos, Target); const FVector2D Before = E.Pos;
            const float Speed = E.Speed * (E.Kind == K::Boss && E.HP < E.MaxHP * .5f ? 1.18f : 1.f);
            if (D > 1) Move(E.Pos, E.Radius, (Target - E.Pos).GetSafeNormal() * FMath::Min(D, Speed * Dt));
            E.Traveled += Distance(E.Pos, Before);
            if (Distance(E.Pos, Before) > .001f) E.Facing = (E.Pos - Before).GetSafeNormal();
            if (E.Path.Num() && Distance(E.Pos, Target) < 1) E.Path.RemoveAt(0);
        }
    }
    // Local separation makes individual silhouettes readable without blocking paths.
    for (int32 I = 0; I < Enemies.Num(); ++I) for (int32 J = I + 1; J < Enemies.Num(); ++J)
    {
        FEnemy& A = Enemies[I]; FEnemy& B = Enemies[J]; const float D = Distance(A.Pos, B.Pos), Gap = (A.Radius + B.Radius) * .85f;
        if (D < Gap && A.AttackRemaining <= 0 && B.AttackRemaining <= 0)
        {
            const FVector2D Direction = D > .01f ? (A.Pos - B.Pos) / D : FVector2D(0, I % 2 ? 1 : -1);
            const float Push = FMath::Min((Gap - D) * .5f, 28 * Dt);
            Move(A.Pos, A.Radius, Direction * Push); Move(B.Pos, B.Radius, -Direction * Push);
        }
    }
}
FString FSimulation::Hint() const
{
    if (bPaused) return TEXT("일시정지 · Esc 계속하기");
    if (Phase == EPhase::Menu) return TEXT("30초 준비 · 채굴, 강화, 방벽에 시간을 배분하세요.");
    if (JobRemaining > 0) return FString::Printf(TEXT("%s 강화 중 %.1f초 · E 취소 / 광석 반환"), JobKind == EUpgradeKind::Armor ? TEXT("보호구") : TEXT("화력"), JobRemaining);
    if (Phase == EPhase::Won || Phase == EPhase::Lost) return TEXT("다른 빌드로 다시 도전해 보세요.");
    if (bCarrying) return TEXT("운반 중 · 사격·회피 불가 · 가까운 바닥에 좌클릭 / E 놓기 · R 회전");
    if (NearForge()) return FString::Printf(TEXT("대장간 · E 화력 %s · F 보호구 %s · 각각 5초"), Rank >= MaxRank ? TEXT("MAX") : *FString::Printf(TEXT("%d광석"), UpgradeCost()), ArmorRank ? TEXT("MAX") : TEXT("10광석"));
    if (Tool == 2 && NearestMine() >= 0) return bAutoMine ? TEXT("채굴 중 · E 또는 이동으로 중지") : TEXT("E 채굴 시작 / 중지 · 좌클릭을 눌러 채굴할 수도 있습니다");
    if (Tool == 3) return TEXT("방벽 가까이에서 E / 좌클릭으로 들기 · 점선은 적의 예상 경로");
    if (Tool == 2) return TEXT("청록색 광석으로 우클릭 이동 · 가까이에서 E 채굴");
    if (Phase == EPhase::Rest) return TEXT("3초 재정비 · 채굴·강화·방벽 이동을 계속할 수 있습니다.");
    if (Phase == EPhase::Combat) return TEXT("좌클릭 사격 · Space 회피 · 전투 중에도 2 채굴 / 3 방벽 이동");
    return TEXT("1 무기 · 2 곡괭이 · 3 맨손 · 대장간에서 E 강화");
}
}
