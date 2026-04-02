#include "Commands/GeometryScript/UnrealMCPGeometryScriptCommands.h"

#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshDeformFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"
#include "GeometryScript/MeshMaterialFunctions.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshSelectionFunctions.h"
#include "GeometryScript/MeshSpatialFunctions.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Components/DynamicMeshComponent.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"

FUnrealMCPGeometryScriptCommands::FUnrealMCPGeometryScriptCommands()
{
}

FUnrealMCPGeometryScriptCommands::~FUnrealMCPGeometryScriptCommands()
{
	for (auto& Pair : MeshRegistry)
	{
		if (Pair.Value)
		{
			Pair.Value->RemoveFromRoot();
		}
	}
	MeshRegistry.Empty();
	MeshLastAccessTime.Empty();
}

// ============================================================================
// Helpers
// ============================================================================

void FUnrealMCPGeometryScriptCommands::UpdateMeshAccess(const FString& Name)
{
	MeshLastAccessTime.Add(Name, FPlatformTime::Seconds());
}

UDynamicMesh* FUnrealMCPGeometryScriptCommands::GetOrCreateMesh(const FString& Name)
{
	UpdateMeshAccess(Name);
	TObjectPtr<UDynamicMesh>* Found = MeshRegistry.Find(Name);
	if (Found && *Found)
	{
		return Found->Get();
	}
	UDynamicMesh* NewMesh = NewObject<UDynamicMesh>();
	NewMesh->AddToRoot();
	MeshRegistry.Add(Name, NewMesh);
	return NewMesh;
}

UDynamicMesh* FUnrealMCPGeometryScriptCommands::FindMesh(const FString& Name)
{
	TObjectPtr<UDynamicMesh>* Found = MeshRegistry.Find(Name);
	if (Found && *Found)
	{
		UpdateMeshAccess(Name);
		return Found->Get();
	}
	return nullptr;
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::MakeErrorResponse(const FString& Message)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), false);
	Result->SetStringField(TEXT("error"), Message);
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::MakeSuccessResponse(const FString& MeshName, UDynamicMesh* Mesh)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("mesh_name"), MeshName);
	if (Mesh)
	{
		Result->SetObjectField(TEXT("mesh_info"), MakeMeshInfoJson(Mesh));
	}
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::MakeMeshInfoJson(UDynamicMesh* Mesh)
{
	TSharedPtr<FJsonObject> Info = MakeShareable(new FJsonObject);
	if (!Mesh) return Info;

	int32 VertexCount = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh);
	int32 TriangleCount = UGeometryScriptLibrary_MeshQueryFunctions::GetNumTriangleIDs(Mesh);

	Info->SetNumberField(TEXT("vertex_count"), VertexCount);
	Info->SetNumberField(TEXT("triangle_count"), TriangleCount);

	FBox BoundingBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
	TSharedPtr<FJsonObject> BoundsJson = MakeShareable(new FJsonObject);
	BoundsJson->SetNumberField(TEXT("min_x"), BoundingBox.Min.X);
	BoundsJson->SetNumberField(TEXT("min_y"), BoundingBox.Min.Y);
	BoundsJson->SetNumberField(TEXT("min_z"), BoundingBox.Min.Z);
	BoundsJson->SetNumberField(TEXT("max_x"), BoundingBox.Max.X);
	BoundsJson->SetNumberField(TEXT("max_y"), BoundingBox.Max.Y);
	BoundsJson->SetNumberField(TEXT("max_z"), BoundingBox.Max.Z);
	Info->SetObjectField(TEXT("bounds"), BoundsJson);

	return Info;
}

FTransform FUnrealMCPGeometryScriptCommands::ParseTransform(const TSharedPtr<FJsonObject>& Params)
{
	FVector Location(0, 0, 0);
	FRotator Rotation(0, 0, 0);
	FVector Scale(1, 1, 1);

	if (Params->HasField(TEXT("location")))
	{
		const TArray<TSharedPtr<FJsonValue>>& Loc = Params->GetArrayField(TEXT("location"));
		if (Loc.Num() >= 3)
		{
			Location = FVector(Loc[0]->AsNumber(), Loc[1]->AsNumber(), Loc[2]->AsNumber());
		}
	}
	if (Params->HasField(TEXT("rotation")))
	{
		const TArray<TSharedPtr<FJsonValue>>& Rot = Params->GetArrayField(TEXT("rotation"));
		if (Rot.Num() >= 3)
		{
			Rotation = FRotator(Rot[0]->AsNumber(), Rot[1]->AsNumber(), Rot[2]->AsNumber());
		}
	}
	if (Params->HasField(TEXT("scale")))
	{
		const TArray<TSharedPtr<FJsonValue>>& Scl = Params->GetArrayField(TEXT("scale"));
		if (Scl.Num() >= 3)
		{
			Scale = FVector(Scl[0]->AsNumber(), Scl[1]->AsNumber(), Scl[2]->AsNumber());
		}
	}

	return FTransform(Rotation, Location, Scale);
}

// ============================================================================
// Command Router
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("gs_create_mesh")) return HandleCreateMesh(Params);
	if (CommandType == TEXT("gs_release_mesh")) return HandleReleaseMesh(Params);
	if (CommandType == TEXT("gs_list_meshes")) return HandleListMeshes(Params);

	if (CommandType == TEXT("gs_append_box")) return HandleAppendBox(Params);
	if (CommandType == TEXT("gs_append_sphere")) return HandleAppendSphere(Params);
	if (CommandType == TEXT("gs_append_cylinder")) return HandleAppendCylinder(Params);
	if (CommandType == TEXT("gs_append_cone")) return HandleAppendCone(Params);
	if (CommandType == TEXT("gs_append_torus")) return HandleAppendTorus(Params);
	if (CommandType == TEXT("gs_append_capsule")) return HandleAppendCapsule(Params);
	if (CommandType == TEXT("gs_append_revolve")) return HandleAppendRevolve(Params);
	if (CommandType == TEXT("gs_append_sweep")) return HandleAppendSweep(Params);

	if (CommandType == TEXT("gs_boolean")) return HandleBoolean(Params);
	if (CommandType == TEXT("gs_self_union")) return HandleSelfUnion(Params);

	if (CommandType == TEXT("gs_offset")) return HandleOffset(Params);
	if (CommandType == TEXT("gs_shell")) return HandleShell(Params);
	if (CommandType == TEXT("gs_linear_extrude_faces")) return HandleLinearExtrudeFaces(Params);
	if (CommandType == TEXT("gs_offset_faces")) return HandleOffsetFaces(Params);
	if (CommandType == TEXT("gs_inset_outset_faces")) return HandleInsetOutsetFaces(Params);
	if (CommandType == TEXT("gs_bevel")) return HandleBevel(Params);

	if (CommandType == TEXT("gs_bend")) return HandleBend(Params);
	if (CommandType == TEXT("gs_twist")) return HandleTwist(Params);
	if (CommandType == TEXT("gs_flare")) return HandleFlare(Params);
	if (CommandType == TEXT("gs_perlin_noise")) return HandlePerlinNoise(Params);
	if (CommandType == TEXT("gs_smooth")) return HandleSmooth(Params);

	if (CommandType == TEXT("gs_query_mesh_info")) return HandleQueryMeshInfo(Params);
	if (CommandType == TEXT("gs_query_vertices")) return HandleQueryVertices(Params);
	if (CommandType == TEXT("gs_get_bounds")) return HandleGetBounds(Params);
	if (CommandType == TEXT("gs_raycast")) return HandleRaycast(Params);
	if (CommandType == TEXT("gs_is_point_inside")) return HandleIsPointInside(Params);

	// Selection
	if (CommandType == TEXT("gs_select_all")) return HandleSelectAll(Params);
	if (CommandType == TEXT("gs_select_by_material")) return HandleSelectByMaterial(Params);
	if (CommandType == TEXT("gs_select_in_box")) return HandleSelectInBox(Params);
	if (CommandType == TEXT("gs_select_by_normal")) return HandleSelectByNormal(Params);
	if (CommandType == TEXT("gs_select_invert")) return HandleSelectInvert(Params);

	if (CommandType == TEXT("gs_auto_uvs")) return HandleAutoGenerateUVs(Params);
	if (CommandType == TEXT("gs_planar_projection")) return HandlePlanarProjection(Params);
	if (CommandType == TEXT("gs_box_projection")) return HandleBoxProjection(Params);

	if (CommandType == TEXT("gs_set_material_id")) return HandleSetMaterialID(Params);
	if (CommandType == TEXT("gs_remap_material_ids")) return HandleRemapMaterialIDs(Params);
	if (CommandType == TEXT("gs_get_material_ids")) return HandleGetMaterialIDs(Params);

	if (CommandType == TEXT("gs_transform_mesh")) return HandleTransformMesh(Params);
	if (CommandType == TEXT("gs_translate")) return HandleTranslateMesh(Params);
	if (CommandType == TEXT("gs_scale")) return HandleScaleMesh(Params);

	if (CommandType == TEXT("gs_copy_to_static_mesh")) return HandleCopyToStaticMesh(Params);
	if (CommandType == TEXT("gs_spawn_dynamic_mesh_actor")) return HandleSpawnDynamicMeshActor(Params);
	if (CommandType == TEXT("gs_copy_from_static_mesh")) return HandleCopyFromStaticMesh(Params);

	if (CommandType == TEXT("gs_append_mesh")) return HandleAppendMesh(Params);

	if (CommandType == TEXT("gs_cleanup_meshes")) return HandleCleanupMeshes(Params);

	return MakeErrorResponse(FString::Printf(TEXT("Unknown geometry script command: %s"), *CommandType));
}

// ============================================================================
// Mesh Lifecycle
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleCreateMesh(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	if (MeshName.IsEmpty()) return MakeErrorResponse(TEXT("mesh_name is required"));
	UDynamicMesh* Mesh = GetOrCreateMesh(MeshName);
	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleReleaseMesh(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	TObjectPtr<UDynamicMesh>* Found = MeshRegistry.Find(MeshName);
	if (Found && *Found)
	{
		(*Found)->RemoveFromRoot();
		MeshRegistry.Remove(MeshName);
	}
	MeshLastAccessTime.Remove(MeshName);
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Released mesh '%s'"), *MeshName));
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleListMeshes(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	TArray<TSharedPtr<FJsonValue>> MeshArray;
	for (auto& Pair : MeshRegistry)
	{
		TSharedPtr<FJsonObject> Entry = MakeShareable(new FJsonObject);
		Entry->SetStringField(TEXT("name"), Pair.Key);
		Entry->SetObjectField(TEXT("info"), MakeMeshInfoJson(Pair.Value.Get()));
		MeshArray.Add(MakeShareable(new FJsonValueObject(Entry)));
	}
	Result->SetArrayField(TEXT("meshes"), MeshArray);
	return Result;
}

// ============================================================================
// Primitives - UE5.7 correct signatures
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleAppendBox(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	if (MeshName.IsEmpty()) return MakeErrorResponse(TEXT("mesh_name is required"));

	UDynamicMesh* Mesh = GetOrCreateMesh(MeshName);
	FGeometryScriptPrimitiveOptions PrimOptions;
	FTransform Transform = ParseTransform(Params);

	float DimX = Params->HasField(TEXT("dimension_x")) ? Params->GetNumberField(TEXT("dimension_x")) : 100.0f;
	float DimY = Params->HasField(TEXT("dimension_y")) ? Params->GetNumberField(TEXT("dimension_y")) : 100.0f;
	float DimZ = Params->HasField(TEXT("dimension_z")) ? Params->GetNumberField(TEXT("dimension_z")) : 100.0f;
	int32 StepsX = Params->HasField(TEXT("steps_x")) ? (int32)Params->GetNumberField(TEXT("steps_x")) : 0;
	int32 StepsY = Params->HasField(TEXT("steps_y")) ? (int32)Params->GetNumberField(TEXT("steps_y")) : 0;
	int32 StepsZ = Params->HasField(TEXT("steps_z")) ? (int32)Params->GetNumberField(TEXT("steps_z")) : 0;

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
		Mesh, PrimOptions, Transform,
		DimX, DimY, DimZ, StepsX, StepsY, StepsZ,
		EGeometryScriptPrimitiveOriginMode::Center, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleAppendSphere(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	if (MeshName.IsEmpty()) return MakeErrorResponse(TEXT("mesh_name is required"));

	UDynamicMesh* Mesh = GetOrCreateMesh(MeshName);
	FGeometryScriptPrimitiveOptions PrimOptions;
	FTransform Transform = ParseTransform(Params);

	float Radius = Params->HasField(TEXT("radius")) ? Params->GetNumberField(TEXT("radius")) : 50.0f;
	int32 StepsX = Params->HasField(TEXT("steps_x")) ? (int32)Params->GetNumberField(TEXT("steps_x")) : 6;
	int32 StepsY = Params->HasField(TEXT("steps_y")) ? (int32)Params->GetNumberField(TEXT("steps_y")) : 6;
	int32 StepsZ = Params->HasField(TEXT("steps_z")) ? (int32)Params->GetNumberField(TEXT("steps_z")) : 6;

	// UE5.7: AppendSphereBox(Mesh, Options, Transform, Radius, StepsX, StepsY, StepsZ, Origin, Debug)
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereBox(
		Mesh, PrimOptions, Transform,
		Radius, StepsX, StepsY, StepsZ,
		EGeometryScriptPrimitiveOriginMode::Center, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleAppendCylinder(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	if (MeshName.IsEmpty()) return MakeErrorResponse(TEXT("mesh_name is required"));

	UDynamicMesh* Mesh = GetOrCreateMesh(MeshName);
	FGeometryScriptPrimitiveOptions PrimOptions;
	FTransform Transform = ParseTransform(Params);

	float Radius = Params->HasField(TEXT("radius")) ? Params->GetNumberField(TEXT("radius")) : 50.0f;
	float Height = Params->HasField(TEXT("height")) ? Params->GetNumberField(TEXT("height")) : 100.0f;
	int32 RadialSteps = Params->HasField(TEXT("radial_steps")) ? (int32)Params->GetNumberField(TEXT("radial_steps")) : 12;
	int32 HeightSteps = Params->HasField(TEXT("height_steps")) ? (int32)Params->GetNumberField(TEXT("height_steps")) : 0;
	bool bCapped = Params->HasField(TEXT("capped")) ? Params->GetBoolField(TEXT("capped")) : true;

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
		Mesh, PrimOptions, Transform,
		Radius, Height, RadialSteps, HeightSteps, bCapped,
		EGeometryScriptPrimitiveOriginMode::Center, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleAppendCone(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	if (MeshName.IsEmpty()) return MakeErrorResponse(TEXT("mesh_name is required"));

	UDynamicMesh* Mesh = GetOrCreateMesh(MeshName);
	FGeometryScriptPrimitiveOptions PrimOptions;
	FTransform Transform = ParseTransform(Params);

	float BaseRadius = Params->HasField(TEXT("base_radius")) ? Params->GetNumberField(TEXT("base_radius")) : 50.0f;
	float TopRadius = Params->HasField(TEXT("top_radius")) ? Params->GetNumberField(TEXT("top_radius")) : 5.0f;
	float Height = Params->HasField(TEXT("height")) ? Params->GetNumberField(TEXT("height")) : 100.0f;
	int32 RadialSteps = Params->HasField(TEXT("radial_steps")) ? (int32)Params->GetNumberField(TEXT("radial_steps")) : 12;
	int32 HeightSteps = Params->HasField(TEXT("height_steps")) ? (int32)Params->GetNumberField(TEXT("height_steps")) : 4;
	bool bCapped = Params->HasField(TEXT("capped")) ? Params->GetBoolField(TEXT("capped")) : true;

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCone(
		Mesh, PrimOptions, Transform,
		BaseRadius, TopRadius, Height, RadialSteps, HeightSteps, bCapped,
		EGeometryScriptPrimitiveOriginMode::Center, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleAppendTorus(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	if (MeshName.IsEmpty()) return MakeErrorResponse(TEXT("mesh_name is required"));

	UDynamicMesh* Mesh = GetOrCreateMesh(MeshName);
	FGeometryScriptPrimitiveOptions PrimOptions;
	FTransform Transform = ParseTransform(Params);

	// UE5.7: AppendTorus(Mesh, Options, Transform, RevolveOptions, MajorRadius, MinorRadius, MajorSteps, MinorSteps, Origin, Debug)
	float MajorRadius = Params->HasField(TEXT("outer_radius")) ? Params->GetNumberField(TEXT("outer_radius")) : 50.0f;
	float MinorRadius = Params->HasField(TEXT("inner_radius")) ? Params->GetNumberField(TEXT("inner_radius")) : 25.0f;
	int32 MajorSteps = Params->HasField(TEXT("circle_steps")) ? (int32)Params->GetNumberField(TEXT("circle_steps")) : 16;
	int32 MinorSteps = Params->HasField(TEXT("cross_section_steps")) ? (int32)Params->GetNumberField(TEXT("cross_section_steps")) : 8;

	FGeometryScriptRevolveOptions RevolveOptions;

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTorus(
		Mesh, PrimOptions, Transform,
		RevolveOptions,
		MajorRadius, MinorRadius,
		MajorSteps, MinorSteps,
		EGeometryScriptPrimitiveOriginMode::Center, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleAppendCapsule(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	if (MeshName.IsEmpty()) return MakeErrorResponse(TEXT("mesh_name is required"));

	UDynamicMesh* Mesh = GetOrCreateMesh(MeshName);
	FGeometryScriptPrimitiveOptions PrimOptions;
	FTransform Transform = ParseTransform(Params);

	float Radius = Params->HasField(TEXT("radius")) ? Params->GetNumberField(TEXT("radius")) : 30.0f;
	float LineLength = Params->HasField(TEXT("line_length")) ? Params->GetNumberField(TEXT("line_length")) : 75.0f;
	int32 HemisphereSteps = Params->HasField(TEXT("hemisphere_steps")) ? (int32)Params->GetNumberField(TEXT("hemisphere_steps")) : 5;
	int32 CircleSteps = Params->HasField(TEXT("circle_steps")) ? (int32)Params->GetNumberField(TEXT("circle_steps")) : 8;

	// UE5.7: AppendCapsule(Mesh, Options, Transform, Radius, LineLength, HemisphereSteps, CircleSteps, SegmentSteps, Origin, Debug)
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCapsule(
		Mesh, PrimOptions, Transform,
		Radius, LineLength,
		HemisphereSteps, CircleSteps, 0,
		EGeometryScriptPrimitiveOriginMode::Center, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleAppendRevolve(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	if (MeshName.IsEmpty()) return MakeErrorResponse(TEXT("mesh_name is required"));

	UDynamicMesh* Mesh = GetOrCreateMesh(MeshName);
	FGeometryScriptPrimitiveOptions PrimOptions;
	FTransform Transform = ParseTransform(Params);

	TArray<FVector2D> PathPoints;
	if (Params->HasField(TEXT("path_points")))
	{
		const TArray<TSharedPtr<FJsonValue>>& PointsArray = Params->GetArrayField(TEXT("path_points"));
		for (const auto& PointVal : PointsArray)
		{
			const TArray<TSharedPtr<FJsonValue>>& Coords = PointVal->AsArray();
			if (Coords.Num() >= 2)
			{
				PathPoints.Add(FVector2D(Coords[0]->AsNumber(), Coords[1]->AsNumber()));
			}
		}
	}
	if (PathPoints.Num() < 2) return MakeErrorResponse(TEXT("path_points must contain at least 2 points"));

	float RevolveDegrees = Params->HasField(TEXT("revolve_degrees")) ? Params->GetNumberField(TEXT("revolve_degrees")) : 360.0f;
	int32 Steps = Params->HasField(TEXT("steps")) ? (int32)Params->GetNumberField(TEXT("steps")) : 8;
	bool bCapped = Params->HasField(TEXT("capped")) ? Params->GetBoolField(TEXT("capped")) : true;

	FGeometryScriptRevolveOptions RevolveOptions;
	RevolveOptions.RevolveDegrees = RevolveDegrees;

	// UE5.7: AppendRevolvePath(Mesh, Options, Transform, PathVertices, RevolveOptions, Steps, bCapped, Debug)
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRevolvePath(
		Mesh, PrimOptions, Transform,
		PathPoints, RevolveOptions,
		Steps, bCapped, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleAppendSweep(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	if (MeshName.IsEmpty()) return MakeErrorResponse(TEXT("mesh_name is required"));

	UDynamicMesh* Mesh = GetOrCreateMesh(MeshName);
	FGeometryScriptPrimitiveOptions PrimOptions;
	FTransform Transform = ParseTransform(Params);

	TArray<FVector2D> PolygonVertices;
	if (Params->HasField(TEXT("polygon_vertices")))
	{
		const TArray<TSharedPtr<FJsonValue>>& VertsArray = Params->GetArrayField(TEXT("polygon_vertices"));
		for (const auto& VertVal : VertsArray)
		{
			const TArray<TSharedPtr<FJsonValue>>& Coords = VertVal->AsArray();
			if (Coords.Num() >= 2)
			{
				PolygonVertices.Add(FVector2D(Coords[0]->AsNumber(), Coords[1]->AsNumber()));
			}
		}
	}

	TArray<FTransform> SweepPath;
	if (Params->HasField(TEXT("sweep_path")))
	{
		const TArray<TSharedPtr<FJsonValue>>& PathArray = Params->GetArrayField(TEXT("sweep_path"));
		for (const auto& PathVal : PathArray)
		{
			const TSharedPtr<FJsonObject>& PathObj = PathVal->AsObject();
			FVector Loc(0, 0, 0);
			if (PathObj->HasField(TEXT("location")))
			{
				const TArray<TSharedPtr<FJsonValue>>& L = PathObj->GetArrayField(TEXT("location"));
				if (L.Num() >= 3) Loc = FVector(L[0]->AsNumber(), L[1]->AsNumber(), L[2]->AsNumber());
			}
			SweepPath.Add(FTransform(Loc));
		}
	}

	if (PolygonVertices.Num() < 3) return MakeErrorResponse(TEXT("polygon_vertices must contain at least 3 vertices"));
	if (SweepPath.Num() < 2) return MakeErrorResponse(TEXT("sweep_path must contain at least 2 transforms"));

	bool bLoop = Params->HasField(TEXT("loop")) ? Params->GetBoolField(TEXT("loop")) : false;

	// UE5.7: AppendSweepPolygon(Mesh, Options, Transform, PolygonVerts, SweepPath, bLoop, bCapped, StartScale, EndScale, RotationAngleDeg, MiterLimit, Debug)
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(
		Mesh, PrimOptions, Transform,
		PolygonVertices, SweepPath, bLoop, true,
		1.0f, 1.0f, 0.0f, 1.0f, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

// ============================================================================
// Booleans - UE5.7 correct signatures
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleBoolean(const TSharedPtr<FJsonObject>& Params)
{
	FString TargetName = Params->GetStringField(TEXT("target_mesh"));
	FString ToolName = Params->GetStringField(TEXT("tool_mesh"));
	FString OutputName = Params->HasField(TEXT("output_mesh")) ? Params->GetStringField(TEXT("output_mesh")) : TargetName;
	FString OpStr = Params->HasField(TEXT("operation")) ? Params->GetStringField(TEXT("operation")) : TEXT("subtract");

	UDynamicMesh* Target = FindMesh(TargetName);
	UDynamicMesh* Tool = FindMesh(ToolName);
	if (!Target) return MakeErrorResponse(FString::Printf(TEXT("Target mesh '%s' not found"), *TargetName));
	if (!Tool) return MakeErrorResponse(FString::Printf(TEXT("Tool mesh '%s' not found"), *ToolName));

	EGeometryScriptBooleanOperation Op = EGeometryScriptBooleanOperation::Subtract;
	if (OpStr == TEXT("union")) Op = EGeometryScriptBooleanOperation::Union;
	else if (OpStr == TEXT("intersect")) Op = EGeometryScriptBooleanOperation::Intersection;

	UDynamicMesh* OutputMesh = GetOrCreateMesh(OutputName);

	FGeometryScriptMeshBooleanOptions BoolOptions;
	// If output != target, copy target content to output first
	if (OutputMesh != Target)
	{
		FGeometryScriptAppendMeshOptions AppendOptions;
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
			OutputMesh, Target, FTransform::Identity, false, AppendOptions, nullptr);
	}
	// UE5.7: ApplyMeshBoolean(TargetMesh, TargetTransform, ToolMesh, ToolTransform, Op, Options, Debug)
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
		OutputMesh, FTransform::Identity, Tool, FTransform::Identity, Op, BoolOptions, nullptr);

	return MakeSuccessResponse(OutputName, OutputMesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleSelfUnion(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FGeometryScriptMeshSelfUnionOptions Options;
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshSelfUnion(Mesh, Options, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

// ============================================================================
// Modeling Operations - UE5.7 correct signatures
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleOffset(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FGeometryScriptMeshOffsetOptions Options;
	Options.OffsetDistance = Params->HasField(TEXT("distance")) ? Params->GetNumberField(TEXT("distance")) : 1.0f;
	Options.bFixedBoundary = Params->HasField(TEXT("fixed_boundary")) ? Params->GetBoolField(TEXT("fixed_boundary")) : false;
	Options.SolveSteps = Params->HasField(TEXT("iterations")) ? (int32)Params->GetNumberField(TEXT("iterations")) : 5;

	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshOffset(Mesh, Options, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleShell(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FGeometryScriptMeshOffsetOptions Options;
	Options.OffsetDistance = Params->HasField(TEXT("thickness")) ? Params->GetNumberField(TEXT("thickness")) : 5.0f;
	Options.SolveSteps = 5;

	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshShell(Mesh, Options, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

// ============================================================================
// Deformations - UE5.7 correct signatures
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleBend(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	// UE5.7: ApplyBendWarpToMesh(Mesh, Options, BendOrientation, BendAngle, BendExtent, Debug)
	FGeometryScriptBendWarpOptions Options;
	float BendAngle = Params->HasField(TEXT("angle")) ? Params->GetNumberField(TEXT("angle")) : 45.0f;
	float BendExtent = Params->HasField(TEXT("extent")) ? Params->GetNumberField(TEXT("extent")) : 50.0f;

	UGeometryScriptLibrary_MeshDeformFunctions::ApplyBendWarpToMesh(
		Mesh, Options, FTransform::Identity, BendAngle, BendExtent, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleTwist(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	// UE5.7: ApplyTwistWarpToMesh(Mesh, Options, TwistOrientation, TwistAngle, TwistExtent, Debug)
	FGeometryScriptTwistWarpOptions Options;
	float TwistAngle = Params->HasField(TEXT("angle")) ? Params->GetNumberField(TEXT("angle")) : 90.0f;
	float TwistExtent = Params->HasField(TEXT("extent")) ? Params->GetNumberField(TEXT("extent")) : 50.0f;

	UGeometryScriptLibrary_MeshDeformFunctions::ApplyTwistWarpToMesh(
		Mesh, Options, FTransform::Identity, TwistAngle, TwistExtent, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleFlare(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	// UE5.7: ApplyFlareWarpToMesh(Mesh, Options, FlareOrientation, FlarePercentX, FlarePercentY, FlareExtent, Debug)
	FGeometryScriptFlareWarpOptions Options;
	float FlareX = Params->HasField(TEXT("flare_x")) ? Params->GetNumberField(TEXT("flare_x")) : 0.5f;
	float FlareY = Params->HasField(TEXT("flare_y")) ? Params->GetNumberField(TEXT("flare_y")) : 0.5f;
	float FlareExtent = Params->HasField(TEXT("extent")) ? Params->GetNumberField(TEXT("extent")) : 50.0f;

	UGeometryScriptLibrary_MeshDeformFunctions::ApplyFlareWarpToMesh(
		Mesh, Options, FTransform::Identity, FlareX, FlareY, FlareExtent, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandlePerlinNoise(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FGeometryScriptPerlinNoiseOptions Options;
	Options.BaseLayer.Magnitude = Params->HasField(TEXT("magnitude")) ? Params->GetNumberField(TEXT("magnitude")) : 5.0f;
	Options.BaseLayer.Frequency = Params->HasField(TEXT("frequency")) ? Params->GetNumberField(TEXT("frequency")) : 0.1f;

	FGeometryScriptMeshSelection Selection; // empty = full mesh

	UGeometryScriptLibrary_MeshDeformFunctions::ApplyPerlinNoiseToMesh2(
		Mesh, Selection, Options, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleSmooth(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	// UE5.7: ApplyIterativeSmoothingToMesh(Mesh, Selection, Options, Debug)
	FGeometryScriptIterativeMeshSmoothingOptions Options;
	Options.NumIterations = Params->HasField(TEXT("iterations")) ? (int32)Params->GetNumberField(TEXT("iterations")) : 10;
	Options.Alpha = Params->HasField(TEXT("speed")) ? Params->GetNumberField(TEXT("speed")) : 0.2f;

	FGeometryScriptMeshSelection Selection;

	UGeometryScriptLibrary_MeshDeformFunctions::ApplyIterativeSmoothingToMesh(
		Mesh, Selection, Options, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

// ============================================================================
// Queries
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleQueryMeshInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));
	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleQueryVertices(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	int32 MaxVertices = Params->HasField(TEXT("max_vertices")) ? (int32)Params->GetNumberField(TEXT("max_vertices")) : 100;
	int32 VertexCount = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexCount(Mesh);
	int32 CountToReturn = FMath::Min(MaxVertices, VertexCount);

	TArray<TSharedPtr<FJsonValue>> VertexArray;
	for (int32 i = 0; i < CountToReturn; i++)
	{
		bool bIsValid = false;
		FVector Pos = UGeometryScriptLibrary_MeshQueryFunctions::GetVertexPosition(Mesh, i, bIsValid);
		if (bIsValid)
		{
			TSharedPtr<FJsonObject> VertObj = MakeShareable(new FJsonObject);
			VertObj->SetNumberField(TEXT("id"), i);
			VertObj->SetNumberField(TEXT("x"), Pos.X);
			VertObj->SetNumberField(TEXT("y"), Pos.Y);
			VertObj->SetNumberField(TEXT("z"), Pos.Z);
			VertexArray.Add(MakeShareable(new FJsonValueObject(VertObj)));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("mesh_name"), MeshName);
	Result->SetNumberField(TEXT("total_vertices"), VertexCount);
	Result->SetNumberField(TEXT("returned_vertices"), CountToReturn);
	Result->SetArrayField(TEXT("vertices"), VertexArray);
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleGetBounds(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FBox BoundingBox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
	FVector Center = BoundingBox.GetCenter();
	FVector Extent = BoundingBox.GetExtent();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("mesh_name"), MeshName);

	TSharedPtr<FJsonObject> BoundsJson = MakeShareable(new FJsonObject);
	BoundsJson->SetNumberField(TEXT("min_x"), BoundingBox.Min.X);
	BoundsJson->SetNumberField(TEXT("min_y"), BoundingBox.Min.Y);
	BoundsJson->SetNumberField(TEXT("min_z"), BoundingBox.Min.Z);
	BoundsJson->SetNumberField(TEXT("max_x"), BoundingBox.Max.X);
	BoundsJson->SetNumberField(TEXT("max_y"), BoundingBox.Max.Y);
	BoundsJson->SetNumberField(TEXT("max_z"), BoundingBox.Max.Z);
	BoundsJson->SetNumberField(TEXT("center_x"), Center.X);
	BoundsJson->SetNumberField(TEXT("center_y"), Center.Y);
	BoundsJson->SetNumberField(TEXT("center_z"), Center.Z);
	BoundsJson->SetNumberField(TEXT("extent_x"), Extent.X);
	BoundsJson->SetNumberField(TEXT("extent_y"), Extent.Y);
	BoundsJson->SetNumberField(TEXT("extent_z"), Extent.Z);
	Result->SetObjectField(TEXT("bounds"), BoundsJson);
	return Result;
}

// ============================================================================
// UV Operations - UE5.7 correct signatures
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleAutoGenerateUVs(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	int32 UVChannel = Params->HasField(TEXT("uv_channel")) ? (int32)Params->GetNumberField(TEXT("uv_channel")) : 0;

	FGeometryScriptPatchBuilderOptions Options;

	UGeometryScriptLibrary_MeshUVFunctions::AutoGeneratePatchBuilderMeshUVs(
		Mesh, UVChannel, Options, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandlePlanarProjection(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	int32 UVChannel = Params->HasField(TEXT("uv_channel")) ? (int32)Params->GetNumberField(TEXT("uv_channel")) : 0;
	FTransform PlaneTransform = ParseTransform(Params);
	FGeometryScriptMeshSelection Selection;

	UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromPlanarProjection(
		Mesh, UVChannel, PlaneTransform, Selection, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleBoxProjection(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	int32 UVChannel = Params->HasField(TEXT("uv_channel")) ? (int32)Params->GetNumberField(TEXT("uv_channel")) : 0;
	FTransform BoxTransform = ParseTransform(Params);
	FGeometryScriptMeshSelection Selection;

	UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromBoxProjection(
		Mesh, UVChannel, BoxTransform, Selection, 2, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

// ============================================================================
// Materials - UE5.7 correct signatures
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleSetMaterialID(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	int32 MaterialID = Params->HasField(TEXT("material_id")) ? (int32)Params->GetNumberField(TEXT("material_id")) : 0;

	if (Params->HasField(TEXT("triangle_id")))
	{
		int32 TriID = (int32)Params->GetNumberField(TEXT("triangle_id"));
		bool bIsValid = false;
		UGeometryScriptLibrary_MeshMaterialFunctions::SetTriangleMaterialID(
			Mesh, TriID, MaterialID, bIsValid, false);
	}
	else
	{
		// Set all triangles: iterate and set each one
		int32 NumTriangles = UGeometryScriptLibrary_MeshQueryFunctions::GetNumTriangleIDs(Mesh);
		for (int32 i = 0; i < NumTriangles; i++)
		{
			bool bIsValid = false;
			UGeometryScriptLibrary_MeshMaterialFunctions::SetTriangleMaterialID(
				Mesh, i, MaterialID, bIsValid, true);
		}
	}

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleRemapMaterialIDs(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	int32 FromID = Params->HasField(TEXT("from_material_id")) ? (int32)Params->GetNumberField(TEXT("from_material_id")) : 0;
	int32 ToID = Params->HasField(TEXT("to_material_id")) ? (int32)Params->GetNumberField(TEXT("to_material_id")) : 1;

	UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(Mesh, FromID, ToID, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleGetMaterialIDs(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	// UE5.7: GetMaxMaterialID(Mesh, bHasMaterialIDs)
	bool bHasMaterialIDs = false;
	int32 MaxMaterialID = UGeometryScriptLibrary_MeshMaterialFunctions::GetMaxMaterialID(Mesh, bHasMaterialIDs);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("mesh_name"), MeshName);
	Result->SetBoolField(TEXT("has_material_ids"), bHasMaterialIDs);
	Result->SetNumberField(TEXT("max_material_id"), MaxMaterialID);
	return Result;
}

// ============================================================================
// Transform
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleTransformMesh(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FTransform Transform = ParseTransform(Params);
	UGeometryScriptLibrary_MeshTransformFunctions::TransformMesh(Mesh, Transform, false, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleTranslateMesh(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FVector Translation(0, 0, 0);
	if (Params->HasField(TEXT("translation")))
	{
		const TArray<TSharedPtr<FJsonValue>>& T = Params->GetArrayField(TEXT("translation"));
		if (T.Num() >= 3) Translation = FVector(T[0]->AsNumber(), T[1]->AsNumber(), T[2]->AsNumber());
	}
	else if (Params->HasField(TEXT("location")))
	{
		const TArray<TSharedPtr<FJsonValue>>& L = Params->GetArrayField(TEXT("location"));
		if (L.Num() >= 3) Translation = FVector(L[0]->AsNumber(), L[1]->AsNumber(), L[2]->AsNumber());
	}

	UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(Mesh, Translation, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleScaleMesh(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FVector ScaleVec(1, 1, 1);
	if (Params->HasField(TEXT("scale")))
	{
		const TArray<TSharedPtr<FJsonValue>>& S = Params->GetArrayField(TEXT("scale"));
		if (S.Num() >= 3) ScaleVec = FVector(S[0]->AsNumber(), S[1]->AsNumber(), S[2]->AsNumber());
	}
	else if (Params->HasField(TEXT("uniform_scale")))
	{
		float US = Params->GetNumberField(TEXT("uniform_scale"));
		ScaleVec = FVector(US, US, US);
	}

	FVector ScaleOrigin(0, 0, 0);
	UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(Mesh, ScaleVec, ScaleOrigin, false, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

// ============================================================================
// Commit / Materialize - UE5.7 correct signatures
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleCopyToStaticMesh(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	if (AssetPath.IsEmpty()) return MakeErrorResponse(TEXT("asset_path is required (e.g. '/Game/Generated/MyMesh')"));

	UStaticMesh* TargetMesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
	if (!TargetMesh)
	{
		FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
		FString MeshAssetName = FPackageName::GetLongPackageAssetName(AssetPath);
		UPackage* Package = CreatePackage(*PackageName);
		TargetMesh = NewObject<UStaticMesh>(Package, *MeshAssetName, RF_Public | RF_Standalone);

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().AssetCreated(TargetMesh);
	}

	FGeometryScriptCopyMeshToAssetOptions Options;
	Options.bEnableRecomputeNormals = true;
	Options.bEnableRecomputeTangents = true;
	FGeometryScriptMeshWriteLOD TargetLOD;
	EGeometryScriptOutcomePins Outcome;

	// UE5.7: CopyMeshToStaticMesh(FromDynamicMesh, ToStaticMeshAsset, Options, TargetLOD, Outcome, bUseSectionMaterials, Debug)
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(
		Mesh, TargetMesh, Options, TargetLOD, Outcome, true, nullptr);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), Outcome == EGeometryScriptOutcomePins::Success);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("mesh_name"), MeshName);
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleSpawnDynamicMeshActor(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return MakeErrorResponse(TEXT("No editor world available"));

	FTransform SpawnTransform = ParseTransform(Params);
	FString ActorLabel = Params->HasField(TEXT("actor_name")) ? Params->GetStringField(TEXT("actor_name")) : MeshName;

	AActor* NewActor = World->SpawnActor<AActor>(AActor::StaticClass(), SpawnTransform);
	if (!NewActor) return MakeErrorResponse(TEXT("Failed to spawn actor"));

	NewActor->SetActorLabel(*ActorLabel);

	UDynamicMeshComponent* MeshComp = NewObject<UDynamicMeshComponent>(NewActor, TEXT("DynamicMeshComponent"));
	MeshComp->RegisterComponent();
	NewActor->SetRootComponent(MeshComp);
	NewActor->AddInstanceComponent(MeshComp);
	MeshComp->SetDynamicMesh(Mesh);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), ActorLabel);
	Result->SetStringField(TEXT("mesh_name"), MeshName);
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleCopyFromStaticMesh(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	if (MeshName.IsEmpty()) return MakeErrorResponse(TEXT("mesh_name is required"));

	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	if (AssetPath.IsEmpty()) return MakeErrorResponse(TEXT("asset_path is required"));

	UStaticMesh* SourceMesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
	if (!SourceMesh) return MakeErrorResponse(FString::Printf(TEXT("Static mesh not found at '%s'"), *AssetPath));

	UDynamicMesh* Mesh = GetOrCreateMesh(MeshName);

	FGeometryScriptCopyMeshFromAssetOptions Options;
	FGeometryScriptMeshReadLOD RequestedLOD;
	EGeometryScriptOutcomePins Outcome;

	// UE5.7: CopyMeshFromStaticMesh(FromStaticMesh, ToDynamicMesh, Options, RequestedLOD, Outcome, Debug)
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
		SourceMesh, Mesh, Options, RequestedLOD, Outcome, nullptr);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), Outcome == EGeometryScriptOutcomePins::Success);
	Result->SetStringField(TEXT("mesh_name"), MeshName);
	if (Outcome == EGeometryScriptOutcomePins::Success)
	{
		Result->SetObjectField(TEXT("mesh_info"), MakeMeshInfoJson(Mesh));
	}
	return Result;
}

// ============================================================================
// Append Mesh to Mesh - UE5.7 correct signature
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleAppendMesh(const TSharedPtr<FJsonObject>& Params)
{
	FString TargetName = Params->GetStringField(TEXT("target_mesh"));
	FString SourceName = Params->GetStringField(TEXT("source_mesh"));

	UDynamicMesh* Target = FindMesh(TargetName);
	UDynamicMesh* Source = FindMesh(SourceName);
	if (!Target) return MakeErrorResponse(FString::Printf(TEXT("Target mesh '%s' not found"), *TargetName));
	if (!Source) return MakeErrorResponse(FString::Printf(TEXT("Source mesh '%s' not found"), *SourceName));

	FTransform Transform = ParseTransform(Params);

	// UE5.7: AppendMesh(TargetMesh, AppendMesh, AppendTransform, bDeferChangeNotifications, AppendOptions, Debug)
	FGeometryScriptAppendMeshOptions AppendOptions;
	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
		Target, Source, Transform, true, AppendOptions, nullptr);

	return MakeSuccessResponse(TargetName, Target);
}

// ============================================================================
// Advanced Modeling Operations
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleLinearExtrudeFaces(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FGeometryScriptMeshLinearExtrudeOptions Options;
	Options.Distance = Params->HasField(TEXT("distance")) ? Params->GetNumberField(TEXT("distance")) : 10.0f;
	if (Params->HasField(TEXT("direction")))
	{
		const TArray<TSharedPtr<FJsonValue>>& Dir = Params->GetArrayField(TEXT("direction"));
		if (Dir.Num() >= 3)
		{
			Options.Direction = FVector(Dir[0]->AsNumber(), Dir[1]->AsNumber(), Dir[2]->AsNumber());
			Options.DirectionMode = EGeometryScriptLinearExtrudeDirection::FixedDirection;
		}
	}

	// Build selection: by material ID or select all
	FGeometryScriptMeshSelection Selection;
	if (Params->HasField(TEXT("material_id")))
	{
		int32 MatID = (int32)Params->GetNumberField(TEXT("material_id"));
		UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByMaterialID(
			Mesh, MatID, Selection);
	}
	else
	{
		UGeometryScriptLibrary_MeshSelectionFunctions::CreateSelectAllMeshSelection(
			Mesh, Selection, EGeometryScriptMeshSelectionType::Triangles);
	}

	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshLinearExtrudeFaces(
		Mesh, Options, Selection, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleOffsetFaces(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FGeometryScriptMeshOffsetFacesOptions Options;
	Options.Distance = Params->HasField(TEXT("distance")) ? Params->GetNumberField(TEXT("distance")) : 1.0f;

	FGeometryScriptMeshSelection Selection;
	if (Params->HasField(TEXT("material_id")))
	{
		int32 MatID = (int32)Params->GetNumberField(TEXT("material_id"));
		UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByMaterialID(
			Mesh, MatID, Selection);
	}
	else
	{
		UGeometryScriptLibrary_MeshSelectionFunctions::CreateSelectAllMeshSelection(
			Mesh, Selection, EGeometryScriptMeshSelectionType::Triangles);
	}

	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshOffsetFaces(
		Mesh, Options, Selection, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleInsetOutsetFaces(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FGeometryScriptMeshInsetOutsetFacesOptions Options;
	Options.Distance = Params->HasField(TEXT("distance")) ? Params->GetNumberField(TEXT("distance")) : 1.0f;
	Options.bReproject = Params->HasField(TEXT("reproject")) ? Params->GetBoolField(TEXT("reproject")) : true;
	Options.Softness = Params->HasField(TEXT("softness")) ? Params->GetNumberField(TEXT("softness")) : 0.0f;

	FGeometryScriptMeshSelection Selection;
	if (Params->HasField(TEXT("material_id")))
	{
		int32 MatID = (int32)Params->GetNumberField(TEXT("material_id"));
		UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByMaterialID(
			Mesh, MatID, Selection);
	}
	else
	{
		UGeometryScriptLibrary_MeshSelectionFunctions::CreateSelectAllMeshSelection(
			Mesh, Selection, EGeometryScriptMeshSelectionType::Triangles);
	}

	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshInsetOutsetFaces(
		Mesh, Options, Selection, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleBevel(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FGeometryScriptMeshBevelOptions Options;
	Options.BevelDistance = Params->HasField(TEXT("distance")) ? Params->GetNumberField(TEXT("distance")) : 1.0f;
	Options.Subdivisions = Params->HasField(TEXT("subdivisions")) ? (int32)Params->GetNumberField(TEXT("subdivisions")) : 0;
	Options.RoundWeight = Params->HasField(TEXT("round_weight")) ? Params->GetNumberField(TEXT("round_weight")) : 1.0f;

	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshPolygroupBevel(
		Mesh, Options, nullptr);

	return MakeSuccessResponse(MeshName, Mesh);
}

// ============================================================================
// Selection Commands
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleSelectAll(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FGeometryScriptMeshSelection Selection;
	UGeometryScriptLibrary_MeshSelectionFunctions::CreateSelectAllMeshSelection(
		Mesh, Selection, EGeometryScriptMeshSelectionType::Triangles);

	EGeometryScriptMeshSelectionType SelectionType;
	int32 NumSelected = 0;
	UGeometryScriptLibrary_MeshSelectionFunctions::GetMeshSelectionInfo(Selection, SelectionType, NumSelected);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("mesh_name"), MeshName);
	Result->SetNumberField(TEXT("num_selected"), NumSelected);
	Result->SetStringField(TEXT("selection_type"), TEXT("triangles"));
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleSelectByMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	int32 MaterialID = Params->HasField(TEXT("material_id")) ? (int32)Params->GetNumberField(TEXT("material_id")) : 0;

	FGeometryScriptMeshSelection Selection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByMaterialID(
		Mesh, MaterialID, Selection);

	EGeometryScriptMeshSelectionType SelectionType;
	int32 NumSelected = 0;
	UGeometryScriptLibrary_MeshSelectionFunctions::GetMeshSelectionInfo(Selection, SelectionType, NumSelected);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("mesh_name"), MeshName);
	Result->SetNumberField(TEXT("material_id"), MaterialID);
	Result->SetNumberField(TEXT("num_selected"), NumSelected);
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleSelectInBox(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FVector Min(0, 0, 0), Max(100, 100, 100);
	if (Params->HasField(TEXT("box_min")))
	{
		const TArray<TSharedPtr<FJsonValue>>& B = Params->GetArrayField(TEXT("box_min"));
		if (B.Num() >= 3) Min = FVector(B[0]->AsNumber(), B[1]->AsNumber(), B[2]->AsNumber());
	}
	if (Params->HasField(TEXT("box_max")))
	{
		const TArray<TSharedPtr<FJsonValue>>& B = Params->GetArrayField(TEXT("box_max"));
		if (B.Num() >= 3) Max = FVector(B[0]->AsNumber(), B[1]->AsNumber(), B[2]->AsNumber());
	}

	FBox Box(Min, Max);
	FGeometryScriptMeshSelection Selection;
	// UE5.7: SelectMeshElementsInBox(Mesh, Selection, Box, SelectionType, bInvert, MinNumTrianglePoints)
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsInBox(
		Mesh, Selection, Box, EGeometryScriptMeshSelectionType::Triangles, false, 3);

	EGeometryScriptMeshSelectionType SelectionType;
	int32 NumSelected = 0;
	UGeometryScriptLibrary_MeshSelectionFunctions::GetMeshSelectionInfo(Selection, SelectionType, NumSelected);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("mesh_name"), MeshName);
	Result->SetNumberField(TEXT("num_selected"), NumSelected);
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleSelectByNormal(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FVector Normal(0, 0, 1);
	if (Params->HasField(TEXT("normal")))
	{
		const TArray<TSharedPtr<FJsonValue>>& N = Params->GetArrayField(TEXT("normal"));
		if (N.Num() >= 3) Normal = FVector(N[0]->AsNumber(), N[1]->AsNumber(), N[2]->AsNumber());
	}
	double MaxAngle = Params->HasField(TEXT("max_angle")) ? Params->GetNumberField(TEXT("max_angle")) : 30.0;

	FGeometryScriptMeshSelection Selection;
	// UE5.7: SelectMeshElementsByNormalAngle(Mesh, Selection, Normal, MaxAngleDeg, SelectionType, bInvert, MinNumTrianglePoints)
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByNormalAngle(
		Mesh, Selection, Normal, MaxAngle, EGeometryScriptMeshSelectionType::Triangles, false, 3);

	EGeometryScriptMeshSelectionType SelectionType;
	int32 NumSelected = 0;
	UGeometryScriptLibrary_MeshSelectionFunctions::GetMeshSelectionInfo(Selection, SelectionType, NumSelected);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("mesh_name"), MeshName);
	Result->SetNumberField(TEXT("num_selected"), NumSelected);
	Result->SetStringField(TEXT("normal"), FString::Printf(TEXT("(%f,%f,%f)"), Normal.X, Normal.Y, Normal.Z));
	Result->SetNumberField(TEXT("max_angle"), MaxAngle);
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleSelectInvert(const TSharedPtr<FJsonObject>& Params)
{
	// For now, return info - full selection persistence would need a selection registry
	return MakeErrorResponse(TEXT("Selection inversion requires persistent selection registry - use material IDs for selection workflows"));
}

// ============================================================================
// Spatial Queries (Raycast with BVH)
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleRaycast(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FVector RayOrigin(0, 0, 1000);
	FVector RayDirection(0, 0, -1);

	if (Params->HasField(TEXT("ray_origin")))
	{
		const TArray<TSharedPtr<FJsonValue>>& O = Params->GetArrayField(TEXT("ray_origin"));
		if (O.Num() >= 3) RayOrigin = FVector(O[0]->AsNumber(), O[1]->AsNumber(), O[2]->AsNumber());
	}
	if (Params->HasField(TEXT("ray_direction")))
	{
		const TArray<TSharedPtr<FJsonValue>>& D = Params->GetArrayField(TEXT("ray_direction"));
		if (D.Num() >= 3) RayDirection = FVector(D[0]->AsNumber(), D[1]->AsNumber(), D[2]->AsNumber());
	}

	// Build BVH and raycast
	FGeometryScriptDynamicMeshBVH BVH;
	UGeometryScriptLibrary_MeshSpatial::BuildBVHForMesh(Mesh, BVH, nullptr);

	FGeometryScriptRayHitResult HitResult;
	FGeometryScriptSpatialQueryOptions QueryOptions;
	EGeometryScriptSearchOutcomePins Outcome;

	UGeometryScriptLibrary_MeshSpatial::FindNearestRayIntersectionWithMesh(
		Mesh, BVH, RayOrigin, RayDirection, QueryOptions, HitResult, Outcome, nullptr);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("hit"), HitResult.bHit);
	if (HitResult.bHit)
	{
		Result->SetNumberField(TEXT("ray_parameter"), HitResult.RayParameter);
		Result->SetNumberField(TEXT("hit_triangle_id"), HitResult.HitTriangleID);
		Result->SetNumberField(TEXT("hit_x"), HitResult.HitPosition.X);
		Result->SetNumberField(TEXT("hit_y"), HitResult.HitPosition.Y);
		Result->SetNumberField(TEXT("hit_z"), HitResult.HitPosition.Z);
	}
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleIsPointInside(const TSharedPtr<FJsonObject>& Params)
{
	FString MeshName = Params->GetStringField(TEXT("mesh_name"));
	UDynamicMesh* Mesh = FindMesh(MeshName);
	if (!Mesh) return MakeErrorResponse(FString::Printf(TEXT("Mesh '%s' not found"), *MeshName));

	FVector Point(0, 0, 0);
	if (Params->HasField(TEXT("point")))
	{
		const TArray<TSharedPtr<FJsonValue>>& P = Params->GetArrayField(TEXT("point"));
		if (P.Num() >= 3) Point = FVector(P[0]->AsNumber(), P[1]->AsNumber(), P[2]->AsNumber());
	}

	FGeometryScriptDynamicMeshBVH BVH;
	UGeometryScriptLibrary_MeshSpatial::BuildBVHForMesh(Mesh, BVH, nullptr);

	FGeometryScriptSpatialQueryOptions QueryOptions;
	bool bIsInside = false;
	EGeometryScriptContainmentOutcomePins Outcome;

	UGeometryScriptLibrary_MeshSpatial::IsPointInsideMesh(
		Mesh, BVH, Point, QueryOptions, bIsInside, Outcome, nullptr);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("is_inside"), bIsInside);
	Result->SetNumberField(TEXT("point_x"), Point.X);
	Result->SetNumberField(TEXT("point_y"), Point.Y);
	Result->SetNumberField(TEXT("point_z"), Point.Z);
	return Result;
}

// ============================================================================
// Mesh Cleanup
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPGeometryScriptCommands::HandleCleanupMeshes(const TSharedPtr<FJsonObject>& Params)
{
	double MaxAge = Params->HasField(TEXT("max_age_seconds")) ? Params->GetNumberField(TEXT("max_age_seconds")) : 300.0;
	double Now = FPlatformTime::Seconds();

	TArray<FString> ToRemove;
	for (auto& Pair : MeshLastAccessTime)
	{
		if ((Now - Pair.Value) > MaxAge)
		{
			ToRemove.Add(Pair.Key);
		}
	}

	TArray<TSharedPtr<FJsonValue>> ReleasedNames;
	for (const FString& Name : ToRemove)
	{
		if (TObjectPtr<UDynamicMesh>* Found = MeshRegistry.Find(Name))
		{
			if (*Found) (*Found)->RemoveFromRoot();
			MeshRegistry.Remove(Name);
		}
		MeshLastAccessTime.Remove(Name);
		ReleasedNames.Add(MakeShareable(new FJsonValueString(Name)));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("released_count"), ToRemove.Num());
	Result->SetNumberField(TEXT("remaining_count"), MeshRegistry.Num());
	Result->SetArrayField(TEXT("released_names"), ReleasedNames);
	return Result;
}
