# Shared helpers for the BOTF Remake build, test and launch scripts. Dot-source this file; it only defines functions.
# Works in Windows PowerShell 5.1 and PowerShell 7.

# Unreal Engine installation used by every script, in order of preference:
#   1. -EngineRoot passed to the script,
#   2. the UE_ROOT environment variable,
#   3. the standard install path for the engine version the project is associated with (BOTFRemake.uproject).
function Resolve-BotfEngineRoot {
    param([string]$EngineRoot = '')
    $taskCandidate = $EngineRoot
    if (-not $taskCandidate) { $taskCandidate = $env:UE_ROOT }
    if (-not $taskCandidate) {
        $taskAssociation = '5.8'
        $taskProjectFile = Join-Path $PSScriptRoot 'BOTFRemake.uproject'
        try {
            $taskDeclared = (Get-Content -LiteralPath $taskProjectFile -Raw | ConvertFrom-Json).EngineAssociation
            if ($taskDeclared) { $taskAssociation = $taskDeclared }
        } catch { }
        $taskProgramFiles = if ($env:ProgramFiles) { $env:ProgramFiles } else { 'C:\Program Files' }
        $taskCandidate = Join-Path $taskProgramFiles ("Epic Games\UE_" + $taskAssociation)
    }
    if (-not (Test-Path -LiteralPath (Join-Path $taskCandidate 'Engine\Build\Build.version'))) {
        throw "Unreal Engine not found at '$taskCandidate'. Pass -EngineRoot or set the UE_ROOT environment variable."
    }
    return (Resolve-Path -LiteralPath $taskCandidate).ProviderPath
}

# Path of $Path relative to $Root ([IO.Path]::GetRelativePath is not available in Windows PowerShell 5.1).
function Get-BotfRelativePath {
    param([string]$Root, [string]$Path)
    $taskRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $taskFull = [IO.Path]::GetFullPath($Path)
    if ($taskFull.StartsWith($taskRoot, [StringComparison]::OrdinalIgnoreCase)) { return $taskFull.Substring($taskRoot.Length) }
    return $taskFull
}

# SHA-256 of every build input: Source, Config, Content and the project file, keyed by project-relative path.
function Get-BotfInputHashes {
    $taskHashes = [ordered]@{}
    $taskFolders = @('Source', 'Config', 'Content') | ForEach-Object { Join-Path $PSScriptRoot $_ } |
        Where-Object { Test-Path -LiteralPath $_ }
    $taskFiles = @()
    if ($taskFolders) { $taskFiles += @(Get-ChildItem -LiteralPath $taskFolders -Recurse -File) }
    $taskFiles += @(Get-Item -LiteralPath (Join-Path $PSScriptRoot 'BOTFRemake.uproject'))
    $taskFiles | Sort-Object FullName | ForEach-Object {
        $taskHashes[(Get-BotfRelativePath $PSScriptRoot $_.FullName)] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
    return $taskHashes
}

# Directory of the most recent package that Build-Unreal.ps1 -Package recorded (its build-manifest.json), or ''.
function Find-BotfLatestPackage {
    $taskBuild = Join-Path $PSScriptRoot 'Build'
    if (-not (Test-Path -LiteralPath $taskBuild)) { return '' }
    $taskLatest = Get-ChildItem -LiteralPath $taskBuild -Recurse -Depth 4 -File -Filter 'build-manifest.json' -ErrorAction SilentlyContinue |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.DirectoryName 'BOTFRemake.exe') } |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ($taskLatest) { return $taskLatest.DirectoryName }
    return ''
}
