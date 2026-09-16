"""Add the procedural biome tint channel to the existing terrain material.

The terrain mesh writes its soft biome blend into UV channel 1 and the blue
component of UV channel 2.  This script multiplies that tint into the current
Base Color without replacing any of the user's texture samples or altitude
logic.

Run with the Unreal Editor closed (or from an editor Python session):
    UnrealEditor-Cmd.exe Mojrachain.uproject -run=PythonScript \
        -Script="Tools/PatchTerrainMaterialForBiomeTint.py" -unattended -nop4
"""

import unreal


MATERIAL_PATH = "/Game/Materials/M_Terrain_Master_Altitude.M_Terrain_Master_Altitude"
TINT_DESCRIPTION = "Mojrachain_BiomeTint"
BIOME_TEXTURE_BLEND_DESCRIPTION = "Moirachain_BiomeTextureBlend"


def expression_description(expression):
    try:
        return str(expression.get_editor_property("desc"))
    except Exception:
        return ""


def make_expression(material, expression_class, x, y, description):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        expression_class,
        x,
        y,
    )
    expression.set_editor_property("desc", description)
    return expression


def add_repaired_tint_channels(material, tint_multiply):
    """Reconnect the UV channels using the actual unnamed ComponentMask pin."""
    uv1 = make_expression(
        material,
        unreal.MaterialExpressionTextureCoordinate,
        1200,
        1280,
        "Moirachain_BiomeTint_Repaired_UV1",
    )
    uv1.set_editor_property("coordinate_index", 1)

    uv2 = make_expression(
        material,
        unreal.MaterialExpressionTextureCoordinate,
        1200,
        1480,
        "Moirachain_BiomeTint_Repaired_UV2",
    )
    uv2.set_editor_property("coordinate_index", 2)

    uv2_r = make_expression(
        material,
        unreal.MaterialExpressionComponentMask,
        1430,
        1480,
        "Moirachain_BiomeTint_Repaired_UV2_R",
    )
    uv2_r.set_editor_property("r", True)
    uv2_r.set_editor_property("g", False)
    uv2_r.set_editor_property("b", False)
    uv2_r.set_editor_property("a", False)

    tint_vector = make_expression(
        material,
        unreal.MaterialExpressionAppendVector,
        1660,
        1320,
        "Moirachain_BiomeTint_Repaired_Vector",
    )

    unreal.MaterialEditingLibrary.connect_material_expressions(uv1, "", tint_vector, "A")
    # ComponentMask's input pin has no exposed name in UE5 Python.
    unreal.MaterialEditingLibrary.connect_material_expressions(uv2, "", uv2_r, "")
    unreal.MaterialEditingLibrary.connect_material_expressions(uv2_r, "", tint_vector, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(tint_vector, "", tint_multiply, "B")


material = unreal.load_asset(MATERIAL_PATH)
if not material:
    unreal.log_error("[Moirachain] Terrain material not found: " + MATERIAL_PATH)
else:
    base_property = unreal.MaterialProperty.MP_BASE_COLOR
    base_node = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material,
        base_property,
    )
    if expression_description(base_node) in (TINT_DESCRIPTION, BIOME_TEXTURE_BLEND_DESCRIPTION):
        if isinstance(base_node, unreal.MaterialExpressionMultiply):
            add_repaired_tint_channels(material, base_node)
            unreal.MaterialEditingLibrary.recompile_material(material)
            unreal.EditorAssetLibrary.save_loaded_asset(material)
            unreal.log("[Moirachain] Repaired the existing biome tint ComponentMask connection.")
        else:
            unreal.log("[Moirachain] A newer biome texture blend is already connected; no graph change was made.")
    else:
        base_output = unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(
            material,
            base_property,
        )

        if not base_node:
            unreal.log_error("[Moirachain] Base Color is not connected; biome tint was not added.")
        else:
            uv1 = make_expression(
                material,
                unreal.MaterialExpressionTextureCoordinate,
                1200,
                780,
                "Moirachain_BiomeTint_UV1",
            )
            uv1.set_editor_property("coordinate_index", 1)

            uv2 = make_expression(
                material,
                unreal.MaterialExpressionTextureCoordinate,
                1200,
                980,
                "Moirachain_BiomeTint_UV2",
            )
            uv2.set_editor_property("coordinate_index", 2)

            uv2_blue = make_expression(
                material,
                unreal.MaterialExpressionComponentMask,
                1430,
                980,
                "Moirachain_BiomeTint_UV2_B",
            )
            uv2_blue.set_editor_property("r", True)
            uv2_blue.set_editor_property("g", False)
            uv2_blue.set_editor_property("b", False)
            uv2_blue.set_editor_property("a", False)

            tint_vector = make_expression(
                material,
                unreal.MaterialExpressionAppendVector,
                1660,
                820,
                "Moirachain_BiomeTint_Vector",
            )

            unreal.MaterialEditingLibrary.connect_material_expressions(
                uv1,
                "",
                tint_vector,
                "A",
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                uv2,
                "",
                uv2_blue,
                "",
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                uv2_blue,
                "",
                tint_vector,
                "B",
            )

            tint_multiply = make_expression(
                material,
                unreal.MaterialExpressionMultiply,
                1930,
                120,
                TINT_DESCRIPTION,
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                base_node,
                base_output,
                tint_multiply,
                "A",
            )
            unreal.MaterialEditingLibrary.connect_material_expressions(
                tint_vector,
                "",
                tint_multiply,
                "B",
            )
            unreal.MaterialEditingLibrary.connect_material_property(
                tint_multiply,
                "",
                base_property,
            )

            unreal.MaterialEditingLibrary.recompile_material(material)
            unreal.EditorAssetLibrary.save_loaded_asset(material)
            unreal.log("[Moirachain] Added UV1/UV2 biome tint to terrain Base Color.")
