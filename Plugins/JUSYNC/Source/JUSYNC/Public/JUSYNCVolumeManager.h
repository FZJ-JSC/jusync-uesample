#pragma once

#include "CoreMinimal.h"
#include "JUSYNCVolumeBuilder.h"

#include <atomic>

class AJUSYNCVolumeActor;
class UMaterialInterface;
class UWorld;

class JUSYNC_API FJUSYNCVolumeManager
{
public:
    explicit FJUSYNCVolumeManager(UWorld* InWorld);
    ~FJUSYNCVolumeManager();

    void Submit(const FString& Key, const FString& ElementName,
                const FJUSYNCPointArrayPtr& Points,
                const FVector& WorldScale, const FVector& WorldOffset);

    void Tick();
    void Remove(const FString& Key);
    void ClearAll();
    void WaitForCompletion(float TimeoutSeconds);

    int32 GetPendingBuildCount() const;

private:
    struct FEntry
    {
        TWeakObjectPtr<AJUSYNCVolumeActor> Actor;
        FString ElementName;
        FJUSYNCVolumeGridPtr Ready;
        uint64 Generation = 0;
        bool bBuilding = false;
        double LastSubmit = 0.0;
    };

    struct FState
    {
        TWeakObjectPtr<UWorld> World;
        TMap<FString, FEntry> Entries;
        std::atomic<int32> BuildsAlive{0};
    };

    TSharedPtr<FState, ESPMode::ThreadSafe> State;
    TWeakObjectPtr<UMaterialInterface> Material;
    uint64 NextGeneration = 0;

    UMaterialInterface* ResolveMaterial();
    void DestroyActor(FEntry& Entry);
};
