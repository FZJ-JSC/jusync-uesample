
using UnrealBuildTool;
using System.IO;

public class JusyncSampleProject : ModuleRules
{
	public JusyncSampleProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "JUSYNC" });

		PrivateDependencyModuleNames.AddRange(new string[] { "UMG", "Slate", "SlateCore", "LidarPointCloudRuntime" });

	}
}
