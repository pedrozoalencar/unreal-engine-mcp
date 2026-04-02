#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler for Level/Scene management MCP commands.
 * Spawn actors by BP path, get/set properties, take screenshots, manage world.
 */
class UNREALMCP_API FUnrealMCPLevelCommands
{
public:
	FUnrealMCPLevelCommands();
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> MakeErrorResponse(const FString& Message);

	// Spawn
	TSharedPtr<FJsonObject> HandleSpawnBlueprintByPath(const TSharedPtr<FJsonObject>& Params);

	// Actor properties
	TSharedPtr<FJsonObject> HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetComponentProperties(const TSharedPtr<FJsonObject>& Params);

	// Viewport
	TSharedPtr<FJsonObject> HandleGetViewportScreenshot(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetViewportCamera(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleFocusActor(const TSharedPtr<FJsonObject>& Params);

	// Level management
	TSharedPtr<FJsonObject> HandleGetLevelInfo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params);

	// Selection
	TSharedPtr<FJsonObject> HandleGetSelection(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetSelection(const TSharedPtr<FJsonObject>& Params);
};
