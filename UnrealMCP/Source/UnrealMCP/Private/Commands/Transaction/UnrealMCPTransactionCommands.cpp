#include "Commands/Transaction/UnrealMCPTransactionCommands.h"
#include "Editor.h"

FUnrealMCPTransactionCommands::FUnrealMCPTransactionCommands()
	: ActiveTransactionIndex(INDEX_NONE)
{
}

TSharedPtr<FJsonObject> FUnrealMCPTransactionCommands::MakeErrorResponse(const FString& Message)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error"), Message);
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPTransactionCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("begin_transaction"))
	{
		return HandleBeginTransaction(Params);
	}
	else if (CommandType == TEXT("end_transaction"))
	{
		return HandleEndTransaction(Params);
	}
	else if (CommandType == TEXT("undo"))
	{
		return HandleUndo(Params);
	}
	else if (CommandType == TEXT("redo"))
	{
		return HandleRedo(Params);
	}

	return MakeErrorResponse(FString::Printf(TEXT("Unknown transaction command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// HandleBeginTransaction
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPTransactionCommands::HandleBeginTransaction(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return MakeErrorResponse(TEXT("GEditor is not available"));
	}

	FString Label = TEXT("MCP Transaction");
	if (Params->HasField(TEXT("label")))
	{
		Label = Params->GetStringField(TEXT("label"));
	}

	ActiveTransactionIndex = GEditor->BeginTransaction(FText::FromString(TEXT("MCP: ") + Label));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("transaction_index"), ActiveTransactionIndex);
	Result->SetStringField(TEXT("label"), Label);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleEndTransaction
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPTransactionCommands::HandleEndTransaction(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return MakeErrorResponse(TEXT("GEditor is not available"));
	}

	bool bCommit = true;
	if (Params->HasField(TEXT("commit")))
	{
		bCommit = Params->GetBoolField(TEXT("commit"));
	}

	if (bCommit)
	{
		GEditor->EndTransaction();
	}
	else
	{
		GEditor->CancelTransaction(ActiveTransactionIndex);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("committed"), bCommit);
	Result->SetNumberField(TEXT("transaction_index"), ActiveTransactionIndex);

	ActiveTransactionIndex = INDEX_NONE;
	return Result;
}

// ---------------------------------------------------------------------------
// HandleUndo
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPTransactionCommands::HandleUndo(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return MakeErrorResponse(TEXT("GEditor is not available"));
	}

	bool bSuccess = GEditor->UndoTransaction();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bSuccess);
	if (!bSuccess)
	{
		Result->SetStringField(TEXT("message"), TEXT("Nothing to undo"));
	}
	return Result;
}

// ---------------------------------------------------------------------------
// HandleRedo
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPTransactionCommands::HandleRedo(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return MakeErrorResponse(TEXT("GEditor is not available"));
	}

	bool bSuccess = GEditor->RedoTransaction();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bSuccess);
	if (!bSuccess)
	{
		Result->SetStringField(TEXT("message"), TEXT("Nothing to redo"));
	}
	return Result;
}
