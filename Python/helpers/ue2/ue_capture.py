"""UE2 Capture tool -- viewport screenshots and camera control."""
import time
from typing import Any, Dict, List

from helpers.ue2.response import UEResponse, cmd, extract_data

TOOL = "ue_capture"
ACTIONS = ["screenshot", "set_camera", "focus_actor"]


def register_capture_tools(mcp, get_connection):
    """Register the ue_capture action-based tool."""

    @mcp.tool()
    def ue_capture(
        action: str,
        file_path: str = "",
        location: List[float] = [],
        rotation: List[float] = [],
        actor_name: str = "",
    ) -> Dict[str, Any]:
        """Viewport capture and camera control for Unreal Editor.

        Actions:
          screenshot    -- Capture a viewport screenshot. Params: file_path (str, optional save path).
          set_camera    -- Set the viewport camera transform. Params: location [x,y,z], rotation [pitch,yaw,roll].
          focus_actor   -- Focus the viewport on a specific actor. Params: actor_name (str).
        """
        t0 = time.time()
        conn = get_connection()
        if conn is None:
            return UEResponse.not_connected(TOOL)

        try:
            # ------------------------------------------------------------------
            if action == "screenshot":
                params = {}
                if file_path:
                    params["file_path"] = file_path
                result = cmd(conn, "level_get_viewport_screenshot", params)
                data = extract_data(result)
                return UEResponse.ok(
                    data,
                    tool=TOOL,
                    duration_ms=(time.time() - t0) * 1000,
                )

            # ------------------------------------------------------------------
            elif action == "set_camera":
                if not location and not rotation:
                    return UEResponse.error(
                        "E_INVALID_ARGUMENT",
                        "At least one of 'location' or 'rotation' is required for set_camera",
                        TOOL,
                    )
                params = {}
                if location:
                    params["location"] = location
                if rotation:
                    params["rotation"] = rotation
                result = cmd(conn, "level_set_viewport_camera", params)
                data = extract_data(result)
                return UEResponse.ok(
                    data,
                    tool=TOOL,
                    duration_ms=(time.time() - t0) * 1000,
                )

            # ------------------------------------------------------------------
            elif action == "focus_actor":
                if not actor_name:
                    return UEResponse.error(
                        "E_INVALID_ARGUMENT",
                        "Parameter 'actor_name' is required for focus_actor",
                        TOOL,
                    )
                result = cmd(conn, "level_focus_actor", {"actor_name": actor_name})
                data = extract_data(result)
                return UEResponse.ok(
                    data,
                    tool=TOOL,
                    duration_ms=(time.time() - t0) * 1000,
                )

            # ------------------------------------------------------------------
            else:
                return UEResponse.invalid_action(action, ACTIONS, TOOL)

        except ConnectionError:
            return UEResponse.not_connected(TOOL)
        except Exception as e:
            return UEResponse.error("E_INTERNAL", str(e), TOOL, retryable=False)
