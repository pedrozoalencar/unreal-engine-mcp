"""Level management, actor spawning, viewport control tools."""
from typing import Dict, Any, Optional, List


def register_level_tools(mcp, get_connection):
    """Register all level MCP tools."""

    @mcp.tool()
    def level_spawn_blueprint(
        blueprint_path: str,
        location: List[float] = [0, 0, 0],
        rotation: List[float] = [0, 0, 0],
        scale: List[float] = [1, 1, 1],
        actor_label: str = ""
    ) -> Dict[str, Any]:
        """Spawn a Blueprint actor in the level by its asset path.
        blueprint_path: e.g. '/Game/Blueprints/BP_GS_Cup'"""
        params = {"blueprint_path": blueprint_path, "location": location,
                  "rotation": rotation, "scale": scale}
        if actor_label: params["actor_label"] = actor_label
        return get_connection().send_command("level_spawn_blueprint_by_path", params)

    @mcp.tool()
    def level_get_actor_properties(actor_name: str) -> Dict[str, Any]:
        """Get all properties of an actor: class, transform, components, visible UPROPERTYs."""
        return get_connection().send_command("level_get_actor_properties", {"actor_name": actor_name})

    @mcp.tool()
    def level_set_actor_property(
        actor_name: str, property_name: str, property_value: Any = None
    ) -> Dict[str, Any]:
        """Set a property on an actor. Supports location/rotation/scale as [x,y,z], bool, int, float, string."""
        return get_connection().send_command("level_set_actor_property", {
            "actor_name": actor_name, "property_name": property_name,
            "property_value": str(property_value)
        })

    @mcp.tool()
    def level_get_component_properties(actor_name: str, component_name: str = "") -> Dict[str, Any]:
        """Get properties of a specific component on an actor."""
        params = {"actor_name": actor_name}
        if component_name: params["component_name"] = component_name
        return get_connection().send_command("level_get_component_properties", params)

    @mcp.tool()
    def level_screenshot(file_path: str = "") -> Dict[str, Any]:
        """Capture viewport screenshot to a PNG file. Returns the file path."""
        params = {}
        if file_path: params["file_path"] = file_path
        return get_connection().send_command("level_get_viewport_screenshot", params)

    @mcp.tool()
    def level_set_camera(
        location: List[float] = [0, 0, 500], rotation: List[float] = [0, 0, 0]
    ) -> Dict[str, Any]:
        """Set the editor viewport camera position and rotation."""
        return get_connection().send_command("level_set_viewport_camera", {
            "location": location, "rotation": rotation
        })

    @mcp.tool()
    def level_focus_actor(actor_name: str) -> Dict[str, Any]:
        """Focus the viewport camera on a specific actor."""
        return get_connection().send_command("level_focus_actor", {"actor_name": actor_name})

    @mcp.tool()
    def level_get_info() -> Dict[str, Any]:
        """Get level info: name, actor count by class, world bounds."""
        return get_connection().send_command("level_get_level_info", {})

    @mcp.tool()
    def level_save() -> Dict[str, Any]:
        """Save the current level."""
        return get_connection().send_command("level_save_current_level", {})
