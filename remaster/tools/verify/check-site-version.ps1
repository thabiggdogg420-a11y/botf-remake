<#
.SYNOPSIS
  Read-only: compares the live site's VERSION.txt with the latest local web build. Prints MATCH, or "site is X, latest build is Y".
  Uploads are done by ChatGPT (BOTF-Web-Latest\UPDATE-SITE.md); this only looks. Never fails the caller (exit 0 always).
#>
[CmdletBinding()]
param(
    [string]$Url = 'https://free4allhub.com/botfremaster/VERSION.txt',
    [string]$Latest = ''   # default: <repo>\..\BOTF-Web-Latest\VERSION.txt (the repo sits in Documents\ChatGPT\Game Remakes)
)
# Site and local files word the line differently; compare the version and short commit only.
function Ident([string]$l) { if ($l -match '(\d+\.\d+\.\d+)\s*\(([0-9a-f]{7,})') { "$($Matches[1]) ($($Matches[2]))" } else { $l } }
function First-Line([string]$t) { (($t -replace "^\uFEFF", '') -split "\r?\n" | Where-Object { $_.Trim() } | Select-Object -First 1).Trim() }
try {
    if (-not $Latest) { $Latest = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) '..\..\..\..\BOTF-Web-Latest\VERSION.txt' }
    $local = Ident (First-Line (Get-Content -LiteralPath $Latest -Raw -Encoding UTF8))
    $site = Ident (First-Line (Invoke-WebRequest -Uri $Url -UseBasicParsing -TimeoutSec 20 -Headers @{ 'Cache-Control' = 'no-cache' }).Content)
    if ($site -eq $local) { Write-Output "MATCH ($local)" } else { Write-Output "site is $site, latest build is $local" }
} catch { Write-Output "site check unavailable: $($_.Exception.Message)" }
exit 0
