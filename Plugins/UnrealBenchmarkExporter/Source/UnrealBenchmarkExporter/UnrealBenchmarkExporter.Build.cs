using UnrealBuildTool;

public class UnrealBenchmarkExporter : ModuleRules
{
	public UnrealBenchmarkExporter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		bEnableExceptions = false;
		bUseRTTI = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTP",
			"Json",
			"JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"RenderCore",
			"RHI",
			"ApplicationCore"
		});

		// Platform-specific configuration
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.AddRange(new string[]
			{
				"kernel32.lib",
				"ws2_32.lib",
				"iphlpapi.lib",
				"userenv.lib",
				"psapi.lib",  // For memory queries
				"pdh.lib"     // For CPU monitoring
				// Note: dxgi.lib and d3d11.lib are loaded dynamically at runtime
			});
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicSystemLibraries.AddRange(new string[]
			{
				"pthread",
				"dl",
				"rt",
				"m"
			});
		}
	}
}