using UnrealBuildTool;

public class PixelRacerTrackEditor : ModuleRules
{
    public PixelRacerTrackEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "PixelRacerCore"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "UnrealEd",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "LevelEditor",
            "Projects",
            "ContentBrowser",
            "AssetTools",
            "AssetRegistry",
            "InputCore",
            "Json",
            "Paper2D",
            "Paper2DEditor"
        });
    }
}
