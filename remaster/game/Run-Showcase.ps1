<#
.SYNOPSIS
    Renders the offscreen presentation showcase: a screenshot of every screen and of a rules 3 battle in each camera
    mode, plus a frame-time report. Nothing opens on screen.

.DESCRIPTION
    Runs the editor build in -game mode with -RenderOffscreen and -BotfShowcase (BotfGameMode.cpp, FBotfShowcase;
    development builds only). Build the editor target first (Package-Windows.ps1 -SkipPackage, or Build.bat).
    Output: <OutDir>\NN-<step>.png and showcase-report.txt (the last line holds the battle's frame times).

.EXAMPLE
    .\Run-Showcase.ps1
.EXAMPLE
    .\Run-Showcase.ps1 -OutDir C:\Dev\botf-shots\run2 -Ships 24 -ResX 2560 -ResY 1440
#>
param(
    [string]$OutDir = '',
    [int]$Ships = 24,
    [int]$ResX = 1920,
    [int]$ResY = 1080,
    [string]$EngineRoot = '',
    [int]$TimeoutSeconds = 600
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BotfTools.ps1')
$engine = if ($EngineRoot) { $EngineRoot } else { Resolve-BotfEngineRoot }
$editor = Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $editor)) { throw "UnrealEditor.exe not found under $engine" }
if (-not $OutDir) { $OutDir = Join-Path $PSScriptRoot ('Build\Showcase\' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
if ((Test-Path -LiteralPath $OutDir) -and @(Get-ChildItem -LiteralPath $OutDir -Force).Count) {
    throw "$OutDir is not empty: pick a new -OutDir (earlier runs are kept, never overwritten)"
}
$project = Join-Path $PSScriptRoot 'BOTFRemake.uproject'
$arguments = @("`"$project`"", '-game', "-BotfShowcase=`"$OutDir`"", "-BotfShowcaseShips=$Ships", '-RenderOffscreen',
    "-ResX=$ResX", "-ResY=$ResY", '-unattended', '-nosplash', '-NoSound', '-log')
$process = Start-Process -FilePath $editor -ArgumentList $arguments -PassThru
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { $process.Kill(); throw "Showcase timed out after $TimeoutSeconds s" }
$report = Join-Path $OutDir 'showcase-report.txt'
if (-not (Test-Path -LiteralPath $report)) { throw "No showcase report: the run did not finish (see Saved\Logs\BOTFRemake.log)" }
Get-Content -LiteralPath $report | Where-Object { $_ -notmatch ' shot ' }
$errors = @(Select-String -LiteralPath $report -Pattern 'BOTF_SHOWCASE_ERROR')
Write-Host ("{0} screenshots in {1}" -f @(Get-ChildItem -LiteralPath $OutDir -Filter *.png).Count, $OutDir)
if ($errors.Count) { exit 1 }
