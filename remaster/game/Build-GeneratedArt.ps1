<#
.SYNOPSIS
    Rebuilds all original procedural art (ArtSource\Generated, Content\Generated). Both folders are gitignored, so run
    this once on a fresh clone before playing or packaging, and again after changing any generator.

.DESCRIPTION
    Steps, each skippable:
      1. Textures   python Tools\generate_space_art.py, generate_hull_textures.py, generate_fx_textures.py
      2. Ships      Blender (headless) Tools\blender_generate_ships.py: 159 hulls as FBX plus generated-ships.json
      3. Unreal     UnrealEditor-Cmd -run=pythonscript: ue_build_environment.py, ue_import_generated_ships.py,
                    ue_build_vfx.py (in that order: the debris material instances the ship hull material)
    Needs Python 3 with numpy and Pillow, Blender 5.x, and the UE 5.8 editor. Nothing here is third-party art.

.EXAMPLE
    .\Build-GeneratedArt.ps1
.EXAMPLE
    .\Build-GeneratedArt.ps1 -SkipTextures -SkipShips      # re-run only the Unreal import
#>
param(
    [switch]$SkipTextures,
    [switch]$SkipShips,
    [switch]$SkipUnreal,
    [int]$SkySize = 8192,
    [string]$Blender = '',
    [string]$EngineRoot = ''
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BotfTools.ps1')
$tools = Join-Path $PSScriptRoot 'Tools'
$logDir = Join-Path $PSScriptRoot 'Build\GeneratedArt'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

function Invoke-Step([string]$Name, [scriptblock]$Body) {
    $start = Get-Date
    Write-Host "== $Name"
    & $Body
    if ($LASTEXITCODE) { throw "$Name failed (exit $LASTEXITCODE)" }
    Write-Host ("   done in {0:n0} s" -f ((Get-Date) - $start).TotalSeconds)
}

if (-not $SkipTextures) {
    Invoke-Step 'Space art (sky, gas giant, moon)' { python (Join-Path $tools 'generate_space_art.py') --size $SkySize }
    Invoke-Step 'Hull plating textures' { python (Join-Path $tools 'generate_hull_textures.py') }
    Invoke-Step 'Effect noise texture' { python (Join-Path $tools 'generate_fx_textures.py') }
}

if (-not $SkipShips) {
    if (-not $Blender) {
        $Blender = Get-ChildItem 'C:\Program Files\Blender Foundation' -Recurse -Filter blender.exe -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
    }
    if (-not $Blender -or -not (Test-Path -LiteralPath $Blender)) { throw 'Blender not found: pass -Blender <path to blender.exe>' }
    $log = Join-Path $logDir 'ships.log'
    Invoke-Step 'Ship hulls (Blender)' {
        & $Blender --background --factory-startup --python (Join-Path $tools 'blender_generate_ships.py') *> $log
        $summary = Select-String -LiteralPath $log -Pattern '^generated \d+/\d+' | Select-Object -Last 1
        if (-not $summary) { throw "Ship generation did not finish (see $log)" }
        Write-Host "   $($summary.Line)"
    }
}

if (-not $SkipUnreal) {
    $engine = if ($EngineRoot) { $EngineRoot } else { Resolve-BotfEngineRoot }
    $cmd = Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $project = Join-Path $PSScriptRoot 'BOTFRemake.uproject'
    foreach ($script in 'ue_build_environment.py', 'ue_import_generated_ships.py', 'ue_build_vfx.py') {
        $log = Join-Path $logDir ($script -replace '\.py$', '.log')
        Invoke-Step "Unreal: $script" {
            & $cmd $project -run=pythonscript "-script=$(Join-Path $tools $script)" -unattended -nosplash -nullrhi *> $log
            $failed = @(Select-String -LiteralPath $log -Pattern 'Traceback|Python script executed with errors|Missing .* input')
            if ($failed.Count) { $failed | Select-Object -First 5 | ForEach-Object { Write-Host "   $($_.Line)" }; throw "$script reported errors (see $log)" }
        }
    }
    $meshes = @(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'Content\Generated\Ships') -Recurse -Filter 'SM_*.uasset').Count
    Write-Host "Generated art ready: $meshes ship meshes in Content\Generated\Ships"
}
