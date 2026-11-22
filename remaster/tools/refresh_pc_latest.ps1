<#
.SYNOPSIS
  Refreshes the fixed PC beta folder Documents\ChatGPT\BOTF-PC-Latest from the Windows export in godot\build.
    powershell -NoProfile -File remaster\tools\refresh_pc_latest.ps1 [-Export] [-Force] [-WhatIf]

  Simon plays the PC beta from one path, always the latest. Every round that produces a web build runs this too (FINISH-PLAN
  machine rules).

  1. Checks that godot\build\BOTFRemastered.exe is the export of HEAD (its product version carries the commit and the tree is
     clean). If it is not, -Export runs verify.ps1 -Target windows -SkipTests -SkipCapture (BelowNormal, takes the heavy lock
     rules of verify) to produce it; without -Export the script stops and says so.
  2. If BOTF-PC-Latest already holds this exact exe, only the text files are refreshed (-Force repackages anyway).
  3. Otherwise the previous BOTF-PC-Latest is MOVED (never deleted) to ~\_archive\<yyyy-mm>\botf-pc-<version>-<stamp>\ and the new
     package (exe, core dll, packs\) is copied in with VERSION.txt, WHATS-NEW.txt (from remaster\docs\PC-WHATS-NEW.txt, edit it
     each round) and BOTF-RC-CHECKSHEET.md (from Documents\ChatGPT, beside the folder).
  4. The move is appended to AI_stuff\standing\MOVES-LOG.md.
  Proof of a package: tools\pc_smoke.gd is excluded from the export, so run the packaged exe with the packaged capture tool
  (SHOT_FULL=1, wrapped in tools\verify\real_saves_guard.py snapshot/check), then look at the frames:
    BOTFRemastered.exe --audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/screenshot.gd -- <out> title new galaxy system empire diplomacy battle-r1 battle-r3 battle-volley
#>
[CmdletBinding()]
param([switch]$Export, [switch]$Force, [switch]$WhatIf)
$ErrorActionPreference = 'Stop'
(Get-Process -Id $PID).PriorityClass = 'BelowNormal'
$remaster = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $remaster 'godot\build'
$chat = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'ChatGPT'
$latest = Join-Path $chat 'BOTF-PC-Latest'
$exe = Join-Path $build 'BOTFRemastered.exe'
$sha = (& git -C $remaster rev-parse --short HEAD).Trim()
$dirty = [bool](& git -C $remaster status --porcelain --untracked-files=no -- .)

function Get-ExeProduct($p) { if (Test-Path -LiteralPath $p) { (Get-Item -LiteralPath $p).VersionInfo.ProductVersion } else { '' } }
$prod = Get-ExeProduct $exe
$fresh = ($prod -match [regex]::Escape("($sha,")) -and -not $dirty
if (-not $fresh) {
    if (-not $Export) { throw "godot\build exe is '$prod', not a clean export of HEAD $sha. Re-run with -Export (or run verify.ps1 -Target windows -SkipTests -SkipCapture)." }
    & powershell -NoProfile -File (Join-Path $remaster 'tools\verify\verify.ps1') -Target windows -SkipTests -SkipCapture
    if ($LASTEXITCODE -ne 0) { throw 'verify -Target windows failed' }
    $prod = Get-ExeProduct $exe
    if ($prod -notmatch [regex]::Escape("($sha")) { throw "export still not HEAD: '$prod'" }
}
$version = ($prod -split ' ')[0]
Write-Host "package: $prod"

$sameExe = (Test-Path -LiteralPath (Join-Path $latest 'BOTFRemastered.exe')) -and
    ((Get-FileHash -LiteralPath $exe).Hash -eq (Get-FileHash -LiteralPath (Join-Path $latest 'BOTFRemastered.exe')).Hash)
$archived = $null
if ((Test-Path -LiteralPath $latest) -and ($Force -or -not $sameExe)) {
    $old = Get-ExeProduct (Join-Path $latest 'BOTFRemastered.exe')
    $oldv = if ($old) { ($old -split ' ')[0] } else { 'unknown' }
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $archived = Join-Path ([Environment]::GetFolderPath('UserProfile')) ("_archive\{0}\botf-pc-{1}-{2}" -f (Get-Date -Format 'yyyy-MM'), $oldv, $stamp)
    Write-Host "archive previous: $latest -> $archived"
    if (-not $WhatIf) {
        New-Item -ItemType Directory -Force -Path (Split-Path $archived) | Out-Null
        Move-Item -LiteralPath $latest -Destination $archived
        $log = Join-Path ([Environment]::GetFolderPath('UserProfile')) 'AI_stuff\standing\MOVES-LOG.md'
        if (Test-Path -LiteralPath $log) {
            Add-Content -LiteralPath $log -Encoding utf8 -Value ("{0} | {1} | {2} | Barclay | refresh_pc_latest.ps1: previous PC beta {3} archived for {4}" -f (Get-Date -Format 'yyyy-MM-dd'), $latest, $archived, $oldv, $version)
        }
    }
}
if ($WhatIf) { Write-Host 'WhatIf: nothing copied.'; return }
New-Item -ItemType Directory -Force -Path $latest | Out-Null
if ($Force -or -not $sameExe) {
    & robocopy $build $latest 'BOTFRemastered.exe' 'libbotf_core.windows.template_release.x86_64.dll' /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy exe failed ($LASTEXITCODE)" }
    & robocopy (Join-Path $build 'packs') (Join-Path $latest 'packs') /E /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy packs failed ($LASTEXITCODE)" }
}
$when = (& git -C $remaster log -1 --format=%cd --date=format:'%Y-%m-%d %H:%M' HEAD).Trim()
Set-Content -LiteralPath (Join-Path $latest 'VERSION.txt') -Encoding utf8 -Value @(
    "BOTF Remastered $prod", "version $version", "commit $sha$(if ($dirty) { ' (dirty tree)' })", "commit date $when", "packaged $(Get-Date -Format 'yyyy-MM-dd HH:mm')",
    'Play: BOTFRemastered.exe in this folder. Always the latest PC beta; the previous one is under _archive.')
foreach ($pair in @(@((Join-Path $remaster 'docs\PC-WHATS-NEW.txt'), 'WHATS-NEW.txt'), @((Join-Path $chat 'BOTF-RC-CHECKSHEET.md'), 'BOTF-RC-CHECKSHEET.md'))) {
    if (Test-Path -LiteralPath $pair[0]) { Copy-Item -LiteralPath $pair[0] -Destination (Join-Path $latest $pair[1]) -Force }
    else { Write-Warning "missing $($pair[0])" }
}
Write-Host "BOTF-PC-Latest ready: $latest ($prod)"
