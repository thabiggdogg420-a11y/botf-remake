# BOTF Remastered source and downloads

This public repository contains the project's source, tests, build helpers and the artwork actually tracked by Git. **It is not yet a complete, reproducible copy of the currently hosted game with all its audio and ship assets.**

[Play the hosted browser beta](https://free4allhub.com/botfremaster/) · [Hosted source/download information](https://botf-remastered.thabiggdogg420.chatgpt.site/source)

The hosted game, this source checkout and older downloadable builds are separate deliverables. A playable hosted build does not prove that every original asset or build input is in this repository.

## Download the tracked source and art

Install Git and Git LFS first, then:

```sh
git clone https://github.com/thabiggdogg420-a11y/botf-remake.git
cd botf-remake
git lfs install --local
git lfs pull
git lfs checkout
git lfs fsck
```

Do not assume GitHub's ordinary source ZIP contains the binary contents of every LFS object. Git LFS pointer text is not a usable model or texture.

A fresh anonymous clone of commit `3a6174c8e4128269f5805fd5fd106699bc0327ab` was checked on 18 November 2026: 1,350 tracked files, with all 266 LFS objects downloaded and hydrated. Their 97,792,066 bytes matched their recorded SHA-256 hashes and sizes, and `git lfs fsck` passed. This checks the tracked download, **not** a complete game rebuild, Godot import, gameplay or future LFS availability.

## Choose the client

- **Godot 4.5 client:** [Godot guide](remaster/godot/README.md). Its native extension requires a C++20 compiler, CMake and the separately fetched Godot C++ bindings. Exporting also needs matching engine templates and target toolchains.
- **Preact/TypeScript browser client:** [browser guide](remaster/web/README.md). This is a separate client, not a promise of an identical reconstruction of the hosted Godot export.
- **Unreal client and standalone simulation:** [game guide](game/README.md), [beta acceptance criteria](planning/BETA-ACCEPTANCE.md) and [beta review status](game/REVIEW-BETA.md). Unreal builds require the documented Windows/Unreal setup.

The [preserved milestone](planning/milestones/BOTF-20260928-PLAYABLE-BETA.md) describes an older packaged Windows beta. Its build and local saves are intentionally not tracked here.

## What is still missing for the entire game

At the checked commit:

- No `.wav`, `.ogg` or `.mp3` files are tracked. The [audio-pack directory](remaster/godot/scripts/audio/pack/README.md) contains instructions, not the generated audio pack. Its documented inputs include original BOTF banks and extracted Trek-library material.
- `remaster/game/LocalAssets/` is deliberately ignored. The Godot `assets/` checkout contains governors and structures, not the complete packed ship set used by a release.
- 42 `.glb` and 2 `.blend` files are tracked across the repository. These are not evidence that every runtime ship and texture is included or used.
- Godot native extension binaries, generated build directories and Godot C++ dependencies are not supplied as a complete ready-to-import native build.
- A public source checkout alone has not been matched, file for file, to the hosted release. No clean native or hosted-equivalent rebuild is claimed.

## Asset provenance and reuse

Keep [community asset credits](game/ASSET-CREDITS.md), [remaster asset-source records](remaster/docs/ASSET-SOURCES.md), original author notices and font licences with the relevant files. The asset-source records distinguish clear, personal-use, restricted and unresolved candidates; a candidate being listed does not mean it was acquired or licensed for redistribution.

This repository does **not** grant a blanket licence to Star Trek designs, original BOTF audio, show clips or third-party mod content. Hosting or linking to an asset does not change its author's terms. The audio and LocalAssets documentation currently identifies private/local-only inputs; these must not be described as freely redistributable merely because a game build contains them.

## Completing a durable distribution

The remaining release work is to reconcile the exact deployed build and its source commit; inventory every runtime pack and build input; record each file's provenance and redistribution terms; and publish an appropriately licensed, versioned release with byte sizes and SHA-256 checksums. A clean unauthenticated download, extraction and build/import test must then establish what actually works.

Compiled browser packs can make an exact playable release downloadable, but are not substitutes for editable source models, unprocessed audio or native build prerequisites. If an input cannot be redistributed, document the gap or provide an appropriately licensed replacement rather than claiming the entire game is available.
