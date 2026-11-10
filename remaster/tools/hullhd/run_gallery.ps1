# Renders hull close-ups with godot/scenes/battle/tools/hull_gallery.gd under the BOTF heavy lock (BelowNormal, Dummy audio).
#   powershell -File run_gallery.ps1 -Out <dir> -Specs ship.fed.nebula,ship.fed.nebula:starboard [-Proj <godot project>] [-Exe <exe>]
# Dev tree: hero models resolve through $BOTF_HERO_MANIFEST / $BOTF_RESOLVED_MANIFEST (the main tree's LocalAssets).
param([Parameter(Mandatory)][string]$Out, [Parameter(Mandatory)][string]$Specs, [string]$Proj = "", [string]$Exe = "", [string]$Log = "", [string]$Res = "1920x1080")
$ErrorActionPreference = "Stop"
$SpecList = @($Specs -split ",")
$lock = "C:\Users\BiggDogg\Documents\ChatGPT\.botf-heavy.lock"
if (-not $Proj) { $Proj = (Resolve-Path (Join-Path $PSScriptRoot "..\..\godot")).Path }
$la = "C:\Users\BiggDogg\Documents\ChatGPT\Game Remakes\remaster\game\LocalAssets"
while (Test-Path $lock) {
    $age = (Get-Date) - (Get-Item $lock).LastWriteTime
    $busy = Get-Process godot*, BOTFRemastered*, realesrgan*, UnrealEditor* -ErrorAction SilentlyContinue
    if ($age.TotalMinutes -gt 40 -and -not $busy) { Remove-Item $lock -Force; break }
    Start-Sleep 60
}
"hullhd $(Get-Date -Format HH:mm) hull gallery $($SpecList.Count) frames" | Set-Content $lock
try {
    New-Item -ItemType Directory -Force $Out | Out-Null
    $env:BOTF_HERO_MANIFEST = "$la\ships\godot-hero-manifest.json"
    $env:BOTF_RESOLVED_MANIFEST = "$la\resolved-manifest.json"
    if ($Exe) {
        $a = @("--audio-driver", "Dummy", "--resolution", $Res, "-s", "res://scenes/battle/tools/hull_gallery.gd", "--", $Out) + $SpecList
        $file = $Exe; $wd = Split-Path $Exe
    } else {
        $a = @("--path", $Proj, "--audio-driver", "Dummy", "--resolution", $Res, "-s", "res://scenes/battle/tools/hull_gallery.gd", "--", $Out) + $SpecList
        $file = "C:\Tools\Godot\Godot_v4.5-stable_win64_console.exe"; $wd = $Proj
    }
    $lg = if ($Log) { $Log } else { Join-Path $Out "gallery.log" }
    $p = Start-Process -FilePath $file -ArgumentList $a -WorkingDirectory $wd -PassThru -NoNewWindow -RedirectStandardOutput $lg -RedirectStandardError "$lg.err"
    $p.PriorityClass = "BelowNormal"
    if (-not $p.WaitForExit(1500 * 1000)) { Stop-Process -Id $p.Id -Force; "TIMEOUT" }
    "godot exit $($p.ExitCode)"
} finally {
    Remove-Item $lock -Force -ErrorAction SilentlyContinue
}
