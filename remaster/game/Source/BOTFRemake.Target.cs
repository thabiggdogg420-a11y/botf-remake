using UnrealBuildTool;
public class BOTFRemakeTarget : TargetRules
{
    public BOTFRemakeTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("BOTFRemake");
    }
}
