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

	// Write user code to a temp file to avoid escaping issues
	FString TempCodeFile = FPaths::ProjectSavedDir() / TEXT("mcp_code.py");
	FString TempOutputFile = FPaths::ProjectSavedDir() / TEXT("mcp_output.txt");
	FString TempErrorFile = FPaths::ProjectSavedDir() / TEXT("mcp_errors.txt");

	// Normalize paths to forward slashes for Python
	TempCodeFile = TempCodeFile.Replace(TEXT("\\"), TEXT("/"));
	TempOutputFile = TempOutputFile.Replace(TEXT("\\"), TEXT("/"));
	TempErrorFile = TempErrorFile.Replace(TEXT("\\"), TEXT("/"));

	FFileHelper::SaveStringToFile(Code, *TempCodeFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	// Build wrapper script that captures stdout/stderr
	FString WrapperCode = FString::Printf(TEXT(
		"import sys, io\n"
		"_mcp_out, _mcp_err = io.StringIO(), io.StringIO()\n"
		"_mcp_old_out, _mcp_old_err = sys.stdout, sys.stderr\n"
		"sys.stdout, sys.stderr = _mcp_out, _mcp_err\n"
		"try:\n"
		"    with open(r'%s', 'r', encoding='utf-8') as _mcp_f:\n"
		"        exec(compile(_mcp_f.read(), '<mcp>', 'exec'))\n"
		"except Exception as _mcp_e:\n"
		"    print(f'Error: {_mcp_e}', file=_mcp_err)\n"
		"finally:\n"
		"    sys.stdout, sys.stderr = _mcp_old_out, _mcp_old_err\n"
		"with open(r'%s', 'w', encoding='utf-8') as _mcp_f:\n"
		"    _mcp_f.write(_mcp_out.getvalue())\n"
		"with open(r'%s', 'w', encoding='utf-8') as _mcp_f:\n"
		"    _mcp_f.write(_mcp_err.getvalue())\n"
	), *TempCodeFile, *TempOutputFile, *TempErrorFile);

	bool bSuccess = PythonPlugin->ExecPythonCommand(*WrapperCode);

	// Read captured output
	FString CapturedOutput, CapturedErrors;
	FFileHelper::LoadFileToString(CapturedOutput, *TempOutputFile);
	FFileHelper::LoadFileToString(CapturedErrors, *TempErrorFile);

	// If the wrapper itself failed and we have errors captured, treat as failure
	if (!CapturedErrors.IsEmpty())
	{
		bSuccess = false;
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("output"), CapturedOutput);
	Result->SetStringField(TEXT("errors"), CapturedErrors);
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
