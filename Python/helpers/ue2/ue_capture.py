"""UE2 Capture tool -- viewport screenshots, camera control, and multi-view asset capture."""
import time
import math
import os
from datetime import datetime
from typing import Any, Dict, List

from helpers.ue2.response import UEResponse, cmd, extract_data

TOOL = "ue_capture"
ACTIONS = ["screenshot", "set_camera", "focus_actor", "asset_turntable"]


def register_capture_tools(mcp, get_connection):
    """Register the ue_capture action-based tool."""

    @mcp.tool()
    def ue_capture(
        action: str,
        file_path: str = "",
        location: List[float] = [],
        rotation: List[float] = [],
        actor_name: str = "",
        output_dir: str = "",
        distance: float = 500.0,
        views: List[str] = [],
        height_offset: float = 0.0,
    ) -> Dict[str, Any]:
        """Viewport capture and camera control for Unreal Editor.

        Actions:
          screenshot       -- Capture a viewport screenshot. Params: file_path (optional).
          set_camera       -- Set viewport camera. Params: location [x,y,z], rotation [pitch,yaw,roll].
          focus_actor      -- Focus viewport on an actor. Params: actor_name.
          asset_turntable  -- Capture multi-view screenshots of an actor (like Unity's asset preview).
                              Takes 4+ photos from different angles: front, back, left, right, top, perspective.
                              Params: actor_name, output_dir (folder for PNGs), distance (camera distance),
                              views (optional custom list, default: ["front","back","left","right","top","perspective"]),
                              height_offset (extra height for camera, default 0).
                              Returns list of captured image file paths.
        """
        t0 = time.time()
        conn = get_connection()
        if conn is None:
            return UEResponse.not_connected(TOOL)

        try:
            # ------------------------------------------------------------------
            if action == "screenshot":
                if not file_path:
                    # Generate default path
                    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
                    default_dir = "C:/Users/Renato/Workspaces/atomic_grids/unreal_demo/Glass/Glass/Saved/Screenshots"
                    os.makedirs(default_dir, exist_ok=True)
                    file_path = f"{default_dir}/screenshot_{ts}.png"
                params = {"file_path": file_path}
                result = cmd(conn, "level_get_viewport_screenshot", params)
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "set_camera":
                if not location and not rotation:
                    return UEResponse.error("E_INVALID_ARGUMENT",
                        "At least one of 'location' or 'rotation' required", TOOL)
                params = {}
                if location: params["location"] = location
                if rotation: params["rotation"] = rotation
                result = cmd(conn, "level_set_viewport_camera", params)
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "focus_actor":
                if not actor_name:
                    return UEResponse.error("E_INVALID_ARGUMENT",
                        "'actor_name' required", TOOL)
                result = cmd(conn, "level_focus_actor", {"actor_name": actor_name})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "asset_turntable":
                if not actor_name:
                    return UEResponse.error("E_INVALID_ARGUMENT",
                        "'actor_name' required for asset_turntable", TOOL)

                # Get actor position via get_actor_properties (uses label matching)
                actor_result = cmd(conn, "level_get_actor_properties", {"actor_name": actor_name})
                actor_data = extract_data(actor_result)
                if not actor_data.get("success", False):
                    return UEResponse.error("E_NOT_FOUND", f"Actor '{actor_name}' not found", TOOL)

                loc = actor_data.get("location", [0, 0, 0])
                cx, cy, cz = loc[0], loc[1], loc[2]

                # Default views
                if not views:
                    views = ["front", "back", "left", "right", "top", "perspective"]

                # Use output_dir or temp
                if not output_dir:
                    output_dir = "C:/Users/Renato/Workspaces/atomic_grids/unreal_demo/Glass/Glass/Saved/Screenshots"

                # Camera angles: {name: (camera_offset_x, camera_offset_y, camera_offset_z, pitch, yaw)}
                d = distance
                h = height_offset
                view_configs = {
                    "front":       (d,   0,   h,    -10,  180,  0),
                    "back":        (-d,  0,   h,    -10,  0,    0),
                    "left":        (0,   -d,  h,    -10,  90,   0),
                    "right":       (0,   d,   h,    -10,  -90,  0),
                    "top":         (0,   0,   d,    -89,  0,    0),
                    "bottom":      (0,   0,   -d,   89,   0,    0),
                    "perspective": (d*0.7, -d*0.7, d*0.5, -25, 135, 0),
                    "persp_alt":   (-d*0.7, d*0.7, d*0.5, -25, -45, 0),
                }

                captured = []
                for view_name in views:
                    config = view_configs.get(view_name)
                    if not config:
                        continue

                    ox, oy, oz, pitch, yaw, roll = config
                    cam_loc = [cx + ox, cy + oy, cz + oz]
                    cam_rot = [pitch, yaw, roll]

                    # Move camera
                    cmd(conn, "level_set_viewport_camera", {
                        "location": cam_loc, "rotation": cam_rot
                    })

                    # Small delay for viewport to update (via a quick ping)
                    cmd(conn, "ping", {})

                    # Capture screenshot
                    img_path = f"{output_dir}/{actor_name}_{view_name}.png"
                    result = cmd(conn, "level_get_viewport_screenshot", {"file_path": img_path})
                    img_data = extract_data(result)

                    captured.append({
                        "view": view_name,
                        "file_path": img_path,
                        "camera_location": cam_loc,
                        "camera_rotation": cam_rot,
                        "success": img_data.get("success", False) if isinstance(img_data, dict) else True
                    })

                return UEResponse.ok({
                    "actor_name": actor_name,
                    "views_captured": len(captured),
                    "images": captured,
                    "output_dir": output_dir
                }, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            else:
                return UEResponse.invalid_action(action, ACTIONS, TOOL)

        except ConnectionError:
            return UEResponse.not_connected(TOOL)
        except Exception as e:
            return UEResponse.error("E_INTERNAL", str(e), TOOL, retryable=False)
