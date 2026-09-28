param(
    # Unreal Engine root. Default: $env:UE_ROOT, else the standard install of the project's engine version.
    [string]$EngineRoot = '',
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BotfTools.ps1')
$EngineRoot = Resolve-BotfEngineRoot $EngineRoot
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'Build-Unreal.ps1') -EngineRoot $EngineRoot }
$taskStamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$taskOutput = Join-Path $PSScriptRoot "Build\UiTests\$taskStamp"
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$taskLog = Join-Path $taskOutput 'automation.log'
$taskStdout = Join-Path $taskOutput 'stdout.log'
$taskProject = Join-Path $PSScriptRoot 'BOTFRemake.uproject'
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $taskProject `
    -NullRHI -Unattended -NoSound '-ExecCmds=Automation RunTests BOTF.UI.' `
    '-TestExit=Automation Test Queue Empty' "-abslog=$taskLog" -stdout -FullStdOutLogOutput *> $taskStdout
$taskExitCode = $LASTEXITCODE
if (-not (Test-Path -LiteralPath $taskLog)) { throw "No automation log produced. See $taskStdout" }
$taskText = [IO.File]::ReadAllText($taskLog)
# Unreal can return zero even when an automation assertion fails. Require actual results.
$taskResults = [regex]::Matches($taskText, 'Test Completed\. Result=\{([^}]+)\} Name=\{[^}]+\} Path=\{(BOTF\.UI\.[^}]+)\}')
if ($taskExitCode -ne 0 -or $taskResults.Count -eq 0) { throw "UI automation did not complete successfully (exit $taskExitCode). See $taskLog" }
$taskFailures = @($taskResults | Where-Object { $_.Groups[1].Value -ne 'Success' })
if ($taskFailures.Count -gt 0) { throw "UI automation reported $($taskFailures.Count) failing tests. See $taskLog" }
Write-Output "$($taskResults.Count) UI automation tests passed. Log: $taskLog"
