using UnrealBuildTool;
using System.IO;
public class BOTFRemake : ModuleRules
{
    public BOTFRemake(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;
        bUseUnity = false;
        bEnableExceptions = true;
        // The core is integer-only in outcome math, but the client compiles the same sources (SimulationBridge.cpp):
        // keep strict floating-point semantics so no compiler contraction or fast-math reordering can ever differ
        // from the standalone GCC/Clang/MSVC builds. FPSemantics requires NoPCHs or a private PCH (NoPCHs above).
        FPSemantics = FPSemanticsMode.Precise;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore" });
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "BotfSim"));
    }
}
