"""Batch execution tool - run multiple MCP commands in a single call."""
from typing import Dict, Any, List


def register_batch_tools(mcp, get_connection):

    @mcp.tool()
    def batch_execute(commands: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Execute multiple MCP commands in a single network round-trip.
        commands: list of {"type": "command_name", "params": {...}} objects.
        Example: [{"type": "gs_create_mesh", "params": {"mesh_name": "a"}},
                  {"type": "gs_append_box", "params": {"mesh_name": "a"}}]
        Returns all results in order. Much faster than calling tools one by one."""
        return get_connection().send_command("batch_execute", {"commands": commands})
