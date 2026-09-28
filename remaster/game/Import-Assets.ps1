<#
.SYNOPSIS
    Resolves the private, local-only third-party assets listed in ..\assets\manifest.json and imports them.

.DESCRIPTION
    Policy: third-party ship models, show audio, mod sounds and original BOTF audio are PRIVATE and LOCAL ONLY.
    This script never writes into tracked folders. Everything it produces goes to LocalAssets\ (gitignored) and,
    with -UnrealImport, to Content\Local\ (/Game/Local, gitignored).

    1. Download archives by hand (Sketchfab, Trekmeshes, BlendSwap, ModDB and Nexus need a login or a click-through)
       into one folder and pass it as -DownloadsPath. Loose files (TrekCore .mp3s) can sit in the same folder.
    2. The script extracts .zip (built in) and .7z/.rar/.exe (7-Zip) archives into LocalAssets\_extracted\.
    3. It matches each manifest key's ranked candidates by, in order: LocalAssets\downloads-map.json
       ({"ship.fed.galaxy": "star-trek-tng-galaxy-class-model.zip"}), a recorded sha256, the Sketchfab model id or the
       slugged model title in the archive name, and the expectedFiles patterns.
    4. Models are converted with Blender (Tools\blender_normalize.py): glTF/OBJ/FBX/3DS/blend, and NIF via the
       NifTools add-on, to FBX with +X forward, +Z up, canon length and SOCKET_ placeholders.
       Audio is converted with ffmpeg to 48 kHz 16-bit PCM WAV, mono or stereo as the manifest says, loudness-normalised.
    5. It writes LocalAssets\resolved-manifest.json (status, chosen candidate, hashes, fallback chain, import files)
       and LocalAssets\ASSET-CREDITS.local.md.
    6. -UnrealImport runs UnrealEditor-Cmd with Tools\ue_import_local_assets.py to import into /Game/Local/...

    Original BOTF audio: run with -BotfInstall "C:\Games\BOTF" to call Tools\Extract-BotfAudio.py first (needs Python 3),
    then map extracted files to voice keys in LocalAssets\botf-original\cue-map.json (see docs\AUDIO-PLAN.md).

    Required tools (only for the steps you use):
      Blender 4.x        https://www.blender.org  (-BlenderPath, or on PATH, or the default install folder)
      NifTools add-on    https://github.com/niftools/blender_niftools_addon  (only for Bridge Commander .nif)
      ffmpeg             winget install Gyan.FFmpeg  (-FfmpegPath or on PATH)
      7-Zip              winget install 7zip.7zip    (only for .7z/.rar/.exe archives)
      Python 3           only for -BotfInstall
      Unreal Engine 5.8  only for -UnrealImport (-EngineRoot or UE_ROOT, see BotfTools.ps1)

.EXAMPLE
    .\Import-Assets.ps1 -DownloadsPath "$env:USERPROFILE\Downloads\botf-assets"
.EXAMPLE
    .\Import-Assets.ps1 -DownloadsPath D:\trek -Keys ship.fed.*,ship.kli.* -UnrealImport
.EXAMPLE
    .\Import-Assets.ps1 -ListSources -Keys ship.rom.*
#>
[CmdletBinding()]
param(
    [string]$DownloadsPath = '',
    [string[]]$Keys = @('*'),
    [string]$BlenderPath = '',
    [string]$FfmpegPath = '',
    [string]$SevenZipPath = '',
    [string]$PythonPath = '',
    [string]$BotfInstall = '',
    [switch]$SkipConvert,
    [switch]$UnrealImport,
    [string]$EngineRoot = '',
    [switch]$ListSources,
    [switch]$AllowRestricted
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BotfTools.ps1')

$ManifestPath = Join-Path $PSScriptRoot '..\assets\manifest.json'
$LocalRoot = Join-Path $PSScriptRoot 'LocalAssets'
$ExtractRoot = Join-Path $LocalRoot '_extracted'
$ToolsDir = Join-Path $PSScriptRoot 'Tools'

function Read-JsonFile([string]$Path) {
    return (Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json)
}

function Get-Props($Object) {
    if ($null -eq $Object) { return @() }
    return @($Object.PSObject.Properties)
}

function Get-Opt($Object, [string]$Name) {
    if ($null -eq $Object) { return $null }
    $p = $Object.PSObject.Properties[$Name]
    if ($p) { return $p.Value }
    return $null
}

function Get-Slug([string]$Text) {
    if (-not $Text) { return '' }
    $t = $Text.ToLowerInvariant() -replace '^(sketchfab|trekmeshes|blendswap|trekcore):\s*', ''
    $t = $t -replace "['’]", '' -replace '[^a-z0-9]+', '-'
    return $t.Trim('-')
}

function Find-Tool([string]$Explicit, [string[]]$Names, [string[]]$Defaults) {
    if ($Explicit) {
        if (Test-Path -LiteralPath $Explicit) { return (Resolve-Path -LiteralPath $Explicit).ProviderPath }
        throw "Tool not found at '$Explicit'."
    }
    foreach ($n in $Names) {
        $c = Get-Command $n -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($c) { return $c.Source }
    }
    foreach ($d in $Defaults) {
        $hits = @(Get-Item -Path $d -ErrorAction SilentlyContinue | Sort-Object FullName -Descending)
        if ($hits.Count -gt 0) { return $hits[0].FullName }
    }
    return ''
}

function Test-KeySelected([string]$Key) {
    foreach ($p in $Keys) { if ($Key -like $p) { return $true } }
    return $false
}

$manifest = Read-JsonFile $ManifestPath

# ---------------------------------------------------------------- list mode
if ($ListSources) {
    foreach ($a in Get-Props $manifest.assets) {
        if (-not (Test-KeySelected $a.Name)) { continue }
        Write-Host ("{0}  [{1}] {2}  (fallback: {3})" -f $a.Name, $a.Value.kind, $a.Value.displayName, $a.Value.fallback) -ForegroundColor Cyan
        foreach ($c in @($a.Value.candidates)) {
            Write-Host ("    {0,-15} {1}" -f $c.permission, $c.sourceName)
            Write-Host ("                    {0}  by {1}" -f $c.page, $c.author) -ForegroundColor DarkGray
        }
    }
    return
}

New-Item -ItemType Directory -Force -Path $LocalRoot, $ExtractRoot | Out-Null
$gitignoreProbe = Join-Path $LocalRoot 'README-LOCAL-ONLY.txt'
if (-not (Test-Path -LiteralPath $gitignoreProbe)) {
    Set-Content -LiteralPath $gitignoreProbe -Encoding UTF8 -Value 'Private third-party and original-game assets. Gitignored. Never commit, upload or package this folder.'
}

# ---------------------------------------------------------------- original BOTF audio
if ($BotfInstall) {
    $py = Find-Tool $PythonPath @('py', 'python', 'python3') @()
    if (-not $py) { throw 'Python 3 not found (needed for -BotfInstall).' }
    $ff = Find-Tool $FfmpegPath @('ffmpeg') @()
    $argList = @((Join-Path $ToolsDir 'Extract-BotfAudio.py'), '--botf', $BotfInstall, '--out', (Join-Path $LocalRoot 'botf-original\audio'))
    if ($ff) { $argList += @('--convert', '--ffmpeg', $ff) }
    & $py @argList
    if ($LASTEXITCODE -ne 0) { throw 'Extract-BotfAudio.py failed.' }
}

# ---------------------------------------------------------------- extract downloads
$archives = @()
$looseFiles = @()
if ($DownloadsPath) {
    if (-not (Test-Path -LiteralPath $DownloadsPath)) { throw "Downloads folder '$DownloadsPath' not found." }
    $sevenZip = Find-Tool $SevenZipPath @('7z') @("$env:ProgramFiles\7-Zip\7z.exe")
    foreach ($f in Get-ChildItem -LiteralPath $DownloadsPath -Recurse -File) {
        $ext = $f.Extension.ToLowerInvariant()
        if (@('.zip', '.7z', '.rar', '.exe') -contains $ext) {
            $dest = Join-Path $ExtractRoot ([IO.Path]::GetFileNameWithoutExtension($f.Name))
            if (-not (Test-Path -LiteralPath $dest)) {
                Write-Host "Extracting $($f.Name)"
                if ($ext -eq '.zip') {
                    Expand-Archive -LiteralPath $f.FullName -DestinationPath $dest -Force
                } elseif ($sevenZip) {
                    & $sevenZip x -y "-o$dest" $f.FullName | Out-Null
                    if ($LASTEXITCODE -ne 0) { Write-Warning "7-Zip failed on $($f.Name)"; continue }
                } else {
                    Write-Warning "Skipping $($f.Name): install 7-Zip or pass -SevenZipPath for $ext archives."
                    continue
                }
            }
            $archives += [pscustomobject]@{ Name = $f.Name; Slug = (Get-Slug $f.BaseName); Path = $f.FullName; Dir = $dest; Sha = $null }
        } else {
            $looseFiles += $f
        }
    }
}
# Previously extracted archives stay usable without re-passing -DownloadsPath.
foreach ($d in Get-ChildItem -LiteralPath $ExtractRoot -Directory -ErrorAction SilentlyContinue) {
    if (-not ($archives | Where-Object { $_.Dir -eq $d.FullName })) {
        $archives += [pscustomobject]@{ Name = $d.Name; Slug = (Get-Slug $d.Name); Path = $null; Dir = $d.FullName; Sha = $null }
    }
}

$downloadsMap = @{}
$mapPath = Join-Path $LocalRoot 'downloads-map.json'
if (Test-Path -LiteralPath $mapPath) { foreach ($p in Get-Props (Read-JsonFile $mapPath)) { $downloadsMap[$p.Name] = [string]$p.Value } }
$cueMap = @{}
$cuePath = Join-Path $LocalRoot 'botf-original\cue-map.json'
if (Test-Path -LiteralPath $cuePath) { foreach ($p in Get-Props (Read-JsonFile $cuePath)) { $cueMap[$p.Name] = @($p.Value) } }
$overrides = @{}
$ovPath = Join-Path $LocalRoot 'overrides.json'
if (Test-Path -LiteralPath $ovPath) { foreach ($p in Get-Props (Read-JsonFile $ovPath)) { $overrides[$p.Name] = $p.Value } }

$hashCache = @{}
function Get-Sha([string]$Path) {
    if (-not $hashCache.ContainsKey($Path)) { $hashCache[$Path] = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
    return $hashCache[$Path]
}

function Find-InDir([string]$Dir, $Patterns) {
    $hits = @()
    $all = @(Get-ChildItem -LiteralPath $Dir -Recurse -File -ErrorAction SilentlyContinue)
    foreach ($pat in @($Patterns)) {
        $leaf = ($pat -split '[\\/]')[-1]
        $hits += @($all | Where-Object { $_.Name -like $leaf -and (Get-BotfRelativePath $Dir $_.FullName).Replace('\', '/') -like ('*' + $pat) })
    }
    return @($hits | Sort-Object FullName -Unique)
}

$modelPreference = @('.glb', '.gltf', '.fbx', '.obj', '.blend', '.3ds', '.nif')
function Select-Primary($Files, [string]$Kind) {
    if ($Kind -eq 'model') {
        foreach ($e in $modelPreference) {
            $m = @($Files | Where-Object { $_.Extension.ToLowerInvariant() -eq $e } | Sort-Object Length -Descending)
            if ($m.Count -gt 0) { return $m[0] }
        }
        return $null
    }
    return @($Files | Where-Object { @('.wav', '.mp3', '.ogg', '.flac') -contains $_.Extension.ToLowerInvariant() })
}

function Resolve-Candidate([string]$Key, $Entry, $Cand) {
    if ($Cand.sourceSite -eq 'in-repo') {
        return @{ status = 'in-repo'; files = @(); archive = $null }
    }
    if ($Cand.sourceSite -eq 'botf-install') {
        if ($cueMap.ContainsKey($Key)) {
            $base = Join-Path $LocalRoot 'botf-original\audio'
            $files = @($cueMap[$Key] | ForEach-Object { Get-Item -LiteralPath (Join-Path $base $_) -ErrorAction SilentlyContinue } | Where-Object { $_ })
            if ($files.Count -gt 0) { return @{ status = 'found'; files = $files; archive = $null } }
        }
        return $null
    }
    # 1. explicit map
    $pool = @()
    if ($downloadsMap.ContainsKey($Key)) {
        $pool = @($archives | Where-Object { $_.Name -eq $downloadsMap[$Key] -or (Split-Path $_.Dir -Leaf) -eq [IO.Path]::GetFileNameWithoutExtension($downloadsMap[$Key]) })
    }
    # 2. hash
    if ($pool.Count -eq 0 -and $Cand.archiveSha256) {
        $pool = @($archives | Where-Object { $_.Path -and (Get-Sha $_.Path) -eq $Cand.archiveSha256.ToLowerInvariant() })
    }
    # 3. Sketchfab id / slug
    if ($pool.Count -eq 0) {
        $slug = Get-Slug $Cand.sourceName
        $pool = @($archives | Where-Object {
                ($Cand.sourceId -and $_.Name -like ('*' + $Cand.sourceId + '*')) -or ($slug -and $_.Slug -eq $slug) -or ($slug.Length -ge 12 -and $_.Slug -like ($slug + '*'))
            })
    }
    foreach ($a in $pool) {
        $files = Find-InDir $a.Dir $Cand.expectedFiles
        if ($Cand.sha256) { $files = @($files | Where-Object { (Get-Sha $_.FullName) -eq $Cand.sha256.ToLowerInvariant() }) }
        if ($files.Count -gt 0) { return @{ status = 'found'; files = $files; archive = $a } }
    }
    # 4. loose files (TrekCore mp3s etc.)
    if ($Entry.kind -ne 'model') {
        $names = @($Cand.expectedFiles | ForEach-Object { ($_ -split '[\\/]')[-1] })
        $hits = @($looseFiles | Where-Object { $f = $_; @($names | Where-Object { $f.Name -like $_ }).Count -gt 0 })
        if ($Cand.sha256) { $hits = @($hits | Where-Object { (Get-Sha $_.FullName) -eq $Cand.sha256.ToLowerInvariant() }) }
        if ($hits.Count -gt 0) { return @{ status = 'found'; files = $hits; archive = $null } }
    }
    return $null
}

# ---------------------------------------------------------------- resolve
$resolved = [ordered]@{}
$blenderJobs = @()
$audioJobs = @()
foreach ($a in Get-Props $manifest.assets) {
    $key = $a.Name; $e = $a.Value
    if (-not (Test-KeySelected $key)) { continue }
    $rec = [ordered]@{ kind = $e.kind; displayName = $e.displayName; status = 'missing'; candidate = $null; source = $null
        author = $null; licence = $null; permission = $null; page = $null; files = @(); sha256 = @(); archive = $null
        archiveSha256 = $null; importFiles = @(); ueFolder = (Get-Opt $e.import 'ueFolder'); fallback = $e.fallback
        faction = (Get-Opt $e 'faction'); botfClasses = @(Get-Opt $e 'botfClasses' | Where-Object { $_ })
        targetLengthMeters = (Get-Opt $e.import 'targetLengthMeters'); ueAssets = @() }
    $i = 0
    foreach ($c in @($e.candidates)) {
        $i++
        if ($c.permission -eq 'restricted' -and -not $AllowRestricted) { continue }
        $hit = Resolve-Candidate $key $e $c
        if (-not $hit) { continue }
        $rec.candidate = $i; $rec.source = $c.sourceName; $rec.author = $c.author; $rec.licence = $c.licence
        $rec.permission = $c.permission; $rec.page = $c.page
        if ($hit.status -eq 'in-repo') {
            $rec.status = 'in-repo'
            if ($e.import.PSObject.Properties['ueAsset']) { $rec.ueAsset = $e.import.ueAsset }
            break
        }
        $rec.status = 'resolved'
        $rec.files = @($hit.files | ForEach-Object { $_.FullName })
        $rec.sha256 = @($hit.files | ForEach-Object { Get-Sha $_.FullName })
        if ($hit.archive) { $rec.archive = $hit.archive.Name; if ($hit.archive.Path) { $rec.archiveSha256 = Get-Sha $hit.archive.Path } }
        $imp = $e.import
        $ov = $null; if ($overrides.ContainsKey($key)) { $ov = $overrides[$key] }
        if ($e.kind -eq 'model') {
            $primary = Select-Primary $hit.files 'model'
            if (-not $primary) { $rec.status = 'unsupported-format'; break }
            $outFbx = Join-Path $LocalRoot ('models\' + $key + '\' + ($key -replace '\.', '_') + '.fbx')
            $job = [ordered]@{ key = $key; input = $primary.FullName; output = $outFbx; targetLengthMeters = (Get-Opt $imp 'targetLengthMeters')
                scale = (Get-Opt $imp 'scale'); sourceForwardAxis = (Get-Opt $imp 'sourceForwardAxis'); maxTriangles = (Get-Opt $imp 'maxTriangles'); sockets = @(Get-Opt $imp 'sockets') }
            if ($ov) { foreach ($p in Get-Props $ov) { $job[$p.Name] = $p.Value } }
            $blenderJobs += [pscustomobject]$job
            $rec.importFiles = @($outFbx)
        } else {
            $outDir = Join-Path $LocalRoot ('audio\' + $key)
            $outs = @()
            foreach ($f in @(Select-Primary $hit.files 'audio')) {
                $out = Join-Path $outDir ([IO.Path]::GetFileNameWithoutExtension($f.Name) + '.wav')
                $audioJobs += [pscustomobject]@{ In = $f.FullName; Out = $out; Rate = (Get-Opt $imp 'targetSampleRate'); Channels = (Get-Opt $imp 'channels'); Lufs = (Get-Opt $imp 'normalizeLufs'); Trim = [bool](Get-Opt $imp 'trimSilence') }
                $outs += $out
            }
            $rec.importFiles = $outs
        }
        break
    }
    # Predicted UE object paths (<ueFolder>/<Name>.<Name>) for UBotfAssetRegistry. ue_import_local_assets.py replaces
    # them with the paths Unreal actually created after -UnrealImport.
    if ($rec.ueFolder -and $rec.importFiles.Count -gt 0) {
        $rec.ueAssets = @($rec.importFiles | ForEach-Object {
            $n = ([IO.Path]::GetFileNameWithoutExtension($_)) -replace '[^A-Za-z0-9_]', '_'
            '{0}/{1}.{1}' -f $rec.ueFolder, $n })
    }
    $resolved[$key] = $rec
}

# Fallback chains (resolved or in-repo keys only).
foreach ($key in @($resolved.Keys)) {
    $r = $resolved[$key]
    if ($r.status -eq 'resolved' -or $r.status -eq 'in-repo') { $r.effectiveKey = $key; continue }
    $seen = @($key); $fb = $r.fallback; $eff = $null
    while ($fb -and -not ($seen -contains $fb)) {
        $seen += $fb
        $fe = $manifest.assets.PSObject.Properties[$fb]
        if (-not $fe) { break }
        if ($resolved.Contains($fb) -and ($resolved[$fb].status -eq 'resolved' -or $resolved[$fb].status -eq 'in-repo')) { $eff = $fb; break }
        if (@($fe.Value.candidates | Where-Object { $_.sourceSite -eq 'in-repo' }).Count -gt 0) { $eff = $fb; break }
        $fb = $fe.Value.fallback
    }
    $r.effectiveKey = $eff
}

# ---------------------------------------------------------------- convert
if (-not $SkipConvert) {
    if ($blenderJobs.Count -gt 0) {
        $blender = Find-Tool $BlenderPath @('blender') @("$env:ProgramFiles\Blender Foundation\Blender*\blender.exe")
        if (-not $blender) {
            Write-Warning 'Blender not found; models were matched but not converted. Pass -BlenderPath.'
        } else {
            $jobsPath = Join-Path $LocalRoot 'blender-jobs.json'
            ConvertTo-Json -InputObject @($blenderJobs) -Depth 6 | Set-Content -LiteralPath $jobsPath -Encoding UTF8
            $bargs = @('--background')
            if (-not @($blenderJobs | Where-Object { $_.input -like '*.nif' }).Count) { $bargs += '--factory-startup' }
            $bargs += @('--python', (Join-Path $ToolsDir 'blender_normalize.py'), '--', '--jobs', $jobsPath)
            Write-Host "Blender: converting $($blenderJobs.Count) model(s)"
            & $blender @bargs
            foreach ($j in $blenderJobs) {
                if (-not (Test-Path -LiteralPath $j.output)) { $resolved[$j.key].status = 'convert-failed' }
                else { $resolved[$j.key].convertReport = $j.output + '.report.json' }
            }
        }
    }
    if ($audioJobs.Count -gt 0) {
        $ffmpeg = Find-Tool $FfmpegPath @('ffmpeg') @()
        if (-not $ffmpeg) {
            Write-Warning 'ffmpeg not found; audio was matched but not converted. Pass -FfmpegPath.'
        } else {
            foreach ($j in $audioJobs) {
                New-Item -ItemType Directory -Force -Path (Split-Path $j.Out) | Out-Null
                $filters = @()
                if ($j.Trim) { $filters += 'silenceremove=start_periods=1:start_threshold=-55dB' }
                if ($j.Lufs) { $filters += ('loudnorm=I={0}:TP=-1.5:LRA=11' -f $j.Lufs) }
                $fargs = @('-v', 'error', '-y', '-i', $j.In)
                if ($filters.Count) { $fargs += @('-af', ($filters -join ',')) }
                $fargs += @('-ar', [string]$j.Rate, '-ac', [string]$j.Channels, '-acodec', 'pcm_s16le', $j.Out)
                & $ffmpeg @fargs
                if ($LASTEXITCODE -ne 0) { Write-Warning "ffmpeg failed: $($j.In)" }
            }
        }
    }
}

# ---------------------------------------------------------------- write resolved manifest + credits
$out = [ordered]@{ generated = (Get-Date).ToString('s'); manifest = (Resolve-Path -LiteralPath $ManifestPath).ProviderPath
    ueContentRoot = $manifest.ueContentRoot; assets = $resolved }
$resolvedPath = Join-Path $LocalRoot 'resolved-manifest.json'
ConvertTo-Json -InputObject $out -Depth 8 | Set-Content -LiteralPath $resolvedPath -Encoding UTF8

$lines = @('# Local asset credits (this PC only, generated by Import-Assets.ps1)', '',
    'Private, local-only assets. Not redistributed. Star Trek designs and audio belong to their rights holders.', '',
    '| Key | Asset | Author | Source | Stated terms | Permission |', '|---|---|---|---|---|---|')
foreach ($key in $resolved.Keys) {
    $r = $resolved[$key]
    if ($r.status -ne 'resolved') { continue }
    $lines += ('| {0} | {1} | {2} | {3} | {4} | {5} |' -f $key, $r.source, $r.author, $r.page, $r.licence, $r.permission)
}
Set-Content -LiteralPath (Join-Path $LocalRoot 'ASSET-CREDITS.local.md') -Value $lines -Encoding UTF8

$counts = $resolved.Values | Group-Object { $_.status } | ForEach-Object { '{0}={1}' -f $_.Name, $_.Count }
Write-Host ("Resolved manifest: {0}  ({1})" -f $resolvedPath, ($counts -join ', ')) -ForegroundColor Green
foreach ($key in $resolved.Keys) {
    $r = $resolved[$key]
    if ($r.status -eq 'missing') { Write-Host ("  missing {0,-22} -> using {1}" -f $key, $r.effectiveKey) -ForegroundColor DarkGray }
}

# ---------------------------------------------------------------- Unreal import
if ($UnrealImport) {
    $engine = Resolve-BotfEngineRoot $EngineRoot
    $cmd = Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $project = Join-Path $PSScriptRoot 'BOTFRemake.uproject'
    $env:BOTF_RESOLVED_MANIFEST = $resolvedPath
    $script = Join-Path $ToolsDir 'ue_import_local_assets.py'
    Write-Host 'Unreal: importing into /Game/Local'
    & $cmd $project '-run=pythonscript' ('-script="' + $script + '"') '-unattended' '-nop4' '-nosplash' '-stdout' '-FullStdOutLogOutput'
    if ($LASTEXITCODE -ne 0) { throw "UnrealEditor-Cmd exited with $LASTEXITCODE" }
}
