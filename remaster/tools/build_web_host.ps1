<#
.SYNOPSIS
  Builds the host-ready static folder for the browser game (F1). One command, from any worktree:
    powershell -NoProfile -File remaster\tools\build_web_host.ps1 [-Out <folder>] [-SkipBuild]

  1. the wasm GDExtension core (godot\core\build-web.ps1), unless core\bin already holds the release module;
  2. verify.ps1 -Target web -SkipTests -SkipCapture: pack build (hulls at web size, planets, audio, music), import, Web export,
     content-hashed packs, packs\manifest.json, gzip and brotli sidecars, sizes.json (needs LocalAssets: found by verify.ps1);
  3. copies the export to <Out> (default godot\build\botf-web-host) without build leftovers (.import files) and adds
     HOSTING.md, .htaccess and nginx.conf.example from godot\web-host.
  -SkipBuild assembles from an export that is already in godot\build\web.
  -Release also mirrors the folder to Documents\ChatGPT\BOTF-Web-Latest (the one folder ChatGPT uploads to
  Free4allhub.com/botfremaster) with VERSION.txt and UPDATE-SITE.md. Nothing is uploaded. The folder uses relative paths only and runs from any sub-path (https://site/botf/).
#>
param(
    [string]$Out,
    [switch]$SkipBuild,
    [switch]$Release
)
$ErrorActionPreference = 'Stop'
$remaster = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$proj = Join-Path $remaster 'godot'
if (-not $Out) { $Out = Join-Path $proj 'build\botf-web-host' }

if (-not $SkipBuild) {
    $wasm = Join-Path $proj 'core\bin\libbotf_core.web.template_release.wasm32.wasm'
    if (-not (Test-Path -LiteralPath $wasm)) {
        & powershell -NoProfile -File (Join-Path $proj 'core\build-web.ps1') -Target release
        if ($LASTEXITCODE -ne 0) { throw 'wasm core build failed' }
    }
    & powershell -NoProfile -File (Join-Path $remaster 'tools\verify\verify.ps1') -Target web -SkipTests -SkipCapture
    if ($LASTEXITCODE -ne 0) { throw 'verify -Target web failed (see the run folder under remaster\captures)' }
}
$src = Join-Path $proj 'build\web'
if (-not (Test-Path -LiteralPath (Join-Path $src 'index.html'))) { throw "no web export in $src" }
if (Test-Path -LiteralPath $Out) { Remove-Item -LiteralPath $Out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
& robocopy $src $Out /E /XF '*.import' 'serve_web.py' 'play-in-browser.bat' /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }
foreach ($f in 'HOSTING.md', '.htaccess', 'nginx.conf.example', 'UPDATE-SITE.md') {
    Copy-Item -LiteralPath (Join-Path $proj "web-host\$f") -Destination (Join-Path $Out $f) -Force
}
$sz = Get-Content -LiteralPath (Join-Path $Out 'sizes.json') -Raw | ConvertFrom-Json
$all = Get-ChildItem -LiteralPath $Out -Recurse -File
Write-Host ("host folder: {0}  ({1} files, {2:N0} MB on disk including .gz/.br sidecars)" -f $Out, $all.Count, (($all | Measure-Object Length -Sum).Sum / 1MB))
Write-Host ("first playable load: {0} MB raw, {1} MB gzip, {2} MB brotli; every pack: {3} MB gzip-served" -f $sz.first_load_raw_mb, $sz.first_load_transfer_mb, $sz.first_load_brotli_mb, $sz.total_transfer_mb)

$sha = (& git -C $remaster rev-parse --short HEAD).Trim()
$pv = Join-Path $proj 'build\VERSION.txt'
$vtxt = if (Test-Path -LiteralPath $pv) { (Get-Content -LiteralPath $pv -TotalCount 1).Trim() } else { 'BOTF Remastered (version unknown)' }
Set-Content -LiteralPath (Join-Path $Out 'VERSION.txt') -Encoding utf8 -Value ("{0}`nbrowser build from commit {1}, {2}" -f $vtxt, $sha, (Get-Date -Format 'yyyy-MM-dd HH:mm'))
if ($Release) {
    $latest = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'ChatGPT\BOTF-Web-Latest'
    New-Item -ItemType Directory -Force -Path $latest | Out-Null
    & robocopy $Out $latest /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy to BOTF-Web-Latest failed ($LASTEXITCODE)" }
    Write-Host "release folder ready for upload: $latest  (build $vtxt, commit $sha)"
}
