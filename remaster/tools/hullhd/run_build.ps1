# Builds hero glbs from the config (hullhd_build.py in Blender, BelowNormal), under the BOTF heavy lock.
#   powershell -File run_build.ps1 -Config <hullhd-ships.json> [-Keys ship.rom.*,ship.fed.nebula] [-Log <file>]
param([Parameter(Mandatory)][string]$Config, [string]$Keys = "", [string]$Log = "")
$ErrorActionPreference = "Stop"
$lock = "C:\Users\BiggDogg\Documents\ChatGPT\.botf-heavy.lock"
$game = "C:\Users\BiggDogg\Documents\ChatGPT\Game Remakes\remaster\game"
$blender = "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
$script = Join-Path $PSScriptRoot "hullhd_build.py"
# wait for the lock (60 s polls); a lock older than 40 min with no heavy process is stale
while (Test-Path $lock) {
    $age = (Get-Date) - (Get-Item $lock).LastWriteTime
    $busy = Get-Process godot*, BOTFRemastered*, realesrgan*, UnrealEditor* -ErrorAction SilentlyContinue
    if ($age.TotalMinutes -gt 40 -and -not $busy) { Remove-Item $lock -Force; break }
    Start-Sleep 60
}
"hullhd $(Get-Date -Format HH:mm) blender hero build" | Set-Content $lock
try {
    $env:BOTF_GAME_DIR = $game
    $a = @("-b", "--factory-startup", "--threads", "2", "--python", $script, "--", "--config", $Config, "--gltf", "--gltf-lods", "0")
    if ($Keys) { $a += @("--keys", $Keys) }
    $p = Start-Process -FilePath $blender -ArgumentList $a -PassThru -NoNewWindow -RedirectStandardOutput $(if ($Log) { $Log } else { "$env:TEMP\hullhd-build.log" })
    $p.PriorityClass = "BelowNormal"
    $p.WaitForExit()
    "blender exit $($p.ExitCode)"
} finally {
    Remove-Item $lock -Force -ErrorAction SilentlyContinue
}
