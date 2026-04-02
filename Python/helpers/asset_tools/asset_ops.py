"""Asset management tools: list, find, duplicate, rename, delete, save."""
from typing import Dict, Any, Optional


def register_asset_tools(mcp, get_connection):
    """Register all asset management MCP tools."""

    @mcp.tool()
    def asset_list(
        directory_path: str = "/Game/", class_filter: str = "", recursive: bool = True
    ) -> Dict[str, Any]:
        """List assets in a directory. Returns path, name, and class for each.
        class_filter: optional, e.g. 'Material', 'StaticMesh', 'Blueprint'"""
        params = {"directory_path": directory_path, "recursive": recursive}
        if class_filter: params["class_filter"] = class_filter
        return get_connection().send_command("asset_list_assets", params)

    @mcp.tool()
    def asset_find(search_text: str, class_name: str = "") -> Dict[str, Any]:
        """Search for assets by name. Returns matching assets with paths."""
        params = {"search_text": search_text}
        if class_name: params["class_name"] = class_name
        return get_connection().send_command("asset_find_assets", params)

    @mcp.tool()
    def asset_get_info(asset_path: str) -> Dict[str, Any]:
        """Get detailed info about an asset: class, package, references, is dirty."""
        return get_connection().send_command("asset_get_asset_info", {"asset_path": asset_path})

    @mcp.tool()
    def asset_duplicate(source_path: str, destination_path: str) -> Dict[str, Any]:
        """Duplicate an asset to a new path."""
        return get_connection().send_command("asset_duplicate_asset", {
            "source_path": source_path, "destination_path": destination_path
        })

    @mcp.tool()
    def asset_rename(source_path: str, destination_path: str) -> Dict[str, Any]:
        """Rename/move an asset."""
        return get_connection().send_command("asset_rename_asset", {
            "source_path": source_path, "destination_path": destination_path
        })

    @mcp.tool()
    def asset_delete(asset_path: str) -> Dict[str, Any]:
        """Delete an asset. Use with caution."""
        return get_connection().send_command("asset_delete_asset", {"asset_path": asset_path})

    @mcp.tool()
    def asset_exists(asset_path: str) -> Dict[str, Any]:
        """Check if an asset exists at the given path."""
        return get_connection().send_command("asset_does_asset_exist", {"asset_path": asset_path})

    @mcp.tool()
    def asset_save(asset_path: str) -> Dict[str, Any]:
        """Save a specific asset."""
        return get_connection().send_command("asset_save_asset", {"asset_path": asset_path})

    @mcp.tool()
    def asset_save_all() -> Dict[str, Any]:
        """Save all unsaved/dirty assets."""
        return get_connection().send_command("asset_save_all", {})
