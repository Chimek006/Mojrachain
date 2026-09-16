"""Blend the actual biome textures using the fixed biome weights written by C++."""

import unreal


MATERIAL_PATH = "/Game/Materials/M_Terrain_Master_Altitude.M_Terrain_Master_Altitude"
FINAL_DESCRIPTION = "Moirachain_BiomeTextureBlend"


def desc(expression):
    try:
        return str(expression.get_editor_property("desc"))
    except Exception:
        return ""


def make(material, expression_class, x, y, description):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        expression_class,
        x,
        y,
    )
    expression.set_editor_property("desc", description)
    return expression


def mask(material, source, output, channel, x, y, description):
    expression = make(
        material,
        unreal.MaterialExpressionComponentMask,
        x,
        y,
        description,
    )
    expression.set_editor_property("r", channel == "R")
    expression.set_editor_property("g", channel == "G")
    expression.set_editor_property("b", channel == "B")
    expression.set_editor_property("a", channel == "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(
        source,
        output,
        expression,
        "None",
    )
    return expression


def add(material, a, b, x, y, description):
    expression = make(material, unreal.MaterialExpressionAdd, x, y, description)
    unreal.MaterialEditingLibrary.connect_material_expressions(a, "", expression, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(b, "", expression, "B")
    return expression


def multiply(material, a, b, x, y, description):
    expression = make(material, unreal.MaterialExpressionMultiply, x, y, description)
    unreal.MaterialEditingLibrary.connect_material_expressions(a, "", expression, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(b, "", expression, "B")
    return expression


def texture_sample(material, parameter_name, texture_path, uv, x, y):
    expression = make(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        x,
        y,
        "Moirachain_BiomeTexture_" + parameter_name,
    )
    expression.set_editor_property("parameter_name", parameter_name)
    texture = unreal.load_asset(texture_path)
    if texture:
        expression.set_editor_property("texture", texture)
    unreal.MaterialEditingLibrary.connect_material_expressions(
        uv,
        "",
        expression,
        "UVs",
    )
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

    if desc(old_base) == FINAL_DESCRIPTION:
        unreal.log("[Moirachain] Biome texture blend already exists; no graph change was made.")
    elif not old_base:
        unreal.log_error("[Moirachain] Base Color is not connected; biome textures were not added.")
    else:
        base_output = unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(
            material,
            base_property,
        )

        # The previous UV tint node is no longer a color tint: UV1/UV2 now
        # carry biome weights. Neutralize it so the original material remains
        # available as the fallback/altitude layer.
        neutral = make(
            material,
            unreal.MaterialExpressionVectorParameter,
            1180,
            -40,
            "Moirachain_BiomeTint_Neutral",
        )
        neutral.set_editor_property("parameter_name", "BiomeTintNeutral")
        neutral.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
        if isinstance(old_base, unreal.MaterialExpressionMultiply):
            unreal.MaterialEditingLibrary.connect_material_expressions(
                neutral,
                "",
                old_base,
                "B",
            )

        vertex_color = make(
            material,
            unreal.MaterialExpressionVertexColor,
            1000,
            700,
            "Moirachain_BiomeTexture_VertexColor",
        )
        weight0 = mask(material, vertex_color, "B", "B", 1220, 620, "Moirachain_BiomeWeight_Grassland")
        weight1 = mask(material, vertex_color, "A", "A", 1220, 700, "Moirachain_BiomeWeight_Forest")

        uv1 = make(material, unreal.MaterialExpressionTextureCoordinate, 1000, 900, "Moirachain_BiomeWeight_UV1")
        uv1.set_editor_property("coordinate_index", 1)
        weight2 = mask(material, uv1, "", "R", 1220, 860, "Moirachain_BiomeWeight_Hills")
        weight3 = mask(material, uv1, "", "G", 1220, 940, "Moirachain_BiomeWeight_Desert")

        uv2 = make(material, unreal.MaterialExpressionTextureCoordinate, 1000, 1100, "Moirachain_BiomeWeight_UV2")
        uv2.set_editor_property("coordinate_index", 2)
        weight4 = mask(material, uv2, "", "R", 1220, 1020, "Moirachain_BiomeWeight_Mountain")
        weight5 = mask(material, uv2, "", "G", 1220, 1100, "Moirachain_BiomeWeight_Swamp")

        uv3 = make(material, unreal.MaterialExpressionTextureCoordinate, 1000, 1300, "Moirachain_BiomeWeight_UV3")
        uv3.set_editor_property("coordinate_index", 3)
        weight6 = mask(material, uv3, "", "R", 1220, 1180, "Moirachain_BiomeWeight_Tundra")

        grass_weight = add(
            material,
            add(material, weight0, weight1, 1430, 620, "Moirachain_GrassWeight_01"),
            add(material, weight2, weight5, 1430, 780, "Moirachain_GrassWeight_25"),
            1640,
            680,
            "Moirachain_GrassWeight",
        )

        uv0 = make(material, unreal.MaterialExpressionTextureCoordinate, 1430, 1500, "Moirachain_BiomeTexture_UV0")
        uv0.set_editor_property("coordinate_index", 0)

        grass_texture = texture_sample(
            material,
            "GrasslandTexture",
            "/Game/Materials/grass",
            uv0,
            1660,
            260,
        )
        sand_texture = texture_sample(
            material,
            "DesertTexture",
            "/Game/Materials/sand",
            uv0,
            1660,
            480,
        )
        rock_texture = texture_sample(
            material,
            "MountainTexture",
            "/Game/Materials/metamorphes-gestein-1024x697",
            uv0,
            1660,
            700,
        )
        snow_texture = texture_sample(
            material,
            "TundraTexture",
            "/Game/Materials/zblizona-tekstura-swiezej-bialej-powierzchni-sniegu_181624-56362",
            uv0,
            1660,
            920,
        )

        grass_part = multiply(material, grass_texture, grass_weight, 1920, 260, "Moirachain_GrassContribution")
        sand_part = multiply(material, sand_texture, weight3, 1920, 480, "Moirachain_DesertContribution")
        rock_part = multiply(material, rock_texture, weight4, 1920, 700, "Moirachain_MountainContribution")
        snow_part = multiply(material, snow_texture, weight6, 1920, 920, "Moirachain_TundraContribution")

        biome_color = add(
            material,
            add(material, grass_part, sand_part, 2160, 360, "Moirachain_BiomeColor_GS"),
            add(material, rock_part, snow_part, 2160, 760, "Moirachain_BiomeColor_RS"),
            2400,
            520,
            "Moirachain_BiomeColor",
        )

        blend_strength = make(
            material,
            unreal.MaterialExpressionScalarParameter,
            2400,
            900,
            "Moirachain_BiomeTextureBlendStrength",
        )
        blend_strength.set_editor_property("parameter_name", "BiomeTextureBlendStrength")
        blend_strength.set_editor_property("default_value", 0.85)

        final_blend = make(
            material,
            unreal.MaterialExpressionLinearInterpolate,
            2700,
            240,
            FINAL_DESCRIPTION,
        )
        unreal.MaterialEditingLibrary.connect_material_expressions(old_base, base_output, final_blend, "A")
        unreal.MaterialEditingLibrary.connect_material_expressions(biome_color, "", final_blend, "B")
        unreal.MaterialEditingLibrary.connect_material_expressions(blend_strength, "", final_blend, "Alpha")
        unreal.MaterialEditingLibrary.connect_material_property(final_blend, "", base_property)

        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
        unreal.log("[Moirachain] Added fixed-weight grass/sand/rock/snow biome texture blend.")
