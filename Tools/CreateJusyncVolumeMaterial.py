# Creates /Game/Materials/M_JusyncVolume, the raymarch material for
# AJUSYNCVolumeActor. One box mesh, one R8 volume render target holding voxel
# density, and a Custom node that integrates the density along the view ray.
#
# The volume texture is wired in as a Custom-node input: the translator declares
# volume inputs as "Texture3D <Name>, SamplerState <Name>Sampler", which is the
# only way a Custom node can sample a texture without a hard-coded sampler.
#
# Blend mode is AlphaComposite, i.e. premultiplied: the node returns already
# transmittance-weighted colour in RGB and 1-transmittance in A.
#
# Scalar/vector parameters: InBoxMin/InBoxMax (world-space raymarch box) and
# InVoxelSize are pushed per frame by AJUSYNCVolumeActor, along with the density
# shaping scalars; the colours and light direction are for tuning in the editor
# (see jusync.Volume.* for the CPU-side knobs).
#
# Run headless:
#   & "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
#     "D:\JSC_Github\jusync-uesample\JusyncSampleProject.uproject" `
#     -run=pythonscript -script="D:/JSC_Github/jusync-uesample/Tools/CreateJusyncVolumeMaterial.py" `
#     -nocompileeditor -stdout -unattended -nopause -nosplash
import unreal

MATERIAL_PATH = "/Game/Materials/M_JusyncVolume"

# VHash/VNoise/VPhase live in the plugin's Shaders directory. A Custom node's
# code is emitted inside a function body, so helper functions cannot be declared
# there; the include lands at file scope instead.
VOLUME_INCLUDE = "/Plugin/JUSYNC/Private/JUSYNCVolume.ush"

MEL = unreal.MaterialEditingLibrary
FAILURES = []

SCALARS = [
    ("InDensity", 2.0),
    ("InThreshold", 0.10),
    ("InContrast", 8.0),
    ("InExtinction", 0.03),
    ("InErosion", 0.0),
    ("InNoiseScale", 16.0),
    ("InAmbient", 0.35),
    # The march's accumulated colour is already albedo x shade x (1 - transmittance), so
    # this is only the exposure knob; 0.03 is what keeps the volume's own emissive below
    # the level's bloom threshold on the current payload.
    ("InGain", 0.03),
    # 1 makes the march report its own internals instead of rendering: R is the
    # brightest density it sampled, G is 1 wherever the box covered the pixel, B is the
    # brightest density after the ramp. Green means the texture arrived empty, magenta
    # means the field was read but the ramp rejected it, white means both worked.
    ("InDebug", 0.0),
]

VECTORS = [
    ("InBoxMin", (0.0, 0.0, 0.0, 0.0)),
    ("InBoxMax", (100.0, 100.0, 100.0, 0.0)),
    ("InVoxelSize", (1.0, 1.0, 1.0, 0.0)),
    ("InLightDir", (0.35, 0.40, 0.85, 0.0)),
]

HLSL = r"""
#define VSTEPS 256

float3 ro = InCamPos;
float3 toPixel = InWorldPos - ro;
float rayLen = max(length(toPixel), 1e-4);
float3 rd = toPixel / rayLen;

float3 dir = rd;
dir.x = abs(dir.x) < 1e-6 ? 1e-6 : dir.x;
dir.y = abs(dir.y) < 1e-6 ? 1e-6 : dir.y;
dir.z = abs(dir.z) < 1e-6 ? 1e-6 : dir.z;
float3 invDir = 1.0 / dir;

float3 boxMin = InBoxMin.rgb;
float3 boxMax = InBoxMax.rgb;

float3 ta = (boxMin - ro) * invDir;
float3 tb = (boxMax - ro) * invDir;
float3 tlo = min(ta, tb);
float3 thi = max(ta, tb);

float tNear = max(max(tlo.x, tlo.y), max(tlo.z, 0.0));
// Deliberately the box's own exit distance, not min(..., rayLen): rayLen is the distance
// to whichever box face shaded this pixel, so keying the segment off it makes the march a
// zero-length integral on the near wall and a full one on the far wall. Debug mode samples
// the same texels either way, which is why it kept rendering while the volume drew nothing.
float tFar = min(min(thi.x, thi.y), thi.z);

if (tFar <= tNear)
{
    return float4(0.0, 0.0, 0.0, 0.0);
}

float3 boxSize = max(boxMax - boxMin, 1e-4);
float3 invBox = 1.0 / boxSize;
float3 voxel = max(InVoxelSize, 1e-4);
float3 texel = voxel * invBox;
float dt = (tFar - tNear) / VSTEPS;

float jitter = VHash(ro * 1.7 + rd * 11.3 + View.GameTime * 0.37);
float3 p = ro + rd * (tNear + dt * jitter);
float3 lightDir = normalize(InLightDir.rgb);

float transmittance = 1.0;
float3 accumulated = 0.0;
float maxRaw = 0.0;
float maxD = 0.0;

[loop]
for (int i = 0; i < VSTEPS; i++)
{
    float3 uvw = saturate((p - boxMin) * invBox);
    float4 vox = VSamp(InVolume, InVolumeSampler, uvw);
    float raw = vox.a;

    if (InErosion > 0.0)
    {
        raw = max(raw - VNoise(uvw * InNoiseScale) * InErosion, 0.0);
    }

    float d = saturate((raw * InDensity - InThreshold) * InContrast);

    if (InDebug > 0.5)
    {
        maxRaw = max(maxRaw, raw);
        maxD = max(maxD, d);
    }

    if (d > 0.004)
    {
        // Central differences of the density channel give the normal that makes the
        // dense core read as solid geometry instead of fog.
        float3 g;
        g.x = VSamp(InVolume, InVolumeSampler, uvw + float3(texel.x, 0.0, 0.0)).a
            - VSamp(InVolume, InVolumeSampler, uvw - float3(texel.x, 0.0, 0.0)).a;
        g.y = VSamp(InVolume, InVolumeSampler, uvw + float3(0.0, texel.y, 0.0)).a
            - VSamp(InVolume, InVolumeSampler, uvw - float3(0.0, texel.y, 0.0)).a;
        g.z = VSamp(InVolume, InVolumeSampler, uvw + float3(0.0, 0.0, texel.z)).a
            - VSamp(InVolume, InVolumeSampler, uvw - float3(0.0, 0.0, texel.z)).a;

        // A plateau in the normalised field - the hottest voxels are clipped to 255 - has
        // an exactly zero gradient, and normalize(0) is NaN. One NaN writes through both
        // the colour and the opacity of the whole pixel, which looks identical to the
        // volume never drawing. Shading a plateau as if it faced the light keeps the
        // dense core solid instead of poisoning it.
        float gradient = length(g);
        float3 n = gradient > 1e-5 ? -normalize(g / voxel) : lightDir;
        float ndl = saturate(dot(n, lightDir));
        float shade = min(InAmbient + (1.0 - InAmbient) * ndl, 1.0);

        // The payload's own per-point colour, so the point cloud and the volume are
        // two views of the same values instead of two different colourings.
        float3 albedo = vox.rgb;

        float sigma = d * InExtinction;
        accumulated += transmittance * albedo * shade * sigma * dt;
        transmittance *= exp(-sigma * dt);

        if (transmittance < 0.01)
        {
            break;
        }
    }

    p += rd * dt;
}

if (InDebug > 2.5)
{
    float seg = saturate((tFar - tNear) / 300.0);
    return float4(seg, 1.0 - seg, 0.0, 1.0);
}

if (InDebug > 1.5)
{
    return float4(saturate(accumulated), 1.0);
}

if (InDebug > 0.5)
{
    return float4(maxRaw, 1.0, maxD, 1.0);
}

return float4(accumulated * InGain, 1.0 - transmittance);
"""


def log(msg):
    unreal.log("[VolumeMat] " + msg)


def fail(msg):
    FAILURES.append(msg)
    unreal.log_error("[VolumeMat] FAIL: " + msg)


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
    return True


def expr(mat, cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


# The translator types a texture parameter from its default asset
# (UTexture::GetMaterialType), so a texture-object parameter needs an actual
# VolumeTexture assigned or it compiles as Texture2D. The actor replaces it at
# runtime; this placeholder is never sampled in game.
def default_volume_texture():
    path = "/Game/Materials/T_JusyncVolumeDefault"
    existing = unreal.EditorAssetLibrary.load_asset(path)
    if existing:
        return existing

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = tools.create_asset("T_JusyncVolumeDefault", "/Game/Materials",
                                 unreal.VolumeTexture, unreal.VolumeTextureFactory())
    unreal.EditorAssetLibrary.save_asset(path)
    return created


def verify_shader():
    # Compiling through the plugin because the asset's own recompile is async
    # and this commandlet would exit before the jobs finish.
    msg = unreal.JUSYNCBlueprintLibrary.compile_material_now(MATERIAL_PATH)
    if msg is None:
        return ["compile_material_now returned None"]
    return [line for line in str(msg).splitlines() if line.strip()]


def build_volume_material():
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH):
        log("deleting stale asset: " + MATERIAL_PATH)
        unreal.EditorAssetLibrary.delete_asset(MATERIAL_PATH)

    unreal.EditorAssetLibrary.make_directory("/Game/Materials")
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset("M_JusyncVolume", "/Game/Materials",
                             unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        raise RuntimeError("create_asset failed for " + MATERIAL_PATH)

    # Blend mode must be set before connections are made.
    set_prop(mat, "blend_mode", unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
    set_prop(mat, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    # One march per pixel: two-sided would draw the box's back faces too, marching the
    # same segment twice and doubling both the emitted colour and the soft band.
    set_prop(mat, "two_sided", False)

    sources = []

    world = expr(mat, unreal.MaterialExpressionWorldPosition, -1100, -700)
    sources.append(("InWorldPos", world))

    camera = expr(mat, unreal.MaterialExpressionCameraPositionWS, -1100, -600)
    sources.append(("InCamPos", camera))

    volume = expr(mat, unreal.MaterialExpressionTextureObjectParameter, -1100, -500)
    set_prop(volume, "parameter_name", "VolumeTex")
    set_prop(volume, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    set_prop(volume, "texture", default_volume_texture())
    set_prop(volume, "group", "Jusync Volume")
    sources.append(("InVolume", volume))

    y = -400
    for name, value in SCALARS:
        node = expr(mat, unreal.MaterialExpressionScalarParameter, -1100, y)
        set_prop(node, "parameter_name", name)
        set_prop(node, "default_value", value)
        set_prop(node, "group", "Jusync Volume")
        sources.append((name, node))
        y += 80

    for name, value in VECTORS:
        node = expr(mat, unreal.MaterialExpressionVectorParameter, -1100, y)
        set_prop(node, "parameter_name", name)
        set_prop(node, "default_value", unreal.LinearColor(value[0], value[1], value[2], value[3]))
        set_prop(node, "group", "Jusync Volume")
        sources.append((name, node))
        y += 80

    custom = expr(mat, unreal.MaterialExpressionCustom, -500, -200)
    set_prop(custom, "description", "JusyncVolumeMarch")
    set_prop(custom, "code", HLSL)
    set_prop(custom, "output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    set_prop(custom, "include_file_paths", [VOLUME_INCLUDE])
    log("include: %r" % (list(custom.get_editor_property("include_file_paths")),))

    pins = []
    for name, _ in sources:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    set_prop(custom, "inputs", pins)

    for name, node in sources:
        connect(node, custom, "", name)

    # Custom node returns float4: rgb is premultiplied colour, a is coverage.
    rgb = expr(mat, unreal.MaterialExpressionComponentMask, -200, -300)
    set_prop(rgb, "r", True)
    set_prop(rgb, "g", True)
    set_prop(rgb, "b", True)
    set_prop(rgb, "a", False)
    connect(custom, rgb, "", "")
    MEL.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    alpha = expr(mat, unreal.MaterialExpressionComponentMask, -200, -100)
    set_prop(alpha, "r", False)
    set_prop(alpha, "g", False)
    set_prop(alpha, "b", False)
    set_prop(alpha, "a", True)
    connect(custom, alpha, "", "")
    MEL.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)

    # This asset and AJUSYNCVolumeActor have drifted before now, so check the settings
    # the renderer depends on instead of trusting set_prop to have stuck.
    for prop_name, want in (("blend_mode", unreal.BlendMode.BLEND_ALPHA_COMPOSITE),
                            ("shading_model", unreal.MaterialShadingModel.MSM_UNLIT),
                            ("two_sided", False)):
        got = mat.get_editor_property(prop_name)
        if got != want:
            fail("%s is %s, expected %s" % (prop_name, got, want))

    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(MATERIAL_PATH)

    errors = verify_shader()
    if errors and errors[0].startswith("Renderer not initialized"):
        log("shader compile skipped: commandlets have no renderer; "
            "run Tools/CompileJusyncVolumeMaterial.py to compile it")
    elif errors:
        for err in errors:
            unreal.log_error("[VolumeMat] shader: " + err)
        fail("shader did not compile")
    else:
        log("shader compiled")
    if FAILURES:
        for f in FAILURES:
            unreal.log_error("[VolumeMat] UNSOLVED: " + f)
        raise RuntimeError("%d failure(s); asset not usable" % len(FAILURES))
    log("OK: " + MATERIAL_PATH + " built (AlphaComposite, unlit, raymarch)")


build_volume_material()
