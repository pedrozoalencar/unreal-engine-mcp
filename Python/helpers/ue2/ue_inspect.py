"""UE2 Inspect tool -- runtime class and enum introspection."""
import time
from typing import Any, Dict

from helpers.ue2.response import UEResponse, cmd, extract_data

TOOL = "ue_inspect"
ACTIONS = ["class_info", "enum_values"]


def register_inspect_tools(mcp, get_connection):
    """Register the ue_inspect action-based tool."""

    @mcp.tool()
    def ue_inspect(
        action: str,
        class_name: str = "",
        include_inherited: bool = False,
        enum_name: str = "",
    ) -> Dict[str, Any]:
        """Runtime introspection for Unreal Engine classes and enums.

        Actions:
          class_info    -- List all BlueprintCallable functions and visible properties of a UE class.
                           Params: class_name (str), include_inherited (bool, default False).
                           Examples: 'Actor', 'StaticMeshComponent', 'GeneratedDynamicMeshActor'.
          enum_values   -- List all values of a UEnum type.
                           Params: enum_name (str).
                           Examples: 'EBlendMode', 'EGeometryScriptBooleanOperation'.
        """
        t0 = time.time()
        conn = get_connection()
        if conn is None:
            return UEResponse.not_connected(TOOL)

        try:
            # ------------------------------------------------------------------
            if action == "class_info":
                if not class_name:
                    return UEResponse.error(
                        "E_INVALID_ARGUMENT",
                        "Parameter 'class_name' is required for class_info",
                        TOOL,
                    )
                result = cmd(conn, "inspect_class", {
                    "class_name": class_name,
                    "include_inherited": include_inherited,
                })
                data = extract_data(result)
                return UEResponse.ok(
                    data,
                    tool=TOOL,
                    duration_ms=(time.time() - t0) * 1000,
                )

            # ------------------------------------------------------------------
            elif action == "enum_values":
                if not enum_name:
                    return UEResponse.error(
                        "E_INVALID_ARGUMENT",
                        "Parameter 'enum_name' is required for enum_values",
                        TOOL,
                    )
                result = cmd(conn, "inspect_enum", {"enum_name": enum_name})
                data = extract_data(result)
                return UEResponse.ok(
                    data,
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
