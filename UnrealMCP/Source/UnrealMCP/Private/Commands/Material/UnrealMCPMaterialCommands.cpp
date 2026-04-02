#include "Commands/Material/UnrealMCPMaterialCommands.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "MaterialEditingLibrary.h"
#include "Factories/MaterialFactoryNew.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UObject/UObjectIterator.h"

FUnrealMCPMaterialCommands::FUnrealMCPMaterialCommands()
{
}

// ============================================================================
// Helpers
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::MakeErrorResponse(const FString& Message)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), false);
	Result->SetStringField(TEXT("error"), Message);
	return Result;
}

UMaterial* FUnrealMCPMaterialCommands::FindMaterial(const FString& AssetPath)
{
	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		return nullptr;
	}
	return Cast<UMaterial>(Asset);
}

UMaterialExpression* FUnrealMCPMaterialCommands::FindExpression(UMaterial* Mat, const FString& NodeId)
{
	if (!Mat)
	{
		return nullptr;
	}

	const TArray<TObjectPtr<UMaterialExpression>>& Expressions = Mat->GetExpressionCollection().Expressions;

	// Try by index first
	if (NodeId.IsNumeric())
	{
		int32 Index = FCString::Atoi(*NodeId);
		if (Expressions.IsValidIndex(Index))
		{
			return Expressions[Index];
		}
	}

	// Try by name
	for (UMaterialExpression* Expr : Expressions)
	{
		if (Expr && Expr->GetName() == NodeId)
		{
			return Expr;
		}
	}

	return nullptr;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::MakeExpressionJson(UMaterialExpression* Expr)
{
	TSharedPtr<FJsonObject> Info = MakeShareable(new FJsonObject);
	if (!Expr) return Info;

	Info->SetStringField(TEXT("class"), Expr->GetClass()->GetName());
	Info->SetStringField(TEXT("name"), Expr->GetName());
	Info->SetNumberField(TEXT("node_pos_x"), Expr->MaterialExpressionEditorX);
	Info->SetNumberField(TEXT("node_pos_y"), Expr->MaterialExpressionEditorY);
	Info->SetStringField(TEXT("desc"), Expr->Desc);

	return Info;
}

// ============================================================================
// Command Router
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("create_material")) return HandleCreateMaterial(Params);
	if (CommandType == TEXT("delete_material")) return HandleDeleteMaterial(Params);
	if (CommandType == TEXT("set_material_property")) return HandleSetMaterialProperty(Params);
	if (CommandType == TEXT("get_material_info")) return HandleGetMaterialInfo(Params);

	if (CommandType == TEXT("add_expression")) return HandleAddExpression(Params);
	if (CommandType == TEXT("delete_expression")) return HandleDeleteExpression(Params);
	if (CommandType == TEXT("list_expressions")) return HandleListExpressions(Params);
	if (CommandType == TEXT("list_pins")) return HandleListPins(Params);

	if (CommandType == TEXT("connect_expressions")) return HandleConnectExpressions(Params);
	if (CommandType == TEXT("connect_to_output")) return HandleConnectToOutput(Params);
	if (CommandType == TEXT("disconnect_expression")) return HandleDisconnectExpression(Params);

	if (CommandType == TEXT("set_expression_value")) return HandleSetExpressionValue(Params);
	if (CommandType == TEXT("compile_material")) return HandleCompileMaterial(Params);
	if (CommandType == TEXT("apply_material_to_actor")) return HandleApplyMaterialToActor(Params);

	return MakeErrorResponse(FString::Printf(TEXT("Unknown material command: %s"), *CommandType));
}

// ============================================================================
// Material Lifecycle
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return MakeErrorResponse(TEXT("asset_path is required"));
	}

	// Check if material already exists
	UMaterial* ExistingMat = FindMaterial(AssetPath);
	if (ExistingMat)
	{
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("asset_path"), AssetPath);
		Result->SetStringField(TEXT("message"), TEXT("Material already exists"));
		return Result;
	}

	// Split path into package path and asset name
	FString PackagePath, AssetName;
	int32 LastSlash;
	if (AssetPath.FindLastChar('/', LastSlash))
	{
		PackagePath = AssetPath.Left(LastSlash);
		AssetName = AssetPath.RightChop(LastSlash + 1);
	}
	else
	{
		return MakeErrorResponse(TEXT("Invalid asset_path format. Expected /Game/Path/MaterialName"));
	}

	// Create material using factory
	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UMaterial::StaticClass(), Factory);

	if (!NewAsset)
	{
		return MakeErrorResponse(TEXT("Failed to create material asset"));
	}

	UMaterial* Material = Cast<UMaterial>(NewAsset);
	if (!Material)
	{
		return MakeErrorResponse(TEXT("Created asset is not a UMaterial"));
	}

	// Apply optional properties
	if (Params->HasField(TEXT("blend_mode")))
	{
		FString BlendMode = Params->GetStringField(TEXT("blend_mode"));
		if (BlendMode == TEXT("Opaque")) Material->BlendMode = BLEND_Opaque;
		else if (BlendMode == TEXT("Translucent")) Material->BlendMode = BLEND_Translucent;
		else if (BlendMode == TEXT("TranslucentColoredTransmittance")) Material->BlendMode = BLEND_TranslucentColoredTransmittance;
		else if (BlendMode == TEXT("TranslucentGreyTransmittance")) Material->BlendMode = BLEND_TranslucentGreyTransmittance;
		else if (BlendMode == TEXT("Additive")) Material->BlendMode = BLEND_Additive;
		else if (BlendMode == TEXT("Modulate")) Material->BlendMode = BLEND_Modulate;
	}

	if (Params->HasField(TEXT("shading_model")))
	{
		FString ShadingModel = Params->GetStringField(TEXT("shading_model"));
		if (ShadingModel == TEXT("DefaultLit")) Material->SetShadingModel(MSM_DefaultLit);
		else if (ShadingModel == TEXT("Unlit")) Material->SetShadingModel(MSM_Unlit);
		else if (ShadingModel == TEXT("ThinTranslucent")) Material->SetShadingModel(MSM_ThinTranslucent);
		else if (ShadingModel == TEXT("ClearCoat")) Material->SetShadingModel(MSM_ClearCoat);
		else if (ShadingModel == TEXT("SubsurfaceProfile")) Material->SetShadingModel(MSM_SubsurfaceProfile);
	}

	if (Params->HasField(TEXT("two_sided")))
	{
		Material->TwoSided = Params->GetBoolField(TEXT("two_sided"));
	}

	if (Params->HasField(TEXT("translucency_lighting_mode")))
	{
		FString TLM = Params->GetStringField(TEXT("translucency_lighting_mode"));
		if (TLM == TEXT("VolumetricNonDirectional")) Material->TranslucencyLightingMode = TLM_VolumetricNonDirectional;
		else if (TLM == TEXT("VolumetricDirectional")) Material->TranslucencyLightingMode = TLM_VolumetricDirectional;
		else if (TLM == TEXT("VolumetricPerVertexNonDirectional")) Material->TranslucencyLightingMode = TLM_VolumetricPerVertexNonDirectional;
		else if (TLM == TEXT("VolumetricPerVertexDirectional")) Material->TranslucencyLightingMode = TLM_VolumetricPerVertexDirectional;
		else if (TLM == TEXT("Surface")) Material->TranslucencyLightingMode = TLM_Surface;
		else if (TLM == TEXT("SurfacePerPixelLighting")) Material->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
		else if (TLM == TEXT("SurfaceForwardShading")) Material->TranslucencyLightingMode = TLM_SurfaceForwardShading;
	}

	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("message"), TEXT("Material created successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleDeleteMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return MakeErrorResponse(TEXT("asset_path is required"));
	}

	bool bDeleted = UEditorAssetLibrary::DeleteAsset(AssetPath);
	if (!bDeleted)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Failed to delete material at '%s'"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Deleted material '%s'"), *AssetPath));
	return Result;
}

// ============================================================================
// Material Properties
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetMaterialProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	FString PropertyValue = Params->GetStringField(TEXT("property_value"));

	if (AssetPath.IsEmpty() || PropertyName.IsEmpty())
	{
		return MakeErrorResponse(TEXT("asset_path and property_name are required"));
	}

	UMaterial* Material = FindMaterial(AssetPath);
	if (!Material)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Material not found at '%s'"), *AssetPath));
	}

	Material->PreEditChange(nullptr);

	bool bPropertySet = false;

	if (PropertyName == TEXT("blend_mode"))
	{
		if (PropertyValue == TEXT("Opaque")) { Material->BlendMode = BLEND_Opaque; bPropertySet = true; }
		else if (PropertyValue == TEXT("Translucent")) { Material->BlendMode = BLEND_Translucent; bPropertySet = true; }
		else if (PropertyValue == TEXT("TranslucentColoredTransmittance")) { Material->BlendMode = BLEND_TranslucentColoredTransmittance; bPropertySet = true; }
		else if (PropertyValue == TEXT("TranslucentGreyTransmittance")) { Material->BlendMode = BLEND_TranslucentGreyTransmittance; bPropertySet = true; }
		else if (PropertyValue == TEXT("Additive")) { Material->BlendMode = BLEND_Additive; bPropertySet = true; }
		else if (PropertyValue == TEXT("Modulate")) { Material->BlendMode = BLEND_Modulate; bPropertySet = true; }
	}
	else if (PropertyName == TEXT("shading_model"))
	{
		if (PropertyValue == TEXT("DefaultLit")) { Material->SetShadingModel(MSM_DefaultLit); bPropertySet = true; }
		else if (PropertyValue == TEXT("Unlit")) { Material->SetShadingModel(MSM_Unlit); bPropertySet = true; }
		else if (PropertyValue == TEXT("ThinTranslucent")) { Material->SetShadingModel(MSM_ThinTranslucent); bPropertySet = true; }
		else if (PropertyValue == TEXT("ClearCoat")) { Material->SetShadingModel(MSM_ClearCoat); bPropertySet = true; }
		else if (PropertyValue == TEXT("SubsurfaceProfile")) { Material->SetShadingModel(MSM_SubsurfaceProfile); bPropertySet = true; }
	}
	else if (PropertyName == TEXT("two_sided"))
	{
		Material->TwoSided = PropertyValue.ToBool();
		bPropertySet = true;
	}
	else if (PropertyName == TEXT("translucency_lighting_mode"))
	{
		if (PropertyValue == TEXT("VolumetricNonDirectional")) { Material->TranslucencyLightingMode = TLM_VolumetricNonDirectional; bPropertySet = true; }
		else if (PropertyValue == TEXT("VolumetricDirectional")) { Material->TranslucencyLightingMode = TLM_VolumetricDirectional; bPropertySet = true; }
		else if (PropertyValue == TEXT("VolumetricPerVertexNonDirectional")) { Material->TranslucencyLightingMode = TLM_VolumetricPerVertexNonDirectional; bPropertySet = true; }
		else if (PropertyValue == TEXT("VolumetricPerVertexDirectional")) { Material->TranslucencyLightingMode = TLM_VolumetricPerVertexDirectional; bPropertySet = true; }
		else if (PropertyValue == TEXT("Surface")) { Material->TranslucencyLightingMode = TLM_Surface; bPropertySet = true; }
		else if (PropertyValue == TEXT("SurfacePerPixelLighting")) { Material->TranslucencyLightingMode = TLM_SurfacePerPixelLighting; bPropertySet = true; }
		else if (PropertyValue == TEXT("SurfaceForwardShading")) { Material->TranslucencyLightingMode = TLM_SurfaceForwardShading; bPropertySet = true; }
	}
	else if (PropertyName == TEXT("opacity_mask_clip_value"))
	{
		Material->OpacityMaskClipValue = FCString::Atof(*PropertyValue);
		bPropertySet = true;
	}

	if (!bPropertySet)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Unknown property '%s' or invalid value '%s'"), *PropertyName, *PropertyValue));
	}

	Material->PostEditChange();
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Set %s = %s"), *PropertyName, *PropertyValue));
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleGetMaterialInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return MakeErrorResponse(TEXT("asset_path is required"));
	}

	UMaterial* Material = FindMaterial(AssetPath);
	if (!Material)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Material not found at '%s'"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), AssetPath);
	Result->SetStringField(TEXT("name"), Material->GetName());

	// Blend mode
	FString BlendModeStr;
	switch (Material->BlendMode)
	{
		case BLEND_Opaque: BlendModeStr = TEXT("Opaque"); break;
		case BLEND_Translucent: BlendModeStr = TEXT("Translucent"); break;
		case BLEND_TranslucentColoredTransmittance: BlendModeStr = TEXT("TranslucentColoredTransmittance"); break;
		case BLEND_TranslucentGreyTransmittance: BlendModeStr = TEXT("TranslucentGreyTransmittance"); break;
		case BLEND_Additive: BlendModeStr = TEXT("Additive"); break;
		case BLEND_Modulate: BlendModeStr = TEXT("Modulate"); break;
		default: BlendModeStr = TEXT("Unknown"); break;
	}
	Result->SetStringField(TEXT("blend_mode"), BlendModeStr);

	// Shading model
	FMaterialShadingModelField ShadingModels = Material->GetShadingModels();
	FString ShadingModelStr = TEXT("Unknown");
	if (ShadingModels.HasShadingModel(MSM_DefaultLit)) ShadingModelStr = TEXT("DefaultLit");
	else if (ShadingModels.HasShadingModel(MSM_Unlit)) ShadingModelStr = TEXT("Unlit");
	else if (ShadingModels.HasShadingModel(MSM_ThinTranslucent)) ShadingModelStr = TEXT("ThinTranslucent");
	else if (ShadingModels.HasShadingModel(MSM_ClearCoat)) ShadingModelStr = TEXT("ClearCoat");
	else if (ShadingModels.HasShadingModel(MSM_SubsurfaceProfile)) ShadingModelStr = TEXT("SubsurfaceProfile");
	Result->SetStringField(TEXT("shading_model"), ShadingModelStr);

	Result->SetBoolField(TEXT("two_sided"), Material->IsTwoSided());

	// Translucency lighting mode
	FString TLMStr;
	switch (Material->TranslucencyLightingMode)
	{
		case TLM_VolumetricNonDirectional: TLMStr = TEXT("VolumetricNonDirectional"); break;
		case TLM_VolumetricDirectional: TLMStr = TEXT("VolumetricDirectional"); break;
		case TLM_VolumetricPerVertexNonDirectional: TLMStr = TEXT("VolumetricPerVertexNonDirectional"); break;
		case TLM_VolumetricPerVertexDirectional: TLMStr = TEXT("VolumetricPerVertexDirectional"); break;
		case TLM_Surface: TLMStr = TEXT("Surface"); break;
		case TLM_SurfacePerPixelLighting: TLMStr = TEXT("SurfacePerPixelLighting"); break;
		case TLM_SurfaceForwardShading: TLMStr = TEXT("SurfaceForwardShading"); break;
		default: TLMStr = TEXT("Unknown"); break;
	}
	Result->SetStringField(TEXT("translucency_lighting_mode"), TLMStr);

	// List expressions
	const TArray<TObjectPtr<UMaterialExpression>>& Expressions = Material->GetExpressionCollection().Expressions;
	TArray<TSharedPtr<FJsonValue>> ExprArray;
	for (int32 i = 0; i < Expressions.Num(); i++)
	{
		if (Expressions[i])
		{
			TSharedPtr<FJsonObject> ExprJson = MakeExpressionJson(Expressions[i]);
			ExprJson->SetNumberField(TEXT("index"), i);
			ExprArray.Add(MakeShareable(new FJsonValueObject(ExprJson)));
		}
	}
	Result->SetArrayField(TEXT("expressions"), ExprArray);
	Result->SetNumberField(TEXT("expression_count"), Expressions.Num());

	return Result;
}

// ============================================================================
// Expression Nodes
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleAddExpression(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ClassName = Params->GetStringField(TEXT("class_name"));

	if (AssetPath.IsEmpty() || ClassName.IsEmpty())
	{
		return MakeErrorResponse(TEXT("asset_path and class_name are required"));
	}

	UMaterial* Material = FindMaterial(AssetPath);
	if (!Material)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Material not found at '%s'"), *AssetPath));
	}

	// Find expression class by name
	UClass* ExprClass = nullptr;
	FString FullClassName = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
	ExprClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, *FullClassName));
	if (!ExprClass)
	{
		// Try without prefix - iterate all UMaterialExpression subclasses
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->IsChildOf(UMaterialExpression::StaticClass()) && It->GetName() == ClassName)
			{
				ExprClass = *It;
				break;
			}
		}
	}

	if (!ExprClass)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Could not find expression class '%s'"), *ClassName));
	}

	if (!ExprClass->IsChildOf(UMaterialExpression::StaticClass()))
	{
		return MakeErrorResponse(FString::Printf(TEXT("Class '%s' is not a UMaterialExpression subclass"), *ClassName));
	}

	// Create expression
	Material->PreEditChange(nullptr);

	UMaterialExpression* NewExpr = NewObject<UMaterialExpression>(Material, ExprClass);
	if (!NewExpr)
	{
		return MakeErrorResponse(TEXT("Failed to create material expression"));
	}

	// Set position
	if (Params->HasField(TEXT("node_pos_x")))
	{
		NewExpr->MaterialExpressionEditorX = static_cast<int32>(Params->GetNumberField(TEXT("node_pos_x")));
	}
	if (Params->HasField(TEXT("node_pos_y")))
	{
		NewExpr->MaterialExpressionEditorY = static_cast<int32>(Params->GetNumberField(TEXT("node_pos_y")));
	}

	// Set description
	if (Params->HasField(TEXT("desc")))
	{
		NewExpr->Desc = Params->GetStringField(TEXT("desc"));
	}

	// Set initial values for common types
	if (UMaterialExpressionConstant* ConstExpr = Cast<UMaterialExpressionConstant>(NewExpr))
	{
		if (Params->HasField(TEXT("value")))
		{
			ConstExpr->R = static_cast<float>(Params->GetNumberField(TEXT("value")));
		}
	}
	else if (UMaterialExpressionConstant3Vector* Const3Expr = Cast<UMaterialExpressionConstant3Vector>(NewExpr))
	{
		if (Params->HasField(TEXT("color")))
		{
			const TArray<TSharedPtr<FJsonValue>>& ColorArr = Params->GetArrayField(TEXT("color"));
			if (ColorArr.Num() >= 3)
			{
				float R = static_cast<float>(ColorArr[0]->AsNumber());
				float G = static_cast<float>(ColorArr[1]->AsNumber());
				float B = static_cast<float>(ColorArr[2]->AsNumber());
				float A = ColorArr.Num() >= 4 ? static_cast<float>(ColorArr[3]->AsNumber()) : 1.0f;
				Const3Expr->Constant = FLinearColor(R, G, B, A);
			}
		}
	}
	else if (UMaterialExpressionFresnel* FresnelExpr = Cast<UMaterialExpressionFresnel>(NewExpr))
	{
		if (Params->HasField(TEXT("exponent")))
		{
			FresnelExpr->Exponent = static_cast<float>(Params->GetNumberField(TEXT("exponent")));
		}
		if (Params->HasField(TEXT("base_reflect_fraction")))
		{
			FresnelExpr->BaseReflectFractionIn = static_cast<float>(Params->GetNumberField(TEXT("base_reflect_fraction")));
		}
	}
	else if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(NewExpr))
	{
		if (Params->HasField(TEXT("parameter_name")))
		{
			ScalarParam->SetParameterName(FName(*Params->GetStringField(TEXT("parameter_name"))));
		}
		if (Params->HasField(TEXT("default_value")))
		{
			ScalarParam->DefaultValue = static_cast<float>(Params->GetNumberField(TEXT("default_value")));
		}
	}
	else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(NewExpr))
	{
		if (Params->HasField(TEXT("parameter_name")))
		{
			VectorParam->SetParameterName(FName(*Params->GetStringField(TEXT("parameter_name"))));
		}
		if (Params->HasField(TEXT("default_value")))
		{
			const TArray<TSharedPtr<FJsonValue>>& ValArr = Params->GetArrayField(TEXT("default_value"));
			if (ValArr.Num() >= 3)
			{
				float R = static_cast<float>(ValArr[0]->AsNumber());
				float G = static_cast<float>(ValArr[1]->AsNumber());
				float B = static_cast<float>(ValArr[2]->AsNumber());
				float A = ValArr.Num() >= 4 ? static_cast<float>(ValArr[3]->AsNumber()) : 1.0f;
				VectorParam->DefaultValue = FLinearColor(R, G, B, A);
			}
		}
	}

	// Add to material expression collection
	Material->GetExpressionCollection().Expressions.Add(NewExpr);

	Material->PostEditChange();
	Material->MarkPackageDirty();

	// Return info about the new expression
	int32 NewIndex = Material->GetExpressionCollection().Expressions.Num() - 1;

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("index"), NewIndex);
	Result->SetStringField(TEXT("class"), NewExpr->GetClass()->GetName());
	Result->SetStringField(TEXT("name"), NewExpr->GetName());
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Created expression '%s' at index %d"), *ClassName, NewIndex));
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleDeleteExpression(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString NodeId = Params->GetStringField(TEXT("node_id"));

	if (AssetPath.IsEmpty() || NodeId.IsEmpty())
	{
		return MakeErrorResponse(TEXT("asset_path and node_id are required"));
	}

	UMaterial* Material = FindMaterial(AssetPath);
	if (!Material)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Material not found at '%s'"), *AssetPath));
	}

	UMaterialExpression* Expr = FindExpression(Material, NodeId);
	if (!Expr)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Expression '%s' not found"), *NodeId));
	}

	Material->PreEditChange(nullptr);

	// Disconnect all connections referencing this expression
	TArray<TObjectPtr<UMaterialExpression>>& Expressions = Material->GetExpressionCollection().Expressions;
	for (UMaterialExpression* OtherExpr : Expressions)
	{
		if (!OtherExpr || OtherExpr == Expr) continue;
		TArray<FExpressionInput*> Inputs = OtherExpr->GetInputs();
		for (FExpressionInput* Input : Inputs)
		{
			if (Input && Input->Expression == Expr)
			{
				Input->Expression = nullptr;
				Input->OutputIndex = 0;
			}
		}
	}

	// Clear material output connections referencing this expression
	// Check all material property inputs
	static const EMaterialProperty MaterialProperties[] = {
		MP_BaseColor, MP_Metallic, MP_Specular, MP_Roughness, MP_Anisotropy,
		MP_EmissiveColor, MP_Opacity, MP_OpacityMask, MP_Normal,
		MP_Tangent, MP_WorldPositionOffset, MP_Refraction,
		MP_AmbientOcclusion, MP_PixelDepthOffset, MP_ShadingModel,
		MP_FrontMaterial, MP_SurfaceThickness, MP_Displacement
	};

	for (EMaterialProperty Prop : MaterialProperties)
	{
		FExpressionInput* MatInput = Material->GetExpressionInputForProperty(Prop);
		if (MatInput && MatInput->Expression == Expr)
		{
			MatInput->Expression = nullptr;
			MatInput->OutputIndex = 0;
		}
	}

	// Remove from expressions array
	Expressions.Remove(Expr);

	Material->PostEditChange();
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Deleted expression '%s'"), *NodeId));
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleListExpressions(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return MakeErrorResponse(TEXT("asset_path is required"));
	}

	UMaterial* Material = FindMaterial(AssetPath);
	if (!Material)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Material not found at '%s'"), *AssetPath));
	}

	const TArray<TObjectPtr<UMaterialExpression>>& Expressions = Material->GetExpressionCollection().Expressions;
	TArray<TSharedPtr<FJsonValue>> ExprArray;

	for (int32 i = 0; i < Expressions.Num(); i++)
	{
		if (Expressions[i])
		{
			TSharedPtr<FJsonObject> ExprJson = MakeExpressionJson(Expressions[i]);
			ExprJson->SetNumberField(TEXT("index"), i);
			ExprArray.Add(MakeShareable(new FJsonValueObject(ExprJson)));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("count"), Expressions.Num());
	Result->SetArrayField(TEXT("expressions"), ExprArray);
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleListPins(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString NodeId = Params->GetStringField(TEXT("node_id"));

	if (AssetPath.IsEmpty() || NodeId.IsEmpty())
	{
		return MakeErrorResponse(TEXT("asset_path and node_id are required"));
	}

	UMaterial* Material = FindMaterial(AssetPath);
	if (!Material)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Material not found at '%s'"), *AssetPath));
	}

	UMaterialExpression* Expression = FindExpression(Material, NodeId);
	if (!Expression)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Expression '%s' not found"), *NodeId));
	}

	// Get inputs
	TArray<TSharedPtr<FJsonValue>> InputArray;
	TArray<FExpressionInput*> Inputs = Expression->GetInputs();
	for (int32 i = 0; i < Inputs.Num(); i++)
	{
		TSharedPtr<FJsonObject> PinJson = MakeShareable(new FJsonObject);
		FName InputName = Expression->GetInputName(i);
		PinJson->SetStringField(TEXT("name"), InputName.ToString());
		PinJson->SetNumberField(TEXT("index"), i);
		PinJson->SetStringField(TEXT("direction"), TEXT("input"));

		bool bConnected = Inputs[i]->Expression != nullptr;
		PinJson->SetBoolField(TEXT("connected"), bConnected);
		if (bConnected)
		{
			PinJson->SetStringField(TEXT("connected_to_class"), Inputs[i]->Expression->GetClass()->GetName());
			PinJson->SetStringField(TEXT("connected_to_name"), Inputs[i]->Expression->GetName());
			PinJson->SetNumberField(TEXT("connected_output_index"), Inputs[i]->OutputIndex);
		}

		InputArray.Add(MakeShareable(new FJsonValueObject(PinJson)));
	}

	// Get outputs
	TArray<TSharedPtr<FJsonValue>> OutputArray;
	TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
	for (int32 i = 0; i < Outputs.Num(); i++)
	{
		TSharedPtr<FJsonObject> PinJson = MakeShareable(new FJsonObject);
		PinJson->SetStringField(TEXT("name"), Outputs[i].OutputName.ToString());
		PinJson->SetNumberField(TEXT("index"), i);
		PinJson->SetStringField(TEXT("direction"), TEXT("output"));

		// Output mask info
		PinJson->SetNumberField(TEXT("mask"), Outputs[i].Mask);
		PinJson->SetNumberField(TEXT("mask_r"), Outputs[i].MaskR);
		PinJson->SetNumberField(TEXT("mask_g"), Outputs[i].MaskG);
		PinJson->SetNumberField(TEXT("mask_b"), Outputs[i].MaskB);
		PinJson->SetNumberField(TEXT("mask_a"), Outputs[i].MaskA);

		OutputArray.Add(MakeShareable(new FJsonValueObject(PinJson)));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("expression_class"), Expression->GetClass()->GetName());
	Result->SetStringField(TEXT("expression_name"), Expression->GetName());
	Result->SetArrayField(TEXT("inputs"), InputArray);
	Result->SetArrayField(TEXT("outputs"), OutputArray);
	Result->SetNumberField(TEXT("input_count"), Inputs.Num());
	Result->SetNumberField(TEXT("output_count"), Outputs.Num());
	return Result;
}

// ============================================================================
// Connections
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleConnectExpressions(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SourceNodeId = Params->GetStringField(TEXT("source_node_id"));
	FString TargetNodeId = Params->GetStringField(TEXT("target_node_id"));
	FString InputPinName = Params->GetStringField(TEXT("input_pin_name"));
	int32 OutputIndex = Params->HasField(TEXT("output_index")) ? static_cast<int32>(Params->GetNumberField(TEXT("output_index"))) : 0;

	if (AssetPath.IsEmpty() || SourceNodeId.IsEmpty() || TargetNodeId.IsEmpty())
	{
		return MakeErrorResponse(TEXT("asset_path, source_node_id, and target_node_id are required"));
	}

	UMaterial* Material = FindMaterial(AssetPath);
	if (!Material)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Material not found at '%s'"), *AssetPath));
	}

	UMaterialExpression* SourceExpr = FindExpression(Material, SourceNodeId);
	if (!SourceExpr)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Source expression '%s' not found"), *SourceNodeId));
	}

	UMaterialExpression* TargetExpr = FindExpression(Material, TargetNodeId);
	if (!TargetExpr)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Target expression '%s' not found"), *TargetNodeId));
	}

	Material->PreEditChange(nullptr);

	bool bConnected = false;

	if (!InputPinName.IsEmpty())
	{
		// Find input by name
		TArray<FExpressionInput*> Inputs = TargetExpr->GetInputs();
		for (int32 i = 0; i < Inputs.Num(); i++)
		{
			FName InputName = TargetExpr->GetInputName(i);
			if (InputName.ToString() == InputPinName)
			{
				Inputs[i]->Expression = SourceExpr;
				Inputs[i]->OutputIndex = OutputIndex;
				bConnected = true;
				break;
			}
		}
	}
	else if (Params->HasField(TEXT("input_index")))
	{
		// Find input by index
		int32 InputIndex = static_cast<int32>(Params->GetNumberField(TEXT("input_index")));
		TArray<FExpressionInput*> Inputs = TargetExpr->GetInputs();
		if (Inputs.IsValidIndex(InputIndex))
		{
			Inputs[InputIndex]->Expression = SourceExpr;
			Inputs[InputIndex]->OutputIndex = OutputIndex;
			bConnected = true;
		}
	}
	else
	{
		// Default: connect to first input
		TArray<FExpressionInput*> Inputs = TargetExpr->GetInputs();
		if (Inputs.Num() > 0)
		{
			Inputs[0]->Expression = SourceExpr;
			Inputs[0]->OutputIndex = OutputIndex;
			bConnected = true;
		}
	}

	if (!bConnected)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Could not find input pin '%s' on target expression"), *InputPinName));
	}

	Material->PostEditChange();
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Connected %s -> %s (pin: %s, output_index: %d)"),
		*SourceNodeId, *TargetNodeId, *InputPinName, OutputIndex));
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleConnectToOutput(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString SourceNodeId = Params->GetStringField(TEXT("source_node_id"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	int32 OutputIndex = Params->HasField(TEXT("output_index")) ? static_cast<int32>(Params->GetNumberField(TEXT("output_index"))) : 0;

	if (AssetPath.IsEmpty() || SourceNodeId.IsEmpty() || PropertyName.IsEmpty())
	{
		return MakeErrorResponse(TEXT("asset_path, source_node_id, and property_name are required"));
	}

	UMaterial* Material = FindMaterial(AssetPath);
	if (!Material)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Material not found at '%s'"), *AssetPath));
	}

	UMaterialExpression* SourceExpr = FindExpression(Material, SourceNodeId);
	if (!SourceExpr)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Source expression '%s' not found"), *SourceNodeId));
	}

	Material->PreEditChange(nullptr);

	// Map property name to EMaterialProperty
	FExpressionInput* MaterialInput = nullptr;

	if (PropertyName == TEXT("BaseColor")) MaterialInput = Material->GetExpressionInputForProperty(MP_BaseColor);
	else if (PropertyName == TEXT("Metallic")) MaterialInput = Material->GetExpressionInputForProperty(MP_Metallic);
	else if (PropertyName == TEXT("Specular")) MaterialInput = Material->GetExpressionInputForProperty(MP_Specular);
	else if (PropertyName == TEXT("Roughness")) MaterialInput = Material->GetExpressionInputForProperty(MP_Roughness);
	else if (PropertyName == TEXT("Anisotropy")) MaterialInput = Material->GetExpressionInputForProperty(MP_Anisotropy);
	else if (PropertyName == TEXT("EmissiveColor")) MaterialInput = Material->GetExpressionInputForProperty(MP_EmissiveColor);
	else if (PropertyName == TEXT("Opacity")) MaterialInput = Material->GetExpressionInputForProperty(MP_Opacity);
	else if (PropertyName == TEXT("OpacityMask")) MaterialInput = Material->GetExpressionInputForProperty(MP_OpacityMask);
	else if (PropertyName == TEXT("Normal")) MaterialInput = Material->GetExpressionInputForProperty(MP_Normal);
	else if (PropertyName == TEXT("Tangent")) MaterialInput = Material->GetExpressionInputForProperty(MP_Tangent);
	else if (PropertyName == TEXT("WorldPositionOffset")) MaterialInput = Material->GetExpressionInputForProperty(MP_WorldPositionOffset);
	else if (PropertyName == TEXT("Refraction")) MaterialInput = Material->GetExpressionInputForProperty(MP_Refraction);
	else if (PropertyName == TEXT("AmbientOcclusion")) MaterialInput = Material->GetExpressionInputForProperty(MP_AmbientOcclusion);
	else if (PropertyName == TEXT("PixelDepthOffset")) MaterialInput = Material->GetExpressionInputForProperty(MP_PixelDepthOffset);
	else if (PropertyName == TEXT("ShadingModel")) MaterialInput = Material->GetExpressionInputForProperty(MP_ShadingModel);
	else if (PropertyName == TEXT("FrontMaterial")) MaterialInput = Material->GetExpressionInputForProperty(MP_FrontMaterial);
	else if (PropertyName == TEXT("SurfaceThickness")) MaterialInput = Material->GetExpressionInputForProperty(MP_SurfaceThickness);
	else if (PropertyName == TEXT("Displacement")) MaterialInput = Material->GetExpressionInputForProperty(MP_Displacement);

	if (!MaterialInput)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Unknown material output property '%s'"), *PropertyName));
	}

	MaterialInput->Expression = SourceExpr;
	MaterialInput->OutputIndex = OutputIndex;

	Material->PostEditChange();
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Connected expression '%s' to material output '%s'"), *SourceNodeId, *PropertyName));
	return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleDisconnectExpression(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString NodeId = Params->GetStringField(TEXT("node_id"));
	FString InputPinName = Params->GetStringField(TEXT("input_pin_name"));

	if (AssetPath.IsEmpty() || NodeId.IsEmpty())
	{
		return MakeErrorResponse(TEXT("asset_path and node_id are required"));
	}

	UMaterial* Material = FindMaterial(AssetPath);
	if (!Material)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Material not found at '%s'"), *AssetPath));
	}

	UMaterialExpression* Expression = FindExpression(Material, NodeId);
	if (!Expression)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Expression '%s' not found"), *NodeId));
	}

	Material->PreEditChange(nullptr);

	bool bDisconnected = false;

	if (!InputPinName.IsEmpty())
	{
		// Disconnect specific input pin by name
		TArray<FExpressionInput*> Inputs = Expression->GetInputs();
		for (int32 i = 0; i < Inputs.Num(); i++)
		{
			FName InputName = Expression->GetInputName(i);
			if (InputName.ToString() == InputPinName)
			{
				Inputs[i]->Expression = nullptr;
				Inputs[i]->OutputIndex = 0;
				bDisconnected = true;
				break;
			}
		}
	}
	else if (Params->HasField(TEXT("input_index")))
	{
		// Disconnect by index
		int32 InputIndex = static_cast<int32>(Params->GetNumberField(TEXT("input_index")));
		TArray<FExpressionInput*> Inputs = Expression->GetInputs();
		if (Inputs.IsValidIndex(InputIndex))
		{
			Inputs[InputIndex]->Expression = nullptr;
			Inputs[InputIndex]->OutputIndex = 0;
			bDisconnected = true;
		}
	}
	else
	{
		// Disconnect all inputs
		TArray<FExpressionInput*> Inputs = Expression->GetInputs();
		for (FExpressionInput* Input : Inputs)
		{
			if (Input)
			{
				Input->Expression = nullptr;
				Input->OutputIndex = 0;
			}
		}
		bDisconnected = true;
	}

	if (!bDisconnected)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Could not find input pin '%s' on expression '%s'"), *InputPinName, *NodeId));
	}

	Material->PostEditChange();
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Disconnected input on expression '%s'"), *NodeId));
	return Result;
}

// ============================================================================
// Set Expression Values
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleSetExpressionValue(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString NodeId = Params->GetStringField(TEXT("node_id"));

	if (AssetPath.IsEmpty() || NodeId.IsEmpty())
	{
		return MakeErrorResponse(TEXT("asset_path and node_id are required"));
	}

	UMaterial* Material = FindMaterial(AssetPath);
	if (!Material)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Material not found at '%s'"), *AssetPath));
	}

	UMaterialExpression* Expression = FindExpression(Material, NodeId);
	if (!Expression)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Expression '%s' not found"), *NodeId));
	}

	Material->PreEditChange(nullptr);

	bool bValueSet = false;
	FString ValueDescription;

	// MaterialExpressionConstant - set R
	if (UMaterialExpressionConstant* ConstExpr = Cast<UMaterialExpressionConstant>(Expression))
	{
		if (Params->HasField(TEXT("value")))
		{
			ConstExpr->R = static_cast<float>(Params->GetNumberField(TEXT("value")));
			ValueDescription = FString::Printf(TEXT("R = %f"), ConstExpr->R);
			bValueSet = true;
		}
		if (Params->HasField(TEXT("r")))
		{
			ConstExpr->R = static_cast<float>(Params->GetNumberField(TEXT("r")));
			ValueDescription = FString::Printf(TEXT("R = %f"), ConstExpr->R);
			bValueSet = true;
		}
	}
	// MaterialExpressionConstant3Vector - set Constant (LinearColor)
	else if (UMaterialExpressionConstant3Vector* Const3Expr = Cast<UMaterialExpressionConstant3Vector>(Expression))
	{
		if (Params->HasField(TEXT("color")))
		{
			const TArray<TSharedPtr<FJsonValue>>& ColorArr = Params->GetArrayField(TEXT("color"));
			if (ColorArr.Num() >= 3)
			{
				float R = static_cast<float>(ColorArr[0]->AsNumber());
				float G = static_cast<float>(ColorArr[1]->AsNumber());
				float B = static_cast<float>(ColorArr[2]->AsNumber());
				float A = ColorArr.Num() >= 4 ? static_cast<float>(ColorArr[3]->AsNumber()) : 1.0f;
				Const3Expr->Constant = FLinearColor(R, G, B, A);
				ValueDescription = FString::Printf(TEXT("Constant = (%f, %f, %f, %f)"), R, G, B, A);
				bValueSet = true;
			}
		}
		if (Params->HasField(TEXT("value")))
		{
			const TArray<TSharedPtr<FJsonValue>>& ValArr = Params->GetArrayField(TEXT("value"));
			if (ValArr.Num() >= 3)
			{
				float R = static_cast<float>(ValArr[0]->AsNumber());
				float G = static_cast<float>(ValArr[1]->AsNumber());
				float B = static_cast<float>(ValArr[2]->AsNumber());
				float A = ValArr.Num() >= 4 ? static_cast<float>(ValArr[3]->AsNumber()) : 1.0f;
				Const3Expr->Constant = FLinearColor(R, G, B, A);
				ValueDescription = FString::Printf(TEXT("Constant = (%f, %f, %f, %f)"), R, G, B, A);
				bValueSet = true;
			}
		}
	}
	// MaterialExpressionScalarParameter - set DefaultValue
	else if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
	{
		if (Params->HasField(TEXT("value")) || Params->HasField(TEXT("default_value")))
		{
			float Val = static_cast<float>(Params->GetNumberField(Params->HasField(TEXT("default_value")) ? TEXT("default_value") : TEXT("value")));
			ScalarParam->DefaultValue = Val;
			ValueDescription = FString::Printf(TEXT("DefaultValue = %f"), Val);
			bValueSet = true;
		}
		if (Params->HasField(TEXT("parameter_name")))
		{
			ScalarParam->SetParameterName(FName(*Params->GetStringField(TEXT("parameter_name"))));
			bValueSet = true;
		}
	}
	// MaterialExpressionVectorParameter - set DefaultValue
	else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression))
	{
		if (Params->HasField(TEXT("value")) || Params->HasField(TEXT("default_value")))
		{
			FString FieldName = Params->HasField(TEXT("default_value")) ? TEXT("default_value") : TEXT("value");
			const TArray<TSharedPtr<FJsonValue>>& ValArr = Params->GetArrayField(FieldName);
			if (ValArr.Num() >= 3)
			{
				float R = static_cast<float>(ValArr[0]->AsNumber());
				float G = static_cast<float>(ValArr[1]->AsNumber());
				float B = static_cast<float>(ValArr[2]->AsNumber());
				float A = ValArr.Num() >= 4 ? static_cast<float>(ValArr[3]->AsNumber()) : 1.0f;
				VectorParam->DefaultValue = FLinearColor(R, G, B, A);
				ValueDescription = FString::Printf(TEXT("DefaultValue = (%f, %f, %f, %f)"), R, G, B, A);
				bValueSet = true;
			}
		}
		if (Params->HasField(TEXT("parameter_name")))
		{
			VectorParam->SetParameterName(FName(*Params->GetStringField(TEXT("parameter_name"))));
			bValueSet = true;
		}
	}
	// MaterialExpressionFresnel - set Exponent and BaseReflectFractionIn
	else if (UMaterialExpressionFresnel* FresnelExpr = Cast<UMaterialExpressionFresnel>(Expression))
	{
		if (Params->HasField(TEXT("exponent")))
		{
			FresnelExpr->Exponent = static_cast<float>(Params->GetNumberField(TEXT("exponent")));
			ValueDescription += FString::Printf(TEXT("Exponent = %f "), FresnelExpr->Exponent);
			bValueSet = true;
		}
		if (Params->HasField(TEXT("base_reflect_fraction")))
		{
			FresnelExpr->BaseReflectFractionIn = static_cast<float>(Params->GetNumberField(TEXT("base_reflect_fraction")));
			ValueDescription += FString::Printf(TEXT("BaseReflectFractionIn = %f"), FresnelExpr->BaseReflectFractionIn);
			bValueSet = true;
		}
	}

	// Generic fallback: try setting common property names via UObject property system
	if (!bValueSet && Params->HasField(TEXT("property_name")) && Params->HasField(TEXT("property_value")))
	{
		FString PropName = Params->GetStringField(TEXT("property_name"));
		FString PropValue = Params->GetStringField(TEXT("property_value"));

		FProperty* Property = Expression->GetClass()->FindPropertyByName(FName(*PropName));
		if (Property)
		{
			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Expression);
			if (Property->ImportText_Direct(*PropValue, ValuePtr, Expression, PPF_None))
			{
				ValueDescription = FString::Printf(TEXT("%s = %s"), *PropName, *PropValue);
				bValueSet = true;
			}
		}
	}

	if (!bValueSet)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Could not set value on expression '%s' (class: %s). Provide recognized params or use property_name/property_value."),
			*NodeId, *Expression->GetClass()->GetName()));
	}

	Material->PostEditChange();
	Material->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Set value on expression '%s': %s"), *NodeId, *ValueDescription));
	return Result;
}

// ============================================================================
// Compile Material
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleCompileMaterial(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	if (AssetPath.IsEmpty())
	{
		return MakeErrorResponse(TEXT("asset_path is required"));
	}

	UMaterial* Material = FindMaterial(AssetPath);
	if (!Material)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Material not found at '%s'"), *AssetPath));
	}

	// Trigger recompilation
	{
		FMaterialUpdateContext UpdateContext;
		UpdateContext.AddMaterial(Material);

		Material->PreEditChange(nullptr);
		Material->PostEditChange();
		Material->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Material '%s' recompiled"), *AssetPath));
	return Result;
}

// ============================================================================
// Apply Material to Actor
// ============================================================================

TSharedPtr<FJsonObject> FUnrealMCPMaterialCommands::HandleApplyMaterialToActor(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString ActorName = Params->GetStringField(TEXT("actor_name"));
	int32 SlotIndex = Params->HasField(TEXT("slot_index")) ? static_cast<int32>(Params->GetNumberField(TEXT("slot_index"))) : 0;

	if (AssetPath.IsEmpty() || ActorName.IsEmpty())
	{
		return MakeErrorResponse(TEXT("asset_path and actor_name are required"));
	}

	UMaterial* Material = FindMaterial(AssetPath);
	if (!Material)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Material not found at '%s'"), *AssetPath));
	}

	// Find actor in the current world
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeErrorResponse(TEXT("No editor world available"));
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetActorLabel() == ActorName || It->GetName() == ActorName || It->GetFName().ToString() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		return MakeErrorResponse(FString::Printf(TEXT("Actor '%s' not found in level"), *ActorName));
	}

	// Find mesh component on the actor
	UMeshComponent* MeshComp = nullptr;

	// Try StaticMeshComponent first
	MeshComp = FoundActor->FindComponentByClass<UStaticMeshComponent>();

	// Try DynamicMeshComponent
	if (!MeshComp)
	{
		MeshComp = FoundActor->FindComponentByClass<UDynamicMeshComponent>();
	}

	// Fall back to any mesh component
	if (!MeshComp)
	{
		MeshComp = FoundActor->FindComponentByClass<UMeshComponent>();
	}

	if (!MeshComp)
	{
		return MakeErrorResponse(FString::Printf(TEXT("No mesh component found on actor '%s'"), *ActorName));
	}

	MeshComp->SetMaterial(SlotIndex, Material);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Applied material '%s' to actor '%s' at slot %d"),
		*AssetPath, *ActorName, SlotIndex));
	return Result;
}
