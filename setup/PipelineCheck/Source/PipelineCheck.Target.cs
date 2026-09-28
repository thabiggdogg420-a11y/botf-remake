using UnrealBuildTool;
public class PipelineCheckTarget : TargetRules
{
    public PipelineCheckTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("PipelineCheck");
    }
}
