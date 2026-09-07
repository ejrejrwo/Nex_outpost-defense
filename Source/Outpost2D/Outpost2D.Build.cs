using UnrealBuildTool;
public class Outpost2D : ModuleRules
{
    public Outpost2D(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore" });
        PrivateDependencyModuleNames.Add("RenderCore");
    }
}
