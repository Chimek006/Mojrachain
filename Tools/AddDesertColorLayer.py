"""Add a DesertColor layer to the current terrain material."""

import unreal


MATERIAL_PATH = "/Game/Materials/M_Terrain_Master_Altitude.M_Terrain_Master_Altitude"
DESERT_COLOR_PARAMETER = "DesertColor"
DESERT_SWITCH_PARAMETER = "BiomeIsDesert"


material = unreal.load_asset(MATERIAL_PATH)
if not material:
    unreal.log_error("[Moirachain] Terrain material not found: " + MATERIAL_PATH)
else:
    vector_names = [str(name) for name in unreal.MaterialEditingLibrary.get_vector_parameter_names(material)]
    scalar_names = [str(name) for name in unreal.MaterialEditingLibrary.get_scalar_parameter_names(material)]

    if DESERT_COLOR_PARAMETER in vector_names and DESERT_SWITCH_PARAMETER in scalar_names:
        unreal.log("[Moirachain] DesertColor layer already exists; no graph change was made.")
    else:
        base_property = unreal.MaterialProperty.MP_BASE_COLOR
        base_node = unreal.MaterialEditingLibrary.get_material_property_input_node(material, base_property)
        base_output = unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(material, base_property)

        if not base_node:
            unreal.log_error("[Moirachain] Base Color is not connected; DesertColor was not added.")
        else:
            desert_color = unreal.MaterialEditingLibrary.create_material_expression(
                material,
                unreal.MaterialExpressionVectorParameter,
                1180,
                260,
            )
            desert_color.set_editor_property("parameter_name", DESERT_COLOR_PARAMETER)
            desert_color.set_editor_property(
                "default_value",
                unreal.LinearColor(0.95, 0.65, 0.15, 1.0),
            )

            desert_switch = unreal.MaterialEditingLibrary.create_material_expression(
                material,
                unreal.MaterialExpressionScalarParameter,
                1180,
                430,
            )
            desert_switch.set_editor_property("parameter_name", DESERT_SWITCH_PARAMETER)
            desert_switch.set_editor_property("default_value", 0.0)

            desert_blend = unreal.MaterialEditingLibrary.create_material_expression(
                material,
                unreal.MaterialExpressionLinearInterpolate,
                1510,
                40,
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                base_node,
                base_output,
                desert_blend,
                "A",
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                desert_color,
                "",
                desert_blend,
                "B",
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                desert_switch,
                "",
                desert_blend,
                "Alpha",
            )
            unreal.MaterialEditingLibrary.connect_material_property(
                desert_blend,
                "",
                base_property,
            )
            unreal.MaterialEditingLibrary.recompile_material(material)
            unreal.EditorAssetLibrary.save_loaded_asset(material)
            unreal.log("[Moirachain] Added DesertColor as a Desert-only Base Color layer.")

