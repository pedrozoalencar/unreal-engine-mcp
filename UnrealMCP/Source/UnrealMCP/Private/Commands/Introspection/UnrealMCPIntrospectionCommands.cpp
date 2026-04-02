#include "Commands/Introspection/UnrealMCPIntrospectionCommands.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"

FUnrealMCPIntrospectionCommands::FUnrealMCPIntrospectionCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPIntrospectionCommands::MakeErrorResponse(const FString& Message)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), false);
	Result->SetStringField(TEXT("error"), Message);
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPIntrospectionCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("inspect_class"))
	{
		return HandleInspectClass(Params);
	}
	else if (CommandType == TEXT("inspect_enum"))
	{
		return HandleInspectEnum(Params);
	}
	return MakeErrorResponse(FString::Printf(TEXT("Unknown introspection command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPIntrospectionCommands::HandleInspectClass(const TSharedPtr<FJsonObject>& Params)
{
	FString ClassName = Params->GetStringField(TEXT("class_name"));
	if (ClassName.IsEmpty())
	{
		return MakeErrorResponse(TEXT("class_name is required"));
	}

	bool bIncludeInherited = false;
	if (Params->HasField(TEXT("include_inherited")))
	{
		bIncludeInherited = Params->GetBoolField(TEXT("include_inherited"));
	}

	// Try to find the class by name
	UClass* FoundClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);

	// If not found directly, try with common prefixes
	if (!FoundClass)
	{
		// Try with U prefix (for UObject-derived classes like UStaticMeshComponent)
		FoundClass = FindFirstObject<UClass>(*(FString(TEXT("U")) + ClassName), EFindFirstObjectOptions::NativeFirst);
	}
	if (!FoundClass)
	{
		// Try with A prefix (for AActor-derived classes)
		FoundClass = FindFirstObject<UClass>(*(FString(TEXT("A")) + ClassName), EFindFirstObjectOptions::NativeFirst);
	}
	if (!FoundClass)
	{
		// Fallback: iterate all classes to find a partial match
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetName() == ClassName || It->GetName().EndsWith(ClassName))
			{
				FoundClass = *It;
				break;
			}
		}
	}

	if (!FoundClass)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Class not found: %s"), *ClassName));
	}

	EFieldIteratorFlags::SuperClassFlags SuperFlag = bIncludeInherited
		? EFieldIteratorFlags::IncludeSuper
		: EFieldIteratorFlags::ExcludeSuper;

	// Collect functions
	TArray<TSharedPtr<FJsonValue>> FunctionsArray;
	for (TFieldIterator<UFunction> FuncIt(FoundClass, SuperFlag); FuncIt; ++FuncIt)
	{
		UFunction* Function = *FuncIt;
		if (!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintEvent))
		{
			continue;
		}

		TSharedPtr<FJsonObject> FuncObj = MakeShareable(new FJsonObject);
		FuncObj->SetStringField(TEXT("name"), Function->GetName());

		// Determine flags
		TArray<TSharedPtr<FJsonValue>> FlagsArray;
		if (Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("BlueprintCallable"))));
		}
		if (Function->HasAnyFunctionFlags(FUNC_BlueprintEvent))
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("BlueprintEvent"))));
		}
		if (Function->HasAnyFunctionFlags(FUNC_BlueprintPure))
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("BlueprintPure"))));
		}
		if (Function->HasAnyFunctionFlags(FUNC_Static))
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("Static"))));
		}
		if (Function->HasAnyFunctionFlags(FUNC_Const))
		{
			FlagsArray.Add(MakeShareable(new FJsonValueString(TEXT("Const"))));
		}
		FuncObj->SetArrayField(TEXT("flags"), FlagsArray);

		// Collect parameters and return type
		TArray<TSharedPtr<FJsonValue>> ParamsArray;
		FString ReturnType = TEXT("void");

		for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
		{
			FProperty* Prop = *ParamIt;
			if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				ReturnType = Prop->GetCPPType();
				continue;
			}
			if (Prop->HasAnyPropertyFlags(CPF_Parm))
			{
				TSharedPtr<FJsonObject> ParamObj = MakeShareable(new FJsonObject);
				ParamObj->SetStringField(TEXT("name"), Prop->GetName());
				ParamObj->SetStringField(TEXT("type"), Prop->GetCPPType());
				ParamObj->SetBoolField(TEXT("is_output"), Prop->HasAnyPropertyFlags(CPF_OutParm) && !Prop->HasAnyPropertyFlags(CPF_ReturnParm));
				ParamsArray.Add(MakeShareable(new FJsonValueObject(ParamObj)));
			}
		}

		FuncObj->SetStringField(TEXT("return_type"), ReturnType);
		FuncObj->SetArrayField(TEXT("parameters"), ParamsArray);
		FunctionsArray.Add(MakeShareable(new FJsonValueObject(FuncObj)));
	}

	// Collect properties
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (TFieldIterator<FProperty> PropIt(FoundClass, SuperFlag); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;

		TSharedPtr<FJsonObject> PropObj = MakeShareable(new FJsonObject);
		PropObj->SetStringField(TEXT("name"), Prop->GetName());
		PropObj->SetStringField(TEXT("type"), Prop->GetCPPType());

		// Collect property flags
		TArray<TSharedPtr<FJsonValue>> PropFlags;
		if (Prop->HasAnyPropertyFlags(CPF_BlueprintVisible))
		{
			PropFlags.Add(MakeShareable(new FJsonValueString(TEXT("BlueprintVisible"))));
		}
		if (Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
		{
			PropFlags.Add(MakeShareable(new FJsonValueString(TEXT("BlueprintReadOnly"))));
		}
		if (Prop->HasAnyPropertyFlags(CPF_Edit))
		{
			PropFlags.Add(MakeShareable(new FJsonValueString(TEXT("EditAnywhere"))));
		}
		if (Prop->HasAnyPropertyFlags(CPF_EditConst))
		{
			PropFlags.Add(MakeShareable(new FJsonValueString(TEXT("VisibleAnywhere"))));
		}
		if (Prop->HasAnyPropertyFlags(CPF_Config))
		{
			PropFlags.Add(MakeShareable(new FJsonValueString(TEXT("Config"))));
		}
		PropObj->SetArrayField(TEXT("flags"), PropFlags);
		PropertiesArray.Add(MakeShareable(new FJsonValueObject(PropObj)));
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("class_name"), FoundClass->GetName());
	Result->SetStringField(TEXT("full_name"), FoundClass->GetPathName());
	if (FoundClass->GetSuperClass())
	{
		Result->SetStringField(TEXT("parent_class"), FoundClass->GetSuperClass()->GetName());
	}
	Result->SetArrayField(TEXT("functions"), FunctionsArray);
	Result->SetArrayField(TEXT("properties"), PropertiesArray);
	Result->SetNumberField(TEXT("function_count"), FunctionsArray.Num());
	Result->SetNumberField(TEXT("property_count"), PropertiesArray.Num());
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPIntrospectionCommands::HandleInspectEnum(const TSharedPtr<FJsonObject>& Params)
{
	FString EnumName = Params->GetStringField(TEXT("enum_name"));
	if (EnumName.IsEmpty())
	{
		return MakeErrorResponse(TEXT("enum_name is required"));
	}

	// Try to find the enum by name
	UEnum* FoundEnum = FindFirstObject<UEnum>(*EnumName, EFindFirstObjectOptions::NativeFirst);

	if (!FoundEnum)
	{
		// Try with E prefix
		FoundEnum = FindFirstObject<UEnum>(*(FString(TEXT("E")) + EnumName), EFindFirstObjectOptions::NativeFirst);
	}
	if (!FoundEnum)
	{
		// Fallback: iterate all enums
		for (TObjectIterator<UEnum> It; It; ++It)
		{
			if (It->GetName() == EnumName || It->GetName().EndsWith(EnumName))
			{
				FoundEnum = *It;
				break;
			}
		}
	}

	if (!FoundEnum)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Enum not found: %s"), *EnumName));
	}

	// Collect enum values (NumEnums() - 1 to exclude the _MAX entry)
	TArray<TSharedPtr<FJsonValue>> ValuesArray;
	int32 NumValues = FoundEnum->NumEnums() - 1;
	for (int32 i = 0; i < NumValues; i++)
	{
		TSharedPtr<FJsonObject> EntryObj = MakeShareable(new FJsonObject);
		EntryObj->SetStringField(TEXT("name"), FoundEnum->GetNameStringByIndex(i));
		EntryObj->SetStringField(TEXT("display_name"), FoundEnum->GetDisplayNameTextByIndex(i).ToString());
		EntryObj->SetNumberField(TEXT("value"), static_cast<double>(FoundEnum->GetValueByIndex(i)));
		ValuesArray.Add(MakeShareable(new FJsonValueObject(EntryObj)));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("enum_name"), FoundEnum->GetName());
	Result->SetStringField(TEXT("full_name"), FoundEnum->GetPathName());
	Result->SetNumberField(TEXT("count"), ValuesArray.Num());
	Result->SetArrayField(TEXT("values"), ValuesArray);
	return Result;
}
