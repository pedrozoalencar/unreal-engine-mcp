#include "Commands/Level/UnrealMCPLevelCommands.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "LevelEditorViewport.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "UObject/PropertyIterator.h"
#include "EditorViewportClient.h"
#include "Engine/Selection.h"

FUnrealMCPLevelCommands::FUnrealMCPLevelCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::MakeErrorResponse(const FString& Message)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error"), Message);
	return Response;
}

TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("spawn_blueprint_by_path"))
	{
		return HandleSpawnBlueprintByPath(Params);
	}
	else if (CommandType == TEXT("get_actor_properties"))
	{
		return HandleGetActorProperties(Params);
	}
	else if (CommandType == TEXT("set_actor_property"))
	{
		return HandleSetActorProperty(Params);
	}
	else if (CommandType == TEXT("get_component_properties"))
	{
		return HandleGetComponentProperties(Params);
	}
	else if (CommandType == TEXT("get_viewport_screenshot"))
	{
		return HandleGetViewportScreenshot(Params);
	}
	else if (CommandType == TEXT("set_viewport_camera"))
	{
		return HandleSetViewportCamera(Params);
	}
	else if (CommandType == TEXT("focus_actor"))
	{
		return HandleFocusActor(Params);
	}
	else if (CommandType == TEXT("get_level_info"))
	{
		return HandleGetLevelInfo(Params);
	}
	else if (CommandType == TEXT("save_current_level"))
	{
		return HandleSaveCurrentLevel(Params);
	}
	else if (CommandType == TEXT("get_selection"))
	{
		return HandleGetSelection(Params);
	}
	else if (CommandType == TEXT("set_selection"))
	{
		return HandleSetSelection(Params);
	}

	return MakeErrorResponse(FString::Printf(TEXT("Unknown level command: %s"), *CommandType));
}

// ---------------------------------------------------------------------------
// Helper: find actor by name (label or internal name) in the editor world
// ---------------------------------------------------------------------------
static AActor* FindActorByName(const FString& ActorName)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

	for (AActor* Actor : AllActors)
	{
		if (!Actor) continue;

		// Match against the actor label (display name) or the internal object name
		if (Actor->GetActorNameOrLabel() == ActorName || Actor->GetName() == ActorName)
		{
			return Actor;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Helper: read a JSON [x,y,z] array into an FVector
// ---------------------------------------------------------------------------
static FVector JsonArrayToVector(const TArray<TSharedPtr<FJsonValue>>& Arr, const FVector& Default = FVector::ZeroVector)
{
	if (Arr.Num() >= 3)
	{
		return FVector(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
	}
	return Default;
}

// ---------------------------------------------------------------------------
// HandleSpawnBlueprintByPath
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleSpawnBlueprintByPath(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath;
	if (!Params->TryGetStringField(TEXT("blueprint_path"), BlueprintPath))
	{
		return MakeErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeErrorResponse(TEXT("No editor world available"));
	}

	// Normalise the path: ensure it ends with the class suffix Unreal expects for LoadObject
	FString ObjectPath = BlueprintPath;
	if (!ObjectPath.EndsWith(TEXT("_C")) && !ObjectPath.Contains(TEXT(".")))
	{
		// Convert "/Game/Foo/BP_Bar" -> "/Game/Foo/BP_Bar.BP_Bar"
		FString LeafName = FPaths::GetCleanFilename(ObjectPath);
		ObjectPath = ObjectPath + TEXT(".") + LeafName;
	}

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!Blueprint)
	{
		// Try loading as asset via EditorAssetLibrary as fallback
		UObject* Loaded = UEditorAssetLibrary::LoadAsset(BlueprintPath);
		Blueprint = Cast<UBlueprint>(Loaded);
	}

	if (!Blueprint)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Could not load blueprint at path: %s"), *BlueprintPath));
	}

	UClass* SpawnClass = Blueprint->GeneratedClass;
	if (!SpawnClass)
	{
		return MakeErrorResponse(TEXT("Blueprint has no GeneratedClass"));
	}

	// Optional transform parameters
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Scale = FVector::OneVector;

	if (Params->HasField(TEXT("location")))
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Params->TryGetArrayField(TEXT("location"), Arr))
		{
			Location = JsonArrayToVector(*Arr);
		}
	}
	if (Params->HasField(TEXT("rotation")))
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Params->TryGetArrayField(TEXT("rotation"), Arr))
		{
			if (Arr->Num() >= 3)
			{
				Rotation = FRotator((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
			}
		}
	}
	if (Params->HasField(TEXT("scale")))
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Params->TryGetArrayField(TEXT("scale"), Arr))
		{
			Scale = JsonArrayToVector(*Arr, FVector::OneVector);
		}
	}

	FActorSpawnParameters SpawnParams;
	FString DesiredName;
	if (Params->TryGetStringField(TEXT("name"), DesiredName) && !DesiredName.IsEmpty())
	{
		SpawnParams.Name = *DesiredName;
	}
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	FTransform SpawnTransform(Rotation.Quaternion(), Location, Scale);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* NewActor = World->SpawnActor(SpawnClass, &SpawnTransform, SpawnParams);
	if (!NewActor)
	{
		return MakeErrorResponse(TEXT("SpawnActor failed"));
	}

	// Build response
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), NewActor->GetName());
	Result->SetStringField(TEXT("actor_label"), NewActor->GetActorNameOrLabel());
	Result->SetStringField(TEXT("class"), SpawnClass->GetName());

	TArray<TSharedPtr<FJsonValue>> LocArr;
	FVector ActorLoc = NewActor->GetActorLocation();
	LocArr.Add(MakeShared<FJsonValueNumber>(ActorLoc.X));
	LocArr.Add(MakeShared<FJsonValueNumber>(ActorLoc.Y));
	LocArr.Add(MakeShared<FJsonValueNumber>(ActorLoc.Z));
	Result->SetArrayField(TEXT("location"), LocArr);

	return Result;
}

// ---------------------------------------------------------------------------
// Helper: collect UPROPERTY values from a UObject into a JSON array
// ---------------------------------------------------------------------------
static TArray<TSharedPtr<FJsonValue>> CollectProperties(UObject* Object)
{
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	if (!Object) return PropertiesArray;

	// Categories we consider "important" for the MCP caller
	static const TSet<FString> ImportantCategories = {
		TEXT("Transform"), TEXT("Rendering"), TEXT("Physics"),
		TEXT("Collision"), TEXT("Lighting"), TEXT("Actor"),
		TEXT("Replication"), TEXT("LOD"), TEXT("Mesh"),
		TEXT("Material"), TEXT("Animation"), TEXT("Movement"),
		TEXT("Gameplay"), TEXT("Default"), TEXT("")
	};

	for (TFieldIterator<FProperty> PropIt(Object->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property) continue;

		// Skip deprecated / transient / editor-internal properties
		if (Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient))
		{
			continue;
		}

		// Only include properties with edit/visible flags (i.e. those shown in details panel)
		if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			continue;
		}

		// Optional category filter
		FString Category = Property->GetMetaData(TEXT("Category"));
		// Allow empty category as well (many built-in properties have none)

		FString ValueStr;
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
		Property->ExportText_Direct(ValueStr, ValuePtr, ValuePtr, Object, PPF_None);

		// Truncate very long values to keep the response manageable
		if (ValueStr.Len() > 512)
		{
			ValueStr = ValueStr.Left(509) + TEXT("...");
		}

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Property->GetName());
		PropObj->SetStringField(TEXT("type"), Property->GetCPPType());
		PropObj->SetStringField(TEXT("value"), ValueStr);
		PropObj->SetStringField(TEXT("category"), Category);
		PropertiesArray.Add(MakeShared<FJsonValueObject>(PropObj));
	}

	return PropertiesArray;
}

// ---------------------------------------------------------------------------
// HandleGetActorProperties
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return MakeErrorResponse(TEXT("Missing 'actor_name' parameter"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("actor_name"), Actor->GetName());
	Result->SetStringField(TEXT("actor_label"), Actor->GetActorNameOrLabel());
	Result->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

	// Transform
	FVector Loc = Actor->GetActorLocation();
	FRotator Rot = Actor->GetActorRotation();
	FVector Scl = Actor->GetActorScale3D();

	auto VecToArr = [](const FVector& V) -> TArray<TSharedPtr<FJsonValue>>
	{
		TArray<TSharedPtr<FJsonValue>> A;
		A.Add(MakeShared<FJsonValueNumber>(V.X));
		A.Add(MakeShared<FJsonValueNumber>(V.Y));
		A.Add(MakeShared<FJsonValueNumber>(V.Z));
		return A;
	};

	Result->SetArrayField(TEXT("location"), VecToArr(Loc));
	Result->SetArrayField(TEXT("rotation"), VecToArr(FVector(Rot.Pitch, Rot.Yaw, Rot.Roll)));
	Result->SetArrayField(TEXT("scale"), VecToArr(Scl));

	// Components
	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	TInlineComponentArray<UActorComponent*> Components;
	Actor->GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		if (!Comp) continue;
		TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
		CompObj->SetStringField(TEXT("name"), Comp->GetName());
		CompObj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
		ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
	}
	Result->SetArrayField(TEXT("components"), ComponentsArray);

	// UPROPERTY values
	Result->SetArrayField(TEXT("properties"), CollectProperties(Actor));

	return Result;
}

// ---------------------------------------------------------------------------
// HandleSetActorProperty
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return MakeErrorResponse(TEXT("Missing 'actor_name' parameter"));
	}

	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return MakeErrorResponse(TEXT("Missing 'property_name' parameter"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	// Special-case common transform properties that map to setter functions
	if (PropertyName.Equals(TEXT("location"), ESearchCase::IgnoreCase) ||
		PropertyName.Equals(TEXT("RelativeLocation"), ESearchCase::IgnoreCase))
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Params->TryGetArrayField(TEXT("value"), Arr))
		{
			Actor->SetActorLocation(JsonArrayToVector(*Arr));
			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetBoolField(TEXT("success"), true);
			R->SetStringField(TEXT("property"), PropertyName);
			return R;
		}
		return MakeErrorResponse(TEXT("'value' must be an [x,y,z] array for location"));
	}

	if (PropertyName.Equals(TEXT("rotation"), ESearchCase::IgnoreCase) ||
		PropertyName.Equals(TEXT("RelativeRotation"), ESearchCase::IgnoreCase))
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Params->TryGetArrayField(TEXT("value"), Arr))
		{
			if (Arr->Num() >= 3)
			{
				Actor->SetActorRotation(FRotator((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber()));
			}
			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetBoolField(TEXT("success"), true);
			R->SetStringField(TEXT("property"), PropertyName);
			return R;
		}
		return MakeErrorResponse(TEXT("'value' must be an [pitch,yaw,roll] array for rotation"));
	}

	if (PropertyName.Equals(TEXT("scale"), ESearchCase::IgnoreCase) ||
		PropertyName.Equals(TEXT("RelativeScale3D"), ESearchCase::IgnoreCase))
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Params->TryGetArrayField(TEXT("value"), Arr))
		{
			Actor->SetActorScale3D(JsonArrayToVector(*Arr, FVector::OneVector));
			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetBoolField(TEXT("success"), true);
			R->SetStringField(TEXT("property"), PropertyName);
			return R;
		}
		return MakeErrorResponse(TEXT("'value' must be an [x,y,z] array for scale"));
	}

	// Generic property set via reflection
	FProperty* Property = Actor->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Property '%s' not found on actor '%s'"), *PropertyName, *ActorName));
	}

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Actor);

	// Determine the value to import
	FString ValueString;

	// If value is an array (vector-like), format as "(X=...,Y=...,Z=...)" for FVector properties
	const TArray<TSharedPtr<FJsonValue>>* ArrVal;
	if (Params->TryGetArrayField(TEXT("value"), ArrVal))
	{
		if (ArrVal->Num() >= 3)
		{
			// Try FVector format
			ValueString = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"),
				(*ArrVal)[0]->AsNumber(), (*ArrVal)[1]->AsNumber(), (*ArrVal)[2]->AsNumber());
		}
		else
		{
			// Fallback: comma-separated
			TArray<FString> Parts;
			for (auto& V : *ArrVal)
			{
				Parts.Add(FString::SanitizeFloat(V->AsNumber()));
			}
			ValueString = FString::Join(Parts, TEXT(","));
		}
	}
	else if (Params->HasField(TEXT("value")))
	{
		// Try as bool
		bool BoolVal;
		if (Params->TryGetBoolField(TEXT("value"), BoolVal))
		{
			ValueString = BoolVal ? TEXT("True") : TEXT("False");
		}
		else
		{
			double NumVal;
			if (Params->TryGetNumberField(TEXT("value"), NumVal))
			{
				ValueString = FString::SanitizeFloat(NumVal);
			}
			else
			{
				Params->TryGetStringField(TEXT("value"), ValueString);
			}
		}
	}
	else
	{
		return MakeErrorResponse(TEXT("Missing 'value' parameter"));
	}

	// Notify the actor we are about to change a property (for undo/redo and editor updates)
	Actor->PreEditChange(Property);

	const TCHAR* ImportResult = Property->ImportText_Direct(*ValueString, ValuePtr, Actor, PPF_None);

	if (!ImportResult)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Failed to set property '%s' to '%s'"), *PropertyName, *ValueString));
	}

	FPropertyChangedEvent ChangedEvent(Property);
	Actor->PostEditChangeProperty(ChangedEvent);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("property"), PropertyName);
	Result->SetStringField(TEXT("value_set"), ValueString);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleGetComponentProperties
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleGetComponentProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return MakeErrorResponse(TEXT("Missing 'actor_name' parameter"));
	}

	FString ComponentName;
	FString ComponentClass;
	Params->TryGetStringField(TEXT("component_name"), ComponentName);
	Params->TryGetStringField(TEXT("component_class"), ComponentClass);

	if (ComponentName.IsEmpty() && ComponentClass.IsEmpty())
	{
		return MakeErrorResponse(TEXT("Must provide 'component_name' or 'component_class' parameter"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	UActorComponent* FoundComp = nullptr;
	TInlineComponentArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		if (!Comp) continue;

		bool bNameMatch = ComponentName.IsEmpty() || Comp->GetName() == ComponentName;
		bool bClassMatch = ComponentClass.IsEmpty() || Comp->GetClass()->GetName() == ComponentClass;

		if (bNameMatch && bClassMatch)
		{
			FoundComp = Comp;
			break;
		}
	}

	if (!FoundComp)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Component not found on actor '%s' (name='%s', class='%s')"),
			*ActorName, *ComponentName, *ComponentClass));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("component_name"), FoundComp->GetName());
	Result->SetStringField(TEXT("component_class"), FoundComp->GetClass()->GetName());

	// If it is a SceneComponent, include relative transform
	if (USceneComponent* SceneComp = Cast<USceneComponent>(FoundComp))
	{
		auto VecToArr = [](const FVector& V) -> TArray<TSharedPtr<FJsonValue>>
		{
			TArray<TSharedPtr<FJsonValue>> A;
			A.Add(MakeShared<FJsonValueNumber>(V.X));
			A.Add(MakeShared<FJsonValueNumber>(V.Y));
			A.Add(MakeShared<FJsonValueNumber>(V.Z));
			return A;
		};

		FVector RelLoc = SceneComp->GetRelativeLocation();
		FRotator RelRot = SceneComp->GetRelativeRotation();
		FVector RelScl = SceneComp->GetRelativeScale3D();

		Result->SetArrayField(TEXT("relative_location"), VecToArr(RelLoc));
		Result->SetArrayField(TEXT("relative_rotation"), VecToArr(FVector(RelRot.Pitch, RelRot.Yaw, RelRot.Roll)));
		Result->SetArrayField(TEXT("relative_scale"), VecToArr(RelScl));
	}

	Result->SetArrayField(TEXT("properties"), CollectProperties(FoundComp));
	return Result;
}

// ---------------------------------------------------------------------------
// HandleGetViewportScreenshot
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleGetViewportScreenshot(const TSharedPtr<FJsonObject>& Params)
{
	FString FilePath;
	if (!Params->TryGetStringField(TEXT("file_path"), FilePath))
	{
		return MakeErrorResponse(TEXT("Missing 'file_path' parameter"));
	}

	// Find the active level viewport
	FViewport* Viewport = nullptr;

	if (GEditor && GEditor->GetActiveViewport())
	{
		Viewport = GEditor->GetActiveViewport();
	}
	else if (GCurrentLevelEditingViewportClient)
	{
		Viewport = GCurrentLevelEditingViewportClient->Viewport;
	}

	if (!Viewport)
	{
		return MakeErrorResponse(TEXT("No active editor viewport found"));
	}

	// Read pixels from the viewport
	TArray<FColor> Bitmap;
	if (!Viewport->ReadPixels(Bitmap))
	{
		return MakeErrorResponse(TEXT("Failed to read pixels from viewport"));
	}

	int32 Width = Viewport->GetSizeXY().X;
	int32 Height = Viewport->GetSizeXY().Y;

	if (Width <= 0 || Height <= 0 || Bitmap.Num() == 0)
	{
		return MakeErrorResponse(TEXT("Viewport has invalid dimensions or empty bitmap"));
	}

	// Compress to PNG
	TArray64<uint8> PngData;
	FImageUtils::PNGCompressImageArray(Width, Height, Bitmap, PngData);

	if (PngData.Num() == 0)
	{
		return MakeErrorResponse(TEXT("Failed to compress screenshot to PNG"));
	}

	// Write to file - convert TArray64 to TArray for SaveArrayToFile
	TArray<uint8> PngDataSmall;
	PngDataSmall.Append(PngData.GetData(), static_cast<int32>(PngData.Num()));
	if (!FFileHelper::SaveArrayToFile(PngDataSmall, *FilePath))
	{
		return MakeErrorResponse(FString::Printf(TEXT("Failed to save screenshot to: %s"), *FilePath));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("file_path"), FilePath);
	Result->SetNumberField(TEXT("width"), Width);
	Result->SetNumberField(TEXT("height"), Height);
	Result->SetNumberField(TEXT("file_size_bytes"), PngDataSmall.Num());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleSetViewportCamera
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleSetViewportCamera(const TSharedPtr<FJsonObject>& Params)
{
	FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	if (!ViewportClient)
	{
		// Fallback: iterate all viewports
		for (FLevelEditorViewportClient* VC : GEditor->GetLevelViewportClients())
		{
			if (VC)
			{
				ViewportClient = VC;
				break;
			}
		}
	}

	if (!ViewportClient)
	{
		return MakeErrorResponse(TEXT("No level editor viewport client found"));
	}

	bool bChanged = false;

	if (Params->HasField(TEXT("location")))
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Params->TryGetArrayField(TEXT("location"), Arr))
		{
			FVector Loc = JsonArrayToVector(*Arr);
			ViewportClient->SetViewLocation(Loc);
			bChanged = true;
		}
	}

	if (Params->HasField(TEXT("rotation")))
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr;
		if (Params->TryGetArrayField(TEXT("rotation"), Arr))
		{
			if (Arr->Num() >= 3)
			{
				FRotator Rot((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
				ViewportClient->SetViewRotation(Rot);
				bChanged = true;
			}
		}
	}

	if (!bChanged)
	{
		return MakeErrorResponse(TEXT("Must provide 'location' and/or 'rotation' as [x,y,z] arrays"));
	}

	ViewportClient->Invalidate();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);

	FVector FinalLoc = ViewportClient->GetViewLocation();
	FRotator FinalRot = ViewportClient->GetViewRotation();

	auto VecToArr = [](const FVector& V) -> TArray<TSharedPtr<FJsonValue>>
	{
		TArray<TSharedPtr<FJsonValue>> A;
		A.Add(MakeShared<FJsonValueNumber>(V.X));
		A.Add(MakeShared<FJsonValueNumber>(V.Y));
		A.Add(MakeShared<FJsonValueNumber>(V.Z));
		return A;
	};

	Result->SetArrayField(TEXT("location"), VecToArr(FinalLoc));
	Result->SetArrayField(TEXT("rotation"), VecToArr(FVector(FinalRot.Pitch, FinalRot.Yaw, FinalRot.Roll)));
	return Result;
}

// ---------------------------------------------------------------------------
// HandleFocusActor
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleFocusActor(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		return MakeErrorResponse(TEXT("Missing 'actor_name' parameter"));
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	// Use GEditor to focus the viewport cameras on the actor
	TArray<AActor*> ActorsToFocus;
	ActorsToFocus.Add(Actor);

	// MoveViewportCamerasToActor focuses all level editor viewports on the given actors
	const bool bInstant = true;
	GEditor->MoveViewportCamerasToActor(ActorsToFocus, bInstant);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("focused_actor"), Actor->GetName());
	return Result;
}

// ---------------------------------------------------------------------------
// HandleGetLevelInfo
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleGetLevelInfo(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeErrorResponse(TEXT("No editor world available"));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("level_name"), World->GetMapName());

	if (World->GetCurrentLevel())
	{
		Result->SetStringField(TEXT("current_level_path"), World->GetCurrentLevel()->GetPathName());
	}

	// Count actors by class
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

	TMap<FString, int32> ClassCounts;
	for (AActor* Actor : AllActors)
	{
		if (Actor)
		{
			FString ClassName = Actor->GetClass()->GetName();
			ClassCounts.FindOrAdd(ClassName)++;
		}
	}

	TSharedPtr<FJsonObject> CountsObj = MakeShared<FJsonObject>();
	for (const auto& Pair : ClassCounts)
	{
		CountsObj->SetNumberField(Pair.Key, Pair.Value);
	}
	Result->SetObjectField(TEXT("actor_counts_by_class"), CountsObj);
	Result->SetNumberField(TEXT("total_actors"), AllActors.Num());

	// World bounds - compute from all scene component bounding boxes
	FBox WorldBounds(ForceInit);
	for (AActor* Actor : AllActors)
	{
		if (!Actor) continue;
		FBox ActorBounds = Actor->GetComponentsBoundingBox(/* bNonColliding */ true);
		if (ActorBounds.IsValid)
		{
			WorldBounds += ActorBounds;
		}
	}

	if (WorldBounds.IsValid)
	{
		auto VecToArr = [](const FVector& V) -> TArray<TSharedPtr<FJsonValue>>
		{
			TArray<TSharedPtr<FJsonValue>> A;
			A.Add(MakeShared<FJsonValueNumber>(V.X));
			A.Add(MakeShared<FJsonValueNumber>(V.Y));
			A.Add(MakeShared<FJsonValueNumber>(V.Z));
			return A;
		};

		TSharedPtr<FJsonObject> BoundsObj = MakeShared<FJsonObject>();
		BoundsObj->SetArrayField(TEXT("min"), VecToArr(WorldBounds.Min));
		BoundsObj->SetArrayField(TEXT("max"), VecToArr(WorldBounds.Max));

		FVector Size = WorldBounds.GetSize();
		BoundsObj->SetArrayField(TEXT("size"), VecToArr(Size));
		Result->SetObjectField(TEXT("world_bounds"), BoundsObj);
	}

	return Result;
}

// ---------------------------------------------------------------------------
// HandleSaveCurrentLevel
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleSaveCurrentLevel(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeErrorResponse(TEXT("No editor world available"));
	}

	// FEditorFileUtils::SaveCurrentLevel saves the currently-active level
	bool bSaved = FEditorFileUtils::SaveCurrentLevel();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), bSaved);

	if (bSaved)
	{
		Result->SetStringField(TEXT("level_name"), World->GetMapName());
		Result->SetStringField(TEXT("message"), TEXT("Level saved successfully"));
	}
	else
	{
		Result->SetStringField(TEXT("error"), TEXT("Save operation returned false. The level may have no filename or save was cancelled."));
	}

	return Result;
}

// ---------------------------------------------------------------------------
// HandleGetSelection
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleGetSelection(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return MakeErrorResponse(TEXT("GEditor is not available"));
	}

	USelection* Selection = GEditor->GetSelectedActors();
	if (!Selection)
	{
		return MakeErrorResponse(TEXT("Could not get selection object"));
	}

	TArray<TSharedPtr<FJsonValue>> ActorArray;
	for (FSelectionIterator It(*Selection); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (!Actor) continue;

		TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
		ActorObj->SetStringField(TEXT("name"), Actor->GetName());
		ActorObj->SetStringField(TEXT("label"), Actor->GetActorNameOrLabel());
		ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

		FVector Loc = Actor->GetActorLocation();
		TArray<TSharedPtr<FJsonValue>> LocArr;
		LocArr.Add(MakeShared<FJsonValueNumber>(Loc.X));
		LocArr.Add(MakeShared<FJsonValueNumber>(Loc.Y));
		LocArr.Add(MakeShared<FJsonValueNumber>(Loc.Z));
		ActorObj->SetArrayField(TEXT("location"), LocArr);

		ActorArray.Add(MakeShared<FJsonValueObject>(ActorObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("count"), ActorArray.Num());
	Result->SetArrayField(TEXT("selected_actors"), ActorArray);
	return Result;
}

// ---------------------------------------------------------------------------
// HandleSetSelection
// ---------------------------------------------------------------------------
TSharedPtr<FJsonObject> FUnrealMCPLevelCommands::HandleSetSelection(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return MakeErrorResponse(TEXT("GEditor is not available"));
	}

	const TArray<TSharedPtr<FJsonValue>>* ActorNamesArray;
	if (!Params->TryGetArrayField(TEXT("actor_names"), ActorNamesArray))
	{
		return MakeErrorResponse(TEXT("Missing 'actor_names' array parameter"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return MakeErrorResponse(TEXT("No editor world available"));
	}

	// Clear current selection
	GEditor->SelectNone(true, true);

	TArray<FString> SelectedNames;
	TArray<FString> NotFoundNames;

	for (const auto& NameVal : *ActorNamesArray)
	{
		FString ActorName = NameVal->AsString();
		AActor* FoundActor = FindActorByName(ActorName);

		if (FoundActor)
		{
			GEditor->SelectActor(FoundActor, /*bInSelected=*/ true, /*bNotify=*/ true);
			SelectedNames.Add(FoundActor->GetName());
		}
		else
		{
			NotFoundNames.Add(ActorName);
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("selected_count"), SelectedNames.Num());

	TArray<TSharedPtr<FJsonValue>> SelectedArr;
	for (const FString& Name : SelectedNames)
	{
		SelectedArr.Add(MakeShared<FJsonValueString>(Name));
	}
	Result->SetArrayField(TEXT("selected"), SelectedArr);

	if (NotFoundNames.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> NotFoundArr;
		for (const FString& Name : NotFoundNames)
		{
			NotFoundArr.Add(MakeShared<FJsonValueString>(Name));
		}
		Result->SetArrayField(TEXT("not_found"), NotFoundArr);
	}

	return Result;
}
