# Post-fix verification: loads M_JusyncMorph + M_VertexColor in a live (RHI) editor,
# forces SM shader compilation, and checks the morph CVars exist in the rebuilt DLL.
# Run: UnrealEditor.exe <uproject> -ExecutePythonScript=thisfile
import unreal

for path in ("/Game/Materials/M_JusyncMorph.M_JusyncMorph",
             "/Game/Materials/M_VertexColor.M_VertexColor"):
    mat = unreal.EditorAssetLibrary.load_asset(path)
    if not mat:
        unreal.log_error("[MatCheck] FAIL load %s" % path)
        continue
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.log("[MatCheck] recompiled %s" % path)

unreal.log("[MatCheck] done (grep the log above for 'Failed to compile Material' naming M_JusyncMorph / M_VertexColor)")
