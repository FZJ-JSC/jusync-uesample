# ThirdParty Dependencies

## Overview

The JUSYNC plugin bundles 5 third-party components under `Plugins/JUSYNC/Source/ThirdParty/`:

| Component | Type | Purpose | License |
|---|---|---|---|
| AnariUsdMiddleware | Native library (.so/.dll) | C-ABI middleware for USD parsing | Custom |
| ZeroMQ | Static library | Zero-copy messaging to ANARI broker | LGPL/MPL |
| glm | Header-only | OpenGL Mathematics (vector/matrix ops) | MIT |
| xxhash | Header + .c | Fast 128-bit non-cryptographic hash | BSD-2 |
| RealtimeMeshComponent | UE Plugin (submodule) | Dynamic mesh rendering system | MIT |

Additionally, the project depends on:
- **LidarPointCloud** — Epic's built-in UE5 plugin for point cloud rendering
- **NodeToCode** — Marketplace plugin (submodule, for BP→C++ export)

---

## AnariUsdMiddleware

**Location**: `Plugins/JUSYNC/Source/ThirdParty/AnariUsdMiddleware/`
**Headers**: `Include/` (17 files)
**Binaries**:
- Linux: `Lib/Linux/libanari_usd_middleware.so` (**131 MB**, built from `jusync/`)
- Win64: `Lib/Win64/anari_usd_middleware.exp` (`.dll` NOT shipped — must be built from `jusync/` repo)

### Build Verification
```
file libanari_usd_middleware.so → ELF 64-bit LSB shared object, x86-64, with debug_info
ldd libanari_usd_middleware.so → libpthread, librt, libm, libc (all resolved)
```

> **LLM Note**: `.so` is built via CMake from `jusync/` repo. Copy to UE project with:
> `cp jusync/build/libanari_usd_middleware.so jusync-uesample/Plugins/JUSYNC/Source/ThirdParty/AnariUsdMiddleware/Lib/Linux/`

### Header Files

| Header | Purpose |
|---|---|
| `AnariUsdMiddleware.h` | Main C++ facade (UsdProcessor, notification callback) |
| `AnariUsdMiddleware_C.h` | C-ABI declarations (middleware creation, USD processing) |
| `AnariUsdClient.h` | Client-facing types and enums |
| `AnariUsdMessages.h` | ZMQ message protocol definitions |
| `UsdProcessor.h` | USD geometry parsing pipeline |
| `ZmqConnector.h` | ZeroMQ DEALER connector (ROUTER stubs present but inactive) |
| `MemoryMonitor.h` | Host memory limits, allocation tracking |
| `HashVerifier.h` | XXH3-128 file integrity verification |
| `CollisionProcessor.h` | Collision data extraction from USD |
| `ParallelDownloader.h` | Parallel file download manager (currently guarded with `#error`) |
| `ParallelDownloadManager.h` | Download queue orchestration |
| `MiddlewareLogging.h` | Log levels, format, destination |
| `GpuContext.h` | GPU context abstraction |
| `GpuKernels.h` | CUDA/OpenCL kernel declarations |
| `GpuMemory.h` | GPU memory allocation |
| `GpuValidation.h` | GPU result validation |
| `IconsFontAwesome6.h` | Font Awesome 6 icon unicode map (UI) |

### C-ABI Entry Points

Functions exposed via `AnariUsdMiddleware_C.h`:
- `CreateMiddlewareInstance_C()` → native handle
- `DestroyMiddlewareInstance_C()` → cleanup
- `ConnectToBroker_C()` / `DisconnectFromBroker_C()`
- `IsConnected_C()` / `IsBrokerConnected_C()`
- `ProcessMeshDataFromUSD_C()` → `CMeshData*` (geometry)
- `ProcessPointCloudFromUSD_C()` → `CPointCloudData*` (point clouds)
- `FreeMeshData_C()` / `FreePointCloudData_C()` → free C arrays
- `RequestWorkerCount_C()` / `RequestTotalWorkerCount_C()` / etc.
- `SetFileReceivedCallback_C()` → register C callback for file data
- `SetMessageCallback_C()` → register C callback for messages
- `SetNotificationCallback_C()` → register C callback for notifications

### Memory Model

- `CMeshData` contains `float*` arrays (points, normals, UVs, vertex_colors) with `*_count` fields
- Caller must free arrays via `FreeMeshData_C()`
- Plugin copies C arrays into UE `TArray` on game thread (single-copy via `SetNum` + index)

---

## ZeroMQ

**Location**: `Plugins/JUSYNC/Source/ThirdParty/ZeroMQ/`
**Structure**:
```
ZeroMQ/
├── include/  # zmq.h, zmq.hpp, zmq_utils.h
└── lib/      # libzmq static library (statically linked into middleware .so)
```

**Usage**: Embedded in the middleware library. The JUSYNC plugin does NOT directly link to ZeroMQ — all ZMQ operations go through AnariUsdMiddleware's C-ABI.

**Configuration**: DEALER-only. The middleware ZmqConnector has ROUTER support in headers but it's compiled-out (stub methods). Socket pattern:
- Plugin → `tcp://broker:port` (DEALER)
- Broker handles ROUTER/QUEUE logic

---

## GLM (OpenGL Mathematics)

**Location**: `Plugins/JUSYNC/Source/ThirdParty/glm/`
**Includes as**: `#include "glm/glm/xxx.hpp"`
**License**: MIT

Middleware uses GLM for:
- `glm::mat4` / `glm::mat3` — transform matrices from USD
- `glm::vec3` / `glm::vec4` — positions, normals, colors
- `glm::affineScale()` / `glm::affineInverse()` — mesh bounds calculations
- Internal `glm→flat` conversion (flattens GLM arrays to UE's `TArray<FVector>`)

**Note**: The header path points to `glm/glm/` — this is GLM's standard directory layout with the top-level `glm/` containing all the `*.hpp` files.

---

## xxHash

**Location**: `Plugins/JUSYNC/Source/ThirdParty/xxhash/`
**Files**: `xxhash.h` (header), `xxhash.c` (implementation)
**License**: BSD-2

**Usage**: XXH3-128 for file hash verification. Stateless hash — no mutex required. Used by `HashVerifier` middleware component.

```cpp
XXH3_128_hash_t hash = XXH3_128bits(data, size, seed);
```

**Integration**: Compiled into the middleware `.so`. The plugin receives hash values as `uint64_t[2]` (lo, hi parts of 128-bit hash).

---

## RealtimeMeshComponent (Git Submodule)

**URL**: `https://github.com/TriAxis-Games/RealtimeMeshComponent.git`
**Location**: `Plugins/RealtimeMeshComponent/`
**Type**: UE Plugin (Runtime + Editor)
**License**: MIT

**Purpose**: Runtime mesh generation system — creates renderable meshes from procedural data without baking to StaticMesh assets.

**Key Classes** (used by JUSYNC):
- `URealtimeMeshComponent` — Dynamic mesh component (replaces StaticMeshComponent)
- `URealtimeMeshSimple` — Simple mesh implementation
- `TRealtimeMeshBuilderLocal<uint32>` — Mesh builder with vertex/normal/UV/color/tangent streams
- `FRealtimeMeshSectionGroupKey` / `FRealtimeMeshSectionKey` — Section management

**How JUSYNC uses RMC**:
```cpp
URealtimeMeshSimple* Mesh = RMC->InitializeRealtimeMesh<URealtimeMeshSimple>();
auto Builder = RealtimeMesh::TRealtimeMeshBuilderLocal<uint32>(Streams);
Builder.EnableTangents();
Builder.EnableTexCoords();
Builder.EnableColors();
Builder.EnablePolyGroups();

for (vertices) {
    Builder.AddVertex(FVector3f(pos));
    Builder.SetNormal(i, normal);
    Builder.SetTexCoord(i, 0, uv);
    Builder.SetColor(i, color);
}
for (triangles) {
    Builder.AddTriangle(i0, i1, i2);
}
Mesh->CreateSectionGroup(GroupKey, Streams);
```

---

## LidarPointCloud

**Type**: Built-in Epic Plugin (no submodule needed)
**Engine**: UE 5.7 (included with engine)
**Platforms**: Win64, Mac, Linux

**Purpose**: Point cloud rendering with LiDAR support (LOD, streaming, color modes).

**Key Classes**:
- `ALidarPointCloudActor` — Actor for point cloud rendering
- `ULidarPointCloudComponent` — Rendering component (color modes, depth range, frustum culling)
- `ULidarPointCloud` — Data container (`CreateFromData(FLidarPointCloudPoint)` factory)
- `FLidarPointCloudPoint` — Point struct (FVector3f position, FColor, bool enabled, float intensity)

**How JUSYNC uses LidarPointCloud**:
- `FJUSYNCPointCloudSpawner` pools `ALidarPointCloudActor` instances
- Background thread builds `TArray64<FLidarPointCloudPoint>` from USD data
- Game thread: `ULidarPointCloud::CreateFromData()` + `Comp->SetPointCloud()`
- Gradient recoloring: deferred until PNG LUT arrives, then batch-recolors pending actors
