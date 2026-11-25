# Hull look check (hull-look lane, 25 Nov 2026): the title fleet of every empire and every battle station, drawn at 1920x1080 and
# measured. Fails on (a) a procedural stand-in hull where a hero model should be, (b) a low-detail model (triangle and texture bars
# in scenes/battle/tools/hull_look_capture.gd), (c) white specks in a fixed close-up (speck count per million hull pixels), and,
# with -Web <build dir>, (d) a web build whose packs lack a title or station hull (check_title_packs.py).
#
#   powershell -File remaster\tools\verify\hull_look_check.ps1 [-Out <dir>] [-Web <web build dir>] [-Empires 1,2,3,4,5] [-NoAssert]
#
# Needs LocalAssets (remaster\game\LocalAssets with ships\godot-hero-manifest.json) or the ship pack beside a packaged exe. Run it
# under the heavy lock, BelowNormal, with audio off (the machine rules); it starts one Godot and no audio. Exit 0 = every bar met.
param(
  [string]$Out = '',
  [string]$Web = '',
  [string]$Empires = '1,2,3,4,5',
  [switch]$NoAssert,
  [string]$Godot = $(if ($env:GODOT) { $env:GODOT } else { 'C:\Tools\Godot\Godot_v4.5-stable_win64_console.exe' })
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$proj = (Resolve-Path (Join-Path $here '..\..\godot')).Path
if ($Out -eq '') { $Out = Join-Path $env:TEMP ('hull-look-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
New-Item -ItemType Directory -Force $Out | Out-Null
$log = Join-Path $Out 'hull-look.log'
$assert = if ($NoAssert) { '' } else { '--assert' }
$a = "--path `"$proj`" --resolution 1920x1080 --audio-driver Dummy -s res://scenes/battle/tools/hull_look_capture.gd -- `"$Out`" all --empires=$Empires $assert"
$p = Start-Process -FilePath $Godot -ArgumentList $a -PassThru -NoNewWindow -RedirectStandardOutput $log -RedirectStandardError ($log + '.err')
$p.PriorityClass = 'BelowNormal'
if (-not $p.WaitForExit(1500000)) { Stop-Process -Id $p.Id -Force; Write-Host 'HULLLOOK FAIL timed out after 25 min'; exit 1 }
Get-Content $log | Where-Object { $_ -match '^HULLLOOK' } | ForEach-Object { Write-Host $_ }
$code = $p.ExitCode
if ($Web -ne '') {
  & python (Join-Path $here 'check_title_packs.py') $Web
  if ($LASTEXITCODE -ne 0) { $code = 1 }
}
Write-Host "hull look check: exit $code (frames and hull-look.json in $Out)"
exit $code
