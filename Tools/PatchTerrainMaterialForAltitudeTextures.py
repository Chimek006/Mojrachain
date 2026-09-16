"""Make the biome texture blend altitude-aware.

The first texture patch selected a raw texture only from the biome weight.
That made a Mountain hex use rock everywhere and removed the useful
grass/rock/snow altitude layering. This patch keeps the fixed biome channels,
but applies height masks before the final biome mix.
"""

import unreal


MATERIAL_PATH = "/Game/Materials/M_Terrain_Master_Altitude.M_Terrain_Master_Altitude"
FINAL_DESCRIPTION = "Moirachain_AltitudeAwareBiomeTextureBlend"


def description(expression):
    try:
        return str(expression.get_editor_property("desc"))
    except Exception:
        return ""


def make(material, expression_class, x, y, desc):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        expression_class,
        x,
        y,
    )
    expression.set_editor_property("desc", desc)
    return expression


def connect(source, source_output, target, target_input):
    unreal.MaterialEditingLibrary.connect_material_expressions(
        source,
        source_output,
        target,
        target_input,
    )


def mask(material, source, output, channel, x, y, desc):
    expression = make(material, unreal.MaterialExpressionComponentMask, x, y, desc)
    expression.set_editor_property("r", channel == "R")
    expression.set_editor_property("g", channel == "G")
    expression.set_editor_property("b", channel == "B")
    expression.set_editor_property("a", channel == "A")
    connect(source, output, expression, "")
    return expression


def add(material, a, b, x, y, desc):
    expression = make(material, unreal.MaterialExpressionAdd, x, y, desc)
    connect(a, "", expression, "A")
    connect(b, "", expression, "B")
    return expression


def multiply(material, a, b, x, y, desc):
    expression = make(material, unreal.MaterialExpressionMultiply, x, y, desc)
    connect(a, "", expression, "A")
    connect(b, "", expression, "B")
    return expression


def subtract(material, a, b, x, y, desc):
    expression = make(material, unreal.MaterialExpressionSubtract, x, y, desc)
    connect(a, "", expression, "A")
    connect(b, "", expression, "B")
    return expression


def divide(material, a, b, x, y, desc):
    expression = make(material, unreal.MaterialExpressionDivide, x, y, desc)
    connect(a, "", expression, "A")
    connect(b, "", expression, "B")
    return expression


def saturate(material, source, x, y, desc):
    expression = make(material, unreal.MaterialExpressionSaturate, x, y, desc)
    # Saturate has one unnamed input in UE5's Python material API.
    connect(source, "", expression, "")
    return expression


def constant(material, value, x, y, desc):
    expression = make(material, unreal.MaterialExpressionConstant, x, y, desc)
    expression.set_editor_property("r", value)
    return expression


def lerp(material, a, b, alpha, x, y, desc):
    expression = make(material, unreal.MaterialExpressionLinearInterpolate, x, y, desc)
    connect(a, "", expression, "A")
    connect(b, "", expression, "B")
    connect(alpha, "", expression, "Alpha")
    return expression


def texture(material, parameter_name, asset_path, uv, x, y):
    expression = make(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        x,
        y,
        "Moirachain_AltitudeTexture_" + parameter_name,
    )
    expression.set_editor_property("parameter_name", parameter_name)
    asset = unreal.load_asset(asset_path)
    if asset:
        expression.set_editor_property("texture", asset)
    connect(uv, "", expression, "UVs")
    return expression


material = unreal.load_asset(MATERIAL_PATH)
if not material:
    unreal.log_error("[Moirachain] Terrain material not found: " + MATERIAL_PATH)
else:
    base_property = unreal.MaterialProperty.MP_BASE_COLOR
    old_base = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material,
        base_property,
    )

    if description(old_base) == FINAL_DESCRIPTION:
        unreal.log("[Moirachain] Altitude-aware biome texture blend already exists.")
    elif not old_base:
        unreal.log_error("[Moirachain] Base Color is not connected; altitude blend was not added.")
    else:
        base_output = unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(
            material,
            base_property,
        )

        vertex_color = make(
            material,
            unreal.MaterialExpressionVertexColor,
            980,
            700,
            "Moirachain_AltitudeTexture_VertexColor",
        )
        altitude = mask(
            material,
            vertex_color,
            "",
            "R",
            1200,
            580,
            "Moirachain_AltitudeTexture_Altitude01",
        )
        slope = mask(
            material,
            vertex_color,
            "",
            "G",
            1200,
            760,
            "Moirachain_AltitudeTexture_Slope",
        )

        uv1 = make(material, unreal.MaterialExpressionTextureCoordinate, 980, 940, "Moirachain_AltitudeTexture_UV1")
        uv1.set_editor_property("coordinate_index", 1)
        uv2 = make(material, unreal.MaterialExpressionTextureCoordinate, 980, 1140, "Moirachain_AltitudeTexture_UV2")
        uv2.set_editor_property("coordinate_index", 2)
        uv3 = make(material, unreal.MaterialExpressionTextureCoordinate, 980, 1340, "Moirachain_AltitudeTexture_UV3")
        uv3.set_editor_property("coordinate_index", 3)

        grassland = mask(material, vertex_color, "", "B", 1220, 900, "Moirachain_AltitudeWeight_Grassland")
        forest = mask(material, vertex_color, "", "A", 1220, 980, "Moirachain_AltitudeWeight_Forest")
        hills = mask(material, uv1, "", "R", 1220, 1060, "Moirachain_AltitudeWeight_Hills")
        desert = mask(material, uv1, "", "G", 1220, 1140, "Moirachain_AltitudeWeight_Desert")
        mountain = mask(material, uv2, "", "R", 1220, 1220, "Moirachain_AltitudeWeight_Mountain")
        swamp = mask(material, uv2, "", "G", 1220, 1300, "Moirachain_AltitudeWeight_Swamp")
        tundra = mask(material, uv3, "", "R", 1220, 1380, "Moirachain_AltitudeWeight_Tundra")

        mountain_hills = add(material, mountain, hills, 1440, 1040, "Moirachain_AltitudeWeight_MountainHills")
        cold_weight = add(material, mountain, tundra, 1440, 1220, "Moirachain_AltitudeWeight_Cold")

        rock_start = constant(material, 0.34, 1440, 540, "Moirachain_Altitude_RockStart")
        rock_end = constant(material, 0.62, 1440, 620, "Moirachain_Altitude_RockEnd")
        rock_range = subtract(material, rock_end, rock_start, 1640, 580, "Moirachain_Altitude_RockRange")
        rock_offset = subtract(material, altitude, rock_start, 1640, 700, "Moirachain_Altitude_RockOffset")
        rock_height = saturate(
            material,
            divide(material, rock_offset, rock_range, 1840, 700, "Moirachain_Altitude_RockDivide"),
            2040,
            700,
            "Moirachain_Altitude_RockFactor",
        )
        slope_strength = constant(material, 0.12, 1640, 820, "Moirachain_Altitude_SlopeStrength")
        slope_rock = multiply(material, slope, slope_strength, 1840, 820, "Moirachain_Altitude_SlopeRock")
        rock_factor = add(material, rock_height, slope_rock, 2040, 820, "Moirachain_Altitude_RockFactorWithSlope")
        rock_mask = saturate(
            material,
            multiply(material, rock_factor, mountain_hills, 2240, 820, "Moirachain_Altitude_RockBiomeMask"),
            2440,
            820,
            "Moirachain_Altitude_RockMask",
        )

        snow_start = constant(material, 0.68, 1440, 940, "Moirachain_Altitude_SnowStart")
        snow_end = constant(material, 0.86, 1440, 1020, "Moirachain_Altitude_SnowEnd")
        snow_range = subtract(material, snow_end, snow_start, 1640, 980, "Moirachain_Altitude_SnowRange")
        snow_offset = subtract(material, altitude, snow_start, 1640, 1100, "Moirachain_Altitude_SnowOffset")
        snow_height = saturate(
            material,
            divide(material, snow_offset, snow_range, 1840, 1100, "Moirachain_Altitude_SnowDivide"),
            2040,
            1100,
            "Moirachain_Altitude_SnowFactor",
        )
        snow_mask = saturate(
            material,
            add(material, multiply(material, snow_height, cold_weight, 2240, 1040, "Moirachain_Altitude_SnowBiomeMask"), tundra, 2440, 1040, "Moirachain_Altitude_SnowWithTundra"),
            2640,
            1040,
            "Moirachain_Altitude_SnowMask",
        )

        uv0 = make(material, unreal.MaterialExpressionTextureCoordinate, 980, 1540, "Moirachain_AltitudeTexture_UV0")
        uv0.set_editor_property("coordinate_index", 0)
        grass_texture = texture(material, "GrasslandTexture", "/Game/Materials/grass", uv0, 1660, 1500)
        sand_texture = texture(material, "DesertTexture", "/Game/Materials/sand", uv0, 1660, 1700)
        rock_texture = texture(material, "MountainTexture", "/Game/Materials/metamorphes-gestein-1024x697", uv0, 1660, 1900)
        snow_texture = texture(material, "TundraTexture", "/Game/Materials/zblizona-tekstura-swiezej-bialej-powierzchni-sniegu_181624-56362", uv0, 1660, 2100)

        rock_surface = lerp(material, grass_texture, rock_texture, rock_mask, 2860, 1500, "Moirachain_Altitude_GrassToRock")
        snow_surface = lerp(material, rock_surface, snow_texture, snow_mask, 3060, 1500, "Moirachain_Altitude_RockToSnow")
        desert_surface = lerp(material, snow_surface, sand_texture, desert, 3260, 1500, "Moirachain_Altitude_DesertOverride")

        blend_strength = make(
            material,
            unreal.MaterialExpressionScalarParameter,
            3260,
            1760,
            "Moirachain_AltitudeTextureBlendStrength",
        )
        blend_strength.set_editor_property("parameter_name", "BiomeTextureBlendStrength")
        blend_strength.set_editor_property("default_value", 1.0)

        final_blend = lerp(
            material,
            old_base,
            desert_surface,
            blend_strength,
            3500,
            1500,
            FINAL_DESCRIPTION,
        )
        unreal.MaterialEditingLibrary.connect_material_property(
            final_blend,
            "",
            base_property,
        )
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
        unreal.log("[Moirachain] Added altitude-aware grass/rock/snow/desert texture layers.")
