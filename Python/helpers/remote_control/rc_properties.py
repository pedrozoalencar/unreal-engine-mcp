"""Remote Control API property and function tools for MCP."""
from typing import Dict, Any, Optional, List
from .rc_client import RemoteControlClient

_rc_client = None


def _get_rc() -> RemoteControlClient:
    global _rc_client
    if _rc_client is None:
        _rc_client = RemoteControlClient()
    return _rc_client


def register_rc_tools(mcp, get_connection):
    """Register Remote Control API tools with the MCP server."""

    @mcp.tool()
    def rc_get_property(
        object_path: str,
        property_name: str
    ) -> Dict[str, Any]:
        """Read a property from any UObject via Unreal Remote Control API.
        object_path: full path like '/Game/Maps/Level.Level:PersistentLevel.MyActor.RootComponent'
        property_name: e.g. 'RelativeLocation', 'Intensity', 'bHidden'"""
        return _get_rc().get_property(object_path, property_name)

    @mcp.tool()
    def rc_set_property(
        object_path: str,
        property_name: str,
        value: Any = None
    ) -> Dict[str, Any]:
        """Write a property on any UObject via Unreal Remote Control API.
        value can be a number, string, boolean, or dict for struct types."""
        return _get_rc().set_property(object_path, property_name, value)

    @mcp.tool()
    def rc_call_function(
        object_path: str,
        function_name: str,
        parameters: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """Call any UFunction on a UObject via Remote Control API.
        object_path: path to the object. function_name: name of the UFUNCTION.
        parameters: dict of function parameters."""
        return _get_rc().call_function(object_path, function_name, parameters)

    @mcp.tool()
    def rc_search_objects(
        query: str,
        class_name: str = "",
        outer_path: str = ""
    ) -> Dict[str, Any]:
        """Search for assets/objects in the Unreal project.
        query: search text. class_name: filter by class (e.g. 'StaticMesh').
        outer_path: filter by package path."""
        return _get_rc().search_objects(query, class_name, outer_path)

    @mcp.tool()
    def rc_list_presets() -> Dict[str, Any]:
        """List all Remote Control presets defined in the project.
        Presets expose specific properties/functions for remote control."""
        return _get_rc().list_presets()

    @mcp.tool()
    def rc_get_preset(preset_name: str) -> Dict[str, Any]:
        """Get details of a specific Remote Control preset including its exposed properties and functions."""
        return _get_rc().get_preset(preset_name)

    @mcp.tool()
    def rc_batch(
        requests: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """Execute multiple Remote Control operations in a single transaction.
        Each request dict should have: objectPath, propertyName (for property ops)
        or objectPath, functionName, parameters (for function calls)."""
        return _get_rc().batch(requests)
