using UnrealBuildTool;

public class DriftGame : ModuleRules
{
	public DriftGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore",
			"EnhancedInput", "PhysicsCore", "ProceduralMeshComponent", "RenderCore"
		});
		// Slate fonts for the menu (crisp text at any size, letter spacing, measuring).
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
	}
}
