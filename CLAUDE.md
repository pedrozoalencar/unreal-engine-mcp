# Unreal MCP Ultimate 2.0 — Claude Code Instructions

## Quick Reference

This project is the **Ultimate Unreal Engine MCP Server** — a fork of flopperam/unreal-engine-mcp extended with ~150 tools for full editor control via Claude Code. It targets **UE5.7** with Geometry Script, Substrate materials, and Blueprint graph editing.

## Architecture

```
Claude Code → Python MCP Server (stdio, port auto) → TCP Socket (127.0.0.1:55557) → C++ Plugin (UnrealMCP)
                                                    → HTTP (localhost:30010) → Remote Control API
```

The Glass project at `Glass/Glass/` is the primary test project with GeometryScripting + RemoteControl plugins.

## Preferred Tools (UE2 Action-Based)

Always prefer `ue_*` tools over legacy tools. They have standardized responses (`{ok, data, meta}`):

### Core Workflow

| Task | Tool | Example |
|------|------|---------|
| Check connection | `ue_session(action="ping")` | |
| Editor context | `ue_session(action="get_context")` | Returns level, selection, project |
| Run Python in editor | `ue_session(action="run_python", code="...")` | Stdout captured in response |
| Undo/Redo | `ue_session(action="undo")` | |

### Scene & Actors

| Task | Tool | Key Params |
|------|------|------------|
| List actors | `ue_scene(action="list_actors")` | class_filter, limit |
| Spawn Blueprint | `ue_scene(action="spawn_actor", blueprint_path="/Game/...")` | location, rotation, scale |
| Get actor info | `ue_scene(action="get_properties", name="ActorLabel")` | |
| Focus viewport | `ue_capture(action="focus_actor", actor_name="...")` | |
| Multi-view capture | `ue_capture(action="asset_turntable", actor_name="...")` | distance, views |

### Materials (Substrate)

| Task | Tool | Key Params |
|------|------|------------|
| Create glass material | `ue_templates(action="mat_glass", asset_path="/Game/M_Glass")` | tint_color, ior, roughness |
| Create PBR material | `ue_templates(action="mat_pbr", asset_path="/Game/M_PBR")` | base_color, roughness, metallic |
| Inspect material pins | `ue_materials(action="list_pins", asset_path="...", node_id="...")` | Essential for Substrate |
| Connect nodes | `ue_materials(action="connect", ...)` | source_node_id, target_node_id, input_pin_name |

### Geometry Script

| Task | Tool | Key Params |
|------|------|------------|
| Create cup | `ue_templates(action="gs_cup", mesh_name="cup")` | outer_radius, height, wall_thickness |
| Create terrain | `ue_templates(action="gs_terrain", mesh_name="terrain")` | size, subdivisions, noise_magnitude |
| Primitive | `ue_geometry(action="primitive", name="mesh", type="box")` | dimensions, radius, height |
| Boolean | `ue_geometry(action="boolean", target="a", tool="b")` | operation: subtract/union/intersect |
| Spawn mesh | `ue_geometry(action="spawn", name="mesh")` | location |

### Blueprints

| Task | Tool | Key Params |
|------|------|------------|
| Create GS Blueprint | `ue_templates(action="bp_geometry_script", name="BP_Test")` | operations list |
| Override event | `ue_blueprints(action="override_event", blueprint_name="...", event_name="OnRebuildGeneratedMesh")` | |
| Add CallFunction node | `ue_blueprints(action="add_node", blueprint_name="...", node_type="CallFunction", target_function="AppendCylinder")` | function_name for graph scope |
| Set pin value | `ue_blueprints(action="set_pin", blueprint_name="...", node_id="...", pin_name="Radius", pin_value="45")` | |

### Assets & Inspection

| Task | Tool |
|------|------|
| Search assets | `ue_assets(action="search", query="Glass")` |
| Import files | `ue_assets(action="import_files", files=["path.fbx"], destination_path="/Game/Imported")` |
| Inspect class | `ue_inspect(action="class_info", class_name="StaticMeshComponent")` |
| Inspect enum | `ue_inspect(action="enum_values", enum_name="EBlendMode")` |
| Read console log | `ue_console(action="read_log", limit=20)` |

## Important Notes

- **Actor naming**: Actors have internal names like `BP_GS_Cup_C_1` and labels like `BP_GS_Cup`. The `ue_scene(get_properties)` uses labels, legacy tools use internal names.
- **Material Substrate pins**: Use `ue_materials(action="list_pins")` to discover pin names before connecting. Substrate SlabBSDF has 18 pins including "Diffuse Albedo", "F0", "F90", "Roughness", "SSS MFP".
- **Plugin sync**: Plugin source is in `unreal-engine-mcp/UnrealMCP/`, copied to `Glass/Glass/Plugins/UnrealMCP/`. After C++ changes, rebuild via: `"c:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat" GlassEditor Win64 Development -Project="path/Glass.uproject"`
- **Blueprint CallFunction**: The resolver searches ALL BlueprintFunctionLibrary classes. For member functions (like AllocateComputeMesh), pass `target_blueprint="GeneratedDynamicMeshActor"`.
- **Glass project must be open** for the TCP connection (port 55557) to work.

## Performance Benchmarks

| Operation | Typical Latency |
|-----------|----------------|
| ping | ~90ms |
| get_context | ~300ms |
| list_actors | ~200ms |
| get_properties | ~100ms |
| material list_pins | ~175ms |
| inspect_class | ~200ms |
| batch_execute (5 ops) | ~500ms |
