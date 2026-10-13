// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class JusyncSampleProjectTarget : TargetRules
{
	public JusyncSampleProjectTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		UndefinedIdentifierWarningLevel = WarningLevel.Error;
		ExtraModuleNames.Add("JusyncSampleProject");
	}
}
