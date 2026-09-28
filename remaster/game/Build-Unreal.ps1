param(
    [switch]$Package,
    # Unreal Engine root. Default: $env:UE_ROOT, else the standard install of the project's engine version.
    [string]$EngineRoot = '',
    [string]$ArchiveDirectory = ''
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BotfTools.ps1')
$EngineRoot = Resolve-BotfEngineRoot $EngineRoot
$taskProject = Join-Path $PSScriptRoot 'BOTFRemake.uproject'
$taskBuild = Join-Path $PSScriptRoot 'Build'
$taskInputsBefore = if ($Package) { Get-BotfInputHashes } else { $null }
New-Item -ItemType Directory -Path $taskBuild -Force | Out-Null
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
    $taskInputsAfter = Get-BotfInputHashes
    if (($taskInputsBefore | ConvertTo-Json -Compress) -ne ($taskInputsAfter | ConvertTo-Json -Compress)) {
        throw 'Source, Config or Content changed during packaging. This package is not verified; rebuild after builders release their files.'
    }
    $taskRuntime = Join-Path $ArchiveDirectory 'Windows\BOTFRemake\Binaries\Win64\BOTFRemake.exe'
    $taskManifest = [ordered]@{
        buildId = 'BOTF-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
        builtUtc = [DateTime]::UtcNow.ToString('o')
        engineRoot = $EngineRoot
        executableSha256 = (Get-FileHash -LiteralPath $taskRuntime -Algorithm SHA256).Hash
        # Every build input (Source, Config, Content, project file), keyed by project-relative path.
        inputSha256 = $taskInputsAfter
        verification = 'Packaged successfully; gameplay and visual acceptance recorded separately.'
    }
    # Launch-Prototype.ps1 starts the package with the newest manifest by default.
    $taskManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $ArchiveDirectory 'Windows\build-manifest.json') -Encoding utf8
    Write-Output "Packaged $($taskManifest.buildId): $(Join-Path $ArchiveDirectory 'Windows\BOTFRemake.exe')"
}
