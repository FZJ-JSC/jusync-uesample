
using UnrealBuildTool;
using System.Collections.Generic;

public class JusyncSampleProjectEditorTarget : TargetRules
{
	public JusyncSampleProjectEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Error;
		ExtraModuleNames.Add("JusyncSampleProject");
	}
}
