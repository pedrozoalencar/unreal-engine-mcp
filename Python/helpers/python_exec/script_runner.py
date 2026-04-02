"""Python script execution MCP tools - run Python code inside Unreal Editor."""
from typing import Dict, Any


def register_python_exec_tools(mcp, get_connection):
    """Register Python execution tools with the MCP server."""

    @mcp.tool()
    def execute_python_in_editor(code: str) -> Dict[str, Any]:
        """Execute Python code directly inside the Unreal Editor.
        The code runs in the editor's Python environment with full access to unreal module.
        Example: 'import unreal; print(unreal.EditorAssetLibrary.list_assets("/Game/"))'
        Requires PythonScriptPlugin to be enabled in the .uproject.
        Output is captured from stdout/stderr."""
        return get_connection().send_command("execute_python", {"code": code})

    @mcp.tool()
    def execute_python_file_in_editor(file_path: str) -> Dict[str, Any]:
        """Execute a Python script file inside the Unreal Editor.
        file_path: absolute path to the .py file on disk.
        The file is read and executed in the editor's Python environment."""
        return get_connection().send_command("execute_python_file", {"file_path": file_path})
