# Building the Unreal client from a cloud Claude session

**Decision:** the owner builds Unreal locally on the Windows PC with `game/Package-Windows.ps1`. A cloud session
(a Linux container with no GPU, an ephemeral disk and network through a proxy) does everything that does not need the
engine. The loop is: the cloud session edits and runs its checks; the owner runs the script; the owner pastes
`FAILURE-SUMMARY.txt` back.

This note is short on purpose. The owner asked to spend the effort on local packaging. The claims below are
from general knowledge as of 2026 and were not re-verified against current Epic or GitHub pages.

| Option | Verdict |
|---|---|
| **Unreal Engine for Linux inside the container** | Not practical. The engine is only available as source to GitHub accounts linked to Epic, so the container needs those credentials. A source build needs roughly 100-200 GB of disk and several hours of CPU, which exceeds an ephemeral session's disk and time, and is lost when the session ends. A Linux compile would also not catch MSVC-only errors, and MSVC is the compiler that matters here. |
| **Linux stand-in checks (in place)** | `game/Tools/check-unreal-tu.sh` and the CI job `unreal-tu` compile the core as UBT's single translation unit, and compile the core and view models with engine and Windows macro names predefined. The out-of-repo Slate shim covers our own widget code. None of these prove that engine APIs exist. |
| **Self-hosted GitHub Actions runner on the PC** | The best fully automated option, if it is wanted later. Pushes from the cloud would trigger real UE builds, and logs would come back as PR checks and artifacts. Costs: the PC must be on, the runner must run as a service with the engine installed, and runner security matters. Only trigger it on `workflow_dispatch` or on PRs from the owner's own branches, never on forks. A workflow would call `Package-Windows.ps1 -SkipPackage` and upload `Build/Logs/**`. It is not added now. |
| **Claude Code on the PC (local, or Remote Control of a local session)** | The simplest way to close the loop without copy-paste. A local Claude Code session can run `Package-Windows.ps1` itself, read the logs and fix errors in place. Recommended for the first-compile iteration. |
| **Epic / ue4-docker containers** | Development images need Epic-linked credentials to pull or build, are very large (tens of GB), and in practice target Linux or Windows Server hosts with plenty of disk. They do not fit an ephemeral cloud session; they suit a dedicated CI host. |
| **Cloud Windows VM (Azure/AWS)** | Works, but costs money per hour. The engine and Visual Studio must be installed or imaged (100+ GB), and it duplicates the PC the owner already has. Worth it only for unattended CI when the PC is not available. |

**Recommendation:** run Claude Code locally on the Windows PC for the first real compiles, driving
`Package-Windows.ps1`. Keep the cloud sessions for core, web and view-model work guarded by the Linux stand-in checks.
If hands-off builds from cloud pushes are wanted later, add a self-hosted Windows runner labelled
`[self-hosted, windows, ue58]` that runs the same script.
