# Unreal Engine 5.8 upgrade

This project targets **Unreal Engine 5.8**.

## Project settings

- `JusyncSampleProject.uproject`: `EngineAssociation` is `5.8`.
- Game and Editor targets use `BuildSettingsVersion.V7` and
  `EngineIncludeOrderVersion.Unreal5_8`.
- `JUSYNC.uplugin`: `EngineVersion` is `5.8.0`; `RealtimeMeshComponent` and
  `LidarPointCloud` are declared plugin dependencies.

## Third-party compatibility

The pinned plugin revisions were kept and patched minimally instead of moving
to newer major releases:

- `RealtimeMeshComponent` `v5.3.2` -> local `ue5.8-compat` branch
  (`83fa7dd`): removed/deprecated UE 5.8 template and mesh-batch APIs.
- `NodeToCode` `v1.2.3` -> local `ue5.8-compat` branch (`68878eb`):
  converted UE 5.8 `FJsonObject` shared-string keys back to `FString`.

These submodule commits currently exist only in this local checkout. Before
publishing the parent repository, push the `ue5.8-compat` branches to the
submodule remotes (or a fork) and update the gitlinks.

## Verification

- UE 5.8.3 Editor target: builds successfully.
- UE 5.8.3 Game target: builds successfully.
- `Automation RunTests JUSYNC`: 33 passed, 0 failed, 1 skipped.
  The skipped test is `JUSYNC.RawGeo.BrokerCurveE2E`, which requires a live
  RawGeo broker endpoint.
- `/Game/Empty` and
  `/Game/Scene_Warehouse/Maps/Industrial_Warehouse` load under 5.8.
