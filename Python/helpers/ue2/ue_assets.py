"""UE2 Assets tool -- search, list, inspect, duplicate, rename, delete, save, import, export."""
import time
import json as _json
from typing import Any, Dict, List, Optional

from helpers.ue2.response import UEResponse, cmd, extract_data

TOOL = "ue_assets"
ACTIONS = [
    "search", "list", "inspect", "duplicate", "rename",
    "delete", "save", "save_all", "exists", "import_files", "export",
]


def register_assets_tools(mcp, get_connection):
    """Register the ue_assets action-based tool."""

    @mcp.tool()
    def ue_assets(
        action: str,
        # search
        query: str = "",
        class_filter: str = "",
        limit: int = 50,
        # list
        directory_path: str = "/Game/",
        recursive: bool = True,
        # inspect / delete / save / exists
        asset_path: str = "",
        # duplicate / rename
        source_path: str = "",
        destination_path: str = "",
        # import_files
        files: List[str] = [],
        replace_existing: bool = True,
        # export
        asset_paths: List[str] = [],
        output_dir: str = "",
    ) -> Dict[str, Any]:
        """Unified asset management tool for Unreal Editor.

        Actions:
          search        -- Search assets by name. Params: query, path="/Game/", class_filter="", limit=50.
          list          -- List assets in a directory. Params: directory_path="/Game/", recursive=True, class_filter="".
          inspect       -- Get detailed asset info. Params: asset_path.
          duplicate     -- Duplicate an asset. Params: source_path, destination_path.
          rename        -- Rename/move an asset. Params: source_path, destination_path.
          delete        -- Delete an asset. Params: asset_path.
          save          -- Save a specific asset. Params: asset_path.
          save_all      -- Save all dirty assets. No params.
          exists        -- Check if asset exists. Params: asset_path.
          import_files  -- Batch-import files from disk. Params: files (list of paths), destination_path, replace_existing=True.
          export        -- Export assets to disk. Params: asset_paths (list), output_dir.
        """
        t0 = time.time()
        conn = get_connection()
        if conn is None:
            return UEResponse.not_connected(TOOL)

        try:
            # ------------------------------------------------------------------
            if action == "search":
                if not query:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'query' is required for search", TOOL)
                params = {"search_text": query}
                if class_filter:
                    params["class_name"] = class_filter
                result = cmd(conn, "asset_find_assets", params)
                data = extract_data(result)
                # Apply client-side limit if the engine doesn't support it
                if isinstance(data, dict) and "assets" in data:
                    data["assets"] = data["assets"][:limit]
                elif isinstance(data, list):
                    data = data[:limit]
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "list":
                params = {"directory_path": directory_path, "recursive": recursive}
                if class_filter:
                    params["class_filter"] = class_filter
                result = cmd(conn, "asset_list_assets", params)
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "inspect":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for inspect", TOOL)
                result = cmd(conn, "asset_get_asset_info", {"asset_path": asset_path})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "duplicate":
                if not source_path or not destination_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameters 'source_path' and 'destination_path' are required for duplicate", TOOL)
                result = cmd(conn, "asset_duplicate_asset", {
                    "source_path": source_path, "destination_path": destination_path
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "rename":
                if not source_path or not destination_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameters 'source_path' and 'destination_path' are required for rename", TOOL)
                result = cmd(conn, "asset_rename_asset", {
                    "source_path": source_path, "destination_path": destination_path
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "delete":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for delete", TOOL)
                result = cmd(conn, "asset_delete_asset", {"asset_path": asset_path})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "save":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for save", TOOL)
                result = cmd(conn, "asset_save_asset", {"asset_path": asset_path})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "save_all":
                result = cmd(conn, "asset_save_all", {})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "exists":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for exists", TOOL)
                result = cmd(conn, "asset_does_asset_exist", {"asset_path": asset_path})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "import_files":
                if not files or not destination_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameters 'files' and 'destination_path' are required for import_files", TOOL)

                # Build Python code for batch import via AssetImportTask
                escaped_files = _json.dumps(files)
                escaped_dest = _json.dumps(destination_path)
                replace_flag = "True" if replace_existing else "False"

                py_code = f"""
import unreal
import json

files = {escaped_files}
destination = {escaped_dest}
replace_existing = {replace_flag}

tasks = []
for filepath in files:
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', filepath)
    task.set_editor_property('destination_path', destination)
    task.set_editor_property('replace_existing', replace_existing)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    tasks.append(task)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_tools.import_asset_tasks(tasks)

results = []
for task in tasks:
    imported = task.get_editor_property('imported_object_paths')
    results.append({{
        "source": task.get_editor_property('filename'),
        "imported_paths": list(imported) if imported else [],
    }})

print(json.dumps({{"imported": results, "count": len(results)}}))
""".strip()
                result = cmd(conn, "execute_python", {"code": py_code})
                data = extract_data(result)

                # Try to parse structured output
                output = data if isinstance(data, str) else data.get("output", "") if isinstance(data, dict) else str(data)
                try:
                    parsed = _json.loads(output.strip().split("\n")[-1])
                except Exception:
                    parsed = {"raw_output": output}

                return UEResponse.ok(parsed, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "export":
                if not asset_paths or not output_dir:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameters 'asset_paths' and 'output_dir' are required for export", TOOL)

                escaped_paths = _json.dumps(asset_paths)
                escaped_dir = _json.dumps(output_dir)

                py_code = f"""
import unreal
import json

asset_paths = {escaped_paths}
output_dir = {escaped_dir}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# Load assets
loaded = []
for p in asset_paths:
    obj = unreal.EditorAssetLibrary.load_asset(p)
    if obj:
        loaded.append(obj)

if not loaded:
    print(json.dumps({{"success": False, "error": "No assets could be loaded"}}))
else:
    # Export
    export_tasks = []
    for obj in loaded:
        task = unreal.AssetExportTask()
        task.set_editor_property('object', obj)
        task.set_editor_property('filename', output_dir + '/' + obj.get_name())
        task.set_editor_property('automated', True)
        task.set_editor_property('replace_identical', True)
        export_tasks.append(task)

    results = []
    for task in export_tasks:
        success = unreal.Exporter.run_asset_export_task(task)
        results.append({{
            "asset": task.get_editor_property('object').get_path_name(),
            "output": task.get_editor_property('filename'),
            "success": success,
        }})

    print(json.dumps({{"exported": results, "count": len(results)}}))
""".strip()
                result = cmd(conn, "execute_python", {"code": py_code})
                data = extract_data(result)

                output = data if isinstance(data, str) else data.get("output", "") if isinstance(data, dict) else str(data)
                try:
                    parsed = _json.loads(output.strip().split("\n")[-1])
                except Exception:
                    parsed = {"raw_output": output}

                return UEResponse.ok(parsed, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            else:
                return UEResponse.invalid_action(action, ACTIONS, TOOL)

        except ConnectionError as e:
            return UEResponse.not_connected(TOOL)
        except Exception as e:
            return UEResponse.error("E_INTERNAL", str(e), TOOL, retryable=False)
