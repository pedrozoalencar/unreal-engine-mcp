// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealMCP : ModuleRules
{
	public UnrealMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDefinitions.Add("UNREALMCP_EXPORTS=1");

		PublicIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "Public"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/BlueprintGraph"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/BlueprintGraph/Nodes"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/GeometryScript"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/PythonExec"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/Material"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/Level"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/Asset"),
				System.IO.Path.Combine(ModuleDirectory, "Public/Commands/Introspection")
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "Private"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/BlueprintGraph"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/BlueprintGraph/Nodes"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/GeometryScript"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/PythonExec"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/Material"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/Level"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/Asset"),
				System.IO.Path.Combine(ModuleDirectory, "Private/Commands/Introspection")
			}
		);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Networking",
				"Sockets",
				"HTTP",
				"Json",
				"JsonUtilities",
				"DeveloperSettings",
				"PhysicsCore",
				"UnrealEd",           // For Blueprint editing
				"BlueprintGraph",     // For K2Node classes
				"KismetCompiler",     // For Blueprint compilation
				// Geometry Script modules
				"GeometryScriptingCore",  // UGeometryScriptLibrary_* classes
				"GeometryCore",           // FDynamicMesh3, geometric types
				"GeometryFramework",      // UDynamicMesh
				"DynamicMesh",            // Dynamic mesh infrastructure
				"ModelingComponents"      // UDynamicMeshComponent
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"Slate",
				"SlateCore",
				"Kismet",
				"Projects",
				"AssetRegistry",
				"MeshDescription",         // For mesh conversion
				"StaticMeshDescription"    // For static mesh conversion
			}
		);
		
		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"PropertyEditor",      // For property editing
					"ToolMenus",           // For editor UI
					"BlueprintEditorLibrary" // For Blueprint utilities
				}
			);
		}
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				"PythonScriptPlugin"  // Loaded dynamically for Python exec commands
			}
		);

		// Add Python plugin include paths (loaded dynamically, not linked)
		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"PythonScriptPlugin"
			}
		);
	}
} 