# GitHub issue draft — TriAxis-Games/RealtimeMeshComponent

File at: https://github.com/TriAxis-Games/RealtimeMeshComponent/issues/new
(No existing duplicate found; nearest neighbours: #304 = *eviction* assert, #256 = older TotalPrimitiveCount assert, both different.)

---

**Title:**

Latent stream-range / live-index-buffer race + no per-mesh BLAS opt-out in `FRealtimeMeshSectionGroupProxy::UpdateRayTracingInfo` (crash-class context: see note)

> **Honesty note (2026-10-16):** our reproduced `D3D12RayTracing.cpp:3317` crashes turned out to be produced by the *engine's* LidarPointCloud octree RT geometry during live point-cloud updates (separate report to Epic). The race below in RMC is from code inspection of `UpdateRayTracingInfo` and is real as a **latent** hazard for any in-place remeshing workload (we validated the clamp locally as hardening), not the confirmed source of our crash. Please treat this as a hardening + API-gap request, not a crash report.

**Body:**

### Environment

- UE 5.7, Win64, DX12 / SM6, hardware ray tracing enabled (`r.RayTracing=1`, Lumen hwRT)
- RMC: `dbf43b7ee9cf1ec71774be7196f4cb55dd0486c8` ("Minor fixes for UE 5.7", uplugin 5.3.2)
- Scenario: one `URealtimeMeshComponent` whose mesh is **completely re-meshed on a ~5 Hz timer** (streamed network geometry, e.g. a torus / re-tessellated point cloud). Vertex **and triangle counts change on most updates**.

### Symptom

Editor asserts within seconds of updates starting:

```
Assertion failed: Initializer.IndexBuffer->GetSize() >=
  (Segment.FirstPrimitive + Segment.NumPrimitives) * FD3D12RayTracingGeometry::IndicesPerPrimitive * IndexStride
  + Initializer.IndexBufferOffset
[File:...\Runtime\D3D12RHI\Private\D3D12RayTracing.cpp] [Line: 3317]
```

Stack goes through `FD3D12RayTracingGeometry` init → RMC `UpdateRayTracingInfo` path. Mesh-only: point clouds / static meshes never hit it.

### Analysis

In `FRealtimeMeshSectionGroupProxy::UpdateRayTracingInfo` the BLAS initializer mixes **two sources of truth that can transiently disagree while the mesh is being updated**:

- `MinIndex/MaxIndex` and each `Segment.FirstPrimitive/NumPrimitives` come from `Section->GetStreamRange()` (already reflects the **new** topology),
- `Initializer.IndexBuffer = IndexStream->IndexBufferRHI` (the buffer resource may still be the **old** one, or the sections' ranges may still be the old ones when the new (smaller/larger) buffer is live).

Any ordering where the range end exceeds the live buffer size trips the D3D12 assert. With per-frame updates that change triangle counts, the window is hit regularly. With constant-vertex-count updates the window is narrower but still exists (buffer resource is released/recreated each update).

Additionally there is **no escape hatch at the RMC level**: `FRealtimeMeshComponentSceneProxy` hardcodes `bSupportsRayTracing = true` and `IsRayTracingRelevant() { return true; }`, so neither `SetVisibleInRayTracing(false)` nor any component/LOD/section property can stop BLAS builds for such a mesh. The only workaround is the global `r.RayTracing=0`, which disables hardware RT for the whole project.

### Steps to reproduce

1. New actor with `URealtimeMeshComponent`; enable hwRT (Lumen ray tracing, `r.RayTracing=1`).
2. On a 0.2 s timer, rebuild the mesh in-place (latent or immediate update): random icosphere/torus tessellation so the index count changes every tick.
3. Start PIE. Crash in `D3D12RayTracing.cpp` typically within 5–30 s.

### Suggested fix

Make the BLAS range consistent with the buffer that actually exists, clamping/skipping instead of asserting (self-heals on the next update):

```cpp
// after PositionStream / IndexStream lookup, before building Initializer:
const uint32 IndexStride = IndexStream->IndexBufferRHI.IsValid() ? IndexStream->IndexBufferRHI->GetStride() : 0;
const int64 LiveIndexCount = IndexStride > 0 ? static_cast<int64>(IndexStream->IndexBufferRHI->GetSize() / IndexStride) : 0;
int32 RTMinIndex = MinIndex, RTMaxIndex = MaxIndex;
if (IndexStride == 0 || LiveIndexCount < 3 || RTMinIndex < 0 || RTMinIndex >= LiveIndexCount)
{
    return false; // ranges and buffer disagree mid-update; retry next update
}
if (RTMaxIndex >= LiveIndexCount)
{
    // throttled warning
    RTMaxIndex = static_cast<int32>(LiveIndexCount) - 1;
}
// use RTMin/RTMax for IndexBufferOffset / TotalPrimitiveCount, and clamp
// Segment.NumPrimitives to (RTMaxIndex + 1) / 3 - Segment.FirstPrimitive,
// dropping segments that start beyond the buffer.
```

(We validated exactly this clamp locally on our fork — crash gone, at worst one update's worth of missing reflections on the streaming mesh.)

Even better long term: derive both sides from one atomic snapshot (the buffer-set record that produced the RHI buffer), and/or expose a per-LOD or per-component "generate ray tracing geometry" toggle so frequently-updated streamed meshes can opt out — related to what PR #304 adds for buffer sets on the 5.8/Core branch.

---

*From the JUSYNC project (streaming point clouds + meshes into UE over the network).*
