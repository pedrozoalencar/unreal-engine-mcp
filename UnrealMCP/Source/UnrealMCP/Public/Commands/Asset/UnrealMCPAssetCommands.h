#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler for Asset management MCP commands.
 * List, find, import, duplicate, rename, delete assets.
 */
class UNREALMCP_API FUnrealMCPAssetCommands
{
public:
	FUnrealMCPAssetCommands();
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> MakeErrorResponse(const FString& Message);

	TSharedPtr<FJsonObject> HandleListAssets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleFindAssets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetAssetInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRenameAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleDoesAssetExist(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSaveAsset(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSaveAll(const TSharedPtr<FJsonObject>& Params);

	// Import/Export
	TSharedPtr<FJsonObject> HandleImportAssets(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleExportAssets(const TSharedPtr<FJsonObject>& Params);
};
