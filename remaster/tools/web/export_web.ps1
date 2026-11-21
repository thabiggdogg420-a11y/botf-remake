<#
.SYNOPSIS
  Exports one browser build (game pck + engine) from a Godot project folder, with the build version stamped in, and puts
  the content-hashed packs beside it. Nothing else is rebuilt (packs come from an existing pack folder).

    powershell -NoProfile -File remaster\tools\web\export_web.ps1 -Preset Web -Out <folder> -Packs <packs folder>
        [-Proj remaster\godot] [-Godot <console exe>] [-Version "0.1.540 (abc1234)"]

  -Preset Web uses Godot 4.5 (WebGL 2, Compatibility); -Preset WebGPU wants the fork editor (-Godot ...godot-webgpu\editor\
  godot.windows.editor.x86_64.exe) run on a copy of the project imported by that editor (never on the main checkout: the 4.8
  import rewrites .import files).
  The version goes into project.godot (application/config/version, read by the title screen's corner label) and the
  export preset's include filter gets the data/audio JSON (as verify.ps1 does); both files are restored afterwards.
  Godot runs at BelowNormal priority with the Dummy audio driver, headless.
#>
param(
    [Parameter(Mandatory)][string]$Preset,
    [Parameter(Mandatory)][string]$Out,
    [string]$Packs,
    [string]$Proj,
    [string]$Godot = 'C:\Tools\Godot\Godot_v4.5-stable_win64_console.exe',
    [string]$Version
)
$ErrorActionPreference = 'Stop'
$remaster = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $Proj) { $Proj = Join-Path $remaster 'godot' }
if (-not $Version) {
    $sha = (git -C $remaster rev-parse --short HEAD).Trim()
    $count = (git -C $remaster rev-list --count HEAD).Trim()
    $Version = "0.1.$count ($sha)"
}
$pg = Join-Path $Proj 'project.godot'
$pp = Join-Path $Proj 'export_presets.cfg'
$pgOrig = [IO.File]::ReadAllText($pg)
$ppOrig = [IO.File]::ReadAllText($pp)
try {
    $stamped = if ($pgOrig -match '(?m)^config/version=') { $pgOrig -replace '(?m)^config/version="[^"]*"', "config/version=`"$Version`"" }
               else { $pgOrig -replace '(?m)^(config/name="[^"]*")', "`$1`nconfig/version=`"$Version`"" }
    [IO.File]::WriteAllText($pg, $stamped)
    [IO.File]::WriteAllText($pp, ($ppOrig -replace 'include_filter="fixtures/\*\.json"', 'include_filter="fixtures/*.json, data/*.pack.json, scripts/audio/pack/*.json, scripts/audio/pack/*/*.json"'))
    New-Item -ItemType Directory -Force -Path $Out | Out-Null
    $target = Join-Path (Resolve-Path $Out).Path 'index.html'
    $log = Join-Path $Out 'export.log'
    $p = Start-Process -FilePath $Godot -ArgumentList "--headless --audio-driver Dummy --path `"$Proj`" --export-release `"$Preset`" `"$target`"" `
        -PassThru -WindowStyle Hidden -RedirectStandardOutput $log -RedirectStandardError "$log.err"
    $null = $p.Handle   # PowerShell 5: without a handle ExitCode stays empty
    try { $p.PriorityClass = 'BelowNormal' } catch {}
    $p.WaitForExit()
    if ($p.ExitCode -ne 0 -or -not (Test-Path -LiteralPath (Join-Path $Out 'index.pck'))) { throw "export $Preset failed (exit $($p.ExitCode)); see $log" }
} finally {
    [IO.File]::WriteAllText($pg, $pgOrig)
    [IO.File]::WriteAllText($pp, $ppOrig)
}
if ($Packs) {
    $dp = Join-Path $Out 'packs'
    New-Item -ItemType Directory -Force -Path $dp | Out-Null
    & robocopy $Packs $dp /E /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy packs failed ($LASTEXITCODE)" }
}
Set-Content -LiteralPath (Join-Path $Out 'VERSION.txt') -Encoding utf8 -Value "BOTF Remastered $Version ($Preset)"
Write-Host "exported $Preset $Version to $Out"
