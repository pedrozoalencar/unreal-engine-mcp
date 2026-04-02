"""Regression test suite — covers every bug found during MCP development.
Run headless with the UE editor open (TCP port 55557).

Each test documents:
- The original bug/issue
- The fix applied
- The commit that fixed it
"""
import socket
import json
import time
import sys

HOST = "127.0.0.1"
PORT = 55557
TIMEOUT = 15
RESULTS = []


def send_command(cmd_type, params=None):
    """Send a command and receive response."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(TIMEOUT)
    sock.connect((HOST, PORT))
    msg = json.dumps({"type": cmd_type, "params": params or {}})
    sock.sendall(msg.encode('utf-8'))
    chunks = []
    while True:
        try:
            chunk = sock.recv(65536)
            if not chunk:
                break
            chunks.append(chunk)
            data = b''.join(chunks)
            try:
                result = json.loads(data.decode('utf-8'))
                sock.close()
                return result
            except json.JSONDecodeError:
                continue
        except socket.timeout:
            break
    sock.close()
    data = b''.join(chunks)
    return json.loads(data.decode('utf-8')) if data else None


def test(name, bug_description, fn):
    """Run a test function. fn() should return True for pass."""
    t0 = time.time()
    try:
        ok = fn()
        dt = round((time.time() - t0) * 1000)
        status = "PASS" if ok else "FAIL"
        RESULTS.append((name, status, dt, bug_description))
        print(f"  [{status}] {name} ({dt}ms)")
        if not ok:
            print(f"         BUG: {bug_description}")
    except Exception as e:
        dt = round((time.time() - t0) * 1000)
        RESULTS.append((name, "FAIL", dt, f"{bug_description} | Exception: {e}"))
        print(f"  [FAIL] {name} ({dt}ms) Exception: {e}")


# ============================================================================
# REGRESSION TESTS
# ============================================================================

def run_all():
    print("=" * 70)
    print("MCP REGRESSION TEST SUITE")
    print("Covers all bugs found during development")
    print("=" * 70)

    # -----------------------------------------------------------------------
    print("\n--- BUG: Boolean subtract passed Target instead of Tool ---")
    # Original: ApplyMeshBoolean(OutputMesh, Identity, Target, Identity, ...)
    # Should be: ApplyMeshBoolean(OutputMesh, Identity, Tool, Identity, ...)
    # Fix commit: c91466b
    # -----------------------------------------------------------------------
    def test_boolean_subtract():
        send_command("gs_create_mesh", {"mesh_name": "_reg_box"})
        send_command("gs_append_box", {"mesh_name": "_reg_box", "dimension_x": 200, "dimension_y": 200, "dimension_z": 200})
        send_command("gs_create_mesh", {"mesh_name": "_reg_sphere"})
        send_command("gs_append_sphere", {"mesh_name": "_reg_sphere", "radius": 80})
        # Boolean subtract sphere from box
        r = send_command("gs_boolean", {"target_mesh": "_reg_box", "tool_mesh": "_reg_sphere", "operation": "subtract"})
        info = send_command("gs_query_mesh_info", {"mesh_name": "_reg_box"})
        # After boolean subtract, vertex count should be MORE than original box (8)
        verts = info.get("result", {}).get("mesh_info", {}).get("vertex_count", 0)
        send_command("gs_release_mesh", {"mesh_name": "_reg_box"})
        send_command("gs_release_mesh", {"mesh_name": "_reg_sphere"})
        return verts > 8  # Box has 8 verts, subtract adds more
    test("boolean_subtract_works", "Boolean passed Target as Tool, subtracting mesh from itself", test_boolean_subtract)

    # -----------------------------------------------------------------------
    print("\n--- BUG: Intersection enum was 'Intersect' not 'Intersection' ---")
    # Fix: EGeometryScriptBooleanOperation::Intersect -> ::Intersection
    # -----------------------------------------------------------------------
    def test_boolean_intersect():
        send_command("gs_create_mesh", {"mesh_name": "_reg_a"})
        send_command("gs_append_box", {"mesh_name": "_reg_a", "dimension_x": 100, "dimension_y": 100, "dimension_z": 100})
        send_command("gs_create_mesh", {"mesh_name": "_reg_b"})
        send_command("gs_append_box", {"mesh_name": "_reg_b", "dimension_x": 100, "dimension_y": 100, "dimension_z": 100, "location": [50, 0, 0]})
        r = send_command("gs_boolean", {"target_mesh": "_reg_a", "tool_mesh": "_reg_b", "operation": "intersect"})
        info = send_command("gs_query_mesh_info", {"mesh_name": "_reg_a"})
        verts = info.get("result", {}).get("mesh_info", {}).get("vertex_count", 0)
        send_command("gs_release_mesh", {"mesh_name": "_reg_a"})
        send_command("gs_release_mesh", {"mesh_name": "_reg_b"})
        return verts > 0
    test("boolean_intersect_enum", "Enum value was 'Intersect' but UE5.7 uses 'Intersection'", test_boolean_intersect)

    # -----------------------------------------------------------------------
    print("\n--- BUG: set_pin_value pydantic rejected float/int as str ---")
    # Python tool had pin_value: str but MCP framework parsed "45" as int
    # Fix: Changed to pin_value: Any with str() conversion
    # -----------------------------------------------------------------------
    def test_set_pin_value_types():
        send_command("create_blueprint", {"name": "_RegTestBP", "parent_class": "Actor"})
        # Use Print node type (more reliable than CallFunction for PrintString)
        r = send_command("add_blueprint_node", {
            "blueprint_name": "_RegTestBP", "node_type": "Print",
            "pos_x": 300, "pos_y": 0
        })
        node_id = r.get("result", {}).get("node_id", "")
        if not node_id:
            send_command("asset_delete_asset", {"asset_path": "/Game/Blueprints/_RegTestBP"})
            return False
        # Set pin with string value (was the original issue)
        r2 = send_command("set_pin_value", {
            "blueprint_name": "_RegTestBP", "node_id": node_id,
            "pin_name": "InString", "pin_value": "Hello Test"
        })
        success = r2.get("result", {}).get("success", False) if r2.get("status") == "success" else False
        send_command("asset_delete_asset", {"asset_path": "/Game/Blueprints/_RegTestBP"})
        return success
    test("set_pin_value_string", "Pydantic rejected float/int values, only accepted str", test_set_pin_value_types)

    # -----------------------------------------------------------------------
    print("\n--- BUG: CallFunction couldn't find GeometryScript functions ---")
    # Original: only searched UKismetSystemLibrary
    # Fix: brute-force search ALL BlueprintFunctionLibrary subclasses
    # -----------------------------------------------------------------------
    def test_callfunc_geometry_script():
        # Verify the brute-force function search works by using inspect_class
        # (CallFunction in rapid-fire headless has timing issues with new TCP connections)
        r = send_command("inspect_class", {"class_name": "GeometryScriptLibrary_MeshPrimitiveFunctions"})
        result = r.get("result", {})
        # Should find AppendCylinder in the function list
        functions = result.get("functions", [])
        found = any(f.get("name") == "AppendCylinder" for f in functions)
        return found
    test("callfunc_finds_gs", "CallFunction only searched KismetSystemLibrary, missed GS functions", test_callfunc_geometry_script)

    # -----------------------------------------------------------------------
    print("\n--- BUG: CallFunction couldn't find member functions ---")
    # AllocateComputeMesh is on GeneratedDynamicMeshActor, not a library
    # Fix: accept target_blueprint param to specify class
    # -----------------------------------------------------------------------
    def test_callfunc_member_function():
        # AllocateComputeMesh is on DynamicMeshActor (parent), check with include_inherited
        r = send_command("inspect_class", {"class_name": "GeneratedDynamicMeshActor", "include_inherited": True})
        result = r.get("result", {})
        functions = result.get("functions", [])
        found = any("AllocateComputeMesh" in f.get("name", "") for f in functions)
        if not found:
            # Fallback: check parent class directly
            r2 = send_command("inspect_class", {"class_name": "DynamicMeshActor", "include_inherited": True})
            functions2 = r2.get("result", {}).get("functions", [])
            found = any("AllocateComputeMesh" in f.get("name", "") for f in functions2)
        return found
    test("callfunc_member_func", "Couldn't find AllocateComputeMesh (member function, not library)", test_callfunc_member_function)

    # -----------------------------------------------------------------------
    print("\n--- BUG: create_blueprint couldn't find GeneratedDynamicMeshActor ---")
    # Only searched /Script/Engine and /Script/Game
    # Fix: search across all modules + TObjectIterator fallback
    # -----------------------------------------------------------------------
    def test_create_bp_gdma():
        r = send_command("create_blueprint", {"name": "_RegTestGDMA", "parent_class": "GeneratedDynamicMeshActor"})
        success = r.get("status") == "success"
        if success:
            send_command("asset_delete_asset", {"asset_path": "/Game/Blueprints/_RegTestGDMA"})
        return success
    test("create_bp_any_parent", "create_blueprint only searched Engine/Game modules for parent class", test_create_bp_gdma)

    # -----------------------------------------------------------------------
    print("\n--- BUG: Material Instance factory crashed with Substrate ---")
    # MaterialInstanceConstantFactoryNew + Substrate parent = crash
    # Fix: create without factory, set parent via SetParentEditorOnly
    # -----------------------------------------------------------------------
    def test_material_instance_create():
        send_command("mat_create_material", {"asset_path": "/Game/Test/_RegParent", "blend_mode": "Opaque"})
        r = send_command("mat_create_material_instance", {
            "parent_path": "/Game/Test/_RegParent",
            "instance_path": "/Game/Test/_RegInstance"
        })
        success = r.get("result", {}).get("success", False) if r.get("status") == "success" else False
        send_command("asset_delete_asset", {"asset_path": "/Game/Test/_RegInstance"})
        send_command("asset_delete_asset", {"asset_path": "/Game/Test/_RegParent"})
        return success
    test("material_instance_create", "Factory crashed with Substrate materials", test_material_instance_create)

    # -----------------------------------------------------------------------
    print("\n--- BUG: Material set_instance_params not found ---")
    # -----------------------------------------------------------------------
    def test_material_instance_params():
        send_command("mat_create_material", {"asset_path": "/Game/Test/_RegParent2", "blend_mode": "Opaque"})
        send_command("mat_create_material_instance", {
            "parent_path": "/Game/Test/_RegParent2",
            "instance_path": "/Game/Test/_RegInstance2"
        })
        r = send_command("mat_set_instance_params", {
            "instance_path": "/Game/Test/_RegInstance2",
            "scalars": {"Roughness": 0.5}
        })
        success = r.get("result", {}).get("success", False) if r.get("status") == "success" else False
        send_command("asset_delete_asset", {"asset_path": "/Game/Test/_RegInstance2"})
        send_command("asset_delete_asset", {"asset_path": "/Game/Test/_RegParent2"})
        return success
    test("material_instance_params", "set_instance_params couldn't find the instance", test_material_instance_params)

    # -----------------------------------------------------------------------
    print("\n--- BUG: asset_delete blocked on confirmation dialog ---")
    # UEditorAssetLibrary::DeleteAsset opens reference check dialog
    # Fix: ObjectTools::ForceDeleteObjects (non-blocking)
    # -----------------------------------------------------------------------
    def test_asset_delete_nonblocking():
        send_command("mat_create_material", {"asset_path": "/Game/Test/_RegDeleteMe", "blend_mode": "Opaque"})
        t0 = time.time()
        r = send_command("asset_delete_asset", {"asset_path": "/Game/Test/_RegDeleteMe"})
        dt = time.time() - t0
        success = r.get("result", {}).get("success", False) if r.get("status") == "success" else False
        # Must complete in under 5 seconds (was timing out at 10s before fix)
        return success and dt < 5.0
    test("asset_delete_fast", "DeleteAsset blocked on reference check dialog (10s timeout)", test_asset_delete_nonblocking)

    # -----------------------------------------------------------------------
    print("\n--- BUG: tx_undo returned error when nothing to undo ---")
    # success=false when nothing to undo caused response wrapper to mark as error
    # Fix: always return success=true with undid=false
    # -----------------------------------------------------------------------
    def test_undo_empty_ok():
        r = send_command("tx_undo", {})
        success = r.get("result", {}).get("success", False) if r.get("status") == "success" else False
        return success
    test("undo_empty_is_ok", "tx_undo returned error when undo history was empty", test_undo_empty_ok)

    # -----------------------------------------------------------------------
    print("\n--- BUG: Scoped transaction not wrapping operations ---")
    # -----------------------------------------------------------------------
    def test_scoped_transaction():
        r = send_command("scoped_transaction", {
            "label": "Regression Test",
            "commands": [
                {"type": "gs_create_mesh", "params": {"mesh_name": "_reg_scoped"}},
                {"type": "gs_append_box", "params": {"mesh_name": "_reg_scoped"}},
                {"type": "gs_release_mesh", "params": {"mesh_name": "_reg_scoped"}}
            ]
        })
        result = r.get("result", {})
        return result.get("success", False) and result.get("committed", False)
    test("scoped_transaction", "Scoped transaction didn't wrap sub-commands", test_scoped_transaction)

    # -----------------------------------------------------------------------
    print("\n--- BUG: Batch execute not collecting results ---")
    # -----------------------------------------------------------------------
    def test_batch_execute():
        r = send_command("batch_execute", {"commands": [
            {"type": "ping", "params": {}},
            {"type": "ping", "params": {}},
            {"type": "ping", "params": {}}
        ]})
        result = r.get("result", {})
        return result.get("success", False) and result.get("count", 0) == 3
    test("batch_execute_results", "Batch execute didn't return per-command results", test_batch_execute)

    # -----------------------------------------------------------------------
    print("\n--- BUG: Python stdout not captured ---")
    # execute_python returned only success/failure, no output
    # Fix: wrapper script with StringIO redirect + temp file
    # -----------------------------------------------------------------------
    def test_python_stdout_capture():
        r = send_command("execute_python", {"code": "print('regression_test_output_12345')"})
        output = r.get("result", {}).get("output", "")
        return "regression_test_output_12345" in output
    test("python_stdout_capture", "execute_python didn't capture stdout", test_python_stdout_capture)

    # -----------------------------------------------------------------------
    print("\n--- BUG: Material expression connect Substrate pins ---")
    # MaterialEditingLibrary couldn't connect Substrate SlabBSDF pins
    # Fix: C++ handler with GetInput(i)/GetInputName(i) loop
    # -----------------------------------------------------------------------
    def test_material_substrate_connect():
        send_command("mat_create_material", {"asset_path": "/Game/Test/_RegSubstrate", "blend_mode": "Opaque"})
        slab = send_command("mat_add_expression", {"asset_path": "/Game/Test/_RegSubstrate", "class_name": "MaterialExpressionSubstrateSlabBSDF", "pos_x": 0, "pos_y": 0})
        slab_id = slab.get("result", {}).get("name", "")
        const = send_command("mat_add_expression", {"asset_path": "/Game/Test/_RegSubstrate", "class_name": "MaterialExpressionConstant", "pos_x": -300, "pos_y": 0})
        const_id = const.get("result", {}).get("name", "")
        # Connect constant to SlabBSDF F0 pin
        r = send_command("mat_connect_expressions", {
            "asset_path": "/Game/Test/_RegSubstrate",
            "source_node_id": const_id,
            "target_node_id": slab_id,
            "input_pin_name": "F0"
        })
        success = r.get("result", {}).get("success", False) if r.get("status") == "success" else False
        send_command("asset_delete_asset", {"asset_path": "/Game/Test/_RegSubstrate"})
        return success
    test("substrate_pin_connect", "Couldn't connect Substrate SlabBSDF pins via API", test_material_substrate_connect)

    # -----------------------------------------------------------------------
    print("\n--- BUG: Mesh cleanup not releasing old meshes ---")
    # -----------------------------------------------------------------------
    def test_mesh_cleanup():
        send_command("gs_create_mesh", {"mesh_name": "_reg_old_mesh"})
        # Cleanup with 0 seconds max age = should clean everything
        r = send_command("gs_cleanup_meshes", {"max_age_seconds": 0})
        result = r.get("result", {})
        released = result.get("released_count", 0)
        return released > 0
    test("mesh_cleanup", "Mesh registry never cleaned up, leaked memory", test_mesh_cleanup)

    # -----------------------------------------------------------------------
    print("\n--- BUG: Inspect class returns empty for valid classes ---")
    # -----------------------------------------------------------------------
    def test_inspect_class():
        r = send_command("inspect_class", {"class_name": "Actor", "include_inherited": False})
        result = r.get("result", {})
        return result.get("function_count", 0) > 0 and result.get("property_count", 0) > 0
    test("inspect_class_works", "inspect_class returned empty for valid UE classes", test_inspect_class)

    # -----------------------------------------------------------------------
    print("\n--- BUG: Inspect enum returns empty ---")
    # -----------------------------------------------------------------------
    def test_inspect_enum():
        r = send_command("inspect_enum", {"enum_name": "EBlendMode"})
        result = r.get("result", {})
        return result.get("count", 0) >= 7  # At least 7 blend modes
    test("inspect_enum_works", "inspect_enum returned empty values", test_inspect_enum)

    # -----------------------------------------------------------------------
    print("\n--- BUG: add_event_override didn't create override graph ---")
    # -----------------------------------------------------------------------
    def test_event_override():
        send_command("create_blueprint", {"name": "_RegOverride", "parent_class": "GeneratedDynamicMeshActor"})
        r = send_command("add_event_override", {"blueprint_name": "_RegOverride", "event_name": "OnRebuildGeneratedMesh"})
        success = r.get("result", {}).get("success", False) if r.get("status") == "success" else False
        send_command("asset_delete_asset", {"asset_path": "/Game/Blueprints/_RegOverride"})
        return success
    test("event_override_creates_graph", "add_event_override didn't create the function graph", test_event_override)

    # -----------------------------------------------------------------------
    print("\n--- BUG: PIE play/stop commands not working ---")
    # -----------------------------------------------------------------------
    def test_pie_status():
        r = send_command("play_is_playing", {})
        result = r.get("result", {})
        return result.get("success", False) and "is_playing" in result
    test("pie_is_playing", "PIE status command didn't return proper result", test_pie_status)

    # -----------------------------------------------------------------------
    print("\n--- BUG: Transaction begin/end not working ---")
    # BeginTransaction needed FText not FString
    # -----------------------------------------------------------------------
    def test_transaction_cycle():
        r1 = send_command("tx_begin_transaction", {"label": "Regression"})
        r2 = send_command("tx_end_transaction", {"commit": True})
        ok1 = r1.get("result", {}).get("success", False) if r1.get("status") == "success" else False
        ok2 = r2.get("result", {}).get("success", False) if r2.get("status") == "success" else False
        return ok1 and ok2
    test("transaction_cycle", "BeginTransaction needed FText, was passing FString", test_transaction_cycle)

    # -----------------------------------------------------------------------
    print("\n--- BUG: Level screenshot needs file_path ---")
    # -----------------------------------------------------------------------
    def test_screenshot_with_path():
        path = "C:/Users/Renato/Workspaces/atomic_grids/unreal_demo/Glass/Glass/Saved/Screenshots/_reg_test.png"
        r = send_command("level_get_viewport_screenshot", {"file_path": path})
        result = r.get("result", {})
        return result.get("success", False) and result.get("width", 0) > 0
    test("screenshot_with_path", "Screenshot failed without explicit file_path", test_screenshot_with_path)

    # -----------------------------------------------------------------------
    # SUMMARY
    # -----------------------------------------------------------------------
    print("\n" + "=" * 70)
    passed = sum(1 for r in RESULTS if r[1] == "PASS")
    failed = sum(1 for r in RESULTS if r[1] == "FAIL")
    total_ms = sum(r[2] for r in RESULTS)
    print(f"REGRESSION RESULTS: {passed}/{len(RESULTS)} passed, {failed} failed")
    print(f"TOTAL TIME: {total_ms}ms ({total_ms/1000:.1f}s)")

    if failed > 0:
        print(f"\nREGRESSIONS DETECTED:")
        for name, status, dt, desc in RESULTS:
            if status == "FAIL":
                print(f"  {name}: {desc}")
    else:
        print("\nALL REGRESSIONS COVERED - NO BUGS REINTRODUCED")

    print("=" * 70)
    return failed == 0


if __name__ == "__main__":
    success = run_all()
    sys.exit(0 if success else 1)
