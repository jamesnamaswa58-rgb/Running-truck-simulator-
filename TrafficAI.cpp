#include "TrafficAI.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

ATrafficAI::ATrafficAI()
{
    PrimaryActorTick.bCanEverTick = true;

    VehicleType           = ETrafficVehicleType::Car;
    CurrentBehavior       = ETrafficBehavior::Cruising;
    CurrentSpeedKPH       = 0.0f;
    DistanceToVehicleAhead = 999.0f;
    bAtRedLight           = false;
    bIndicatorLeft        = false;
    bIndicatorRight       = false;
    OvertakeTimer         = 0.0f;
    ReactionTimer         = 0.0f;
    BrakeInput            = 0.0f;
    ThrottleInput         = 0.0f;
    SteeringInput         = 0.0f;
    CurrentWaypointIndex  = 0;
    VehicleAhead          = nullptr;
}

void ATrafficAI::BeginPlay()
{
    Super::BeginPlay();

    // Randomise desired speed based on vehicle type
    switch (VehicleType)
    {
        case ETrafficVehicleType::Car:
            DesiredSpeedKPH = FMath::FRandRange(70.0f, 110.0f); break;
        case ETrafficVehicleType::Bus:
            DesiredSpeedKPH = FMath::FRandRange(60.0f,  80.0f); break;
        case ETrafficVehicleType::Truck:
            DesiredSpeedKPH = FMath::FRandRange(70.0f,  90.0f); break;
        case ETrafficVehicleType::Motorcycle:
            DesiredSpeedKPH = FMath::FRandRange(80.0f, 130.0f); break;
        case ETrafficVehicleType::EmergencyVehicle:
            DesiredSpeedKPH = FMath::FRandRange(90.0f, 140.0f); break;
    }

    // Slight reaction time variation per vehicle
    ReactionTimeSeconds += FMath::FRandRange(-0.2f, 0.3f);
}

void ATrafficAI::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    ScanForVehiclesAhead();
    ScanForTrafficSignals();

    ReactionTimer += DeltaTime;
    if (ReactionTimer >= ReactionTimeSeconds)
    {
        ReactionTimer = 0.0f;
        UpdateBehavior(DeltaTime);
    }

    ExecuteBehavior(DeltaTime);
    FollowLane(DeltaTime);
}

// ─────────────────────────────────────────────────────────────────────────────
void ATrafficAI::ScanForVehiclesAhead()
{
    const FVector Start  = GetActorLocation();
    const FVector End    = Start + GetActorForwardVector() * 80.0f;

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End,
                                              ECC_Vehicle, Params))
    {
        VehicleAhead          = Hit.GetActor();
        DistanceToVehicleAhead = Hit.Distance;
    }
    else
    {
        VehicleAhead          = nullptr;
        DistanceToVehicleAhead = 999.0f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void ATrafficAI::ScanForTrafficSignals()
{
    // Sphere overlap for traffic signal actors within braking distance
    TArray<FOverlapResult> Overlaps;
    const FVector Center = GetActorLocation();

    GetWorld()->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity,
                                       ECC_WorldStatic,
                                       FCollisionShape::MakeSphere(30.0f));
    bAtRedLight = false;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        if (Overlap.GetActor() && Overlap.GetActor()->ActorHasTag(FName("TrafficLight_Red")))
        {
            bAtRedLight = true;
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void ATrafficAI::UpdateBehavior(float DeltaTime)
{
    // Emergency: vehicle very close ahead
    if (DistanceToVehicleAhead < EmergencyBrakeDistance)
    {
        TransitionTo(ETrafficBehavior::EmergencyBraking);
        return;
    }

    // Red light stop
    if (bAtRedLight && CurrentSpeedKPH < 5.0f)
    {
        TransitionTo(ETrafficBehavior::Stopped);
        return;
    }
    if (bAtRedLight)
    {
        TransitionTo(ETrafficBehavior::Braking);
        return;
    }

    // Normal following distance
    if (VehicleAhead && DistanceToVehicleAhead < FollowDistance)
    {
        // Consider overtaking aggressive drivers
        OvertakeTimer += DeltaTime;
        if (OvertakeTimer > OvertakeCheckInterval)
        {
            OvertakeTimer = 0.0f;
            ConsiderOvertake();
        }
        else
        {
            TransitionTo(ETrafficBehavior::Braking);
        }
        return;
    }

    // Yield to player truck (always detected by player tag)
    if (VehicleAhead && VehicleAhead->ActorHasTag(FName("PlayerTruck")))
    {
        TransitionTo(ETrafficBehavior::Yielding);
        return;
    }

    TransitionTo(ETrafficBehavior::Cruising);
}

// ─────────────────────────────────────────────────────────────────────────────
void ATrafficAI::ConsiderOvertake()
{
    // Only aggressive drivers overtake, and not trucks/buses
    if (AggressionLevel < 0.4f) return;
    if (VehicleType == ETrafficVehicleType::Bus ||
        VehicleType == ETrafficVehicleType::Truck) return;

    bIndicatorRight = true;
    TransitionTo(ETrafficBehavior::Overtaking);
}

// ─────────────────────────────────────────────────────────────────────────────
void ATrafficAI::ExecuteBehavior(float DeltaTime)
{
    switch (CurrentBehavior)
    {
        case ETrafficBehavior::Cruising:
            bIndicatorLeft  = false;
            bIndicatorRight = false;
            ApplyThrottle(FMath::GetMappedRangeValueClamped(
                FVector2D(0.0f, DesiredSpeedKPH),
                FVector2D(1.0f, 0.0f),
                CurrentSpeedKPH));
            ApplyBrake(0.0f);
            break;

        case ETrafficBehavior::Braking:
        {
            const float BrakeStrength = FMath::GetMappedRangeValueClamped(
                FVector2D(FollowDistance, 0.0f),
                FVector2D(0.0f, 1.0f),
                DistanceToVehicleAhead);
            ApplyThrottle(0.0f);
            ApplyBrake(BrakeStrength);
            break;
        }

        case ETrafficBehavior::Stopped:
            ApplyThrottle(0.0f);
            ApplyBrake(1.0f);
            break;

        case ETrafficBehavior::EmergencyBraking:
            ApplyThrottle(0.0f);
            ApplyBrake(1.0f);
            break;

        case ETrafficBehavior::Overtaking:
            ApplyThrottle(1.0f);
            // Steer right (overtake lane) — handled by FollowLane with offset
            break;

        case ETrafficBehavior::Yielding:
            ApplyThrottle(0.2f);
            ApplyBrake(0.3f);
            break;

        default: break;
    }

    // Speed update from inputs (simplified — real version uses Chaos vehicle)
    const float Acceleration = (ThrottleInput * 4.0f - BrakeInput * 8.0f) * DeltaTime;
    CurrentSpeedKPH = FMath::Clamp(CurrentSpeedKPH + Acceleration * 3.6f,
                                    0.0f, DesiredSpeedKPH * 1.1f);
}

// ─────────────────────────────────────────────────────────────────────────────
void ATrafficAI::FollowLane(float DeltaTime)
{
    if (WaypointPath.Num() == 0) return;

    if (HasReachedWaypoint()) AdvanceWaypoint();

    SteerTowards(GetNextWaypoint(), DeltaTime);
}

void ATrafficAI::SteerTowards(FVector TargetPoint, float DeltaTime)
{
    const FVector ToTarget   = (TargetPoint - GetActorLocation()).GetSafeNormal();
    const FVector Forward    = GetActorForwardVector();
    const float   DotProduct = FVector::DotProduct(Forward, ToTarget);
    const FVector CrossProduct = FVector::CrossProduct(Forward, ToTarget);

    SteeringInput = FMath::Clamp(CrossProduct.Z * 2.0f, -1.0f, 1.0f);
}

void ATrafficAI::ApplyThrottle(float Amount) { ThrottleInput = FMath::Clamp(Amount, 0.0f, 1.0f); }
void ATrafficAI::ApplyBrake(float Amount)    { BrakeInput    = FMath::Clamp(Amount, 0.0f, 1.0f); }

void ATrafficAI::TransitionTo(ETrafficBehavior NewBehavior)
{
    if (CurrentBehavior == NewBehavior) return;
    CurrentBehavior = NewBehavior;
}

FVector ATrafficAI::GetNextWaypoint() const
{
    if (WaypointPath.IsValidIndex(CurrentWaypointIndex))
        return WaypointPath[CurrentWaypointIndex];
    return GetActorLocation();
}

bool ATrafficAI::HasReachedWaypoint() const
{
    return FVector::Dist(GetActorLocation(), GetNextWaypoint()) < WaypointReachRadius;
}

void ATrafficAI::AdvanceWaypoint()
{
    CurrentWaypointIndex = (CurrentWaypointIndex + 1) % WaypointPath.Num();
}
