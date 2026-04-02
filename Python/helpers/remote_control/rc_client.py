"""HTTP client for Unreal Engine Remote Control API."""
import requests
import logging
from typing import Dict, Any, Optional

logger = logging.getLogger("UnrealMCP_RC")

RC_BASE_URL = "http://localhost:30010/remote"


class RemoteControlClient:
    """Client for Unreal Engine Remote Control API (HTTP REST on port 30010)."""

    def __init__(self, base_url: str = RC_BASE_URL):
        self.base_url = base_url
        self.session = requests.Session()
        self.session.headers.update({"Content-Type": "application/json"})

    def get_property(self, object_path: str, property_name: str) -> Dict[str, Any]:
        try:
            resp = self.session.put(
                f"{self.base_url}/object/property",
                json={
                    "objectPath": object_path,
                    "propertyName": property_name,
                    "access": "READ_ACCESS"
                }
            )
            resp.raise_for_status()
            return {"success": True, "data": resp.json()}
        except Exception as e:
            return {"success": False, "error": str(e)}

    def set_property(self, object_path: str, property_name: str, value: Any) -> Dict[str, Any]:
        try:
            resp = self.session.put(
                f"{self.base_url}/object/property",
                json={
                    "objectPath": object_path,
                    "propertyName": property_name,
                    "propertyValue": {"value" if not isinstance(value, dict) else None: value} if not isinstance(value, dict) else value,
                    "access": "WRITE_TRANSACTION_ACCESS"
                }
            )
            resp.raise_for_status()
            return {"success": True, "data": resp.json()}
        except Exception as e:
            return {"success": False, "error": str(e)}

    def call_function(self, object_path: str, function_name: str, params: Optional[Dict] = None) -> Dict[str, Any]:
        try:
            resp = self.session.put(
                f"{self.base_url}/object/call",
                json={
                    "objectPath": object_path,
                    "functionName": function_name,
                    "parameters": params or {}
                }
            )
            resp.raise_for_status()
            return {"success": True, "data": resp.json()}
        except Exception as e:
            return {"success": False, "error": str(e)}

    def search_objects(self, query: str, class_name: str = "", outer_path: str = "") -> Dict[str, Any]:
        try:
            body = {"Query": query}
            if class_name: body["Class"] = class_name
            if outer_path: body["Outer"] = outer_path
            resp = self.session.put(f"{self.base_url}/search/assets", json=body)
            resp.raise_for_status()
            return {"success": True, "data": resp.json()}
        except Exception as e:
            return {"success": False, "error": str(e)}

    def list_presets(self) -> Dict[str, Any]:
        try:
            resp = self.session.get(f"{self.base_url}/presets")
            resp.raise_for_status()
            return {"success": True, "data": resp.json()}
        except Exception as e:
            return {"success": False, "error": str(e)}

    def get_preset(self, preset_name: str) -> Dict[str, Any]:
        try:
            resp = self.session.get(f"{self.base_url}/preset/{preset_name}")
            resp.raise_for_status()
            return {"success": True, "data": resp.json()}
        except Exception as e:
            return {"success": False, "error": str(e)}

    def batch(self, requests_list: list) -> Dict[str, Any]:
        try:
            resp = self.session.put(
                f"{self.base_url}/object/transaction",
                json={"Requests": requests_list}
            )
            resp.raise_for_status()
            return {"success": True, "data": resp.json()}
        except Exception as e:
            return {"success": False, "error": str(e)}
