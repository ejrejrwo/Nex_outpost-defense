using UnrealBuildTool;
using System.Collections.Generic;
public class Outpost2DEditorTarget : TargetRules
{
    public Outpost2DEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("Outpost2D");
    }
}
