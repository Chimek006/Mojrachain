"""Create the first procedural terrain material used by AProceduralTerrainActor.

Run from Unreal Editor with the Python plugin enabled:
    UnrealEditor.exe Mojrachain.uproject -ExecutePythonScript="Tools/CreateTerrainMaterial.py"
"""

import unreal


MATERIAL_PATH = "/Game/Materials"
MATERIAL_NAME = "M_Terrain_Master_Altitude"
REBUILD_MATERIAL = False


def make_expression(material, expression_class, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        expression_class,
        x,
        y,
    )
    return expression


def set_parameter_name(expression, name):
    expression.set_editor_property("parameter_name", name)


def create_master_material():
    unreal.EditorAssetLibrary.make_directory(MATERIAL_PATH)

    asset_path = f"{MATERIAL_PATH}/{MATERIAL_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        existing = unreal.EditorAssetLibrary.load_asset(asset_path)
        if existing and not REBUILD_MATERIAL:
            unreal.MaterialEditingLibrary.recompile_material(existing)
            unreal.EditorAssetLibrary.save_loaded_asset(existing)
            unreal.log(f"Terrain material already exists: {asset_path}")
            return existing
        unreal.log(f"Replacing generated terrain material: {asset_path}")
        unreal.EditorAssetLibrary.delete_asset(asset_path)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(
        MATERIAL_NAME,
        MATERIAL_PATH,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )

    material.set_editor_property("two_sided", False)

    world_position = make_expression(
        material,
        unreal.MaterialExpressionWorldPosition,
        -1000,
        0,
    )

    pattern_scale = make_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        -1000,
        180,
    )
    set_parameter_name(pattern_scale, "PatternScale")
    pattern_scale.set_editor_property("default_value", 0.0028)

    scaled_position = make_expression(
        material,
        unreal.MaterialExpressionMultiply,
        -760,
        80,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        world_position,
        "",
        scaled_position,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        pattern_scale,
        "",
        scaled_position,
        "B",
    )

    noise = make_expression(
        material,
        unreal.MaterialExpressionNoise,
        -520,
        80,
    )
    noise.set_editor_property("scale", 1.0)
    noise.set_editor_property("quality", 1)
    noise.set_editor_property("levels", 3)
    noise.set_editor_property("output_min", 0.0)
    noise.set_editor_property("output_max", 1.0)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        scaled_position,
        "",
        noise,
        "Position",
    )

    surface_color = make_expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        -520,
        -160,
    )
    set_parameter_name(surface_color, "SurfaceColor")
    surface_color.set_editor_property(
        "default_value",
        unreal.LinearColor(0.16, 0.38, 0.055, 1.0),
    )

    accent_color = make_expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        -520,
        -320,
    )
    set_parameter_name(accent_color, "AccentColor")
    accent_color.set_editor_property(
        "default_value",
        unreal.LinearColor(0.40, 0.68, 0.12, 1.0),
    )

    color_blend = make_expression(
        material,
        unreal.MaterialExpressionLinearInterpolate,
        -160,
        -40,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        surface_color,
        "",
        color_blend,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        accent_color,
        "",
        color_blend,
        "B",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        noise,
        "",
        color_blend,
        "Alpha",
    )

    rock_color = make_expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        40,
        -300,
    )
    set_parameter_name(rock_color, "RockColor")
    rock_color.set_editor_property(
        "default_value",
        unreal.LinearColor(0.30, 0.31, 0.30, 1.0),
    )

    snow_color = make_expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        40,
        -460,
    )
    set_parameter_name(snow_color, "SnowColor")
    snow_color.set_editor_property(
        "default_value",
        unreal.LinearColor(0.92, 0.96, 1.0, 1.0),
    )

    vertex_color = make_expression(
        material,
        unreal.MaterialExpressionVertexColor,
        -520,
        600,
    )

    altitude_mask = make_expression(
        material,
        unreal.MaterialExpressionComponentMask,
        -320,
        540,
    )
    altitude_mask.set_editor_property("r", True)
    altitude_mask.set_editor_property("g", False)
    altitude_mask.set_editor_property("b", False)
    altitude_mask.set_editor_property("a", False)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        vertex_color,
        "R",
        altitude_mask,
        "",
    )

    slope_mask = make_expression(
        material,
        unreal.MaterialExpressionComponentMask,
        -320,
        700,
    )
    slope_mask.set_editor_property("r", False)
    slope_mask.set_editor_property("g", True)
    slope_mask.set_editor_property("b", False)
    slope_mask.set_editor_property("a", False)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        vertex_color,
        "G",
        slope_mask,
        "",
    )

    rock_start = make_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        -320,
        860,
    )
    set_parameter_name(rock_start, "RockStart")
    rock_start.set_editor_property("default_value", 0.55)

    rock_end = make_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        -320,
        1020,
    )
    set_parameter_name(rock_end, "RockEnd")
    rock_end.set_editor_property("default_value", 0.78)

    rock_range = make_expression(
        material,
        unreal.MaterialExpressionSubtract,
        -80,
        980,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        rock_end,
        "",
        rock_range,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        rock_start,
        "",
        rock_range,
        "B",
    )

    rock_height_offset = make_expression(
        material,
        unreal.MaterialExpressionSubtract,
        -80,
        760,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        altitude_mask,
        "",
        rock_height_offset,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        rock_start,
        "",
        rock_height_offset,
        "B",
    )

    rock_height_mask = make_expression(
        material,
        unreal.MaterialExpressionDivide,
        140,
        760,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        rock_height_offset,
        "",
        rock_height_mask,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        rock_range,
        "",
        rock_height_mask,
        "B",
    )

    rock_height_saturated = make_expression(
        material,
        unreal.MaterialExpressionSaturate,
        360,
        760,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        rock_height_mask,
        "",
        rock_height_saturated,
        "",
    )

    slope_strength = make_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        -80,
        1160,
    )
    set_parameter_name(slope_strength, "SlopeRockStrength")
    slope_strength.set_editor_property("default_value", 0.45)

    slope_rock = make_expression(
        material,
        unreal.MaterialExpressionMultiply,
        140,
        980,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        slope_mask,
        "",
        slope_rock,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        slope_strength,
        "",
        slope_rock,
        "B",
    )

    rock_mask = make_expression(
        material,
        unreal.MaterialExpressionMax,
        380,
        900,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        rock_height_saturated,
        "",
        rock_mask,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        slope_rock,
        "",
        rock_mask,
        "B",
    )

    rock_blend = make_expression(
        material,
        unreal.MaterialExpressionLinearInterpolate,
        660,
        0,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        color_blend,
        "",
        rock_blend,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        rock_color,
        "",
        rock_blend,
        "B",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        rock_mask,
        "",
        rock_blend,
        "Alpha",
    )

    snow_start = make_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        140,
        1220,
    )
    set_parameter_name(snow_start, "SnowStart")
    snow_start.set_editor_property("default_value", 0.82)

    snow_end = make_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        360,
        1220,
    )
    set_parameter_name(snow_end, "SnowEnd")
    snow_end.set_editor_property("default_value", 0.96)

    snow_range = make_expression(
        material,
        unreal.MaterialExpressionSubtract,
        560,
        1120,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_end,
        "",
        snow_range,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_start,
        "",
        snow_range,
        "B",
    )

    snow_offset = make_expression(
        material,
        unreal.MaterialExpressionSubtract,
        560,
        940,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        altitude_mask,
        "",
        snow_offset,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_start,
        "",
        snow_offset,
        "B",
    )

    snow_mask_divided = make_expression(
        material,
        unreal.MaterialExpressionDivide,
        760,
        940,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_offset,
        "",
        snow_mask_divided,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_range,
        "",
        snow_mask_divided,
        "B",
    )

    snow_mask = make_expression(
        material,
        unreal.MaterialExpressionSaturate,
        940,
        940,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_mask_divided,
        "",
        snow_mask,
        "",
    )

    snow_blend = make_expression(
        material,
        unreal.MaterialExpressionLinearInterpolate,
        960,
        0,
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        rock_blend,
        "",
        snow_blend,
        "A",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_color,
        "",
        snow_blend,
        "B",
    )
    unreal.MaterialEditingLibrary.connect_material_expressions(
        snow_mask,
        "",
        snow_blend,
        "Alpha",
    )

    roughness = make_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        -160,
        220,
    )
    set_parameter_name(roughness, "SurfaceRoughness")
    roughness.set_editor_property("default_value", 0.86)

    flat_normal = make_expression(
        material,
        unreal.MaterialExpressionConstant3Vector,
        -160,
        380,
    )
    flat_normal.set_editor_property(
        "constant",
        unreal.LinearColor(0.5, 0.5, 1.0, 1.0),
    )

    unreal.MaterialEditingLibrary.connect_material_property(
        snow_blend,
        "",
        unreal.MaterialProperty.MP_BASE_COLOR,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness,
        "",
        unreal.MaterialProperty.MP_ROUGHNESS,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        flat_normal,
        "",
        unreal.MaterialProperty.MP_NORMAL,
    )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"Created procedural terrain material: {asset_path}")
    return material


create_master_material()
