param([switch]$Test, [switch]$Tactical, [switch]$Sessions, [switch]$Intelligence, [switch]$Diplomacy, [switch]$Gameplay)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskBuild = Join-Path $PSScriptRoot 'Build'
$taskCore = Join-Path $PSScriptRoot 'Source\BotfSim'
$taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $taskVswhere)) { throw 'Visual Studio Installer/vswhere is required.' }
$taskVisualStudio = & $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $taskVisualStudio) { throw 'MSVC C++ Build Tools were not found.' }
$taskDevCmd = Join-Path $taskVisualStudio 'Common7\Tools\VsDevCmd.bat'
# The batch file only discovers the compiler environment; compilation uses argument arrays.
$taskEnvironment = & $env:ComSpec /d /s /c "`"$taskDevCmd`" -no_logo -arch=x64 -host_arch=x64 && set"
if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the MSVC environment.' }
$taskCompilerPath = $null
foreach ($taskLine in $taskEnvironment) {
    if ($taskLine -match '^([^=]+)=(.*)$') {
        # Some hosts supply both PATH and Path; VsDevCmd updates uppercase PATH.
        if ($Matches[1] -ieq 'PATH') {
            if ($Matches[1] -ceq 'PATH' -or -not $taskCompilerPath) { $taskCompilerPath = $Matches[2] }
            continue
        }
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
}
if (-not $taskCompilerPath) { throw 'Compiler environment did not provide PATH.' }
$env:Path = $taskCompilerPath
$taskCompiler = Join-Path $env:VCToolsInstallDir 'bin\Hostx64\x64\cl.exe'
New-Item -ItemType Directory -Path $taskBuild -Force | Out-Null
$taskCommon = @('/nologo', '/std:c++20', '/EHsc', '/W4', '/WX', '/permissive-', '/utf-8', '/O2', "/I$taskCore")
# Core translation units: Simulation.cpp plus the schema-13 gameplay layer (keep in
# sync with BOTF_CORE_SOURCES in remaster/CMakeLists.txt and SimulationBridge.cpp).
$taskCoreSources = @('Simulation.cpp', 'Rules.cpp', 'Stations.cpp', 'GalaxyFeatures.cpp', 'GalacticEvents.cpp',
    'BuildTemplates.cpp', 'Victory.cpp', 'Chronicle.cpp', 'Threats.cpp', 'Scenarios.cpp') | ForEach-Object { Join-Path $taskCore $_ }
Push-Location $taskBuild
try {
    & $taskCompiler @taskCommon @taskCoreSources (Join-Path $taskCore 'Main.cpp') '/Fe:BotfSim.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Simulation compilation failed.' }
    & $taskCompiler @taskCommon @taskCoreSources (Join-Path $taskRoot 'tests\scenarios\SimulationTests.cpp') '/Fe:BotfSimTests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Scenario-test compilation failed.' }
    if ($Test) {
        & (Join-Path $taskBuild 'BotfSimTests.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Scenario tests failed.' }
    }
    $taskTacticalTests = Join-Path $taskRoot 'tests\scenarios\TacticalCombatTests.cpp'
    if ($Tactical -and (Test-Path -LiteralPath $taskTacticalTests)) {
        & $taskCompiler @taskCommon @taskCoreSources $taskTacticalTests '/Fe:BotfTacticalTests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Tactical-test compilation failed.' }
        if ($Test) {
            & (Join-Path $taskBuild 'BotfTacticalTests.exe')
            if ($LASTEXITCODE -ne 0) { throw 'Tactical tests failed.' }
        }
    }
    if ($Sessions) {
        & $taskCompiler @taskCommon @taskCoreSources (Join-Path $taskRoot 'tests\scenarios\CampaignSessionTests.cpp') '/Fe:BotfSessionTests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Campaign-session test compilation failed.' }
        if ($Test) {
            & (Join-Path $taskBuild 'BotfSessionTests.exe')
            if ($LASTEXITCODE -ne 0) { throw 'Campaign-session tests failed.' }
        }
    }
    if ($Gameplay) {
        & $taskCompiler @taskCommon @taskCoreSources (Join-Path $taskRoot 'tests\scenarios\RemasterGameplayTests.cpp') '/Fe:BotfRemasterGameplayTests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Gameplay-layer test compilation failed.' }
        if ($Test) {
            & (Join-Path $taskBuild 'BotfRemasterGameplayTests.exe')
            if ($LASTEXITCODE -ne 0) { throw 'Gameplay-layer tests failed.' }
        }
    }
    if ($Intelligence) {
        & $taskCompiler @taskCommon (Join-Path $taskRoot 'tests\scenarios\IntelligenceTests.cpp') '/Fe:BotfIntelligenceTests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Intelligence-test compilation failed.' }
        if ($Test) {
            & (Join-Path $taskBuild 'BotfIntelligenceTests.exe')
            if ($LASTEXITCODE -ne 0) { throw 'Intelligence tests failed.' }
        }
    }
    if ($Diplomacy) {
        & $taskCompiler @taskCommon (Join-Path $taskRoot 'tests\scenarios\DiplomacyTests.cpp') '/Fe:BotfDiplomacyTests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Diplomacy-test compilation failed.' }
        if ($Test) {
            & (Join-Path $taskBuild 'BotfDiplomacyTests.exe')
            if ($LASTEXITCODE -ne 0) { throw 'Diplomacy tests failed.' }
        }
    }
} finally {
    Pop-Location
}
Write-Output "Built: $(Join-Path $taskBuild 'BotfSim.exe')"
