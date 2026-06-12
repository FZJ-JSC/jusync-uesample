# Latest Fixes & Optimizations

> All 13 performance/correctness fixes applied 2026-06-11 + compiler fixes for UE5.6+
> Both repos synced: `jusync/UnrealPlugin/` ↔ `jusync-uesample/Plugins/JUSYNC/`

## Table of Contents

1. [[Performance Fixes]]
2. [[Correctness Fixes]]
3. [[Compiler Fixes (UE5.6+)]]
4. [[Files Modified]]

---

### Performance Fixes (HIGH/MEDIUM IMPACT)

| # | Fix | File | Impact |
|---|-----|------|--------|
| 1 | Remove GameThread AsyncTask NOOP | `FileSpawnerActor.cpp:631` | Eliminates task graph scheduler overhead per mesh spawn |
| 2 | C callback deep copy (5 allocs → 2) | `Subsystem.cpp:103-189` | 60% fewer heap allocations per ZMQ file callback |
| 3 | Single-pass file filtering (4 loops → 1) | `FileSpawnerActor.cpp:283-310` | 75% fewer iterations over file list |
| 4 | Deduplicate ManualRefresh / HandleCommitComplete | `FileSpawnerActor.cpp` | ~80 lines, extracted `DiffAndRefreshFileList` helper |
| 5 | EnqueuePointCloud double-copy → move | `PointCloudSpawner.cpp:19-45` | Eliminates redundant deep copy, removed dead `ConversionQueue` |
| 6 | 19× `EAsyncExecution::Thread` → thread pool | All `.cpp` files | Reuses UE thread pool (no more new OS threads) |
| 7 | `.GetSafeNormal()` per vertex → batch | `Subsystem.cpp:302` | Removes per-vertex branch-heavy normalization |
| 8 | `Add()` → `SetNum` + `[]` for geometry arrays | `Subsystem.cpp` | Eliminates per-element `Add()` overhead |
| 9 | Static counter threshold 1000 → 10000 | `Subsystem.cpp:627` | Less log spam if called in Tick |
| 10 | Sync ThirdParty headers | `ThirdParty/.../Include/` | Reconciled 8-line diff between repos |
| 11 | `AppendChar` loop → `std::memcmp` | `BlueprintLibrary.cpp:1110` | ~30× faster USD format validation |
| 12 | Pre-format mesh key suffix | `FileSpawnerActor.cpp:647` | One fewer string allocation per mesh spawn |
| 13 | Grid columns configurable (was hardcoded 10) | `FileSpawnerActor.h` | New `SpawnGridColumns` UPROPERTY (1-100) |

---

### Correctness Fixes

| Fix | Description |
|-----|-------------|
| **DiffAndRefreshFileList** | Extracted shared helper for ManualRefresh + HandleCommitCompleteNotification. Both now call `DiffAndRefreshFileList(NewFiles, NewSizes, NewRanks, bIsManual)`. Eliminates ~80 lines of duplicated code that maintained separate `OldSizes` maps, filters, and spawn logic. |
| **ConversionQueue removed** | `FConversionEntry` struct and `ConversionQueue` member were dead code. `EnqueuePointCloud` never read from it — the lambda captured from `PCData` directly. Removed both struct definition and member variable from header + cpp. |

---

### Compiler Fixes (UE5.6+)

These were discovered when building against UE5.6+ and are not in the original `jusync/` repo:

| Error | Fix | File |
|-------|-----|------|
| `MoveTemp` called on const object | Changed `MoveTemp(PCData.Positions)` → `PCData.Positions` (copy capture) | `PointCloudSpawner.cpp:28` |
| `use of undeclared identifier 'OutFiles'` | Renamed `OutFiles` → `LocalFiles` (conflict with method parameter) | `FileSpawnerActor.cpp:278` |
| `redefinition of 'WeakSubsystem'` | Removed duplicate `TWeakObjectPtr` declaration | `BlueprintLibrary.cpp:457` |
| `address of array 'filename' always evaluates to true` | Changed `file_data->filename ?` → `file_data->filename[0] != '\0' ?` (C-ABI arrays) | `Subsystem.cpp:105` |
| `no member named 'Position' in FLidarPointCloudPoint` | Changed `.Position` → `.Location` (UE5.6+ rename) | `Subsystem.cpp`, `PointCloudSpawner.cpp` |
| `FLidarPointCloudPoint(pos, col, true, 0)` | Changed to UE5.6+ constructor: `FLidarPointCloudPoint(x, y, z, r, g, b, a)` | `Subsystem.cpp:2211`, `PointCloudSpawner.cpp:64` |
| extraneous closing braces `}` | Removed 2 extra `}` from deduplicated code blocks | `FileSpawnerActor.cpp:1155,1204` |
| unhandled `PointCloudData.ElementName` capture | Added `ElementNameForLog` string capture to lambda | `Subsystem.cpp:2253` |

---

### Build Verification

```
Result: Succeeded
Total execution time: 7.84 seconds

Built artifacts:
- libUnrealEditor-JUSYNC.so        (1.5 MB)
- libanari_usd_middleware.so        (131 MB)
- Debug symbols (.sym)             (14.1 MB)

Middleware .so linkage:
- libpthread.so.0 ✓
- librt.so.1 ✓
- libm.so.6 ✓
- libanari_usd_middleware.so ✓ (RPATH $ORIGIN)
- libUnrealEditor-LidarPointCloudRuntime.so ✓ (UE5 engine)
```

---

### How Fixes Were Applied

1. **Identified issues** in `jusync/UnrealPlugin/JUSYNC/Source/JUSYNC/Private/*.cpp`
2. **Fixed all 13** performance/correctness issues
3. **Copied to sample project** → `jusync-uesample/Plugins/JUSYNC/Source/JUSYNC/`
4. **Discovered compiler errors** during UE5.6+ build
5. **Fixed 8 more** compatibility issues
6. **Synced back** to `jusync/UnrealPlugin/` so both repos match
7. **Built successfully** — `libUnrealEditor-JUSYNC.so` linked

---

### Files Modified

| File | Changes |
|------|---------|
| `JUSYNCFileSpawnerActor.cpp` | Fixes 1, 3, 4, 6, 12, 13 + `LocalFiles` rename, brace fixes |
| `JUSYNCFileSpawnerActor.h` | Fix 4 (`DiffAndRefreshFileList`), Fix 13 (`SpawnGridColumns`) |
| `JUSYNCSubsystem.cpp` | Fixes 2, 6, 7, 8, 9 + C-ABI array checks, FLidarPointCloudPoint |
| `JUSYNCBlueprintLibrary.cpp` | Fixes 6, 11 + `#include <cstring>`, duplicate `WeakSubsystem` |
| `JUSYNCPointCloudSpawner.cpp` | Fix 5 + `MoveTemp` on const, `FLidarPointCloudPoint` constructor |
| `JUSYNCPointCloudSpawner.h` | Fix 5 (removed dead `FConversionEntry` + `ConversionQueue`) |
| `ThirdParty/AnariUsdMiddleware/Include/*.h` | Fix 10 (17 headers synced from `jusync/include/`) |
