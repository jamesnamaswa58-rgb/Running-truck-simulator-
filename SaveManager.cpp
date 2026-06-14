#include "SaveManager.h"
#include "Kismet/GameplayStatics.h"
#include "Vehicle/TruckController.h"
#include "Vehicle/FuelSystem.h"
#include "Vehicle/TransmissionController.h"
#include "Gameplay/JobSystem.h"

USaveManager::USaveManager()
{
    TimeSinceLastSave = 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
bool USaveManager::SaveGame(int32 SlotIndex, UWorld* World)
{
    if (SlotIndex < 0 || SlotIndex >= MaxSaveSlots) return false;

    UVolvoTruckSaveGame* SaveData = Cast<UVolvoTruckSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UVolvoTruckSaveGame::StaticClass()));

    if (!SaveData) return false;

    SaveData->TruckData    = CollectTruckData(World);
    SaveData->PlayerData   = CollectPlayerData(World);
    SaveData->JobData      = CollectJobData(World);
    SaveData->SaveSlotName = GetSaveSlotName(SlotIndex);
    SaveData->SaveTimestamp = FDateTime::Now();

    const bool bSuccess = UGameplayStatics::SaveGameToSlot(
        SaveData, GetSaveSlotName(SlotIndex), 0);

    OnSaveComplete.Broadcast(bSuccess);
    TimeSinceLastSave = 0.0f;

    if (bSuccess)
        UE_LOG(LogTemp, Log, TEXT("Game saved to slot %d"), SlotIndex);
    else
        UE_LOG(LogTemp, Error, TEXT("Save FAILED for slot %d"), SlotIndex);

    return bSuccess;
}

// ─────────────────────────────────────────────────────────────────────────────
bool USaveManager::LoadGame(int32 SlotIndex, UWorld* World)
{
    if (!DoesSaveExist(SlotIndex)) return false;

    UVolvoTruckSaveGame* SaveData = Cast<UVolvoTruckSaveGame>(
        UGameplayStatics::LoadGameFromSlot(GetSaveSlotName(SlotIndex), 0));

    if (!SaveData)
    {
        OnLoadComplete.Broadcast(false);
        return false;
    }

    ApplyTruckData(SaveData->TruckData, World);
    ApplyPlayerData(SaveData->PlayerData, World);
    ApplyJobData(SaveData->JobData, World);

    OnLoadComplete.Broadcast(true);
    UE_LOG(LogTemp, Log, TEXT("Game loaded from slot %d (saved: %s)"),
           SlotIndex, *SaveData->SaveTimestamp.ToString());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool USaveManager::DeleteSave(int32 SlotIndex)
{
    if (!DoesSaveExist(SlotIndex)) return false;
    return UGameplayStatics::DeleteGameInSlot(GetSaveSlotName(SlotIndex), 0);
}

bool USaveManager::DoesSaveExist(int32 SlotIndex) const
{
    return UGameplayStatics::DoesSaveGameExist(GetSaveSlotName(SlotIndex), 0);
}

FString USaveManager::GetSaveSlotName(int32 SlotIndex) const
{
    return SlotIndex == AutoSaveSlotIndex
        ? TEXT("AutoSave")
        : FString::Printf(TEXT("Save_%02d"), SlotIndex);
}

// ─────────────────────────────────────────────────────────────────────────────
void USaveManager::TickAutoSave(float DeltaTime, UWorld* World)
{
    TimeSinceLastSave += DeltaTime;
    if (TimeSinceLastSave >= AutoSaveIntervalMinutes * 60.0f)
    {
        UE_LOG(LogTemp, Log, TEXT("Auto-saving..."));
        SaveGame(AutoSaveSlotIndex, World);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
FTruckSaveData USaveManager::CollectTruckData(UWorld* World) const
{
    FTruckSaveData Data;

    ATruckController* Truck = Cast<ATruckController>(
        UGameplayStatics::GetPlayerPawn(World, 0));

    if (Truck)
    {
        Data.Position   = Truck->GetActorLocation();
        Data.Rotation   = Truck->GetActorRotation();
        Data.FuelLitres = Truck->FuelSystem ? Truck->FuelSystem->GetFuelLitres() : 0.0f;
        Data.CurrentGear = Truck->TransmissionController
                           ? Truck->TransmissionController->CurrentGear : 0;
    }

    return Data;
}

FPlayerSaveData USaveManager::CollectPlayerData(UWorld* World) const
{
    FPlayerSaveData Data;
    // In production this comes from the XPSystem/EconomySystem actors
    // Placeholder values for now
    Data.PlayerLevel    = 1;
    Data.MoneyEUR       = 5000.0f;
    Data.PlaytimeHours  = 0.0f;
    return Data;
}

FActiveJobSaveData USaveManager::CollectJobData(UWorld* World) const
{
    FActiveJobSaveData Data;

    AJobSystem* JobSystem = Cast<AJobSystem>(
        UGameplayStatics::GetActorOfClass(World, AJobSystem::StaticClass()));

    if (JobSystem && JobSystem->bHasActiveJob)
    {
        const FDeliveryJob& Job = JobSystem->ActiveJob;
        Data.bHasActiveJob      = true;
        Data.JobID              = Job.JobID;
        Data.OriginCity         = Job.OriginCity;
        Data.DestinationCity    = Job.DestinationCity;
        Data.BasePayEUR         = Job.BasePayEUR;
        Data.TimeRemainingHours = Job.TimeRemainingHours;
        Data.CargoCondition     = Job.CargoCondition;
        Data.CargoType          = (int32)Job.CargoType;
    }

    return Data;
}

// ─────────────────────────────────────────────────────────────────────────────
void USaveManager::ApplyTruckData(const FTruckSaveData& Data, UWorld* World)
{
    ATruckController* Truck = Cast<ATruckController>(
        UGameplayStatics::GetPlayerPawn(World, 0));

    if (Truck)
    {
        Truck->SetActorLocation(Data.Position);
        Truck->SetActorRotation(Data.Rotation);

        if (Truck->FuelSystem)
            Truck->FuelSystem->CurrentFuelLitres = Data.FuelLitres;
    }
}

void USaveManager::ApplyPlayerData(const FPlayerSaveData& Data, UWorld* World)
{
    // Pass to XPSystem/EconomySystem — implementation left for those modules
    UE_LOG(LogTemp, Log, TEXT("Player data restored: Level %d, €%.0f"),
           Data.PlayerLevel, Data.MoneyEUR);
}

void USaveManager::ApplyJobData(const FActiveJobSaveData& Data, UWorld* World)
{
    if (!Data.bHasActiveJob) return;

    AJobSystem* JobSystem = Cast<AJobSystem>(
        UGameplayStatics::GetActorOfClass(World, AJobSystem::StaticClass()));

    if (JobSystem)
    {
        // Re-accept the saved job
        JobSystem->AcceptJob(Data.JobID);
    }
}
