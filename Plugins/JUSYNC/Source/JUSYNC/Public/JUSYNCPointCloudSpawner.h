#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "JUSYNCTypes.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include <atomic>
#include <memory>

#ifdef WITH_ANARI_USD_MIDDLEWARE
#include "LidarPointCloud.h"
#include "LidarPointCloudComponent.h"
#endif

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPointCloudSpawned, const FString&, AActor*);

struct FPointCloudReadyEntry
{
#ifdef WITH_ANARI_USD_MIDDLEWARE
    TArray64<FLidarPointCloudPoint> Points;
#endif
    FString ElementName;
    FString Key;
    int32 Rank;
    float PointSize;
    bool bSpawned;
    bool bNeedsRecolor;

    // Kept for gradient recoloring when LUT arrives after white fallback
    TArray<FVector> Positions;
    TArray<float> Widths;

    FPointCloudReadyEntry()
        : Rank(0), PointSize(1.0f), bSpawned(false), bNeedsRecolor(false)
    {}
};

class JUSYNC_API FJUSYNCPointCloudSpawner : public FTickableGameObject
{
public:
    FJUSYNCPointCloudSpawner(TWeakObjectPtr<UObject> InOwner);
    virtual ~FJUSYNCPointCloudSpawner();

    void EnqueuePointCloud(const FJUSYNCPointCloudData& PCData, int32 InRank = 0);
    // Rvalue overload: moves the point cloud into the async task (no copy).
    // bAllowNoGradient: when true a missing gradient LUT does NOT defer the
    // spawn (used by the PendingNoColorClouds expiry flush). Default false so
    // freshly parsed clouds still wait briefly for their colors.
    void EnqueuePointCloud(FJUSYNCPointCloudData&& PCData, int32 InRank = 0, bool bAllowNoGradient = false);
    // Shared overload: avoids an extra copy when the caller owns the payload exclusively.
    void EnqueuePointCloud(FJUSYNCPointCloudRef&& PCRef, int32 InRank = 0);

    /** jusync.Spawner.IgnoreElements check (comma-separated substrings / * patterns).
     *  Public so dispatchers can drop elements BEFORE booking pipeline counters. */
    bool IsElementFiltered(const FString& ElementName) const;
    virtual void Tick(float DeltaTime) override;
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
    virtual bool IsTickable() const override { return Owner.IsValid() && !bShuttingDown.load(); }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FJUSYNCPointCloudSpawner, STATGROUP_Tickables); }

    void SetMaxPoolSize(int32 InMax) { MaxPoolSize = InMax; }
    void SetBudgetMs(float InBudget) { BudgetMs = InBudget; }
    void SetSpawnLocation(const FVector& In);
    void SetSpawnScale(float In) { SpawnScale = In; }

    void SetPointSize(float InSize) { PointSize = InSize; }
    void SetPointSizeBias(float InBias) { PointSizeBias = InBias; }
    void SetGapFillingStrength(float InStrength) { GapFillingStrength = InStrength; }
    void SetPerfLogging(bool InEnabled) { bPerfLogging = InEnabled; }
    void SetNormalCalculation(bool bEnabled, int32 InMaxPoints, int32 InQuality, float InNoiseTolerance, float InCooldownSeconds)
    {
        bCalculateNormals = bEnabled;
        MaxPointsForNormals = FMath::Max(0, InMaxPoints);
        NormalsQuality = FMath::Clamp(InQuality, 1, 100);
        NormalsNoiseTolerance = FMath::Max(0.0f, InNoiseTolerance);
        NormalsCooldownSeconds = FMath::Max(0.0f, InCooldownSeconds);
    }
#ifdef WITH_ANARI_USD_MIDDLEWARE
    void SetPointShape(ELidarPointCloudSpriteShape InShape) { PointShape = InShape; }
    void SetPointOrientation(ELidarPointCloudSpriteOrientation InOrientation) { PointOrientation = InOrientation; }
    void SetPointScaling(ELidarPointCloudScalingMethod InScaling) { PointScaling = InScaling; }
    void ApplyVisualSettingsToComponent(ULidarPointCloudComponent* Comp) { ApplyVisualSettings(Comp); }

    /** Sets the sprite shape and re-applies it to every actor this spawner currently owns. */
    void ApplyPointShapeToActiveActors(ELidarPointCloudSpriteShape InShape);

    /** Re-applies the current point-sizing settings (octree-free FixedScreenSize vs scaling
     *  method) to every actor this spawner currently owns, dirtying their render state. */
    void ApplySizingToActiveActors();
    void RequestNormalCalculation(ULidarPointCloud* Cloud, AActor* Actor, int32 PointCount) { MaybeCalculateNormals(Cloud, Actor, PointCount); }
#endif

    void ClearAllActors();
    void WaitForCompletion(float TimeoutSeconds = 5.0f);

    /** Destroy a tracked actor and remove it from all internal bookkeeping. */
    void DestroyTrackedActor(AActor* Actor);

    /** Destroy ALL actors including pooled ones (for full scene cleanup) */
    void DestroyAllActors();

    /** Set a 256-entry gradient color LUT from a decoded PNG row. Attribute0 values map to these colors. */
    void SetGradientLUT(const TArray<FColor>& InLUT);
    TArray<FColor> GetGradientLUT() const { FScopeLock Lock(&GradientMutex); return GradientLUT; }
    uint64 GetLUTVersion() const { FScopeLock Lock(&GradientMutex); return LUTVersion; }

    /** Recolor all actors spawned with white fallback (no gradient at spawn time). Call after SetGradientLUT. */
    void RecolorGradientPendingActors();

    int32 GetGradientPendingCount() const { return GradientPendingData.Num(); }

    int32 GetReadyQueueCount() const { return ReadyQueue.Num(); }
    int32 GetActiveActorCount() const { return ActiveActors.Num(); }

    AActor* FindActorByElementKey(const FString& ElementKey) const;
    FString GetElementKeyForActor(AActor* Actor) const;

    void DrainReadyQueue();
    void DrainReadyQueueLocked();

    /** Async live-streaming path: is a background cloud build in flight / stashed for this key? */
    int32 GetInFlightBuildCount() const { return InFlightBuilds.Num(); }

    FOnPointCloudSpawned OnPointCloudSpawned;

private:
    TWeakObjectPtr<UObject> Owner;
    TArray<FPointCloudReadyEntry> ReadyQueue;
    FCriticalSection QueueMutex;
    mutable FCriticalSection GradientMutex;
    TArray<AActor*> AvailablePool;
    TSet<AActor*> ActiveActors;
    int32 MaxPoolSize;
    FVector SpawnLocation;
    float SpawnScale;
    float BudgetMs;
    float PointSize = 1.0f;
    float PointSizeBias = 0.035f;
    float GapFillingStrength = 0.0f;
    bool bPerfLogging = false;
#ifdef WITH_ANARI_USD_MIDDLEWARE
    ELidarPointCloudSpriteShape PointShape = ELidarPointCloudSpriteShape::Circle;
    ELidarPointCloudSpriteOrientation PointOrientation = ELidarPointCloudSpriteOrientation::PreferFacingCamera;
    ELidarPointCloudScalingMethod PointScaling = ELidarPointCloudScalingMethod::PerNodeAdaptive;
    bool bCalculateNormals = false;
    int32 MaxPointsForNormals = 1000000;
    int32 NormalsQuality = 10;
    float NormalsNoiseTolerance = 0.05f;
    float NormalsCooldownSeconds = 2.0f;
    TMap<AActor*, double> LastNormalCalcTime;
    void MaybeCalculateNormals(ULidarPointCloud* Cloud, AActor* Actor, int32 PointCount);
    // Last-applied sizing cvar values; impossible defaults so the first Tick
    // always re-applies sizing to on-screen clouds once.
    int32 LastFixedSizeState = -999999;
    float LastFixedScreenSize = -999999.0f;
#endif
    std::atomic<bool> bShuttingDown{false};
    std::atomic<int32> InFlightTasks{0};
    // Async cloud builds are counted in a shared_ptr'd atomic rather than
    // InFlightTasks: Epic's worker thread decrements it without dereferencing
    // the spawner, and AsyncBuildsAlive (cleared in ~spawner) guards the GT
    // swap hop so a completion that lands after teardown is dropped safely.
    std::shared_ptr<std::atomic<int32>> AsyncBuildsOutstanding = std::make_shared<std::atomic<int32>>(0);
    std::shared_ptr<std::atomic<bool>> AsyncBuildsAlive = std::make_shared<std::atomic<bool>>(true);

    bool BeginTask();
    void EndTask();

    TArray<FColor> GradientLUT;
    uint64 LUTVersion = 0;
    TSet<AActor*> GradientPendingActors;
    struct FRecolorData { TArray<FVector> Positions; TArray<float> Widths; };
    TMap<AActor*, FRecolorData> GradientPendingData;

    TMap<FString, AActor*> ElementToActor;
    TMap<AActor*, FString> ActorToElement;

#ifdef WITH_ANARI_USD_MIDDLEWARE
    /** State of one in-flight background ULidarPointCloud build (game-thread only). */
    struct FInFlightCloudBuild
    {
        // Heap points read by the Epic async build thread; freed on completion.
        TArray64<FLidarPointCloudPoint>* Points = nullptr;
        // Strong ref so the freshly-created asset can't be GC'd while the
        // worker builds its octree (lambdas/task captures are not roots).
        TUniquePtr<TStrongObjectPtr<UObject>> CloudRef;
        TWeakObjectPtr<AActor> Actor;
        FPointCloudReadyEntry PendingReplacement;
        double BuildStarted = 0.0;
        bool bEverSwapped = false;
        bool bHasPending = false;
        bool bBuilding = false;
        uint64 Generation = 0;

        // --- GPU-free async ramp (see FCloudRampChain) ---
        /** Swap is held on the render-finished asset until this wall time (0 = apply now). */
        double EarliestApply = 0.0;
        /** Build finished but its swap is scheduled for later (ramp hold window). */
        bool bDeferred = false;
        /** This build belongs to a ramp chain (swap records display positions). */
        bool bChainStep = false;
        /** The last step of a chain: swapping it in completes the ramp. */
        bool bChainFinal = false;
        /** A newer sample superseded this step: drop its cloud instead of swapping. */
        bool bChainAborted = false;
        /** Input-order positions this build will display; recorded on swap. */
        TArray<FVector3f> RampPos;

        // Frees the heap point array if the entry dies without a GT completion
        // (shutdown path). WaitForCompletion waits for every worker to finish
        // before the spawner (and this map) can die, so by contract the worker
        // is no longer touching Points here.
        ~FInFlightCloudBuild() { delete Points; Points = nullptr; }
    };
    TMap<FString, TSharedRef<FInFlightCloudBuild>> InFlightBuilds;
    uint64 NextCloudBuildGeneration = 0;

    /**
     * Async ramp chain: on a live update for an existing cloud we synthesize
     * 1..N interpolated point sets (prev -> new), build each through the normal
     * async pipeline, and swap them in on a timed schedule so the cloud glides
     * across the blend window instead of popping on one instant.
     */
    struct FCloudRampChain
    {
        FPointCloudReadyEntry Entry;     // the real sample, held for the final step
        TArray<FVector3f> PrevPos;       // displayed positions at chain start (input order)
        AActor* Actor = nullptr;
        double T0 = 0.0;
        double Duration = 0.0;
        int32 Step = 0;                  // steps KICKED so far (swap timing handled by build deferral)
        int32 TotalSteps = 2;            // interpolated steps + 1 final
        bool bFinalKicked = false;
    };
    TMap<FString, FCloudRampChain> RampChains;

    /** Last applied / in-place-displayed input-order positions per streaming key. */
    TMap<FString, TArray<FVector3f>> CloudDisplayPos;
    TMap<FString, double> CloudLastArrival;

    /**
     * Per-frame SMOOTH cloud morph (the butter option): instead of stepped
     * ramp swaps, the DISPLAYED cloud's own point data is lerped every frame
     * through the plugin's public ExecuteActionOnAllPoints (which flags the
     * touched nodes' render data dirty -> the LOD manager re-uploads to the
     * GPU every tick). No plugin modification. Stable source point order
     * (VTK/USD streaming) is the correspondence source; on mismatch we fall
     * back to the ramp chain. The authoritative new cloud builds concurrently
     * and swaps at blend end (deferred-swap machinery).
     */
    struct FCloudSmoothMorph
    {
        /** Octree-walk-order positions captured at morph start / retarget. */
        TArray<FVector3f> Start;
        /** Destination positions, same walk order (prefix/birth rules apply). */
        TArray<FVector3f> End;
        TWeakObjectPtr<AActor> Actor;
        double T0 = 0.0;
        double Duration = 0.0;
    };
    TMap<FString, FCloudSmoothMorph> SmoothMorphs;

    /**
     * Walk-slot -> input-order index for the currently displayed cloud
     * (resolved once per swapped-in cloud by exact-quantized position match;
     * reused for retargets since walk order is stable while the asset lives).
     */
    TMap<FString, TArray<int32>> CloudWalkInputIdx;

    bool StartCloudSmoothMorph(const FString& Key, FPointCloudReadyEntry& Entry, AActor* Actor,
                               double T0, double Duration);
    void AdvanceCloudSmoothMorphs(double NowSeconds);

    void StartAsyncCloudBuild(const FString& Key, FPointCloudReadyEntry&& Entry, AActor* Actor,
                              double EarliestApply = 0.0, bool bChainStep = false, bool bChainFinal = false);
    void OnAsyncCloudBuildFinished(const FString& Key, uint64 Generation, bool bSuccess);

    // Ramp helpers (game-thread only)
    bool ShouldStartCloudRamp(const FString& Key, double NowSeconds) const;
    void KickRampStep(const FString& Key, FCloudRampChain& Chain);
    void AbortDeferredSwap(const FString& Key);
    void TickCloudRamps(double NowSeconds);
# endif

    struct FPendingNoColorCloud
    {
        FJUSYNCPointCloudData Data;
        int32 Rank = 0;
        double QueuedTime = 0.0;
    };
    TArray<FPendingNoColorCloud> PendingNoColorClouds;

    void UntrackActorElement(AActor* Actor);

    /** Applies the current SpawnLocation / SpawnScale to an actor, including pooled/reused ones. */
    void ApplyTransformToActor(AActor* Actor);

    AActor* AllocateActor();
    void ReleaseActor(AActor*);
    void FlushExpiredNoColorClouds();
#ifdef WITH_ANARI_USD_MIDDLEWARE
    ULidarPointCloud* GetOrCreateCloudForComponent(ULidarPointCloudComponent* Comp);
    void ApplyVisualSettings(ULidarPointCloudComponent* Comp);
    void ApplyVisualSettingsToActor(AActor* Actor);
#endif
};
