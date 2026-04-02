"""High-level geometry script templates. Each tool creates complex geometry in 1 call."""
import math
import logging
from typing import Dict, Any, Optional, List

logger = logging.getLogger("UnrealMCP_Templates")


def _cmd(conn, command: str, params: dict) -> dict:
    result = conn.send_command(command, params)
    if result and result.get("status") == "error":
        raise RuntimeError(f"{command} failed: {result.get('error', 'unknown')}")
    return result


def register_geometry_template_tools(mcp, get_connection):
    """Register high-level geometry script template tools."""

    @mcp.tool()
    def tpl_gs_cup(
        mesh_name: str,
        outer_radius: float = 45.0,
        height: float = 110.0,
        wall_thickness: float = 7.0,
        base_thickness: float = 10.0,
        segments: int = 32,
        spawn: bool = True,
        spawn_location: List[float] = [0, 0, 0]
    ) -> Dict[str, Any]:
        """Create a complete hollow cup mesh using boolean subtract.
        Creates outer cylinder, inner cavity cylinder, boolean subtracts, and optionally spawns."""
        conn = get_connection()
        if not conn:
            return {"success": False, "error": "Not connected"}

        try:
            inner_radius = outer_radius - wall_thickness
            inner_height = height - base_thickness
            inner_offset_z = base_thickness

            # Outer body
            _cmd(conn, "gs_append_cylinder", {
                "mesh_name": mesh_name, "radius": outer_radius,
                "height": height, "radial_steps": segments
            })

            # Inner cavity
            temp_name = f"__{mesh_name}_cavity"
            _cmd(conn, "gs_append_cylinder", {
                "mesh_name": temp_name, "radius": inner_radius,
                "height": inner_height, "radial_steps": segments,
                "location": [0, 0, inner_offset_z]
            })

            # Boolean subtract
            _cmd(conn, "gs_boolean", {
                "target_mesh": mesh_name, "tool_mesh": temp_name, "operation": "subtract"
            })

            # Cleanup temp
            _cmd(conn, "gs_release_mesh", {"mesh_name": temp_name})

            # Optionally spawn
            if spawn:
                _cmd(conn, "gs_spawn_dynamic_mesh_actor", {
                    "mesh_name": mesh_name, "actor_name": mesh_name,
                    "location": spawn_location
                })

            return {"success": True, "mesh_name": mesh_name,
                    "message": f"Cup created: R={outer_radius}, H={height}, wall={wall_thickness}, base={base_thickness}"}
        except Exception as e:
            return {"success": False, "error": str(e)}

    @mcp.tool()
    def tpl_gs_pipe(
        mesh_name: str,
        outer_radius: float = 30.0,
        wall_thickness: float = 5.0,
        path_points: List[List[float]] = [[0, 0, 0], [0, 0, 200]],
        segments: int = 16,
        spawn: bool = True,
        spawn_location: List[float] = [0, 0, 0]
    ) -> Dict[str, Any]:
        """Create a pipe (hollow tube) along a path using sweep polygon.
        path_points: list of [x,y,z] waypoints for the pipe centerline."""
        conn = get_connection()
        if not conn:
            return {"success": False, "error": "Not connected"}

        try:
            # Create circular cross-section polygon (outer)
            outer_verts = []
            inner_radius = outer_radius - wall_thickness
            for i in range(segments):
                angle = 2 * math.pi * i / segments
                outer_verts.append([outer_radius * math.cos(angle), outer_radius * math.sin(angle)])

            # Sweep the outer tube
            sweep_path = [{"location": p} for p in path_points]
            _cmd(conn, "gs_append_sweep", {
                "mesh_name": mesh_name, "polygon_vertices": outer_verts,
                "sweep_path": sweep_path
            })

            # Create inner tube and subtract
            inner_verts = []
            for i in range(segments):
                angle = 2 * math.pi * i / segments
                inner_verts.append([inner_radius * math.cos(angle), inner_radius * math.sin(angle)])

            temp_name = f"__{mesh_name}_inner"
            _cmd(conn, "gs_append_sweep", {
                "mesh_name": temp_name, "polygon_vertices": inner_verts,
                "sweep_path": sweep_path
            })

            _cmd(conn, "gs_boolean", {
                "target_mesh": mesh_name, "tool_mesh": temp_name, "operation": "subtract"
            })
            _cmd(conn, "gs_release_mesh", {"mesh_name": temp_name})

            if spawn:
                _cmd(conn, "gs_spawn_dynamic_mesh_actor", {
                    "mesh_name": mesh_name, "actor_name": mesh_name,
                    "location": spawn_location
                })

            return {"success": True, "mesh_name": mesh_name, "message": "Pipe created"}
        except Exception as e:
            return {"success": False, "error": str(e)}

    @mcp.tool()
    def tpl_gs_column(
        mesh_name: str,
        radius: float = 30.0,
        height: float = 300.0,
        base_radius: float = 40.0,
        base_height: float = 20.0,
        cap_radius: float = 38.0,
        cap_height: float = 15.0,
        segments: int = 24,
        spawn: bool = True,
        spawn_location: List[float] = [0, 0, 0]
    ) -> Dict[str, Any]:
        """Create a classical column with base, shaft, and capital.
        Uses cylinder primitives composed together."""
        conn = get_connection()
        if not conn:
            return {"success": False, "error": "Not connected"}

        try:
            # Base (wider, short)
            _cmd(conn, "gs_append_cylinder", {
                "mesh_name": mesh_name, "radius": base_radius,
                "height": base_height, "radial_steps": segments
            })

            # Shaft
            shaft = f"__{mesh_name}_shaft"
            _cmd(conn, "gs_append_cylinder", {
                "mesh_name": shaft, "radius": radius,
                "height": height, "radial_steps": segments,
                "location": [0, 0, base_height]
            })
            _cmd(conn, "gs_append_mesh", {
                "target_mesh": mesh_name, "source_mesh": shaft
            })
            _cmd(conn, "gs_release_mesh", {"mesh_name": shaft})

            # Capital (wider, short, on top)
            cap = f"__{mesh_name}_cap"
            _cmd(conn, "gs_append_cylinder", {
                "mesh_name": cap, "radius": cap_radius,
                "height": cap_height, "radial_steps": segments,
                "location": [0, 0, base_height + height]
            })
            _cmd(conn, "gs_append_mesh", {
                "target_mesh": mesh_name, "source_mesh": cap
            })
            _cmd(conn, "gs_release_mesh", {"mesh_name": cap})

            if spawn:
                _cmd(conn, "gs_spawn_dynamic_mesh_actor", {
                    "mesh_name": mesh_name, "actor_name": mesh_name,
                    "location": spawn_location
                })

            return {"success": True, "mesh_name": mesh_name,
                    "message": f"Column created: R={radius}, H={height}"}
        except Exception as e:
            return {"success": False, "error": str(e)}

    @mcp.tool()
    def tpl_gs_terrain(
        mesh_name: str,
        size: float = 1000.0,
        subdivisions: int = 32,
        noise_magnitude: float = 50.0,
        noise_frequency: float = 0.05,
        spawn: bool = True,
        spawn_location: List[float] = [0, 0, 0]
    ) -> Dict[str, Any]:
        """Create a procedural terrain patch using a subdivided box + Perlin noise displacement.
        size: terrain size in XY. subdivisions: detail level. noise_magnitude/frequency: terrain variation."""
        conn = get_connection()
        if not conn:
            return {"success": False, "error": "Not connected"}

        try:
            _cmd(conn, "gs_append_box", {
                "mesh_name": mesh_name,
                "dimension_x": size, "dimension_y": size, "dimension_z": 10.0,
                "steps_x": subdivisions, "steps_y": subdivisions, "steps_z": 0
            })

            _cmd(conn, "gs_perlin_noise", {
                "mesh_name": mesh_name,
                "magnitude": noise_magnitude, "frequency": noise_frequency
            })

            _cmd(conn, "gs_auto_uvs", {"mesh_name": mesh_name})

            if spawn:
                _cmd(conn, "gs_spawn_dynamic_mesh_actor", {
                    "mesh_name": mesh_name, "actor_name": mesh_name,
                    "location": spawn_location
                })

            return {"success": True, "mesh_name": mesh_name,
                    "message": f"Terrain created: {size}x{size}, {subdivisions}x{subdivisions} subdivisions"}
        except Exception as e:
            return {"success": False, "error": str(e)}
