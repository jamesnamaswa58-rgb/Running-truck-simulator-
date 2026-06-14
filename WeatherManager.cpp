#include "WeatherManager.h"
#include "Components/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PostProcessComponent.h"
#include "Kismet/GameplayStatics.h"

AWeatherManager::AWeatherManager()
{
    PrimaryActorTick.bCanEverTick = true;

    RainParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("RainParticles"));
    RainParticles->SetupAttachment(RootComponent);
    RainParticles->bAutoActivate = false;

    RainAudio    = CreateDefaultSubobject<UAudioComponent>(TEXT("RainAudio"));
    ThunderAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ThunderAudio"));
    RainAudio->bAutoActivate    = false;
    ThunderAudio->bAutoActivate = false;

    WeatherPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
    WeatherPostProcess->SetupAttachment(RootComponent);

    CurrentWeather    = EWeatherState::Sunny;
    TargetWeather     = EWeatherState::Sunny;
    TransitionTimer   = 0.0f;
    TransitionProgress = 1.0f;
    DynamicWeatherTimer = 0.0f;
    ThunderTimer      = 0.0f;
    NextThunderDelay  = FMath::FRandRange(5.0f, 20.0f);

    ActiveEffects   = GetEffectsForState(EWeatherState::Sunny);
    TargetEffects   = ActiveEffects;
    PreviousEffects = ActiveEffects;
}

void AWeatherManager::BeginPlay()
{
    Super::BeginPlay();
    SetWeather(CurrentWeather);
}

void AWeatherManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Blend transition
    if (TransitionProgress < 1.0f)
    {
        TransitionTimer   += DeltaTime;
        TransitionProgress = FMath::Clamp(TransitionTimer / WeatherTransitionTime, 0.0f, 1.0f);
        ApplyTransition(TransitionProgress);
    }

    // Dynamic weather changes
    if (bDynamicWeather)
    {
        TryDynamicWeatherChange(DeltaTime);
    }

    // Thunder during storms
    if (CurrentWeather == EWeatherState::Storm || TargetWeather == EWeatherState::Storm)
    {
        ThunderTimer += DeltaTime;
        if (ThunderTimer >= NextThunderDelay)
        {
            PlayThunder();
            ThunderTimer    = 0.0f;
            NextThunderDelay = FMath::FRandRange(4.0f, 18.0f);
        }
    }

    UpdateVisuals(DeltaTime);
}

void AWeatherManager::SetWeather(EWeatherState NewWeather)
{
    if (NewWeather == TargetWeather) return;

    PreviousEffects = ActiveEffects;
    TargetWeather   = NewWeather;
    TargetEffects   = GetEffectsForState(NewWeather);

    TransitionTimer    = 0.0f;
    TransitionProgress = 0.0f;

    UE_LOG(LogTemp, Log, TEXT("Weather changing to: %d"), (int32)NewWeather);
}

FWeatherEffects AWeatherManager::GetEffectsForState(EWeatherState State) const
{
    FWeatherEffects E;
    switch (State)
    {
        case EWeatherState::Sunny:
            E.RoadGripMultiplier = 1.00f;
            E.VisibilityMetres   = 2000.0f;
            E.WindSpeedKPH       = 10.0f;
            E.RainfallIntensity  = 0.0f;
            E.FogDensity         = 0.0f;
            E.bWetRoads          = false;
            E.bThunder           = false;
            break;

        case EWeatherState::Cloudy:
            E.RoadGripMultiplier = 0.95f;
            E.VisibilityMetres   = 1500.0f;
            E.WindSpeedKPH       = 20.0f;
            E.RainfallIntensity  = 0.0f;
            E.FogDensity         = 0.05f;
            E.bWetRoads          = false;
            E.bThunder           = false;
            break;

        case EWeatherState::Rain:
            E.RoadGripMultiplier = 0.75f;
            E.VisibilityMetres   = 600.0f;
            E.WindSpeedKPH       = 30.0f;
            E.RainfallIntensity  = 0.5f;
            E.FogDensity         = 0.1f;
            E.bWetRoads          = true;
            E.bThunder           = false;
            break;

        case EWeatherState::HeavyRain:
            E.RoadGripMultiplier = 0.60f;
            E.VisibilityMetres   = 300.0f;
            E.WindSpeedKPH       = 50.0f;
            E.RainfallIntensity  = 0.9f;
            E.FogDensity         = 0.2f;
            E.bWetRoads          = true;
            E.bThunder           = false;
            break;

        case EWeatherState::Fog:
            E.RoadGripMultiplier = 0.85f;
            E.VisibilityMetres   = 80.0f;
            E.WindSpeedKPH       = 5.0f;
            E.RainfallIntensity  = 0.0f;
            E.FogDensity         = 0.85f;
            E.bWetRoads          = true;
            E.bThunder           = false;
            break;

        case EWeatherState::Storm:
            E.RoadGripMultiplier = 0.50f;
            E.VisibilityMetres   = 150.0f;
            E.WindSpeedKPH       = 90.0f;
            E.RainfallIntensity  = 1.0f;
            E.FogDensity         = 0.3f;
            E.bWetRoads          = true;
            E.bThunder           = true;
            break;

        case EWeatherState::Night:
            E.RoadGripMultiplier = 0.90f;
            E.VisibilityMetres   = 100.0f; // headlight-limited
            E.WindSpeedKPH       = 15.0f;
            E.RainfallIntensity  = 0.0f;
            E.FogDensity         = 0.0f;
            E.bWetRoads          = false;
            E.bThunder           = false;
            break;
    }
    return E;
}

void AWeatherManager::ApplyTransition(float Alpha)
{
    // Lerp all numeric weather properties
    ActiveEffects.RoadGripMultiplier = FMath::Lerp(PreviousEffects.RoadGripMultiplier,
                                                     TargetEffects.RoadGripMultiplier, Alpha);
    ActiveEffects.VisibilityMetres   = FMath::Lerp(PreviousEffects.VisibilityMetres,
                                                     TargetEffects.VisibilityMetres, Alpha);
    ActiveEffects.WindSpeedKPH       = FMath::Lerp(PreviousEffects.WindSpeedKPH,
                                                     TargetEffects.WindSpeedKPH, Alpha);
    ActiveEffects.RainfallIntensity  = FMath::Lerp(PreviousEffects.RainfallIntensity,
                                                     TargetEffects.RainfallIntensity, Alpha);
    ActiveEffects.FogDensity         = FMath::Lerp(PreviousEffects.FogDensity,
                                                     TargetEffects.FogDensity, Alpha);
    ActiveEffects.bWetRoads          = TargetEffects.bWetRoads;
    ActiveEffects.bThunder           = TargetEffects.bThunder;

    // Snap current weather at 50% through transition
    if (Alpha >= 0.5f && CurrentWeather != TargetWeather)
    {
        CurrentWeather = TargetWeather;
    }
}

void AWeatherManager::UpdateVisuals(float DeltaTime)
{
    // Rain particles: drive intensity from rainfall
    if (RainParticles)
    {
        if (ActiveEffects.RainfallIntensity > 0.01f)
        {
            if (!RainParticles->IsActive()) RainParticles->Activate();
            RainParticles->SetFloatParameter(FName("RainRate"), ActiveEffects.RainfallIntensity);
        }
        else
        {
            RainParticles->Deactivate();
        }
    }

    // Rain audio volume
    if (RainAudio)
    {
        if (ActiveEffects.RainfallIntensity > 0.01f)
        {
            if (!RainAudio->IsPlaying()) RainAudio->Play();
            RainAudio->SetVolumeMultiplier(ActiveEffects.RainfallIntensity);
        }
        else
        {
            RainAudio->Stop();
        }
    }

    // Post-process: fog via exponential height fog driven by FogDensity
    // (actual UE fog actor is driven via Blueprint using ActiveEffects.FogDensity)
}

void AWeatherManager::TryDynamicWeatherChange(float DeltaTime)
{
    DynamicWeatherTimer += DeltaTime;
    if (DynamicWeatherTimer < 60.0f) return; // check once per minute

    DynamicWeatherTimer = 0.0f;
    if (FMath::FRand() < WeatherChangeProbabilityPerMinute)
    {
        // Pick a random neighboring weather state
        const TArray<EWeatherState> States = {
            EWeatherState::Sunny, EWeatherState::Cloudy,
            EWeatherState::Rain,  EWeatherState::HeavyRain,
            EWeatherState::Fog,   EWeatherState::Storm
        };
        const int32 Idx = FMath::RandRange(0, States.Num() - 1);
        SetWeather(States[Idx]);
    }
}

void AWeatherManager::PlayThunder()
{
    if (ThunderAudio)
    {
        const float Delay = FMath::FRandRange(0.0f, 3.0f); // random delay for realism
        ThunderAudio->SetVolumeMultiplier(FMath::FRandRange(0.6f, 1.0f));
        ThunderAudio->Play(Delay);
    }
}
