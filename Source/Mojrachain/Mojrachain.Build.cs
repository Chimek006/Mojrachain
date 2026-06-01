using UnrealBuildTool;

public class Mojrachain : ModuleRules
{
    public Mojrachain(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "ProceduralMeshComponent",
            "UMG"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });
    }
}