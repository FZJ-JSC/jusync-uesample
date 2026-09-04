#include "JUSYNCFileSpawnerActor.h"
#include <atomic>
#include "JUSYNCSubsystem.h"
#include "JUSYNCPointCloudSpawner.h"
#include "JUSYNCUSDLoader.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Kismet/GameplayStatics.h"
#include "Templates/UniquePtr.h"
#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Async/Async.h"
#include "Tasks/Task.h"

static bool JUSYNCIsLiveUpdateGeometryFile(const FString& Filename)
{
    if (!Filename.EndsWith(TEXT(".usda")))
    {
        return false;
    }
    if (Filename.StartsWith(TEXT("Session_")) ||
        Filename == TEXT("scene.usda") ||
        Filename.Contains(TEXT("manifest")) ||
        Filename.Contains(TEXT("images/")) ||
        Filename.Contains(TEXT("shared/")) ||
        Filename.Contains(TEXT("primstages/")) ||
        Filename.Contains(TEXT("_Light.usda")) ||
        Filename.Contains(TEXT("_Material.usda")) ||
        Filename.Contains(TEXT("_Camera.usda")) ||
        Filename.Contains(TEXT("_Sampler.usda")))
    {
        return false;
    }
    return true;
}

AJUSYNCFileSpawnerActor::AJUSYNCFileSpawnerActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    BrokerEndpoint = TEXT("tcp://localhost:5556");
    RequestTimeoutMs = 5000;
    BandwidthBytesPerSecond = 10737418240.0f;
    MinimumFileSizeBytes = 1000;
    bFilterUSDOnly = true;
    bClipsOnly = true;
    SpawnTargetActor = nullptr;
    BaseSpawnLocation = FVector::ZeroVector;
    SpawnSpacing = 0.0f;
    SpawnGridColumns = 10;
    SpawnMaterial = nullptr;
    TextureSampleParameterName = TEXT("");
    bMeshTextureLoading = false;
    bMeshTextureReady = false;
    SpawnScale = FVector::OneVector;
    bUseUniformScaling = true;
    bAutoStart = true;
    bEnableLiveUpdates = true;
    LiveUpdatePollInterval = 20.0f;
    bAutoRefreshMeshes = true;
    LastCommitCompleteTime = 0.0;
    CommitCompleteCooldownSeconds = 0.5f;
    bSceneDiffInFlight = false;
    bInitialSpawnDone = false;
    bSpawnPointClouds = true;
    bUseGradientColors = true;
    GradientPngFilename = TEXT("");
    PointCloudSize = 1.0f;
    PointShape = EJUSYNCPointShape::Circle;
    PointOrientation = EJUSYNCPointOrientation::FacingCamera;
    PointScaling = EJUSYNCPointScaling::PerNodeAdaptive;
    PointSizeBias = 0.035f;
    GapFillingStrength = 0.0f;
    PointCloudPoolSize = 16;
    bCalculatePointCloudNormals = true;
    PointCloudNormalsMaxPoints = 1000000;
    PointCloudNormalsQuality = 10;
    PointCloudNormalsNoiseTolerance = 0.05f;
    PointCloudNormalsCooldownSeconds = 2.0f;
    bGradientReady = false;
    bGradientDownloadStarted = false;
    bGradientAttempted = false;
    PipelineDepth = 10;
    bEnablePerfLogging = false;
    MaxRetries = 2;
    CurrentRetryCount = 0;
    CurrentState = EJUSYNCSpawnerState::Idle;
    FilesDownloaded = 0;
    FilesTotal = 0;
    ActorsSpawned = 0;
    NextSpawnIndex = 0;
    PendingDownloads = 0;
    PendingAsyncSpawns = 0;
    PendingAsyncPCS = 0;
    PendingParseTasks = 0;
    PipelineNextIndex = 0;
    PipelineActive = 0;
    bIsCancelled = false;
    MaxSpawnsPerFrame = 3;

    bEnableMeshCache = true;
    MeshCacheMaxEntries = 64;
    MeshCacheMaxMB = 2048;
    bCachePointClouds = true;

    bEnableTimeStepAnimation = false;
    TimeStepPlaybackFPS = 30.0f;
    bLoopTimeStepAnimation = true;

    MeshCache = MakeUnique<FJUSYNCMeshCache>();
    ChangeTracker = MakeUnique<FJUSYNCFileChangeTracker>();
    AnimationController = MakeUnique<FJUSYNCAnimationController>();
}

void AJUSYNCFileSpawnerActor::BeginPlay()
{
    Super::BeginPlay();

    if (MeshCache)
    {
        MeshCache->SetLimits(MeshCacheMaxEntries, static_cast<int64>(MeshCacheMaxMB) * 1024 * 1024);
    }
    if (AnimationController)
    {
        AnimationController->SetPlaybackSettings(TimeStepPlaybackFPS, bLoopTimeStepAnimation);
    }

    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    if (Subsystem)
    {
        Subsystem->OnNotificationReceived.AddDynamic(this, &AJUSYNCFileSpawnerActor::OnBrokerNotification);
    }

    if (bAutoStart)
    {
        UE_LOG(LogTemp, Display, TEXT("JUSYNC Spawner: auto-starting pipeline"));
        StartSpawning();
    }
}

void AJUSYNCFileSpawnerActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopLiveUpdatePolling();

    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    if (Subsystem)
    {
        Subsystem->OnNotificationReceived.RemoveAll(this);
    }

    CancelSpawning();
    ClearSpawnedActors();
    Super::EndPlay(EndPlayReason);
}

void AJUSYNCFileSpawnerActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bEnableLiveUpdates && LiveUpdatePollInterval > 0.0f &&
        bInitialSpawnDone && CurrentState == EJUSYNCSpawnerState::Complete)
    {
        LiveUpdatePollAccumulator += DeltaSeconds;
        if (LiveUpdatePollAccumulator >= LiveUpdatePollInterval)
        {
            LiveUpdatePollAccumulator = 0.0f;
            OnLiveUpdateTimer();
        }
    }

    if (AnimationController)
    {
        if (bEnableTimeStepAnimation)
        {
            AnimationController->Tick(DeltaSeconds);
        }
        else
        {
            AnimationController->ShowAll();
        }
    }

    if (bInitialSpawnDone)
    {
        FlushHiddenMeshUpdates();
    }
}

void AJUSYNCFileSpawnerActor::StartSpawning()
{
    if (CurrentState != EJUSYNCSpawnerState::Idle &&
        CurrentState != EJUSYNCSpawnerState::Complete &&
        CurrentState != EJUSYNCSpawnerState::Error)
    {
        UE_LOG(LogTemp, Warning, TEXT("JUSYNC Spawner: already running in state %d"), (int32)CurrentState);
        return;
    }

    ClearSpawnedActors();
    RawFileList.Empty(); RawFileSizes.Empty(); RawFileRanks.Empty();
    FilteredFiles.Empty(); FilteredSizes.Empty(); FilteredRanks.Empty();
    GradientPngRankMap.Empty();
    ParseFailedIndices.Empty();
    GradientPendingMeshes.Empty();
    bGradientReady = false;
    bGradientDownloadStarted = false;
    FilesDownloaded = 0; FilesTotal = 0; ActorsSpawned = 0;
    FailedFileIndices.Empty();
    NextSpawnIndex = 0; PendingDownloads = 0; PendingAsyncSpawns = 0;
    PendingAsyncPCS = 0; PendingParseTasks = 0;
    bIsCancelled = false;
    CurrentRetryCount = 0;
    CurrentState = EJUSYNCSpawnerState::Connecting;
    bInitialSpawnDone = false;
    bSceneDiffInFlight = false;
    PipelineActive = 0;
    if (ChangeTracker) ChangeTracker->Clear();
    DeferredSpawns.Empty();
    FilenameToPCActors.Empty();
    PCElementToFilename.Empty();
    if (AnimationController) AnimationController->Clear();

    UE_LOG(LogTemp, Display, TEXT("JUSYNC Spawner: starting pipeline"));
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("[Spawner] Connecting to %s"), *BrokerEndpoint));

    if (UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem())
    {
        if (FJUSYNCPointCloudSpawner* Spawner = Subsystem->GetPointCloudSpawner())
        {
            Spawner->OnPointCloudSpawned.Clear();
            Spawner->OnPointCloudSpawned.AddUObject(this, &AJUSYNCFileSpawnerActor::OnPointCloudSpawnedHandler);
        }
    }

    ConnectToBroker();
}

void AJUSYNCFileSpawnerActor::CancelSpawning()
{
    bIsCancelled = true;
    PendingAsyncSpawns = 0;
    PendingAsyncPCS = 0;
    PendingParseTasks = 0;
    CurrentState = EJUSYNCSpawnerState::Idle;
    PendingDownloads = 0;
    PipelineActive = 0;
    bSceneDiffInFlight = false;
    if (ChangeTracker) ChangeTracker->Clear();
    DeferredSpawns.Empty();
}

void AJUSYNCFileSpawnerActor::ClearSpawnedActors()
{
    if (UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem())
    {
        Subsystem->ClearProcessedFiles();
        if (FJUSYNCPointCloudSpawner* PCSpawner = Subsystem->GetPointCloudSpawner())
        {
            PCSpawner->DestroyAllActors();
        }
    }

    for (AActor* Actor : SpawnedActors)
    {
        if (Actor && Actor->IsValidLowLevel())
        {
            if (AnimationController) AnimationController->RemoveActor(Actor);
            Actor->Destroy();
        }
    }
    SpawnedActors.Empty();
    ActorsSpawned = 0;
    FileToActorMap.Empty();
    FilenameToActors.Empty();
    FilenameToPCActors.Empty();
    PCElementToFilename.Empty();
    FileLastSize.Empty();
    FileHashLo.Empty();
    FileHashHi.Empty();
    GradientPendingMeshes.Empty();
    HiddenPendingMeshUpdates.Empty();
    if (AnimationController) AnimationController->Clear();
}

FVector AJUSYNCFileSpawnerActor::GetNextSpawnLocation() const
{
    FVector Origin = BaseSpawnLocation;
    if (SpawnTargetActor && SpawnTargetActor->IsValidLowLevel() && !SpawnTargetActor->HasAnyFlags(RF_ClassDefaultObject))
    {
        Origin = SpawnTargetActor->GetActorLocation();
    }
    else if (GetWorld() && !HasAnyFlags(RF_ClassDefaultObject))
    {
        Origin = GetActorLocation();
    }

    int32 Col = NextSpawnIndex % SpawnGridColumns;
    int32 Row = NextSpawnIndex / SpawnGridColumns;
    return Origin + FVector(Col * SpawnSpacing, Row * SpawnSpacing, 0.0f);
}

int32 AJUSYNCFileSpawnerActor::CalculateDynamicTimeout(int64 FileSizeBytes) const
{
    const int32 BaseTimeout = 5000;
    const int32 MaxTimeout = 600000;
    float TimeForTransfer = (static_cast<float>(FileSizeBytes) / FMath::Max(1.0f, BandwidthBytesPerSecond)) * 1000.0f;
    int32 DynamicTimeout = BaseTimeout + static_cast<int32>(TimeForTransfer);
    return FMath::Clamp(DynamicTimeout, RequestTimeoutMs, MaxTimeout);
}

void AJUSYNCFileSpawnerActor::ConnectToBroker()
{
    if (bIsCancelled) return;

    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    if (!Subsystem)
    {
        CurrentState = EJUSYNCSpawnerState::Error;
        OnError.Broadcast(TEXT("Subsystem not available"));
        return;
    }

    bool bInitialized = Subsystem->InitializeMiddleware();
    if (!bInitialized)
    {
        UE_LOG(LogTemp, Display, TEXT("JUSYNC Spawner: middleware initialization failed"));
        GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red, TEXT("[Spawner] Middleware init failed."));
        CurrentState = EJUSYNCSpawnerState::Error;
        OnError.Broadcast(TEXT("Middleware initialization failed"));
        return;
    }

    bool bConnected = Subsystem->ConnectToBroker(BrokerEndpoint, 5000);
    if (!bConnected)
    {
        UE_LOG(LogTemp, Display, TEXT("JUSYNC Spawner: failed to connect to broker at %s"), *BrokerEndpoint);
        GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red, TEXT("[Spawner] Failed to connect to broker!"));
        CurrentState = EJUSYNCSpawnerState::Error;
        OnError.Broadcast(TEXT("Failed to connect to broker"));
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("JUSYNC Spawner: connected, requesting file list"));
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, TEXT("[Spawner] Connected, fetching file list..."));
    CurrentState = EJUSYNCSpawnerState::FetchingList;
    RequestFileList();
}

void AJUSYNCFileSpawnerActor::RequestFileList()
{
    if (bIsCancelled) return;

    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    if (!Subsystem)
    {
        OnFileListError(TEXT("Subsystem not available"));
        return;
    }

    TWeakObjectPtr<UJUSYNCSubsystem> WeakSubsystem = Subsystem;
    TWeakObjectPtr<AJUSYNCFileSpawnerActor> WeakThis = this;

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakSubsystem, WeakThis]()
    {
        if (!WeakSubsystem.IsValid() || !WeakThis.IsValid()) return;

        TArray<FString> Files;
        TArray<int64> Sizes;
        TArray<int32> Ranks;
        TArray<uint64> HashLo;
        TArray<uint64> HashHi;
        bool bSuccess = WeakSubsystem->RequestFileListWithSizesAndRanks(-1, 15000, Files, Sizes, Ranks, &HashLo, &HashHi);

        FFunctionGraphTask::CreateAndDispatchWhenReady(
            [WeakThis, Files = MoveTemp(Files), Sizes = MoveTemp(Sizes), Ranks = MoveTemp(Ranks), HashLo = MoveTemp(HashLo), HashHi = MoveTemp(HashHi), bSuccess]()
            {
                if (WeakThis.IsValid())
                {
                    WeakThis->RawHashLo = HashLo;
                    WeakThis->RawHashHi = HashHi;
                    WeakThis->OnFileListReceived_Internal(Files, Sizes, Ranks, bSuccess, HashLo, HashHi);
                }
            },
            TStatId(), nullptr, ENamedThreads::GameThread);
    });
}

void AJUSYNCFileSpawnerActor::FilterFileList(const TArray<FString>& InFiles, const TArray<int64>& InSizes, const TArray<int32>& InRanks,
    const TArray<uint64>& InHashLo, const TArray<uint64>& InHashHi,
    TArray<FString>& OutFiles, TArray<int64>& OutSizes, TArray<int32>& OutRanks,
    TArray<uint64>& OutHashLo, TArray<uint64>& OutHashHi,
    TArray<FString>& OutPngFiles, TArray<int32>& OutPngRanks) const
{
    OutFiles.Empty();
    OutSizes.Empty();
    OutRanks.Empty();
    OutHashLo.Empty();
    OutHashHi.Empty();
    OutPngFiles.Empty();
    OutPngRanks.Empty();

    OutFiles.Reserve(InFiles.Num());
    OutSizes.Reserve(InFiles.Num());
    OutRanks.Reserve(InFiles.Num());
    OutHashLo.Reserve(InFiles.Num());
    OutHashHi.Reserve(InFiles.Num());

    for (int32 i = 0; i < InFiles.Num(); ++i)
    {
        const FString& Fname = InFiles[i];
        int64 Fsize = InSizes.IsValidIndex(i) ? InSizes[i] : 0;
        int32 Frank = InRanks.IsValidIndex(i) ? InRanks[i] : 0;
        uint64 FhLo = InHashLo.IsValidIndex(i) ? InHashLo[i] : 0;
        uint64 FhHi = InHashHi.IsValidIndex(i) ? InHashHi[i] : 0;

        if (Fname.EndsWith(TEXT(".png")) || Fname.EndsWith(TEXT(".PNG")))
        {
            OutPngFiles.Add(Fname);
            OutPngRanks.Add(Frank);
            continue;
        }

        if (bFilterUSDOnly)
        {
            FString Ext = FPaths::GetExtension(Fname).ToLower();
            if (Ext != TEXT("usd") && Ext != TEXT("usda") && Ext != TEXT("usdc") && Ext != TEXT("usdz"))
                continue;
        }

        if (MinimumFileSizeBytes > 0 && Fsize < MinimumFileSizeBytes)
            continue;

        if (bClipsOnly && !Fname.StartsWith(TEXT("clips/")))
            continue;

        OutFiles.Add(Fname);
        OutSizes.Add(Fsize);
        OutRanks.Add(Frank);
        OutHashLo.Add(FhLo);
        OutHashHi.Add(FhHi);
    }
}

void AJUSYNCFileSpawnerActor::OnFileListReceived_Internal(const TArray<FString>& FileList, const TArray<int64>& FileSizes, const TArray<int32>& FileRanks, bool bSuccess, const TArray<uint64>& HashLo, const TArray<uint64>& HashHi)
{
    if (bIsCancelled) return;

    if (!bSuccess || FileList.Num() == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("JUSYNC Spawner: failed to retrieve file list from broker"));
        GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Red, TEXT("[Spawner] Failed to get file list from broker!"));
        CurrentState = EJUSYNCSpawnerState::Error;
        OnError.Broadcast(TEXT("Failed to retrieve file list"));
        return;
    }

    RawFileList = FileList;
    RawFileSizes = FileSizes;
    RawFileRanks = FileRanks;
    RawHashLo = HashLo;
    RawHashHi = HashHi;

    UE_LOG(LogTemp, Display, TEXT("JUSYNC Spawner: received %d files"), FileList.Num());
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("[Spawner] Got %d files, filtering..."), FileList.Num()));

    TArray<FString> PngFiles;
    TArray<int32> PngRanks;
    TArray<FString> LocalFiles;
    TArray<int64> LocalSizes;
    TArray<int32> LocalRanks;
    TArray<uint64> LocalHashLo;
    TArray<uint64> LocalHashHi;
    FilterFileList(FileList, FileSizes, FileRanks, HashLo, HashHi,
        LocalFiles, LocalSizes, LocalRanks, LocalHashLo, LocalHashHi, PngFiles, PngRanks);

    for (int32 i = 0; i < PngFiles.Num(); ++i)
    {
        GradientPngRankMap.Add(PngFiles[i], PngRanks[i]);
    }

    RawFileList = MoveTemp(LocalFiles);
    RawFileSizes = MoveTemp(LocalSizes);
    RawFileRanks = MoveTemp(LocalRanks);
    RawHashLo = MoveTemp(LocalHashLo);
    RawHashHi = MoveTemp(LocalHashHi);
    UE_LOG(LogTemp, Display, TEXT("JUSYNC Spawner: single-pass filter: %d geometry files, %d PNGs"), RawFileList.Num(), PngFiles.Num());

    FilteredFiles = RawFileList;
    FilteredSizes = RawFileSizes;
    FilteredRanks = RawFileRanks;
    FilteredHashLo = RawHashLo;
    FilteredHashHi = RawHashHi;
    FilesTotal = FilteredFiles.Num();

    if (FilteredFiles.Num() == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("JUSYNC Spawner: no files after filtering"));
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("[Spawner] No files after filtering! Check filters."));
        bInitialSpawnDone = true;
        CurrentState = EJUSYNCSpawnerState::Complete;
        OnAllComplete.Broadcast(0, false);
        if (bEnableLiveUpdates)
        {
            StartLiveUpdatePolling();
        }
        return;
    }

    if (UJUSYNCSubsystem* GrdSubsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem())
    {
        MaybeTriggerGradientLoad(FileList, FileRanks, GrdSubsystem);
        LoadMeshTextureAsync(GrdSubsystem);
    }

    CurrentState = EJUSYNCSpawnerState::Downloading;
    UE_LOG(LogTemp, Display, TEXT("JUSYNC Spawner: downloading %d files"), FilesTotal);
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, FString::Printf(TEXT("[Spawner] Downloading %d geometry clips..."), FilesTotal));
    ProcessAndDownloadFiles();
}

void AJUSYNCFileSpawnerActor::OnFileListError(const FString& ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("JUSYNC Spawner: file list error: %s"), *ErrorMessage);
    CurrentState = EJUSYNCSpawnerState::Error;
    OnError.Broadcast(ErrorMessage);
}

void AJUSYNCFileSpawnerActor::ProcessAndDownloadFiles()
{
    if (bIsCancelled || FilteredFiles.Num() == 0) return;

    PendingDownloads = FilteredFiles.Num();
    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    if (!Subsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("JUSYNC Spawner: subsystem not available"));
        return;
    }

    PipelineNextIndex = 0;
    PipelineActive = 0;

    int32 InitialBatch = FMath::Min(PipelineDepth, FilteredFiles.Num());
    for (int32 i = 0; i < InitialBatch; ++i)
    {
        PipelineDownloadNext(Subsystem);
    }
}

void AJUSYNCFileSpawnerActor::PipelineDownloadNext(UJUSYNCSubsystem* Subsystem)
{
    if (!Subsystem || bIsCancelled) return;
    if (PipelineNextIndex >= FilteredFiles.Num()) return;

    const int32 i = PipelineNextIndex++;
    const FString Filename = FilteredFiles[i];
    const int32 TargetRank = FilteredRanks.IsValidIndex(i) ? FilteredRanks[i] : 0;
    const int64 FileSize = FilteredSizes.IsValidIndex(i) ? FilteredSizes[i] : int64(1048576);
    const uint64 HashLo = FilteredHashLo.IsValidIndex(i) ? FilteredHashLo[i] : 0;
    const uint64 HashHi = FilteredHashHi.IsValidIndex(i) ? FilteredHashHi[i] : 0;

    PipelineActive++;
    LoadFileThroughPipeline(Filename, TargetRank, FileSize, HashLo, HashHi, i, true);
}

void AJUSYNCFileSpawnerActor::LoadFileThroughPipeline(const FString& Filename, int32 Rank, int64 Size, uint64 HashLo, uint64 HashHi, int32 FileIndex, bool bIsInitial)
{
    if (bIsCancelled || Filename.IsEmpty())
    {
        PipelineActive = FMath::Max(0, PipelineActive - 1);
        return;
    }

    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    if (!Subsystem)
    {
        PipelineActive = FMath::Max(0, PipelineActive - 1);
        if (bIsInitial && !bInitialSpawnDone)
        {
            FilesDownloaded++;
            OnFileProgress.Broadcast(FilesDownloaded, FilesTotal);
            if (FileIndex >= 0 && !FailedFileIndices.Contains(FileIndex))
                FailedFileIndices.Add(FileIndex);
            CheckAllDownloadsComplete();
        }
        else if (ChangeTracker)
        {
            ChangeTracker->MarkFailed(Filename);
            ChainRefreshNext();
        }
        return;
    }

    TArray<FColor> ParseLUT;
    uint64 ParseLUTVersion = 0;
    if (bGradientReady.load() && !bMeshTextureReady && Subsystem->GetPointCloudSpawner())
    {
        ParseLUT = Subsystem->GetPointCloudSpawner()->GetGradientLUT();
        ParseLUTVersion = Subsystem->GetPointCloudSpawner()->GetLUTVersion();
    }

    const uint64 ExpectedLUTVersion = ParseLUT.Num() > 1 ? ParseLUTVersion : 0;

    if (bEnableMeshCache && MeshCache && (HashLo != 0 || HashHi != 0 || Size > 0))
    {
        TArray<FJUSYNCMeshData> CachedMeshes;
        TArray<FJUSYNCPointCloudData> CachedPointClouds;
        if (MeshCache->Get(Filename, HashLo, HashHi, Size, ExpectedLUTVersion, CachedMeshes, CachedPointClouds))
        {
            if (!bCachePointClouds)
            {
                CachedPointClouds.Empty();
            }

            UE_LOG(LogTemp, Display, TEXT("[Spawner] Cache hit for '%s' (%d meshes, %d PCs)"), *Filename, CachedMeshes.Num(), CachedPointClouds.Num());

            if (bIsInitial && !bInitialSpawnDone)
            {
                FilesDownloaded++;
                OnFileProgress.Broadcast(FilesDownloaded, FilesTotal);
                if (FileIndex >= 0)
                {
                    NextSpawnIndex = FileIndex;
                }
            }

            PipelineActive = FMath::Max(0, PipelineActive - 1);

            if (ChangeTracker && !bIsInitial)
            {
                ChangeTracker->SetState(Filename, EJUSYNCFileChangeState::Parsing);
            }

            PendingParseTasks++;
            TWeakObjectPtr<AJUSYNCFileSpawnerActor> WeakThis = this;
            const uint64 Generation = (ChangeTracker && !bIsInitial) ? ChangeTracker->GetGeneration(Filename) : 0;

            FFunctionGraphTask::CreateAndDispatchWhenReady(
                [WeakThis, Filename, FileIndex, Rank, Generation, HashLo, HashHi, Size, bIsInitial, ExpectedLUTVersion,
                  CachedMeshes = MoveTemp(CachedMeshes), CachedPointClouds = MoveTemp(CachedPointClouds), ParseLUT = MoveTemp(ParseLUT)]() mutable
                {
                    FJUSYNCParsedFileResult Result;
                    Result.Filename = Filename;
                    Result.FileIndex = FileIndex;
                    Result.Rank = Rank;
                    Result.Generation = Generation;
                    Result.HashLo = HashLo;
                    Result.HashHi = HashHi;
                    Result.Size = Size;
                    Result.bParsed = true;
                    Result.LUTVersion = ExpectedLUTVersion;
                    Result.Meshes = MoveTemp(CachedMeshes);
                    Result.PointClouds = MoveTemp(CachedPointClouds);

                    if (ParseLUT.Num() > 1)
                    {
                        FJUSYNCUSDLoader::BakeLUTIntoMeshes(Result.Meshes, ParseLUT);
                    }

                    if (Result.bParsed && !bIsInitial)
                    {
                        Result.PrebuiltStreams.SetNum(Result.Meshes.Num());
                        for (int32 StreamIdx = 0; StreamIdx < Result.Meshes.Num(); ++StreamIdx)
                        {
                            if (!Result.Meshes[StreamIdx].IsValid())
                            {
                                continue;
                            }

                            TUniquePtr<RealtimeMesh::FRealtimeMeshStreamSet> Streams = MakeUnique<RealtimeMesh::FRealtimeMeshStreamSet>();
                            if (JUSYNCBuildRealtimeMeshStreams(Result.Meshes[StreamIdx], *Streams))
                            {
                                Result.PrebuiltStreams[StreamIdx] = MoveTemp(Streams);
                            }
                        }
                    }

                    FFunctionGraphTask::CreateAndDispatchWhenReady(
                        [WeakThis, Result = MoveTemp(Result)]() mutable
                        {
                            if (WeakThis.IsValid())
                            {
                                WeakThis->ApplyParsedFileData(MoveTemp(Result));
                            }
                        },
                        TStatId(), nullptr, ENamedThreads::GameThread);
                },
                TStatId(), nullptr, ENamedThreads::AnyBackgroundThreadNormalTask);

            if (bIsInitial)
            {
                PipelineDownloadNext(Subsystem);
            }
            else
            {
                ChainRefreshNext();
            }
            return;
        }
    }

    TWeakObjectPtr<UJUSYNCSubsystem> WeakSubsystem = Subsystem;
    TWeakObjectPtr<AJUSYNCFileSpawnerActor> WeakThis = this;
    TUniquePtr<TArray<uint8>> FileData = MakeUnique<TArray<uint8>>();
    const int32 DynamicTimeout = CalculateDynamicTimeout(Size);
    const uint64 ExpectedSize = static_cast<uint64>(FMath::Max<int64>(0, Size));

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakSubsystem, WeakThis, Filename, Rank, Size, HashLo, HashHi, FileIndex, bIsInitial, DynamicTimeout, ExpectedSize, FileData = MoveTemp(FileData)]() mutable
    {
        if (!WeakSubsystem.IsValid() || !WeakThis.IsValid())
        {
            return;
        }

        bool bSuccess = WeakSubsystem->RequestFileSized(Filename, Rank, DynamicTimeout, *FileData, ExpectedSize);

        FFunctionGraphTask::CreateAndDispatchWhenReady(
            [WeakThis, Filename, Rank, Size, HashLo, HashHi, FileIndex, bIsInitial, FileData = MoveTemp(FileData), bSuccess]() mutable
            {
                if (!WeakThis.IsValid()) return;
                AJUSYNCFileSpawnerActor* Self = WeakThis.Get();
                Self->PipelineActive = FMath::Max(0, Self->PipelineActive - 1);

                if (!bSuccess || !FileData || FileData->Num() == 0)
                {
                    UE_LOG(LogTemp, Display, TEXT("JUSYNC Spawner: [DOWNLOAD FAILED] '%s' (rank %d)"), *Filename, Rank);
                    if (bIsInitial && !Self->bInitialSpawnDone)
                    {
                        Self->FilesDownloaded++;
                        Self->OnFileProgress.Broadcast(Self->FilesDownloaded, Self->FilesTotal);
                        if (FileIndex >= 0 && !Self->FailedFileIndices.Contains(FileIndex))
                            Self->FailedFileIndices.Add(FileIndex);
                        Self->CheckAllDownloadsComplete();
                    }
                    else
                    {
                        if (Self->ChangeTracker)
                        {
                            Self->ChangeTracker->MarkFailed(Filename);
                        }
                        Self->ChainRefreshNext();
                    }
                    return;
                }

                if (bIsInitial && !Self->bInitialSpawnDone)
                {
                    Self->FilesDownloaded++;
                    Self->OnFileProgress.Broadcast(Self->FilesDownloaded, Self->FilesTotal);
                    if (FileIndex >= 0)
                    {
                        Self->NextSpawnIndex = FileIndex;
                    }
                }

                if (Self->ChangeTracker && !bIsInitial)
                {
                    Self->ChangeTracker->SetState(Filename, EJUSYNCFileChangeState::Parsing);
                }

                Self->PendingParseTasks++;

                TArray<FColor> ParseLUT;
                uint64 ParseLUTVersion = 0;
                if (Self->bGradientReady.load() && !Self->bMeshTextureReady)
                {
                    if (UJUSYNCSubsystem* GrdSub = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem())
                    {
                        if (FJUSYNCPointCloudSpawner* PCSpawner = GrdSub->GetPointCloudSpawner())
                        {
                            ParseLUT = PCSpawner->GetGradientLUT();
                            ParseLUTVersion = PCSpawner->GetLUTVersion();
                        }
                    }
                }

                const uint64 Generation = (Self->ChangeTracker && !bIsInitial) ? Self->ChangeTracker->GetGeneration(Filename) : 0;
                TArray<uint8> Buffer = MoveTemp(*FileData);

                FJUSYNCUSDLoader::ParseAsync(
                    UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem(),
                    MoveTemp(Buffer),
                    Filename,
                    MoveTemp(ParseLUT),
                    ParseLUTVersion,
                    !bIsInitial,
                    FileIndex,
                    Rank,
                    Generation,
                    HashLo,
                    HashHi,
                    Size,
                    [WeakThis, bIsInitial](FJUSYNCParsedFileResult&& ParsedResult) mutable
                    {
                        if (WeakThis.IsValid())
                        {
                            WeakThis->ApplyParsedFileData(MoveTemp(ParsedResult));
                        }
                    });

                if (bIsInitial)
                {
                    if (UJUSYNCSubsystem* S = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem())
                    {
                        Self->PipelineDownloadNext(S);
                    }
                }
                else
                {
                    Self->ChainRefreshNext();
                }
            },
            TStatId(), nullptr, ENamedThreads::GameThread);
    });
}

void AJUSYNCFileSpawnerActor::ApplyParsedFileData(FJUSYNCParsedFileResult&& Result)
{
    const double ApplyStart = FPlatformTime::Seconds();

    if (bIsCancelled)
    {
        PendingParseTasks = FMath::Max(0, PendingParseTasks - 1);
        return;
    }

    const FString Filename = Result.Filename;
    const bool bIsInitial = !bInitialSpawnDone;

    if (!bIsInitial && ChangeTracker)
    {
        const FJUSYNCFileChangeRequest* Request = ChangeTracker->Find(Filename);
        if (Request &&
            (Result.HashLo != 0 || Result.HashHi != 0) &&
            (Request->HashLo != Result.HashLo || Request->HashHi != Result.HashHi))
        {
            UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] Discarding stale parsed result for '%s' (hash superseded)"), *Filename);
            PendingParseTasks = FMath::Max(0, PendingParseTasks - 1);
            ChangeTracker->MarkCompleted(Filename);
            ChainRefreshNext();
            return;
        }
    }

    if (ChangeTracker && !bIsInitial)
    {
        ChangeTracker->SetState(Filename, EJUSYNCFileChangeState::Spawning);
    }

    if (bEnableMeshCache && MeshCache && Result.bParsed &&
        (Result.Meshes.Num() > 0 || Result.PointClouds.Num() > 0) &&
        !(Result.PointClouds.Num() > 0 && !bCachePointClouds))
    {
        MeshCache->Store(Filename, Result.HashLo, Result.HashHi, Result.Size, Result.LUTVersion, Result.Meshes, Result.PointClouds);
    }

    TArray<AActor*> OldMeshActors;
    if (TArray<AActor*>* pOldMeshActors = FilenameToActors.Find(Filename))
    {
        OldMeshActors.Append(*pOldMeshActors);
    }

    TArray<AActor*> OldPCActors;
    if (TArray<AActor*>* pOldPCActors = FilenameToPCActors.Find(Filename))
    {
        OldPCActors.Append(*pOldPCActors);
    }

    const bool bIsRefresh = bInitialSpawnDone || OldMeshActors.Num() > 0 || OldPCActors.Num() > 0;

    if (Result.bParsed && Result.Meshes.Num() > 0)
    {
        int32 ValidMeshCount = 0;
        for (const FJUSYNCMeshData& M : Result.Meshes)
        {
            if (M.IsValid()) ValidMeshCount++;
        }

        if (ValidMeshCount > 0)
        {
            TArray<AActor*> ReplacementActors;
            int32 SpawnCount = 0;
            TArray<FJUSYNCMeshData> RemainingMeshes;
            TArray<TUniquePtr<RealtimeMesh::FRealtimeMeshStreamSet>> RemainingStreams;

            for (int32 i = 0; i < Result.Meshes.Num(); ++i)
            {
                if (!Result.Meshes[i].IsValid())
                {
                    continue;
                }
                if (SpawnCount >= FMath::Max(1, MaxSpawnsPerFrame))
                {
                    RemainingMeshes.Add(MoveTemp(Result.Meshes[i]));
                    if (i < Result.PrebuiltStreams.Num())
                    {
                        RemainingStreams.Add(MoveTemp(Result.PrebuiltStreams[i]));
                    }
                    continue;
                }

                RealtimeMesh::FRealtimeMeshStreamSet* Stream = nullptr;
                if (i < Result.PrebuiltStreams.Num() && Result.PrebuiltStreams[i])
                {
                    Stream = Result.PrebuiltStreams[i].Get();
                }
                AActor* Actor = SpawnOrUpdateMesh(Result.Meshes[i], Filename, bIsRefresh, ReplacementActors, Stream);
                if (Actor)
                {
                    SpawnCount++;
                }
            }

            if (RemainingMeshes.Num() > 0)
            {
                FDeferredSpawnEntry Entry;
                Entry.Meshes = MoveTemp(RemainingMeshes);
                Entry.PrebuiltStreams = MoveTemp(RemainingStreams);
                Entry.Filename = Filename;
                Entry.OldActors = MoveTemp(OldMeshActors);
                Entry.ReplacementActors = MoveTemp(ReplacementActors);
                Entry.ExpectedNewMeshes = ValidMeshCount;
                Entry.SpawnedNewMeshes = SpawnCount;
                Entry.Generation = (ChangeTracker && !bIsInitial) ? ChangeTracker->GetGeneration(Filename) : 0;
                DeferredSpawns.Add(MoveTemp(Entry));

                FTimerHandle DummyHandle;
                GetWorldTimerManager().SetTimer(DummyHandle, FTimerDelegate::CreateUObject(this, &AJUSYNCFileSpawnerActor::ProcessDeferredSpawns), 0.02f, false);
            }
            else if (bIsRefresh && OldMeshActors.Num() > 0)
            {
                DestroyUnreplacedMeshActors(Filename, OldMeshActors, ReplacementActors);
            }

            UE_LOG(LogTemp, Verbose, TEXT("[Spawner] %s: spawned/updated %d/%d meshes (%d deferred)"),
                *Filename, SpawnCount, Result.Meshes.Num(), RemainingMeshes.Num());
        }
    }

    TSet<FString> NewPCKeys;
    if (bSpawnPointClouds && Result.PointClouds.Num() > 0)
    {
        int32 ValidPCCount = 0;
        for (const FJUSYNCPointCloudData& PC : Result.PointClouds)
        {
            if (PC.IsValid()) ValidPCCount++;
        }

        if (ValidPCCount > 0)
        {
            if (UJUSYNCSubsystem* S = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem())
            {
                    if (FJUSYNCPointCloudSpawner* Spawner = S->GetPointCloudSpawner())
                    {
                        ApplyPointCloudSettingsToSpawner(Spawner);
                        Spawner->SetSpawnLocation(GetNextSpawnLocation());
                        Spawner->SetSpawnScale(bUseUniformScaling ? SpawnScale.X : 1.0f);

                    for (FJUSYNCPointCloudData& PC : Result.PointClouds)
                    {
                        if (!PC.IsValid())
                        {
                            continue;
                        }
                        const FString PCKey = FString::Printf(TEXT("%s_r%d"), *PC.ElementName, Result.Rank);
                        NewPCKeys.Add(PCKey);
                        PCElementToFilename.Add(PCKey, Filename);
                        PendingAsyncPCS++;
                        Spawner->EnqueuePointCloud(MoveTemp(PC), Result.Rank);
                    }

                    UE_LOG(LogTemp, Log, TEXT("[Spawner] Dispatched %d PCs from '%s'"), ValidPCCount, *Filename);
                }
            }
        }
    }

    DestroyUnreplacedPCActors(Filename, OldPCActors, NewPCKeys);

    if (!Result.bParsed || (Result.Meshes.Num() == 0 && Result.PointClouds.Num() == 0))
    {
        if (bIsInitial && !bInitialSpawnDone)
        {
            if (Result.FileIndex >= 0 && !ParseFailedIndices.Contains(Result.FileIndex))
            {
                ParseFailedIndices.Add(Result.FileIndex);
            }
            CheckAllDownloadsComplete();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[LiveUpdate] Parse returned no data for: '%s' — keeping existing actors"), *Filename);
        }
    }
    else if (!bIsInitial)
    {
        UE_LOG(LogTemp, Display, TEXT("[Refresh] Applied parsed data for '%s'"), *Filename);
    }

    PendingParseTasks = FMath::Max(0, PendingParseTasks - 1);

    if (bEnablePerfLogging)
    {
        UE_LOG(LogTemp, Display, TEXT("JUSYNC PERF ApplyParsedFile '%s' %d meshes %d PCs %.3f ms"),
               *Filename, Result.Meshes.Num(), Result.PointClouds.Num(), (FPlatformTime::Seconds() - ApplyStart) * 1000.0);
    }

    if (!bIsInitial)
    {
        if (ChangeTracker)
        {
            ChangeTracker->MarkCompleted(Filename);
        }
        ChainRefreshNext();
    }
    else
    {
        CheckAllDownloadsComplete();
    }
}

void AJUSYNCFileSpawnerActor::ApplyPointCloudSettingsToSpawner(FJUSYNCPointCloudSpawner* Spawner)
{
    if (!Spawner)
    {
        return;
    }

    Spawner->SetMaxPoolSize(PointCloudPoolSize);
    Spawner->SetPointSize(PointCloudSize);
    Spawner->SetPointSizeBias(PointSizeBias);
    Spawner->SetGapFillingStrength(GapFillingStrength);
    Spawner->SetPerfLogging(bEnablePerfLogging);
    Spawner->SetNormalCalculation(
        bCalculatePointCloudNormals,
        PointCloudNormalsMaxPoints,
        PointCloudNormalsQuality,
        PointCloudNormalsNoiseTolerance,
        PointCloudNormalsCooldownSeconds);

#ifdef WITH_ANARI_USD_MIDDLEWARE
    switch (PointShape)
    {
        case EJUSYNCPointShape::Square:
            Spawner->SetPointShape(ELidarPointCloudSpriteShape::Square);
            break;
        case EJUSYNCPointShape::Circle:
        default:
            Spawner->SetPointShape(ELidarPointCloudSpriteShape::Circle);
            break;
    }

    switch (PointOrientation)
    {
        case EJUSYNCPointOrientation::FacingNormal:
            Spawner->SetPointOrientation(ELidarPointCloudSpriteOrientation::PreferFacingNormal);
            break;
        case EJUSYNCPointOrientation::FacingCamera:
        default:
            Spawner->SetPointOrientation(ELidarPointCloudSpriteOrientation::PreferFacingCamera);
            break;
    }

    switch (PointScaling)
    {
        case EJUSYNCPointScaling::PerNode:
            Spawner->SetPointScaling(ELidarPointCloudScalingMethod::PerNode);
            break;
        case EJUSYNCPointScaling::PerPoint:
            Spawner->SetPointScaling(ELidarPointCloudScalingMethod::PerPoint);
            break;
        case EJUSYNCPointScaling::FixedScreenSize:
            Spawner->SetPointScaling(ELidarPointCloudScalingMethod::FixedScreenSize);
            break;
        case EJUSYNCPointScaling::PerNodeAdaptive:
        default:
            Spawner->SetPointScaling(ELidarPointCloudScalingMethod::PerNodeAdaptive);
            break;
    }
#endif
}

void AJUSYNCFileSpawnerActor::FlushHiddenMeshUpdates()
{
    if (HiddenPendingMeshUpdates.Num() == 0)
    {
        return;
    }

    TArray<AActor*> Ready;
    for (const TPair<AActor*, FRecolorMeshEntry>& Pair : HiddenPendingMeshUpdates)
    {
        if (!Pair.Key || !Pair.Key->IsValidLowLevel() || !Pair.Key->IsHidden())
        {
            Ready.Add(Pair.Key);
        }
    }

    if (Ready.Num() == 0)
    {
        return;
    }

    int32 Processed = 0;
    const int32 MaxUpdatesThisFrame = FMath::Max(1, MaxSpawnsPerFrame);
    for (AActor* Actor : Ready)
    {
        if (Processed >= MaxUpdatesThisFrame)
        {
            break;
        }

        FRecolorMeshEntry* PendingPtr = HiddenPendingMeshUpdates.Find(Actor);
        if (!PendingPtr)
        {
            continue;
        }
        FRecolorMeshEntry Pending = MoveTemp(*PendingPtr);
        HiddenPendingMeshUpdates.Remove(Actor);

        if (Actor && Actor->IsValidLowLevel())
        {
            TArray<AActor*> ReplacementActors;
            SpawnOrUpdateMesh(Pending.Mesh, Pending.Filename, true, ReplacementActors);
            Processed++;
        }
    }
}

AActor* AJUSYNCFileSpawnerActor::SpawnOrUpdateMesh(FJUSYNCMeshData& Mesh, const FString& Filename, bool bIsRefresh, TArray<AActor*>& ReplacementActors, RealtimeMesh::FRealtimeMeshStreamSet* PrebuiltStreams)
{
    if (!Mesh.IsValid())
    {
        return nullptr;
    }

    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    if (!Subsystem)
    {
        return nullptr;
    }

    const FString Key = Mesh.ElementName + TEXT("|") + Filename;
    AActor* Existing = FileToActorMap.FindRef(Key);

    if (bIsRefresh && Existing && Existing->IsValidLowLevel() && Existing->IsHidden())
    {
        FRecolorMeshEntry Pending;
        Pending.Mesh = Mesh;
        Pending.Filename = Filename;
        Pending.Loc = Existing->GetActorLocation();
        Pending.Rot = Existing->GetActorRotation();
        Pending.Scale = Existing->GetActorScale3D();
        HiddenPendingMeshUpdates.Add(Existing, Pending);
        RegisterMeshForLUTRecolor(Existing, Mesh, Filename, Existing->GetActorLocation());

        if (!ReplacementActors.Contains(Existing))
        {
            ReplacementActors.Add(Existing);
        }

        UE_LOG(LogTemp, Verbose, TEXT("[Spawner] Deferred hidden mesh update for '%s'"), *Filename);
        return Existing;
    }

    UMaterialInterface* SpawnMat = nullptr;
    if (!BakeLUTVertexColor(Mesh, SpawnMat))
    {
        SpawnMat = SpawnMaterial;
    }

    AActor* Actor = nullptr;

    if (bIsRefresh && Existing && Existing->IsValidLowLevel())
    {
        if (URealtimeMeshComponent* Comp = Existing->GetComponentByClass<URealtimeMeshComponent>())
        {
            if (Subsystem->UpdateRealtimeMeshFromJUSYNC(Mesh, Comp, SpawnMat, PrebuiltStreams))
            {
                Actor = Existing;
                TArray<AActor*>& FileActors = FilenameToActors.FindOrAdd(Filename);
                if (!FileActors.Contains(Actor))
                {
                    FileActors.Add(Actor);
                }
                if (SpawnScale != FVector::ZeroVector)
                {
                    Actor->SetActorScale3D(bUseUniformScaling ? FVector(SpawnScale.X) : FVector(SpawnScale));
                }
                if (bMeshTextureReady)
                {
                    ApplySenderTextureToActor(Actor);
                }
                RegisterMeshForLUTRecolor(Actor, Mesh, Filename, Actor->GetActorLocation());
            }
        }
    }

    if (!Actor)
    {
        const FVector SpawnLoc = GetNextSpawnLocation();
        Actor = UJUSYNCBlueprintLibrary::SpawnRealtimeMeshAtLocation(Mesh, SpawnLoc, FRotator::ZeroRotator, SpawnMat);
        if (!Actor)
        {
            return nullptr;
        }

        SpawnedActors.Add(Actor);
        ActorsSpawned++;
        FileToActorMap.Add(Key, Actor);
        FilenameToActors.FindOrAdd(Filename).Add(Actor);
        Actor->SetActorEnableCollision(false);

        if (SpawnScale != FVector::ZeroVector)
        {
            Actor->SetActorScale3D(bUseUniformScaling ? FVector(SpawnScale.X) : FVector(SpawnScale));
        }

        OnFileComplete.Broadcast(Filename, Actor);
        NextSpawnIndex++;

        if (bMeshTextureReady)
        {
            ApplySenderTextureToActor(Actor);
        }

        RegisterMeshForLUTRecolor(Actor, Mesh, Filename, SpawnLoc);

        if (bEnableTimeStepAnimation && AnimationController)
        {
            AnimationController->AddActor(Filename, Actor);
        }
    }

    if (!ReplacementActors.Contains(Actor))
    {
        ReplacementActors.Add(Actor);
    }

    return Actor;
}

void AJUSYNCFileSpawnerActor::DestroyUnreplacedMeshActors(const FString& Filename, const TArray<AActor*>& OldMeshActors, const TArray<AActor*>& ReplacementActors)
{
    TSet<AActor*> ReplacementSet;
    ReplacementSet.Reserve(ReplacementActors.Num());
    for (AActor* Actor : ReplacementActors)
    {
        if (Actor && Actor->IsValidLowLevel())
        {
            ReplacementSet.Add(Actor);
        }
    }

    TSet<AActor*> DestroySet;
    for (AActor* Actor : OldMeshActors)
    {
        if (Actor && Actor->IsValidLowLevel() && !ReplacementSet.Contains(Actor))
        {
            DestroySet.Add(Actor);
        }
    }

    for (AActor* Actor : DestroySet)
    {
        SpawnedActors.Remove(Actor);
        if (ActorsSpawned > 0)
        {
            ActorsSpawned--;
        }
        if (AnimationController) AnimationController->RemoveActor(Actor);
        Actor->Destroy();
    }

    for (TPair<FString, TArray<AActor*>>& Pair : FilenameToActors)
    {
        Pair.Value.RemoveAll([&DestroySet](AActor* A) { return DestroySet.Contains(A); });
    }

    for (auto It = FileToActorMap.CreateIterator(); It; ++It)
    {
        if (DestroySet.Contains(It.Value()))
        {
            It.RemoveCurrent();
        }
    }
}

void AJUSYNCFileSpawnerActor::DestroyUnreplacedPCActors(const FString& Filename, const TArray<AActor*>& OldPCActors, const TSet<FString>& NewPCKeys)
{
    if (OldPCActors.Num() == 0)
    {
        return;
    }

    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    FJUSYNCPointCloudSpawner* PCSpawner = Subsystem ? Subsystem->GetPointCloudSpawner() : nullptr;

    TSet<AActor*> DestroySet;
    for (AActor* Actor : OldPCActors)
    {
        if (!Actor || !Actor->IsValidLowLevel())
        {
            continue;
        }
        const FString Key = PCSpawner ? PCSpawner->GetElementKeyForActor(Actor) : FString();
        if (NewPCKeys.Contains(Key))
        {
            continue;
        }
        if (!Key.IsEmpty())
        {
            const FString KeyFile = PCElementToFilename.FindRef(Key);
            if (!KeyFile.IsEmpty() && KeyFile != Filename)
            {
                continue;
            }
        }
        DestroySet.Add(Actor);
    }

    for (AActor* Actor : DestroySet)
    {
        SpawnedActors.Remove(Actor);
        if (ActorsSpawned > 0)
        {
            ActorsSpawned--;
        }
        if (AnimationController) AnimationController->RemoveActor(Actor);
        if (PCSpawner)
        {
            PCSpawner->DestroyTrackedActor(Actor);
        }
        else
        {
            Actor->Destroy();
        }
    }

    if (TArray<AActor*>* Arr = FilenameToPCActors.Find(Filename))
    {
        Arr->RemoveAll([&DestroySet](AActor* A) { return DestroySet.Contains(A); });
    }

    for (auto It = FileToActorMap.CreateIterator(); It; ++It)
    {
        if (DestroySet.Contains(It.Value()))
        {
            It.RemoveCurrent();
        }
    }
}

void AJUSYNCFileSpawnerActor::DownloadGradientPng(UJUSYNCSubsystem* Subsystem)
{
    if (!Subsystem) return;

    FString PngPath = GradientPngFilename;
    int32 PngRank = 0;

    if (PngPath.IsEmpty())
    {
        if (GradientPngRankMap.Num() > 0)
        {
            for (auto It = GradientPngRankMap.CreateConstIterator(); It; ++It)
            {
                PngPath = It.Key();
                PngRank = It.Value();
                break;
            }
        }
    }
    else
    {
        if (GradientPngRankMap.Contains(PngPath))
        {
            PngRank = GradientPngRankMap[PngPath];
        }
        else if (FilteredRanks.Num() > 0)
        {
            PngRank = FilteredRanks[0];
        }
    }

    if (PngPath.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("JUSYNC Spawner: no gradient PNG found in broker"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("JUSYNC Spawner: downloading gradient PNG '%s' from rank %d"), *PngPath, PngRank);

    TWeakObjectPtr<UJUSYNCSubsystem> WeakSub = Subsystem;
    TWeakObjectPtr<AJUSYNCFileSpawnerActor> WeakThis = this;
    FString PngCopy = PngPath;

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakSub, WeakThis, PngCopy, PngRank]()
    {
        if (!WeakSub.IsValid() || !WeakThis.IsValid()) return;

        TArray<uint8> PngData;
        bool bOk = false;

        for (int32 Retry = 0; Retry < 3; ++Retry)
        {
            if (Retry > 0)
            {
                UE_LOG(LogTemp, Log, TEXT("JUSYNC Spawner: retrying gradient PNG download (attempt %d/3)"), Retry + 1);
                FPlatformProcess::Sleep(1.0f * Retry);
            }
            bOk = WeakSub->RequestFile(PngCopy, PngRank, 15000, PngData);
            if (bOk && PngData.Num() > 0) break;
            PngData.Empty();
        }

        TArray<FColor> LUT;
        if (bOk && PngData.Num() > 0)
        {
            IImageWrapperModule& ImgMod = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
            TSharedPtr<IImageWrapper> Img = ImgMod.CreateImageWrapper(EImageFormat::PNG);
            if (Img.IsValid() && Img->SetCompressed(PngData.GetData(), PngData.Num()))
            {
                TArray64<uint8> RawData;
                if (Img->GetRaw(RawData))
                {
                    int64 W = Img->GetWidth();
                    int64 H = Img->GetHeight();
                    if (H > 2)
                    {
                        UE_LOG(LogTemp, Log, TEXT("JUSYNC Spawner: '%s' is a full texture (height %lld), not a gradient — skipping LUT"), *PngCopy, (long long)H);
                    }
                    else
                    {
                        LUT.Reserve(FMath::Min(W, 256));
                        for (int64 x = 0; x < W && x < 256; ++x)
                        {
                            const uint8* Pixel = RawData.GetData() + x * 4;
                            LUT.Add(FColor(Pixel[2], Pixel[1], Pixel[0], 255));
                        }
                    }
                }
            }
        }

        if (LUT.Num() == 0)
        {
            UE_LOG(LogTemp, Log, TEXT("JUSYNC Spawner: PNG decode failed, trying middleware gradient cache"));
            UJUSYNCSubsystem* S = WeakSub.Get();
            if (S)
            {
                S->ApplyCachedGradientToSpawner();
                FJUSYNCPointCloudSpawner* Sp = S->GetPointCloudSpawner();
                if (Sp && Sp->GetGradientLUT().Num() > 0)
                {
                    LUT = Sp->GetGradientLUT();
                    UE_LOG(LogTemp, Log, TEXT("JUSYNC Spawner: fallback gradient cache: %d colors"), LUT.Num());
                }
            }
        }

        if (LUT.Num() == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("JUSYNC Spawner: failed to obtain gradient after PNG + cache fallback"));
        }

        if (LUT.Num() > 0)
        {
            FFunctionGraphTask::CreateAndDispatchWhenReady(
                [WeakThis, LUT = MoveTemp(LUT)]()
                {
                    if (!WeakThis.IsValid()) return;

                    UJUSYNCSubsystem* S = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
                    if (!S) return;

                    FJUSYNCPointCloudSpawner* Sp = S->GetPointCloudSpawner();
                    if (Sp)
                    {
                        Sp->SetGradientLUT(LUT);
                        WeakThis->bGradientReady = true;

                        UE_LOG(LogTemp, Log, TEXT("[Spawner] Gradient LUT loaded: %d colors"), LUT.Num());
                        Sp->RecolorGradientPendingActors();
                        WeakThis->RecolorGradientPendingMeshes();
                    }
                },
                TStatId(), nullptr, ENamedThreads::GameThread);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("JUSYNC Spawner: could not decode gradient PNG '%s'"), *PngCopy);
        }
    });
}

void AJUSYNCFileSpawnerActor::MaybeTriggerGradientLoad(const TArray<FString>& Files, const TArray<int32>& Ranks, UJUSYNCSubsystem* Subsystem)
{
    if (!Subsystem) return;
    if (bGradientReady.load() || bMeshTextureReady) return;
    if (bGradientDownloadStarted.load()) return;

    int32 Added = 0;
    for (int32 i = 0; i < Files.Num(); ++i)
    {
        const FString& F = Files[i];
        if (!(F.EndsWith(TEXT(".png")) || F.EndsWith(TEXT(".PNG")))) continue;
        if (GradientPngRankMap.Contains(F)) continue;
        GradientPngRankMap.Add(F, Ranks.IsValidIndex(i) ? Ranks[i] : 0);
        ++Added;
    }

    if (GradientPngRankMap.Num() > 0)
    {
        bGradientDownloadStarted.store(true);
        UE_LOG(LogTemp, Display, TEXT("[Spawner] Gradient PNG present (%d total, %d new) - loading LUT"), GradientPngRankMap.Num(), Added);
        DownloadGradientPng(Subsystem);
    }
}

void AJUSYNCFileSpawnerActor::LoadMeshTextureAsync(UJUSYNCSubsystem* Subsystem)
{
    if (!Subsystem) return;
    if (bMeshTextureLoading.load() || bMeshTextureReady) return;
    if (GradientPngRankMap.Num() == 0) return;

    bMeshTextureLoading.store(true);

    TArray<TPair<FString, int32>> Candidates;
    Candidates.Reserve(GradientPngRankMap.Num());
    for (const TPair<FString, int32>& Png : GradientPngRankMap)
    {
        Candidates.Add(Png);
    }

    TWeakObjectPtr<UJUSYNCSubsystem> WeakSub = Subsystem;
    TWeakObjectPtr<AJUSYNCFileSpawnerActor> WeakThis = this;

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakSub, WeakThis, Candidates]()
    {
        if (!WeakSub.IsValid() || !WeakThis.IsValid()) return;
        UJUSYNCSubsystem* S = WeakSub.Get();

        FString ChosenPng;
        FJUSYNCTextureData TexData;

        TArray<uint8> PngData;
        for (const TPair<FString, int32>& Png : Candidates)
        {
            PngData.Reset();
            if (!S->RequestFile(Png.Key, Png.Value, 60000, PngData) || PngData.Num() == 0)
            {
                continue;
            }

            int32 W = 0, H = 0, C = 0;
            if (!S->GetPNGDimensions(PngData, W, H, C))
            {
                continue;
            }
            if (H <= 2)
            {
                continue;
            }

            TexData = S->CreateTextureFromBuffer(PngData);
            if (TexData.Data.Num() == 0)
            {
                continue;
            }

            ChosenPng = Png.Key;
            break;
        }

        FFunctionGraphTask::CreateAndDispatchWhenReady(
            [WeakSub, WeakThis, TexData = MoveTemp(TexData), ChosenPng]()
            {
                if (!WeakSub.IsValid() || !WeakThis.IsValid()) return;
                UJUSYNCSubsystem* S = WeakSub.Get();
                AJUSYNCFileSpawnerActor* T = WeakThis.Get();
                if (!S || !T) return;

                if (ChosenPng.IsEmpty() || TexData.Data.Num() == 0)
                {
                    T->bMeshTextureLoading.store(false);
                    UE_LOG(LogTemp, Log, TEXT("[Spawner] No full-texture PNG found (only gradients / none) — meshes keep spawn material"));
                    return;
                }

                UTexture2D* Tex = S->CreateUETextureFromJUSYNC(TexData);
                if (!Tex)
                {
                    T->bMeshTextureLoading.store(false);
                    return;
                }

                T->MeshTextureCache.Add(ChosenPng, Tex);
                T->ActiveMeshTexture = Tex;
                T->bMeshTextureReady = true;
                T->bMeshTextureLoading.store(false);

                UE_LOG(LogTemp, Log, TEXT("[Spawner] Mesh texture ready: %s (%dx%d)"), *ChosenPng, TexData.Width, TexData.Height);

                for (AActor* A : T->SpawnedActors)
                {
                    T->ApplySenderTextureToActor(A);
                }
            },
            TStatId(), nullptr, ENamedThreads::GameThread);
    });
}

void AJUSYNCFileSpawnerActor::ApplySenderTextureToActor(AActor* Actor)
{
    if (!Actor) return;
    URealtimeMeshComponent* Comp = Actor->GetComponentByClass<URealtimeMeshComponent>();
    if (!Comp) return;
    UTexture2D* Tex = ActiveMeshTexture.Get();
    if (!Tex) return;
    ApplySenderTextureToComponent(Comp, Tex);
}

UMaterialInstanceDynamic* AJUSYNCFileSpawnerActor::GetOrCreateSenderMID(URealtimeMeshComponent* Comp, UTexture2D* SenderTex)
{
    if (!Comp || !SenderTex)
    {
        return nullptr;
    }

    UMaterial* BaseMat = SpawnMaterial ? SpawnMaterial->GetMaterial() : nullptr;
    if (!BaseMat)
    {
        if (UMaterialInterface* Cur = Comp->GetMaterial(0))
        {
            BaseMat = Cur->GetMaterial();
        }
    }
    if (!BaseMat)
    {
        BaseMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    }
    if (!BaseMat)
    {
        return nullptr;
    }

    const FName ParamName = !TextureSampleParameterName.IsEmpty() ? *TextureSampleParameterName : TEXT("BaseColor");

    if (TWeakObjectPtr<UMaterialInstanceDynamic>* Cached = SenderMIDCache.Find(Comp))
    {
        if (UMaterialInstanceDynamic* MID = Cached->Get())
        {
            if (MID->GetBaseMaterial() == BaseMat)
            {
                MID->SetTextureParameterValue(ParamName, SenderTex);
                return MID;
            }
        }
    }

    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, Comp);
    if (MID)
    {
        MID->SetTextureParameterValue(ParamName, SenderTex);
        SenderMIDCache.Add(Comp, MID);
    }
    return MID;
}

void AJUSYNCFileSpawnerActor::ApplySenderTextureToComponent(URealtimeMeshComponent* Comp, UTexture2D* SenderTex)
{
    if (!Comp || !SenderTex) return;

    UMaterialInstanceDynamic* MID = GetOrCreateSenderMID(Comp, SenderTex);
    if (!MID) return;

    Comp->SetMaterial(0, MID);
    Comp->MarkRenderStateDirty();
}

bool AJUSYNCFileSpawnerActor::BakeLUTVertexColor(FJUSYNCMeshData& Mesh, UMaterialInterface*& OutVertexMaterial)
{
    OutVertexMaterial = nullptr;

    if (bMeshTextureReady || !bGradientReady.load())
    {
        return false;
    }
    if (Mesh.Vertices.Num() == 0 || Mesh.UVs.Num() != Mesh.Vertices.Num())
    {
        return false;
    }

    if (Mesh.VertexColors.Num() != Mesh.Vertices.Num())
    {
        UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
        if (!Subsystem || !Subsystem->GetPointCloudSpawner())
        {
            return false;
        }
        const TArray<FColor> LUT = Subsystem->GetPointCloudSpawner()->GetGradientLUT();
        if (LUT.Num() <= 1)
        {
            return false;
        }
        FJUSYNCUSDLoader::BakeLUTIntoMesh(Mesh, LUT);
    }

    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    OutVertexMaterial = Subsystem ? Subsystem->GetCachedMaterial(TEXT("/Game/Materials/M_VertexColor")) : nullptr;
    if (OutVertexMaterial)
    {
        UE_LOG(LogTemp, Display, TEXT("[Spawner] Mesh LUT color: %d verts (attribute0 -> UV.x) + M_VertexColor"), Mesh.Vertices.Num());
    }
    return OutVertexMaterial != nullptr;
}

void AJUSYNCFileSpawnerActor::RegisterMeshForLUTRecolor(AActor* Spawned, const FJUSYNCMeshData& Mesh, const FString& Filename, const FVector& Loc)
{
    if (!Spawned || !Spawned->IsValidLowLevel())
    {
        return;
    }
    if (bMeshTextureReady || bMeshTextureLoading.load() || bGradientReady.load())
    {
        return;
    }
    if (Mesh.Vertices.Num() == 0 || Mesh.UVs.Num() != Mesh.Vertices.Num())
    {
        return;
    }

    FRecolorMeshEntry Entry;
    Entry.Mesh = Mesh;
    Entry.Filename = Filename;
    Entry.Loc = Loc;
    if (SpawnScale != FVector::ZeroVector)
    {
        Entry.Scale = bUseUniformScaling ? FVector(SpawnScale.X) : FVector(SpawnScale);
    }
    GradientPendingMeshes.Add(Spawned, MoveTemp(Entry));
}

void AJUSYNCFileSpawnerActor::RecolorGradientPendingMeshes()
{
    if (GradientPendingMeshes.Num() == 0) return;
    if (!bGradientReady.load()) return;

    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    if (!Subsystem || !Subsystem->GetPointCloudSpawner()) return;
    const TArray<FColor> LUT = Subsystem->GetPointCloudSpawner()->GetGradientLUT();
    if (LUT.Num() <= 1) return;

    int32 Recolored = 0;
    TArray<AActor*> Done;
    for (auto It = GradientPendingMeshes.CreateIterator(); It; ++It)
    {
        AActor* OldActor = It.Key();
        FRecolorMeshEntry& Entry = It.Value();

        if (!OldActor || !OldActor->IsValidLowLevel() || !Entry.Mesh.IsValid())
        {
            Done.Add(OldActor);
            continue;
        }

        UMaterialInterface* SpawnMat = nullptr;
        if (!BakeLUTVertexColor(Entry.Mesh, SpawnMat) || !SpawnMat)
        {
            Done.Add(OldActor);
            continue;
        }

        URealtimeMeshComponent* Comp = OldActor->GetComponentByClass<URealtimeMeshComponent>();
        if (Comp && Subsystem->UpdateRealtimeMeshFromJUSYNC(Entry.Mesh, Comp, SpawnMat))
        {
            if (bMeshTextureReady)
            {
                ApplySenderTextureToActor(OldActor);
            }
            Recolored++;
            Done.Add(OldActor);
            continue;
        }

        AActor* NewActor = UJUSYNCBlueprintLibrary::SpawnRealtimeMeshAtLocation(Entry.Mesh, Entry.Loc, Entry.Rot, SpawnMat);
        if (!NewActor)
        {
            Done.Add(OldActor);
            continue;
        }
        NewActor->SetActorEnableCollision(false);
        NewActor->SetActorScale3D(Entry.Scale);

        SpawnedActors.Remove(OldActor);
        SpawnedActors.Add(NewActor);
        const FString Key = Entry.Mesh.ElementName + TEXT("|") + Entry.Filename;
        if (FileToActorMap.Contains(Key))
        {
            FileToActorMap.Add(Key, NewActor);
        }
        if (TArray<AActor*>* Arr = FilenameToActors.Find(Entry.Filename))
        {
            Arr->Remove(OldActor);
            Arr->Add(NewActor);
        }

        OldActor->Destroy();
        Recolored++;
        Done.Add(OldActor);
    }

    for (AActor* A : Done)
    {
        GradientPendingMeshes.Remove(A);
    }

    if (Recolored > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("[Spawner] Recolored %d mesh actor(s) with LUT gradient"), Recolored);
    }
}

void AJUSYNCFileSpawnerActor::CheckAllDownloadsComplete()
{
    UE_LOG(LogTemp, Display, TEXT("[COMPLETION] ENTRY: FilesDownloaded=%d, FilesTotal=%d, PipelineActive=%d, PendingParseTasks=%d, PendingAsyncPCS=%d, Deferred=%d, bInitialSpawnDone=%d"),
        FilesDownloaded, FilesTotal, PipelineActive, PendingParseTasks, PendingAsyncPCS, DeferredSpawns.Num(), bInitialSpawnDone ? 1 : 0);

    if (FilesDownloaded >= FilesTotal)
    {
        if (PipelineActive > 0)
        {
            return;
        }

        int32 TotalFailed = FailedFileIndices.Num() + ParseFailedIndices.Num();
        if (TotalFailed > 0 && CurrentRetryCount < MaxRetries)
        {
            UE_LOG(LogTemp, Display, TEXT("JUSYNC Spawner: %d files failed (dl=%d, parse=%d), retrying (%d/%d)..."),
                TotalFailed, FailedFileIndices.Num(), ParseFailedIndices.Num(), CurrentRetryCount + 1, MaxRetries);
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
                FString::Printf(TEXT("[Spawner] Retrying %d failed files (attempt %d/%d)..."),
                TotalFailed, CurrentRetryCount + 1, MaxRetries));
            CurrentRetryCount++;
            FilesDownloaded = FilesTotal - TotalFailed;
            PipelineActive = 0;
            PipelineNextIndex = FilesTotal;
            RetryFailedDownloads();
            return;
        }

        if (UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem())
        {
            if (FJUSYNCPointCloudSpawner* PCSpawner = Subsystem->GetPointCloudSpawner())
            {
                PCSpawner->DrainReadyQueue();
            }
        }

        if (PendingAsyncSpawns > 0 || PendingAsyncPCS > 0 || PendingParseTasks > 0 || DeferredSpawns.Num() > 0)
        {
            TWeakObjectPtr<AJUSYNCFileSpawnerActor> WeakThis = this;
            FTimerHandle WaitSpawnHandle;
            FTimerDelegate WaitSpawnDelay;
            WaitSpawnDelay.BindLambda([WeakThis]()
            {
                TWeakObjectPtr<AJUSYNCFileSpawnerActor> Wt = WeakThis;
                FFunctionGraphTask::CreateAndDispatchWhenReady(
                    [Wt]() { if (Wt.IsValid()) Wt->CheckAllDownloadsComplete(); },
                    TStatId(), nullptr, ENamedThreads::GameThread);
            });
            if (GWorld)
                GWorld->GetTimerManager().SetTimer(WaitSpawnHandle, WaitSpawnDelay, 0.2f, false);
            return;
        }

        CurrentState = EJUSYNCSpawnerState::Complete;
        bool bSuccess = ActorsSpawned > 0;
        int32 DownloadFailed = FailedFileIndices.Num();
        int32 ParseFailed = ParseFailedIndices.Num();
        int32 FailedCount = FilesTotal - ActorsSpawned - DownloadFailed - ParseFailed;
        int32 ActualNotSpawned = FMath::Max(0, FailedCount);
        UE_LOG(LogTemp, Display, TEXT("JUSYNC Spawner: pipeline complete. Spawned %d actors from %d files (%d not spawned, %d download failed, %d parse failed)"),
            ActorsSpawned, FilesTotal, ActualNotSpawned, DownloadFailed, ParseFailed);
        FColor StatusColor = bSuccess ? FColor::Green : FColor::Yellow;
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, StatusColor,
            FString::Printf(TEXT("[Spawner] DONE: spawned %d actors from %d files (%d not spawned, %d failed)."), ActorsSpawned, FilesTotal, ActualNotSpawned, DownloadFailed + ParseFailed));

        for (int32 idx : FailedFileIndices)
        {
            if (FilteredFiles.IsValidIndex(idx))
            {
                UE_LOG(LogTemp, Warning, TEXT("JUSYNC Spawner: FAILED FILE: %s (rank %d)"),
                    *FilteredFiles[idx], FilteredRanks.IsValidIndex(idx) ? FilteredRanks[idx] : -1);
            }
        }

        for (int32 idx : ParseFailedIndices)
        {
            if (FilteredFiles.IsValidIndex(idx))
            {
                UE_LOG(LogTemp, Warning, TEXT("JUSYNC Spawner: PARSE FAILED: %s (rank %d)"),
                    *FilteredFiles[idx], FilteredRanks.IsValidIndex(idx) ? FilteredRanks[idx] : -1);
            }
        }

        FailedFileIndices.Empty();
        ParseFailedIndices.Empty();
        OnAllComplete.Broadcast(ActorsSpawned, bSuccess);

        bInitialSpawnDone = true;

        if (bEnableLiveUpdates)
        {
            StartLiveUpdatePolling();
        }
    }
}

void AJUSYNCFileSpawnerActor::RetryFailedDownloads()
{
    int32 TotalFailed = FailedFileIndices.Num() + ParseFailedIndices.Num();
    if (TotalFailed == 0)
    {
        CheckAllDownloadsComplete();
        return;
    }

    for (int32 idx : ParseFailedIndices)
    {
        if (!FailedFileIndices.Contains(idx))
            FailedFileIndices.Add(idx);
    }
    ParseFailedIndices.Empty();

    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    if (!Subsystem) return;

    int32 AvailableSpots = FMath::Max(0, PipelineDepth - PipelineActive);
    int32 Spawned = 0;

    for (int32 i = 0; i < FailedFileIndices.Num() && Spawned < AvailableSpots; ++i)
    {
        int32 idx = FailedFileIndices[i];
        if (bIsCancelled || !FilteredFiles.IsValidIndex(idx)) continue;

        const FString Filename = FilteredFiles[idx];
        const int32 TargetRank = FilteredRanks.IsValidIndex(idx) ? FilteredRanks[idx] : 0;
        const int64 FileSize = FilteredSizes.IsValidIndex(idx) ? FilteredSizes[idx] : int64(2097152);
        const uint64 HashLo = FilteredHashLo.IsValidIndex(idx) ? FilteredHashLo[idx] : 0;
        const uint64 HashHi = FilteredHashHi.IsValidIndex(idx) ? FilteredHashHi[idx] : 0;

        FailedFileIndices.RemoveAt(i);
        --i;
        PipelineActive++;
        Spawned++;
        LoadFileThroughPipeline(Filename, TargetRank, FileSize, HashLo, HashHi, idx, true);
    }
}

void AJUSYNCFileSpawnerActor::OnPointCloudSpawnedHandler(const FString& EleName, AActor* Spawned)
{
    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    FJUSYNCPointCloudSpawner* PCSpawner = Subsystem ? Subsystem->GetPointCloudSpawner() : nullptr;
    const FString FileName = PCElementToFilename.FindRef(EleName);

    auto DestroyPC = [this, PCSpawner](AActor* Actor)
    {
        if (!Actor)
        {
            return;
        }
        if (PCSpawner)
        {
            PCSpawner->DestroyTrackedActor(Actor);
        }
        else if (Actor->IsValidLowLevel())
        {
            Actor->Destroy();
        }
    };

    AActor* Existing = FileToActorMap.FindRef(EleName);

    if (Spawned && Existing && Existing == Spawned)
    {
        if (!FileName.IsEmpty())
        {
            TArray<AActor*>& Arr = FilenameToPCActors.FindOrAdd(FileName);
            if (!Arr.Contains(Spawned))
            {
                Arr.Add(Spawned);
            }
        }
        if (bEnableTimeStepAnimation && AnimationController && !FileName.IsEmpty())
        {
            AnimationController->AddActor(FileName, Spawned);
        }
        PendingAsyncPCS = FMath::Max(0, PendingAsyncPCS - 1);
        return;
    }

    if (Spawned)
    {
        if (!bInitialSpawnDone && Existing && Existing != Spawned)
        {
            SpawnedActors.Remove(Spawned);
            if (ActorsSpawned > 0) ActorsSpawned--;
            if (!FileName.IsEmpty())
            {
                if (TArray<AActor*>* pPCList = FilenameToPCActors.Find(FileName))
                {
                    pPCList->Remove(Spawned);
                }
            }
            DestroyPC(Spawned);
            PendingAsyncPCS = FMath::Max(0, PendingAsyncPCS - 1);
            return;
        }

        SpawnedActors.Add(Spawned);
        ActorsSpawned++;
        Spawned->SetActorEnableCollision(false);
#if WITH_EDITORONLY_DATA
        Spawned->SetActorLabel(EleName);
#endif

        if (!FileName.IsEmpty())
        {
            FilenameToPCActors.FindOrAdd(FileName).Add(Spawned);
        }

        if (!EleName.IsEmpty())
        {
            if (Existing && Existing != Spawned && Existing->IsValidLowLevel())
            {
                SpawnedActors.Remove(Existing);
                if (ActorsSpawned > 0) ActorsSpawned--;
                if (!FileName.IsEmpty())
                {
                    if (TArray<AActor*>* pPCList = FilenameToPCActors.Find(FileName))
                    {
                        pPCList->Remove(Existing);
                    }
                }
                DestroyPC(Existing);
            }
            FileToActorMap.Add(EleName, Spawned);
        }

        if (bEnableTimeStepAnimation && AnimationController && !FileName.IsEmpty())
        {
            AnimationController->AddActor(FileName, Spawned);
        }

        OnFileComplete.Broadcast(EleName, Spawned);
        NextSpawnIndex++;
    }

    PendingAsyncPCS = FMath::Max(0, PendingAsyncPCS - 1);

    if (!bInitialSpawnDone)
    {
        CheckAllDownloadsComplete();
    }
}

void AJUSYNCFileSpawnerActor::ProcessDeferredSpawns()
{
    if (DeferredSpawns.Num() == 0) return;

    int32 Processed = 0;
    TArray<FDeferredSpawnEntry> Remaining;
    Remaining.Reserve(DeferredSpawns.Num());

    for (auto& Entry : DeferredSpawns)
    {
        const uint64 CurrentGeneration = (ChangeTracker) ? ChangeTracker->GetGeneration(Entry.Filename) : 0;
        if (Entry.Generation != CurrentGeneration)
        {
            UE_LOG(LogTemp, Log, TEXT("[DeferredSpawn] Discarding stale deferred spawn for '%s' (generation %llu != %llu)"),
                *Entry.Filename, (unsigned long long)Entry.Generation, (unsigned long long)CurrentGeneration);
            continue;
        }

        int32 CanSpawn = FMath::Max(0, FMath::Max(1, MaxSpawnsPerFrame) - Processed);
        if (CanSpawn <= 0)
        {
            Remaining.Add(MoveTemp(Entry));
            continue;
        }

        int32 SpawnCount = 0;
        TArray<FJUSYNCMeshData> Leftovers;
        Leftovers.Reserve(Entry.Meshes.Num());
        TArray<TUniquePtr<RealtimeMesh::FRealtimeMeshStreamSet>> LeftoverStreams;
        LeftoverStreams.Reserve(Entry.Meshes.Num());

        for (int32 i = 0; i < Entry.Meshes.Num(); ++i)
        {
            if (!Entry.Meshes[i].IsValid()) continue;
            if (SpawnCount >= CanSpawn)
            {
                Leftovers.Add(MoveTemp(Entry.Meshes[i]));
                if (i < Entry.PrebuiltStreams.Num())
                {
                    LeftoverStreams.Add(MoveTemp(Entry.PrebuiltStreams[i]));
                }
                continue;
            }

            RealtimeMesh::FRealtimeMeshStreamSet* Stream = nullptr;
            if (i < Entry.PrebuiltStreams.Num() && Entry.PrebuiltStreams[i])
            {
                Stream = Entry.PrebuiltStreams[i].Get();
            }
            AActor* Actor = SpawnOrUpdateMesh(Entry.Meshes[i], Entry.Filename, true, Entry.ReplacementActors, Stream);
            if (Actor)
            {
                SpawnCount++;
            }
        }

        Entry.SpawnedNewMeshes += SpawnCount;
        Processed += SpawnCount;

        if (Leftovers.Num() == 0)
        {
            if (Entry.OldActors.Num() > 0 && Entry.SpawnedNewMeshes >= Entry.ExpectedNewMeshes)
            {
                DestroyUnreplacedMeshActors(Entry.Filename, Entry.OldActors, Entry.ReplacementActors);
            }
        }
        else
        {
            Entry.Meshes = MoveTemp(Leftovers);
            Entry.PrebuiltStreams = MoveTemp(LeftoverStreams);
            Remaining.Add(MoveTemp(Entry));
        }
    }

    DeferredSpawns = MoveTemp(Remaining);
    if (DeferredSpawns.Num() > 0)
    {
        FTimerHandle DummyHandle;
        GetWorldTimerManager().SetTimer(DummyHandle, FTimerDelegate::CreateUObject(this, &AJUSYNCFileSpawnerActor::ProcessDeferredSpawns), 0.02f, false);
    }
    else if (!bInitialSpawnDone)
    {
        CheckAllDownloadsComplete();
    }
}

void AJUSYNCFileSpawnerActor::OnBrokerNotification(const FJUSYNCNotification& Notification)
{
    const char* TypeStr = (Notification.Type == EJUSYNCNotificationType::FileUpdateV2) ? "FileUpdateV2" :
                           (Notification.Type == EJUSYNCNotificationType::CommitComplete) ? "CommitComplete" : "FileUpdate";
    UE_LOG(LogTemp, Verbose, TEXT("[V2HANDLER] Received %s for '%s' | state=%d enabled=%d initialDone=%d"),
           ANSI_TO_TCHAR(TypeStr), *Notification.Filename, (int32)CurrentState, bEnableLiveUpdates ? 1 : 0, bInitialSpawnDone ? 1 : 0);

    if (!bEnableLiveUpdates)
    {
        UE_LOG(LogTemp, Warning, TEXT("[SIGNAL] DROPPED — 'Enable Live Updates' is FALSE in spawner Details panel"));
        return;
    }
    if (CurrentState != EJUSYNCSpawnerState::Complete)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[LiveUpdate] BLOCKED: CurrentState=%d (need Complete=%d)"), (int32)CurrentState, (int32)EJUSYNCSpawnerState::Complete);
        return;
    }

    if (Notification.Type == EJUSYNCNotificationType::FileUpdate)
    {
        UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] File update notification: '%s' (rank %d, size=%lld)"),
               *Notification.Filename, Notification.SourceRank, (long long)Notification.FileSize);
        if (Notification.FileSize >= 2048)
        {
            HandleFileUpdateNotification(Notification.Filename, Notification.SourceRank, Notification.FileSize, 0, 0);
        }
    }
    else if (Notification.Type == EJUSYNCNotificationType::FileUpdateV2)
    {
        bool bHashChanged = (Notification.HashLo != Notification.HashPrevLo ||
                             Notification.HashHi != Notification.HashPrevHi);
        UE_LOG(LogTemp, Display, TEXT("[LiveUpdate] FileUpdateV2: '%s' (rank %d, hash_changed=%d)"),
               *Notification.Filename, Notification.SourceRank, bHashChanged ? 1 : 0);
        if (bHashChanged)
        {
            if (Notification.FileSize < 2048)
            {
                UE_LOG(LogTemp, Verbose, TEXT("[LiveUpdate] Skipping tiny file in V2 notification: '%s' (%lld bytes)"),
                       *Notification.Filename, (long long)Notification.FileSize);
            }
            else
            {
                HandleFileUpdateNotification(Notification.Filename, Notification.SourceRank, Notification.FileSize,
                    static_cast<uint64>(Notification.HashLo), static_cast<uint64>(Notification.HashHi));
            }
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] FileUpdateV2: hash unchanged, skipping '%s'"), *Notification.Filename);
        }
    }
    else if (Notification.Type == EJUSYNCNotificationType::CommitComplete)
    {
        UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] Commit complete notification"));
        HandleCommitCompleteNotification();
    }
}

void AJUSYNCFileSpawnerActor::HandleFileUpdateNotification(const FString& Filename, int32_t SourceRank, int64 FileSize, uint64 HashLo, uint64 HashHi)
{
    if (!bAutoRefreshMeshes || Filename.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] Skipping file update: auto-refresh disabled or empty filename"));
        return;
    }

    if (!bInitialSpawnDone)
    {
        UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] Blocking: initial spawn not done yet (file='%s')"), *Filename);
        return;
    }

    const uint64 OldHashLo = FileHashLo.FindRef(Filename);
    const uint64 OldHashHi = FileHashHi.FindRef(Filename);
    UpdateTrackedFileMetadata(Filename, SourceRank, FileSize, HashLo, HashHi);

    if (!JUSYNCIsLiveUpdateGeometryFile(Filename))
    {
        UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] Skipping non-clip USD file: '%s'"), *Filename);
        return;
    }

    if ((HashLo != 0 || HashHi != 0) &&
        OldHashLo == HashLo &&
        OldHashHi == HashHi)
    {
        UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] Dedup: hash already tracked for '%s'"), *Filename);
        return;
    }

    if (ChangeTracker && ChangeTracker->Contains(Filename))
    {
        const FJUSYNCFileChangeRequest* ReqBefore = ChangeTracker->Find(Filename);
        const bool bWasQueued = ReqBefore && ReqBefore->State == EJUSYNCFileChangeState::Queued;

        ChangeTracker->QueueChange(Filename, SourceRank, FileSize, HashLo, HashHi);

        if (bWasQueued)
        {
            UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] Coalesced queued update for '%s'"), *Filename);
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] Superseded in-flight update for '%s'"), *Filename);
        }
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] Refreshing file: '%s' from rank %d"), *Filename, SourceRank);

    SeenV2Files.Add(Filename, SourceRank);

    int32 TargetRank = SourceRank;
    if (TargetRank < 0)
    {
        for (int32 i = 0; i < FilteredFiles.Num(); ++i)
        {
            if (FilteredFiles[i] == Filename)
            {
                TargetRank = FilteredRanks.IsValidIndex(i) ? FilteredRanks[i] : 0;
                break;
            }
        }
        if (TargetRank < 0)
        {
            for (int32 i = 0; i < RawFileRanks.Num(); ++i)
            {
                if (RawFileList.IsValidIndex(i) && RawFileList[i] == Filename)
                {
                    TargetRank = RawFileRanks[i];
                    break;
                }
            }
        }
        if (TargetRank < 0)
        {
            int32* pRank = SeenV2Files.Find(Filename);
            if (pRank)
            {
                TargetRank = *pRank;
            }
        }
    }

    if (QueueRefreshFile(Filename, TargetRank, FileSize, HashLo, HashHi))
    {
        ChainRefreshNext();
    }
}

void AJUSYNCFileSpawnerActor::HandleCommitCompleteNotification(bool bIsTimer)
{
    if (!bAutoRefreshMeshes)
    {
        UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] Skipping commit complete: auto-refresh disabled"));
        return;
    }

    double Now = FPlatformTime::Seconds();
    if (bSceneDiffInFlight)
    {
        UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] Skipping commit complete: diff already in progress"));
        return;
    }

    if (ChangeTracker && ChangeTracker->HasActiveWork())
    {
        UE_LOG(LogTemp, Verbose, TEXT("[LiveUpdate] Skipping commit complete: file changes active"));
        return;
    }

    if (!bIsTimer && Now - LastCommitCompleteTime < CommitCompleteCooldownSeconds)
    {
        double Remaining = CommitCompleteCooldownSeconds - (Now - LastCommitCompleteTime);
        UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] Skipping commit complete: cooldown active (%.1fs remaining)"), Remaining);
        return;
    }

    if (!bIsTimer) LastCommitCompleteTime = Now;
    bSceneDiffInFlight = true;
    UE_LOG(LogTemp, Display, TEXT("[LiveUpdate] Commit complete - re-fetching file list"));

    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    if (!Subsystem || !Subsystem->IsBrokerConnected())
    {
        UE_LOG(LogTemp, Warning, TEXT("[LiveUpdate] Cannot refresh: broker not connected"));
        bSceneDiffInFlight = false;
        return;
    }

    TWeakObjectPtr<AJUSYNCFileSpawnerActor> WeakThis = this;
    TWeakObjectPtr<UJUSYNCSubsystem> WeakSubsystem = Subsystem;

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakSubsystem, WeakThis]()
    {
        if (!WeakSubsystem.IsValid() || !WeakThis.IsValid()) return;

        TArray<FString> NewFiles;
        TArray<int64> NewSizes;
        TArray<int32> NewRanks;
        TArray<uint64> NewHashLo;
        TArray<uint64> NewHashHi;
        bool bSuccess = WeakSubsystem->RequestFileListWithSizesAndRanks(-1, 15000, NewFiles, NewSizes, NewRanks, &NewHashLo, &NewHashHi);

        FFunctionGraphTask::CreateAndDispatchWhenReady(
            [WeakThis, NewFiles = MoveTemp(NewFiles), NewSizes = MoveTemp(NewSizes), NewRanks = MoveTemp(NewRanks), NewHashLo = MoveTemp(NewHashLo), NewHashHi = MoveTemp(NewHashHi), bSuccess]()
            {
                if (!WeakThis.IsValid())
                {
                    return;
                }

                if (!bSuccess || NewFiles.Num() == 0)
                {
                    WeakThis->bSceneDiffInFlight = false;
                    return;
                }

                WeakThis->RawHashLo = NewHashLo;
                WeakThis->RawHashHi = NewHashHi;
                WeakThis->DiffAndRefreshFileList(NewFiles, NewSizes, NewRanks, false);
            },
            TStatId(), nullptr, ENamedThreads::GameThread);
    });
}

void AJUSYNCFileSpawnerActor::DiffAndRefreshFileList(const TArray<FString>& NewFiles, const TArray<int64>& NewSizes, const TArray<int32>& NewRanks, bool bIsManual)
{
    if (ChangeTracker && ChangeTracker->HasActiveWork())
    {
        UE_LOG(LogTemp, Verbose, TEXT("[LiveUpdate] Skipping diff: file changes already active"));
        bSceneDiffInFlight = false;
        return;
    }

    if (UJUSYNCSubsystem* GrdSubsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem())
    {
        MaybeTriggerGradientLoad(NewFiles, NewRanks, GrdSubsystem);
    }

    TArray<FString> OldFileList = FilteredFiles;

    TMap<FString, int64> OldSizes;
    TMap<FString, uint64> OldHashLo;
    TMap<FString, uint64> OldHashHi;
    for (int32 i = 0; i < FilteredFiles.Num(); ++i)
    {
        OldSizes.Add(FilteredFiles[i], FilteredSizes.IsValidIndex(i) ? FilteredSizes[i] : 0);
        if (FilteredHashLo.IsValidIndex(i) && FilteredHashHi.IsValidIndex(i))
        {
            OldHashLo.Add(FilteredFiles[i], FilteredHashLo[i]);
            OldHashHi.Add(FilteredFiles[i], FilteredHashHi[i]);
        }
    }

    TArray<FString> NewFilteredFiles;
    TArray<int64> NewFilteredSizes;
    TArray<int32> NewFilteredRanks;
    TArray<uint64> NewFilteredHashLo;
    TArray<uint64> NewFilteredHashHi;
    TArray<FString> NewPngFiles;
    TArray<int32> NewPngRanks;
    FilterFileList(NewFiles, NewSizes, NewRanks, RawHashLo, RawHashHi,
        NewFilteredFiles, NewFilteredSizes, NewFilteredRanks, NewFilteredHashLo, NewFilteredHashHi,
        NewPngFiles, NewPngRanks);

    int32 ChangedCount = 0;
    int32 NewCount = 0;

    for (int32 i = 0; i < NewFilteredFiles.Num(); ++i)
    {
        const FString& Fname = NewFilteredFiles[i];
        const int64 NewSize = NewFilteredSizes[i];
        const int32 Rank = NewFilteredRanks[i];
        const uint64 NewHLo = NewFilteredHashLo[i];
        const uint64 NewHHi = NewFilteredHashHi[i];

        if (NewSize < 2048)
        {
            UE_LOG(LogTemp, Verbose, TEXT("[LiveUpdate] Skipping tiny file '%s' (%lld bytes)"), *Fname, (long long)NewSize);
            continue;
        }

        int64 OldSize = 0;
        int64* pOldSize = OldSizes.Find(Fname);
        bool bFound = (pOldSize != nullptr);
        if (bFound) OldSize = *pOldSize;

        uint64 OldHLo = 0, OldHHi = 0;
        uint64* pHLo = OldHashLo.Find(Fname);
        uint64* pHHi = OldHashHi.Find(Fname);
        bool bHasOldHash = (pHLo != nullptr && pHHi != nullptr);
        if (bHasOldHash) { OldHLo = *pHLo; OldHHi = *pHHi; }

        bool bHashChanged = (NewHLo != OldHLo || NewHHi != OldHHi);
        bool bSizeChanged = (NewSize != OldSize);

        bool bShouldRefresh = bIsManual || !bFound || bSizeChanged;
        if (!bShouldRefresh && NewHLo && bHashChanged)
            bShouldRefresh = true;
        if (!bShouldRefresh && !bHasOldHash && NewHLo)
            bShouldRefresh = true;

        if (bShouldRefresh)
        {
            if (QueueRefreshFile(Fname, Rank, NewSize, NewHLo, NewHHi))
            {
                const char* Reason = bIsManual ? "manual" : (!bFound ? "new" : (bSizeChanged ? "size" : "hash"));
                UE_LOG(LogTemp, Verbose, TEXT("[LiveUpdate] File changed '%s': %s"), *Fname, ANSI_TO_TCHAR(Reason));
                if (!bFound) NewCount++; else ChangedCount++;
            }
        }
    }

    TSet<FString> NewFileSet;
    NewFileSet.Reserve(NewFiles.Num());
    for (const FString& Nf : NewFiles)
    {
        NewFileSet.Add(Nf);
    }

    int32 DeletedCount = 0;
    for (const FString& OldName : OldFileList)
    {
        if (NewFileSet.Contains(OldName))
        {
            continue;
        }
        const int32 Destroyed = DestroyFileActors(OldName);
        if (Destroyed > 0)
        {
            DeletedCount += Destroyed;
            UE_LOG(LogTemp, Warning, TEXT("[LiveUpdate] Destroyed %d orphan actor(s) for deleted file '%s'"), Destroyed, *OldName);
        }
    }
    if (DeletedCount > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("[LiveUpdate] Cleaned up %d orphan actors from deleted files"), DeletedCount);
    }

    const int32 SkippedCount = FMath::Max(0, NewFiles.Num() - NewFilteredFiles.Num());
    FString Tag = bIsManual ? TEXT("[ManualRefresh]") : TEXT("[LiveUpdate]");
    FString Summary = FString::Printf(TEXT("%s %d changed, %d new, %d deleted, %d skipped"), *Tag, ChangedCount, NewCount, DeletedCount, SkippedCount);
    UE_LOG(LogTemp, Display, TEXT("%s"), *Summary);
    GEngine->AddOnScreenDebugMessage(-1, 4.0f, bIsManual ? FColor::Green : FColor::Orange, Summary);

    RawFileList = NewFiles;
    RawFileSizes = NewSizes;
    RawFileRanks = NewRanks;
    FilteredFiles = MoveTemp(NewFilteredFiles);
    FilteredSizes = MoveTemp(NewFilteredSizes);
    FilteredRanks = MoveTemp(NewFilteredRanks);
    FilteredHashLo = MoveTemp(NewFilteredHashLo);
    FilteredHashHi = MoveTemp(NewFilteredHashHi);
    FilesTotal = FilteredFiles.Num();

    for (int32 i = 0; i < FilteredFiles.Num(); ++i)
    {
        FileLastSize.Add(FilteredFiles[i], FilteredSizes.IsValidIndex(i) ? FilteredSizes[i] : 0);
        FileHashLo.Add(FilteredFiles[i], FilteredHashLo.IsValidIndex(i) ? FilteredHashLo[i] : 0);
        FileHashHi.Add(FilteredFiles[i], FilteredHashHi.IsValidIndex(i) ? FilteredHashHi[i] : 0);
    }

    bSceneDiffInFlight = false;

    if (ChangeTracker && ChangeTracker->QueuedCount() > 0)
    {
        ChainRefreshNext();
    }
}

void AJUSYNCFileSpawnerActor::ManualRefresh()
{
    UE_LOG(LogTemp, Display, TEXT("[ManualRefresh] Triggered manual refresh"));
    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("[Spawner] Manual refresh started..."));

    if (CurrentState != EJUSYNCSpawnerState::Complete)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ManualRefresh] Not in Complete state (%d), cannot refresh"), (int32)CurrentState);
        return;
    }

    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    if (!Subsystem || !Subsystem->IsBrokerConnected())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ManualRefresh] Broker not connected"));
        return;
    }

    bSceneDiffInFlight = true;

    TWeakObjectPtr<AJUSYNCFileSpawnerActor> WeakThis = this;
    TWeakObjectPtr<UJUSYNCSubsystem> WeakSubsystem = Subsystem;

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakSubsystem, WeakThis]()
    {
        if (!WeakSubsystem.IsValid() || !WeakThis.IsValid()) return;

        TArray<FString> NewFiles;
        TArray<int64> NewSizes;
        TArray<int32> NewRanks;
        TArray<uint64> NewHashLo;
        TArray<uint64> NewHashHi;
        bool bSuccess = WeakSubsystem->RequestFileListWithSizesAndRanks(-1, 15000, NewFiles, NewSizes, NewRanks, &NewHashLo, &NewHashHi);

        FFunctionGraphTask::CreateAndDispatchWhenReady(
            [WeakThis, NewFiles = MoveTemp(NewFiles), NewSizes = MoveTemp(NewSizes), NewRanks = MoveTemp(NewRanks), NewHashLo = MoveTemp(NewHashLo), NewHashHi = MoveTemp(NewHashHi), bSuccess]()
            {
                if (!WeakThis.IsValid())
                {
                    return;
                }

                if (!bSuccess || NewFiles.Num() == 0)
                {
                    WeakThis->bSceneDiffInFlight = false;
                    return;
                }

                WeakThis->RawHashLo = NewHashLo;
                WeakThis->RawHashHi = NewHashHi;
                WeakThis->DiffAndRefreshFileList(NewFiles, NewSizes, NewRanks, true);
            },
            TStatId(), nullptr, ENamedThreads::GameThread);
    });
}

void AJUSYNCFileSpawnerActor::StartLiveUpdatePolling()
{
    if (!bEnableLiveUpdates || LiveUpdatePollInterval <= 0.0f)
        return;

    if (GWorld)
    {
        GWorld->GetTimerManager().ClearTimer(LiveUpdateTimerHandle);
    }
    UE_LOG(LogTemp, Log, TEXT("[LiveUpdate] Tick-driven backstop polling active (interval: %.1fs)"), LiveUpdatePollInterval);
}

void AJUSYNCFileSpawnerActor::StopLiveUpdatePolling()
{
    if (GWorld)
    {
        GWorld->GetTimerManager().ClearTimer(LiveUpdateTimerHandle);
    }
}

void AJUSYNCFileSpawnerActor::OnLiveUpdateTimer()
{
    UE_LOG(LogTemp, Display, TEXT("[LiveUpdate] TICK complete=%d autoRefresh=%d activeWork=%d diffBusy=%d initialDone=%d tracked=%d raw=%d"),
        CurrentState == EJUSYNCSpawnerState::Complete ? 1 : 0,
        bAutoRefreshMeshes ? 1 : 0,
        (ChangeTracker && ChangeTracker->HasActiveWork()) ? 1 : 0,
        bSceneDiffInFlight ? 1 : 0,
        bInitialSpawnDone ? 1 : 0,
        FilteredFiles.Num(),
        RawFileList.Num());

    if (CurrentState != EJUSYNCSpawnerState::Complete)
        return;

    if (bAutoRefreshMeshes)
    {
        HandleCommitCompleteNotification(true);
        return;
    }

    if (!bInitialSpawnDone) return;
    if (bSceneDiffInFlight) return;

    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    if (!Subsystem || !Subsystem->IsBrokerConnected()) return;

    bSceneDiffInFlight = true;

    TWeakObjectPtr<AJUSYNCFileSpawnerActor> WeakThis = this;
    TWeakObjectPtr<UJUSYNCSubsystem> WeakSubsystem = Subsystem;

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakSubsystem, WeakThis]()
    {
        if (!WeakSubsystem.IsValid() || !WeakThis.IsValid()) return;

        TArray<FString> NewFiles;
        TArray<int64> NewSizes;
        TArray<int32> NewRanks;
        TArray<uint64> NewHashLo;
        TArray<uint64> NewHashHi;
        bool bSuccess = WeakSubsystem->RequestFileListWithSizesAndRanks(-1, 10000, NewFiles, NewSizes, NewRanks, &NewHashLo, &NewHashHi);

        FFunctionGraphTask::CreateAndDispatchWhenReady(
            [WeakThis, NewFiles = MoveTemp(NewFiles), NewSizes = MoveTemp(NewSizes), NewRanks = MoveTemp(NewRanks), NewHashLo = MoveTemp(NewHashLo), NewHashHi = MoveTemp(NewHashHi), bSuccess]()
            {
                if (!WeakThis.IsValid() || !bSuccess || NewFiles.Num() == 0)
                {
                    if (WeakThis.IsValid()) WeakThis->bSceneDiffInFlight = false;
                    return;
                }

                WeakThis->RawHashLo = NewHashLo;
                WeakThis->RawHashHi = NewHashHi;
                WeakThis->DiffAndRefreshFileList(NewFiles, NewSizes, NewRanks, false);
            },
            TStatId(), nullptr, ENamedThreads::GameThread);
    });
}

bool AJUSYNCFileSpawnerActor::QueueRefreshFile(const FString& Filename, int32 Rank, int64 Size, uint64 HashLo, uint64 HashHi)
{
    if (!bAutoRefreshMeshes || Filename.IsEmpty())
    {
        return false;
    }
    if (!ChangeTracker)
    {
        return false;
    }
    if (ChangeTracker->Contains(Filename))
    {
        return false;
    }

    ChangeTracker->QueueChange(Filename, Rank, Size, HashLo, HashHi, -1, false);
    return true;
}

void AJUSYNCFileSpawnerActor::UpdateTrackedFileMetadata(const FString& Filename, int32 Rank, int64 Size, uint64 HashLo, uint64 HashHi)
{
    if (Filename.IsEmpty())
    {
        return;
    }

    if (Rank >= 0)
    {
        SeenV2Files.Add(Filename, Rank);
    }
    if (Size > 0)
    {
        FileLastSize.Add(Filename, Size);
    }
    if (HashLo != 0 || HashHi != 0)
    {
        FileHashLo.Add(Filename, HashLo);
        FileHashHi.Add(Filename, HashHi);
    }

    for (int32 i = 0; i < FilteredFiles.Num(); ++i)
    {
        if (FilteredFiles[i] != Filename)
        {
            continue;
        }
        if (Rank >= 0) FilteredRanks[i] = Rank;
        if (Size > 0) FilteredSizes[i] = Size;
        if (HashLo != 0 || HashHi != 0)
        {
            if (HashLo != 0) FilteredHashLo[i] = HashLo;
            if (HashHi != 0) FilteredHashHi[i] = HashHi;
        }
        break;
    }

    for (int32 i = 0; i < RawFileList.Num(); ++i)
    {
        if (RawFileList[i] != Filename)
        {
            continue;
        }
        if (Rank >= 0) RawFileRanks[i] = Rank;
        if (Size > 0) RawFileSizes[i] = Size;
        if (HashLo != 0 || HashHi != 0)
        {
            if (HashLo != 0) RawHashLo[i] = HashLo;
            if (HashHi != 0) RawHashHi[i] = HashHi;
        }
        break;
    }
}

int32 AJUSYNCFileSpawnerActor::DestroyFileActors(const FString& Filename)
{
    if (Filename.IsEmpty())
    {
        return 0;
    }

    TArray<AActor*> MeshActors;
    if (TArray<AActor*>* pMeshActors = FilenameToActors.Find(Filename))
    {
        MeshActors.Append(*pMeshActors);
    }

    TArray<AActor*> PCActors;
    if (TArray<AActor*>* pPCActors = FilenameToPCActors.Find(Filename))
    {
        PCActors.Append(*pPCActors);
    }

    if (MeshActors.Num() == 0 && PCActors.Num() == 0)
    {
        return 0;
    }

    TSet<AActor*> PCSet;
    for (AActor* Actor : PCActors)
    {
        if (Actor && Actor->IsValidLowLevel())
        {
            PCSet.Add(Actor);
        }
    }

    TSet<AActor*> Processed;
    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    FJUSYNCPointCloudSpawner* PCSpawner = Subsystem ? Subsystem->GetPointCloudSpawner() : nullptr;
    int32 Destroyed = 0;

    auto DestroyOne = [&](AActor* Actor)
    {
        if (!Actor || !Actor->IsValidLowLevel() || Processed.Contains(Actor))
        {
            return;
        }
        Processed.Add(Actor);
        SpawnedActors.Remove(Actor);
        if (ActorsSpawned > 0)
        {
            ActorsSpawned--;
        }
        if (AnimationController) AnimationController->RemoveActor(Actor);
        if (PCSet.Contains(Actor) && PCSpawner)
        {
            PCSpawner->DestroyTrackedActor(Actor);
        }
        else
        {
            Actor->Destroy();
        }
        Destroyed++;
    };

    for (AActor* Actor : MeshActors)
    {
        DestroyOne(Actor);
    }
    for (AActor* Actor : PCActors)
    {
        DestroyOne(Actor);
    }

    if (TArray<AActor*>* pMeshActors = FilenameToActors.Find(Filename))
    {
        pMeshActors->Empty();
    }
    if (TArray<AActor*>* pPCActors = FilenameToPCActors.Find(Filename))
    {
        pPCActors->Empty();
    }

    for (auto It = FileToActorMap.CreateIterator(); It; ++It)
    {
        if (Processed.Contains(It.Value()))
        {
            It.RemoveCurrent();
        }
    }

    for (auto It = PCElementToFilename.CreateIterator(); It; ++It)
    {
        if (It.Value() == Filename)
        {
            It.RemoveCurrent();
        }
    }

    return Destroyed;
}

void AJUSYNCFileSpawnerActor::ChainRefreshNext()
{
    if (bIsCancelled) return;
    if (!ChangeTracker) return;
    if (ChangeTracker->QueuedCount() == 0) return;

    UJUSYNCSubsystem* Subsystem = UJUSYNCBlueprintLibrary::GetJUSYNCSubsystem();
    if (!Subsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LiveUpdate] ChainRefreshNext: subsystem unavailable, leaving %d files queued"), ChangeTracker->QueuedCount());
        return;
    }

    int32 AvailableSpots = FMath::Max(0, PipelineDepth - PipelineActive);
    int32 Spawned = 0;

    while (AvailableSpots > 0 && ChangeTracker->QueuedCount() > 0)
    {
        TPair<FString, int32> Popped = ChangeTracker->PopNextQueued();
        if (Popped.Key.IsEmpty())
        {
            break;
        }

        const FJUSYNCFileChangeRequest* Request = ChangeTracker->Find(Popped.Key);
        PipelineActive++;
        AvailableSpots--;

        LoadFileThroughPipeline(
            Popped.Key,
            Popped.Value,
            Request ? Request->Size : 0,
            Request ? Request->HashLo : 0,
            Request ? Request->HashHi : 0,
            Request ? Request->FileIndex : -1,
            false);

        Spawned++;
    }

    if (Spawned == 0 && ChangeTracker->QueuedCount() > 0)
    {
        FTimerHandle RetryHandle;
        GetWorldTimerManager().SetTimer(RetryHandle, FTimerDelegate::CreateUObject(this, &AJUSYNCFileSpawnerActor::ChainRefreshNext), 0.02f, false);
    }
}

void AJUSYNCFileSpawnerActor::RetryRemainingFiles()
{
    if (FailedFileIndices.Num() == 0) return;
    if (CurrentRetryCount >= MaxRetries) return;

    RetryFailedDownloads();
}

void AJUSYNCFileSpawnerActor::PlayTimeStepAnimation()
{
    if (AnimationController)
    {
        AnimationController->Play();
    }
}

void AJUSYNCFileSpawnerActor::StopTimeStepAnimation()
{
    if (AnimationController)
    {
        AnimationController->Stop();
    }
}

void AJUSYNCFileSpawnerActor::SetTimeStepIndex(int32 NewIndex)
{
    if (AnimationController)
    {
        AnimationController->SetCurrentTimeStepIndex(NewIndex);
    }
}
