# Compiles /Game/Materials/M_JusyncVolume with the renderer present and quits.
#
#   UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="<this file>" \
#     -unattended -nosplash -nocompileeditor -stdout
#
# This cannot run as a commandlet: UMaterial::CacheResourceShadersForRendering
# returns early when FApp::CanEverRender() is false, so a commandlet never
# allocates a material resource or queues a shader job.
import unreal

MATERIAL_PATH = "/Game/Materials/M_JusyncVolume"

errors = unreal.JUSYNCBlueprintLibrary.compile_material_now(MATERIAL_PATH)
text = "" if errors is None else str(errors).strip()

if text:
    unreal.log_error("[VolumeShader] FAIL: " + text)
else:
    unreal.log("[VolumeShader] PASS: " + MATERIAL_PATH + " compiled")

unreal.SystemLibrary.quit_editor()
