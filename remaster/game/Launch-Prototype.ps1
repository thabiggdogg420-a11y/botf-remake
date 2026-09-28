param(
    [switch]$EditorRuntime,
    [string]$PackageDirectory = ''
)
$ErrorActionPreference = 'Stop'
if (-not $PackageDirectory) { $PackageDirectory = Join-Path $PSScriptRoot 'Build\CommandPreview\Windows' }
$taskPackage = Join-Path $PackageDirectory 'BOTFRemake.exe'
if (-not $EditorRuntime -and (Test-Path -LiteralPath $taskPackage)) {
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
    if (-not $EditorRuntime) { throw "Packaged game not found: $taskPackage. Build the package or use -EditorRuntime." }
    $taskEditor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
    $taskProject = Join-Path $PSScriptRoot 'BOTFRemake.uproject'
    $taskArguments = '"' + $taskProject + '" /Game/Maps/Galaxy -game -nosplash'
    Start-Process -FilePath $taskEditor -ArgumentList $taskArguments
}
