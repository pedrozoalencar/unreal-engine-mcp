"""Geometry Script deformation tools (bend, twist, flare, noise, smooth)."""
from typing import Dict, Any, Optional


def register_deformation_tools(mcp, get_connection):
    """Register deformation geometry tools with the MCP server."""

    @mcp.tool()
    def gs_bend(
        mesh_name: str,
        angle: float = 45.0,
        lower_bounds: float = -50.0,
        upper_bounds: float = 50.0,
        symmetric: bool = False
    ) -> Dict[str, Any]:
        """Apply a bend warp deformation to a mesh. Bends the mesh along its Z axis.
        angle: bend angle in degrees. bounds: Z range affected."""
        return get_connection().send_command("gs_bend", {
            "mesh_name": mesh_name, "angle": angle,
            "lower_bounds": lower_bounds, "upper_bounds": upper_bounds, "symmetric": symmetric
        })

    @mcp.tool()
    def gs_twist(
        mesh_name: str,
        angle: float = 90.0,
        lower_bounds: float = -50.0,
        upper_bounds: float = 50.0,
        symmetric: bool = False
    ) -> Dict[str, Any]:
        """Apply a twist warp deformation around the Z axis.
        angle: total twist angle in degrees across the bounds range."""
        return get_connection().send_command("gs_twist", {
            "mesh_name": mesh_name, "angle": angle,
            "lower_bounds": lower_bounds, "upper_bounds": upper_bounds, "symmetric": symmetric
        })

    @mcp.tool()
    def gs_flare(
        mesh_name: str,
        flare_x: float = 0.5,
        flare_y: float = 0.5,
        lower_bounds: float = -50.0,
        upper_bounds: float = 50.0,
        symmetric: bool = False
    ) -> Dict[str, Any]:
        """Apply a flare (taper) warp deformation. Scales X/Y based on Z position.
        flare_x/y: percentage of scaling at the top of the bounds."""
        return get_connection().send_command("gs_flare", {
            "mesh_name": mesh_name, "flare_x": flare_x, "flare_y": flare_y,
            "lower_bounds": lower_bounds, "upper_bounds": upper_bounds, "symmetric": symmetric
        })

    @mcp.tool()
    def gs_perlin_noise(
        mesh_name: str,
        magnitude: float = 5.0,
        frequency: float = 0.1
    ) -> Dict[str, Any]:
        """Apply Perlin noise displacement to a mesh surface.
        Creates organic-looking surface variation. Great for terrain patches."""
        return get_connection().send_command("gs_perlin_noise", {
            "mesh_name": mesh_name, "magnitude": magnitude, "frequency": frequency
        })

    @mcp.tool()
    def gs_smooth(
        mesh_name: str,
        iterations: int = 10,
        speed: float = 0.5
    ) -> Dict[str, Any]:
        """Apply iterative Laplacian smoothing to a mesh.
        Reduces sharp edges and noise. More iterations = smoother but loses detail."""
        return get_connection().send_command("gs_smooth", {
            "mesh_name": mesh_name, "iterations": iterations, "speed": speed
        })

