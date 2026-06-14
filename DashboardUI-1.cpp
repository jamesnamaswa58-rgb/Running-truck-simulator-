#include "DashboardUI.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Vehicle/TruckController.h"
#include "Vehicle/EngineController.h"
#include "Vehicle/TransmissionController.h"
#include "Vehicle/FuelSystem.h"
#include "Gameplay/JobSystem.h"
#include "Kismet/GameplayStatics.h"

void UDashboardUI::NativeConstruct()
{
    Super::NativeConstruct();

    IndicatorBlinkTimer = 0.0f;
    bIndicatorBlinkState = false;
    TripDistanceKM = 0.0f;

    // Find the player's truck
    APawn* PlayerPawn = GetOwningPlayerPawn();
    TruckController   = Cast<ATruckController>(PlayerPawn);

    if (!TruckController)
    {
        UE_LOG(LogTemp, Warning, TEXT("DashboardUI: Could not find TruckController"));
    }
}

void UDashboardUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!TruckController) return;

    // Accumulate trip distance
    TripDistanceKM += TruckController->GetSpeedKPH() * InDeltaTime / 3600.0f;

    UpdateSpeedometer();
    UpdateRPMGauge();
    UpdateGearDisplay();
    UpdateFuelGauge();
    UpdateTemperature();
    UpdateIndicators(InDeltaTime);
    UpdateWarningLights();
    UpdateJobHUD();
}

// ─────────────────────────────────────────────────────────────────────────────
void UDashboardUI::UpdateSpeedometer()
{
    const float Speed = TruckController->GetSpeedKPH();

    if (SpeedText)
    {
        SpeedText->SetText(FText::FromString(
            FString::Printf(TEXT("%.0f km/h"), Speed)));

        // Color warning at high speed
        const FLinearColor Color = (Speed > 90.0f) ? WarningColor : NormalColor;
        SpeedText->SetColorAndOpacity(Color);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void UDashboardUI::UpdateRPMGauge()
{
    if (!TruckController->EngineController) return;

    const float RPM    = TruckController->EngineController->GetCurrentRPM();
    const float MaxRPM = TruckController->EngineController->MaxRPM;

    if (RPMText)
    {
        RPMText->SetText(FText::FromString(
            FString::Printf(TEXT("%.0f RPM"), RPM)));
        RPMText->SetColorAndOpacity(GetRPMColor());
    }

    if (RPMBar)
    {
        RPMBar->SetPercent(RPM / MaxRPM);
        RPMBar->SetFillColorAndOpacity(GetRPMColor());
    }

    if (TurboPressureBar)
    {
        TurboPressureBar->SetPercent(TruckController->EngineController->TurboPressure);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void UDashboardUI::UpdateGearDisplay()
{
    if (!TruckController->TransmissionController) return;

    if (GearText)
    {
        const FString GearLabel = TruckController->TransmissionController->CurrentGearLabel;
        GearText->SetText(FText::FromString(GearLabel));

        // Grinding gear: flash red
        const bool bGrinding = TruckController->TransmissionController->bIsGrinding;
        GearText->SetColorAndOpacity(bGrinding ? CriticalColor : NormalColor);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void UDashboardUI::UpdateFuelGauge()
{
    if (!TruckController->FuelSystem) return;

    const float FuelFraction = TruckController->FuelSystem->GetFuelFraction();

    if (FuelText)
    {
        FuelText->SetText(FText::FromString(
            FString::Printf(TEXT("%.0f L"), TruckController->FuelSystem->GetFuelLitres())));

        const FLinearColor Color = (FuelFraction < 0.15f) ? CriticalColor
                                 : (FuelFraction < 0.25f) ? WarningColor
                                 : NormalColor;
        FuelText->SetColorAndOpacity(Color);
    }

    if (FuelBar)
    {
        FuelBar->SetPercent(FuelFraction);
        const FLinearColor BarColor = (FuelFraction < 0.15f) ? CriticalColor
                                    : (FuelFraction < 0.25f) ? WarningColor
                                    : NormalColor;
        FuelBar->SetFillColorAndOpacity(BarColor);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void UDashboardUI::UpdateTemperature()
{
    if (!TruckController->EngineController) return;

    const float Temp = TruckController->EngineController->EngineTemperature;

    if (EngineTemperatureText)
    {
        EngineTemperatureText->SetText(FText::FromString(
            FString::Printf(TEXT("%.0f°C"), Temp)));

        const FLinearColor Color = (Temp > 105.0f) ? CriticalColor
                                 : (Temp > 95.0f)  ? WarningColor
                                 : NormalColor;
        EngineTemperatureText->SetColorAndOpacity(Color);
    }

    if (TripDistanceText)
    {
        TripDistanceText->SetText(FText::FromString(
            FString::Printf(TEXT("%.1f km"), TripDistanceKM)));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void UDashboardUI::UpdateIndicators(float DeltaTime)
{
    IndicatorBlinkTimer += DeltaTime;
    if (IndicatorBlinkTimer >= 0.5f)
    {
        IndicatorBlinkTimer  = 0.0f;
        bIndicatorBlinkState = !bIndicatorBlinkState;
    }

    const bool ShowLeft  = TruckController->bLeftIndicator  && bIndicatorBlinkState;
    const bool ShowRight = TruckController->bRightIndicator && bIndicatorBlinkState;

    if (LeftIndicatorLight)
        LeftIndicatorLight->SetVisibility(ShowLeft
            ? ESlateVisibility::Visible : ESlateVisibility::Hidden);

    if (RightIndicatorLight)
        RightIndicatorLight->SetVisibility(ShowRight
            ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

// ─────────────────────────────────────────────────────────────────────────────
void UDashboardUI::UpdateWarningLights()
{
    const bool bEngine = TruckController->EngineController &&
                         TruckController->EngineController->bIsStalled;
    if (CheckEngineIcon)
        CheckEngineIcon->SetVisibility(bEngine
            ? ESlateVisibility::Visible : ESlateVisibility::Hidden);

    if (HandbrakeIcon)
        HandbrakeIcon->SetVisibility(TruckController->bHandbrakeActive
            ? ESlateVisibility::Visible : ESlateVisibility::Hidden);

    if (HeadlightIcon)
        HeadlightIcon->SetVisibility(TruckController->bHeadlightsOn
            ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

// ─────────────────────────────────────────────────────────────────────────────
void UDashboardUI::UpdateJobHUD()
{
    // Job system is a world actor — find it
    AJobSystem* JobSystem = Cast<AJobSystem>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AJobSystem::StaticClass()));

    if (!JobSystem || !JobSystem->bHasActiveJob)
    {
        if (JobDestinationText)
            JobDestinationText->SetText(FText::FromString(TEXT("No active job")));
        return;
    }

    const FDeliveryJob& Job = JobSystem->ActiveJob;

    if (JobDestinationText)
        JobDestinationText->SetText(FText::FromString(
            FString::Printf(TEXT("→ %s"), *Job.DestinationCity)));

    if (JobDistanceText)
    {
        const float Dist = JobSystem->GetDistanceToDestination(
            TruckController->GetActorLocation());
        JobDistanceText->SetText(FText::FromString(
            FString::Printf(TEXT("%.1f km remaining"), Dist)));
    }

    if (JobTimeRemainingText)
    {
        const int32 Hours   = (int32)Job.TimeRemainingHours;
        const int32 Minutes = (int32)((Job.TimeRemainingHours - Hours) * 60.0f);
        const FLinearColor TimeColor = Job.TimeRemainingHours < 0.5f
                                     ? CriticalColor : NormalColor;
        JobTimeRemainingText->SetText(FText::FromString(
            FString::Printf(TEXT("%02d:%02d"), Hours, Minutes)));
        JobTimeRemainingText->SetColorAndOpacity(TimeColor);
    }

    if (CargoConditionBar)
        CargoConditionBar->SetPercent(Job.CargoCondition);
}

// ─────────────────────────────────────────────────────────────────────────────
FLinearColor UDashboardUI::GetRPMColor() const
{
    if (!TruckController->EngineController) return NormalColor;

    const float RPM    = TruckController->EngineController->GetCurrentRPM();
    const float MaxRPM = TruckController->EngineController->MaxRPM;
    const float Ratio  = RPM / MaxRPM;

    if (Ratio > 0.90f) return CriticalColor;
    if (Ratio > 0.75f) return WarningColor;
    return NormalColor;
}
