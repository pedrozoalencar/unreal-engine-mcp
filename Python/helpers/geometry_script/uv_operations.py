"""Geometry Script UV operation tools."""
from typing import Dict, Any, Optional, List


def register_uv_tools(mcp, get_connection):
    """Register UV operation tools with the MCP server."""

    @mcp.tool()
    def gs_auto_uvs(
        mesh_name: str,
        uv_channel: int = 0
    ) -> Dict[str, Any]:
        """Auto-generate patch-based UVs for a mesh.
        Best general-purpose UV unwrapping for procedural geometry."""
        return get_connection().send_command("gs_auto_uvs", {
            "mesh_name": mesh_name, "uv_channel": uv_channel
        })

    @mcp.tool()
    def gs_planar_projection(
        mesh_name: str,
        uv_channel: int = 0,
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """Project UVs from a plane onto the mesh. Good for flat surfaces.
        The plane orientation is defined by location and rotation."""
        params = {"mesh_name": mesh_name, "uv_channel": uv_channel}
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        return get_connection().send_command("gs_planar_projection", params)

    @mcp.tool()
    def gs_box_projection(
        mesh_name: str,
        uv_channel: int = 0,
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """Project UVs using a box projection. Good for boxy shapes.
        Projects from 6 cube faces, each face picks the closest projection."""
        params = {"mesh_name": mesh_name, "uv_channel": uv_channel}
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        return get_connection().send_command("gs_box_projection", params)
