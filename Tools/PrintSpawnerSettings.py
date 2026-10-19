import unreal

MAP = "/Game/Scene_Warehouse/Maps/Industrial_Warehouse"
unreal.EditorLoadingAndSavingUtils.load_map(MAP)

actors = unreal.EditorLevelLibrary.get_all_level_actors()
found = 0
for a in actors:
    cls = a.get_class().get_name()
    if "JUSYNCFileSpawner" not in cls:
        continue
    found += 1
    unreal.log("[SpawnerCfg] actor=%s class=%s" % (a.get_name(), cls))
    for prop in ("bMorphEnabled", "MeshBlendMs", "MeshMaxBlendMs", "bSpawnPointClouds",
                 "bFitToVolume", "bEnableLiveUpdates", "bAutoRefreshMeshes",
                 "bUseGradientColors", "MaxSpawnsPerFrame"):
        try:
            value = a.get_editor_property(prop)
            unreal.log("[SpawnerCfg]   %s = %r" % (prop, value))
        except Exception as exc:  # noqa: BLE001
            unreal.log("[SpawnerCfg]   %s = <error %s>" % (prop, exc))
    try:
        mat = a.get_editor_property("SpawnMaterial")
        unreal.log("[SpawnerCfg]   SpawnMaterial = %r" % (mat.get_path_name() if mat else None))
    except Exception as exc:  # noqa: BLE001
        unreal.log("[SpawnerCfg]   SpawnMaterial = <error %s>" % exc)

unreal.log("[SpawnerCfg] total spawners found = %d" % found)
