"""Material creation, editing, and Substrate BSDF tools."""
from typing import Dict, Any, Optional, List


def register_material_tools(mcp, get_connection):
    """Register all material MCP tools."""

    @mcp.tool()
    def mat_create_material(
        asset_path: str,
        blend_mode: str = "Opaque",
        shading_model: str = "DefaultLit",
        two_sided: bool = False
    ) -> Dict[str, Any]:
        """Create a new material asset.
        blend_mode: Opaque, Translucent, TranslucentColoredTransmittance, TranslucentGreyTransmittance, Additive, Modulate
        shading_model: DefaultLit, Unlit, ThinTranslucent, ClearCoat, SubsurfaceProfile"""
        return get_connection().send_command("mat_create_material", {
            "asset_path": asset_path, "blend_mode": blend_mode,
            "shading_model": shading_model, "two_sided": two_sided
        })

    @mcp.tool()
    def mat_set_property(
        asset_path: str, property_name: str, property_value: Any = None
    ) -> Dict[str, Any]:
        """Set a material property: blend_mode, shading_model, two_sided, translucency_lighting_mode."""
        return get_connection().send_command("mat_set_material_property", {
            "asset_path": asset_path, "property_name": property_name,
            "property_value": str(property_value)
        })

    @mcp.tool()
    def mat_get_info(asset_path: str) -> Dict[str, Any]:
        """Get material info: settings and list of expression nodes."""
        return get_connection().send_command("mat_get_material_info", {"asset_path": asset_path})

    @mcp.tool()
    def mat_add_expression(
        asset_path: str, expression_class: str,
        pos_x: int = 0, pos_y: int = 0, name: str = ""
    ) -> Dict[str, Any]:
        """Add a material expression node. Returns node_id for connections.
        expression_class: MaterialExpressionConstant, MaterialExpressionConstant3Vector,
        MaterialExpressionFresnel, MaterialExpressionLinearInterpolate, MaterialExpressionMultiply,
        MaterialExpressionSubstrateSlabBSDF, MaterialExpressionSubstrateTransmittanceToMFP, etc."""
        params = {"asset_path": asset_path, "class_name": expression_class,
                  "pos_x": pos_x, "pos_y": pos_y}
        if name: params["name"] = name
        return get_connection().send_command("mat_add_expression", params)

    @mcp.tool()
    def mat_delete_expression(asset_path: str, node_id: str) -> Dict[str, Any]:
        """Delete a material expression node by its ID."""
        return get_connection().send_command("mat_delete_expression", {
            "asset_path": asset_path, "node_id": node_id
        })

    @mcp.tool()
    def mat_list_expressions(asset_path: str) -> Dict[str, Any]:
        """List all expression nodes in a material with class, name, position, and ID."""
        return get_connection().send_command("mat_list_expressions", {"asset_path": asset_path})

    @mcp.tool()
    def mat_list_pins(asset_path: str, node_id: str) -> Dict[str, Any]:
        """List ALL input and output pins of a material expression node.
        Essential for discovering Substrate BSDF pin names."""
        return get_connection().send_command("mat_list_pins", {
            "asset_path": asset_path, "node_id": node_id
        })

    @mcp.tool()
    def mat_connect(
        asset_path: str, source_node_id: str, target_node_id: str,
        input_pin_name: str, source_output_index: int = 0
    ) -> Dict[str, Any]:
        """Connect output of one expression to input of another.
        source_node_id: ID of the source node. target_node_id: ID of the target node.
        input_pin_name: name of the input pin (use mat_list_pins to discover).
        source_output_index: output pin index (usually 0)."""
        return get_connection().send_command("mat_connect_expressions", {
            "asset_path": asset_path, "source_node_id": source_node_id,
            "target_node_id": target_node_id, "input_pin_name": input_pin_name,
            "output_index": source_output_index
        })

    @mcp.tool()
    def mat_connect_to_output(
        asset_path: str, source_node_id: str,
        material_property: str, source_output_index: int = 0
    ) -> Dict[str, Any]:
        """Connect an expression to a material output property.
        material_property: BaseColor, Roughness, Metallic, Specular, Normal,
        Opacity, Refraction, EmissiveColor, FrontMaterial (for Substrate), etc."""
        return get_connection().send_command("mat_connect_to_output", {
            "asset_path": asset_path, "source_node_id": source_node_id,
            "property_name": material_property, "output_index": source_output_index
        })

    @mcp.tool()
    def mat_set_expression_value(
        asset_path: str, node_id: str, property_name: str, value: Any = None
    ) -> Dict[str, Any]:
        """Set a value on a material expression node.
        Constant: property_name='R', value=1.52
        Constant3Vector: property_name='Constant', value='(R=0.8,G=0.9,B=0.95,A=1.0)'
        Fresnel: 'Exponent' or 'BaseReflectFraction'
        ScalarParameter: 'DefaultValue'"""
        return get_connection().send_command("mat_set_expression_value", {
            "asset_path": asset_path, "node_id": node_id,
            "property_name": property_name, "value": str(value)
        })

    @mcp.tool()
    def mat_compile(asset_path: str) -> Dict[str, Any]:
        """Recompile and save a material."""
        return get_connection().send_command("mat_compile_material", {"asset_path": asset_path})

    @mcp.tool()
    def mat_apply_to_actor(
        asset_path: str, actor_name: str, slot_index: int = 0
    ) -> Dict[str, Any]:
        """Apply a material to an actor's mesh component."""
        return get_connection().send_command("mat_apply_material_to_actor", {
            "asset_path": asset_path, "actor_name": actor_name, "slot_index": slot_index
        })

    @mcp.tool()
    def mat_disconnect(asset_path: str, node_id: str, input_name: str) -> Dict[str, Any]:
        """Disconnect an input pin on an expression node."""
        return get_connection().send_command("mat_disconnect_expression", {
            "asset_path": asset_path, "node_id": node_id, "input_pin_name": input_name
        })
