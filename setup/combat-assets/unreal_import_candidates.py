"""Unreal Engine Python automation script to import candidate combat assets.

Reads game/ArtSource/CombatCandidates/manifest.json and imports verified game-ready GLB
models into /Game/CombatCandidates/<Faction>/<ShipName> as StaticMesh assets.

Features:
- Validates GLB file hashes against manifest prior to import.
- Filters by status: defaults to importing 'Ready' candidates only; excludes experimental visual studies unless explicitly enabled.
- Sanitizes faction path names (removes slashes, spaces, and illegal package characters).
- Accurately resolves imported StaticMesh assets using task.imported_object_paths.
- Asserts that imported assets are UStaticMesh instances and logs measured Unreal bounds.
- Supports --dry-run mode outside Unreal to verify manifests, hashes, and planned paths.

Usage in Unreal Editor Python console or command line:
UnrealEditor-Cmd.exe <project.uproject> -run=pythonscript -script="setup/combat-assets/unreal_import_candidates.py"
"""
import os
import sys
import json
import re
import hashlib
from pathlib import Path
from typing import Dict, Any, List, Optional

try:
    import unreal
except ImportError:
    unreal = None


def sanitize_path_segment(name: str) -> str:
    """Sanitize faction or class string into a valid Unreal package path segment."""
    # Replace slashes, spaces, hyphens with underscores
    clean = re.sub(r'[^a-zA-Z0-9_]', '_', name)
    clean = re.sub(r'_+', '_', clean).strip('_')
    return clean or "Common"


def compute_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


def find_manifest(start_dir: Optional[Path] = None) -> Optional[Path]:
    candidates = [
        Path(start_dir or Path.cwd()) / "game" / "ArtSource" / "CombatCandidates" / "manifest.json",
        Path(start_dir or Path.cwd()) / "ArtSource" / "CombatCandidates" / "manifest.json",
        Path(__file__).resolve().parents[2] / "game" / "ArtSource" / "CombatCandidates" / "manifest.json"
    ]
    for c in candidates:
        if c.is_file():
            return c.resolve()
    return None


def run_dry_run(include_experimental: bool = False) -> Dict[str, Any]:
    """Dry-run validation mode for command-line verification outside Unreal Engine."""
    manifest_path = find_manifest()
    if not manifest_path:
        raise FileNotFoundError("Could not find game/ArtSource/CombatCandidates/manifest.json")

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    converted_dir = manifest_path.parent / "converted"
    candidates = manifest.get("initial_candidates", [])

    print(f"=== DRY-RUN COMBAT CANDIDATE IMPORT VERIFICATION ===")
    print(f"Manifest: {manifest_path}")
    print(f"Total entries in manifest: {len(candidates)}")
    print(f"Include experimental studies: {include_experimental}\n")

    planned: List[Dict[str, Any]] = []
    skipped: List[Dict[str, Any]] = []
    errors: List[str] = []

    for cand in candidates:
        name = cand["candidate_name"]
        status = cand.get("status", "Ready")
        faction = cand.get("canonical_faction", "Common")
        clean_faction = sanitize_path_segment(faction)
        glb_filename = cand["output_glb"]
        source_file = converted_dir / glb_filename
        expected_hash = cand.get("output_glb_sha256")

        if status != "Ready" and not include_experimental:
            skipped.append({
                "name": name,
                "reason": f"Status is '{status}' (experimental study excluded from default import)"
            })
            continue

        if not source_file.is_file():
            errors.append(f"Missing GLB file: {source_file}")
            continue

        actual_hash = compute_sha256(source_file)
        if expected_hash and actual_hash != expected_hash:
            errors.append(f"Hash mismatch for {name}: expected {expected_hash}, got {actual_hash}")
            continue

        dest_dir = f"/Game/CombatCandidates/{clean_faction}/{name}"
        planned.append({
            "name": name,
            "status": status,
            "faction": clean_faction,
            "source_glb": str(source_file),
            "sha256": actual_hash,
            "destination_path": dest_dir,
            "expected_static_mesh": f"{dest_dir}/{name}.{name}"
        })

    print("PLANNED IMPORTS:")
    for p in planned:
        print(f"  [{p['status']:12}] {p['name']:16} -> {p['destination_path']}")

    print("\nEXCLUDED / SKIPPED ENTRIES:")
    for s in skipped:
        print(f"  {s['name']:16}: {s['reason']}")

    if errors:
        print(f"\nERRORS ENCOUNTERED: {len(errors)}")
        for err in errors:
            print(f"  ERROR: {err}")

    summary = {
        "valid": len(errors) == 0,
        "planned_count": len(planned),
        "skipped_count": len(skipped),
        "error_count": len(errors),
        "planned": planned,
        "skipped": skipped,
        "errors": errors
    }
    return summary


def run_unreal_import(include_experimental: bool = False):
    """Executes live import within Unreal Engine Python environment."""
    if unreal is None:
        print("Unreal module not available. Running dry-run verification instead:")
        return run_dry_run(include_experimental)

    project_dir = Path(unreal.Paths.project_dir()).resolve()
    manifest_path = find_manifest(project_dir)
    if not manifest_path:
        unreal.log_error(f"Cannot find candidate manifest at: {project_dir}")
        return

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    candidates = manifest.get("initial_candidates", [])
    converted_dir = manifest_path.parent / "converted"

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    unreal.log(f"Starting combat candidate batch import: {len(candidates)} total manifest entries.")

    successful_imports = 0
    skipped_count = 0

    for cand in candidates:
        name = cand["candidate_name"]
        status = cand.get("status", "Ready")
        faction = cand.get("canonical_faction", "Common")
        clean_faction = sanitize_path_segment(faction)
        glb_filename = cand["output_glb"]
        source_file = converted_dir / glb_filename
        expected_hash = cand.get("output_glb_sha256")

        if status != "Ready" and not include_experimental:
            unreal.log(f"Skipping experimental study {name} (status '{status}'). Use include_experimental=True to import.")
            skipped_count += 1
            continue

        if not source_file.is_file():
            unreal.log_error(f"GLB file missing for candidate {name}: {source_file}")
            continue

        actual_hash = compute_sha256(source_file)
        if expected_hash and actual_hash != expected_hash:
            unreal.log_error(f"Hash mismatch for {name}: manifest={expected_hash}, file={actual_hash}")
            continue

        dest_dir = f"/Game/CombatCandidates/{clean_faction}/{name}"
        task = unreal.AssetImportTask()
        task.filename = str(source_file)
        task.destination_path = dest_dir
        task.destination_name = name
        task.automated = True
        task.save = True
        task.replace_existing = True

        tools.import_asset_tasks([task])

        # Verify imported asset using imported_object_paths
        imported_paths = task.imported_object_paths
        verified_mesh = None

        if imported_paths:
            for p in imported_paths:
                obj = unreal.load_object(None, p)
                if isinstance(obj, unreal.StaticMesh):
                    verified_mesh = obj
                    break

        if not verified_mesh:
            fallback_path = f"{dest_dir}/{name}.{name}"
            obj = unreal.load_object(None, fallback_path)
            if isinstance(obj, unreal.StaticMesh):
                verified_mesh = obj

        if verified_mesh:
            bounds = verified_mesh.get_bounds()
            unreal.log(f"SUCCESS: Imported StaticMesh {name} -> {verified_mesh.get_path_name()} (SphereRadius: {bounds.sphere_radius:.3f})")
            successful_imports += 1
        else:
            unreal.log_warning(f"Import task completed but no valid UStaticMesh resolved for {name}")

    unreal.log(f"Combat candidate import finished: {successful_imports} imported, {skipped_count} skipped (experimental).")


if __name__ == "__main__":
    incl_exp = "--include-experimental" in sys.argv
    if unreal is not None:
        run_unreal_import(include_experimental=incl_exp)
    else:
        res = run_dry_run(include_experimental=incl_exp)
        if not res["valid"]:
            sys.exit(1)
