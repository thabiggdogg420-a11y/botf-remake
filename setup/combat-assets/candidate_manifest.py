"""Combat candidate pipeline orchestrator and manifest generator.

Safely unpacks permitted community assets, runs headless Blender conversions,
evaluates candidate suitability against the BOTF canonical roster, distinguishes
production-ready candidates from experimental visual studies, and writes
the authoritative candidate manifest in game/ArtSource/CombatCandidates/manifest.json.
"""
import sys
import os
from pathlib import Path
import subprocess
import json
import hashlib
from typing import Dict, Any, List

# Setup project paths
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parents[1]
ARCHIVE_PATH = PROJECT_ROOT / "setup" / "output" / "iwmeshpack1.zip"
CANDIDATES_ROOT = PROJECT_ROOT / "game" / "ArtSource" / "CombatCandidates"
SOURCE_DIR = CANDIDATES_ROOT / "source"
CONVERTED_DIR = CANDIDATES_ROOT / "converted"
MANIFEST_PATH = CANDIDATES_ROOT / "manifest.json"

BLENDER_EXE = Path(r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe")

# Import local archive validator
sys.path.insert(0, str(SCRIPT_DIR))
from archive_validator import SafeArchiveValidator


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


def extract_sources() -> Dict[str, Any]:
    """Safely extracts candidate files from archive into source directory."""
    print(f"Inspecting archive: {ARCHIVE_PATH}")
    validator = SafeArchiveValidator(ARCHIVE_PATH)
    archive_info = validator.inspect()
    if not archive_info["valid"] or not archive_info["all_entries_safe"]:
        raise ValueError(f"Archive failed safety validation: {archive_info}")

    SOURCE_DIR.mkdir(parents=True, exist_ok=True)
    CONVERTED_DIR.mkdir(parents=True, exist_ok=True)

    needed_prefixes = [
        "Completed/ranger/",
        "Completed/reaver/",
        "Completed/ulysses/",
        "Completed/lca/",
        "Unfinished/kqultoq.ms3d",
        "Unfinished/RemanBoP/",
        "Unfinished/Romulan Hellhawk/",
        "README.txt"
    ]
    extracted = validator.safe_extract_filtered(SOURCE_DIR / "Icewolf132", needed_prefixes)
    print(f"Extracted {len(extracted)} files safely into {SOURCE_DIR / 'Icewolf132'}")
    return archive_info


def convert_asset(source_file: Path, output_file: Path, faction: str, author: str,
                  target_class: str, status: str = "Ready", source_rel: str = "") -> Dict[str, Any]:
    """Run Blender conversion script to convert source model to game-ready GLB."""
    meta_file = CONVERTED_DIR / f"{output_file.stem}.meta.json"
    cmd = [
        str(BLENDER_EXE),
        "--background",
        "--python",
        str(SCRIPT_DIR / "convert_combat_assets.py"),
        "--",
        "--source", str(source_file),
        "--output", str(output_file),
        "--meta", str(meta_file),
        "--faction", faction,
        "--author", author,
        "--class", target_class,
        "--status", status,
        "--source-rel", source_rel
    ]
    res = subprocess.run(cmd, capture_output=True, text=True, check=True)
    if not meta_file.is_file():
        raise RuntimeError(f"Conversion did not generate metadata file {meta_file}: {res.stderr}")
    return json.loads(meta_file.read_text(encoding='utf-8'))


def run_pipeline() -> Dict[str, Any]:
    archive_info = extract_sources()

    ice_root = SOURCE_DIR / "Icewolf132"
    conversions: List[Dict[str, Any]] = []

    # =========================================================================
    # 1. PRODUCTION-READY INITIAL CANDIDATES (Status: "Ready")
    # =========================================================================

    # 1.1 Federation: Ranger Scout / Light Escort
    conversions.append(convert_asset(
        source_file=ice_root / "Completed" / "ranger" / "franger.ms3d",
        output_file=CONVERTED_DIR / "franger.glb",
        faction="Federation",
        author="Icewolf132",
        target_class="Ranger Scout / Light Escort (Provisional Illustration)",
        status="Ready",
        source_rel="source/Icewolf132/Completed/ranger/franger.ms3d"
    ))

    # 1.2 Federation: Reaver Variant 1 (Fast Destroyer)
    conversions.append(convert_asset(
        source_file=ice_root / "Completed" / "reaver" / "freaver1.ms3d",
        output_file=CONVERTED_DIR / "freaver1.glb",
        faction="Federation",
        author="Icewolf132",
        target_class="Reaver Fast Destroyer (Variant 1)",
        status="Ready",
        source_rel="source/Icewolf132/Completed/reaver/freaver1.ms3d"
    ))

    # 1.3 Federation: Reaver Variant 2 (Mainline Destroyer / Miranda tier)
    conversions.append(convert_asset(
        source_file=ice_root / "Completed" / "reaver" / "freaver2.ms3d",
        output_file=CONVERTED_DIR / "freaver2.glb",
        faction="Federation",
        author="Icewolf132",
        target_class="Reaver Mainline Destroyer (Variant 2)",
        status="Ready",
        source_rel="source/Icewolf132/Completed/reaver/freaver2.ms3d"
    ))

    # 1.4 Federation: Reaver Variant 3 (Heavy Destroyer / Constellation tier)
    conversions.append(convert_asset(
        source_file=ice_root / "Completed" / "reaver" / "freaver3.ms3d",
        output_file=CONVERTED_DIR / "freaver3.glb",
        faction="Federation",
        author="Icewolf132",
        target_class="Reaver Heavy Destroyer (Variant 3)",
        status="Ready",
        source_rel="source/Icewolf132/Completed/reaver/freaver3.ms3d"
    ))

    # 1.5 Federation: Ulysses Dreadnought / Command Cruiser
    conversions.append(convert_asset(
        source_file=ice_root / "Completed" / "ulysses" / "fUlysses.ms3d",
        output_file=CONVERTED_DIR / "fUlysses.glb",
        faction="Federation",
        author="Icewolf132 (Original design: Taldren)",
        target_class="Ulysses Dreadnought / Command Cruiser",
        status="Ready",
        source_rel="source/Icewolf132/Completed/ulysses/fUlysses.ms3d"
    ))

    # 1.6 Klingon: Attack Cruiser Study (Vor'cha / K't'inga archetype)
    fleet_klingon = PROJECT_ROOT / "game" / "ArtSource" / "FleetStudies" / "Klingon.glb"
    conversions.append(convert_asset(
        source_file=fleet_klingon,
        output_file=CONVERTED_DIR / "klingon_cruiser.glb",
        faction="Klingon",
        author="BOTF Remake Project",
        target_class="Klingon Attack Cruiser Study (Vor'cha / K't'inga archetype)",
        status="Ready",
        source_rel="game/ArtSource/FleetStudies/Klingon.glb"
    ))

    # 1.7 Independent / Permitted: Lyran Tiger Cruiser
    conversions.append(convert_asset(
        source_file=ice_root / "Completed" / "lca" / "lcruise1.ms3d",
        output_file=CONVERTED_DIR / "lcruise1.glb",
        faction="Independent",
        author="Icewolf132 (Original design: Taldren)",
        target_class="Lyran Tiger Cruiser",
        status="Ready",
        source_rel="source/Icewolf132/Completed/lca/lcruise1.ms3d"
    ))

    # =========================================================================
    # 2. EXPERIMENTAL VISUAL STUDIES (Status: "Experimental")
    # Excluded from default batch import; retained as recoverable visual studies
    # =========================================================================

    # 2.1 Klingon Concept: K'qultoq Fighter Concept Mesh
    conversions.append(convert_asset(
        source_file=ice_root / "Unfinished" / "kqultoq.ms3d",
        output_file=CONVERTED_DIR / "kqultoq.glb",
        faction="Klingon",
        author="Icewolf132",
        target_class="K'qultoq Fighter Concept Mesh (Unfinished author concept; synthetic PBR armor)",
        status="Experimental",
        source_rel="source/Icewolf132/Unfinished/kqultoq.ms3d"
    ))

    # 2.2 Reman Concept: Reman Bird of Prey Study
    conversions.append(convert_asset(
        source_file=ice_root / "Unfinished" / "RemanBoP" / "RemanBoP.ms3d",
        output_file=CONVERTED_DIR / "RemanBoP.glb",
        faction="Reman",
        author="Icewolf132",
        target_class="Reman Bird of Prey Study (Abandoned author project; distinct from Romulan BOTF classes)",
        status="Experimental",
        source_rel="source/Icewolf132/Unfinished/RemanBoP/RemanBoP.ms3d"
    ))

    # 2.3 Romulan Concept: Hellhawk TMP Study
    conversions.append(convert_asset(
        source_file=ice_root / "Unfinished" / "Romulan Hellhawk" / "rhhawk_TMP.ms3d",
        output_file=CONVERTED_DIR / "rhhawk_TMP.glb",
        faction="Romulan",
        author="Icewolf132",
        target_class="Romulan Hellhawk TMP Study (Unfinished author concept)",
        status="Experimental",
        source_rel="source/Icewolf132/Unfinished/Romulan Hellhawk/rhhawk_TMP.ms3d"
    ))

    # Master manifest
    manifest = {
        "manifest_version": "1.1.0",
        "description": "BOTF 3D Combat Ship Asset Pipeline - Candidate Manifest",
        "date_generated": "2026-09-27",
        "provenance_archive": {
            "archive_file": ARCHIVE_PATH.name,
            "archive_sha256": archive_info["sha256"],
            "archive_size_bytes": archive_info["size_bytes"],
            "license_terms_file": "source/Icewolf132/README.txt",
            "license_grant": "All are free to complete, edit, rip, kitbash, or convert the models and files within the pack without seeking permission first, under the condition that credit is given where it is due. This is not limited to the Armada community; feel free to use for another game or purpose.",
            "franchise_rights_notice": "Author terms apply strictly to the 3D model files created by Icewolf132. Star Trek designs, faction names, and likenesses remain the intellectual property of Paramount/CBS. Community author terms do not transfer franchise rights."
        },
        "initial_candidates": conversions,
        "external_shortlist_evaluations": [
            {
                "candidate": "U.S.S. Defiant NX-74205 (FatManInABox)",
                "source_portal": "BlendSwap (blend/22476)",
                "botf_fit": "Federation Heavy Escort (Defiant, Race 1, ID 34)",
                "declared_terms": "CC-BY (declared by uploader); description asks for non-commercial credit",
                "status": "Candidate - Gated",
                "acquisition_blocker": "BlendSwap downloads require authenticated user account login; platform developer documentation explicitly disclaims license verification; franchise rights unestablished",
                "work_required_when_acquired": "Cycles procedural material conversion to PBR textures; pivot/scale normalization; LOD generation"
            },
            {
                "candidate": "Star Trek Sovereign Class (woki)",
                "source_portal": "BlendSwap (blend/10430)",
                "botf_fit": "Federation Sovereign Dreadnought (Race 1, ID 26/27)",
                "declared_terms": "CC-BY (declared by uploader)",
                "status": "Candidate - Gated / Unfinished",
                "acquisition_blocker": "BlendSwap user account login required; author notes model is unfinished and shader work remains",
                "work_required_when_acquired": "Mesh completion, UV mapping, material creation, engine conversion"
            },
            {
                "candidate": "Vor'cha Battlecruiser",
                "source_portal": "Trekmeshes (download/all)",
                "botf_fit": "Klingon Vor'cha Attack Cruiser (Race 3, ID 69/70)",
                "declared_terms": "Unresolved; no general game reuse grant on catalogue",
                "status": "Candidate - Unresolved",
                "acquisition_blocker": "Trekmeshes has no general redistribution grant; site pending closure; author contact barred by hard rules / personal-use boundary",
                "work_required_when_acquired": "Author clearance; MAX/3DS legacy mesh import; material rebuilding; scale normalization"
            },
            {
                "candidate": "Klingon Bird of Prey (B'rel / K'Vort)",
                "source_portal": "Trekmeshes (download/all)",
                "botf_fit": "Klingon B'rel Scout (ID 75) / K'Vort Heavy Cruiser (ID 83)",
                "declared_terms": "Unresolved; multiple distinct designs listed under same name",
                "status": "Candidate - Unresolved",
                "acquisition_blocker": "Author terms unresolved; ambiguity across variants; download redirection",
                "work_required_when_acquired": "Variant identification, author clearance, LWO/3DS conversion"
            },
            {
                "candidate": "Negh'Var Flagship",
                "source_portal": "Trekmeshes (download/all)",
                "botf_fit": "Klingon Negh'Var Strike Cruiser (Race 3, ID 65/66)",
                "declared_terms": "Unresolved (Knightrider release)",
                "status": "Candidate - Unresolved",
                "acquisition_blocker": "Author reuse terms unverified; no external messaging permitted",
                "work_required_when_acquired": "Author verification, MAX/3DS conversion, PBR texture creation"
            }
        ],
        "class_mapping_policy": {
            "rule": "Strict Class Identity: Do not silently equate one hull with every class or tech tier. Unfinished concept meshes must remain experimental studies, not asserted canonical classes.",
            "production_candidates": {
                "franger.glb": "Permitted Scout / Escort illustration (provisional stand-in; not an Oberth or Defiant).",
                "freaver1.glb": "Permitted Fast Destroyer variant.",
                "freaver2.glb": "Permitted Mainline Destroyer variant (Miranda-tier destroyer illustration).",
                "freaver3.glb": "Permitted Heavy Destroyer variant (Constellation-tier destroyer illustration).",
                "fUlysses.glb": "Permitted Dreadnought illustration (3-nacelle dreadnought; Sovereign/Galaxy-X counterpart; not an Enterprise-D/Galaxy).",
                "klingon_cruiser.glb": "Project-authored Vor'cha / K't'inga Attack Cruiser study (interim battlecruiser silhouette).",
                "lcruise1.glb": "Permitted Independent Lyran Tiger Cruiser."
            },
            "experimental_studies": {
                "kqultoq.glb": "Experimental visual study: K'qultoq Klingon fighter concept mesh from Icewolf132; unfinished concept mesh with synthetic tactical PBR armor; excluded from default batch import.",
                "RemanBoP.glb": "Experimental visual study: Reman Bird of Prey concept from Icewolf132; abandoned author project; Reman identity distinct from Romulan BOTF classes; excluded from default batch import.",
                "rhhawk_TMP.glb": "Experimental visual study: Romulan Hellhawk TMP from Icewolf132; unfinished author concept; excluded from default batch import."
            }
        }
    }

    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"Manifest written successfully to: {MANIFEST_PATH}")
    return manifest


if __name__ == "__main__":
    run_pipeline()
