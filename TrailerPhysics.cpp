#include "TrailerPhysics.h"

UTrailerPhysics::UTrailerPhysics()
{
    PrimaryComponentTick.bCanEverTick = true;

    TrailerType              = ETrailerType::Container;
    TrailerMassKg            = 20000.0f;
    ArticulationAngleDeg     = 0.0f;
    SwayAngleDeg             = 0.0f;
    SwayVelocity             = 0.0f;
    bJackknifing             = false;
    bIsAttached              = false;
    TrailerBrakeForce        = 0.0f;
    PreviousArticulationAngle = 0.0f;
}

void UTrailerPhysics::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UTrailerPhysics::AttachTrailer(ETrailerType Type, float MassKg)
{
    TrailerType   = Type;
    TrailerMassKg = MassKg;
    bIsAttached   = true;
    UE_LOG(LogTemp, Log, TEXT("Trailer attached: %.0f kg"), MassKg);
}

void UTrailerPhysics::DetachTrailer()
{
    bIsAttached          = false;
    ArticulationAngleDeg = 0.0f;
    SwayAngleDeg         = 0.0f;
    bJackknifing         = false;
    UE_LOG(LogTemp, Log, TEXT("Trailer detached"));
}

void UTrailerPhysics::UpdateTrailerPhysics(float DeltaTime, float TruckSpeedKPH,
                                            float SteeringAngle, float BrakeInput)
{
    if (!bIsAttached) return;

    SimulateArticulation(DeltaTime, SteeringAngle, TruckSpeedKPH);
    SimulateSway(DeltaTime, TruckSpeedKPH);
    DetectJackknife();

    // Trailer brake contribution
    TrailerBrakeForce = BrakeInput * TrailerMassKg * 9.81f * 0.6f;
}

void UTrailerPhysics::SimulateArticulation(float DeltaTime, float SteeringAngle,
                                            float SpeedKPH)
{
    // Articulation follows steering with delay scaled by speed
    const float ResponseSpeed = FMath::GetMappedRangeValueClamped(
        FVector2D(0.0f, 120.0f),
        FVector2D(4.0f, 1.5f),
        SpeedKPH);

    // Target articulation angle is a fraction of steering angle
    const float TargetArticulation = SteeringAngle * 0.6f;

    PreviousArticulationAngle = ArticulationAngleDeg;
    ArticulationAngleDeg      = FMath::FInterpTo(ArticulationAngleDeg,
                                                   TargetArticulation,
                                                   DeltaTime, ResponseSpeed);
}

void UTrailerPhysics::SimulateSway(float DeltaTime, float SpeedKPH)
{
    // Trailer sway: pendulum-like oscillation at speed
    // High centre of gravity (livestock, containers) swings more
    float SwayAmplitude = 0.0f;

    switch (TrailerType)
    {
        case ETrailerType::Livestock:  SwayAmplitude = 2.0f;  break;
        case ETrailerType::Container:  SwayAmplitude = 1.5f;  break;
        case ETrailerType::FuelTank:   SwayAmplitude = 1.8f;  break;
        case ETrailerType::Flatbed:    SwayAmplitude = 0.8f;  break;
        case ETrailerType::Lowboy:     SwayAmplitude = 0.5f;  break;
        case ETrailerType::Logging:    SwayAmplitude = 1.2f;  break;
        default:                       SwayAmplitude = 1.0f;  break;
    }

    // Speed multiplier: sway is worse at highway speeds
    const float SpeedFactor = FMath::GetMappedRangeValueClamped(
        FVector2D(60.0f, 130.0f),
        FVector2D(0.0f,  1.0f),
        SpeedKPH);

    const float ArticulationRate = ArticulationAngleDeg - PreviousArticulationAngle;
    SwayVelocity += ArticulationRate * SwayAmplitude * SpeedFactor;
    SwayVelocity *= 0.95f; // damping

    SwayAngleDeg = FMath::Clamp(SwayAngleDeg + SwayVelocity * DeltaTime, -20.0f, 20.0f);
    SwayAngleDeg = FMath::FInterpTo(SwayAngleDeg, 0.0f, DeltaTime, 0.5f); // self-centres
}

void UTrailerPhysics::DetectJackknife()
{
    // Jackknife: articulation angle exceeds safe threshold
    bJackknifing = FMath::Abs(ArticulationAngleDeg) > JackknifThreshold;

    if (bJackknifing)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("JACKKNIFE WARNING — articulation angle: %.1f°"),
               ArticulationAngleDeg);
    }
}
