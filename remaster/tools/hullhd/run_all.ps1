# Builds every ship of a config in batches (the heavy lock is taken and released per batch so other lanes get turns).
#   powershell -File run_all.ps1 -Config <cfg.json> [-Roles trial,new,swap,rebuild] [-Batch 16] [-Log <prefix>]
param([Parameter(Mandatory)][string]$Config, [string]$Roles = "trial,new,swap,rebuild", [int]$Batch = 16, [string]$Log = "C:\Users\BiggDogg\Documents\ChatGPT\hullhd-captures\logs\build")
$cfg = Get-Content -Raw $Config | ConvertFrom-Json
$keys = @()
foreach ($r in ($Roles -split ",")) { $keys += @($cfg.ships | Where-Object { $_.role -eq $r } | ForEach-Object { $_.key }) }
for ($i = 0; $i -lt $keys.Count; $i += $Batch) {
    $chunk = $keys[$i..([Math]::Min($i + $Batch, $keys.Count) - 1)] -join ","
    $n = [int]($i / $Batch) + 1
    "batch $n : $chunk"
    & (Join-Path $PSScriptRoot "run_build.ps1") -Config $Config -Keys $chunk -Log "$Log-$n.log"
    Start-Sleep 5
}
"all batches done: $($keys.Count) ships"
