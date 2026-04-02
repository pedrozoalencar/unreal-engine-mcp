"""Geometry Script modeling operation tools (offset, shell, extrude faces, bevel, inset)."""
from typing import Dict, Any, Optional, List


def register_modeling_tools(mcp, get_connection):
    """Register modeling geometry tools with the MCP server."""

    @mcp.tool()
    def gs_offset(
        mesh_name: str,
        distance: float = 1.0,
        iterations: int = 5,
        fixed_boundary: bool = False
    ) -> Dict[str, Any]:
        """Offset mesh surface uniformly by a distance (inflate/deflate).
        Positive distance inflates, negative deflates."""
        params = {
            "mesh_name": mesh_name, "distance": distance,
            "iterations": iterations, "fixed_boundary": fixed_boundary
        }
        return get_connection().send_command("gs_offset", params)

    @mcp.tool()
    def gs_shell(
        mesh_name: str,
        thickness: float = 5.0
    ) -> Dict[str, Any]:
        """Create a hollow shell from a solid mesh with the given wall thickness."""
        return get_connection().send_command("gs_shell", {"mesh_name": mesh_name, "thickness": thickness})

    @mcp.tool()
    def gs_linear_extrude_faces(
        mesh_name: str,
        distance: float = 10.0,
        direction: Optional[List[float]] = None,
        material_id: Optional[int] = None
    ) -> Dict[str, Any]:
        """Extrude faces of a mesh linearly along a direction.
        direction: [x,y,z] fixed direction. If omitted, extrudes along average face normals.
        material_id: only extrude faces with this material ID. If omitted, extrudes all faces."""
        params = {"mesh_name": mesh_name, "distance": distance}
        if direction: params["direction"] = direction
        if material_id is not None: params["material_id"] = material_id
        return get_connection().send_command("gs_linear_extrude_faces", params)

    @mcp.tool()
    def gs_offset_faces(
        mesh_name: str,
        distance: float = 1.0,
        material_id: Optional[int] = None
    ) -> Dict[str, Any]:
        """Offset selected faces along their normals (push in/out).
        material_id: only offset faces with this material ID. If omitted, offsets all faces."""
        params = {"mesh_name": mesh_name, "distance": distance}
        if material_id is not None: params["material_id"] = material_id
        return get_connection().send_command("gs_offset_faces", params)

    @mcp.tool()
    def gs_inset_outset_faces(
        mesh_name: str,
        distance: float = 1.0,
        reproject: bool = True,
        softness: float = 0.0,
        material_id: Optional[int] = None
    ) -> Dict[str, Any]:
        """Inset (positive distance) or outset (negative) faces within their plane.
        Creates a border ring around selected faces. Great for adding detail.
        material_id: only inset faces with this material ID."""
        params = {"mesh_name": mesh_name, "distance": distance, "reproject": reproject, "softness": softness}
        if material_id is not None: params["material_id"] = material_id
        return get_connection().send_command("gs_inset_outset_faces", params)

    @mcp.tool()
    def gs_bevel(
        mesh_name: str,
        distance: float = 1.0,
        subdivisions: int = 0,
        round_weight: float = 1.0
    ) -> Dict[str, Any]:
        """Bevel all polygroup edges on a mesh. Rounds sharp edges.
        subdivisions: extra edge loops for smoother rounding.
        round_weight: 0=flat, 1=round."""
        return get_connection().send_command("gs_bevel", {
            "mesh_name": mesh_name, "distance": distance,
            "subdivisions": subdivisions, "round_weight": round_weight
        })
