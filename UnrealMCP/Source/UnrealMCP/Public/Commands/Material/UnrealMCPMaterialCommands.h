#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class UMaterial;
class UMaterialExpression;

/**
 * Handler for Material editing MCP commands.
 * Full control over UE5 material graphs including Substrate Slab BSDF.
 */
class UNREALMCP_API FUnrealMCPMaterialCommands
{
public:
	FUnrealMCPMaterialCommands();
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> MakeErrorResponse(const FString& Message);

	// Material lifecycle
	TSharedPtr<FJsonObject> HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteMaterial(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetMaterialProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetMaterialInfo(const TSharedPtr<FJsonObject>& Params);

	// Expression nodes
	TSharedPtr<FJsonObject> HandleAddExpression(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteExpression(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListExpressions(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleListPins(const TSharedPtr<FJsonObject>& Params);

	// Connections
	TSharedPtr<FJsonObject> HandleConnectExpressions(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleConnectToOutput(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDisconnectExpression(const TSharedPtr<FJsonObject>& Params);

	// Set expression values
	TSharedPtr<FJsonObject> HandleSetExpressionValue(const TSharedPtr<FJsonObject>& Params);

	// Compile
	TSharedPtr<FJsonObject> HandleCompileMaterial(const TSharedPtr<FJsonObject>& Params);

	// Apply to actor
	TSharedPtr<FJsonObject> HandleApplyMaterialToActor(const TSharedPtr<FJsonObject>& Params);

	// Helpers
	UMaterial* FindMaterial(const FString& AssetPath);
	UMaterialExpression* FindExpression(UMaterial* Mat, const FString& NodeId);
	TSharedPtr<FJsonObject> MakeExpressionJson(UMaterialExpression* Expr);
};
