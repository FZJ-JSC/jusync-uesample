#include "JUSYNCVolumeManager.h"

#include "Async/Async.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "JUSYNCVolumeActor.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

static TAutoConsoleVariable<int32> CVarJusyncVolumeResolution(
    TEXT("jusync.Volume.Resolution"), 256,
    TEXT("Voxel grid edge length used by the volume display mode; lowered automatically when it exceeds what the point count can fill."));

static TAutoConsoleVariable<int32> CVarJusyncVolumeBlurPasses(
    TEXT("jusync.Volume.BlurPasses"), 1,
    TEXT("Separable [1,2,1] smoothing passes applied to the density grid."));

static TAutoConsoleVariable<float> CVarJusyncVolumeSplatRadius(
    TEXT("jusync.Volume.SplatRadius"), 1.5f,
    TEXT("Gaussian splat radius in voxels; keeps the field continuous where voxels are finer than the point spacing. 0 writes each point into its own voxel."));

static TAutoConsoleVariable<int32> CVarJusyncVolumeMinIntervalMs(
    TEXT("jusync.Volume.MinIntervalMs"), 200,
    TEXT("Minimum milliseconds between voxelization submits for one element."));

static TAutoConsoleVariable<FString> CVarJusyncVolumeMaterial(
    TEXT("jusync.Volume.Material"), TEXT("/Game/Materials/M_JusyncVolume.M_JusyncVolume"),
    TEXT("Raymarch material applied to volume actors."));

FJUSYNCVolumeManager::FJUSYNCVolumeManager(UWorld* InWorld)
{
    State = MakeShared<FState, ESPMode::ThreadSafe>();
    State->World = InWorld;
}

FJUSYNCVolumeManager::~FJUSYNCVolumeManager()
{
    ClearAll();
}

void FJUSYNCVolumeManager::Submit(const FString& Key, const FString& ElementName,
                                  const FJUSYNCPointArrayPtr& Points,
                                  const FVector& WorldScale, const FVector& WorldOffset)
{
    if (!State.IsValid() || !Points.IsValid() || Points->Num() == 0)
    {
        return;
    }

    const double Now = FPlatformTime::Seconds();
    const double MinInterval = FMath::Max(CVarJusyncVolumeMinIntervalMs.GetValueOnGameThread(), 0) / 1000.0;

    FEntry& Entry = State->Entries.FindOrAdd(Key);
    Entry.ElementName = ElementName;

    if (Entry.bBuilding || (Now - Entry.LastSubmit) < MinInterval)
    {
        return;
    }

    const int32 Resolution = FMath::Clamp(CVarJusyncVolumeResolution.GetValueOnGameThread(), 16, 256);
    const int32 BlurPasses = FMath::Clamp(CVarJusyncVolumeBlurPasses.GetValueOnGameThread(), 0, 8);
    const float SplatRadius = FMath::Clamp(CVarJusyncVolumeSplatRadius.GetValueOnGameThread(), 0.f, 3.f);

    Entry.bBuilding = true;
    Entry.LastSubmit = Now;
    const uint64 Generation = ++NextGeneration;
    Entry.Generation = Generation;

    FJUSYNCVolumeGridPtr Grid = MakeShared<FJUSYNCVolumeGrid, ESPMode::ThreadSafe>();
    TSharedPtr<FState, ESPMode::ThreadSafe> LocalState = State;
    LocalState->BuildsAlive.fetch_add(1);

    Async(EAsyncExecution::ThreadPool,
        [LocalState, Key, Generation, Grid, Points, WorldScale, WorldOffset,
         Resolution, BlurPasses, SplatRadius]()
        {
            FJUSYNCVolumeBuilder::Build(*Points, WorldScale, WorldOffset, Resolution, BlurPasses, SplatRadius, *Grid);
            LocalState->BuildsAlive.fetch_sub(1);

            AsyncTask(ENamedThreads::GameThread, [LocalState, Key, Generation, Grid]()
            {
                if (FEntry* Found = LocalState->Entries.Find(Key))
                {
                    Found->bBuilding = false;
                    if (Found->Generation == Generation)
                    {
                        Found->Ready = Grid;
                    }
                }
            });
        });
}

void FJUSYNCVolumeManager::Tick()
{
    if (!State.IsValid())
    {
        return;
    }

    if (!Material.IsValid())
    {
        ResolveMaterial();
    }
    UMaterialInterface* VolumeMaterial = Material.Get();

    for (TPair<FString, FEntry>& Pair : State->Entries)
    {
        FEntry& Entry = Pair.Value;
        if (!Entry.Ready.IsValid())
        {
            continue;
        }

        FJUSYNCVolumeGridPtr Grid = Entry.Ready;
        Entry.Ready.Reset();

        if (!Grid.IsValid() || !Grid->IsValid())
        {
            continue;
        }

        AJUSYNCVolumeActor* Actor = Entry.Actor.Get();
        if (!IsValid(Actor))
        {
            UWorld* World = State->World.Get();
            if (!World || World->bIsTearingDown)
            {
                continue;
            }

            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Actor = World->SpawnActor<AJUSYNCVolumeActor>(
                AJUSYNCVolumeActor::StaticClass(), FTransform::Identity, Params);
            Entry.Actor = Actor;
        }

        if (IsValid(Actor))
        {
            Actor->ApplyGrid(*Grid, VolumeMaterial);
        }
    }
}

void FJUSYNCVolumeManager::Remove(const FString& Key)
{
    if (!State.IsValid())
    {
        return;
    }

    FEntry Entry;
    if (State->Entries.RemoveAndCopyValue(Key, Entry))
    {
        DestroyActor(Entry);
    }
}

void FJUSYNCVolumeManager::ClearAll()
{
    if (!State.IsValid())
    {
        return;
    }

    for (TPair<FString, FEntry>& Pair : State->Entries)
    {
        DestroyActor(Pair.Value);
    }
    State->Entries.Reset();
}

void FJUSYNCVolumeManager::WaitForCompletion(float TimeoutSeconds)
{
    if (!State.IsValid())
    {
        return;
    }

    const double Deadline = FPlatformTime::Seconds() + FMath::Max(TimeoutSeconds, 0.f);
    while (State->BuildsAlive.load() > 0 && FPlatformTime::Seconds() < Deadline)
    {
        FPlatformProcess::Sleep(0.001f);
    }
}

int32 FJUSYNCVolumeManager::GetPendingBuildCount() const
{
    return State.IsValid() ? State->BuildsAlive.load() : 0;
}

UMaterialInterface* FJUSYNCVolumeManager::ResolveMaterial()
{
    const FString Path = CVarJusyncVolumeMaterial.GetValueOnGameThread();
    if (Path.IsEmpty())
    {
        return nullptr;
    }

    UMaterialInterface* Loaded = LoadObject<UMaterialInterface>(nullptr, *Path);
    if (Loaded)
    {
        Material = Loaded;
    }
    return Loaded;
}

void FJUSYNCVolumeManager::DestroyActor(FEntry& Entry)
{
    AJUSYNCVolumeActor* Actor = Entry.Actor.Get();
    Entry.Actor = nullptr;
    Entry.Ready.Reset();

    if (!IsValid(Actor))
    {
        return;
    }

    UWorld* World = Actor->GetWorld();
    if (World && World->bIsTearingDown)
    {
        return;
    }

    Actor->Destroy();
}
