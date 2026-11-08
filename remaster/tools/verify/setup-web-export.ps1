<#
.SYNOPSIS
  One-time setup for the Web export: copies Godot's web export templates out of the template package into the
  per-user template folder. Run it once per Godot version; verify.ps1 reports the Web step BLOCKED until it has been run.
  Writes only to %APPDATA%\Godot\export_templates\4.5.stable (the web_*.zip files). Needs no admin rights.
#>
$ErrorActionPreference = 'Stop'
$tpz = 'C:\Tools\Godot\Godot_v4.5-stable_export_templates.tpz'
$dest = Join-Path $env:APPDATA 'Godot\export_templates\4.5.stable'
if (-not (Test-Path -LiteralPath $tpz)) { throw "template package not found: $tpz" }
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($tpz)
try {
    foreach ($e in $zip.Entries) {
        $name = [System.IO.Path]::GetFileName($e.FullName)
        if ($name -like 'web_*.zip') {
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($e, (Join-Path $dest $name), $true)
            Write-Host "installed $name"
        }
    }
} finally { $zip.Dispose() }
