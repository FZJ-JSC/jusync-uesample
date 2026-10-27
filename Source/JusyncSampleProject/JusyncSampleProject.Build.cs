// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class JusyncSampleProject : ModuleRules
{
	public JusyncSampleProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Test sources live in Tests/, so include the module root explicitly
		// instead of relying on relative include paths.
		PublicIncludePaths.Add(ModuleDirectory);
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "JUSYNC" });

		// The playback HUD builds its own UMG tree in C++, so this module
		// needs UMG plus the Slate widgets it constructs.
		//
		// LidarPointCloudRuntime is here because the JUSYNC public headers
		// (JUSYNCTypes.h, JUSYNCPointCloudSpawner.h) include its headers while
		// the plugin only declares it as a private dependency; without this,
		// including JUSYNCFileSpawnerActor.h from outside the plugin cannot
		// resolve LidarPointCloudShared.h.
		PrivateDependencyModuleNames.AddRange(new string[] { "UMG", "Slate", "SlateCore", "LidarPointCloudRuntime" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
