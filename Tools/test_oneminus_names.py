# OneMinus input-name binding test (GUI editor run only: -ExecutePythonScript).
# Each material feeds OneMinus -> WPO; if the candidate input name failed to bind,
# the live-editor SM compile validator logs, prefixed with that asset's path:
#   (Node OneMinus) Missing 1-x input
# Numbered, collision-free names. Nothing saved permanently (saved, but deleted after).
import unreal

MEL = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# label -> connect input name (None = leave deliberately dangling as the control)
CASES = [
    ("t_om0_dangle", None),
    ("t_om1_empty", ""),
    ("t_om2_Input", "Input"),
    ("t_om3_input_lc", "input"),
    ("t_om4_dash", "1-x"),
]

for name, in_name in CASES:
    path = "/Game/Materials/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    mat = tools.create_asset(name, "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    p = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, 0, 0)
    om = MEL.create_material_expression(mat, unreal.MaterialExpressionOneMinus, 200, 0)
    if in_name is not None:
        try:
            MEL.connect_material_expressions(p, "", om, in_name)
            unreal.log("[OMTest] %s: connect('%s') returned without raising" % (name, in_name))
        except Exception as exc:  # noqa: BLE001
            unreal.log("[OMTest] %s: connect('%s') RAISED: %s" % (name, in_name, str(exc)[:100]))
    else:
        unreal.log("[OMTest] %s: control, left dangling on purpose" % name)
    MEL.connect_material_property(om, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    unreal.EditorAssetLibrary.save_asset(path)
    MEL.recompile_material(mat)

unreal.log("[OMTest] all cases recompiled; look for '(Node OneMinus)' lines naming each asset")
