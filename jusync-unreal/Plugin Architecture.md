# Plugin Architecture

## JUSYNC Plugin Overview

The JUSYNC plugin is a **Runtime module** that provides real-time USD data synchronization between an ANARI (OpenANARI) rendering pipeline and Unreal Engine. It uses ZeroMQ for broker-dealer messaging, parses USD files via native C++ middleware, and spawns meshes using RealtimeMeshComponent.

## Module Registration

**Module Name**: `JUSYNC`
**Loading Phase**: `Default`
**Type**: `Runtime`
**Platforms**: Win64, Linux
**C++ Standard**: C++20
**PCH**: Explicit/shared (uses UE shared PCH)

```cpp
// RegisterModule() in JUSYNCModule.cpp
- Loads module
- Auto-registers UJUSYNCSubsystem as a UGameInstanceSubsystem
- Preloads common materials (M_VertexColor, BasicShapeMaterial, DefaultMaterial)
```

## Core Class Graph

```
FJusyncSampleProjectModule (game)
│
└── UJUSYNCModule (IPlugin + IModuleInterface)
    │
    └── UJUSYNCSubsystem (UGameInstanceSubsystem)
    │   ├── ZMQ socket to broker (DEALER)
    │   ├── Middleware C-ABI calls (ProcessMeshDataFromUSD_C)
    │   ├── OnFileReceived / OnMessageReceived / OnNotification events
    │   └── FJUSYNCPointCloudSpawner (member, tickable)
    │
    ├── UJUSYNCBlueprintLibrary (BlueprintFunctionLibrary)
    │   ├── InitializeJUSYNCMiddleware()
    │   ├── LoadUSDFromBuffer() / LoadUSDFullFromBuffer()
    │   ├── SpawnRealtimeMeshAtLocation() / BatchSpawn()
    │   ├── ConnectToANARIUSDBroker() / RequestFile*()
    │   └── *Async variants (10 methods)
    │
    ├── AJUSYNCFileSpawnerActor (AActor)
    │   ├── Spawns meshes from broker USD files
    │   ├── PipelineDepth-controlled async downloads
    │   ├── Live update with commit-diff notifications
    │   └── FJUSYNCPointCloudSpawner (via subsystem)
    │
    ├── AJUSYNCPointCloudTestActor (AActor)
    │   └── Test actor for point cloud debug
    │
    └── UJUSYNCAsyncAction* (UAsyncAction)
        └── Async action wrappers for long ops
```

## Class Details

### UJUSYNCSubsystem (UGameInstanceSubsystem)

**Purpose**: Central hub — middleware lifecycle, ZMQ broker, file dispatch.

**Key Members**:
- `g_SubsystemInstance` — static atomic for middleware C-ABI callbacks
- `MiddlewareMutex` — FCriticalSection guarding middleware calls
- `OnFileReceived` — dynamic multicast delegate (FJUSYNCFileData)
- `OnMessageReceived` — dynamic multicast delegate (FString)
- `OnNotificationProcessed` — dynamic multicast delegate (FJUSYNCNotification)
- `PCSpawner` — FJUSYNCPointCloudSpawner (tickable)
- `MaterialCache` — TMap<FString, TSoftObjectPtr<UMaterialInterface>>

**Lifecycle**:
1. `InitializeMiddleware()` → calls `CreateMiddlewareInstance_C()` (C-ABI)
2. Registers ZMQ file/message/notification callbacks
3. `ConnectToBroker()` → calls `ConnectToBroker_C()` (C-ABI)
4. Callbacks fire → marshal to game thread → broadcast delegates
5. `ShutdownMiddleware()` → tears down ZMQ + middleware instance

**Thread Safety**:
- C-ABI callbacks fire on middleware threads → marshal to game thread via `AsyncTask(ENamedThreads::GameThread)`
- `MiddlewareMutex` protects all middleware calls
- `g_SubsystemInstance` (atomic) provides callback routing
- **Optimized**: C callback now uses 2 allocations instead of 5 (direct `FString` capture + single `TArray<uint8>` copy)

### UJUSYNCBlueprintLibrary (BlueprintFunctionLibrary)

**Purpose**: Blueprint-exposed functions for all USD/mesh operations.

**Function Categories**:

| Category | Functions |
|---|---|
| Connection | `InitializeJUSYNCMiddleware`, `ShutdownJUSYNCMiddleware`, `IsJUSYNCConnected` |
| Broker | `ConnectToANARIUSDBroker`, `RequestFileList*`, `RequestFile`, `RequestFrame` |
| Worker | `RequestWorkerStatus`, `RequestWorkerCount`, `RequestTotalWorkerCount` |
| USD Parse | `LoadUSDFromBuffer`, `LoadUSDFullFromBuffer`, `ValidateUSDFormat` |
| Mesh Spawn | `SpawnRealtimeMeshAtLocation`, `BatchSpawnMeshesAtLocations` |
| Texture | `CreateTextureFromBuffer`, `CreateUETextureFromJUSYNC`, `GetPNGDimensions` |
| Point Cloud | `LoadUSDPointCloudFromBuffer`, `SpawnPointCloudAtLocation`, `BatchSpawnPointClouds` |
| Async | `*Async` variants of all above (10 methods, delegate callbacks) |
| Benchmark | `RunBenchmark*, GetBenchmarkResults, ClearBenchmarkResults` |

### AJUSYNCFileSpawnerActor (AActor)

**Purpose**: Fully automated pipeline: connect broker → fetch file list → download → parse USD → spawn meshes (with live updates).

**State Machine**: `EJUSYNCSpawnerState`

```
Idle → Connecting → FetchingList → Downloading → Spawning → Complete
                                            ↓
                                          Error (at any step)
```

**Pipeline**:
1. `StartSpawning()` → `ConnectToBroker()` → `RequestFileList()` (async)
2. `OnFileListReceived_Internal()` → **single-pass** filter (USD extension + size + clip path + PNG bucket) in 1 loop
3. Spawn meshes in `PipelineDepth` batches via `PipelineDownloadNext()`
4. Download each file async → `OnSingleFileDownloaded()` → parse USD → `SpawnMeshFromData()` (direct call, no AsyncTask NOOP)
5. Point clouds deferred to `FJUSYNCPointCloudSpawner`
6. Gradient PNG download (auto-detects first `.png` from broker)

**Live Update System**:
- `bEnableLiveUpdates` + `bAutoRefreshMeshes` triggers commit-diff polling
- Broker notifications → `OnBrokerNotification()` → `HandleFileUpdateNotification()` / `HandleCommitCompleteNotification()`
- `DiffAndRefreshFileList()` shared helper compares old/new file sizes, calls `RefreshSingleFile()` for changed files
- `SpawnGridColumns` (1-100, default 10) drives grid layout

**Key UProperties**:

| Property | Type | Purpose |
|---|---|---|
| `BrokerEndpoint` | FString | "tcp://host:port" |
| `RequestTimeoutMs` | int32 | Min timeout (dynamic based on size) |
| `BandwidthBytesPerSecond` | float | For dynamic timeout calc |
| `MinimumFileSizeBytes` | int32 | Skip small files (manifests) |
| `bFilterUSDOnly` | bool | .usd/.usda/.usdc/.usdz only |
| `bClipsOnly` | bool | `clips/` prefix filter |
| `SpawnTargetActor` | AActor* | Origin spawn point |
| `BaseSpawnLocation` | FVector | Fallback origin |
| `SpawnSpacing` | float | Grid cell size |
| `SpawnGridColumns` | int32 | Grid columns (1-100) |
| `SpawnMaterial` | UMaterialInterface* | Per-mesh material (dynamic instance) |
| `PipelineDepth` | int32 | Download-ahead depth (1-16) |
| `bSpawnPointClouds` | bool | Toggle PC spawning |
| `PointCloudSize` | float | PC point size |
| `bUseGradientColors` | bool | Map attribute0 → LUT color |
| `bEnableLiveUpdates` | bool | Commit-diff + broker notifications |
| `LiveUpdatePollInterval` | float | Poll seconds (0 = notification-only) |

### FJUSYNCPointCloudSpawner (FTickableGameObject)

**Purpose**: Async point cloud conversion + actor pooling.

**Design**:
- `EnqueuePointCloud(PCData, Rank)` → background thread → marshal to game thread
- Game thread: `Tick()` → `DrainReadyQueue()` (budget-limited, default 16ms)
- Actor pool (`MaxPoolSize`): reuse actors to avoid GC thrashing
- Gradient LUT: `SetGradientLUT()` from decoded PNG → `RecolorGradientPendingActors()`
- Uses `TArray64<FLidarPointCloudPoint>` for LiDAR plugin

### AJUSYNCPointCloudTestActor

**Purpose**: Debug actor for point cloud visualization testing.

## Data Flow Diagram

```
Broker (ZMQ, DEALER/ROUTER)
    │
    │  Request: REQ_FILE_LIST, REQ_FILE
    │  Response: RESP_FILE_DATA, RESP_FILE_LIST, NOTIFY_COMMIT_COMPLETE
    │
    ▼
UJUSYNCSubsystem (ZmqConnector)
    │
    │  C-ABI callback on middleware thread
    │
    ▼ ProcessMeshDataFromUSD_C()
    ▼
Native Middleware (anari_usd_middleware)
    ├── UsdProcessor (geometry parsing, glm→flat)
    ├── MemoryMonitor (memory limits)
    ├── HashVerifier (XXH3-128)
    └── ZmqConnector (DEALER-only)
    │
    ▼ CMeshData (C struct)
    ▼
ConvertCMeshDataToUE_Helper()
    ├── axis flip: (X, Z, -Y) USD → UE
    ├── normal normalization (batch)
    ├── color interpolation (vertex/face)
    └── returns FJUSYNCMeshData
    │
    ▼
UJUSYNCBlueprintLibrary / AJUSYNCFileSpawnerActor
    ├── BatchSpawnMeshesAtLocations()
    │   └── CreateRealtimeMeshFromJUSYNC()
    │       └── RMC: TRealtimeMeshBuilderLocal
    │
    └── FJUSYNCPointCloudSpawner
        └── Background conversion → ALidarPointCloudActor
```

## Memory Management

| Allocation | Strategy |
|---|---|
| Mesh vertex arrays | `SetNum()` + direct index writes (no `Add()`) |
| Normal arrays | Batch normalize (one sqrt pass) |
| UV arrays | `SetNum()` + direct index writes |
| Color arrays | `SetNum()` + direct index writes |
| Texture data | `TArray<uint8>` → `UTexture2D::CreateTransient()` |
| Material cache | `TSoftObjectPtr` in `TMap` with FScopeLock |
| Point clouds | Actor pool + `TArray64` for large datasets |
| Gradient LUT | FCriticalSection-guarded `TArray<FColor>` |

## Key Design Decisions

1. **Middleware as native library** — C-ABI only, no C++ template exposure. Plugin calls C functions, middleware owns all parsing logic.
2. **No ROUTER support** — `ZmqConnector` is DEALER-only. `receiveFile`/`receiveAnyMessage` are logging stubs.
3. **Game thread marshaling** — All callbacks fire on middleware threads, marshal to game thread for actor operations.
4. **Actor pooling** — Point cloud actors reused from pool to avoid GC thrashing. Max pool size configurable.
5. **Pipeline depth** — Download-ahead pattern: while spawning N meshes, download N+PipelineDepth. Prevents idle CPU.
6. **Live update via diff** — Compare file sizes between broker snapshots. Only refresh changed files.
7. **Hash algorithm** — XXH3-128 (stateless, no mutex required).
8. **Memory limit sync** — `setMemoryLimit(X)` updates both `memoryLimitMB` (TinyUSDZ) and `memoryLimitBytes` (internal gate).
