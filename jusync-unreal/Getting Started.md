# Getting Started — JUSYNC Unreal Sample Project

> Setup guide for both human developers and LLMs. Follow each step sequentially.

## Table of Contents

1. [[Clone and Setup]]
2. [[Build Middleware (Linux)]]
3. [[Build Middleware (Windows)]]
4. [[Wire Up Unreal Plugin]]
5. [[Generate Project Files and Build UE]]
6. [[Run Your First USD Scene]]

---

## Prerequisites

### Both Platforms
- **Git** (2.x+)
- **CMake** (3.16+)
- **Unreal Engine 5.7+** launcher installed

### Linux Only
- **GCC** 7+ or **Clang** 6+
- **OpenSSL dev**: `sudo apt install libssl-dev`
- **pkg-config**: `sudo apt install pkg-config`

### Windows Only
- **Visual Studio 2022** (Desktop C++ workload)
- **Python 3.x** (for Unreal project generation)

---

## 1. Clone Both Repositories

```bash
# Clone middleware (C++ source)
git clone https://github.com/FZJ-JSC/jusync.git
cd jusync
git checkout main          # or desired branch
cd ..

# Clone Unreal sample project (with submodules)
git clone --recurse-submodules https://github.com/FZJ-JSC/jusync-uesample.git
cd jusync-uesample
```

### Git Submodule Notes

The Unreal project has **2 git submodules** — `--recurse-submodules` fetches them automatically:

| Submodule | Path | Purpose |
|---|---|---|
| `RealtimeMeshComponent` | `Plugins/RealtimeMeshComponent/` | Dynamic mesh rendering |
| `NodeToCode` | `Plugins/NodeToCode/` | Blueprint → C++ export |

If you cloned without `--recurse-submodules`, run:
```bash
git submodule init
git submodule update --recursive
```

---

## 2. Build Middleware — Linux

```bash
# From jusync/ root
cd /path/to/jusync
mkdir -p build && cd build

# Configure (ZeroMQ is bundled statically, no system libzmq needed)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release -j$(nproc)

# Verify output
ls -la libanari_usd_middleware.so
```

### Key CMake Details

| Setting | Value | Why |
|---|---|---|
| ZeroMQ | Static (bundled) | No system dependency for end users |
| OpenSSL | System package | Hash verification (XXH3/SHA-256) |
| `-static-libstdc++` | ON | Avoids C++ runtime version conflicts |
| `-static-libgcc` | ON | Same reason |
| RPATH | `$ORIGIN` | UE finds `.so` next to plugin |
| C++ Standard | 17 | Middleware uses `std::filesystem` |
| CUDA | OFF (default) | Optional GPU acceleration |

Optional: enable CUDA for GPU-accelerated processing:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_CUDA=ON
```

### Troubleshooting Linux Build

| Error | Fix |
|---|---|
| `GLIBC_X.XX not found` | The library is built with `-static-libstdc++` to avoid this |
| `libssl not found` | `sudo apt install libssl-dev` |
| `ZMQ not found` | ZeroMQ builds from bundled source — no system package needed |
| `ld.so: cannot open libanari_usd_middleware.so` | Copy `.so` to UE plugin `Lib/Linux/` (see step 4) |

---

## 3. Build Middleware — Windows

```powershell
# From jusync/ root
cd C:\path\to\jusync
mkdir build; cd build

# Configure with Visual Studio generator
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_INSTALL_PREFIX="../build/install"

# Build Release
cmake --build . --config Release

# Verify output
dir anari_usd_middleware.lib
dir anari_usd_middleware.dll
```

### Key CMake Details (Windows)

| Setting | Value | Why |
|---|---|---|
| Generator | VS 17 2022 | x64 MSVC |
| ZeroMQ | Static (bundled) | No system dependency |
| OpenSSL | System path | Requires `OPENSSL_ROOT_DIR` |
| DLLs staged | 3 locations | UE editor, PIE, packaged builds |
| C++ Standard | 17 | Middleware uses `std::filesystem` |
| CUDA | OFF (default) | Optional |

### OpenSSL Path

If OpenSSL isn't at the default path, override:
```powershell
cmake .. -DOPENSSL_ROOT_DIR="C:\path\to\OpenSSL"
```

Default search paths:
- `C:\Program Files\FireDaemon OpenSSL 3`
- `${OPENSSL_ROOT_DIR}` (environment variable or cmake arg)

---

## 4. Wire Up Unreal Plugin

After building the middleware, copy the native library into the Unreal plugin's ThirdParty directory.

### Linux — Copy .so

```bash
cp /path/to/jusync/build/libanari_usd_middleware.so \
   /path/to/jusync-uesample/Plugins/JUSYNC/Source/ThirdParty/AnariUsdMiddleware/Lib/Linux/
```

### Windows — Copy .lib + .dll

```powershell
copy C:\path\to\jusync\build\Release\anari_usd_middleware.lib ^
  C:\path\to\jusync-uesample\Plugins\JUSYNC\Source\ThirdParty\AnariUsdMiddleware\Lib\Win64\

copy C:\path\to\jusync\build\Release\anari_usd_middleware.dll ^
  C:\path\to\jusync-uesample\Plugins\JUSYNC\Source\ThirdParty\AnariUsdMiddleware\Lib\Win64\
```

### Sync Headers (if you edited middleware source)

If you made changes to `jusync/include/`, copy updated headers to the Unreal plugin:

```bash
cp -u /path/to/jusync/include/*.h \
   /path/to/jusync-uesample/Plugins/JUSYNC/Source/ThirdParty/AnariUsdMiddleware/Include/
```

> **LLM Note**: `-u` = copy only when source is newer. Use this to avoid overwriting local UE changes.

### Verify the Plugin Sees the Library

Open `JUSYNC.Build.cs` and check that the `LibFile` path resolves. In the UE editor log, look for:
```
JUSYNC: ✅ Linux middleware enabled with static ZeroMQ
# or
JUSYNC: ✅ Windows middleware enabled with enhanced staging
```

If you see `❌ ... library not found`, the file path is wrong.

---

## 5. Generate Project Files and Build

### Generate Project (first time only)

**Linux**:
```bash
# From jusync-uesample/ root
/path/to/Engine/Build/BatchFiles/GenerateProjectFiles.sh -project="JusyncSampleProject.uproject"
```

**Windows**:
```powershell
# From jusync-uesample/ root
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" JusyncSampleProject Development Linux
# Then:
/path/to/Engine/Build/BatchFiles/GenerateProjectFiles.bat JusyncSampleProject.uproject
```

### Build UE Project

**Linux** (from VSCode or Rider with UE plugin):
```bash
/path/to/Engine/Build/BatchFiles/Linux/Build.sh JusyncSampleProjectEditor Linux Development
# Or using UBT directly:
dotnet /path/to/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll \
  JusyncSampleProjectEditor Linux Development \
  -Project="/path/to/jusync-uesample/JusyncSampleProject.uproject" \
  -editorproject="/path/to/jusync-uesample/"
```

**Windows** (in Visual Studio):
1. Open `JusyncSampleProject.vcxproj`
2. Set configuration: `Development Editor`
3. Build solution (`Ctrl+Shift+B`)

### Open in UE Editor

**Linux** (UE5 uses `UnrealEditor`, not `UE4Editor`):
```bash
/path/to/Engine/Binaries/Linux/UnrealEditor /path/to/jusync-uesample/JusyncSampleProject.uproject
```

**Windows**:
1. Double-click `JusyncSampleProject.uproject`
2. Or open via Epic Games Launcher → Library → Local Projects

---

## 6. Run Your First USD Scene

### Quick Test in UE Editor

1. Open `JusyncSampleProject.uproject`
2. Open `Content/MainMap.umap` (or `Content/Empty.umap` for blank)
3. In the Content Browser, find `Content/JusyncSpawner.uasset`
4. Drag the `JusyncSpawner` Blueprint into the level
5. With the spawner selected, open Details panel → JUSYNC|Spawner|Connection:
   - **BrokerEndpoint**: `tcp://localhost:5556` (or your broker IP)
6. Click the `Start Spawning` button in the Details panel

### Expected Behavior

1. **State: Connecting** → connects to broker
2. **State: Fetching File List** → requests USD file list
3. **State: Downloading** → downloads files in `PipelineDepth` batches
4. **State: Spawning** → parses USD → creates RealtimeMesh components
5. **State: Complete** → all meshes spawned in grid layout

### Verify on Screen

- Meshes appear in the viewport at `SpawnTargetActor` location (or spawner location)
- Point clouds render if `bSpawnPointClouds` is enabled
- Gradient colors apply if `bUseGradientColors` is enabled and a PNG exists on the broker
- Live updates trigger if `bEnableLiveUpdates` and `bAutoRefreshMeshes` are enabled

### Troubleshooting

| Symptom | Fix |
|---|---|
| `❌ Windows library not found` | Copy `.dll` + `.lib` to `Lib/Win64/` |
| `❌ Linux library not found` | Copy `.so` to `Lib/Linux/` |
| Broker not connecting | Verify `tcp://host:port` is correct, broker is running |
| Meshes don't appear | Check UE log for `Failed to create mesh` errors |
| `Fs.inotify.max_user_watches` exceeded | `echo 524288 | sudo tee -a /etc/sysctl.conf && sudo sysctl -p` |
| No point clouds | Ensure `LidarPointCloud` plugin is enabled |

---

## Anatomy of the Two-Repo Setup

```
GitHub
├── FZJ-JSC/jusync                    # ← C++ middleware source
│   ├── src/*.cpp                     # Native implementation
│   ├── include/*.h                   # Public C++ headers
│   ├── CMakeLists.txt                # Builds .so / .dll
│   └── UnrealPlugin/JUSYNC/Source/   # Plugin source (reference copy)
│
└── FZJ-JSC/jusync-uesample           # ← Unreal sample project
    ├── Plugins/JUSYNC/               # Plugin (consumes .so/.dll)
    │   ├── Source/JUSYNC/            # Plugin C++ (1:1 with reference copy)
    │   └── Source/ThirdParty/        # Native lib + headers
    └── Content/                      # Maps, BPs, materials

          Build .so/.dll
              │
jusync ──────┘
              │
              ▼  copy .so/.dll into Lib/
          jusync-uesample/Plugins/.../ThirdParty/
              │
              ▼  UE Build → Editor → Spawn meshes
          JusyncSampleProject.uproject
```
