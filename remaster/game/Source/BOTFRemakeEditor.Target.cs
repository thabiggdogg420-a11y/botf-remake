using UnrealBuildTool;
public class BOTFRemakeEditorTarget : TargetRules
{
    public BOTFRemakeEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("BOTFRemake");
    }
}
