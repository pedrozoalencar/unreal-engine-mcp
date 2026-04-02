"""UE2 Console tool -- read output log and execute console commands."""
import time
from typing import Any, Dict

from helpers.ue2.response import UEResponse, cmd, extract_data

TOOL = "ue_console"
ACTIONS = ["read_log", "exec_command"]


def register_console_tools(mcp, get_connection):
    """Register the ue_console action-based tool."""

    @mcp.tool()
    def ue_console(
        action: str,
        limit: int = 50,
        level: str = "all",
        command: str = "",
    ) -> Dict[str, Any]:
        """Output log and console command interface for Unreal Editor.

        Actions:
          read_log      -- Read recent lines from the Unreal output log file.
                           Params: limit (int, default 50), level ('all'|'error'|'warning').
          exec_command  -- Execute a console command inside the editor.
                           Params: command (str, the console command to run).
        """
        t0 = time.time()
        conn = get_connection()
        if conn is None:
            return UEResponse.not_connected(TOOL)

        try:
            # ------------------------------------------------------------------
            if action == "read_log":
                # Build Python code that reads the project log file and filters
                level_filter = level.lower()
                py_code = f"""
import unreal, os, json

log_dir = str(unreal.Paths.project_log_dir())
# Find the most recent .log file
log_files = [f for f in os.listdir(log_dir) if f.endswith('.log')]
if not log_files:
    print(json.dumps({{"lines": [], "error": "No log files found"}}))
else:
    log_files.sort(key=lambda f: os.path.getmtime(os.path.join(log_dir, f)), reverse=True)
    log_path = os.path.join(log_dir, log_files[0])
    with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
        all_lines = f.readlines()

    # Filter by level
    level_filter = '{level_filter}'
    if level_filter == 'error':
        filtered = [l.rstrip() for l in all_lines if 'Error' in l or 'error' in l]
    elif level_filter == 'warning':
        filtered = [l.rstrip() for l in all_lines if 'Warning' in l or 'warning' in l]
    else:
        filtered = [l.rstrip() for l in all_lines]

    # Take last N lines
    limit = {limit}
    result_lines = filtered[-limit:]
    print(json.dumps({{"lines": result_lines, "total_lines": len(all_lines), "filtered_count": len(filtered), "file": log_path}}))
""".strip()
                result = cmd(conn, "execute_python", {"code": py_code})
                data = extract_data(result)

                import json as _json
                output = data if isinstance(data, str) else data.get("output", "") if isinstance(data, dict) else str(data)
                try:
                    parsed = _json.loads(output.strip().split("\n")[-1])
                except Exception:
                    parsed = {"raw_output": output}

                return UEResponse.ok(
                    parsed,
                    tool=TOOL,
                    duration_ms=(time.time() - t0) * 1000,
                )

            # ------------------------------------------------------------------
            elif action == "exec_command":
                if not command:
                    return UEResponse.error(
                        "E_INVALID_ARGUMENT",
                        "Parameter 'command' is required for exec_command",
                        TOOL,
                    )
                py_code = (
                    "import unreal\n"
                    f"unreal.SystemLibrary.execute_console_command(None, '{command}')\n"
                    "print('CONSOLE_COMMAND_EXECUTED')"
                )
                result = cmd(conn, "execute_python", {"code": py_code})
                data = extract_data(result)
                return UEResponse.ok(
                    {"command": command, "raw": data},
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
