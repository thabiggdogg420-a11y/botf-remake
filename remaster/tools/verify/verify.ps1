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

  Sequential is the default: one Godot process at a time (exports, suites, captures, smoke), BelowNormal priority, so a shared PC
  is not flooded with windows. -Parallel restores the old all-at-once phases.

  Packs: before the import, the audio pack (godot\scripts\audio\pack, from tools\pack_audio_web.py) and the planet packs
  (planets_desktop.pck, planets_web.pck, from scenes\galaxy\tools\pack_planets.gd) are built from LocalAssets when missing or stale.
  The exe embeds the audio pack and carries build\packs\planets_desktop.pck beside it; the web export gets packs\planets_web.pck
  and one audio_<empire>_web.pck per empire (the main web pck keeps only the shared audio). test_cue_trace must run with the pack
  present: a 'pack: none' or SKIP line fails the run.

  Environment: GODOT overrides the Godot console exe (default C:\Tools\Godot\Godot_v4.5-stable_win64_console.exe).
#>
[CmdletBinding()]
param(
    [ValidateSet('both', 'windows', 'web')][string]$Target = 'both',
    [switch]$SkipExport,
    [switch]$SkipTests,
    [switch]$SkipCapture,
    [switch]$Sequential,   # the default now; kept so old command lines still work
    [switch]$Parallel,     # opt in to the old all-at-once phases (several GPU windows open together)
    [int]$SuiteTimeoutSec = 900,
    [int]$CaptureTimeoutSec = 900
)

$ErrorActionPreference = 'Stop'
$Sequential = -not $Parallel
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

# ---- Pack build (before the import, so the audio .ogg files are imported and exported) -----------------------------------
# LocalAssets is private and never in git: find it here, in the main checkout of this repo, or in $env:BOTF_LOCALASSETS.
$localAssets = $null
foreach ($c in @($env:BOTF_LOCALASSETS, (Join-Path $remaster 'game\LocalAssets'))) {
    if ($c -and (Test-Path -LiteralPath (Join-Path $c 'audio\cues.json'))) { $localAssets = $c; break }
}
if (-not $localAssets) {
    $common = (git -C $remaster rev-parse --git-common-dir 2>$null)
    if ($common) {
        $mainRoot = Split-Path -Parent (Resolve-Path -LiteralPath $common).Path
        $c = Join-Path $mainRoot 'remaster\game\LocalAssets'
        if (Test-Path -LiteralPath (Join-Path $c 'audio\cues.json')) { $localAssets = $c }
    }
}
$userPacks = Join-Path $env:APPDATA 'Godot\app_userdata\BOTF Remastered (Godot)\packs'
New-Item -ItemType Directory -Force -Path $userPacks | Out-Null
$audioPack = Join-Path $proj 'scripts\audio\pack'
$audioManifest = Join-Path $audioPack 'pack_manifest.json'
if ($localAssets) {
    $srcAudio = @('audio\cues.json', 'audio\trek\cues.json') | ForEach-Object { Join-Path $localAssets $_ } | Where-Object { Test-Path -LiteralPath $_ }
    $newest = (@($srcAudio) + (Join-Path $remaster 'tools\pack_audio_web.py') | ForEach-Object { (Get-Item -LiteralPath $_).LastWriteTime } | Measure-Object -Maximum).Maximum
    if (-not (Test-Path -LiteralPath $audioManifest) -or (Get-Item -LiteralPath $audioManifest).LastWriteTime -lt $newest) {
        $ap = Start-Process -FilePath $python -ArgumentList "`"$(Join-Path $remaster 'tools\pack_audio_web.py')`" --audio `"$(Join-Path $localAssets 'audio')`" --out `"$audioPack`" --jobs 2" `
            -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $logs 'pack-audio.log') -RedirectStandardError (Join-Path $logs 'pack-audio.err')
        $null = $ap.Handle
        try { $ap.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal } catch {}
        if (-not $ap.WaitForExit(1800 * 1000)) { try { Stop-Process -Id $ap.Id -Force } catch {} }
    }
}
$audioOk = Test-Path -LiteralPath $audioManifest
$audioDetail = 'no LocalAssets found: audio pack missing'
$am = $null
if ($audioOk) {
    $am = Get-Content -LiteralPath $audioManifest -Raw | ConvertFrom-Json
    $shared = [Math]::Round($am.packs.shared.bytes / 1MB, 1)
    $audioDetail = "$(@($am.packs.PSObject.Properties).Count) packs, shared $shared MB, $(@($am.violations).Count) budget notes"
}
Add-Result 'audio-pack' $(if ($audioOk) { 'ok' } else { 'FAIL' }) $audioDetail

function Build-PlanetPack([string]$Mode) {
    $out = Join-Path $userPacks "planets_$Mode.pck"
    $tool = Join-Path $proj 'scenes\galaxy\tools\pack_planets.gd'
    $src = if ($localAssets) { Join-Path $localAssets 'planets' } else { $null }
    $stale = -not (Test-Path -LiteralPath $out) -or (Get-Item -LiteralPath $out).LastWriteTime -lt (Get-Item -LiteralPath $tool).LastWriteTime
    if ($stale -and $src -and (Test-Path -LiteralPath $src)) {
        $j = Start-Godot "pack-planets-$Mode" "--headless --audio-driver Dummy --path `"$proj`" -s res://scenes/galaxy/tools/pack_planets.gd -- `"$src`" `"$out`" $Mode"
        Wait-Jobs @($j) 600
    }
    return (Test-Path -LiteralPath $out)
}
$planetsOk = (Build-PlanetPack 'web') -and (Build-PlanetPack 'desktop')
$planetDetail = 'planet packs missing (no LocalAssets\planets)'
if ($planetsOk) {
    $planetDetail = 'planets_desktop.pck {0:N0} MB, planets_web.pck {1:N1} MB' -f ((Get-Item (Join-Path $userPacks 'planets_desktop.pck')).Length / 1MB), ((Get-Item (Join-Path $userPacks 'planets_web.pck')).Length / 1MB)
}
Add-Result 'planet-packs' $(if ($planetsOk) { 'ok' } else { 'FAIL' }) $planetDetail

# Hero ships (M1 webparity): the packaged game reads them from packs, never from LocalAssets. ships_desktop.pck (every glb as
# the dev run reads it) goes to the user packs folder (editor runs, pc_smoke) and beside the exe; the web gets one reduced pack
# per faction plus planets_hi_web.pck (the desktop planet tier as 2048 WebP), staged here and copied to build\web\packs after
# the export. Each tool rebuilds only what is stale.
$webStage = Join-Path $proj 'build\web-pack-stage'
New-Item -ItemType Directory -Force -Path $webStage | Out-Null
$shipsDesk = Join-Path $userPacks 'ships_desktop.pck'
$shipsOk = $false
$shipDetail = 'no LocalAssets\ships: hero ship packs missing'
if ($localAssets -and (Test-Path -LiteralPath (Join-Path $localAssets 'ships\godot-hero-manifest.json'))) {
    $sp = Start-Process -FilePath $python -ArgumentList "`"$(Join-Path $remaster 'tools\build_ship_packs.py')`" --ships `"$(Join-Path $localAssets 'ships')`" --portraits `"$(Join-Path $localAssets 'portraits')`" --desktop `"$shipsDesk`" --web `"$webStage`" --godot `"$godot`" --proj `"$proj`"" `
        -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $logs 'pack-ships.log') -RedirectStandardError (Join-Path $logs 'pack-ships.err')
    $null = $sp.Handle
    try { $sp.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal } catch {}
    if (-not $sp.WaitForExit(3600 * 1000)) { try { Stop-Process -Id $sp.Id -Force } catch {} }
    $hp = Start-Process -FilePath $python -ArgumentList "`"$(Join-Path $remaster 'tools\build_planet_hi_web.py')`" --planets `"$(Join-Path $localAssets 'planets')`" --out `"$(Join-Path $webStage 'planets_hi_web.pck')`" --godot `"$godot`" --proj `"$proj`"" `
        -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $logs 'pack-planets-hi.log') -RedirectStandardError (Join-Path $logs 'pack-planets-hi.err')
    $null = $hp.Handle
    try { $hp.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal } catch {}
    if (-not $hp.WaitForExit(3600 * 1000)) { try { Stop-Process -Id $hp.Id -Force } catch {} }
    $webShips = @(Get-ChildItem -LiteralPath $webStage -Filter 'ships_*_web.pck' -ErrorAction SilentlyContinue)
    $shipsOk = (Test-Path -LiteralPath $shipsDesk) -and $webShips.Count -ge 5 -and (Test-Path -LiteralPath (Join-Path $webStage 'planets_hi_web.pck'))
    $shipDetail = 'ships_desktop.pck {0:N0} MB; web: {1} faction packs {2:N1} MB, planets_hi_web.pck {3:N1} MB' -f ((Get-Item -LiteralPath $shipsDesk -ErrorAction SilentlyContinue).Length / 1MB), $webShips.Count, (($webShips | Measure-Object Length -Sum).Sum / 1MB), ((Get-Item -LiteralPath (Join-Path $webStage 'planets_hi_web.pck') -ErrorAction SilentlyContinue).Length / 1MB)
}
Add-Result 'ship-packs' $(if ($shipsOk) { 'ok' } else { 'FAIL' }) $shipDetail

# Baked ship pictures (M1 trektext): LocalAssets\ship_icons packed into ship_icons_desktop.pck, so the packaged game draws the real
# pictures and not the vector fallback. Packs what is on disk; re-baking after a model change is
# `python remaster\tools\build_ship_icons.py --bake` (bake and pack in one go).
$iconsPck = Join-Path $userPacks 'ship_icons_desktop.pck'
$iconsOk = $false
$iconsDetail = 'no LocalAssets\ship_icons: icon pack missing (run build_ship_icons.py --bake)'
if ($localAssets -and (Test-Path -LiteralPath (Join-Path $localAssets 'ship_icons'))) {
    $ip = Start-Process -FilePath $python -ArgumentList "`"$(Join-Path $remaster 'tools\build_ship_icons.py')`" --localassets `"$localAssets`" --out `"$iconsPck`" --godot `"$godot`" --proj `"$proj`"" `
        -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $logs 'pack-ship-icons.log') -RedirectStandardError (Join-Path $logs 'pack-ship-icons.err')
    $null = $ip.Handle
    try { $ip.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal } catch {}
    if (-not $ip.WaitForExit(900 * 1000)) { try { Stop-Process -Id $ip.Id -Force } catch {} }
    $iconsOk = $ip.ExitCode -eq 0 -and (Test-Path -LiteralPath $iconsPck)
    $iconsDetail = (Get-Content -LiteralPath (Join-Path $logs 'pack-ship-icons.log') -ErrorAction SilentlyContinue | Select-Object -Last 1)
}
Add-Result 'ship-icon-pack' $(if ($iconsOk) { 'ok' } else { 'FAIL' }) $iconsDetail

# Large packs go beside the exports as hard links where the volume allows (no second copy of 1 GB), else copies.
function Link-Pack([string]$Src, [string]$DestDir) {
    $dest = Join-Path $DestDir (Split-Path -Leaf $Src)
    if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Force }
    try { New-Item -ItemType HardLink -Path $dest -Target $Src -ErrorAction Stop | Out-Null } catch { Copy-Item -LiteralPath $Src -Destination $dest -Force }
}

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
            -replace 'include_filter="fixtures/\*\.json"', 'include_filter="fixtures/*.json, data/*.pack.json, scripts/audio/pack/*.json, scripts/audio/pack/*/*.json"'
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
            @('trek-icons', 'res://scenes/ui/tests/test_trek_icons.gd'),
            @('trek-text', 'res://scenes/ui/tests/test_trek_text.gd'),
            @('battle-timeline', 'res://scenes/battle/tests/test_battle_timeline.gd'),
            @('camera-rig', 'res://scenes/battle/tests/test_camera_rig.gd'),
            @('hero-models', 'res://scenes/battle/tests/test_hero_models.gd'),
            @('title-fleet', 'res://scenes/battle/tests/test_title_fleet.gd'),
            @('galaxy-map', 'res://scenes/galaxy/tests/test_galaxy_map.gd'),
            @('audio', 'res://scripts/audio/tests/test_audio.gd'),
            @('cue-trace', 'res://scripts/audio/tests/test_cue_trace.gd'),
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
        if ($j.Name -eq 'suite-cue-trace') {
            $ct = (Get-Content -LiteralPath (Join-Path $logs 'suite-cue-trace.log') -ErrorAction SilentlyContinue) -join "`n"
            if ($ct -match 'pack: none|SKIP') { $ok = $false; $detail = 'file checks SKIPPED (audio pack not loaded): ' + $detail }
            else { $pl = ($ct -split "`n" | Where-Object { $_ -match 'pack:' } | Select-Object -First 1); $detail = "$($pl.Trim()); $detail" }
        }
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

# Asset bundles beside the exports. Desktop: build\packs\planets_desktop.pck beside the exe (AppLoader mounts <exe dir>/packs/*_desktop.pck);
# the audio pack is embedded in the exe. Web: build\web\packs\ gets planets_web.pck and audio_<empire>_web.pck for each empire; the
# main web pck carries only the shared audio (export preset excludes the empire folders). Never a desktop pack on the web side.
if (-not $SkipExport) {
    if ($wantWin) {
        $winPacks = Join-Path $proj 'build\packs'
        New-Item -ItemType Directory -Force -Path $winPacks | Out-Null
        Get-ChildItem -LiteralPath $winPacks -Filter '*.pck' -ErrorAction SilentlyContinue | Remove-Item -Force
        foreach ($n in 'planets_desktop.pck', 'ships_desktop.pck', 'ship_icons_desktop.pck') {
            $dp = Join-Path $userPacks $n
            if (Test-Path -LiteralPath $dp) { Link-Pack $dp $winPacks }
        }
        $exeFile = Join-Path $proj 'build\BOTFRemastered.exe'
        $exeMb = if (Test-Path $exeFile) { [Math]::Round((Get-Item $exeFile).Length / 1MB) } else { 0 }
        $winOk = (Test-Path (Join-Path $winPacks 'planets_desktop.pck')) -and (Test-Path (Join-Path $winPacks 'ships_desktop.pck')) -and $audioOk
        Add-Result 'windows-packs' $(if ($winOk) { 'ok' } else { 'FAIL' }) "build\packs\planets_desktop.pck and ships_desktop.pck beside the exe; audio embedded (exe $exeMb MB)"

        # The packaged exe in a folder with no LocalAssets and no BOTF_* variables (M1 webparity: the beta fell back to
        # procedural hulls because the models were only on the dev disk). Every ship whose class has a hero model must be
        # drawn with it: screenshot.gd SHOT_HERO_CHECK prints HERO_CHECK lines and exits 1 on a fallback hull.
        $clean = Join-Path $env:TEMP 'botf-clean-exe'
        if (Test-Path -LiteralPath $clean) { Remove-Item -LiteralPath $clean -Recurse -Force }
        New-Item -ItemType Directory -Force -Path (Join-Path $clean 'packs') | Out-Null
        foreach ($f in 'BOTFRemastered.exe', 'libbotf_core.windows.template_release.x86_64.dll') { Link-Pack (Join-Path $proj "build\$f") $clean }
        Get-ChildItem -LiteralPath $winPacks -Filter '*.pck' | ForEach-Object { Link-Pack $_.FullName (Join-Path $clean 'packs') }
        $exeShots = Join-Path $run 'exe-clean'
        New-Item -ItemType Directory -Force -Path $exeShots | Out-Null
        $saved = @{}
        foreach ($v in 'BOTF_HERO_MANIFEST', 'BOTF_RESOLVED_MANIFEST', 'BOTF_SHIP_ICONS') { $saved[$v] = [Environment]::GetEnvironmentVariable($v); [Environment]::SetEnvironmentVariable($v, $null) }
        $env:SHOT_HERO_CHECK = '1'; $env:SHOT_FULL = '1'
        # The exe shares user:// with the editor runs (user\packs\*_desktop.pck from this script): a private APPDATA keeps the run from mounting those.
        $savedAppData = $env:APPDATA; $env:APPDATA = Join-Path $clean 'appdata'; New-Item -ItemType Directory -Force -Path $env:APPDATA | Out-Null
        $hc = Start-Process -FilePath (Join-Path $clean 'BOTFRemastered.exe') -WorkingDirectory $clean -PassThru `
            -ArgumentList "--audio-driver Dummy --resolution 1920x1080 -s res://scenes/ui/tools/screenshot.gd -- `"$exeShots`" battle-closeup battle-overview" `
            -RedirectStandardOutput (Join-Path $logs 'exe-hero-hulls.log') -RedirectStandardError (Join-Path $logs 'exe-hero-hulls.err')
        $null = $hc.Handle
        try { $hc.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal } catch {}
        if (-not $hc.WaitForExit(600 * 1000)) { try { Stop-Process -Id $hc.Id -Force } catch {} }
        Remove-Item Env:\SHOT_HERO_CHECK, Env:\SHOT_FULL -ErrorAction SilentlyContinue; $env:APPDATA = $savedAppData
        foreach ($v in $saved.Keys) { [Environment]::SetEnvironmentVariable($v, $saved[$v]) }
        $hl = @(Get-Content -LiteralPath (Join-Path $logs 'exe-hero-hulls.log') -ErrorAction SilentlyContinue | Where-Object { $_ -match '^HERO_CHECK' })
        $heroOk = $hc.ExitCode -eq 0 -and $hl.Count -ge 2 -and -not ($hl -match 'FAIL')
        Add-Result 'exe-hero-hulls' $(if ($heroOk) { 'ok' } else { 'FAIL' }) ((@($hl | ForEach-Object { $_ -replace '^HERO_CHECK\s+', '' -replace 'manifest \S+, ', '' }) -join ' | ') + " (clean folder $clean)")
        # The baked ship pictures, in the same clean folder (no LocalAssets, no BOTF_* variables): icon_check.gd exits 1 unless
        # ship_icons_desktop.pck mounted and every icon in it loads through ShipIcons. The vector line drawing counts as a failure.
        $icLog = Join-Path $logs 'exe-ship-icons.log'
        $env:APPDATA = Join-Path $clean 'appdata'
        foreach ($v in 'BOTF_HERO_MANIFEST', 'BOTF_RESOLVED_MANIFEST', 'BOTF_SHIP_ICONS') { [Environment]::SetEnvironmentVariable($v, $null) }
        $ic = Start-Process -FilePath (Join-Path $clean 'BOTFRemastered.exe') -WorkingDirectory $clean -PassThru `
            -ArgumentList "--audio-driver Dummy --resolution 640x360 -s res://scenes/ui/tools/icon_check.gd" `
            -RedirectStandardOutput $icLog -RedirectStandardError (Join-Path $logs 'exe-ship-icons.err')
        $null = $ic.Handle
        try { $ic.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal } catch {}
        if (-not $ic.WaitForExit(300 * 1000)) { try { Stop-Process -Id $ic.Id -Force } catch {} }
        $env:APPDATA = $savedAppData
        foreach ($v in $saved.Keys) { [Environment]::SetEnvironmentVariable($v, $saved[$v]) }
        $il = @(Get-Content -LiteralPath $icLog -ErrorAction SilentlyContinue | Where-Object { $_ -match '^ICON_CHECK' })
        Add-Result 'exe-ship-icons' $(if ($ic.ExitCode -eq 0 -and $il.Count -ge 1 -and -not ($il -match '^ICON_CHECK\s+FAIL')) { 'ok' } else { 'FAIL' }) ((@($il | ForEach-Object { $_ -replace '^ICON_CHECK\s+', '' }) -join ' | ') + " (clean folder $clean)")
    }
    if ($wantWeb) {
        $webPacks = Join-Path $proj 'build\web\packs'
        New-Item -ItemType Directory -Force -Path $webPacks | Out-Null
        Get-ChildItem -LiteralPath $webPacks -Filter '*.pck' -ErrorAction SilentlyContinue | Remove-Item -Force
        Get-ChildItem -LiteralPath $webPacks -ErrorAction SilentlyContinue | Where-Object { $_.Name -like '*.pck.gz' -or $_.Name -eq 'manifest.json' } | Remove-Item -Force
        $wp = Join-Path $userPacks 'planets_web.pck'
        if (Test-Path -LiteralPath $wp) { Copy-Item -LiteralPath $wp -Destination $webPacks -Force }
        Get-ChildItem -LiteralPath $webStage -Filter '*_web.pck' -ErrorAction SilentlyContinue | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $webPacks -Force }
        if ($audioOk) {
            $ej = Start-Godot 'pack-audio-empires' "--headless --audio-driver Dummy --path `"$proj`" -s res://tools/pack_audio_empire.gd -- `"$audioPack`" `"$webPacks`""
            Wait-Jobs @($ej) 600
        }
        # Content-hashed names, packs\manifest.json, gzip sidecars and the download sizes (hash_web_packs.py). Budgets (coordinator,
        # 9 Nov): the first playable load (engine, main pck, page) under 60 MB as sent, everything under 400 MB.
        $ho = & $python (Join-Path $PSScriptRoot 'hash_web_packs.py') (Join-Path $proj 'build\web') 2>&1 | Out-String
        $hx = $LASTEXITCODE
        $ho | Set-Content -LiteralPath (Join-Path $logs 'web-sizes.log')
        $pk = @(Get-ChildItem -LiteralPath $webPacks -Filter '*.pck' -ErrorAction SilentlyContinue)
        $kinds = @('planets_web', 'planets_hi_web', 'ships_fed_web', 'audio_fed_web') | Where-Object { $k = $_; -not ($pk | Where-Object { $_.Name -like "$k.*.pck" }) }
        $sz = ($ho -split "`n" | Where-Object { $_ -match '^(first playable|total with)' } | ForEach-Object { $_.Trim() }) -join '; '
        Add-Result 'web-packs' $(if ($hx -eq 0 -and -not $kinds) { 'ok' } else { 'FAIL' }) ("$($pk.Count) hashed packs; $sz" + $(if ($kinds) { "; missing: $($kinds -join ', ')" } else { '' }))
    }
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
