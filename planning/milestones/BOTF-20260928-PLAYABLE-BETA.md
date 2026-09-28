# BOTF playable beta milestone — 28 September 2026

This is the parked playable beta baseline. Future gameplay and interface changes should start from a separate development branch or working copy and leave this milestone untouched.

- Launch the preserved Windows package at `game/Build/Milestone-BOTF-20260928/Windows/BOTFRemake.exe`. Keep its entire `Windows` directory together.
- Build ID: `BOTF-20260928-000531`.
- The matching project snapshot is `game/Build/Milestone-BOTF-20260928/source-snapshot.zip`. It contains `Source`, `Config`, `Content`, `ArtSource`, the `.uproject`, build/test/launch scripts, project notes, and the beta handoff. It excludes generated directories, user saves, local reference extraction, and unrelated workspace files.
- `game/Build/Milestone-BOTF-20260928/milestone-manifest.json` records SHA-256 and byte length for all 62 packaged files plus the source snapshot. Each copied package file was hash-compared with `game/Build/EraCombatBeta-20260928/Windows` when this milestone was made.
- The package's `build-manifest.json` records 27 source file hashes. All 27 matched the live project source immediately before snapshotting. Its inner executable SHA-256 is `8D8146A9AF555FB811B3CFC40043D9CAC511943AF261EA672D73EF4AD90EB57B` and matched the packaged binary.

The existing beta handoff records successful native, Unreal, UI, save, packaging, and hidden NullRHI startup checks. Interactive rendered gameplay and visual quality still need a human pass; this preservation does not add that acceptance. The default launcher remains outside this milestone.

The repository currently has no commits or remotes. This milestone is a file snapshot, not a Git tag. When work resumes, create a separate development branch or working copy after establishing a deliberate project baseline; do not fold future changes into this frozen package. The next requested design work is troop transport outpost/starbase ability, sideways ship movement, and a full system-management interface redesign replacing queue buttons. Expanded tech/research and the ship roster follow later. None of those changes are included here.
