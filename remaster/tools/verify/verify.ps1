<#
.SYNOPSIS
  One-command verification of BOTF Remastered: export Windows and Web, run every suite, capture and build contact sheets.
  Run it after every merge. Nothing here is scheduled; it only runs when someone runs it.

.EXAMPLE
  powershell -NoProfile -File remaster\tools\verify\verify.ps1
  powershell -NoProfile -File remaster\tools\verify\verify.ps1 -Target windows -SkipCapture   # exe + suites only
  powershell -NoProfile -File remaster\tools\verify\verify.ps1 -SkipExport                     # suites + captures only

.DESCRIPTION
  Phases (as parallel as the machine allows):
    0  import the project once, stamp the version (restored afterwards), work out the run folder
    1  headless, no GPU, all at once: exports (Windows exe with icon and version, Web) and every test suite
    2  GPU, all at once: pc_smoke, and the capture groups (screens, battles, galaxy, compat) in their own windows
    3  web capture in headless Chrome against the localhost server (if the web export exists)
    4  contact sheets (sheet-<group>.png) and summary.md in remaster\captures\<yyyy-MM-dd>\<HHmmss>\
  Exit code 0 only if every export, suite and smoke check passed. A blocked web step is reported, and counts as a failure of the
  web target (it is a real gap), so use -Target windows to leave it out.

  -Sequential runs one Godot process at a time (exports, suites, captures, smoke), BelowNormal priority: use it on a shared PC.

  Environment: GODOT overrides the Godot console exe (default C:\Tools\Godot\Godot_v4.5-stable_win64_console.exe).
#>
[CmdletBinding()]
param(
    [ValidateSet('both', 'windows', 'web')][string]$Target = 'both',
    [switch]$SkipExport,
    [switch]$SkipTests,
    [switch]$SkipCapture,
    [switch]$Sequential,
    [int]$SuiteTimeoutSec = 900,
    [int]$CaptureTimeoutSec = 900
)

$ErrorActionPreference = 'Stop'
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$remaster = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$proj = Join-Path $remaster 'godot'
$godot = if ($env:GODOT) { $env:GODOT } else { 'C:\Tools\Godot\Godot_v4.5-stable_win64_console.exe' }
if (-not (Test-Path -LiteralPath $godot)) { throw "Godot not found at $godot (set GODOT)" }
$python = (Get-Command python -ErrorAction Stop).Source
$stampDate = Get-Date
$run = Join-Path $remaster ("captures\{0}\{1}" -f $stampDate.ToString('yyyy-MM-dd'), $stampDate.ToString('HHmmss'))
$logs = Join-Path $run 'logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$results = New-Object System.Collections.Generic.List[object]

function Add-Result($Name, $Status, $Detail) {
    $results.Add([pscustomobject]@{ Step = $Name; Status = $Status; Detail = $Detail })
    $color = switch ($Status) { 'ok' { 'Green' } 'BLOCKED' { 'Yellow' } 'skipped' { 'DarkGray' } default { 'Red' } }
    Write-Host ("  {0,-8} {1,-26} {2}" -f $Status, $Name, $Detail) -ForegroundColor $color
}

# Starts Godot with the given argument string; stdout and stderr go to logs\<name>.log / .err.
function Start-Godot([string]$Name, [string]$ArgString) {
    $p = Start-Process -FilePath $godot -ArgumentList $ArgString -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $logs "$Name.log") -RedirectStandardError (Join-Path $logs "$Name.err")
    $null = $p.Handle   # caches the handle so ExitCode is readable after redirected-output runs
    try { $p.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal } catch {}
    if ($Sequential) { if (-not $p.WaitForExit(900 * 1000)) { try { Stop-Process -Id $p.Id -Force } catch {} } }
    [pscustomobject]@{ Name = $Name; Proc = $p; Start = Get-Date }
}

# Waits for every job; kills any that pass the timeout. Returns the jobs with .Exit and .TimedOut set.
function Wait-Jobs($Jobs, [int]$TimeoutSec) {
    foreach ($j in $Jobs) {
        $left = [Math]::Max(1, $TimeoutSec - [int]((Get-Date) - $j.Start).TotalSeconds)
        if (-not $j.Proc.WaitForExit($left * 1000)) {
            try { Stop-Process -Id $j.Proc.Id -Force } catch {}
            $j | Add-Member -NotePropertyName TimedOut -NotePropertyValue $true -Force
            $j | Add-Member -NotePropertyName Exit -NotePropertyValue -1 -Force
        } else {
            $j.Proc.WaitForExit()
            $j | Add-Member -NotePropertyName TimedOut -NotePropertyValue $false -Force
            $j | Add-Member -NotePropertyName Exit -NotePropertyValue $j.Proc.ExitCode -Force
        }
    }
}

function Tail-Summary([string]$Name) {
    $f = Join-Path $logs "$Name.log"
    if (-not (Test-Path $f)) { return '' }
    $lines = Get-Content -LiteralPath $f -ErrorAction SilentlyContinue | Where-Object { $_ -match 'passed|failures|checks|FAIL|SMOKE|ok=' }
    if ($lines) { return (($lines | Select-Object -Last 1).Trim()) }
    return ''
}

Write-Host "BOTF verify  run folder: $run" -ForegroundColor Cyan
$wantWin = $Target -in 'both', 'windows'
$wantWeb = $Target -in 'both', 'web'

# ---- Phase 0: import, version stamp ---------------------------------------------------------------------------------
$sha = (git -C $remaster rev-parse --short HEAD 2>$null)
$count = (git -C $remaster rev-list --count HEAD 2>$null)
$dirty = if (git -C $remaster status --porcelain -- . 2>$null) { '-dirty' } else { '' }
$version = "0.1.$count"
$productVersion = "$version ($sha$dirty, $($stampDate.ToString('yyyy-MM-dd HH:mm')))"
Write-Host "version $productVersion"
$presets = Join-Path $proj 'export_presets.cfg'
$presetsOriginal = [System.IO.File]::ReadAllText($presets)

# Content packs (docs/DATA-FORMAT.md section 10): copy remaster/data/*.pack.json into res://data/ so the exports carry them
# inside the .pck (the core never opens files; the client reads res://data/<pack>.pack.json and calls loadContent).
# Byte copy: the checksum is over the exact bytes. The folder ignores itself, so the copy never makes the tree dirty.
$packDir = Join-Path $proj 'data'
New-Item -ItemType Directory -Force -Path $packDir | Out-Null
[System.IO.File]::WriteAllText((Join-Path $packDir '.gitignore'), "*`n")
$packs = @(Get-ChildItem -LiteralPath (Join-Path $remaster 'data') -Filter '*.pack.json' -File)
foreach ($p in $packs) { Copy-Item -LiteralPath $p.FullName -Destination (Join-Path $packDir $p.Name) -Force }
$packsOk = $packs.Count -gt 0 -and @($packs | Where-Object {
    (Get-FileHash -LiteralPath $_.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $packDir $_.Name)).Hash }).Count -eq 0
Add-Result 'content-packs' $(if ($packsOk) { 'ok' } else { 'FAIL' }) "$($packs.Count) pack(s) copied to res://data"

$imp = Start-Godot 'import' "--headless --audio-driver Dummy --path `"$proj`" --import"
Wait-Jobs @($imp) 300
Add-Result 'import' $(if ($imp.Exit -eq 0) { 'ok' } else { 'FAIL' }) "exit $($imp.Exit)"

$jobs = @()
$exportJobs = @()
$suiteJobs = @()

# ---- Phase 1: exports and suites (headless, no GPU) -----------------------------------------------------------------------
try {
    if (-not $SkipExport) {
        # Stamp the Windows preset's version fields for the export; the finally block restores the committed file.
        $stamped = $presetsOriginal -replace 'application/file_version="[^"]*"', "application/file_version=`"$version.0`"" `
            -replace 'application/product_version="[^"]*"', "application/product_version=`"$productVersion`"" `
            -replace 'include_filter="fixtures/\*\.json"', 'include_filter="fixtures/*.json, data/*.pack.json"'
        [System.IO.File]::WriteAllText($presets, $stamped)
        New-Item -ItemType Directory -Force -Path (Join-Path $proj 'build\web') | Out-Null
        Set-Content -LiteralPath (Join-Path $proj 'build\VERSION.txt') -Value "BOTF Remastered $productVersion" -Encoding ASCII
        if ($wantWin) {
            $exportJobs += Start-Godot 'export-windows' "--headless --audio-driver Dummy --path `"$proj`" --export-release `"Windows Desktop`" build/BOTFRemastered.exe"
        }
        if ($wantWeb) {
            $exportJobs += Start-Godot 'export-web' "--headless --audio-driver Dummy --path `"$proj`" --export-release `"Web`" build/web/index.html"
        }
        # Give the exporters a moment to read the preset file before anything restores it.
        Start-Sleep -Seconds 4
    }

    if (-not $SkipTests) {
        $suites = @(
            @('core', 'res://tests/run_tests.gd'),
            @('trade-route', 'res://tests/test_trade_route.gd'),
            @('ui', 'res://scenes/ui/tests/test_campaign_ui.gd'),
            @('battle-timeline', 'res://scenes/battle/tests/test_battle_timeline.gd'),
            @('camera-rig', 'res://scenes/battle/tests/test_camera_rig.gd'),
            @('hero-models', 'res://scenes/battle/tests/test_hero_models.gd'),
            @('title-fleet', 'res://scenes/battle/tests/test_title_fleet.gd'),
            @('galaxy-map', 'res://scenes/galaxy/tests/test_galaxy_map.gd'),
            @('audio', 'res://scripts/audio/tests/test_audio.gd'),
            @('check-scripts', 'res://scenes/ui/tools/check_scripts.gd')
        )
        foreach ($s in $suites) {
            $suiteJobs += Start-Godot ("suite-" + $s[0]) "--headless --audio-driver Dummy --path `"$proj`" -s $($s[1])"
        }
    }

    # While phase 1 runs, the GPU is free: start phase 2 now (the captures do not touch the build folder).
    $capJobs = @()
    if (-not $SkipCapture) {
        $env:SHOT_FULL = '1'
        $shotArgs = "--audio-driver Dummy --resolution 1920x1080 --path `"$proj`" -s res://scenes/ui/tools/screenshot.gd --"
        $groups = [ordered]@{
            'screens' = @('title', 'new', 'galaxy', 'system', 'empire', 'research', 'fleets', 'diplomacy', 'intel', 'database')
            'battles' = @('battle-r1', 'battle-r3', 'battle-volley', 'battle-strip', 'battle-closeup')
            'galaxy'  = @('galaxy-open', 'galaxy-features', 'galaxy-ion', 'galaxy-nebula', 'galaxy-rift', 'galaxy-wormhole', 'galaxy-blackhole')
        }
        foreach ($g in $groups.Keys) {
            $out = Join-Path $run $g
            New-Item -ItemType Directory -Force -Path $out | Out-Null
            $capJobs += Start-Godot "capture-$g" "$shotArgs `"$out`" $($groups[$g] -join ' ')"
        }
        # The same anomaly shots under the Compatibility renderer (what the web build uses).
        $out = Join-Path $run 'compat'
        New-Item -ItemType Directory -Force -Path $out | Out-Null
        $capJobs += Start-Godot 'capture-compat' ("--rendering-method gl_compatibility --rendering-driver opengl3 $shotArgs `"$out`" title galaxy-features galaxy-ion galaxy-nebula galaxy-rift galaxy-wormhole galaxy-blackhole")
        $smokeOut = Join-Path $run 'smoke'
        New-Item -ItemType Directory -Force -Path $smokeOut | Out-Null
        $capJobs += Start-Godot 'pc-smoke' "--audio-driver Dummy --path `"$proj`" -s res://tools/pc_smoke.gd -- `"$smokeOut`""
        Remove-Item Env:\SHOT_FULL -ErrorAction SilentlyContinue
    }

    # ---- Collect phase 1 ------------------------------------------------------------------------------------------------
    if ($exportJobs) { Wait-Jobs $exportJobs $SuiteTimeoutSec }
} finally {
    [System.IO.File]::WriteAllText($presets, $presetsOriginal)   # the committed preset file is never left stamped
}

foreach ($j in $exportJobs) {
    $what = $j.Name
    if ($j.TimedOut) { Add-Result $what 'FAIL' 'timed out'; continue }
    $log = (Get-Content -LiteralPath (Join-Path $logs "$what.log") -ErrorAction SilentlyContinue) + (Get-Content -LiteralPath (Join-Path $logs "$what.err") -ErrorAction SilentlyContinue)
    $problem = ($log | Where-Object { $_ -match 'ERROR|FAILED|not found|Cannot|No export template|rcedit' } | Select-Object -First 2) -join ' | '
    if ($what -eq 'export-windows') {
        $exe = Join-Path $proj 'build\BOTFRemastered.exe'
        if ($j.Exit -eq 0 -and (Test-Path $exe) -and ((Get-Item $exe).LastWriteTime -gt $stampDate.AddMinutes(-1))) {
            $info = (Get-Item $exe).VersionInfo
            $size = '{0:N0} MB' -f ((Get-Item $exe).Length / 1MB)
            Add-Result $what 'ok' "$size, file version '$($info.FileVersion)', product '$($info.ProductVersion)'"
            if ($info.FileVersion -ne "$version.0") {
                Add-Result 'exe-version-stamp' 'FAIL' "exe file version '$($info.FileVersion)', expected '$version.0'"
            }
            $con = Join-Path $proj 'build\BOTFRemastered.console.exe'
            if (Test-Path $con) {
                $boot = & $con --headless --audio-driver Dummy --quit-after 30 2>&1 | Out-String
                Add-Result 'exe-boot' $(if ($boot -match 'core ok=true') { 'ok' } else { 'FAIL' }) (($boot -split "`n" | Where-Object { $_ -match 'core ok|ERROR' } | Select-Object -First 1).Trim())
            }
        } else {
            Add-Result $what 'FAIL' "exit $($j.Exit). $problem"
        }
    } else {
        $idx = Join-Path $proj 'build\web\index.html'
        if ($j.Exit -eq 0 -and (Test-Path $idx) -and ((Get-Item $idx).LastWriteTime -gt $stampDate.AddMinutes(-1))) {
            Add-Result $what 'ok' 'build\web\index.html'
        } else {
            $why = if ($problem -match 'No export template') { 'web export templates not installed: run tools/verify/setup-web-export.ps1 once' } else { $problem }
            Add-Result $what 'BLOCKED' "exit $($j.Exit). $why"
        }
    }
}

if ($suiteJobs) {
    Wait-Jobs $suiteJobs $SuiteTimeoutSec
    foreach ($j in $suiteJobs) {
        $detail = Tail-Summary $j.Name
        $errs = @(Get-Content -LiteralPath (Join-Path $logs "$($j.Name).err"), (Join-Path $logs "$($j.Name).log") -ErrorAction SilentlyContinue | Where-Object { $_ -match 'SCRIPT ERROR|Parse Error' }).Count
        $ok = ($j.Exit -eq 0 -and -not $j.TimedOut -and $errs -eq 0)
        if ($j.TimedOut) { $detail = 'timed out' } elseif ($errs -gt 0) { $detail = "$errs script errors; $detail" }
        Add-Result $j.Name $(if ($ok) { 'ok' } else { 'FAIL' }) $detail
    }
}

# ---- Phase 2 collect ------------------------------------------------------------------------------------------------------
if ($capJobs) {
    Wait-Jobs $capJobs $CaptureTimeoutSec
    foreach ($j in $capJobs) {
        $detail = Tail-Summary $j.Name
        $isSmoke = $j.Name -eq 'pc-smoke'
        $png = 0
        if (-not $isSmoke) { $png = @(Get-ChildItem -Path (Join-Path $run ($j.Name -replace '^capture-', '')) -Filter *.png -ErrorAction SilentlyContinue).Count }
        $ok = ($j.Exit -eq 0 -and -not $j.TimedOut)
        if ($j.TimedOut) { $detail = 'timed out' } elseif (-not $isSmoke) { $detail = "$png shots saved. $detail" }
        Add-Result $j.Name $(if ($ok) { 'ok' } else { 'FAIL' }) $detail
    }
}

# battle-scene is wall-clock sensitive (a hitch under load teleports a ship in its frame-step check): it runs alone, after the
# parallel work, so a pass or fail means the code and not the machine's load.
if (-not $SkipTests) {
    $bs = Start-Godot 'suite-battle-scene' "--headless --audio-driver Dummy --path `"$proj`" -s res://scenes/battle/tests/test_battle_scene.gd"
    Wait-Jobs @($bs) $SuiteTimeoutSec
    $errs = @(Get-Content -LiteralPath (Join-Path $logs 'suite-battle-scene.err'), (Join-Path $logs 'suite-battle-scene.log') -ErrorAction SilentlyContinue | Where-Object { $_ -match 'SCRIPT ERROR|Parse Error' }).Count
    $note = ''
    if ($bs.Exit -ne 0 -or $errs -gt 0) {
        # One retry, reported: other builds on this PC (L4/L5 compile the C++ core) can stall a frame past the test's step check.
        $first = (Get-Content -LiteralPath (Join-Path $logs 'suite-battle-scene.log') | Where-Object { $_ -match 'FAIL' } | Select-Object -First 1)
        $bs = Start-Godot 'suite-battle-scene-retry' "--headless --audio-driver Dummy --path `"$proj`" -s res://scenes/battle/tests/test_battle_scene.gd"
        Wait-Jobs @($bs) $SuiteTimeoutSec
        $note = " (retry; first run: $($first.Trim()))"
    }
    $name = if ($note) { 'suite-battle-scene-retry' } else { 'suite-battle-scene' }
    $errs = @(Get-Content -LiteralPath (Join-Path $logs "$name.err"), (Join-Path $logs "$name.log") -ErrorAction SilentlyContinue | Where-Object { $_ -match 'SCRIPT ERROR|Parse Error' }).Count
    Add-Result 'suite-battle-scene' $(if ($bs.Exit -eq 0 -and $errs -eq 0) { 'ok' } else { 'FAIL' }) ((Tail-Summary $name) + $note)
}

# Web asset bundle: only the web tier goes beside the web export (packs/planets_web.pck, fetched by AppLoader.fetch_web_packs).
# The desktop pack (planets_desktop.pck) is never copied here. Built by scenes/galaxy/tools/pack_planets.gd ... web and kept in the
# user data folder's packs/ (private; not in git). Web bundle budget: under 40 MB all told.
if ($wantWeb -and -not $SkipExport) {
    $webPacks = Join-Path $proj 'build\web\packs'
    New-Item -ItemType Directory -Force -Path $webPacks | Out-Null
    Get-ChildItem -LiteralPath $webPacks -Filter '*.pck' -ErrorAction SilentlyContinue | Remove-Item -Force
    $userPacks = Join-Path $env:APPDATA 'Godot\app_userdata\BOTF Remastered (Godot)\packs'
    $wp = Join-Path $userPacks 'planets_web.pck'
    if (Test-Path -LiteralPath $wp) { Copy-Item -LiteralPath $wp -Destination $webPacks -Force }
    $webBytes = (Get-ChildItem -LiteralPath $webPacks -Filter '*.pck' -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum
    $webMb = [Math]::Round(([double]$webBytes) / 1MB, 1)
    Add-Result 'web-packs' $(if ($webBytes -gt 0 -and $webBytes -lt 40MB) { 'ok' } else { 'FAIL' }) "$webMb MB in build\web\packs (budget 40 MB; desktop pack excluded)"
}

# ---- Phase 3: web capture -------------------------------------------------------------------------------------------------
if ($wantWeb -and -not $SkipCapture) {
    $webOut = Join-Path $run 'web'
    $webIdx = Join-Path $proj 'build\web\index.html'
    if (Test-Path $webIdx) {
        $o = & $python (Join-Path $PSScriptRoot 'web_capture.py') (Join-Path $proj 'build\web') $webOut 2>&1 | Out-String
        $o | Set-Content -LiteralPath (Join-Path $logs 'web-capture.log')
        $first = ($o -split "`n" | Where-Object { $_ -match 'web:|error' } | Select-Object -First 2) -join ' | '
        Add-Result 'web-capture' $(if ($LASTEXITCODE -eq 0) { 'ok' } else { 'BLOCKED' }) $first
    } else {
        Add-Result 'web-capture' 'BLOCKED' 'no web export to capture'
    }
}

# ---- Phase 4: contact sheets ----------------------------------------------------------------------------------------------
if (-not $SkipCapture) {
    $s = & $python (Join-Path $PSScriptRoot 'make_sheets.py') $run 2>&1 | Out-String
    Write-Host $s.TrimEnd()
    Add-Result 'contact-sheets' $(if ($LASTEXITCODE -eq 0) { 'ok' } else { 'FAIL' }) "$run"
}

# ---- Summary --------------------------------------------------------------------------------------------------------------
$fail = @($results | Where-Object { $_.Status -in 'FAIL', 'BLOCKED' })
$md = @("# verify $($stampDate.ToString('yyyy-MM-dd HH:mm'))  $productVersion", '', "Target: $Target. Elapsed: $([int]$sw.Elapsed.TotalSeconds) s.", '', '| Step | Status | Detail |', '|---|---|---|')
foreach ($r in $results) { $md += "| $($r.Step) | $($r.Status) | $($r.Detail -replace '\|', '/') |" }
$md | Set-Content -LiteralPath (Join-Path $run 'summary.md')
$results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'summary.json')
Set-Content -LiteralPath (Join-Path $remaster 'captures\latest.txt') -Value $run
Write-Host ("verify finished in {0} s: {1} ok, {2} failed or blocked. Contact sheets: {3}\sheet-*.png" -f [int]$sw.Elapsed.TotalSeconds, @($results | Where-Object Status -eq 'ok').Count, $fail.Count, $run) -ForegroundColor Cyan
exit $(if (@($results | Where-Object { $_.Status -eq 'FAIL' }).Count -gt 0 -or ($Target -ne 'windows' -and @($results | Where-Object { $_.Status -eq 'BLOCKED' }).Count -gt 0)) { 1 } else { 0 })
