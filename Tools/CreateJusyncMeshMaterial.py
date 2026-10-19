# Creates /Game/Materials/M_JusyncMesh, the material used for streamed USD
# meshes that carry baked vertex colors.
#
# M_VertexColor is plain DefaultLit with VertexColor -> BaseColor, so a streamed
# mesh only shows what the scene lights hit and reads much darker than the
# streamed point cloud (M_JusyncCloud adds an emissive floor). This material
# mirrors the cloud look:
#   BaseColor   = VertexColor (lit, keeps some shape shading)
#   Emissive    = VertexColor * EmissiveFloor (default 0.05: subtle lift, no halo)
#   Two sided   = on (the morph tube is closed, but winding from VTK is not
#                 guaranteed to match UE's front-face convention)
#
# Consumed by JUSYNCFileSpawnerActor::BakeLUTVertexColorCompact, with
# M_VertexColor as the fallback when this asset does not exist.
#
# Run headless (editor closed):
#   & "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
#     "D:\JSC_Github\jusync-uesample\JusyncSampleProject.uproject" `
#     -run=pythonscript -script="D:/JSC_Github/jusync-uesample/Tools/CreateJusyncMeshMaterial.py" `
#     -nocompileeditor -stdout -unattended -nopause -nosplash
import unreal

MESH_PATH = "/Game/Materials/M_JusyncMesh"
MEL = unreal.MaterialEditingLibrary
FAILURES = []


def log(msg):
    unreal.log("[MeshMat] " + msg)


def fail(msg):
    FAILURES.append(msg)
    unreal.log_error("[MeshMat] FAIL: " + msg)


def set_prop(node, name, value):
    try:
        node.set_editor_property(name, value)
        return True
    except Exception as exc:  # noqa: BLE001
        fail("set %s.%s = %r: %s" % (node.get_class().get_name(), name, value, exc))
        return False


def connect(from_node, to_node, out_name, in_name):
    try:
        ret = MEL.connect_material_expressions(from_node, out_name, to_node, in_name)
    except Exception as exc:  # noqa: BLE001
        fail("connect %s['%s'] -> %s['%s'] raised: %s" % (
            from_node.get_class().get_name(), out_name,
            to_node.get_class().get_name(), in_name, exc))
        return False
    if ret is not True:
        fail("connect %s['%s'] -> %s['%s'] returned %r (NOT connected)" % (
            from_node.get_class().get_name(), out_name,
            to_node.get_class().get_name(), in_name, ret))
        return False
    log("connected %s -> %s['%s']" % (
        from_node.get_class().get_name(), to_node.get_class().get_name(), in_name))
    return True


def expr(mat, cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


def build_mesh_material():
    if unreal.EditorAssetLibrary.does_asset_exist(MESH_PATH):
        log("deleting stale asset: " + MESH_PATH)
        unreal.EditorAssetLibrary.delete_asset(MESH_PATH)
    unreal.EditorAssetLibrary.make_directory("/Game/Materials")
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset("M_JusyncMesh", "/Game/Materials",
                             unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        raise RuntimeError("create_asset failed for " + MESH_PATH)

    set_prop(mat, "blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    set_prop(mat, "two_sided", True)

    # Lit base color from the baked vertex colors.
    vcol = expr(mat, unreal.MaterialExpressionVertexColor, -300, -300)
    MEL.connect_material_property(vcol, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Small self-lit lift so the surface is not black in the dim scene, but low
    # enough that bloom does not turn the whole mesh into a halo.
    floor_param = expr(mat, unreal.MaterialExpressionScalarParameter, -600, -100)
    set_prop(floor_param, "parameter_name", "EmissiveFloor")
    set_prop(floor_param, "default_value", 0.004)
    set_prop(floor_param, "group", "Jusync")
    vcol2 = expr(mat, unreal.MaterialExpressionVertexColor, -600, -500)
    emul = expr(mat, unreal.MaterialExpressionMultiply, -300, -100)
    connect(vcol2, emul, "", "A")
    connect(floor_param, emul, "", "B")
    MEL.connect_material_property(emul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # Matte: the default 0.5 roughness gives a big two-sided surface a broad
    # specular sheen that reads as "glowing" even with no emissive floor.
    rough = expr(mat, unreal.MaterialExpressionConstant, -600, -700)
    set_prop(rough, "r", 0.92)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = expr(mat, unreal.MaterialExpressionConstant, -600, -800)
    set_prop(spec, "r", 0.03)
    MEL.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(MESH_PATH)

    if FAILURES:
        for f in FAILURES:
            unreal.log_error("[MeshMat] UNSOLVED: " + f)
        raise RuntimeError("%d failure(s); asset not usable" % len(FAILURES))
    log("OK: " + MESH_PATH + " built (opaque, lit + emissive floor, two sided)")


build_mesh_material()
