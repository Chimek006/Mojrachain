"""Separate mountain altitude layers from the tundra snow layer.

The altitude-aware material already has a grass -> rock -> snow chain, but
its original snow mask treated Mountain and Tundra as one cold biome.  That
made a Mountain hex become snow-covered much too early.  This patch keeps the
existing material and reconnects only its texture-layer inputs:

* Tundra uses its tundra weight as a native snow layer.
* Mountain gets a slightly warm grass tint at low altitude.
* Mountain gets snow only in a high-altitude band, after the rock layer.

The patch is intentionally material-only; it does not change terrain
generation, height, geometry, biome selection, or vertex data.
"""

import unreal


MATERIAL_PATH = "/Game/Materials/M_Terrain_Master_Altitude.M_Terrain_Master_Altitude"
PATCH_MARKER = "Moirachain_MountainTundraTextureSplit_v1"


def description(expression):
    try:
        return str(expression.get_editor_property("desc"))
    except Exception:
        return ""


def find_expression(expressions, desc):
    for expression in expressions:
        if description(expression) == desc:
            return expression
    return None


def collect_reachable_expressions(material, root):
    """Walk the connected material graph using UE 5.7's public Python API."""
    result = []
    visited = set()

    def visit(expression):
        if not expression:
            return
        key = str(expression)
        if key in visited:
            return
        visited.add(key)
        result.append(expression)
        try:
            inputs = unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
                material,
                expression,
            )
        except Exception:
            inputs = []
        for input_expression in inputs:
            visit(input_expression)

    visit(root)
    return result


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


def add(material, a, b, x, y, desc):
    expression = make(material, unreal.MaterialExpressionAdd, x, y, desc)
    connect(a, "", expression, "A")
    connect(b, "", expression, "B")
    return expression


def mask(material, source, channel, x, y, desc):
    expression = make(material, unreal.MaterialExpressionComponentMask, x, y, desc)
    expression.set_editor_property("r", channel == "R")
    expression.set_editor_property("g", channel == "G")
    expression.set_editor_property("b", channel == "B")
    expression.set_editor_property("a", channel == "A")
    connect(source, "", expression, "")
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


material = unreal.load_asset(MATERIAL_PATH)
if not material:
    unreal.log_error("[Moirachain] Terrain material not found: " + MATERIAL_PATH)
else:
    base_property = unreal.MaterialProperty.MP_BASE_COLOR
    base_node = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material,
        base_property,
    )
    expressions = collect_reachable_expressions(material, base_node)
    if find_expression(expressions, PATCH_MARKER):
        unreal.log("[Moirachain] Mountain/tundra texture split already exists.")
    else:
        # These are the nodes in the active BiomeTextureBlend graph.  The
        # older altitude graph is still present in the asset, but is no
        # longer connected to Base Color after the later biome-texture patch.
        required = {
            "vertex_color": "Moirachain_BiomeTexture_VertexColor",
            "mountain": "Moirachain_BiomeWeight_Mountain",
            "grass": "Moirachain_BiomeTexture_GrasslandTexture",
            "rock": "Moirachain_BiomeTexture_MountainTexture",
            "snow": "Moirachain_BiomeTexture_TundraTexture",
            "mountain_contribution": "Moirachain_MountainContribution",
        }
        nodes = {
            key: find_expression(expressions, desc)
            for key, desc in required.items()
        }
        missing = [key for key, value in nodes.items() if not value]
        if missing:
            unreal.log_error(
                "[Moirachain] Cannot split mountain/tundra texture layers; "
                "missing nodes: " + ", ".join(missing)
            )
        else:
            altitude = mask(
                material,
                nodes["vertex_color"],
                "R",
                2920,
                2220,
                "Moirachain_MountainAltitude01",
            )

            # A subtle warm tint keeps low mountain slopes visually distinct
            # from grassland without replacing the grass texture.
            mountain_grass_tint = make(
                material,
                unreal.MaterialExpressionVectorParameter,
                3160,
                1960,
                "Moirachain_MountainGrassTint",
            )
            mountain_grass_tint.set_editor_property(
                "parameter_name", "MountainGrassTint"
            )
            mountain_grass_tint.set_editor_property(
                "default_value", unreal.LinearColor(1.0, 0.88, 0.68, 1.0)
            )
            warm_grass = multiply(
                material,
                nodes["grass"],
                mountain_grass_tint,
                3400,
                1960,
                "Moirachain_MountainGrassTintMultiply",
            )
            mountain_grass = lerp(
                material,
                nodes["grass"],
                warm_grass,
                nodes["mountain"],
                3640,
                1960,
                "Moirachain_MountainGrassBeforeRock",
            )

            # Rock begins gradually above the lower mountain grass band.
            rock_start = constant(
                material,
                0.32,
                2920,
                2420,
                "Moirachain_MountainRockStart",
            )
            rock_end = constant(
                material,
                0.65,
                2920,
                2500,
                "Moirachain_MountainRockEnd",
            )
            rock_range = subtract(
                material,
                rock_end,
                rock_start,
                3160,
                2420,
                "Moirachain_MountainRockRange",
            )
            rock_offset = subtract(
                material,
                altitude,
                rock_start,
                3160,
                2540,
                "Moirachain_MountainRockOffset",
            )
            rock_height = saturate(
                material,
                divide(
                    material,
                    rock_offset,
                    rock_range,
                    3400,
                    2480,
                    "Moirachain_MountainRockDivide",
                ),
                3640,
                2480,
                "Moirachain_MountainRockHeight",
            )
            mountain_rock = lerp(
                material,
                mountain_grass,
                nodes["rock"],
                rock_height,
                3880,
                2100,
                "Moirachain_MountainGrassToRock",
            )

            # Mountains use a stricter, independent snow band.  The altitude
            # channel is normalized inside each generated hex, so this puts
            # snow only on the highest part of a mountain profile.
            mountain_snow_start = constant(
                material,
                0.86,
                2920,
                2700,
                "Moirachain_MountainSnowStart",
            )
            mountain_snow_end = constant(
                material,
                0.99,
                2920,
                2780,
                "Moirachain_MountainSnowEnd",
            )
            mountain_snow_range = subtract(
                material,
                mountain_snow_end,
                mountain_snow_start,
                3160,
                2700,
                "Moirachain_MountainSnowRange",
            )
            mountain_snow_offset = subtract(
                material,
                altitude,
                mountain_snow_start,
                3160,
                2820,
                "Moirachain_MountainSnowOffset",
            )
            mountain_snow_height = saturate(
                material,
                divide(
                    material,
                    mountain_snow_offset,
                    mountain_snow_range,
                    3640,
                    2760,
                    "Moirachain_MountainSnowDivide",
                ),
                3880,
                2760,
                "Moirachain_MountainSnowHeight",
            )
            mountain_layer = lerp(
                material,
                mountain_rock,
                nodes["snow"],
                mountain_snow_height,
                4120,
                2100,
                PATCH_MARKER,
            )

            # The active graph multiplies this result by the mountain biome
            # weight in MountainContribution. TundraContribution is left
            # untouched, so tundra remains natively snow-covered.
            connect(mountain_layer, "", nodes["mountain_contribution"], "A")

            unreal.MaterialEditingLibrary.recompile_material(material)
            unreal.EditorAssetLibrary.save_loaded_asset(material)
            unreal.log(
                "[Moirachain] Separated tundra snow from mountain altitude snow."
            )
