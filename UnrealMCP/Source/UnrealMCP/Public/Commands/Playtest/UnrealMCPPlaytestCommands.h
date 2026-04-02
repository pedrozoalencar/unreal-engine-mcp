#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler for Playtest MCP commands.
 * Play/simulate, stop, query play state.
 */
class UNREALMCP_API FUnrealMCPPlaytestCommands
{
public:
	FUnrealMCPPlaytestCommands();
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> MakeErrorResponse(const FString& Message);
	TSharedPtr<FJsonObject> HandlePlaySimulate(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleStop(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleIsPlaying(const TSharedPtr<FJsonObject>& Params);
};
