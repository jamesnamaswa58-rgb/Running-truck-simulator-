#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "TruckController.generated.h"

class UEngineController;
class UTransmissionController;
class UClutchController;
class USuspensionController;
class UFuelSystem;
class UDamageSystem;
class USpringArmComponent;
class UCameraComponent;
class UAudioComponent;

UCLASS()
class VOLVOTRUCKSIM_API ATruckController : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    ATruckController();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // ── Input state ──────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, Category = "Input")
    float ThrottleInput;

    UPROPERTY(BlueprintReadOnly, Category = "Input")
    float BrakeInput;

    UPROPERTY(BlueprintReadOnly, Category = "Input")
    float SteeringInput;

    UPROPERTY(BlueprintReadOnly, Category = "Input")
    float ClutchInput;

    UPROPERTY(BlueprintReadOnly, Category = "Input")
    bool bHandbrakeActive;

    // ── Vehicle state ─────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, Category = "Vehicle")
    float CurrentSpeedKPH;

    UPROPERTY(BlueprintReadOnly, Category = "Vehicle")
    float CurrentSpeedMPS;

    UPROPERTY(BlueprintReadOnly, Category = "Vehicle")
    bool bEngineRunning;

    UPROPERTY(BlueprintReadOnly, Category = "Vehicle")
    bool bLeftIndicator;

    UPROPERTY(BlueprintReadOnly, Category = "Vehicle")
    bool bRightIndicator;

    UPROPERTY(BlueprintReadOnly, Category = "Vehicle")
    bool bHeadlightsOn;

    UPROPERTY(BlueprintReadOnly, Category = "Vehicle")
    bool bWipersOn;

    // ── Camera ────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* ExteriorCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* InteriorCamera;

    UPROPERTY(BlueprintReadOnly, Category = "Camera")
    bool bInteriorView;

    // ── Sub-systems ───────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Systems")
    UEngineController* EngineController;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Systems")
    UTransmissionController* TransmissionController;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Systems")
    UClutchController* ClutchController;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Systems")
    USuspensionController* SuspensionController;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Systems")
    UFuelSystem* FuelSystem;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Systems")
    UDamageSystem* DamageSystem;

    // ── Public API ────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category = "Vehicle")
    void StartEngine();

    UFUNCTION(BlueprintCallable, Category = "Vehicle")
    void StopEngine();

    UFUNCTION(BlueprintCallable, Category = "Vehicle")
    void ToggleCameraView();

    UFUNCTION(BlueprintCallable, Category = "Vehicle")
    void HonkHorn();

    UFUNCTION(BlueprintPure, Category = "Vehicle")
    float GetSpeedKPH() const { return CurrentSpeedKPH; }

private:
    // Input handlers
    void OnThrottle(float Value);
    void OnBrake(float Value);
    void OnSteer(float Value);
    void OnClutch(float Value);
    void OnHandbrake();
    void OnHandbrakeRelease();
    void OnToggleLeftIndicator();
    void OnToggleRightIndicator();
    void OnToggleHeadlights();
    void OnToggleWipers();
    void OnShiftUp();
    void OnShiftDown();

    void UpdateSpeed(float DeltaTime);
    void UpdateCamera(float DeltaTime);
    void ApplyDrivingForces(float DeltaTime);

    // Horn audio
    UPROPERTY(VisibleAnywhere)
    UAudioComponent* HornAudio;

    // Indicator timer
    float IndicatorTimer;
    bool bIndicatorState;

    // Steering smoothing
    float SmoothedSteering;

    static constexpr float SteeringSmoothing = 4.0f;
    static constexpr float MaxSteeringAngle  = 35.0f;
};
