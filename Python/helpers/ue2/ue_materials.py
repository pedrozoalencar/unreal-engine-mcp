"""UE2 Material tool -- create, edit expressions, connect, compile, apply, instances."""
import time
import json as _json
from typing import Any, Dict, Optional

from helpers.ue2.response import UEResponse, cmd, extract_data

TOOL = "ue_materials"
ACTIONS = [
    "create", "set_property", "get_info",
    "add_expression", "list_expressions", "list_pins",
    "connect", "connect_to_output", "set_value",
    "compile", "apply",
    "create_instance", "set_instance_params",
]


def register_material_tools(mcp, get_connection):
    """Register the ue_materials action-based tool."""

    @mcp.tool()
    def ue_materials(
        action: str,
        # common
        asset_path: str = "",
        # create
        blend_mode: str = "Opaque",
        shading_model: str = "DefaultLit",
        two_sided: bool = False,
        # set_property / set_value
        property_name: str = "",
        value: Any = None,
        # add_expression
        class_name: str = "",
        pos_x: int = 0,
        pos_y: int = 0,
        # list_pins / set_value / connect
        node_id: str = "",
        # connect
        source_node_id: str = "",
        target_node_id: str = "",
        input_pin_name: str = "",
        output_index: int = 0,
        # connect_to_output
        material_property: str = "",
        # apply
        actor_name: str = "",
        slot: int = 0,
        # create_instance
        parent_path: str = "",
        instance_path: str = "",
        # set_instance_params
        scalars: Optional[Dict[str, float]] = None,
        vectors: Optional[Dict[str, list]] = None,
        textures: Optional[Dict[str, str]] = None,
    ) -> Dict[str, Any]:
        """Unified material tool for Unreal Editor.

        Actions:
          create             -- Create a new material asset.
                                Params: asset_path, blend_mode="Opaque",
                                shading_model="DefaultLit", two_sided=False.
          set_property       -- Set a material property (blend_mode, shading_model, etc.).
                                Params: asset_path, property_name, value.
          get_info           -- Get material info: settings and expression nodes.
                                Params: asset_path.
          add_expression     -- Add a material expression node. Returns node_id.
                                Params: asset_path, class_name, pos_x=0, pos_y=0.
          list_expressions   -- List all expression nodes in a material.
                                Params: asset_path.
          list_pins          -- List all input/output pins of an expression node.
                                Params: asset_path, node_id.
          connect            -- Connect output of one expression to input of another.
                                Params: asset_path, source_node_id, target_node_id,
                                input_pin_name, output_index=0.
          connect_to_output  -- Connect an expression to a material output property.
                                Params: asset_path, source_node_id, material_property,
                                output_index=0.
          set_value          -- Set a value on a material expression node.
                                Params: asset_path, node_id, property_name, value.
          compile            -- Recompile and save a material.
                                Params: asset_path.
          apply              -- Apply a material to an actor's mesh component.
                                Params: asset_path, actor_name, slot=0.
          create_instance    -- Create a MaterialInstanceConstant from a parent material.
                                Params: parent_path, instance_path.
          set_instance_params -- Set scalar/vector/texture parameters on a material instance.
                                Params: instance_path, scalars={}, vectors={}, textures={}.
        """
        t0 = time.time()
        conn = get_connection()
        if conn is None:
            return UEResponse.not_connected(TOOL)

        try:
            # ------------------------------------------------------------------
            if action == "create":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for create", TOOL)
                result = cmd(conn, "mat_create_material", {
                    "asset_path": asset_path,
                    "blend_mode": blend_mode,
                    "shading_model": shading_model,
                    "two_sided": two_sided,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "set_property":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for set_property", TOOL)
                if not property_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'property_name' is required for set_property", TOOL)
                result = cmd(conn, "mat_set_material_property", {
                    "asset_path": asset_path,
                    "property_name": property_name,
                    "property_value": str(value),
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "get_info":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for get_info", TOOL)
                result = cmd(conn, "mat_get_material_info", {"asset_path": asset_path})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "add_expression":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for add_expression", TOOL)
                if not class_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'class_name' is required for add_expression", TOOL)
                result = cmd(conn, "mat_add_expression", {
                    "asset_path": asset_path,
                    "class_name": class_name,
                    "pos_x": pos_x,
                    "pos_y": pos_y,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "list_expressions":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for list_expressions", TOOL)
                result = cmd(conn, "mat_list_expressions", {"asset_path": asset_path})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "list_pins":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for list_pins", TOOL)
                if not node_id:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'node_id' is required for list_pins", TOOL)
                result = cmd(conn, "mat_list_pins", {
                    "asset_path": asset_path,
                    "node_id": node_id,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "connect":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for connect", TOOL)
                if not source_node_id or not target_node_id:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameters 'source_node_id' and 'target_node_id' are required for connect", TOOL)
                if not input_pin_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'input_pin_name' is required for connect", TOOL)
                result = cmd(conn, "mat_connect_expressions", {
                    "asset_path": asset_path,
                    "source_node_id": source_node_id,
                    "target_node_id": target_node_id,
                    "input_pin_name": input_pin_name,
                    "output_index": output_index,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "connect_to_output":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for connect_to_output", TOOL)
                if not source_node_id:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'source_node_id' is required for connect_to_output", TOOL)
                if not material_property:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'material_property' is required for connect_to_output", TOOL)
                result = cmd(conn, "mat_connect_to_output", {
                    "asset_path": asset_path,
                    "source_node_id": source_node_id,
                    "property_name": material_property,
                    "output_index": output_index,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "set_value":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for set_value", TOOL)
                if not node_id:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'node_id' is required for set_value", TOOL)
                if not property_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'property_name' is required for set_value", TOOL)
                result = cmd(conn, "mat_set_expression_value", {
                    "asset_path": asset_path,
                    "node_id": node_id,
                    "property_name": property_name,
                    "value": str(value),
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "compile":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for compile", TOOL)
                result = cmd(conn, "mat_compile_material", {"asset_path": asset_path})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "apply":
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for apply", TOOL)
                if not actor_name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'actor_name' is required for apply", TOOL)
                result = cmd(conn, "mat_apply_material_to_actor", {
                    "asset_path": asset_path,
                    "actor_name": actor_name,
                    "slot_index": slot,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "create_instance":
                if not parent_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'parent_path' is required", TOOL)
                if not instance_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'instance_path' is required", TOOL)
                # Native C++ handler
                result = cmd(conn, "mat_create_material_instance", {
                    "parent_path": parent_path, "instance_path": instance_path
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "set_instance_params":
                if not instance_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'instance_path' is required", TOOL)
                # Native C++ handler
                params = {"instance_path": instance_path}
                if scalars: params["scalars"] = scalars
                if vectors: params["vectors"] = vectors
                if textures: params["textures"] = textures
                result = cmd(conn, "mat_set_instance_params", params)
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            else:
                return UEResponse.invalid_action(action, ACTIONS, TOOL)

        except ConnectionError:
            return UEResponse.not_connected(TOOL)
        except Exception as e:
            return UEResponse.error("E_INTERNAL", str(e), TOOL, retryable=False)
