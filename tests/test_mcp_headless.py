"""Headless MCP test script — runs inside UE5 editor via -ExecutePythonScript.
Tests the TCP MCP server by connecting as a client and sending commands.
"""
import socket
import json
import time
import sys

HOST = "127.0.0.1"
PORT = 55557
TIMEOUT = 10
RESULTS = []


def send_command(sock, cmd_type, params=None):
    """Send a command and receive response."""
    msg = json.dumps({"type": cmd_type, "params": params or {}})
    sock.sendall(msg.encode('utf-8'))

    chunks = []
    sock.settimeout(TIMEOUT)
    while True:
        try:
            chunk = sock.recv(65536)
            if not chunk:
                break
            chunks.append(chunk)
            data = b''.join(chunks)
            try:
                return json.loads(data.decode('utf-8'))
            except json.JSONDecodeError:
                continue
        except socket.timeout:
            break

    data = b''.join(chunks)
    return json.loads(data.decode('utf-8')) if data else None


def test(name, cmd_type, params=None, check=None):
    """Run a single test."""
    t0 = time.time()
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(TIMEOUT)
        sock.connect((HOST, PORT))
        result = send_command(sock, cmd_type, params)
        sock.close()

        dt = round((time.time() - t0) * 1000)
        ok = True
        detail = ""

        if result is None:
            ok = False
            detail = "No response"
        elif result.get("status") == "error":
            # Check if this was expected
            if check and check == "expect_error":
                ok = True
                detail = f"Expected error: {result.get('error', '')[:50]}"
            else:
                ok = False
                detail = result.get("error", "Unknown error")[:80]
        elif check and callable(check):
            ok = check(result)
            detail = "Custom check " + ("passed" if ok else "FAILED")
        else:
            detail = "OK"

        status = "PASS" if ok else "FAIL"
        RESULTS.append((name, status, dt, detail))
        print(f"  [{status}] {name} ({dt}ms) {detail}")
        return result

    except Exception as e:
        dt = round((time.time() - t0) * 1000)
        RESULTS.append((name, "FAIL", dt, str(e)[:80]))
        print(f"  [FAIL] {name} ({dt}ms) {e}")
        return None


def main():
    print("=" * 60)
    print("MCP HEADLESS TEST SUITE")
    print("=" * 60)

    # --- Session ---
    print("\n--- Session ---")
    test("ping", "ping")

    # --- Geometry Script ---
    print("\n--- Geometry Script ---")
    test("gs_create_mesh", "gs_create_mesh", {"mesh_name": "test_headless"})
    test("gs_append_box", "gs_append_box", {"mesh_name": "test_headless", "dimension_x": 100, "dimension_y": 100, "dimension_z": 100})
    test("gs_query_info", "gs_query_mesh_info", {"mesh_name": "test_headless"},
         lambda r: r.get("result", {}).get("mesh_info", {}).get("vertex_count", 0) == 8)
    test("gs_append_sphere", "gs_append_sphere", {"mesh_name": "test_sphere", "radius": 50})
    test("gs_boolean", "gs_boolean", {"target_mesh": "test_headless", "tool_mesh": "test_sphere", "operation": "subtract"})
    test("gs_cleanup", "gs_cleanup_meshes", {"max_age_seconds": 0})
    test("gs_release", "gs_release_mesh", {"mesh_name": "test_headless"})

    # --- Materials ---
    print("\n--- Materials ---")
    test("mat_create", "mat_create_material", {"asset_path": "/Game/Test/M_HeadlessTest", "blend_mode": "Opaque"})
    test("mat_add_slab", "mat_add_expression", {"asset_path": "/Game/Test/M_HeadlessTest", "class_name": "MaterialExpressionSubstrateSlabBSDF", "pos_x": 0, "pos_y": 0})
    test("mat_list_expr", "mat_list_expressions", {"asset_path": "/Game/Test/M_HeadlessTest"})
    test("mat_compile", "mat_compile_material", {"asset_path": "/Game/Test/M_HeadlessTest"})

    # --- Material Instance ---
    print("\n--- Material Instance ---")
    test("mat_instance_create", "mat_create_material_instance", {"parent_path": "/Game/Test/M_HeadlessTest", "instance_path": "/Game/Test/MI_HeadlessTest"})
    test("mat_instance_params", "mat_set_instance_params", {"instance_path": "/Game/Test/MI_HeadlessTest", "scalars": {"Roughness": 0.5}})

    # --- Blueprint ---
    print("\n--- Blueprint ---")
    test("bp_create", "create_blueprint", {"name": "BP_HeadlessTest", "parent_class": "Actor"})
    test("bp_compile", "compile_blueprint", {"blueprint_name": "BP_HeadlessTest"})

    # --- Level ---
    print("\n--- Level ---")
    test("level_list", "get_actors_in_level", {})
    test("level_screenshot", "level_get_viewport_screenshot", {"file_path": "C:/Users/Renato/Workspaces/atomic_grids/unreal_demo/Glass/Glass/Saved/Screenshots/headless_test.png"})

    # --- Introspection ---
    print("\n--- Introspection ---")
    test("inspect_class", "inspect_class", {"class_name": "Actor"},
         lambda r: r.get("result", {}).get("function_count", 0) > 0)
    test("inspect_enum", "inspect_enum", {"enum_name": "EBlendMode"},
         lambda r: r.get("result", {}).get("count", 0) > 0)

    # --- Transaction ---
    print("\n--- Transaction ---")
    test("tx_begin", "tx_begin_transaction", {"label": "Headless Test"})
    test("tx_end", "tx_end_transaction", {"commit": True})
    test("tx_undo", "tx_undo", {})

    # --- Batch ---
    print("\n--- Batch ---")
    test("batch", "batch_execute", {"commands": [
        {"type": "ping", "params": {}},
        {"type": "gs_create_mesh", "params": {"mesh_name": "batch_test"}},
        {"type": "gs_release_mesh", "params": {"mesh_name": "batch_test"}}
    ]})

    # --- Scoped Transaction ---
    print("\n--- Scoped Transaction ---")
    test("scoped_tx", "scoped_transaction", {
        "label": "Headless Scoped",
        "commands": [
            {"type": "gs_create_mesh", "params": {"mesh_name": "scoped_test"}},
            {"type": "gs_append_box", "params": {"mesh_name": "scoped_test"}}
        ]
    })

    # --- Python Exec ---
    print("\n--- Python Exec ---")
    test("python_exec", "execute_python", {"code": "print('headless_ok')"})

    # --- Playtest ---
    print("\n--- Playtest ---")
    test("play_is_playing", "play_is_playing", {})

    # --- Asset ---
    print("\n--- Asset ---")
    test("asset_exists", "asset_does_asset_exist", {"asset_path": "/Game/Test/M_HeadlessTest"})
    test("asset_delete_mat", "asset_delete_asset", {"asset_path": "/Game/Test/M_HeadlessTest"})
    test("asset_delete_mi", "asset_delete_asset", {"asset_path": "/Game/Test/MI_HeadlessTest"})
    test("asset_delete_bp", "asset_delete_asset", {"asset_path": "/Game/Blueprints/BP_HeadlessTest"})

    # --- Summary ---
    print("\n" + "=" * 60)
    passed = sum(1 for r in RESULTS if r[1] == "PASS")
    failed = sum(1 for r in RESULTS if r[1] == "FAIL")
    total_ms = sum(r[2] for r in RESULTS)
    print(f"RESULTS: {passed}/{len(RESULTS)} passed, {failed} failed")
    print(f"TOTAL TIME: {total_ms}ms ({total_ms/1000:.1f}s)")
    print(f"AVG LATENCY: {total_ms//len(RESULTS)}ms per test")

    if failed > 0:
        print(f"\nFAILED TESTS:")
        for name, status, dt, detail in RESULTS:
            if status == "FAIL":
                print(f"  {name}: {detail}")

    print("=" * 60)


if __name__ == "__main__":
    main()
