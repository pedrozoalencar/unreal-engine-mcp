#include "Commands/BlueprintGraph/Nodes/UtilityNodes.h"
#include "Commands/BlueprintGraph/Nodes/NodeCreatorUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Select.h"
#include "K2Node_SpawnActorFromClass.h"
#include "EdGraphSchema_K2.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Json.h"
#include "UObject/UObjectIterator.h"

// Helper: find a UClass by name across all loaded modules
static UClass* FindClassByName(const FString& ClassName)
{
	// 1. Try StaticFindObject with full path (e.g. "/Script/GeometryScriptingCore.GeometryScriptLibrary_MeshPrimitiveFunctions")
	UClass* Found = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, *ClassName));
	if (Found) return Found;

	// 2. Try common module paths
	TArray<FString> ModulePaths = {
		TEXT("/Script/Engine"),
		TEXT("/Script/CoreUObject"),
		TEXT("/Script/GeometryScriptingCore"),
		TEXT("/Script/GeometryScriptingEditor"),
		TEXT("/Script/DynamicMesh"),
		TEXT("/Script/GeometryCore"),
		TEXT("/Script/GeometryFramework"),
		TEXT("/Script/ModelingComponents"),
		TEXT("/Script/UMG"),
		TEXT("/Script/Niagara"),
		TEXT("/Script/PCG")
	};

	for (const FString& ModulePath : ModulePaths)
	{
		FString FullPath = FString::Printf(TEXT("%s.%s"), *ModulePath, *ClassName);
		Found = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, *FullPath));
		if (Found) return Found;
	}

	// 3. Try with 'U' prefix if not present
	if (!ClassName.StartsWith(TEXT("U")) && !ClassName.StartsWith(TEXT("A")))
	{
		FString WithPrefix = TEXT("U") + ClassName;
		for (const FString& ModulePath : ModulePaths)
		{
			FString FullPath = FString::Printf(TEXT("%s.%s"), *ModulePath, *WithPrefix);
			Found = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, *FullPath));
			if (Found) return Found;
		}
	}

	// 4. Brute force: iterate all UClasses looking for name match
	FString ShortName = ClassName;
	// Strip module prefix if present (e.g. "GeometryScriptLibrary_MeshPrimitiveFunctions")
	if (ShortName.Contains(TEXT(".")))
	{
		ShortName = ShortName.RightChop(ShortName.Find(TEXT(".")) + 1);
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->GetName() == ShortName || Class->GetName() == ClassName)
		{
			return Class;
		}
	}

	return nullptr;
}

// Helper: find a UFunction by name, searching common library classes and any specified class
static UFunction* FindFunctionAcrossClasses(const FString& FunctionName, const FString& ClassName)
{
	UFunction* Func = nullptr;

	// If a class was specified, search it first
	if (!ClassName.IsEmpty())
	{
		UClass* TargetClass = FindClassByName(ClassName);
		if (TargetClass)
		{
			Func = TargetClass->FindFunctionByName(FName(*FunctionName));
			if (Func) return Func;
		}
	}

	// Search common Kismet libraries
	TArray<UClass*> CommonClasses = {
		UKismetSystemLibrary::StaticClass(),
		UKismetMathLibrary::StaticClass(),
		UGameplayStatics::StaticClass()
	};

	for (UClass* Class : CommonClasses)
	{
		Func = Class->FindFunctionByName(FName(*FunctionName));
		if (Func) return Func;
	}

	// Brute force: search ALL BlueprintFunctionLibrary subclasses
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UBlueprintFunctionLibrary::StaticClass()))
		{
			Func = Class->FindFunctionByName(FName(*FunctionName));
			if (Func) return Func;
		}
	}

	return nullptr;
}

UK2Node* FUtilityNodeCreator::CreatePrintNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	UK2Node_CallFunction* PrintNode = NewObject<UK2Node_CallFunction>(Graph);
	if (!PrintNode)
	{
		return nullptr;
	}

	UFunction* PrintFunc = UKismetSystemLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString)
	);

	if (!PrintFunc)
	{
		return nullptr;
	}

	PrintNode->SetFromFunction(PrintFunc);

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	PrintNode->NodePosX = static_cast<int32>(PosX);
	PrintNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(PrintNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(PrintNode, Graph);

	FString Message;
	if (Params->TryGetStringField(TEXT("message"), Message))
	{
		UEdGraphPin* InStringPin = PrintNode->FindPin(TEXT("InString"));
		if (InStringPin)
		{
			InStringPin->DefaultValue = Message;
		}
	}

	return PrintNode;
}

UK2Node* FUtilityNodeCreator::CreateCallFunctionNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	FString TargetFunction;
	if (!Params->TryGetStringField(TEXT("target_function"), TargetFunction))
	{
		return nullptr;
	}

	// Accept both "target_class" and "target_blueprint" as the class specifier
	FString ClassName;
	if (!Params->TryGetStringField(TEXT("target_class"), ClassName))
	{
		Params->TryGetStringField(TEXT("target_blueprint"), ClassName);
	}

	// Find the function using our comprehensive search
	UFunction* TargetFunc = FindFunctionAcrossClasses(TargetFunction, ClassName);

	if (!TargetFunc)
	{
		UE_LOG(LogTemp, Error, TEXT("CallFunction: Could not find function '%s' (class hint: '%s')"), *TargetFunction, *ClassName);
		return nullptr;
	}

	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
	if (!CallNode)
	{
		return nullptr;
	}

	CallNode->SetFromFunction(TargetFunc);

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	CallNode->NodePosX = static_cast<int32>(PosX);
	CallNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(CallNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(CallNode, Graph);

	// Set default pin values if provided
	const TSharedPtr<FJsonObject>* PinDefaults = nullptr;
	if (Params->TryGetObjectField(TEXT("pin_defaults"), PinDefaults))
	{
		for (auto& Pair : (*PinDefaults)->Values)
		{
			UEdGraphPin* Pin = CallNode->FindPin(FName(*Pair.Key));
			if (Pin)
			{
				Pin->DefaultValue = Pair.Value->AsString();
			}
		}
	}

	return CallNode;
}

UK2Node* FUtilityNodeCreator::CreateSelectNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	UK2Node_Select* SelectNode = NewObject<UK2Node_Select>(Graph);
	if (!SelectNode)
	{
		return nullptr;
	}

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	SelectNode->NodePosX = static_cast<int32>(PosX);
	SelectNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(SelectNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(SelectNode, Graph);

	return SelectNode;
}

UK2Node* FUtilityNodeCreator::CreateSpawnActorNode(UEdGraph* Graph, const TSharedPtr<FJsonObject>& Params)
{
	if (!Graph || !Params.IsValid())
	{
		return nullptr;
	}

	UK2Node_SpawnActorFromClass* SpawnActorNode = NewObject<UK2Node_SpawnActorFromClass>(Graph);
	if (!SpawnActorNode)
	{
		return nullptr;
	}

	double PosX, PosY;
	FNodeCreatorUtils::ExtractNodePosition(Params, PosX, PosY);
	SpawnActorNode->NodePosX = static_cast<int32>(PosX);
	SpawnActorNode->NodePosY = static_cast<int32>(PosY);

	Graph->AddNode(SpawnActorNode, true, false);
	FNodeCreatorUtils::InitializeK2Node(SpawnActorNode, Graph);

	return SpawnActorNode;
}
