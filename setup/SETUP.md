# Game development environment

Owner: this Codex game-remakes chat, authorised by Simon on 26 September 2026.
Scope: local personal-use development tooling. No remote repository or uploads configured.

## Verified hardware

- NVIDIA GeForce RTX 4090
- 32 GB system RAM
- C: had approximately 333 GiB free before installation.

## Tooling

- Git 2.55.0.windows.2: already installed.
- Git LFS 3.7.1: already installed; initialised for this repository only.
- Visual Studio Build Tools 2026 18.7.3: already installed.
- MSVC 14.51.36231 and Windows SDK 10.0.26100.0: already installed.
- Blender 5.2.1 LTS: installed with Windows Package Manager from Blender Foundation.
  Executable: `C:\Program Files\Blender Foundation\Blender 5.2\blender.exe`.
- Epic Games Launcher: installed with Windows Package Manager; first-run update completed,
  Simon signed in; launcher restarted after its Epic Online Services prerequisite.
  Executable: `C:\Program Files\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe`.
- .NET SDK 10.0.401: installed; verified with `dotnet --list-sdks`.
- Unreal Engine 5.8.3: installed at `C:\Program Files\Epic Games\UE_5.8`;
  verified Build.version (CL 58210709), editor compilation and Windows packaging.
- .NET Framework 4.8 SDK: added to existing Visual Studio Build Tools to resolve
  Unreal's missing NetFxSDK build failure. Installer completed with exit 0.

## Verification performed

- `setup/compiler-check.cpp` compiled and ran with the installed MSVC compiler:
  `GAME_REMAKES_CPP_OK`.
- `setup/blender-check.py` ran in Blender background mode, saved a Blender scene,
  and exported a nonempty GLB: `GAME_REMAKES_BLENDER_EXPORT_OK`.
- `git check-attr` confirmed LFS filtering for `.uasset` and `.blend` files.
- `setup/PipelineCheck/PipelineCheck.uproject`: editor target compiled successfully.
- `setup/check-unreal-import.py`: imported the Blender GLB as a saved Unreal
  StaticMesh; `BOTF_BLENDER_UNREAL_IMPORT_OK` in `output/unreal-import.log`.
  Commandlet reported 0 errors and 1 warning. This is a cube pipeline check,
  not ship art qualification.
- `game/BOTFRemake.uproject`: editor target compiled; Galaxy map saved; Windows
  Development build cooked, staged and packaged successfully. Standalone game
  launched with the interactive galaxy/colony interface.
- Evidence: `output/pipeline-build.log`, `output/unreal-import.log`, and
  `../game/Build/` build, map, package and play logs. These are developer checks,
  not independent audit. MSVC is newer than Unreal's preferred compiler version;
  the installed combination successfully built both projects.

## Project conventions

`.gitattributes` routes Unreal assets/maps, Blender scenes, FBX/GLB, WAV, and
common texture formats through Git LFS. `.gitignore` excludes Unreal-generated
build/cache folders and `setup/output/` verification artifacts.

Computer Use is already available. No third-party Unreal or Blender connector
has been installed; local scripting and command-line tools are sufficient for
the initial workflow. Unreal's Python editor plugin is enabled in both projects.

## References

- https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/scripting-the-unreal-editor-using-python


### 27 September 2026 — local reference extractor

Added project-local innoextract 1.9 under ignored `setup/output/innoextract-1.9`.
Official archive: https://constexpr.org/innoextract/files/innoextract-1.9/innoextract-1.9-windows.zip
SHA-256: 6989342C9B026A00A72A38F23B62A8E6A22CC5DE69805CF47D68AC2FEC993065.
Published MD5 was checked: 72D0D0DD874B6236EAA44411F4470EE1.
No system registration or game installation was performed. English Vanilla
components were extracted from Simon's supplied All-in-One 1.0.1 installer into
`research/reference-local/aio-1.0.1-vanilla`, preserving filename collisions.
Inventory/extraction logs are in `setup/output`. This reference tree is ignored by
Git and excluded from game packaging. Reproducible table reading is implemented by
`research/export-reference.py`; see the research log for provenance and limitations.
