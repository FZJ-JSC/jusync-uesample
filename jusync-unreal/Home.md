# JUSYNC Unreal Sample Project

> LLM-friendly entry point for navigating this UE5.7 project.
> Follow the links below to drill into any subsystem.

## Quick Links

| Area | Notes |
|---|---|
| [[Project Hierarchy]] | Full directory tree with descriptions |
| [[Plugin Architecture]] | JUSYNC plugin internals, class graph, module layout |
| [[Latest Fixes & Optimizations]] | All 13 performance fixes + compiler fixes |
| [[ThirdParty Dependencies]] | Middleware, glm, xxhash, ZeroMQ, RMC, LidarPointCloud |
| [[Build Configuration]] | .uproject, Build.cs, .uplugin, INI configs |
| [[Content Assets Inventory]] | Content/ folder asset catalog (Blueprints, maps, materials) |
| [[Git Submodules]] | RealtimeMeshComponent, NodeToCode submodule setup |

## Project Summary

- **Engine**: Unreal Engine 5.7
- **Project Name**: `JusyncSampleProject`
- **Purpose**: Real-time USD data synchronization for ANARI rendering pipeline
- **Core Plugin**: JUSYNC (custom, Win64+Linux)
- **Submodules**: RealtimeMeshComponent (TriAxis), NodeToCode (Marketplace)
- **Platform**: Win64 (DX12/SM6), Linux (Vulkan SM6)
- **Default Map**: `/Game/Empty.Empty`
- **Key Feature**: Pipeline depth-based async USD download → parse → RealtimeMesh spawn

## Key Files (for LLM navigation)

| What you want to find | File / Path |
|---|---|
| **Build artifacts** | `Plugins/JUSYNC/Binaries/Linux/libUnrealEditor-JUSYNC.so` (1.5 MB) |
| **Middleware .so** | `ThirdParty/AnariUsdMiddleware/Lib/Linux/libanari_usd_middleware.so` (131 MB) |
| Plugin entry point | `Plugins/JUSYNC/Source/JUSYNC/Private/JUSYNCModule.cpp` |
| ZMQ / broker logic | `Plugins/JUSYNC/Source/JUSYNC/Private/JUSYNCSubsystem.cpp` |
| USD parsing + mesh spawn | `Plugins/JUSYNC/Source/JUSYNC/Private/JUSYNCFileSpawnerActor.cpp` |
| Blueprint-exposed functions | `Plugins/JUSYNC/Source/JUSYNC/Private/JUSYNCBlueprintLibrary.cpp` |
| Point cloud spawner | `Plugins/JUSYNC/Source/JUSYNC/Private/JUSYNCPointCloudSpawner.cpp` |
| Data types (FJUSYNCMeshData, etc.) | `Plugins/JUSYNC/Source/JUSYNC/Public/JUSYNCTypes.h` |
| Middleware C headers | `Plugins/JUSYNC/Source/ThirdParty/AnariUsdMiddleware/Include/` |
| Build rules | `Plugins/JUSYNC/Source/JUSYNC/JUSYNC.Build.cs` |
| Main map | `Content/MainMap.umap` |
| Spawner BP | `Content/JusyncSpawner.uasset` |

## Engine Settings (from `DefaultEngine.ini`)

- Static lighting: **disabled** (`r.AllowStaticLighting=False`)
- Ray tracing: **enabled**
- Virtual shadows: **enabled**
- Dynamic GI: **Screen Space (method=1)**
- Reflections: **Screen Space (method=1)**
- RHI: DX12 SM6 (Win), Vulkan SM6 (Linux)
