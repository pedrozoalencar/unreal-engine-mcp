"""Geometry Script primitive generation tools."""
from typing import Dict, Any, List, Optional


def register_primitive_tools(mcp, get_connection):
    """Register all primitive geometry tools with the MCP server."""

    @mcp.tool()
    def gs_create_mesh(mesh_name: str) -> Dict[str, Any]:
        """Create an empty named dynamic mesh in the Geometry Script registry.
        This mesh can then be modified with append, boolean, and deformation operations.
        Must be called before other gs_* operations on a new mesh name."""
        return get_connection().send_command("gs_create_mesh", {"mesh_name": mesh_name})

    @mcp.tool()
    def gs_release_mesh(mesh_name: str) -> Dict[str, Any]:
        """Release a named dynamic mesh from the registry, freeing its memory."""
        return get_connection().send_command("gs_release_mesh", {"mesh_name": mesh_name})

    @mcp.tool()
    def gs_list_meshes() -> Dict[str, Any]:
        """List all dynamic meshes currently in the registry with their vertex/triangle counts."""
        return get_connection().send_command("gs_list_meshes", {})

    @mcp.tool()
    def gs_append_box(
        mesh_name: str,
        dimension_x: float = 100.0,
        dimension_y: float = 100.0,
        dimension_z: float = 100.0,
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None,
        steps_x: int = 0,
        steps_y: int = 0,
        steps_z: int = 0
    ) -> Dict[str, Any]:
        """Append a box primitive to a named dynamic mesh.
        Creates the mesh if it doesn't exist. Steps add subdivisions."""
        params = {
            "mesh_name": mesh_name,
            "dimension_x": dimension_x,
            "dimension_y": dimension_y,
            "dimension_z": dimension_z,
            "steps_x": steps_x,
            "steps_y": steps_y,
            "steps_z": steps_z
        }
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        return get_connection().send_command("gs_append_box", params)

    @mcp.tool()
    def gs_append_sphere(
        mesh_name: str,
        radius: float = 50.0,
        steps_x: int = 16,
        steps_y: int = 16,
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """Append a sphere primitive to a named dynamic mesh."""
        params = {"mesh_name": mesh_name, "radius": radius, "steps_x": steps_x, "steps_y": steps_y}
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        return get_connection().send_command("gs_append_sphere", params)

    @mcp.tool()
    def gs_append_cylinder(
        mesh_name: str,
        radius: float = 50.0,
        height: float = 100.0,
        radial_steps: int = 16,
        height_steps: int = 0,
        capped: bool = True,
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """Append a cylinder primitive to a named dynamic mesh."""
        params = {
            "mesh_name": mesh_name, "radius": radius, "height": height,
            "radial_steps": radial_steps, "height_steps": height_steps, "capped": capped
        }
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        return get_connection().send_command("gs_append_cylinder", params)

    @mcp.tool()
    def gs_append_cone(
        mesh_name: str,
        base_radius: float = 50.0,
        top_radius: float = 0.0,
        height: float = 100.0,
        radial_steps: int = 16,
        height_steps: int = 0,
        capped: bool = True,
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """Append a cone (or truncated cone) primitive. Set top_radius > 0 for frustum."""
        params = {
            "mesh_name": mesh_name, "base_radius": base_radius, "top_radius": top_radius,
            "height": height, "radial_steps": radial_steps, "height_steps": height_steps, "capped": capped
        }
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        return get_connection().send_command("gs_append_cone", params)

    @mcp.tool()
    def gs_append_torus(
        mesh_name: str,
        inner_radius: float = 25.0,
        outer_radius: float = 50.0,
        cross_section_steps: int = 16,
        circle_steps: int = 16,
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """Append a torus (donut shape) primitive to a named dynamic mesh."""
        params = {
            "mesh_name": mesh_name, "inner_radius": inner_radius, "outer_radius": outer_radius,
            "cross_section_steps": cross_section_steps, "circle_steps": circle_steps
        }
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        return get_connection().send_command("gs_append_torus", params)

    @mcp.tool()
    def gs_append_capsule(
        mesh_name: str,
        radius: float = 30.0,
        line_length: float = 75.0,
        hemisphere_steps: int = 5,
        circle_steps: int = 16,
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """Append a capsule (cylinder with hemisphere caps) to a named dynamic mesh."""
        params = {
            "mesh_name": mesh_name, "radius": radius, "line_length": line_length,
            "hemisphere_steps": hemisphere_steps, "circle_steps": circle_steps
        }
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        return get_connection().send_command("gs_append_capsule", params)

    @mcp.tool()
    def gs_append_revolve(
        mesh_name: str,
        path_points: List[List[float]],
        revolve_degrees: float = 360.0,
        steps: int = 16,
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """Revolve a 2D profile path around the Y axis to create a surface of revolution.
        path_points: list of [x, y] coordinates defining the profile (min 2 points).
        Use revolve_degrees < 360 for partial revolution (e.g. 180 for half)."""
        params = {
            "mesh_name": mesh_name, "path_points": path_points,
            "revolve_degrees": revolve_degrees, "steps": steps
        }
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        return get_connection().send_command("gs_append_revolve", params)

    @mcp.tool()
    def gs_append_sweep(
        mesh_name: str,
        polygon_vertices: List[List[float]],
        sweep_path: List[dict],
        loop: bool = False,
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """Sweep a 2D polygon along a 3D path to create extruded geometry.
        polygon_vertices: list of [x, y] defining the cross-section (min 3 vertices).
        sweep_path: list of {"location": [x, y, z]} defining the path (min 2 points).
        loop: whether to close the sweep path into a ring."""
        params = {
            "mesh_name": mesh_name, "polygon_vertices": polygon_vertices,
            "sweep_path": sweep_path, "loop": loop
        }
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        return get_connection().send_command("gs_append_sweep", params)
