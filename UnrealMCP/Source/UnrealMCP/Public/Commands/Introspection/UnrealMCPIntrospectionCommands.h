#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for UE reflection/introspection MCP commands.
 * Inspects UClasses and UEnums at runtime, returning their properties,
 * functions, and enum values as JSON.
 */
class UNREALMCP_API FUnrealMCPIntrospectionCommands
{
public:
	FUnrealMCPIntrospectionCommands();

	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> MakeErrorResponse(const FString& Message);
	TSharedPtr<FJsonObject> HandleInspectClass(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleInspectEnum(const TSharedPtr<FJsonObject>& Params);
};
