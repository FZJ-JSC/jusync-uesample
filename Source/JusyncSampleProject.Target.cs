// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class JusyncSampleProjectTarget : TargetRules
{
	public JusyncSampleProjectTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Error;
		ExtraModuleNames.Add("JusyncSampleProject");
	}
}
