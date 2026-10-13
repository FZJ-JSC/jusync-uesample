# Build Configuration

## Project File

**File**: `JusyncSampleProject.uproject`

```json
{
  "FileVersion": 3,
  "EngineAssociation": "5.7",
  "Modules": [{ "Name": "JusyncSampleProject", "Type": "Runtime" }],
  "Plugins": [
    { "Name": "ModelingToolsEditorMode", "Enabled": true, "TargetAllowList": ["Editor"] },
    { "Name": "JUSYNC", "Enabled": true },
    { "Name": "NodeToCode", "Enabled": true, "MarketplaceURL": "..." },
    { "Name": "LidarPointCloud", "Enabled": true, "SupportedTargetPlatforms": ["Win64", "Mac", "Linux"] }
  ]
}
```

### Module

| Field | Value |
|---|---|
| Name | `JusyncSampleProject` |
| Type | `Runtime` |
| LoadingPhase | `Default` |

### Plugins Declared Here

4 plugins are declared in `.uproject`:
1. **ModelingToolsEditorMode** — built-in UE5.7 (Editor only)
2. **JUSYNC** — custom plugin (local `Plugins/JUSYNC/`)
3. **NodeToCode** — Marketplace plugin (git submodule)
4. **LidarPointCloud** — built-in UE5.7 (Win64/Mac/Linux)

---

## Plugin Manifest

**File**: `Plugins/JUSYNC/JUSYNC.uplugin`

```json
{
  "Version": 1,
  "VersionName": "1.0.0",
  "FriendlyName": "JUSYNC",
  "Category": "Rendering",
  "EngineVersion": "5.7.0",
  "CanContainContent": true,
  "Installed": true,
  "Modules": [{
    "Name": "JUSYNC",
    "Type": "Runtime",
    "LoadingPhase": "Default",
    "PlatformAllowList": ["Win64", "Linux"]
  }],
  "Plugins": [
    { "Name": "RealtimeMeshComponent", "Enabled": true },
    { "Name": "LidarPointCloud", "Enabled": true }
  ]
}
```

### Plugin Auto-Depends On

The `Plugins` block in `.uplugin` auto-enables RealtimeMeshComponent and LidarPointCloud when JUSYNC is enabled. This is why RMC appears both as a submodule AND as a plugin dependency — JUSYNC's `.uplugin` references it, and the project also has it as a submodule for completeness.

---

## Module Build Rules

### Game Module: `JusyncSampleProject.Build.cs`

```csharp
public class JusyncSampleProject : ModuleRules
{
    PublicDependencyModuleNames = { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" };
    PrivateDependencyModuleNames = { /* none */ };
}
```

Simple blank-template rules. No JUSYNC dependency — the game relies on Blueprint to use JUSYNC.

### Plugin Module: `JUSYNC.Build.cs`

```csharp
PublicDependencyModuleNames = { "Core", "CoreUObject", "Engine", "RealtimeMeshComponent" };
PrivateDependencyModuleNames = { "Slate", "SlateCore", "RenderCore", "RHI", "GameplayTasks", "LidarPointCloudRuntime", "ImageWrapper" };

// Include paths
PublicIncludePaths.Add("ThirdParty/glm");
PublicIncludePaths.Add("ThirdParty/AnariUsdMiddleware/Include");
```

#### Include Resolution

| Path | Resolves To |
|---|---|
| `ThirdParty/glm` | `Plugins/JUSYNC/Source/ThirdParty/glm/` (header-only, `#include "glm/glm/..."`) |
| `ThirdParty/AnariUsdMiddleware/Include` | `Plugins/JUSYNC/Source/ThirdParty/AnariUsdMiddleware/Include/` (17 headers) |

#### Platform Configuration

**Windows (`ConfigureWindows`)**:
| Setting | Value |
|---|---|
| System libs | `kernel32.lib`, `ws2_32.lib`, `iphlpapi.lib`, `userenv.lib`, `DXGI.lib` |
| Middleware lib | `AnariUsdMiddleware/Lib/Win64/anari_usd_middleware.lib` |
| DLL staging | 3 locations: BinaryOutputDir, ProjectDir/Binaries/Win64, PluginDir/Binaries/Win64 |
| Definition | `WITH_ANARI_USD_MIDDLEWARE=1` (if .lib exists) |

**Linux (`ConfigureLinux`)**:
| Setting | Value |
|---|---|
| System libs | `pthread`, `dl`, `rt`, `m` |
| Middleware lib | `AnariUsdMiddleware/Lib/Linux/libanari_usd_middleware.so` |
| Definition | `WITH_ANARI_USD_MIDDLEWARE=1` (if .so exists) |

**Other Platforms**: `WITH_ANARI_USD_MIDDLEWARE=0` — middleware disabled.

#### DLL Staging Strategy

Windows DLL is staged to 3 locations for comprehensive coverage:
1. `$(BinaryOutputDir)` — packaged builds
2. `$(ProjectDir)/Binaries/Win64/` — editor + PIE
3. `$(PluginDir)/Binaries/Win64/` — plugin-specific loading

**Important**: The `anari_usd_middleware.dll` is NOT in the repo — only `.exp` file exists. It must be built from the `jusync/` C++ source via CMake.

---

## Engine Configuration

### DefaultEngine.ini

```ini
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Game/Empty.Empty
EditorStartupMap=/Game/Empty.Empty

[/Script/Engine.RendererSettings]
r.AllowStaticLighting=False          # No baked lighting
r.GenerateMeshDistanceFields=True
r.DynamicGlobalIlluminationMethod=1   # Screen Space
r.ReflectionMethod=1                  # Screen Space
r.SkinCache.CompileShaders=True
r.RayTracing=True
r.Shadow.Virtual.Enable=1

[/Script/WindowsTargetPlatform.WindowsTargetSettings]
DefaultGraphicsRHI=DefaultGraphicsRHI_DX12
+D3D12TargetedShaderFormats=PCD3D_SM6

[/Script/LinuxTargetPlatform.LinuxTargetSettings]
+TargetedRHIs=SF_VULKAN_SM6

[/Script/HardwareTargeting.HardwareTargetingSettings]
TargetedHardwareClass=Desktop
DefaultGraphicsPerformance=Maximum

[HTTP]
HttpConnectionTimeout=300.000000
HttpActivityTimeout=3600.000000       # 1 hour — for large file transfers
```

### DefaultGame.ini

```ini
[/Script/EngineSettings.GeneralProjectSettings]
ProjectID=281D241440F59EFE5B9F8BA33560C9A6

[StartupActions]
bAddPacks=True
InsertPack=(PackSource="StarterContent.upack",PackName="StarterContent")
```

Standard blank-template project. StarterContent autopacks on first open.

### DefaultEditor.ini

Default editor settings (empty in current state).

### DefaultInput.ini

Default input bindings (empty, enhanced input used instead).

---

## Build Pipeline

```
CMAKE (jusync/ source)              UE Build (jusync-uesample/)
─────────────────────               ─────────────────────────────
src/*.cpp                           Source/JusyncSampleProject/*.cpp
include/*.h                         Plugins/JUSYNC/Source/JUSYNC/*.h/.cpp
glm/                                Plugins/JUSYNC/Source/ThirdParty/glm/
xxhash/                             Plugins/JUSYNC/Source/ThirdParty/xxhash/
ZeroMQ/                             Plugins/JUSYNC/Source/ThirdParty/ZeroMQ/
                                        │
     Produces:                        JUSYNC.Build.cs reads:
     └── libanari_usd_middleware.so     ├── ThirdParty/*/Include/ (headers)
     └── anari_usd_middleware.dll       ├── ThirdParty/*/Lib/     (native libs)
                                        └── Public/ + Private/    (plugin code)
                                             │
                                             ▼
                                       Compiled Module (Win64/Linux)
```

To build the middleware from source:
```bash
cd /home/staticxg7/Desktop/Github/jusync
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)   # Linux: libanari_usd_middleware.so
                  # Win: anari_usd_middleware.dll/lib
```

Then copy the `.so`/`.dll` to:
```
jusync-uesample/Plugins/JUSYNC/Source/ThirdParty/AnariUsdMiddleware/Lib/Linux/
jusync-uesample/Plugins/JUSYNC/Source/ThirdParty/AnariUsdMiddleware/Lib/Win64/
```

---

## FilterPlugin.ini

**File**: `Plugins/JUSYNC/Config/FilterPlugin.ini`

Controls which asset paths from the plugin are deployed. Standard pattern — typically only deploys `Content/JUSYNC/` path.

---

## Build Verification (Post-Build)

### Built Artifacts

```
Plugins/JUSYNC/Binaries/Linux/
├── libUnrealEditor-JUSYNC.so      (1.5 MB) — UE plugin module
├── libUnrealEditor-JUSYNC.pdb     (14.1 MB) — Debug symbols
└── libUnrealEditor-JUSYNC.sym     — Breakpad symbols
```

### Middleware .so

```
Plugins/JUSYNC/Source/ThirdParty/AnariUsdMiddleware/Lib/Linux/
└── libanari_usd_middleware.so     (131 MB) — From CMake build in jusync/
```

### Link Dependencies (ldd)

```
libpthread.so.0    → /usr/lib/x86_64-linux-gnu/ ✓
librt.so.1         → /usr/lib/x86_64-linux-gnu/ ✓
libm.so.6          → /usr/lib/x86_64-linux-gnu/ ✓
libanari_usd_middleware.so → ThirdParty/.../Lib/Linux/ ✓ (RPATH $ORIGIN)
libUnrealEditor-LidarPointCloudRuntime.so → UE5 engine ✓
libUnrealEditor-Core.so → UE5 engine (at runtime)
libUnrealEditor-Engine.so → UE5 engine (at runtime)
```

### Symbol Table

```bash
# Plugin module
nm -D libUnrealEditor-JUSYNC.so | grep Spawner:
  _Z42Z_Construct_UClass_AJUSYNCFileSpawnerActorv
  _Z10StaticEnumI19EJUSYNCSpawnerStateEP5UEnumv
  ...

# Middleware
nm -D libanari_usd_middleware.so | grep ProcessMeshDataFromUSD_C:
  T ... _Z30ProcessMeshDataFromUSD_CPP      # C-ABI entry point
```

### Build Commands Summary

```bash
# 1. Build middleware
cd /path/to/jusync && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j$(nproc)

# 2. Copy .so to UE project
cp libanari_usd_middleware.so /path/to/jusync-uesample/Plugins/JUSYNC/Source/ThirdParty/AnariUsdMiddleware/Lib/Linux/

# 3. Build UE plugin
dotnet /path/to/UE/Engine/Binaries/DotNET/UnrealBuildTool.dll JusyncSampleProjectEditor Linux Development \
  -Project="/path/to/jusync-uesample/JusyncSampleProject.uproject" \
  -editorproject="/path/to/jusync-uesample/"
```
