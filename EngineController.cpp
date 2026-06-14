#include "EngineController.h"
#include "Math/UnrealMathUtility.h"

UEngineController::UEngineController()
{
    PrimaryComponentTick.bCanEverTick = true;

    CurrentRPM         = 0.0f;
    CurrentTorqueNm    = 0.0f;
    TurboPressure      = 0.0f;
    EngineTemperature  = 20.0f; // ambient
    bIsStalled         = false;
    bEngineRunning     = false;
    StartupTimer       = 0.0f;

    BuildDefaultTorqueCurve();
}

// ─────────────────────────────────────────────────────────────────────────────
void UEngineController::BeginPlay()
{
    Super::BeginPlay();
}

void UEngineController::TickComponent(float DeltaTime, ELevelTick TickType,
                                       FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    // Actual update driven externally by TruckController::Tick
}

// ─────────────────────────────────────────────────────────────────────────────
void UEngineController::UpdateEngine(float DeltaTime, float Throttle,
                                      float Clutch, bool bRunning)
{
    bEngineRunning = bRunning;

    if (!bEngineRunning)
    {
        // Engine off - coast RPM down
        CurrentRPM = FMath::FInterpTo(CurrentRPM, 0.0f, DeltaTime, 3.0f);
        CurrentTorqueNm = 0.0f;
        TurboPressure   = FMath::FInterpTo(TurboPressure, 0.0f, DeltaTime, 1.0f);
        return;
    }

    SimulateRPM(DeltaTime, Throttle, Clutch);
    SimulateTurbo(DeltaTime);
    SimulateTemperature(DeltaTime, Throttle);

    CurrentTorqueNm = GetTorqueAtRPM(CurrentRPM) * Throttle;
}

// ─────────────────────────────────────────────────────────────────────────────
void UEngineController::SimulateRPM(float DeltaTime, float Throttle, float Clutch)
{
    // Target RPM: idle when no throttle, scales linearly to max
    const float TargetRPM = FMath::Lerp(IdleRPM, MaxRPM, Throttle);

    // Clutch disengagement reduces load on RPM climb
    const float LoadFactor = 1.0f - (Clutch * 0.8f);

    const float RPMDelta = (TargetRPM - CurrentRPM) * LoadFactor;
    CurrentRPM = FMath::Clamp(
        CurrentRPM + RPMDelta * DeltaTime / EngineInertia,
        0.0f, MaxRPM);

    // Stall detection: if engine is running but RPM drops below idle under load
    if (bEngineRunning && CurrentRPM < IdleRPM * 0.8f && Throttle < 0.05f && Clutch < 0.3f)
    {
        bIsStalled = true;
        bEngineRunning = false;
        CurrentRPM = 0.0f;
    }
    else
    {
        bIsStalled = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void UEngineController::SimulateTurbo(float DeltaTime)
{
    // Turbo pressure builds with RPM, lags behind due to inertia
    const float TargetPressure = FMath::GetMappedRangeValueClamped(
        FVector2D(800.0f, 2000.0f),
        FVector2D(0.0f,   1.0f),
        CurrentRPM);

    TurboPressure = FMath::FInterpTo(TurboPressure, TargetPressure,
                                      DeltaTime, TurboLagResponse);
}

// ─────────────────────────────────────────────────────────────────────────────
void UEngineController::SimulateTemperature(float DeltaTime, float Load)
{
    // Warm up to ~90°C operating temp; load increases heat
    const float TargetTemp = 90.0f + (Load * 20.0f);
    EngineTemperature = FMath::FInterpTo(EngineTemperature, TargetTemp,
                                          DeltaTime, 1.0f / ThermalMass);
}

// ─────────────────────────────────────────────────────────────────────────────
float UEngineController::GetTorqueAtRPM(float RPM) const
{
    if (TorqueCurve.Num() == 0) return 0.0f;

    // Interpolate along the torque curve
    for (int32 i = 0; i < TorqueCurve.Num() - 1; ++i)
    {
        const FTorqueCurvePoint& A = TorqueCurve[i];
        const FTorqueCurvePoint& B = TorqueCurve[i + 1];

        if (RPM >= A.RPM && RPM <= B.RPM)
        {
            const float Alpha = (RPM - A.RPM) / (B.RPM - A.RPM);
            return FMath::Lerp(A.TorqueNm, B.TorqueNm, Alpha);
        }
    }

    return TorqueCurve.Last().TorqueNm;
}

// ─────────────────────────────────────────────────────────────────────────────
void UEngineController::OnEngineStart()
{
    bEngineRunning = true;
    bIsStalled     = false;
    StartupTimer   = 0.0f;
    CurrentRPM     = IdleRPM;
    UE_LOG(LogTemp, Log, TEXT("Engine started — idle at %.0f RPM"), IdleRPM);
}

void UEngineController::OnEngineStop()
{
    bEngineRunning = false;
    UE_LOG(LogTemp, Log, TEXT("Engine stopped"));
}

// ─────────────────────────────────────────────────────────────────────────────
void UEngineController::BuildDefaultTorqueCurve()
{
    // Volvo D13 torque curve data (from spec sheet)
    TorqueCurve.Add({ 600.0f,  1000.0f });
    TorqueCurve.Add({ 1000.0f, 1800.0f });
    TorqueCurve.Add({ 1400.0f, 2600.0f }); // peak torque
    TorqueCurve.Add({ 1800.0f, 2300.0f });
    TorqueCurve.Add({ 2200.0f, 1800.0f });
    TorqueCurve.Add({ 2500.0f, 1200.0f }); // redline drop-off
}
