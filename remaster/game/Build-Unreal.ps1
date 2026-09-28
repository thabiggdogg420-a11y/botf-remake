param(
    [switch]$Package,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$ArchiveDirectory = ''
)
$ErrorActionPreference = 'Stop'
$taskProject = Join-Path $PSScriptRoot 'BOTFRemake.uproject'
$taskBuild = Join-Path $PSScriptRoot 'Build'
function Get-TaskSourceHashes {
    $taskHashes = [ordered]@{}
    Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'Source') -Recurse -File |
        Sort-Object FullName | ForEach-Object {
            $taskHashes[[IO.Path]::GetRelativePath($PSScriptRoot, $_.FullName)] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }
    return $taskHashes
}
$taskSourceBefore = if ($Package) { Get-TaskSourceHashes } else { $null }
New-Item -ItemType Directory -Path $taskBuild -Force | Out-Null
if (-not (Test-Path -LiteralPath (Join-Path $EngineRoot 'Engine\Build\Build.version'))) {
    throw 'EngineRoot must point to an installed Unreal Engine.'
}
if ($Package) {
    if (-not $ArchiveDirectory) { $ArchiveDirectory = Join-Path $taskBuild 'Packaged' }
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat') BuildCookRun `
        "-project=$taskProject" -noP4 -platform=Win64 -clientconfig=Development `
        -build -cook '-map=/Game/Maps/Galaxy' -stage -pak -archive `
        "-archivedirectory=$ArchiveDirectory" -unattended -utf8output -WaitForUATMutex
} else {
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') BOTFRemakeEditor Win64 Development `
        "-Project=$taskProject" -WaitMutex -NoHotReloadFromIDE "-Log=$(Join-Path $taskBuild 'unreal-editor-build.log')"
}
if ($LASTEXITCODE -ne 0) { throw "Unreal build failed (exit $LASTEXITCODE). Close the editor/game before rebuilding." }
if ($Package) {
    # Keep the author's usage terms and conversion provenance alongside the executable.
    $taskCreditSource = Join-Path $PSScriptRoot 'ArtSource\Community\Icewolf132'
    $taskCreditDestination = Join-Path $ArchiveDirectory 'Windows\Credits\Icewolf132'
    New-Item -ItemType Directory -Path $taskCreditDestination -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $taskCreditSource 'README-original.txt') -Destination $taskCreditDestination -Force
    Copy-Item -LiteralPath (Join-Path $taskCreditSource 'conversion-manifest.json') -Destination $taskCreditDestination -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'ASSET-CREDITS.md') -Destination (Join-Path $ArchiveDirectory 'Windows\Credits') -Force
    $taskSourceAfter = Get-TaskSourceHashes
    if (($taskSourceBefore | ConvertTo-Json -Compress) -ne ($taskSourceAfter | ConvertTo-Json -Compress)) {
        throw 'Source changed during packaging. This package is not verified; rebuild after builders release their files.'
    }
    $taskRuntime = Join-Path $ArchiveDirectory 'Windows\BOTFRemake\Binaries\Win64\BOTFRemake.exe'
    $taskManifest = [ordered]@{
        buildId = 'BOTF-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
        builtUtc = [DateTime]::UtcNow.ToString('o')
        executableSha256 = (Get-FileHash -LiteralPath $taskRuntime -Algorithm SHA256).Hash
        sourceSha256 = $taskSourceAfter
        verification = 'Packaged successfully; gameplay and visual acceptance recorded separately.'
    }
    $taskManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $ArchiveDirectory 'Windows\build-manifest.json') -Encoding utf8
}
