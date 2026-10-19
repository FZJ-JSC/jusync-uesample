import unreal

MEL = unreal.MaterialEditingLibrary
PATHS = [
    "/Game/Materials/M_JusyncMesh",
    "/Game/Materials/M_JusyncMorph",
    "/Game/Materials/M_VertexColor",
    "/Game/Materials/M_JusyncCloud",
]

for path in PATHS:
    mat = unreal.EditorAssetLibrary.load_asset(path)
    if not mat:
        unreal.log("[Params] %s MISSING" % path)
        continue
    try:
        floor = MEL.get_material_default_scalar_parameter_value(mat, "EmissiveFloor")
    except Exception as exc:  # noqa: BLE001
        floor = "ERROR:%s" % exc
    try:
        morph_t = MEL.get_material_default_scalar_parameter_value(mat, "JusyncMorphT")
    except Exception:  # noqa: BLE001
        morph_t = None
    two_sided = None
    try:
        two_sided = mat.get_editor_property("two_sided")
    except Exception:  # noqa: BLE001
        pass
    unreal.log("[Params] %s EmissiveFloor=%r JusyncMorphT=%r TwoSided=%r" % (
        path, floor, morph_t, two_sided))
