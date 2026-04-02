"""Geometry Script mesh query and spatial tools."""
from typing import Dict, Any, Optional, List


def register_query_tools(mcp, get_connection):
    """Register mesh query and spatial tools with the MCP server."""

    @mcp.tool()
    def gs_query_mesh_info(mesh_name: str) -> Dict[str, Any]:
        """Get basic mesh info: vertex count, triangle count, and bounding box."""
        return get_connection().send_command("gs_query_mesh_info", {"mesh_name": mesh_name})

    @mcp.tool()
    def gs_query_vertices(
        mesh_name: str,
        max_vertices: int = 100
    ) -> Dict[str, Any]:
        """Get vertex positions from a mesh. Returns up to max_vertices positions."""
        return get_connection().send_command("gs_query_vertices", {
            "mesh_name": mesh_name, "max_vertices": max_vertices
        })

    @mcp.tool()
    def gs_get_bounds(mesh_name: str) -> Dict[str, Any]:
        """Get detailed bounding box: min, max, center, and extent for each axis."""
        return get_connection().send_command("gs_get_bounds", {"mesh_name": mesh_name})

    @mcp.tool()
    def gs_raycast(
        mesh_name: str,
        ray_origin: List[float] = [0, 0, 1000],
        ray_direction: List[float] = [0, 0, -1]
    ) -> Dict[str, Any]:
        """Cast a ray against a mesh using BVH acceleration.
        Returns hit position, triangle ID, and ray parameter if hit."""
        return get_connection().send_command("gs_raycast", {
            "mesh_name": mesh_name, "ray_origin": ray_origin, "ray_direction": ray_direction
        })

    @mcp.tool()
    def gs_is_point_inside(
        mesh_name: str,
        point: List[float] = [0, 0, 0]
    ) -> Dict[str, Any]:
        """Test if a point is inside a closed mesh volume. Uses BVH for fast queries."""
        return get_connection().send_command("gs_is_point_inside", {
            "mesh_name": mesh_name, "point": point
        })

    @mcp.tool()
    def gs_select_all(mesh_name: str) -> Dict[str, Any]:
        """Select all triangles in a mesh. Returns the count of selected elements."""
        return get_connection().send_command("gs_select_all", {"mesh_name": mesh_name})

    @mcp.tool()
    def gs_select_by_material(
        mesh_name: str,
        material_id: int = 0
    ) -> Dict[str, Any]:
        """Select all triangles with a specific material ID."""
        return get_connection().send_command("gs_select_by_material", {
            "mesh_name": mesh_name, "material_id": material_id
        })

    @mcp.tool()
    def gs_select_in_box(
        mesh_name: str,
        box_min: List[float] = [0, 0, 0],
        box_max: List[float] = [100, 100, 100]
    ) -> Dict[str, Any]:
        """Select triangles inside a bounding box."""
        return get_connection().send_command("gs_select_in_box", {
            "mesh_name": mesh_name, "box_min": box_min, "box_max": box_max
        })

    @mcp.tool()
    def gs_select_by_normal(
        mesh_name: str,
        normal: List[float] = [0, 0, 1],
        max_angle: float = 30.0
    ) -> Dict[str, Any]:
        """Select triangles whose normal is within max_angle degrees of the given direction.
        normal=[0,0,1] with max_angle=30 selects upward-facing faces."""
        return get_connection().send_command("gs_select_by_normal", {
            "mesh_name": mesh_name, "normal": normal, "max_angle": max_angle
        })
