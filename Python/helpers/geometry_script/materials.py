"""Geometry Script material ID tools."""
from typing import Dict, Any


def register_material_tools(mcp, get_connection):
    """Register material ID tools with the MCP server."""

    @mcp.tool()
    def gs_set_material_id(
        mesh_name: str,
        material_id: int = 0,
        all_triangles: bool = True,
        triangle_id: int = -1
    ) -> Dict[str, Any]:
        """Set material ID on mesh triangles.
        all_triangles=True sets all triangles. Otherwise specify triangle_id.
        Material IDs map to material slots when the mesh is used in a component."""
        params = {"mesh_name": mesh_name, "material_id": material_id, "all_triangles": all_triangles}
        if triangle_id >= 0: params["triangle_id"] = triangle_id
        return get_connection().send_command("gs_set_material_id", params)

    @mcp.tool()
    def gs_remap_material_ids(
        mesh_name: str,
        from_material_id: int = 0,
        to_material_id: int = 1
    ) -> Dict[str, Any]:
        """Remap all triangles with from_material_id to to_material_id.
        Useful for reassigning material slots."""
        return get_connection().send_command("gs_remap_material_ids", {
            "mesh_name": mesh_name, "from_material_id": from_material_id, "to_material_id": to_material_id
        })

    @mcp.tool()
    def gs_get_material_ids(mesh_name: str) -> Dict[str, Any]:
        """Get the number of distinct material IDs used in the mesh."""
        return get_connection().send_command("gs_get_material_ids", {"mesh_name": mesh_name})

    @mcp.tool()
    def gs_transform_mesh(
        mesh_name: str,
        location: list = None,
        rotation: list = None,
        scale: list = None
    ) -> Dict[str, Any]:
        """Apply a full transform (location + rotation + scale) to all vertices of a mesh.
        This modifies the actual vertex positions, not just the actor transform."""
        params = {"mesh_name": mesh_name}
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        if scale: params["scale"] = scale
        return get_connection().send_command("gs_transform_mesh", params)

    @mcp.tool()
    def gs_translate(
        mesh_name: str,
        translation: list = None
    ) -> Dict[str, Any]:
        """Translate (move) all vertices of a mesh by a vector [x, y, z]."""
        params = {"mesh_name": mesh_name}
        if translation: params["translation"] = translation
        return get_connection().send_command("gs_translate", params)

    @mcp.tool()
    def gs_scale(
        mesh_name: str,
        scale: list = None,
        uniform_scale: float = None
    ) -> Dict[str, Any]:
        """Scale all vertices of a mesh. Use scale=[x,y,z] for non-uniform or uniform_scale for uniform."""
        params = {"mesh_name": mesh_name}
        if scale: params["scale"] = scale
        if uniform_scale is not None: params["uniform_scale"] = uniform_scale
        return get_connection().send_command("gs_scale", params)

    @mcp.tool()
    def gs_copy_to_static_mesh(
        mesh_name: str,
        asset_path: str
    ) -> Dict[str, Any]:
        """Commit a dynamic mesh to a StaticMesh asset in the Content Browser.
        asset_path: e.g. '/Game/Generated/MyMesh'. Creates the asset if it doesn't exist.
        This is how you materialize procedural geometry into a reusable asset."""
        return get_connection().send_command("gs_copy_to_static_mesh", {
            "mesh_name": mesh_name, "asset_path": asset_path
        })

    @mcp.tool()
    def gs_spawn_dynamic_mesh_actor(
        mesh_name: str,
        actor_name: str = "",
        location: list = None,
        rotation: list = None,
        scale: list = None
    ) -> Dict[str, Any]:
        """Spawn a dynamic mesh as an actor in the current level.
        Places the mesh in the world without creating a static mesh asset."""
        params = {"mesh_name": mesh_name}
        if actor_name: params["actor_name"] = actor_name
        if location: params["location"] = location
        if rotation: params["rotation"] = rotation
        if scale: params["scale"] = scale
        return get_connection().send_command("gs_spawn_dynamic_mesh_actor", params)

    @mcp.tool()
    def gs_copy_from_static_mesh(
        mesh_name: str,
        asset_path: str
    ) -> Dict[str, Any]:
        """Load a StaticMesh asset into a named dynamic mesh for editing.
        asset_path: e.g. '/Game/Meshes/SM_Rock'. The loaded mesh can then be
        modified with boolean, deformation, and other operations."""
        return get_connection().send_command("gs_copy_from_static_mesh", {
            "mesh_name": mesh_name, "asset_path": asset_path
        })
