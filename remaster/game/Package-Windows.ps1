<#
.SYNOPSIS
    One-command Windows build of the BOTF Remake Unreal client: prerequisite checks, core tests, UBT builds and a
    versioned package with a build manifest.

.DESCRIPTION
    Steps (each writes its own log under Build\Logs\<stamp>\):
      0. Prerequisites: UE 5.8 install, Visual Studio 2022+ with the MSVC x64 toolset, a Windows 10/11 SDK, the .NET
         SDK UnrealBuildTool runs on, Git LFS content, free disk space, editor not running.
      1. Core tests: Build-Simulation.ps1 compiles the deterministic core with MSVC /W4 /WX and runs every suite.
      2. UBT: BOTFRemakeEditor Win64 Development, then BOTFRemake Win64 for each -GameConfigurations entry.
      3. UAT BuildCookRun: cook, stage, pak and archive to Build\Packages\<version>-<commit>-<config>-<stamp>\.
      4. build-manifest.json next to the executable (Launch-Prototype.ps1 starts the newest one).
    On failure it writes Build\Logs\<stamp>\FAILURE-SUMMARY.txt: a short excerpt of the errors to paste back into a
    Claude session, and copies it to the clipboard.

    Works in Windows PowerShell 5.1 and PowerShell 7. See remaster\docs\PACKAGING.md.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\Package-Windows.ps1
.EXAMPLE
    .\Package-Windows.ps1 -PackageConfig Shipping -EngineRoot 'D:\Epic\UE_5.8'
.EXAMPLE
    .\Package-Windows.ps1 -CheckOnly
#>
param(
    # Unreal Engine root. Default: $env:UE_ROOT, else the standard install of the project's engine version.
    [string]$EngineRoot = '',
    # Game target configurations compiled by UBT (step 2).
    [ValidateSet('Development', 'Shipping', 'DebugGame', 'Test')]
    [string[]]$GameConfigurations = @('Development', 'Shipping'),
    # Configuration that is cooked and packaged (step 3). Development keeps logging and the console.
    [ValidateSet('Development', 'Shipping', 'DebugGame', 'Test')]
    [string]$PackageConfig = 'Development',
    # Parent folder of the versioned package folders. Default: Build\Packages next to this script.
    [string]$OutputRoot = '',
    # Only check prerequisites, build nothing.
    [switch]$CheckOnly,
    [switch]$SkipCoreTests,
    [switch]$SkipEditorBuild,
    [switch]$SkipGameBuild,
    [switch]$SkipPackage,
    # Also run the BOTF.UI.* automation tests in the editor after the editor build (Test-UnrealUi.ps1).
    [switch]$RunUiTests,
    # Continue past prerequisite warnings that are normally fatal (an engine that is not 5.8, low disk space).
    [switch]$Force,
    # Echo every tool's output to the console (it is always in the step logs).
    [switch]$ShowToolOutput
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
. (Join-Path $PSScriptRoot 'BotfTools.ps1')

$taskStart = Get-Date
$taskStamp = $taskStart.ToString('yyyyMMdd-HHmmss')
$taskProject = Join-Path $PSScriptRoot 'BOTFRemake.uproject'
$taskBuild = Join-Path $PSScriptRoot 'Build'
$taskLogs = Join-Path $taskBuild "Logs\$taskStamp"
New-Item -ItemType Directory -Path $taskLogs -Force | Out-Null
$taskUtf8 = New-Object Text.UTF8Encoding($false)
$script:taskSteps = New-Object System.Collections.ArrayList
$script:taskFacts = [ordered]@{}
$script:taskWarnings = New-Object System.Collections.ArrayList
$script:taskStepIndex = 0

function Write-BotfLine([string]$Text, [string]$Color = 'Gray') { Write-Host $Text -ForegroundColor $Color }
function Add-BotfWarning([string]$Text) { [void]$script:taskWarnings.Add($Text); Write-BotfLine "  WARNING: $Text" 'Yellow' }

# Lines worth pasting back: compiler, linker, UBT, UAT, cook and PowerShell errors.
$taskErrorPattern = '(error [A-Z]+\d+|: error|: fatal error|fatal error [A-Z]*\d*|error LNK\d+|unresolved external|' +
    'Cannot open include file|No such file or directory|UnrealBuildTool.*(error|exception)|BuildException|' +
    'AutomationException|AutomationTool exiting with ExitCode=[1-9]|ERROR:|Error:|LogCook: Error|LogInit: Error|' +
    'LogLinker: Error|LogUObjectGlobals: Error|Unhandled exception|Exception:|Result=\{Fail|FAILED|throw )'

function Write-BotfFailureSummary {
    param([string]$Step, [string]$Command, [int]$ExitCode, [string]$Log, [string]$Message)
    $taskSummary = Join-Path $taskLogs 'FAILURE-SUMMARY.txt'
    $taskOut = New-Object System.Collections.ArrayList
    [void]$taskOut.Add("BOTF Remake Package-Windows.ps1 FAILED at step: $Step")
    [void]$taskOut.Add("Exit code: $ExitCode    When: $((Get-Date).ToString('u'))")
    if ($Command) { [void]$taskOut.Add("Command: $Command") }
    if ($Message) { [void]$taskOut.Add("Message: $Message") }
    foreach ($taskKey in $script:taskFacts.Keys) { [void]$taskOut.Add(('{0}: {1}' -f $taskKey, $script:taskFacts[$taskKey])) }
    foreach ($taskWarning in $script:taskWarnings) { [void]$taskOut.Add("Warning: $taskWarning") }
    if ($Log -and (Test-Path -LiteralPath $Log)) {
        $taskLines = @([IO.File]::ReadAllLines($Log))
        $taskSeen = @{}
        $taskErrors = New-Object System.Collections.ArrayList
        foreach ($taskLine in $taskLines) {
            if ($taskLine -match $taskErrorPattern) {
                # Strip the long repository prefix so identical errors from two paths collapse.
                $taskShort = $taskLine.Replace($PSScriptRoot, '<game>').Trim()
                if ($taskShort.Length -gt 400) { $taskShort = $taskShort.Substring(0, 400) + ' ...' }
                if (-not $taskSeen.ContainsKey($taskShort)) { $taskSeen[$taskShort] = $true; [void]$taskErrors.Add($taskShort) }
            }
            if ($taskErrors.Count -ge 60) { break }
        }
        [void]$taskOut.Add('')
        [void]$taskOut.Add("---- First $($taskErrors.Count) distinct error lines ($Log) ----")
        foreach ($taskLine in $taskErrors) { [void]$taskOut.Add($taskLine) }
        [void]$taskOut.Add('')
        [void]$taskOut.Add('---- Last 40 lines ----')
        $taskFrom = [Math]::Max(0, $taskLines.Count - 40)
        for ($i = $taskFrom; $i -lt $taskLines.Count; $i++) { [void]$taskOut.Add($taskLines[$i].Replace($PSScriptRoot, '<game>')) }
    }
    # Engine-side logs that often hold the real cause.
    $taskMore = @(
        (Join-Path $env:LOCALAPPDATA 'UnrealBuildTool\Log.txt'),
        (Join-Path $PSScriptRoot 'Saved\Logs\BOTFRemake.log')
    )
    if ($script:taskFacts.Contains('Engine root')) {
        $taskMore += (Join-Path $script:taskFacts['Engine root'] 'Engine\Programs\AutomationTool\Saved\Logs')
    }
    [void]$taskOut.Add('')
    [void]$taskOut.Add('---- Other logs (attach if the excerpt is not enough) ----')
    [void]$taskOut.Add("Step logs: $taskLogs")
    foreach ($taskPath in $taskMore) { if ($taskPath -and (Test-Path -LiteralPath $taskPath)) { [void]$taskOut.Add($taskPath) } }
    $taskText = ($taskOut -join "`r`n")
    if ($taskText.Length -gt 24000) { $taskText = $taskText.Substring(0, 24000) + "`r`n... (truncated)" }
    [IO.File]::WriteAllText($taskSummary, $taskText, $taskUtf8)
    try { Set-Clipboard -Value $taskText -ErrorAction Stop; $taskClip = ' (also copied to the clipboard)' } catch { $taskClip = '' }
    Write-BotfLine ''
    Write-BotfLine "FAILED: $Step" 'Red'
    Write-BotfLine "Paste this file back into your Claude session$($taskClip):" 'Red'
    Write-BotfLine "  $taskSummary" 'Red'
    return $taskSummary
}

function Save-BotfRunRecord([string]$Outcome) {
    $taskRecord = [ordered]@{
        outcome = $Outcome; started = $taskStart.ToString('o'); finished = (Get-Date).ToString('o')
        facts = $script:taskFacts; warnings = @($script:taskWarnings); steps = @($script:taskSteps)
    }
    [IO.File]::WriteAllText((Join-Path $taskLogs 'run.json'), ($taskRecord | ConvertTo-Json -Depth 6), $taskUtf8)
}

function Stop-BotfRun {
    param([string]$Step, [string]$Message, [string]$Command = '', [int]$ExitCode = 1, [string]$Log = '')
    [void](Write-BotfFailureSummary -Step $Step -Command $Command -ExitCode $ExitCode -Log $Log -Message $Message)
    Save-BotfRunRecord 'failed'
    exit 1
}

# Runs a native tool, streaming its output to a UTF-8 step log. Windows PowerShell 5.1 turns native stderr into error
# records, which 'Stop' would treat as fatal, so the preference is relaxed only around the call.
function Invoke-BotfStep {
    param([string]$Name, [string]$FilePath, [string[]]$Arguments)
    $script:taskStepIndex++
    $taskSlug = ($Name -replace '[^A-Za-z0-9]+', '-').Trim('-').ToLowerInvariant()
    $taskLog = Join-Path $taskLogs ('{0:00}-{1}.log' -f $script:taskStepIndex, $taskSlug)
    $taskCommand = '"' + $FilePath + '" ' + (($Arguments | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }) -join ' ')
    Write-BotfLine ''
    Write-BotfLine "[$($script:taskStepIndex)] $Name" 'Cyan'
    Write-BotfLine "    log: $taskLog" 'DarkGray'
    $taskWatch = [Diagnostics.Stopwatch]::StartNew()
    $taskWriter = New-Object IO.StreamWriter($taskLog, $false, $taskUtf8)
    $taskWriter.AutoFlush = $true
    $taskWriter.WriteLine("> $taskCommand")
    $taskExit = -1
    $taskPrevious = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $global:LASTEXITCODE = 0
        & $FilePath @Arguments 2>&1 | ForEach-Object {
            $taskLine = "$_"
            $taskWriter.WriteLine($taskLine)
            if ($ShowToolOutput) { Write-Host $taskLine }
            elseif ($taskLine -match $taskErrorPattern) { Write-Host "    $taskLine" -ForegroundColor DarkYellow }
        }
        $taskExit = $LASTEXITCODE
    } catch {
        $taskWriter.WriteLine("Launcher exception: $($_.Exception.Message)")
        $taskExit = -2
    } finally {
        $ErrorActionPreference = $taskPrevious
        $taskWriter.Dispose()
    }
    $taskWatch.Stop()
    [void]$script:taskSteps.Add([ordered]@{ name = $Name; exitCode = $taskExit; seconds = [Math]::Round($taskWatch.Elapsed.TotalSeconds, 1); log = $taskLog })
    if ($taskExit -ne 0) {
        Stop-BotfRun -Step $Name -Message 'The tool returned a non-zero exit code.' -Command $taskCommand -ExitCode $taskExit -Log $taskLog
    }
    Write-BotfLine ("    ok ({0:N0} s)" -f $taskWatch.Elapsed.TotalSeconds) 'Green'
}

function Get-BotfIniValue([string]$Path, [string]$Key) {
    if (-not (Test-Path -LiteralPath $Path)) { return '' }
    foreach ($taskLine in [IO.File]::ReadAllLines($Path)) {
        if ($taskLine -match ('^\s*' + [regex]::Escape($Key) + '\s*=\s*(.+?)\s*$')) { return $Matches[1] }
    }
    return ''
}

# ------------------------------------------------------------------------------------------------------------------
# 0. Prerequisites
# ------------------------------------------------------------------------------------------------------------------
Write-BotfLine "BOTF Remake Windows package  ($taskStamp)" 'White'
Write-BotfLine '[0] Prerequisites' 'Cyan'
$script:taskFacts['PowerShell'] = $PSVersionTable.PSVersion.ToString()
$script:taskFacts['Windows'] = [Environment]::OSVersion.VersionString
if ($env:OS -ne 'Windows_NT') { Stop-BotfRun -Step 'Prerequisites' -Message 'Package-Windows.ps1 runs on Windows only.' }

# Unreal Engine 5.8
try { $EngineRoot = Resolve-BotfEngineRoot $EngineRoot }
catch { Stop-BotfRun -Step 'Prerequisites: Unreal Engine' -Message ($_.Exception.Message + ' Install UE 5.8 from the Epic Games Launcher (Library > Engine Versions), or pass -EngineRoot.') }
$script:taskFacts['Engine root'] = $EngineRoot
$taskVersion = Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine\Build\Build.version') -Raw | ConvertFrom-Json
$taskEngineVersion = '{0}.{1}.{2}' -f $taskVersion.MajorVersion, $taskVersion.MinorVersion, $taskVersion.PatchVersion
$script:taskFacts['Engine version'] = "$taskEngineVersion ($($taskVersion.BranchName), CL $($taskVersion.Changelist))"
Write-BotfLine "  Unreal Engine $taskEngineVersion at $EngineRoot"
if (-not ($taskVersion.MajorVersion -eq 5 -and $taskVersion.MinorVersion -eq 8)) {
    $taskText = "The project is associated with UE 5.8 but this engine is $taskEngineVersion."
    if ($Force) { Add-BotfWarning $taskText } else { Stop-BotfRun -Step 'Prerequisites: Unreal Engine' -Message ($taskText + ' Pass -Force to try anyway.') }
}
$taskBuildBat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$taskRunUat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
foreach ($taskTool in @($taskBuildBat, $taskRunUat)) {
    if (-not (Test-Path -LiteralPath $taskTool)) { Stop-BotfRun -Step 'Prerequisites: Unreal Engine' -Message "Missing $taskTool. Verify the engine install in the Epic Games Launcher." }
}

# Visual Studio 2022 (or newer) with the MSVC x64 toolset
$taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $taskVswhere)) {
    Stop-BotfRun -Step 'Prerequisites: Visual Studio' -Message 'Visual Studio Installer (vswhere.exe) not found. Install Visual Studio 2022 with "Game development with C++" and "Desktop development with C++" (see PACKAGING.md).'
}
# Parse from a variable: Windows PowerShell 5.1 emits a piped JSON array as one object.
$taskVsJson = (& $taskVswhere -products '*' -version '[17.0,' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json) -join "`n"
$taskVs = @()
if ($taskVsJson.Trim()) { $taskVsParsed = ConvertFrom-Json -InputObject $taskVsJson; $taskVs = @($taskVsParsed) }
if ($taskVs.Count -eq 0 -or -not $taskVs[0]) {
    Stop-BotfRun -Step 'Prerequisites: Visual Studio' -Message 'No Visual Studio 2022+ install with "MSVC v143 - VS 2022 C++ x64/x86 build tools" found. Add that component in the Visual Studio Installer.'
}
$taskVs = $taskVs | Sort-Object { [version]$_.installationVersion } -Descending | Select-Object -First 1
$script:taskFacts['Visual Studio'] = "$($taskVs.displayName) $($taskVs.installationVersion)"
$taskMsvcRoot = Join-Path $taskVs.installationPath 'VC\Tools\MSVC'
$taskMsvc = @(Get-ChildItem -LiteralPath $taskMsvcRoot -Directory -ErrorAction SilentlyContinue |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'bin\Hostx64\x64\cl.exe') } |
    Sort-Object { [version]$_.Name } -Descending)
if ($taskMsvc.Count -eq 0) { Stop-BotfRun -Step 'Prerequisites: MSVC' -Message "No x64 MSVC compiler under $taskMsvcRoot." }
$script:taskFacts['MSVC toolsets'] = ($taskMsvc | ForEach-Object { $_.Name }) -join ', '
Write-BotfLine "  $($script:taskFacts['Visual Studio']); MSVC $($script:taskFacts['MSVC toolsets'])"
if ([version]$taskMsvc[0].Name -lt [version]'14.38') {
    Add-BotfWarning "Newest MSVC toolset is $($taskMsvc[0].Name). Recent UE 5 releases want 14.38 or newer; update Visual Studio if UBT rejects it."
}

# Windows 10/11 SDK
$taskKits = $null
try { $taskKits = (Get-ItemProperty -Path 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots' -ErrorAction Stop).KitsRoot10 } catch { }
if (-not $taskKits) { $taskKits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10' }
$taskSdks = @(Get-ChildItem -LiteralPath (Join-Path $taskKits 'Include') -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '^10\.0\.\d+\.\d+$' -and (Test-Path -LiteralPath (Join-Path $_.FullName 'um\windows.h')) } |
    Sort-Object { [version]$_.Name } -Descending)
if ($taskSdks.Count -eq 0) {
    Stop-BotfRun -Step 'Prerequisites: Windows SDK' -Message 'No Windows 10/11 SDK found. Add "Windows 11 SDK (10.0.22621 or newer)" in the Visual Studio Installer.'
}
$script:taskFacts['Windows SDKs'] = ($taskSdks | ForEach-Object { $_.Name }) -join ', '
Write-BotfLine "  Windows SDK $($taskSdks[0].Name)"

# .NET: UnrealBuildTool and AutomationTool run on the .NET SDK the engine bundles; a system SDK is the fallback.
$taskDotnet = @(Get-ChildItem -LiteralPath (Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\DotNet') -Recurse -Depth 4 -Filter 'dotnet.exe' -ErrorAction SilentlyContinue)
if ($taskDotnet.Count -gt 0) {
    $script:taskFacts['.NET'] = "bundled: $($taskDotnet[0].DirectoryName)"
} else {
    $taskSystemSdks = ''
    try { $taskSystemSdks = (& dotnet --list-sdks 2>$null) -join '; ' } catch { }
    if (-not $taskSystemSdks) {
        Stop-BotfRun -Step 'Prerequisites: .NET' -Message 'No .NET SDK: the engine has no bundled dotnet and none is on PATH. Repair the engine install (Epic Games Launcher > Verify) or install the .NET 8 SDK.'
    }
    $script:taskFacts['.NET'] = "system: $taskSystemSdks"
}
Write-BotfLine "  .NET $($script:taskFacts['.NET'])"

# Git LFS: maps and assets must be real files, not LFS pointer stubs, or the cook fails with confusing errors.
$taskAssets = @(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'Content') -Recurse -File -Include '*.uasset', '*.umap' -ErrorAction SilentlyContinue)
$taskPointers = @($taskAssets | Where-Object {
    $_.Length -lt 1024 -and ([IO.File]::ReadAllText($_.FullName)).StartsWith('version https://git-lfs')
})
if ($taskPointers.Count -gt 0) {
    Stop-BotfRun -Step 'Prerequisites: Git LFS content' -Message ("$($taskPointers.Count) of $($taskAssets.Count) assets are Git LFS pointers (e.g. $($taskPointers[0].FullName)). " +
        'Run "git lfs install" and "git lfs pull" in the repository, then rerun.')
}
$script:taskFacts['Content assets'] = "$($taskAssets.Count) (.uasset/.umap)"

# Repository state
$taskCommit = 'nogit'
try {
    $taskGit = (& git -C $PSScriptRoot rev-parse --short=10 HEAD 2>$null)
    if ($LASTEXITCODE -eq 0 -and $taskGit) {
        $taskCommit = "$taskGit".Trim()
        $taskDirty = (& git -C $PSScriptRoot status --porcelain -- . 2>$null)
        if ($taskDirty) { $taskCommit += '-dirty' }
    }
} catch { }
$script:taskFacts['Commit'] = $taskCommit
$taskProjectVersion = Get-BotfIniValue (Join-Path $PSScriptRoot 'Config\DefaultGame.ini') 'ProjectVersion'
if (-not $taskProjectVersion) { $taskProjectVersion = '0.0.0' }
$script:taskFacts['Project version'] = $taskProjectVersion

# Disk space and path length
$taskDrive = New-Object IO.DriveInfo([IO.Path]::GetPathRoot($PSScriptRoot))
$taskFreeGb = [Math]::Round($taskDrive.AvailableFreeSpace / 1GB, 1)
$script:taskFacts['Free disk'] = "$taskFreeGb GB on $($taskDrive.Name)"
if ($taskFreeGb -lt 25) {
    $taskText = "Only $taskFreeGb GB free on $($taskDrive.Name); a first editor + game build and cook can use 20-30 GB."
    if ($Force) { Add-BotfWarning $taskText } else { Stop-BotfRun -Step 'Prerequisites: disk space' -Message ($taskText + ' Free space or pass -Force.') }
}
if ($PSScriptRoot.Length -gt 70) {
    $taskLongPaths = 0
    try { $taskLongPaths = (Get-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -ErrorAction Stop).LongPathsEnabled } catch { }
    if ($taskLongPaths -ne 1) {
        Add-BotfWarning "The project path is $($PSScriptRoot.Length) characters and Windows long paths are off. Cooking can hit the 260-character limit; clone to a short path such as C:\src\botf."
    }
}

# A running editor with this project open holds the module DLL (and Live Coding blocks UBT).
$taskEditors = @(Get-Process -Name 'UnrealEditor', 'UnrealEditor-Cmd' -ErrorAction SilentlyContinue)
if ($taskEditors.Count -gt 0) {
    Add-BotfWarning 'Unreal Editor is running. Close it (or at least this project) before building, or UBT will fail to overwrite the module DLL.'
}

if ($CheckOnly) {
    Write-BotfLine ''
    Write-BotfLine 'Prerequisites OK.' 'Green'
    foreach ($taskKey in $script:taskFacts.Keys) { Write-BotfLine ('  {0}: {1}' -f $taskKey, $script:taskFacts[$taskKey]) }
    Save-BotfRunRecord 'check-only'
    exit 0
}

$taskInputsBefore = Get-BotfInputHashes
$taskPowerShell = (Get-Process -Id $PID).Path

# ------------------------------------------------------------------------------------------------------------------
# 1. Core tests (MSVC /W4 /WX, every scenario suite)
# ------------------------------------------------------------------------------------------------------------------
if (-not $SkipCoreTests) {
    Invoke-BotfStep -Name 'Core tests (MSVC)' -FilePath $taskPowerShell -Arguments @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot 'Build-Simulation.ps1'),
        '-Test', '-Tactical', '-Sessions', '-Gameplay', '-Intelligence', '-Diplomacy')
}

# ------------------------------------------------------------------------------------------------------------------
# 2. UnrealBuildTool
# ------------------------------------------------------------------------------------------------------------------
$taskUbtCommon = @("-Project=$taskProject", '-WaitMutex', '-NoHotReloadFromIDE')
if (-not $SkipEditorBuild) {
    Invoke-BotfStep -Name 'UBT BOTFRemakeEditor Win64 Development' -FilePath $taskBuildBat -Arguments (@('BOTFRemakeEditor', 'Win64', 'Development') + $taskUbtCommon)
    if ($RunUiTests) {
        Invoke-BotfStep -Name 'UI automation tests (BOTF.UI.*)' -FilePath $taskPowerShell -Arguments @(
            '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot 'Test-UnrealUi.ps1'),
            '-EngineRoot', $EngineRoot, '-SkipBuild')
    }
}
if (-not $SkipGameBuild) {
    foreach ($taskConfig in $GameConfigurations) {
        Invoke-BotfStep -Name "UBT BOTFRemake Win64 $taskConfig" -FilePath $taskBuildBat -Arguments (@('BOTFRemake', 'Win64', $taskConfig) + $taskUbtCommon)
    }
}

# ------------------------------------------------------------------------------------------------------------------
# 3. Cook, stage, pak and archive
# ------------------------------------------------------------------------------------------------------------------
if ($SkipPackage) {
    Save-BotfRunRecord 'built'
    Write-BotfLine ''
    Write-BotfLine ('Build finished in {0:N0} min (packaging skipped). Logs: {1}' -f ((Get-Date) - $taskStart).TotalMinutes, $taskLogs) 'Green'
    exit 0
}
if (-not $OutputRoot) { $OutputRoot = Join-Path $taskBuild 'Packages' }
$taskPackageName = '{0}-{1}-{2}-{3}' -f $taskProjectVersion, $taskCommit, $PackageConfig, $taskStamp
$taskArchive = Join-Path $OutputRoot $taskPackageName
New-Item -ItemType Directory -Path $taskArchive -Force | Out-Null
$script:taskFacts['Package folder'] = $taskArchive
$taskUatArguments = @(
    'BuildCookRun', "-project=$taskProject", '-noP4', '-platform=Win64', "-clientconfig=$PackageConfig",
    '-build', '-cook', '-map=/Game/Maps/Galaxy', '-stage', '-pak', '-prereqs', '-archive', "-archivedirectory=$taskArchive",
    '-unattended', '-utf8output', '-WaitForUATMutex')
# The cooker loads the editor module; it was just built above, so UAT need not rebuild it.
if (-not $SkipEditorBuild) { $taskUatArguments += '-nocompileeditor' }
Invoke-BotfStep -Name "UAT BuildCookRun Win64 $PackageConfig" -FilePath $taskRunUat -Arguments $taskUatArguments

# ------------------------------------------------------------------------------------------------------------------
# 4. Credits and build manifest
# ------------------------------------------------------------------------------------------------------------------
$taskWindows = Join-Path $taskArchive 'Windows'
$taskBinaries = Join-Path $taskWindows 'BOTFRemake\Binaries\Win64'
$taskRuntime = @(Get-ChildItem -LiteralPath $taskBinaries -Filter 'BOTFRemake*.exe' -File -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1)
if (-not (Test-Path -LiteralPath (Join-Path $taskWindows 'BOTFRemake.exe')) -or $taskRuntime.Count -eq 0) {
    Stop-BotfRun -Step 'Package layout' -Message "BuildCookRun succeeded but $taskWindows has no BOTFRemake.exe / Binaries\Win64\BOTFRemake*.exe."
}
$taskCreditSource = Join-Path $PSScriptRoot 'ArtSource\Community\Icewolf132'
$taskCreditDestination = Join-Path $taskWindows 'Credits\Icewolf132'
New-Item -ItemType Directory -Path $taskCreditDestination -Force | Out-Null
foreach ($taskFile in @('README-original.txt', 'conversion-manifest.json')) {
    $taskPath = Join-Path $taskCreditSource $taskFile
    if (Test-Path -LiteralPath $taskPath) { Copy-Item -LiteralPath $taskPath -Destination $taskCreditDestination -Force }
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'ASSET-CREDITS.md') -Destination (Join-Path $taskWindows 'Credits') -Force

$taskInputsAfter = Get-BotfInputHashes
$taskInputsStable = (($taskInputsBefore | ConvertTo-Json -Compress) -eq ($taskInputsAfter | ConvertTo-Json -Compress))
if (-not $taskInputsStable) {
    Add-BotfWarning 'Source, Config or Content changed while this script ran; the package is marked unverified.'
}
$taskManifest = [ordered]@{
    buildId = 'BOTF-' + $taskStamp
    version = $taskProjectVersion
    commit = $taskCommit
    configuration = $PackageConfig
    builtUtc = [DateTime]::UtcNow.ToString('o')
    engineRoot = $EngineRoot
    engineVersion = $script:taskFacts['Engine version']
    toolchain = [ordered]@{
        visualStudio = $script:taskFacts['Visual Studio']; msvc = $taskMsvc[0].Name; windowsSdk = $taskSdks[0].Name; dotnet = $script:taskFacts['.NET']
    }
    gameConfigurationsBuilt = @($(if ($SkipGameBuild) { @() } else { $GameConfigurations }))
    coreTests = $(if ($SkipCoreTests) { 'skipped' } else { 'passed' })
    uiTests = $(if ($RunUiTests -and -not $SkipEditorBuild) { 'passed' } else { 'not run' })
    # Path of the real game binary relative to this manifest (the top-level BOTFRemake.exe is a launcher stub).
    runtimeExecutable = Get-BotfRelativePath $taskWindows $taskRuntime[0].FullName
    executableSha256 = (Get-FileHash -LiteralPath $taskRuntime[0].FullName -Algorithm SHA256).Hash
    inputSha256 = $taskInputsAfter
    inputsStableDuringBuild = $taskInputsStable
    steps = @($script:taskSteps)
    verification = $(if ($taskInputsStable) { 'Packaged successfully; gameplay and visual acceptance recorded separately.' } else { 'UNVERIFIED: inputs changed during the build.' })
}
# Launch-Prototype.ps1 starts the package with the newest manifest by default.
[IO.File]::WriteAllText((Join-Path $taskWindows 'build-manifest.json'), ($taskManifest | ConvertTo-Json -Depth 6), $taskUtf8)
Save-BotfRunRecord 'packaged'

$taskMinutes = ((Get-Date) - $taskStart).TotalMinutes
Write-BotfLine ''
Write-BotfLine ('Packaged {0} in {1:N0} min' -f $taskManifest.buildId, $taskMinutes) 'Green'
Write-BotfLine "  Game:     $(Join-Path $taskWindows 'BOTFRemake.exe')" 'Green'
Write-BotfLine "  Manifest: $(Join-Path $taskWindows 'build-manifest.json')"
Write-BotfLine "  Logs:     $taskLogs"
Write-BotfLine '  Launch:   .\Launch-Prototype.ps1'
foreach ($taskWarning in $script:taskWarnings) { Write-BotfLine "  WARNING: $taskWarning" 'Yellow' }
