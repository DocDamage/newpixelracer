using UnrealBuildTool;

public class PixelRacerRuntimeEditor : ModuleRules
{
    public PixelRacerRuntimeEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "PixelRacerCore"
        });
    }
}
