"""High-level material creation templates. Each tool creates a complete material in 1 call."""
import logging
from typing import Dict, Any, Optional, List

logger = logging.getLogger("UnrealMCP_Templates")


def _cmd(conn, command: str, params: dict) -> dict:
    """Send command and return result, raising on failure."""
    result = conn.send_command(command, params)
    if result and result.get("status") == "error":
        raise RuntimeError(f"Command {command} failed: {result.get('error', 'unknown')}")
    return result


def register_material_template_tools(mcp, get_connection):
    """Register high-level material template tools."""

    @mcp.tool()
    def tpl_mat_glass(
        asset_path: str,
        tint_color: List[float] = [0.85, 0.92, 0.97],
        ior: float = 1.52,
        roughness: float = 0.02,
        thickness: float = 5.0,
        two_sided: bool = True,
        apply_to_actor: str = ""
    ) -> Dict[str, Any]:
        """Create a complete Substrate glass material with SlabBSDF, TransmittanceToMFP,
        Fresnel refraction, and all nodes connected. Ready to use.
        tint_color: [R,G,B] for glass color tint (white = clear glass).
        ior: index of refraction (1.52 = standard glass, 1.33 = water, 2.42 = diamond).
        thickness: controls light absorption through the glass volume.
        apply_to_actor: optional actor name to immediately apply the material."""
        conn = get_connection()
        if not conn:
            return {"success": False, "error": "Not connected to Unreal"}

        try:
            # 1. Create material
            _cmd(conn, "mat_create_material", {
                "asset_path": asset_path,
                "blend_mode": "TranslucentColoredTransmittance",
                "shading_model": "DefaultLit",
                "two_sided": two_sided
            })

            path = asset_path

            # 2. Add Substrate Slab BSDF
            slab = _cmd(conn, "mat_add_expression", {
                "asset_path": path, "class_name": "MaterialExpressionSubstrateSlabBSDF",
                "pos_x": 0, "pos_y": 0
            })
            slab_id = slab.get("result", {}).get("node_id", "0")

            # 3. Diffuse Albedo = black (glass has no diffuse scattering)
            diff = _cmd(conn, "mat_add_expression", {
                "asset_path": path, "class_name": "MaterialExpressionConstant3Vector",
                "pos_x": -400, "pos_y": -100
            })
            diff_id = diff.get("result", {}).get("node_id", "1")

            # 4. F0 = 0.04 (4% reflectance at normal incidence for glass)
            f0 = _cmd(conn, "mat_add_expression", {
                "asset_path": path, "class_name": "MaterialExpressionConstant",
                "pos_x": -400, "pos_y": 50
            })
            f0_id = f0.get("result", {}).get("node_id", "2")

            # 5. F90 = 1.0 (100% reflectance at grazing angle)
            f90 = _cmd(conn, "mat_add_expression", {
                "asset_path": path, "class_name": "MaterialExpressionConstant",
                "pos_x": -400, "pos_y": 120
            })
            f90_id = f90.get("result", {}).get("node_id", "3")

            # 6. Roughness
            rg = _cmd(conn, "mat_add_expression", {
                "asset_path": path, "class_name": "MaterialExpressionConstant",
                "pos_x": -400, "pos_y": 190
            })
            rg_id = rg.get("result", {}).get("node_id", "4")

            # 7. Transmittance-To-MeanFreePath
            trans = _cmd(conn, "mat_add_expression", {
                "asset_path": path, "class_name": "MaterialExpressionSubstrateTransmittanceToMFP",
                "pos_x": -300, "pos_y": 300
            })
            trans_id = trans.get("result", {}).get("node_id", "5")

            # 8. Transmittance color (tint)
            tint = _cmd(conn, "mat_add_expression", {
                "asset_path": path, "class_name": "MaterialExpressionConstant3Vector",
                "pos_x": -600, "pos_y": 280
            })
            tint_id = tint.get("result", {}).get("node_id", "6")

            # 9. Thickness constant
            thick = _cmd(conn, "mat_add_expression", {
                "asset_path": path, "class_name": "MaterialExpressionConstant",
                "pos_x": -600, "pos_y": 380
            })
            thick_id = thick.get("result", {}).get("node_id", "7")

            # 10. Fresnel for refraction
            fresnel = _cmd(conn, "mat_add_expression", {
                "asset_path": path, "class_name": "MaterialExpressionFresnel",
                "pos_x": -600, "pos_y": 500
            })
            fresnel_id = fresnel.get("result", {}).get("node_id", "8")

            # 11. Lerp for IOR-based refraction
            lerp = _cmd(conn, "mat_add_expression", {
                "asset_path": path, "class_name": "MaterialExpressionLinearInterpolate",
                "pos_x": -300, "pos_y": 500
            })
            lerp_id = lerp.get("result", {}).get("node_id", "9")

            # 12. IOR constants for Lerp A and B
            ior_a = _cmd(conn, "mat_add_expression", {
                "asset_path": path, "class_name": "MaterialExpressionConstant",
                "pos_x": -600, "pos_y": 470
            })
            ior_a_id = ior_a.get("result", {}).get("node_id", "10")

            ior_b = _cmd(conn, "mat_add_expression", {
                "asset_path": path, "class_name": "MaterialExpressionConstant",
                "pos_x": -600, "pos_y": 540
            })
            ior_b_id = ior_b.get("result", {}).get("node_id", "11")

            # Set values
            _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": diff_id, "property_name": "Constant", "value": f"(R=0,G=0,B=0,A=1)"})
            _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": f0_id, "property_name": "R", "value": "0.04"})
            _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": f90_id, "property_name": "R", "value": "1.0"})
            _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": rg_id, "property_name": "R", "value": str(roughness)})
            _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": tint_id, "property_name": "Constant", "value": f"(R={tint_color[0]},G={tint_color[1]},B={tint_color[2]},A=1)"})
            _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": thick_id, "property_name": "R", "value": str(thickness)})
            _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": fresnel_id, "property_name": "Exponent", "value": "3.0"})
            _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": fresnel_id, "property_name": "BaseReflectFraction", "value": "0.04"})
            _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": ior_a_id, "property_name": "R", "value": "1.0"})
            _cmd(conn, "mat_set_expression_value", {"asset_path": path, "node_id": ior_b_id, "property_name": "R", "value": str(ior)})

            # Connect: DiffuseAlbedo, F0, F90, Roughness -> SlabBSDF
            _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": diff_id, "target_node_id": slab_id, "input_pin_name": "Diffuse Albedo"})
            _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": f0_id, "target_node_id": slab_id, "input_pin_name": "F0"})
            _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": f90_id, "target_node_id": slab_id, "input_pin_name": "F90"})
            _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": rg_id, "target_node_id": slab_id, "input_pin_name": "Roughness"})

            # Connect: TintColor -> TransmittanceToMFP, Thickness -> TransmittanceToMFP
            _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": tint_id, "target_node_id": trans_id, "input_pin_name": "TransmittanceColor"})
            _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": thick_id, "target_node_id": trans_id, "input_pin_name": "Thickness"})

            # Connect: TransmittanceToMFP -> SlabBSDF SSS MFP
            _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": trans_id, "target_node_id": slab_id, "input_pin_name": "SSS MFP"})

            # Connect: SlabBSDF -> FrontMaterial
            _cmd(conn, "mat_connect_to_output", {"asset_path": path, "source_node_id": slab_id, "property_name": "FrontMaterial"})

            # Connect: Fresnel -> Lerp Alpha, IOR values -> Lerp A/B, Lerp -> Refraction
            _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": ior_a_id, "target_node_id": lerp_id, "input_pin_name": "A"})
            _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": ior_b_id, "target_node_id": lerp_id, "input_pin_name": "B"})
            _cmd(conn, "mat_connect_expressions", {"asset_path": path, "source_node_id": fresnel_id, "target_node_id": lerp_id, "input_pin_name": "Alpha"})
            _cmd(conn, "mat_connect_to_output", {"asset_path": path, "source_node_id": lerp_id, "property_name": "Refraction"})

            # Compile
            _cmd(conn, "mat_compile_material", {"asset_path": path})

            # Optional: apply to actor
            if apply_to_actor:
                _cmd(conn, "mat_apply_material_to_actor", {
                    "asset_path": path, "actor_name": apply_to_actor, "slot_index": 0
                })

            return {"success": True, "asset_path": path, "message": "Glass material created with Substrate SlabBSDF + TransmittanceToMFP + Fresnel refraction"}

        except Exception as e:
            logger.error(f"tpl_mat_glass failed: {e}")
            return {"success": False, "error": str(e)}

    @mcp.tool()
    def tpl_mat_pbr(
        asset_path: str,
        base_color: List[float] = [0.5, 0.5, 0.5],
        roughness: float = 0.5,
        metallic: float = 0.0,
        apply_to_actor: str = ""
    ) -> Dict[str, Any]:
        """Create a complete PBR material with Substrate SlabBSDF. Ready to use.
        base_color: [R,G,B] diffuse color. roughness: 0=mirror, 1=rough. metallic: 0=dielectric, 1=metal."""
        conn = get_connection()
        if not conn:
            return {"success": False, "error": "Not connected"}

        try:
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

            return {"success": True, "asset_path": path, "message": "PBR material created"}
        except Exception as e:
            return {"success": False, "error": str(e)}

    @mcp.tool()
    def tpl_mat_emissive(
        asset_path: str,
        color: List[float] = [1.0, 0.5, 0.0],
        intensity: float = 10.0,
        apply_to_actor: str = ""
    ) -> Dict[str, Any]:
        """Create an emissive material. Great for glowing objects, lights, and UI elements.
        color: [R,G,B] emissive color. intensity: multiplier for brightness."""
        conn = get_connection()
        if not conn:
            return {"success": False, "error": "Not connected"}

        try:
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

            return {"success": True, "asset_path": path, "message": "Emissive material created"}
        except Exception as e:
            return {"success": False, "error": str(e)}
