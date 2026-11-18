<#
.SYNOPSIS
  Builds the wasm32 GDExtension core for the web export, from any worktree (docs/DATA-FORMAT.md section 11).
  Writes core\bin\libbotf_core.web.template_<target>.wasm32.wasm. No file from another checkout is used.

  powershell -NoProfile -File remaster\godot\core\build-web.ps1 [-Target release|debug|both] [-Clean]

.DESCRIPTION
  Needs (all outside the repo, no admin): Emscripten SDK 3.1.64 (C:\Tools\emsdk, or $env:EMSDK), Python 3, git (only for the
  one-time godot-cpp clone) and cmake + ninja (on PATH, or the copies in VS Build Tools). The script:
    1. activates emsdk and checks emcc is 3.1.64;
    2. builds the wasm-exception libc++abi/libunwind PIC libraries once (embuilder; a no-op when they exist);
    3. finds godot-cpp: -GodotCpp, $env:BOTF_GODOT_CPP, core\godot-cpp, else the shared copy in
       %LOCALAPPDATA%\botf-cache\godot-cpp-godot-4.5-stable, cloned once from the pinned 4.5-stable tag;
    4. configures and builds in core\build-web-<target> (gitignored; the first godot-cpp compile is about 1,000 objects).
  The build is deterministic from the repo plus those pinned inputs; running it in a fresh worktree gives the same module.
#>
param(
    [ValidateSet('release', 'debug', 'both')][string]$Target = 'release',
    [switch]$Clean,
    [string]$GodotCpp = $env:BOTF_GODOT_CPP,
    [int]$Jobs = 8,
    [string]$EmccVersion = '3.1.64'   # WebGPU trial: the fork's template needs 6.0.x
)
$ErrorActionPreference = 'Stop'
$core = $PSScriptRoot
$tag = 'godot-4.5-stable'

# --- tools ---------------------------------------------------------------------------------------------------------------
$emsdk = if ($env:EMSDK) { $env:EMSDK } else { 'C:\Tools\emsdk' }
if (-not (Test-Path -LiteralPath (Join-Path $emsdk 'emsdk_env.bat'))) { throw "Emscripten SDK not found at $emsdk (set EMSDK)" }
# emsdk_env.bat through cmd, then import its environment (emsdk_env.ps1 prints bash-style paths when SHELL is set).
$env:EMSDK_QUIET = '1'
Remove-Item Env:MSYSTEM, Env:SHELL -ErrorAction SilentlyContinue   # emsdk writes bash-style paths when it sees an MSYS shell
& cmd.exe /c "`"$(Join-Path $emsdk 'emsdk_env.bat')`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -LiteralPath "Env:$($Matches[1])" -Value $Matches[2] }
}
$env:EMSDK = $emsdk
$ver = (& emcc --version | Select-Object -First 1)
if ($ver -notmatch [regex]::Escape($EmccVersion)) { throw "emcc must be $EmccVersion (3.1.64 is the proven pairing with the Godot 4.5 dlink template); found: $ver" }
foreach ($t in 'cmake', 'ninja') {
    if (-not (Get-Command $t -ErrorAction SilentlyContinue)) {
        $hit = Get-ChildItem 'C:\Program Files (x86)\Microsoft Visual Studio', 'C:\Program Files\Microsoft Visual Studio' -Recurse -Filter "$t.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $hit) { throw "$t not found on PATH or in VS Build Tools" }
        $env:PATH = "$($hit.DirectoryName);$env:PATH"
    }
}
$py = (Get-Command python -ErrorAction Stop).Source

# --- godot-cpp -----------------------------------------------------------------------------------------------------------
if (-not $GodotCpp) {
    $local = Join-Path $core 'godot-cpp'
    $GodotCpp = if (Test-Path -LiteralPath (Join-Path $local 'CMakeLists.txt')) { $local } else { Join-Path $env:LOCALAPPDATA "botf-cache\godot-cpp-$tag" }
}
if (-not (Test-Path -LiteralPath (Join-Path $GodotCpp 'CMakeLists.txt'))) {
    Write-Host "[web-core] cloning godot-cpp $tag into $GodotCpp (one time)"
    New-Item -ItemType Directory -Force -Path (Split-Path $GodotCpp) | Out-Null
    & git clone --quiet --depth 1 --branch $tag https://github.com/godotengine/godot-cpp $GodotCpp
    if ($LASTEXITCODE -ne 0) { throw 'git clone godot-cpp failed' }
    New-Item -ItemType File -Force -Path (Join-Path $GodotCpp '.gdignore') | Out-Null
}
Write-Host "[web-core] emcc $ver; godot-cpp $GodotCpp"

# --- side-module C++ runtime with wasm exceptions, once ------------------------------------------------------------------
# emsdk 3.x names the wasm-exception runtime '-except'; 4.x and later '-legacyexcept' (legacy wasm EH is still their default).
$eh = if ($EmccVersion -like '3.*') { 'except' } else { 'legacyexcept' }
$pic = Join-Path $emsdk 'upstream\emscripten\cache\sysroot\lib\wasm32-emscripten\pic'
if (-not ((Test-Path (Join-Path $pic "libc++abi-$eh.a")) -and (Test-Path (Join-Path $pic "libunwind-$eh.a")))) {
    Write-Host "[web-core] embuilder libc++abi-$eh libunwind-$eh --pic"
    & embuilder build "libc++abi-$eh" "libunwind-$eh" --pic
    if ($LASTEXITCODE -ne 0) { throw 'embuilder failed' }
}

# --- build -----------------------------------------------------------------------------------------------------------------
$targets = if ($Target -eq 'both') { @('release', 'debug') } else { @($Target) }
foreach ($t in $targets) {
    $dir = Join-Path $core "build-web-$t"
    if ($Clean -and (Test-Path -LiteralPath $dir)) { Remove-Item -LiteralPath $dir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    New-Item -ItemType File -Force -Path (Join-Path $dir '.gdignore') | Out-Null   # keep Godot from importing .o files
    & emcmake cmake -S $core -B $dir -G Ninja -DCMAKE_BUILD_TYPE=Release "-DBOTF_GODOT_TARGET=template_$t" -DGODOTCPP_THREADS=OFF "-DBOTF_GODOT_CPP_DIR=$GodotCpp" "-DPython3_EXECUTABLE=$py" "-DBOTF_EM_EH=$eh"
    if ($LASTEXITCODE -ne 0) { throw "configure failed ($t)" }
    & cmake --build $dir -j $Jobs
    if ($LASTEXITCODE -ne 0) { throw "build failed ($t)" }
    $out = Join-Path $core "bin\libbotf_core.web.template_$t.wasm32.wasm"
    if (-not (Test-Path -LiteralPath $out)) { throw "missing $out" }
    $f = Get-Item -LiteralPath $out
    $hash = (Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash.Substring(0, 16)
    Write-Host ("[web-core] built {0} ({1:N0} bytes, sha256 {2})" -f $f.Name, $f.Length, $hash)
}
