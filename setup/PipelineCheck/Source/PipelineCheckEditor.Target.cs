using UnrealBuildTool;
public class PipelineCheckEditorTarget : TargetRules
{
    public PipelineCheckEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("PipelineCheck");
    }
}
