"""UE2 Playtest tool -- PIE play/stop and status."""
import time
from typing import Any, Dict

from helpers.ue2.response import UEResponse, cmd, extract_data

TOOL = "ue_playtest"
ACTIONS = ["play", "stop", "is_playing"]


def register_playtest_tools(mcp, get_connection):
    """Register the ue_playtest action-based tool."""

    @mcp.tool()
    def ue_playtest(
        action: str,
    ) -> Dict[str, Any]:
        """Play-in-Editor (PIE) control for Unreal Engine.

        Actions:
          play        -- Start Play-in-Editor (simulate mode).
          stop        -- Stop the active PIE session.
          is_playing  -- Check whether a PIE session is currently active.
        """
        t0 = time.time()
        conn = get_connection()
        if conn is None:
            return UEResponse.not_connected(TOOL)

        try:
            # ------------------------------------------------------------------
            if action == "play":
                py_code = (
                    "import unreal\n"
                    "subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)\n"
                    "subsystem.editor_play_simulate()\n"
                    "print('PIE_STARTED')"
                )
                result = cmd(conn, "execute_python", {"code": py_code})
                data = extract_data(result)
                return UEResponse.ok(
                    {"status": "playing", "raw": data},
                    tool=TOOL,
                    duration_ms=(time.time() - t0) * 1000,
                )

            # ------------------------------------------------------------------
            elif action == "stop":
                py_code = (
                    "import unreal\n"
                    "subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)\n"
                    "subsystem.editor_request_end_play()\n"
                    "print('PIE_STOPPED')"
                )
                result = cmd(conn, "execute_python", {"code": py_code})
                data = extract_data(result)
                return UEResponse.ok(
                    {"status": "stopped", "raw": data},
                    tool=TOOL,
                    duration_ms=(time.time() - t0) * 1000,
                )

            # ------------------------------------------------------------------
            elif action == "is_playing":
                py_code = (
                    "import unreal, json\n"
                    "subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)\n"
                    "# GEditor->IsPlaySessionInProgress() exposed via Python\n"
                    "is_playing = unreal.EditorLevelLibrary.get_game_world() is not None\n"
                    "print(json.dumps({'is_playing': is_playing}))"
                )
                result = cmd(conn, "execute_python", {"code": py_code})
                data = extract_data(result)

                import json as _json
                output = data if isinstance(data, str) else data.get("output", "") if isinstance(data, dict) else str(data)
                try:
                    parsed = _json.loads(output.strip().split("\n")[-1])
                except Exception:
                    parsed = {"is_playing": None, "raw_output": output}

                return UEResponse.ok(
                    parsed,
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
