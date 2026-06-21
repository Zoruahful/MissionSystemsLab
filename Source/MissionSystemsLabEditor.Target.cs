using UnrealBuildTool;
using System.Collections.Generic;

public class MissionSystemsLabEditorTarget : TargetRules
{
	public MissionSystemsLabEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MissionSystemsLab");
	}
}
