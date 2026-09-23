using UnrealBuildTool;

public class PixelRacerCore : ModuleRules
{
    public PixelRacerCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Json",
            "JsonUtilities",
            "Paper2D",
            "InputCore"
        });
    }
}
