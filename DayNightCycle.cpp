// ============================================================
// DayNightCycle.cpp
// ============================================================
#include "DayNightCycle.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"

ADayNightCycle::ADayNightCycle()
{
    PrimaryActorTick.bCanEverTick = true;

    SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
    SunLight->SetupAttachment(RootComponent);
    SunLight->Intensity = 10.0f;

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(RootComponent);
    SkyLight->bRealTimeCapture = true;
}

void ADayNightCycle::BeginPlay()
{
    Super::BeginPlay();
    UpdateSunPosition();
    UpdateLighting();
}

void ADayNightCycle::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Advance time
    TimeOfDay += (DeltaTime / 3600.0f) * TimeScale;
    if (TimeOfDay >= 24.0f) TimeOfDay -= 24.0f;

    NormalisedTimeOfDay = TimeOfDay / 24.0f;
    bIsNight = (TimeOfDay < SunriseHour || TimeOfDay > SunsetHour);

    UpdateSunPosition();
    UpdateLighting();
}

void ADayNightCycle::UpdateSunPosition()
{
    // Map 0-24h to sun pitch: -90° (midnight) → +90° (noon) → -90° (midnight)
    const float DayFraction = (TimeOfDay - SunriseHour) / (SunsetHour - SunriseHour);
    const float SunPitch    = FMath::Lerp(-10.0f, 180.0f, FMath::Clamp(DayFraction, 0.0f, 1.0f)) - 90.0f;

    if (SunLight)
    {
        SunLight->SetRelativeRotation(FRotator(SunPitch, 45.0f, 0.0f));
    }
}

void ADayNightCycle::UpdateLighting()
{
    if (!SunLight) return;

    const float Intensity = GetSunIntensity();
    SunLight->SetIntensity(Intensity);
    SunLight->SetLightColor(GetSkyColor());
}

float ADayNightCycle::GetSunIntensity() const
{
    if (bIsNight) return 0.0f;

    const float DayFraction = (TimeOfDay - SunriseHour) / (SunsetHour - SunriseHour);
    // Bell curve: dim at sunrise/sunset, bright at noon
    const float NoonFactor  = FMath::Sin(DayFraction * PI);
    return FMath::Lerp(0.5f, 10.0f, NoonFactor);
}

FLinearColor ADayNightCycle::GetSkyColor() const
{
    if (bIsNight)
        return FLinearColor(0.01f, 0.02f, 0.08f); // deep blue night

    const float DayFraction = (TimeOfDay - SunriseHour) / (SunsetHour - SunriseHour);

    if (DayFraction < 0.1f || DayFraction > 0.9f)
    {
        // Golden hour — warm orange
        return FLinearColor(1.0f, 0.5f, 0.1f);
    }

    // Daytime white-yellow
    return FLinearColor(1.0f, 0.95f, 0.85f);
}

void ADayNightCycle::UpdateFog() { /* Driven by WeatherManager fog density */ }


// ============================================================
// DamageSystem.cpp
// ============================================================
#include "DamageSystem.h"

UDamageSystem::UDamageSystem()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDamageSystem::ApplyCollisionDamage(float ImpactForce, FVector ImpactLocation)
{
    if (ImpactForce < CollisionDamageThreshold) return;

    const float DamageAmount = CalculateDamageFromForce(ImpactForce);

    // High-speed frontal impact → engine damage
    CurrentDamage.EngineHealth     = FMath::Max(0.0f, CurrentDamage.EngineHealth - DamageAmount * 0.4f);
    CurrentDamage.TransmissionHealth = FMath::Max(0.0f, CurrentDamage.TransmissionHealth - DamageAmount * 0.2f);
    CurrentDamage.SuspensionHealth = FMath::Max(0.0f, CurrentDamage.SuspensionHealth - DamageAmount * 0.3f);
    CurrentDamage.BodyDamage       = FMath::Min(1.0f, CurrentDamage.BodyDamage + DamageAmount * 0.5f);

    if (ImpactForce > CollisionDamageThreshold * 3.0f)
    {
        CurrentDamage.bWindshieldCracked = true;
    }

    OnDamageReceived.Broadcast(EDamageZone::Body, DamageAmount);

    UE_LOG(LogTemp, Warning, TEXT("Collision damage: %.2f (force: %.0f N)"),
           DamageAmount, ImpactForce);
}

void UDamageSystem::UpdateTireWear(float DeltaTime, float SpeedKPH, bool bWheelSpin)
{
    // Tires wear slowly with mileage, faster when spinning
    const float WearRate = bWheelSpin ? 0.001f : 0.0001f;
    CurrentDamage.TireCondition = FMath::Max(0.0f,
        CurrentDamage.TireCondition - WearRate * DeltaTime);

    if (CurrentDamage.TireCondition < 0.2f)
    {
        UE_LOG(LogTemp, Warning, TEXT("Tire condition critical: %.0f%%"),
               CurrentDamage.TireCondition * 100.0f);
    }
}

void UDamageSystem::RepairAll()
{
    CurrentDamage.EngineHealth        = 1.0f;
    CurrentDamage.TransmissionHealth  = 1.0f;
    CurrentDamage.SuspensionHealth    = 1.0f;
    CurrentDamage.BodyDamage          = 0.0f;
    CurrentDamage.bWindshieldCracked  = false;
    CurrentDamage.TireCondition       = 1.0f;
    UE_LOG(LogTemp, Log, TEXT("Truck fully repaired"));
}

void UDamageSystem::RepairZone(EDamageZone Zone)
{
    switch (Zone)
    {
        case EDamageZone::Engine:       CurrentDamage.EngineHealth       = 1.0f; break;
        case EDamageZone::Transmission: CurrentDamage.TransmissionHealth = 1.0f; break;
        case EDamageZone::Suspension:   CurrentDamage.SuspensionHealth   = 1.0f; break;
        case EDamageZone::Body:         CurrentDamage.BodyDamage         = 0.0f; break;
        case EDamageZone::Windshield:   CurrentDamage.bWindshieldCracked = false; break;
        case EDamageZone::Tires:        CurrentDamage.TireCondition      = 1.0f; break;
    }
}

float UDamageSystem::GetRepairCostEUR() const
{
    float Cost = 0.0f;
    Cost += (1.0f - CurrentDamage.EngineHealth)       * 5000.0f;
    Cost += (1.0f - CurrentDamage.TransmissionHealth) * 3000.0f;
    Cost += (1.0f - CurrentDamage.SuspensionHealth)   * 2000.0f;
    Cost += CurrentDamage.BodyDamage                   * 4000.0f;
    Cost += CurrentDamage.bWindshieldCracked           ? 800.0f : 0.0f;
    Cost += (1.0f - CurrentDamage.TireCondition)       * 1200.0f;
    return Cost;
}

float UDamageSystem::CalculateDamageFromForce(float Force) const
{
    // Map force to 0-1 damage: threshold = 0 damage, 10× threshold = 1.0
    return FMath::GetMappedRangeValueClamped(
        FVector2D(CollisionDamageThreshold, CollisionDamageThreshold * 10.0f),
        FVector2D(0.0f, 1.0f),
        Force);
}
