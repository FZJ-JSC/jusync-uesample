# Project Hierarchy

## Root File Tree

```
jusync-uesample/
│
├── JusyncSampleProject.uproject          # UE5.7 project config (EngineAssociation: 5.7)
├── .gitmodules                           # 2 submodules: RMC + NodeToCode
├── .gitignore
├── .vsconfig
├── LICENSE
├── README.md
│
├── Source/                               # Game C++ module
│   ├── JusyncSampleProject.Target.cs     # Game target (shipping)
│   ├── JusyncSampleProjectEditor.Target.cs
│   └── JusyncSampleProject/
│       ├── JusyncSampleProject.Build.cs  # Module rules
│       ├── JusyncSampleProject.cpp       # FJusyncSampleProjectModule
│       └── JusyncSampleProject.h
│
├── Config/                               # Default INI configs
│   ├── DefaultEngine.ini                 # Renderer, RHI, platform settings
│   ├── DefaultGame.ini                   # Project ID, startup actions
│   ├── DefaultEditor.ini
│   └── DefaultInput.ini
│
├── Content/                              # UAsset content (game assets)
│   ├── Empty.umap                        # Default map (GameMapsSettings)
│   ├── MainMap.umap                      # Main gameplay level
│   ├── MainMap_HLOD0_Instancing.uasset   # HLOD auto-generated
│   ├── Base_Material.uasset
│   ├── JusyncSpawner.uasset              # BP_AJUSYNCFileSpawnerActor
│   ├── JusyncSpawnerSimplified.uasset    # Simplified BP variant
│   ├── Collections/                      # World partition / level collections
│   ├── Developers/staticxg7/Collections/
│   ├── __ExternalActors__/
│   ├── __ExternalObjects__/
│   └── StarterContent/                   # UE5 blank-template starter pack
│       ├── Architecture/
│       ├── Audio/
│       ├── Blueprints/
│       ├── HDRI/
│       ├── Maps/
│       ├── Materials/                    # 43 PBR materials
│       ├── Particles/
│       ├── Props/
│       ├── Shapes/
│       └── Textures/
│
├── Plugins/                              # 3 installed plugins
│   ├── JUSYNC/                           # ⭐ CUSTOM PLUGIN (local)
│   ├── RealtimeMeshComponent/            # ⬇️ GIT SUBMODULE (TriAxis-Games)
│   └── NodeToCode/                       # ⬇️ GIT SUBMODULE (Marketplace)
│
├── Benchmark/                            # Raw benchmark JSONs
│   ├── benchmark_results_20260310_140826.json
│   └── benchmark_results_20260310_143528.json
│
├── DerivedDataCache/                     # (generated, gitignored)
├── Intermediate/                         # (generated, gitignored)
├── Binaries/                             # (generated, gitignored)
└── Saved/                                # (generated: config, logs, screenshots)
```

## Plugin: JUSYNC Internal Structure

```
Plugins/JUSYNC/
├── JUSYNC.uplugin                        # Plugin manifest (v1.0.0, Runtime, Win64+Linux)
├── Config/FilterPlugin.ini
├── Source/
│   ├── JUSYNC/
│   │   ├── JUSYNC.Build.cs               # Module build rules (C++20, DLL staging)
│   │   ├── Public/                       # 8 header files
│   │   │   ├── JUSYNCModule.h
│   │   │   ├── JUSYNCSubsystem.h
│   │   │   ├── JUSYNCBlueprintLibrary.h
│   │   │   ├── JUSYNCAsyncActions.h
│   │   │   ├── JUSYNCFileSpawnerActor.h  # Main spawner actor (UCLASS)
│   │   │   ├── JUSYNCPointCloudSpawner.h # Point cloud spawner (FTickable)
│   │   │   ├── JUSYNCPointCloudTestActor.h
│   │   │   └── JUSYNCTypes.h             # All FJUSYNCXXX structs
│   │   └── Private/                      # 8 .cpp files (1:1 with .h)
│   │       ├── JUSYNCModule.cpp          # IModuleInterface impl
│   │       ├── JUSYNCSubsystem.cpp       # UObjectSubsystem (ZMQ broker)
│   │       ├── JUSYNCBlueprintLibrary.cpp
│   │       ├── JUSYNCAsyncActions.cpp
│   │       ├── JUSYNCFileSpawnerActor.cpp
│   │       ├── JUSYNCPointCloudSpawner.cpp
│   │       ├── JUSYNCPointCloudTestActor.cpp
│   │       └── JUSYNCTypes.cpp
│   └── ThirdParty/
│       ├── AnariUsdMiddleware/           # Native C++ middleware
│       │   ├── Include/                  # 17 headers
│       │   └── Lib/
│       │       ├── Linux/libanari_usd_middleware.so
│       │       └── Win64/anari_usd_middleware.exp (NO .dll shipped)
│       ├── glm/                          # Header-only math (glm/glm/...)
│       ├── xxhash/                       # xxhash.h + xxhash.c
│       └── ZeroMQ/                       # zmq include/ + lib/
```

## Game Content Categories

| Category | Path | Files | Purpose |
|---|---|---|---|
| Default maps | `Content/*.umap` | 2 (Empty, MainMap) | GameMapsSettings default |
| Spawner BPs | `Content/JusyncSpawner*.uasset` | 2 | BP wrappers for AJUSYNCFileSpawnerActor |
| Materials | `Content/StarterContent/Materials/` | 43 | UE5 template PBR materials |
| HLOD | `Content/*_HLOD*` | 1 | Auto-generated HLOD instances |
| Starter pack | `Content/StarterContent/` | 10 subfolders | Blank template content |
