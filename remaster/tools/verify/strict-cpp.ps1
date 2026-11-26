<#
.SYNOPSIS
  The strict C++ compile GitHub CI (remaster-ci) does, run locally in WSL: gcc and clang with -Wall -Wextra -Werror, clang with
  ASan+UBSan, plus the Unreal one-TU stand-in (-Wshadow -Werror, engine macro names). Windows MSVC does not catch what these do
  (missing field initializers, incomplete types in libstdc++, unused-but-set variables), which is how CI sat red 8-26 Nov 2026.

.DESCRIPTION
  Needs a WSL distro with g++, clang++, cmake and make:  wsl -u root -- apt-get install -y g++ clang cmake make
  Builds in the distro's own filesystem (~/botf-strict-gcc, ~/botf-strict-clang), so repeat runs are incremental.
  Compile only by default (that is the part Windows cannot see); -RunTests also runs ctest in each build.
  Exit code 0 only if every compile passed. Exit 2 = the tooling is missing (reported, not silently skipped).

.EXAMPLE
  powershell -NoProfile -File remaster\tools\verify\strict-cpp.ps1
  powershell -NoProfile -File remaster\tools\verify\strict-cpp.ps1 -Only gcc -Jobs 4
#>
[CmdletBinding()]
param(
    [ValidateSet('both', 'gcc', 'clang')][string]$Only = 'both',
    [int]$Jobs = 6,
    [switch]$RunTests,
    [string]$Distro = 'Ubuntu'
)
$ErrorActionPreference = 'Stop'
$remaster = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

function Invoke-Wsl([string]$Script) {
    $ErrorActionPreference = 'Continue'   # wsl.exe writes warnings to stderr; the exit code is what counts
    # The script goes in on stdin: wsl.exe's own argument handling would expand $VARIABLES before bash sees them.
    $out = ($Script -replace "`r?`n", "`n") | & wsl.exe -d $Distro -- bash -s 2>&1
    [pscustomobject]@{ Exit = $LASTEXITCODE; Text = (($out | ForEach-Object { "$_" -replace "`0", '' }) -join "`n") }
}

$probe = Invoke-Wsl 'command -v g++ clang++ cmake make >/dev/null && echo TOOLS_OK'
if ($probe.Text -notmatch 'TOOLS_OK') {
    Write-Host "strict-cpp: WSL distro '$Distro' lacks g++/clang++/cmake/make. Install: wsl -d $Distro -u root -- apt-get install -y g++ clang cmake make" -ForegroundColor Red
    exit 2
}
$src = (Invoke-Wsl "wslpath -a '$remaster'").Text.Trim()
$mem = (Invoke-Wsl 'cat /proc/meminfo').Text
$freeMb = if ($mem -match 'MemAvailable:\s+(\d+)') { [int]([long]$Matches[1] / 1024) } else { 99999 }
if ($freeMb -lt 4000) { $Jobs = [Math]::Min($Jobs, 2); Write-Host "strict-cpp: only $freeMb MB free in WSL, using -j$Jobs" -ForegroundColor Yellow }

$cfgs = @()
if ($Only -in 'both', 'gcc')   { $cfgs += @{ Name = 'gcc';   Cxx = 'g++';     San = 'OFF' } }
if ($Only -in 'both', 'clang') { $cfgs += @{ Name = 'clang'; Cxx = 'clang++'; San = 'ON' } }

$failed = $false
foreach ($c in $cfgs) {
    $b = "`$HOME/botf-strict-$($c.Name)"
    $cmd = "set -o pipefail; cmake -S '$src' -B $b -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_COMPILER=$($c.Cxx) -DBOTF_SANITIZE=$($c.San) >/dev/null && cmake --build $b -j $Jobs 2>&1 | grep -E 'error|warning: ' | cut -c1-300 | head -25; test `${PIPESTATUS[0]} -eq 0"
    $r = Invoke-Wsl $cmd
    if ($r.Exit -eq 0 -and $RunTests) { $r = Invoke-Wsl "ctest --test-dir $b --output-on-failure 2>&1 | tail -15; test `${PIPESTATUS[0]} -eq 0" }
    if ($r.Exit -eq 0) { Write-Host ("  ok       strict-{0,-6} {1} -Wall -Wextra -Werror" -f $c.Name, $c.Cxx) -ForegroundColor Green }
    else { $failed = $true; Write-Host ("  FAIL     strict-{0,-6} {1}`n{2}" -f $c.Name, $c.Cxx, $r.Text) -ForegroundColor Red }
}

$comp = (@($cfgs | ForEach-Object { $_.Cxx }) -join ' ')
$u = Invoke-Wsl "bash '$src/game/Tools/check-unreal-tu.sh' $comp 2>&1 | tail -15; test `${PIPESTATUS[0]} -eq 0"
if ($u.Exit -eq 0) { Write-Host "  ok       strict-unreal-tu     one-TU core and view models, engine macro names" -ForegroundColor Green }
else { $failed = $true; Write-Host "  FAIL     strict-unreal-tu`n$($u.Text)" -ForegroundColor Red }

exit $(if ($failed) { 1 } else { 0 })
