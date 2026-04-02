#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Python script execution MCP commands.
 * Executes Python code inside the Unreal Editor via IPythonScriptPlugin.
 */
class UNREALMCP_API FUnrealMCPPythonExecCommands
{
public:
	FUnrealMCPPythonExecCommands();

	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> HandleExecutePython(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleExecutePythonFile(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> MakeErrorResponse(const FString& Message);
};
