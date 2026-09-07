using UnrealBuildTool;
using System.Collections.Generic;
public class Outpost2DTarget : TargetRules
{
    public Outpost2DTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("Outpost2D");
    }
}
