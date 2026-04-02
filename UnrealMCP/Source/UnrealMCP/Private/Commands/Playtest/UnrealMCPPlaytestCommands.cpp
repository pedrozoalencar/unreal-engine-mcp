#include "Commands/Playtest/UnrealMCPPlaytestCommands.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

FUnrealMCPPlaytestCommands::FUnrealMCPPlaytestCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPPlaytestCommands::MakeErrorResponse(const FString& Message)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error"), Message);
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPPlaytestCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("simulate"))
	{
		return HandlePlaySimulate(Params);
	}
	else if (CommandType == TEXT("stop"))
	{
		return HandleStop(Params);
	}
	else if (CommandType == TEXT("is_playing"))
	{
		return HandleIsPlaying(Params);
	}

	return MakeErrorResponse(FString::Printf(TEXT("Unknown playtest command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// HandlePlaySimulate
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPPlaytestCommands::HandlePlaySimulate(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return MakeErrorResponse(TEXT("GEditor is not available"));
	}

	if (GEditor->PlayWorld != nullptr)
	{
		return MakeErrorResponse(TEXT("A play session is already in progress"));
	}

	FRequestPlaySessionParams PlayParams;
	PlayParams.WorldType = EPlaySessionWorldType::SimulateInEditor;
	GEditor->RequestPlaySession(PlayParams);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("mode"), TEXT("simulate_in_editor"));
	Result->SetStringField(TEXT("message"), TEXT("Play session requested in Simulate mode"));
	return Result;
}

// ---------------------------------------------------------------------------
// HandleStop
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPPlaytestCommands::HandleStop(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return MakeErrorResponse(TEXT("GEditor is not available"));
	}

	if (GEditor->PlayWorld == nullptr)
	{
		return MakeErrorResponse(TEXT("No play session is currently active"));
	}

	GEditor->RequestEndPlayMap();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), TEXT("Stop play session requested"));
	return Result;
}

// ---------------------------------------------------------------------------
// HandleIsPlaying
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPPlaytestCommands::HandleIsPlaying(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return MakeErrorResponse(TEXT("GEditor is not available"));
	}

	bool bIsPlaying = GEditor->PlayWorld != nullptr;
	bool bIsSimulating = GEditor->bIsSimulatingInEditor;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("is_playing"), bIsPlaying);
	Result->SetBoolField(TEXT("is_simulating"), bIsSimulating);
	return Result;
}
