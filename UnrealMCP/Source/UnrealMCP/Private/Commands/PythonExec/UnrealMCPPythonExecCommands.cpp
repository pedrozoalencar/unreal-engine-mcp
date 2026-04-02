#include "Commands/PythonExec/UnrealMCPPythonExecCommands.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "IPythonScriptPlugin.h"

FUnrealMCPPythonExecCommands::FUnrealMCPPythonExecCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPPythonExecCommands::MakeErrorResponse(const FString& Message)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), false);
	Result->SetStringField(TEXT("error"), Message);
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPPythonExecCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("execute_python"))
	{
		return HandleExecutePython(Params);
	}
	else if (CommandType == TEXT("execute_python_file"))
	{
		return HandleExecutePythonFile(Params);
	}
	return MakeErrorResponse(FString::Printf(TEXT("Unknown python exec command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPPythonExecCommands::HandleExecutePython(const TSharedPtr<FJsonObject>& Params)
{
	FString Code = Params->GetStringField(TEXT("code"));
	if (Code.IsEmpty()) return MakeErrorResponse(TEXT("code is required"));

	// IPythonScriptPlugin::Get() uses FModuleManager::GetModulePtr internally
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();

	if (!PythonPlugin)
	{
		return MakeErrorResponse(TEXT("PythonScriptPlugin is not loaded. Enable PythonScriptPlugin in your .uproject."));
	}

	if (!PythonPlugin->IsPythonAvailable())
	{
		return MakeErrorResponse(TEXT("Python is not available in this editor session."));
	}

	bool bSuccess = PythonPlugin->ExecPythonCommand(*Code);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("message"), bSuccess ? TEXT("Python code executed successfully") : TEXT("Python execution failed"));
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPPythonExecCommands::HandleExecutePythonFile(const TSharedPtr<FJsonObject>& Params)
{
	FString FilePath = Params->GetStringField(TEXT("file_path"));
	if (FilePath.IsEmpty()) return MakeErrorResponse(TEXT("file_path is required"));

	FString Code;
	if (!FFileHelper::LoadFileToString(Code, *FilePath))
	{
		return MakeErrorResponse(FString::Printf(TEXT("Failed to read file: %s"), *FilePath));
	}

	TSharedPtr<FJsonObject> NewParams = MakeShareable(new FJsonObject);
	NewParams->SetStringField(TEXT("code"), Code);
	return HandleExecutePython(NewParams);
}
