# Browser-target check (M0 L4): runs the UI suite and the view-model suite in a real window on the Compatibility
# renderer (gl_compatibility, the renderer of the web export), with dummy audio. Exit code 0 when both pass.
#   powershell -ExecutionPolicy Bypass -File remaster\godot\scenes\ui\tests\run_ui_compat.ps1
param([string]$Godot = "C:\Tools\Godot\Godot_v4.5-stable_win64_console.exe")
$proj = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$env:BOTF_EXPECT_RENDERER = "gl_compatibility"
$fail = 0
foreach ($suite in @("res://scenes/ui/tests/test_campaign_ui.gd", "res://scripts/viewmodels/tests/test_viewmodels.gd")) {
    Write-Host "== $suite (gl_compatibility)"
    & $Godot --path $proj --rendering-method gl_compatibility --audio-driver Dummy --resolution 1920x1080 -s $suite 2>&1 |
        Select-String -Pattern "renderer:|FAIL|SCRIPT ERROR|checks" | ForEach-Object { $_.Line }
    if ($LASTEXITCODE -ne 0) { $fail = 1 }
}
Remove-Item Env:\BOTF_EXPECT_RENDERER
exit $fail
