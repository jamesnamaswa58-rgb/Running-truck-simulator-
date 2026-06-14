#include "TransmissionController.h"
#include "EngineController.h"

UTransmissionController::UTransmissionController()
{
    PrimaryComponentTick.bCanEverTick = false;

    CurrentGear      = 0;
    TargetGear       = 0;
    CurrentRange     = EGearRange::Low;
    bSplitterEngaged = false;
    bIsGrinding      = false;
    ShiftDelay       = 0.3f;
    ShiftTimer       = 0.0f;

    BuildDefaultGearRatios();
    UpdateGearLabel();
}

// ─────────────────────────────────────────────────────────────────────────────
void UTransmissionController::ShiftUp()
{
    if (!CanShift()) return;

    if (CurrentGear == -1)          // R → N
    {
        ShiftToNeutral();
    }
    else if (CurrentGear == 0)      // N → 1
    {
        ShiftToGear(1);
    }
    else if (CurrentGear < MaxForwardGear)
    {
        // Check if we should toggle range at the split point (gear 4 Low → 1 High)
        if (CurrentRange == EGearRange::Low && CurrentGear == 4)
        {
            CurrentRange = EGearRange::High;
            ShiftToGear(1);
        }
        else
        {
            ShiftToGear(CurrentGear + 1);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void UTransmissionController::ShiftDown()
{
    if (!CanShift()) return;

    if (CurrentGear == 1)
    {
        if (CurrentRange == EGearRange::High)
        {
            CurrentRange = EGearRange::Low;
            ShiftToGear(4);
        }
        else
        {
            ShiftToNeutral();
        }
    }
    else if (CurrentGear > 1)
    {
        ShiftToGear(CurrentGear - 1);
    }
    else if (CurrentGear == 0)
    {
        ShiftToReverse();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void UTransmissionController::ShiftToGear(int32 Gear)
{
    if (Gear < -1 || Gear > MaxForwardGear) return;

    const bool bGrind = WillGrind(CurrentGear, Gear);
    bIsGrinding = bGrind;

    if (bGrind)
    {
        // Don't complete the shift — gear grind blocks engagement
        UE_LOG(LogTemp, Warning, TEXT("Gear grind! Clutch not fully pressed or RPM mismatch"));
        return;
    }

    CurrentGear = Gear;
    UpdateGearLabel();

    UE_LOG(LogTemp, Log, TEXT("Shifted to: %s"), *CurrentGearLabel);
}

void UTransmissionController::ShiftToReverse()  { ShiftToGear(-1); }
void UTransmissionController::ShiftToNeutral()  { CurrentGear = 0; UpdateGearLabel(); }

// ─────────────────────────────────────────────────────────────────────────────
void UTransmissionController::ToggleRange()
{
    if (CurrentRange == EGearRange::Low)
        CurrentRange = EGearRange::High;
    else
        CurrentRange = EGearRange::Low;

    UpdateGearLabel();
}

void UTransmissionController::ToggleSplitter()
{
    bSplitterEngaged = !bSplitterEngaged;
    UpdateGearLabel();
}

// ─────────────────────────────────────────────────────────────────────────────
float UTransmissionController::GetCurrentGearRatio() const
{
    if (CurrentGear == -1) return ReverseRatio;
    if (CurrentGear ==  0) return 0.0f;

    const int32 Idx = CurrentGear - 1;
    if (!ForwardGears.IsValidIndex(Idx)) return 1.0f;

    float Ratio = ForwardGears[Idx].Ratio;

    // Apply range multiplier
    if (CurrentRange == EGearRange::Low)
        Ratio *= RangeMultiplierLow;

    // Splitter provides a half-step ratio boost
    if (bSplitterEngaged)
        Ratio *= 1.25f;

    return Ratio;
}

// ─────────────────────────────────────────────────────────────────────────────
float UTransmissionController::GetOutputTorque(float EngineTorque) const
{
    return EngineTorque * GetCurrentGearRatio() * FinalDriveRatio;
}

// ─────────────────────────────────────────────────────────────────────────────
bool UTransmissionController::CanShift() const
{
    // Shifting requires clutch to be sufficiently pressed (>70%)
    // This is checked by TruckController before calling ShiftUp/Down
    return ShiftTimer <= 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
bool UTransmissionController::WillGrind(int32 FromGear, int32 ToGear) const
{
    if (!EngineController) return false;

    // If RPM is far from sync RPM for the target gear, there is a chance of grind
    const float RPM = EngineController->GetCurrentRPM();
    const float IdealRPM = 1400.0f; // approximate sync RPM for a clean shift

    const float RPMDelta = FMath::Abs(RPM - IdealRPM);
    const float GrindProbability = FMath::GetMappedRangeValueClamped(
        FVector2D(0.0f, 800.0f),
        FVector2D(0.0f, GrindChance),
        RPMDelta);

    return FMath::FRand() < GrindProbability;
}

// ─────────────────────────────────────────────────────────────────────────────
void UTransmissionController::UpdateGearLabel()
{
    if (CurrentGear == -1)
    {
        CurrentGearLabel = TEXT("R");
    }
    else if (CurrentGear == 0)
    {
        CurrentGearLabel = TEXT("N");
    }
    else
    {
        const FString RangeStr = (CurrentRange == EGearRange::Low)  ? TEXT("L") : TEXT("H");
        const FString SplitStr = bSplitterEngaged                   ? TEXT("+") : TEXT("");
        CurrentGearLabel = FString::Printf(TEXT("%d%s%s"), CurrentGear, *RangeStr, *SplitStr);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void UTransmissionController::BuildDefaultGearRatios()
{
    // Volvo I-Shift 12-speed approximate ratios
    ForwardGears.Add({ 1,  14.94f, TEXT("1")  });
    ForwardGears.Add({ 2,  11.73f, TEXT("2")  });
    ForwardGears.Add({ 3,   9.38f, TEXT("3")  });
    ForwardGears.Add({ 4,   7.38f, TEXT("4")  });
    ForwardGears.Add({ 5,   5.93f, TEXT("5")  });
    ForwardGears.Add({ 6,   4.64f, TEXT("6")  });
    ForwardGears.Add({ 7,   3.74f, TEXT("7")  });
    ForwardGears.Add({ 8,   2.94f, TEXT("8")  });
    ForwardGears.Add({ 9,   2.37f, TEXT("9")  });
    ForwardGears.Add({ 10,  1.84f, TEXT("10") });
    ForwardGears.Add({ 11,  1.41f, TEXT("11") });
    ForwardGears.Add({ 12,  1.00f, TEXT("12") });
}
