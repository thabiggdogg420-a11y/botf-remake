# Packaging the Unreal client on Windows

`remaster/game/Package-Windows.ps1` does the whole job in one command: it checks prerequisites, runs the core tests,
builds the editor and game targets, and cooks a versioned package with a build manifest. If a step fails, it writes a
short error excerpt that you can paste back into a Claude session.

> The Unreal client has so far been checked only with stand-in compilers (see [UNREAL-CLIENT.md](UNREAL-CLIENT.md)).
> Expect the first real UE 5.8 compile to show a few errors. The script is built for that loop: run, paste
> the summary, apply the fix, run again.

## 1. One-time setup

| Need | How |
|---|---|
| **Unreal Engine 5.8** | Epic Games Launcher > Unreal Engine > Library > Engine Versions > **+** > 5.8 > Install. The default path is `C:\Program Files\Epic Games\UE_5.8`. If you install elsewhere, set `UE_ROOT` (`setx UE_ROOT "D:\Epic\UE_5.8"`) or pass `-EngineRoot`. |
| **Visual Studio 2022** (Community is fine) | Workloads: **Game development with C++** and **Desktop development with C++**. Individual components: **MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)** and **Windows 11 SDK (10.0.22621.0 or newer)**. If the UE 5.8 release notes name a newer Visual Studio or MSVC version, install that one. |
| **.NET** | Nothing to install. UE ships the .NET SDK that UnrealBuildTool runs on. The script checks that it is present. |
| **Git + Git LFS** | Maps and assets (`*.umap`, `*.uasset`) are stored in LFS. Run `git lfs install` once, then `git lfs pull` in the clone. The script stops if it finds LFS pointer files instead of real assets. |
| **Disk** | About 25 GB free for the first editor build, game builds and cook, on top of the engine install. |
| **Short path** | Clone to a short path such as `C:\src\botf`. Cooking can exceed Windows' 260-character path limit otherwise. You can also turn on long paths: `HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled = 1`. |

## 2. Check the machine

Open **Windows PowerShell** (5.1 is fine) or PowerShell 7 and run:

```powershell
cd C:\src\botf\remaster\game
powershell -NoProfile -ExecutionPolicy Bypass -File .\Package-Windows.ps1 -CheckOnly
```

This command builds nothing. It prints the engine version, the Visual Studio, MSVC and Windows SDK versions, the
.NET location, the asset count, the commit and the free disk space, or it names the one thing that is missing.

## 3. Build and package

Close the Unreal Editor first. A running editor locks the module DLL, and Live Coding blocks UnrealBuildTool.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Package-Windows.ps1
```

| Step | What runs | Typical first-run time |
|---|---|---|
| 0 | Prerequisite checks | seconds |
| 1 | `Build-Simulation.ps1 -Test ...`: the core, compiled with MSVC `/W4 /WX`, and every scenario suite | 2-5 min |
| 2 | UBT `BOTFRemakeEditor Win64 Development`, then `BOTFRemake Win64 Development` and `Shipping` | 5-20 min (the first build also compiles shared engine code) |
| 3 | UAT `BuildCookRun` (build, cook `/Game/Maps/Galaxy`, stage, pak, prerequisites, archive) | 5-30 min on the first cook |
| 4 | Credits and `build-manifest.json` | seconds |

The package is written to:

```
remaster\game\Build\Packages\<version>-<commit>-<config>-<yyyyMMdd-HHmmss>\Windows\
    BOTFRemake.exe            launcher
    BOTFRemake\Binaries\Win64\BOTFRemake.exe (or BOTFRemake-Win64-Shipping.exe)
    build-manifest.json       version, commit, engine, toolchain, steps, exe SHA-256, input hashes
    Credits\
```

Start the newest package with `.\Launch-Prototype.ps1`. It checks the executable against the manifest before launching.

### Useful switches

| Switch | Effect |
|---|---|
| `-PackageConfig Shipping` | Package a Shipping build. The default is Development, which keeps logs and the console, and is better for a first run. |
| `-GameConfigurations Development` | Compile only the listed game configurations in step 2. The default is Development and Shipping. |
| `-RunUiTests` | After the editor build, run the `BOTF.UI.*` automation tests headless (`Test-UnrealUi.ps1`). |
| `-SkipCoreTests`, `-SkipEditorBuild`, `-SkipGameBuild`, `-SkipPackage` | Skip that step. For example, `-SkipPackage` compiles only. |
| `-EngineRoot D:\Epic\UE_5.8` | Use an engine that is not at the default path. |
| `-OutputRoot D:\BotfBuilds` | Write packages somewhere else. |
| `-ShowToolOutput` | Echo the full compiler and UAT output. Otherwise only error lines are shown, and everything goes to the logs. |
| `-Force` | Continue past an engine other than 5.8 or low disk space. |

## 4. When it fails

Every step writes `remaster\game\Build\Logs\<stamp>\NN-<step>.log`. When a step fails, the script also writes:

```
remaster\game\Build\Logs\<stamp>\FAILURE-SUMMARY.txt
```

This file is also copied to the clipboard. It contains the failed step, the exit code and the command, the engine and
toolchain versions, up to 60 distinct error lines with the repository path shortened to `<game>`, the last 40 log lines,
and the paths of the UBT and UAT logs. Paste it into the Claude session as it is. If the excerpt is not enough, attach
the step log it names.

`run.json` in the same folder records each step's exit code and duration.

### Common first-run problems

| Symptom | Fix |
|---|---|
| `Unreal Engine not found at ...` | Install 5.8, or set `UE_ROOT` or `-EngineRoot`. |
| `... are Git LFS pointers` | `git lfs install` and `git lfs pull`. |
| UBT: `Unable to build while Live Coding is active` or the DLL is in use | Close the editor and run again. |
| UBT: `... compiler version ... is not supported` | Update Visual Studio, or install the MSVC version the UE 5.8 release notes name. |
| C++ errors in `Source\BOTFRemake\...` | Expected on the first compile. Paste `FAILURE-SUMMARY.txt` back. The spots most likely to fail are listed in [UNREAL-CLIENT.md](UNREAL-CLIENT.md#first-real-compile-risk-list). |
| Cook: `LogLinker: Error` / assets fail to load | The assets were saved by a newer engine than yours, or LFS content is missing. |
| Paths over 260 characters | Clone to a shorter path, or enable long paths. |

## 5. What the Linux CI checks, and what it cannot

`remaster-ci.yml` job **unreal-tu** runs `game/Tools/check-unreal-tu.sh`. It compiles:

- the core as the single translation unit UBT builds (`SimulationBridge.cpp`), with `-Wshadow -Werror` (UE treats
  shadowing as an error);
- the core and the view-model headers again, with engine and `<windows.h>` macro names predefined (`near`, `far`,
  `IN`, `OUT`, `check`, `verify`, `TEXT`, `PI`, `interface`, `small`, ...).

These checks catch a class of errors early. They do not prove that any Slate or Engine API exists or has the
signature we use. Only this script, on a machine with UE 5.8, can prove that.
