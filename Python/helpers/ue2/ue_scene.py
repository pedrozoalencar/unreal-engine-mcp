"""UE2 Scene tool -- actors, transforms, properties, selection, levels."""
import time
import json as _json
from typing import Any, Dict, List, Optional

from helpers.ue2.response import UEResponse, cmd, extract_data

TOOL = "ue_scene"
ACTIONS = [
    "list_actors", "find_actor", "spawn_actor", "destroy_actor",
    "set_transform", "get_properties", "set_property",
    "get_selection", "set_selection", "load_level", "save_level",
    "duplicate_actor", "focus_actor",
]


def _resolve_actor_name(conn, name: str) -> str:
    """Resolve actor name: tries label first via level handler, falls back to internal name.
    Returns the internal name that legacy commands need."""
    # Try as label via level_get_actor_properties (accepts labels)
    try:
        result = conn.send_command("level_get_actor_properties", {"actor_name": name})
        if result and result.get("status") != "error":
            data = result.get("result", result)
            if data.get("success"):
                return data.get("actor_name", name)  # Returns internal name
    except Exception:
        pass
    return name  # Fallback: assume it's already the internal name


def register_scene_tools(mcp, get_connection):
    """Register the ue_scene action-based tool."""

    @mcp.tool()
    def ue_scene(
        action: str,
        # list_actors
        class_filter: str = "",
        label_query: str = "",
        limit: int = 100,
        # find_actor / destroy_actor / focus_actor / get_properties / duplicate_actor
        name: str = "",
        # spawn_actor
        blueprint_path: str = "",
        class_name: str = "",
        location: List[float] = [0, 0, 0],
        rotation: List[float] = [0, 0, 0],
        scale: List[float] = [1, 1, 1],
        label: str = "",
        # set_property
        property_name: str = "",
        value: Any = None,
        # set_selection
        actor_paths: List[str] = [],
        # load_level
        level_path: str = "",
    ) -> Dict[str, Any]:
        """Unified scene/level management tool for Unreal Editor.

        Actions:
          list_actors    -- List actors in the level. Params: class_filter="", label_query="", limit=100.
          find_actor     -- Find actor(s) by name. Params: name.
          spawn_actor    -- Spawn an actor. Params: blueprint_path OR class_name, location, rotation, scale, label.
          destroy_actor  -- Delete an actor. Params: name.
          set_transform  -- Set actor transform. Params: name, location, rotation, scale.
          get_properties -- Get all properties of an actor. Params: name.
          set_property   -- Set a single property. Params: name, property_name, value.
          get_selection  -- Get currently selected actors. No params.
          set_selection  -- Select actors by path. Params: actor_paths (list).
          load_level     -- Load a level in the editor. Params: level_path.
          save_level     -- Save the current level. No params.
          duplicate_actor -- Duplicate an actor. Params: name.
          focus_actor    -- Focus viewport on an actor. Params: name.
        """
        t0 = time.time()
        conn = get_connection()
        if conn is None:
            return UEResponse.not_connected(TOOL)

        try:
            # ------------------------------------------------------------------
            if action == "list_actors":
                # Use label_query if provided, else list all
                if label_query:
                    result = cmd(conn, "find_actors_by_name", {"search_text": label_query})
                else:
                    result = cmd(conn, "get_actors_in_level", {})

                data = extract_data(result)

                # Client-side filtering and limiting
                actors = data if isinstance(data, list) else data.get("actors", []) if isinstance(data, dict) else []

                if class_filter and isinstance(actors, list):
                    cf_lower = class_filter.lower()
                    actors = [a for a in actors if isinstance(a, dict) and cf_lower in a.get("class", "").lower()]

                if isinstance(actors, list):
                    actors = actors[:limit]

                return UEResponse.ok(
                    {"actors": actors, "count": len(actors)},
                    tool=TOOL, duration_ms=(time.time() - t0) * 1000,
                )

            # ------------------------------------------------------------------
            elif action == "find_actor":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for find_actor", TOOL)
                result = cmd(conn, "find_actors_by_name", {"search_text": name})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "spawn_actor":
                if blueprint_path:
                    # Spawn blueprint actor via the level handler
                    params = {
                        "blueprint_path": blueprint_path,
                        "location": location,
                        "rotation": rotation,
                        "scale": scale,
                    }
                    if label:
                        params["actor_label"] = label
                    result = cmd(conn, "level_spawn_blueprint_by_path", params)
                elif class_name:
                    # Spawn basic class actor via the editor handler
                    params = {
                        "class_name": class_name,
                        "location": location,
                        "rotation": rotation,
                        "scale": scale,
                    }
                    if label:
                        params["actor_name"] = label
                    result = cmd(conn, "spawn_actor", params)
                else:
                    return UEResponse.error(
                        "E_INVALID_ARGUMENT",
                        "Either 'blueprint_path' or 'class_name' is required for spawn_actor",
                        TOOL,
                    )

                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "destroy_actor":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for destroy_actor", TOOL)
                resolved = _resolve_actor_name(conn, name)
                result = cmd(conn, "delete_actor", {"actor_name": resolved})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "set_transform":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for set_transform", TOOL)
                resolved = _resolve_actor_name(conn, name)
                params = {
                    "actor_name": resolved,
                    "location": location,
                    "rotation": rotation,
                    "scale": scale,
                }
                result = cmd(conn, "set_actor_transform", params)
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "get_properties":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for get_properties", TOOL)
                result = cmd(conn, "level_get_actor_properties", {"actor_name": name})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "set_property":
                if not name or not property_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameters 'name' and 'property_name' are required for set_property", TOOL)
                result = cmd(conn, "level_set_actor_property", {
                    "actor_name": name,
                    "property_name": property_name,
                    "property_value": str(value) if value is not None else "",
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "get_selection":
                py_code = """
import unreal
import json

subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
selected = subsystem.get_selected_level_actors()

actors = []
for actor in selected:
    actors.append({
        "name": actor.get_actor_label(),
        "class": actor.get_class().get_name(),
        "path": actor.get_path_name(),
        "location": [
            actor.get_actor_location().x,
            actor.get_actor_location().y,
            actor.get_actor_location().z,
        ],
    })

print(json.dumps({"selected": actors, "count": len(actors)}))
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
            elif action == "set_selection":
                if not actor_paths:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'actor_paths' is required for set_selection", TOOL)

                escaped_paths = _json.dumps(actor_paths)
                py_code = f"""
import unreal
import json

actor_paths = {escaped_paths}

subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
# Clear current selection
subsystem.select_nothing()

world = unreal.EditorLevelLibrary.get_editor_world()
all_actors = unreal.EditorLevelLibrary.get_all_level_actors()

matched = []
for actor in all_actors:
    if actor.get_path_name() in actor_paths or actor.get_actor_label() in actor_paths:
        subsystem.set_actor_selection_state(actor, True)
        matched.append(actor.get_actor_label())

print(json.dumps({{"selected": matched, "count": len(matched)}}))
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
            elif action == "load_level":
                if not level_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'level_path' is required for load_level", TOOL)

                escaped_path = _json.dumps(level_path)
                py_code = f"""
import unreal
import json

level_path = {escaped_path}
subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
success = subsystem.load_level(level_path)
world = unreal.EditorLevelLibrary.get_editor_world()
level_name = world.get_name() if world else "Unknown"

print(json.dumps({{"success": success, "level_name": level_name, "level_path": level_path}}))
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
            elif action == "save_level":
                result = cmd(conn, "level_save_current_level", {})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "duplicate_actor":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for duplicate_actor", TOOL)

                escaped_name = _json.dumps(name)
                py_code = f"""
import unreal
import json

actor_name = {escaped_name}

all_actors = unreal.EditorLevelLibrary.get_all_level_actors()
target = None
for actor in all_actors:
    if actor.get_actor_label() == actor_name or actor.get_name() == actor_name:
        target = actor
        break

if target is None:
    print(json.dumps({{"success": False, "error": f"Actor '{{actor_name}}' not found"}}))
else:
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    duplicated = subsystem.duplicate_actors([target])
    results = []
    for dup in duplicated:
        results.append({{
            "name": dup.get_actor_label(),
            "class": dup.get_class().get_name(),
            "path": dup.get_path_name(),
        }})
    print(json.dumps({{"success": True, "duplicated": results, "count": len(results)}}))
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
            elif action == "focus_actor":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for focus_actor", TOOL)
                result = cmd(conn, "level_focus_actor", {"actor_name": name})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            else:
                return UEResponse.invalid_action(action, ACTIONS, TOOL)

        except ConnectionError as e:
            return UEResponse.not_connected(TOOL)
        except Exception as e:
            return UEResponse.error("E_INTERNAL", str(e), TOOL, retryable=False)
