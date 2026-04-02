#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler for Transaction/Undo MCP commands.
 * Begin/end transactions, undo, redo operations.
 */
class UNREALMCP_API FUnrealMCPTransactionCommands
{
public:
	FUnrealMCPTransactionCommands();
	TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	TSharedPtr<FJsonObject> MakeErrorResponse(const FString& Message);
	TSharedPtr<FJsonObject> HandleBeginTransaction(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleEndTransaction(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleUndo(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRedo(const TSharedPtr<FJsonObject>& Params);

	int32 ActiveTransactionIndex;
};
