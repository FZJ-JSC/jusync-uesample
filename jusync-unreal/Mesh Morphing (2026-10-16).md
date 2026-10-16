# Mesh Morphing (2026-10-16)

> Makes GPU mesh morphing work with the ANARI-USD sender's **changing topology** and
> ships the missing morph material. Previously `FJUSYNCMeshMorpher` never engaged
> because both of its gates always failed for streamed data.

## Why it never worked before

1. **No morph-capable material** — the morpher requires the applied material to declare
   `JusyncMorphT`/`JusyncMorphActive` (WPO branch). A scan of all 420 sample-project assets
   found none, so every update took the instant-apply path.
2. **Topology changes every timestep** — the sender (ANARI-USD `UsdBridge`) writes
   `points` **and** `faceVertexIndices`/`faceVertexCounts` as time-varying attributes, so the
   tri-index hash mismatched on every sample and `BeginUpdate` returned `Normal`.

## What changed

| Piece | Change | File |
|-------|--------|------|
| Cross-topology morph | When the tri hash differs, new vertices are matched to nearest previous-sample vertices via a uniform-grid spatial hash; matched positions drive the blend (same GPU path as before) | `JUSYNCMeshMorpher.cpp/.h` |
| Delta packing | Blend streams now pack `prev - current` per vertex into UV1/UV2 (small values, float32-safe; no position nodes needed in the material) | `JUSYNCStreamBuilder.cpp` |
| UV layout | Non-blend compact streams write 3 float UV sets with UV1/UV2 = 0 → WPO delta exactly 0 when idle | `JUSYNCStreamBuilder.cpp` |
| Material auto-wire | If the material about to be applied lacks `JusyncMorphT`, a per-component MID of `jusync.Morph.MeshMaterial` replaces it | `JUSYNCSubsystem.cpp` |
| Morph material | New `/Game/Materials/M_JusyncMorph`: VertexColor → BaseColor; `WPO = UV12-delta * (1 - JusyncMorphT) * JusyncMorphActive` | `Tools/CreateJusyncMorphMaterial.py` (regenerates the .uasset) |

## Material contract (WPO branch)

```
UV1 = (dX, dY)   UV2 = (dZ, 0)      per-vertex delta to previous sample
WPO = (dX, dY, dZ) * (1 - JusyncMorphT) * JusyncMorphActive
JusyncMorphT:      0 = previous sample, 1 = current (ramped by morpher Tick)
JusyncMorphActive: 1 during a blend, else 0
```

Rebuild the asset any time:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "D:\jsc_github\jusync-uesample\JusyncSampleProject.uproject" `
  -run=pythonscript -script="D:/jsc_github/jusync-uesample/Tools/CreateJusyncMorphMaterial.py" `
  -nocompileeditor -stdout -unattended -nopause -nosplash
```

## Console variables

| CVar | Default | Meaning |
|------|---------|---------|
| `jusync.Morph.Enabled` | 1 | Master switch for mesh morphing |
| `jusync.Morph.MeshBlendMs` | 0 | Fixed blend window (ms); 0 = auto from sample gap |
| `jusync.Morph.MeshMaxBlendMs` | 2000 | Cap for the auto blend window |
| `jusync.Morph.MeshTopologyMatch` | **1 (new)** | 1 = morph across topology changes via nearest-vertex matching; 0 = identical-topology only (old behavior) |
| `jusync.Morph.MeshMatchMaxVerts` | **3000000 (new)** | Per-side vertex budget for matching (game thread); larger meshes instant-apply |
| `jusync.Morph.MeshMatchMinPct` | **40 (new)** | Minimum matched-vertex percentage required, else instant-apply |
| `jusync.Morph.MeshMaterial` | **/Game/Materials/M_JusyncMorph.M_JusyncMorph (new)** | Material substituted when the applied one isn't morph-capable; empty = never override |

## Behavioral notes / trade-offs

- Matching runs on the **game thread** per sample (same policy as the cloud morpher).
  ~1M verts ≈ a few tens of ms; beyond `MeshMatchMaxVerts` updates fall back to instant-apply.
- A sample arriving **mid-blend** restarts from the previous sample's matched positions
  instead of the in-flight interpolated ones (sub-frame error at streaming rates).
- The `MeshMaterial` override **replaces** non-capable materials (including sender-texture
  MIDs) while morphing is enabled; set `jusync.Morph.MeshMaterial ""` to keep your own
  materials (then morphing only runs for morph-capable ones), or add the WPO branch above
  to your material.
- Diagnostics: `LogJUSYNC` logs `cross-topology morph, matched N/M verts` (throttled 2s)
  and Verbose lines for every fallback reason.

## Try it with the sender container

`staticxg7/pv-anari-usd:6.6` ships `/opt/pvd-examples/animated-demo/` — two synced
30-step PVDs (plasma torus mesh: 51,200 verts/102,400 tris with `energy` scalars;
storm points: 320,000 with `speed`/`temp`), seamless-looping. Open both in the
container's ParaView over VNC (`localhost:6080`), press Play, and the JUSYNC
broker streams them to UE where the morpher path exercises live. Regenerate with
`jusync/tools/generate-pvd-demo.py` (header has the container recipe).

## Files

- `UnrealPlugin/JUSYNC/Source/JUSYNC/Public/JUSYNCMeshMorpher.h`
- `UnrealPlugin/JUSYNC/Source/JUSYNC/Private/JUSYNCMeshMorpher.cpp`
- `UnrealPlugin/JUSYNC/Source/JUSYNC/Private/JUSYNCStreamBuilder.cpp`
- `UnrealPlugin/JUSYNC/Source/JUSYNC/Private/JUSYNCSubsystem.cpp`
- `jusync-uesample/Tools/CreateJusyncMorphMaterial.py`
- `jusync-uesample/Content/Materials/M_JusyncMorph.uasset`
