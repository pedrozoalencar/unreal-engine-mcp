"""UE2 Geometry tool -- procedural mesh creation, booleans, deformations, UVs, transforms."""
import time
from typing import Any, Dict, List, Optional

from helpers.ue2.response import UEResponse, cmd, extract_data

TOOL = "ue_geometry"
ACTIONS = [
    "create_mesh", "release_mesh", "list_meshes", "cleanup_meshes",
    "primitive", "boolean", "self_union",
    "extrude_faces", "offset", "shell", "bevel",
    "deform", "set_uvs", "transform",
    "query", "commit", "spawn",
    "load_from_asset", "append_mesh",
]

PRIMITIVE_TYPES = ["box", "sphere", "cylinder", "cone", "torus", "capsule"]
DEFORM_TYPES = ["bend", "twist", "flare", "noise", "smooth"]
QUERY_TYPES = ["info", "vertices", "bounds"]
UV_METHODS = ["auto", "planar", "box"]


def register_geometry_tools(mcp, get_connection):
    """Register the ue_geometry action-based tool."""

    @mcp.tool()
    def ue_geometry(
        action: str,
        # common mesh name
        name: str = "",
        # primitive
        type: str = "box",
        # box dimensions
        dimension_x: float = 100.0,
        dimension_y: float = 100.0,
        dimension_z: float = 100.0,
        steps_x: int = 0,
        steps_y: int = 0,
        steps_z: int = 0,
        # sphere / cylinder / capsule
        radius: float = 50.0,
        # cylinder / cone
        height: float = 100.0,
        radial_steps: int = 16,
        height_steps: int = 0,
        capped: bool = True,
        # cone
        base_radius: float = 50.0,
        top_radius: float = 0.0,
        # torus
        inner_radius: float = 25.0,
        outer_radius: float = 50.0,
        cross_section_steps: int = 16,
        circle_steps: int = 16,
        # capsule
        line_length: float = 75.0,
        hemisphere_steps: int = 5,
        # common location/rotation/scale
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None,
        scale: Optional[List[float]] = None,
        # boolean
        target: str = "",
        tool: str = "",
        operation: str = "subtract",
        output: str = "",
        # extrude_faces
        distance: float = 10.0,
        direction: Optional[List[float]] = None,
        material_id: Optional[int] = None,
        # shell
        thickness: float = 5.0,
        # bevel
        subdivisions: int = 0,
        # deform -- bend/twist
        angle: float = 45.0,
        lower_bounds: float = -50.0,
        upper_bounds: float = 50.0,
        symmetric: bool = False,
        # deform -- flare
        flare_x: float = 0.5,
        flare_y: float = 0.5,
        # deform -- noise
        magnitude: float = 5.0,
        frequency: float = 0.1,
        # deform -- smooth
        iterations: int = 10,
        speed: float = 0.5,
        # set_uvs
        method: str = "auto",
        uv_channel: int = 0,
        # query
        what: str = "info",
        max_vertices: int = 100,
        # commit / load_from_asset
        asset_path: str = "",
        # spawn
        actor_name: str = "",
        # append_mesh
        source: str = "",
    ) -> Dict[str, Any]:
        """Unified geometry/procedural-mesh tool for Unreal Editor (Geometry Script).

        Actions:
          create_mesh       -- Create an empty named dynamic mesh.
                               Params: name.
          release_mesh      -- Release a dynamic mesh from the registry.
                               Params: name.
          list_meshes       -- List all dynamic meshes with vertex/triangle counts.

          primitive         -- Append a primitive shape to a mesh.
                               Params: name, type="box" (box/sphere/cylinder/cone/torus/capsule),
                               plus shape-specific params (dimensions, radius, height, steps, etc.),
                               location, rotation.
          boolean           -- Boolean operation between two meshes.
                               Params: target, tool, operation="subtract", output="".
          self_union        -- Resolve self-intersections in a mesh.
                               Params: name.

          extrude_faces     -- Extrude faces linearly.
                               Params: name, distance, direction=None, material_id=None.
          offset            -- Offset mesh surface (inflate/deflate).
                               Params: name, distance.
          shell             -- Create a hollow shell from a solid mesh.
                               Params: name, thickness.
          bevel             -- Bevel polygroup edges.
                               Params: name, distance, subdivisions=0.

          deform            -- Apply a deformation.
                               Params: name, type="bend" (bend/twist/flare/noise/smooth),
                               plus deformation-specific params (angle, bounds, magnitude, etc.).
          set_uvs           -- Generate UVs.
                               Params: name, method="auto" (auto/planar/box), uv_channel=0.
          transform         -- Apply full transform to mesh vertices.
                               Params: name, location=None, rotation=None, scale=None.

          query             -- Query mesh data.
                               Params: name, what="info" (info/vertices/bounds),
                               max_vertices=100 (for vertices).
          commit            -- Copy dynamic mesh to a StaticMesh asset.
                               Params: name, asset_path.
          spawn             -- Spawn dynamic mesh as an actor in the level.
                               Params: name, location=[0,0,0], actor_name="".
          load_from_asset   -- Load a StaticMesh into a dynamic mesh for editing.
                               Params: name, asset_path.
          append_mesh       -- Append (combine) one mesh into another.
                               Params: target, source, location=None.
        """
        t0 = time.time()
        conn = get_connection()
        if conn is None:
            return UEResponse.not_connected(TOOL)

        try:
            # ------------------------------------------------------------------
            if action == "create_mesh":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for create_mesh", TOOL)
                result = cmd(conn, "gs_create_mesh", {"mesh_name": name})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "release_mesh":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for release_mesh", TOOL)
                result = cmd(conn, "gs_release_mesh", {"mesh_name": name})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "list_meshes":
                result = cmd(conn, "gs_list_meshes", {})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            elif action == "cleanup_meshes":
                max_age = distance  # reuse distance param as max_age_seconds
                if max_age <= 0:
                    max_age = 300.0
                result = cmd(conn, "gs_cleanup_meshes", {"max_age_seconds": max_age})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "primitive":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for primitive", TOOL)
                ptype = type.lower()
                if ptype not in PRIMITIVE_TYPES:
                    return UEResponse.error("E_INVALID_ARGUMENT",
                        f"Unknown primitive type '{type}'. Valid: {', '.join(PRIMITIVE_TYPES)}", TOOL)

                if ptype == "box":
                    params = {
                        "mesh_name": name,
                        "dimension_x": dimension_x,
                        "dimension_y": dimension_y,
                        "dimension_z": dimension_z,
                        "steps_x": steps_x,
                        "steps_y": steps_y,
                        "steps_z": steps_z,
                    }
                    if location: params["location"] = location
                    if rotation: params["rotation"] = rotation
                    result = cmd(conn, "gs_append_box", params)

                elif ptype == "sphere":
                    params = {
                        "mesh_name": name,
                        "radius": radius,
                        "steps_x": steps_x if steps_x else 16,
                        "steps_y": steps_y if steps_y else 16,
                    }
                    if location: params["location"] = location
                    if rotation: params["rotation"] = rotation
                    result = cmd(conn, "gs_append_sphere", params)

                elif ptype == "cylinder":
                    params = {
                        "mesh_name": name,
                        "radius": radius,
                        "height": height,
                        "radial_steps": radial_steps,
                        "height_steps": height_steps,
                        "capped": capped,
                    }
                    if location: params["location"] = location
                    if rotation: params["rotation"] = rotation
                    result = cmd(conn, "gs_append_cylinder", params)

                elif ptype == "cone":
                    params = {
                        "mesh_name": name,
                        "base_radius": base_radius,
                        "top_radius": top_radius,
                        "height": height,
                        "radial_steps": radial_steps,
                        "height_steps": height_steps,
                        "capped": capped,
                    }
                    if location: params["location"] = location
                    if rotation: params["rotation"] = rotation
                    result = cmd(conn, "gs_append_cone", params)

                elif ptype == "torus":
                    params = {
                        "mesh_name": name,
                        "inner_radius": inner_radius,
                        "outer_radius": outer_radius,
                        "cross_section_steps": cross_section_steps,
                        "circle_steps": circle_steps,
                    }
                    if location: params["location"] = location
                    if rotation: params["rotation"] = rotation
                    result = cmd(conn, "gs_append_torus", params)

                elif ptype == "capsule":
                    params = {
                        "mesh_name": name,
                        "radius": radius,
                        "line_length": line_length,
                        "hemisphere_steps": hemisphere_steps,
                        "circle_steps": circle_steps,
                    }
                    if location: params["location"] = location
                    if rotation: params["rotation"] = rotation
                    result = cmd(conn, "gs_append_capsule", params)

                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "boolean":
                if not target:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'target' is required for boolean", TOOL)
                if not tool:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'tool' is required for boolean", TOOL)
                params = {
                    "target_mesh": target,
                    "tool_mesh": tool,
                    "operation": operation,
                }
                if output:
                    params["output_mesh"] = output
                result = cmd(conn, "gs_boolean", params)
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "self_union":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for self_union", TOOL)
                result = cmd(conn, "gs_self_union", {"mesh_name": name})
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "extrude_faces":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for extrude_faces", TOOL)
                params = {"mesh_name": name, "distance": distance}
                if direction:
                    params["direction"] = direction
                if material_id is not None:
                    params["material_id"] = material_id
                result = cmd(conn, "gs_linear_extrude_faces", params)
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "offset":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for offset", TOOL)
                result = cmd(conn, "gs_offset", {
                    "mesh_name": name,
                    "distance": distance,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "shell":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for shell", TOOL)
                result = cmd(conn, "gs_shell", {
                    "mesh_name": name,
                    "thickness": thickness,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "bevel":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for bevel", TOOL)
                result = cmd(conn, "gs_bevel", {
                    "mesh_name": name,
                    "distance": distance,
                    "subdivisions": subdivisions,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "deform":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for deform", TOOL)
                dtype = type.lower()
                if dtype not in DEFORM_TYPES:
                    return UEResponse.error("E_INVALID_ARGUMENT",
                        f"Unknown deform type '{type}'. Valid: {', '.join(DEFORM_TYPES)}", TOOL)

                if dtype == "bend":
                    result = cmd(conn, "gs_bend", {
                        "mesh_name": name,
                        "angle": angle,
                        "lower_bounds": lower_bounds,
                        "upper_bounds": upper_bounds,
                        "symmetric": symmetric,
                    })
                elif dtype == "twist":
                    result = cmd(conn, "gs_twist", {
                        "mesh_name": name,
                        "angle": angle,
                        "lower_bounds": lower_bounds,
                        "upper_bounds": upper_bounds,
                        "symmetric": symmetric,
                    })
                elif dtype == "flare":
                    result = cmd(conn, "gs_flare", {
                        "mesh_name": name,
                        "flare_x": flare_x,
                        "flare_y": flare_y,
                        "lower_bounds": lower_bounds,
                        "upper_bounds": upper_bounds,
                        "symmetric": symmetric,
                    })
                elif dtype == "noise":
                    result = cmd(conn, "gs_perlin_noise", {
                        "mesh_name": name,
                        "magnitude": magnitude,
                        "frequency": frequency,
                    })
                elif dtype == "smooth":
                    result = cmd(conn, "gs_smooth", {
                        "mesh_name": name,
                        "iterations": iterations,
                        "speed": speed,
                    })

                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "set_uvs":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for set_uvs", TOOL)
                uv_method = method.lower()
                if uv_method not in UV_METHODS:
                    return UEResponse.error("E_INVALID_ARGUMENT",
                        f"Unknown UV method '{method}'. Valid: {', '.join(UV_METHODS)}", TOOL)

                params = {"mesh_name": name, "uv_channel": uv_channel}

                if uv_method == "auto":
                    result = cmd(conn, "gs_auto_uvs", params)
                elif uv_method == "planar":
                    if location: params["location"] = location
                    if rotation: params["rotation"] = rotation
                    result = cmd(conn, "gs_planar_projection", params)
                elif uv_method == "box":
                    if location: params["location"] = location
                    if rotation: params["rotation"] = rotation
                    result = cmd(conn, "gs_box_projection", params)

                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "transform":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for transform", TOOL)
                params = {"mesh_name": name}
                if location: params["location"] = location
                if rotation: params["rotation"] = rotation
                if scale: params["scale"] = scale
                result = cmd(conn, "gs_transform_mesh", params)
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "query":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for query", TOOL)
                qtype = what.lower()
                if qtype not in QUERY_TYPES:
                    return UEResponse.error("E_INVALID_ARGUMENT",
                        f"Unknown query type '{what}'. Valid: {', '.join(QUERY_TYPES)}", TOOL)

                if qtype == "info":
                    result = cmd(conn, "gs_query_mesh_info", {"mesh_name": name})
                elif qtype == "vertices":
                    result = cmd(conn, "gs_query_vertices", {
                        "mesh_name": name,
                        "max_vertices": max_vertices,
                    })
                elif qtype == "bounds":
                    result = cmd(conn, "gs_get_bounds", {"mesh_name": name})

                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "commit":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for commit", TOOL)
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for commit", TOOL)
                result = cmd(conn, "gs_copy_to_static_mesh", {
                    "mesh_name": name,
                    "asset_path": asset_path,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "spawn":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for spawn", TOOL)
                params = {"mesh_name": name}
                if location:
                    params["location"] = location
                else:
                    params["location"] = [0, 0, 0]
                if actor_name:
                    params["actor_name"] = actor_name
                result = cmd(conn, "gs_spawn_dynamic_mesh_actor", params)
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "load_from_asset":
                if not name:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'name' is required for load_from_asset", TOOL)
                if not asset_path:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'asset_path' is required for load_from_asset", TOOL)
                result = cmd(conn, "gs_copy_from_static_mesh", {
                    "mesh_name": name,
                    "asset_path": asset_path,
                })
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            elif action == "append_mesh":
                if not target:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'target' is required for append_mesh", TOOL)
                if not source:
                    return UEResponse.error("E_INVALID_ARGUMENT", "Parameter 'source' is required for append_mesh", TOOL)
                params = {
                    "target_mesh": target,
                    "source_mesh": source,
                }
                if location:
                    params["location"] = location
                result = cmd(conn, "gs_append_mesh", params)
                data = extract_data(result)
                return UEResponse.ok(data, tool=TOOL, duration_ms=(time.time() - t0) * 1000)

            # ------------------------------------------------------------------
            else:
                return UEResponse.invalid_action(action, ACTIONS, TOOL)

        except ConnectionError:
            return UEResponse.not_connected(TOOL)
        except Exception as e:
            return UEResponse.error("E_INTERNAL", str(e), TOOL, retryable=False)
