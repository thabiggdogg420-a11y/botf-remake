param(
    [switch]$EditorRuntime,
    # Package to launch. Default: the most recent package produced by Build-Unreal.ps1 -Package (newest build manifest).
    [string]$PackageDirectory = '',
    # Unreal Engine root for -EditorRuntime. Default: $env:UE_ROOT, else the standard install of the project's engine version.
    [string]$EngineRoot = ''
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BotfTools.ps1')
if (-not $EditorRuntime) {
    if (-not $PackageDirectory) { $PackageDirectory = Find-BotfLatestPackage }
    if (-not $PackageDirectory) {
        throw "No packaged game found under $(Join-Path $PSScriptRoot 'Build'). Run Build-Unreal.ps1 -Package, pass -PackageDirectory, or use -EditorRuntime."
    }
    $taskPackage = Join-Path $PackageDirectory 'BOTFRemake.exe'
    if (-not (Test-Path -LiteralPath $taskPackage)) { throw "Packaged game not found: $taskPackage" }
    $taskManifestPath = Join-Path $PackageDirectory 'build-manifest.json'
    if (Test-Path -LiteralPath $taskManifestPath) {
        $taskManifest = Get-Content -LiteralPath $taskManifestPath -Raw | ConvertFrom-Json
        $taskRuntime = Join-Path $PackageDirectory 'BOTFRemake\Binaries\Win64\BOTFRemake.exe'
        if (-not $taskManifest.buildId -or -not $taskManifest.executableSha256 -or
            -not (Test-Path -LiteralPath $taskRuntime) -or
            (Get-FileHash -LiteralPath $taskRuntime -Algorithm SHA256).Hash -ne $taskManifest.executableSha256) {
            throw "Package executable does not match its build record: $PackageDirectory"
        }
        Write-Host "Launching $($taskManifest.buildId): $taskPackage"
    } else {
        Write-Host "Launching earlier checkpoint (no build manifest): $taskPackage"
    }
    # Visible interactive game window is intentional.
    # Let persisted GameUserSettings control the display after the first run.
    Start-Process -FilePath $taskPackage
} else {
    $EngineRoot = Resolve-BotfEngineRoot $EngineRoot
    $taskEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $taskProject = Join-Path $PSScriptRoot 'BOTFRemake.uproject'
    $taskArguments = '"' + $taskProject + '" /Game/Maps/Galaxy -game -nosplash'
    Start-Process -FilePath $taskEditor -ArgumentList $taskArguments
}
