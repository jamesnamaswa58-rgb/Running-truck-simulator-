#include "JobSystem.h"
#include "Kismet/KismetMathLibrary.h"

AJobSystem::AJobSystem()
{
    PrimaryActorTick.bCanEverTick = true;
    bHasActiveJob   = false;
    JobRefreshTimer = 0.0f;
}

void AJobSystem::BeginPlay()
{
    Super::BeginPlay();
    RefreshAvailableJobs(1);
}

void AJobSystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Countdown active job timer
    if (bHasActiveJob)
    {
        ActiveJob.TimeRemainingHours -= DeltaTime / 3600.0f;

        if (ActiveJob.TimeRemainingHours <= 0.0f)
        {
            ActiveJob.Status = EJobStatus::Failed;
            OnJobFailed.Broadcast(ActiveJob);
            bHasActiveJob = false;
            UE_LOG(LogTemp, Warning, TEXT("Job FAILED — time limit exceeded"));
        }
    }

    // Expire old available jobs and refresh
    JobRefreshTimer += DeltaTime;
    if (JobRefreshTimer >= JobRefreshInterval)
    {
        JobRefreshTimer = 0.0f;
        // Remove expired jobs
        AvailableJobs.RemoveAll([](const FDeliveryJob& J) {
            return J.Status == EJobStatus::Expired;
        });
    }
}

bool AJobSystem::AcceptJob(const FString& JobID)
{
    if (bHasActiveJob)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot accept job — already have active delivery"));
        return false;
    }

    for (FDeliveryJob& Job : AvailableJobs)
    {
        if (Job.JobID == JobID && Job.Status == EJobStatus::Available)
        {
            Job.Status      = EJobStatus::Accepted;
            ActiveJob       = Job;
            bHasActiveJob   = true;
            ActiveJob.Status = EJobStatus::InProgress;

            UE_LOG(LogTemp, Log, TEXT("Job accepted: %s → %s (%.0f km, €%.0f)"),
                   *ActiveJob.OriginCity, *ActiveJob.DestinationCity,
                   ActiveJob.DistanceKM, ActiveJob.BasePayEUR);
            return true;
        }
    }
    return false;
}

void AJobSystem::AbandonJob()
{
    if (!bHasActiveJob) return;

    ActiveJob.Status = EJobStatus::Failed;
    OnJobFailed.Broadcast(ActiveJob);
    bHasActiveJob = false;

    UE_LOG(LogTemp, Log, TEXT("Job abandoned: %s → %s"),
           *ActiveJob.OriginCity, *ActiveJob.DestinationCity);
}

void AJobSystem::UpdateCargoCondition(float DamageAmount)
{
    if (!bHasActiveJob) return;

    ActiveJob.CargoCondition = FMath::Clamp(
        ActiveJob.CargoCondition - DamageAmount, 0.0f, 1.0f);

    if (ActiveJob.CargoCondition < 0.1f)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cargo critically damaged!"));
    }
}

void AJobSystem::OnPlayerArrivedAtDestination()
{
    if (!bHasActiveJob) return;

    ActiveJob.Status = EJobStatus::Delivered;
    OnJobCompleted.Broadcast(ActiveJob);
    bHasActiveJob = false;

    const float Pay = CalculateFinalPay();
    UE_LOG(LogTemp, Log, TEXT("Delivery complete! Payout: €%.0f (cargo condition: %.0f%%)"),
           Pay, ActiveJob.CargoCondition * 100.0f);
}

float AJobSystem::CalculateFinalPay() const
{
    if (!bHasActiveJob) return 0.0f;

    // Time bonus: arriving early pays extra
    const float TimeBonus      = FMath::Clamp(ActiveJob.TimeRemainingHours / ActiveJob.TimeLimitHours, 0.0f, 1.0f);
    const float CargoBonus     = ActiveJob.CargoCondition;
    const float EfficiencyMultiplier = 0.7f + (TimeBonus * 0.15f) + (CargoBonus * 0.15f);

    return ActiveJob.BasePayEUR * EfficiencyMultiplier;
}

float AJobSystem::GetDistanceToDestination(FVector PlayerLocation) const
{
    if (!bHasActiveJob) return 0.0f;

    return FVector::Dist(PlayerLocation, ActiveJob.DestinationLocation) / 100000.0f; // cm → km
}

void AJobSystem::RefreshAvailableJobs(int32 PlayerLevel)
{
    AvailableJobs.Empty();
    const int32 JobCount = FMath::Min(MaxAvailableJobs, 3 + PlayerLevel / 5);

    for (int32 i = 0; i < JobCount; ++i)
    {
        AvailableJobs.Add(GenerateJob(PlayerLevel));
    }

    UE_LOG(LogTemp, Log, TEXT("Refreshed %d available jobs for level %d"),
           AvailableJobs.Num(), PlayerLevel);
}

FDeliveryJob AJobSystem::GenerateJob(int32 PlayerLevel) const
{
    FDeliveryJob Job;

    // City pairs (in a real project these come from a data table)
    const TArray<TPair<FString, FString>> CityPairs = {
        { TEXT("Stockholm"),  TEXT("Gothenburg") },
        { TEXT("Oslo"),       TEXT("Bergen")      },
        { TEXT("Helsinki"),   TEXT("Tampere")     },
        { TEXT("Copenhagen"), TEXT("Aarhus")      },
        { TEXT("Malmö"),      TEXT("Helsingborg") },
        { TEXT("Turku"),      TEXT("Oulu")        },
        { TEXT("Stavanger"),  TEXT("Trondheim")   }
    };

    const int32 PairIdx  = FMath::RandRange(0, CityPairs.Num() - 1);
    Job.OriginCity       = CityPairs[PairIdx].Key;
    Job.DestinationCity  = CityPairs[PairIdx].Value;
    Job.JobID            = GenerateJobID();
    Job.Status           = EJobStatus::Available;
    Job.CargoCondition   = 1.0f;

    // Cargo type scales with player level
    const int32 MaxCargoIdx = FMath::Min((int32)ECargoType::Fuel,
                                          FMath::Max(0, PlayerLevel / 5));
    Job.CargoType = (ECargoType)FMath::RandRange(0, MaxCargoIdx);

    Job.DistanceKM        = FMath::FRandRange(80.0f, 600.0f);
    Job.CargoWeightTonnes = FMath::FRandRange(5.0f, 24.0f);
    Job.BasePayEUR        = CalculateBaseRate(Job.CargoType, Job.DistanceKM);
    Job.TimeLimitHours    = Job.DistanceKM / 70.0f * 1.5f; // 50% buffer over ideal speed
    Job.TimeRemainingHours = Job.TimeLimitHours;
    Job.MinPlayerLevel    = FMath::Max(1, (int32)Job.CargoType * 5);

    // Placeholder world positions (real game uses actual city actors)
    Job.OriginLocation      = FVector::ZeroVector;
    Job.DestinationLocation = FVector(Job.DistanceKM * 100000.0f, 0.0f, 0.0f);

    return Job;
}

float AJobSystem::CalculateBaseRate(ECargoType Cargo, float DistanceKM) const
{
    float RatePerKM = 2.0f; // base €/km

    switch (Cargo)
    {
        case ECargoType::General:      RatePerKM = 2.0f;  break;
        case ECargoType::Refrigerated: RatePerKM = 2.8f;  break;
        case ECargoType::Hazardous:    RatePerKM = 4.5f;  break;
        case ECargoType::Oversized:    RatePerKM = 6.0f;  break;
        case ECargoType::Livestock:    RatePerKM = 3.2f;  break;
        case ECargoType::Timber:       RatePerKM = 2.5f;  break;
        case ECargoType::Fuel:         RatePerKM = 3.8f;  break;
    }

    return DistanceKM * RatePerKM;
}

FString AJobSystem::GenerateJobID() const
{
    return FString::Printf(TEXT("JOB-%08X"), FMath::Rand());
}
