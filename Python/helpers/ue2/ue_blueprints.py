"""UE2 Blueprint tool -- create, compile, add nodes, connect, set pins, variables, functions."""
import time
from typing import Any, Dict, List, Optional

from helpers.ue2.response import UEResponse, cmd, extract_data

TOOL = "ue_blueprints"
ACTIONS = [
    "create", "compile", "add_node", "connect", "set_pin",
    "override_event", "add_variable", "create_function",
    "analyze_graph", "read_content",
]


def register_blueprint_tools(mcp, get_connection):
    """Register the ue_blueprints action-based tool."""

    @mcp.tool()
    def ue_blueprints(
        action: str,
        # create
        name: str = "",
        parent_class: str = "Actor",
        path: str = "/Game/Blueprints",
        # add_node
        blueprint_name: str = "",
        node_type: str = "",
        target_function: str = "",
        function_name: str = "",
        pos_x: int = 0,
        pos_y: int = 0,
        target_blueprint: str = "",
        # connect
        source_node_id: str = "",
        source_pin: str = "",
        target_node_id: str = "",
        target_pin: str = "",
        # set_pin
        node_id: str = "",
        pin_name: str = "",
        pin_value: str = "",
        # override_event
        event_name: str = "",
        # add_variable
        variable_name: str = "",
        variable_type: str = "",
        # analyze_graph
        blueprint_path: str = "",
        graph_name: str = "EventGraph",
    ) -> Dict[str, Any]:
        """Unified blueprint tool for Unreal Editor.

        Actions:
          create           -- Create a new Blueprint asset.
                              Params: name, parent_class="Actor", path="/Game/Blueprints".
          compile          -- Compile a Blueprint.
                              Params: name.
          add_node         -- Add a node to a Blueprint graph.
                              Params: blueprint_name, node_type, target_function="",
                              function_name="", pos_x=0, pos_y=0, target_blueprint="".
          connect          -- Connect two node pins.
                              Params: blueprint_name, source_node_id, source_pin,
                              target_node_id, target_pin, function_name="".
          set_pin          -- Set a pin value on a node.
                              Params: blueprint_name, node_id, pin_name, pin_value,
                              function_name="".
          override_event   -- Add an event override (e.g. BeginPlay, Tick).
                              Params: blueprint_name, event_name.
          add_variable     -- Create a variable in a Blueprint.
                              Params: blueprint_name, variable_name, variable_type.
          create_function  -- Create a new function graph in a Blueprint.
                              Params: blueprint_name, function_name.
          analyze_graph    -- Analyze a Blueprint graph and return node/connection info.
                              Params: blueprint_path, graph_name="EventGraph".
          read_content     -- Read full Blueprint content (nodes, connections, variables).
                              Params: blueprint_path.
        """
        t0 = time.time()
        conn = get_connection()
        if conn is None:
            return UEResponse.not_connected(TOOL)

        try:
            # ------------------------------------------------------------------
            if action == "create":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for create", TOOL)
                result = cmd(conn, "create_blueprint", {
                    "name": name,
                    "parent_class": parent_class,
                    "path": path,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "compile":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for compile", TOOL)
                result = cmd(conn, "compile_blueprint", {"name": name})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "add_node":
                if not blueprint_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'blueprint_name' is required for add_node", TOOL)
                if not node_type:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'node_type' is required for add_node", TOOL)
                params = {
                    "blueprint_name": blueprint_name,
                    "node_type": node_type,
                    "pos_x": pos_x,
                    "pos_y": pos_y,
                }
                if target_function:
                    params["target_function"] = target_function
                if function_name:
                    params["function_name"] = function_name
                if target_blueprint:
                    params["target_blueprint"] = target_blueprint
                result = cmd(conn, "add_blueprint_node", params)
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "connect":
                if not blueprint_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'blueprint_name' is required for connect", TOOL)
                if not source_node_id or not target_node_id:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameters 'source_node_id' and 'target_node_id' are required for connect", TOOL)
                if not source_pin or not target_pin:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameters 'source_pin' and 'target_pin' are required for connect", TOOL)
                params = {
                    "blueprint_name": blueprint_name,
                    "source_node_id": source_node_id,
                    "source_pin": source_pin,
                    "target_node_id": target_node_id,
                    "target_pin": target_pin,
                }
                if function_name:
                    params["function_name"] = function_name
                result = cmd(conn, "connect_nodes", params)
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "set_pin":
                if not blueprint_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'blueprint_name' is required for set_pin", TOOL)
                if not node_id or not pin_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameters 'node_id' and 'pin_name' are required for set_pin", TOOL)
                params = {
                    "blueprint_name": blueprint_name,
                    "node_id": node_id,
                    "pin_name": pin_name,
                    "pin_value": pin_value,
                }
                if function_name:
                    params["function_name"] = function_name
                result = cmd(conn, "set_pin_value", params)
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "override_event":
                if not blueprint_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'blueprint_name' is required for override_event", TOOL)
                if not event_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'event_name' is required for override_event", TOOL)
                result = cmd(conn, "add_event_override", {
                    "blueprint_name": blueprint_name,
                    "event_name": event_name,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "add_variable":
                if not blueprint_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'blueprint_name' is required for add_variable", TOOL)
                if not variable_name or not variable_type:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameters 'variable_name' and 'variable_type' are required for add_variable", TOOL)
                result = cmd(conn, "create_variable", {
                    "blueprint_name": blueprint_name,
                    "variable_name": variable_name,
                    "variable_type": variable_type,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "create_function":
                if not blueprint_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'blueprint_name' is required for create_function", TOOL)
                if not function_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'function_name' is required for create_function", TOOL)
                result = cmd(conn, "create_function", {
                    "blueprint_name": blueprint_name,
                    "function_name": function_name,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "analyze_graph":
                if not blueprint_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'blueprint_path' is required for analyze_graph", TOOL)
                result = cmd(conn, "analyze_blueprint_graph", {
                    "blueprint_path": blueprint_path,
                    "graph_name": graph_name,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "read_content":
                if not blueprint_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'blueprint_path' is required for read_content", TOOL)
                result = cmd(conn, "read_blueprint_content", {
                    "blueprint_path": blueprint_path,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            else:
                return UEResponse.invalid_action(action, ACTIONS, TOOL)

        except ConnectionError:
            return UEResponse.not_connected(TOOL)
        except Exception as e:
            return UEResponse.error("E_INTERNAL", str(e), TOOL, retryable=False)
