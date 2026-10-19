# Creates /Game/Materials/M_JusyncMorph: a vertex-color material with the World
# Position Offset morph branch that FJUSYNCMeshMorpher drives for streamed USD meshes.
# Also creates /Game/Materials/M_VertexColor (plain VertexColor -> BaseColor), the
# non-morph color material the subsystem/spawner already tries to load by that path.
#
# Contract with the plugin (see JUSYNCStreamBuilder.cpp / JUSYNCMeshMorpher.cpp):
#   UV1 = (dX, dY), UV2 = (dZ, 0)   per-vertex delta (previous sample - current sample)
#   WPO = (dX, dY, dZ) * (1 - JusyncMorphT) * JusyncMorphActive
#   JusyncMorphT: 0 = previous sample, 1 = current sample (ramped by the morpher each frame)
#   JusyncMorphActive: 1 while a blend is running, 0 otherwise
#
# Run headless:
#   & "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
#     "D:\jsc_github\jusync-uesample\JusyncSampleProject.uproject" `
#     -run=pythonscript -script="D:/jsc_github/jusync-uesample/Tools/CreateJusyncMorphMaterial.py" `
#     -nocompileeditor -stdout -unattended -nopause -nosplash
#
# v2: v1 left a dangling OneMinus node ("Missing 1-x input" at SM6 compile, silently
# swallowed by try/except during connect). Every connection is now read back and must
# verify, or the script exits loudly with FAIL lines.
import unreal

MORPH_PATH = "/Game/Materials/M_JusyncMorph"
VCOL_PATH = "/Game/Materials/M_VertexColor"
MEL = unreal.MaterialEditingLibrary
FAILURES = []


def log(msg):
    unreal.log("[MorphMat] " + msg)


def fail(msg):
    FAILURES.append(msg)
    unreal.log_error("[MorphMat] FAIL: " + msg)


def set_prop(node, name, value):
    try:
        node.set_editor_property(name, value)
        return True
    except Exception as exc:  # noqa: BLE001
        fail("set %s.%s = %r: %s" % (node.get_class().get_name(), name, value, exc))
        return False


def connect(from_node, to_node, out_name, in_name):
    # connect_material_expressions raises on invalid input names, and UE 5.7 protects
    # the raw 'A'/'B'/'Input' slots from Python property access - so a non-raising
    # connect IS the verification. (v1 bug: v1 never got this far; it aborted mid-graph
    # via set_editor_property on protected slots, leaving the dangling OneMinus node.)
    try:
        MEL.connect_material_expressions(from_node, out_name, to_node, in_name)
    except Exception as exc:  # noqa: BLE001
        fail("connect %s['%s'] -> %s['%s'] raised: %s" % (
            from_node.get_class().get_name(), out_name,
            to_node.get_class().get_name(), in_name, exc))
        return False
    log("connected %s -> %s['%s']" % (
        from_node.get_class().get_name(), to_node.get_class().get_name(), in_name))
    return True


def recreate(folder, name):
    path = folder + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        log("deleting stale asset: " + path)
        unreal.EditorAssetLibrary.delete_asset(path)
    unreal.EditorAssetLibrary.make_directory(folder)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        raise RuntimeError("create_asset failed for " + path)
    log("created " + path)
    return mat, path


def expr(mat, cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


def add_emissive_floor(mat, x, y, default_value):
    """VertexColor * EmissiveFloor -> EmissiveColor.

    Kept low: a large mesh surface with a strong self-lit term blooms into a
    halo. 0.05 only lifts the darkest parts; the rest of the brightness comes
    from the (matte) lit base color.
    """
    vcol = expr(mat, unreal.MaterialExpressionVertexColor, x, y - 300)
    floor_param = expr(mat, unreal.MaterialExpressionScalarParameter, x - 300, y)
    set_prop(floor_param, "parameter_name", "EmissiveFloor")
    set_prop(floor_param, "default_value", default_value)
    set_prop(floor_param, "group", "Jusync")
    emul = expr(mat, unreal.MaterialExpressionMultiply, x + 300, y)
    connect(vcol, emul, "", "A")
    connect(floor_param, emul, "", "B")
    MEL.connect_material_property(emul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def make_matte(mat, x, y):
    """Kill the broad specular sheen that reads as "glow" on a big surface."""
    rough = expr(mat, unreal.MaterialExpressionConstant, x, y)
    set_prop(rough, "r", 0.92)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    spec = expr(mat, unreal.MaterialExpressionConstant, x, y + 120)
    set_prop(spec, "r", 0.03)
    MEL.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)


def build_vertex_color():
    mat, path = recreate("/Game/Materials", "M_VertexColor")
    set_prop(mat, "two_sided", True)
    vcol = expr(mat, unreal.MaterialExpressionVertexColor, 0, 0)
    MEL.connect_material_property(vcol, "", unreal.MaterialProperty.MP_BASE_COLOR)
    add_emissive_floor(mat, 0, 400, 0.004)
    make_matte(mat, 0, 700)
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    log(path + " rebuilt")


def build_morph_material():
    mat, path = recreate("/Game/Materials", "M_JusyncMorph")
    set_prop(mat, "two_sided", True)

    # Parameters (names must match FJUSYNCMeshMorpher exactly)
    morph_t = expr(mat, unreal.MaterialExpressionScalarParameter, -800, 400)
    set_prop(morph_t, "parameter_name", "JusyncMorphT")
    set_prop(morph_t, "default_value", 1.0)
    morph_active = expr(mat, unreal.MaterialExpressionScalarParameter, -800, 600)
    set_prop(morph_active, "parameter_name", "JusyncMorphActive")
    set_prop(morph_active, "default_value", 0.0)

    # Delta xyz from packed UVs: TexCoord1=(dX,dY) TexCoord2=(dZ,0)
    tc1 = expr(mat, unreal.MaterialExpressionTextureCoordinate, -800, -100)
    set_prop(tc1, "coordinate_index", 1)
    tc2 = expr(mat, unreal.MaterialExpressionTextureCoordinate, -800, 100)
    set_prop(tc2, "coordinate_index", 2)

    delta4 = expr(mat, unreal.MaterialExpressionAppendVector, -500, 0)
    connect(tc1, delta4, "", "A")
    connect(tc2, delta4, "", "B")
    delta3 = expr(mat, unreal.MaterialExpressionComponentMask, -300, 0)
    set_prop(delta3, "r", True)
    set_prop(delta3, "g", True)
    set_prop(delta3, "b", True)
    connect(delta4, delta3, "", "")

    # Gate scalar: (1 - JusyncMorphT) * JusyncMorphActive.
    # NOTE: UE 5.7 Python binds a node's DEFAULT input only via the EMPTY input name;
    # 'Input'/'input'/'1-x' silently no-op (proved by Tools/test_oneminus_names.py,
    # which compiles clean only with ''). This was the M_JusyncMorph dangling-node bug.
    one_minus = expr(mat, unreal.MaterialExpressionOneMinus, -500, 400)
    connect(morph_t, one_minus, "", "")
    gate = expr(mat, unreal.MaterialExpressionMultiply, -250, 300)
    connect(one_minus, gate, "", "A")
    connect(morph_active, gate, "", "B")

    # WPO = delta * gate
    wpo = expr(mat, unreal.MaterialExpressionMultiply, 0, 100)
    connect(delta3, wpo, "", "A")
    connect(gate, wpo, "", "B")
    MEL.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    # Vertex colors (same look as the LUT-baked path)
    vcol = expr(mat, unreal.MaterialExpressionVertexColor, -200, -400)
    MEL.connect_material_property(vcol, "", unreal.MaterialProperty.MP_BASE_COLOR)
    add_emissive_floor(mat, -200, 800, 0.004)
    make_matte(mat, -200, 1100)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)

    if FAILURES:
        for f in FAILURES:
            unreal.log_error("[MorphMat] UNSOLVED: " + f)
        raise RuntimeError("%d connection/property failure(s) - asset NOT trustworthy" % len(FAILURES))
    log("OK: " + path + " rebuilt; all connections verified by read-back")


build_vertex_color()
build_morph_material()
