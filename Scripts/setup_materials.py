# Creates the project's simple materials (skips any that already exist):
#  /Game/Materials/M_Color        - "Color" vector parameter (ground, trees, props)
#  /Game/Materials/M_VertexColor  - vertex colours x Roughness/Metallic/Emissive params, two-sided
#                                   (procedural car body, wheels, road, kerbs, walls)
#  /Game/Materials/M_Glass        - translucent glass
#  /Game/Materials/M_Smoke        - tyre smoke puffs (translucent, per-instance opacity)
#  /Game/Materials/M_Ground       - vertex colour x world-space grain (road, lot, grass, terrain)
#  /Game/Audio/S_*                - looping car sounds imported from Scripts/Audio/*.wav
import unreal

PATH = "/Game/Materials"
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def scalar(mat, name, value, x, y):
    p = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", value)
    return p


if not unreal.EditorAssetLibrary.does_asset_exist(PATH + "/M_Color"):
    mat = tools.create_asset("M_Color", PATH, unreal.Material, unreal.MaterialFactoryNew())
    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(mat, "Roughness", 0.8, -400, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(PATH + "/M_Color")
    unreal.log("DriftGame: created M_Color")

if not unreal.EditorAssetLibrary.does_asset_exist(PATH + "/M_VertexColor"):
    mat = tools.create_asset("M_VertexColor", PATH, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("two_sided", True)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -600, 0)
    mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(mat, "Roughness", 0.5, -600, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar(mat, "Metallic", 0.0, -600, 300), "", unreal.MaterialProperty.MP_METALLIC)
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 400)
    mel.connect_material_expressions(vc, "", mul, "A")
    mel.connect_material_expressions(scalar(mat, "Emissive", 0.0, -600, 450), "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(PATH + "/M_VertexColor")
    unreal.log("DriftGame: created M_VertexColor")

if not unreal.EditorAssetLibrary.does_asset_exist(PATH + "/M_Glass"):
    mat = tools.create_asset("M_Glass", PATH, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", True)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -600, 0)
    mel.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(mat, "Opacity", 0.3, -600, 200), "", unreal.MaterialProperty.MP_OPACITY)
    mel.connect_material_property(scalar(mat, "Roughness", 0.05, -600, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar(mat, "Metallic", 0.3, -600, 400), "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(PATH + "/M_Glass")
    unreal.log("DriftGame: created M_Glass")

if not unreal.EditorAssetLibrary.does_asset_exist(PATH + "/M_Smoke"):
    # Tyre smoke puffs: lit translucent spheres, soft at the edges, per-instance opacity (custom data 0).
    mat = tools.create_asset("M_Smoke", PATH, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -600, 0)
    col.set_editor_property("constant", unreal.LinearColor(0.78, 0.78, 0.8, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(mat, "Roughness", 1.0, -600, 120), "", unreal.MaterialProperty.MP_ROUGHNESS)
    fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -800, 300)
    fres.set_editor_property("exponent", 1.2)
    fres.set_editor_property("base_reflect_fraction", 0.0)
    soft = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, -600, 300)
    soft.set_editor_property("const_a", 1.0)
    mel.connect_material_expressions(fres, "", soft, "B")
    alpha = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -800, 450)
    alpha.set_editor_property("data_index", 0)
    alpha.set_editor_property("const_default_value", 0.0)
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 350)
    mel.connect_material_expressions(soft, "", mul, "A")
    mel.connect_material_expressions(alpha, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(PATH + "/M_Smoke")
    unreal.log("DriftGame: created M_Smoke")

if not unreal.EditorAssetLibrary.does_asset_exist(PATH + "/M_Ground"):
    # Asphalt, grass and dirt: vertex colour broken up by fine world-space grain (35 cm, 3 octaves) and
    # larger patches (7 m). The grain fades out with distance so it doesn't shimmer. Same parameters as
    # M_VertexColor. Without texture on the ground the eye can't tell how fast it's moving.
    try:
        mat = tools.create_asset("M_Ground", PATH, unreal.Material, unreal.MaterialFactoryNew())
        mat.set_editor_property("two_sided", True)
        vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -900, 0)
        wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1500, 250)

        def noise(scale_cm, levels, lo, hi, y):
            s = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1300, y)
            s.set_editor_property("const_b", 1.0 / scale_cm)
            mel.connect_material_expressions(wp, "", s, "A")
            n = mel.create_material_expression(mat, unreal.MaterialExpressionNoise, -1100, y)
            n.set_editor_property("noise_function", unreal.NoiseFunction.NOISEFUNCTION_VALUE_ALU)
            n.set_editor_property("scale", 1.0)
            n.set_editor_property("levels", levels)
            n.set_editor_property("output_min", lo)
            n.set_editor_property("output_max", hi)
            mel.connect_material_expressions(s, "", n, "Position")
            return n

        fine = noise(35.0, 3, 0.62, 1.3, 200)
        coarse = noise(700.0, 2, 0.8, 1.15, 450)
        detail = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -800, 300)
        mel.connect_material_expressions(fine, "", detail, "A")
        mel.connect_material_expressions(coarse, "", detail, "B")
        depth = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -1100, 650)
        div = mel.create_material_expression(mat, unreal.MaterialExpressionDivide, -900, 650)
        div.set_editor_property("const_b", 9000.0)
        mel.connect_material_expressions(depth, "", div, "A")
        sat = mel.create_material_expression(mat, unreal.MaterialExpressionSaturate, -750, 650)
        mel.connect_material_expressions(div, "", sat, "")
        fade = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -600, 400)
        fade.set_editor_property("const_b", 0.96)
        mel.connect_material_expressions(detail, "", fade, "A")
        mel.connect_material_expressions(sat, "", fade, "Alpha")
        base = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 100)
        mel.connect_material_expressions(vc, "", base, "A")
        mel.connect_material_expressions(fade, "", base, "B")
        mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(scalar(mat, "Roughness", 0.8, -600, 800), "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(scalar(mat, "Metallic", 0.0, -600, 900), "", unreal.MaterialProperty.MP_METALLIC)
        emul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 1000)
        mel.connect_material_expressions(vc, "", emul, "A")
        mel.connect_material_expressions(scalar(mat, "Emissive", 0.0, -600, 1050), "", emul, "B")
        mel.connect_material_property(emul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.recompile_material(mat)
        unreal.EditorAssetLibrary.save_asset(PATH + "/M_Ground")
        unreal.log("DriftGame: created M_Ground")
    except Exception as e:  # the game falls back to M_VertexColor
        unreal.log_warning("DriftGame: M_Ground failed: %s" % e)
        if unreal.EditorAssetLibrary.does_asset_exist(PATH + "/M_Ground"):
            unreal.EditorAssetLibrary.delete_asset(PATH + "/M_Ground")

# Looping sounds for the car (Scripts/Audio/*.wav, synthesised): engine at 1800 and 5000 rpm, wind, tyres.
import os
AUDIO = "/Game/Audio"
AUDIO_SRC = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Audio")
# The NSX has its own V6 loops (Scripts/Audio/make_v6.py).
for wav, asset in [("engine_low", "S_EngineLow"), ("engine_high", "S_EngineHigh"), ("wind", "S_Wind"), ("tyre_squeal", "S_TyreSqueal"),
                   ("engine_low_v6", "S_EngineLowV6"), ("engine_high_v6", "S_EngineHighV6"), ("explosion", "S_Explosion")]:
    path = AUDIO + "/" + asset
    src = os.path.join(AUDIO_SRC, wav + ".wav")
    if unreal.EditorAssetLibrary.does_asset_exist(path) or not os.path.exists(src):
        continue
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", src)
    task.set_editor_property("destination_path", AUDIO)
    task.set_editor_property("destination_name", asset)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    wave = unreal.EditorAssetLibrary.load_asset(path)
    if wave:
        wave.set_editor_property("looping", asset != "S_Explosion")
        unreal.EditorAssetLibrary.save_asset(path)
        unreal.log("DriftGame: imported " + asset)

# Map polish: soft haze for depth, warmer sun. Safe to run more than once.
MAP = "/Game/Maps/L_DriftTrack"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if les.load_level(MAP):
    actors = eas.get_all_level_actors()
    if not any(isinstance(a, unreal.ExponentialHeightFog) for a in actors):
        fog = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
        comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
        comp.set_editor_property("fog_density", 0.008)
        comp.set_editor_property("fog_height_falloff", 0.2)
    for a in actors:
        if isinstance(a, unreal.DirectionalLight):
            a.set_actor_rotation(unreal.Rotator(0, -35, -40), False)
            lc = a.get_component_by_class(unreal.DirectionalLightComponent)
            lc.set_editor_property("intensity", 6.0)
            lc.set_editor_property("light_color", unreal.Color(255, 244, 225, 255))
        # Fully dynamic lighting: nothing to bake, so no "LIGHTING NEEDS TO BE REBUILT" warning.
        if isinstance(a, (unreal.DirectionalLight, unreal.SkyLight)):
            root = a.get_editor_property("root_component")
            if root:
                root.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        if isinstance(a, unreal.SkyLight):
            sc = a.get_component_by_class(unreal.SkyLightComponent)
            if sc:
                sc.set_editor_property("real_time_capture", True)
    les.save_current_level()
    unreal.log("DriftGame: map updated")
