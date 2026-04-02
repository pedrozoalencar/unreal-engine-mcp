#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UDynamicMesh;

/**
 * Handler class for Geometry Script MCP commands.
 * Maintains a named registry of UDynamicMesh objects and named selections.
 */
class UNREALMCP_API FUnrealMCPGeometryScriptCommands
{
public:
	FUnrealMCPGeometryScriptCommands();
	~FUnrealMCPGeometryScriptCommands();

	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Mesh registry
	TMap<FString, TObjectPtr<UDynamicMesh>> MeshRegistry;

	// Mesh access timestamps for auto-cleanup
	TMap<FString, double> MeshLastAccessTime;
	void UpdateMeshAccess(const FString& Name);

	// Cleanup command
	TSharedPtr<FJsonObject> HandleCleanupMeshes(const TSharedPtr<FJsonObject>& Params);

	// Registry helpers
	UDynamicMesh* GetOrCreateMesh(const FString& Name);
	UDynamicMesh* FindMesh(const FString& Name);
	TSharedPtr<FJsonObject> MakeMeshInfoJson(UDynamicMesh* Mesh);
	TSharedPtr<FJsonObject> MakeErrorResponse(const FString& Message);
	TSharedPtr<FJsonObject> MakeSuccessResponse(const FString& MeshName, UDynamicMesh* Mesh);
	FTransform ParseTransform(const TSharedPtr<FJsonObject>& Params);

	// --- Mesh Lifecycle ---
	TSharedPtr<FJsonObject> HandleCreateMesh(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleReleaseMesh(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListMeshes(const TSharedPtr<FJsonObject>& Params);

	// --- Primitives ---
	TSharedPtr<FJsonObject> HandleAppendBox(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAppendSphere(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAppendCylinder(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAppendCone(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAppendTorus(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAppendCapsule(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAppendRevolve(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAppendSweep(const TSharedPtr<FJsonObject>& Params);

	// --- Booleans ---
	TSharedPtr<FJsonObject> HandleBoolean(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSelfUnion(const TSharedPtr<FJsonObject>& Params);

	// --- Modeling Operations ---
	TSharedPtr<FJsonObject> HandleOffset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleShell(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleLinearExtrudeFaces(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleOffsetFaces(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleInsetOutsetFaces(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleBevel(const TSharedPtr<FJsonObject>& Params);

	// --- Deformations ---
	TSharedPtr<FJsonObject> HandleBend(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleTwist(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleFlare(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandlePerlinNoise(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSmooth(const TSharedPtr<FJsonObject>& Params);

	// --- Queries ---
	TSharedPtr<FJsonObject> HandleQueryMeshInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleQueryVertices(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetBounds(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRaycast(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleIsPointInside(const TSharedPtr<FJsonObject>& Params);

	// --- Selection ---
	TSharedPtr<FJsonObject> HandleSelectAll(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSelectByMaterial(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSelectInBox(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSelectByNormal(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSelectInvert(const TSharedPtr<FJsonObject>& Params);

	// --- UV Operations ---
	TSharedPtr<FJsonObject> HandleAutoGenerateUVs(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandlePlanarProjection(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleBoxProjection(const TSharedPtr<FJsonObject>& Params);

	// --- Materials ---
	TSharedPtr<FJsonObject> HandleSetMaterialID(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemapMaterialIDs(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetMaterialIDs(const TSharedPtr<FJsonObject>& Params);

	// --- Transform ---
	TSharedPtr<FJsonObject> HandleTransformMesh(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleTranslateMesh(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleScaleMesh(const TSharedPtr<FJsonObject>& Params);

	// --- Commit / Materialize ---
	TSharedPtr<FJsonObject> HandleCopyToStaticMesh(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSpawnDynamicMeshActor(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleCopyFromStaticMesh(const TSharedPtr<FJsonObject>& Params);

	// --- Append Mesh ---
	TSharedPtr<FJsonObject> HandleAppendMesh(const TSharedPtr<FJsonObject>& Params);
};
