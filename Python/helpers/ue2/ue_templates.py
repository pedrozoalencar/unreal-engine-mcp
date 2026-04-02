"""UE2 Templates tool -- high-level compound operations that compose multiple commands.
Materials, geometry, scene layouts, and Blueprint Geometry Script creation."""
import math
import time
from typing import Any, Dict, List

from helpers.ue2.response import UEResponse, cmd, extract_data

TOOL = "ue_templates"
ACTIONS = [
    "mat_glass", "mat_pbr", "mat_emissive",
    "gs_cup", "gs_terrain", "gs_column",
    "scene_grid", "scene_circle",
    "bp_geometry_script",
]


def _cmd(conn, command: str, params: dict) -> dict:
    """Send command and return result, raising on failure."""
    result = conn.send_command(command, params)
    if result and result.get("status") == "error":
        raise RuntimeError(f"Command {command} failed: {result.get('error', 'unknown')}")
    return result


def register_template_tools(mcp, get_connection):
    """Register the ue_templates action-based tool."""

    @mcp.tool()
    def ue_templates(
        action: str,
        # --- Material params ---
        asset_path: str = "",
        tint: List[float] = [],
        ior: float = 1.52,
        roughness: float = 0.5,
        thickness: float = 5.0,
        base_color: List[float] = [],
        metallic: float = 0.0,
        color: List[float] = [],
        intensity: float = 10.0,
        apply_to_actor: str = "",
        # --- Geometry Script params ---
        name: str = "",
        outer_radius: float = 45.0,
        height: float = 110.0,
        wall_thickness: float = 7.0,
        base_thickness: float = 10.0,
        segments: int = 32,
        spawn: bool = True,
        location: List[float] = [],
        # --- gs_terrain ---
        size: float = 1000.0,
        subdivisions: int = 32,
        noise_magnitude: float = 50.0,
        noise_frequency: float = 0.05,
        # --- gs_column ---
        radius: float = 30.0,
        base_radius: float = 40.0,
        # --- Scene params ---
        blueprint_path: str = "",
        rows: int = 3,
        cols: int = 3,
        spacing: float = 200.0,
        start: List[float] = [],
        count: int = 8,
        center: List[float] = [],
        material_path: str = "",
        # --- bp_geometry_script ---
        operations: List[Dict[str, Any]] = [],
    ) -> Dict[str, Any]:
        """High-level templates that compose multiple Unreal commands into one call.

        MATERIAL TEMPLATES:
          mat_glass       -- Create complete Substrate glass material with SlabBSDF,
                             TransmittanceToMFP, and Fresnel refraction.
                             Params: asset_path, tint=[0.85,0.92,0.97], ior=1.52,
                             roughness=0.02, thickness=5.0, apply_to_actor="".
          mat_pbr         -- Create PBR material with Substrate SlabBSDF.
                             Params: asset_path, base_color=[0.5,0.5,0.5], roughness=0.5,
                             metallic=0.0, apply_to_actor="".
          mat_emissive    -- Create emissive (glowing) material.
                             Params: asset_path, color=[1,0.5,0], intensity=10,
                             apply_to_actor="".

        GEOMETRY SCRIPT TEMPLATES:
          gs_cup          -- Create hollow cup via boolean subtract.
                             Params: name, outer_radius=45, height=110, wall_thickness=7,
                             base_thickness=10, segments=32, spawn=True, location=[0,0,0].
          gs_terrain      -- Create terrain patch with Perlin noise.
                             Params: name, size=1000, subdivisions=32, noise_magnitude=50,
                             noise_frequency=0.05, spawn=True, location=[0,0,0].
          gs_column       -- Create classical column (base + shaft + capital).
                             Params: name, radius=30, height=300, base_radius=40,
                             segments=24, spawn=True, location=[0,0,0].

        SCENE LAYOUT TEMPLATES:
          scene_grid      -- Spawn actors in a grid pattern.
                             Params: blueprint_path, rows=3, cols=3, spacing=200,
                             start=[0,0,0], material_path="".
          scene_circle    -- Spawn actors in a circle pattern.
                             Params: blueprint_path, count=8, radius=500,
                             center=[0,0,0], material_path="".

        BLUEPRINT TEMPLATES:
          bp_geometry_script -- Create a complete Geometry Script Blueprint with
                                OnRebuildGeneratedMesh override, add GS nodes, connect, compile.
                                Params: name, operations=[{"func":"AppendCylinder","pins":{"Radius":"45"}}].
        """
        t0 = time.time()
        conn = get_connection()
        if conn is None:
            return UEResponse.not_connected(TOOL)

        try:
            # ==================================================================
            # MATERIAL TEMPLATES
            # ==================================================================
            if action == "mat_glass":
                return _tpl_mat_glass(
                    conn, t0, asset_path,
                    tint=tint or [0.85, 0.92, 0.97],
                    ior=ior,
                    roughness=roughness if roughness != 0.5 else 0.02,
                    thickness=thickness,
                    apply_to_actor=apply_to_actor,
                )

            elif action == "mat_pbr":
                return _tpl_mat_pbr(
                    conn, t0, asset_path,
                    base_color=base_color or [0.5, 0.5, 0.5],
                    roughness=roughness,
                    metallic=metallic,
                    apply_to_actor=apply_to_actor,
                )

            elif action == "mat_emissive":
                return _tpl_mat_emissive(
                    conn, t0, asset_path,
                    color=color or [1.0, 0.5, 0.0],
                    intensity=intensity,
                    apply_to_actor=apply_to_actor,
                )

            # ==================================================================
            # GEOMETRY SCRIPT TEMPLATES
            # ==================================================================
            elif action == "gs_cup":
                return _tpl_gs_cup(
                    conn, t0,
                    name=name,
                    outer_radius=outer_radius,
                    height=height,
                    wall_thickness=wall_thickness,
                    base_thickness=base_thickness,
                    segments=segments,
                    spawn=spawn,
                    location=location or [0, 0, 0],
                )

            elif action == "gs_terrain":
                return _tpl_gs_terrain(
                    conn, t0,
                    name=name,
                    size=size,
                    subdivisions=subdivisions,
                    noise_magnitude=noise_magnitude,
                    noise_frequency=noise_frequency,
                    spawn=spawn,
                    location=location or [0, 0, 0],
                )

            elif action == "gs_column":
                return _tpl_gs_column(
                    conn, t0,
                    name=name,
                    radius=radius,
                    height=height if height != 110.0 else 300.0,
                    base_radius=base_radius,
                    segments=segments if segments != 32 else 24,
                    spawn=spawn,
                    location=location or [0, 0, 0],
                )

            # ==================================================================
            # SCENE LAYOUT TEMPLATES
            # ==================================================================
            elif action == "scene_grid":
                return _tpl_scene_grid(
                    conn, t0,
                    blueprint_path=blueprint_path,
                    rows=rows,
                    cols=cols,
                    spacing=spacing,
                    start=start or [0, 0, 0],
                    material_path=material_path,
                )

            elif action == "scene_circle":
                return _tpl_scene_circle(
                    conn, t0,
                    blueprint_path=blueprint_path,
                    count=count,
                    radius=radius if radius != 30.0 else 500.0,
                    center=center or [0, 0, 0],
                    material_path=material_path,
                )

            # ==================================================================
            # BLUEPRINT TEMPLATES
            # ==================================================================
            elif action == "bp_geometry_script":
                return _tpl_bp_geometry_script(
                    conn, t0,
                    name=name,
                    operations=operations,
                )

            # ==================================================================
            else:
                return UEResponse.invalid_action(action, ACTIONS, TOOL)

        except ConnectionError:
            return UEResponse.not_connected(TOOL)
        except Exception as e:
            return UEResponse.error("E_INTERNAL", str(e), TOOL, retryable=False)


# ======================================================================
# MATERIAL TEMPLATE IMPLEMENTATIONS
# ======================================================================

def _tpl_mat_glass(conn, t0, asset_path, tint, ior, roughness, thickness, apply_to_actor):
    """Create a complete Substrate glass material."""
    if not asset_path:
        return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required", TOOL)

    path = asset_path

    # 1. Create material
    _cmd(conn, "mat_create_material", {
        "asset_path": path,
        "blend_mode": "TranslucentColoredTransmittance",
        "shading_model": "DefaultLit",
        "two_sided": True,
    })

    # 2. Substrate Slab BSDF
    slab = _cmd(conn, "mat_add_expression", {
        "asset_path": path, "class_name": "MaterialExpressionSubstrateSlabBSDF",
        "pos_x": 0, "pos_y": 0,
    })
    slab_id = slab.get("result", {}).get("node_id", "0")

    # 3. Diffuse Albedo = black (glass has no diffuse)
    diff = _cmd(conn, "mat_add_expression", {
        "asset_path": path, "class_name": "MaterialExpressionConstant3Vector",
        "pos_x": -400, "pos_y": -100,
    })
    diff_id = diff.get("result", {}).get("node_id", "1")

    # 4. F0 = 0.04
    f0 = _cmd(conn, "mat_add_expression", {
        "asset_path": path, "class_name": "MaterialExpressionConstant",
        "pos_x": -400, "pos_y": 50,
    })
    f0_id = f0.get("result", {}).get("node_id", "2")

    # 5. F90 = 1.0
    f90 = _cmd(conn, "mat_add_expression", {
        "asset_path": path, "class_name": "MaterialExpressionConstant",
        "pos_x": -400, "pos_y": 120,
    })
    f90_id = f90.get("result", {}).get("node_id", "3")

    # 6. Roughness
    rg = _cmd(conn, "mat_add_expression", {
        "asset_path": path, "class_name": "MaterialExpressionConstant",
        "pos_x": -400, "pos_y": 190,
    })
    rg_id = rg.get("result", {}).get("node_id", "4")

    # 7. TransmittanceToMFP
    trans = _cmd(conn, "mat_add_expression", {
        "asset_path": path, "class_name": "MaterialExpressionSubstrateTransmittanceToMFP",
        "pos_x": -300, "pos_y": 300,
    })
    trans_id = trans.get("result", {}).get("node_id", "5")

    # 8. Tint color
    tint_node = _cmd(conn, "mat_add_expression", {
        "asset_path": path, "class_name": "MaterialExpressionConstant3Vector",
        "pos_x": -600, "pos_y": 280,
    })
    tint_id = tint_node.get("result", {}).get("node_id", "6")

    # 9. Thickness
    thick = _cmd(conn, "mat_add_expression", {
        "asset_path": path, "class_name": "MaterialExpressionConstant",
        "pos_x": -600, "pos_y": 380,
    })
    thick_id = thick.get("result", {}).get("node_id", "7")

    # 10. Fresnel
    fresnel = _cmd(conn, "mat_add_expression", {
        "asset_path": path, "class_name": "MaterialExpressionFresnel",
        "pos_x": -600, "pos_y": 500,
    })
    fresnel_id = fresnel.get("result", {}).get("node_id", "8")

    # 11. Lerp for IOR
    lerp = _cmd(conn, "mat_add_expression", {
        "asset_path": path, "class_name": "MaterialExpressionLinearInterpolate",
        "pos_x": -300, "pos_y": 500,
    })
    lerp_id = lerp.get("result", {}).get("node_id", "9")

    # 12. IOR constants
    ior_a = _cmd(conn, "mat_add_expression", {
        "asset_path": path, "class_name": "MaterialExpressionConstant",
        "pos_x": -600, "pos_y": 470,
    })
    ior_a_id = ior_a.get("result", {}).get("node_id", "10")

    ior_b = _cmd(conn, "mat_add_expression", {
        "asset_path": path, "class_name": "MaterialExpressionConstant",
        "pos_x": -600, "pos_y": 540,
    })
    ior_b_id = ior_b.get("result", {}).get("node_id", "11")

    # Set values
    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": diff_id, "property_name": "Constant", "value": "(R=0,G=0,B=0,A=1)"})
    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": f0_id, "property_name": "R", "value": "0.04"})
    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": f90_id, "property_name": "R", "value": "1.0"})
    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": rg_id, "property_name": "R", "value": str(roughness)})
    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": tint_id, "property_name": "Constant", "value": f"(R={tint[0]},G={tint[1]},B={tint[2]},A=1)"})
    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": thick_id, "property_name": "R", "value": str(thickness)})
    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": fresnel_id, "property_name": "Exponent", "value": "3.0"})
    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": fresnel_id, "property_name": "BaseReflectFraction", "value": "0.04"})
    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": ior_a_id, "property_name": "R", "value": "1.0"})
    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": ior_b_id, "property_name": "R", "value": str(ior)})

    # Connect nodes to SlabBSDF
    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": diff_id, "target_node_id": slab_id, "input_pin_name": "Diffuse Albedo"})
    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": f0_id, "target_node_id": slab_id, "input_pin_name": "F0"})
    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": f90_id, "target_node_id": slab_id, "input_pin_name": "F90"})
    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": rg_id, "target_node_id": slab_id, "input_pin_name": "Roughness"})

    # Connect TransmittanceToMFP
    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": tint_id, "target_node_id": trans_id, "input_pin_name": "TransmittanceColor"})
    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": thick_id, "target_node_id": trans_id, "input_pin_name": "Thickness"})
    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": trans_id, "target_node_id": slab_id, "input_pin_name": "SSS MFP"})

    # Connect to output
    _cmd(conn, "mat_connect_to_output", {"asset_path": path, "source_node_id": slab_id, "property_name": "FrontMaterial"})

    # Connect Fresnel refraction chain
    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": ior_a_id, "target_node_id": lerp_id, "input_pin_name": "A"})
    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": ior_b_id, "target_node_id": lerp_id, "input_pin_name": "B"})
    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": fresnel_id, "target_node_id": lerp_id, "input_pin_name": "Alpha"})
    _cmd(conn, "mat_connect_to_output", {"asset_path": path, "source_node_id": lerp_id, "property_name": "Refraction"})

    # Compile
    _cmd(conn, "mat_compile_material", {"asset_path": path})

    # Optional: apply to actor
    if apply_to_actor:
        _cmd(conn, "mat_apply_material_to_actor", {
            "asset_path": path, "actor_name": apply_to_actor, "slot_index": 0,
        })

    return UEResponse.ok(
        {"asset_path": path, "message": "Glass material created with Substrate SlabBSDF + TransmittanceToMFP + Fresnel refraction"},
        tool=TOOL,
        duration_ms=(time.time() - t0) * 1000,
    )


def _tpl_mat_pbr(conn, t0, asset_path, base_color, roughness, metallic, apply_to_actor):
    """Create a complete PBR material with Substrate SlabBSDF."""
    if not asset_path:
        return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required", TOOL)

    path = asset_path
    _cmd(conn, "mat_create_material", {"asset_path": path, "blend_mode": "Opaque"})

    slab = _cmd(conn, "mat_add_expression", {"asset_path": path, "class_name": "MaterialExpressionSubstrateSlabBSDF", "pos_x": 0, "pos_y": 0})
    slab_id = slab.get("result", {}).get("node_id", "0")

    bc = _cmd(conn, "mat_add_expression", {"asset_path": path, "class_name": "MaterialExpressionConstant3Vector", "pos_x": -400, "pos_y": -50})
    bc_id = bc.get("result", {}).get("node_id", "1")

    rg = _cmd(conn, "mat_add_expression", {"asset_path": path, "class_name": "MaterialExpressionConstant", "pos_x": -400, "pos_y": 100})
    rg_id = rg.get("result", {}).get("node_id", "2")

    mt = _cmd(conn, "mat_add_expression", {"asset_path": path, "class_name": "MaterialExpressionConstant", "pos_x": -400, "pos_y": 200})
    mt_id = mt.get("result", {}).get("node_id", "3")

    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": bc_id, "property_name": "Constant", "value": f"(R={base_color[0]},G={base_color[1]},B={base_color[2]},A=1)"})
    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": rg_id, "property_name": "R", "value": str(roughness)})
    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": mt_id, "property_name": "R", "value": str(metallic)})

    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": bc_id, "target_node_id": slab_id, "input_pin_name": "Diffuse Albedo"})
    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": rg_id, "target_node_id": slab_id, "input_pin_name": "Roughness"})
    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": mt_id, "target_node_id": slab_id, "input_pin_name": "F0"})

    _cmd(conn, "mat_connect_to_output", {"asset_path": path, "source_node_id": slab_id, "property_name": "FrontMaterial"})
    _cmd(conn, "mat_compile_material", {"asset_path": path})

    if apply_to_actor:
        _cmd(conn, "mat_apply_material_to_actor", {"asset_path": path, "actor_name": apply_to_actor, "slot_index": 0})

    return UEResponse.ok(
        {"asset_path": path, "message": "PBR material created with Substrate SlabBSDF"},
        tool=TOOL,
        duration_ms=(time.time() - t0) * 1000,
    )


def _tpl_mat_emissive(conn, t0, asset_path, color, intensity, apply_to_actor):
    """Create an emissive material."""
    if not asset_path:
        return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required", TOOL)

    path = asset_path
    _cmd(conn, "mat_create_material", {"asset_path": path, "blend_mode": "Opaque"})

    color_node = _cmd(conn, "mat_add_expression", {"asset_path": path, "class_name": "MaterialExpressionConstant3Vector", "pos_x": -600, "pos_y": 0})
    color_id = color_node.get("result", {}).get("node_id", "0")

    intensity_node = _cmd(conn, "mat_add_expression", {"asset_path": path, "class_name": "MaterialExpressionConstant", "pos_x": -600, "pos_y": 150})
    intensity_id = intensity_node.get("result", {}).get("node_id", "1")

    multiply = _cmd(conn, "mat_add_expression", {"asset_path": path, "class_name": "MaterialExpressionMultiply", "pos_x": -300, "pos_y": 50})
    multiply_id = multiply.get("result", {}).get("node_id", "2")

    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": color_id, "property_name": "Constant", "value": f"(R={color[0]},G={color[1]},B={color[2]},A=1)"})
    _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": intensity_id, "property_name": "R", "value": str(intensity)})

    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": color_id, "target_node_id": multiply_id, "input_pin_name": "A"})
    _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": intensity_id, "target_node_id": multiply_id, "input_pin_name": "B"})
    _cmd(conn, "mat_connect_to_output", {"asset_path": path, "source_node_id": multiply_id, "property_name": "EmissiveColor"})

    _cmd(conn, "mat_compile_material", {"asset_path": path})

    if apply_to_actor:
        _cmd(conn, "mat_apply_material_to_actor", {"asset_path": path, "actor_name": apply_to_actor, "slot_index": 0})

    return UEResponse.ok(
        {"asset_path": path, "message": "Emissive material created"},
        tool=TOOL,
        duration_ms=(time.time() - t0) * 1000,
    )


# ======================================================================
# GEOMETRY SCRIPT TEMPLATE IMPLEMENTATIONS
# ======================================================================

def _tpl_gs_cup(conn, t0, name, outer_radius, height, wall_thickness, base_thickness, segments, spawn, location):
    """Create a hollow cup mesh using boolean subtract."""
    if not name:
        return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required", TOOL)

    inner_radius = outer_radius - wall_thickness
    inner_height = height - base_thickness
    inner_offset_z = base_thickness

    # Outer body
    _cmd(conn, "gs_append_cylinder", {
        "mesh_name": name, "radius": outer_radius,
        "height": height, "radial_steps": segments,
    })

    # Inner cavity
    temp_name = f"__{name}_cavity"
    _cmd(conn, "gs_append_cylinder", {
        "mesh_name": temp_name, "radius": inner_radius,
        "height": inner_height, "radial_steps": segments,
        "location": [0, 0, inner_offset_z],
    })

    # Boolean subtract
    _cmd(conn, "gs_boolean", {
        "target_mesh": name, "tool_mesh": temp_name, "operation": "subtract",
    })

    # Cleanup temp
    _cmd(conn, "gs_release_mesh", {"mesh_name": temp_name})

    # Optionally spawn
    if spawn:
        _cmd(conn, "gs_spawn_dynamic_mesh_actor", {
            "mesh_name": name, "actor_name": name, "location": location,
        })

    return UEResponse.ok(
        {"mesh_name": name, "message": f"Cup created: R={outer_radius}, H={height}, wall={wall_thickness}, base={base_thickness}"},
        tool=TOOL,
        duration_ms=(time.time() - t0) * 1000,
    )


def _tpl_gs_terrain(conn, t0, name, size, subdivisions, noise_magnitude, noise_frequency, spawn, location):
    """Create a procedural terrain patch using subdivided box + Perlin noise."""
    if not name:
        return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required", TOOL)

    _cmd(conn, "gs_append_box", {
        "mesh_name": name,
        "dimension_x": size, "dimension_y": size, "dimension_z": 10.0,
        "steps_x": subdivisions, "steps_y": subdivisions, "steps_z": 0,
    })

    _cmd(conn, "gs_perlin_noise", {
        "mesh_name": name,
        "magnitude": noise_magnitude, "frequency": noise_frequency,
    })

    _cmd(conn, "gs_auto_uvs", {"mesh_name": name})

    if spawn:
        _cmd(conn, "gs_spawn_dynamic_mesh_actor", {
            "mesh_name": name, "actor_name": name, "location": location,
        })

    return UEResponse.ok(
        {"mesh_name": name, "message": f"Terrain created: {size}x{size}, {subdivisions}x{subdivisions} subdivisions"},
        tool=TOOL,
        duration_ms=(time.time() - t0) * 1000,
    )


def _tpl_gs_column(conn, t0, name, radius, height, base_radius, segments, spawn, location):
    """Create a classical column with base, shaft, and capital."""
    if not name:
        return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required", TOOL)

    base_height = 20.0
    cap_radius = base_radius - 2.0
    cap_height = 15.0

    # Base
    _cmd(conn, "gs_append_cylinder", {
        "mesh_name": name, "radius": base_radius,
        "height": base_height, "radial_steps": segments,
    })

    # Shaft
    shaft = f"__{name}_shaft"
    _cmd(conn, "gs_append_cylinder", {
        "mesh_name": shaft, "radius": radius,
        "height": height, "radial_steps": segments,
        "location": [0, 0, base_height],
    })
    _cmd(conn, "gs_append_mesh", {
        "target_mesh": name, "source_mesh": shaft,
    })
    _cmd(conn, "gs_release_mesh", {"mesh_name": shaft})

    # Capital
    cap = f"__{name}_cap"
    _cmd(conn, "gs_append_cylinder", {
        "mesh_name": cap, "radius": cap_radius,
        "height": cap_height, "radial_steps": segments,
        "location": [0, 0, base_height + height],
    })
    _cmd(conn, "gs_append_mesh", {
        "target_mesh": name, "source_mesh": cap,
    })
    _cmd(conn, "gs_release_mesh", {"mesh_name": cap})

    if spawn:
        _cmd(conn, "gs_spawn_dynamic_mesh_actor", {
            "mesh_name": name, "actor_name": name, "location": location,
        })

    return UEResponse.ok(
        {"mesh_name": name, "message": f"Column created: R={radius}, H={height}, base_R={base_radius}"},
        tool=TOOL,
        duration_ms=(time.time() - t0) * 1000,
    )


# ======================================================================
# SCENE LAYOUT TEMPLATE IMPLEMENTATIONS
# ======================================================================

def _tpl_scene_grid(conn, t0, blueprint_path, rows, cols, spacing, start, material_path):
    """Spawn actors in a grid pattern."""
    if not blueprint_path:
        return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'blueprint_path' is required", TOOL)

    spawned = []
    for r in range(rows):
        for c in range(cols):
            loc = [
                start[0] + c * spacing,
                start[1] + r * spacing,
                start[2],
            ]
            result = _cmd(conn, "level_spawn_blueprint_by_path", {
                "blueprint_path": blueprint_path,
                "location": loc, "rotation": [0, 0, 0], "scale": [1, 1, 1],
                "name": f"Grid_{r}_{c}",
            })
            actor_name = result.get("result", {}).get("actor_name", "")
            if material_path and actor_name:
                _cmd(conn, "mat_apply_material_to_actor", {
                    "asset_path": material_path, "actor_name": actor_name, "slot_index": 0,
                })
            spawned.append(actor_name)

    return UEResponse.ok(
        {"count": len(spawned), "actors": spawned, "message": f"Grid {rows}x{cols} = {len(spawned)} actors"},
        tool=TOOL,
        duration_ms=(time.time() - t0) * 1000,
    )


def _tpl_scene_circle(conn, t0, blueprint_path, count, radius, center, material_path):
    """Spawn actors in a circle pattern, facing center."""
    if not blueprint_path:
        return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'blueprint_path' is required", TOOL)

    spawned = []
    for i in range(count):
        angle = 2 * math.pi * i / count
        loc = [
            center[0] + radius * math.cos(angle),
            center[1] + radius * math.sin(angle),
            center[2],
        ]
        # Yaw to face center
        rot = [0, math.degrees(angle) + 180, 0]

        result = _cmd(conn, "level_spawn_blueprint_by_path", {
            "blueprint_path": blueprint_path,
            "location": loc, "rotation": rot, "scale": [1, 1, 1],
            "name": f"Circle_{i}",
        })
        actor_name = result.get("result", {}).get("actor_name", "")
        if material_path and actor_name:
            _cmd(conn, "mat_apply_material_to_actor", {
                "asset_path": material_path, "actor_name": actor_name, "slot_index": 0,
            })
        spawned.append(actor_name)

    return UEResponse.ok(
        {"count": len(spawned), "actors": spawned, "message": f"Circle of {count} actors, R={radius}"},
        tool=TOOL,
        duration_ms=(time.time() - t0) * 1000,
    )


# ======================================================================
# BLUEPRINT TEMPLATE IMPLEMENTATIONS
# ======================================================================

def _tpl_bp_geometry_script(conn, t0, name, operations):
    """Create a complete Geometry Script Blueprint with OnRebuildGeneratedMesh override.

    Steps:
      1. Create Blueprint with parent GeneratedDynamicMeshActor
      2. Override OnRebuildGeneratedMesh
      3. For each operation, add a CallFunction node for the GS function
      4. Set pin values on each node
      5. Chain execution pins and connect TargetMesh/ReturnValue through the sequence
      6. Compile the Blueprint

    operations format:
      [
        {"func": "AppendCylinder", "pins": {"Radius": "45", "Height": "100"}},
        {"func": "AppendBox", "pins": {"DimensionX": "50", ...}},
      ]
    """
    if not name:
        return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required", TOOL)
    if not operations:
        return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'operations' is required (list of GS operations)", TOOL)

    bp_name = name

    # 1. Create Blueprint with GeneratedDynamicMeshActor parent
    _cmd(conn, "create_blueprint", {
        "name": bp_name,
        "parent_class": "GeneratedDynamicMeshActor",
    })

    # 2. Override OnRebuildGeneratedMesh
    override_result = _cmd(conn, "add_event_override", {
        "blueprint_name": bp_name,
        "event_name": "OnRebuildGeneratedMesh",
    })
    # The override creates a function entry node; extract its ID and output pin info
    override_data = override_result.get("result", {})
    entry_node_id = override_data.get("node_id", override_data.get("entry_node_id", ""))
    func_name = "OnRebuildGeneratedMesh"

    # Track previous node for execution chaining and mesh data flow
    prev_node_id = entry_node_id
    prev_exec_pin = "Then"  # Output exec pin from the entry node

    # 3-5. Add GS function nodes, set pins, and chain
    node_ids = []
    pos_x = 300
    for idx, op in enumerate(operations):
        func = op.get("func", "")
        pins = op.get("pins", {})

        # Add CallFunction node for this GS function
        node_result = _cmd(conn, "add_node", {
            "blueprint_name": bp_name,
            "node_type": "CallFunction",
            "target_function": func,
            "pos_x": pos_x,
            "pos_y": 0,
            "function_name": func_name,
        })
        node_data = node_result.get("result", {})
        node_id = node_data.get("node_id", str(idx + 1))
        node_ids.append(node_id)

        # Set pin values
        for pin_name, pin_value in pins.items():
            _cmd(conn, "set_pin_value", {
                "blueprint_name": bp_name,
                "node_id": node_id,
                "pin_name": pin_name,
                "value": str(pin_value),
                "function_name": func_name,
            })

        # Chain execution: previous node's exec output -> this node's exec input
        _cmd(conn, "connect_nodes", {
            "blueprint_name": bp_name,
            "source_node_id": prev_node_id,
            "source_pin_name": prev_exec_pin,
            "target_node_id": node_id,
            "target_pin_name": "execute",
            "function_name": func_name,
        })

        # Connect TargetMesh data flow:
        # First node gets TargetMesh from the entry node's output pin,
        # subsequent nodes get ReturnValue (mesh) from previous node
        if idx == 0:
            # Entry node's TargetMesh output -> first GS node's TargetMesh input
            _cmd(conn, "connect_nodes", {
                "blueprint_name": bp_name,
                "source_node_id": entry_node_id,
                "source_pin_name": "TargetMesh",
                "target_node_id": node_id,
                "target_pin_name": "TargetMesh",
                "function_name": func_name,
            })
        else:
            # Previous node's ReturnValue -> this node's TargetMesh
            _cmd(conn, "connect_nodes", {
                "blueprint_name": bp_name,
                "source_node_id": node_ids[idx - 1],
                "source_pin_name": "ReturnValue",
                "target_node_id": node_id,
                "target_pin_name": "TargetMesh",
                "function_name": func_name,
            })

        prev_node_id = node_id
        prev_exec_pin = "then"
        pos_x += 400

    # 6. Compile
    _cmd(conn, "compile_blueprint", {"blueprint_name": bp_name})

    return UEResponse.ok(
        {
            "blueprint_name": bp_name,
            "operations_count": len(operations),
            "node_ids": node_ids,
            "message": f"Geometry Script Blueprint '{bp_name}' created with {len(operations)} operations, compiled successfully",
        },
        tool=TOOL,
        duration_ms=(time.time() - t0) * 1000,
    )
