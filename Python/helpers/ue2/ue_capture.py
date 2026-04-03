"""UE2 Capture tool -- viewport screenshots, camera control, and multi-view asset capture."""
import time
import math
import os
from datetime import datetime
from typing import Any, Dict, List

from helpers.ue2.response import UEResponse, cmd, extract_data

TOOL = "ue_capture"
ACTIONS = ["screenshot", "set_camera", "focus_actor", "asset_turntable", "contact_sheet"]


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
          contact_sheet    -- Capture 4 views (front, right, back, perspective) and compose a single
                              2x2 grid image with labels. One image, all angles.
                              Params: actor_name, distance (default 500), file_path (optional output),
                              height_offset (default 0).
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
            elif action == "contact_sheet":
                if not actor_name:
                    return UEResponse.error("E_INVALID_ARGUMENT",
                        "'actor_name' required for contact_sheet", TOOL)

                # Get actor position
                actor_result = cmd(conn, "level_get_actor_properties", {"actor_name": actor_name})
                actor_data = extract_data(actor_result)
                if not actor_data.get("success", False):
                    return UEResponse.error("E_NOT_FOUND", f"Actor '{actor_name}' not found", TOOL)

                loc = actor_data.get("location", [0, 0, 0])
                cx, cy, cz = loc[0], loc[1], loc[2]

                d = distance
                h = height_offset
                # 4 views: front, right, back, perspective
                sheet_views = [
                    ("front",       d,    0,    h,   -10, 180,  0),
                    ("right",       0,    d,    h,   -10, -90,  0),
                    ("back",       -d,    0,    h,   -10,   0,  0),
                    ("perspective", d*0.7, -d*0.7, d*0.5, -25, 135, 0),
                ]

                # Use temp dir for individual captures
                tmp_dir = output_dir or os.path.join(
                    "C:/Users/Renato/Workspaces/atomic_grids/unreal_demo/Glass/Glass/Saved/Screenshots",
                    "_contact_tmp"
                )
                os.makedirs(tmp_dir, exist_ok=True)

                temp_paths = []
                for vname, ox, oy, oz, pitch, yaw, roll in sheet_views:
                    cam_loc = [cx + ox, cy + oy, cz + oz]
                    cam_rot = [pitch, yaw, roll]
                    cmd(conn, "level_set_viewport_camera", {"location": cam_loc, "rotation": cam_rot})
                    img_path = f"{tmp_dir}/{actor_name}_{vname}.png"
                    cmd(conn, "level_get_viewport_screenshot", {"file_path": img_path})
                    temp_paths.append((vname, img_path))

                # Compose 2x2 grid using PIL
                try:
                    from PIL import Image, ImageDraw, ImageFont
                except ImportError:
                    return UEResponse.error("E_DEPENDENCY", "Pillow (PIL) required: pip install Pillow", TOOL)

                images = []
                for vname, p in temp_paths:
                    try:
                        images.append((vname, Image.open(p)))
                    except Exception:
                        return UEResponse.error("E_INTERNAL", f"Failed to open {p}", TOOL)

                # Resize all to same size (use smallest dimensions)
                w = min(img.size[0] for _, img in images)
                h_img = min(img.size[1] for _, img in images)
                # Cap at 1920x1080 per tile for reasonable file size
                max_tile_w, max_tile_h = 1920, 1080
                if w > max_tile_w:
                    scale = max_tile_w / w
                    w = max_tile_w
                    h_img = int(h_img * scale)
                if h_img > max_tile_h:
                    scale = max_tile_h / h_img
                    h_img = max_tile_h
                    w = int(w * scale)

                resized = []
                for vname, img in images:
                    resized.append((vname, img.resize((w, h_img), Image.LANCZOS)))

                # Create 2x2 grid with labels
                padding = 4
                label_h = 32
                grid_w = w * 2 + padding * 3
                grid_h = (h_img + label_h) * 2 + padding * 3
                canvas = Image.new("RGB", (grid_w, grid_h), (30, 30, 30))
                draw = ImageDraw.Draw(canvas)

                positions = [(0, 0), (1, 0), (0, 1), (1, 1)]
                for i, (vname, img) in enumerate(resized):
                    col, row = positions[i]
                    x = padding + col * (w + padding)
                    y = padding + row * (h_img + label_h + padding)
                    # Draw label
                    draw.rectangle([x, y, x + w, y + label_h], fill=(50, 50, 50))
                    draw.text((x + 10, y + 6), vname.upper(), fill=(220, 220, 220))
                    # Paste image
                    canvas.paste(img, (x, y + label_h))

                # Save contact sheet
                if file_path:
                    sheet_path = file_path
                else:
                    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
                    sheet_dir = "C:/Users/Renato/Workspaces/atomic_grids/unreal_demo/Glass/Glass/Saved/Screenshots"
                    sheet_path = f"{sheet_dir}/{actor_name}_contact_{ts}.png"

                canvas.save(sheet_path, "PNG")

                # Cleanup temp files
                for _, p in temp_paths:
                    try:
                        os.remove(p)
                    except OSError:
                        pass

                return UEResponse.ok({
                    "actor_name": actor_name,
                    "file_path": sheet_path,
                    "grid": "2x2",
                    "views": [v for v, _ in temp_paths],
                    "tile_size": [w, h_img],
                    "total_size": [grid_w, grid_h],
                    "file_size_bytes": os.path.getsize(sheet_path)
                }, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            else:
                return UEResponse.invalid_action(action, ACTIONS, TOOL)

        except ConnectionError:
            return UEResponse.not_connected(TOOL)
        except Exception as e:
            return UEResponse.error("E_INTERNAL", str(e), TOOL, retryable=False)
