using UnrealBuildTool;
using System.Collections.Generic;

public class DriftGameTarget : TargetRules
{
	public DriftGameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DriftGame");
	}
}
