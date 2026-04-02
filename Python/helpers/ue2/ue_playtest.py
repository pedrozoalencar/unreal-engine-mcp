"""UE2 Playtest tool -- PIE play/stop/status via native C++ commands."""
import time
from typing import Any, Dict

from helpers.ue2.response import UEResponse, cmd, extract_data

TOOL = "ue_playtest"
ACTIONS = ["play", "stop", "is_playing"]


def register_playtest_tools(mcp, get_connection):

    @mcp.tool()
    def ue_playtest(action: str) -> Dict[str, Any]:
        """Play-In-Editor (PIE) control.

        Actions:
          play        -- Start PIE in Simulate mode.
          stop        -- Stop the current PIE session.
          is_playing  -- Check if a PIE session is active.
        """
        t0 = time.time()
        conn = get_connection()
        if conn is None:
            return UEResponse.not_connected(TOOL)

        try:
            if action == "play":
                result = cmd(conn, "play_simulate", {})
                return UEResponse.ok(extract_data(result), tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            elif action == "stop":
                result = cmd(conn, "play_stop", {})
                return UEResponse.ok(extract_data(result), tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            elif action == "is_playing":
                result = cmd(conn, "play_is_playing", {})
                return UEResponse.ok(extract_data(result), tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            else:
                return UEResponse.invalid_action(action, ACTIONS, TOOL)

        except ConnectionError:
            return UEResponse.not_connected(TOOL)
        except Exception as e:
            return UEResponse.error("E_INTERNAL", str(e), TOOL)
