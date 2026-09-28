using UnrealBuildTool;
using System.IO;
public class BOTFRemake : ModuleRules
{
    public BOTFRemake(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;
        bUseUnity = false;
        bEnableExceptions = true;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore" });
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "BotfSim"));
    }
}
