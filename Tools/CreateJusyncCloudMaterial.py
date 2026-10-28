# Creates /Game/Materials/M_JusyncCloud, the non-emissive point material for
# streamed LidarPointCloud components. The plugin only ships emissive materials
# (M_LidarPointCloud / _Masked: Translucent + EmissiveColor + Gain > 1 bloom),
# which is what made streamed clouds glow. This one is Masked shading with:
#   BaseColor   = VertexColor (lit)
#   Emissive    = VertexColor * EmissiveFloor param (<=1 stays under bloom)
#   OpacityMask = lerp(1, point disc, PointShapeCircle) * point presence
#                 The material graph's VertexColor node is float3 (no alpha),
#                 so the Lidar VS's shape coverage cannot be read here. The
#                 spawner pushes PointShapeCircle (0 = square, 1 = circle) from
#                 the SHAPE buttons instead.
#                 The hue filter cannot use bVisible: the Lidar octree caches
#                 NumVisiblePoints and does not rebuild it for in-place point
#                 edits. Non-matching points are therefore painted black and
#                 cut out here by a huge color-presence gain; any non-zero
#                 source color survives the mask.
#
# Requires the bUsedWithLidarPointCloud usage flag - the Lidar vertex factory only
# compiles permutations for materials carrying it
# (FLidarPointCloudVertexFactoryBase::ShouldCompilePermutation).
#
# Consumed via jusync.Spawner.CloudMaterial (default /Game/Materials/M_JusyncCloud;
# empty CVar = plugin default emissive material).
#
# Run headless:
#   & "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
#     "D:\jsc_github\jusync-uesample\JusyncSampleProject.uproject" `
#     -run=pythonscript -script="D:/jsc_github/jusync-uesample/Tools/CreateJusyncCloudMaterial.py" `
#     -nocompileeditor -stdout -unattended -nopause -nosplash
import unreal

CLOUD_PATH = "/Game/Materials/M_JusyncCloud"
MEL = unreal.MaterialEditingLibrary
FAILURES = []


def log(msg):
    unreal.log("[CloudMat] " + msg)


def fail(msg):
    FAILURES.append(msg)
    unreal.log_error("[CloudMat] FAIL: " + msg)


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
        # UE returns False (no exception) for a bad input name, leaving a dangling pin.
        fail("connect %s['%s'] -> %s['%s'] returned %r (NOT connected)" % (
            from_node.get_class().get_name(), out_name,
            to_node.get_class().get_name(), in_name, ret))
        return False
    log("connected %s -> %s['%s']" % (
        from_node.get_class().get_name(), to_node.get_class().get_name(), in_name))
    return True


def expr(mat, cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


def build_cloud_material():
    if unreal.EditorAssetLibrary.does_asset_exist(CLOUD_PATH):
        log("deleting stale asset: " + CLOUD_PATH)
        unreal.EditorAssetLibrary.delete_asset(CLOUD_PATH)
    unreal.EditorAssetLibrary.make_directory("/Game/Materials")
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset("M_JusyncCloud", "/Game/Materials",
                             unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        raise RuntimeError("create_asset failed for " + CLOUD_PATH)

    # Blend mode must be set before connections are made.
    set_prop(mat, "blend_mode", unreal.BlendMode.BLEND_MASKED)

    # VF usage gate. UE Python drops the 'b' prefix on bool properties.
    if not set_prop(mat, "used_with_lidar_point_cloud", True):
        fail("could not set the LidarPointCloud usage flag; this asset will not "
             "render on ULidarPointCloudComponent")

    # Base color from the LUT-baked vertex colors.
    vcol = expr(mat, unreal.MaterialExpressionVertexColor, -300, -300)
    MEL.connect_material_property(vcol, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Emissive floor: camera-facing sprites receive almost no scene light, so a purely
    # lit cloud reads as flat gray. A small self-lit term keeps colors readable without
    # reaching the bloom threshold. Value is pushed by the spawner from
    # jusync.Spawner.CloudEmissiveFloor.
    floor_param = expr(mat, unreal.MaterialExpressionScalarParameter, -600, -100)
    set_prop(floor_param, "parameter_name", "EmissiveFloor")
    set_prop(floor_param, "default_value", 0.004)
    set_prop(floor_param, "group", "Jusync")
    vcol2 = expr(mat, unreal.MaterialExpressionVertexColor, -600, -500)
    emul = expr(mat, unreal.MaterialExpressionMultiply, -300, -100)
    connect(vcol2, emul, "", "A")
    connect(floor_param, emul, "", "B")
    MEL.connect_material_property(emul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # Point disc mask from the sprite-corner UV (TexCoord0, written by the Lidar VS).
    # Distance needs both inputs the same width, hence the float2 center constant.
    tc = expr(mat, unreal.MaterialExpressionTextureCoordinate, -600, 100)
    set_prop(tc, "coordinate_index", 0)
    center = expr(mat, unreal.MaterialExpressionConstant2Vector, -600, 300)
    set_prop(center, "r", 0.5)
    set_prop(center, "g", 0.5)
    dist = expr(mat, unreal.MaterialExpressionDistance, -400, 100)
    connect(tc, dist, "", "A")
    connect(center, dist, "", "B")
    radius = expr(mat, unreal.MaterialExpressionConstant, -600, 500)
    set_prop(radius, "r", 0.5)
    sub = expr(mat, unreal.MaterialExpressionSubtract, -400, 400)
    connect(radius, sub, "", "A")
    connect(dist, sub, "", "B")
    # Steep gain against clip 0.5 gives a hard disc edge.
    gain = expr(mat, unreal.MaterialExpressionConstant, -400, 600)
    set_prop(gain, "r", 64.0)
    disc = expr(mat, unreal.MaterialExpressionMultiply, -200, 400)
    connect(sub, disc, "", "A")
    connect(gain, disc, "", "B")

    # Shape selector, pushed by FJUSYNCPointCloudSpawner::ApplyVisualSettings:
    # 0 = keep the full quad (square), 1 = clip to the disc (circle).
    shape_circle = expr(mat, unreal.MaterialExpressionScalarParameter, -600, 800)
    set_prop(shape_circle, "parameter_name", "PointShapeCircle")
    set_prop(shape_circle, "default_value", 1.0)
    set_prop(shape_circle, "group", "Jusync")
    one = expr(mat, unreal.MaterialExpressionConstant, -400, 800)
    set_prop(one, "r", 1.0)
    shape = expr(mat, unreal.MaterialExpressionLinearInterpolate, -200, 700)
    connect(one, shape, "", "A")
    connect(disc, shape, "", "B")
    connect(shape_circle, shape, "", "Alpha")

    # Presence mask: non-matching points are painted pure black by the spawner.
    # Dot with white sums the RGB channels; x256 turns any non-zero 8-bit
    # component into >= 1, so only exactly-black points fall below the 0.5 clip.
    white = expr(mat, unreal.MaterialExpressionConstant3Vector, -600, 1000)
    set_prop(white, "constant", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    presence = expr(mat, unreal.MaterialExpressionDotProduct, -400, 1000)
    connect(vcol, presence, "", "A")
    connect(white, presence, "", "B")
    presence_gain = expr(mat, unreal.MaterialExpressionConstant, -400, 1150)
    set_prop(presence_gain, "r", 256.0)
    present_scaled = expr(mat, unreal.MaterialExpressionMultiply, -200, 1000)
    connect(presence, present_scaled, "", "A")
    connect(presence_gain, present_scaled, "", "B")

    opacity = expr(mat, unreal.MaterialExpressionMultiply, 0, 850)
    connect(shape, opacity, "", "A")
    connect(present_scaled, opacity, "", "B")
    MEL.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    set_prop(mat, "opacity_mask_clip_value", 0.5)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(CLOUD_PATH)

    if FAILURES:
        for f in FAILURES:
            unreal.log_error("[CloudMat] UNSOLVED: " + f)
        raise RuntimeError("%d failure(s); asset not usable" % len(FAILURES))
    log("OK: " + CLOUD_PATH + " built (Masked, lit, LidarVF usage set, no emissive)")


build_cloud_material()
