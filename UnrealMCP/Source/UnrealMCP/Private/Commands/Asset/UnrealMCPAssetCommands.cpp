#include "Commands/Asset/UnrealMCPAssetCommands.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Engine/Texture2D.h"
#include "Engine/Blueprint.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "AssetImportTask.h"

FUnrealMCPAssetCommands::FUnrealMCPAssetCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::MakeErrorResponse(const FString& Message)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), false);
	Result->SetStringField(TEXT("error"), Message);
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("list_assets"))
	{
		return HandleListAssets(Params);
	}
	else if (CommandType == TEXT("find_assets"))
	{
		return HandleFindAssets(Params);
	}
	else if (CommandType == TEXT("get_asset_info"))
	{
		return HandleGetAssetInfo(Params);
	}
	else if (CommandType == TEXT("duplicate_asset"))
	{
		return HandleDuplicateAsset(Params);
	}
	else if (CommandType == TEXT("rename_asset"))
	{
		return HandleRenameAsset(Params);
	}
	else if (CommandType == TEXT("delete_asset"))
	{
		return HandleDeleteAsset(Params);
	}
	else if (CommandType == TEXT("does_asset_exist"))
	{
		return HandleDoesAssetExist(Params);
	}
	else if (CommandType == TEXT("save_asset"))
	{
		return HandleSaveAsset(Params);
	}
	else if (CommandType == TEXT("save_all"))
	{
		return HandleSaveAll(Params);
	}
	else if (CommandType == TEXT("import_assets"))
	{
		return HandleImportAssets(Params);
	}
	else if (CommandType == TEXT("export_assets"))
	{
		return HandleExportAssets(Params);
	}

	return MakeErrorResponse(FString::Printf(TEXT("Unknown asset command: %s"), *CommandType));
}

// ─────────────────────────────────────────────────────────────────────────────
// HandleListAssets
// ─────────────────────────────────────────────────────────────────────────────
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleListAssets(const TSharedPtr<FJsonObject>& Params)
{
	FString DirectoryPath = TEXT("/Game");
	if (Params->HasField(TEXT("directory_path")))
	{
		DirectoryPath = Params->GetStringField(TEXT("directory_path"));
	}

	FString ClassFilter;
	if (Params->HasField(TEXT("class_filter")))
	{
		ClassFilter = Params->GetStringField(TEXT("class_filter"));
	}

	bool bRecursive = true;
	if (Params->HasField(TEXT("recursive")))
	{
		bRecursive = Params->GetBoolField(TEXT("recursive"));
	}

	TArray<FString> AssetPaths = UEditorAssetLibrary::ListAssets(DirectoryPath, bRecursive);

	TArray<TSharedPtr<FJsonValue>> AssetsArray;
	for (const FString& AssetPath : AssetPaths)
	{
		FAssetData AssetData = UEditorAssetLibrary::FindAssetData(AssetPath);
		if (!AssetData.IsValid())
		{
			continue;
		}

		FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();

		// Apply class filter if specified
		if (!ClassFilter.IsEmpty() && !ClassName.Contains(ClassFilter))
		{
			continue;
		}

		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		AssetObj->SetStringField(TEXT("class"), ClassName);

		AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("count"), AssetsArray.Num());
	Result->SetArrayField(TEXT("assets"), AssetsArray);
	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// HandleFindAssets
// ─────────────────────────────────────────────────────────────────────────────
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleFindAssets(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params->HasField(TEXT("search_text")))
	{
		return MakeErrorResponse(TEXT("Missing required parameter: search_text"));
	}

	FString SearchText = Params->GetStringField(TEXT("search_text"));

	FString ClassNameFilter;
	if (Params->HasField(TEXT("class_name")))
	{
		ClassNameFilter = Params->GetStringField(TEXT("class_name"));
	}

	// Use the Asset Registry for an efficient search
	IAssetRegistry& AssetRegistry = *IAssetRegistry::Get();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName(TEXT("/Game")));

	if (!ClassNameFilter.IsEmpty())
	{
		// Build a top-level asset path from the class name.
		// Common engine classes live under /Script/Engine.
		FTopLevelAssetPath ClassPath(TEXT("/Script/Engine"), *ClassNameFilter);
		Filter.ClassPaths.Add(ClassPath);
	}

	TArray<FAssetData> AllAssets;
	AssetRegistry.GetAssets(Filter, AllAssets);

	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	for (const FAssetData& AssetData : AllAssets)
	{
		FString AssetName = AssetData.AssetName.ToString();
		FString PackagePath = AssetData.PackageName.ToString();

		// Filter by search text (case-insensitive)
		if (!AssetName.Contains(SearchText, ESearchCase::IgnoreCase) &&
			!PackagePath.Contains(SearchText, ESearchCase::IgnoreCase))
		{
			continue;
		}

		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		AssetObj->SetStringField(TEXT("name"), AssetName);
		AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
		AssetObj->SetStringField(TEXT("package_path"), PackagePath);

		ResultsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("count"), ResultsArray.Num());
	Result->SetArrayField(TEXT("assets"), ResultsArray);
	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// HandleGetAssetInfo
// ─────────────────────────────────────────────────────────────────────────────
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleGetAssetInfo(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params->HasField(TEXT("asset_path")))
	{
		return MakeErrorResponse(TEXT("Missing required parameter: asset_path"));
	}

	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		return MakeErrorResponse(FString::Printf(TEXT("Asset does not exist: %s"), *AssetPath));
	}

	FAssetData AssetData = UEditorAssetLibrary::FindAssetData(AssetPath);
	if (!AssetData.IsValid())
	{
		return MakeErrorResponse(FString::Printf(TEXT("Could not find asset data for: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();
	InfoObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
	InfoObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
	InfoObj->SetStringField(TEXT("object_path"), AssetData.GetObjectPathString());
	InfoObj->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());
	InfoObj->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());

	// Disk size
	int64 DiskSize = AssetData.GetTagValueRef<int64>(FName("Size"));
	// If tag not available, try to get from the package file
	if (DiskSize == 0)
	{
		FString PackageFilename;
		if (FPackageName::DoesPackageExist(AssetData.PackageName.ToString(), &PackageFilename))
		{
			DiskSize = IFileManager::Get().FileSize(*PackageFilename);
		}
	}
	InfoObj->SetNumberField(TEXT("disk_size_bytes"), static_cast<double>(DiskSize));

	// Is dirty (has unsaved changes)
	UPackage* Package = AssetData.GetPackage();
	bool bIsDirty = Package ? Package->IsDirty() : false;
	InfoObj->SetBoolField(TEXT("is_dirty"), bIsDirty);

	// Check for referencers
	TArray<FName> Referencers;
	IAssetRegistry& AssetRegistry = *IAssetRegistry::Get();
	AssetRegistry.GetReferencers(AssetData.PackageName, Referencers);
	InfoObj->SetBoolField(TEXT("has_referencers"), Referencers.Num() > 0);
	InfoObj->SetNumberField(TEXT("referencer_count"), Referencers.Num());

	// Check for dependencies
	TArray<FName> Dependencies;
	AssetRegistry.GetDependencies(AssetData.PackageName, Dependencies);
	InfoObj->SetNumberField(TEXT("dependency_count"), Dependencies.Num());

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetObjectField(TEXT("asset_info"), InfoObj);
	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// HandleDuplicateAsset
// ─────────────────────────────────────────────────────────────────────────────
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleDuplicateAsset(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params->HasField(TEXT("source_path")) || !Params->HasField(TEXT("destination_path")))
	{
		return MakeErrorResponse(TEXT("Missing required parameters: source_path and destination_path"));
	}

	FString SourcePath = Params->GetStringField(TEXT("source_path"));
	FString DestinationPath = Params->GetStringField(TEXT("destination_path"));

	if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
	{
		return MakeErrorResponse(FString::Printf(TEXT("Source asset does not exist: %s"), *SourcePath));
	}

	UObject* DuplicatedAsset = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath);
	if (!DuplicatedAsset)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Failed to duplicate asset from '%s' to '%s'"), *SourcePath, *DestinationPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("source_path"), SourcePath);
	Result->SetStringField(TEXT("destination_path"), DestinationPath);
	Result->SetStringField(TEXT("duplicated_asset_class"), DuplicatedAsset->GetClass()->GetName());
	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// HandleRenameAsset
// ─────────────────────────────────────────────────────────────────────────────
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleRenameAsset(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params->HasField(TEXT("source_path")) || !Params->HasField(TEXT("destination_path")))
	{
		return MakeErrorResponse(TEXT("Missing required parameters: source_path and destination_path"));
	}

	FString SourcePath = Params->GetStringField(TEXT("source_path"));
	FString DestinationPath = Params->GetStringField(TEXT("destination_path"));

	if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
	{
		return MakeErrorResponse(FString::Printf(TEXT("Source asset does not exist: %s"), *SourcePath));
	}

	bool bSuccess = UEditorAssetLibrary::RenameAsset(SourcePath, DestinationPath);
	if (!bSuccess)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Failed to rename asset from '%s' to '%s'"), *SourcePath, *DestinationPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("old_path"), SourcePath);
	Result->SetStringField(TEXT("new_path"), DestinationPath);
	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// HandleDeleteAsset
// ─────────────────────────────────────────────────────────────────────────────
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params->HasField(TEXT("asset_path")))
	{
		return MakeErrorResponse(TEXT("Missing required parameter: asset_path"));
	}

	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		return MakeErrorResponse(FString::Printf(TEXT("Asset does not exist: %s"), *AssetPath));
	}

	bool bSuccess = UEditorAssetLibrary::DeleteAsset(AssetPath);
	if (!bSuccess)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Failed to delete asset: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("deleted_path"), AssetPath);
	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// HandleDoesAssetExist
// ─────────────────────────────────────────────────────────────────────────────
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleDoesAssetExist(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params->HasField(TEXT("asset_path")))
	{
		return MakeErrorResponse(TEXT("Missing required parameter: asset_path"));
	}

	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	bool bExists = UEditorAssetLibrary::DoesAssetExist(AssetPath);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("exists"), bExists);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// HandleSaveAsset
// ─────────────────────────────────────────────────────────────────────────────
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSaveAsset(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params->HasField(TEXT("asset_path")))
	{
		return MakeErrorResponse(TEXT("Missing required parameter: asset_path"));
	}

	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		return MakeErrorResponse(FString::Printf(TEXT("Asset does not exist: %s"), *AssetPath));
	}

	bool bSuccess = UEditorAssetLibrary::SaveAsset(AssetPath, /*bOnlyIfIsDirty=*/ false);
	if (!bSuccess)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Failed to save asset: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("saved_path"), AssetPath);
	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// HandleSaveAll
// ─────────────────────────────────────────────────────────────────────────────
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleSaveAll(const TSharedPtr<FJsonObject>& Params)
{
	// Collect dirty packages before saving so we can report what was saved
	TArray<UPackage*> DirtyPackages;
	TArray<FString> SavedAssets;

	for (TObjectIterator<UPackage> It; It; ++It)
	{
		UPackage* Package = *It;
		if (Package && Package->IsDirty() && !Package->HasAnyFlags(RF_Transient))
		{
			FString PackageName = Package->GetName();
			if (PackageName.StartsWith(TEXT("/Game")) ||
				PackageName.StartsWith(TEXT("/Engine")) ||
				FPackageName::IsValidLongPackageName(PackageName))
			{
				DirtyPackages.Add(Package);
			}
		}
	}

	// Collect all dirty assets and save them
	TArray<UObject*> AssetsToSave;
	for (UPackage* Package : DirtyPackages)
	{
		UObject* Asset = Package->FindAssetInPackage();
		if (Asset) AssetsToSave.Add(Asset);
	}
	bool bSuccess = AssetsToSave.Num() > 0 ? UEditorAssetLibrary::SaveLoadedAssets(AssetsToSave, true) : true;

	// Report which packages were dirty (and thus candidates for saving)
	for (UPackage* Package : DirtyPackages)
	{
		if (Package)
		{
			SavedAssets.Add(Package->GetName());
		}
	}

	TArray<TSharedPtr<FJsonValue>> SavedArray;
	for (const FString& Name : SavedAssets)
	{
		SavedArray.Add(MakeShared<FJsonValueString>(Name));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetNumberField(TEXT("saved_count"), SavedAssets.Num());
	Result->SetArrayField(TEXT("saved_assets"), SavedArray);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleImportAssets
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleImportAssets(const TSharedPtr<FJsonObject>& Params)
{
	const TArray<TSharedPtr<FJsonValue>>* FilesArray;
	if (!Params->TryGetArrayField(TEXT("files"), FilesArray))
	{
		return MakeErrorResponse(TEXT("Missing required parameter: files (array of file paths)"));
	}

	FString DestPath = TEXT("/Game");
	if (Params->HasField(TEXT("destination_path")))
	{
		DestPath = Params->GetStringField(TEXT("destination_path"));
	}

	bool bReplace = false;
	if (Params->HasField(TEXT("replace_existing")))
	{
		bReplace = Params->GetBoolField(TEXT("replace_existing"));
	}

	TArray<UAssetImportTask*> ImportTasks;
	for (const auto& FileVal : *FilesArray)
	{
		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = FileVal->AsString();
		Task->DestinationPath = DestPath;
		Task->bAutomated = true;
		Task->bReplaceExisting = bReplace;
		Task->bSave = true;
		ImportTasks.Add(Task);
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.ImportAssetTasks(ImportTasks);

	// Collect results
	TArray<TSharedPtr<FJsonValue>> ImportedArray;
	TArray<TSharedPtr<FJsonValue>> FailedArray;

	for (UAssetImportTask* Task : ImportTasks)
	{
		if (Task->GetObjects().Num() > 0)
		{
			for (UObject* ImportedObj : Task->GetObjects())
			{
				TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
				Obj->SetStringField(TEXT("source_file"), Task->Filename);
				Obj->SetStringField(TEXT("asset_path"), ImportedObj->GetPathName());
				Obj->SetStringField(TEXT("class"), ImportedObj->GetClass()->GetName());
				ImportedArray.Add(MakeShared<FJsonValueObject>(Obj));
			}
		}
		else
		{
			FailedArray.Add(MakeShared<FJsonValueString>(Task->Filename));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), FailedArray.Num() == 0);
	Result->SetNumberField(TEXT("imported_count"), ImportedArray.Num());
	Result->SetArrayField(TEXT("imported"), ImportedArray);

	if (FailedArray.Num() > 0)
	{
		Result->SetArrayField(TEXT("failed"), FailedArray);
	}

	return Result;
}

// ---------------------------------------------------------------------------
// HandleExportAssets
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleExportAssets(const TSharedPtr<FJsonObject>& Params)
{
	const TArray<TSharedPtr<FJsonValue>>* AssetPathsArray;
	if (!Params->TryGetArrayField(TEXT("asset_paths"), AssetPathsArray))
	{
		return MakeErrorResponse(TEXT("Missing required parameter: asset_paths (array of asset paths)"));
	}

	FString OutputDir;
	if (!Params->TryGetStringField(TEXT("output_dir"), OutputDir))
	{
		return MakeErrorResponse(TEXT("Missing required parameter: output_dir"));
	}

	TArray<UObject*> AssetsToExport;
	TArray<FString> NotFoundPaths;

	for (const auto& PathVal : *AssetPathsArray)
	{
		FString AssetPath = PathVal->AsString();
		UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
		if (Asset)
		{
			AssetsToExport.Add(Asset);
		}
		else
		{
			NotFoundPaths.Add(AssetPath);
		}
	}

	if (AssetsToExport.Num() == 0)
	{
		return MakeErrorResponse(TEXT("No valid assets found to export"));
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TArray<FString> ExportedFiles;
	AssetTools.ExportAssets(AssetsToExport, OutputDir);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("exported_count"), AssetsToExport.Num());
	Result->SetStringField(TEXT("output_dir"), OutputDir);

	TArray<TSharedPtr<FJsonValue>> ExportedArr;
	for (UObject* Asset : AssetsToExport)
	{
		ExportedArr.Add(MakeShared<FJsonValueString>(Asset->GetPathName()));
	}
	Result->SetArrayField(TEXT("exported_assets"), ExportedArr);

	if (NotFoundPaths.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> NotFoundArr;
		for (const FString& Path : NotFoundPaths)
		{
			NotFoundArr.Add(MakeShared<FJsonValueString>(Path));
		}
		Result->SetArrayField(TEXT("not_found"), NotFoundArr);
	}

	return Result;
}
