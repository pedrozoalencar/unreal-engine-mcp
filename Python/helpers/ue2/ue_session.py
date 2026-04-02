"""UE2 Session tool -- ping, editor context, Python execution."""
import json as _json
import time
from typing import Any, Dict, Optional

from helpers.ue2.response import UEResponse, cmd, extract_data

TOOL = "ue_session"
ACTIONS = ["ping", "get_context", "run_python"]


def register_session_tools(mcp, get_connection):
    """Register the ue_session action-based tool."""

    @mcp.tool()
    def ue_session(
        action: str,
        code: str = "",
    ) -> Dict[str, Any]:
        """Unified session/utility tool for Unreal Editor.

        Actions:
          ping          -- Check connection to Unreal Engine and return engine info.
          get_context   -- Get current editor context: active level, selected actors, project name.
          run_python    -- Execute arbitrary Python code inside the Unreal Editor. Params: code (str).
        """
        t0 = time.time()
        conn = get_connection()
        if conn is None:
            return UEResponse.not_connected(TOOL)

        try:
            # ------------------------------------------------------------------
            if action == "ping":
                result = cmd(conn, "ping", {})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "get_context":
                py_code = """
import unreal
import json

# Active level
world = unreal.EditorLevelLibrary.get_editor_world()
level_name = world.get_name() if world else "Unknown"

# Project name
project_dir = unreal.Paths.project_dir()
project_name = unreal.Paths.get_base_filename(unreal.Paths.get_project_file_path())

# Selected actors
subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
selected = subsystem.get_selected_level_actors()
selected_info = []
for actor in selected:
    selected_info.append({
        "name": actor.get_actor_label(),
        "class": actor.get_class().get_name(),
        "path": actor.get_path_name()
    })

# Actor count
all_actors = unreal.EditorLevelLibrary.get_all_level_actors()
class_counts = {}
for a in all_actors:
    cls = a.get_class().get_name()
    class_counts[cls] = class_counts.get(cls, 0) + 1

context = {
    "level_name": level_name,
    "project_name": project_name,
    "project_dir": str(project_dir),
    "selected_actors": selected_info,
    "selected_count": len(selected_info),
    "total_actors": len(all_actors),
    "actor_class_counts": class_counts,
}
print(json.dumps(context))
""".strip()
                result = cmd(conn, "execute_python", {"code": py_code})
                data = extract_data(result)

                # Try to parse the JSON from stdout
                output = data if isinstance(data, str) else data.get("output", "") if isinstance(data, dict) else str(data)
                try:
                    context = _json.loads(output.strip().split("\n")[-1])
                except Exception:
                    context = {"raw_output": output}

                return UEResponse.ok(context, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "run_python":
                if not code:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'code' is required for run_python", TOOL)

                result = cmd(conn, "execute_python", {"code": code})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            else:
                return UEResponse.invalid_action(action, ACTIONS, TOOL)

        except ConnectionError as e:
            return UEResponse.not_connected(TOOL)
        except Exception as e:
            return UEResponse.error("E_INTERNAL", str(e), TOOL, retryable=False)
