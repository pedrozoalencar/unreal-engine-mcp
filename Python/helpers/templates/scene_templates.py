"""High-level scene composition templates. Spawn, arrange, and style actors in 1 call."""
import math
import logging
from typing import Dict, Any, Optional, List

logger = logging.getLogger("UnrealMCP_Templates")


def _cmd(conn, command: str, params: dict) -> dict:
    result = conn.send_command(command, params)
    if result and result.get("status") == "error":
        raise RuntimeError(f"{command} failed: {result.get('error', 'unknown')}")
    return result


def register_scene_template_tools(mcp, get_connection):
    """Register high-level scene composition tools."""

    @mcp.tool()
    def tpl_scene_create_and_place(
        blueprint_path: str,
        location: List[float] = [0, 0, 0],
        rotation: List[float] = [0, 0, 0],
        scale: List[float] = [1, 1, 1],
        material_path: str = "",
        actor_label: str = ""
    ) -> Dict[str, Any]:
        """Spawn a Blueprint actor and optionally apply a material - all in one call.
        blueprint_path: e.g. '/Game/Blueprints/BP_GS_Cup'
        material_path: optional material to apply (e.g. '/Game/GeometryScript/M_Glass')"""
        conn = get_connection()
        if not conn:
            return {"success": False, "error": "Not connected"}

        try:
            result = _cmd(conn, "level_spawn_blueprint_by_path", {
                "blueprint_path": blueprint_path,
                "location": location, "rotation": rotation, "scale": scale,
                "name": actor_label
            })

            actor_name = result.get("result", {}).get("actor_name", "")

            if material_path and actor_name:
                _cmd(conn, "mat_apply_material_to_actor", {
                    "asset_path": material_path, "actor_name": actor_name, "slot_index": 0
                })

            return {"success": True, "actor_name": actor_name,
                    "message": f"Spawned {blueprint_path}" + (f" with material {material_path}" if material_path else "")}
        except Exception as e:
            return {"success": False, "error": str(e)}

    @mcp.tool()
    def tpl_scene_arrange_grid(
        blueprint_path: str,
        rows: int = 3,
        cols: int = 3,
        spacing: float = 200.0,
        start_location: List[float] = [0, 0, 0],
        material_path: str = ""
    ) -> Dict[str, Any]:
        """Spawn a grid of Blueprint actors. Great for testing materials or layouts.
        Creates rows*cols actors evenly spaced."""
        conn = get_connection()
        if not conn:
            return {"success": False, "error": "Not connected"}

        try:
            spawned = []
            for r in range(rows):
                for c in range(cols):
                    loc = [
                        start_location[0] + c * spacing,
                        start_location[1] + r * spacing,
                        start_location[2]
                    ]
                    result = _cmd(conn, "level_spawn_blueprint_by_path", {
                        "blueprint_path": blueprint_path,
                        "location": loc, "rotation": [0, 0, 0], "scale": [1, 1, 1],
                        "name": f"Grid_{r}_{c}"
                    })
                    actor_name = result.get("result", {}).get("actor_name", "")
                    if material_path and actor_name:
                        _cmd(conn, "mat_apply_material_to_actor", {
                            "asset_path": material_path, "actor_name": actor_name, "slot_index": 0
                        })
                    spawned.append(actor_name)

            return {"success": True, "count": len(spawned), "actors": spawned,
                    "message": f"Grid {rows}x{cols} = {len(spawned)} actors"}
        except Exception as e:
            return {"success": False, "error": str(e)}

    @mcp.tool()
    def tpl_scene_arrange_circle(
        blueprint_path: str,
        count: int = 8,
        radius: float = 500.0,
        center: List[float] = [0, 0, 0],
        face_center: bool = True,
        material_path: str = ""
    ) -> Dict[str, Any]:
        """Arrange Blueprint actors in a circle pattern.
        face_center: if True, actors rotate to face the center point."""
        conn = get_connection()
        if not conn:
            return {"success": False, "error": "Not connected"}

        try:
            spawned = []
            for i in range(count):
                angle = 2 * math.pi * i / count
                loc = [
                    center[0] + radius * math.cos(angle),
                    center[1] + radius * math.sin(angle),
                    center[2]
                ]
                rot = [0, 0, 0]
                if face_center:
                    # Yaw to face center
                    rot = [0, math.degrees(angle) + 180, 0]

                result = _cmd(conn, "level_spawn_blueprint_by_path", {
                    "blueprint_path": blueprint_path,
                    "location": loc, "rotation": rot, "scale": [1, 1, 1],
                    "name": f"Circle_{i}"
                })
                actor_name = result.get("result", {}).get("actor_name", "")
                if material_path and actor_name:
                    _cmd(conn, "mat_apply_material_to_actor", {
                        "asset_path": material_path, "actor_name": actor_name, "slot_index": 0
                    })
                spawned.append(actor_name)

            return {"success": True, "count": len(spawned), "actors": spawned,
                    "message": f"Circle of {count} actors, R={radius}"}
        except Exception as e:
            return {"success": False, "error": str(e)}
