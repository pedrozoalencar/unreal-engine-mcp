"""Standardized response contract for UE2 tools.
All responses follow: {ok, data, warnings, error, meta}"""
import time
import uuid
from typing import Any, Dict, List, Optional

# Global request counter for tracing
_request_counter = 0


def _next_request_id() -> str:
    """Generate a unique request ID for tracing."""
    global _request_counter
    _request_counter += 1
    return f"req-{_request_counter:04d}"


class UEResponse:
    """Builder for standardized MCP responses."""

    @staticmethod
    def ok(data: Any = None, tool: str = "", warnings: List[str] = None, duration_ms: float = 0, request_id: str = "") -> Dict:
        rid = request_id or _next_request_id()
        resp = {
            "ok": True,
            "data": data or {},
            "meta": {
                "tool": tool,
                "duration_ms": round(duration_ms, 1),
                "request_id": rid
            }
        }
        if warnings:
            resp["warnings"] = [{"message": w} for w in warnings]
        return resp

    @staticmethod
    def error(code: str, message: str, tool: str = "", details: Any = None, retryable: bool = False, request_id: str = "") -> Dict:
        rid = request_id or _next_request_id()
        return {
            "ok": False,
            "error": {"code": code, "message": message, "details": details, "retryable": retryable},
            "meta": {"tool": tool, "request_id": rid}
        }

    @staticmethod
    def not_connected(tool: str = "") -> Dict:
        return UEResponse.error("E_NOT_CONNECTED", "Not connected to Unreal Engine", tool, retryable=True)

    @staticmethod
    def not_found(what: str, tool: str = "") -> Dict:
        return UEResponse.error("E_NOT_FOUND", f"{what} not found", tool)

    @staticmethod
    def invalid_action(action: str, valid: List[str], tool: str = "") -> Dict:
        return UEResponse.error("E_INVALID_ARGUMENT",
            f"Unknown action '{action}'. Valid: {', '.join(valid)}", tool)


def cmd(conn, command: str, params: dict) -> dict:
    """Send command to Unreal and return raw result. Raises on connection error."""
    result = conn.send_command(command, params)
    if result is None:
        raise ConnectionError("No response from Unreal")
    return result


def extract_data(result: dict) -> Any:
    """Extract the data payload from a raw MCP response."""
    if "result" in result:
        return result["result"]
    return result
