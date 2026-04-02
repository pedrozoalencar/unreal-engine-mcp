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
        asset_path: e.g. '/Game/Materials/M_Glass'
        blend_mode: Opaque, Translucent, TranslucentColoredTransmittance, TranslucentGreyTransmittance, Additive, Modulate
        shading_model: DefaultLit, Unlit, ThinTranslucent, ClearCoat, SubsurfaceProfile"""
        return get_connection().send_command("mat_create_material", {
            "asset_path": asset_path, "blend_mode": blend_mode,
            "shading_model": shading_model, "two_sided": two_sided
        })

    @mcp.tool()
    def mat_set_property(
        asset_path: str,
        property_name: str,
        property_value: Any = None
    ) -> Dict[str, Any]:
        """Set a material property by name.
        property_name: blend_mode, shading_model, two_sided, translucency_lighting_mode, etc.
        property_value: the value (string for enums, bool for flags)."""
        return get_connection().send_command("mat_set_material_property", {
            "asset_path": asset_path, "property_name": property_name,
            "property_value": str(property_value)
        })

    @mcp.tool()
    def mat_get_info(asset_path: str) -> Dict[str, Any]:
        """Get material info: settings, list of expression nodes with classes and names."""
        return get_connection().send_command("mat_get_material_info", {"asset_path": asset_path})

    @mcp.tool()
    def mat_add_expression(
        asset_path: str,
        expression_class: str,
        pos_x: int = 0,
        pos_y: int = 0,
        name: str = ""
    ) -> Dict[str, Any]:
        """Add a material expression node by class name.
        expression_class: e.g. 'MaterialExpressionConstant', 'MaterialExpressionConstant3Vector',
        'MaterialExpressionFresnel', 'MaterialExpressionLinearInterpolate',
        'MaterialExpressionMultiply', 'MaterialExpressionSubstrateSlabBSDF',
        'MaterialExpressionSubstrateTransmittanceToMFP', 'MaterialExpressionScalarParameter',
        'MaterialExpressionVectorParameter', etc.
        Returns node index for use in connections."""
        params = {"asset_path": asset_path, "expression_class": expression_class,
                  "pos_x": pos_x, "pos_y": pos_y}
        if name: params["name"] = name
        return get_connection().send_command("mat_add_expression", params)

    @mcp.tool()
    def mat_delete_expression(
        asset_path: str,
        node_index: int
    ) -> Dict[str, Any]:
        """Delete a material expression node by its index."""
        return get_connection().send_command("mat_delete_expression", {
            "asset_path": asset_path, "node_index": node_index
        })

    @mcp.tool()
    def mat_list_expressions(asset_path: str) -> Dict[str, Any]:
        """List all expression nodes in a material with their class, name, position, and index."""
        return get_connection().send_command("mat_list_expressions", {"asset_path": asset_path})

    @mcp.tool()
    def mat_list_pins(
        asset_path: str,
        node_index: int
    ) -> Dict[str, Any]:
        """List ALL input and output pins of a material expression node.
        Essential for discovering Substrate BSDF pin names.
        Returns pin name, type, direction, and whether it's connected."""
        return get_connection().send_command("mat_list_pins", {
            "asset_path": asset_path, "node_index": node_index
        })

    @mcp.tool()
    def mat_connect(
        asset_path: str,
        source_index: int,
        source_output: int,
        target_index: int,
        target_input: str
    ) -> Dict[str, Any]:
        """Connect output of one expression to input of another.
        source_index: index of the source expression node
        source_output: output pin index (usually 0)
        target_index: index of the target expression node
        target_input: name of the input pin on the target (e.g. 'DiffuseAlbedo', 'F0', 'Roughness')
        Use mat_list_pins to discover available pin names."""
        return get_connection().send_command("mat_connect_expressions", {
            "asset_path": asset_path, "source_index": source_index,
            "source_output": source_output, "target_index": target_index,
            "target_input": target_input
        })

    @mcp.tool()
    def mat_connect_to_output(
        asset_path: str,
        source_index: int,
        source_output: int,
        material_property: str
    ) -> Dict[str, Any]:
        """Connect an expression to a material output property.
        material_property: BaseColor, Roughness, Metallic, Specular, Normal,
        Opacity, OpacityMask, Refraction, EmissiveColor, WorldPositionOffset,
        AmbientOcclusion, FrontMaterial (for Substrate), etc."""
        return get_connection().send_command("mat_connect_to_output", {
            "asset_path": asset_path, "source_index": source_index,
            "source_output": source_output, "material_property": material_property
        })

    @mcp.tool()
    def mat_set_expression_value(
        asset_path: str,
        node_index: int,
        property_name: str,
        value: Any = None
    ) -> Dict[str, Any]:
        """Set a value on a material expression node.
        For Constant: property_name='R', value=1.52
        For Constant3Vector: property_name='Constant', value=[0.8, 0.9, 0.95]
        For Fresnel: property_name='Exponent' or 'BaseReflectFractionIn'
        For ScalarParameter: property_name='DefaultValue'"""
        return get_connection().send_command("mat_set_expression_value", {
            "asset_path": asset_path, "node_index": node_index,
            "property_name": property_name, "value": str(value)
        })

    @mcp.tool()
    def mat_compile(asset_path: str) -> Dict[str, Any]:
        """Recompile and save a material. Call after making changes."""
        return get_connection().send_command("mat_compile_material", {"asset_path": asset_path})

    @mcp.tool()
    def mat_apply_to_actor(
        asset_path: str,
        actor_name: str,
        slot_index: int = 0
    ) -> Dict[str, Any]:
        """Apply a material to an actor's mesh component at the given material slot."""
        return get_connection().send_command("mat_apply_material_to_actor", {
            "asset_path": asset_path, "actor_name": actor_name, "slot_index": slot_index
        })

    @mcp.tool()
    def mat_disconnect(
        asset_path: str,
        node_index: int,
        input_name: str
    ) -> Dict[str, Any]:
        """Disconnect an input pin on an expression node."""
        return get_connection().send_command("mat_disconnect_expression", {
            "asset_path": asset_path, "node_index": node_index, "input_name": input_name
        })
