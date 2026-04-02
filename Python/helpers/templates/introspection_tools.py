"""Introspection tools - discover UE classes, functions, enums at runtime."""
from typing import Dict, Any


def register_introspection_tools(mcp, get_connection):

    @mcp.tool()
    def inspect_class(
        class_name: str,
        include_inherited: bool = False
    ) -> Dict[str, Any]:
        """List all BlueprintCallable functions and visible properties of any UE class.
        class_name: e.g. 'Actor', 'StaticMeshComponent', 'GeneratedDynamicMeshActor'
        include_inherited: include parent class members (can be large!)
        Use this to discover what functions/properties are available before using them."""
        return get_connection().send_command("inspect_class", {
            "class_name": class_name, "include_inherited": include_inherited
        })

    @mcp.tool()
    def inspect_enum(enum_name: str) -> Dict[str, Any]:
        """List all values of a UEnum type.
        enum_name: e.g. 'EBlendMode', 'EGeometryScriptBooleanOperation'
        Use this to discover valid parameter values for enum-typed pins."""
        return get_connection().send_command("inspect_enum", {"enum_name": enum_name})
