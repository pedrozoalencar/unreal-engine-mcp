"""Geometry Script boolean operation tools."""
from typing import Dict, Any, Optional


def register_boolean_tools(mcp, get_connection):
    """Register boolean geometry tools with the MCP server."""

    @mcp.tool()
    def gs_boolean(
        target_mesh: str,
        tool_mesh: str,
        operation: str = "subtract",
        output_mesh: Optional[str] = None
    ) -> Dict[str, Any]:
        """Apply a boolean operation between two named meshes.
        operation: 'union', 'intersect', or 'subtract' (default).
        output_mesh: name for result (defaults to target_mesh, overwriting it).
        The tool_mesh is unchanged. Example: subtract a sphere from a box to make a hollow."""
        params = {
            "target_mesh": target_mesh,
            "tool_mesh": tool_mesh,
            "operation": operation
        }
        if output_mesh: params["output_mesh"] = output_mesh
        return get_connection().send_command("gs_boolean", params)

    @mcp.tool()
    def gs_self_union(mesh_name: str) -> Dict[str, Any]:
        """Apply self-union to resolve self-intersections in a mesh.
        Useful after combining multiple primitives that overlap."""
        return get_connection().send_command("gs_self_union", {"mesh_name": mesh_name})

    @mcp.tool()
    def gs_append_mesh(
        target_mesh: str,
        source_mesh: str,
        location: Optional[list] = None,
        rotation: Optional[list] = None,
        scale: Optional[list] = None
    ) -> Dict[str, Any]:
        """Append (combine) one mesh into another with an optional transform.
        Unlike boolean union, this simply merges triangles without resolving intersections.
        Much faster than boolean union when you don't need clean topology."""
        params = {"target_mesh": target_mesh, "source_mesh": source_mesh}
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        if scale: params["scale"] = scale
        return get_connection().send_command("gs_append_mesh", params)
